/*
 * Copyright (C) 2019 MediaTek Inc.
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

#define TAG_NAME "[flashligh_sywt78_drv]"
#define PK_DBG_NONE(fmt, arg...)  do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)  pr_debug(TAG_NAME "%s: " fmt, __func__, ##arg)
#define PK_ERR(fmt, arg...)       pr_info(TAG_NAME "%s: " fmt, __func__, ##arg)

#define DEBUG_LEDS_STROBE
#ifdef DEBUG_LEDS_STROBE
#define PK_INF(fmt, arg...)       pr_info(TAG_NAME "%s is called.\n", __func__)
#define PK_DBG                    PK_DBG_FUNC
#else
#define PK_INF(fmt, arg...)       do {} while (0)
#define PK_DBG(a, ...)
#endif


/* define device tree */
#ifndef SYWT78_DTNAME
#define SYWT78_DTNAME "mediatek,flashlights_sywt78"
#endif

#define SYWT78_NAME "flashlights-sywt78"

/* define registers */

/* define mutex and work queue */
static DEFINE_MUTEX(sywt78_mutex);
static struct work_struct sywt78_work;

/* define pinctrl */
#define SYWT78_PINCTRL_PIN_TORCH 0
#define SYWT78_PINCTRL_PIN_FLASH 1
#define SYWT78_PINCTRL_PINSTATE_LOW 0
#define SYWT78_PINCTRL_PINSTATE_HIGH 1
#define SYWT78_PINCTRL_STATE_FLASH_HIGH "flash_high"
#define SYWT78_PINCTRL_STATE_FLASH_LOW  "flash_low"
#define SYWT78_PINCTRL_STATE_TORCH_HIGH "torch_high"
#define SYWT78_PINCTRL_STATE_TORCH_LOW  "torch_low"

static struct pinctrl *sywt78_pinctrl;
static struct pinctrl_state *sywt78_flash_high;
static struct pinctrl_state *sywt78_flash_low;
static struct pinctrl_state *sywt78_torch_high;
static struct pinctrl_state *sywt78_torch_low;

/* define usage count */
static int use_count;

static int g_flash_duty = -1;

/* platform data */
struct sywt78_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int sywt78_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	sywt78_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(sywt78_pinctrl)) {
		PK_ERR("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(sywt78_pinctrl);
	}

	/*  Flashlight pin initialization */
	sywt78_flash_high = pinctrl_lookup_state(
			sywt78_pinctrl, SYWT78_PINCTRL_STATE_FLASH_HIGH);
	if (IS_ERR(sywt78_flash_high)) {
		PK_ERR("Failed to init (%s)\n",
			SYWT78_PINCTRL_STATE_FLASH_HIGH);
		ret = PTR_ERR(sywt78_flash_high);
	}
	sywt78_flash_low = pinctrl_lookup_state(
			sywt78_pinctrl, SYWT78_PINCTRL_STATE_FLASH_LOW);
	if (IS_ERR(sywt78_flash_low)) {
		PK_ERR("Failed to init (%s)\n", SYWT78_PINCTRL_STATE_FLASH_LOW);
		ret = PTR_ERR(sywt78_flash_low);
	}

	sywt78_torch_high = pinctrl_lookup_state(
			sywt78_pinctrl, SYWT78_PINCTRL_STATE_TORCH_HIGH);
	if (IS_ERR(sywt78_torch_high)) {
		PK_ERR("Failed to init (%s)\n",
			SYWT78_PINCTRL_STATE_TORCH_HIGH);
		ret = PTR_ERR(sywt78_torch_high);
	}
	sywt78_torch_low = pinctrl_lookup_state(
			sywt78_pinctrl, SYWT78_PINCTRL_STATE_TORCH_LOW);
	if (IS_ERR(sywt78_torch_low)) {
		PK_ERR("Failed to init (%s)\n", SYWT78_PINCTRL_STATE_TORCH_LOW);
		ret = PTR_ERR(sywt78_torch_low);
	}

	return ret;
}

