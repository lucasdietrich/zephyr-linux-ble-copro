#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <led.h>

/* The devicetree node identifier for the "led1" alias. */
#define LED_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

int board_led_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		return -EIO;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_set_dt(&led, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int board_led_on(void)
{
	return gpio_pin_set_dt(&led, 1);
}

int board_led_off(void)
{
	return gpio_pin_set_dt(&led, 0);
}