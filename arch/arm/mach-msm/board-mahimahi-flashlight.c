/*
 * arch/arm/mach-msm/flashlight.c - flashlight driver
 *
 *  Copyright (C) 2009 zion huang <zion_huang@htc.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/wakelock.h>
#include <linux/hrtimer.h>
#include <mach/msm_iomap.h>
#include <asm/gpio.h>
#include <asm/io.h>

#include "board-mahimahi-flashlight.h"

struct flashlight_struct {
	struct led_classdev fl_lcdev;
	struct early_suspend early_suspend_flashlight;
	spinlock_t spin_lock;
	struct hrtimer timer;
	int brightness;
	int gpio_torch;
	int gpio_flash;
	int flash_duration_ms;
};

static struct flashlight_struct the_fl;

static inline void toggle(void)
{
	gpio_direction_output(the_fl.gpio_torch, 0);
	udelay(2);
	gpio_direction_output(the_fl.gpio_torch, 1);
	udelay(2);
}

static void flashlight_hw_command(uint8_t addr, uint8_t data)
{
	int i;

	for (i = 0; i < addr + 17; i++)
		toggle();
	udelay(500);

	for (i = 0; i < data; i++)
		toggle();
	udelay(500);
}

static enum hrtimer_restart flashlight_timeout(struct hrtimer *timer)
{
	unsigned long flags;

	pr_debug("%s\n", __func__);

	spin_lock_irqsave(&the_fl.spin_lock, flags);
	gpio_direction_output(the_fl.gpio_flash, 0);
	the_fl.brightness = LED_OFF;
	spin_unlock_irqrestore(&the_fl.spin_lock, flags);

	return HRTIMER_NORESTART;
}

int flashlight_control(int mode)
{
	int ret = 0;
	unsigned long flags;

	pr_debug("%s: mode %d -> %d\n", __func__,
			the_fl.brightness, mode);

	spin_lock_irqsave(&the_fl.spin_lock, flags);

	the_fl.brightness = mode;

	switch (mode) {
	case FLASHLIGHT_TORCH:
		pr_info("%s: half\n", __func__);
		/* If we are transitioning from flash to torch, make sure to
		 * cancel the flash timeout timer, otherwise when it expires,
		 * the torch will go off as well.
		 */
		hrtimer_cancel(&the_fl.timer);
		flashlight_hw_command(2, 4);
		break;

	case FLASHLIGHT_FLASH:
		pr_info("%s: full\n", __func__);
		hrtimer_cancel(&the_fl.timer);
		gpio_direction_output(the_fl.gpio_flash, 0);
		udelay(40);
		gpio_direction_output(the_fl.gpio_flash, 1);
		hrtimer_start(&the_fl.timer,
			ktime_set(the_fl.flash_duration_ms / 1000,
					(the_fl.flash_duration_ms % 1000) *
						NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
		/* Flash overrides torch mode, and after the flash period, the
		 * flash LED will turn off.
		 */
		mode = LED_OFF;
		break;

	case FLASHLIGHT_OFF:
		pr_info("%s: off\n", __func__);
		gpio_direction_output(the_fl.gpio_flash, 0);
		gpio_direction_output(the_fl.gpio_torch, 0);
		break;

	default:
		pr_err("%s: unknown flash_light flags: %d\n", __func__, mode);
		ret = -EINVAL;
		goto done;
	}

done:
	spin_unlock_irqrestore(&the_fl.spin_lock, flags);
	return ret;
}
EXPORT_SYMBOL(flashlight_control);

static void fl_lcdev_brightness_set(struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	int level;
	switch (brightness) {
	case LED_HALF:
		level = FLASHLIGHT_TORCH;
		break;
	case LED_FULL:
		level = FLASHLIGHT_FLASH;
		break;
	case LED_OFF:
	default:
		level = FLASHLIGHT_OFF;
	};

	flashlight_control(level);
}

