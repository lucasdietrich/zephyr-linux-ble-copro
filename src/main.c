#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <observer.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

int main(void)
{
	printk("Hello, World!\n");

	/* Initialize the Bluetooth Subsystem */
	int ret = bt_enable(NULL);
	if (ret != 0) {
		LOG_INF("Bluetooth init failed (ret %d)", ret);
		return ret;
	}

	LOG_INF("Bluetooth initialized %d", 0);

	/* Start the BLE observer thread */
	ble_observer_start();

	return 0;
}