/*
 * TI LMU (Lighting Management Unit) Backlight Device Data
 *
 * Copyright 2016 Texas Instruments
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-backlight.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>

/* LM3532 */
static u32 lm3532_init_regs[] = {
	LM3532_INIT_ZONE_0,
	LM3532_INIT_ZONE_1,
	LM3532_INIT_ZONE_2,
};

static u32 lm3532_channel_regs[] = {
	LM3532_CHANNEL_1,
	LM3532_CHANNEL_2,
	LM3532_CHANNEL_3,
};

static u32 lm3532_mode_regs[] = {
	LM3532_MODE_PWM_A,
	LM3532_MODE_PWM_B,
	LM3532_MODE_PWM_C,
};

static u32 lm3532_ramp_regs[] = {
	LM3532_RAMPUP,
	LM3532_RAMPDN,
};

static u8 lm3532_enable_reg = LM3532_REG_ENABLE;

static u8 lm3532_brightness_regs[] = {
	LM3532_REG_BRT_A,
	LM3532_REG_BRT_B,
	LM3532_REG_BRT_C,
};

static const struct ti_lmu_bl_reg lm3532_reg_info = {
	.init		= lm3532_init_regs,
	.num_init	= ARRAY_SIZE(lm3532_init_regs),
	.channel	= lm3532_channel_regs,
	.mode		= lm3532_mode_regs,
	.ramp		= lm3532_ramp_regs,
	.enable		= &lm3532_enable_reg,
	.brightness_msb	= lm3532_brightness_regs,
};

/* LM36272 and LM36274 */
static u32 lm3627x_init_regs[] = {
	LM3627X_INIT_OVP_25V,
	LM3627X_INIT_SWFREQ_1MHZ,
};

static u32 lm36272_channel_regs[]  = {
	LM36272_SINGLE_CHANNEL,
	LM36272_DUAL_CHANNEL,
};

static u32 lm36274_channel_regs[]  = {
	LM36274_CHANNEL_1,
	LM36274_CHANNEL_2,
	LM36274_CHANNEL_3,
	LM36274_CHANNEL_4,
};

static u32 lm3627x_mode_reg = LM3627X_MODE_PWM;
static u8 lm3627x_enable_reg = LM3627X_REG_ENABLE;
static u8 lm3627x_brightness_msb_reg = LM3627X_REG_BRT_MSB;
static u8 lm3627x_brightness_lsb_reg = LM3627X_REG_BRT_LSB;

static const struct ti_lmu_bl_reg lm36272_reg_info = {
	.init		= lm3627x_init_regs,
	.num_init	= ARRAY_SIZE(lm3627x_init_regs),
	.channel	= lm36272_channel_regs,
	.mode		= &lm3627x_mode_reg,
	.enable		= &lm3627x_enable_reg,
	.enable_offset	= LM3627X_BL_ENABLE_OFFSET,
	.brightness_msb	= &lm3627x_brightness_msb_reg,
	.brightness_lsb	= &lm3627x_brightness_lsb_reg,
};

static const struct ti_lmu_bl_reg lm36274_reg_info = {
	.init		= lm3627x_init_regs,
	.num_init	= ARRAY_SIZE(lm3627x_init_regs),
	.channel	= lm36274_channel_regs,
	.mode		= &lm3627x_mode_reg,
	.enable		= &lm3627x_enable_reg,
	.enable_offset	= LM3627X_BL_ENABLE_OFFSET,
	.brightness_msb	= &lm3627x_brightness_msb_reg,
	.brightness_lsb	= &lm3627x_brightness_lsb_reg,
};

/* LM3631 */
static u32 lm3631_init_regs[] = {
	LM3631_INIT_BRT_MODE,
	LM3631_INIT_DIMMING_MODE,
};

static u32 lm3631_channel_regs[]  = {
	LM3631_SINGLE_CHANNEL,
	LM3631_DUAL_CHANNEL,
};

static u32 lm3631_ramp_reg = LM3631_RAMP;
static u8 lm3631_enable_reg = LM3631_REG_DEVCTRL;
static u8 lm3631_brightness_msb_reg = LM3631_REG_BRT_MSB;
static u8 lm3631_brightness_lsb_reg = LM3631_REG_BRT_LSB;

