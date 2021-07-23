/*
 *  drivers/misc/mediatek/pmic/mt6360/mt6360_pmu_rgbled.c
 *  Driver for MT6360 PMU RGBLed part
 *
 *  Copyright (C) 2018 Mediatek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/workqueue.h>

#include "../inc/mt6360_pmu.h"
#include "../inc/mt6360_pmu_rgbled.h"
#include "../inc/mt_led_trigger.h"

struct mt6360_pmu_rgbled_info {
	struct mt_led_info l_info; /* most be the first member */
	struct device *dev;
	struct mt6360_pmu_info *mpi;
	int index;
	struct delayed_work dwork;
};

static const struct mt6360_rgbled_platform_data def_platform_data = {
};

static struct mt6360_pmu_irq_desc mt6360_pmu_rgbled_irq_desc[] = {
};

static void mt6360_pmu_rgbled_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_rgbled_irq_desc); i++) {
		irq_desc = mt6360_pmu_rgbled_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
};

static int mt6360_rgbled_apply_pdata(struct mt6360_pmu_rgbled_info *mpri,
		struct mt6360_rgbled_platform_data *pdata)
{
	int ret;

	dev_dbg(mpri->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpri->mpi, pdata, mt6360_pdata_props,
			ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpri->dev, "%s ++\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
};

static int mt6360_rgbled_parse_dt_data(struct device *dev,
				      struct mt6360_rgbled_platform_data *pdata)
{
	int ret = 0;
	int name_cnt = 0, trigger_cnt = 0;
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
		mt6360_val_props, ARRAY_SIZE(mt6360_val_props));

	while (true) {
		const char *name = NULL;

		ret = of_property_read_string_index(np, "mt,led_name",
			name_cnt, &name);
		if (ret < 0)
			break;
		pdata->led_name[name_cnt] = name;
		name_cnt++;
	}

	while (true) {
		const char *name = NULL;

		ret = of_property_read_string_index(np,
			"mt,led_default_trigger", trigger_cnt, &name);
		if (ret < 0)
			break;
		pdata->led_default_trigger[trigger_cnt] = name;
		trigger_cnt++;

	}
	if (name_cnt != MT6360_LED_MAX || trigger_cnt != MT6360_LED_MAX)
		return -EINVAL;

	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

struct rgbled_reg_val {
	uint8_t reg_addr;
	uint8_t reg_val;
};

#define RGBLED_REG_VAL(val, addr)  \
	{                          \
		.reg_addr = addr,  \
		.reg_val = val,    \
	}

static const struct rgbled_reg_val rgbled_init_data[] = {
	RGBLED_REG_VAL(0x08,  MT6360_PMU_RGB_EN),
	RGBLED_REG_VAL(0xca,  MT6360_PMU_RGB1_ISNK),
	RGBLED_REG_VAL(0xca,  MT6360_PMU_RGB2_ISNK),
	RGBLED_REG_VAL(0xca,  MT6360_PMU_RGB3_ISNK),
	RGBLED_REG_VAL(0x8c,  MT6360_PMU_RGB_ML_ISNK),
	RGBLED_REG_VAL(0x1f,  MT6360_PMU_RGB1_DIM),
	RGBLED_REG_VAL(0x1f,  MT6360_PMU_RGB2_DIM),
	RGBLED_REG_VAL(0x1f,  MT6360_PMU_RGB3_DIM),
	RGBLED_REG_VAL(0x48,  MT6360_PMU_RGB12_Freq),
	RGBLED_REG_VAL(0x40,  MT6360_PMU_RGB34_Freq),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB1_Tr),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB1_Tf),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB1_TON_TOFF),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB2_Tr),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB2_Tf),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB2_TON_TOFF),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB3_Tr),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB3_Tf),
	RGBLED_REG_VAL(0x11,  MT6360_PMU_RGB3_TON_TOFF),
};

static int mt6360_rgbled_init_register(struct mt6360_pmu_rgbled_info *mpri)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(rgbled_init_data); i++) {
		ret |= mt6360_pmu_reg_write(mpri->mpi,
			rgbled_init_data[i].reg_addr,
			rgbled_init_data[i].reg_val);
	}

	if (ret)
		return ret;
	return 0;
}

static inline int mt6360_led_get_index(struct led_classdev *led)
{
	struct mt6360_pmu_rgbled_info *led_info =
		(struct mt6360_pmu_rgbled_info *)led;

	return led_info->index;
}

