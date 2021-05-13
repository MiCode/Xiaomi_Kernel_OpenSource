
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/sched.h>
#include <linux/sched/clock.h>


#include "leds-mtk-i2c.h"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

#ifdef MET_USER_EVENT_SUPPORT
#include <mt-plat/met_drv.h>
#endif

#define CONFIG_LEDS_BRIGHTNESS_CHANGED
/****************************************************************************
 * variables
 ***************************************************************************/

struct led_debug_info {
	unsigned long long current_t;
	unsigned long long last_t;
	char buffer[4096];
	int count;
};

struct led_desp {
	int index;
	char name[16];
};

struct leds_desp_info {
	int lens;
	struct led_desp *leds[0];
};

struct mtk_led_data {
	struct led_conf_info conf;
	int last_brightness;
	int last_level;
	int hw_level;
	struct led_desp desp;
	struct led_debug_info debug;
};

struct mtk_leds_info {
	int			nums;
	struct mtk_led_data leds[0];
};

struct leds_desp_info *leds_desp;

static DEFINE_MUTEX(leds_mutex);
static BLOCKING_NOTIFIER_HEAD(mtk_leds_chain_head);

static int led_level_i2c_set(struct mtk_led_data *s_led, int brightness);

int mtk_leds_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mtk_leds_chain_head, nb);
}
EXPORT_SYMBOL_GPL(mtk_leds_register_notifier);

int mtk_leds_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mtk_leds_chain_head, nb);
}
EXPORT_SYMBOL_GPL(mtk_leds_unregister_notifier);

int mtk_leds_call_notifier(unsigned long action, void *data)
{
	return blocking_notifier_call_chain(&mtk_leds_chain_head, action, data);
}
EXPORT_SYMBOL_GPL(mtk_leds_call_notifier);

static int __maybe_unused call_notifier(int event, struct led_conf_info *led_info)
{
	int err;

	err = mtk_leds_call_notifier(event, led_info);
	if (err)
		dev_info(led_info->cdev.dev, "[ERROR] notifier_call_chain error\n");
	return err;
}

/****************************************************************************
 * Internal function
 ***************************************************************************/

static struct mtk_led_data *getLedData(char *name)
{
	int i = 0;
	struct mtk_led_data *led_dat;

	while (i < leds_desp->lens) {
		if (!strcmp(name, leds_desp->leds[i]->name))
			break;
		i++;
	}
	if (i == leds_desp->lens) {
		pr_notice("can not find leds by led_desp %s", name);
		return NULL;
	}

	led_dat = container_of(leds_desp->leds[i],
		struct mtk_led_data, desp);

	return led_dat;
}

static int brightness_maptolevel(struct led_conf_info *led_dat, int brightness)
{
	return ((((1 << led_dat->trans_bits) - 1) * brightness
				+ ((led_dat->cdev.max_brightness) / 2))
				/ (led_dat->cdev.max_brightness));
}

/****************************************************************************
 * add API for temperature control
 ***************************************************************************/

int setMaxBrightness(char *name, int percent, bool enable)
{
	struct mtk_led_data *led_dat;
	int max_l = 0, limit_l = 0, cur_l = 0;

	led_dat = getLedData(name);
	if (!led_dat)
		return -EINVAL;

	max_l = (1 << led_dat->conf.trans_bits) - 1;
	limit_l = (percent * max_l) / 100;

	dev_info(led_dat->conf.cdev.dev,
			"Set Led: %s, percent : %d, limit_l : %d, enable: %d",
			led_dat->conf.cdev.name, percent, limit_l, enable);

	if (enable) {
		led_dat->conf.max_level = limit_l;
		cur_l = min(led_dat->last_level, limit_l);
	} else {
		cur_l = led_dat->last_level;
		led_dat->conf.max_level = max_l;
	}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	call_notifier(3, &led_dat->conf);
#endif
	if (cur_l != 0)
		led_level_i2c_set(led_dat, cur_l);

	dev_info(led_dat->conf.cdev.dev,
			"after: name: %s, cur_l : %d\n",
			led_dat->conf.cdev.name, cur_l);
	return 0;

}
EXPORT_SYMBOL(setMaxBrightness);

int mt_leds_brightness_set(char *name, int level)
{
	struct mtk_led_data *led_dat;

	led_dat = getLedData(name);
	if (!led_dat)
		return -EINVAL;

	led_level_i2c_set(led_dat, level);
	led_dat->last_level = level;

	return 0;
}
EXPORT_SYMBOL(mt_leds_brightness_set);

