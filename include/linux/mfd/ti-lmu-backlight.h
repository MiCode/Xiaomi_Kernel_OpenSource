/*
 * TI LMU (Lighting Management Unit) Backlight Common Driver
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

#ifndef __TI_LMU_BACKLIGHT_H__
#define __TI_LMU_BACKLIGHT_H__

#include <linux/backlight.h>
#include <linux/device.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/notifier.h>

/**
 * LMU backlight register data
 *	value[23:16] | mask[15:8] | address[7:0]
 */
#define LMU_BL_REG(addr, mask, value)				\
	((value << 16) | (mask << 8) | addr)

#define LMU_BL_GET_ADDR(x)	(x & 0xFF)
#define LMU_BL_GET_MASK(x)	((x >> 8) & 0xFF)
#define LMU_BL_GET_VAL(x)	((x >> 16) & 0xFF)

#define LM3532_INIT_ZONE_0						\
	LMU_BL_REG(LM3532_REG_ZONE_CFG_A, LM3532_ZONE_MASK, LM3532_ZONE_0)
#define LM3532_INIT_ZONE_1						\
	LMU_BL_REG(LM3532_REG_ZONE_CFG_B, LM3532_ZONE_MASK, LM3532_ZONE_1)
#define LM3532_INIT_ZONE_2						\
	LMU_BL_REG(LM3532_REG_ZONE_CFG_C, LM3532_ZONE_MASK, LM3532_ZONE_2)
#define LM3532_CHANNEL_1						\
	LMU_BL_REG(LM3532_REG_OUTPUT_CFG, LM3532_ILED1_CFG_MASK,	\
	LM3532_ILED1_CFG_SHIFT)
#define LM3532_CHANNEL_2						\
	LMU_BL_REG(LM3532_REG_OUTPUT_CFG, LM3532_ILED2_CFG_MASK,	\
	LM3532_ILED2_CFG_SHIFT)
#define LM3532_CHANNEL_3						\
	LMU_BL_REG(LM3532_REG_OUTPUT_CFG, LM3532_ILED3_CFG_MASK,	\
	LM3532_ILED3_CFG_SHIFT)
#define LM3532_MODE_PWM_A						\
	LMU_BL_REG(LM3532_REG_PWM_A_CFG, LM3532_PWM_A_MASK, LM3532_PWM_ZONE_0)
#define LM3532_MODE_PWM_B						\
	LMU_BL_REG(LM3532_REG_PWM_B_CFG, LM3532_PWM_B_MASK, LM3532_PWM_ZONE_1)
#define LM3532_MODE_PWM_C						\
	LMU_BL_REG(LM3532_REG_PWM_C_CFG, LM3532_PWM_C_MASK, LM3532_PWM_ZONE_2)
#define LM3532_RAMPUP							\
	LMU_BL_REG(LM3532_REG_RAMPUP, LM3532_RAMPUP_MASK, LM3532_RAMPUP_SHIFT)
#define LM3532_RAMPDN							\
	LMU_BL_REG(LM3532_REG_RAMPDN, LM3532_RAMPDN_MASK, LM3532_RAMPDN_SHIFT)

#define LM3627X_INIT_OVP_25V						\
	LMU_BL_REG(LM3627X_REG_CONFIG1, LM3627X_OVP_MASK, LM3627X_OVP_25V)
#define LM3627X_INIT_SWFREQ_1MHZ					\
	LMU_BL_REG(LM3627X_REG_CONFIG2, LM3627X_SWFREQ_MASK, LM3627X_SWFREQ_1MHZ)
#define LM36272_SINGLE_CHANNEL						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36272_BL_CHANNEL_MASK,		\
	LM36272_BL_SINGLE_CHANNEL)
#define LM36272_DUAL_CHANNEL						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36272_BL_CHANNEL_MASK,		\
	LM36272_BL_DUAL_CHANNEL)
#define LM36274_CHANNEL_1						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36274_BL_CHANNEL_1,		\
	LM36274_BL_CHANNEL_1)
#define LM36274_CHANNEL_2						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36274_BL_CHANNEL_2,		\
	LM36274_BL_CHANNEL_2)
