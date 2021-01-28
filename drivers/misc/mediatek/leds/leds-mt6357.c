// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6357/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "mt_led_trigger.h"

#ifndef UNUSED
#define UNUSED(x) { (void)(x); }
#endif

/*
 * Register field for mt6357_TOP_CKPDN0 to enable
 * 128K clock common for LED device.
 */

#define RG_DRV_ISINK_CK_PDN					MT6357_XPP_TOP_CKPDN_CON0
#define RG_DRV_128K_CK_PDN_SHIFT			4
#define RG_DRV_128K_CK_PDN_MASK				0x1
#define RG_DRV_ISINK1_CK_PDN_SHIFT			1
#define RG_DRV_ISINK1_CK_PDN_MASK			0x1
#define RG_DRV_CHRIND_CK_PDN_SHIFT			5
#define RG_DRV_CHRIND_CK_PDN_MASK			0x1

//ISINK channel enable
#define ISINK_EN_CTRL						MT6357_ISINK_EN_CTRL
#define ISINK_CH1_BIAS_EN_SHIFT				11
#define ISINK_CH1_BIAS_EN_MASK				0x1
#define ISINK_CH1_EN_SHIFT					1
#define ISINK_CH1_EN_MASK					0x1
#define ISINK_CHOP1_EN_SHIFT				5
#define ISINK_CHOP1_EN_MASK					0x1

//ISINK Step
#define ISINK_CH1_STEP						MT6357_ISINK1_CON1
#define ISINK_CH1_STEP_MASK			        13
#define ISINK_CH1_STEP_SHIFT				0x7
#define ISINK_CH1_STEP_MAX					0x7

//ISINK mode
#define ISINK_CH1_MODE						MT6357_ISINK_MODE_CTRL
#define ISINK_CH1_MODE_SHIFT				12
#define ISINK_CH1_MODE_MASK					0x3
#define ISINK_CH1_PWM_MODE_SHIFT			5
#define ISINK_CH1_PWM_MODE_MASK				0x1

#define ISINK_DIM1_FSEL						MT6357_ISINK1_CON0
#define ISINK_DIM1_FSEL_SHIFT				0
#define ISINK_DIM1_FSEL_MASK				0xFFFF

#define ISINK_DIM1_DUTY						MT6357_ISINK1_CON1
#define ISINK_DIM1_DUTY_SHIFT				5
#define ISINK_DIM1_DUTY_MASK				0xFF

//Breath mode :
#define ISINK_BREATH1_TR_SEL				MT6357_ISINK1_CON2
#define ISINK_BREATH1_TR1_SEL_SHIFT			12
#define ISINK_BREATH1_TR1_SEL_MASK			0xF
#define ISINK_BREATH1_TR2_SEL_SHIFT			8
#define ISINK_BREATH1_TR2_SEL_MASK			0xF

#define ISINK_BREATH1_TF1_SEL_SHIFT			4
#define ISINK_BREATH1_TF1_SEL_MASK			0xF
#define ISINK_BREATH1_TF2_SEL_SHIFT			0
#define ISINK_BREATH1_TF2_SEL_MASK			0xF

#define ISINK_BREATH1_TON_SEL				MT6357_ISINK1_CON3
#define ISINK_BREATH1_TON_SEL_SHIFT			8
#define ISINK_BREATH1_TON_SEL_MASK			0xF
#define ISINK_BREATH1_TOFF_SEL_SHIFT		0
#define ISINK_BREATH1_TOFF_SEL_MASK			0xF

#define ISINK_SFSTR1						MT6357_ISINK_SFSTR
#define ISINK_SFSTR1_TC_SHIFT				9
#define ISINK_SFSTR1_TC_MASK				0x3
#define ISINK_SFSTR1_EN_SHIFT				8
#define ISINK_SFSTR1_EN_MASK				0x1

#define ISINK_MODE_PWM			(0x00)
#define ISINK_MODE_BREATH		(0x01)
#define ISINK_MODE_REGISTER		(0x11)

#define mt6357_MAX_PERIOD		10000
#define mt6357_MAX_BRIGHTNESS	255

//#define LED_TEST
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

enum {
	MT6357_ISINK0 = 0,
	MT6357_ISINK1,
	MT6357_ISINK2,
	MT6357_ISINK3,
	MT6357_ISINK_MAX,
};

enum {
	MT_LEDMODE_DEFAULT,
	MT_LEDMODE_REGISTER,
	MT_LEDMODE_PWM,
	MT_LEDMODE_BREATH,
	MT_LEDMODE_MAX,
};

