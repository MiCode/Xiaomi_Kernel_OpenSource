/*
* Copyright (C) 2021 Awinic Inc.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <media/soc_camera.h>
#include <linux/gpio_keys.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include "flashlight.h"
#include "flashlight-dt.h"
#include "flashlight-core.h"

#define  AW3641E_SYSTEM_NODE
#ifdef AW3641E_SYSTEM_NODE
#include <linux/leds.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#endif

#define AW3641E_DRIVER_VERSION "V1.1.0"

#define CONFIG_OF				1
#define AW3641E_NSEC_TIME		1000
#define AW3641E_NAME			"flashlights_aw3641e"	//"aw3641e"

#define AW3641E_CHANNEL_NUM		1
#define AW3641E_CHANNEL_CH1		0
#define AW3641E_LEVEL_FLASH		8
#define AW3641E_LEVEL_TORCH		0

static struct hrtimer timer_close;
static struct work_struct aw3641e_work;
static unsigned int flash_en_gpio;
static unsigned int flash_sel_gpio;
static int flash_level = -1;
static spinlock_t aw3641e_lock;
//#define AW3641E_PINCTRL
#ifdef AW3641E_PINCTRL
static struct pinctrl *aw3641e_pinctrl;
static struct pinctrl_state *aw3641e_hwen_high;
static struct pinctrl_state *aw3641e_hwen_low;
#endif
//#define AW3641E_FLASH_TIMER
#define AW3641E_C3T
#ifdef AW3641E_FLASH_TIMER
static struct hrtimer timer_flash;
#endif

static unsigned int aw3641e_timeout_ms[AW3641E_CHANNEL_NUM];
static const unsigned char aw3641e_torch_level[ ] = {0};

static const unsigned char aw3641e_flash_level[AW3641E_LEVEL_FLASH + 1] = {
					0, 16, 15, 14, 13, 12, 11, 10, 9};
static void aw3641e_torch_on(void);
static void aw3641e_flash_on(void);
static void aw3641e_flash_off(void);

static int aw3641e_is_torch(int level)
{
	pr_info("%s.\n", __func__);

	if (level > AW3641E_LEVEL_TORCH)
		return -1;

	return 0;
}

static int aw3641e_verify_level(int level)
{
	pr_info("%s.\n", __func__);

	if (level <= 0)
		level = 0;
	else if (level >= AW3641E_LEVEL_FLASH)
		level = AW3641E_LEVEL_FLASH;

	return level;
}

void aw3641e_torch_on(void)
{
	pr_info("%s.\n", __func__);

	gpio_set_value(flash_sel_gpio, 0);
	gpio_set_value(flash_en_gpio, 1);
}

void aw3641e_flash_off(void)
{
	pr_info("%s.\n", __func__);

	gpio_set_value(flash_sel_gpio, 0);
	gpio_set_value(flash_en_gpio, 0);
	udelay(500);
}
#ifdef AW3641E_FLASH_TIMER
static enum hrtimer_restart aw3641e_timer_flash(struct hrtimer *timer)
{
	pinctrl_select_state(aw3641e_pinctrl, aw3641e_hwen_low);
	pinctrl_select_state(aw3641e_pinctrl, aw3641e_hwen_high);

	return (--flash_level) ? HRTIMER_RESTART : HRTIMER_NORESTART;
}
#endif
void aw3641e_flash_on(void)
{
#ifdef AW3641E_FLASH_TIMER
	ktime_t kt = 500;
	hrtimer_start(&timer_flash, kt, HRTIMER_MODE_REL);
#else
	int i = 0;

	pr_info("%s.flash_level = %d.\n", __func__, flash_level);

	gpio_set_value(flash_sel_gpio, 1);

	#ifdef AW3641E_C3T
	gpio_set_value(flash_en_gpio, 1);
	return;
	#endif
	#ifdef AW3641E_PINCTRL
	for (i = 0; i < flash_level - 1; i++) {
		pinctrl_select_state(aw3641e_pinctrl, aw3641e_hwen_high);
		udelay(2);
		pinctrl_select_state(aw3641e_pinctrl, aw3641e_hwen_low);
		udelay(2);
	}
	pinctrl_select_state(aw3641e_pinctrl, aw3641e_hwen_high);
	#else //AW3641E_PINCTRL
	for (i = 0; i < flash_level - 1; i++) {
		gpio_set_value(flash_en_gpio, 1);
		udelay(2);
		gpio_set_value(flash_en_gpio, 0);
		udelay(2);
	}
	gpio_set_value(flash_en_gpio, 1);
	#endif //AW3641E_PINCTRL
#endif
}
static void aw3641e_work_disable(struct work_struct *data)
{
	pr_info("%s.\n", __func__);
	aw3641e_flash_off();
}

static enum hrtimer_restart aw3641e_timer_close(struct hrtimer *timer)
{
	schedule_work(&aw3641e_work);
	return HRTIMER_NORESTART;
}

int aw3641e_timer_start(int channel, ktime_t ktime)
{
	pr_info("%s.\n", __func__);

	if (channel == AW3641E_CHANNEL_CH1)
		hrtimer_start(&timer_close, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("%s, Error channel\n", __func__);
		return -1;
	}

	return 0;
}

int aw3641e_timer_cancel(int channel)
{
	pr_info("%s.\n", __func__);

	if (channel == AW3641E_CHANNEL_CH1) {
		hrtimer_cancel(&timer_close);
	    #ifdef AW3641E_FLASH_TIMER
		hrtimer_cancel(&timer_flash);
		#endif
	}

	else {
		pr_err("Error channel\n");
		return -EINVAL;
	}

	return 0;
}

void aw3641e_enable(int channel)
{
	pr_info("%s,flash_level = %d.\n", __func__, flash_level);

	if (!aw3641e_is_torch(flash_level)) {
		/* torch mode */
		aw3641e_torch_on();
	} else {
		/* flash mode */
		aw3641e_flash_on();
	}

}
void aw3641e_disable(int channel)
{
	pr_info("%s.\n", __func__);

	if (channel == AW3641E_CHANNEL_CH1) {
		aw3641e_flash_off();
	} else {
		pr_err("%s, Error channel\n", __func__);
		return;
	}
}
static int aw3641e_set_level(int channel, int level)
{
	pr_info("%s.\n", __func__);

	if (channel == AW3641E_CHANNEL_CH1) {
		flash_level =  aw3641e_flash_level[aw3641e_verify_level(level)];
	} else {
		pr_err("%s, Error channel\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/****************************************************************
*		flashlights platform interface			*
*****************************************************************/
static int aw3641e_open(void)
{
	return 0;
}

static int aw3641e_release(void)
{
	hrtimer_cancel(&timer_close);
	#ifdef AW3641E_FLASH_TIMER
	hrtimer_cancel(&timer_flash);
	#endif
	return 0;
}

static int aw3641e_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_info("%s.\n", __func__);
	/* modify flash level of fl_arg->arg acrodding your level for test */
	/*
	if (fl_arg->arg == 1)
		fl_arg->arg = 10;
	*/
	/* verify channel */
	if (channel < 0 || channel >= AW3641E_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("%s, FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		aw3641e_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("%s,FLASH_IOC_SET_DUTY(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		aw3641e_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("%s,FLASH_IOC_SET_ONOFF(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		if (fl_arg->arg >= 1) {
			if (aw3641e_timeout_ms[channel]) {
				ktime =
				ktime_set(aw3641e_timeout_ms[channel] / 1000,
				(aw3641e_timeout_ms[channel] % 1000) * 1000000);
				aw3641e_timer_start(channel, ktime);
			}
		spin_lock(&aw3641e_lock);
		aw3641e_enable(channel);
		spin_unlock(&aw3641e_lock);
		} else {
			aw3641e_disable(channel);
			aw3641e_timer_cancel(channel);
		}
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

int aw3641e_set_driver(int set)
{
	/* init chip and set usage count */
	return 0;
}

static ssize_t aw3641e_strobe_store(struct flashlight_arg arg)
{
	aw3641e_set_level(arg.ct, arg.level);
	return 0;
}

static struct flashlight_operations aw3641e_ops = {
	aw3641e_open,
	aw3641e_release,
	aw3641e_ioctl,
	aw3641e_strobe_store,
	aw3641e_set_driver
};

#ifdef  AW3641E_SYSTEM_NODE
static ssize_t aw3641e_torch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf =  0;
	sscanf(buf, "%d", &databuf);
	pr_info("%s : debug:%s(%d)", __func__, buf, databuf);
	if ( databuf == 0 ) { // torch disable
		aw3641e_disable(AW3641E_CHANNEL_CH1);
		aw3641e_timer_cancel(AW3641E_CHANNEL_CH1);
	} else if ( databuf == 1) { //torch enable
        pr_info("%s : enter torch", __func__);
		aw3641e_set_level(0, 0);
		spin_lock(&aw3641e_lock);
		aw3641e_enable(AW3641E_CHANNEL_CH1);
		spin_unlock(&aw3641e_lock);
	} else {
        pr_info("%s : enter flash", __func__);
		aw3641e_set_level(0, 1);
		spin_lock(&aw3641e_lock);
		aw3641e_enable(AW3641E_CHANNEL_CH1);
		spin_unlock(&aw3641e_lock);
    }

	return count;
}
static ssize_t aw3641e_torch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "%s : flash_level : %d  \n",__func__, flash_level);
	return len;
}
static ssize_t aw3641e_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf =  0;
	sscanf(buf, "%d", &databuf);
	pr_info("%s : debug:%s(%d)", __func__, buf, databuf);
	if ( databuf == 0 ) { // torch disable
		aw3641e_disable(AW3641E_CHANNEL_CH1);
		aw3641e_timer_cancel(AW3641E_CHANNEL_CH1);
	} else { //torch enable
		aw3641e_set_level(0, 0);
		spin_lock(&aw3641e_lock);
		aw3641e_enable(AW3641E_CHANNEL_CH1);
		spin_unlock(&aw3641e_lock);
	}

	return count;
}
static ssize_t aw3641e_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "%s : flash_level : %d  \n",__func__, flash_level);
	return len;
}
static DEVICE_ATTR(aw3641e_FLASH, 0664, aw3641e_torch_show, aw3641e_torch_store);
static DEVICE_ATTR(aw3641e_FLASH_PWN, 0664, aw3641e_flash_show, aw3641e_flash_store);
static struct attribute *flashlight_aw3641e_attrs[ ] = {
	&dev_attr_aw3641e_FLASH.attr,
	&dev_attr_aw3641e_FLASH_PWN.attr,
	NULL,
};
static int aw3641e_sysfs_create_files(struct platform_device *pdev)
{
	int err = 0, i = 0;
	for ( i = 0; flashlight_aw3641e_attrs[i] != NULL; i++) {
		err = sysfs_create_file(&pdev->dev.kobj, flashlight_aw3641e_attrs[i]);
		if ( err < 0) {
			pr_err("%s: create file node fail err = %d\n", __func__, err);
			return err;
		}
	}
	return 0;
}

