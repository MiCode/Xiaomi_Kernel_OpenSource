/*
 * MAXIM MAX77663 GPIO driver
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/mfd/max77663-core.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

/* GPIO control registers */
#define MAX77663_REG_GPIO_IRQ		0x0A
#define MAX77663_REG_GPIO_CTRL0		0x36
#define MAX77663_REG_GPIO_CTRL1		0x37
#define MAX77663_REG_GPIO_CTRL2		0x38
#define MAX77663_REG_GPIO_CTRL3		0x39
#define MAX77663_REG_GPIO_CTRL4		0x3A
#define MAX77663_REG_GPIO_CTRL5		0x3B
#define MAX77663_REG_GPIO_CTRL6		0x3C
#define MAX77663_REG_GPIO_CTRL7		0x3D
#define MAX77663_REG_GPIO_PU		0x3E
#define MAX77663_REG_GPIO_PD		0x3F
#define MAX77663_REG_GPIO_ALT		0x40

#define GPIO_REG_ADDR(offset) (MAX77663_REG_GPIO_CTRL0 + offset)

#define GPIO_CTRL_DBNC_MASK		(3 << 6)
#define GPIO_CTRL_DBNC_SHIFT		6
#define GPIO_CTRL_REFE_IRQ_MASK		(3 << 4)
#define GPIO_CTRL_REFE_IRQ_SHIFT	4
#define GPIO_CTRL_DOUT_MASK		(1 << 3)
#define GPIO_CTRL_DOUT_SHIFT		3
#define GPIO_CTRL_DIN_MASK		(1 << 2)
#define GPIO_CTRL_DIN_SHIFT		2
#define GPIO_CTRL_DIR_MASK		(1 << 1)
#define GPIO_CTRL_DIR_SHIFT		1
#define GPIO_CTRL_OUT_DRV_MASK		(1 << 0)
#define GPIO_CTRL_OUT_DRV_SHIFT		0
#define GPIO_DBNC_NONE			0
#define GPIO_DBNC_8MS			1
#define GPIO_DBNC_16MS			2
#define GPIO_DBNC_32MS			3

#define MAX77663_GPIO_IRQ(n) (MAX77663_IRQ_GPIO0 + n)

#define GPIO_REFE_IRQ_NONE		0
#define GPIO_REFE_IRQ_EDGE_FALLING	1
#define GPIO_REFE_IRQ_EDGE_RISING	2
#define GPIO_REFE_IRQ_EDGE_BOTH		3

struct max77663_gpio {
	struct gpio_chip	gpio_chip;
	struct irq_chip		irq_chip;
	struct device		*parent;
	struct device		*dev;
	struct			mutex irq_lock;
	int			gpio_irq;
	int			irq_base;
	int			gpio_base;
	unsigned int		trigger_type[MAX77663_GPIO_NR];
	u8			cache_gpio_ctrl[MAX77663_GPIO_NR];
	u8			cache_gpio_pu;
	u8			cache_gpio_pd;
	u8			cache_gpio_alt;
};

static struct max77663_gpio *max77663_gpio_chip;

static inline struct max77663_gpio *to_max77663_gpio(struct gpio_chip *gpio)
{
	return container_of(gpio, struct max77663_gpio, gpio_chip);
}

static inline int max77663_cache_write(struct device *dev, u8 addr, u8 mask,
			u8 val, u8 *cache)
{
	u8 new_val;
	int ret;

	new_val = (*cache & ~mask) | (val & mask);
	if (*cache != new_val) {
		ret = max77663_write(dev, addr, &new_val, 1, 0);
		if (ret < 0)
			return ret;
		*cache = new_val;
	}
	return 0;
}

static int max77663_gpio_set_pull_up(struct max77663_gpio *max77663_gpio,
		int offset, int pull_up)
{
	u8 val = 0;

	if ((offset < MAX77663_GPIO0) || (MAX77663_GPIO7 < offset))
		return -EINVAL;

	if (pull_up == GPIO_PU_ENABLE)
		val = (1 << offset);

	return max77663_cache_write(max77663_gpio->parent, MAX77663_REG_GPIO_PU,
			(1 << offset), val, &max77663_gpio->cache_gpio_pu);
}