struct mt6357_leds;

/**
 * struct mt6357_led - state container for the LED device
 * @id:			the identifier in mt6357 LED device
 * @parent:		the pointer to mt6357 LED controller
 * @cdev:		LED class device for this LED device
 * @current_brightness: current state of the LED device
 */
struct mt6357_led {
	struct mt_led_info l_info; /* most be the first member */
	int			id;
	struct mt6357_leds	*parent;
	enum led_brightness	current_brightness;
	int step;
};

/**
 * struct mt6357_leds -	state container for holding LED controller
 *			of the driver
 * @dev:		the device pointer
 * @hw:			the underlying hardware providing shared
 *			bus for the register operations
 * @lock:		the lock among process context
 * @led:		the array that contains the state of individual
 *			LED device
 */
struct mt6357_leds {
	struct device		*dev;
	struct regmap *regmap;
	/* protect among process context */
	struct mutex		lock;
	struct mt6357_led	*led[MT6357_ISINK_MAX];
};

static int mt6357_led_get_clock(struct led_classdev *cdev, int *en)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret = 0;

	ret = regmap_read(regmap, RG_DRV_ISINK_CK_PDN, en);
	if (ret < 0) {
		dev_notice(led->parent->dev,
				"%s: RG_DRV_ISINK_CK_PDN Reg(0x%x) Read ERROR.\n",
				__func__, RG_DRV_ISINK_CK_PDN);
		return ret;
	}
	pr_info("get RG_DRV_ISINK_CK_PDN[0x%0x]: %d", RG_DRV_ISINK_CK_PDN, *en);
	switch (led->id) {
	case MT6357_ISINK1:
		*en = (~*en >> RG_DRV_ISINK1_CK_PDN_SHIFT) & RG_DRV_ISINK1_CK_PDN_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	pr_info("%s: PMIC LED(%d) Get clock %s.\n",
		__func__, led->id, en ? "True" : "False");

	return ret;
}

static int mt6357_led_set_clock(struct led_classdev *cdev, int en)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	unsigned int value;
	int ret = 0;

	dev_info(led->parent->dev, "%s: PMIC LED(%d) Set clock %s.\n",
		__func__, led->id, en ? "True" : "False");

	//XPP clock control
	switch (led->id) {
	case MT6357_ISINK1:
		value = RG_DRV_ISINK1_CK_PDN_MASK;
		value = en ? (~value & RG_DRV_ISINK1_CK_PDN_MASK) : value;
		ret = regmap_update_bits(regmap,
			RG_DRV_ISINK_CK_PDN,
			RG_DRV_ISINK1_CK_PDN_MASK << RG_DRV_ISINK1_CK_PDN_SHIFT,
			value << RG_DRV_ISINK1_CK_PDN_SHIFT);
		if (ret < 0) {
			dev_notice(led->parent->dev,
				"%s: RG_DRV_ISINK_CK_PDN Reg(0x%x) Write ERROR.\n",
				__func__, RG_DRV_ISINK_CK_PDN);
			return ret;
		}
		pr_info("set ISINK_EN_CTRL[0x%0x]: 0x%0x",
			RG_DRV_ISINK_CK_PDN,
			value << RG_DRV_ISINK1_CK_PDN_SHIFT);
		mt6357_led_get_clock(cdev, &en);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}
	return ret;
}

static int mt6357_led_set_ISINK(struct led_classdev *cdev, int en)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	unsigned int en_value;
	unsigned int chop_value;
	int ret = 0;

	dev_dbg(led->parent->dev, "%s: PMIC LED(%d) Set ISINK %s.\n",
		__func__, led->id, en ? "True" : "False");

	//PMIC ISINK enable
	switch (led->id) {
	case MT6357_ISINK1:
		en_value = ISINK_CH1_EN_MASK;
		chop_value = ISINK_CHOP1_EN_MASK;
		if (!en) {
			en_value = ~en_value;
			chop_value = ~chop_value;
		}
		en_value = (en_value & ISINK_CH1_EN_MASK) << ISINK_CH1_EN_SHIFT;
		chop_value = (chop_value & ISINK_CHOP1_EN_MASK) << ISINK_CHOP1_EN_SHIFT;
		ret = regmap_update_bits(regmap,
			ISINK_EN_CTRL,
			(ISINK_CHOP1_EN_MASK << ISINK_CHOP1_EN_SHIFT) |
			(ISINK_CH1_EN_MASK << ISINK_CH1_EN_SHIFT),
			en_value | chop_value);
		if (ret < 0) {
			dev_notice(led->parent->dev,
					"%s: ISINK_EN_CTRL Reg(0x%x) Write ERROR.\n",
					__func__, ISINK_EN_CTRL);
			return ret;
		}
		pr_info("set ISINK_EN_CTRL[0x%0x]: 0x%0x", ISINK_EN_CTRL, en_value | chop_value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;

	}
	return ret;
}