#define LM36274_CHANNEL_3						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36274_BL_CHANNEL_3,		\
	LM36274_BL_CHANNEL_3)
#define LM36274_CHANNEL_4						\
	LMU_BL_REG(LM3627X_REG_ENABLE, LM36274_BL_CHANNEL_4,		\
	LM36274_BL_CHANNEL_4)
#define LM3627X_MODE_PWM						\
	LMU_BL_REG(LM3627X_REG_CONFIG1, LM3627X_PWM_MASK, LM3627X_PWM_MODE)

#define LM3631_INIT_BRT_MODE						\
	LMU_BL_REG(LM3631_REG_BRT_MODE, LM3631_MODE_MASK, LM3631_DEFAULT_MODE)
#define LM3631_INIT_DIMMING_MODE					\
	LMU_BL_REG(LM3631_REG_BL_CFG, LM3631_MAP_MASK, LM3631_EXPONENTIAL_MAP)
#define LM3631_SINGLE_CHANNEL						\
	LMU_BL_REG(LM3631_REG_BL_CFG, LM3631_BL_CHANNEL_MASK,		\
	LM3631_BL_SINGLE_CHANNEL)
#define LM3631_DUAL_CHANNEL						\
	LMU_BL_REG(LM3631_REG_BL_CFG, LM3631_BL_CHANNEL_MASK,		\
	LM3631_BL_DUAL_CHANNEL)
#define LM3631_RAMP							\
	LMU_BL_REG(LM3631_REG_SLOPE, LM3631_SLOPE_MASK, LM3631_SLOPE_SHIFT)

#define LM3632_INIT_OVP_25V						\
	LMU_BL_REG(LM3632_REG_CONFIG1, LM3632_OVP_MASK, LM3632_OVP_25V)
#define LM3632_INIT_SWFREQ_1MHZ						\
	LMU_BL_REG(LM3632_REG_CONFIG2, LM3632_SWFREQ_MASK, LM3632_SWFREQ_1MHZ)
#define LM3632_SINGLE_CHANNEL						\
	LMU_BL_REG(LM3632_REG_ENABLE, LM3632_BL_CHANNEL_MASK,		\
	LM3632_BL_SINGLE_CHANNEL)
#define LM3632_DUAL_CHANNEL						\
	LMU_BL_REG(LM3632_REG_ENABLE, LM3632_BL_CHANNEL_MASK,		\
	LM3632_BL_DUAL_CHANNEL)
#define LM3632_MODE_PWM							\
	LMU_BL_REG(LM3632_REG_IO_CTRL, LM3632_PWM_MASK, LM3632_PWM_MODE)

#define LM3633_INIT_OVP_40V						\
	LMU_BL_REG(LM3633_REG_BOOST_CFG, LM3633_OVP_MASK, LM3633_OVP_40V)
#define LM3633_INIT_RAMP_SELECT						\
	LMU_BL_REG(LM3633_REG_BL_RAMP_CONF, LM3633_BL_RAMP_MASK,	\
	LM3633_BL_RAMP_EACH)
#define LM3633_CHANNEL_HVLED1						\
	LMU_BL_REG(LM3633_REG_HVLED_OUTPUT_CFG, LM3633_HVLED1_CFG_MASK,	\
	LM3633_HVLED1_CFG_SHIFT)
#define LM3633_CHANNEL_HVLED2						\
	LMU_BL_REG(LM3633_REG_HVLED_OUTPUT_CFG, LM3633_HVLED2_CFG_MASK,	\
	LM3633_HVLED2_CFG_SHIFT)
#define LM3633_CHANNEL_HVLED3						\
	LMU_BL_REG(LM3633_REG_HVLED_OUTPUT_CFG, LM3633_HVLED3_CFG_MASK,	\
	LM3633_HVLED3_CFG_SHIFT)
#define LM3633_MODE_PWM_A						\
	LMU_BL_REG(LM3633_REG_PWM_CFG, LM3633_PWM_A_MASK, LM3633_PWM_A_MASK)
#define LM3633_MODE_PWM_B						\
	LMU_BL_REG(LM3633_REG_PWM_CFG, LM3633_PWM_B_MASK, LM3633_PWM_B_MASK)
