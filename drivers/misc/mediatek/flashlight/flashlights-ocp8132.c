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
#include <linux/leds.h>




#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

#define TAG_NAME "[flashligh_ocp8132_drv]"
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
#ifndef OCP8132_DTNAME
#define OCP8132_DTNAME "mediatek,flashlights_ocp8132"
#endif

#define OCP8132_NAME "flashlights_ocp8132"

/* define registers */

/* define mutex and work queue */
static DEFINE_MUTEX(ocp8132_mutex);
static struct work_struct ocp8132_work;

/* define pinctrl */
//#define OCP8132_PINCTRL_PIN_HWEN 0
#define OCP8132_PINCTRL_PIN_TORCH 0
#define OCP8132_PINCTRL_PIN_FLASH 1
#define OCP8132_PINCTRL_PINSTATE_LOW 0
#define OCP8132_PINCTRL_PINSTATE_HIGH 1
#define OCP8132_PINCTRL_STATE_HW_CH0_HIGH "hw_ch0_high"
#define OCP8132_PINCTRL_STATE_HW_CH0_LOW  "hw_ch0_low"
#define OCP8132_PINCTRL_STATE_HW_CH1_HIGH "hw_ch1_high"
#define OCP8132_PINCTRL_STATE_HW_CH1_LOW  "hw_ch1_low"


static struct pinctrl *ocp8132_pinctrl;
static struct pinctrl_state *ocp8132_hw_ch0_high;
static struct pinctrl_state *ocp8132_hw_ch0_low;
static struct pinctrl_state *ocp8132_hw_ch1_high;
static struct pinctrl_state *ocp8132_hw_ch1_low;

/* define usage count */
static int use_count;
static int g_flash_duty = -1;
static int g_flash_channel_idx;

static char node_one_buf[20] = {"0"};
static unsigned int flash_enable;


/* platform data */
struct ocp8132_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int ocp8132_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	ocp8132_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ocp8132_pinctrl)) {
		PK_ERR("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(ocp8132_pinctrl);
		return -1;
	}

	/*  Flashlight pin initialization */
	ocp8132_hw_ch0_high = pinctrl_lookup_state(ocp8132_pinctrl,
		OCP8132_PINCTRL_STATE_HW_CH0_HIGH);
	if (IS_ERR(ocp8132_hw_ch0_high)) {
		PK_ERR("Failed to init (%s)\n",
			OCP8132_PINCTRL_STATE_HW_CH0_HIGH);
		ret = PTR_ERR(ocp8132_hw_ch0_high);
	}
	ocp8132_hw_ch0_low = pinctrl_lookup_state(ocp8132_pinctrl,
		OCP8132_PINCTRL_STATE_HW_CH0_LOW);
	if (IS_ERR(ocp8132_hw_ch0_low)) {
		PK_ERR("Failed to init (%s)\n",
			OCP8132_PINCTRL_STATE_HW_CH0_LOW);
		ret = PTR_ERR(ocp8132_hw_ch0_low);
	}

	ocp8132_hw_ch1_high = pinctrl_lookup_state(ocp8132_pinctrl,
		OCP8132_PINCTRL_STATE_HW_CH1_HIGH);
	if (IS_ERR(ocp8132_hw_ch1_high)) {
		PK_ERR("Failed to init (%s)\n",
			OCP8132_PINCTRL_STATE_HW_CH1_HIGH);
		ret = PTR_ERR(ocp8132_hw_ch1_high);
	}
	ocp8132_hw_ch1_low = pinctrl_lookup_state(ocp8132_pinctrl,
		OCP8132_PINCTRL_STATE_HW_CH1_LOW);
	if (IS_ERR(ocp8132_hw_ch1_low)) {
		PK_ERR("Failed to init (%s)\n",
			OCP8132_PINCTRL_STATE_HW_CH1_LOW);
		ret = PTR_ERR(ocp8132_hw_ch1_low);
	}

	return ret;
}

