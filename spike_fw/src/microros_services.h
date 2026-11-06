#ifndef LEXXHARD_MICROROS_SERVICES_H_
#define LEXXHARD_MICROROS_SERVICES_H_

#include <rcl/rcl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Services of the UART2 session: one to one port of the rosserial services of the MainBoard
 * (rosserial_actuator_service.hpp, rosserial_board_service.hpp).
 *
 * init creates every service of kind SRV of microros_entities.h in table order with the reliable default
 * options, prepares the static request/response buffers and one rcl wait set. It returns 0 or the first
 * rcl_ret_t that is not RCL_RET_OK. spin runs one non-blocking pass (zero timeout wait, take, callback and
 * response of every ready service in table order). A handler may block up to 30 s inside actuator_controller
 * (as in rosserial). Both are called from the UART2 session thread only.
 */
int lexxhard_microros_services_init(rcl_node_t * node, rcl_context_t * context);
void lexxhard_microros_services_spin(void);

#ifdef __cplusplus
}
#endif

#endif /* LEXXHARD_MICROROS_SERVICES_H_ */
