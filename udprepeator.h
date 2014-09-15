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
UdpRepeator * UdpRepeator_new();

/**
 * @brief UdpRepeator_setSource
 * @param thiz
 * @param host
 * @param port
 */
void UdpRepeator_setSource(UdpRepeator *thiz,const char *host,uint16_t port);

/**
 * @brief UdpRepeator_setDestination
 * @param thiz
 * @param host
 * @param port
 */
void UdpRepeator_setDestination(UdpRepeator *thiz,const char *host,uint16_t port);

/**
 * @brief UdpRepeator_setRepeatorMode
 * @param isRepeat2Local
 */

void UdpRepeator_setRepeatorMode(UdpRepeator *thiz,int isRepeat2Local);

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

void UdpRepeator_setDebug(int debug);
#endif // UDPREPEATOR_H
