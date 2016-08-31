/*
 * MAXIM MAX77660 GPIO driver
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 * Author: Laxman dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/regmap.h>

#define GPIO_REG_ADDR(offset) (MAX77660_REG_CNFG_GPIO0 + offset)

struct max77660_gpio {
	struct gpio_chip	gpio_chip;
	struct device		*parent;
	struct device		*dev;
	int			gpio_irq;
	int			irq_base;
	int			gpio_base;
};

static const struct regmap_irq max77660_gpio_irqs[] = {
	[MAX77660_IRQ_GPIO0 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE0,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 0,
	},
	[MAX77660_IRQ_GPIO1 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE1,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 1,
	},
	[MAX77660_IRQ_GPIO2 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE2,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 2,
	},
	[MAX77660_IRQ_GPIO3 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE3,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 3,
	},
	[MAX77660_IRQ_GPIO4 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE4,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 4,
	},
	[MAX77660_IRQ_GPIO5 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE5,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 5,
	},
	[MAX77660_IRQ_GPIO6 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE6,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 6,
	},
	[MAX77660_IRQ_GPIO7 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ1_LVL2_GPIO_EDGE7,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 0,
		.type_reg_offset = 7,
	},
	[MAX77660_IRQ_GPIO8 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ2_LVL2_GPIO_EDGE8,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 1,
		.type_reg_offset = 8,
	},
	[MAX77660_IRQ_GPIO9 - MAX77660_IRQ_GPIO0] = {
		.mask = MAX77660_IRQ2_LVL2_GPIO_EDGE9,
		.type_rising_mask = MAX77660_CNFG_GPIO_INT_RISING,
		.type_falling_mask = MAX77660_CNFG_GPIO_INT_FALLING,
		.reg_offset = 1,
		.type_reg_offset = 9,
	},
};

static struct regmap_irq_chip max77660_gpio_irq_chip = {
	.name = "max77660-gpio",
	.irqs = max77660_gpio_irqs,
	.num_irqs = ARRAY_SIZE(max77660_gpio_irqs),
	.num_regs = 2,
	.num_type_reg = 10,
	.irq_reg_stride = 1,
	.type_reg_stride = 1,
	.status_base = MAX77660_REG_IRQ1_LVL2_GPIO,
	.type_base = MAX77660_REG_CNFG_GPIO0,
};

static inline struct max77660_gpio *to_max77660_gpio(struct gpio_chip *gpio)
{
	return container_of(gpio, struct max77660_gpio, gpio_chip);
}

static int max77660_gpio_dir_input(struct gpio_chip *gpio, unsigned offset)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);
	struct device *dev = max77660_gpio->dev;
	struct device *parent = max77660_gpio->parent;
	int ret;

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
		GPIO_REG_ADDR(offset), MAX77660_CNFG_GPIO_DIR_INPUT,
		MAX77660_CNFG_GPIO_DIR_MASK);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx dir update failed: %d\n", ret);
	return ret;
}

static int max77660_gpio_get(struct gpio_chip *gpio, unsigned offset)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);
	struct device *dev = max77660_gpio->dev;
	struct device *parent = max77660_gpio->parent;
	u8 val;
	int ret;

	ret = max77660_reg_read(parent, MAX77660_PWR_SLAVE,
				GPIO_REG_ADDR(offset), &val);
	if (ret < 0) {
		dev_err(dev, "CNFG_GPIOx read failed: %d\n", ret);
		return ret;
	}

	return !!(val & MAX77660_CNFG_GPIO_INPUT_VAL_MASK);
}

static int max77660_gpio_dir_output(struct gpio_chip *gpio, unsigned offset,
				int value)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);
	struct device *dev = max77660_gpio->dev;
	struct device *parent = max77660_gpio->parent;
	u8 val;
	int ret;

	if (value)
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_HIGH;
	else
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_LOW;

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
			GPIO_REG_ADDR(offset),
			val, MAX77660_CNFG_GPIO_OUTPUT_VAL_MASK);
	if (ret < 0) {
		dev_err(dev, "CNFG_GPIOx val update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
		GPIO_REG_ADDR(offset), MAX77660_CNFG_GPIO_DIR_OUTPUT,
		MAX77660_CNFG_GPIO_DIR_MASK);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx dir update failed: %d\n", ret);
	return ret;
}


static int max77660_gpio_set_debounce(struct gpio_chip *gpio,
		unsigned offset, unsigned debounce)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);
	struct device *dev = max77660_gpio->dev;
	struct device *parent = max77660_gpio->parent;
	u8 val;
	int ret;

	/* ES 1.0 errata: GPIO1 debaunce does not worked in ES1.0 */
	if (max77660_is_es_1_0(parent) && (offset == 1)) {
		dev_err(dev, "ES1.0: GPIO1 Debaunce is not supported in HW\n");
		return -EINVAL;
	}

	if (debounce == 0)
		val = MAX77660_CNFG_GPIO_DBNC_None;
	else if ((0 < debounce) && (debounce <= 8))
		val = MAX77660_CNFG_GPIO_DBNC_8ms;
	else if ((8 < debounce) && (debounce <= 16))
		val = MAX77660_CNFG_GPIO_DBNC_16ms;
	else if ((16 < debounce) && (debounce <= 32))
		val = MAX77660_CNFG_GPIO_DBNC_32ms;
	else {
		dev_err(dev, "%s(): illegal value %u\n", __func__, debounce);
		return -EINVAL;
	}

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
		GPIO_REG_ADDR(offset), val, MAX77660_CNFG_GPIO_DBNC_MASK);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx debounce update failed: %d\n", ret);
	return ret;
}

