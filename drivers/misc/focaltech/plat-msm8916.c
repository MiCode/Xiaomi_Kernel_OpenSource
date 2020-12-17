/**
 * plat-msm8916.c
 *
**/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "ff_log.h"
#include "ff_ctl.h"

# undef LOG_TAG
#define LOG_TAG "msm8916"

#define FF_COMPATIBLE_NODE "focaltech,fingerprint"

/*
 * Driver configuration. See ff_ctl.c
 */
extern ff_driver_config_t *g_config;

int ff_ctl_init_pins(int *irq_num)
{
    int err = 0, gpio;
    struct device_node *dev_node = NULL;
    enum of_gpio_flags flags;
    bool b_config_dirtied = false;
    FF_LOGV("'%s' enter.", __func__);

	if (unlikely(!g_config)) {
		return (-ENOSYS);
	}

	/* Find device tree node. */
	dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE);
		return (-ENODEV);
	}

	/* Initialize RST pin. */
	gpio = of_get_named_gpio_flags(dev_node, "fp,reset_gpio", 0, &flags);
	if (gpio > 0) {
		g_config->gpio_rst_pin = gpio;
		b_config_dirtied = true;
	}
	if (!gpio_is_valid(g_config->gpio_rst_pin)) {
		FF_LOGE("g_config->gpio_rst_pin(%d) is invalid.", g_config->gpio_rst_pin);
		return (-ENODEV);
	}
	err = gpio_request(g_config->gpio_rst_pin, "ff_gpio_rst_pin");
	if (err) {
		FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_rst_pin, err);
		return err;
	}
	err = gpio_direction_output(g_config->gpio_rst_pin, 1);
	if (err) {
		FF_LOGE("gpio_direction_output(%d, 1) = %d.", g_config->gpio_rst_pin, err);
		return err;
	}
#ifdef	FF_VDD_GPIO
	/* Initialize PWR/VDD pin. */
	gpio = of_get_named_gpio_flags(dev_node, "fp,vdd_gpio", 0, &flags);
	if (gpio > 0) {
		g_config->gpio_power_pin = gpio;
		b_config_dirtied = true;
	}
	if (!gpio_is_valid(g_config->gpio_power_pin)) {
		FF_LOGE("g_config->gpio_power_pin(%d) is invalid.", g_config->gpio_power_pin);
		return (-ENODEV);
	}
	err = gpio_request(g_config->gpio_power_pin, "ff_gpio_power_pin");
	if (err) {
		FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_power_pin, err);
		return err;
	}
	err = gpio_direction_output(g_config->gpio_power_pin, 0); // power off.
	if (err) {
		FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_config->gpio_power_pin, err);
		return err;
	}
#endif

#ifdef	FF_IOVCC_GPIO
	/* Initialize IOVCC pin. */
	gpio = of_get_named_gpio_flags(dev_node, "fp,iovcc_gpio", 0, &flags);
	if (gpio > 0) {
		g_config->gpio_iovcc_pin = gpio;
		b_config_dirtied = true;
	}
	if (!gpio_is_valid(g_config->gpio_iovcc_pin)) {
		FF_LOGE("g_config->gpio_iovcc_pin(%d) is invalid.", g_config->gpio_iovcc_pin);
		return (-ENODEV);
	}
	err = gpio_request(g_config->gpio_iovcc_pin, "ff_gpio_iovcc_pin");
	if (err) {
		FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_iovcc_pin, err);
		return err;
	}
	err = gpio_direction_output(g_config->gpio_iovcc_pin, 0); // power off.
	if (err) {
		FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_config->gpio_iovcc_pin, err);
		return err;
	}
#endif

	/* Initialize INT pin. */
	gpio = of_get_named_gpio_flags(dev_node, "fp,irq_gpio", 0, &flags);
	if (gpio > 0) {
		g_config->gpio_int_pin = gpio;
		b_config_dirtied = true;
	}
	if (!gpio_is_valid(g_config->gpio_int_pin)) {
		FF_LOGE("g_config->gpio_int_pin(%d) is invalid.", g_config->gpio_int_pin);
		return (-ENODEV);
	}
	err = gpio_request(g_config->gpio_int_pin, "ff_gpio_int_pin");
	if (err) {
		FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_int_pin, err);
		return err;
	}
	err = gpio_direction_input(g_config->gpio_int_pin);
	if (err) {
		FF_LOGE("gpio_direction_input(%d) = %d.", g_config->gpio_int_pin, err);
		return err;
	}

	/* Retrieve the IRQ number. */
	*irq_num = gpio_to_irq(g_config->gpio_int_pin);
	if (*irq_num < 0) {
		FF_LOGE("gpio_to_irq(%d) failed.", g_config->gpio_int_pin);
		return (-EIO);
	} else {
		FF_LOGD("gpio_to_irq(%d) = %d.", g_config->gpio_int_pin, *irq_num);
	}

	/* Configuration is dirty, must sync back to HAL. */
	if (!err && b_config_dirtied) {
		err = 1;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_free_pins(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	/* Release GPIO resources. */
	gpio_free(g_config->gpio_rst_pin);
	gpio_free(g_config->gpio_int_pin);
#ifdef	FF_IOVCC_GPIO
	gpio_free(g_config->gpio_iovcc_pin);
#endif
#ifdef	FF_VDD_GPIO
	gpio_free(g_config->gpio_power_pin);
#endif
	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_spiclk(bool on)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);
	FF_LOGD("clock: '%s'.", on ? "enable" : "disabled");

	if (on) {
		// TODO:
	} else {
		// TODO:
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_power(bool on)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);
	FF_LOGD("power: '%s'.", on ? "on" : "off");

	if (unlikely(!g_config)) {
		return (-ENOSYS);
	}

	if (on) {
#ifdef	FF_VDD_GPIO
		err = gpio_direction_output(g_config->gpio_power_pin, 1);
#endif
		msleep(5);
#ifdef	FF_IOVCC_GPIO
		err = gpio_direction_output(g_config->gpio_iovcc_pin, 1);
#endif
	} else {
#ifdef	FF_IOVCC_GPIO
		err = gpio_direction_output(g_config->gpio_iovcc_pin, 0);
#endif
		msleep(5);
#ifdef	FF_VDD_GPIO
		err = gpio_direction_output(g_config->gpio_power_pin, 0);
#endif
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_reset_device(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	if (unlikely(!g_config)) {
		return (-ENOSYS);
	}

	/* 3-1: Pull down RST pin. */
	err = gpio_direction_output(g_config->gpio_rst_pin, 0);

	/* 3-2: Delay for 10ms. */
	mdelay(10);

	/* Pull up RST pin. */
	err = gpio_direction_output(g_config->gpio_rst_pin, 1);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

const char *ff_ctl_arch_str(void)
{
	return "msm8916";
}