static const struct ti_lmu_bl_reg lm3631_reg_info = {
	.init		= lm3631_init_regs,
	.num_init	= ARRAY_SIZE(lm3631_init_regs),
	.channel	= lm3631_channel_regs,
	.ramp		= &lm3631_ramp_reg,
	.enable		= &lm3631_enable_reg,
	.brightness_msb	= &lm3631_brightness_msb_reg,
	.brightness_lsb	= &lm3631_brightness_lsb_reg,
};

/* LM3632 */
static u32 lm3632_init_regs[] = {
	LM3632_INIT_OVP_25V,
	LM3632_INIT_SWFREQ_1MHZ,
};

static u32 lm3632_channel_regs[]  = {
	LM3632_SINGLE_CHANNEL,
	LM3632_DUAL_CHANNEL,
};

static u32 lm3632_mode_reg = LM3632_MODE_PWM;
static u8 lm3632_enable_reg = LM3632_REG_ENABLE;
static u8 lm3632_brightness_msb_reg = LM3632_REG_BRT_MSB;
static u8 lm3632_brightness_lsb_reg = LM3632_REG_BRT_LSB;

static const struct ti_lmu_bl_reg lm3632_reg_info = {
	.init		= lm3632_init_regs,
	.num_init	= ARRAY_SIZE(lm3632_init_regs),
	.channel	= lm3632_channel_regs,
	.mode		= &lm3632_mode_reg,
	.enable		= &lm3632_enable_reg,
	.brightness_msb	= &lm3632_brightness_msb_reg,
	.brightness_lsb	= &lm3632_brightness_lsb_reg,
};

/* LM3633 */
static u32 lm3633_init_regs[] = {
	LM3633_INIT_OVP_40V,
	LM3633_INIT_RAMP_SELECT,
};

static u32 lm3633_channel_regs[]  = {
	LM3633_CHANNEL_HVLED1,
	LM3633_CHANNEL_HVLED2,
	LM3633_CHANNEL_HVLED3,
};

static u32 lm3633_mode_regs[] = {
	LM3633_MODE_PWM_A,
	LM3633_MODE_PWM_B,
};

static u32 lm3633_ramp_regs[] = {
	LM3633_RAMPUP,
	LM3633_RAMPDN,
};

static u8 lm3633_enable_reg = LM3633_REG_ENABLE;

static u8 lm3633_brightness_msb_regs[] = {
	LM3633_REG_BRT_HVLED_A_MSB,
	LM3633_REG_BRT_HVLED_B_MSB,
};

static u8 lm3633_brightness_lsb_regs[] = {
	LM3633_REG_BRT_HVLED_A_LSB,
	LM3633_REG_BRT_HVLED_B_LSB,
};

static const struct ti_lmu_bl_reg lm3633_reg_info = {
	.init		 = lm3633_init_regs,
	.num_init	 = ARRAY_SIZE(lm3633_init_regs),
	.channel	 = lm3633_channel_regs,
	.mode		 = lm3633_mode_regs,
	.ramp		 = lm3633_ramp_regs,
	.ramp_reg_offset = 1, /* For LM3633_REG_BL1_RAMPUP/DN */
	.enable		 = &lm3633_enable_reg,
	.brightness_msb	 = lm3633_brightness_msb_regs,
	.brightness_lsb	 = lm3633_brightness_lsb_regs,
};

/* LM3695 */
static u32 lm3695_init_regs[] = {
	LM3695_INIT_BRT_MODE,
};

static u32 lm3695_channel_regs[]  = {
	LM3695_SINGLE_CHANNEL,
	LM3695_DUAL_CHANNEL,
};

static u8 lm3695_enable_reg = LM3695_REG_GP;
static u8 lm3695_brightness_msb_reg = LM3695_REG_BRT_MSB;
static u8 lm3695_brightness_lsb_reg = LM3695_REG_BRT_LSB;

static const struct ti_lmu_bl_reg lm3695_reg_info = {
	.init		= lm3695_init_regs,
	.num_init	= ARRAY_SIZE(lm3695_init_regs),
	.channel	= lm3695_channel_regs,
	.enable		= &lm3695_enable_reg,
	.enable_usec	= 600,
	.brightness_msb	= &lm3695_brightness_msb_reg,
	.brightness_lsb	= &lm3695_brightness_lsb_reg,
};