static int mt6357_led_get_ISINK(struct led_classdev *cdev, int *CTL0, int *CTL1)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret = 0;
	unsigned int value;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_EN_CTRL, &value);
		if (ret < 0) {
			dev_notice(led->parent->dev,
					"%s: ISINK_EN_CTRL Reg(0x%x) Read ERROR.\n",
					__func__, ISINK_EN_CTRL);
			return ret;
		}
		pr_info("get ISINK_EN_CTRL[0x%0x]: 0x%0x", ISINK_EN_CTRL, value);
		*CTL0 = (value >> ISINK_CHOP1_EN_SHIFT) & ISINK_CHOP1_EN_MASK;
		*CTL1 = (value >> ISINK_CH1_EN_SHIFT) & ISINK_CH1_EN_MASK;
		break;
	case MT6357_ISINK2:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}
	return ret;
}


/*
 * Trigger : register mode / breath mode / PWM mode
 */
 //Trigger mode change
static int mt6357_led_set_current_step(struct mt_led_info *info, int step);
static int mt6357_led_set_pwm_dim_freq(struct mt_led_info *info, int freq);
static int mt6357_led_hw_brightness(struct led_classdev *cdev, enum led_brightness brightness);

static int mt6357_led_change_mode(struct led_classdev *cdev, int mode)
{

	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret = 0;
	int mode_val = 0;

	if (mode >= MT_LEDMODE_MAX)
		return -EINVAL;

	mutex_lock(&leds->lock);

	dev_info(led->parent->dev, "%s mode = %s\n",
		__func__, mt_led_trigger_mode_name[mode]);
	switch (mode) {
	case MT_LEDMODE_REGISTER:
		mode_val = 2;
		break;
	case MT_LEDMODE_DEFAULT:
	case MT_LEDMODE_PWM:
		mode_val = 0;
		break;
	case MT_LEDMODE_BREATH:
		mode_val = 1;
		break;
	}

	//PMIC ISINK disable
	mt6357_led_set_ISINK(cdev, false);

	//PMIC mode: PWM mode(0x00) / Breath mode(0x01) / CC mode(0x10)
	switch (led->id) {
	case MT6357_ISINK1:
		mode_val = mode_val << ISINK_CH1_MODE_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_CH1_MODE,
			ISINK_CH1_MODE_MASK << ISINK_CH1_MODE_SHIFT, mode_val);
		pr_info("set ISINK_CH1_MODE[0x%0x]: 0x%0x", ISINK_CH1_MODE, mode_val);
		if (ret < 0) {
			dev_notice(led->parent->dev,
					"%s: ISINK_CH1_MODE Reg(0x%x) Write ERROR.\n",
					__func__, ISINK_CH1_MODE);
			return ret;
		}
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	//PMIC ISINK enable
	mt6357_led_set_ISINK(cdev, true);

	//default mode
	if (MT_LEDMODE_PWM == mode || MT_LEDMODE_DEFAULT == mode) {
		mt6357_led_set_current_step(l_info, led->step);
		mt6357_led_set_pwm_dim_freq(l_info, 0x0);
		mt6357_led_hw_brightness(cdev, mt6357_MAX_BRIGHTNESS);
	}
	mutex_unlock(&leds->lock);

	return ret;
}

static int mt6357_led_get_current_step(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		//PMIC step
		ret = regmap_read(regmap, ISINK_CH1_STEP, value);
		if (ret < 0) {
			dev_notice(led->parent->dev,
				"%s: ISINK1_CH1_STEP Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_CH1_STEP);
			return ret;
		}
		pr_info("get ISINK_CH1_STEP[0x%0x]: 0x%0x", ISINK_CH1_STEP, *value);
		*value = (*value >> ISINK_CH1_STEP_MASK) & ISINK_CH1_STEP_SHIFT;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_current_step(struct mt_led_info *info, int step)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	unsigned int value;
	int ret;

	if (step > 8 || step < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, step);
		return -EINVAL;
	}

	switch (led->id) {
	case MT6357_ISINK1:
		//PMIC step
		value = step << ISINK_CH1_STEP_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_CH1_STEP,
			ISINK_CH1_STEP_MASK << ISINK_CH1_STEP_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK1_CH1_STEP Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_CH1_STEP);
			return -1;
		}
		pr_info("set ISINK_CH1_STEP[0x%0x]: 0x%0x", ISINK_CH1_STEP, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}
	return value;
}

