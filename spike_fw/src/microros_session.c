/*
 * Real micro-ROS sessions (unit h): one per UART port, each with its own custom transport.
 * Entities are added later through microros_entities.h.
 */
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rmw_microros/ping.h>
#include <rmw_microros/rmw_microros.h>
#include <rmw_microros/timing.h>
#include <uxr/client/util/ping.h>

/* private header of the rmw library: uxrSession of a context (see spike_fw/CMakeLists.txt for the include path) */
#include <rmw_microros_internal/types.h>

#include "microros_publishers.h"
#include "microros_services.h"
#include "microros_session.h"
#include "microros_subscribers.h"
#include "microros_transports.h"

/* compile-time entity counts from the table */
#define EXPECTED_PUB 20
#define EXPECTED_SUB 12
#define EXPECTED_SRV 3

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

/*
 * Session life cycle per port (one thread each), independent of the start order of the agent and
 * surviving an agent restart. No division or modulo, only k_uptime_get comparisons.
 *
 *   WAITING   ping the agent every WAITING_PERIOD_MS (custom transport in the init options, no context)
 *   BUILDING  init options, support, node, entities; any failure destroys what was built and goes to WAITING
 *   CONNECTED the loops of the unit h; ping the own session every PING_PERIOD_MS
 *   TEARDOWN  after PING_TIMEOUT_MS without a successful ping (or, UART6 only, after
 *             LEXXHARD_MICROROS_PUBLISH_FAILURE_LIMIT consecutive publish failures) destroy everything and go to WAITING
 */
#define PING_TIMEOUT_MS_PER_TRY 100
#define PING_ATTEMPTS 1
#define WAITING_PERIOD_MS 500
#define PING_PERIOD_MS 2500
#define PING_TIMEOUT_MS 11000
#define LOG_EVERY_FAILURES 20
#define BUILD_RETRY_PAUSE_MS 500

/*
 * UCLIENT_PROFILE_MULTITHREAD is off, so the global rmw pools (sessions, nodes, entities, wait sets,
 * init options) are not locked: the two session threads hold this mutex while they build or destroy.
 * It is not held in CONNECTED, in any sleep or during the pings of WAITING.
 */
K_MUTEX_DEFINE(session_mutex);

struct session_state {
	rcl_init_options_t init_options;
	rcl_allocator_t allocator;
	rclc_support_t support;
	rcl_node_t node;
	bool options_ready;
	bool support_ready;
	bool node_ready;
	bool publishers_started;
	bool subscribers_started;
	bool services_started;
	bool first_logged;
	unsigned suppressed_failures;
};

/* First failure is printed, then one line per LOG_EVERY_FAILURES failures (counter reset, no modulo). */
static void log_failure(struct session_state * s, const char * what, const int rc)
{
	bool print = false;
	if (!s->first_logged) {
		s->first_logged = true;
		s->suppressed_failures = 0;
		print = true;
	} else {
		++s->suppressed_failures;
		if (s->suppressed_failures >= LOG_EVERY_FAILURES) {
			s->suppressed_failures = 0;
			print = true;
		}
	}
	if (print) {
		printf("micro-ROS session: %s failed: %d. Retrying.\n", what, rc);
	}
}

/* Destroys everything that was built, in reverse order, without waiting for the (possibly gone) agent. */
static void destroy_session_locked(struct session_state * s)
{
	if (s->support_ready) {
		rmw_context_t * const rmw_context = rcl_context_get_rmw_context(&s->support.context);
		if (rmw_context != NULL) {
			const rmw_ret_t timeout_rc = rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);
			(void)timeout_rc;
		}
	}
	if (s->subscribers_started) {
		lexxhard_microros_subscribers_fini(&s->node);
	}
	if (s->services_started) {
		lexxhard_microros_services_fini(&s->node);
	}
	if (s->publishers_started) {
		lexxhard_microros_publishers_fini(&s->node);
	}
	if (s->node_ready) {
		const rcl_ret_t rc = rcl_node_fini(&s->node);
		(void)rc;
	}
	if (s->support_ready) {
		/* rcl_shutdown inside closes the session and the transport */
		const rcl_ret_t rc = rclc_support_fini(&s->support);
		(void)rc;
	}
	if (s->options_ready) {
		const rcl_ret_t rc = rcl_init_options_fini(&s->init_options);
		(void)rc;
	}
	s->options_ready = false;
	s->support_ready = false;
	s->node_ready = false;
	s->publishers_started = false;
	s->subscribers_started = false;
	s->services_started = false;
}

