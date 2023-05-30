/*
 * bq28z610 fuel gauge driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __BQ28Z16_H
#define __BQ28Z16_H

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/alarmtimer.h>
#include <linux/kernel.h>
#include <linux/i2c.h> 

#define RANDOM_CHALLENGE_LEN_MAX	32
#define RANDOM_CHALLENGE_LEN_BQ27Z561	32
#define RANDOM_CHALLENGE_LEN_BQ28Z610	20
#define FG_DEVICE_CHEM_LEN_MAX  10

#define FG_MONITOR_DELAY_3S		3000
#define FG_MONITOR_DELAY_5S		5000
#define FG_MONITOR_DELAY_10S		10000
#define FG_MONITOR_DELAY_30S		30000

#define BQ_REPORT_FULL_SOC	9800
#define BQ_CHARGE_FULL_SOC	9750
#define BQ_RECHARGE_SOC		9800
#define BQ_DEFUALT_FULL_SOC	100

#define HW_REPORT_FULL_SOC 9500

enum bq_fg_device_name {
	BQ_FG_UNKNOWN,
	BQ_FG_BQ27Z561,
	BQ_FG_BQ28Z610,
	BQ_FG_NFG1000A,
	BQ_FG_NFG1000B,
};

enum bq_fg_reg_idx {
	BQ_FG_REG_CTRL = 0,
	BQ_FG_REG_TEMP,		/* Battery Temperature */
	BQ_FG_REG_VOLT,		/* Battery Voltage */
	BQ_FG_REG_CN,		/* Current Now */
	BQ_FG_REG_AI,		/* Average Current */
	BQ_FG_REG_BATT_STATUS,	/* BatteryStatus */
	BQ_FG_REG_TTE,		/* Time to Empty */
	BQ_FG_REG_TTF,		/* Time to Full */
	BQ_FG_REG_FCC,		/* Full Charge Capacity */
	BQ_FG_REG_RM,		/* Remaining Capacity */
	BQ_FG_REG_CC,		/* Cycle Count */
	BQ_FG_REG_SOC,		/* Relative State of Charge */
	BQ_FG_REG_SOH,		/* State of Health */
	BQ_FG_REG_CHG_VOL,	/* Charging Voltage*/
	BQ_FG_REG_CHG_CUR,	/* Charging Current*/
	BQ_FG_REG_DC,		/* Design Capacity */
	BQ_FG_REG_ALT_MAC,	/* AltManufactureAccess*/
	BQ_FG_REG_MAC_DATA,	/* MACData*/
	BQ_FG_REG_MAC_CHKSUM,	/* MACChecksum */
	BQ_FG_REG_MAC_DATA_LEN,	/* MACDataLen */
	NUM_REGS,
};

static u8 bq_fg_regs[NUM_REGS] = {
	0x00,	/* CONTROL */
	0x06,	/* TEMP */
	0x08,	/* VOLT */
	0x0C,	/* CURRENT NOW */
	0x14,	/* AVG CURRENT */
	0x0A,	/* FLAGS */
	0x16,	/* Time to empty */
	0x18,	/* Time to full */
	0x12,	/* Full charge capacity */
	0x10,	/* Remaining Capacity */
	0x2A,	/* CycleCount */
	0x2C,	/* State of Charge */
	0x2E,	/* State of Health */
	0x30,	/* Charging Voltage*/
	0x32,	/* Charging Current*/
	0x3C,	/* Design Capacity */
	0x3E,	/* AltManufacturerAccess*/
	0x40,	/* MACData*/
	0x60,	/* MACChecksum */
	0x61,	/* MACDataLen */
};

