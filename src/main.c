#include <zephyr/kernel.h>

int main(void)
{
	while (true) {
		printk("Hello, World!\n");
		k_sleep(K_MSEC(1000));
	}
}