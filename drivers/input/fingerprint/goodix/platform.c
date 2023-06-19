/*
 * platform indepent driver interface
 *
 * Coypritht (c) 2017 Goodix
 */
#define DEBUG
#define pr_fmt(fmt)		"gf_platform: " fmt

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
int power_enable = 0;

static int gf_pinctrl_init(struct gf_dev *gf_dev)
{
	int ret;

	pr_info("%s form dts pinctrl\n", __func__);

	gf_dev->pinctrl_gpios = devm_pinctrl_get(&gf_dev->spi->dev);
	if (IS_ERR(gf_dev->pinctrl_gpios)) {
		ret = PTR_ERR(gf_dev->pinctrl_gpios);
		pr_err("%s can't find fingerprint pinctrl\n", __func__);
		goto err_pinctrl_get;
	}
	pr_info("%s get fingerprint pinctrl success\n", __func__);

	gf_dev->gf_pwr_high = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "goodix_pwr_outputhigh");
	if (IS_ERR(gf_dev->gf_pwr_high)) {
		ret = PTR_ERR(gf_dev->gf_pwr_high);
		pr_err("%s can't find fingerprint pinctrl gf_pwr_high\n", __func__);
		goto err_pinctrl_lookup;
	}
	pr_info("%s get gf_pwr_high pinctrl success\n", __func__);
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->gf_pwr_high);
	pr_info("%s goodix power for plsensor successfully\n", __func__);
	gf_dev->gf_pwr_low = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "goodix_pwr_outputlow");
	if (IS_ERR(gf_dev->gf_pwr_low)) {
		ret = PTR_ERR(gf_dev->gf_pwr_low);
		pr_err("%s can't find fingerprint pinctrl gf_pwr_low\n", __func__);
		goto err_pinctrl_lookup;
	}
	pr_info("%s get gf_pwr_low pinctrl success\n", __func__);

	gf_dev->gf_reset_high = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "goodix_reset_outputhigh");
	if (IS_ERR(gf_dev->gf_reset_high)) {
		ret = PTR_ERR(gf_dev->gf_reset_high);
		pr_err("%s can't find fingerprint pinctrl gf_reset_high\n", __func__);
		goto err_pinctrl_lookup;
	}
	pr_info("%s get gf_reset_high pinctrl success\n", __func__);

	gf_dev->gf_reset_low = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "goodix_reset_outputlow");
	if (IS_ERR(gf_dev->gf_reset_low)) {
		ret = PTR_ERR(gf_dev->gf_reset_low);
		pr_err("%s can't find fingerprint pinctrl gf_reset_low\n", __func__);
		goto err_pinctrl_lookup;
	}
	pr_info("%s get gf_reset_low pinctrl success\n", __func__);
	return 0;

err_pinctrl_get:
	if (gf_dev->pinctrl_gpios) {
        devm_pinctrl_put(gf_dev->pinctrl_gpios);
    }
err_pinctrl_lookup:
	gf_dev->gf_pwr_high = NULL;
	gf_dev->gf_pwr_low = NULL;
	gf_dev->gf_reset_high = NULL;
	gf_dev->gf_reset_low = NULL;
	return ret;
}

int gf_parse_dts(struct gf_dev *gf_dev)
{
#ifdef GF_PW_CTL
	int rc = 0;
	/*get pwr resource */
	gf_dev->pwr_gpio =
	    of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-pwr", 0);
	if (!gpio_is_valid(gf_dev->pwr_gpio)) {
		pr_info("PWR GPIO is invalid.\n");
		return FAIL;
	}
	rc = gpio_request(gf_dev->pwr_gpio, "goodix_pwr");
	if (rc) {
		dev_err(&gf_dev->spi->dev,
			"Failed to request PWR GPIO. rc = %d\n", rc);
		return FAIL;
	}
#endif

	gf_pinctrl_init(gf_dev);

	/*get reset resource */
	/*gf_dev->reset_gpio =
	    of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-reset", 0);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_info("RESET GPIO is invalid.\n");
		return -EPERM;
	}*/

/*
	gf_dev->irq_gpio =
	    of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-irq", 0);
	pr_info("gf::irq_gpio:%d\n", gf_dev->irq_gpio);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_info("IRQ GPIO is invalid.\n");
		return -EPERM;
	}
*/
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

	if (power_enable == 0)
		pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->gf_pwr_high);
	else
		pr_info("---- power already on, no need power on\n");
#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		rc = gpio_direction_output(gf_dev->pwr_gpio, 1);
		pr_info("---- power on result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
	mdelay(5);
	gpio_set_value(gf_dev->pwr_gpio, 1);
	pr_info("%s success\n", __func__);
#endif

	msleep(10);
	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;
	/*
	if (power_enable == 1)
		pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->gf_pwr_low);
	else
		pr_info("---- power already off, no need power off.\n");

#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		rc = gpio_direction_output(gf_dev->pwr_gpio, 0);
		pr_info("---- power off result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
#endif
*/
	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EPERM;
	}

	/*gpio_direction_output(gf_dev->reset_gpio, 1);
	mdelay(3);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);*/
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->gf_reset_low);
	mdelay(3);
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->gf_reset_high);
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