static int max77663_gpio_set_pull_down(struct max77663_gpio *max77663_gpio,
		int offset, int pull_down)
{
	u8 val = 0;

	if ((offset < MAX77663_GPIO0) || (MAX77663_GPIO7 < offset))
		return -EINVAL;

	if (pull_down == GPIO_PD_ENABLE)
		val = (1 << offset);

	return max77663_cache_write(max77663_gpio->parent, MAX77663_REG_GPIO_PD,
			(1 << offset), val, &max77663_gpio->cache_gpio_pd);
}

static inline int max77663_gpio_is_alternate(
		struct max77663_gpio *max77663_gpio, int offset)
{
	return (max77663_gpio->cache_gpio_alt & (1 << offset)) ? 1 : 0;
}

static int max77663_gpio_config_alternate(int gpio, int alternate)
{
	struct max77663_gpio *max77663_gpio = max77663_gpio_chip;
	u8 val = 0;
	int ret = 0;

	if (!max77663_gpio)
		return -ENXIO;

	gpio -= max77663_gpio->gpio_base;
	if ((gpio < MAX77663_GPIO0) || (MAX77663_GPIO7 < gpio))
		return -EINVAL;

	if (alternate == GPIO_ALT_ENABLE) {
		val = (1 << gpio);
		if (gpio == MAX77663_GPIO7) {
			ret = max77663_gpio_set_pull_up(max77663_gpio, gpio, 0);
			if (ret < 0)
				return ret;

			ret = max77663_gpio_set_pull_down(max77663_gpio,
						gpio, 0);
			if (ret < 0)
				return ret;
		}
	}

	return max77663_cache_write(max77663_gpio->parent,
			MAX77663_REG_GPIO_ALT,
			(1 << gpio), val, &max77663_gpio->cache_gpio_alt);
}

static int max77663_gpio_dir_input(struct gpio_chip *gpio, unsigned offset)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);

	if (max77663_gpio_is_alternate(max77663_gpio, offset)) {
		dev_warn(max77663_gpio->dev,
			"gpio%u is used as alternate mode\n", offset);
		return 0;
	}

	return max77663_cache_write(max77663_gpio->parent,
				GPIO_REG_ADDR(offset),
				GPIO_CTRL_DIR_MASK, GPIO_CTRL_DIR_MASK,
				&max77663_gpio->cache_gpio_ctrl[offset]);
}

static int max77663_gpio_get(struct gpio_chip *gpio, unsigned offset)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);
	u8 val;
	int ret;

	if (max77663_gpio_is_alternate(max77663_gpio, offset)) {
		dev_warn(max77663_gpio->dev,
			"gpio%u is used as alternate mode\n", offset);
		return 0;
	}

	ret = max77663_read(max77663_gpio->parent, GPIO_REG_ADDR(offset),
			&val, 1, 0);
	if (ret < 0)
		return ret;

	max77663_gpio->cache_gpio_ctrl[offset] = val;
	return (val & GPIO_CTRL_DIN_MASK) >> GPIO_CTRL_DIN_SHIFT;
}

static int max77663_gpio_dir_output(struct gpio_chip *gpio, unsigned offset,
				int value)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);
	u8 mask = GPIO_CTRL_DIR_MASK | GPIO_CTRL_DOUT_MASK;
	u8 val = (value ? 1 : 0) << GPIO_CTRL_DOUT_SHIFT;

	if (max77663_gpio_is_alternate(max77663_gpio, offset)) {
		dev_warn(max77663_gpio->dev,
			"gpio%u is used as alternate mode\n", offset);
		return 0;
	}

	return max77663_cache_write(max77663_gpio->parent,
			GPIO_REG_ADDR(offset), mask, val,
			&max77663_gpio->cache_gpio_ctrl[offset]);
}

