#include "udprepeator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <winsock2.h>
#define s_addr S_un.S_addr
#define gettimeofday mingw_gettimeofday
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define closesocket(s) close(s)
#endif

#define UNUSED(x) (void)x;
#define UDP_PACKSIZE_MAX 65535
#define UDP_CONECTION_PAIR_MAX 128
#define UDP_TIMEROUT_MSECONDS 60 * 1000
#define UDP_HEARTBETA_MSECONDS 1 * 1000
#define UDP_RECONNECT_MSECONDS 4 * 1000

typedef struct UdpHeader{
    uint16_t random;
    uint16_t port;
    uint32_t host;
}UdpHeader;

typedef struct UdpPacket{
    UdpHeader header;
    uint8_t data[UDP_PACKSIZE_MAX -sizeof(UdpHeader)];
}UdpPacket;

typedef struct UdpPair{
    SOCKET sock;
	uint16_t timer_leftin;
	uint16_t timer_rightin;
    uint16_t flag_port;
    uint32_t flag_host;
    struct sockaddr_in  addr;
}UdpPair;

typedef struct UdpPairArray{
    int count;
    int capacity;
    UdpPair array[0];
}UdpPairArray;

static int g_debug = 0;

void UdpRepeator_setDebug(int debug)
{
   g_debug = debug;
}

static inline int hDebug(const char*format,...)
{
    if(g_debug)
    {
        int ret;
        va_list args;
        va_start(args, format);
        ret = vfprintf(stdout,format,args);
        va_end(args);
        return ret;
    }

    return 0;
}

static inline int hError(const char*format,...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = vfprintf(stderr,format,args);
    va_end(args);
    return ret;
}

static inline int gettimeofday_atmsecond()
{
    struct timeval tv = {0,0};
    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000 + tv.tv_usec /1000;
}

static inline int sockaddr_cmp(const struct sockaddr_in *addr,const struct sockaddr_in *that)
{
    if((addr->sin_family == that->sin_family)
            &&(addr->sin_port == that->sin_port)
            &&(addr->sin_addr.s_addr == that->sin_addr.s_addr))
    {
        return 0;
    }

    return -1;
}

