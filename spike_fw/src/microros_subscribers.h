#ifndef LEXXHARD_MICROROS_SUBSCRIBERS_H_
#define LEXXHARD_MICROROS_SUBSCRIBERS_H_

#include <rcl/rcl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subscribers of the UART6 session (unit h): one to one port of the rosserial subscribers of the MainBoard.
 *
 * init creates every subscription of kind SUB of microros_entities.h in table order with the reliable default
 * QoS, prepares the static message buffers and one rcl wait set. It returns 0 or the first rcl_ret_t that is
 * not RCL_RET_OK. spin runs one non-blocking pass (zero timeout wait, take and callback of every ready
 * subscription in table order). Both are called from the UART6 session thread only.
 */
int lexxhard_microros_subscribers_init(rcl_node_t * node, rcl_context_t * context);
void lexxhard_microros_subscribers_spin(void);

#ifdef __cplusplus
}
#endif

#endif /* LEXXHARD_MICROROS_SUBSCRIBERS_H_ */
