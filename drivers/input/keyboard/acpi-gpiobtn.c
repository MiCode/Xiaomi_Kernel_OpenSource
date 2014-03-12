/*
 * GPIO button array driver
 * Copyright (c) 2014, Intel Corporation.
 *
 * Author: Andy Ross <andrew.j.ross@intel.com>
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio_keys.h>

static struct gpio_keys_button gpiobtn_keys[] = {
	{
		.code = KEY_POWER,
		.desc = "Power",
		.type = EV_KEY,
		.wakeup = 1,
	},
	{
		.code = KEY_LEFTMETA,
		.desc = "Home",
		.type = EV_KEY,
		.wakeup = 1,
	},
	{
		.code = KEY_VOLUMEUP,
		.desc = "Volume Up",
		.type = EV_KEY,
	},
	{
		.code = KEY_VOLUMEDOWN,
		.desc = "Volume Down",
		.type = EV_KEY,
	},
	{
		.code = SW_ROTATE_LOCK,
		.desc = "Rotation Lock",
		.type = EV_SW,
	},
};

static struct gpio_keys_platform_data gpiobtn_keys_pdata = {
	.buttons = gpiobtn_keys,
};

static struct platform_device gpiobtn_keys_pdev = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &gpiobtn_keys_pdata,
	},
};

static int gpiobtn_probe(struct platform_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gpiobtn_keys); i++) {
		struct gpio_desc *d = gpiod_get_index(&dev->dev, NULL, i);
		if (!d || IS_ERR(d))
			break;

		gpiobtn_keys[i].gpio = desc_to_gpio(d);
		gpiobtn_keys[i].desc = d;

		/* ACPI firmware gets this wrong on at least one
		 * device, but the fact that all these devices work
		 * with a driver from a single vendor implies that the
		 * polarity is fixed by specification.
		 *
		 * gpiobtn_keys[i].active_low = gpiod_is_active_low(d);
		 */
		gpiobtn_keys[i].active_low = 1;

		gpiod_put(d);
	}

	if (!i) {
		dev_err(&dev->dev, "No GPIO button lines detected");
		return -1;
	}

	gpiobtn_keys_pdata.nbuttons = i;
	return platform_device_register(&gpiobtn_keys_pdev);
}

static int gpiobtn_remove(struct platform_device *dev)
{
	platform_device_unregister(&gpiobtn_keys_pdev);
	return 0;
}

static const struct acpi_device_id acpi_ids[] = {
	{ "PNP0C40" },
	{}
};

static struct platform_driver gpiobtn_plat_drv = {
	.probe	= gpiobtn_probe,
	.remove = gpiobtn_remove,
	.driver = {
		.name = "ACPI GPIO Buttons",
		.acpi_match_table = acpi_ids,
	},
};

static int __init gpiobtn_init(void)
{
	return platform_driver_register(&gpiobtn_plat_drv);
}

static void __exit gpiobtn_exit(void)
{
	platform_driver_unregister(&gpiobtn_plat_drv);
}

module_init(gpiobtn_init);
module_exit(gpiobtn_exit);
