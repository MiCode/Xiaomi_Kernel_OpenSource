/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

#define TAG_NAME "[flashligh_led191_drv]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...) \
	pr_debug(TAG_NAME "%s: " fmt, __func__, ##arg)
#define PK_ERR(fmt, arg...) \
	pr_info(TAG_NAME "%s: " fmt, __func__, ##arg)

#define DEBUG_LEDS_STROBE
#ifdef DEBUG_LEDS_STROBE
#define PK_LOG(fmt, arg...) \
	pr_info(TAG_NAME "%s is called.\n", __func__)
#define PK_DBG         PK_DBG_FUNC
#else
#define PK_LOG(fmt, arg...)       do {} while (0)
#define PK_DBG(a, ...)
#endif

/* define device tree */
#ifndef LED191_DTNAME
#define LED191_DTNAME "mediatek,flashlights_led191"
#endif

#define LED191_NAME "flashlights_led191"

/* define registers */

/* define mutex and work queue */
static DEFINE_MUTEX(led191_mutex);
static struct work_struct led191_work;

/* define pinctrl */
#define LED191_PINCTRL_PIN_HWEN 0
#define LED191_PINCTRL_PINSTATE_LOW 0
#define LED191_PINCTRL_PINSTATE_HIGH 1
#define LED191_PINCTRL_STATE_HW_CH0_HIGH "hw_ch0_high"
#define LED191_PINCTRL_STATE_HW_CH0_LOW  "hw_ch0_low"
#define LED191_PINCTRL_STATE_HW_CH1_HIGH "hw_ch1_high"
#define LED191_PINCTRL_STATE_HW_CH1_LOW  "hw_ch1_low"


static struct pinctrl *led191_pinctrl;
static struct pinctrl_state *led191_hw_ch0_high;
static struct pinctrl_state *led191_hw_ch0_low;
static struct pinctrl_state *led191_hw_ch1_high;
static struct pinctrl_state *led191_hw_ch1_low;

/* define usage count */
static int use_count;
static int g_flash_duty = -1;
static int g_flash_channel_idx;

/* platform data */
struct led191_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int led191_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	led191_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(led191_pinctrl)) {
		PK_ERR("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(led191_pinctrl);
		return -1;
	}

	/*  Flashlight pin initialization */
	led191_hw_ch0_high = pinctrl_lookup_state(led191_pinctrl,
		LED191_PINCTRL_STATE_HW_CH0_HIGH);
	if (IS_ERR(led191_hw_ch0_high)) {
		PK_ERR("Failed to init (%s)\n",
			LED191_PINCTRL_STATE_HW_CH0_HIGH);
		ret = PTR_ERR(led191_hw_ch0_high);
	}
	led191_hw_ch0_low = pinctrl_lookup_state(led191_pinctrl,
		LED191_PINCTRL_STATE_HW_CH0_LOW);
	if (IS_ERR(led191_hw_ch0_low)) {
		PK_ERR("Failed to init (%s)\n",
			LED191_PINCTRL_STATE_HW_CH0_LOW);
		ret = PTR_ERR(led191_hw_ch0_low);
	}

	if (flashlight_device_num == 2)	{
		led191_hw_ch1_high = pinctrl_lookup_state(led191_pinctrl,
			LED191_PINCTRL_STATE_HW_CH1_HIGH);
		if (IS_ERR(led191_hw_ch1_high)) {
			PK_ERR("Failed to init (%s)\n",
				LED191_PINCTRL_STATE_HW_CH1_HIGH);
			ret = PTR_ERR(led191_hw_ch1_high);
		}
		led191_hw_ch1_low = pinctrl_lookup_state(led191_pinctrl,
			LED191_PINCTRL_STATE_HW_CH1_LOW);
		if (IS_ERR(led191_hw_ch1_low)) {
			PK_ERR("Failed to init (%s)\n",
				LED191_PINCTRL_STATE_HW_CH1_LOW);
			ret = PTR_ERR(led191_hw_ch1_low);
		}
	}
	return ret;
}

static int led191_pinctrl_set(int pin, int state)
{
	int ret = 0;
	struct pinctrl_state *led191_hw_chx_low = led191_hw_ch0_low;
	struct pinctrl_state *led191_hw_chx_high = led191_hw_ch0_high;

	if (IS_ERR(led191_pinctrl)) {
		PK_ERR("pinctrl is not available\n");
		return -1;
	}

	PK_DBG("g_flash_channel_idx = %d\n", g_flash_channel_idx);
	if (g_flash_channel_idx == 0) {
		led191_hw_chx_low = led191_hw_ch0_low;
		led191_hw_chx_high = led191_hw_ch0_high;
	} else if (g_flash_channel_idx == 1) {
		led191_hw_chx_low = led191_hw_ch1_low;
		led191_hw_chx_high = led191_hw_ch1_high;
	} else {
		PK_DBG("please check g_flash_channel_idx!!!\n");
	}

	switch (pin) {
	case LED191_PINCTRL_PIN_HWEN:
		if (state == LED191_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(led191_hw_chx_low))
			ret = pinctrl_select_state(led191_pinctrl,
				led191_hw_chx_low);
		else if (state == LED191_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(led191_hw_chx_high))
			ret = pinctrl_select_state(led191_pinctrl,
				led191_hw_chx_high);
		else
			PK_ERR("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		PK_ERR("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	PK_DBG("pin(%d) state(%d), ret:%d\n", pin, state, ret);

	return ret;
}

/******************************************************************************
 * led191 operations
 *****************************************************************************/
/* flashlight enable function */
static int led191_enable(void)
{
	int pin = LED191_PINCTRL_PIN_HWEN;

	if (g_flash_duty == 1) {
		led191_pinctrl_set(pin, 1);
	} else {
		led191_pinctrl_set(pin, 1);
		led191_pinctrl_set(pin, 0);
	}
	led191_pinctrl_set(pin, 1);

	return 0;
}

/* flashlight disable function */
static int led191_disable(void)
{
	int pin = 0, state = 0;
	return led191_pinctrl_set(pin, state);
}

/* set flashlight level */
static int led191_set_level(int level)
{
	g_flash_duty = level;
	return 0;
}

/* flashlight init */
static int led191_init(void)
{
	int pin = 0, state = 0;
	return led191_pinctrl_set(pin, state);
}

/* flashlight uninit */
static int led191_uninit(void)
{
	int pin = 0, state = 0;
	return led191_pinctrl_set(pin, state);
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer led191_timer;
static unsigned int led191_timeout_ms;

static void led191_work_disable(struct work_struct *data)
{
	PK_DBG("work queue callback\n");
	led191_disable();
}

static enum hrtimer_restart led191_timer_func(struct hrtimer *timer)
{
	schedule_work(&led191_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int led191_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;
	g_flash_channel_idx = channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		led191_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PK_DBG("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		led191_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PK_DBG("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (led191_timeout_ms) {
				ktime = ktime_set(led191_timeout_ms / 1000,
					(led191_timeout_ms % 1000) * 1000000);
				hrtimer_start(&led191_timer, ktime,
					HRTIMER_MODE_REL);
			}
			led191_enable();
		} else {
			led191_disable();
			hrtimer_cancel(&led191_timer);
		}
		break;
	default:
		PK_LOG("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int led191_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int led191_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int led191_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&led191_mutex);
	if (set) {
		if (!use_count)
			ret = led191_init();
		use_count++;
		PK_DBG("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = led191_uninit();
		if (use_count < 0)
			use_count = 0;
		PK_DBG("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&led191_mutex);

	return ret;
}

static ssize_t led191_strobe_store(struct flashlight_arg arg)
{
	led191_set_driver(1);
	led191_set_level(arg.level);
	led191_timeout_ms = 0;
	led191_enable();
	msleep(arg.dur);
	led191_disable();
	led191_set_driver(0);

	return 0;
}

static struct flashlight_operations led191_ops = {
	led191_open,
	led191_release,
	led191_ioctl,
	led191_strobe_store,
	led191_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int led191_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * led191_init();
	 */

	return 0;
}

static int led191_parse_dt(struct device *dev,
		struct led191_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		PK_LOG("Parse no dt, node.\n");
		return 0;
	}
	PK_LOG("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		PK_LOG("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				LED191_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int led191_probe(struct platform_device *pdev)
{
	struct led191_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	PK_DBG("Probe start.\n");

	/* init pinctrl */
	if (led191_pinctrl_init(pdev)) {
		PK_DBG("Failed to init pinctrl.\n");
		err = -EFAULT;
		goto err;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err;
		}
		pdev->dev.platform_data = pdata;
		err = led191_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&led191_work, led191_work_disable);

	/* init timer */
	hrtimer_init(&led191_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	led191_timer.function = led191_timer_func;
	led191_timeout_ms = 100;

	/* init chip hw */
	led191_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&led191_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(LED191_NAME, &led191_ops)) {
			err = -EFAULT;
			goto err;
		}
	}

	PK_DBG("Probe done.\n");

	return 0;
err:
	return err;
}

static int led191_remove(struct platform_device *pdev)
{
	struct led191_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	PK_DBG("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(LED191_NAME);

	/* flush work queue */
	flush_work(&led191_work);

	PK_DBG("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id led191_gpio_of_match[] = {
	{.compatible = LED191_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, led191_gpio_of_match);
#else
static struct platform_device led191_gpio_platform_device[] = {
	{
		.name = LED191_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, led191_gpio_platform_device);
#endif

static struct platform_driver led191_platform_driver = {
	.probe = led191_probe,
	.remove = led191_remove,
	.driver = {
		.name = LED191_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = led191_gpio_of_match,
#endif
	},
};

static int __init flashlight_led191_init(void)
{
	int ret;

	PK_DBG("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&led191_gpio_platform_device);
	if (ret) {
		PK_ERR("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&led191_platform_driver);
	if (ret) {
		PK_ERR("Failed to register platform driver\n");
		return ret;
	}

	PK_DBG("Init done.\n");

	return 0;
}

static void __exit flashlight_led191_exit(void)
{
	PK_DBG("Exit start.\n");

	platform_driver_unregister(&led191_platform_driver);

	PK_DBG("Exit done.\n");
}

module_init(flashlight_led191_init);
module_exit(flashlight_led191_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight LED191 GPIO Driver");