static int max77663_gpio_set_debounce(struct gpio_chip *gpio,
		unsigned offset, unsigned debounce)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);
	u8 shift = GPIO_CTRL_DBNC_SHIFT;
	u8 val = 0;

	if (max77663_gpio_is_alternate(max77663_gpio, offset)) {
		dev_warn(max77663_gpio->dev,
			"gpio%u is used as alternate mode\n", offset);
		return 0;
	}

	if (debounce == 0)
		val = 0;
	else if ((0 < debounce) && (debounce <= 8))
		val = (GPIO_DBNC_8MS << shift);
	else if ((8 < debounce) && (debounce <= 16))
		val = (GPIO_DBNC_16MS << shift);
	else if ((16 < debounce) && (debounce <= 32))
		val = (GPIO_DBNC_32MS << shift);
	else
		return -EINVAL;

	return max77663_cache_write(max77663_gpio->parent,
			GPIO_REG_ADDR(offset),
			GPIO_CTRL_DBNC_MASK, val,
			&max77663_gpio->cache_gpio_ctrl[offset]);
}

static void max77663_gpio_set(struct gpio_chip *gpio, unsigned offset,
			int value)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);
	u8 val = (value ? 1 : 0) << GPIO_CTRL_DOUT_SHIFT;

	if (max77663_gpio_is_alternate(max77663_gpio, offset)) {
		dev_warn(max77663_gpio->dev,
			"gpio%u is used as alternate mode\n", offset);
		return;
	}

	max77663_cache_write(max77663_gpio->parent, GPIO_REG_ADDR(offset),
			GPIO_CTRL_DOUT_MASK, val,
			&max77663_gpio->cache_gpio_ctrl[offset]);
}

static int max77663_gpio_to_irq(struct gpio_chip *gpio, unsigned offset)
{
	struct max77663_gpio *max77663_gpio = to_max77663_gpio(gpio);

	return max77663_gpio->irq_base + offset;
}

static int max77663_gpio_set_config(struct max77663_gpio *max77663_gpio,
				struct max77663_gpio_config *gpio_cfg)
{
	int gpio = gpio_cfg->gpio;
	u8 val = 0, mask = 0;
	int ret = 0;

	if ((gpio < MAX77663_GPIO0) || (MAX77663_GPIO7 < gpio))
		return -EINVAL;

	if (gpio_cfg->pull_up != GPIO_PU_DEF) {
		ret = max77663_gpio_set_pull_up(max77663_gpio, gpio,
					gpio_cfg->pull_up);
		if (ret < 0) {
			dev_err(max77663_gpio->dev,
				"Failed to set gpio%d pull-up\n", gpio);
			return ret;
		}
	}

	if (gpio_cfg->pull_down != GPIO_PD_DEF) {
		ret = max77663_gpio_set_pull_down(max77663_gpio, gpio,
					gpio_cfg->pull_down);
		if (ret < 0) {
			dev_err(max77663_gpio->dev,
				"Failed to set gpio%d pull-down\n", gpio);
			return ret;
		}
	}

	if (gpio_cfg->dir != GPIO_DIR_DEF) {
		mask = GPIO_CTRL_DIR_MASK;
		if (gpio_cfg->dir == GPIO_DIR_IN) {
			val |= GPIO_CTRL_DIR_MASK;
		} else {
			if (gpio_cfg->dout != GPIO_DOUT_DEF) {
				mask |= GPIO_CTRL_DOUT_MASK;
				if (gpio_cfg->dout == GPIO_DOUT_HIGH)
					val |= GPIO_CTRL_DOUT_MASK;
			}

			if (gpio_cfg->out_drv != GPIO_OUT_DRV_DEF) {
				mask |= GPIO_CTRL_OUT_DRV_MASK;
				if (gpio_cfg->out_drv == GPIO_OUT_DRV_PUSH_PULL)
					val |= GPIO_CTRL_OUT_DRV_MASK;
			}
		}

		ret = max77663_cache_write(max77663_gpio->parent,
				GPIO_REG_ADDR(gpio), mask,
				val, &max77663_gpio->cache_gpio_ctrl[gpio]);
		if (ret < 0) {
			dev_err(max77663_gpio->dev,
				"Failed to set gpio%d control\n", gpio);
			return ret;
		}
	}