/*
 * Trigger : PWM mode
 */
static int mt6357_led_list_pwm_duty(struct mt_led_info *info, char *buf)
{
	snprintf(buf, PAGE_SIZE, "%s\n", "0~255");
	return 0;
}

static int mt6357_led_get_pwm_dim_duty(struct mt_led_info *info, int *value)

{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		//PMIC step
		ret = regmap_read(regmap, ISINK_DIM1_DUTY, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK1_DIM1_DUTY Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_DIM1_DUTY);
			return -1;
		}
		pr_info("get ISINK_DIM1_DUTY[0x%0x]: 0x%0x", ISINK_DIM1_DUTY, *value);
		*value = (*value >> ISINK_DIM1_DUTY_SHIFT) & ISINK_DIM1_DUTY_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}
	return ret;
}

static int mt6357_led_set_pwm_dim_duty(struct mt_led_info *info, int duty)

{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (duty > 255 || duty < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, duty);
		return -EINVAL;
	}

	//PWM Duty
	switch (led->id) {
	case MT6357_ISINK1:
		//PMIC step
		value = duty << ISINK_DIM1_DUTY_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_DIM1_DUTY,
			ISINK_DIM1_DUTY_MASK << ISINK_DIM1_DUTY_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_DIM1_DUTY Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_DIM1_DUTY);
			return -1;
		}
		pr_info("set ISINK_DIM1_DUTY[0x%0x]: 0x%0x", ISINK_DIM1_DUTY, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_list_pwm_freq(struct mt_led_info *info, char *buf)
{
	snprintf(buf, PAGE_SIZE, "%s\n", "0~65535 (500 HZ ~ 0.076HZ)");
	return 0;
}

static int mt6357_led_get_pwm_dim_freq(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_DIM1_FSEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_DIM1_FSEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_DIM1_FSEL);
			return -1;
		}
		pr_info("get ISINK_DIM1_FSEL [0x%0x]: 0x%0x", ISINK_DIM1_FSEL, *value);
		*value = (*value >> ISINK_DIM1_FSEL_SHIFT) & ISINK_DIM1_FSEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_pwm_dim_freq(struct mt_led_info *info, int freq)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	unsigned int value;
	int ret;

	if (freq > 0xFFFF || freq < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, freq);
		return -EINVAL;
	}

	//PWM Frequency
	switch (led->id) {
	case MT6357_ISINK1:
		value = value << ISINK_DIM1_FSEL_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_DIM1_FSEL,
			ISINK_DIM1_FSEL_MASK << ISINK_DIM1_FSEL_SHIFT, value);
		pr_info("set ISINK_DIM1_FSEL[0x%0x]: 0x%0x", ISINK_DIM1_FSEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_DIM1_FSEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_DIM1_FSEL);
			return -1;
		}
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}
	return ret;
}

/*
 * Trigger : breath mode
 */