#define LM3633_RAMPUP							\
	LMU_BL_REG(LM3633_REG_BL0_RAMP, LM3633_BL_RAMPUP_MASK,		\
	LM3633_BL_RAMPUP_SHIFT)
#define LM3633_RAMPDN							\
	LMU_BL_REG(LM3633_REG_BL0_RAMP, LM3633_BL_RAMPDN_MASK,		\
	LM3633_BL_RAMPDN_SHIFT)

#define LM3695_INIT_BRT_MODE						\
	LMU_BL_REG(LM3695_REG_GP, LM3695_BRT_RW_MASK, LM3695_BRT_RW_MASK)
#define LM3695_SINGLE_CHANNEL						\
	LMU_BL_REG(LM3695_REG_GP, LM3695_BL_CHANNEL_MASK,		\
	LM3695_BL_SINGLE_CHANNEL)
#define LM3695_DUAL_CHANNEL						\
	LMU_BL_REG(LM3695_REG_GP, LM3695_BL_CHANNEL_MASK,		\
	LM3695_BL_DUAL_CHANNEL)

#define LM3697_INIT_RAMP_SELECT						\
	LMU_BL_REG(LM3697_REG_RAMP_CONF, LM3697_RAMP_MASK, LM3697_RAMP_EACH)
#define LM3697_CHANNEL_1						\
	LMU_BL_REG(LM3697_REG_HVLED_OUTPUT_CFG, LM3697_HVLED1_CFG_MASK,	\
	LM3697_HVLED1_CFG_SHIFT)
#define LM3697_CHANNEL_2						\
	LMU_BL_REG(LM3697_REG_HVLED_OUTPUT_CFG, LM3697_HVLED2_CFG_MASK,	\
	LM3697_HVLED2_CFG_SHIFT)
#define LM3697_CHANNEL_3						\
	LMU_BL_REG(LM3697_REG_HVLED_OUTPUT_CFG, LM3697_HVLED3_CFG_MASK,	\
	LM3697_HVLED3_CFG_SHIFT)
#define LM3697_MODE_PWM_A						\
	LMU_BL_REG(LM3697_REG_PWM_CFG, LM3697_PWM_A_MASK, LM3697_PWM_A_MASK)
#define LM3697_MODE_PWM_B						\
	LMU_BL_REG(LM3697_REG_PWM_CFG, LM3697_PWM_B_MASK, LM3697_PWM_B_MASK)
#define LM3697_RAMPUP							\
	LMU_BL_REG(LM3697_REG_BL0_RAMP, LM3697_RAMPUP_MASK, LM3697_RAMPUP_SHIFT)
#define LM3697_RAMPDN							\
	LMU_BL_REG(LM3697_REG_BL0_RAMP, LM3697_RAMPDN_MASK, LM3697_RAMPDN_SHIFT)

#define LM3532_MAX_CHANNELS		3
#define LM36272_MAX_CHANNELS		2
#define LM36274_MAX_CHANNELS		4
#define LM3631_MAX_CHANNELS		2
#define LM3632_MAX_CHANNELS		2
#define LM3633_MAX_CHANNELS		3
#define LM3695_MAX_CHANNELS		2
#define LM3697_MAX_CHANNELS		3

#define MAX_BRIGHTNESS_8BIT		255
#define MAX_BRIGHTNESS_11BIT		2047

enum ti_lmu_bl_ctrl_mode {
	BL_REGISTER_BASED,
	BL_PWM_BASED,
};

enum ti_lmu_bl_pwm_action {
	/* Update PWM duty, no brightness register update is required */
	UPDATE_PWM_ONLY,
	/* Update not only duty but also brightness register */
	UPDATE_PWM_AND_BRT_REGISTER,
	/* Update max value in brightness registers */
	UPDATE_MAX_BRT,
};

enum ti_lmu_bl_ramp_mode {
	BL_RAMP_UP,
	BL_RAMP_DOWN,
};

struct ti_lmu_bl;
struct ti_lmu_bl_chip;