	if (gpio_cfg->alternate != GPIO_ALT_DEF) {
		ret = max77663_gpio_config_alternate(
				gpio + max77663_gpio->gpio_base,
				gpio_cfg->alternate);
		if (ret < 0) {
			dev_err(max77663_gpio->dev,
				"Failed to set gpio%d alternate\n", gpio);
			return ret;
		}
	}

	return 0;
}

static void max77663_gpio_irq_lock(struct irq_data *data)
{
	struct max77663_gpio *max77663_gpio = irq_data_get_irq_chip_data(data);

	mutex_lock(&max77663_gpio->irq_lock);
}

static void max77663_gpio_irq_sync_unlock(struct irq_data *data)
{
	struct max77663_gpio *max77663_gpio = irq_data_get_irq_chip_data(data);

	mutex_unlock(&max77663_gpio->irq_lock);
}

static void max77663_gpio_irq_mask(struct irq_data *data)
{
	struct max77663_gpio *max77663_gpio = irq_data_get_irq_chip_data(data);
	int offset = data->irq - max77663_gpio->irq_base;
	int ret;

	ret = max77663_cache_write(max77663_gpio->parent,
			GPIO_REG_ADDR(offset), 0x30, 0x0,
			&max77663_gpio->cache_gpio_ctrl[offset]);
	if (ret < 0)
		dev_err(max77663_gpio->dev,
			"gpio register write failed, e %d\n", ret);
}

static void max77663_gpio_irq_unmask(struct irq_data *data)
{
	struct max77663_gpio *max77663_gpio = irq_data_get_irq_chip_data(data);
	int irq_mask = GPIO_REFE_IRQ_EDGE_FALLING << GPIO_CTRL_REFE_IRQ_SHIFT;
	int offset = data->irq - max77663_gpio->irq_base;
	int ret;

	if (max77663_gpio->trigger_type[offset])
		irq_mask = max77663_gpio->trigger_type[offset];
	ret = max77663_cache_write(max77663_gpio->parent,
			GPIO_REG_ADDR(offset), 0x30, irq_mask,
			&max77663_gpio->cache_gpio_ctrl[offset]);
	if (ret < 0)
		dev_err(max77663_gpio->dev,
			"gpio register write failed, e %d\n", ret);
}

static int max77663_irq_gpio_set_type(struct irq_data *data, unsigned int type)
{
	struct max77663_gpio *max77663_gpio = irq_data_get_irq_chip_data(data);
	unsigned offset = data->irq - max77663_gpio->irq_base;
	u8 val;
	int ret;

	switch (type) {
	case IRQ_TYPE_NONE:
	case IRQ_TYPE_EDGE_FALLING:
		val = (GPIO_REFE_IRQ_EDGE_FALLING << GPIO_CTRL_REFE_IRQ_SHIFT);
		break;

	case IRQ_TYPE_EDGE_RISING:
		val = (GPIO_REFE_IRQ_EDGE_RISING << GPIO_CTRL_REFE_IRQ_SHIFT);
		break;

	case IRQ_TYPE_EDGE_BOTH:
		val = (GPIO_REFE_IRQ_EDGE_BOTH << GPIO_CTRL_REFE_IRQ_SHIFT);
		break;

	default:
		return -EINVAL;
	}

	max77663_gpio->trigger_type[offset] = type;
	ret = max77663_cache_write(max77663_gpio->parent,
			GPIO_REG_ADDR(offset), 0x30, val,
			&max77663_gpio->cache_gpio_ctrl[offset]);
	if (ret < 0)
		dev_err(max77663_gpio->dev,
				"gpio register write failed, e %d\n", ret);

	return ret;
}