static void max77660_gpio_set(struct gpio_chip *gpio, unsigned offset,
			int value)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);
	struct device *dev = max77660_gpio->dev;
	struct device *parent = max77660_gpio->parent;
	u8 val;
	int ret;

	if (value)
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_HIGH;
	else
		val = MAX77660_CNFG_GPIO_OUTPUT_VAL_LOW;

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
			GPIO_REG_ADDR(offset), val,
			MAX77660_CNFG_GPIO_OUTPUT_VAL_MASK);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx val update failed: %d\n", ret);
}

static int max77660_gpio_to_irq(struct gpio_chip *gpio, unsigned offset)
{
	struct max77660_gpio *max77660_gpio = to_max77660_gpio(gpio);

	return max77660_gpio->irq_base + offset;
}

static int max77660_gpio_irq_init(struct max77660_gpio *max77660_gpio,
		struct max77660_platform_data *pdata)
{
	struct device *parent = max77660_gpio->parent;
	struct max77660_chip *chip = dev_get_drvdata(parent);
	int ret;

	max77660_gpio->irq_base = pdata->irq_base + MAX77660_IRQ_GPIO0;
	ret = regmap_add_irq_chip(chip->rmap[MAX77660_PWR_SLAVE],
		max77660_gpio->gpio_irq, IRQF_ONESHOT,
		max77660_gpio->irq_base,
		&max77660_gpio_irq_chip, &chip->gpio_irq_data);
	if (ret < 0) {
		dev_err(max77660_gpio->dev,
			"Failed to add gpio irq_chip %d\n", ret);
		return ret;
	}
	return 0;
}

static void max77660_gpio_irq_remove(struct max77660_gpio *max77660_gpio)
{
	struct max77660_chip *chip;

	chip = dev_get_drvdata(max77660_gpio->dev->parent);
	regmap_del_irq_chip(max77660_gpio->gpio_irq,
			chip->gpio_irq_data);
	chip->gpio_irq_data = NULL;
}

static int max77660_gpio_probe(struct platform_device *pdev)
{
	struct max77660_platform_data *pdata;
	struct max77660_gpio *max77660_gpio;
	int ret;
	int gpio_irq;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "Platform data not found\n");
		return -ENODEV;
	}

	gpio_irq = platform_get_irq(pdev, 0);
	if (gpio_irq <= 0) {
		dev_err(&pdev->dev, "Gpio interrupt is not available\n");
		return -ENODEV;
	}

	max77660_gpio = devm_kzalloc(&pdev->dev,
				sizeof(*max77660_gpio), GFP_KERNEL);
	if (!max77660_gpio) {
		dev_err(&pdev->dev, "Could not allocate max77660_gpio\n");
		return -ENOMEM;
	}

	max77660_gpio->parent = pdev->dev.parent;
	max77660_gpio->dev = &pdev->dev;
	max77660_gpio->gpio_irq = gpio_irq;

	max77660_gpio->gpio_chip.owner = THIS_MODULE;
	max77660_gpio->gpio_chip.label = pdev->name;
	max77660_gpio->gpio_chip.dev = &pdev->dev;
	max77660_gpio->gpio_chip.direction_input = max77660_gpio_dir_input;
	max77660_gpio->gpio_chip.get = max77660_gpio_get;
	max77660_gpio->gpio_chip.direction_output = max77660_gpio_dir_output;
	max77660_gpio->gpio_chip.set_debounce = max77660_gpio_set_debounce;
	max77660_gpio->gpio_chip.set = max77660_gpio_set;
	max77660_gpio->gpio_chip.to_irq = max77660_gpio_to_irq;
	max77660_gpio->gpio_chip.ngpio = MAX77660_GPIO_NR;
	max77660_gpio->gpio_chip.can_sleep = 1;
	if (pdata->gpio_base)
		max77660_gpio->gpio_chip.base = pdata->gpio_base;
	else
		max77660_gpio->gpio_chip.base = -1;

	platform_set_drvdata(pdev, max77660_gpio);

	ret = gpiochip_add(&max77660_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio_init: Failed to add max77660_gpio\n");
		return ret;
	}
	max77660_gpio->gpio_base = max77660_gpio->gpio_chip.base;

	ret = max77660_gpio_irq_init(max77660_gpio, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio irq init failed: %d\n", ret);
		goto fail;
	}

	return 0;

fail:
	ret = gpiochip_remove(&max77660_gpio->gpio_chip);
	return ret;
}

static int max77660_gpio_remove(struct platform_device *pdev)
{
	struct max77660_gpio *max77660_gpio = platform_get_drvdata(pdev);
	int ret;

	max77660_gpio_irq_remove(max77660_gpio);

	ret = gpiochip_remove(&max77660_gpio->gpio_chip);
	if (ret < 0)
		dev_err(max77660_gpio->dev,
			"gpiochip_remove failed: %d\n", ret);
	return ret;
}

static struct platform_driver max77660_gpio_driver = {
	.driver.name	= "max77660-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= max77660_gpio_probe,
	.remove		= max77660_gpio_remove,
};

static int __init max77660_gpio_init(void)
{
	return platform_driver_register(&max77660_gpio_driver);
}
subsys_initcall(max77660_gpio_init);

static void __exit max77660_gpio_exit(void)
{
	platform_driver_unregister(&max77660_gpio_driver);
}
module_exit(max77660_gpio_exit);

MODULE_DESCRIPTION("GPIO interface for MAX77660 PMIC");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Maxim Integrated");
MODULE_ALIAS("platform:max77660-gpio");