static int mt6360_rgbled_change_mode(struct led_classdev *led, int mode)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(led->dev->parent);
	int led_index = mt6360_led_get_index(led);
	uint8_t reg_addr = 0;
	int ret = 0;
	int mode_val = 0;

	if (mode >= MT_LED_MODE_MAX)
		return -EINVAL;
	dev_info(rgbled_info->dev, "%s mode = %s\n",
		__func__, mt_led_trigger_mode_name[mode]);
	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_ISNK;
		ret = mt6360_pmu_reg_set_bits(rgbled_info->mpi,
			MT6360_PMU_RGB_EN, MT6360_LED1_SWMODE_MASK);
		if (ret < 0)
			return ret;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_ISNK;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_ISNK;
		break;
	case MT6360_LED_MOONLIGHT:
		/* Only CC Mode */
		dev_dbg(rgbled_info->dev,
			"%s led MoonLight only cc mode\n", __func__);
		return 0;
	}

	switch (mode) {
	case MT_LED_CC_MODE:
		mode_val = 2;
		break;
	case MT_LED_PWM_MODE:
		mode_val = 0;
		break;
	case MT_LED_BREATH_MODE:
		mode_val = 1;
		break;
	}


	ret = mt6360_pmu_reg_update_bits(rgbled_info->mpi, reg_addr,
			MT6360_LED_MODEMASK, mode_val << MT6360_LED_MODESHFT);

	return 0;
}

static int mt6360_rgbled_get_soft_start_step(struct mt_led_info *info)
{
	dev_dbg(info->led.dev, "MT6360 Not Support\n");
	return 0;
}

static int mt6360_rgbled_set_soft_start_step(struct mt_led_info *info, int ns)
{
	dev_dbg(info->led.dev, "MT6360 Not Support\n");
	return 0;
}

static int mt6360_rgbled_list_pwm_duty(struct mt_led_info *info, char *buf)
{
	snprintf(buf, PAGE_SIZE, "%s\n", "0~255");
	return 0;
}

static int mt6360_rgbled_get_pwm_dim_duty(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0;
	int ret = 0;

	dev_dbg(rgbled_info->dev, "%s idx = %d\n", __func__, led_index);
	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_DIM;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_DIM;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_DIM;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;

	ret = (ret & MT6360_LED_PWMDUTYMASK) >> MT6360_LED_PWMDUTYSHFT;
	return ret;
}

static int mt6360_rgbled_set_pwm_dim_duty(struct mt_led_info *info, int duty)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_data = 0;
	int ret = 0;

	if (duty > 255 || duty < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_DIM;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_DIM;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_DIM;
		break;
	default:
		return 0;
	}

	reg_data = duty << MT6360_LED_PWMDUTYSHFT;
	ret = mt6360_pmu_reg_update_bits(rgbled_info->mpi, reg_addr,
				MT6360_LED_PWMDUTYMASK, reg_data);
	if (ret < 0)
		return ret;
	return 0;
}

static int mt6360_pwm_freq[8] = {
	125,
	250,
	500,
	1000,
	2000,
	4000,
	128000,
	256000,
};

static int mt6360_rgbled_list_pwm_freq(struct mt_led_info *info, char *buf)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6360_pwm_freq); i++) {
		snprintf(buf+strlen(buf), PAGE_SIZE,
			"sel %d: %d.%dHz\n", i,
			mt6360_pwm_freq[i]/1000, mt6360_pwm_freq[i]%1000);
	}
	return 0;
}

static int mt6360_rgbled_get_pwm_dim_freq(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift = 0;
	uint8_t reg_data = 0;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED1_PWMFREQMASK;
		reg_shift = MT6360_LED1_PWMFREQSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED2_PWMFREQMASK;
		reg_shift = MT6360_LED2_PWMFREQSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB34_Freq;
		reg_mask = MT6360_LED3_PWMFREQMASK;
		reg_shift = MT6360_LED3_PWMFREQSHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_pwm_freq[reg_data];
}

static int mt6360_rgbled_set_pwm_dim_freq(
		struct mt_led_info *info, int freq)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift = 0;
	uint8_t reg_data = 0;
	int ret = 0;

	if (freq > 7 || freq < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED1_PWMFREQMASK;
		reg_shift = MT6360_LED1_PWMFREQSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED2_PWMFREQMASK;
		reg_shift = MT6360_LED2_PWMFREQSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB34_Freq;
		reg_mask = MT6360_LED3_PWMFREQMASK;
		reg_shift = MT6360_LED3_PWMFREQSHFT;
		break;
	default:
		return 0;
	}

	reg_data = freq << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static const int mt6360_breath_time[] = {
	125, 375, 625, 875,
	1125, 1375, 1625, 1875,
	2125, 2375, 2625, 2875,
	3125, 3375, 3625, 3875,
};

