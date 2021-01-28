// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>


/****************************************************************************
 * variables
 ***************************************************************************/

struct mtk_leds_info;
struct led_desp *leds_desp;

static int led_pwm_level_set(struct led_classdev *led_cdev,
					 enum led_brightness brightness);

struct led_pwm_info {
	struct pwm_device *pwm;
	struct led_pwm config;
	unsigned long long duty;
};

struct led_debug_info {
	unsigned long long current_t;
	unsigned long long last_t;
	char buffer[4096];
	int count;
};

struct led_limit_info {

	unsigned int limit_l;
	u8 flag;
	unsigned int current_l;
	unsigned int last_l;
};

struct led_desp {
	int index;
	char name[16];
};

struct mtk_led_data {
	struct led_desp desp;
	struct led_classdev	cdev;
	struct led_pwm_info info;
	int level;
	int delay_on;
	int delay_off;
	int led_bits;
	int trans_bits;
	struct device_node *np;
	struct mtk_leds_info	*parent;
	struct led_debug_info debug;
	struct led_limit_info limit;
	struct work_struct work;
};

struct mtk_leds_info {
	struct device *dev;
	struct mutex lock;
	int			nums;
	struct mtk_led_data leds[0];
	struct wakeup_source leds_suspend_lock;
};

static DEFINE_MUTEX(leds_mutex);


/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/

#define LEDS_DRV_TAG "[LED_DRV]"
#define LEDS_DRV_INFO(format, args...) \
	pr_info("%s:%s() line-%d: " format,	\
		LEDS_DRV_TAG, __func__, __LINE__, ## args)

static void led_debug_log(struct mtk_led_data *s_led,
		int level, int mappingLevel)
{
	unsigned long cur_time_mod = 0;
	unsigned long long cur_time_display = 0;
	int ret = 0;

	s_led->debug.current_t = sched_clock();
	cur_time_display = s_led->debug.current_t;
	do_div(cur_time_display, 1000000);
	cur_time_mod = do_div(cur_time_display, 1000);

	ret = snprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		4095 - strlen(s_led->debug.buffer),
		"T:%lld.%ld,L:%d map:%d    ",
		cur_time_display, cur_time_mod, level, mappingLevel);

	s_led->debug.count++;

	if (ret < 0 || ret >= 4096) {
		pr_info("print log error!");
		s_led->debug.count = 5;
	}

	if (level == 0 || s_led->debug.count >= 5 ||
		(s_led->debug.current_t - s_led->debug.last_t) > 1000000000) {
		LEDS_DRV_INFO("%s", s_led->debug.buffer);
		s_led->debug.count = 0;
		s_led->debug.buffer[strlen("[Light] Set directly ") +
			strlen(s_led->cdev.name)] = '\0';
	}

	s_led->debug.last_t = sched_clock();
}


/****************************************************************************
 * add API for temperature control
 ***************************************************************************/

struct led_desp *getLedDesp(char *name)
{
	while (leds_desp++) {
		if (strcmp(name, leds_desp->name) == 0)
			return leds_desp;
	}
	return NULL;
}
EXPORT_SYMBOL(getLedDesp);

int setMaxBrightness(struct led_desp *desp, int percent, int enable)
{
	struct mtk_led_data *led_dat;
	int limit_l, max_l;

	if (!desp) {
		LEDS_DRV_INFO("can not find leds by led_desp %s",
			desp->name);
		return -1;
	}
	led_dat = container_of(desp, struct mtk_led_data, desp);

	if (!led_dat) {
		LEDS_DRV_INFO("not support led %s CONTROL_BL_TEMPERATURE!",
			desp->name);
		return -1;
	}
	max_l = led_dat->cdev.max_brightness;
	limit_l = (percent * max_l) / 100;
	LEDS_DRV_INFO("name: %s, limit_l : %d, enable: %d",
		desp->name, limit_l, enable);
	if (enable) {
		led_dat->limit.flag = 1;
		led_dat->limit.limit_l = limit_l;
		if (led_dat->limit.current_l != 0) {
			if (led_dat->limit.limit_l < led_dat->limit.last_l) {
				LEDS_DRV_INFO
					("set value control start! limit=%d\n",
					 led_dat->limit.limit_l);
				led_dat->level = led_dat->limit.limit_l;
				led_pwm_level_set(&led_dat->cdev,
					led_dat->limit.limit_l);
			} else {
				led_pwm_level_set(&led_dat->cdev,
					led_dat->limit.last_l);
			}
		}
	} else {
		led_dat->limit.flag = 0;
		led_dat->limit.limit_l = (1 << led_dat->led_bits) - 1;

		if (led_dat->limit.current_l != 0) {
			LEDS_DRV_INFO("control temperature close:limit=%d\n",
					   led_dat->limit.limit_l);
			led_pwm_level_set(&led_dat->cdev,
					    led_dat->limit.last_l);
		}
	}

	return 0;

}
EXPORT_SYMBOL(setMaxBrightness);

