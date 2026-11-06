// Temporary link stubs for units not ported yet; never part of the final firmware.
// The size measured while stubs are active is lower than the real firmware.
// Remove this file when U9f is done.

#include <zephyr/kernel.h>
#include "led_controller.hpp"
#include "misc_controller.hpp"

#if !FW_HAS_LED
namespace lexxhard::led_controller {

namespace {
char __aligned(4) stub_msgq_buffer[8 * sizeof (msg)];
struct stub_msgq_init {
    stub_msgq_init() { k_msgq_init(&msgq, stub_msgq_buffer, sizeof (msg), 8); }
} stub_msgq_init_instance;
}

k_msgq msgq;

}
#endif  // !FW_HAS_LED

#if !FW_HAS_MISC
namespace lexxhard::misc_controller {

float get_main_board_temp()
{
    return 0.0f;
}

float get_actuator_board_temp(int index)
{
    return 0.0f;
}

}
#endif  // !FW_HAS_MISC
