#ifndef UDPREPEATOR_H
#define UDPREPEATOR_H
#include <stdint.h>

typedef struct UdpRepeator UdpRepeator;
/**
 * @brief UdpRepeator_new
 * @param host
 * @param port
 * @return
 */
UdpRepeator * UdpRepeator_new(int mode,const char *right_host,uint16_t right_port,const char *left_host,uint16_t left_port);
/**
 * @brief UdpRepeator_destroy
 * @param thiz
 */
void UdpRepeator_destroy(UdpRepeator *thiz);
/**
 * @brief UdpRepeator_exec
 * @param thiz
 * @return
 */
int UdpRepeator_exec(UdpRepeator *thiz);

#endif // UDPREPEATOR_H
