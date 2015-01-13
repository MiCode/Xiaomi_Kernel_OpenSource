/*
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>

#include <linux/rfkill-gpio.h>

struct rfkill_gpio_data {
	const char		*name;
	enum rfkill_type	type;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*shutdown_gpio;
	struct gpio_desc	*wake_gpio;

	int			host_wake_irq;

	struct rfkill		*rfkill_dev;
	struct clk		*clk;

	bool			clk_enabled;
};

struct rfkill_gpio_desc {
	enum rfkill_type	type;

	int			reset_idx;
	int			shutdown_idx;
	int			wake_idx;
	int			host_wake_idx;
	int			host_wake_trigger;
};

static void rfkill_set_gpio(struct gpio_desc *desc, int value)
{
	if (gpiod_cansleep(desc))
		gpiod_set_value_cansleep(desc, value);
	else
		gpiod_set_value(desc, value);
}

static int rfkill_gpio_set_power(void *data, bool blocked)
{
	struct rfkill_gpio_data *rfkill = data;

	if (blocked) {
		rfkill_set_gpio(rfkill->shutdown_gpio, 0);
		rfkill_set_gpio(rfkill->reset_gpio, 0);
		rfkill_set_gpio(rfkill->wake_gpio, 0);
		if (!IS_ERR(rfkill->clk) && rfkill->clk_enabled)
			clk_disable(rfkill->clk);
	} else {
		if (!IS_ERR(rfkill->clk) && !rfkill->clk_enabled)
			clk_enable(rfkill->clk);
		rfkill_set_gpio(rfkill->reset_gpio, 1);
		rfkill_set_gpio(rfkill->shutdown_gpio, 1);
		rfkill_set_gpio(rfkill->wake_gpio, 1);
	}

	rfkill->clk_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops rfkill_gpio_ops = {
	.set_block = rfkill_gpio_set_power,
};

static irqreturn_t rfkill_gpio_host_wake(int irq, void *dev)
{
	struct rfkill_gpio_data *rfkill = dev;

	pr_info("Host wake IRQ for %s\n", rfkill->name);

	return IRQ_HANDLED;
}

static int rfkill_gpio_init(struct device *dev, struct rfkill_gpio_desc *desc)
{
	int ret;
	struct gpio_desc *gpio;
	struct rfkill_gpio_data *rfkill = dev_get_drvdata(dev);

	if (!rfkill->name) {
		dev_err(dev, "invalid platform data\n");
		return -EINVAL;
	}

	rfkill->clk = devm_clk_get(dev, NULL);

	if (!desc)
		gpio = devm_gpiod_get_index(dev, "reset", 0);
	else if (desc->reset_idx >= 0)
		gpio = devm_gpiod_get_index(dev, "reset", desc->reset_idx);
	else
		gpio = NULL;

	if (gpio && !IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		rfkill->reset_gpio = gpio;
	}

	if (!desc)
		gpio = devm_gpiod_get_index(dev, "shutdown", 1);
	else if (desc->shutdown_idx >= 0)
		gpio = devm_gpiod_get_index(dev, "shutdown",
					    desc->shutdown_idx);
	else
		gpio = NULL;

	if (gpio && !IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		rfkill->shutdown_gpio = gpio;
	}

	if (desc && (desc->wake_idx >= 0)) {
		gpio = devm_gpiod_get_index(dev, "wake", desc->wake_idx);
		if (!IS_ERR(gpio)) {
			ret = gpiod_direction_output(gpio, 0);
			if (ret)
				return ret;
			rfkill->wake_gpio = gpio;
		}
	}

	rfkill->host_wake_irq = -1;

	if (desc && (desc->host_wake_idx >= 0)) {
		gpio = devm_gpiod_get_index(dev, "host_wake",
					    desc->host_wake_idx);
		if (!IS_ERR(gpio)) {
			int irqf = IRQF_NO_SUSPEND | IRQF_ONESHOT;
			if (desc->host_wake_trigger > 0)
				irqf |= desc->host_wake_trigger;
			else {
				dev_err(dev, "missing IRQ type\n");
				return -EINVAL;
			}

			ret = gpiod_direction_input(gpio);
			if (ret)
				return ret;

			rfkill->host_wake_irq = gpiod_to_irq(gpio);
			ret = devm_request_threaded_irq(dev,
							rfkill->host_wake_irq,
							NULL,
							rfkill_gpio_host_wake,
							irqf, "host_wake",
							rfkill);
			if (ret)
				return ret;

			/* Wakeup IRQ, only used in suspend */
			disable_irq(rfkill->host_wake_irq);
		}
	}

	/* Make sure at-least one of the GPIO is defined */
	if (!rfkill->reset_gpio && !rfkill->shutdown_gpio) {
		dev_err(dev, "invalid platform data\n");
		return -EINVAL;
	}

	return 0;
}

static int rfkill_gpio_acpi_probe(struct device *dev,
				  struct rfkill_gpio_data *rfkill)
{
	const struct acpi_device_id *id;
	struct rfkill_gpio_desc *desc;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return -ENODEV;

	desc = (struct rfkill_gpio_desc *)id->driver_data;
	if (!desc)
		return -ENODEV;

	rfkill->name = dev_name(dev);
	rfkill->type = desc->type;

	return rfkill_gpio_init(dev, desc);
}

