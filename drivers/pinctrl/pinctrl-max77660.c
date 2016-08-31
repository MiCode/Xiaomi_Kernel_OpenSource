/*
 * pinctrl-max77660.c -- MAXIM MAX77660 PIN Control
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/mfd/max77660/max77660-core.h>

struct max77660_pinctrl {
	struct device		*dev;
	struct device		*parent;
};

struct max77660_pinctrl_config {
	int gpio_reg_add;
	int reg_offset;
	int pullup_mask;
	int pulldn_mask;
	int ame_mask;
	int od_mask;
};

#define MAX77660_PIN(_pin, _offset, _bit)				   \
[MAX77660_PINS_##_pin] = {						   \
				.gpio_reg_add = MAX77660_REG_CNFG_##_pin,  \
				.reg_offset = _offset,			   \
				.pullup_mask = BIT(_bit),		   \
				.pulldn_mask = BIT(_bit),		   \
				.ame_mask = BIT(_bit),			   \
				.od_mask = BIT(0),			   \
			}

struct max77660_pinctrl_config max77660_pin_data[MAX77660_PINS_MAX] = {
	MAX77660_PIN(GPIO0, 0, 0),
	MAX77660_PIN(GPIO1, 0, 1),
	MAX77660_PIN(GPIO2, 0, 2),
	MAX77660_PIN(GPIO3, 0, 3),
	MAX77660_PIN(GPIO4, 0, 4),
	MAX77660_PIN(GPIO5, 0, 5),
	MAX77660_PIN(GPIO6, 0, 6),
	MAX77660_PIN(GPIO7, 0, 7),
	MAX77660_PIN(GPIO8, 1, 0),
	MAX77660_PIN(GPIO9, 1, 1),
};

static int max77660_init_pin_gpio_mode(struct device *dev,
		unsigned offset, unsigned flag)
{
	struct device *parent = dev->parent;
	u8 val;
	int ret;

	if (flag & GPIOF_DIR_IN)
		return 0;

	if (flag & GPIOF_INIT_HIGH)
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_HIGH;
	else
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_LOW;

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_CNFG_GPIO0 + offset,
			val, MAX77660_CNFG_GPIO_OUTPUT_VAL_MASK);
	if (ret < 0) {
		dev_err(dev, "CNFG_GPIOx val update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_CNFG_GPIO0 + offset, MAX77660_CNFG_GPIO_DIR_OUTPUT,
		MAX77660_CNFG_GPIO_DIR_MASK);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx dir update failed: %d\n", ret);
	return ret;
}

static int max77660_pinctrl_probe(struct platform_device *pdev)
{
	struct max77660_platform_data *pdata;
	struct device *parent = pdev->dev.parent;
	int ret;
	int i;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata || !pdata->num_pinctrl) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -EINVAL;
	}

	for (i = 0; i < pdata->num_pinctrl; ++i) {
		bool pup_enable;
		bool pdn_enable;
		struct max77660_pinctrl_config *pcfg;
		struct max77660_pinctrl_platform_data *pctrl_pdata;

		pctrl_pdata =  &pdata->pinctrl_pdata[i];
		pcfg =  &max77660_pin_data[pctrl_pdata->pin_id];

		if (pctrl_pdata->pullup_dn_normal == MAX77660_PIN_DEFAULT)
			goto skip_pup_dn;

		pup_enable = false;
		pdn_enable = false;
		if (pctrl_pdata->pullup_dn_normal == MAX77660_PIN_PULL_DOWN)
			pdn_enable = true;
		if (pctrl_pdata->pullup_dn_normal == MAX77660_PIN_PULL_UP)
			pup_enable = true;

		if (pdn_enable)
			ret = max77660_reg_set_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_PDE1_GPIO + pcfg->reg_offset,
				pcfg->pulldn_mask);
		else
			ret = max77660_reg_clr_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_PDE1_GPIO + pcfg->reg_offset,
				pcfg->pulldn_mask);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"PDEx_GPIO update failed: %d\n", ret);
			return ret;
		}

		if (pup_enable)
			ret = max77660_reg_set_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_PUE1_GPIO + pcfg->reg_offset,
				pcfg->pullup_mask);
		else
			ret = max77660_reg_clr_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_PUE1_GPIO + pcfg->reg_offset,
				pcfg->pullup_mask);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"PUEx_GPIO update failed: %d\n", ret);
			return ret;
		}

skip_pup_dn:
		if (pctrl_pdata->open_drain)
			ret = max77660_reg_clr_bits(parent, MAX77660_PWR_SLAVE,
					pcfg->gpio_reg_add, pcfg->od_mask);
		else
			ret = max77660_reg_set_bits(parent, MAX77660_PWR_SLAVE,
					pcfg->gpio_reg_add, pcfg->od_mask);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"CNFG_GPIOx update failed: %d\n", ret);
			return ret;
		}

		if (pctrl_pdata->gpio_pin_mode) {
			ret = max77660_init_pin_gpio_mode(&pdev->dev, i,
						pctrl_pdata->gpio_init_flag);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"Pin init failed: %d\n", ret);
				return ret;
			}
			ret = max77660_reg_clr_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_AME1_GPIO + pcfg->reg_offset,
				pcfg->ame_mask);
		} else {
			ret = max77660_reg_set_bits(parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_AME1_GPIO + pcfg->reg_offset,
				pcfg->ame_mask);
		}
		if (ret < 0) {
			dev_err(&pdev->dev,
				"AMEx_GPIO update failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static struct platform_driver max77660_pinctrl_driver = {
	.probe = max77660_pinctrl_probe,
	.driver = {
		.name = "max77660-pinctrl",
		.owner = THIS_MODULE,
	},
};

static int __init max77660_pinctrl_init(void)
{
	return platform_driver_register(&max77660_pinctrl_driver);
}
subsys_initcall(max77660_pinctrl_init);

static void __exit max77660_pinctrl_exit(void)
{
	platform_driver_unregister(&max77660_pinctrl_driver);
}
module_exit(max77660_pinctrl_exit);

MODULE_DESCRIPTION("max77660 PinControl driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-pinctrl");
MODULE_LICENSE("GPL v2");
