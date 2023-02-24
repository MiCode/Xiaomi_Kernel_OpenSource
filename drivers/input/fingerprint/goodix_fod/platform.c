/*
 * platform indepent driver interface
 *
 * Coypritht (c) 2017 Goodix
 */
#define DEBUG
#define pr_fmt(fmt)     "gf_platform: " fmt

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

#define GOODIX_FINGERPRINT_PINCTRL_DEFAULT_STATE "fingerprint_goodix_default"

static int gf_pinctrl_init(struct gf_dev *gf_dev)
{
	int ret = 0;

	gf_dev->pinctrl = devm_pinctrl_get(&gf_dev->spi->dev);
	if (IS_ERR_OR_NULL(gf_dev->pinctrl)) {
		pr_info("Failed to get pinctrl, please check dts\n");
		ret = PTR_ERR(gf_dev->pinctrl);
		goto err_pinctrl_get;
	}

	gf_dev->gf_default_state = pinctrl_lookup_state(gf_dev->pinctrl, GOODIX_FINGERPRINT_PINCTRL_DEFAULT_STATE);
	if (IS_ERR_OR_NULL(gf_dev->gf_default_state)) {
		pr_info("Pin state[default] not found\n");
		ret = PTR_ERR(gf_dev->gf_default_state);
		goto err_pinctrl_lookup;
	}

	pr_info("gf_pinctrl_init done\n");
	return 0;
err_pinctrl_lookup:
	if (gf_dev->pinctrl) {
		devm_pinctrl_put(gf_dev->pinctrl);
	}
err_pinctrl_get:
	gf_dev->pinctrl = NULL;
	gf_dev->gf_default_state = NULL;
	return ret;
}

int gf_parse_dts(struct gf_dev *gf_dev)
{
#ifdef GF_PW_CTL
	/*get pwr resource */
	gf_dev->pwr_gpio =
		of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-pwr", 0);

	if (!gpio_is_valid(gf_dev->pwr_gpio)) {
		pr_info("PWR GPIO is invalid.\n");
		return -EPERM;
	}
#endif
	/*get reset resource */
	gf_dev->reset_gpio =
		of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio-reset", 0);

	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_info("RESET GPIO is invalid.\n");
		return -EPERM;
	}
	pr_info("gf::gpio-reset:%d\n", gf_dev->reset_gpio);

	/*get irq resourece */
	gf_dev->irq_gpio =
		of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio-irq", 0);
	pr_info("gf::irq_gpio:%d\n", gf_dev->irq_gpio);

	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_info("IRQ GPIO is invalid.\n");
		return -EPERM;
	}

	gf_pinctrl_init(gf_dev);

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
		rc = gpio_direction_output(gf_dev->pwr_gpio, 1);
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
	pr_info("%s\n", __func__);
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
