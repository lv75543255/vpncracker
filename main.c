#include <stdio.h>
#include "udprepeator.h"
int main(int argc,char *argv[])
{
    int ret;
    UdpRepeator *repeator;

	if(argc < 2)
	{
		printf("start local repeator\n");
		repeator = UdpRepeator_new(0,"127.0.0.1",1194,"54.64.132.2",1201);
	}
	else
	{
		printf("start remote repeator\n");
		repeator = UdpRepeator_new(1,"0.0.0.0",1201,"127.0.0.1",11194);
	}

    ret = UdpRepeator_exec(repeator);
    UdpRepeator_destroy(repeator);

    return ret;
}