enum bq_fg_mac_cmd {
	FG_MAC_CMD_CTRL_STATUS	= 0x0000,
	FG_MAC_CMD_DEV_TYPE	= 0x0001,
	FG_MAC_CMD_FW_VER	= 0x0002,
	FG_MAC_CMD_HW_VER	= 0x0003,
	FG_MAC_CMD_IF_SIG	= 0x0004,
	FG_MAC_CMD_CHEM_ID	= 0x0006,
	FG_MAC_CMD_SHUTDOWN	= 0x0010,
	FG_MAC_CMD_GAUGING	= 0x0021,
	FG_MAC_CMD_SEAL		= 0x0030,
	FG_MAC_CMD_FASTCHARGE_EN = 0x003E,
	FG_MAC_CMD_FASTCHARGE_DIS = 0x003F,
	FG_MAC_CMD_DEV_RESET	= 0x0041,
	FG_MAC_CMD_DEVICE_NAME	= 0x004A,
	FG_MAC_CMD_DEVICE_CHEM	= 0x004B,
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_LIFETIME1	= 0x0060,
	FG_MAC_CMD_LIFETIME3	= 0x0062,
	FG_MAC_CMD_DASTATUS1	= 0x0071,
	FG_MAC_CMD_ITSTATUS1	= 0x0073,
	FG_MAC_CMD_QMAX		= 0x0075,
	FG_MAC_CMD_FCC_SOH	= 0x0077,
	FG_MAC_CMD_RA_TABLE	= 0x40C0,
};

struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
	struct mutex data_lock;

	u8 regs[NUM_REGS];
	char model_name[I2C_NAME_SIZE];
	char log_tag[I2C_NAME_SIZE];
	char device_chem[FG_DEVICE_CHEM_LEN_MAX];
	int device_name;
	bool chip_ok;
	bool batt_fc;
	struct wakeup_source *bms_wakelock;
	int monitor_delay;
	int ui_soc;
	int rsoc;
	int raw_soc;
	int fcc;
	int rm;
	int dc;
	int soh;
	u16 qmax[2];
	u16 cell_voltage[3];
	bool fast_chg;
	int vbat;
	int tbat;
	int ibat;
	int charge_current;
	int charge_voltage;
	int cycle_count;
	int fake_cycle_count;
	int last_soc;

	int fake_soc;
	int fake_tbat;
	int i2c_error_count;
	int fake_i2c_error_count;

	struct	delayed_work monitor_work;
	struct power_supply *fg_psy;
	struct power_supply *batt_psy;
	struct power_supply_desc fg_psy_d;

	u8 digest[RANDOM_CHALLENGE_LEN_MAX];
	bool authenticate;
	bool	update_now;
	int	optimiz_soc;
	bool	ffc_smooth;
	int	*dec_rate_seq;
	int	dec_rate_len;

	int	max_chg_power_120w;
	int	report_full_rsoc;
	int	soc_gap;
	int	normal_shutdown_vbat;
	int	critical_shutdown_vbat;
	int	cool_critical_shutdown_vbat;
	int	old_critical_shutdown_vbat;
	bool	shutdown_delay;
	bool	fake_shutdown_delay_enable;
	bool	enable_shutdown_delay;
	bool	shutdown_flag;
	bool	shutdown_mode;
};

#define BMS_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, bms_sysfs_show, bms_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}
#define BMS_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, bms_sysfs_show, bms_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}
enum bms_property {
	BMS_PROP_FASTCHARGE_MODE,
	BMS_PROP_MONITOR_DELAY,
	BMS_PROP_FCC,
	BMS_PROP_RM,
	BMS_PROP_RSOC,
	BMS_PROP_SHUTDOWN_DELAY,
	BMS_PROP_CAPACITY_RAW,
	BMS_PROP_SOC_DECIMAL,
	BMS_PROP_SOC_DECIMAL_RATE,
	BMS_PROP_RESISTANCE_ID,
	BMS_PROP_AUTHENTIC,
	BMS_PROP_SHUTDOWN_MODE,
	BMS_PROP_CHIP_OK,
	BMS_PROP_CHARGE_DONE,
	BMS_PROP_SOH,
	BMS_PROP_RESISTANCE,
	BMS_PROP_I2C_ERROR_COUNT,
	BMS_PROP_AV_CURRENT,
	BMS_PROP_RAW_SOC,
};

struct mtk_bms_sysfs_field_info {
	struct device_attribute attr;
	enum bms_property prop;
	int (*set)(struct bq_fg_chip *gm,
		struct mtk_bms_sysfs_field_info *attr, int val);
	int (*get)(struct bq_fg_chip *gm,
		struct mtk_bms_sysfs_field_info *attr, int *val);
};

extern int bms_get_property(enum bms_property bp, int *val);
extern int bms_set_property(enum bms_property bp, int val);

#endif /* __PMIC_VOTER_H */

