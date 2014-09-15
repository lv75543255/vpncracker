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
    ret = UdpRepeator_exec(repeator);
    UdpRepeator_destroy(repeator);
    return ret;
}

int util_read_settings(UdpRepeator *repeator)
{
	FILE *pf = fopen("config.ini","rw");
	if(pf != NULL)
	{
		int ret;
		char buf[256];
		int in_port;
		int out_port;
		int ipaddr[4];
		for(;;)
		{
			if(fgets(buf,sizeof(buf),pf) != buf)
			{
				break;
			}

			ret = sscanf(buf,"remote:%d@%d",&in_port,&out_port);
			if(ret > 0)
			{
				out_port = ret > 1 ? out_port:in_port;
				printf("start <remote> repeator @0.0.0.0:%u -> 127.0.0.1:%u\n",in_port,out_port);
				UdpRepeator_setSource(repeator,"0.0.0.0",in_port);
				UdpRepeator_setDestination(repeator,"127.0.0.1",out_port);
				UdpRepeator_setRepeatorMode(repeator,1);
				fclose(pf);
				return 0;
			}

			ret = sscanf(buf,"local:%d@%d.%d.%d.%d:%d",&in_port,&ipaddr[0],&ipaddr[1],&ipaddr[2],&ipaddr[3],&out_port);
			if(ret > 4)
			{
				out_port = ret > 5 ? out_port:in_port;
				char addr[16];
				sprintf(addr,"%d.%d.%d.%d",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
				printf("start <local> repeator @127.0.0.1:%u -> %s:%u\n",in_port,addr,out_port);

				UdpRepeator_setSource(repeator,"127.0.0.1",in_port);
				UdpRepeator_setDestination(repeator,addr,out_port);
				UdpRepeator_setRepeatorMode(repeator,0);
				fclose(pf);
				return 0;
			}

		}
	}
	else
	{
		perror("what?");
	}
	printf("Invalied config.ini,please check!!\n");
	printf("example:\n");
	printf("\tremote:1195@54.64.132.2:1201\n");
	printf("or\n");
	printf("\tlocal:1201:11194\n");

	fclose(pf);
	return -1;
}

//TODO
int util_save_settings(UdpRepeator *repeator)
{
	FILE *pf = fopen("config.ini","rw");
	if(pf != NULL)
	{

	}
	else
	{
		perror("what?");
	}

	fclose(pf);
	return -1;
}

//TODO
int util_parse_arguments(UdpRepeator *repeator, int argc, char *argv[])
{
//	int ret;
//	char * short_options = "s:l:t:c:d";
//	struct option long_options[] = {
//	{ "source", required_argument, NULL, 's' },
//	{ "dest", required_argument, NULL, 'd' },
//	{ "timeout", required_argument, NULL, 't' },
//	{ "config", no_argument, NULL, 'c' },
//	{ "debug", no_argument, NULL, 'g' },
//	{ 0, 0, 0, 0},
//	};

//	for(;;)
//	{
//		int c;
//		c = getopt_long (argc, argv, short_options, long_options, NULL);
//		if(c != -1 )
//		{
//			switch(c)
//			{
//			case 'l':
//			{
//				int in_port;
//				int out_port;
//				printf("left %s",optarg);
//				int addr[4];
//				ret = sscanf(optarg,"%d.%d.%d.%d:%d@%d",addr,addr+1,addr+2,addr+3,&in_port,&out_port);
//				if(ret == 1)
//				{
//					out_port = in_port;
//				}

//				printf("ret =%d,%d@%d\n",ret,in_port,out_port);
//			}
//				break;
//			case 'r':
//			{
//				int in_port;
//				int out_port;
//				printf("right %s",optarg);

//				ret = sscanf(optarg,"%d@%d",&in_port,&out_port);
//				if(ret == 1)
//				{
//					out_port = in_port;
//				}

//				printf("ret =%d,%d@%d\n",ret,in_port,out_port);
//			}
//				break;
//			case 't':
//				printf("timeout %s\n",optarg);
//				break;
//			case 'c':
//				printf("timeout %s\n",optarg);
//				break;
//			case 'd':
//				printf("start with debug mode\n");
//				UdpRepeator_setDebug(1);
//				break;
//			case '?':
//			default:
//				printf("usage:%s [-f filename] [-n] testdata\n", argv[0]);
//				getchar();
//				return -1;
//			}
//		}
//		else
//		{
//			break;
//		}
//	}

	printf("load settings(./config.ini)\n");
	if(util_read_settings(repeator) == 0)
	{
		return 0;
	}


}

int util_help_useage(char *path)
{

}
