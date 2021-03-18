
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

struct mtk_led_data {
	struct led_conf_info conf;
	int last_brightness;
	int last_level;
	int hw_level;
	struct led_debug_info debug;
};

struct mtk_leds_info {
	int			nums;
	struct i2c_client	*i2c;
	struct mtk_led_data leds[0];
};

static DEFINE_MUTEX(leds_mutex);
static struct i2c_client *_lcm_i2c_client;
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
	struct mtk_leds_info *leds = i2c_get_clientdata(_lcm_i2c_client);

	if (!leds)
		return NULL;

	while (i < leds->nums) {
		if (!strcmp(name, leds->leds[i].conf.cdev.name))
			return &leds->leds[i];
		i++;
	}

	return NULL;
}

static int brightness_maptolevel(struct led_conf_info *led_dat, int brightness)
{
	return ((((1 << led_dat->trans_bits) - 1) * brightness
				+ (((1 << led_dat->led_bits) - 1) / 2))
				/ ((1 << led_dat->led_bits) - 1));
}

int lcm_i2c_write_bytes(unsigned char addr,	unsigned char value)
{
	int ret = 0;
	char write_data[2] = { 0 };

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(_lcm_i2c_client, write_data, 2);
	if (ret < 0)
		dev_info(&_lcm_i2c_client->dev, "[LED][ERROR][%d] _lcm_i2c write data fail: %0x, %0x!!\n",
				ret, addr, value);

	return ret;
}
EXPORT_SYMBOL_GPL(lcm_i2c_write_bytes);
#define BACKLIGHT_CONFIG_1		0x02
#define BACKLIGHT_CONFIG_2		0x03
#define BACKLIGHT_BRIGHTNESS_LSB	0x04
#define BACKLIGHT_BRIGHTNESS_MSB	0x05
#define BACKLIGHT_AUTO_FREQ_LOW		0x06
#define BACKLIGHT_AUTO_FREQ_HIGH	0x07
#define BACKLIGHT_ENABLE		0x08

#define FLAGS				0x0F
#define BACKLIGHT_OPTION_1		0x10
#define BACKLIGHT_OPTION_2		0x11

static int mtk_leds_restore_brightness(char *name)
{
	struct mtk_led_data *led_dat;
	int level_h, level_l;

	led_dat = getLedData(name);

	if (!led_dat)
		return -EINVAL;
	pr_info("%s recovery brightness %d", name, led_dat->hw_level);

	level_h = (led_dat->hw_level & 0x7F8) >> 3;
	level_l = led_dat->hw_level & 0x7;

	lcm_i2c_write_bytes(BACKLIGHT_BRIGHTNESS_LSB, level_l);
	lcm_i2c_write_bytes(BACKLIGHT_BRIGHTNESS_MSB, level_h);
	udelay(200);
	return 0;
}

void mtk_leds_init_power(void)
{
	/*BL enable*/
	lcm_i2c_write_bytes(BACKLIGHT_CONFIG_1, 0x68);
	lcm_i2c_write_bytes(BACKLIGHT_CONFIG_2, 0x9D);
	lcm_i2c_write_bytes(BACKLIGHT_AUTO_FREQ_LOW, 0x00);
	lcm_i2c_write_bytes(BACKLIGHT_AUTO_FREQ_HIGH, 0x00);
	lcm_i2c_write_bytes(BACKLIGHT_OPTION_1, 0x06);
	lcm_i2c_write_bytes(BACKLIGHT_OPTION_2, 0xB7);
	/*bias enable*/
	lcm_i2c_write_bytes(BACKLIGHT_ENABLE, 0x15);
	/*brightness set*/
	mtk_leds_restore_brightness("lcd-backlight");
}
EXPORT_SYMBOL_GPL(mtk_leds_init_power);

void mtk_leds_deinit_power(void)
{
	lcm_i2c_write_bytes(BACKLIGHT_ENABLE, 0x00);
}
EXPORT_SYMBOL_GPL(mtk_leds_deinit_power);

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

	level_h = (level & 0x7F8) >> 3;
	level_l = level & 0x7;

	led_debug_log(s_led, level);

