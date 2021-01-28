// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "rtfled.h"
#include <linux/init.h>
#include <linux/version.h>

#define RTFLED_INFO(format, args...)	\
	pr_info("%s:%s() line-%d: " format,	\
		ALIAS_NAME, __func__, __LINE__, ## args)
#define RTFLED_WARN(format, args...)	\
	pr_warn("%s:%s() line-%d: " format, \
		ALIAS_NAME, __func__, __LINE__, ## args)
#define RTFLED_ERR(format, args...)	\
	pr_err("%s:%s() line-%d: " format,	\
		ALIAS_NAME, __func__, __LINE__, ## args)

#define RT_FLED_DEVICE  "rt-flash-led"
#define ALIAS_NAME RT_FLED_DEVICE

struct rt_fled_dev *rt_fled_get_dev(const char *name)
{
	struct flashlight_device *flashlight_dev;

	flashlight_dev = find_flashlight_by_name(name ? name : RT_FLED_DEVICE);
	if (flashlight_dev == NULL)
		return (struct rt_fled_dev *)NULL;
	return flashlight_get_data(flashlight_dev);
}
EXPORT_SYMBOL(rt_fled_get_dev);


static int rtfled_set_torch_brightness(struct flashlight_device *flashlight_dev,
							int brightness_sel)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_set_torch_current_sel(fled_dev,
								brightness_sel);
}

static int rtfled_set_strobe_brightness(struct flashlight_device
					*flashlight_dev, int brightness_sel)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_set_strobe_current_sel(fled_dev,
								brightness_sel);
}

static int rtfled_set_strobe_timeout(struct flashlight_device *flashlight_dev,
					int timeout)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);
	int sel;

	return fled_dev->hal->rt_hal_fled_set_strobe_timeout(fled_dev, timeout,
								timeout, &sel);
}

static int rtfled_list_strobe_timeout(struct flashlight_device *flashlight_dev,
					int selector)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_strobe_timeout_list(fled_dev,
								selector);
}

static int rtfled_set_mode(struct flashlight_device *flashlight_dev, int mode)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_set_mode(fled_dev, mode);
}

static int rtfled_strobe(struct flashlight_device *flashlight_dev)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_strobe(fled_dev);
}

static int rtfled_is_ready(struct flashlight_device *flashlight_dev)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	return fled_dev->hal->rt_hal_fled_get_is_ready(fled_dev);
}

static int rtfled_set_color_temperature(struct flashlight_device
					*flashlight_dev, int color_temp)
{
	/* Doesn't support color temperature */
	return -EINVAL;
}

static int rtfled_list_color_temperature(struct flashlight_device
					 *flashlight_dev, int selector)
{
	/* Doesn't support color temperature */
	return -EINVAL;
}

static int rtfled_suspend(struct flashlight_device *flashlight_dev,
				pm_message_t state)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	if (fled_dev->hal->rt_hal_fled_suspend)
		return fled_dev->hal->rt_hal_fled_suspend(fled_dev, state);
	return 0;
}

static int rtfled_resume(struct flashlight_device *flashlight_dev)
{
	struct rt_fled_dev *fled_dev = flashlight_get_data(flashlight_dev);

	if (fled_dev->hal->rt_hal_fled_resume)
		return fled_dev->hal->rt_hal_fled_resume(fled_dev);
	return 0;
}

static struct flashlight_ops rtfled_impl_ops = {
	.set_torch_brightness = rtfled_set_torch_brightness,
	.set_strobe_brightness = rtfled_set_strobe_brightness,
	.set_strobe_timeout = rtfled_set_strobe_timeout,
	.list_strobe_timeout = rtfled_list_strobe_timeout,
	.set_mode = rtfled_set_mode,
	.strobe = rtfled_strobe,
	.is_ready = rtfled_is_ready,
	.set_color_temperature = rtfled_set_color_temperature,
	.list_color_temperature = rtfled_list_color_temperature,
	.suspend = rtfled_suspend,
	.resume = rtfled_resume,
};

static void rfled_shutdown(struct platform_device *pdev)
{
	struct rt_fled_dev *fled_dev = platform_get_drvdata(pdev);

	if (fled_dev->hal->rt_hal_fled_shutdown)
		fled_dev->hal->rt_hal_fled_shutdown(fled_dev);
}

static int rtled_impl_set_torch_current(struct rt_fled_dev *fled_dev,
					int min_uA, int max_uA, int *selector)
{
	int sel = 0;
	int rc;

	for (sel = 0;; sel++) {
		rc = fled_dev->hal->rt_hal_fled_torch_current_list(fled_dev,
									sel);
		if (rc < 0)
			return rc;
		if (rc >= min_uA && rc <= max_uA) {
			*selector = sel;
			return fled_dev->hal->rt_hal_fled_set_torch_current_sel
								(fled_dev, sel);
		}
	}
	return -EINVAL;
}