static int mt6357_led_get_breath_tr1(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TR_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TR_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TR_SEL[0x%0x]1: 0x%0x", ISINK_BREATH1_TR_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TR1_SEL_SHIFT) & ISINK_BREATH1_TR1_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_get_breath_tr2(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TR_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TR_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TR_SEL[0x%0x]1: 0x%0x", ISINK_BREATH1_TR_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TR2_SEL_SHIFT) & ISINK_BREATH1_TR2_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_get_breath_tf1(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TR_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TR_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TF1_SEL_SHIFT) & ISINK_BREATH1_TF1_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_get_breath_tf2(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TR_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TR_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TF2_SEL_SHIFT) & ISINK_BREATH1_TF2_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_get_breath_ton(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TON_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TON_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TON_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TON_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TON_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TON_SEL_SHIFT) & ISINK_BREATH1_TON_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_get_breath_toff(struct mt_led_info *info, int *value)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;

	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_read(regmap, ISINK_BREATH1_TON_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TON_SEL Reg(0x%x) Read ERROR.\n",
				__func__, ISINK_BREATH1_TON_SEL);
			return -1;
		}
		pr_info("get ISINK_BREATH1_TON_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TON_SEL, *value);
		*value = (*value >> ISINK_BREATH1_TOFF_SEL_SHIFT) & ISINK_BREATH1_TOFF_SEL_MASK;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_breath_tr1(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > 15 || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	switch (led->id) {
	case MT6357_ISINK1:
		value = time << ISINK_BREATH1_TR1_SEL_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TR_SEL,
			ISINK_BREATH1_TR1_SEL_MASK << ISINK_BREATH1_TR1_SEL_SHIFT, value);
		pr_info("set ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TR_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_breath_tr2(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > 15 || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	switch (led->id) {
	case MT6357_ISINK1:
		value = time << ISINK_BREATH1_TR2_SEL_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TR_SEL,
			ISINK_BREATH1_TR2_SEL_MASK << ISINK_BREATH1_TR2_SEL_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK1_BREATH1_TR_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("set ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_breath_tf1(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > 15 || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	value = time << ISINK_BREATH1_TF1_SEL_SHIFT;
	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TR_SEL,
			ISINK_BREATH1_TF1_SEL_MASK << ISINK_BREATH1_TF1_SEL_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK1_BREATH1_TR_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("set ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, value);

		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}


	return ret;
}

static int mt6357_led_set_breath_tf2(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > 15 || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	value = time << ISINK_BREATH1_TF2_SEL_SHIFT;
	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TR_SEL,
			ISINK_BREATH1_TF2_SEL_MASK << ISINK_BREATH1_TF2_SEL_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK1_BREATH1_TR_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TR_SEL);
			return -1;
		}
		pr_info("set ISINK_BREATH1_TR_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TR_SEL, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_breath_ton(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > ISINK_BREATH1_TON_SEL_MASK || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	value = time << ISINK_BREATH1_TON_SEL_SHIFT;
	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TON_SEL,
			ISINK_BREATH1_TON_SEL_MASK << ISINK_BREATH1_TON_SEL_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TON_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TON_SEL);
			return -1;
		}
		pr_info("set ISINK_BREATH1_TON_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TON_SEL, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

static int mt6357_led_set_breath_toff(struct mt_led_info *info, int time)
{
	struct mt6357_led *led = container_of(info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	if (time > ISINK_BREATH1_TOFF_SEL_MASK || time < 0) {
		dev_notice(led->parent->dev, "%s: Input %d is out of range.\n", __func__, time);
		return -EINVAL;
	}

	//Breath Time
	value = time << ISINK_BREATH1_TOFF_SEL_SHIFT;
	switch (led->id) {
	case MT6357_ISINK1:
		ret = regmap_update_bits(regmap, ISINK_BREATH1_TON_SEL,
			ISINK_BREATH1_TOFF_SEL_MASK << ISINK_BREATH1_TOFF_SEL_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_BREATH1_TON_SEL Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_BREATH1_TON_SEL);
			return -1;
		}
		pr_info("set ISINK_BREATH1_TON_SEL[0x%0x]: 0x%0x", ISINK_BREATH1_TON_SEL, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}

	return ret;
}

struct mt_led_ops mt6357_led_ops = {
	.change_mode = &mt6357_led_change_mode,
	.get_current_step = &mt6357_led_get_current_step,
	.set_current_step = &mt6357_led_set_current_step,
	.get_pwm_dim_duty = &mt6357_led_get_pwm_dim_duty,
	.set_pwm_dim_duty = &mt6357_led_set_pwm_dim_duty,
	.get_pwm_dim_freq = &mt6357_led_get_pwm_dim_freq,
	.set_pwm_dim_freq = &mt6357_led_set_pwm_dim_freq,
	.get_breath_tr1 = &mt6357_led_get_breath_tr1,
	.get_breath_tr2 = &mt6357_led_get_breath_tr2,
	.get_breath_tf1 = &mt6357_led_get_breath_tf1,
	.get_breath_tf2 = &mt6357_led_get_breath_tf2,
	.get_breath_ton = &mt6357_led_get_breath_ton,
	.get_breath_toff = &mt6357_led_get_breath_toff,
	.set_breath_tr1 = &mt6357_led_set_breath_tr1,
	.set_breath_tr2 = &mt6357_led_set_breath_tr2,
	.set_breath_tf1 = &mt6357_led_set_breath_tf1,
	.set_breath_tf2 = &mt6357_led_set_breath_tf2,
	.set_breath_ton = &mt6357_led_set_breath_ton,
	.set_breath_toff = &mt6357_led_set_breath_toff,
	.list_pwm_duty = &mt6357_led_list_pwm_duty,
	.list_pwm_freq = &mt6357_led_list_pwm_freq,
};

/*
 * Setup current output for the corresponding
 * brightness level.
 */
static int mt6357_led_hw_brightness(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	struct regmap *regmap = leds->regmap;
	int ret;
	unsigned int value;

	//PMIC Duty
	switch (led->id) {
	case MT6357_ISINK1:
		value = brightness << ISINK_DIM1_DUTY_SHIFT;
		ret = regmap_update_bits(regmap, ISINK_DIM1_DUTY,
			ISINK_DIM1_DUTY_MASK << ISINK_DIM1_DUTY_SHIFT, value);
		if (ret < 0) {
			dev_notice(led->parent->dev, "%s: ISINK_DIM1_DUTY Reg(0x%x) Write ERROR.\n",
				__func__, ISINK_DIM1_DUTY);
			return -1;
		}
		pr_info("set ISINK_DIM1_DUTY[0x%0x]: 0x%0x", ISINK_DIM1_DUTY, value);
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT.\n", __func__, led->id);
		break;
	}


	return 0;
}

static int mt6357_led_hw_off(struct led_classdev *cdev)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);

	dev_dbg(led->parent->dev, "%s: PMIC LED(%d) disable.\n",
		__func__, led->id);

	//PMIC ISINK disable
	return mt6357_led_set_ISINK(cdev, false);
}

static enum led_brightness
mt6357_get_led_hw_brightness(struct led_classdev *cdev)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	unsigned int status_CTL0, status_CTL1;
	int value;
	int ret;

	mt6357_led_get_ISINK(cdev, &status_CTL0, &status_CTL1);

	switch (led->id) {
	case MT6357_ISINK1:
		if (!(status_CTL0 & ISINK_CHOP1_EN_MASK))
			return 0;

		if (!(status_CTL1 & ISINK_CH1_EN_MASK))
			return 0;
		break;
	case MT6357_ISINK2:
	case MT6357_ISINK3:
	default:
		dev_notice(led->parent->dev, "%s: ISINK%d NOT SUPPORT SET MODE.\n",
			__func__, led->id);
		break;
	}
	ret = mt6357_led_get_pwm_dim_duty(l_info, &value);
	if (ret < 0)
		return ret;

	return  value;
}

