/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/leds.h>


#include "flashlight-core.h"
#include "flashlight-dt.h"

#define TAG_NAME "[flashligh_sy7803_drv]"
#define PK_DBG_NONE(fmt, arg...)  do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)  pr_debug(TAG_NAME "%s: " fmt, __func__, ##arg)
#define PK_ERR(fmt, arg...)       pr_info(TAG_NAME "%s: " fmt, __func__, ##arg)

#define DEBUG_LEDS_STROBE
#ifdef DEBUG_LEDS_STROBE
#define PK_INF(fmt, arg...)       pr_info(TAG_NAME "%s is called.\n" fmt, __func__, ##arg)
#define PK_DBG                    PK_DBG_FUNC
#else
#define PK_INF(fmt, arg...)       do {} while (0)
#define PK_DBG(a, ...)
#endif


/* define device tree */
#ifndef SY7803_DTNAME
#define SY7803_DTNAME "mediatek,flashlights_sy7803"
#endif

#define SY7803_NAME "flashlights-sy7803"

/* define registers */

/* define mutex and work queue */
static DEFINE_MUTEX(sy7803_mutex);
static struct work_struct sy7803_work;

/* define pinctrl */
#define SY7803_PINCTRL_PIN_TORCH 0
#define SY7803_PINCTRL_PIN_FLASH 1
#define SY7803_PINCTRL_PINSTATE_LOW 0
#define SY7803_PINCTRL_PINSTATE_HIGH 1
#define SY7803_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define SY7803_PINCTRL_STATE_HWEN_LOW  "hwen_low"
#define SY7803_PINCTRL_STATE_HWCE_HIGH "hwce_high"
#define SY7803_PINCTRL_STATE_HWCE_LOW  "hwce_low"

static struct pinctrl *sy7803_pinctrl;
static struct pinctrl_state *sy7803_flash_high;
static struct pinctrl_state *sy7803_flash_low;
static struct pinctrl_state *sy7803_torch_high;
static struct pinctrl_state *sy7803_torch_low;

/* define usage count */
static int use_count;

static int g_flash_duty = -1;

