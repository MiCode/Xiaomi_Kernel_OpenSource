// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include "richtek/rt-flashlight.h"
#include "mtk_charger.h"

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef RT5081_DTNAME
#define RT5081_DTNAME "mediatek,flashlights_rt5081"
#endif

#define RT5081_NAME "flashlights-rt5081"

/* define channel, level */
#define RT5081_CHANNEL_NUM 2
#define RT5081_CHANNEL_CH1 0
#define RT5081_CHANNEL_CH2 1

#define RT5081_NONE (-1)
#define RT5081_DISABLE 0
#define RT5081_ENABLE 1
#define RT5081_ENABLE_TORCH 1
#define RT5081_ENABLE_FLASH 2

#define RT5081_LEVEL_NUM 32
#define RT5081_LEVEL_TORCH 16
#define RT5081_LEVEL_FLASH RT5081_LEVEL_NUM
#define RT5081_WDT_TIMEOUT 1248 /* ms */
#define RT5081_HW_TIMEOUT 400 /* ms */

/* define mutex, work queue and timer */
static DEFINE_MUTEX(rt5081_mutex);
static struct work_struct rt5081_work_ch1;
static struct work_struct rt5081_work_ch2;
static struct hrtimer rt5081_timer_ch1;
static struct hrtimer rt5081_timer_ch2;
static unsigned int rt5081_timeout_ms[RT5081_CHANNEL_NUM];

/* define usage count */
static int use_count;

/* define RTK flashlight device */
static struct flashlight_device *flashlight_dev_ch1;
static struct flashlight_device *flashlight_dev_ch2;
#define RT_FLED_DEVICE_CH1  "rt-flash-led1"
#define RT_FLED_DEVICE_CH2  "rt-flash-led2"

/* define charger consumer */
static struct charger_consumer *flashlight_charger_consumer;
#define CHARGER_SUPPLY_NAME "charger_port1"

/* is decrease voltage */
static int is_decrease_voltage;

/* platform data */
struct rt5081_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * rt5081 operations
 *****************************************************************************/
static const int rt5081_current[RT5081_LEVEL_NUM] = {
	  25,   50,  75, 100, 125, 150, 175,  200,  225,  250,
	 275,  300, 325, 350, 375, 400, 450,  500,  550,  600,
	 650,  700, 750, 800, 850, 900, 950, 1000, 1050, 1100,
	1150, 1200
};

static const unsigned char rt5081_torch_level[RT5081_LEVEL_TORCH] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12,
	0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E
};

static const unsigned char rt5081_strobe_level[RT5081_LEVEL_FLASH] = {
	0x80, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10,
	0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x20, 0x24, 0x28, 0x2C,
	0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54,
	0x58, 0x5C
};

static int rt5081_decouple_mode;
static int rt5081_en_ch1;
static int rt5081_en_ch2;
static int rt5081_level_ch1;
static int rt5081_level_ch2;

static int rt5081_is_charger_ready(void)
{
	if (flashlight_is_ready(flashlight_dev_ch1) &&
			flashlight_is_ready(flashlight_dev_ch2))
		return FLASHLIGHT_CHARGER_READY;
	else
		return FLASHLIGHT_CHARGER_NOT_READY;
}

static int rt5081_is_torch(int level)
{
	if (level >= RT5081_LEVEL_TORCH)
		return -1;

	return 0;
}

static int rt5081_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= RT5081_LEVEL_NUM)
		level = RT5081_LEVEL_NUM - 1;

	return level;
}

/* flashlight enable function */
static int rt5081_enable(void)
{
	int ret = 0;
	enum flashlight_mode mode = FLASHLIGHT_MODE_TORCH;

	if (!flashlight_dev_ch1 || !flashlight_dev_ch2) {
		pr_info("Failed to enable since no flashlight device.\n");
		return -1;
	}

	/* set flash mode if any channel is flash mode */
	if ((rt5081_en_ch1 == RT5081_ENABLE_FLASH)
			|| (rt5081_en_ch2 == RT5081_ENABLE_FLASH))
		mode = FLASHLIGHT_MODE_FLASH;

	/* enable channel 1 and channel 2 */
	if (rt5081_en_ch1)
		ret |= flashlight_set_mode(
				flashlight_dev_ch1, mode);
	else
		ret |= flashlight_set_mode(
				flashlight_dev_ch1, FLASHLIGHT_MODE_OFF);
	if (rt5081_en_ch2)
		ret |= flashlight_set_mode(
				flashlight_dev_ch2, mode);
	else
		ret |= flashlight_set_mode(
				flashlight_dev_ch2, FLASHLIGHT_MODE_OFF);

	if (ret < 0)
		pr_info("Failed to enable.\n");

	return ret;
}

