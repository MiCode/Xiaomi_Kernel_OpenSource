// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifdef CONFIG_LEDS_MTK_PWM
#include <leds-mtk-pwm.h>
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#elif defined(CONFIG_LEDS_MTK_I2C)
#include <leds-mtk-i2c.h>
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#endif
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "mtk_drm_gateic.h"

#define ADDR_BACKLIGHT_CONFIG1			(0x02)
#define ADDR_BACKLIGHT_CONFIG2			(0x03)
#define ADDR_BACKLIGHT_BRIGHTNESS_LSB	(0x04)
#define ADDR_BACKLIGHT_BRIGHTNESS_MSB	(0x05)
#define ADDR_BACKLIGHT_AUTO_FREQ_LOW	(0x06)
#define ADDR_BACKLIGHT_AUTO_FREQ_HIGH	(0x07)
#define ADDR_BACKLIGHT_ENABLE			(0x08)
#define ADDR_BACKLIGHT_OPTION1			(0x10)
#define ADDR_BACKLIGHT_OPTION2			(0x11)
#define ADDR_BACKLIGHT_SMOOTH			(0x14)
#define ADDR_BIAS_CONFIG1				(0x09)
#define ADDR_BIAS_CONFIG2				(0x0A)
#define ADDR_BIAS_CONFIG3				(0x0B)
#define ADDR_BIAS_LCM					(0x0C)
#define ADDR_BIAS_VPOS					(0x0D)
#define ADDR_BIAS_VNEG					(0x0E)

#define BRIGHTNESS_LOW_OFFSET			(0)
#define BRIGHTNESS_HIGH_OFFSET			(3)
#define BRIGHTNESS_LOW_MASK				(0x07)
#define BRIGHTNESS_HIGH_MASK			(0x7F8)

#define RT4831A_VOL_UNIT (50) //50mV
#define RT4831A_VOL_MIN_LEVEL (4000) //4000mV
#define RT4831A_VOL_MAX_LEVEL (6500) //6500mV
#define RT4831A_VOL_REG_VALUE(level) ((level - RT4831A_VOL_MIN_LEVEL) / RT4831A_VOL_UNIT)

struct mtk_gateic_data ctx_rt4831a;

static int rt4831a_push_i2c_data(unsigned char *table, unsigned int size,
		unsigned int unit)
{
	int ret = 0, i = 0;

	if (IS_ERR_OR_NULL(table) ||
		size == 0 || unit < 2) {
		DDPMSG("%s, invalid table, size:%u, unit:%u\n",
			__func__, size, unit);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		if (unit > 2)
			ret = mtk_panel_i2c_write_multiple_bytes(*(table + i * unit),
					table + i * unit + 1, unit - 1);
		else if (unit == 2)
			ret = mtk_panel_i2c_write_bytes(*(table + i * 2),
					*(table + i * 2 + 1));

		if (ret < 0) {
			DDPMSG("%s, failed of i2c write, i:%u addr:0x%x, unit:0x%x",
				__func__, i, *(table + i * unit), unit);
			break;
		}
	}

	return ret;
}

static int rt4831a_update_backlight_table(unsigned int level, unsigned char *table,
		unsigned int size, unsigned int unit)
{
	int i = 0;

	if (IS_ERR_OR_NULL(table) ||
		size == 0 || unit < 2) {
		DDPMSG("%s, invalid table, size:%u, unit:%u\n",
			__func__, size, unit);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		if (*(table + i * unit) == ADDR_BACKLIGHT_BRIGHTNESS_LSB)
			*(table + i * unit + 1) = (level & BRIGHTNESS_LOW_MASK) >>
						BRIGHTNESS_LOW_OFFSET;
		else if (*(table + i * unit) == ADDR_BACKLIGHT_BRIGHTNESS_MSB)
			*(table + i * unit + 1) = (level & BRIGHTNESS_HIGH_MASK) >>
						BRIGHTNESS_HIGH_OFFSET;
	}

	return 0;
}