static int sywt78_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(sywt78_pinctrl)) {
		PK_ERR("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case SYWT78_PINCTRL_PIN_FLASH:
		if (state == SYWT78_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(sywt78_flash_low))
			ret = pinctrl_select_state(
					sywt78_pinctrl, sywt78_flash_low);
		else if (state == SYWT78_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(sywt78_flash_high))
			ret = pinctrl_select_state(
					sywt78_pinctrl, sywt78_flash_high);
		else
			PK_ERR("set err, pin(%d) state(%d)\n", pin, state);
		break;
	case SYWT78_PINCTRL_PIN_TORCH:
		if (state == SYWT78_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(sywt78_torch_low))
			ret = pinctrl_select_state(
					sywt78_pinctrl, sywt78_torch_low);
		else if (state == SYWT78_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(sywt78_torch_high))
			ret = pinctrl_select_state(
					sywt78_pinctrl, sywt78_torch_high);
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
 * sywt78 operations
 *****************************************************************************/
/* flashlight enable function */
static int sywt78_enable(void)
{
	int pin_flash = SYWT78_PINCTRL_PIN_FLASH;
	int pin_torch = SYWT78_PINCTRL_PIN_TORCH;

	PK_DBG("g_flash_duty %d\n", g_flash_duty);

	if (g_flash_duty == 1)   /* flash mode */
		sywt78_pinctrl_set(pin_flash, SYWT78_PINCTRL_PINSTATE_HIGH);
	else                     /* torch mode */
		sywt78_pinctrl_set(pin_torch, SYWT78_PINCTRL_PINSTATE_HIGH);

	return 0;
}

/* flashlight disable function */
static int sywt78_disable(void)
{
	int pin_flash = SYWT78_PINCTRL_PIN_FLASH;
	int pin_torch = SYWT78_PINCTRL_PIN_TORCH;
	int state = SYWT78_PINCTRL_PINSTATE_LOW;

	sywt78_pinctrl_set(pin_torch, state);
	sywt78_pinctrl_set(pin_flash, state);

	return 0;
}

/* set flashlight level */
static int sywt78_set_level(int level)
{
	g_flash_duty = level;
	return 0;
}

/* flashlight init */
static int sywt78_init(void)
{
	int pin_flash = SYWT78_PINCTRL_PIN_FLASH;
	int pin_torch = SYWT78_PINCTRL_PIN_TORCH;
	int state = SYWT78_PINCTRL_PINSTATE_LOW;

	sywt78_pinctrl_set(pin_torch, state);
	sywt78_pinctrl_set(pin_flash, state);

	return 0;
}

/* flashlight uninit */
static int sywt78_uninit(void)
{
	sywt78_disable();
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer sywt78_timer;
static unsigned int sywt78_timeout_ms;

static void sywt78_work_disable(struct work_struct *data)
{
	PK_DBG("work queue callback\n");
	sywt78_disable();
}

static enum hrtimer_restart sywt78_timer_func(struct hrtimer *timer)
{
	schedule_work(&sywt78_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int sywt78_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		sywt78_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PK_DBG("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		sywt78_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PK_DBG("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (sywt78_timeout_ms) {
				s = sywt78_timeout_ms / 1000;
				ns = sywt78_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&sywt78_timer, ktime,
						HRTIMER_MODE_REL);
			}
			sywt78_enable();
		} else {
			sywt78_disable();
			hrtimer_cancel(&sywt78_timer);
		}
		break;
	default:
		PK_INF("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sywt78_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sywt78_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sywt78_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&sywt78_mutex);
	if (set) {
		if (!use_count)
			ret = sywt78_init();
		use_count++;
		PK_DBG("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sywt78_uninit();
		if (use_count < 0)
			use_count = 0;
		PK_DBG("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&sywt78_mutex);

	return ret;
}

static ssize_t sywt78_strobe_store(struct flashlight_arg arg)
{
	sywt78_set_driver(1);
	sywt78_set_level(arg.level);
	sywt78_timeout_ms = 0;
	sywt78_enable();
	msleep(arg.dur);
	sywt78_disable();
	sywt78_set_driver(0);

	return 0;
}

static struct flashlight_operations sywt78_ops = {
	sywt78_open,
	sywt78_release,
	sywt78_ioctl,
	sywt78_strobe_store,
	sywt78_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sywt78_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * sywt78_init();
	 */

	return 0;
}

static int sywt78_parse_dt(struct device *dev,
		struct sywt78_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		PK_INF("Parse no dt, node.\n");
		return 0;
	}
	PK_INF("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		PK_INF("Parse no dt, decouple.\n");

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
				SYWT78_NAME);
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

static int sywt78_probe(struct platform_device *pdev)
{
	struct sywt78_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	PK_DBG("Probe start.\n");

	/* init pinctrl */
	if (sywt78_pinctrl_init(pdev)) {
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
		err = sywt78_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&sywt78_work, sywt78_work_disable);

	/* init timer */
	hrtimer_init(&sywt78_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sywt78_timer.function = sywt78_timer_func;
	sywt78_timeout_ms = 100;

	/* init chip hw */
	sywt78_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&sywt78_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(SYWT78_NAME, &sywt78_ops)) {
			err = -EFAULT;
			goto err;
		}
	}

	PK_DBG("Probe done.\n");

	return 0;
err:
	return err;
}

static int sywt78_remove(struct platform_device *pdev)
{
	struct sywt78_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	PK_DBG("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(SYWT78_NAME);

	/* flush work queue */
	flush_work(&sywt78_work);

	PK_DBG("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sywt78_gpio_of_match[] = {
	{.compatible = SYWT78_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, sywt78_gpio_of_match);
#else
static struct platform_device sywt78_gpio_platform_device[] = {
	{
		.name = SYWT78_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sywt78_gpio_platform_device);
#endif

static struct platform_driver sywt78_platform_driver = {
	.probe = sywt78_probe,
	.remove = sywt78_remove,
	.driver = {
		.name = SYWT78_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sywt78_gpio_of_match,
#endif
	},
};

static int __init flashlight_sywt78_init(void)
{
	int ret;

	PK_DBG("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sywt78_gpio_platform_device);
	if (ret) {
		PK_ERR("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&sywt78_platform_driver);
	if (ret) {
		PK_ERR("Failed to register platform driver\n");
		return ret;
	}

	PK_DBG("Init done.\n");

	return 0;
}

static void __exit flashlight_sywt78_exit(void)
{
	PK_DBG("Exit start.\n");

	platform_driver_unregister(&sywt78_platform_driver);

	PK_DBG("Exit done.\n");
}

module_init(flashlight_sywt78_init);
module_exit(flashlight_sywt78_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dongchun Zhu <dongchun.zhu@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight SYWT78 GPIO Driver");