static void destroy_session(struct session_state * s)
{
	(void)k_mutex_lock(&session_mutex, K_FOREVER);
	destroy_session_locked(s);
	(void)k_mutex_unlock(&session_mutex);
}

/* Creates the init options with the custom transport under the session mutex; true when ready. */
static bool create_options(lexxhard_port_t * port, struct session_state * s)
{
	(void)k_mutex_lock(&session_mutex, K_FOREVER);
	bool ready = false;
	s->init_options = rcl_get_zero_initialized_init_options();
	const rcl_ret_t init_rc = rcl_init_options_init(&s->init_options, s->allocator);
	if (init_rc != RCL_RET_OK) {
		log_failure(s, "rcl_init_options_init", (int)init_rc);
	} else {
		rmw_init_options_t * const rmw_options = rcl_init_options_get_rmw_init_options(&s->init_options);
		const rmw_ret_t transport_rc = rmw_uros_options_set_custom_transport(
			LEXXHARD_MICRO_ROS_FRAMING_REQUIRED,
			port,
			lexxhard_port_open,
			lexxhard_port_close,
			lexxhard_port_write,
			lexxhard_port_read,
			rmw_options);
		if (transport_rc != RMW_RET_OK) {
			log_failure(s, "rmw_uros_options_set_custom_transport", (int)transport_rc);
			const rcl_ret_t fini_rc = rcl_init_options_fini(&s->init_options);
			(void)fini_rc;
		} else {
			s->options_ready = true;
			ready = true;
		}
	}
	(void)k_mutex_unlock(&session_mutex);
	return ready;
}

/* WAITING: returns when the agent answered a ping; the init options (with the transport) are then ready. */
static void wait_for_agent(lexxhard_port_t * port, struct session_state * s)
{
	for (;;) {
		if (!s->options_ready) {
			const bool created = create_options(port, s);
			if (!created) {
				k_msleep(WAITING_PERIOD_MS);
				continue;
			}
		}

		rmw_init_options_t * const rmw_options = rcl_init_options_get_rmw_init_options(&s->init_options);
		if (rmw_uros_ping_agent_options(PING_TIMEOUT_MS_PER_TRY, PING_ATTEMPTS, rmw_options) == RMW_RET_OK) {
			return;
		}
		k_msleep(WAITING_PERIOD_MS);
	}
}

/* BUILDING: true when everything exists; on failure everything is destroyed, the state is WAITING again. */
static bool build_session_locked(lexxhard_port_t * port, const char * node_name, struct session_state * s)
{
	rcl_ret_t rc = rclc_support_init_with_options(&s->support, 0, NULL, &s->init_options, &s->allocator);
	if (rc != RCL_RET_OK) {
		log_failure(s, "rclc_support_init_with_options", (int)rc);
		if (rcl_context_is_valid(&s->support.context)) {
			/* the context was initialised (a later step failed): free the session slot and the transport */
			const rcl_ret_t shutdown_rc = rcl_shutdown(&s->support.context);
			const rcl_ret_t context_fini_rc = rcl_context_fini(&s->support.context);
			(void)shutdown_rc;
			(void)context_fini_rc;
		}
		destroy_session_locked(s);
		return false;
	}
	s->support_ready = true;

	s->node = rcl_get_zero_initialized_node();
	rc = rclc_node_init_default(&s->node, node_name, "", &s->support);
	if (rc != RCL_RET_OK) {
		log_failure(s, "rclc_node_init_default", (int)rc);
		destroy_session_locked(s);
		return false;
	}
	s->node_ready = true;

	if (port == &lexxhard_port_uart6) {
		/* host-synchronised time for the diagnostics stamp; the result is ignored */
		const rmw_ret_t sync_rc = rmw_uros_sync_session(1000);
		(void)sync_rc;

		s->publishers_started = true;
		const int publishers_rc = lexxhard_microros_publishers_init(&s->node);
		if (publishers_rc != 0) {
			log_failure(s, "publishers init", publishers_rc);
			destroy_session_locked(s);
			return false;
		}

		s->subscribers_started = true;
		const int subscribers_rc = lexxhard_microros_subscribers_init(&s->node, &s->support.context);
		if (subscribers_rc != 0) {
			log_failure(s, "subscribers init", subscribers_rc);
			destroy_session_locked(s);
			return false;
		}
	} else if (port == &lexxhard_port_uart2) {
		s->services_started = true;
		const int services_rc = lexxhard_microros_services_init(&s->node, &s->support.context);
		if (services_rc != 0) {
			log_failure(s, "services init", services_rc);
			destroy_session_locked(s);
			return false;
		}
	}
	return true;
}

