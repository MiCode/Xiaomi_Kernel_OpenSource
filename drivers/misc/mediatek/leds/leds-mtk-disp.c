// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include "leds-mtk-disp.h"


#ifdef CONFIG_DRM_MEDIATEK
extern int mtkfb_set_backlight_level(unsigned int level);
#endif

#ifdef MET_USER_EVENT_SUPPORT
#include <mt-plat/met_drv.h>
#endif

#define CONFIG_LEDS_BRIGHTNESS_CHANGED
/****************************************************************************
 * variables
 ***************************************************************************/
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

struct mtk_leds_info;

static int led_level_set(struct led_classdev *led_cdev,
					 enum led_brightness brightness);

struct led_debug_info {
	unsigned long long current_t;
	unsigned long long last_t;
	char buffer[4096];
	int count;
};

struct led_limit_info {

	unsigned int limit_l;
	u8 flag;
	unsigned int set_l;
	struct mutex lock;
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
	struct led_desp desp;
	struct led_classdev	cdev;
	int level;
	int led_bits;
	int trans_bits;
	int max_brightness;
	struct mtk_leds_info	*parent;
	struct led_debug_info debug;
	struct led_limit_info limit;
	struct work_struct work;
};

struct mtk_leds_info {
	struct device *dev;
	int			nums;
	struct mtk_led_data leds[0];
};

static DEFINE_MUTEX(leds_mutex);
struct leds_desp_info *leds_info;
static BLOCKING_NOTIFIER_HEAD(mtk_leds_chain_head);

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


static int call_notifier(int event, struct mtk_led_data *led_dat)
{
	int err;

	err = mtk_leds_call_notifier(event, &led_dat->cdev);
	if (err)
		pr_info("notifier_call_chain error\n");
	return err;
}

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/

static void led_debug_log(struct mtk_led_data *s_led,
		int level, int mappingLevel)
{
	unsigned long cur_time_mod = 0;
	unsigned long long cur_time_display = 0;

	s_led->debug.current_t = sched_clock();
	cur_time_display = s_led->debug.current_t;
	cur_time_mod = do_div(cur_time_display, 1000000000);

	sprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		"T:%lld.%ld,L:%d map:%d    ",
		cur_time_display, cur_time_mod/1000000,
		level, mappingLevel);

	s_led->debug.count++;

	if (level == 0 || s_led->debug.count >= 5 ||
		(s_led->debug.current_t - s_led->debug.last_t) > 1000000000) {
		pr_info("%s", s_led->debug.buffer);
		s_led->debug.count = 0;
		s_led->debug.buffer[strlen("[Light] Set directly ") +
			strlen(s_led->cdev.name)] = '\0';
	}

	s_led->debug.last_t = sched_clock();
}


static int getLedDespIndex(char *name)
{
	int i = 0;

	while (i < leds_info->lens) {
		if (!strcmp(name, leds_info->leds[i]->name))
			return i;
		i++;
	}
	return -1;
}


/****************************************************************************
 * add API for temperature control
 ***************************************************************************/

int setMaxBrightness(char *name, int percent, bool enable)
{
	struct mtk_led_data *led_dat;
	int limit_l, max_l, cur_l, index;

	index = getLedDespIndex(name);
	if (index < 0) {
		pr_notice("can not find leds by led_desp %s", name);
		return -1;
	}
	led_dat = container_of(leds_info->leds[index],
		struct mtk_led_data, desp);
//	pr_info("getLedData: %s", led_dat->desp.name);

	max_l = led_dat->max_brightness;
	limit_l = (percent * max_l) / 100;
	pr_info("before: name: %s, percent : %d, limit_l : %d, enable: %d",
		leds_info->leds[index]->name, percent, limit_l, enable);
	mutex_lock(&(led_dat->limit.lock));
	if (enable) {
		led_dat->limit.flag = 1;
		led_dat->limit.limit_l = limit_l;
		if (led_dat->limit.limit_l < led_dat->level)
			cur_l = led_dat->limit.limit_l;
		else
			cur_l = led_dat->level;
	} else {
		led_dat->limit.flag = 0;
		led_dat->limit.limit_l = max_l;
		cur_l = led_dat->level;
	}
	pr_info("after: flag: %d, cur_l: %d, limit_l: %d, level: %d",
		enable, cur_l, led_dat->limit.limit_l, led_dat->level);
	mutex_unlock(&(led_dat->limit.lock));
	if (led_dat->limit.set_l == 0)
		return 0;
	return led_level_set(&led_dat->cdev, cur_l);

}
EXPORT_SYMBOL(setMaxBrightness);


/****************************************************************************
 * driver functions
 ***************************************************************************/
static int led_level_set_disp(struct mtk_led_data *s_led,
	enum led_brightness brightness)
{
	int mappingLevel;

	if (s_led->level == brightness)
		return 0;
	mappingLevel = (
		(((1 << s_led->trans_bits) - 1) * brightness
		+ (((1 << s_led->led_bits) - 1) / 2))
		/ ((1 << s_led->led_bits) - 1));

