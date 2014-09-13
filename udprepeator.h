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
UdpRepeator * UdpRepeator_new(const char *host,uint16_t port);
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