static bool build_session(lexxhard_port_t * port, const char * node_name, struct session_state * s)
{
	(void)k_mutex_lock(&session_mutex, K_FOREVER);
	const bool built = build_session_locked(port, node_name, s);
	(void)k_mutex_unlock(&session_mutex);
	return built;
}

/* One ping of the session of this port (not the global ping: another port must not answer for it). */
static bool ping_own_session(struct session_state * s)
{
	rmw_context_t * const rmw_context = rcl_context_get_rmw_context(&s->support.context);
	if (rmw_context == NULL || rmw_context->impl == NULL) {
		return false;
	}
	return uxr_ping_agent_session(&rmw_context->impl->session, PING_TIMEOUT_MS_PER_TRY, PING_ATTEMPTS);
}

/* CONNECTED: returns when the session must be torn down. */
static void run_connected(lexxhard_port_t * port, struct session_state * s)
{
	const bool is_uart6 = (port == &lexxhard_port_uart6);
	const bool is_uart2 = (port == &lexxhard_port_uart2);
	int64_t next_ping = k_uptime_get() + PING_PERIOD_MS;
	int64_t last_success = k_uptime_get();

	bool first_ping_logged = false;

	s->first_logged = false;
	s->suppressed_failures = 0;

	for (;;) {
		if (is_uart6) {
			lexxhard_microros_subscribers_spin();
			lexxhard_microros_publishers_poll();
			if (lexxhard_microros_publishers_consecutive_failures() >= LEXXHARD_MICROROS_PUBLISH_FAILURE_LIMIT) {
				printf("micro-ROS session: %d publish failures in a row. Restarting.\n", LEXXHARD_MICROROS_PUBLISH_FAILURE_LIMIT);
				return;
			}
			k_usleep(1);
		} else if (is_uart2) {
			const int64_t spin_start = k_uptime_get();
			lexxhard_microros_services_spin();
			const int64_t spin_ms = k_uptime_get() - spin_start;
			if (spin_ms > PING_PERIOD_MS) {
				/* a service handler blocked: the timers are held during it (TESTPLAN 9.2.2) */
				next_ping += spin_ms;
				last_success += spin_ms;
			}
			k_usleep(1);
		} else {
			k_msleep(100);
		}

		/* timers are checked between spins only; a long UART2 spin (handler up to 30 s) shifted them above */
		if (k_uptime_get() >= next_ping) {
			if (ping_own_session(s)) {
				last_success = k_uptime_get();
				if (!first_ping_logged) {
					first_ping_logged = true;
					printf("micro-ROS session: first ping ok\n");
				}
			}
			next_ping = k_uptime_get() + PING_PERIOD_MS;
		}
		if (k_uptime_get() - last_success >= PING_TIMEOUT_MS) {
			printf("micro-ROS session: no ping answer for %d ms. Restarting.\n", PING_TIMEOUT_MS);
			return;
		}
	}
}

static void run_session(lexxhard_port_t * port, const char * node_name)
{
	struct session_state state;
	memset(&state, 0, sizeof state);
	state.allocator = rcl_get_default_allocator();

	for (;;) {
		wait_for_agent(port, &state);
		if (!build_session(port, node_name, &state)) {
			k_msleep(BUILD_RETRY_PAUSE_MS);
			continue;
		}
		run_connected(port, &state);
		destroy_session(&state);
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
