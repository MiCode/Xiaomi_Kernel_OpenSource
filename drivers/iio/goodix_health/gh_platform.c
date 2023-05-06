#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gh_common.h"

/*GPIO pins reference.*/
int gh_get_gpio_dts_info(struct gh_device *gh_dev)
{
	int rc = 0;

#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	struct device_node *np = gh_dev->spi->dev.of_node;
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	struct device_node *np = gh_dev->client->dev.of_node;
#endif
	/*get pwr resource*/
/*	gh_dev->cs_gpio = of_get_named_gpio(gh_dev->spi->dev.of_node, "goodix,gpio_pwr", 0);
	if (!gpio_is_valid(gh_dev->cs_gpio)) {
		gh_debug(ERR_LOG, "%s, PWR GPIO is invalid.\n", __func__);
		return -1;
	}
	gh_debug(DEBUG_LOG, "%s, gh:goodix_pwr:%d\n", __func__, gh_dev->cs_gpio);
	rc = gpio_request(gh_dev->cs_gpio, "goodix_pwr");
	if (rc) {
		gh_debug(ERR_LOG, "%s, Failed to request PWR GPIO. rc = %d\n", __func__, rc);
		return -1;
	}
*/
	/*get vdd boost en resource*/
	gh_dev->boost_en_gpio = of_get_named_gpio(np,GH_GPIO_BOOST_EN, 0);
	if(!gpio_is_valid(gh_dev->boost_en_gpio)) {
		gh_debug(ERR_LOG, "%s, BOOST EN GPIO is invalid.\n", __func__);
		return -1;
	}
	rc = gpio_request(gh_dev->boost_en_gpio, "goodix_boost_en");
	if(rc) {
		gh_debug(ERR_LOG, "%s, Failed to request BOOST EN GPIO. rc = %d\n", __func__, rc);
		return -1;
	}
	gpio_direction_output(gh_dev->boost_en_gpio, 1);
	/*get reset resource*/
	gh_dev->reset_gpio = of_get_named_gpio(np,GH_GPIO_RESET,0);
	if(!gpio_is_valid(gh_dev->reset_gpio)) {
		gh_debug(ERR_LOG, "%s, RESET GPIO is invalid.\n", __func__);
		return -1;
	}
	rc = gpio_request(gh_dev->reset_gpio, "goodix_reset");
	if(rc) {
		gh_debug(ERR_LOG, "%s, Failed to request RESET GPIO. rc = %d\n", __func__, rc);
		return -1;
	}
	gpio_direction_output(gh_dev->reset_gpio, 1);

	/*get irq resourece*/
	gh_dev->irq_gpio = of_get_named_gpio(np,GH_GPIO_IRQ,0);
	gh_debug(DEBUG_LOG, "%s, gh:irq_gpio:%d\n", __func__, gh_dev->irq_gpio);
	if(!gpio_is_valid(gh_dev->irq_gpio)) {
		gh_debug(ERR_LOG, "%s, IRQ GPIO is invalid.\n", __func__);
		return -1;
	}

	rc = gpio_request(gh_dev->irq_gpio, "goodix_irq");
	if (rc) {
		gh_debug(ERR_LOG, "%s, Failed to request IRQ GPIO. rc = %d\n", __func__, rc);
		return -1;
	}
	gpio_direction_input(gh_dev->irq_gpio);

	return 0;
}

void gh_cleanup_info(struct gh_device *gh_dev)
{
	if (gpio_is_valid(gh_dev->irq_gpio)) {
		gpio_free(gh_dev->irq_gpio);
		gh_debug(DEBUG_LOG, "%s, remove irq_gpio success\n", __func__);
	}
	if (gpio_is_valid(gh_dev->reset_gpio)) {
		gpio_free(gh_dev->reset_gpio);
		gh_debug(DEBUG_LOG, "%s, remove reset_gpio success\n", __func__);
	}
/*	if (gpio_is_valid(gh_dev->cs_gpio)) {
		gpio_free(gh_dev->cs_gpio);
		gh_debug(DEBUG_LOG, "%s, remove reset_gpio success\n", __func__);
	}
*/
}

static void gh_hw_power_enable_common(struct device *dev, const char *name, gh_power_cfg *power_cfg, u8 onoff)
{
	/* TODO: LDO configure */
	int rc = 0;
	struct regulator *vreg;

	vreg = regulator_get(dev, name);
	if (vreg == NULL) {
		dev_err(dev, "%s regulator get failed!\n", name);
		goto exit;
	}
	if(onoff) {
		if (regulator_is_enabled(vreg)) {
			pr_info("%s is already enabled!\n", name);
		} else {
#if (1 == GH_CUSTOMIZATION_POWER)
			rc = regulator_set_load(vreg, power_cfg->load_uA);
#endif
			if (rc) {
				 dev_err(dev, "error set %s load!\n", name);
				 regulator_put(vreg);
				 vreg = NULL;
				 goto exit;
			}
			rc = regulator_set_voltage(vreg, power_cfg->min_uV, power_cfg->max_uV);
			 if (rc) {
				 dev_err(dev, "error set %s voltage!\n", name);
				 regulator_put(vreg);
				 vreg = NULL;
				 goto exit;
			}
			rc = regulator_enable(vreg);
			if (rc) {
				dev_err(dev, "error enabling %s!\n", name);
				regulator_put(vreg);
				vreg = NULL;
				goto exit;
			}
		}
	}
	else{
		if (!regulator_is_enabled(vreg)) {
			pr_info("%s is already disabled!\n", name);
		} else {
			rc = regulator_disable(vreg);
			if (rc) {
				dev_err(dev, "error disabling %s!\n", name);
				regulator_put(vreg);
				vreg = NULL;
				goto exit;
			}
		}
	}

exit:
	return;
}

void gh_hw_power_enable(struct gh_device *gh_dev, u8 onoff)
{
#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
	struct device *dev = &gh_dev->spi->dev;
#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)
	struct device *dev = &gh_dev->client->dev;
#endif

	gh_hw_power_enable_common(dev, GH_POWER_VDD, &g_vdd_cfg, onoff);
	gh_hw_power_enable_common(dev, GH_POWER_VDD_IO, &g_vdd_io_cfg, onoff);
}

void gh_hw_reset(struct gh_device *gh_dev, u8 delay)
{
	if(gh_dev == NULL) {
		gh_debug(ERR_LOG, "%s, Input buff is NULL.\n", __func__);
		return;
	}
	gpio_direction_output(gh_dev->reset_gpio, 1);
	gpio_set_value(gh_dev->reset_gpio, 0);
	mdelay(3);
	gpio_set_value(gh_dev->reset_gpio, 1);
	mdelay(delay);
	return;
}

void gh_hw_set_reset_value(struct gh_device *gh_dev, u8 value)
{
	gpio_direction_output(gh_dev->reset_gpio, 1);
	gpio_set_value(gh_dev->reset_gpio, value);
	return;
}

void gh_irq_gpio_cfg(struct gh_device *gh_dev)
{
	if (gh_dev == NULL) {
		gh_debug(ERR_LOG, "%s, Input buff is NULL.\n", __func__);
		return;
	}
	gh_dev->irq = gpio_to_irq(gh_dev->irq_gpio);
}