static int ocp8132_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(ocp8132_pinctrl)) {
		PK_ERR("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case OCP8132_PINCTRL_PIN_TORCH:
		if (state == OCP8132_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(ocp8132_hw_ch0_low))
			ret = pinctrl_select_state(ocp8132_pinctrl,
				ocp8132_hw_ch0_low);
		else if (state == OCP8132_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(ocp8132_hw_ch0_high))
			ret = pinctrl_select_state(ocp8132_pinctrl,
				ocp8132_hw_ch0_high);
		else
			PK_ERR("set err, pin(%d) state(%d)\n", pin, state);
		break;
	case OCP8132_PINCTRL_PIN_FLASH:
		if (state == OCP8132_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(ocp8132_hw_ch1_low))
			ret = pinctrl_select_state(ocp8132_pinctrl,
				ocp8132_hw_ch1_low);
		else if (state == OCP8132_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(ocp8132_hw_ch1_high))
			ret = pinctrl_select_state(ocp8132_pinctrl,
				ocp8132_hw_ch1_high);
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
 * ocp8132 operations
 *****************************************************************************/
/* flashlight enable function */
static int ocp8132_enable(void)  //FIXME: 0=152 ENM;  1=153 ENF
{
	//int pin = OCP8132_PINCTRL_PIN_HWEN;
	int pin_flash = OCP8132_PINCTRL_PIN_FLASH;
	int pin_torch = OCP8132_PINCTRL_PIN_TORCH;

	PK_DBG("g_flash_duty %d\n", g_flash_duty);

	if (g_flash_duty == 6)   /* torch mode */
		ocp8132_pinctrl_set(pin_torch, OCP8132_PINCTRL_PINSTATE_HIGH);
	else                     /* flash mode */
		ocp8132_pinctrl_set(pin_flash, OCP8132_PINCTRL_PINSTATE_HIGH);

	return 0;
}

/* flashlight disable function */
static int ocp8132_disable(void)
{
	int pin_flash = OCP8132_PINCTRL_PIN_FLASH;
	int pin_torch = OCP8132_PINCTRL_PIN_TORCH;
	int state = OCP8132_PINCTRL_PINSTATE_LOW;

	ocp8132_pinctrl_set(pin_torch, state);
	ocp8132_pinctrl_set(pin_flash, state);

	return 0;
}

/* set flashlight level */
static int ocp8132_set_level(int level)
{
	g_flash_duty = level;
	return 0;
}

/* flashlight init */
static int ocp8132_init(void)
{
	int pin_flash = OCP8132_PINCTRL_PIN_FLASH;
	int pin_torch = OCP8132_PINCTRL_PIN_TORCH;
	int state = OCP8132_PINCTRL_PINSTATE_LOW;

	ocp8132_pinctrl_set(pin_torch, state);
	ocp8132_pinctrl_set(pin_flash, state);

	return 0;
}

/* flashlight uninit */
static int ocp8132_uninit(void)
{
	ocp8132_disable();
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer ocp8132_timer;
static unsigned int ocp8132_timeout_ms;

static void ocp8132_work_disable(struct work_struct *data)
{
	PK_DBG("work queue callback\n");
	ocp8132_disable();
}

static enum hrtimer_restart ocp8132_timer_func(struct hrtimer *timer)
{
	schedule_work(&ocp8132_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int ocp8132_ioctl(unsigned int cmd, unsigned long arg)
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
		ocp8132_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PK_DBG("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		ocp8132_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PK_DBG("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (ocp8132_timeout_ms) {
				ktime = ktime_set(ocp8132_timeout_ms / 1000,
					(ocp8132_timeout_ms % 1000) * 1000000);
				hrtimer_start(&ocp8132_timer, ktime,
					HRTIMER_MODE_REL);
			}
			ocp8132_enable();
		} else {
			ocp8132_disable();
			hrtimer_cancel(&ocp8132_timer);
		}
		break;
	case FLASH_IOC_GET_DUTY_NUMBER:   //FIXME
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = 2;
		break;
	default:
		PK_LOG("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ocp8132_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8132_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8132_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&ocp8132_mutex);
	if (set) {
		if (!use_count)
			ret = ocp8132_init();
		use_count++;
		PK_DBG("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = ocp8132_uninit();
		if (use_count < 0)
			use_count = 0;
		PK_DBG("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&ocp8132_mutex);

	return ret;
}

static ssize_t ocp8132_strobe_store(struct flashlight_arg arg)
{
	ocp8132_set_driver(1);
	ocp8132_set_level(arg.level);
	ocp8132_timeout_ms = 0;
	ocp8132_enable();
	msleep(arg.dur);
	ocp8132_disable();
	ocp8132_set_driver(0);

	return 0;
}


static ssize_t att_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	printk("echo ocp8132_FLASH debug buf,   %s ", buf);
	sprintf(node_one_buf, "%s", buf);

	if ((strcmp ("0", buf) == 0) || (strcmp ("0\x0a", buf) == 0)) {
		printk(" ocp8132_FLASH  0");
		ocp8132_disable();
		ocp8132_set_driver(0);
	} else{
		printk(" ocp8132_FLASH  1");
		ocp8132_set_driver(1);
		ocp8132_set_level(0);
		ocp8132_timeout_ms = 0;
		ocp8132_enable();
	}
	return count;
}

static ssize_t att_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s", node_one_buf);
}

static DEVICE_ATTR(ocp8132_FLASH, 0664, att_show, att_store);


static struct flashlight_operations ocp8132_ops = {
	ocp8132_open,
	ocp8132_release,
	ocp8132_ioctl,
	ocp8132_strobe_store,
	ocp8132_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int ocp8132_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * ocp8132_init();
	 */

	return 0;
}

static int ocp8132_parse_dt(struct device *dev,
		struct ocp8132_platform_data *pdata)
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
				OCP8132_NAME);
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

static int flash_is_use;
static void mtk_flashlight_brightness_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	flash_enable = 0;
	if (value == LED_OFF) {
		pr_info("disable flashlight");
		flash_is_use = 0;
		ocp8132_disable();
		ocp8132_set_driver(0);
		flash_enable = value;
	} else if ((value > 0) && (value <= 255)) {
		flash_is_use = 1;
		ocp8132_set_driver(1);
		ocp8132_set_level(0);
		ocp8132_timeout_ms = 0;
		ocp8132_enable();
		flash_enable = value;
} else {
	pr_err("invalid value %d or enabled %d", value, flash_enable);
	}


}

static enum led_brightness mtk_flashlight_brightness_get(
struct led_classdev *led_cdev)
{
	return flash_enable;
}

static struct led_classdev pmic_flashlight_led = {
	.name           = "flashlight",
	.brightness_set = mtk_flashlight_brightness_set,
	.brightness_get = mtk_flashlight_brightness_get,
	.brightness     = LED_OFF,
};

static struct led_classdev pmic_torch_led = {
	.name           = "torch-light0",
	.brightness_set = mtk_flashlight_brightness_set,
	.brightness_get = mtk_flashlight_brightness_get,
	.brightness     = LED_OFF,
};

int32_t mtk_flashlight_create_classdev(struct platform_device *pdev)
{
	int32_t rc = 0;

	rc = led_classdev_register(&pdev->dev, &pmic_flashlight_led);
	if (rc) {
		pr_err("Failed to register  led dev. rc = %d\n", rc);
		return rc;
	}
	return 0;
}

int32_t mtk_torch_create_classdev(struct platform_device *pdev)
{
	int32_t rc = 0;

	rc = led_classdev_register(&pdev->dev, &pmic_torch_led);
	if (rc) {
		pr_err("Failed to register  led dev. rc = %d\n", rc);
		return rc;
	}
	return 0;
}


static int ocp8132_probe(struct platform_device *pdev)
{
	struct ocp8132_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	PK_DBG("Probe start.\n");

	/* init pinctrl */
	if (ocp8132_pinctrl_init(pdev)) {
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
		err = ocp8132_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&ocp8132_work, ocp8132_work_disable);

	/* init timer */
	hrtimer_init(&ocp8132_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp8132_timer.function = ocp8132_timer_func;
	ocp8132_timeout_ms = 100;

	/* init chip hw */
	ocp8132_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&ocp8132_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(OCP8132_NAME, &ocp8132_ops)) {
			err = -EFAULT;
			goto err;
		}
	}

	// 增加节点/sys/devices/platform/att_ocp8132_FLASH
	sysfs_create_file(&pdev->dev.kobj, &dev_attr_ocp8132_FLASH.attr);
	mtk_flashlight_create_classdev(pdev);
	mtk_torch_create_classdev(pdev);
	PK_DBG("Probe done.\n");

	return 0;
err:
	return err;
}

static int ocp8132_remove(struct platform_device *pdev)
{
	struct ocp8132_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	PK_DBG("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(OCP8132_NAME);

	/* flush work queue */
	flush_work(&ocp8132_work);

	PK_DBG("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ocp8132_gpio_of_match[] = {
	{.compatible = OCP8132_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp8132_gpio_of_match);
#else
static struct platform_device ocp8132_gpio_platform_device[] = {
	{
		.name = OCP8132_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ocp8132_gpio_platform_device);
#endif

static struct platform_driver ocp8132_platform_driver = {
	.probe = ocp8132_probe,
	.remove = ocp8132_remove,
	.driver = {
		.name = OCP8132_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ocp8132_gpio_of_match,
#endif
	},
};

static int __init flashlight_ocp8132_init(void)
{
	int ret;

	PK_DBG("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&ocp8132_gpio_platform_device);
	if (ret) {
		PK_ERR("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&ocp8132_platform_driver);
	if (ret) {
		PK_ERR("Failed to register platform driver\n");
		return ret;
	}

	PK_DBG("Init done.\n");

	return 0;
}

static void __exit flashlight_ocp8132_exit(void)
{
	PK_DBG("Exit start.\n");

	platform_driver_unregister(&ocp8132_platform_driver);

	PK_DBG("Exit done.\n");
}

module_init(flashlight_ocp8132_init);
module_exit(flashlight_ocp8132_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight OCP8132 GPIO Driver");

