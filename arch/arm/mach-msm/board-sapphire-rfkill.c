/* linux/arch/arm/mach-msm/board-sapphire-rfkill.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

/* Control bluetooth power for sapphire platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include "gpio_chip.h"
#include "board-sapphire.h"

static struct rfkill *bt_rfk;
static const char bt_name[] = "brf6300";

extern int sapphire_bt_fastclock_power(int on);

static int bluetooth_set_power(void *data, bool blocked)
{
	if (!blocked) {
		sapphire_bt_fastclock_power(1);
		gpio_set_value(SAPPHIRE_GPIO_BT_32K_EN, 1);
		udelay(10);
		gpio_direction_output(101, 1);
	} else {
		gpio_direction_output(101, 0);
		gpio_set_value(SAPPHIRE_GPIO_BT_32K_EN, 0);
		sapphire_bt_fastclock_power(0);
	}
	return 0;
}

static struct rfkill_ops sapphire_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int sapphire_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */

	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &sapphire_rfkill_ops, NULL);
	if (!bt_rfk)
		return -ENOMEM;

	/* userspace cannot take exclusive control */

	rfkill_set_states(bt_rfk, default_state, false);

	rc = rfkill_register(bt_rfk);

	if (rc)
		rfkill_destroy(bt_rfk);
	return rc;
}

static int sapphire_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);

	return 0;
}

static struct platform_driver sapphire_rfkill_driver = {
	.probe = sapphire_rfkill_probe,
	.remove = sapphire_rfkill_remove,
	.driver = {
		.name = "sapphire_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init sapphire_rfkill_init(void)
{
	return platform_driver_register(&sapphire_rfkill_driver);
}

static void __exit sapphire_rfkill_exit(void)
{
	platform_driver_unregister(&sapphire_rfkill_driver);
}

module_init(sapphire_rfkill_init);
module_exit(sapphire_rfkill_exit);
MODULE_DESCRIPTION("sapphire rfkill");
MODULE_AUTHOR("Nick Pelly <npelly@google.com>");
MODULE_LICENSE("GPL");