static int mt6357_led_hw_on(struct led_classdev *cdev,
			    enum led_brightness brightness)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	int ret = 0;

	dev_dbg(led->parent->dev, "%s: PMIC LED(%d) enable.\n",
		__func__, led->id);

	//PMIC ISINK enable
	mt6357_led_set_ISINK(cdev, true);

	ret = mt6357_led_hw_brightness(cdev, brightness);
	if (ret < 0)
		return ret;

	return 0;
}

static void mt6357_led_set_brightness(struct led_classdev *cdev,
				     enum led_brightness brightness)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	struct mt6357_leds *leds = led->parent;
	int ret;

	mutex_lock(&leds->lock);

	if (!led->current_brightness && brightness) {
		ret = mt6357_led_hw_on(cdev, brightness);
		if (ret < 0)
			goto out;
	} else if (brightness) {
		ret = mt6357_led_hw_brightness(cdev, brightness);
		if (ret < 0)
			goto out;
	} else {
		ret = mt6357_led_hw_off(cdev);
		if (ret < 0)
			goto out;
	}

	led->current_brightness = brightness;
out:
	mutex_unlock(&leds->lock);
}

static int mt6357_led_set_blink(struct led_classdev *cdev,
				unsigned long *delay_on,
				unsigned long *delay_off)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	unsigned long period;
	int duty;
	int freq;
	int precision = 1000;
	int ret = 0;

	//We do not care about delay on
	dev_info(led->parent->dev, "%s: delay_on = %lu, delay_off=%lu\n",
		__func__, *delay_on, *delay_off);

	/*
	 * Units are in ms, if over the hardware able
	 * to support, fallback into software blink
	 */
	if ((*delay_on) < 0 || (*delay_off) < 0) {
		dev_notice(led->parent->dev, "%s: delay_on (%lu) or delay_off (%lu) is invalid value.\n",
			__func__, *delay_on, *delay_off);
		return -EINVAL;
	}

	/*
	 * LED subsystem requires a default user
	 * friendly blink pattern for the LED so using
	 * 1Hz duty cycle 50% here if without specific
	 * value delay_on and delay off being assigned.
	 */
	if (!(*delay_on) && !(*delay_off)) {
		*delay_on = 500;
		*delay_off = 500;
	}

	period = (*delay_on) + (*delay_off);
	if (period > mt6357_MAX_PERIOD) {
		dev_notice(led->parent->dev,
			"%s: delay_on + delay_off = %lu is invalid value.\n",
			__func__, period);
		return -EINVAL;
	}
	dev_info(led->parent->dev, "%s: period = %lu\n", __func__, period);

	//change mode to PWM
	ret = mt6357_led_change_mode(cdev, MT_LED_PWM_MODE);
	if (ret < 0) {
		dev_notice(led->parent->dev,
			"%s: mt6357_led_change_mode (%d) ERROR.\n",
			__func__, MT_LED_PWM_MODE);
		goto error;
	}

	//duty is the ratio between 1~256
	duty = precision*256*(*delay_on) / period;
	duty /= precision;
	duty = duty - 1; //0~255
	dev_info(led->parent->dev, "%s: Duty = 0x%0x\n", __func__, duty);
	ret = mt6357_led_hw_brightness(cdev, duty);
	if (ret < 0) {
		dev_notice(led->parent->dev,
			"%s: mt6357_led_hw_brightness (%d) ERROR.\n",
			__func__, duty);
		goto error;
	}

	//freq=(period/2) -1 , unit of period is ms
	freq = ((period/2)-1 > 0) ? ((period/2)-1) : 0;
	dev_info(led->parent->dev, "%s: Frequency = 0x%0x\n", __func__, freq);
	ret = mt6357_led_set_pwm_dim_freq(l_info, freq);
	if (ret < 0) {
		dev_notice(led->parent->dev,
			"%s: mt6357_led_set_pwm_dim_freq (%d) ERROR.\n",
			__func__, freq);
		goto error;
	}
	return 0;

