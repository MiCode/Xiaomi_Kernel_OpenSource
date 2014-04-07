/*
 * soc_button_array.c: supports the button array on SoC tablets originally
 * running Windows 8.
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

/*
 * Definition of buttons on the tablet. The ACPI index of each button
 * is defined in section 2.8.7.2 of "Windows ACPI Design Guide for SoC
 * Platforms"
 */
#define	MAX_NBUTTONS	5

struct soc_button_info {
	const char *name;
	int acpi_index;
	unsigned int event_code;
	int repeat;
	int wakeup;
	int gpio;
};

static struct soc_button_info soc_button_tbl[] = {
	{"power", 0, KEY_POWER, 0, 1, -1},
	{"home", 1, KEY_HOMEPAGE, 0, 1, -1},
	{"volume_up", 2, KEY_VOLUMEUP, 1, 0, -1},
	{"volume_down", 3, KEY_VOLUMEDOWN, 1, 0, -1},
	{"rotation_lock", 4, KEY_RO, 0, 0, -1},
};

/*
 * Some of the buttons like volume up/down are auto repeat, while others
 * are not. To support both, we register two platform devices, and put
 * buttons into them based on whether the key should be auto repeat.
 */
#define	BUTTON_TYPES	2

static struct gpio_keys_button soc_buttons[BUTTON_TYPES][MAX_NBUTTONS];

static struct gpio_keys_platform_data soc_button_data[BUTTON_TYPES] = {
	{
		.buttons	= soc_buttons[0],
		.nbuttons	= 0,
		.rep		= 0
	},
	{
		.buttons	= soc_buttons[1],
		.nbuttons	= 0,
		.rep		= 1
	}
};

static struct platform_device *soc_button_device[BUTTON_TYPES];

/*
 * Get the Nth GPIO number from the ACPI object.
 */
static int soc_button_lookup_gpio(struct device *dev, int acpi_index)
{
	struct gpio_desc *desc;
	int ret;

	desc = gpiod_get_index(dev, KBUILD_MODNAME, acpi_index);

	if (IS_ERR(desc))
		return -1;

	ret = desc_to_gpio(desc);

	gpiod_put(desc);

	return ret;
}

/*
 * Register platform device for a set of buttons.
 */
static int soc_button_register(int r)
{
	struct platform_device *pd;
	int err;

	pd = platform_device_alloc("gpio-keys", PLATFORM_DEVID_AUTO);
	if (!pd) {
		err = -ENOMEM;
		goto err0;
	}

	err = platform_device_add_data(pd, &soc_button_data[r],
					sizeof(soc_button_data[r]));
	if (err)
		goto err1;

	err = platform_device_add(pd);
	if (err)
		goto err1;

	soc_button_device[r] = pd;

	return 0;

err1:
	platform_device_put(pd);
err0:
	return err;
}

static int soc_button_pnp_probe(struct pnp_dev *pdev,
	const struct pnp_device_id *id)
{
	int i, j, r, ret;
	int sz_tbl = sizeof(soc_button_tbl) / sizeof(soc_button_tbl[0]);
	struct gpio_keys_button *gk;

	/* Find GPIO number of all the buttons */
	for (i = 0; i < sz_tbl; i++) {
		soc_button_tbl[i].gpio = soc_button_lookup_gpio(&pdev->dev,
						soc_button_tbl[i].acpi_index);
	}

	for (r = 0; r < BUTTON_TYPES; r++) {
		gk = soc_buttons[r];
		j = 0;

		/* Register buttons in the correct device */
		for (i = 0; i < sz_tbl; i++) {
			if (soc_button_tbl[i].repeat == r &&
			    soc_button_tbl[i].gpio > 0) {
				gk[j].code = soc_button_tbl[i].event_code;
				gk[j].gpio = soc_button_tbl[i].gpio;
				gk[j].active_low = 1;
				gk[j].desc = soc_button_tbl[i].name;
				gk[j].type = EV_KEY;
				gk[j].wakeup = soc_button_tbl[i].wakeup;
				j++;
			}
		}

		soc_button_data[r].nbuttons = j;

		if (j == 0)
			continue;

		if (soc_button_register(r)) {
			dev_err(&pdev->dev, "failed to register %d\n", r);
			return ret;
		}
	}

	return 0;
}

static void soc_button_remove(struct pnp_dev *pdev)
{
	int r;

	for (r = 0; r < BUTTON_TYPES; r++) {
		if (soc_button_device[r]) {
			platform_device_unregister(soc_button_device[r]);
			soc_button_device[r] = NULL;
		}
	}
}

static const struct pnp_device_id soc_button_pnp_match[] = {
	{.id = "INTCFD9", .driver_data = 0},
	{.id = ""}
};

MODULE_DEVICE_TABLE(pnp, soc_button_pnp_match);

static struct pnp_driver soc_button_pnp_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= soc_button_pnp_match,
	.probe          = soc_button_pnp_probe,
	.remove		= soc_button_remove,
};

static int __init soc_button_init(void)
{
	return pnp_register_driver(&soc_button_pnp_driver);
}

static void __exit soc_button_exit(void)
{
	pnp_unregister_driver(&soc_button_pnp_driver);
}

module_init(soc_button_init);
module_exit(soc_button_exit);

MODULE_LICENSE("GPL");
