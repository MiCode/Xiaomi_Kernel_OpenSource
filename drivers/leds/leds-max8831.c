/*
 * LEDs driver for MAX8831
 *
 * Copyright (c) 2008-2012, NVIDIA Corporation.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/ctype.h>
#include <linux/mfd/max8831.h>
#include <linux/delay.h>

struct max8831_led {
	struct led_classdev	cdev;
	struct work_struct	work;
	enum led_brightness	new_brightness;
	struct device		*master;
	int			id;
};

static void max8831_led_work(struct work_struct *work)
{
	struct max8831_led *led = container_of(work, struct max8831_led, work);
	unsigned int on_off_addr = MAX8831_CTRL;
	unsigned int blink_reg = 0;
	unsigned int curr_ctrl_reg = 0;
	unsigned int led_enb_mask = 0;

	on_off_addr = MAX8831_CTRL;

	switch (led->id) {
	case MAX8831_ID_LED1:
		curr_ctrl_reg = MAX8831_CURRENT_CTRL_LED1;
		led_enb_mask = MAX8831_CTRL_LED1_ENB;
		break;
	case MAX8831_ID_LED2:
		curr_ctrl_reg = MAX8831_CURRENT_CTRL_LED2;
		led_enb_mask = MAX8831_CTRL_LED2_ENB;
		break;
	case MAX8831_ID_LED3:
		curr_ctrl_reg = MAX8831_CURRENT_CTRL_LED3;
		led_enb_mask = MAX8831_CTRL_LED3_ENB;
		blink_reg = MAX8831_BLINK_CTRL_LED3;
		break;
	case MAX8831_ID_LED4:
		curr_ctrl_reg = MAX8831_CURRENT_CTRL_LED4;
		led_enb_mask = MAX8831_CTRL_LED4_ENB;
		blink_reg = MAX8831_BLINK_CTRL_LED4;
		break;
	case MAX8831_ID_LED5:
		curr_ctrl_reg = MAX8831_CURRENT_CTRL_LED5;
		led_enb_mask = MAX8831_CTRL_LED5_ENB;
		blink_reg = MAX8831_BLINK_CTRL_LED5;
		break;
	}

	/* disable blinking */
	if (blink_reg)
		max8831_update_bits(led->master, blink_reg,
							MAX8831_BLINK_ENB, 0);

	if (led->new_brightness == 0) {
		max8831_update_bits(led->master, on_off_addr, led_enb_mask, 0);
	} else {
		max8831_update_bits(led->master, on_off_addr, led_enb_mask,
								led_enb_mask);
		max8831_write(led->master, curr_ctrl_reg, led->new_brightness);
	}
}

static void max8831_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct max8831_led *led;

	led = container_of(led_cdev, struct max8831_led, cdev);

	led->new_brightness = value;
	/*
	 * Must use workqueue for the actual I/O since I2C operations
	 * can sleep.
	 */
	schedule_work(&led->work);
}

static int max8831_led_set_blink(struct led_classdev *led_cdev,
	unsigned long *delay_on,
	unsigned long *delay_off)
{
	struct max8831_led *led;
	unsigned int reg_val = 0;
	unsigned int reg = 0;
	led = container_of(led_cdev, struct max8831_led, cdev);

	if (*delay_on == 256)
		reg_val |= 0;
	if (*delay_on == 512)
		reg_val |= 1;
	if (*delay_on == 1024)
		reg_val |= 2;
	if (*delay_on == 2048)
		reg_val |= 3;

	if (*delay_off == 1024)
		reg_val |= 0 << MAX8831_BLINK_OFF_TIMER_SHIFT;
	if (*delay_off == 2048)
		reg_val |= 1 << MAX8831_BLINK_OFF_TIMER_SHIFT;
	if (*delay_off == 4096)
		reg_val |= 2 << MAX8831_BLINK_OFF_TIMER_SHIFT;
	if (*delay_off == 8192)
		reg_val |= 3 << MAX8831_BLINK_OFF_TIMER_SHIFT;

	reg_val = reg_val | MAX8831_BLINK_ENB;

	if (led->id == MAX8831_ID_LED3)
		reg = 0x17;
	else if (led->id == MAX8831_ID_LED4)
		reg = 0x18;
	else if (led->id == MAX8831_ID_LED5)
		reg = 0x19;

	max8831_led_set(&led->cdev, MAX8831_KEY_LEDS_MAX_CURR);
	max8831_write(led->master, reg, reg_val);
	return 0;
}

static int __devinit max8831_led_probe(struct platform_device *pdev)
{
	struct led_info *pdata = pdev->dev.platform_data;
	struct max8831_led *led;
	int err;

	if (!pdata)
		return -ENODEV;

	led = devm_kzalloc(&pdev->dev,
				sizeof(struct max8831_led), GFP_KERNEL);

	led->cdev.name = pdata->name;
	led->cdev.brightness_set = max8831_led_set;

	led->id = pdev->id;
	led->new_brightness = LED_OFF;
	led->master = pdev->dev.parent;

	if (led->id > 1) {
		led->cdev.blink_set = max8831_led_set_blink;
		led->cdev.max_brightness = MAX8831_KEY_LEDS_MAX_CURR;
	} else {
		led->cdev.max_brightness = MAX8831_BL_LEDS_MAX_CURR;
	}

	err = led_classdev_register(led->master, &led->cdev);
	if (err < 0)
		return err;


	INIT_WORK(&led->work, max8831_led_work);
	platform_set_drvdata(pdev, led);
	return 0;
}

static int __devexit max8831_led_remove(struct platform_device *pdev)
{
	struct max8831_led *led = platform_get_drvdata(pdev);
	led_classdev_unregister(&led->cdev);
	cancel_work_sync(&led->work);
	return 0;
}

static struct platform_driver max8831_led_driver = {
	.driver = {
		.name	= "max8831_led_bl",
		.owner	= THIS_MODULE,
	},
	.probe	= max8831_led_probe,
	.remove	= __devexit_p(max8831_led_remove),
};
module_platform_driver(max8831_led_driver);

MODULE_AUTHOR("Chaitanya Bandi<bandik@nvidia.com>");
MODULE_DESCRIPTION("MAX8831 LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:max8831-led");
