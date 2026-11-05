#include <zephyr/kernel.h>

int main(void)
{
	for (;;) {
		printk("spike_minimal alive %u\n", k_uptime_get_32());
		k_sleep(K_SECONDS(1));
	}
	return 0;
}