/* flashlight disable function */
static int rt5081_disable(void)
{
	int ret = 0;

	if (!flashlight_dev_ch1 || !flashlight_dev_ch2) {
		pr_info("Failed to disable since no flashlight device.\n");
		return -1;
	}

	/* disable channel 1 and channel 2 */
	ret |= flashlight_set_mode(flashlight_dev_ch1, FLASHLIGHT_MODE_OFF);
	ret |= flashlight_set_mode(flashlight_dev_ch2, FLASHLIGHT_MODE_OFF);

	if (ret < 0)
		pr_info("Failed to disable.\n");

	return ret;
}

/* set flashlight level */
static int rt5081_set_level_ch1(int level)
{
	level = rt5081_verify_level(level);
	rt5081_level_ch1 = level;

	if (!flashlight_dev_ch1) {
		pr_info("Failed to set ht level since no flashlight device.\n");
		return -1;
	}

	/* set brightness level */
	if (!rt5081_is_torch(level))
		flashlight_set_torch_brightness(
				flashlight_dev_ch1, rt5081_torch_level[level]);
	flashlight_set_strobe_brightness(
			flashlight_dev_ch1, rt5081_strobe_level[level]);

	return 0;
}

static int rt5081_set_level_ch2(int level)
{
	level = rt5081_verify_level(level);
	rt5081_level_ch2 = level;

	if (!flashlight_dev_ch2) {
		pr_info("Failed to set lt level since no flashlight device.\n");
		return -1;
	}

	/* set brightness level */
	if (!rt5081_is_torch(level))
		flashlight_set_torch_brightness(
				flashlight_dev_ch2, rt5081_torch_level[level]);
	flashlight_set_strobe_brightness(
			flashlight_dev_ch2, rt5081_strobe_level[level]);

	return 0;
}

static int rt5081_set_level(int channel, int level)
{
	if (channel == RT5081_CHANNEL_CH1)
		rt5081_set_level_ch1(level);
	else if (channel == RT5081_CHANNEL_CH2)
		rt5081_set_level_ch2(level);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int rt5081_set_scenario(int scenario)
{
	/* set decouple mode */
	rt5081_decouple_mode = scenario & FLASHLIGHT_SCENARIO_DECOUPLE_MASK;

	/* notify charger to increase or decrease voltage */
	if (!flashlight_charger_consumer) {
		pr_info("Failed with no charger consumer handler.\n");
		return -1;
	}

	mutex_lock(&rt5081_mutex);
	if (scenario & FLASHLIGHT_SCENARIO_CAMERA_MASK) {
		if (!is_decrease_voltage) {
			pr_info("Decrease voltage level.\n");
			charger_manager_enable_high_voltage_charging(
					flashlight_charger_consumer, false);
			is_decrease_voltage = 1;
		}
	} else {
		if (is_decrease_voltage) {
			pr_info("Increase voltage level.\n");
			charger_manager_enable_high_voltage_charging(
					flashlight_charger_consumer, true);
			is_decrease_voltage = 0;
		}
	}
	mutex_unlock(&rt5081_mutex);

	return 0;
}

/* flashlight init */
static int rt5081_init(void)
{
	/* clear flashlight state */
	rt5081_en_ch1 = RT5081_NONE;
	rt5081_en_ch2 = RT5081_NONE;

	/* clear decouple mode */
	rt5081_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;

	/* clear charger status */
	is_decrease_voltage = 0;

	return 0;
}

/* flashlight uninit */
static int rt5081_uninit(void)
{
	/* clear flashlight state */
	rt5081_en_ch1 = RT5081_NONE;
	rt5081_en_ch2 = RT5081_NONE;

	/* clear decouple mode */
	rt5081_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;

	/* clear charger status */
	is_decrease_voltage = 0;

	return rt5081_disable();
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static void rt5081_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	rt5081_disable();
}

static void rt5081_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	rt5081_disable();
}

static enum hrtimer_restart rt5081_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&rt5081_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart rt5081_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&rt5081_work_ch2);
	return HRTIMER_NORESTART;
}

