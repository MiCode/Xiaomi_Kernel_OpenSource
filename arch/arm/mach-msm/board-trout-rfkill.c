/*
 * Copyright (C) 2008 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Control bluetooth power for trout platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <asm/gpio.h>

#include "board-trout.h"

static struct rfkill *bt_rfk;
static const char bt_name[] = "brf6300";

static int bluetooth_set_power(void *data, bool blocked)
{
	if (!blocked) {
		gpio_set_value(TROUT_GPIO_BT_32K_EN, 1);
		udelay(10);
		gpio_direction_output(101, 1);
	} else {
		gpio_direction_output(101, 0);
		gpio_set_value(TROUT_GPIO_BT_32K_EN, 0);
	}
	return 0;
}

static struct rfkill_ops trout_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int trout_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */

	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
				&trout_rfkill_ops, NULL);
	if (!bt_rfk)
		return -ENOMEM;

	rfkill_set_states(bt_rfk, default_state, false);

	/* userspace cannot take exclusive control */

	rc = rfkill_register(bt_rfk);

	if (rc)
		rfkill_destroy(bt_rfk);
	return rc;
}

static int trout_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);

	return 0;
}

static struct platform_driver trout_rfkill_driver = {
	.probe = trout_rfkill_probe,
	.remove = trout_rfkill_remove,
	.driver = {
		.name = "trout_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init trout_rfkill_init(void)
{
	return platform_driver_register(&trout_rfkill_driver);
}

static void __exit trout_rfkill_exit(void)
{
	platform_driver_unregister(&trout_rfkill_driver);
}

module_init(trout_rfkill_init);
module_exit(trout_rfkill_exit);
MODULE_DESCRIPTION("trout rfkill");
MODULE_AUTHOR("Nick Pelly <npelly@google.com>");
MODULE_LICENSE("GPL");