static void mtk_flashlight_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	if ( value == LED_OFF ) { // torch disable
		aw3641e_disable(AW3641E_CHANNEL_CH1);
		aw3641e_timer_cancel(AW3641E_CHANNEL_CH1);
	}  else if (( value > 0 ) && ( value <= 255 )) { //torch enable
		aw3641e_set_level(0, 0);
		spin_lock(&aw3641e_lock);
		aw3641e_enable(AW3641E_CHANNEL_CH1);
		spin_unlock(&aw3641e_lock);
	}else {
		pr_err("invalid value %d ", value);
	}
}

static enum led_brightness mtk_flashlight_brightness_get(
struct led_classdev *led_cdev)
{
	return 0;
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
#endif //miaozhongshu
#ifdef AW3641E_PINCTRL
static int aw3641e_pinctrl_init(struct platform_device *pdev)
{
		int ret = 0;

	/* get pinctrl */
	aw3641e_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw3641e_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw3641e_pinctrl);
	}
	aw3641e_hwen_high = pinctrl_lookup_state(aw3641e_pinctrl, "hwen_high");
	aw3641e_hwen_low  = pinctrl_lookup_state(aw3641e_pinctrl, "hwen_low");
	return ret;
}
#endif
/********************************************************************
*			driver probe
*********************************************************************/
static int aw3641e_probe(struct platform_device *dev)
{
	struct device_node *node = dev->dev.of_node;

	pr_info("%s Probe start.\n", __func__);

	flash_en_gpio = of_get_named_gpio(node, "flash-en-gpio", 0);
	if ((!gpio_is_valid(flash_en_gpio))) {
		pr_err("%s: dts don't provide flash_en_gpio\n", __func__);
		return -EINVAL;
	}
	pr_info("%s prase dts with flash_en_gpio success.\n", __func__);

	flash_sel_gpio = of_get_named_gpio(node, "flash-sel-gpio", 0);
	if ((!gpio_is_valid(flash_sel_gpio))) {
		pr_err("%s: dts don't provide flash_sel_gpio\n", __func__);
		return -EINVAL;
	}
	pr_info("%s prase dts with flash_sel_gpio success.\n", __func__);

	if (devm_gpio_request_one(&dev->dev, flash_en_gpio,
				  GPIOF_DIR_OUT | GPIOF_INIT_LOW,
		      "Flash-En")) {
		pr_err("%s, gpio Flash-En failed\n", __func__);
		return -1;
	}
	if (devm_gpio_request_one(&dev->dev, flash_sel_gpio,
				  GPIOF_DIR_OUT | GPIOF_INIT_LOW,
		      "Flash-SEL")) {
		pr_err("%s, gpio Flash-SEL failed\n", __func__);
		goto err_gpio;
	}
	/* register flashlight operations */
	if (flashlight_dev_register(AW3641E_NAME, &aw3641e_ops)) {
		pr_err("Failed to register flashlight device.\n");
		goto err_free;
	}

	/* init work queue */
	INIT_WORK(&aw3641e_work, aw3641e_work_disable);

	spin_lock_init(&aw3641e_lock);

	/* init timer close */
	hrtimer_init(&timer_close, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer_close.function = aw3641e_timer_close;
	aw3641e_timeout_ms[AW3641E_CHANNEL_CH1] = 100;
	#ifdef  AW3641E_SYSTEM_NODE
	aw3641e_sysfs_create_files(dev);
	mtk_flashlight_create_classdev(dev);
	mtk_torch_create_classdev(dev);
	#endif
	#ifdef AW3641E_PINCTRL
	aw3641e_pinctrl_init(dev);
	#endif
	#ifdef AW3641E_FLASH_TIMER
	hrtimer_init(&timer_flash, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer_flash.function = aw3641e_timer_flash;
	#endif
	pr_info("%s Probe finish.\n", __func__);

	return 0;
err_gpio:
	devm_gpio_free(&dev->dev, flash_en_gpio);
err_free:
	devm_gpio_free(&dev->dev, flash_sel_gpio);

	return -EINVAL;
}
static int aw3641e_remove(struct platform_device *dev)
{
	flashlight_dev_unregister(AW3641E_NAME);
	devm_gpio_free(&dev->dev, flash_en_gpio);
	devm_gpio_free(&dev->dev, flash_sel_gpio);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw3641e_of_match[] = {
	{.compatible = "awinic,aw3641e"},
	{},
};
MODULE_DEVICE_TABLE(of, aw3641e_of_match);
#else
static struct platform_device aw3641e_platform_device[] = {
	{
		.name = AW3641E_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, aw3641e_platform_device);
#endif

static struct platform_driver aw3641e_platform_driver = {
	.probe = aw3641e_probe,
	.remove = aw3641e_remove,
	.driver = {
		.name = AW3641E_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw3641e_of_match,
#endif
	},
};

static int __init aw3641e_flash_init(void)
{
	int ret = 0;

	pr_info("%s driver version %s.\n", __func__, AW3641E_DRIVER_VERSION);

#ifndef CONFIG_OF
	ret = platform_device_register(&aw3641e_platform_device);
	if (ret) {
		pr_err("%s,Failed to register platform device\n", __func__);
		return ret;
	}
#endif
	ret = platform_driver_register(&aw3641e_platform_driver);
	if (ret) {
		pr_err("%s,Failed to register platform driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit aw3641e_flash_exit(void)
{
	pr_info("%s.\n", __func__);

	platform_driver_unregister(&aw3641e_platform_driver);
}

module_init(aw3641e_flash_init);
module_exit(aw3641e_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shiqiang@awinic.com");
MODULE_DESCRIPTION("GPIO Flash driver");