static int mt6360_rgbled_list_breath_time(struct mt_led_info *info, char *buf)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6360_breath_time); i++) {
		snprintf(buf+strlen(buf), PAGE_SIZE, "sel %d: %d.%dms\n", i,
			mt6360_breath_time[i]/1000, mt6360_breath_time[i]%1000);
	}
	return 0;
}

static int mt6360_rgbled_get_breath_tr1(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data = 0;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_get_breath_tr2(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_get_breath_tf1(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_get_breath_tf2(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_get_breath_ton(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_get_breath_toff(struct mt_led_info *info)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift;
	uint8_t reg_data;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	default:
		return 0;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (ret & reg_mask) >> reg_shift;

	return mt6360_breath_time[reg_data];
}

static int mt6360_rgbled_set_breath_tr1(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tr;
		reg_mask = MT6360_LED_TR1MASK;
		reg_shift = MT6360_LED_TR1SHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static int mt6360_rgbled_set_breath_tr2(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tr;
		reg_mask = MT6360_LED_TR2MASK;
		reg_shift = MT6360_LED_TR2SHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static int mt6360_rgbled_set_breath_tf1(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tf;
		reg_mask = MT6360_LED_TF1MASK;
		reg_shift = MT6360_LED_TF1SHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static int mt6360_rgbled_set_breath_tf2(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_Tf;
		reg_mask = MT6360_LED_TF2MASK;
		reg_shift = MT6360_LED_TF2SHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static int mt6360_rgbled_set_breath_ton(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_TON_TOFF;
		reg_mask = MT6360_LED_TONMASK;
		reg_shift = MT6360_LED_TONSHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

static int mt6360_rgbled_set_breath_toff(struct mt_led_info *info, int time)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(info->led.dev->parent);
	int led_index = mt6360_led_get_index(&info->led);
	uint8_t reg_addr = 0, reg_mask = 0, reg_shift, reg_data;
	int ret = 0;

	dev_dbg(rgbled_info->dev,
		"%s led_index = %d, time = %d\n", __func__, led_index, time);
	if (time > 16 || time < 0)
		return -EINVAL;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_TON_TOFF;
		reg_mask = MT6360_LED_TOFFMASK;
		reg_shift = MT6360_LED_TOFFSHFT;
		break;
	default:
		return 0;
	}

	reg_data = time << reg_shift;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, reg_data);
	return ret;
}

struct mt_led_ops mt6360_led_ops = {
	.change_mode = &mt6360_rgbled_change_mode,
	.get_soft_start_step = &mt6360_rgbled_get_soft_start_step,
	.set_soft_start_step = &mt6360_rgbled_set_soft_start_step,
	.get_pwm_dim_duty = &mt6360_rgbled_get_pwm_dim_duty,
	.set_pwm_dim_duty = &mt6360_rgbled_set_pwm_dim_duty,
	.get_pwm_dim_freq = &mt6360_rgbled_get_pwm_dim_freq,
	.set_pwm_dim_freq = &mt6360_rgbled_set_pwm_dim_freq,
	.get_breath_tr1 = &mt6360_rgbled_get_breath_tr1,
	.get_breath_tr2 = &mt6360_rgbled_get_breath_tr2,
	.get_breath_tf1 = &mt6360_rgbled_get_breath_tf1,
	.get_breath_tf2 = &mt6360_rgbled_get_breath_tf2,
	.get_breath_ton = &mt6360_rgbled_get_breath_ton,
	.get_breath_toff = &mt6360_rgbled_get_breath_toff,
	.set_breath_tr1 = &mt6360_rgbled_set_breath_tr1,
	.set_breath_tr2 = &mt6360_rgbled_set_breath_tr2,
	.set_breath_tf1 = &mt6360_rgbled_set_breath_tf1,
	.set_breath_tf2 = &mt6360_rgbled_set_breath_tf2,
	.set_breath_ton = &mt6360_rgbled_set_breath_ton,
	.set_breath_toff = &mt6360_rgbled_set_breath_toff,
	.list_pwm_duty = &mt6360_rgbled_list_pwm_duty,
	.list_pwm_freq = &mt6360_rgbled_list_pwm_freq,
	.list_breath_time = &mt6360_rgbled_list_breath_time,

};

static inline void mt6360_led_enable_dwork(struct led_classdev *led)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(led->dev->parent);

	cancel_delayed_work_sync(&rgbled_info->dwork);
	schedule_delayed_work(&rgbled_info->dwork, msecs_to_jiffies(100));
}

static void mt6360_led_bright_set(
	struct led_classdev *led, enum led_brightness bright)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(led->dev->parent);
	int led_index = mt6360_led_get_index(led);
	uint8_t reg_addr = 0, reg_mask = 0xf, reg_shift = 0, en_mask = 0;
	uint8_t cur_level = 0;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_ISNK;
		en_mask = 0x80;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_ISNK;
		en_mask = 0x40;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_ISNK;
		en_mask = 0x20;
		break;
	case MT6360_LED_MOONLIGHT:
		reg_addr = MT6360_PMU_RGB_ML_ISNK;
		en_mask = 0x10;
		reg_mask = 0x1f;
		break;
	}
	if (bright && led_index == MT6360_LED_1)
		cur_level = (bright & reg_mask)- 1;
	else
		cur_level = (bright & reg_mask);

	ret = mt6360_pmu_reg_update_bits(rgbled_info->mpi, reg_addr,
			reg_mask, cur_level << reg_shift);
	if (ret < 0) {
		dev_err(led->dev, "update brightness fail\n");
		return;
	}

	if (!bright) {
		ret = mt6360_pmu_reg_update_bits(
			rgbled_info->mpi, MT6360_PMU_RGB_EN,
			en_mask, ~en_mask);
		if (ret < 0)
			dev_err(led->dev, "update enable bit fail\n");
	} else
		mt6360_led_enable_dwork(led);
}

static enum led_brightness mt6360_led_bright_get(struct led_classdev *led)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(led->dev->parent);
	int led_index = mt6360_led_get_index(led);
	uint8_t reg_addr = 0, reg_mask = 0xf, reg_shift = 0, en_mask = 0;
	int ret = 0;

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_ISNK;
		en_mask = 0x80;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_ISNK;
		en_mask = 0x40;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_ISNK;
		en_mask = 0x20;
		break;
	case MT6360_LED_MOONLIGHT:
		reg_addr = MT6360_PMU_RGB_ML_ISNK;
		en_mask = 0x10;
		reg_mask = 0x1f;
		break;
	}

	ret = mt6360_pmu_reg_read(rgbled_info->mpi, MT6360_PMU_RGB_EN);
	if (ret < 0)
		return ret;
	if (!(ret & en_mask))
		return LED_OFF;
	ret = mt6360_pmu_reg_read(rgbled_info->mpi, reg_addr);
	if (ret < 0)
		return ret;
	return (ret & reg_mask) >> reg_shift;
}

static inline int mt6360_led_config_pwm(struct led_classdev *led,
	unsigned long *delay_on, unsigned long *delay_off)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
				dev_get_drvdata(led->dev->parent);
	const ulong dim_time[] = {8000, 4000, 2000, 1000, 500, 250, 7, 3};
	const unsigned long ton = *delay_on, toff = *delay_off;
	int led_index = mt6360_led_get_index(led);
	int reg_addr, reg_mask, reg_shift;
	int i = 0, ret = 0, j = 0;

	dev_dbg(led->dev, "%s, on %lu, off %lu\n", __func__, ton, toff);

	for (i = ARRAY_SIZE(dim_time)-1; i >= 0; i--) {
		if (dim_time[i] >= (ton + toff))
			break;
	}
	if (i < 0) {
		dev_warn(led->dev, "no match, sum %lu\n", ton + toff);
		i = 0;
	}

	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED1_PWMFREQMASK;
		reg_shift = MT6360_LED1_PWMFREQSHFT;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB12_Freq;
		reg_mask = MT6360_LED2_PWMFREQMASK;
		reg_shift = MT6360_LED2_PWMFREQSHFT;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB34_Freq;
		reg_mask = MT6360_LED3_PWMFREQMASK;
		reg_shift = MT6360_LED3_PWMFREQSHFT;
		break;
	default:
		return 0;
	}
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, i << reg_shift);
	if (ret < 0)
		return ret;

	/* find the closet pwm duty */
	j = 32 * ton / (ton + toff);
	if (j == 0)
		j = 1;
	j--;
	switch (led_index) {
	case MT6360_LED_1:
		reg_addr = MT6360_PMU_RGB1_DIM;
		break;
	case MT6360_LED_2:
		reg_addr = MT6360_PMU_RGB2_DIM;
		break;
	case MT6360_LED_3:
		reg_addr = MT6360_PMU_RGB3_DIM;
		break;
	default:
		return -EINVAL;
	}

	reg_mask = MT6360_LED_PWMDUTYMASK;
	reg_shift = MT6360_LED_PWMDUTYSHFT;
	ret = mt6360_pmu_reg_update_bits(
		rgbled_info->mpi, reg_addr, reg_mask, j << reg_shift);
	if (ret < 0)
		return ret;
	return 0;
}

