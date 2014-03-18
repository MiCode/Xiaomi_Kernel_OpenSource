/*
 * baytrail_button.c: supports the GPIO buttons on some Baytrail
 * tablets.
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/pnp.h>

#define DEVICE_NAME "baytrail_button"

static void byt_button_device_release(struct device *);

/*
 * Definition of buttons on the tablet. The ACPI index of each button
 * is defined in "Windows ACPI Design Guide for SoC Platforms"
 */
#define	MAX_NBUTTONS	5

struct byt_button_info {
	const char *name;
	int acpi_index;
	unsigned int event_code;
	int repeat;
	int wakeup;
	int gpio;
};

static struct byt_button_info byt_button_tbl[] = {
	{"power", 0, KEY_POWER, 0, 1, -1},
	{"home", 1, KEY_HOME, 0, 1, -1},
	{"volume_up", 2, KEY_VOLUMEUP, 1, 0, -1},
	{"volume_down", 3, KEY_VOLUMEDOWN, 1, 0, -1},
	{"rotation_lock", 4, KEY_RO, 0, 0, -1},
};

/*
 * Some of the buttons like volume up/down are auto repeat, while others
 * are not. To support both, we register two gpio-keys device, and put
 * buttons into them based on whether the key should be auto repeat.
 */
#define	BUTTON_TYPES	2

static struct gpio_keys_button byt_buttons[BUTTON_TYPES][MAX_NBUTTONS];

static struct gpio_keys_platform_data byt_button_data[BUTTON_TYPES] = {
	{
		.buttons	= byt_buttons[0],
		.nbuttons	= 0,
		.rep		= 0
	},
	{
		.buttons	= byt_buttons[1],
		.nbuttons	= 0,
		.rep		= 1
	}
};

static struct platform_device byt_button_device[BUTTON_TYPES] = {
	{
		.name	= "gpio-keys",
		.id	= PLATFORM_DEVID_AUTO,
		.dev	= {
			.release	= byt_button_device_release,
			.platform_data	= &byt_button_data[0],
		}
	},
	{
		.name	= "gpio-keys",
		.id	= PLATFORM_DEVID_AUTO,
		.dev	= {
			.release	= byt_button_device_release,
			.platform_data	= &byt_button_data[1],
		}
	}
};

/*
 * Get the Nth GPIO number from the ACPI object.
 */
static int byt_button_lookup_gpio(struct device *dev, int acpi_index)
{
	struct gpio_desc *desc;
	int ret;

	desc = gpiod_get_index(dev, DEVICE_NAME, acpi_index);

	if (IS_ERR(desc))
		return -1;

	ret = desc_to_gpio(desc);

	gpiod_put(desc);

	return ret;
}

static int byt_button_pnp_probe(struct pnp_dev *pdev,
	const struct pnp_device_id *id)
{
	int i, j, r, ret;
	int sz_tbl = sizeof(byt_button_tbl) / sizeof(byt_button_tbl[0]);
	struct gpio_keys_button *gk;

	/* Find GPIO number of all the buttons */
	for (i = 0; i < sz_tbl; i++) {
		byt_button_tbl[i].gpio = byt_button_lookup_gpio(&pdev->dev,
						byt_button_tbl[i].acpi_index);
	}

	for (r = 0; r < BUTTON_TYPES; r++) {
		gk = byt_buttons[r];
		j = 0;

		/* Register buttons in the correct device */
		for (i = 0; i < sz_tbl; i++) {
			if (byt_button_tbl[i].repeat == r &&
			    byt_button_tbl[i].gpio > 0) {
				gk[j].code = byt_button_tbl[i].event_code;
				gk[j].gpio = byt_button_tbl[i].gpio;
				gk[j].active_low = 1;
				gk[j].desc = byt_button_tbl[i].name;
				gk[j].type = EV_KEY;
				gk[j].wakeup = byt_button_tbl[i].wakeup;
				j++;
			}
		}

		byt_button_data[r].nbuttons = j;

		if (j == 0)
			continue;

		ret = platform_device_register(&byt_button_device[r]);
		if (ret) {
			dev_err(&pdev->dev, "failed to register %d\n", r);
			return ret;
		}
	}

	return 0;
}

static void byt_button_device_release(struct device *dev)
{
	/* Nothing to do */
}

static void byt_button_remove(struct pnp_dev *pdev)
{
	int r;

	for (r = 0; r < BUTTON_TYPES; r++) {
		if (byt_button_data[r].nbuttons)
			platform_device_unregister(&byt_button_device[r]);
	}
}

static const struct pnp_device_id byt_button_pnp_match[] = {
	{.id = "INTCFD9", .driver_data = 0},
	{.id = ""}
};

MODULE_DEVICE_TABLE(pnp, byt_button_pnp_match);

static struct pnp_driver byt_button_pnp_driver = {
	.name		= DEVICE_NAME,
	.id_table	= byt_button_pnp_match,
	.probe          = byt_button_pnp_probe,
	.remove		= byt_button_remove,
};

static int __init byt_button_init(void)
{
	return pnp_register_driver(&byt_button_pnp_driver);
}

static void __exit byt_button_exit(void)
{
	pnp_unregister_driver(&byt_button_pnp_driver);
}

module_init(byt_button_init);
module_exit(byt_button_exit);

MODULE_LICENSE("GPL");
