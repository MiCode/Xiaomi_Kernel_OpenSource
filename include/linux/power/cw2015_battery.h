/*
 * Gas_Gauge driver for CW2015/2013
 * Copyright (C) 2012, CellWise
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on
 * I2C and uses IIC bus for communication with the host.
 *
 */

#ifndef __LINUX_CW2015_BATTERY_H__
#define __LINUX_CW2015_BATTERY_H__

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SIZE_BATINFO 64



struct cw_bat_platform_data {
	int (*io_init)(void);

	int is_usb_charge;
	int is_dc_charge;
	u8 *cw_bat_config_info;
	u32 irq_flags;
	u32 dc_det_pin;
	u32 dc_det_level;

	u32 bat_low_pin;
	u32 bat_low_level;
	u32 chg_ok_pin;
	u32 chg_ok_level;
	u32 chg_mode_sel_pin;
	u32 chg_mode_sel_level;
};

#endif