static int mt6360_led_blink_set(struct led_classdev *led,
	unsigned long *delay_on, unsigned long *delay_off)
{
	int mode_sel = MT_LED_PWM_MODE;
	int led_index = mt6360_led_get_index(led);
	int ret = 0;

	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;
	if (!*delay_off || led_index == MT6360_LED_MOONLIGHT)
		mode_sel = MT_LED_CC_MODE;

	if (mode_sel == MT_LED_PWM_MODE) {
		ret = mt6360_led_config_pwm(led, delay_on, delay_off);
		if (ret < 0) {
			dev_err(led->dev, "%s: cfg pwm fail\n", __func__);
			return ret;
		}
	}

	ret = mt6360_rgbled_change_mode(led, mode_sel);
	if (ret < 0)
		dev_err(led->dev, "%s: change mode fail\n", __func__);

	return ret;
}

static struct mt6360_pmu_rgbled_info mt6360_led_info[MT6360_LED_MAX] = {
	{
		.l_info = {
			.led = {
				.max_brightness = 13,
				.brightness_set = mt6360_led_bright_set,
				.brightness_get = mt6360_led_bright_get,
				.blink_set = mt6360_led_blink_set,
			},
			.magic_code = MT_LED_ALL_MAGIC_CODE,
		},
		.index = MT6360_LED_1,
	},
	{
		.l_info = {
			.led = {
				.max_brightness = 12,
				.brightness_set = mt6360_led_bright_set,
				.brightness_get = mt6360_led_bright_get,
				.blink_set = mt6360_led_blink_set,
			},
			.magic_code = MT_LED_ALL_MAGIC_CODE,
		},
		.index = MT6360_LED_2,
	},
	{
		.l_info = {
			.led = {
				.max_brightness = 12,
				.brightness_set = mt6360_led_bright_set,
				.brightness_get = mt6360_led_bright_get,
				.blink_set = mt6360_led_blink_set,
			},
			.magic_code = MT_LED_ALL_MAGIC_CODE,
		},
		.index = MT6360_LED_3,
	},
	{
		.l_info = {
			.led = {
				.max_brightness = 30,
				.brightness_set = mt6360_led_bright_set,
				.brightness_get = mt6360_led_bright_get,
				.blink_set = mt6360_led_blink_set,
			},
			.magic_code = MT_LED_CC_MAGIC_CODE,
		},
		.index = MT6360_LED_MOONLIGHT,
	},
};