static int rt4831a_set_backlight(unsigned int level)
{
	int ret = 0;
	unsigned char table[2][2] = {
		{ADDR_BACKLIGHT_BRIGHTNESS_LSB, 0},
		{ADDR_BACKLIGHT_BRIGHTNESS_MSB, 0}
	};
	unsigned int unit = ARRAY_SIZE(table[0]);
	unsigned int size = sizeof(table) / unit;

	if (atomic_read(&ctx_rt4831a.init) != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (ctx_rt4831a.backlight_mode != BL_I2C_MODE) {
		DDPPR_ERR("%s do not support I2C mode\n", __func__);
		return -1;
	}

	rt4831a_update_backlight_table(level, &table[0][0], size, unit);
	ret = rt4831a_push_i2c_data(&table[0][0], size, unit);
	if (ret < 0)
		DDPMSG("%s: ERROR %d!! i2c write data fail 0x%0x, 0x%0x !!\n",
				__func__, ret, table[0][1], table[1][1]);

	ctx_rt4831a.backlight_level = level;
	return 0; //for evb case, i2c ops are always failed
}

static int rt4831a_enable_backlight(void)
{
	int ret = 0;
	unsigned char table[5][2] = {
		{ADDR_BACKLIGHT_CONFIG2, 0x9D},
		{ADDR_BACKLIGHT_OPTION1, 0x06},
		{ADDR_BACKLIGHT_OPTION2, 0xB7},
		{ADDR_BACKLIGHT_ENABLE, 0x15},
		{ADDR_BACKLIGHT_SMOOTH, 0x03}
	};
	unsigned int unit = ARRAY_SIZE(table[0]);
	unsigned int size = sizeof(table) / unit;

	//DDPMSG("%s+\n", __func__);
	switch (ctx_rt4831a.backlight_mode) {
	case BL_PWM_MODE:
		ret = mtk_panel_i2c_write_bytes(ADDR_BACKLIGHT_CONFIG1, 0x6B);
		break;
	case BL_I2C_MODE:
		ret = mtk_panel_i2c_write_bytes(ADDR_BACKLIGHT_CONFIG1, 0x68);
		if (atomic_read(&ctx_rt4831a.backlight_status) == 0)
			rt4831a_set_backlight(0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		DDPMSG("%s, failed to enable backlight, mode:%u",
			__func__, ctx_rt4831a.backlight_mode);
		return ret;
	}

	ret = rt4831a_push_i2c_data(&table[0][0], size, unit);
	if (ret < 0)
		DDPMSG("%s, failed to push backlight table, mode:%u, ret:%d",
			__func__, ctx_rt4831a.backlight_mode, ret);

	//DDPMSG("%s--, %d\n", __func__, ret);
	return 0; //for evb case, i2c ops are always failed
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int rt4831a_backlight_event(struct notifier_block *nb, unsigned long event,
	void *v)
{
	struct led_conf_info *led_conf;

	DDPMSG("%s+\n", __func__);

	led_conf = (struct led_conf_info *)v;

	switch (event) {
	case 1:
		if (led_conf->cdev.brightness > 0)
			atomic_set(&ctx_rt4831a.backlight_status, 1);
		else
			atomic_set(&ctx_rt4831a.backlight_status, 0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block rt4831a_leds_init_notifier = {
	.notifier_call = rt4831a_backlight_event,
};
#endif

static int rt4831a_reset(int on)
{
	ctx_rt4831a.reset_gpio =
		devm_gpiod_get(ctx_rt4831a.dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4831a.reset_gpio)) {
		dev_err(ctx_rt4831a.dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx_rt4831a.reset_gpio));
		return PTR_ERR(ctx_rt4831a.reset_gpio);
	}
	gpiod_set_value(ctx_rt4831a.reset_gpio, on);
	DDPINFO("%s, gpio:0x%x, on:%d\n",
		__func__, ctx_rt4831a.reset_gpio, on);
	devm_gpiod_put(ctx_rt4831a.dev, ctx_rt4831a.reset_gpio);

	return 0;
}

static int rt4831a_power_on(void)
{
	int ret = 0;
	unsigned char table[8][2] = {
		{ADDR_BIAS_CONFIG2, 0x11},
		{ADDR_BIAS_CONFIG3, 0x00},
		/*default voltage of 5.4v*/
		{ADDR_BIAS_LCM, 0x24},
		{ADDR_BIAS_VPOS, 0x1c},
		{ADDR_BIAS_VNEG, 0x1c},
		/*set dsv FPWM mode*/
		{0xF0, 0x69},
		{0xB1, 0x6c},
		/* bias enable*/
		{ADDR_BIAS_CONFIG1, 0x9e},
	};
	unsigned int unit = ARRAY_SIZE(table[0]);
	unsigned int size = sizeof(table) / unit;

	if (atomic_read(&ctx_rt4831a.init) != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (atomic_read(&ctx_rt4831a.ref) > 0) {
		atomic_inc(&ctx_rt4831a.ref);
		DDPPR_ERR("%s gate ic (%u) already power on\n",
			__func__, atomic_read(&ctx_rt4831a.ref));
		return 0;
	}

	DDPMSG("%s++ size:%u, unit:%u, backlight status:%d\n", __func__,
		size, unit, atomic_read(&ctx_rt4831a.backlight_status));
	ctx_rt4831a.bias_pos_gpio = devm_gpiod_get(ctx_rt4831a.dev,
		"pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4831a.bias_pos_gpio)) {
		dev_err(ctx_rt4831a.dev, "%s: cannot get pm-enable:%ld\n",
			__func__, PTR_ERR(ctx_rt4831a.bias_pos_gpio));
		return PTR_ERR(ctx_rt4831a.bias_pos_gpio);
	}
	gpiod_set_value(ctx_rt4831a.bias_pos_gpio, 1);
	devm_gpiod_put(ctx_rt4831a.dev, ctx_rt4831a.bias_pos_gpio);
	atomic_inc(&ctx_rt4831a.ref);

	ret = rt4831a_enable_backlight();
	if (ret < 0)
		DDPMSG("%s, failed to enable backlight, ret:%d",
			__func__, ret);

	ret = rt4831a_push_i2c_data(&table[0][0], size, unit);
	if (ret < 0)
		DDPMSG("%s, failed to push power on table, ret:%d",
			__func__, ret);
	//DDPMSG("%s--\n", __func__);

	return 0; //for evb case, i2c ops are always failed
}

static int rt4831a_power_off(void)
{
	int ret = 0;

	if (atomic_read(&ctx_rt4831a.init) != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (atomic_read(&ctx_rt4831a.ref) == 0) {
		DDPPR_ERR("%s gate ic (%u) already power off\n",
			__func__, atomic_read(&ctx_rt4831a.ref));
		return 0;
	}

	DDPMSG("%s++\n", __func__);
	atomic_dec(&ctx_rt4831a.ref);
	if (atomic_read(&ctx_rt4831a.ref) > 0) {
		DDPMSG("%s, %d, do nothing, there are other users, %u\n",
			__func__, __LINE__, atomic_read(&ctx_rt4831a.ref));
		return 0;
	}

	if (atomic_read(&ctx_rt4831a.backlight_status) == 0) {
		ret = mtk_panel_i2c_write_bytes(ADDR_BIAS_CONFIG1, 0x18);
		if (ret < 0)
			DDPMSG("%s, %d, failed of i2c write\n", __func__, __LINE__);

		ctx_rt4831a.bias_pos_gpio = devm_gpiod_get(ctx_rt4831a.dev,
			"pm-enable", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx_rt4831a.bias_pos_gpio)) {
			dev_err(ctx_rt4831a.dev, "%s: cannot get pm-enable:%ld\n",
				__func__, PTR_ERR(ctx_rt4831a.bias_pos_gpio));
			return PTR_ERR(ctx_rt4831a.bias_pos_gpio);
		}
		gpiod_set_value(ctx_rt4831a.bias_pos_gpio, 0);
		devm_gpiod_put(ctx_rt4831a.dev, ctx_rt4831a.bias_pos_gpio);
	}

	DDPMSG("%s--\n", __func__);
	return 0; //for evb case, i2c ops are always failed
}

static int rt4831a_set_voltage(unsigned int level)
{
	int ret = 0, i = 0;
	unsigned char level_id = 0;
	unsigned char table[3][2] = {
		{ADDR_BIAS_LCM, 0},
		{ADDR_BIAS_VPOS, 0},
		{ADDR_BIAS_VNEG, 0}
	};
	unsigned int unit = ARRAY_SIZE(table[0]);
	unsigned int size = sizeof(table) / unit;

	if (atomic_read(&ctx_rt4831a.init) != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (atomic_read(&ctx_rt4831a.ref) == 0) {
		DDPPR_ERR("%s gate ic (%u) is power off\n",
			__func__, atomic_read(&ctx_rt4831a.ref));
		return -2;
	}

	if (level < RT4831A_VOL_MIN_LEVEL || level > RT4831A_VOL_MAX_LEVEL) {
		DDPPR_ERR("%s invalid voltage level:%d\n", __func__, level);
		return -3;
	}

	level_id = (unsigned char)RT4831A_VOL_REG_VALUE(level);
	DDPMSG("%s++ level:%d, id:%u\n", __func__, level, level_id);
	for (i = 0; i < size; i++)
		table[i][1] = level_id;

	ret = rt4831a_push_i2c_data(&table[0][0], size, unit);
	if (ret < 0)
		DDPMSG("%s, failed to push voltage table,level:%u-%u, ret:%d",
			__func__, level, RT4831A_VOL_REG_VALUE(level), ret);

	return 0; //for evb case, i2c ops are always failed
}

static int rt4831a_match_lcm_list(const char *lcm_name)
{
	if (atomic_read(&ctx_rt4831a.init) != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return 0;
	}

	if (mtk_gateic_match_lcm_list(lcm_name, ctx_rt4831a.lcm_list,
		ctx_rt4831a.lcm_count, "rt4831a") == true) {
		return 1;
	}

	return 0;
}

static struct mtk_gateic_funcs rt4831a_ops = {
	.reset = rt4831a_reset,
	.power_on = rt4831a_power_on,
	.power_off = rt4831a_power_off,
	.set_voltage = rt4831a_set_voltage,
	.enable_backlight = rt4831a_enable_backlight,
	.set_backlight = rt4831a_set_backlight,
	.match_lcm_list = rt4831a_match_lcm_list,
};

static int rt4831a_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0, len = 0;

	if (atomic_read(&ctx_rt4831a.init) == 1)
		return 0;

	DDPMSG("%s++\n", __func__);
	ctx_rt4831a.dev = dev;

	len = of_property_count_strings(dev->of_node, "panel-list");
	if (len > 0) {
		//DDPMSG("%s, %d, len:%d\n", __func__, __LINE__, len);
		ctx_rt4831a.lcm_list = kcalloc(len, sizeof(char *), GFP_KERNEL);
		if (IS_ERR_OR_NULL(ctx_rt4831a.lcm_list)) {
			DDPPR_ERR("%s, %d, failed to allocate lcm list, len:%d\n",
				__func__, __LINE__, len);
			return -ENOMEM;
		}

		len = of_property_read_string_array(dev->of_node, "panel-list",
				ctx_rt4831a.lcm_list, len);
		if (len < 0) {
			DDPPR_ERR("%s, %d, failed to get panel-list, len:%d\n",
				__func__, __LINE__, len);
			ret = -EINVAL;
			goto error;
		}
		ctx_rt4831a.lcm_count = (unsigned int)len;
	} else {
		DDPPR_ERR("%s, %d, failed to get lcm_pinctrl_names, %d\n",
			__func__, __LINE__, len);
		return -EFAULT;
	}

	ctx_rt4831a.reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4831a.reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx_rt4831a.reset_gpio));
		ret = PTR_ERR(ctx_rt4831a.reset_gpio);
		goto error;
	}
	devm_gpiod_put(dev, ctx_rt4831a.reset_gpio);

	ctx_rt4831a.bias_pos_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4831a.bias_pos_gpio)) {
		dev_err(dev, "%s: cannot get pm-enable:%ld\n",
			__func__, PTR_ERR(ctx_rt4831a.bias_pos_gpio));
		ret = PTR_ERR(ctx_rt4831a.bias_pos_gpio);
		goto error;
	}
	devm_gpiod_put(dev, ctx_rt4831a.bias_pos_gpio);

	ret = of_property_read_u32(dev->of_node, "backlight_mode", &ctx_rt4831a.backlight_mode);
	if (ret != 0) {
		dev_err(dev, "%s: failed to get backlight mode, %d\n",
			__func__, ret);
		ctx_rt4831a.backlight_mode = BL_PWM_MODE;
	}

	ctx_rt4831a.backlight_level = 2047;
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_register_notifier(&rt4831a_leds_init_notifier);
#endif
	atomic_set(&ctx_rt4831a.ref, 1);
	atomic_set(&ctx_rt4831a.init, 1);

	ret = mtk_drm_gateic_register(&rt4831a_ops, MTK_LCM_FUNC_DSI);
	DDPMSG("%s--, %d, backlight mode:%u\n", __func__, ret, ctx_rt4831a.backlight_mode);

	return ret;

error:
	if (ctx_rt4831a.lcm_list != NULL)
		kfree(ctx_rt4831a.lcm_list);
	ctx_rt4831a.lcm_count = 0;

	return ret;
}

static int rt4831a_drv_remove(struct platform_device *pdev)
{
	DDPMSG("%s\n", __func__);
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_unregister_notifier(&rt4831a_leds_init_notifier);
#endif

	return 0;
}


static const struct of_device_id rt4831a_of_match[] = {
	{ .compatible = "mediatek,mtk-drm-gateic-drv-rt4831a", },
	{ }
};

MODULE_DEVICE_TABLE(of, rt4831a_of_match);

struct platform_driver mtk_gateic_rt4831a_driver = {
	.probe = rt4831a_drv_probe,
	.remove = rt4831a_drv_remove,
	.driver = {
		.name = "mtk-drm-gateic-drv-rt4831a",
		.owner = THIS_MODULE,
		.of_match_table = rt4831a_of_match,
	},
};

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm GATE IC driver of rt4831a");
MODULE_LICENSE("GPL v2");
