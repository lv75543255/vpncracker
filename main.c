#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include "udprepeator.h"

int util_parse_arguments(UdpRepeator *repeator,int argc,char *argv[]);
int util_load_settings(UdpRepeator *repeator,int argc,char *argv[]);
int util_help_useage(char *path);

int main(int argc,char *argv[])
{
    int ret;
    UdpRepeator *repeator;
    repeator = UdpRepeator_new();


    util_parse_arguments(repeator,argc,argv);

    if(argc < 2)
    {
        printf("start local repeator\n");
        UdpRepeator_setSource(repeator,"127.0.0.1",1194);
        UdpRepeator_setDestination(repeator,"54.64.132.2",1201);
        UdpRepeator_setRepeatorMode(repeator,0);
    }
    else
    {
        printf("start remote repeator\n");
        UdpRepeator_setSource(repeator,"0.0.0.0",1201);
        UdpRepeator_setDestination(repeator,"127.0.0.1",11194);
        UdpRepeator_setRepeatorMode(repeator,1);
    }

    ret = UdpRepeator_exec(repeator);
    UdpRepeator_destroy(repeator);
    return ret;
}

int util_parse_arguments(UdpRepeator *repeator, int argc, char *argv[])
{
    int ret;
    char * short_options = "s:l:t:c:d";
    struct option long_options[] = {
    { "source", required_argument, NULL, 's' },
    { "dest", required_argument, NULL, 'd' },
    { "timeout", required_argument, NULL, 't' },
    { "config", no_argument, NULL, 'c' },
    { "debug", no_argument, NULL, 'g' },
    { 0, 0, 0, 0},
    };

    for(;;)
    {
        int c;
        c = getopt_long (argc, argv, short_options, long_options, NULL);
        if(c != -1 )
        {
            switch(c)
            {
            case 'l':
            {
                int ipaddrs[4];
                int port;
                printf("left %s",optarg);
                ret = sscanf(optarg,"%d.%d.%d.%d:%d",ipaddrs,ipaddrs+1,ipaddrs+2,ipaddrs+3,&port);
                printf("ret =%d,%d.%d.%d.%d:%d\n",ret,ipaddrs[0],ipaddrs[1],ipaddrs[2],ipaddrs[3],port);
                ret = sscanf(optarg,":%d",&port);
                printf("ret =%d,:%d\n",ret,port);
            }
                break;
            case 'r':
                printf("right %s\n",optarg);
                break;
            case 't':
                printf("timeout %s\n",optarg);
                break;
            case 'd':
				printf("start with debug mode\n");
				UdpRepeator_setDebug(1);
                break;
            case '?':
            default:
                printf("usage:%s [-f filename] [-n] testdata\n", argv[0]);
                getchar();
                return -1;
            }
        }
        else
        {
            return -2;
//            break;
        }

    }
}

int util_help_useage(char *path)
{

}