//	level_l = 0x7;
//	level_h = 0xff;

	lcm_i2c_write_bytes(BACKLIGHT_BRIGHTNESS_LSB, level_l);
	lcm_i2c_write_bytes(BACKLIGHT_BRIGHTNESS_MSB, level_h);
	udelay(200);

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
	struct device_node *node;
	struct mtk_led_data *s_led;
	int ret = 0;
	const char *state;
	int num = 0;
	enum led_brightness brightness;

	node = dev->of_node;
	s_led = &(m_leds->leds[num]);

	ret = of_property_read_string(node, "label", &s_led->conf.cdev.name);
	if (ret) {
		dev_info(dev, "[ERROR] Fail to read label property\n");
		goto out_led_dt;
	}

	ret = of_property_read_u32(node, "led-bits", &(s_led->conf.led_bits));
	if (ret) {
		dev_info(dev, "No led-bits, use default value 8\n");
		s_led->conf.led_bits = 8;
	}
	s_led->conf.cdev.max_brightness = (1 << s_led->conf.led_bits) - 1;

	ret = of_property_read_u32(node, "trans-bits", &(s_led->conf.trans_bits));
	if (ret) {
		dev_info(dev, "No trans-bits, use default value 10\n");
		s_led->conf.trans_bits = 10;
	}

	s_led->conf.max_level = (1 << s_led->conf.trans_bits) - 1;

	ret = of_property_read_string(node, "default-state", &state);
	if (!ret) {
		if (!strcmp(state, "half"))
			brightness = LED_HALF;
		else if (!strcmp(state, "on"))
			brightness = LED_ON;
		else if (!strcmp(state, "full"))
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

	s_led->last_brightness = 0;

	ret = led_data_init(dev, s_led, brightness);
	if (ret)
		goto out_led_dt;
	num++;

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
static int mtk_leds_number(struct device_node *node)
{
	struct device_node *child;
	int num = 0, childnum = 0;

	childnum = of_get_child_count(node);

	for_each_available_child_of_node(node, child) {
		pr_info("node name: %s\n", child->name);
		if (strstr(child->name, "backlight"))
			num++;
	}

	return num;
}

static int leds_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct mtk_leds_info *leds;
	int ret, nums;

	dev_info(dev, "begin probe i2c leds\n");

	nums = mtk_leds_number(dev->of_node->parent);

	leds = devm_kzalloc(dev, (sizeof(struct mtk_leds_info) +
				(sizeof(struct mtk_led_data) * (nums))),
				GFP_KERNEL);
	if (!leds)
		return -ENOMEM;

	leds->i2c = i2c;
	_lcm_i2c_client = i2c;
	i2c_set_clientdata(i2c, leds);

	mtk_leds_init_power();
	ret = mtk_leds_parse_dt(&i2c->dev, leds);
	if (ret) {
		dev_info(dev, "[ERROR] Failed to parse devicetree!\n");
		return ret;
	}

	dev_info(dev, "probe i2c leds end\n");

	return ret;
}

static int leds_i2c_remove(struct i2c_client *i2c)
{
	int i;
	struct mtk_leds_info *m_leds = i2c_get_clientdata(i2c);

	for (i = 0; i < m_leds->nums; i++)
		led_classdev_unregister(&m_leds->leds[i].conf.cdev);

	kfree(m_leds);
	m_leds = NULL;

	return 0;
}

static void leds_i2c_shutdown(struct i2c_client *i2c)
{
	int i;
	struct mtk_leds_info *m_leds = i2c_get_clientdata(i2c);

	for (i = 0; m_leds && i < m_leds->nums; i++) {
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
		call_notifier(2, &(m_leds->leds[i].conf));
#ifdef CONFIG_MTK_AAL_SUPPORT
		continue;
#endif
#endif
		led_level_i2c_set(&m_leds->leds[i], 0);
	}
}

static const struct of_device_id of_leds_i2c_match[] = {
	{ .compatible = "mediatek,i2c-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_leds_i2c_match);

static struct i2c_driver i2c_leds_driver = {
	.driver = {
		   .name = "mtk-i2c-leds",
		   .of_match_table = of_leds_i2c_match,
		   },
	.probe = leds_i2c_probe,
	.remove = leds_i2c_remove,
	.shutdown = leds_i2c_shutdown,
};
module_i2c_driver(i2c_leds_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Display Backlight Driver");
MODULE_LICENSE("GPL");

