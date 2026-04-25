#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <led.h>

static struct k_work_delayable led_blink_work;
static uint32_t led_blink_period_ms;
static bool led_blink_state;

static void led_blink_handler(struct k_work *work)
{
	led_blink_state = !led_blink_state;
	gpio_pin_set_dt(&led, led_blink_state ? 1 : 0);
	k_work_reschedule(&led_blink_work, K_MSEC(led_blink_period_ms));
}

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

	k_work_init_delayable(&led_blink_work, led_blink_handler);

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

int board_led_set(bool on)
{
	return gpio_pin_set_dt(&led, on ? 1 : 0);
}

int board_led_blink_start(uint32_t period_ms)
{
	led_blink_period_ms = period_ms;
	led_blink_state = false;
	return k_work_reschedule(&led_blink_work, K_NO_WAIT);
}

void board_led_blink_stop(void)
{
	k_work_cancel_delayable(&led_blink_work);
}