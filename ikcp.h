//=====================================================================
//
// KCP - A Better ARQ Protocol Implementation
// skywind3000 (at) gmail.com, 2010-2011
//  
// Features:
// + Average RTT reduce 30% - 40% vs traditional ARQ like tcp.
// + Maximum RTT reduce three times vs tcp.
// + Lightweight, distributed as a single source file.
//
//=====================================================================
#ifndef __IKCP_H__
#define __IKCP_H__

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


//=====================================================================
// 32BIT INTEGER DEFINITION 
//=====================================================================
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__
#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(_M_AMD64)
	typedef unsigned int ISTDUINT32;
	typedef int ISTDINT32;
#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
	typedef unsigned long ISTDUINT32;
	typedef long ISTDINT32;
#elif defined(__MACOS__)
	typedef UInt32 ISTDUINT32;
	typedef SInt32 ISTDINT32;
#elif defined(__APPLE__) && defined(__MACH__)
	#include <sys/types.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif defined(__BEOS__)
	#include <sys/inttypes.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif (defined(_MSC_VER) || defined(__BORLANDC__)) && (!defined(__MSDOS__))
	typedef unsigned __int32 ISTDUINT32;
	typedef __int32 ISTDINT32;
#elif defined(__GNUC__)
	#include <stdint.h>
	typedef uint32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#else 
	typedef unsigned long ISTDUINT32; 
	typedef long ISTDINT32;
#endif
#endif


//=====================================================================
// Integer Definition
//=====================================================================
#ifndef __IINT8_DEFINED
#define __IINT8_DEFINED
typedef char IINT8;
#endif

#ifndef __IUINT8_DEFINED
#define __IUINT8_DEFINED
typedef unsigned char IUINT8;
#endif

#ifndef __IUINT16_DEFINED
#define __IUINT16_DEFINED
typedef unsigned short IUINT16;
#endif

#ifndef __IINT16_DEFINED
#define __IINT16_DEFINED
typedef short IINT16;
#endif

#ifndef __IINT32_DEFINED
#define __IINT32_DEFINED
typedef ISTDINT32 IINT32;
#endif

#ifndef __IUINT32_DEFINED
#define __IUINT32_DEFINED
typedef ISTDUINT32 IUINT32;
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IUINT64;
#else
typedef unsigned long long IUINT64;
#endif
#endif

#ifndef INLINE
#if defined(__GNUC__)

#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define INLINE         __inline__ __attribute__((always_inline))
#else
#define INLINE         __inline__
#endif

#elif (defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE 
#endif
#endif

#if (!defined(__cplusplus)) && (!defined(inline))
#define inline INLINE
#endif


//=====================================================================
// QUEUE DEFINITION                                                  
//=====================================================================
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


//---------------------------------------------------------------------
// queue init                                                         
//---------------------------------------------------------------------
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


//---------------------------------------------------------------------
// queue operation                     
//---------------------------------------------------------------------
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )
	

#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)


#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

#endif


//---------------------------------------------------------------------
// BYTE ORDER & ALIGNMENT
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MIPSEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif

#ifndef IWORDS_MUST_ALIGN
	#if defined(__i386__) || defined(__i386) || defined(_i386_)
		#define IWORDS_MUST_ALIGN 0
	#elif defined(_M_IX86) || defined(_X86_) || defined(__x86_64__)
		#define IWORDS_MUST_ALIGN 0
	#elif defined(__amd64) || defined(__amd64__)
		#define IWORDS_MUST_ALIGN 0
	#else
		#define IWORDS_MUST_ALIGN 1
	#endif
#endif

//---------------------------------------------------------------------
// encode / decode
//---------------------------------------------------------------------

/* encode 8 bits unsigned int */
static inline char *ikcp_encode8u(char *p, unsigned char c)
{
	*(unsigned char*)p++ = c;
	return p;
}

/* decode 8 bits unsigned int */
static inline const char *ikcp_decode8u(const char *p, unsigned char *c)
{
	*c = *(unsigned char*)p++;
	return p;
}

/* encode 16 bits unsigned int (lsb) */
static inline char *ikcp_encode16u(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*(unsigned char*)(p + 0) = (w & 255);
	*(unsigned char*)(p + 1) = (w >> 8);
#else
	memcpy(p, &w, 2);
#endif
	p += 2;
	return p;
}

/* decode 16 bits unsigned int (lsb) */
static inline const char *ikcp_decode16u(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*w = *(const unsigned char*)(p + 1);
	*w = *(const unsigned char*)(p + 0) + (*w << 8);
#else
	memcpy(w, p, 2);
#endif
	p += 2;
	return p;
}