/* LM3697 */
static u32 lm3697_init_regs[] = {
	//LM3697_INIT_RAMP_SELECT,
	LM3697_REG_HVLED_OUTPUT_CFG,
	LM3697_REG_BOOST_CFG,
	LM3697_REG_PWM_CFG,
	LM3697_REG_BRT_B_LSB,
	LM3697_REG_BRT_B_MSB,
	LM3697_REG_ENABLE,
};

static u32 lm3697_channel_regs[]  = {
	LM3697_CHANNEL_1,
	LM3697_CHANNEL_2,
	LM3697_CHANNEL_3,
};

static u32 lm3697_mode_regs[] = {
	LM3697_MODE_PWM_B,
};

static u32 lm3697_ramp_regs[] = {
	LM3697_RAMPUP,
	LM3697_RAMPDN,
};

static u8 lm3697_enable_reg = LM3697_REG_ENABLE;

static u8 lm3697_brightness_msb_regs[] = {
	LM3697_REG_BRT_B_MSB,
};

static u8 lm3697_brightness_lsb_regs[] = {
	LM3697_REG_BRT_B_LSB,
};

static const struct ti_lmu_bl_reg lm3697_reg_info = {
	.init		 = lm3697_init_regs,
	.num_init	 = ARRAY_SIZE(lm3697_init_regs),
	.channel	 = lm3697_channel_regs,
	.mode		 = lm3697_mode_regs,
	.ramp		 = lm3697_ramp_regs,
	.ramp_reg_offset = 1, /* For LM3697_REG_BL1_RAMPUP/DN */
	.enable		 = &lm3697_enable_reg,
	.brightness_msb	 = lm3697_brightness_msb_regs,
	.brightness_lsb	 = lm3697_brightness_lsb_regs,
};

static int lm3532_ramp_table[] = { 0, 1, 2, 4, 8, 16, 32, 65 };

static int lm3631_ramp_table[] = {
	   0,   1,   2,    5,   10,   20,   50,  100,
	 250, 500, 750, 1000, 1500, 2000, 3000, 4000,
};

static int common_ramp_table[] = {
	   2, 250, 500, 1000, 2000, 4000, 8000, 16000,
};

struct ti_lmu_bl_cfg lmu_bl_cfg[LMU_MAX_ID] = {
	{
		.reginfo		= &lm3532_reg_info,
		.num_channels		= LM3532_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_8BIT,
		.pwm_action		= UPDATE_PWM_AND_BRT_REGISTER,
		.ramp_table		= lm3532_ramp_table,
		.size_ramp		= ARRAY_SIZE(lm3532_ramp_table),
	},
	{
		.reginfo		= &lm36272_reg_info,
		.num_channels		= LM36272_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_ONLY,
	},
	{
		.reginfo		= &lm36274_reg_info,
		.num_channels		= LM36274_MAX_CHANNELS,
		.single_bank_used	= true,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_ONLY,
	},
	{
		.reginfo		= &lm3631_reg_info,
		.num_channels		= LM3631_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_ONLY,
		.ramp_table		= lm3631_ramp_table,
		.size_ramp		= ARRAY_SIZE(lm3631_ramp_table),
	},
	{
		.reginfo		= &lm3632_reg_info,
		.num_channels		= LM3632_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_ONLY,
	},
	{
		.reginfo		= &lm3633_reg_info,
		.num_channels		= LM3633_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_MAX_BRT,
		.ramp_table		= common_ramp_table,
		.size_ramp		= ARRAY_SIZE(common_ramp_table),
		.fault_monitor_used	= true,
	},
	{
		.reginfo		= &lm3695_reg_info,
		.num_channels		= LM3695_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_AND_BRT_REGISTER,
	},
	{
		.reginfo		= &lm3697_reg_info,
		.num_channels		= LM3697_MAX_CHANNELS,
		.max_brightness		= MAX_BRIGHTNESS_11BIT,
		.pwm_action		= UPDATE_PWM_AND_BRT_REGISTER,
		.ramp_table		= common_ramp_table,
		.size_ramp		= ARRAY_SIZE(common_ramp_table),
		.fault_monitor_used	= true,
	},
};
EXPORT_SYMBOL_GPL(lmu_bl_cfg);
