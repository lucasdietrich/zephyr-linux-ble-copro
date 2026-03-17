#include "ble_server.h"
#include "zephyr/logging/log.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <button.h>

/* The devicetree node identifier for the "sw0" alias. */
#define SW_NODE DT_ALIAS(sw0)

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec sw = GPIO_DT_SPEC_GET(SW_NODE, gpios);

static struct gpio_callback button_cb_data;

bool button_pressed_flag;

struct k_work button_work;

static void button_work_handler(struct k_work *work)
{
    my_lbs_send_sensor_notify(k_cycle_get_32());
	my_lbs_send_button_state_indicate(button_pressed_flag);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	LOG_INF("Button pressed at %" PRIu32, k_cycle_get_32());
    button_pressed_flag = board_button_is_pressed();
	
	k_work_submit(&button_work);
}

int board_button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&sw)) {
		return -EIO;
	}

	ret = gpio_pin_configure_dt(&sw, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

    ret = gpio_pin_interrupt_configure_dt(&sw, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		return ret;
	}

	k_work_init(&button_work, button_work_handler);

	gpio_init_callback(&button_cb_data, button_pressed, BIT(sw.pin));
	gpio_add_callback(sw.port, &button_cb_data);

	return 0;
}

int board_button_is_pressed(void)
{
	return gpio_pin_get_dt(&sw);
}