static inline void sockaddr_set(struct sockaddr_in *addr,uint32_t host,uint16_t port)
{
    memset(addr,0,sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = port;
    addr->sin_addr.s_addr = host;
}

static inline void sockaddr_get(struct sockaddr_in *addr,uint32_t *phost,uint16_t *pport)
{
    *pport = addr->sin_port;
    *phost = addr->sin_addr.s_addr;
}


static void UdpPairArray_init(UdpPairArray *thiz,int capacity)
{ 
        thiz->count = 0;
        thiz->capacity = capacity;
        memset(thiz->array,0,sizeof(UdpPair) * capacity);
}

static UdpPair *UdpPairArray_append(UdpPairArray *thiz,UdpPair *pair)
{
    UdpPair *temp = NULL;

    if(thiz->count < thiz->capacity)
    {
        temp = thiz->array + thiz->count;
        memcpy(temp,pair,sizeof(*pair));

        thiz->count++;
        return temp;
    }

    return temp;
}


inline UdpPair *UdpPairArray_findByAddr(UdpPairArray *thiz,struct sockaddr_in *paddr)
{
    int i;
    const int count = thiz->count;
    UdpPair *array = thiz->array;
    hDebug("[%s:%d] count = %d!\n",__FUNCTION__,__LINE__,count);

    for (i = 0;i < count; ++i)
    {
        hDebug("[%s:%d] find %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(array[i].addr.sin_addr),htons(array[i].addr.sin_port));
        if (sockaddr_cmp(&(array[i].addr),paddr) == 0)
        {
            return array + i;
        }
    }
    hDebug("[%s:%d] not find %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(paddr->sin_addr),htons(paddr->sin_port));
    return NULL;
}

inline UdpPair *UdpPairArray_findByFlag(UdpPairArray *thiz,uint32_t host,uint16_t port)
{
    int i;
    const int count = thiz->count;
    UdpPair *array = thiz->array;
    hDebug("[%s:%d] count = %d, %x:%x!\n",__FUNCTION__,__LINE__,count,host,port);

    for (i = 0;i < count; ++i)
    {
        hDebug("[%s:%d] find %x:%x\n",__FUNCTION__,__LINE__,array[i].flag_host,array[i].flag_port);

        if (array[i].flag_port == port && array[i].flag_host == host)
        {
            return array + i;
        }
    }
    hDebug("[%s:%d] not find %x:%x\n",__FUNCTION__,__LINE__,host,port);
    return NULL;
}

// N --> reaplear
struct UdpRepeator{
    void (*processTimer)(UdpRepeator *thiz,int mseconds);
    void (*sendToLeft)(UdpRepeator *thiz,SOCKET sock,void *packet,void *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there);
    void (*sendToRight)(UdpRepeator *thiz,SOCKET sock,void *packet,void *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there);
	UdpPair* (*propareTransfer)(UdpRepeator * thiz,struct sockaddr_in *addr, void *buffer);

    SOCKET sock;
    struct sockaddr_in source_addr;
    struct sockaddr_in target_addr;
	UdpPairArray udppairs;
};

//远端模式（根据超时情况，清理端口）
static void UdpRepeator_processTimer_remote(UdpRepeator *thiz,int mseconds)
{

    int i = 0;
    int j = 0;
	UdpPairArray *udppairs = &thiz->udppairs;
    const int count = udppairs->count;
    UdpPair *array = udppairs->array;

    while(i < count)
    {

		array[i].timer_leftin += mseconds;
		if(array[i].timer_leftin > UDP_TIMEROUT_MSECONDS)
        {
			hDebug("remove a left,timerout(%d:%d)\n",array[i].timer_leftin,mseconds);
            ++i;
            --(udppairs->count);
            continue;
        }

        if(i > j)
        {
            memcpy(array+j,array+i,sizeof(UdpPair));
        }

        ++i;
        ++j;
    }
}


//近端模式切换端口，（根据超时情况，清理端口）
static void UdpRepeator_processTimer_local(UdpRepeator *thiz,int mseconds)
{

    int i = 0;
    int j = 0;
	UdpPairArray *udppairs = &thiz->udppairs;
    const int count = udppairs->count;
    UdpPair *array = udppairs->array;

    while(i < count)
    {
		array[i].timer_leftin += mseconds;
		array[i].timer_rightin += mseconds;
        //超过一段时间没有，发送将关闭
		if(array[i].timer_leftin > UDP_TIMEROUT_MSECONDS)
        {
			hError("remove a left,because left->right timerout(%d:%d)\n",array[i].timer_leftin,mseconds);
            ++i;
            --(udppairs->count);
            continue;
        }
        //发送完后，超过一段时间没有收到将关闭

		if(array[i].timer_rightin > UDP_RECONNECT_MSECONDS)
        {
			hError("remove a left,because left<-right timerout(%d:%d)\n",array[i].timer_rightin,mseconds);
            ++i;
            --(udppairs->count);
            continue;
        }

		if(array[i].timer_leftin > UDP_HEARTBETA_MSECONDS)
        {
			hError("send heart beat,because left->right timerout(%d:%d)\n",array[i].timer_leftin,mseconds);
            UdpPacket packet;
            thiz->sendToLeft(thiz,array[i].sock,&packet,&packet.data,0,&(array[i].addr),&(thiz->target_addr));
        }


        if(i > j)
        {
            memcpy(array+j,array+i,sizeof(UdpPair));
        }

        ++i;
        ++j;
    }
}

static inline void encrypto_method(void *data,int size)
{
	int i;
	unsigned char temp;
	unsigned char* buffer = (unsigned char*)data;
	const int count = size - 3;
	for (i  = 0; i < count; i += 4)
	{
		temp = buffer[i];
		buffer[i] = temp << 5 | temp >> 3;

		temp = buffer[i+1];
		buffer[i+1] = buffer[i+2];
		buffer[i+2] = temp;

		buffer[i+3] = ~buffer[i+3];
	}
}

static inline void decrypto_method(void *data,int size)
{
	int i;
	unsigned char temp;
	unsigned char* buffer = (unsigned char*)data;
	const int count = size - 3;
	for (i  = 0; i < count; i += 4)
	{
		temp = buffer[i];
		buffer[i] = temp >> 5 | temp << 3;

		temp = buffer[i+1];
		buffer[i+1] = buffer[i+2];
		buffer[i+2] = temp;

		buffer[i+3] = ~buffer[i+3];
	}
}

static void UdpRepeator_encrypto_send(UdpRepeator *thiz,SOCKET sock,void *head,void *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
    hDebug("[%s:%d] to %x:%d\n",__FUNCTION__,__LINE__,there->sin_addr.s_addr,htons(there->sin_port));
	UNUSED(thiz);
    UdpPacket *packet =(UdpPacket *) head;
    packet->header.random = rand() % 0x100;
    if(here)
    {
        packet->header.port = here->sin_port;
        packet->header.host = here->sin_addr.s_addr;
    }
    else
    {
        packet->header.port = rand() % 0x100;
        packet->header.host = rand();
    }

	encrypto_method(packet,size + sizeof(packet->header));

    sendto(sock,packet, size + sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
}

static void UdpRepeator_decrypto_send_remote(UdpRepeator *thiz,SOCKET sock,void *head,void *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
	UNUSED(head)
    UdpPacket *packet =(UdpPacket *) buffer;

    hDebug("[%s:%d] to %x:%d\n",__FUNCTION__,__LINE__,there->sin_addr.s_addr,htons(there->sin_port));
	decrypto_method(packet,size);

    if(size - sizeof(packet->header) > 0)
    {
        sendto(sock,packet->data,size - sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
    }
    else
    {
        UdpHeader header;
        header.random = rand() % 0x100;
        header.port = rand() % 0x100;
        header.host = rand();
		encrypto_method(&header,sizeof(header));
        sendto(thiz->sock,&header,sizeof(header) ,0,(struct sockaddr *)here,sizeof(*here));
        hDebug("[%s:%d] heart beat process,%x:%d\n",__FUNCTION__,__LINE__,here->sin_addr.s_addr,htons(here->sin_port));
    }
}

static void UdpRepeator_decrypto_send_local(UdpRepeator *thiz,SOCKET sock,void *head,void *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
	UNUSED(thiz)
	UNUSED(head)
	UNUSED(here)
    UdpPacket *packet =(UdpPacket *) buffer;

    hDebug("[%s:%d] to %x:%p\n",__FUNCTION__,__LINE__,there->sin_addr.s_addr,htons(there->sin_port));
	decrypto_method(packet,size);
    if(size - sizeof(packet->header) > 0)
    {
        sendto(sock,packet->data,size - sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
    }
    else
    {
        hDebug("[%s:%d] heart beat ignore\n",__FUNCTION__,__LINE__);
    }
}


static UdpPair *UdpRepeator_prepareTansfer_remote(UdpRepeator * thiz,struct sockaddr_in *addr, void *buffer)
{
	UdpPairArray * udppairs = &(thiz->udppairs);
    UdpPacket *packet =(UdpPacket *) buffer;
    UdpPair * pair = UdpPairArray_findByFlag(udppairs,packet->header.host,packet->header.port);
    if(pair == NULL)
    {
        hDebug("[%s:%d] start, new one %x:%x!\n",__FUNCTION__,__LINE__,packet->header.host,packet->header.port);
        UdpPair temp;
		temp.timer_leftin = 0;
		temp.timer_rightin = 0;
        temp.sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        temp.flag_host = packet->header.host;
        temp.flag_port = packet->header.port;

        memcpy(&temp.addr,addr,sizeof(*addr));
        pair = UdpPairArray_append(udppairs,&temp);
        hDebug("[%s:%d] end, new one %x:%x!\n",__FUNCTION__,__LINE__,pair->flag_host,pair->flag_port);

    }
    else
    {
        //
        hDebug("[%s:%d] find one!\n",__FUNCTION__,__LINE__);
        if(sockaddr_cmp(&pair->addr,addr) != 0)
        {
			hError("[%s:%d] find one,you change addr %s:%d!\n",__FUNCTION__,__LINE__,inet_ntoa(addr->sin_addr),htons(addr->sin_port));
            memcpy(&pair->addr,addr,sizeof(*addr));
        }
    }

    return pair;
}

static UdpPair *UdpRepeator_prepareTansfer_local(UdpRepeator * thiz,struct sockaddr_in *addr, void *buffer)
{
	UNUSED(buffer);
	UdpPairArray * udppairs = &(thiz->udppairs);
    UdpPair * pair = UdpPairArray_findByAddr(udppairs,addr);
    if(pair == NULL)
    {
        hDebug("[%s:%d] new one %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(addr->sin_addr),htons(addr->sin_port));
        UdpPair temp;
		temp.timer_leftin = 0;
		temp.timer_rightin = 0;
        temp.sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        memcpy(&temp.addr,addr,sizeof(*addr));

        pair = UdpPairArray_append(udppairs,&temp);
    }
    else
    {
        hDebug("[%s:%d] find one!\n",__FUNCTION__,__LINE__);
    }

    return pair;
}

UdpRepeator * UdpRepeator_new()
{
    UdpRepeator *       thiz;
#if defined(__MINGW32__) || defined(__MINGW64__)
    WSADATA             wsaData;
    WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
    srand(time(NULL));

	thiz = (UdpRepeator *)malloc(sizeof(UdpRepeator)+sizeof(UdpPairArray) + sizeof(UdpPair) * UDP_CONECTION_PAIR_MAX);
	if(thiz != NULL)
    {
		UdpPairArray_init(&thiz->udppairs,UDP_CONECTION_PAIR_MAX);
    }
	else
	{
		hError("[%s:%d] Error malloc of UdpRepeator failed\n",__FUNCTION__,__LINE__);
	}

    return thiz;
}

void UdpRepeator_setSource(UdpRepeator *thiz,const char *host,uint16_t port)
{
    assert(thiz != NULL);
    hDebug("[%s:%d] %s:%d --->\n",__FUNCTION__,__LINE__,host,port);
    sockaddr_set(&thiz->source_addr,inet_addr(host),htons(port));
}

void UdpRepeator_setDestination(UdpRepeator *thiz,const char *host,uint16_t port)
{
    assert(thiz != NULL);
    hDebug("[%s:%d] ---> %s:%d\n",__FUNCTION__,__LINE__,host,port);
    sockaddr_set(&thiz->target_addr,inet_addr(host),htons(port));
}

void UdpRepeator_setRepeatorMode(UdpRepeator *thiz,int isRepeat2Local)
{
	assert(thiz != NULL);
    if(isRepeat2Local)
    {
        thiz->processTimer = UdpRepeator_processTimer_remote;
        thiz->sendToLeft = UdpRepeator_decrypto_send_remote;
        thiz->sendToRight = UdpRepeator_encrypto_send;
        thiz->propareTransfer = UdpRepeator_prepareTansfer_remote;
    }
    else
    {
        thiz->processTimer = UdpRepeator_processTimer_local;
        thiz->sendToLeft = UdpRepeator_encrypto_send;
        thiz->sendToRight = UdpRepeator_decrypto_send_local;
        thiz->propareTransfer = UdpRepeator_prepareTansfer_local;
    }
}

void UdpRepeator_destroy(UdpRepeator *thiz)
{
    if(thiz != NULL)
    {
        if(thiz->sock > 0)
        {
            closesocket(thiz->sock);
        }

        free(thiz);
#if defined(__MINGW32__) || defined(__MINGW64__)
        WSACleanup();
#endif
    }
}


static int UdpRepeator_prepare_fdsets(UdpRepeator* thiz,fd_set *rfds)
{
    assert(thiz != NULL);
    int maxfd = thiz->sock;
	const int count = thiz->udppairs.count;
	UdpPair *array = thiz->udppairs.array;
    int i;
    for(i = 0;i < count; ++i)
    {
        FD_SET(array[i].sock,rfds);
        if(array[i].sock > maxfd )
        {
            maxfd = array[i].sock;
        }
    }

    return maxfd;
}

static int UdpRepeator_process_reightin(UdpRepeator* thiz,int ready,fd_set *prfds)
{
    assert(thiz != NULL);

    if(FD_ISSET(thiz->sock,prfds))
    {
        struct sockaddr_in  addr;
		socklen_t           addr_len;
        int                 nbytes;
        UdpPacket packet;
        addr_len = sizeof(addr);
        memset(&addr,0,sizeof(addr));
        nbytes = recvfrom(thiz->sock,&packet.data,sizeof(packet.data),0,(struct sockaddr *)&addr,&addr_len);
        if(nbytes > 0)
        {
//			hDebug("message --> %d:%s\n",nbytes,packet.data);
			UdpPair * pair = thiz->propareTransfer(thiz,&addr,&packet.data);
            if(pair)
            {
				pair->timer_leftin = 0;
                thiz->sendToLeft(thiz,pair->sock,&packet,&packet.data,nbytes,&addr,&thiz->target_addr);
            }
        }

        --ready;
    }

    return ready;
}

static int UdpRepeator_process_leftin(UdpRepeator* thiz,int ready,fd_set *prfds)
{
    assert(thiz != NULL);

    int i;
	int count = thiz->udppairs.count;
	UdpPair *array = thiz->udppairs.array;

    for(i = 0;i < count; ++i)
    {
        if(ready > 0 )
        {
//			hDebug("[%s:%d] <-- %d@%s:%d\n",__FUNCTION__,__LINE__,array[i].sock,inet_ntoa(array[i].addr.sin_addr),htons(array[i].addr.sin_port));
            if(FD_ISSET(array[i].sock,prfds))
            {
                int     nbytes;
                UdpPacket packet;
                nbytes = recvfrom(array[i].sock,packet.data,sizeof(packet.data),0,NULL,NULL);
                if(nbytes > 0)
                {
					array[i].timer_rightin = 0;
                    thiz->sendToRight(thiz,thiz->sock,&packet,&packet.data,nbytes,NULL,&(array[i].addr));
                }

                --ready;
            }
        }
        else
        {
            break;
        }

    }

    return 0;
}


static int UdpRepeator_prepare(UdpRepeator *thiz)
{
    if (thiz == NULL)
    {
        return -1;
    }

    if (thiz->processTimer == NULL
            || thiz->sendToLeft == NULL
            || thiz->sendToRight == NULL
            || thiz->propareTransfer == NULL
            )
    {
		hError("[%s:%d] Error invalid repeator mode\n",__FUNCTION__,__LINE__);
        return -1;
    }

    thiz->sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (!(thiz->sock > 0))
    {
		hError("[%s:%d] Error socket() failed\n",__FUNCTION__,__LINE__);
        return -1;
    }

    if (bind(thiz->sock,(struct sockaddr *)&thiz->source_addr,sizeof(thiz->source_addr)) != 0)
    {
		hError("[%s:%d] Error bind() failed\n",__FUNCTION__,__LINE__);
        closesocket(thiz->sock);
        thiz->sock = 0;
        return -1;
    }


    return 0;
}

int UdpRepeator_exec(UdpRepeator *thiz)
{
    if(UdpRepeator_prepare(thiz) == 0)
    {
        int ready;
        int maxfd;
        int begin_timer;
        int end_timer;
        SOCKET server;
        UdpPairArray *udppairs;
        fd_set rfds;
		struct timeval time_out;
		begin_timer = 0;
		end_timer = 0;
        server = thiz->sock;
		udppairs = &thiz->udppairs;

        for(;;)
        {
            FD_ZERO(&rfds);
            FD_SET(server,&rfds);
            // time_out 兼容 linux[in/out],此参数在windows作为常量传入[in]
            time_out.tv_sec = 1;
            time_out.tv_usec = 0;
            // maxfd 兼容 linux,此参数在windows中无效
            maxfd = UdpRepeator_prepare_fdsets(thiz,&rfds);
			begin_timer = begin_timer > 0 ? begin_timer : gettimeofday_atmsecond();
            ready = select(maxfd + 1,&rfds,NULL,NULL,&time_out);
            if(ready > 0)
            {
                //N --> 1
                ready = UdpRepeator_process_reightin(thiz,ready,&rfds);
                //N <-- 1
                if(ready > 0)
                {
                    UdpRepeator_process_leftin(thiz,ready,&rfds);
                }
            }
            else if(ready < 0)
            {
				hError("--------------\n");
				hError("server  error,go out!");
				hError("--------------\n");
                return -1;
            }

            //超时处理，移除socket
            end_timer = gettimeofday_atmsecond();
            if(end_timer - begin_timer > 100)
            {
                thiz->processTimer(thiz,end_timer - begin_timer);
				begin_timer = 0;
            }
        }

        return 0;
    }
    else
    {
        return -1;
    }
}