static void mt6360_led_enable_dwork_func(struct work_struct *work)
{
	struct mt6360_pmu_rgbled_info *rgbled_info =
		container_of(work, struct mt6360_pmu_rgbled_info, dwork.work);
	uint8_t reg_data = 0, reg_mask = 0xf0;
	int ret = 0;

	dev_dbg(rgbled_info->dev, "%s\n", __func__);
	/* led 1 */
	if (mt6360_led_info[0].l_info.led.brightness != 0)
		reg_data |= 0x80;
	if (mt6360_led_info[1].l_info.led.brightness != 0)
		reg_data |= 0x40;
	if (mt6360_led_info[2].l_info.led.brightness != 0)
		reg_data |= 0x20;
	if (mt6360_led_info[3].l_info.led.brightness != 0)
		reg_data |= 0x10;

	ret = mt6360_pmu_reg_update_bits(rgbled_info->mpi, MT6360_PMU_RGB_EN,
					reg_mask, reg_data);
	if (ret < 0)
		dev_err(rgbled_info->dev, "timer update enable bit fail\n");
}

static ssize_t mt6360_rgbled_disable_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	buf[0] = '\0';
	snprintf(buf, PAGE_SIZE, "%s\n",
		"echo 1 > disable : it will set brightness = 0");
	return strlen(buf);
}