static int rt5081_timer_start(int channel, ktime_t ktime)
{
	if (channel == RT5081_CHANNEL_CH1)
		hrtimer_start(&rt5081_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == RT5081_CHANNEL_CH2)
		hrtimer_start(&rt5081_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int rt5081_timer_cancel(int channel)
{
	if (channel == RT5081_CHANNEL_CH1)
		hrtimer_cancel(&rt5081_timer_ch1);
	else if (channel == RT5081_CHANNEL_CH2)
		hrtimer_cancel(&rt5081_timer_ch2);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * Flashlight operation wrapper function
 *****************************************************************************/
static int rt5081_operate(int channel, int enable)
{
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	/* setup enable/disable */
	if (channel == RT5081_CHANNEL_CH1) {
		rt5081_en_ch1 = enable;
		if (rt5081_en_ch1)
			if (rt5081_is_torch(rt5081_level_ch1))
				rt5081_en_ch1 = RT5081_ENABLE_FLASH;
	} else if (channel == RT5081_CHANNEL_CH2) {
		rt5081_en_ch2 = enable;
		if (rt5081_en_ch2)
			if (rt5081_is_torch(rt5081_level_ch2))
				rt5081_en_ch2 = RT5081_ENABLE_FLASH;
	} else {
		pr_info("Error channel\n");
		return -1;
	}

	/* decouple mode */
	if (rt5081_decouple_mode) {
		if (channel == RT5081_CHANNEL_CH1) {
			rt5081_en_ch2 = RT5081_DISABLE;
			rt5081_timeout_ms[RT5081_CHANNEL_CH2] = 0;
		} else if (channel == RT5081_CHANNEL_CH2) {
			rt5081_en_ch1 = RT5081_DISABLE;
			rt5081_timeout_ms[RT5081_CHANNEL_CH1] = 0;
		}
	}

	/* operate flashlight and setup timer */
	if ((rt5081_en_ch1 != RT5081_NONE) && (rt5081_en_ch2 != RT5081_NONE)) {
		if ((rt5081_en_ch1 == RT5081_DISABLE) &&
				(rt5081_en_ch2 == RT5081_DISABLE)) {
			rt5081_disable();
			rt5081_timer_cancel(RT5081_CHANNEL_CH1);
			rt5081_timer_cancel(RT5081_CHANNEL_CH2);
		} else {
			if (rt5081_timeout_ms[RT5081_CHANNEL_CH1]) {
				s = rt5081_timeout_ms[RT5081_CHANNEL_CH1]
					/ 1000;
				ns = rt5081_timeout_ms[RT5081_CHANNEL_CH1]
						% 1000 * 1000000;
				ktime = ktime_set(s, ns);
				rt5081_timer_start(RT5081_CHANNEL_CH1, ktime);
			}
			if (rt5081_timeout_ms[RT5081_CHANNEL_CH2]) {
				s = rt5081_timeout_ms[RT5081_CHANNEL_CH2]
					/ 1000;
				ns = rt5081_timeout_ms[RT5081_CHANNEL_CH2]
						% 1000 * 1000000;
				ktime = ktime_set(s, ns);
				rt5081_timer_start(RT5081_CHANNEL_CH2, ktime);
			}
			rt5081_enable();
		}

		/* clear flashlight state */
		rt5081_en_ch1 = RT5081_NONE;
		rt5081_en_ch2 = RT5081_NONE;
	}

	return 0;
}

/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int rt5081_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= RT5081_CHANNEL_NUM) {
		pr_info("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		rt5081_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		rt5081_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		rt5081_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		rt5081_operate(channel, fl_arg->arg);
		break;

	case FLASH_IOC_IS_CHARGER_READY:
		pr_debug("FLASH_IOC_IS_CHARGER_READY(%d)\n", channel);
		fl_arg->arg = rt5081_is_charger_ready();
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = RT5081_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = RT5081_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = rt5081_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = rt5081_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = RT5081_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int rt5081_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int rt5081_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int rt5081_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&rt5081_mutex);
	if (set) {
		if (!use_count)
			ret = rt5081_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = rt5081_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&rt5081_mutex);

	return ret;
}

static ssize_t rt5081_strobe_store(struct flashlight_arg arg)
{
	rt5081_set_driver(1);
	rt5081_set_scenario(
			FLASHLIGHT_SCENARIO_CAMERA |
			FLASHLIGHT_SCENARIO_COUPLE);
	rt5081_set_level(arg.channel, arg.level);
	rt5081_timeout_ms[arg.channel] = 0;

	if (arg.level < 0)
		rt5081_operate(arg.channel, RT5081_DISABLE);
	else
		rt5081_operate(arg.channel, RT5081_ENABLE);

	msleep(arg.dur);
	rt5081_set_scenario(
			FLASHLIGHT_SCENARIO_FLASHLIGHT |
			FLASHLIGHT_SCENARIO_COUPLE);
	rt5081_operate(arg.channel, RT5081_DISABLE);
	rt5081_set_driver(0);

	return 0;
}

static struct flashlight_operations rt5081_ops = {
	rt5081_open,
	rt5081_release,
	rt5081_ioctl,
	rt5081_strobe_store,
	rt5081_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int rt5081_parse_dt(struct device *dev,
		struct rt5081_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

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
				RT5081_NAME);
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

static int rt5081_probe(struct platform_device *pdev)
{
	struct rt5081_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int ret;
	int i;

	pr_debug("Probe start.\n");

	/* parse dt */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		pdev->dev.platform_data = pdata;
		ret = rt5081_parse_dt(&pdev->dev, pdata);
		if (ret)
			return ret;
	}

	/* init work queue */
	INIT_WORK(&rt5081_work_ch1, rt5081_work_disable_ch1);
	INIT_WORK(&rt5081_work_ch2, rt5081_work_disable_ch2);

	/* init timer */
	hrtimer_init(&rt5081_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt5081_timer_ch1.function = rt5081_timer_func_ch1;
	hrtimer_init(&rt5081_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt5081_timer_ch2.function = rt5081_timer_func_ch2;
	rt5081_timeout_ms[RT5081_CHANNEL_CH1] = 600;
	rt5081_timeout_ms[RT5081_CHANNEL_CH2] = 600;

	/* clear attributes */
	use_count = 0;
	is_decrease_voltage = 0;

	/* get RTK flashlight handler */
	flashlight_dev_ch1 = find_flashlight_by_name(RT_FLED_DEVICE_CH1);
	if (!flashlight_dev_ch1) {
		pr_info("Failed to get ht flashlight device.\n");
		return -EFAULT;
	}
	flashlight_dev_ch2 = find_flashlight_by_name(RT_FLED_DEVICE_CH2);
	if (!flashlight_dev_ch2) {
		pr_info("Failed to get lt flashlight device.\n");
		return -EFAULT;
	}

	/* setup strobe mode timeout */
	if (flashlight_set_strobe_timeout(flashlight_dev_ch1,
				RT5081_HW_TIMEOUT, RT5081_HW_TIMEOUT + 200) < 0)
		pr_info("Failed to set strobe timeout.\n");

	/* get charger consumer manager */
	flashlight_charger_consumer = charger_manager_get_by_name(
			&flashlight_dev_ch1->dev, CHARGER_SUPPLY_NAME);
	if (!flashlight_charger_consumer) {
		pr_info("Failed to get charger manager.\n");
		return -EFAULT;
	}

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i], &rt5081_ops))
				return -EFAULT;
	} else {
		if (flashlight_dev_register(RT5081_NAME, &rt5081_ops))
			return -EFAULT;
	}

	pr_debug("Probe done.\n");

	return 0;
}

static int rt5081_remove(struct platform_device *pdev)
{
	struct rt5081_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(RT5081_NAME);

	/* flush work queue */
	flush_work(&rt5081_work_ch1);
	flush_work(&rt5081_work_ch2);

	/* clear RTK flashlight device */
	flashlight_dev_ch1 = NULL;
	flashlight_dev_ch2 = NULL;

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rt5081_of_match[] = {
	{.compatible = RT5081_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, rt5081_of_match);
#else
static struct platform_device rt5081_platform_device[] = {
	{
		.name = RT5081_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, rt5081_platform_device);
#endif

static struct platform_driver rt5081_platform_driver = {
	.probe = rt5081_probe,
	.remove = rt5081_remove,
	.driver = {
		.name = RT5081_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rt5081_of_match,
#endif
	},
};

static int __init flashlight_rt5081_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&rt5081_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&rt5081_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_rt5081_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&rt5081_platform_driver);

	pr_debug("Exit done.\n");
}

/* replace module_init() since conflict in kernel init process */
late_initcall(flashlight_rt5081_init);
module_exit(flashlight_rt5081_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight RT5081 Driver");

