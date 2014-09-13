#include <stdio.h>
#include "udprepeator.h"

int main()
{
    int ret;
    UdpRepeator *repeator;

    repeator = UdpRepeator_new(0,"0.0.0.0",1201,"127.0.0.1",11194);

    ret = UdpRepeator_exec(repeator);
    UdpRepeator_destroy(repeator);

    return ret;
}


