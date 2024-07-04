/*
 * platform indepent driver interface
 *
 * Coypritht (c) 2017 Goodix
 */
#define DEBUG

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#define FAIL (-1)

int gf_parse_dts(struct gf_dev *gf_dev)
{
#ifdef GF_PW_CTL
	int rc = 0;
	struct device *dev = NULL;
	struct device_node *np = NULL;

	if (!gf_dev) {
		pr_err("gf_dev is NULL\n");
		return -EPERM;
	}

	if (!gf_dev->spi) {
		pr_err("gf_dev->spi is NULL\n");
		return -EPERM;
	}

	dev = &gf_dev->spi->dev;

	if (!dev) {
		pr_err("dev is NULL\n");
		return -EPERM;
	}

	np = dev->of_node;

	gf_dev->pwr_gpio = of_get_named_gpio(np, "power_enable", 0);
	if (gf_dev->pwr_gpio < 0) {
		pr_err("falied to get pwr_gpio!\n");
		return gf_dev->pwr_gpio;
	}
	rc = devm_gpio_request(dev, gf_dev->pwr_gpio, "power_enable");
	if (rc) {
		pr_err("failed to request power_enable, rc = %d\n", rc);
		return FAIL;
	}

	#if 0 //trl add
	rc = of_property_read_string(np, "goodix-fp-ldo", &name);
	if (rc < 0){
		gf_dev->custom_ldo_name= NULL;
		printk("nod found goodix-fp-ldo\n");
		}
	else
		gf_dev->custom_ldo_name = name;
	#endif

	gf_dev->reset_gpio = of_get_named_gpio(np, "fp-gpio-reset", 0);
	if (gf_dev->reset_gpio < 0) {
		pr_err("falied to get reset gpio!\n");
		return gf_dev->reset_gpio;
	}

	gf_dev->irq_gpio = of_get_named_gpio(np, "fp-gpio-irq", 0);
	if (gf_dev->irq_gpio < 0) {
		pr_err("falied to get irq gpio!\n");
		return gf_dev->irq_gpio;
	}
#endif
	return 0;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
	pr_info("[info] %s\n", __func__);
	if (gpio_is_valid(gf_dev->irq_gpio)) {
		gpio_free(gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		gpio_free(gf_dev->pwr_gpio);
		pr_info("remove pwr_gpio success\n");
	}
#endif
}

int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		gpio_set_value(gf_dev->pwr_gpio, 1);
		pr_info("---- power on result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
#endif

	msleep(10);
	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;
#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		gpio_direction_output(gf_dev->reset_gpio, 0);
		rc = gpio_direction_output(gf_dev->pwr_gpio, 0);
		pr_info("---- power off result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
#endif

	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EPERM;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);
	mdelay(3);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	pr_info("%s success\n", __func__);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EPERM;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}