static int rtled_impl_set_strobe_current(struct rt_fled_dev *fled_dev,
					 int min_uA, int max_uA, int *selector)
{
	int sel = 0;
	int rc;

	for (sel = 0;; sel++) {
		rc = fled_dev->hal->rt_hal_fled_strobe_current_list(fled_dev,
									sel);
		if (rc < 0)
			return rc;
		if (rc >= min_uA && rc <= max_uA) {
			*selector = sel;
			return fled_dev->hal->rt_hal_fled_set_strobe_current_sel
								(fled_dev, sel);
		}
	}
	return -EINVAL;
}

static int rtled_impl_set_timeout_level(struct rt_fled_dev *fled_dev,
					int min_uA, int max_uA, int *selector)
{
	int sel = 0;
	int rc;

	for (sel = 0;; sel++) {
		rc = fled_dev->hal->rt_hal_fled_timeout_level_list(fled_dev,
									sel);
		if (rc < 0)
			return rc;
		if (rc >= min_uA && rc <= max_uA) {
			*selector = sel;
			return fled_dev->hal->rt_hal_fled_set_timeout_level_sel
								(fled_dev, sel);
		}
	}
	return -EINVAL;
}

static int rtled_impl_set_lv_protection(struct rt_fled_dev *fled_dev,
					int min_mV, int max_mV, int *selector)
{
	int sel = 0;
	int rc;

	for (sel = 0;; sel++) {
		rc = fled_dev->hal->rt_hal_fled_lv_protection_list(fled_dev,
									sel);
		if (rc < 0)
			return rc;
		if (rc >= min_mV && rc <= max_mV) {
			*selector = sel;
			return fled_dev->hal->rt_hal_fled_set_lv_protection_sel
								(fled_dev, sel);
		}
	}
	return -EINVAL;
}

static int rtled_impl_set_strobe_timeout(struct rt_fled_dev *fled_dev,
					 int min_ms, int max_ms, int *selector)
{
	int sel = 0;
	int rc;

	for (sel = 0;; sel++) {
		rc = fled_dev->hal->rt_hal_fled_strobe_timeout_list(fled_dev,
									sel);
		if (rc < 0)
			return rc;
		if (rc >= min_ms && rc <= max_ms) {
			*selector = sel;
			return fled_dev->hal->rt_hal_fled_set_strobe_timeout_sel
								(fled_dev, sel);
		}
	}
	return -EINVAL;
}

static int rtled_impl_get_torch_current(struct rt_fled_dev *fled_dev)
{
	int sel = fled_dev->hal->rt_hal_fled_get_torch_current_sel(fled_dev);

	if (sel < 0)
		return sel;
	return fled_dev->hal->rt_hal_fled_torch_current_list(fled_dev, sel);
}

static int rtled_impl_get_strobe_current(struct rt_fled_dev *fled_dev)
{
	int sel = fled_dev->hal->rt_hal_fled_get_strobe_current_sel(fled_dev);

	if (sel < 0)
		return sel;
	return fled_dev->hal->rt_hal_fled_strobe_current_list(fled_dev, sel);
}

static int rtled_impl_get_timeout_level(struct rt_fled_dev *fled_dev)
{
	int sel = fled_dev->hal->rt_hal_fled_get_timeout_level_sel(fled_dev);

	if (sel < 0)
		return sel;
	return fled_dev->hal->rt_hal_fled_timeout_level_list(fled_dev, sel);
}

static int rtled_impl_get_lv_protection(struct rt_fled_dev *fled_dev)
{
	int sel = fled_dev->hal->rt_hal_fled_get_lv_protection_sel(fled_dev);

	if (sel < 0)
		return sel;
	return fled_dev->hal->rt_hal_fled_lv_protection_list(fled_dev, sel);
}

static int rtled_impl_get_strobe_timeout(struct rt_fled_dev *fled_dev)
{
	int sel = fled_dev->hal->rt_hal_fled_get_strobe_timeout_sel(fled_dev);

	if (sel < 0)
		return sel;
	return fled_dev->hal->rt_hal_fled_strobe_timeout_list(fled_dev, sel);
}

static int rtled_impl_get_is_ready(struct rt_fled_dev *fled_dev)
{
	/* if not implemented, just return ready always */
	return 1;
}

#define HAL_NOT_IMPLEMENTED(x) (hal->x == NULL)
static inline int check_hal_implemented(void *x)
{
	if (x == NULL)
		return -EINVAL;
	return 0;
}

