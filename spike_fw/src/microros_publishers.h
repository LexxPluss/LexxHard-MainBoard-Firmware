#ifndef LEXXHARD_MICROROS_PUBLISHERS_H_
#define LEXXHARD_MICROROS_PUBLISHERS_H_

#include <rcl/rcl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Publishers of the UART6 session (unit h): one to one port of the rosserial publishers of the MainBoard.
 *
 * init creates every publisher of kind PUB of microros_entities.h in table order with the reliable default
 * QoS and prepares the static message objects. It returns 0 or the first rcl_ret_t that is not RCL_RET_OK.
 * poll runs one pass of all publisher poll functions in the order of the old rosserial run loop. Both are
 * called from the UART6 session thread only.
 */
int lexxhard_microros_publishers_init(rcl_node_t * node);
void lexxhard_microros_publishers_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* LEXXHARD_MICROROS_PUBLISHERS_H_ */