/**
 * struct ti_lmu_bl_reg
 *
 * @init:		Device initialization registers
 * @num_init:		Numbers of initialization registers
 * @channel:		Backlight channel configuration registers
 * @mode:		Brightness control mode registers
 * @ramp:		Ramp registers for lighting effect
 * @ramp_reg_offset:	Ramp register offset.
 *			Only used for multiple ramp registers.
 * @enable:		Enable control register address
 * @enable_offset:	Enable control register bit offset
 * @enable_usec:	Delay time for updating enable register.
 *			Unit is microsecond.
 * @brightness_msb:	Brightness MSB(Upper 8 bits) registers.
 *			Concatenated with LSB in 11 bit dimming mode.
 *			In 8 bit dimming, only MSB is used.
 * @brightness_lsb:	Brightness LSB(Lower 3 bits) registers.
 *			Only valid in 11 bit dimming mode.
 */
struct ti_lmu_bl_reg {
	u32 *init;
	int num_init;
	u32 *channel;
	u32 *mode;
	u32 *ramp;
	int ramp_reg_offset;
	u8 *enable;
	u8 enable_offset;
	unsigned long enable_usec;
	u8 *brightness_msb;
	u8 *brightness_lsb;
};

/**
 * struct ti_lmu_bl_cfg
 *
 * @reginfo:		Device register configuration
 * @num_channels:	Number of backlight channels
 * @single_bank_used:	[Optional] Set true if one bank controls multiple channels.
 *			Only used for LM36274.
 * @max_brightness:	Max brightness value of backlight device
 * @pwm_action:		How to control brightness registers in PWM mode
 * @ramp_table:		[Optional] Ramp time table for lighting effect.
 *			It's used for searching approximate register index.
 * @size_ramp:		[Optional] Size of ramp table
 * @fault_monitor_used:	[Optional] Set true if the device needs to handle
 *			LMU fault monitor event.
 *
 * This structure is used for device specific data configuration.
 */
struct ti_lmu_bl_cfg {
	const struct ti_lmu_bl_reg *reginfo;
	int num_channels;
	bool single_bank_used;
	int max_brightness;
	enum ti_lmu_bl_pwm_action pwm_action;
	int *ramp_table;
	int size_ramp;
	bool fault_monitor_used;
};

/**
 * struct ti_lmu_bl_chip
 *
 * @dev:		Parent device pointer
 * @lmu:		LMU structure.
 *			Used for register R/W access and notification.
 * @cfg:		Device configuration data
 * @lmu_bl:		Multiple backlight channels
 * @num_backlights:	Number of backlight channels
 * @nb:			Notifier block for handling LMU fault monitor event
 *
 * One backlight chip can have multiple backlight channels, 'ti_lmu_bl'.
 */
struct ti_lmu_bl_chip {
	struct device *dev;
	struct ti_lmu *lmu;
	const struct ti_lmu_bl_cfg *cfg;
	struct ti_lmu_bl *lmu_bl;
	int num_backlights;
	struct notifier_block nb;
};

/**
 * struct ti_lmu_bl
 *
 * @chip:		Pointer to parent backlight device
 * @bl_dev:		Backlight subsystem device structure
 * @bank_id:		Backlight bank ID
 * @name:		Backlight channel name
 * @mode:		Backlight control mode
 * @led_sources:	Backlight output channel configuration.
 *			Bit mask is set on parsing DT.
 * @default_brightness:	[Optional] Initial brightness value
 * @ramp_up_msec:	[Optional] Ramp up time
 * @ramp_down_msec:	[Optional] Ramp down time
 * @pwm_period:		[Optional] PWM period
 * @pwm:		[Optional] PWM subsystem structure
 *
 * Each backlight device has its own channel configuration.
 * For chip control, parent chip data structure is used.
 */
struct ti_lmu_bl {
	struct ti_lmu_bl_chip *chip;
	struct backlight_device *bl_dev;

	int bank_id;
	const char *name;
	enum ti_lmu_bl_ctrl_mode mode;
	unsigned long led_sources;

	unsigned int default_brightness;

	/* Used for lighting effect */
	unsigned int ramp_up_msec;
	unsigned int ramp_down_msec;

	/* Only valid in PWM mode */
	unsigned int pwm_period;
	struct pwm_device *pwm;
};

extern struct ti_lmu_bl_cfg lmu_bl_cfg[LMU_MAX_ID];
#endif