static int rfkill_gpio_probe(struct platform_device *pdev)
{
	struct rfkill_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct rfkill_gpio_data *rfkill;
	int ret;

	rfkill = devm_kzalloc(&pdev->dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	platform_set_drvdata(pdev, rfkill);

	if (ACPI_HANDLE(&pdev->dev)) {
		ret = rfkill_gpio_acpi_probe(&pdev->dev, rfkill);
		if (ret)
			return ret;
	} else if (pdata) {
		rfkill->name = pdata->name;
		rfkill->type = pdata->type;
		ret = rfkill_gpio_init(&pdev->dev, NULL);
		if (ret)
			return ret;
	} else {
		return -ENODEV;
	}

	rfkill->rfkill_dev = rfkill_alloc(rfkill->name, &pdev->dev,
					  rfkill->type, &rfkill_gpio_ops,
					  rfkill);
	if (!rfkill->rfkill_dev)
		return -ENOMEM;

	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		return ret;

	dev_info(&pdev->dev, "%s device registered.\n", rfkill->name);

	return 0;
}

#ifdef CONFIG_PM
static int rfkill_gpio_suspend(struct device *dev)
{
	struct rfkill_gpio_data *rfkill = dev_get_drvdata(dev);

	dev_dbg(dev, "Suspend\n");

	if (!rfkill->clk_enabled)
		return 0;

	if (rfkill->host_wake_irq >= 0)
		enable_irq(rfkill->host_wake_irq);

	gpiod_set_value_cansleep(rfkill->wake_gpio, 0);

	return 0;
}

static int rfkill_gpio_resume(struct device *dev)
{
	struct rfkill_gpio_data *rfkill = dev_get_drvdata(dev);

	dev_dbg(dev, "Resume\n");

	if (!rfkill->clk_enabled)
		return 0;

	if (rfkill->host_wake_irq >= 0)
		disable_irq(rfkill->host_wake_irq);

	gpiod_set_value_cansleep(rfkill->wake_gpio, 1);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(rfkill_gpio_pm, rfkill_gpio_suspend,
			 rfkill_gpio_resume);

static int rfkill_gpio_remove(struct platform_device *pdev)
{
	struct rfkill_gpio_data *rfkill = platform_get_drvdata(pdev);

	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);

	return 0;
}

#ifdef CONFIG_ACPI
static struct rfkill_gpio_desc acpi_default_bluetooth = {
	.type = RFKILL_TYPE_BLUETOOTH,
	.reset_idx = 0,
	.shutdown_idx = 1,
	.wake_idx = -1,
	.host_wake_idx = -1,
};

static struct rfkill_gpio_desc acpi_default_gps = {
	.type = RFKILL_TYPE_GPS,
	.reset_idx = 0,
	.shutdown_idx = 1,
	.wake_idx = -1,
	.host_wake_idx = -1,
};

static struct rfkill_gpio_desc acpi_bluetooth_wake = {
	.type = RFKILL_TYPE_BLUETOOTH,
	.reset_idx = -1,
	.shutdown_idx = 1,
	.wake_idx = 0,
	.host_wake_idx = 2,
	.host_wake_trigger = IRQF_TRIGGER_RISING,
};

static struct rfkill_gpio_desc acpi_gps_wake = {
	.type = RFKILL_TYPE_GPS,
	.reset_idx = -1,
	.shutdown_idx = 1,
	.wake_idx = -1,
	.host_wake_idx = 0,
	.host_wake_trigger = IRQF_TRIGGER_RISING,
};

static struct rfkill_gpio_desc acpi_gps_wake_falling = {
	.type = RFKILL_TYPE_GPS,
	.reset_idx = -1,
	.shutdown_idx = 1,
	.wake_idx = -1,
	.host_wake_idx = 0,
	.host_wake_trigger = IRQF_TRIGGER_FALLING,
};

static const struct acpi_device_id rfkill_acpi_match[] = {
	{ "BCM2E1A", (kernel_ulong_t)&acpi_default_bluetooth },
	{ "BCM2E39", (kernel_ulong_t)&acpi_default_bluetooth },
	{ "BCM2E3D", (kernel_ulong_t)&acpi_default_bluetooth },
	{ "BCM2E3A", (kernel_ulong_t)&acpi_bluetooth_wake },
	{ "OBDA8723", (kernel_ulong_t)&acpi_bluetooth_wake },
	{ "BCM4752", (kernel_ulong_t)&acpi_default_gps },
	{ "LNV4752", (kernel_ulong_t)&acpi_default_gps },
	{ "BCM4752E", (kernel_ulong_t)&acpi_default_gps },
	{ "BCM47521", (kernel_ulong_t)&acpi_gps_wake },
	{ "INT33A2", (kernel_ulong_t)&acpi_gps_wake_falling },
	{ },
};
MODULE_DEVICE_TABLE(acpi, rfkill_acpi_match);
#endif

static struct platform_driver rfkill_gpio_driver = {
	.probe = rfkill_gpio_probe,
	.remove = rfkill_gpio_remove,
	.driver = {
		.name = "rfkill_gpio",
		.owner = THIS_MODULE,
		.pm = &rfkill_gpio_pm,
		.acpi_match_table = ACPI_PTR(rfkill_acpi_match),
	},
};

module_platform_driver(rfkill_gpio_driver);

MODULE_DESCRIPTION("gpio rfkill");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