static irqreturn_t max77663_gpio_isr(int irq, void *data)
{
	struct max77663_gpio *max77663_gpio = data;
	int ret;
	int i;
	u8 val;

	ret = max77663_read(max77663_gpio->dev, MAX77663_REG_GPIO_IRQ,
					&val, 1, 0);
	if (ret < 0) {
		dev_err(max77663_gpio->dev,
			"gpio irq reg read Failed %d\n", ret);
		return IRQ_NONE;
	}

	for (i = 0; i < MAX77663_GPIO_NR; ++i) {
		if (val & (1 << i))
			handle_nested_irq(max77663_gpio->irq_base + i);
	}
	return IRQ_HANDLED;
}

static int max77663_gpio_irq_init(struct max77663_gpio *max77663_gpio,
		struct max77663_platform_data *pdata)
{
	int i;
	u8 val;
	int ret;

	max77663_gpio->irq_base = pdata->irq_base + MAX77663_GPIO_IRQ(0);
	max77663_gpio->irq_chip.name = "max77663-gpio-irq";
	max77663_gpio->irq_chip.irq_mask = max77663_gpio_irq_mask;
	max77663_gpio->irq_chip.irq_unmask = max77663_gpio_irq_unmask;
	max77663_gpio->irq_chip.irq_set_type = max77663_irq_gpio_set_type;
	max77663_gpio->irq_chip.irq_bus_lock = max77663_gpio_irq_lock;
	max77663_gpio->irq_chip.irq_bus_sync_unlock =
					max77663_gpio_irq_sync_unlock;

	for (i = 0; i < MAX77663_GPIO_NR; ++i) {
		int irq = max77663_gpio->irq_base + i;
		irq_set_chip_data(irq, max77663_gpio);
		irq_set_chip_and_handler(irq, &max77663_gpio->irq_chip,
					handle_simple_irq);
		irq_set_nested_thread(irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(irq, IRQF_VALID);
#else
		irq_set_noprobe(irq);
#endif
	}

	/* IRQ_LVL2_GPIO is rea on clear */
	max77663_read(max77663_gpio->parent, MAX77663_REG_GPIO_IRQ, &val, 1, 0);

	ret = request_threaded_irq(max77663_gpio->gpio_irq, NULL,
			max77663_gpio_isr, IRQF_ONESHOT, "max77663-gpio-irq",
			max77663_gpio);
	if (ret < 0)
		dev_err(max77663_gpio->dev, "Failed to request irq %d, e %d\n",
			max77663_gpio->gpio_irq, ret);
	return ret;
}

static void max77663_gpio_irq_remove(struct max77663_gpio *max77663_gpio)
{
	int gpio;

	for (gpio = 0; gpio < MAX77663_GPIO_NR; ++gpio) {
		int irq = max77663_gpio->irq_base + gpio;
#ifdef CONFIG_ARM
		set_irq_flags(irq, 0);
#endif
		irq_set_chip_and_handler(irq, NULL, NULL);
		irq_set_chip_data(irq, NULL);
	}
	free_irq(max77663_gpio->gpio_irq, max77663_gpio);
}

static int max77663_gpio_init_regs(struct max77663_gpio *max77663_gpio,
		struct max77663_platform_data *pdata)
{
	int ret;
	int i;

	ret = max77663_read(max77663_gpio->parent, MAX77663_REG_GPIO_CTRL0,
			&max77663_gpio->cache_gpio_ctrl, MAX77663_GPIO_NR, 0);
	if (ret < 0) {
		dev_err(max77663_gpio->dev, "Failed to get gpio control\n");
		return ret;
	}

	ret = max77663_read(max77663_gpio->parent, MAX77663_REG_GPIO_PU,
			&max77663_gpio->cache_gpio_pu, 1, 0);
	if (ret < 0) {
		dev_err(max77663_gpio->dev, "Failed to get gpio pull-up\n");
		return ret;
	}

	ret = max77663_read(max77663_gpio->parent, MAX77663_REG_GPIO_PD,
			&max77663_gpio->cache_gpio_pd, 1, 0);
	if (ret < 0) {
		dev_err(max77663_gpio->dev, "Failed to get gpio pull-down\n");
		return ret;
	}

