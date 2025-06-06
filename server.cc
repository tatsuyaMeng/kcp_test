#include "server.h"

// KCP 输出回调函数
int32_t kcp_output(const char* buf, int32_t len, ikcpcb* kcp, void* user) {
    int32_t sent = sendto(bind_fd, buf, len, 0, (sockaddr*)user, sizeof(*(sockaddr*)user));
    if (sent < 0) {
        if (errno == EAGAIN) {
            // 监听 EPOLLOUT
        } else {
            std::cout << "对方不具备接受能力，关闭连接" << std::endl;
            return -1;  // 告知KCP发送失败
        }
    }

    printHead(buf, len, true, kcp);
    //std::cout << "sendto info: " << string(buf, len) << " len: "<< len << std::endl;
    return sent;
}

// 网络线程函数
void NetThreadFunc() {
    int is_use_kcp = 1, tcp_fd = -1;
    char recv_buffer[65535];
    char send_buffer[65535];
    uint32_t check_count = 0;
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    epoll_event events[MAX_EVENTS];
    int32_t epoll_fd = epoll_create1(0);
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = bind_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, bind_fd, &ev);

    while (true) {
        int32_t n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1);

        if (n > 0) {
            for (int32_t i = 0; i < n; ++i) {
                if (events[i].events & EPOLLERR) {
                    std::cout << "epoll error: " << events[i].data.fd << ", " << error << ", " << strerror(errno) << std::endl;
                }

                if (events[i].data.fd == bind_fd && events[i].events & EPOLLIN) {
                    while (true)
                    {
                        ssize_t len = recvfrom(bind_fd, recv_buffer, sizeof(recv_buffer), 0,
                                            (sockaddr*)&client_addr, &addr_len);
                        if (len <= 0) {
                            if (errno != EAGAIN && errno != EINTR) {
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, bind_fd, &ev);
                                std::cout << "recvfrom error: " << errno  << " " << strerror(errno) << std::endl;
                            }

                            break;
                        }
    
                        if (len < (int)IKCP_OVERHEAD)
                        {
                            std::cout << "err, recv buf len < kcp head" << std::endl;
                            continue;
                        }
                        
                        IUINT32 kcp_conv;
                        ikcp_decode32u(recv_buffer, &kcp_conv);
                        uint64_t key = session_key(client_addr);
                        auto it = session_map.find(key);
                        if (it != session_map.end()) {
                            if (it->second.kcp->conv != kcp_conv) {
                                std::cout << "err, kcp conv not match, close old kcp and create new kcp" << std::endl;
                                delete (sockaddr*)it->second.kcp->user;
                                ikcp_release(it->second.kcp);
                                session_map.erase(it);
                                it = session_map.end();
                            }
                        }

                        if (it == session_map.end()) {
                            if (!is_use_kcp){
                                std::cout << "err, not use kcp" << std::endl;
                                continue;
                            }

                            std::cout << "receive new client conv:" << kcp_conv << " " << client_addr.sin_addr.s_addr << ":" << client_addr.sin_port << std::endl;

                            if (session_map.size() >= MAX_SESSION_NUM) {
                                std::cout << "max session num, close new client" << std::endl;
                                continue;
                            }

                            if (len > sizeof(recv_buffer)) {
                                std::cout << "recv buffer overflow" << std::endl;
                                continue;
                            }

                            sockaddr *user = new sockaddr();
                            *user = *(sockaddr*)&client_addr;
                            ikcpcb *kcp = ikcp_create(kcp_conv, (void*)user);
                            ikcp_setoutput(kcp, kcp_output);
                            ikcp_wndsize(kcp, SND_WND_SIZE, SND_WND_SIZE);
                            ikcp_nodelay(kcp, 1, 10, 2, 1);

                            session_map[key] = {kcp, get_clock()};
                            it = session_map.find(key);
                        }

                        printHead(recv_buffer, len, false, it->second.kcp);
                        int res = ikcp_input(it->second.kcp, recv_buffer, len);
                        if (res < 0)
                        {
                            std::cout << "ikcp_input error" << res << " " << recv_buffer;
                            continue;
                        }

                        it->second.last_active = get_clock();
                        int32_t data_len = ikcp_recv(it->second.kcp, recv_buffer, sizeof(recv_buffer));
                        if (data_len > 0) {                   
                            // std::cout << "Received: " << string(recv_buffer, data_len)  << " len:" << data_len << std::endl;
                            EventQueue *event = new EventQueue(key, data_len);
                            kcp_event_push(event);
                            kcp_recv_buf_queue.push_back(recv_buffer, data_len);
                        }
                    }
                } 
                else if (!is_use_kcp && events[i].data.fd == tcp_fd) {
                    // 创建新 TCP 连接
                } 
                else if (events[i].events & EPOLLIN) {
                    // 处理已存在的 TCP 连接
                }
            }
        }

        EventQueue req;
        while (kcp_response_pop(&req))
        {
            kcp_send_buf_queue.pop_front(send_buffer, req.len_);
            auto it = session_map.find(req.key_);
            if (it != session_map.end()) {
                // std::cout << "send:" << send_buffer << std::endl;
                ikcp_send(it->second.kcp, send_buffer, req.len_);
            } else {
                std::cout << "send error, cant't find req kcp" << req.len_ << std::endl;
            }
        }

        // 定时更新及清理超时连接
        auto now_msec = get_clock();
        for (auto it = session_map.begin(); it != session_map.end();)
        {
            if (now_msec - it->second.last_active > 30000 || ikcp_waitsnd(it->second.kcp) > MAX_WAIT_SND_SIZE) {
                //std::cout << "timeout or wait snd size > " << MAX_WAIT_SND_SIZE << ", release kcp conv:" << it->second.kcp->conv << std::endl;
                delete (sockaddr*)it->second.kcp->user;
                ikcp_release(it->second.kcp);
                it = session_map.erase(it);
                continue;
            }

            if (now_msec >= ikcp_check(it->second.kcp, now_msec))
            {
                ikcp_update(it->second.kcp, now_msec);
            }
             
            ++it;
        }
    }
    close(epoll_fd);
}