/****************************************************************************
 * driver functions
 ***************************************************************************/
static void __led_pwm_set(struct led_pwm_info *led_info)
{
	int new_duty = led_info->duty;

	pwm_config(led_info->pwm, new_duty, led_info->config.pwm_period_ns);
	if (new_duty == 0)
		pwm_disable(led_info->pwm);
	else
		pwm_enable(led_info->pwm);
}

static int led_pwm_set(struct mtk_led_data *led_dat,
				unsigned int brightness)
{
	unsigned int max;
	unsigned long long duty;

	led_dat->level = brightness;
	max = led_dat->cdev.max_brightness;
	duty = led_dat->info.config.pwm_period_ns;
	duty *= brightness;
	LEDS_DRV_INFO("brightness=%d, max_brightness=%d, duty=%lld",
		brightness, max, duty);
	do_div(duty, max);

	if (led_dat->info.config.active_low)
		duty = led_dat->info.config.pwm_period_ns - duty;

	led_dat->info.duty = duty;

	__led_pwm_set(&led_dat->info);

	return 0;
}


void mtk_led_work(struct work_struct *work)
{
	struct mtk_led_data *led_data =
	    container_of(work, struct mtk_led_data, work);

	mutex_lock(&leds_mutex);
	led_pwm_set(led_data, led_data->level);
	mutex_unlock(&leds_mutex);
}

static int led_level_set(struct mtk_led_data *s_led,
	enum led_brightness brightness)
{

	unsigned int mappingLevel = (
		(((1 << s_led->trans_bits) - 1) * brightness
		+ (((1 << s_led->led_bits) - 1) / 2))
		/ ((1 << s_led->led_bits) - 1));

	schedule_work(&s_led->work);
	s_led->level = brightness;
	led_debug_log(s_led, brightness, mappingLevel);
	led_pwm_set(s_led, brightness);
	return 0;
}

static int led_pwm_disable(struct led_pwm_info *led_info)
{

	pwm_config(led_info->pwm, 0, led_info->config.pwm_period_ns);
	pwm_disable(led_info->pwm);

	return 0;
}

static int led_pwm_level_set(struct led_classdev *led_cdev,
					  enum led_brightness brightness)
{
	struct mtk_led_data *led_dat =
		container_of(led_cdev, struct mtk_led_data, cdev);

	if (strcmp(led_dat->info.config.name, "lcd-backlight")) {
		led_dat->limit.current_l = brightness;
		if (led_dat->limit.flag) {
			if (led_dat->limit.limit_l < led_dat->limit.current_l)
				brightness = led_dat->limit.limit_l;
		} else
			led_dat->limit.last_l = brightness;
	}
	if (led_dat->level != brightness)
		return led_level_set(led_dat, brightness);
	return 0;
}

static void led_data_init(struct mtk_led_data *s_led)
{
	int ret = 0;

	if (!strcmp(s_led->info.config.name, "lcd-backlight")) {
		s_led->limit.last_l = 0;
		s_led->limit.limit_l = 255;
		s_led->limit.flag = 0;
		s_led->limit.current_l = 0;
	}
	INIT_WORK(&s_led->work, mtk_led_work);
	ret = snprintf(s_led->debug.buffer + strlen(s_led->debug.buffer),
		4095 - strlen(s_led->debug.buffer),
		"[Light] Set %s directly ", s_led->info.config.name);

	s_led->debug.count++;

	if (ret < 0 || ret >= 4096)
		pr_info("print log init error!");

}

