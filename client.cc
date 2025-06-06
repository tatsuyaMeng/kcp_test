#include "client.h"

// KCP输出回调
int kcp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    int32_t sent = send(*(int*)user, buf, len, 0);
    if (sent == -1 && errno != EAGAIN && errno!= EINTR) {
        return -1;
    }

    // printHead(buf, len, true, kcp);
    // std::cout << "sendto info: " << string(buf, len) << " len: "<< len << std::endl;
    return sent;
}

// 发送线程
void send_thread() {
    char recv_buffer[65535];
    char send_buffer[65535] = "test111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111101234\0";
    // char send_buffer[65535] = "test222222222222222222222222222222222222222222222222222222222222\0";
    uint32_t send_len = strlen(send_buffer);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(fd, (sockaddr*)&local_addr, sizeof(local_addr));

    int32_t flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    // 配置服务器地址
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect error");
        close(fd);
        return;
    }

    // 初始化KCP
    ikcpcb *kcp = ikcp_create(get_kcp_conv(), &fd);
    ikcp_setoutput(kcp, kcp_output);
    ikcp_wndsize(kcp, 256, 256);  
    ikcp_nodelay(kcp, 1, 10, 2, 1); 

    int snd_buf_size = 1024 * 1024; 
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_buf_size, sizeof(snd_buf_size));

    uint32_t count = 0;
    uint32_t seq = 1;
    while (true)
    {
        ssize_t recv_len = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
        if (recv_len > 0) {
            printHead(recv_buffer, recv_len, false, kcp);
            ikcp_input(kcp, recv_buffer, recv_len);
            //cout << "recv original:" << string(recv_buffer, recv_len) << "len" << recv_len << endl;
        }

        while (true) {
            int32_t data_len = ikcp_recv(kcp, recv_buffer, sizeof(recv_buffer));
            if (data_len > 0) {
                std::cout << "Received: " << string(recv_buffer, data_len) << " len:" << data_len << std::endl;
            } else {
                break;
            }
        }
        

        if (++count % 10 == 0) {
            // snprintf(send_buffer, sizeof(send_buffer), "%d", seq);
            // cout << "send data: " << string(send_buffer, strlen(send_buffer)) << endl;
            ikcp_send(kcp, send_buffer, strlen(send_buffer));
            // seq += 2;
        }

        auto now_msec = get_clock();
        if (now_msec >= ikcp_check(kcp, now_msec))
        {
            ikcp_update(kcp, now_msec);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main()
{
    std::vector<std::thread> threads;
    for (int i = 0; i < WORKER_THREAD_COUNT; ++i) {
        threads.emplace_back(send_thread);
    }

    std::cout << "Client started, sending to " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    for (auto& t : threads) t.join();
    std::cout << "program finish =================================" << std::endl;
    return 0;
}