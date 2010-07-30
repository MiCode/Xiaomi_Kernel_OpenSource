/* Quanta I2C Backlight Driver
 *
 * Copyright (C) 2009 Quanta Computer Inc.
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

/*
 *
 *  The Driver with I/O communications via the I2C Interface for ST15 platform.
 *  And it is only working on the nuvoTon WPCE775x Embedded Controller.
 *
 */

#include <linux/module.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/wpce775x.h>

#define EC_CMD_SET_BACKLIGHT 0xB1

static void qci_backlight_store(struct led_classdev *led_cdev,
	enum led_brightness val);

static struct platform_device *bl_pdev;
static struct led_classdev lcd_backlight = {
	.name = "lcd-backlight",
	.brightness = 147,
	.brightness_set = qci_backlight_store,
};

static void qci_backlight_store(struct led_classdev *led_cdev,
	enum led_brightness val)
{
	u16 value = val;
	wpce_smbus_write_word_data(EC_CMD_SET_BACKLIGHT, value);
	msleep(10);

	dev_dbg(&bl_pdev->dev, "[backlight_store] : value  = %d\n", value);
}

static int __init qci_backlight_init(void)
{
	int err = 0;
	bl_pdev = platform_device_register_simple("backlight", 0, NULL, 0);
	err = led_classdev_register(&bl_pdev->dev, &lcd_backlight);
	return err;
}

static void __exit qci_backlight_exit(void)
{
	led_classdev_unregister(&lcd_backlight);
	platform_device_unregister(bl_pdev);
}

module_init(qci_backlight_init);
module_exit(qci_backlight_exit);

MODULE_AUTHOR("Quanta Computer Inc.");
MODULE_DESCRIPTION("Quanta Embedded Controller I2C Backlight Driver");
MODULE_LICENSE("GPL v2");