static int led_pwm_config_add(struct device *dev,
				 struct mtk_led_data *s_led)
{
	struct pwm_args pargs;
	int ret = 0;

	s_led->cdev.name = s_led->info.config.name;
	s_led->cdev.default_trigger = s_led->info.config.default_trigger;
	s_led->cdev.brightness = s_led->level;
	s_led->cdev.max_brightness = s_led->info.config.max_brightness;
	s_led->cdev.flags = LED_CORE_SUSPENDRESUME;
	s_led->cdev.brightness_set_blocking = led_pwm_level_set;
	ret = devm_led_classdev_register(dev, &(s_led->cdev));
	LEDS_DRV_INFO("%s devm_led_classdev_register ok! ", s_led->cdev.name);

	if (s_led->np != NULL)
		s_led->info.pwm = devm_of_pwm_get(dev, s_led->np,
			s_led->info.config.name);
	else
		s_led->info.pwm = devm_pwm_get(dev, s_led->info.config.name);
	if (IS_ERR(s_led->info.pwm)) {
		ret = PTR_ERR(s_led->info.pwm);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "unable to request PWM for %s, err_code: %d\n",
				s_led->info.config.name, ret);
			goto err;
		}
	}

	pwm_apply_args(s_led->info.pwm);
	pwm_get_args(s_led->info.pwm, &pargs);

	s_led->info.config.pwm_period_ns = pargs.period;
	if (!s_led->info.config.pwm_period_ns && (pargs.period > 0))
		s_led->info.config.pwm_period_ns = pargs.period;
	LEDS_DRV_INFO("info.config.pwm_period_ns = %d!",
		s_led->info.config.pwm_period_ns);

	led_pwm_level_set(&s_led->cdev, s_led->cdev.brightness);
	LEDS_DRV_INFO("set led pwm OK!");
	return ret;

 err:
	dev_err(dev, "add pwm failed!\n");
	ret = -ENOMEM;
	return ret;

}

static int mtk_leds_parse_dt(struct device *dev,
		struct mtk_leds_info *m_leds)
{
	struct device_node *leds_np, *child;
	struct mtk_led_data *s_led;
	int ret = 0, num = 0;
	const char *state;

	if (!dev->of_node) {
		dev_err(dev, "Error load dts: node not exist!\n");
		return ret;
	}
	leds_np = of_find_node_by_name(dev->of_node, "backlight");
	if (!leds_np) {
		dev_err(dev, "Error load dts node, node name error!\n");
		return ret;
	}

