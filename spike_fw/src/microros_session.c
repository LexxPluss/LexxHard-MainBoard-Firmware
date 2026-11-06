/*
 * Real micro-ROS sessions (unit h): one per UART port, each with its own custom transport.
 * Entities are added later through microros_entities.h.
 */
#include <stdio.h>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>

#include "microros_session.h"
#include "microros_transports.h"

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if (temp_rc != RCL_RET_OK) { printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); for (;;) {}; } }

/* compile-time entity counts from the table */
#define EXPECTED_PUB 0
#define EXPECTED_SUB 0
#define EXPECTED_SRV 0

/* simple counting: each include contributes 1 per matching entry */
#define MICROROS_KIND_PUB 0
#define MICROROS_KIND_SUB 1
#define MICROROS_KIND_SRV 2

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) +(MICROROS_KIND_##kind == 0)
enum { NUM_PUB = 0
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) +(MICROROS_KIND_##kind == 1)
enum { NUM_SUB = 0
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) +(MICROROS_KIND_##kind == 2)
enum { NUM_SRV = 0
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

_Static_assert(NUM_PUB == EXPECTED_PUB, "publisher count differs from the entity table");
_Static_assert(NUM_SUB == EXPECTED_SUB, "subscriber count differs from the entity table");
_Static_assert(NUM_SRV == EXPECTED_SRV, "service count differs from the entity table");

static void run_session(lexxhard_port_t * port, const char * node_name)
{
	rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;
	rcl_node_t node = rcl_get_zero_initialized_node();

	RCCHECK(rcl_init_options_init(&init_options, allocator));

	rmw_init_options_t * rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
	const rmw_ret_t transport_rc = rmw_uros_options_set_custom_transport(
		LEXXHARD_MICRO_ROS_FRAMING_REQUIRED,
		port,
		lexxhard_port_open,
		lexxhard_port_close,
		lexxhard_port_write,
		lexxhard_port_read,
		rmw_options);
	if (transport_rc != RMW_RET_OK) {
		printf("Failed to set custom transport: %d. Aborting.\n", (int)transport_rc);
		for (;;) {}
	}

	RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));
	RCCHECK(rclc_node_init_default(&node, node_name, "", &support));

	for (;;) {
		k_msleep(100);
	}
}

__attribute__((used, noinline)) void lexxhard_microros_session_uart6(void)
{
	run_session(&lexxhard_port_uart6, "mainboard_uart6");
}

__attribute__((used, noinline)) void lexxhard_microros_session_uart2(void)
{
	run_session(&lexxhard_port_uart2, "mainboard_uart2");
}