static int rtfled_check_hal_implement(struct rt_fled_hal *hal)
{
	int rc = 0;

	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_set_torch_current))
		hal->rt_hal_fled_set_torch_current =
						rtled_impl_set_torch_current;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_set_strobe_current))
		hal->rt_hal_fled_set_strobe_current =
						rtled_impl_set_strobe_current;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_set_timeout_level))
		hal->rt_hal_fled_set_timeout_level =
						rtled_impl_set_timeout_level;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_set_lv_protection))
		hal->rt_hal_fled_set_lv_protection =
						rtled_impl_set_lv_protection;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_set_strobe_timeout))
		hal->rt_hal_fled_set_strobe_timeout =
						rtled_impl_set_strobe_timeout;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_torch_current))
		hal->rt_hal_fled_get_torch_current =
						rtled_impl_get_torch_current;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_strobe_current))
		hal->rt_hal_fled_get_strobe_current =
						rtled_impl_get_strobe_current;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_timeout_level))
		hal->rt_hal_fled_get_timeout_level =
						rtled_impl_get_timeout_level;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_lv_protection))
		hal->rt_hal_fled_get_lv_protection =
						rtled_impl_get_lv_protection;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_strobe_timeout))
		hal->rt_hal_fled_get_strobe_timeout =
						rtled_impl_get_strobe_timeout;
	if (HAL_NOT_IMPLEMENTED(rt_hal_fled_get_is_ready))
		hal->rt_hal_fled_get_is_ready = rtled_impl_get_is_ready;
	rc |= check_hal_implemented(hal->rt_hal_fled_set_mode);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_mode);
	rc |= check_hal_implemented(hal->rt_hal_fled_strobe);
	rc |= check_hal_implemented(hal->rt_hal_fled_torch_current_list);
	rc |= check_hal_implemented(hal->rt_hal_fled_strobe_current_list);
	rc |= check_hal_implemented(hal->rt_hal_fled_timeout_level_list);
	rc |= check_hal_implemented(hal->rt_hal_fled_lv_protection_list);
	rc |= check_hal_implemented(hal->rt_hal_fled_strobe_timeout_list);
	rc |= check_hal_implemented(hal->rt_hal_fled_set_torch_current_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_set_strobe_current_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_set_timeout_level_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_set_lv_protection_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_set_strobe_timeout_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_torch_current_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_strobe_current_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_timeout_level_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_lv_protection_sel);
	rc |= check_hal_implemented(hal->rt_hal_fled_get_strobe_timeout_sel);

	if (rc != 0)
		RTFLED_WARN("check_hal_implemented have NULL item.\n");

	return 0;
}


static int rtfled_probe(struct platform_device *pdev)
{
	struct rt_fled_dev *fled_dev = dev_get_drvdata(pdev->dev.parent);
	int rc;

	WARN_ON(fled_dev == NULL);
	if (!fled_dev)
		return -ENODEV;
	WARN_ON(fled_dev->hal == NULL);
	if (!fled_dev->hal)
		return -EPERM;

	RTFLED_INFO("Richtek FlashLED Driver is probing\n");
	rc = rtfled_check_hal_implement(fled_dev->hal);
	if (rc < 0) {
		RTFLED_ERR("HAL implemented incompletely\n");
		goto err_check_hal;
	}
	platform_set_drvdata(pdev, fled_dev);
	fled_dev->flashlight_dev =
	    flashlight_device_register
	    (fled_dev->name ? fled_dev->name : RT_FLED_DEVICE, &pdev->dev,
	       fled_dev, &rtfled_impl_ops, fled_dev->init_props);
	if (fled_dev->hal->rt_hal_fled_init) {
		rc = fled_dev->hal->rt_hal_fled_init(fled_dev);
		if (rc < 0) {
			RTFLED_ERR("Initialization failed\n");
			goto err_init;
		}
	}
	RTFLED_INFO("Richtek FlashLED Driver initialized successfully\n");
	return 0;
err_init:
	flashlight_device_unregister(fled_dev->flashlight_dev);
err_check_hal:
	return rc;
}

static int rtfled_remove(struct platform_device *pdev)
{
	struct rt_fled_dev *fled_dev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	flashlight_device_unregister(fled_dev->flashlight_dev);
	return 0;
}

static struct platform_driver rt_flash_led_driver = {
	.driver = {
		   .name = RT_FLED_DEVICE,
		   .owner = THIS_MODULE,
		   },
	.shutdown = rfled_shutdown,
	.probe = rtfled_probe,
	.remove = rtfled_remove,
};

static int __init rtfled_init(void)
{
	return platform_driver_register(&rt_flash_led_driver);
}
subsys_initcall(rtfled_init);

static void __exit rtfled_exit(void)
{
	platform_driver_unregister(&rt_flash_led_driver);
}
module_exit(rtfled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com");
MODULE_VERSION("1.0.2_G");
MODULE_DESCRIPTION("Richtek Flash LED Driver");