	for_each_available_child_of_node(dev->of_node, child) {

		s_led = &(m_leds->leds[num]);
		ret = of_property_read_string(child, "label",
			&(s_led->info.config.name));
		if (ret) {
			dev_err(dev, "Fail to read label property");
			goto out_led_dt;
		}
		ret = of_property_read_string(child, "default-trigger",
			&(s_led->info.config.default_trigger));
		if (ret) {
			dev_err(dev, "Fail to read default-trigger property");
			goto out_led_dt;
		}
		ret = of_property_read_u8(child, "active-low",
			&(s_led->info.config.active_low));
		if (ret) {
			dev_err(dev, "Fail to read active-low property\n");
			goto out_led_dt;
		}
		ret = of_property_read_u32(child,
			"led-bits", &(s_led->led_bits));
		if (ret) {
			LEDS_DRV_INFO("No led-bits, use default value 8");
			s_led->led_bits = 8;
		}
		s_led->info.config.max_brightness =
			(1 << s_led->led_bits) - 1;
		ret = of_property_read_u8(child,
			"limit-state", &(s_led->limit.flag));
		if (ret) {
			LEDS_DRV_INFO("No limit-state, use default value 0");
			s_led->limit.flag = 0;
		}
		ret = of_property_read_u32(child,
			"trans-bits", &(s_led->trans_bits));
		if (ret) {
			LEDS_DRV_INFO("No trans-bits, use default value 10");
			s_led->trans_bits = 10;
		}
		ret = of_property_read_string(child, "default-state", &state);
		if (!ret) {
			if (!strcmp(state, "half"))
				s_led->level =
					s_led->info.config.max_brightness / 2;
			else if (!strcmp(state, "on"))
				s_led->level =
					s_led->info.config.max_brightness;
			else
				s_led->level = 0;

		} else
			s_led->level = s_led->info.config.max_brightness;
		LEDS_DRV_INFO("parse %d leds dt: %s, %s, %d, %d, %d\n",
			num, s_led->info.config.name,
			s_led->info.config.default_trigger,
			s_led->info.config.active_low,
			s_led->info.config.max_brightness,
			s_led->led_bits);
		s_led->np = child;
		s_led->parent = m_leds;
		s_led->desp.index = num;
		strncpy(s_led->desp.name, s_led->info.config.name,
			strlen(s_led->info.config.name));
		leds_desp[num] = s_led->desp;
		led_data_init(s_led);
		led_pwm_config_add(dev, s_led);
		led_pwm_level_set(&s_led->cdev, s_led->level);
		num++;
	}
	m_leds->nums = num;
	LEDS_DRV_INFO("load dts ok!");
	return ret;
out_led_dt:
	dev_err(dev, "Error load dts node!\n");
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

	LEDS_DRV_INFO("probe begain +++");

	nums = of_get_child_count(dev->of_node);
	LEDS_DRV_INFO("Load dts node nums: %d", nums);
	m_leds = devm_kzalloc(dev, (sizeof(struct mtk_leds_info) +
			  (sizeof(struct mtk_led_data) * (nums))), GFP_KERNEL);
	if (!m_leds)
		goto err;
	leds_desp = devm_kzalloc(dev,
		(sizeof(struct led_desp) * (nums)), GFP_KERNEL);
	if (!leds_desp)
		goto err;

	platform_set_drvdata(pdev, m_leds);
	m_leds->dev = dev;
	mutex_init(&m_leds->lock);
	ret = mtk_leds_parse_dt(&(pdev->dev), m_leds);
	if (ret) {
		dev_err(&pdev->dev, "Failed to parse devicetree!\n");
		goto err;
	}

	LEDS_DRV_INFO("probe end ---");
	return 0;
 err:
	dev_err(&pdev->dev, "Failed to probe!\n");
	ret = -ENOMEM;
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

	LEDS_DRV_INFO("Turn off backlight\n");

	for (i = 0; m_leds && i < m_leds->nums; i++) {
		if (!&(m_leds->leds[i]))
			continue;
		 led_pwm_disable(&(m_leds->leds[i].info));
	}

}

static const struct of_device_id of_mtk_pwm_leds_match[] = {
	{ .compatible = "mediatek,pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_mtk_pwm_leds_match);

static struct platform_driver mtk_pwm_leds_driver = {
	.driver = {
		   .name = "mtk-pwm-leds",
		   .owner = THIS_MODULE,
		   .of_match_table = of_mtk_pwm_leds_match,
		   },
	.probe = mtk_leds_probe,
	.remove = mtk_leds_remove,
	.shutdown = mtk_leds_shutdown,

};

static int __init mtk_leds_init(void)
{
	int ret;

	LEDS_DRV_INFO("Leds init\n");
	ret = platform_driver_register(&mtk_pwm_leds_driver);

	if (ret) {
		LEDS_DRV_INFO("driver register error: %d\n", ret);
		return ret;
	}

	return ret;
}

static void __exit mtk_leds_exit(void)
{
	platform_driver_unregister(&mtk_pwm_leds_driver);
}

/* delay leds init, for (1)display has delayed to use clock upstream.
 * (2)to fix repeat switch battary and power supply caused BL KE issue,
 * battary calling bl .shutdown whitch need to call disp_pwm and display
 * function and they not yet probe.
 */
late_initcall(mtk_leds_init);
module_exit(mtk_leds_exit);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Disp PWM Backlight Driver");
MODULE_LICENSE("GPL");


