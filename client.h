#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <thread>
#include "ikcp.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 10000
#define WORKER_THREAD_COUNT 30
#define MAX_PACKET_SIZE 1400

using namespace std;
std::mutex mutex_;
int32_t kcp_conv_start = 1;

// 自定义毫秒级时钟
static inline uint32_t get_clock() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

int get_kcp_conv() {
    std::lock_guard<std::mutex> lock(mutex_);
    return kcp_conv_start++;
}

// 拆包器
int unpack(const char* buf, int32_t len, char* out_buf, int32_t* out_len) {
    if (len < 4) {
        return -1; // 数据长度不足，无法拆包
    }
}

// 打包器
uint32_t pack(const char* buf, int32_t len, char* out_buf) {
    memcpy(out_buf, &len, sizeof(len));
    memcpy(out_buf + sizeof(len), buf, len);
    return sizeof(len) + len;
}

void printHead(const char* data, int32_t data_len, bool is_send, ikcpcb* kcp)
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
    //     cout << "SEND!!!" << "len:" << data_len <<  " data:" << string(ptr + 24, data_len - 24) << " cmd:" << cmd << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " snd_una:" << kcp->snd_una <<  " snd_nxt:" << kcp->snd_nxt << " rcv_nxt:" << kcp->rcv_nxt << endl; 
    // } else {
    //     cout << "RECV!!!" << "len:" << data_len <<  " data:" << string(ptr + 24, data_len - 24) << " cmd:" << cmd << " wnd:" << wnd << " ts:" << ts << " sn:" << sn << " una:" << una << " snd_una:" << kcp->snd_una <<  " snd_nxt:" << kcp->snd_nxt << " rcv_nxt:" << kcp->rcv_nxt << endl; 
    // }
}