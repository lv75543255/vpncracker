#include "udprepeator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <winsock2.h>
#define s_addr S_un.S_addr
#define gettimeofday mingw_gettimeofday
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET int
#define closesocket(s) close(s)
#endif

#define UDP_PACKSIZE_MAX 65535
#define UDP_CONECTION_PAIR_MAX 64
#define UDP_TIMEROUT_MSECONDS 60 * 1000
#define UDP_RECONNECT_MSECONDS 5 * 1000

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
    uint16_t timerin;
    uint16_t timerout;
    uint16_t flag_port;
    uint32_t flag_host;
    struct sockaddr_in  addr;
}UdpPair;

typedef struct UdpPairArray{
    int count;
    int capacity;
    UdpPair array[0];
}UdpPairArray;

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


UdpPairArray *UdpPairArray_new(int capacity)
{
    UdpPairArray *thiz;
    thiz = (UdpPairArray*)malloc(sizeof(UdpPairArray) + sizeof(UdpPair) * capacity);
    if(thiz != NULL)
    {
        thiz->count = 0;
        thiz->capacity = capacity;
        memset(thiz->array,0,sizeof(UdpPair) * capacity);
    }

    return thiz;
}


void UdpPairArray_destroy(UdpPairArray *thiz)
{
    free(thiz);
}