/* encode 32 bits unsigned int (lsb) */
static inline char *ikcp_encode32u(char *p, IUINT32 l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*(unsigned char*)(p + 0) = (unsigned char)((l >>  0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
	memcpy(p, &l, 4);
#endif
	p += 4;
	return p;
}

/* decode 32 bits unsigned int (lsb) */
static inline const char *ikcp_decode32u(const char *p, IUINT32 *l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*l = *(const unsigned char*)(p + 3);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	memcpy(l, p, 4);
#endif
	p += 4;
	return p;
}

static inline IUINT32 _imin_(IUINT32 a, IUINT32 b) {
	return a <= b ? a : b;
}

static inline IUINT32 _imax_(IUINT32 a, IUINT32 b) {
	return a >= b ? a : b;
}

static inline IUINT32 _ibound_(IUINT32 lower, IUINT32 middle, IUINT32 upper) 
{
	return _imin_(_imax_(lower, middle), upper);
}

static inline long _itimediff(IUINT32 later, IUINT32 earlier) 
{
	return ((IINT32)(later - earlier));
}

//=====================================================================
// KCP BASIC
//=====================================================================
const IUINT32 IKCP_RTO_NDL = 30;		// no delay min rto
const IUINT32 IKCP_RTO_MIN = 100;		// normal min rto
const IUINT32 IKCP_RTO_DEF = 200;
const IUINT32 IKCP_RTO_MAX = 60000;
const IUINT32 IKCP_CMD_PUSH = 81;		// cmd: push data
const IUINT32 IKCP_CMD_ACK  = 82;		// cmd: ack
const IUINT32 IKCP_CMD_WASK = 83;		// cmd: window probe (ask)
const IUINT32 IKCP_CMD_WINS = 84;		// cmd: window size (tell)
const IUINT32 IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK
const IUINT32 IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS
const IUINT32 IKCP_WND_SND = 32;
const IUINT32 IKCP_WND_RCV = 128;       // must >= max fragment size
const IUINT32 IKCP_MTU_DEF = 1400;
const IUINT32 IKCP_ACK_FAST	= 3;
const IUINT32 IKCP_INTERVAL	= 100;
const IUINT32 IKCP_OVERHEAD = 24;
const IUINT32 IKCP_DEADLINK = 20;
const IUINT32 IKCP_THRESH_INIT = 2;
const IUINT32 IKCP_THRESH_MIN = 2;
const IUINT32 IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size
const IUINT32 IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window
const IUINT32 IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack


//=====================================================================
// SEGMENT
//=====================================================================
struct IKCPSEG
{
	struct IQUEUEHEAD node;
	IUINT32 conv;
	IUINT32 cmd;
	IUINT32 frg;
	IUINT32 wnd; // window, 可用窗口大小
	IUINT32 ts; // timestamp, 发送的时间戳
	IUINT32 sn; // sequence number, segment 序号
	IUINT32 una; // unacknowledged, 当前未收到的序号: 也代表这个序号之前的包均收到
	IUINT32 len; // length, 数据长度, 不是总包长度
	IUINT32 resendts; // 超时重发的时间戳
	IUINT32 rto; // 超时重传的时间间隔
	IUINT32 fastack; // ack跳过的次数，用于快速重传。在每帧调用input时，没有该seg的ack包，则fastack++。
	IUINT32 xmit; // 发送的次数(即重传的次数)
	char data[1];
};


//---------------------------------------------------------------------
// IKCPCB
//---------------------------------------------------------------------
struct IKCPCB
{
	IUINT32 conv, mtu, mss, state;
	// sun_una：第一个未确认的包，sen_nxt：待发送包的序号，rcv_nxt：待接收消息的序号
	IUINT32 snd_una, snd_nxt, rcv_nxt;
	IUINT32 ts_recent, ts_lastack, ssthresh;
	// rx_rttval：ack接收rtt浮动值，rx_srtt：ack接收rtt平滑值(smoothed)，rx_rto：由ack接收延迟计算出来的复原时间，rx_minrto：最小复原时间,rto最少为rx_srtt + rx_minrto
	IINT32 rx_rttval, rx_srtt, rx_rto, rx_minrto;
	IUINT32 snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
	IUINT32 current, interval, ts_flush, xmit;
	IUINT32 nrcv_buf, nsnd_buf;
	IUINT32 nrcv_que, nsnd_que;
	IUINT32 nodelay, updated;
	IUINT32 ts_probe, probe_wait;
	// dead_link：最大重传次数，incr：可发送的最大数据量，一般情况下，incr=cwnd*mss，当cwnd超过ssthresh时，incr就不是mss整数倍了，累加一个小于mss的数值
	IUINT32 dead_link, incr;
	struct IQUEUEHEAD snd_queue;
	struct IQUEUEHEAD rcv_queue;
	struct IQUEUEHEAD snd_buf;
	struct IQUEUEHEAD rcv_buf;
	IUINT32 *acklist;	// 待发送的ack的列表，里面的元素是sn1,timestamp1,sn2,timestamp2 ……
	IUINT32 ackcount;
	IUINT32 ackblock;	// acklist的大小
	void *user;
	char *buffer;
	int fastresend;
	int fastlimit;
	int nocwnd, stream;	// nocwnd：取消拥塞控制，取消后cwnd和ssthresh就不起作用了  stream：是否采用流传输模式
	int logmask;
	int (*output)(const char *buf, int len, struct IKCPCB *kcp, void *user);
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};


typedef struct IKCPCB ikcpcb;

#define IKCP_LOG_OUTPUT			1
#define IKCP_LOG_INPUT			2
#define IKCP_LOG_SEND			4
#define IKCP_LOG_RECV			8
#define IKCP_LOG_IN_DATA		16
#define IKCP_LOG_IN_ACK			32
#define IKCP_LOG_IN_PROBE		64
#define IKCP_LOG_IN_WINS		128
#define IKCP_LOG_OUT_DATA		256
#define IKCP_LOG_OUT_ACK		512
#define IKCP_LOG_OUT_PROBE		1024
#define IKCP_LOG_OUT_WINS		2048

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// interface
//---------------------------------------------------------------------

// create a new kcp control object, 'conv' must equal in two endpoint
// from the same connection. 'user' will be passed to the output callback
// output callback can be setup like this: 'kcp->output = my_udp_output'
ikcpcb* ikcp_create(IUINT32 conv, void *user);

// release kcp control object
void ikcp_release(ikcpcb *kcp);

// set output callback, which will be invoked by kcp
void ikcp_setoutput(ikcpcb *kcp, int (*output)(const char *buf, int len, 
	ikcpcb *kcp, void *user));

// user/upper level recv: returns size, returns below zero for EAGAIN
int ikcp_recv(ikcpcb *kcp, char *buffer, int len);

// user/upper level send, returns below zero for error
int ikcp_send(ikcpcb *kcp, const char *buffer, int len);

// update state (call it repeatedly, every 10ms-100ms), or you can ask 
// ikcp_check when to call it again (without ikcp_input/_send calling).
// 'current' - current timestamp in millisec. 
void ikcp_update(ikcpcb *kcp, IUINT32 current);

// Determine when should you invoke ikcp_update:
// returns when you should invoke ikcp_update in millisec, if there 
// is no ikcp_input/_send calling. you can call ikcp_update in that
// time, instead of call update repeatly.
// Important to reduce unnacessary ikcp_update invoking. use it to 
// schedule ikcp_update (eg. implementing an epoll-like mechanism, 
// or optimize ikcp_update when handling massive kcp connections)
IUINT32 ikcp_check(const ikcpcb *kcp, IUINT32 current);

// when you received a low level packet (eg. UDP packet), call it
int ikcp_input(ikcpcb *kcp, const char *data, long size);

// flush pending data
void ikcp_flush(ikcpcb *kcp);

// check the size of next message in the recv queue
int ikcp_peeksize(const ikcpcb *kcp);

// change MTU size, default is 1400
int ikcp_setmtu(ikcpcb *kcp, int mtu);

// set maximum window size: sndwnd=32, rcvwnd=32 by default
int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);

// get how many packet is waiting to be sent
int ikcp_waitsnd(const ikcpcb *kcp);

// fastest: ikcp_nodelay(kcp, 1, 20, 2, 1)
// nodelay: 0:disable(default), 1:enable
// interval: internal update timer interval in millisec, default is 100ms 
// resend: 0:disable fast resend(default), 1:enable fast resend
// nc: 0:normal congestion control(default), 1:disable congestion control
int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc);


void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...);

// setup allocator
void ikcp_allocator(void* (*new_malloc)(size_t), void (*new_free)(void*));

// read conv
IUINT32 ikcp_getconv(const void *ptr);


#ifdef __cplusplus
}
#endif

#endif