static void led_debug_log(struct mtk_led_data *s_led, int level)
{
	unsigned long cur_time_mod = 0;
	unsigned long long cur_time_display = 0;
	int ret = 0;
	int level_h, level_l;

	s_led->debug.current_t = sched_clock();
	cur_time_display = s_led->debug.current_t;
	do_div(cur_time_display, 1000000);
	cur_time_mod = do_div(cur_time_display, 1000);
	level_h = (level & 0x7F8) >> 3;
	level_l = level & 0x7;

	ret = snprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		4095 - strlen(s_led->debug.buffer),
		"T:%lld.%ld,  B:%d L:%d,  L:%0x H:%0x    ",
		cur_time_display, cur_time_mod,
		s_led->last_brightness, level, level_l, level_h);

	s_led->debug.count++;

	if (ret < 0 || ret >= 4096) {
		dev_info(s_led->conf.cdev.dev, "[ERROR] print log error!");
		s_led->debug.count = 5;
	}

	if (level == 0 || s_led->debug.count >= 5 ||
		(s_led->debug.current_t - s_led->debug.last_t) > 1000000000) {
		dev_info(s_led->conf.cdev.dev, "%s", s_led->debug.buffer);
		s_led->debug.count = 0;
		s_led->debug.buffer[strlen("[Light] Set directly ") +
			strlen(s_led->conf.cdev.name)] = '\0';
	}

	s_led->debug.last_t = sched_clock();
}


/****************************************************************************
 * driver functions
 ***************************************************************************/
static int led_level_i2c_set(struct mtk_led_data *s_led, int level)
{
	int level_l, level_h;

	level = min(level, s_led->conf.max_level);
	if (s_led->hw_level == level)
		return 0;

#ifdef MET_USER_EVENT_SUPPORT
	if (enable_met_backlight_tag())
		output_met_backlight_tag(level);
#endif

	level_h = (level >> 3) & 0xFF;
	level_l = level & 0x7;

	led_debug_log(s_led, level);
#ifdef CONFIG_MTK_GATE_IC
	_gate_ic_backlight_set(level);
#endif

	s_led->hw_level = level;

	return 0;
}

static int led_level_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	int trans_level = 0;

	struct led_conf_info *led_conf =
		container_of(led_cdev, struct led_conf_info, cdev);
	struct mtk_led_data *led_data =
		container_of(led_conf, struct mtk_led_data, conf);

	if (led_data->last_brightness == brightness)
		return 0;

	trans_level = brightness_maptolevel(led_conf, brightness);

#ifdef MET_USER_EVENT_SUPPORT
	if (enable_met_backlight_tag())
		output_met_backlight_tag(brightness);
#endif
	pr_debug("set brightness: %d, %d", brightness, trans_level);

	led_data->last_brightness = brightness;

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	call_notifier(1, led_conf);
#endif

#ifdef CONFIG_MTK_AAL_SUPPORT
	disp_pq_notify_backlight_changed(trans_level);
#else
	led_level_i2c_set(led_data, trans_level);
	led_data->last_level = trans_level;
#endif

	return 0;
}

static int led_data_init(struct device *dev, struct mtk_led_data *s_led,
				enum led_brightness brightness)
{
	int ret;
	int level;

	s_led->conf.cdev.flags = LED_CORE_SUSPENDRESUME;
	s_led->conf.cdev.brightness_set_blocking = led_level_set;
	level = brightness_maptolevel(&s_led->conf, brightness);

	ret = devm_led_classdev_register(dev, &(s_led->conf.cdev));
	if (ret < 0) {
		dev_info(dev, "[ERROR] led class register fail!\n");
		return ret;
	}

	dev_info(dev, "%s devm_led_classdev_register ok, set brightness: %d!\n",
			s_led->conf.cdev.name, level);

	ret = snprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		4095 - strlen(s_led->debug.buffer),
		"[Light] Set %s directly ", s_led->conf.cdev.name);

	if (ret < 0 || ret >= 4096)
		dev_info(dev, "[ERROR] print log init error!");

	s_led->last_brightness = brightness;
	s_led->last_level = level;
	led_level_i2c_set(s_led, level);

	return 0;
}