// 工作线程函数（处理业务逻辑）
void WorkerThreadFunc() {
    EventQueue evt;
    char buffer[65535];
    uint32_t recv_count = 0;
    while (true)
    {
        if (!kcp_event_pop(&evt)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        kcp_recv_buf_queue.pop_front(buffer, evt.len_);
        //std::cout << "Received: " << string(buffer, evt.len_) << std::endl;
        int32_t num = atoi(buffer);
        snprintf(buffer, sizeof(buffer), "%d", ++num);
        int32_t send_len = strlen(buffer);

        EventQueue *response = new EventQueue(evt.key_, send_len);
        kcp_response_push(response);
        kcp_send_buf_queue.push_back(buffer, send_len);
    }
}

int32_t main() {
    // 创建 UDP 套接字
    bind_fd = socket(AF_INET, SOCK_DGRAM, 0);
    // int32_t opt = 1;
    // setsockopt(bind_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    int32_t buf_size = 1024 * 1024;
    setsockopt(bind_fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

    // 绑定地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(bind_fd, (sockaddr*)&server_addr, sizeof(server_addr));

    // 设置非阻塞模式
    int32_t flags = fcntl(bind_fd, F_GETFL, 0);
    fcntl(bind_fd, F_SETFL, flags | O_NONBLOCK);

    // 启动网络线程和工作线程
    std::thread networker_thread(NetThreadFunc);
    std::thread worker_thread(WorkerThreadFunc);

    std::cout << "Server started on port " << PORT << std::endl;

    networker_thread.join();
    worker_thread.join();
    
    close(bind_fd);
    return 0;
}