/* platform data */
struct sy7803_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
	struct led_classdev led_cdev;
	struct led_classdev led_cdev_2;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int sy7803_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	sy7803_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(sy7803_pinctrl)) {
		PK_ERR("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(sy7803_pinctrl);
	}

	/*  Flashlight pin initialization */
	sy7803_flash_high = pinctrl_lookup_state(
			sy7803_pinctrl, SY7803_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(sy7803_flash_high)) {
		PK_ERR("Failed to init (%s)\n",
			SY7803_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(sy7803_flash_high);
	}
	sy7803_flash_low = pinctrl_lookup_state(
			sy7803_pinctrl, SY7803_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(sy7803_flash_low)) {
		PK_ERR("Failed to init (%s)\n", SY7803_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(sy7803_flash_low);
	}

	sy7803_torch_high = pinctrl_lookup_state(
			sy7803_pinctrl, SY7803_PINCTRL_STATE_HWCE_HIGH);
	if (IS_ERR(sy7803_torch_high)) {
		PK_ERR("Failed to init (%s)\n",
			SY7803_PINCTRL_STATE_HWCE_HIGH);
		ret = PTR_ERR(sy7803_torch_high);
	}
	sy7803_torch_low = pinctrl_lookup_state(
			sy7803_pinctrl, SY7803_PINCTRL_STATE_HWCE_LOW);
	if (IS_ERR(sy7803_torch_low)) {
		PK_ERR("Failed to init (%s)\n", SY7803_PINCTRL_STATE_HWCE_LOW);
		ret = PTR_ERR(sy7803_torch_low);
	}

	return ret;
}

static int sy7803_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(sy7803_pinctrl)) {
		PK_ERR("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case SY7803_PINCTRL_PIN_FLASH:
		if (state == SY7803_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(sy7803_flash_low))
			ret = pinctrl_select_state(
					sy7803_pinctrl, sy7803_flash_low);
		else if (state == SY7803_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(sy7803_flash_high))
			ret = pinctrl_select_state(
					sy7803_pinctrl, sy7803_flash_high);
		else
			PK_ERR("set err, pin(%d) state(%d)\n", pin, state);
		break;
	case SY7803_PINCTRL_PIN_TORCH:
		if (state == SY7803_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(sy7803_torch_low))
			ret = pinctrl_select_state(
					sy7803_pinctrl, sy7803_torch_low);
		else if (state == SY7803_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(sy7803_torch_high))
			ret = pinctrl_select_state(
					sy7803_pinctrl, sy7803_torch_high);
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
 * sy7803 operations
 *****************************************************************************/
/* flashlight enable function */
static int sy7803_enable(void)
{
	int pin_flash = SY7803_PINCTRL_PIN_FLASH;
	int pin_torch = SY7803_PINCTRL_PIN_TORCH;

	PK_INF("g_flash_duty %d\n", g_flash_duty);

	if (g_flash_duty == 0)   /* torch mode */
		sy7803_pinctrl_set(pin_torch, SY7803_PINCTRL_PINSTATE_HIGH);
	else                     /* flash mode */
		sy7803_pinctrl_set(pin_flash, SY7803_PINCTRL_PINSTATE_HIGH);

	return 0;
}

/* flashlight disable function */
static int sy7803_disable(void)
{
	int pin_flash = SY7803_PINCTRL_PIN_FLASH;
	int pin_torch = SY7803_PINCTRL_PIN_TORCH;
	int state = SY7803_PINCTRL_PINSTATE_LOW;

	sy7803_pinctrl_set(pin_torch, state);
	sy7803_pinctrl_set(pin_flash, state);

	return 0;
}

/* set flashlight level */
static int sy7803_set_level(int level)
{
	g_flash_duty = level;
	return 0;
}

/* flashlight init */
static int sy7803_init(void)
{
	int pin_flash = SY7803_PINCTRL_PIN_FLASH;
	int pin_torch = SY7803_PINCTRL_PIN_TORCH;
	int state = SY7803_PINCTRL_PINSTATE_LOW;

	sy7803_pinctrl_set(pin_torch, state);
	sy7803_pinctrl_set(pin_flash, state);

	return 0;
}

/* flashlight uninit */
static int sy7803_uninit(void)
{
	sy7803_disable();
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer sy7803_timer;
static unsigned int sy7803_timeout_ms;

static void sy7803_work_disable(struct work_struct *data)
{
	PK_DBG("work queue callback\n");
	sy7803_disable();
}

static enum hrtimer_restart sy7803_timer_func(struct hrtimer *timer)
{
	schedule_work(&sy7803_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int sy7803_ioctl(unsigned int cmd, unsigned long arg)
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
		PK_INF("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		sy7803_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PK_INF("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		sy7803_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PK_INF("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (sy7803_timeout_ms) {
				s = sy7803_timeout_ms / 1000;
				ns = sy7803_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&sy7803_timer, ktime,
						HRTIMER_MODE_REL);
			}
			sy7803_enable();
		} else {
			sy7803_disable();
			hrtimer_cancel(&sy7803_timer);
		}
		break;
	default:
		PK_INF("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sy7803_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sy7803_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sy7803_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&sy7803_mutex);
	if (set) {
		if (!use_count)
			ret = sy7803_init();
		use_count++;
		PK_DBG("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sy7803_uninit();
		if (use_count < 0)
			use_count = 0;
		PK_DBG("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&sy7803_mutex);

	return ret;
}

static ssize_t sy7803_strobe_store(struct flashlight_arg arg)
{
	sy7803_set_driver(1);
	sy7803_set_level(arg.level);
	sy7803_timeout_ms = 0;
	sy7803_enable();
	msleep(arg.dur);
	sy7803_disable();
	sy7803_set_driver(0);

	return 0;
}

static struct flashlight_operations sy7803_ops = {
	sy7803_open,
	sy7803_release,
	sy7803_ioctl,
	sy7803_strobe_store,
	sy7803_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sy7803_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * sy7803_init();
	 */

	return 0;
}

static int sy7803_parse_dt(struct device *dev,
		struct sy7803_platform_data *pdata)
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
				SY7803_NAME);
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

static void flashlight_led_brightness_set(struct led_classdev *led_cdev,enum led_brightness value)
{
	if(value > 0){
		led_cdev->brightness = 100;
		sy7803_pinctrl_set(SY7803_PINCTRL_PIN_TORCH, SY7803_PINCTRL_PINSTATE_HIGH);
	}else{
		led_cdev->brightness = 0;
		sy7803_pinctrl_set(SY7803_PINCTRL_PIN_TORCH, SY7803_PINCTRL_PINSTATE_LOW);
	}
}

static enum led_brightness flashlight_led_brightness_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int sy7803_probe(struct platform_device *pdev)
{
	struct sy7803_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;


	PK_DBG("2020.02.14 wsy Probe start.\n");

	/* init pinctrl */
	if (sy7803_pinctrl_init(pdev)) {
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
		err = sy7803_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}
	pdata->led_cdev.name = "flashlight";
	pdata->led_cdev.brightness = 0;
	pdata->led_cdev.max_brightness = 100;
	pdata->led_cdev.default_trigger = "none";
	pdata->led_cdev.brightness_set = flashlight_led_brightness_set;
	pdata->led_cdev.brightness_get = flashlight_led_brightness_get;

    pdata->led_cdev_2.name = "torch-light0";
	pdata->led_cdev_2.brightness = 0;
	pdata->led_cdev_2.max_brightness = 100;
	pdata->led_cdev_2.default_trigger = "none";
	pdata->led_cdev_2.brightness_set = flashlight_led_brightness_set;
	pdata->led_cdev_2.brightness_get = flashlight_led_brightness_get;

	/* init work queue */
	INIT_WORK(&sy7803_work, sy7803_work_disable);

	/* init timer */
	hrtimer_init(&sy7803_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sy7803_timer.function = sy7803_timer_func;
	sy7803_timeout_ms = 100;

	/* init chip hw */
	sy7803_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&sy7803_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(SY7803_NAME, &sy7803_ops)) {
			err = -EFAULT;
			goto err;
		}
	}
	if(led_classdev_register(&pdev->dev,&pdata->led_cdev)){
			err = -EFAULT;
			goto err;
		}

	if(led_classdev_register(&pdev->dev,&pdata->led_cdev_2)){
			err = -EFAULT;
			goto err;
		}

	PK_DBG("Probe done.\n");

	return 0;
err:
	return err;
}

static int sy7803_remove(struct platform_device *pdev)
{
	struct sy7803_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	PK_DBG("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(SY7803_NAME);

	/* flush work queue */
	flush_work(&sy7803_work);

	PK_DBG("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sy7803_gpio_of_match[] = {
	{.compatible = SY7803_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, sy7803_gpio_of_match);
#else
static struct platform_device sy7803_gpio_platform_device[] = {
	{
		.name = SY7803_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sy7803_gpio_platform_device);
#endif

static struct platform_driver sy7803_platform_driver = {
	.probe = sy7803_probe,
	.remove = sy7803_remove,
	.driver = {
		.name = SY7803_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sy7803_gpio_of_match,
#endif
	},
};

static int __init flashlight_sy7803_init(void)
{
	int ret;

	PK_DBG("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sy7803_gpio_platform_device);
	if (ret) {
		PK_ERR("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&sy7803_platform_driver);
	if (ret) {
		PK_ERR("Failed to register platform driver\n");
		return ret;
	}

	PK_DBG("Init done.\n");

	return 0;
}

static void __exit flashlight_sy7803_exit(void)
{
	PK_DBG("Exit start.\n");

	platform_driver_unregister(&sy7803_platform_driver);

	PK_DBG("Exit done.\n");
}

module_init(flashlight_sy7803_init);
module_exit(flashlight_sy7803_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dongchun Zhu <dongchun.zhu@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight SY7803 GPIO Driver");