	s_led->level = brightness;
	led_debug_log(s_led, brightness, mappingLevel);

#ifdef MET_USER_EVENT_SUPPORT
	if (enable_met_backlight_tag())
		output_met_backlight_tag(level);
#endif
#ifdef CONFIG_DRM_MEDIATEK
	return mtkfb_set_backlight_level(brightness);
#endif
	return 0;

}

void mtk_led_work(struct work_struct *work)
{
	struct mtk_led_data *led_data =
	    container_of(work, struct mtk_led_data, work);

	mutex_lock(&leds_mutex);
	led_level_set_disp(led_data, led_data->level);
	mutex_unlock(&leds_mutex);
}

int mt_leds_brightness_set(char *name, int level)
{
	struct mtk_led_data *led_dat;
	int index, led_Level;

	index = getLedDespIndex(name);
	if (index < 0) {
		pr_notice("can not find leds by led_desp %s", name);
		return -1;
	}
	led_dat = container_of(leds_info->leds[index],
		struct mtk_led_data, desp);
	led_Level = (
		(((1 << led_dat->led_bits) - 1) * level
		+ (((1 << led_dat->trans_bits) - 1) / 2))
		/ ((1 << led_dat->trans_bits) - 1));

//	pr_info("getLedData: %s, level: %d[%d]",
//		led_dat->desp.name, level, led_Level);

	schedule_work(&led_dat->work);
	return led_level_set_disp(led_dat, led_Level);
}
EXPORT_SYMBOL(mt_leds_brightness_set);

static int led_level_set(struct led_classdev *led_cdev,
					  enum led_brightness brightness)
{
	int trans_level;

	struct mtk_led_data *led_dat =
		container_of(led_cdev, struct mtk_led_data, cdev);

	if (led_dat->level == brightness)
		return 0;
	trans_level = (
		((1 << led_dat->trans_bits) - 1) * brightness
		+ (((1 << led_dat->led_bits) - 1) / 2))
		/ ((1 << led_dat->led_bits) - 1);
	pr_debug("set level: %d->%d,%d[%d]",
		led_dat->level, brightness, led_cdev->brightness, trans_level);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
#ifdef CONFIG_MTK_AAL_SUPPORT
		disp_pq_notify_backlight_changed(trans_level);
		return call_notifier(1, led_dat);
#else
		call_notifier(1, led_dat);
#endif
#endif
	schedule_work(&led_dat->work);
	return led_level_set_disp(led_dat, brightness);

}

static int led_data_init(struct device *dev, struct mtk_led_data *s_led)
{
	int ret;

	s_led->cdev.brightness = s_led->level;
	s_led->cdev.max_brightness = s_led->max_brightness;
	s_led->cdev.flags = LED_CORE_SUSPENDRESUME;
	s_led->cdev.brightness_set_blocking = led_level_set;
	ret = devm_led_classdev_register(dev, &(s_led->cdev));
	if (ret < 0) {
		pr_notice("led class register fail!");
		return ret;
	}
	pr_info("%s devm_led_classdev_register ok! ", s_led->cdev.name);
	mutex_init(&(s_led->limit.lock));
	if (!strcmp(s_led->cdev.name, "lcd-backlight")) {
		mutex_lock(&(s_led->limit.lock));
		s_led->limit.limit_l = 255;
		s_led->limit.flag = 0;
		s_led->limit.set_l = s_led->level;
		mutex_unlock(&(s_led->limit.lock));
	}
	INIT_WORK(&s_led->work, mtk_led_work);
	sprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		"[Light] Set %s directly ", s_led->cdev.name);
	return 0;

}

static int mtk_leds_parse_dt(struct device *dev,
		struct mtk_leds_info *m_leds)
{
	struct device_node *leds_np, *child;
	struct mtk_led_data *s_led;
	int ret = 0, num = 0;
	const char *state;

	leds_np = of_find_node_by_name(dev->of_node, "backlight");
	if (!leds_np) {
		pr_info("Error load dts node, node name error!");
		ret = -EINVAL;
		return ret;
	}