	ret = max77663_read(max77663_gpio->parent, MAX77663_REG_GPIO_ALT,
			&max77663_gpio->cache_gpio_alt, 1, 0);
	if (ret < 0) {
		dev_err(max77663_gpio->dev, "Failed to get gpio alternate\n");
		return ret;
	}

	for (i = 0; i < pdata->num_gpio_cfgs; i++) {
		ret = max77663_gpio_set_config(max77663_gpio,
					&pdata->gpio_cfgs[i]);
		if (ret < 0) {
			dev_err(max77663_gpio->dev,
				"Failed to set gpio config\n");
			return ret;
		}
	}

	return 0;
}

static int max77663_gpio_probe(struct platform_device *pdev)
{
	struct max77663_platform_data *pdata;
	struct max77663_gpio *max77663_gpio;
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

	max77663_gpio = devm_kzalloc(&pdev->dev,
				sizeof(*max77663_gpio), GFP_KERNEL);
	if (!max77663_gpio) {
		dev_err(&pdev->dev, "Could not allocate max77663_gpio\n");
		return -ENOMEM;
	}

	mutex_init(&max77663_gpio->irq_lock);
	max77663_gpio->parent = pdev->dev.parent;
	max77663_gpio->dev = &pdev->dev;
	max77663_gpio->gpio_irq = gpio_irq;

	max77663_gpio->gpio_chip.owner = THIS_MODULE;
	max77663_gpio->gpio_chip.label = pdev->name;
	max77663_gpio->gpio_chip.dev = &pdev->dev;
	max77663_gpio->gpio_chip.direction_input = max77663_gpio_dir_input;
	max77663_gpio->gpio_chip.get = max77663_gpio_get;
	max77663_gpio->gpio_chip.direction_output = max77663_gpio_dir_output;
	max77663_gpio->gpio_chip.set_debounce = max77663_gpio_set_debounce;
	max77663_gpio->gpio_chip.set = max77663_gpio_set;
	max77663_gpio->gpio_chip.to_irq = max77663_gpio_to_irq;
	max77663_gpio->gpio_chip.ngpio = MAX77663_GPIO_NR;
	max77663_gpio->gpio_chip.can_sleep = 1;
	if (pdata->gpio_base)
		max77663_gpio->gpio_chip.base = pdata->gpio_base;
	else
		max77663_gpio->gpio_chip.base = -1;

	max77663_gpio_chip = max77663_gpio;
	ret = max77663_gpio_init_regs(max77663_gpio, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio_init regs failed\n");
		return ret;
	}

	ret = gpiochip_add(&max77663_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio_init: Failed to add gpiomax77663_gpio\n");
		return ret;
	}
	max77663_gpio->gpio_base = max77663_gpio->gpio_chip.base;

	ret = max77663_gpio_irq_init(max77663_gpio, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio irq init failed, e %d\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, max77663_gpio);
	return 0;

fail:
	if (gpiochip_remove(&max77663_gpio->gpio_chip))
		dev_err(&pdev->dev, "%s gpiochip_remove failed\n", __func__);

	return ret;
}

static int max77663_gpio_remove(struct platform_device *pdev)
{
	struct max77663_gpio *max77663_gpio = platform_get_drvdata(pdev);

	max77663_gpio_irq_remove(max77663_gpio);
	max77663_gpio_chip = 0;

	return gpiochip_remove(&max77663_gpio->gpio_chip);
}

static struct platform_driver max77663_gpio_driver = {
	.driver.name	= "max77663-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= max77663_gpio_probe,
	.remove		= max77663_gpio_remove,
};

static int __init max77663_gpio_init(void)
{
	return platform_driver_register(&max77663_gpio_driver);
}
subsys_initcall(max77663_gpio_init);

static void __exit max77663_gpio_exit(void)
{
	platform_driver_unregister(&max77663_gpio_driver);
}
module_exit(max77663_gpio_exit);

MODULE_ALIAS("platform:max77663-gpio");
MODULE_DESCRIPTION("GPIO interface for MAX77663 PMIC");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