UdpPair *UdpPairArray_append(UdpPairArray *thiz,UdpPair *pair)
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
    printf("[%s:%d] count = %d!\n",__FUNCTION__,__LINE__,count);

    for (i = 0;i < count; ++i)
    {
        printf("[%s:%d] find %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(array[i].addr.sin_addr),htons(array[i].addr.sin_port));
        if (sockaddr_cmp(&(array[i].addr),paddr) == 0)
        {
            return array + i;
        }
    }
    printf("[%s:%d] not find %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(paddr->sin_addr),htons(paddr->sin_port));
    return NULL;
}

inline UdpPair *UdpPairArray_findByFlag(UdpPairArray *thiz,uint32_t host,uint16_t port)
{
    int i;
    const int count = thiz->count;
    UdpPair *array = thiz->array;
	printf("[%s:%d] count = %d, %x:%x!\n",__FUNCTION__,__LINE__,count,host,port);

    for (i = 0;i < count; ++i)
    {
		printf("[%s:%d] find %x:%x\n",__FUNCTION__,__LINE__,array[i].flag_host,array[i].flag_port);

        if (array[i].flag_port == port && array[i].flag_host == host)
        {
            return array + i;
        }
    }
	printf("[%s:%d] not find %x:%x\n",__FUNCTION__,__LINE__,host,port);
    return NULL;
}

//远端模式（根据超时情况，清理端口）
void UdpPairArray_processTimer_remote(UdpPairArray *thiz,int mseconds)
{

    int i = 0;
    int j = 0;
    const int count = thiz->count;
    UdpPair *array = thiz->array;

    while(i < count)
    {

        array[i].timerin += mseconds;
        if(array[i].timerin > UDP_TIMEROUT_MSECONDS)
        {
            printf("remove a left,timerout(%d:%d)\n",array[i].timerin,mseconds);
            ++i;
            --(thiz->count);
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
void UdpPairArray_processTimer_local(UdpPairArray *thiz,int mseconds)
{

    int i = 0;
    int j = 0;
    const int count = thiz->count;
    UdpPair *array = thiz->array;

    while(i < count)
    {
        array[i].timerin += mseconds;
        array[i].timerout += mseconds;
        if(array[i].timerin > UDP_TIMEROUT_MSECONDS)
        {
            printf("remove a left,because left->right timerout(%d:%d)\n",array[i].timerin,mseconds);
            ++i;
            --(thiz->count);
            continue;
        }
        else if(array[i].timerin < UDP_RECONNECT_MSECONDS && array[i].timerout > UDP_RECONNECT_MSECONDS)
        {
            printf("remove a left,because left<-right timerout(%d:%d)\n",array[i].timerout,mseconds);
            ++i;
            --(thiz->count);
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

struct UdpRepeator{
    UdpPairArray *udppairs;

    void (*processTimer)(UdpPairArray *thiz,int mseconds);
	void (*sendToLeft)(SOCKET sock,void *packet,char *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there);
	void (*sendToRight)(SOCKET sock,void *packet,char *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there);
	UdpPair* (*propareTransfer)(UdpPairArray * udppairs,struct sockaddr_in *addr, void *buffer);

    SOCKET sock;
    struct sockaddr_in target_addr;
};

void encrypto_send_local(SOCKET sock,void *head,char *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
	printf("[%s:%d] to %x:%x!\n",__FUNCTION__,__LINE__,there->sin_addr.s_addr,there->sin_port);

    UdpPacket *packet =(UdpPacket *) head;
    packet->header.random = rand() % 0x100;
	packet->header.port = here->sin_port;
	packet->header.host = here->sin_addr.s_addr;

	sendto(sock,packet, size + sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
}

void encrypto_send_remote(SOCKET sock,void *head,char *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
	printf("[%s:%d] to %x:%x!\n",__FUNCTION__,__LINE__,there->sin_addr.s_addr,there->sin_port);

	UdpPacket *packet =(UdpPacket *) head;
	packet->header.random = rand() % 0x100;
	packet->header.port = rand() % 0x100;
	packet->header.host = rand();

	sendto(sock,packet, size + sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
}
void decrypto_send(SOCKET sock,void *head,char *buffer,int size,struct sockaddr_in *here,struct sockaddr_in *there)
{
    UdpPacket *packet =(UdpPacket *) buffer;
	printf("[%s:%d] %s!\n",__FUNCTION__,__LINE__,packet->data);

	sendto(sock,packet->data,size - sizeof(packet->header) ,0,(struct sockaddr *)there,sizeof(*there));
}

UdpPair *UdpRepeator_prepareTansfer_remote(UdpPairArray * udppairs,struct sockaddr_in *addr, void *buffer)
{
    UdpPacket *packet =(UdpPacket *) buffer;
    UdpPair * pair = UdpPairArray_findByFlag(udppairs,packet->header.host,packet->header.port);
    if(pair == NULL)
    {
		printf("[%s:%d] start, new one %x:%x!\n",__FUNCTION__,__LINE__,packet->header.host,packet->header.port);
        UdpPair temp;
        temp.timerin = 0;
        temp.timerout = 0;
        temp.sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        temp.flag_host = packet->header.host;
		temp.flag_port = packet->header.port;

		memcpy(&temp.addr,addr,sizeof(*addr));
		pair = UdpPairArray_append(udppairs,&temp);
		printf("[%s:%d] end, new one %x:%x!\n",__FUNCTION__,__LINE__,pair->flag_host,pair->flag_port);

    }
	else
    {
		//
		printf("[%s:%d] find one!\n",__FUNCTION__,__LINE__);
		if(sockaddr_cmp(&pair->addr,addr) != 0)
		{
			printf("[%s:%d] find one,you change addr %s:%d!\n",__FUNCTION__,__LINE__,inet_ntoa(addr->sin_addr),htons(addr->sin_port));
			memcpy(&pair->addr,addr,sizeof(*addr));
		}
    }

    return pair;
}

UdpPair *UdpRepeator_prepareTansfer_local(UdpPairArray * udppairs,struct sockaddr_in *addr, void *buffer)
{
    UdpPair * pair = UdpPairArray_findByAddr(udppairs,addr);
    if(pair == NULL)
    {
        printf("[%s:%d] new one %s:%d\n",__FUNCTION__,__LINE__,inet_ntoa(addr->sin_addr),htons(addr->sin_port));
        UdpPair temp;
        temp.timerin = 0;
        temp.timerout = 0;
        temp.sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        memcpy(&temp.addr,addr,sizeof(*addr));

        pair = UdpPairArray_append(udppairs,&temp);
    }
    else
    {
        printf("[%s:%d] find one!\n",__FUNCTION__,__LINE__);
    }

    return pair;
}

UdpRepeator * UdpRepeator_new(int mode,const char *right_host,uint16_t right_port,const char *left_host,uint16_t left_port)
{
    UdpRepeator *       thiz;
    struct sockaddr_in  saddr;
#if defined(__MINGW32__) || defined(__MINGW64__)
    WSADATA             wsaData;
    WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
    srand(time(NULL));
	printf("[%s:%d] start %s:%d -> %s:%d\n",__FUNCTION__,__LINE__,right_host,right_port,left_host,left_port);

    thiz = (UdpRepeator *)malloc(sizeof(UdpRepeator));
    if(thiz == NULL)
    {
        printf("[%s:%d] Error malloc of UdpRepeator failed\n",__FUNCTION__,__LINE__);
        return NULL;
    }

    thiz->udppairs = UdpPairArray_new(UDP_CONECTION_PAIR_MAX);
    if(thiz->udppairs == NULL)
    {
        printf("[%s:%d] Error new of UdpPairArray failed\n",__FUNCTION__,__LINE__);
        free(thiz);
        return NULL;
    }


    thiz->sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(!(thiz->sock >0))
    {
        printf("[%s:%d] Error socket() failed\n",__FUNCTION__,__LINE__);

        UdpPairArray_destroy(thiz->udppairs);
        free(thiz);
        return NULL;
    }

    sockaddr_set(&saddr,inet_addr(right_host),htons(right_port));
    if(bind(thiz->sock,(struct sockaddr *)&saddr,sizeof(saddr)) != 0)
    {
        printf("[%s:%d] Error bind() failed\n",__FUNCTION__,__LINE__);
        UdpPairArray_destroy(thiz->udppairs);
        closesocket(thiz->sock);
        free(thiz);
        return NULL;
    }

    if(mode)
    {
        thiz->processTimer = UdpPairArray_processTimer_remote;
		thiz->sendToLeft = decrypto_send;
		thiz->sendToRight = encrypto_send_remote;
        thiz->propareTransfer = UdpRepeator_prepareTansfer_remote;
    }
    else
    {
        thiz->processTimer = UdpPairArray_processTimer_local;
		thiz->sendToLeft = encrypto_send_local;
		thiz->sendToRight = decrypto_send;
        thiz->propareTransfer = UdpRepeator_prepareTansfer_local;
    }

    sockaddr_set(&thiz->target_addr,inet_addr(left_host),htons(left_port));
    return thiz;
}

void UdpRepeator_destroy(UdpRepeator *thiz)
{
    if(thiz != NULL)
    {
        closesocket(thiz->sock);
        free(thiz);
#if defined(__MINGW32__) || defined(__MINGW64__)
        WSACleanup();
#endif
    }
}


int UdpRepeator_prepare_fdsets(UdpRepeator* thiz,fd_set *rfds)
{
    assert(thiz != NULL);
    int maxfd = thiz->sock;
    const int count = thiz->udppairs->count;
    UdpPair *array = thiz->udppairs->array;
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

int UdpRepeator_process_reightin(UdpRepeator* thiz,int ready,fd_set *prfds)
{
    assert(thiz != NULL);

    if(FD_ISSET(thiz->sock,prfds))
    {
        struct sockaddr_in  addr;
        int                 addr_len;
        int                 nbytes;
        UdpPacket packet;
        addr_len = sizeof(addr);
        memset(&addr,0,sizeof(addr));
        nbytes = recvfrom(thiz->sock,&packet.data,sizeof(packet.data),0,(struct sockaddr *)&addr,&addr_len);
        if(nbytes > 0)
        {
//			printf("message --> %d:%s\n",nbytes,packet.data);
            UdpPair * pair = thiz->propareTransfer(thiz->udppairs,&addr,&packet.data);
			if(pair)
			{
				pair->timerin = 0;
				thiz->sendToLeft(pair->sock,&packet,&packet.data,nbytes,&addr,&thiz->target_addr);
			}
        }

        --ready;
    }

    return ready;
}

int UdpRepeator_process_leftin(UdpRepeator* thiz,int ready,fd_set *prfds)
{
    assert(thiz != NULL);

    int i;
    int count = thiz->udppairs->count;
    UdpPair *array = thiz->udppairs->array;

    for(i = 0;i < count; ++i)
    {
        if(ready > 0 )
        {
//			printf("[%s:%d] <-- %d@%s:%d\n",__FUNCTION__,__LINE__,array[i].sock,inet_ntoa(array[i].addr.sin_addr),htons(array[i].addr.sin_port));
            if(FD_ISSET(array[i].sock,prfds))
            {
                int     nbytes;
                UdpPacket packet;
                nbytes = recvfrom(array[i].sock,packet.data,sizeof(packet.data),0,NULL,NULL);
                if(nbytes > 0)
                {
                    array[i].timerout = 0;
					thiz->sendToRight(thiz->sock,&packet,&packet.data,nbytes,NULL,&(array[i].addr));
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

int UdpRepeator_exec(UdpRepeator *thiz)
{
    int ready;
    int maxfd;
    int begin_timer;
    int end_timer;
    SOCKET server;
    UdpPairArray *udppairs;
    fd_set rfds;
    struct timeval time_out;;
    server = thiz->sock;
    udppairs = thiz->udppairs;

    for(;;)
    {
        FD_ZERO(&rfds);
        FD_SET(server,&rfds);
        // time_out 兼容 linux[in/out],此参数在windows作为常量传入[in]
        time_out.tv_sec = 1;
        time_out.tv_usec = 0;
        // maxfd 兼容 linux,此参数在windows中无效
        maxfd = UdpRepeator_prepare_fdsets(thiz,&rfds);
        begin_timer = gettimeofday_atmsecond();
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
            printf("--------------\n");
            printf("server  error,go out!");
            printf("--------------\n");
            return -1;
        }

        //超时处理，移除socket
        end_timer = gettimeofday_atmsecond();
        thiz->processTimer(udppairs,end_timer - begin_timer);
    }
    return 0;
}
