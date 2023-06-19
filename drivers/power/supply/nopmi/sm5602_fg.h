/*
 * drivers/battery/sm5602_fg.h
 *
 * Copyright (C) 2018 SiliconMitus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef SM5602_FG_H
#define SM5602_FG_H

#define FG_INIT_MARK				0xA000

#define FG_PARAM_UNLOCK_CODE	  		0x3700
#define FG_PARAM_LOCK_CODE	  		0x0000
#define FG_TABLE_LEN				0x18//real table length -1
#define FG_ADD_TABLE_LEN			0x8//real table length -1
#define FG_INIT_B_LEN		    		0x7//real table length -1
#define FG_TABLE_MAX_LEN			0x18//real table length -1

#define ENABLE_EN_TEMP_IN           0x0200
#define ENABLE_EN_TEMP_EX           0x0400
#define ENABLE_EN_BATT_DET          0x0800
#define ENABLE_IOCV_MAN_MODE        0x1000
#define ENABLE_FORCED_SLEEP         0x2000
#define ENABLE_SLEEPMODE_EN         0x4000
#define ENABLE_SHUTDOWN             0x8000

/* REG */
#define FG_REG_SOC_CYCLE			0x0B
#define FG_REG_SOC_CYCLE_CFG		0x15
#define FG_REG_BATT_ID			0x1F
#define FG_REG_ALPHA             	0x20
#define FG_REG_BETA              	0x21
#define FG_REG_RS                	0x24
#define FG_REG_RS_1     			0x25
#define FG_REG_RS_2            		0x26
#define FG_REG_RS_3            		0x27
#define FG_REG_RS_0            		0x29
#define FG_REG_END_V_IDX			0x2F
#define FG_REG_START_LB_V			0x30
#define FG_REG_START_CB_V			0x38
#define FG_REG_START_LB_I			0x40
#define FG_REG_START_CB_I			0x48
#define FG_REG_VOLT_CAL				0x70
#define FG_REG_CURR_IN_OFFSET		0x75
#define FG_REG_CURR_IN_SLOPE		0x76
#define FG_REG_RMC					0x84

#define FG_REG_SRADDR				0x8C
#define FG_REG_SRDATA				0x8D
#define FG_REG_SWADDR				0x8E
#define FG_REG_SWDATA				0x8F

#define FG_REG_AGING_CTRL			0x9C

#define FG_TEMP_TABLE_CNT_MAX       0x65

#define FG_PARAM_VERION       		0x1E

#define INIT_CHECK_MASK         	0x0010
#define DISABLE_RE_INIT         	0x0010

#define I2C_ERROR_COUNT_MAX			0x5

enum {
	BATTERY_VENDOR_START = 0,
	BATTERY_VENDOR_GY = 1,
	BATTERY_VENDOR_XWD = 2,
	BATTERY_VENDOR_NVT = 3,
	BATTERY_VENDOR_UNKNOWN = 4
};//Please be consistent with the sm5602_fg.h 

//2021.09.06 wsy edit for remove irq
#define FG_REMOVE_IRQ	1

#endif /* SM5602_FG_H */
