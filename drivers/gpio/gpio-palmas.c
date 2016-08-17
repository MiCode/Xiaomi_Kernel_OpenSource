/*
 * gpiolib support for Palmas Series PMICS
 *
 * Copyright 2011 Texas Instruments
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 * Based on gpio-wm831x.c
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <linux/mfd/palmas.h>

struct palmas_gpio {
	struct palmas *palmas;
	struct gpio_chip gpio_chip;
};

#define GPIO_SLAVE	PALMAS_BASE_TO_SLAVE(PALMAS_GPIO_BASE)

int palmas_gpio_read(struct palmas *palmas, unsigned int reg,
				unsigned int *dest)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_GPIO_BASE, reg);

	return regmap_read(palmas->regmap[GPIO_SLAVE], addr, dest);
}

int palmas_gpio_write(struct palmas *palmas, unsigned int reg,
				unsigned int value)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_GPIO_BASE, reg);

	return regmap_write(palmas->regmap[GPIO_SLAVE], addr, value);
}

static inline struct palmas_gpio *to_palmas_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct palmas_gpio, gpio_chip);
}

static int palmas_gpio_direction_in(struct gpio_chip *chip, unsigned offset)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;
	int ret;
	unsigned int reg = 0;

	if (!((1 << offset) & palmas->gpio_muxed))
		return -EINVAL;

	ret = palmas_gpio_read(palmas, PALMAS_GPIO_DATA_DIR, &reg);
	if (ret)
		return ret;

	reg &= ~(1 << offset);

	return palmas_gpio_write(palmas, PALMAS_GPIO_DATA_DIR, reg);
}

static int palmas_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;
	unsigned int reg = 0;

	if (!((1 << offset) & palmas->gpio_muxed))
		return 0;

	palmas_gpio_read(palmas, PALMAS_GPIO_DATA_IN, &reg);

	return !!(reg & (1 << offset));
}

static void palmas_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;
	unsigned int reg;

	if (!((1 << offset) & palmas->gpio_muxed))
		return;

	palmas_gpio_read(palmas, PALMAS_GPIO_DATA_OUT, &reg);

	reg &= ~(1 << offset);
	reg |= value << offset;

	palmas_gpio_write(palmas, PALMAS_GPIO_DATA_OUT, reg);
}

static int palmas_gpio_direction_out(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;
	int ret;
	unsigned int reg;

	if (!((1 << offset) & palmas->gpio_muxed))
		return -EINVAL;

	ret = palmas_gpio_read(palmas, PALMAS_GPIO_DATA_DIR, &reg);
	if (ret)
		return ret;

	reg |= 1 << offset;

	return palmas_gpio_write(palmas, PALMAS_GPIO_DATA_DIR, reg);
}

static int palmas_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;

	return palmas_irq_get_virq(palmas, PALMAS_GPIO_0_IRQ + offset);
}

static int palmas_gpio_set_debounce(struct gpio_chip *chip, unsigned offset,
				    unsigned debounce)
{
	struct palmas_gpio *palmas_gpio = to_palmas_gpio(chip);
	struct palmas *palmas = palmas_gpio->palmas;
	int ret;
	unsigned int reg;

	if (!((1 << offset) & palmas->gpio_muxed))
		return -EINVAL;

	ret = palmas_gpio_read(palmas, PALMAS_GPIO_DATA_DIR, &reg);
	if (ret)
		return ret;

	if (debounce)
		reg |= 1 << offset;
	else
		reg &= ~(1 << offset);

	return palmas_gpio_write(palmas, PALMAS_GPIO_DATA_DIR, reg);
}

static struct gpio_chip palmas_gpio_chip = {
	.label			= "palmas",
	.owner			= THIS_MODULE,
	.direction_input	= palmas_gpio_direction_in,
	.get			= palmas_gpio_get,
	.direction_output	= palmas_gpio_direction_out,
	.set			= palmas_gpio_set,
	.to_irq			= palmas_gpio_to_irq,
	.set_debounce		= palmas_gpio_set_debounce,
	.can_sleep		= 1,
	.ngpio			= 8,
};

static int __devinit palmas_gpio_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *pdata = palmas->dev->platform_data;
	struct palmas_gpio *gpio;
	int ret;

	gpio = kzalloc(sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	gpio->palmas = palmas;
	gpio->gpio_chip = palmas_gpio_chip;
	gpio->gpio_chip.dev = &pdev->dev;

	if (pdata && pdata->gpio_base)
		gpio->gpio_chip.base = pdata->gpio_base;
	else
		gpio->gpio_chip.base = -1;

	ret = gpiochip_add(&gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n",
			ret);
		goto err;
	}

	platform_set_drvdata(pdev, gpio);

	return ret;

err:
	kfree(gpio);
	return ret;
}

static int __devexit palmas_gpio_remove(struct platform_device *pdev)
{
	struct palmas_gpio *gpio = platform_get_drvdata(pdev);
	int ret;

	ret = gpiochip_remove(&gpio->gpio_chip);
	if (ret == 0)
		kfree(gpio);

	return ret;
}

static struct platform_driver palmas_gpio_driver = {
	.driver.name	= "palmas-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= palmas_gpio_probe,
	.remove		= __devexit_p(palmas_gpio_remove),
};

static int __init palmas_gpio_init(void)
{
	return platform_driver_register(&palmas_gpio_driver);
}
subsys_initcall(palmas_gpio_init);

static void __exit palmas_gpio_exit(void)
{
	platform_driver_unregister(&palmas_gpio_driver);
}
module_exit(palmas_gpio_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("GPIO interface for the Palmas series chips");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:palmas-gpio");