	for_each_available_child_of_node(dev->of_node, child) {

		s_led = &(m_leds->leds[num]);
		ret = of_property_read_string(child, "label",
			&s_led->cdev.name);
		if (ret) {
			pr_info("Fail to read label property");
			ret = -EINVAL;
			goto out_led_dt;
		}
		ret = of_property_read_u32(child,
			"led-bits", &(s_led->led_bits));
		if (ret) {
			pr_info("No led-bits, use default value 8");
			s_led->led_bits = 8;
		}
		s_led->max_brightness =
			(1 << s_led->led_bits) - 1;
		ret = of_property_read_u8(child,
			"limit-state", &(s_led->limit.flag));
		if (ret) {
			pr_info("No limit-state, use default value 0");
			s_led->limit.flag = 0;
		}
		ret = of_property_read_u32(child,
			"trans-bits", &(s_led->trans_bits));
		if (ret) {
			pr_info("No trans-bits, use default value 10");
			s_led->trans_bits = 10;
		}
		ret = of_property_read_string(child, "default-state", &state);
		if (!ret) {
			if (!strcmp(state, "half"))
				s_led->level =
					s_led->max_brightness / 2;
			else if (!strcmp(state, "on"))
				s_led->level =
					s_led->max_brightness;
			else
				s_led->level = 0;

		} else
			s_led->level = s_led->max_brightness;
		pr_info("parse %d leds dt: %s, %d, %d",
			num, s_led->cdev.name,
			s_led->max_brightness,
			s_led->led_bits);
		strncpy(s_led->desp.name, s_led->cdev.name,
			strlen(s_led->cdev.name));
		s_led->desp.index = num;
		leds_info->leds[num] = &s_led->desp;
		ret = led_data_init(dev, s_led);
		if (ret)
			goto out_led_dt;
		led_level_set(&s_led->cdev, s_led->level);
		num++;
	}
	m_leds->nums = num;
	pr_info("load dts ok!");
	return 0;
out_led_dt:
	pr_notice("Error load dts node!");
	of_node_put(child);
	return ret;
}


/****************************************************************************
 * driver functions
 ***************************************************************************/

static int mtk_leds_probe(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct mtk_leds_info *m_leds;
	int ret, nums;

	pr_info("probe begain +++");

	nums = of_get_child_count(dev->of_node);
	pr_info("Load dts node nums: %d", nums);
	m_leds = devm_kzalloc(dev, (sizeof(struct mtk_leds_info) +
		(sizeof(struct mtk_led_data) * (nums))), GFP_KERNEL);
	if (!m_leds) {
		ret = -ENOMEM;
		goto err;
	}
	leds_info = devm_kzalloc(dev, (sizeof(struct leds_desp_info) +
		sizeof(struct led_desp *) * (nums)),
		GFP_KERNEL);
	leds_info->lens = nums;
	if (!leds_info) {
		ret = -ENOMEM;
		goto err;
	}

	ret = mtk_leds_parse_dt(&(pdev->dev), m_leds);
	if (ret) {
		pr_notice("Failed to parse devicetree!\n");
		goto err;
	}

	platform_set_drvdata(pdev, m_leds);
	m_leds->dev = dev;

	pr_info("probe end ---");
	//setMaxBrightness("lcd-backlight", 50, 1);
	//mt_leds_brightness_set("lcd-backlight", 255);
	return ret;
 err:
	pr_notice("Failed to probe!");
	return ret;
}

static int mtk_leds_remove(struct platform_device *pdev)
{
	int i;
	struct mtk_leds_info *m_leds = dev_get_platdata(&pdev->dev);

	if (m_leds)
		return 0;
	for (i = 0; i < m_leds->nums; i++) {
		if (!m_leds->leds[i].parent)
			continue;
		led_classdev_unregister(&m_leds->leds[i].cdev);
		cancel_work_sync(&m_leds->leds[i].work);
		m_leds->leds[i].parent = NULL;
	}
	kfree(m_leds);
	m_leds = NULL;

	return 0;
}

static void mtk_leds_shutdown(struct platform_device *pdev)
{
	int i;
	struct mtk_leds_info *m_leds = dev_get_platdata(&pdev->dev);

	pr_info("Turn off backlight\n");

	for (i = 0; m_leds && i < m_leds->nums; i++) {
		if (!&(m_leds->leds[i]))
			continue;
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
		call_notifier(2, &m_leds->leds[i]);
#ifdef CONFIG_MTK_AAL_SUPPORT
		continue;
#endif
#endif
		led_level_set_disp(&m_leds->leds[i], 0);
	}
}

static const struct of_device_id of_mtk_disp_leds_match[] = {
	{ .compatible = "mediatek,disp-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_mtk_disp_leds_match);

static struct platform_driver mtk_disp_leds_driver = {
	.driver = {
		   .name = "mtk-disp-leds",
		   .of_match_table = of_mtk_disp_leds_match,
		   },
	.probe = mtk_leds_probe,
	.remove = mtk_leds_remove,
	.shutdown = mtk_leds_shutdown,
};

static int __init mtk_leds_init(void)
{
	int ret;

	pr_info("Leds init");
	ret = platform_driver_register(&mtk_disp_leds_driver);

	if (ret) {
		pr_info("driver register error: %d", ret);
		return ret;
	}

	return ret;
}

static void __exit mtk_leds_exit(void)
{
	platform_driver_unregister(&mtk_disp_leds_driver);
}

/* delay leds init, for (1)display has delayed to use clock upstream.
 * (2)to fix repeat switch battary and power supply caused BL KE issue,
 * battary calling bl .shutdown whitch need to call disp_pwm and display
 * function and they not yet probe.
 */
late_initcall(mtk_leds_init);
module_exit(mtk_leds_exit);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Display Backlight Driver");
MODULE_LICENSE("GPL");



