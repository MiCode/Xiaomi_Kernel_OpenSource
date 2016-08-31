/*
 * as3720-gpio.c  --  gpiolib support for ams AS3720 PMICs
 *
 * Copyright 2012 ams
 *
 * Author: Bernhard Breinbauer <bernhard.breinbauer@ams.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <linux/mfd/as3720.h>

struct as3720_gpio {
	struct as3720 *as3720;
	struct gpio_chip gpio_chip;
};

static inline struct as3720_gpio *to_as3720_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct as3720_gpio, gpio_chip);
}

static int as3720_gpio_direction_in(struct gpio_chip *chip, unsigned offset)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;

	return as3720_set_bits(as3720, AS3720_GPIO0_CONTROL_REG + offset,
			       AS3720_GPIO_MODE_MASK,
			       AS3720_GPIO_MODE_INPUT);
}

static int as3720_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;
	int ret;
	u32 val;

	ret = as3720_reg_read(as3720, AS3720_GPIO_SIGNAL_IN_REG, &val);
	if (ret < 0)
		return ret;

	if (val & (AS3720_GPIO1_SIGNAL_MASK << offset))
		return 1;
	else
		return 0;
}

static int as3720_gpio_direction_out(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;

	return as3720_set_bits(as3720, AS3720_GPIO0_CONTROL_REG + offset,
			       AS3720_GPIO_MODE_MASK,
			       AS3720_GPIO_MODE_OUTPUT_VDDH);
}

static void as3720_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;

	as3720_set_bits(as3720, AS3720_GPIO_SIGNAL_OUT_REG, 1 << offset,
			value << offset);
}

static int as3720_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;

	if (!as3720->irq_base)
		return -EINVAL;

	return as3720->irq_base + AS3720_GPIO_IRQ_BASE + offset;
}

static int as3720_gpio_set_config(struct as3720_gpio *as3720_gpio,
	struct as3720_gpio_config *gpio_cfg)
{
	int ret = 0;
	u8 val = 0;
	int gpio = gpio_cfg->gpio;
	struct as3720 *as3720 = as3720_gpio->as3720;
	if ((gpio < AS3720_GPIO0) || (gpio > AS3720_GPIO7))
		return -EINVAL;

	/* .invert + .iosf + .mode */
	/* set up write of the GPIOX control register */
	val = (gpio_cfg->iosf & AS3720_GPIO_IOSF_MASK) +
		(gpio_cfg->mode & AS3720_GPIO_MODE_MASK);
	if (gpio_cfg->invert)
		val += (AS3720_GPIO_INV & AS3720_GPIO_INV_MASK);

	ret = as3720_reg_write(as3720, AS3720_GPIO0_CONTROL_REG + gpio, val);
	if (ret != 0) {
		printk(KERN_INFO "AS3720_GPIO%d_CTRL_REG write err, ret: %d\n",
			gpio, ret);
		return ret;
	}

	/* if GPIO is configured as an output, set initial output state */
	if ((gpio_cfg->mode == AS3720_GPIO_MODE_OUTPUT_VDDH) ||
		(gpio_cfg->mode == AS3720_GPIO_MODE_OUTPUT_VDDL)) {
		/*GPIO0 -> bit 0, ..., GPIO7 -> bit 7, output_state = 0 or 1*/
		val = (gpio_cfg->output_state ^ gpio_cfg->invert) << gpio;
		ret = as3720_set_bits(as3720, AS3720_GPIO_SIGNAL_OUT_REG,
				1 << gpio, val);
	}

	return ret;
}

