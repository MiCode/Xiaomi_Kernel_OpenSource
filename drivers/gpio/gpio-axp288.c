/*
 * gpio-axp288.c - AXP288 GPIO Driver
 *
 * Copyright (C) 2012, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Iyer, Yegnesh S <yegnesh.s.iyer@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/gpio.h>

#define AXP288_GPIOIRQ	0x44
#define AXP288_GPIOIRQ_STAT 0x4C
#define AXP288_GPIO0CTL 0x90
#define AXP288_GPIO1CTL 0x92
#define AXP288_GPIO_SIGNAL 0x94

#define AXP288_GPIOIRQ_MASK 0x03
#define AXP288_GPIO_EDGE_BOTH_ENABLE 0xC0
#define AXP288_GPIO_EDGE_FALLING_ENABLE 0x40
#define AXP288_GPIO_EDGE_RISING_ENABLE 0x80

#define AXP288_GPIO_CTL_MASK 0xF8
#define AXP288_GPIO_OUT_DRIVE_MASK 0x01
#define AXP288_GPIO_CTL_INPUT 0x02

static struct pmic_gpio gpio_info;

static void __axp288_irq_mask(int gpio, int mask)
{
	if (mask)
		intel_soc_pmic_setb(AXP288_GPIOIRQ, 1 << gpio);
	else
		intel_soc_pmic_clearb(AXP288_GPIOIRQ, 1 << gpio);
}

static void __axp288_irq_type(int gpio, int type)
{
	u8 ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;

	type &= IRQ_TYPE_EDGE_BOTH;

	intel_soc_pmic_clearb(ctl, AXP288_GPIO_EDGE_BOTH_ENABLE);
	if (type == IRQ_TYPE_EDGE_BOTH)
		intel_soc_pmic_setb(ctl, AXP288_GPIO_EDGE_BOTH_ENABLE);
	else if (type == IRQ_TYPE_EDGE_RISING)
		intel_soc_pmic_setb(ctl, AXP288_GPIO_EDGE_RISING_ENABLE);
	else if (type & IRQ_TYPE_EDGE_FALLING)
		intel_soc_pmic_setb(ctl, AXP288_GPIO_EDGE_FALLING_ENABLE);
}

static int axp288_gpio_direction_input(struct gpio_chip *chip,
					    unsigned gpio)
{
	u8 ctl;
	u8 reg;

	if (gpio > gpio_info.gpio_data->num_gpio)
		return 0;

	ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;

	reg = intel_soc_pmic_readb(ctl);
	reg &= AXP288_GPIO_CTL_MASK;
	reg |= AXP288_GPIO_CTL_INPUT;
	intel_soc_pmic_writeb(ctl, reg);

	return 0;
}

static int axp288_gpio_direction_output(struct gpio_chip *chip,
					     unsigned gpio, int value)
{
	u8 ctl;
	u8 reg;

	if (gpio > gpio_info.gpio_data->num_gpio)
		return 0;

	ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;
	reg = intel_soc_pmic_readb(ctl);
	reg &= AXP288_GPIO_CTL_MASK;
	reg |= (value & AXP288_GPIO_OUT_DRIVE_MASK);
	intel_soc_pmic_writeb(ctl, reg);

	return 0;
}

static int axp288_gpio_get(struct gpio_chip *chip, unsigned gpio)
{
	u8 ctl;
	u8 reg;
	u8 val;

	if (gpio > gpio_info.gpio_data->num_gpio)
		return 0;

	ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;
	if (ctl & AXP288_GPIO_CTL_INPUT) {
		val = intel_soc_pmic_readb(AXP288_GPIO_SIGNAL) & (1 << gpio);
		return val >> gpio;
	} else
		return intel_soc_pmic_readb(ctl) & AXP288_GPIO_OUT_DRIVE_MASK;
}

static void axp288_gpio_set(struct gpio_chip *chip,
				 unsigned gpio, int value)
{
	u8 ctl;
	u8 reg;

	if (gpio > gpio_info.gpio_data->num_gpio)
		return;

	ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;
	reg = intel_soc_pmic_readb(ctl);
	reg &= AXP288_GPIO_CTL_MASK;
	reg |= (value & AXP288_GPIO_OUT_DRIVE_MASK);
	intel_soc_pmic_writeb(ctl, reg);

	return;
}

static int axp288_irq_type(struct irq_data *data, unsigned type)
{
	struct pmic_gpio *cg = irq_data_get_irq_chip_data(data);

	cg->trigger_type = type;
	cg->update |= UPDATE_TYPE;

	return 0;
}

static int axp288_gpio_to_irq(struct gpio_chip *chip, unsigned gpio)
{
	struct pmic_gpio *cg =
		container_of(chip, struct pmic_gpio, chip);

	if (gpio > cg->gpio_data->num_gpio)
		return -1;

	return cg->irq_base + gpio;
}

static void axp288_bus_lock(struct irq_data *data)
{
	struct pmic_gpio *cg = irq_data_get_irq_chip_data(data);

	mutex_lock(&cg->buslock);
}

static void axp288_bus_sync_unlock(struct irq_data *data)
{
	struct pmic_gpio *cg = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - cg->irq_base;

	if (cg->update & UPDATE_TYPE)
		__axp288_irq_type(gpio, cg->trigger_type);
	if (cg->update & UPDATE_MASK)
		__axp288_irq_mask(gpio, cg->irq_mask);
	cg->update = 0;

	mutex_unlock(&cg->buslock);
}

static void axp288_irq_unmask(struct irq_data *data)
{
	struct pmic_gpio *cg = irq_data_get_irq_chip_data(data);

	cg->irq_mask = 0;
	cg->update |= UPDATE_MASK;
}

static void axp288_irq_mask(struct irq_data *data)
{
	struct pmic_gpio *cg = irq_data_get_irq_chip_data(data);

	cg->irq_mask = 1;
	cg->update |= UPDATE_MASK;
}

static struct irq_chip axp288_irqchip = {
	.name			= "PMIC-GPIO",
	.irq_mask		= axp288_irq_mask,
	.irq_unmask		= axp288_irq_unmask,
	.irq_set_type		= axp288_irq_type,
	.irq_bus_lock		= axp288_bus_lock,
	.irq_bus_sync_unlock	= axp288_bus_sync_unlock,
};

static irqreturn_t axp288_gpio_irq_handler(int irq, void *data)
{
	struct pmic_gpio *cg = data;
	int pending;
	int gpio;

	pending = intel_soc_pmic_readb(AXP288_GPIOIRQ_STAT) &
			AXP288_GPIOIRQ_MASK;
	intel_soc_pmic_setb(AXP288_GPIOIRQ_STAT,
				pending & AXP288_GPIOIRQ_MASK);

	local_irq_disable();
	for (gpio = 0; gpio < cg->gpio_data->num_gpio; gpio++)
		if (pending & (1 << gpio))
			generic_handle_irq(cg->irq_base + gpio);
	local_irq_enable();

	return IRQ_HANDLED;
}

static void axp288_gpio_dbg_show(struct seq_file *s,
				      struct gpio_chip *chip)
{
	int gpio;
	u8 ctl;
	u8 mask;
	u8 irq;
	u8 dir;
	u8 value;
	u8 reg;

	irq = intel_soc_pmic_readb(AXP288_GPIOIRQ_STAT);
	mask = intel_soc_pmic_readb(AXP288_GPIOIRQ);
	for (gpio = 0; gpio < gpio_info.gpio_data->num_gpio; gpio++) {
		ctl = gpio ? AXP288_GPIO1CTL : AXP288_GPIO0CTL;
		reg = intel_soc_pmic_readb(ctl);
		dir = (reg & AXP288_GPIO_CTL_INPUT);
		if (dir)
			value = (intel_soc_pmic_readb(AXP288_GPIO_SIGNAL) >>
					gpio);
		else
			value = (reg & AXP288_GPIO_OUT_DRIVE_MASK);

		seq_printf(s, " gpio-%-2d %s %s %s %s %s %s",
				gpio, dir ? "in " : "out",
				value ? "hi" : "lo",
				ctl & AXP288_GPIO_EDGE_FALLING_ENABLE ?
					"fall" : "    ",
				ctl & AXP288_GPIO_EDGE_RISING_ENABLE ?
					"rise" : "    ",
				mask & (1 << gpio) ? "s0 mask  " : "s0 unmask",
				irq & (1 << gpio) ? "pending" : "       ");
		seq_printf(s, "\n");
	}
}

static int axp288_gpio_probe(struct platform_device *pdev)
{
	int irq = platform_get_irq(pdev, 0);
	struct pmic_gpio *cg = &gpio_info;
	int retval;
	int i;
	struct device *dev = intel_soc_pmic_dev();
	struct pmic_gpio_data *gpio_data;

	gpio_data = (struct pmic_gpio_data *)pdev->dev.platform_data;

	mutex_init(&cg->buslock);
	cg->chip.label = "axp288-gpio";
	cg->chip.direction_input = axp288_gpio_direction_input;
	cg->chip.direction_output = axp288_gpio_direction_output;
	cg->chip.get = axp288_gpio_get;
	cg->chip.set = axp288_gpio_set;
	cg->chip.to_irq = axp288_gpio_to_irq;
	cg->chip.base = -1;
	cg->gpio_data = gpio_data;
	cg->chip.ngpio = cg->gpio_data->num_vgpio;
	cg->chip.can_sleep = 1;
	cg->chip.dev = dev;
	cg->chip.dbg_show = axp288_gpio_dbg_show;

	retval = gpiochip_add(&cg->chip);
	if (retval) {
		dev_warn(&pdev->dev, "add gpio chip error: %d\n", retval);
		return retval;
	}

	cg->irq_base = irq_alloc_descs(-1, INTEL_PMIC_IRQBASE,
					cg->gpio_data->num_gpio, 0);

	for (i = 0; i < cg->gpio_data->num_gpio; i++) {
		irq_set_chip_data(i + cg->irq_base, cg);
		irq_set_chip_and_handler_name(i + cg->irq_base,
					      &axp288_irqchip,
					      handle_simple_irq,
					      "demux");
	}

	retval = request_threaded_irq(irq, NULL, axp288_gpio_irq_handler,
				      IRQF_ONESHOT, "axp288_gpio", cg);

	if (retval) {
		dev_warn(&pdev->dev, "request irq failed: %d\n", retval);
		return retval;
	}

	return 0;
}

static struct platform_device_id axp288_gpio_id_table[] = {
	{ .name = "dollar_cove_gpio" },
	{},
};
static struct platform_driver axp288_gpio_driver = {
	.probe = axp288_gpio_probe,
	.driver = {
		.name = "axp288_gpio",
	},
	.id_table = axp288_gpio_id_table,
};

MODULE_DEVICE_TABLE(platform, axp288_gpio_id_table);
module_platform_driver(axp288_gpio_driver);

MODULE_AUTHOR("Iyer, Yegnesh S <yegnesh.s.iyer@intel.com>");
MODULE_DESCRIPTION("AXP288 GPIO Driver");
MODULE_LICENSE("GPL");