error:
	//disable LED
	mt6357_led_set_ISINK(cdev, false);
	return -EIO;
}

static int mt6357_led_set_dt_default(struct led_classdev *cdev,
				     struct device_node *np)
{
	struct mt_led_info *l_info = (struct mt_led_info *)cdev;
	struct mt6357_led *led = container_of(l_info, struct mt6357_led, l_info);
	const char *state;
	int ret = 0;

	dev_info(led->parent->dev, "mt6357 parse led start\n");

	led->l_info.cdev.name = of_get_property(np, "label", NULL) ? : np->name;
	led->l_info.cdev.default_trigger = of_get_property(np,
		"linux,default-trigger",
		NULL);
	state = of_get_property(np, "default-state", NULL);
	if (state) {
		if (!strcmp(state, "keep")) {
			ret = mt6357_get_led_hw_brightness(cdev);
			if (ret < 0)
				return ret;
			led->current_brightness = ret;
			ret = 0;
		} else if (!strcmp(state, "on")) {
			mt6357_led_set_brightness(cdev, cdev->max_brightness);
		} else  {
			mt6357_led_set_brightness(cdev, LED_OFF);
		}
	}
	pr_info("mt6357 parse led[%d]: %s, %s, %s\n",
		led->id, led->l_info.cdev.name, state, led->l_info.cdev.default_trigger);

	return ret;
}