static ssize_t mt6360_rgbled_disable_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct mt6360_pmu_rgbled_info *mpri = dev_get_drvdata(dev);
	struct led_classdev *led = &mpri->l_info.led;
	unsigned long val = 0;
	int ret = 0;

	if (!led)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val)
		led_set_brightness(led, LED_OFF);

	return cnt;
}

static const struct device_attribute mt6360_rgbled_attrs[] = {
	__ATTR(disable, 0644,
		mt6360_rgbled_disable_read, mt6360_rgbled_disable_write),
};


static int mt6360_pmu_rgbled_probe(struct platform_device *pdev)
{
	struct mt6360_rgbled_platform_data *pdata =
			dev_get_platdata(&pdev->dev);
	struct mt6360_pmu_rgbled_info *mpri;
	bool use_dt = pdev->dev.of_node;
	int ret = 0, i = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (use_dt) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_rgbled_parse_dt_data(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "parse dt fail\n");
			return ret;
		}
		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mpri = devm_kzalloc(&pdev->dev, sizeof(*mpri), GFP_KERNEL);
	if (!mpri)
		return -ENOMEM;
	mpri->dev = &pdev->dev;
	mpri->mpi = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, mpri);
	INIT_DELAYED_WORK(&mpri->dwork, mt6360_led_enable_dwork_func);

	ret = mt6360_rgbled_init_register(mpri);
	if (ret < 0) {
		dev_err(mpri->dev, "mt6360 rgbled init register fail\n");
		return -EINVAL;
	}

	ret = mt_led_trigger_register(&mt6360_led_ops);
	if (ret < 0) {
		dev_err(mpri->dev, "mt6360 rgbled trigger register fail\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mt6360_led_info); i++) {
		mt6360_led_info[i].l_info.led.name = pdata->led_name[i];
		mt6360_led_info[i].l_info.led.default_trigger =
						pdata->led_default_trigger[i];
		mt6360_led_info[i].l_info.ops = &mt6360_led_ops;
		ret = led_classdev_register(&pdev->dev,
				&mt6360_led_info[i].l_info.led);
		if (ret < 0) {
			dev_err(&pdev->dev, "register led %d fail\n", i);
			goto out_led_register;
		}
		ret = device_create_file(mt6360_led_info[i].l_info.led.dev,
				mt6360_rgbled_attrs);
		if (ret < 0) {
			dev_err(&pdev->dev, "register led disable fail\n");
			goto out_led_disable;
		}
	}

	/* apply platform data */
	ret = mt6360_rgbled_apply_pdata(mpri, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "apply pdata fail\n");
		return ret;
	}
	/* irq register */
	mt6360_pmu_rgbled_irq_register(pdev);
	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;

out_led_disable:
	while (--i >= 0) {
		device_remove_file(mt6360_led_info[i].l_info.led.dev,
			mt6360_rgbled_attrs);
		led_classdev_unregister(&mt6360_led_info[i].l_info.led);
	}
	mt_led_trigger_unregister();

	return -EINVAL;

out_led_register:
	while (--i >= 0)
		led_classdev_unregister(&mt6360_led_info[i].l_info.led);

	mt_led_trigger_unregister();

	return -EINVAL;
}

static int mt6360_pmu_rgbled_remove(struct platform_device *pdev)
{
	struct mt6360_pmu_rgbled_info *mpri = platform_get_drvdata(pdev);
	int i = 0;

	dev_dbg(mpri->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mt6360_led_info); i++) {
		led_classdev_unregister(&mt6360_led_info[i].l_info.led);
		device_remove_file(mt6360_led_info[i].l_info.led.dev,
			mt6360_rgbled_attrs);
	}
	mt_led_trigger_unregister();

	dev_info(mpri->dev, "%s successfullt\n", __func__);
	return 0;
}

static int __maybe_unused mt6360_pmu_rgbled_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_rgbled_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_rgbled_pm_ops,
			 mt6360_pmu_rgbled_suspend, mt6360_pmu_rgbled_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_rgbled_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_rgbled", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_rgbled_of_id);

static const struct platform_device_id mt6360_pmu_rgbled_id[] = {
	{ "mt6360_pmu_rgbled", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_rgbled_id);

static struct platform_driver mt6360_pmu_rgbled_driver = {
	.driver = {
		.name = "mt6360_pmu_rgbled",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_rgbled_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_rgbled_of_id),
	},
	.probe = mt6360_pmu_rgbled_probe,
	.remove = mt6360_pmu_rgbled_remove,
	.id_table = mt6360_pmu_rgbled_id,
};
module_platform_driver(mt6360_pmu_rgbled_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU RGBLED Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