static void flashlight_early_suspend(struct early_suspend *handler)
{
	flashlight_control(FLASHLIGHT_OFF);
}

static int flashlight_setup_gpio(struct flashlight_platform_data *fl_pdata)
{
	int ret;

	pr_debug("%s\n", __func__);

	if (fl_pdata->gpio_init) {
		ret = fl_pdata->gpio_init();
		if (ret < 0) {
			pr_err("%s: gpio init failed: %d\n", __func__,
				ret);
			return ret;
		}
	}

	if (fl_pdata->torch) {
		ret = gpio_request(fl_pdata->torch, "flashlight_torch");
		if (ret < 0) {
			pr_err("%s: gpio_request failed\n", __func__);
			return ret;
		}
	}

	if (fl_pdata->flash) {
		ret = gpio_request(fl_pdata->flash, "flashlight_flash");
		if (ret < 0) {
			pr_err("%s: gpio_request failed\n", __func__);
			gpio_free(fl_pdata->torch);
			return ret;
		}
	}

	the_fl.gpio_torch = fl_pdata->torch;
	the_fl.gpio_flash = fl_pdata->flash;
	the_fl.flash_duration_ms = fl_pdata->flash_duration_ms;
	return 0;
}

static int flashlight_probe(struct platform_device *pdev)
{
	struct flashlight_platform_data *fl_pdata = pdev->dev.platform_data;
	int err = 0;

	pr_debug("%s\n", __func__);

	err = flashlight_setup_gpio(fl_pdata);
	if (err < 0) {
		pr_err("%s: setup GPIO failed\n", __func__);
		goto fail_free_mem;
	}

	spin_lock_init(&the_fl.spin_lock);
	the_fl.fl_lcdev.name = pdev->name;
	the_fl.fl_lcdev.brightness_set = fl_lcdev_brightness_set;
	the_fl.fl_lcdev.brightness = LED_OFF;
	err = led_classdev_register(&pdev->dev, &the_fl.fl_lcdev);
	if (err < 0) {
		pr_err("failed on led_classdev_register\n");
		goto fail_free_gpio;
	}

	hrtimer_init(&the_fl.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	the_fl.timer.function = flashlight_timeout;

#ifdef CONFIG_HAS_EARLYSUSPEND
	the_fl.early_suspend_flashlight.suspend = flashlight_early_suspend;
	the_fl.early_suspend_flashlight.resume = NULL;
	register_early_suspend(&the_fl.early_suspend_flashlight);
#endif

	return 0;

fail_free_gpio:
	if (fl_pdata->torch)
		gpio_free(fl_pdata->torch);
	if (fl_pdata->flash)
		gpio_free(fl_pdata->flash);
fail_free_mem:
	return err;
}

static int flashlight_remove(struct platform_device *pdev)
{
	struct flashlight_platform_data *fl_pdata = pdev->dev.platform_data;

	pr_debug("%s\n", __func__);

	hrtimer_cancel(&the_fl.timer);
	unregister_early_suspend(&the_fl.early_suspend_flashlight);
	flashlight_control(FLASHLIGHT_OFF);
	led_classdev_unregister(&the_fl.fl_lcdev);
	if (fl_pdata->torch)
		gpio_free(fl_pdata->torch);
	if (fl_pdata->flash)
		gpio_free(fl_pdata->flash);
	return 0;
}

static struct platform_driver flashlight_driver = {
	.probe		= flashlight_probe,
	.remove		= flashlight_remove,
	.driver		= {
		.name		= FLASHLIGHT_NAME,
		.owner		= THIS_MODULE,
	},
};

static int __init flashlight_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&flashlight_driver);
}

static void __exit flashlight_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&flashlight_driver);
}

module_init(flashlight_init);
module_exit(flashlight_exit);

MODULE_AUTHOR("Zion Huang <zion_huang@htc.com>");
MODULE_DESCRIPTION("flash light driver");
MODULE_LICENSE("GPL");