static int mtk_leds_parse_dt(struct device *dev, struct mtk_leds_info *m_leds)
{
	struct device_node *child;
	struct mtk_led_data *s_led;
	int ret = 0;
	const char *state;
	int num = 0;
	enum led_brightness brightness;

	if (!dev->of_node) {
		pr_notice("Error load dts: node not exist!\n");
		return ret;
	}

	for_each_available_child_of_node(dev->of_node, child) {

		s_led = &(m_leds->leds[num]);

		ret = of_property_read_string(child, "label", &s_led->conf.cdev.name);
		if (ret) {
			dev_info(dev, "[ERROR] Fail to read label property\n");
			goto out_led_dt;
		}

		ret = of_property_read_u32(child, "led-bits", &(s_led->conf.led_bits));
		if (ret) {
			dev_info(dev, "No led-bits, use default value 8\n");
			s_led->conf.led_bits = 8;
		}

		ret = of_property_read_u32(child, "max-brightness",
			&(s_led->conf.cdev.max_brightness));
		if (ret) {
			dev_info(dev, "No led-bits, use default value 8\n");
			s_led->conf.cdev.max_brightness = (1 << s_led->conf.led_bits) - 1;
		}

		ret = of_property_read_u32(child, "trans-bits", &(s_led->conf.trans_bits));
		if (ret) {
			dev_info(dev, "No trans-bits, use default value 10\n");
			s_led->conf.trans_bits = 10;
		}

		s_led->conf.max_level = (1 << s_led->conf.trans_bits) - 1;

		ret = of_property_read_string(child, "default-state", &state);
		if (!ret) {
			if (!strcmp(state, "half"))
				brightness = LED_HALF;
			else if (!strcmp(state, "on"))
				brightness = LED_FULL;
			else
				brightness = LED_OFF;
		} else {
			brightness = LED_FULL;
		}
		dev_info(dev, "parse %d leds dt: %s, %d, %d, %d\n",
						 num, s_led->conf.cdev.name,
						 s_led->conf.led_bits,
						 s_led->conf.trans_bits,
						 s_led->conf.max_level);

		s_led->desp.index = num;
		strncpy(s_led->desp.name, s_led->conf.cdev.name,
			strlen(s_led->conf.cdev.name));
		s_led->desp.index = num;
		leds_desp->leds[num] = &s_led->desp;

		s_led->last_brightness = 0;

		ret = led_data_init(dev, s_led, brightness);
		if (ret)
			goto out_led_dt;
		num++;
	}

	m_leds->nums = num;
	dev_info(dev, "%s: load dts ok, num = %d!\n", __func__, num);

	return 0;

out_led_dt:
	dev_info(dev, "[ERROR] Error load dts node!\n");
	return ret;
}


/****************************************************************************
 * driver functions
 ***************************************************************************/

static int leds_i2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_leds_info *leds;
	int ret, nums;

	dev_info(dev, "begin probe i2c leds\n");

	nums = of_get_child_count(dev->of_node);

	leds = devm_kzalloc(dev, (sizeof(struct mtk_leds_info) +
				(sizeof(struct mtk_led_data) * (nums))),
				GFP_KERNEL);
	if (!leds)
		return -ENOMEM;

	leds_desp = devm_kzalloc(dev, (sizeof(struct leds_desp_info) +
		sizeof(struct led_desp *) * (nums)),
		GFP_KERNEL);
	leds_desp->lens = nums;
	if (!leds_desp)
		return -ENOMEM;

	ret = mtk_leds_parse_dt(&(pdev->dev), leds);
	if (ret) {
		dev_info(dev, "[ERROR] Failed to parse devicetree!\n");
		return ret;
	}

	dev_info(dev, "probe i2c leds end\n");

	return ret;
}

static int leds_i2c_remove(struct platform_device *pdev)
{
	int i;
	struct mtk_leds_info *m_leds = dev_get_platdata(&pdev->dev);

	for (i = 0; i < m_leds->nums; i++)
		led_classdev_unregister(&m_leds->leds[i].conf.cdev);

	kfree(m_leds);
	m_leds = NULL;

	return 0;
}

static void leds_i2c_shutdown(struct platform_device *pdev)
{
	int i;
	struct mtk_leds_info *m_leds = dev_get_platdata(&pdev->dev);

	pr_info("Turn off backlight\n");

	for (i = 0; m_leds && i < m_leds->nums; i++) {
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
		call_notifier(2, &(m_leds->leds[i].conf));
#endif
		led_level_i2c_set(&m_leds->leds[i], 0);
	}
}

static const struct of_device_id of_leds_i2c_match[] = {
	{ .compatible = "mediatek,i2c-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_leds_i2c_match);

static struct platform_driver i2c_leds_driver = {
	.driver = {
		   .name = "mtk-i2c-leds",
		   .of_match_table = of_leds_i2c_match,
		   },
	.probe = leds_i2c_probe,
	.remove = leds_i2c_remove,
	.shutdown = leds_i2c_shutdown,
};

static int __init mtk_leds_init(void)
{
	int ret;

	pr_info("Leds init\n");
	ret = platform_driver_register(&i2c_leds_driver);

	if (ret) {
		pr_info("driver register error: %d\n", ret);
		return ret;
	}

	return ret;
}

static void __exit mtk_leds_exit(void)
{
	platform_driver_unregister(&i2c_leds_driver);
}

late_initcall(mtk_leds_init);
module_exit(mtk_leds_exit);


MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Display Backlight Driver");
MODULE_LICENSE("GPL");

