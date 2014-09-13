#include "udprepeator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <winsock2.h>
#define s_addr S_un.S_addr
#define gettimeofday mingw_gettimeofday
#else
#include <sys/socket.h>
#include <netinet/in.h>
#define SOCKET int
#define closesocket(s) close(s)
#endif

#define UDP_PACKSIZE_MAX 65535
#define UDP_CONECTION_PAIR_MAX 64
#define MAX_MSECONDS 60 * 1000
typedef struct UdpPair{
    SOCKET sock;
    uint16_t timer;
    uint16_t timerout;
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


UdpPair *UdpPairArray_findBySock(UdpPairArray *thiz,SOCKET fd)
{
    int i;
    int count = thiz->count;
    UdpPair *array = thiz->array;
    for(i = 0;i < count; ++i)
    {
        if(array[i].sock == fd )
        {
            return array + i;
        }
    }

    return NULL;
}


UdpPair *UdpPairArray_findByAddr(UdpPairArray *thiz,struct sockaddr_in *paddr)
{
    int i;
    int count = thiz->count;
    UdpPair *array = thiz->array;
    for (i = 0;i < count; ++i)
    {
        if (sockaddr_cmp(&(array[i].addr),paddr) == 0)
        {
            return array + i;
        }
    }
    return NULL;
}


void UdpPairArray_processTimer(UdpPairArray *thiz,int mseconds)
{

    int i = 0;
    int j = 0;
    const int count = thiz->count;
    UdpPair *array = thiz->array;

    while(i < count)
    {

        array[i].timer += mseconds;
        if(array[i].timer > MAX_MSECONDS)
        {
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
    SOCKET sock;
    struct sockaddr_in target_addr;
};

UdpRepeator * UdpRepeator_new(const char *host,uint16_t port)
{
    UdpRepeator *       thiz;
    struct sockaddr_in  saddr;
#if defined(__MINGW32__) || defined(__MINGW64__)
    WSADATA             wsaData;
    WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
    printf("[%s:%d] start %d\n",__FUNCTION__,__LINE__,sizeof(struct sockaddr_in));

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

    sockaddr_set(&saddr,inet_addr(host),port);
    if(bind(thiz->sock,(struct sockaddr *)&saddr,sizeof(saddr)) != 0)
    {
        printf("[%s:%d] Error bind() failed\n",__FUNCTION__,__LINE__);
        perror("what?");
        UdpPairArray_destroy(thiz->udppairs);
        closesocket(thiz->sock);
        free(thiz);
        return NULL;
    }

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
    char buf[65536];
    memset(&buf,0,sizeof(buf));
    if(FD_ISSET(thiz->sock,prfds))
    {
        struct sockaddr_in  addr;
        int                 addr_len;
        int                 nbytes;
        char                buf[0xff];
        addr_len = sizeof(addr);
        memset(&addr,0,sizeof(addr));
        nbytes = recvfrom(thiz->sock,buf,sizeof(buf),0,(struct sockaddr *)&addr,&addr_len);
        if(nbytes > 0)
        {
            printf("--------------\n");
            printf("server  error!");
            printf("--------------\n");

            UdpPair * pair = UdpPairArray_findByAddr(thiz->udppairs,&addr);
            if(pair == NULL)
            {
                UdpPair temp;
                temp.timer = 0;
                temp.sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
                memcpy(&temp.addr,&addr,sizeof(addr));

                pair = UdpPairArray_append(thiz->udppairs,&temp);
            }
            else
            {

            }

            sendto(pair->sock,buf,nbytes,0,(struct sockaddr *)&thiz->target_addr,sizeof(thiz->target_addr));
        }
        else
        {
            printf("================= %s\n",buf);
            //find then to send 1
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
            if(FD_ISSET(array[i].sock,prfds))
            {
                int     nbytes;
                char    buf[0xff];
                // recive data form 1,so we don't care addr
                nbytes = recvfrom(array[i].sock,buf,sizeof(buf),0,NULL,NULL);
                if(nbytes > 0)
                {
                    int                 addr_len;
                    addr_len = sizeof(array[i].addr);
                    sendto(thiz->sock,buf,nbytes,0,(struct sockaddr *)&(array[i].addr),addr_len);
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
        time_out.tv_sec = 1;
        time_out.tv_usec = 0;
        begin_timer = gettimeofday_atmsecond();
        maxfd = UdpRepeator_prepare_fdsets(thiz,&rfds);
        ready = select(maxfd,&rfds,NULL,NULL,&time_out);
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
        UdpPairArray_processTimer(udppairs,end_timer - begin_timer);
    }
    return 0;
}
