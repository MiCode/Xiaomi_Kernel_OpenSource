/*
 *  USB analog headset driver
 *
 *  Copyright (c) 2016 WangNannan <wangnannan@xiaomi.com>
 *  Copyright (C) 2018 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define DEBUG
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
#include <soc/qcom/socinfo.h>
#include "usb-headset.h"

#undef pr_debug
#define pr_debug pr_err
#undef dev_dbg
#define dev_dbg dev_err
#undef pr_info
#define pr_info pr_err


#define USBHS_SWITCH_AUDIO		0
#define USBHS_SWITCH_USB		1

struct usb_headset {
	struct power_supply *psy;
	struct notifier_block power_supply_notifier;
	struct work_struct psy_work;
	int typec_mode;
	int pr;

	int asel_gpio;
	enum of_gpio_flags asel_flags;
	int hsdet_gpio;
	enum of_gpio_flags hsdet_flags;
	struct device_node *us_euro_en_gpio_p;
};

static struct usb_headset *usb_hs;

static void usbhs_select(struct usb_headset *usbhs, bool audio)
{
	if (gpio_is_valid(usbhs->asel_gpio)) {
		if (audio) {
			if (usbhs->asel_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value(usbhs->asel_gpio, 0);
			else
				gpio_set_value(usbhs->asel_gpio, 1);
		} else {
			if (usbhs->asel_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value(usbhs->asel_gpio, 1);
			else
				gpio_set_value(usbhs->asel_gpio, 0);
		}
	}
}

static void usbhs_detect(struct usb_headset *usbhs, bool inserted)
{
	if (gpio_is_valid(usbhs->hsdet_gpio)) {
		if (inserted) {
			if (usbhs->hsdet_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value(usbhs->hsdet_gpio, 0);
			else
				gpio_set_value(usbhs->hsdet_gpio, 1);
		} else {
			if (usbhs->hsdet_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value(usbhs->hsdet_gpio, 1);
			else
				gpio_set_value(usbhs->hsdet_gpio, 0);
		}
	}
}

static void usbhs_switch_state(struct usb_headset *usbhs)
{
	union power_supply_propval pval;

	memset(&pval, 0, sizeof(pval));

	switch (usbhs->typec_mode) {
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		pr_debug("%s: Switch to state AUDIO\n", __func__);
		power_supply_get_property(usbhs->psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &pval);
		usbhs->pr = pval.intval;
		pr_info("%s: backup power role %d\n", __func__, pval.intval);
		pr_info("%s: set power role to SOURCE\n", __func__);
		pval.intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		if (power_supply_set_property(usbhs->psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &pval))
			pr_err("%s: force PR_SOURCE mode unsuccessful\n", __func__);
		if (usbhs->us_euro_en_gpio_p)
			msm_cdc_pinctrl_select_active_state(usbhs->us_euro_en_gpio_p);
		if (get_hw_version_platform() != HARDWARE_PLATFORM_SAGIT)
			msleep(200);
		usbhs_select(usbhs, true);
		usbhs_detect(usbhs, true);
		break;

	case POWER_SUPPLY_TYPEC_NONE:
		pr_debug("%s: Switch to state USB\n", __func__);
		usbhs_detect(usbhs, false);
		if (usbhs->us_euro_en_gpio_p)
			msm_cdc_pinctrl_select_sleep_state(usbhs->us_euro_en_gpio_p);
		usbhs_select(usbhs, false);
		pr_info("%s: restore power role to %d\n", __func__, usbhs->pr);
		pval.intval = usbhs->pr;
		if (power_supply_set_property(usbhs->psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &pval))
			pr_err("%s: force PR_DUAL mode unsuccessful\n", __func__);
		break;

	default:
		pr_err("%s: Invalid mode %d\n", __func__, usbhs->typec_mode);
		break;
	}
}

static int usbhs_get_typec_mode(struct usb_headset *usbhs)
{
	int ret = 0;
	union power_supply_propval prop;

	ret = power_supply_get_property(usbhs->psy, POWER_SUPPLY_PROP_TYPEC_MODE, &prop);
	if (ret < 0) {
		pr_err("%s: Cannot get typec mode property %d\n", __func__, ret);
		return ret;
	}

	pr_debug("%s: Type-C mode is %d\n", __func__, prop.intval);
	return prop.intval;
}

static void usbhs_psy_work(struct work_struct *work)
{
	struct usb_headset *usbhs = container_of(work, struct usb_headset, psy_work);

	pr_debug("%s: Switch state to %d\n", __func__, usbhs->typec_mode);
	usbhs_switch_state(usbhs);
}

static int usbhs_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct usb_headset *usbhs = container_of(nb, struct usb_headset, power_supply_notifier);
	int mode;

	if (ptr != usbhs->psy || event != PSY_EVENT_PROP_CHANGED)
		return 0;

	if (strncmp(usbhs->psy->desc->name, "usb", strlen("usb")) != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	mode = usbhs_get_typec_mode(usbhs);
	if (mode < 0) {
		pr_err("%s: Unable to read USB TYPEC_MODE: %d\n", __func__, mode);
		return 0;
	}

	switch (mode) {
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
	case POWER_SUPPLY_TYPEC_NONE:
		pr_debug("%s: typec mode %d to %d\n", __func__,
			usbhs->typec_mode, mode);
		if (usbhs->typec_mode == mode)
			break;
		usbhs->typec_mode = mode;
		schedule_work(&usbhs->psy_work);
		break;
	default:
		break;
	}

	return 0;
}

static int usbhs_parse_dt(struct device *dev, struct usb_headset *usbhs)
{
	struct device_node *np = dev->of_node;

	usbhs->hsdet_gpio = of_get_named_gpio_flags(np, "qcom,hsdet-gpio",
				0, &usbhs->hsdet_flags);
	if (!gpio_is_valid(usbhs->hsdet_gpio))
		pr_info("%s: Not get hsdet gpio\n", __func__);

	usbhs->asel_gpio = of_get_named_gpio_flags(np, "qcom,asel-gpio",
					0, &usbhs->asel_flags);
	pr_info("usbhs_parse_dt: get_hw_version_platform=%d\n", get_hw_version_platform());
	switch (get_hw_version_platform()) {
	case HARDWARE_PLATFORM_CHIRON:
	case HARDWARE_PLATFORM_CHIRON_S:
		if ((get_hw_version_major() == 1) &&
		    (get_hw_version_minor() == 0)) {
			usbhs->asel_gpio = of_get_named_gpio_flags(np, "qcom,asel-p1-gpio",
							0, &usbhs->asel_flags);
		}
		break;
	default:
		break;
	}

	pr_info("%s: audio select gpio is %d\n", __func__, usbhs->asel_gpio);
	if (!gpio_is_valid(usbhs->asel_gpio))
		pr_info("%s: Not get ASEL gpio\n", __func__);

	usbhs->us_euro_en_gpio_p = of_parse_phandle(np,
					"qcom,us-euro-switch-en-gpio", 0);
	if (!usbhs->us_euro_en_gpio_p) {
		pr_info("%s: property %s not detected in node %s", __func__,
			"qcom,us-euro-switch-en-gpio", np->full_name);
	}

	return 0;
}

int usbhs_init(struct platform_device *pdev)
{
	int ret = 0;
	struct usb_headset *usbhs;

	if (!pdev->dev.of_node) {
		pr_err("%s: Invalid platform data!\n", __func__);
		return -EINVAL;
	}

	usbhs = devm_kzalloc(&pdev->dev,
			sizeof(struct usb_headset), GFP_KERNEL);
	if (!usbhs) {
		pr_err("%s: Failed to allocate usbhs memory!", __func__);
		return -ENOMEM;
	}

	ret = usbhs_parse_dt(&pdev->dev, usbhs);
	if (ret < 0) {
		pr_err("%s: Failed parsing device tree!\n", __func__);
		goto err_free_usbhs;
	}

	if (gpio_is_valid(usbhs->asel_gpio)) {
		if (usbhs->asel_flags == OF_GPIO_ACTIVE_LOW) {
			ret = gpio_request_one(usbhs->asel_gpio,
				GPIOF_OUT_INIT_HIGH, "usb_asel");
		} else {
			ret = gpio_request_one(usbhs->asel_gpio,
				GPIOF_OUT_INIT_LOW, "usb_asel");
		}
		if (ret < 0) {
			pr_err("%s: Unable to request asel gpio %d\n", __func__,
					usbhs->asel_gpio);
			goto err_free_usbhs;
		}
	}

	if (gpio_is_valid(usbhs->hsdet_gpio)) {
		if (usbhs->hsdet_flags == OF_GPIO_ACTIVE_LOW) {
			ret = gpio_request_one(usbhs->hsdet_gpio,
				GPIOF_OUT_INIT_HIGH, "headset_detect");
		} else {
			ret = gpio_request_one(usbhs->hsdet_gpio,
				GPIOF_OUT_INIT_LOW, "headset_detect");
		}
		if (ret < 0) {
			pr_err("%s: Unable to request hsdet gpio %d\n", __func__,
					usbhs->hsdet_gpio);
			goto err_free_asel;
		}
	}

	if (usbhs->us_euro_en_gpio_p)
		msm_cdc_pinctrl_select_sleep_state(usbhs->us_euro_en_gpio_p);

	usbhs->psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(usbhs->psy)) {
		pr_err("%s: could not get USB psy info\n", __func__);
		ret = -EPROBE_DEFER;
		if (IS_ERR(usbhs->psy))
			ret = PTR_ERR(usbhs->psy);
		usbhs->psy = NULL;
		goto err_free_hsdet;
	}

	usbhs->power_supply_notifier.notifier_call = usbhs_power_supply_event;
	ret = power_supply_reg_notifier(&usbhs->power_supply_notifier);
	if (ret < 0) {
		pr_err("%s: Error register power supply notifier!\n", __func__);
		goto err_free_psy;
	}
	INIT_WORK(&usbhs->psy_work, usbhs_psy_work);

	/* Set initial Type-C state */
	usbhs->pr = POWER_SUPPLY_TYPEC_PR_DUAL;
	usbhs->typec_mode = usbhs_get_typec_mode(usbhs);
	pr_info("%s: Switch to initial Type-C state %d\n", __func__, usbhs->typec_mode);
	usbhs_switch_state(usbhs);

	usb_hs = usbhs;
	pr_info("%s: succeeded\n", __func__);

	return 0;

err_free_psy:
	power_supply_put(usbhs->psy);
err_free_hsdet:
	gpio_free(usbhs->hsdet_gpio);
err_free_asel:
	gpio_free(usbhs->asel_gpio);
err_free_usbhs:
	devm_kfree(&pdev->dev, usbhs);

	return ret;
}
EXPORT_SYMBOL(usbhs_init);

void usbhs_deinit()
{
	if (!usb_hs) {
		cancel_work_sync(&usb_hs->psy_work);
		power_supply_unreg_notifier(&usb_hs->power_supply_notifier);
		power_supply_put(usb_hs->psy);
		gpio_free(usb_hs->asel_gpio);
		gpio_free(usb_hs->hsdet_gpio);
		usb_hs = NULL;
	}

	return;
}
EXPORT_SYMBOL(usbhs_deinit);

MODULE_DESCRIPTION("USB Analog headset module");
MODULE_LICENSE("GPL");
