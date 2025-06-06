#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>          
#include <ctime>       
#include <chrono>  
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "ikcp.h"

using namespace std;
using namespace std::chrono;

#define MAX_EVENTS 2
#define PORT 10000
#define KCP_UPDATE_INTERVAL 10
#define SND_WND_SIZE 128
#define MAX_WAIT_SND_SIZE SND_WND_SIZE * 2
#define MAX_SESSION_NUM 10000

int bind_fd;

// 会话结构�?
typedef struct
{
    ikcpcb* kcp;
    uint32_t last_active;
} Session;

typedef struct
{
    int32_t bind_fd;
    sockaddr_in peer_addr;
} KcpUser;

class EventQueue
{
public:
    int32_t len_;
    uint64_t key_;

    EventQueue(uint64_t key, int32_t len) {
        key_ = key;
        len_ = len;
    }

    EventQueue() {};
};

//---------------------------------------------------------------
// 事件队列
mutex kcp_req_mutex;
mutex kcp_event_mutex;
list<EventQueue *> recv_response_event_queue;
list<EventQueue *> recv_kcp_event_queue;

bool kcp_response_pop(EventQueue* req)
{
    EventQueue *temp = nullptr;
    kcp_req_mutex.lock();
    if (!recv_response_event_queue.empty()) {
        temp = recv_response_event_queue.front();
        recv_response_event_queue.pop_front();
    }
    kcp_req_mutex.unlock();

    if (temp == nullptr) {
        return false;
    }

    *req = *temp;
    delete temp;
    return true;
}

void kcp_response_push(EventQueue *event)
{
    kcp_req_mutex.lock();
    recv_response_event_queue.push_back(event);
    kcp_req_mutex.unlock();
}

bool kcp_event_pop(EventQueue* req)
{
    EventQueue *temp = nullptr;
    kcp_event_mutex.lock();
    if (!recv_kcp_event_queue.empty()) {
        temp = recv_kcp_event_queue.front();
        recv_kcp_event_queue.pop_front();
    }
    kcp_event_mutex.unlock();

    if (temp == nullptr) {
        return false;
    }
    
    *req = *temp;
    delete temp;
    return true;
}

void kcp_event_push(EventQueue *event)
{
    kcp_event_mutex.lock();
    recv_kcp_event_queue.push_back(event);
    kcp_event_mutex.unlock();
}
//---------------------------------------------------------------



//---------------------------------------------------------------
// 环形缓冲�?
class RingBuffer
{
private:
    char* head_;
    char* tail_;
    char* data_;
    int32_t size_;

public:
    RingBuffer(int32_t buffer_size) : size_(buffer_size) {
        data_ = new char[buffer_size];
        head_ = data_;
        tail_ = data_;
    };

    bool push_back(const char *recv_data, int32_t len)
    {
        int32_t temp = (int32_t)(tail_ + (uint32_t)size_ - head_);
        int32_t used = temp >= size_ ? temp - size_ : temp;
        if (len + used + 1 > size_) {
            return false;
        }

        if (tail_ + len >= data_ + size_) {
            int32_t seg1 = (int32_t)(data_ + size_ - tail_);
            int32_t seg2 = len - seg1;
            memcpy(tail_, recv_data, seg1);
            memcpy(data_, recv_data + seg1, seg2);
            tail_ = data_ + seg2;
        } else {
            memcpy(tail_, recv_data, len);
            tail_ += len;
        }
        return true;
    }

    bool pop_front(char* buf, int32_t len)
    {
        int32_t temp = (int32_t)(tail_ + (uint32_t)size_ - head_);
        int32_t used = temp >= size_ ? temp - size_ : temp;

        if (len > used) {
            return false;
        }

        if (head_ + len >= data_ + size_) {
            int32_t seg1 = (int32_t)(data_ + size_ - head_);
            int32_t seg2 = len - seg1;
            memcpy(buf, head_, seg1);
            memcpy(buf + seg1, data_, seg2);
            head_ = data_ + seg2;
        } else {
            memcpy(buf, head_, len);
            head_ += len;
        }

        return true;
    }
};

RingBuffer kcp_recv_buf_queue(1024 * 1024);
RingBuffer kcp_send_buf_queue(1024 * 1024);
std::unordered_map<uint64_t, Session> session_map;
std::mutex session_mutex;
//---------------------------------------------------------------

// 生成会话键�?
uint64_t session_key(const sockaddr_in& peer_addr) 
{
    return (static_cast<uint64_t>(peer_addr.sin_addr.s_addr) << 32) | peer_addr.sin_port;
    // return peer_addr.sin_port;
}

// 获取毫秒时间
uint32_t get_clock() 
{
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void printHead(const char* data, int32_t data_len, bool is_send, ikcpcb* kcp = nullptr)
{
        // const char* ptr = data;
        // IUINT32 ts, sn, una, conv, len;
		// IUINT16 wnd;
		// IUINT8 cmd, frg;

        // data = ikcp_decode32u(data, &conv);
		// data = ikcp_decode8u(data, &cmd);
		// data = ikcp_decode8u(data, &frg);
		// data = ikcp_decode16u(data, &wnd);
		// data = ikcp_decode32u(data, &ts);
		// data = ikcp_decode32u(data, &sn);
		// data = ikcp_decode32u(data, &una);
        // data = ikcp_decode32u(data, &len);

        // // if (is_send) {
        // //     cout << "SEND!!!" << "len:" << data_len <<  " data:" << string(ptr, data_len) << " conv:" << conv << " cmd:" << cmd << " frg:" << frg << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " len:" << len << endl;
        // // } else {
        // //     cout << "RECV!!!" << "len:" << data_len <<  " data:" << string(ptr, data_len) << " conv:" << conv << " cmd:" << cmd << " frg:" << frg << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " len:" << len << endl;
        // // }

        // if (is_send) {
        //     cout << "SEND!!!" << "len:" << data_len <<  " data:" << string(ptr + 24, data_len - 24) << " cmd:" << (uint32_t)cmd << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " snd_una:" << kcp->snd_una <<  " snd_nxt:" << kcp->snd_nxt << " rcv_nxt:" << kcp->rcv_nxt << endl; 
        // } else {
        //     cout << "RECV!!!" << "len:" << data_len <<  " data:" << string(ptr + 24, data_len - 24) << " cmd:" << (uint32_t)cmd << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " snd_una:" << kcp->snd_una <<  " snd_nxt:" << kcp->snd_nxt << " rcv_nxt:" << kcp->rcv_nxt << endl; 
        // }
}