static int as3720_gpio_init_regs(struct as3720_gpio *as3720_gpio,
		struct as3720_platform_data *pdata)
{
	int ret;
	int i;
	for (i = 0; i < pdata->num_gpio_cfgs; i++) {
		ret = as3720_gpio_set_config(as3720_gpio,
			&pdata->gpio_cfgs[i]);
		if (ret < 0) {
			dev_err(as3720_gpio->as3720->dev,
				"Failed to set gpio config\n");
			return ret;
		}
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void as3720_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct as3720_gpio *as3720_gpio = to_as3720_gpio(chip);
	struct as3720 *as3720 = as3720_gpio->as3720;
	int i;

	for (i = 0; i < chip->ngpio; i++) {
		int gpio = i + chip->base;
		u32 reg;
		int ret;
		const char *label, *pull, *direction;

		/* We report the GPIO even if it's not requested since
		 * we're also reporting things like alternate
		 * functions which apply even when the GPIO is not in
		 * use as a GPIO.
		 */
		label = gpiochip_is_requested(chip, i);
		if (!label)
			label = "Unrequested";

		seq_printf(s, " gpio-%-3d (%-20.20s) ", gpio, label);

		ret = as3720_reg_read(as3720,
			AS3720_GPIO0_CONTROL_REG + i, &reg);
		if (ret < 0) {
			dev_err(as3720->dev,
				"GPIO control %d read failed: %d\n",
				gpio, ret);
			seq_printf(s, "\n");
			continue;
		}

		switch (reg & AS3720_GPIO_MODE_MASK) {
		case AS3720_GPIO_MODE_INPUT:
			direction = "in";
			pull = "nopull";
			break;
		case AS3720_GPIO_MODE_OUTPUT_VDDH:
			direction = "out";
			pull = "push and pull";
			break;
		case AS3720_GPIO_MODE_IO_OPEN_DRAIN:
			direction = "io";
			pull = "nopull";
			break;
		case AS3720_GPIO_MODE_INPUT_W_PULLUP:
			direction = "in";
			pull = "pullup";
			break;
		case AS3720_GPIO_MODE_INPUT_W_PULLDOWN:
			direction = "in";
			pull = "pulldown";
			break;
		case AS3720_GPIO_MODE_IO_OPEN_DRAIN_PULLUP:
			direction = "io";
			pull = "pullup";
			break;
		case AS3720_GPIO_MODE_OUTPUT_VDDL:
			direction = "out";
			pull = "push and pull";
			break;
		default:
			direction = "INVALID DIRECTION/MODE";
			pull = "INVALID PULL";
			break;
		}

		seq_printf(s, " %s %s %s\n"
			   "                                  %s (0x%4x)\n",
			   direction,
			   as3720_gpio_get(chip, i) ? "high" : "low",
			   pull,
			   reg & AS3720_GPIO_INV_MASK ? " inverted" : "",
			   reg);
	}
}
#else
#define as3720_gpio_dbg_show NULL
#endif

static struct gpio_chip as3720_gpio_chip = {
	.label			= "as3720",
	.owner			= THIS_MODULE,
	.direction_input	= as3720_gpio_direction_in,
	.get			= as3720_gpio_get,
	.direction_output	= as3720_gpio_direction_out,
	.set			= as3720_gpio_set,
	.to_irq			= as3720_gpio_to_irq,
	.dbg_show		= as3720_gpio_dbg_show,
	.can_sleep		= 1,
};

static int as3720_gpio_probe(struct platform_device *pdev)
{
	struct as3720 *as3720 =  dev_get_drvdata(pdev->dev.parent);
	struct as3720_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct as3720_gpio *as3720_gpio;
	int ret;

	as3720_gpio = devm_kzalloc(&pdev->dev,
			sizeof(*as3720_gpio), GFP_KERNEL);
	if (as3720_gpio == NULL) {
		dev_err(&pdev->dev, "Memory allocaiton failure\n");
		return -ENOMEM;
	}

	as3720_gpio->as3720 = as3720;
	as3720_gpio->gpio_chip = as3720_gpio_chip;
	as3720_gpio->gpio_chip.ngpio = AS3720_NUM_GPIO;
	as3720_gpio->gpio_chip.dev = &pdev->dev;
	if (pdata && pdata->gpio_base)
		as3720_gpio->gpio_chip.base = pdata->gpio_base;
	else
		as3720_gpio->gpio_chip.base = -1;

	platform_set_drvdata(pdev, as3720_gpio);

	ret = as3720_gpio_init_regs(as3720_gpio, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio_init_regs failed\n");
		return ret;
	}

	ret = gpiochip_add(&as3720_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int as3720_gpio_remove(struct platform_device *pdev)
{
	struct as3720_gpio *as3720_gpio = platform_get_drvdata(pdev);

	return gpiochip_remove(&as3720_gpio->gpio_chip);
}

static struct platform_driver as3720_gpio_driver = {
	.driver.name	= "as3720-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= as3720_gpio_probe,
	.remove		= as3720_gpio_remove,
};

static int __init as3720_gpio_init(void)
{
	return platform_driver_register(&as3720_gpio_driver);
}
subsys_initcall(as3720_gpio_init);

static void __exit as3720_gpio_exit(void)
{
	platform_driver_unregister(&as3720_gpio_driver);
}
module_exit(as3720_gpio_exit);

MODULE_AUTHOR("Bernhard Breinbauer <bernhard.breinbauer@ams.com>");
MODULE_DESCRIPTION("GPIO interface for AS3720 PMICs");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:as3720-gpio");