static int mt6357_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct mt6357_leds *leds;
	struct mt6357_led *led;
	struct mt6397_chip *pmic_chip = dev_get_drvdata(pdev->dev.parent);
	int ret;
	u32 reg;
	u32 step;

	dev_info(&pdev->dev, "mt6357 led probe\n");

	leds = devm_kzalloc(dev, sizeof(*leds), GFP_KERNEL);
	if (!leds)
		return -ENOMEM;

	platform_set_drvdata(pdev, leds);
	leds->dev = dev;
	leds->regmap = pmic_chip->regmap;

	/*
	 * leds->hw points to the underlying bus for the register
	 * controlled.
	 */
	if (!leds->regmap) {
		dev_notice(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}
	mutex_init(&leds->lock);

	ret = regmap_write(leds->regmap, RG_DRV_ISINK_CK_PDN,
		~(RG_DRV_128K_CK_PDN_MASK << RG_DRV_128K_CK_PDN_SHIFT));
	if (ret < 0)
		return ret;

	for_each_available_child_of_node(np, child) {
		ret = of_property_read_u32(child, "reg", &reg);
		if (ret) {
			dev_notice(dev, "Failed to read led 'reg' property\n");
			goto put_child_node;
		}
		ret = of_property_read_u32(child, "step", &step);
		if (!ret)
			dev_info(dev, "read led 'step' property: %d\n", step);
		else
			step = ISINK_CH1_STEP_MAX;

		if (reg >= MT6357_ISINK_MAX || leds->led[reg]) {
			dev_notice(dev, "Invalid led reg %u\n", reg);
			ret = -EINVAL;
			goto put_child_node;
		}

		led = devm_kzalloc(dev, sizeof(*led), GFP_KERNEL);
		if (!led) {
			ret = -ENOMEM;
			goto put_child_node;
		}

		leds->led[reg] = led;
		leds->led[reg]->id = reg;
		leds->led[reg]->l_info.cdev.max_brightness = mt6357_MAX_BRIGHTNESS;
		leds->led[reg]->l_info.cdev.brightness_set =
					mt6357_led_set_brightness;
		leds->led[reg]->l_info.cdev.blink_set = mt6357_led_set_blink;
		leds->led[reg]->l_info.cdev.brightness_get =
					mt6357_get_led_hw_brightness;
		leds->led[reg]->l_info.magic_code = MT_LED_ALL_MAGIC_CODE;
		leds->led[reg]->l_info.ops = &mt6357_led_ops;
		leds->led[reg]->parent = leds;

		ret = mt6357_led_set_dt_default(&leds->led[reg]->l_info.cdev, child);
		if (ret < 0) {
			dev_notice(leds->dev,
				"Failed to parse LED[%d] node from devicetree\n", reg);
			goto put_child_node;
		}

		ret = devm_led_classdev_register(dev, &leds->led[reg]->l_info.cdev);
		if (ret) {
			dev_notice(&pdev->dev, "Failed to register LED: %d\n",
				ret);
			goto put_child_node;
		}
		leds->led[reg]->l_info.cdev.dev->of_node = child;

		//check operations and register trigger
		mt_led_trigger_register(&mt6357_led_ops);

		//clock ON
		mt6357_led_set_clock(&leds->led[reg]->l_info.cdev, true);

		//default PWM mode
		mt6357_led_change_mode(&leds->led[reg]->l_info.cdev, MT_LED_PWM_MODE);

		//default PWM step
		mt6357_led_set_current_step(&leds->led[reg]->l_info, step);
		leds->led[reg]->step = step;

		//ISINK OFF
		mt6357_led_hw_off(&leds->led[reg]->l_info.cdev);
	}
	pr_info("mt6357 led end!");

	return 0;

put_child_node:
	of_node_put(child);
	return ret;
}

static int mt6357_led_remove(struct platform_device *pdev)
{
	struct mt6357_leds *leds = platform_get_drvdata(pdev);
	int i;

	/* Turn the LEDs off on driver removal. */
	for (i = 0 ; leds->led[i] ; i++) {
		//ISINK disable
		mt6357_led_hw_off(&leds->led[i]->l_info.cdev);

		//clock OFF
		mt6357_led_set_clock(&leds->led[i]->l_info.cdev, false);
	}

	mutex_destroy(&leds->lock);

	return 0;
}

static const struct of_device_id mt6357_led_dt_match[] = {
	{ .compatible = "mediatek,mt6357_leds" },
	{},
};

MODULE_DEVICE_TABLE(of, mt6357_led_dt_match);

static struct platform_driver mt6357_led_driver = {
	.probe		= mt6357_led_probe,
	.remove		= mt6357_led_remove,
	.driver		= {
		.name	= "leds-mt6357",
		.of_match_table = mt6357_led_dt_match,
	},
};

static int __init mt6357_leds_init(void)
{
	int ret;

	pr_info("Leds init");
	ret = platform_driver_register(&mt6357_led_driver);

	if (ret) {
		pr_info("driver register error: %d", ret);
		return ret;
	}

	return ret;
}

static void __exit mt6357_leds_exit(void)
{
	platform_driver_unregister(&mt6357_led_driver);
}

module_init(mt6357_leds_init);
module_exit(mt6357_leds_exit);

//module_platform_driver(mt6357_led_driver);

MODULE_DESCRIPTION("LED driver for Mediatek mt6357 PMIC");
MODULE_AUTHOR("Mediatek Corporation");
MODULE_LICENSE("GPL");
