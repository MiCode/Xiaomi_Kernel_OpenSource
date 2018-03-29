/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Roh¨¢r <pali.rohar@gmail.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 * http://www.ti.com/product/bq27411-g1
 * http://www.ti.com/product/bq27421-g1
 * http://www.ti.com/product/bq27425-g1
 * http://www.ti.com/product/bq27441-g1
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/power/bq27x00_battery.h>
#include <mach/upmu_sw.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <asm/unaligned.h>
#include "mt_battery_meter_hal.h"

#define DRIVER_VERSION			"1.2.0"
#define INVALID_REG_ADDR		0xFF

int gauge_firmware_load_flag = 0;
extern int g_rtc_fg_soc;

enum bq27xxx_reg_index {
	BQ27XXX_REG_CTRL = 0,
	BQ27XXX_REG_TEMP,
	BQ27XXX_REG_INT_TEMP,
	BQ27XXX_REG_VOLT,
	BQ27XXX_REG_AI,
	BQ27XXX_REG_FLAGS,
	BQ27XXX_REG_TTE,
	BQ27XXX_REG_TTF,
	BQ27XXX_REG_TTES,
	BQ27XXX_REG_TTECP,
	BQ27XXX_REG_NAC,
	BQ27XXX_REG_FCC,
	BQ27XXX_REG_CYCT,
	BQ27XXX_REG_AE,
	BQ27XXX_REG_SOC,
	BQ27XXX_REG_DCAP,
	BQ27XXX_POWER_AVG,
	BQ27XXX_REG_UFSOC,
	NUM_REGS
};

/* bq27500 registers */
static __initdata u8 bq27500_regs[NUM_REGS] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0xFF,	/* INT TEMP -NA	*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0x18,	/* TTF		*/
	0x1c,	/* TTES		*/
	0x26,	/* TTECP	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD(FCC)	*/
	0x2A,	/* CYCT		*/
	0x22,	/* AE		*/
	0x2C,	/* SOC(RSOC)	*/
	0x3C,	/* DCAP(ILMD)	*/
	0x24,	/* AP		*/
};

/* bq27520 registers */
static __initdata u8 bq27520_regs[] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0xFF,	/* INT TEMP - NA*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0x18,	/* TTF		*/
	0x1c,	/* TTES		*/
	0x26,	/* TTECP	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD		*/
	0xFF,	/* CYCT - NA	*/
	0x22,	/* AE		*/
	0x20,	/* SOC(RSOC	*/
	0x2E,	/* DCAP(ILMD) - NA */
	0x24,	/* AP		*/
	0x74,	/* UFSOC		*/
};

/* bq2753x registers */
static __initdata u8 bq2753x_regs[] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0xFF,	/* INT TEMP - NA*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0xFF,	/* TTF - NA	*/
	0xFF,	/* TTES - NA	*/
	0xFF,	/* TTECP - NA	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD(FCC)	*/
	0x2A,	/* CYCT		*/
	0xFF,	/* AE - NA	*/
	0x2C,	/* SOC(RSOC)	*/
	0xFF,	/* DCAP(ILMD) - NA */
	0x24,	/* AP		*/
};

/* bq2754x registers */
static __initdata u8 bq2754x_regs[NUM_REGS] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0x28,	/* INT TEMP - NA*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0xFF,	/* TTF - NA	*/
	0xFF,	/* TTES - NA	*/
	0xFF,	/* TTECP - NA	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD(FCC)	*/
	0x2A,	/* CYCT		*/
	0xFF,	/* AE - NA	*/
	0x2C,	/* SOC(RSOC)	*/
	0xFF,	/* DCAP(ILMD) - NA */
	0xFF,	/* AP		*/
};

/* bq27200 registers */
static __initdata u8 bq27200_regs[NUM_REGS] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0xFF,	/* INT TEMP - NA	*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0x18,	/* TTF		*/
	0x1c,	/* TTES		*/
	0x26,	/* TTECP	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD(FCC)	*/
	0x2A,	/* CYCT		*/
	0x22,	/* AE		*/
	0x0B,	/* SOC(RSOC)	*/
	0x76,	/* DCAP(ILMD)	*/
	0x24,	/* AP		*/
};

/* bq274xx registers */
static __initdata u8 bq274xx_regs[NUM_REGS] = {
	0x00,	/* CONTROL	*/
	0x02,	/* TEMP		*/
	0x1e,	/* INT TEMP	*/
	0x04,	/* VOLT		*/
	0x10,	/* AVG CURR	*/
	0x06,	/* FLAGS	*/
	0xFF,	/* TTE - NA	*/
	0xFF,	/* TTF - NA	*/
	0xFF,	/* TTES - NA	*/
	0xFF,	/* TTECP - NA	*/
	0x08,	/* NAC		*/
	0x0E,	/* FCC		*/
	0xFF,	/* CYCT - NA	*/
	0xFF,	/* AE - NA	*/
	0x1C,	/* SOC		*/
	0x3C,	/* DCAP - NA	*/
	0x18,	/* AP		*/
};

/* bq276xx registers - same as bq274xx except CYCT */
static __initdata u8 bq276xx_regs[NUM_REGS] = {
	0x00,	/* CONTROL	*/
	0x02,	/* TEMP		*/
	0x1e,	/* INT TEMP	*/
	0x04,	/* VOLT		*/
	0x10,	/* AVG CURR	*/
	0x06,	/* FLAGS	*/
	0xFF,	/* TTE - NA	*/
	0xFF,	/* TTF - NA	*/
	0xFF,	/* TTES - NA	*/
	0xFF,	/* TTECP - NA	*/
	0x08,	/* NAC		*/
	0x0E,	/* FCC		*/
	0x22,	/* CYCT		*/
	0xFF,	/* AE - NA	*/
	0x1C,	/* SOC		*/
	0x3C,	/* DCAP - NA	*/
	0x18,	/* AP		*/
};

/*
 * SBS Commands for DF access - these are pretty standard
 * So, no need to go in the command array
 */
#define BLOCK_DATA_CLASS		0x3E
#define DATA_BLOCK			0x3F
#define BLOCK_DATA			0x40
#define BLOCK_DATA_CHECKSUM		0x60
#define BLOCK_DATA_CONTROL		0x61

/* bq274xx/bq276xx specific command information */
#define BQ274XX_UNSEAL_KEY		0x80008000
#define BQ274XX_SOFT_RESET		0x43
#define BQ274XX_FLAG_ITPOR				0x20
#define BQ274XX_CTRL_STATUS_INITCOMP	0x80
#define BQ27XXX_FLAG_DSC		BIT(0)
#define BQ27XXX_FLAG_SOCF		BIT(1) /* State-of-Charge threshold final */
#define BQ27XXX_FLAG_SOC1		BIT(2) /* State-of-Charge threshold 1 */
#define BQ27XXX_FLAG_FC		BIT(9)
#define BQ27XXX_FLAG_OTD		BIT(14)
#define BQ27XXX_FLAG_OTC		BIT(15)

/* BQ27000 has different layout for Flags register */
#define BQ27200_FLAG_EDVF		BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27200_FLAG_EDV1		BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27200_FLAG_CI			BIT(4) /* Capacity Inaccurate flag */
#define BQ27200_FLAG_FC			BIT(5)
#define BQ27200_FLAG_CHGS		BIT(7) /* Charge state flag */
#define BQ27200_RS			20 /* Resistor sense */
#define BQ27200_POWER_CONSTANT		(256 * 29200 / 1000)

/* Subcommands of Control() */
#define CONTROL_STATUS_SUBCMD		0x0000
#define DEV_TYPE_SUBCMD			0x0001
#define FW_VER_SUBCMD			0x0002
#define DF_VER_SUBCMD			0x001F
#define RESET_SUBCMD			0x0041
#define SET_CFGUPDATE_SUBCMD		0x0013
#define SEAL_SUBCMD			0x0020

/* Location of SEAL enable bit in bq276xx DM */
#define BQ276XX_OP_CFG_B_SUBCLASS	64
#define BQ276XX_OP_CFG_B_OFFSET		2
#define BQ276XX_OP_CFG_B_DEF_SEAL_BIT	(1 << 5)

static struct bq27x00_device_info *bq27x00_di;
static struct mt_battery_meter_custom_data *bat_meter_data;

 static int  bq27x00_read_battery_current(struct bq27x00_device_info *di);
 int bq27x00_battery_reset(struct bq27x00_device_info *di);

unsigned int PMIC_IMM_GetOneChannelValue(pmic_adc_ch_list_enum dwChannel, int deCount, int trimd);
extern int  IMM_GetOneChannelValue_Cali(int Channel, int *voltage);

struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
	int (*write)(struct bq27x00_device_info *di, u8 reg, int value,
			bool single);
	int (*blk_read)(struct bq27x00_device_info *di, u8 reg, u8 *data,
		u8 sz);
	int (*blk_write)(struct bq27x00_device_info *di, u8 reg, u8 *data,
		u8 sz);
};

enum bq27x00_chip { BQ27200, BQ27500, BQ27520, BQ274XX, BQ276XX, BQ2753X,
	BQ27542, BQ27545};

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int energy;
	int flags;
	int power_avg;
	int health;
	int bat_soc;
	int bat_ufsoc;
	int nac;
	int bat_curr;
};

struct dm_reg {
	u8 subclass;
	u8 offset;
	u8 len;
	u32 data;
};

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
	enum bq27x00_chip 	chip;
	struct bq27x00_reg_cache  cache;
	int  charge_design_full;
	unsigned long  last_update;
	struct delayed_work  work;
	struct  delayed_work fw_dl_work;
	struct  delayed_work dumpdf_work;
	struct power_supply	bat;
	struct bq27x00_access_methods  bus;
	struct mutex  lock;
	int fw_ver;
	int df_ver;
	u8 regs[NUM_REGS];
	struct dm_reg *dm_regs;
	u16 dm_regs_count;
};

static __initdata enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
};

static __initdata enum power_supply_property bq27520_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static __initdata enum power_supply_property bq2753x_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static __initdata enum power_supply_property bq27542_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static __initdata enum power_supply_property bq27545_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
};

static __initdata enum power_supply_property bq274xx_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static __initdata enum power_supply_property bq276xx_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

/*
 * Ordering the parameters based on subclass and then offset will help in
 * having fewer flash writes while updating.
 * Customize these values and, if necessary, add more based on system needs.
 */
 static struct dm_reg bq27520_dm_regs[] = {
	{48, 0, 2, 2000},       /*Design Capacity*/
	{49, 0, 2, 3400},	/*Final voltage*/
	{82, 0, 30, 2000},
	{83, 0, 2, 2000},		/* Qmax 1 */
	{84, 0, 2, 2000},		/* Qmax 2 */
	{91, 0, 2, 2000},
	{92, 0, 2, 2000},
	{93, 0, 19, 100},	/* Design Capacity */
	{94, 0, 19, 100},	/* Design Energy */
};

static struct dm_reg bq274xx_dm_regs[] = {
	{82, 0, 2, 1000},	/* Qmax */
	{82, 5, 1, 0x81},	/* Load Select */
	{82, 10, 2, 1340},	/* Design Capacity */
	{82, 12, 2, 3700},	/* Design Energy */
	{82, 16, 2, 3250},	/* Terminate Voltage */
	{82, 27, 2, 110},	/* Taper rate */
};

static struct dm_reg bq276xx_dm_regs[] = {
	{64, 2, 1, 0x2C},	/* Op Config B */
	{82, 0, 2, 1000},	/* Qmax */
	{82, 2, 1, 0x81},	/* Load Select */
	{82, 3, 2, 1340},	/* Design Capacity */
	{82, 5, 2, 3700},	/* Design Energy */
	{82, 9, 2, 3250},	/* Terminate Voltage */
	{82, 20, 2, 110},	/* Taper rate */
};

static unsigned int poll_interval = 360;
module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - " \
				"0 disables polling");

/*
 * Forward Declarations
 */
static int read_dm_block(struct bq27x00_device_info *di, u8 subclass,
	u8 offset, u8 *data);
/*
 * Common code for BQ27x00 devices
 */

static inline int bq27xxx_read(struct bq27x00_device_info *di, int reg_index,
		bool single)
{
	int val;
	/* Reports 0 for invalid/missing registers */
	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return 0;
	val = di->bus.read(di, di->regs[reg_index], single);
	return val;
}

static inline int bq27xxx_write(struct bq27x00_device_info *di, int reg_index,
		int value, bool single)
{
	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;
	return di->bus.write(di, di->regs[reg_index], value, single);
}

static int control_cmd_wr(struct bq27x00_device_info *di, u16 cmd)
{
	dev_dbg(di->dev, "%s: cmd - %04x\n", __func__, cmd);
	return di->bus.write(di, BQ27XXX_REG_CTRL, cmd, false);
}

static int control_cmd_read(struct bq27x00_device_info *di, u16 cmd)
{
	dev_dbg(di->dev, "%s: cmd - %04x\n", __func__, cmd);
	di->bus.write(di, BQ27XXX_REG_CTRL, cmd, false);
	msleep(5);
	return di->bus.read(di, BQ27XXX_REG_CTRL, false);
}

/*
 * It is assumed that the gauge is in unsealed mode when this function
 * is called
 */
static int bq276xx_seal_enabled(struct bq27x00_device_info *di)
{
	u8 buf[32];
	u8 op_cfg_b;
	if (!read_dm_block(di, BQ276XX_OP_CFG_B_SUBCLASS,
		BQ276XX_OP_CFG_B_OFFSET, buf)) {
		return 1; /* Err on the side of caution and try to seal */
	}
	op_cfg_b = buf[BQ276XX_OP_CFG_B_OFFSET & 0x1F];
	if (op_cfg_b & BQ276XX_OP_CFG_B_DEF_SEAL_BIT)
		return 1;
	return 0;
}

#define SEAL_UNSEAL_POLLING_RETRY_LIMIT	1000
static inline int sealed(struct bq27x00_device_info *di)
{
	return control_cmd_read(di, CONTROL_STATUS_SUBCMD) & (1 << 13);
}

static int unseal(struct bq27x00_device_info *di, u32 key)
{
	int i = 0;
	dev_dbg(di->dev, "%s: key - %08x\n", __func__, key);
	if (!sealed(di))
		goto out;
	di->bus.write(di, BQ27XXX_REG_CTRL, key & 0xFFFF, false);
	msleep(5);
	di->bus.write(di, BQ27XXX_REG_CTRL, (key & 0xFFFF0000) >> 16, false);
	msleep(5);
	while (i < SEAL_UNSEAL_POLLING_RETRY_LIMIT) {
		i++;
		if (!sealed(di))
			break;
		msleep(10);
	}
out:
	if (i == SEAL_UNSEAL_POLLING_RETRY_LIMIT) {
		dev_err(di->dev, "%s: failed\n", __func__);
		return 0;
	} else {
		return 1;
	}
}

static int seal(struct bq27x00_device_info *di)
{
	int i = 0;
	int is_sealed;
	dev_dbg(di->dev, "%s:\n", __func__);
	is_sealed = sealed(di);
	if (is_sealed)
		return is_sealed;
	if (di->chip == BQ276XX && !bq276xx_seal_enabled(di)) {
		dev_dbg(di->dev, "%s: sealing is not enabled\n", __func__);
		return is_sealed;
	}
	di->bus.write(di, BQ27XXX_REG_CTRL, SEAL_SUBCMD, false);
	while (i < SEAL_UNSEAL_POLLING_RETRY_LIMIT) {
		i++;
		is_sealed = sealed(di);
		if (is_sealed)
			break;
		msleep(10);
	}
	if (!is_sealed)
		dev_err(di->dev, "%s: failed\n", __func__);
	return is_sealed;
}

#define CFG_UPDATE_POLLING_RETRY_LIMIT 50
static int enter_cfg_update_mode(struct bq27x00_device_info *di)
{
	int i = 0;
	u16 flags;
	dev_dbg(di->dev, "%s:\n", __func__);
	if (!unseal(di, BQ274XX_UNSEAL_KEY))
		return 0;
	control_cmd_wr(di, SET_CFGUPDATE_SUBCMD);
	msleep(5);
	while (i < CFG_UPDATE_POLLING_RETRY_LIMIT) {
		i++;
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (flags & (1 << 4))
			break;
		msleep(100);
	}
	if (i == CFG_UPDATE_POLLING_RETRY_LIMIT) {
		dev_err(di->dev, "%s: failed %04x\n", __func__, flags);
		return 0;
	}
	return 1;
}

static int exit_cfg_update_mode(struct bq27x00_device_info *di)
{
	int i = 0;
	u16 flags;
	dev_dbg(di->dev, "%s:\n", __func__);
	control_cmd_wr(di, BQ274XX_SOFT_RESET);
	while (i < CFG_UPDATE_POLLING_RETRY_LIMIT) {
		i++;
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (!(flags & (1 << 4)))
			break;
		msleep(100);
	}
	if (i == CFG_UPDATE_POLLING_RETRY_LIMIT) {
		dev_err(di->dev, "%s: failed %04x\n", __func__, flags);
		return 0;
	}
	if (seal(di))
		return 1;
	else
		return 0;
}

static u8 checksum(u8 *data)
{
	u16 sum = 0;
	int i;
	for (i = 0; i < 32; i++)
		sum += data[i];
	sum &= 0xFF;
	return 0xFF - sum;
}

static void print_buf(const char *msg, u8 *buf)
{
	int i;
	printk("\nbq: %s buf: ", msg);
	for (i = 0; i < 32; i++)
		printk("%02x ", buf[i]);
	printk("\n");
}

static int update_dm_block(struct bq27x00_device_info *di, u8 subclass,
	u8 offset, u8 *data)
{
	u8 buf[32];
	u8 cksum;
	u8 blk_offset = offset >> 5;
	dev_dbg(di->dev, "%s: subclass %d offset %d\n",
		__func__, subclass, offset);
	di->bus.write(di, BLOCK_DATA_CONTROL, 0, true);
	msleep(5);
	di->bus.write(di, BLOCK_DATA_CLASS, subclass, true);
	msleep(5);
	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(5);
	di->bus.blk_write(di, BLOCK_DATA, data, 32);
	msleep(5);
	print_buf(__func__, data);
	cksum = checksum(data);
	di->bus.write(di, BLOCK_DATA_CHECKSUM, cksum, true);
	msleep(5);
	/* Read back and compare to make sure write is successful */
	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(5);
	di->bus.blk_read(di, BLOCK_DATA, buf, 32);
	if (memcmp(data, buf, 32)) {
		dev_err(di->dev, "%s: error updating subclass %d offset %d\n",
			__func__, subclass, offset);
		return 0;
	} else {
		return 1;
	}
}

static int read_dm_block(struct bq27x00_device_info *di, u8 subclass,
	u8 offset, u8 *data)
{
	u8 cksum_calc, cksum;
	u8 blk_offset = offset >> 5;
	dev_info(di->dev, "%s: subclass %d offset %d\n",
		__func__, subclass, offset);
	di->bus.write(di, BLOCK_DATA_CONTROL, 0, true);
	msleep(5);
	di->bus.write(di, BLOCK_DATA_CLASS, subclass, true);
	msleep(5);
	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(5);
	di->bus.blk_read(di, BLOCK_DATA, data, 32);
	cksum_calc = checksum(data);
	cksum = di->bus.read(di, BLOCK_DATA_CHECKSUM, true);
	if (cksum != cksum_calc) {
		dev_err(di->dev, "%s: error reading subclass %d offset %d\n",
			__func__, subclass, offset);
		return 0;
	}
	print_buf(__func__, data);
	return 1;
}

/*
 * Return the battery State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_soc(struct bq27x00_device_info *di)
{
	int soc;
	if(gauge_firmware_load_flag == 1) {
		return 50;
	}
	soc = bq27xxx_read(di, BQ27XXX_REG_SOC, false);
	if (soc < 0)
		dev_dbg(di->dev, "error reading relative State-of-Charge\n");
	return soc;
}

static int bq27x00_battery_read_ufsoc(struct bq27x00_device_info *di)
{
	int ufsoc;
	ufsoc = bq27xxx_read(di, BQ27XXX_REG_UFSOC, false);
	if (ufsoc < 0)
		dev_dbg(di->dev, "error reading relative State-of-Charge\n");
	return ufsoc;
}

/*
 * Return a battery charge value in ¦ÌAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_charge(struct bq27x00_device_info *di, u8 reg)
{
	int charge;
	charge = bq27xxx_read(di, reg, false);
	if (charge < 0) {
		dev_dbg(di->dev, "error reading charge register %02x: %d\n",
			reg, charge);
		return charge;
	}
	if (di->chip == BQ27200)
		charge = charge * 3570 / BQ27200_RS;
	else
		charge *= 1000;
	return charge;
}

/*
 * Return the battery Nominal available capaciy in ¦ÌAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_nac(struct bq27x00_device_info *di)
{
	int flags;
	if (di->chip == BQ27200) {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, true);
		if (flags >= 0 && (flags & BQ27200_FLAG_CI))
			return -ENODATA;
	}
	return bq27x00_battery_read_charge(di, BQ27XXX_REG_NAC);
}

/*
 * Return the battery Last measured discharge in ¦ÌAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_fcc(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27XXX_REG_FCC);
}

/*
 * Return the Design Capacity in ¦ÌAh
 * Or < 0 if something fails.
 */
 /*bq27520 L1 not support charge full capacity */
static int bq27x00_battery_read_dcap(struct bq27x00_device_info *di)
{
	int dcap;
	return 6600000;
	dcap = bq27xxx_read(di, BQ27XXX_REG_DCAP, false);
	if (dcap < 0) {
		dev_dbg(di->dev, "error reading initial last measured discharge\n");
		return dcap;
	}
	if (di->chip == BQ27200)
		dcap = dcap * 256 * 3570 / BQ27200_RS;
	else
		dcap *= 1000;
	return dcap;
}

/*
 * Return the battery Available energy in ¦ÌWh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_energy(struct bq27x00_device_info *di)
{
	int ae;
	ae = bq27xxx_read(di, BQ27XXX_REG_AE, false);
	if (ae < 0) {
		dev_dbg(di->dev, "error reading available energy\n");
		return ae;
	}
	if (di->chip == BQ27200)
		ae = ae * 29200 / BQ27200_RS;
	else
		ae *= 1000;
	return ae;
}

/*
 * Return the battery temperature in tenths of degree Kelvin
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_temperature(struct bq27x00_device_info *di)
{
	int temp;
	if (gauge_firmware_load_flag == 1) {
		return 2981;
	}
	temp = bq27xxx_read(di, BQ27XXX_REG_TEMP, false);
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return temp;
	}
	if (di->chip == BQ27200)
		temp = 5 * temp / 2;
	return temp;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;
	cyct = bq27xxx_read(di, BQ27XXX_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");
	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_time(struct bq27x00_device_info *di, u8 reg)
{
	int tval;
	tval = bq27xxx_read(di, reg, false);
	if (tval < 0) {
		dev_dbg(di->dev, "error reading time register %02x: %d\n",
			reg, tval);
		return tval;
	}
	if (tval == 65535)
		return -ENODATA;
	return tval * 60;
}

/*
 * Read a power avg register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_pwr_avg(struct bq27x00_device_info *di, u8 reg)
{
	int tval;
	tval = bq27xxx_read(di, reg, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading power avg rgister  %02x: %d\n",
			reg, tval);
		return tval;
	}
	if (di->chip == BQ27200)
		return (tval * BQ27200_POWER_CONSTANT) / BQ27200_RS;
	else
		return tval;
}

static int overtemperature(struct bq27x00_device_info *di, u16 flags)
{
	if (di->chip == BQ27520)
		return flags & (BQ27XXX_FLAG_OTC | BQ27XXX_FLAG_OTD);
	else
		return flags & BQ27XXX_FLAG_OTC;
}

/*
 * Read flag register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_health(struct bq27x00_device_info *di)
{
	u16 tval;
	tval = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading flag register:%d\n", tval);
		return tval;
	}
	if ((di->chip == BQ27200)) {
		if (tval & BQ27200_FLAG_EDV1)
			tval = POWER_SUPPLY_HEALTH_DEAD;
		else
			tval = POWER_SUPPLY_HEALTH_GOOD;
		return tval;
	} else {
		if (tval & BQ27XXX_FLAG_SOCF)
			tval = POWER_SUPPLY_HEALTH_DEAD;
		else if (overtemperature(di, tval))
			tval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			tval = POWER_SUPPLY_HEALTH_GOOD;
		return tval;
	}
	return -EINVAL;
}

static void bq27x00_update(struct bq27x00_device_info *di)
{
	struct bq27x00_reg_cache cache = {0, };
	bool is_bq27200 = di->chip == BQ27200;
	bool is_bq27500 = di->chip == BQ27500;
	bool is_bq274xx = di->chip == BQ274XX;
	bool is_bq276xx = di->chip == BQ276XX;

	cache.flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, !is_bq27500);
	if (cache.flags >= 0) {
		if (is_bq27200 && (cache.flags & BQ27200_FLAG_CI)) {
			dev_info(di->dev, "battery is not calibrated! ignoring capacity values\n");
			cache.capacity = -ENODATA;
			cache.energy = -ENODATA;
			cache.time_to_empty = -ENODATA;
			cache.time_to_empty_avg = -ENODATA;
			cache.time_to_full = -ENODATA;
			cache.charge_full = -ENODATA;
			cache.health = -ENODATA;
		} else {
			cache.capacity = bq27x00_battery_read_soc(di);
			if (!(is_bq274xx || is_bq276xx)) {
				cache.energy = bq27x00_battery_read_energy(di);
				cache.time_to_empty =
					bq27x00_battery_read_time(di,
							BQ27XXX_REG_TTE);
				cache.time_to_empty_avg =
					bq27x00_battery_read_time(di,
							BQ27XXX_REG_TTECP);
				cache.time_to_full =
					bq27x00_battery_read_time(di,
							BQ27XXX_REG_TTF);
			}
			cache.charge_full = bq27x00_battery_read_fcc(di);
			cache.health = bq27x00_battery_read_health(di);
			cache.bat_soc = bq27x00_battery_read_soc(di);
			cache.bat_ufsoc = bq27x00_battery_read_ufsoc(di);
			cache.bat_curr = bq27x00_read_battery_current(di);
			cache.nac = bq27x00_battery_read_nac(di);
		}
		cache.temperature = bq27x00_battery_read_temperature(di);
		if (!is_bq274xx)
			cache.cycle_count = bq27x00_battery_read_cyct(di);
		cache.power_avg =
			bq27x00_battery_read_pwr_avg(di, BQ27XXX_POWER_AVG);
		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27x00_battery_read_dcap(di);
	}

	dev_info(di->dev, "BQ27x00 gauge information: energy=%d, time_to_empty =%d, time_to_empty_avg =%d, time_to_full =%d,\
		charge_full =%d, health=%d, temperature=%d, bat_soc=%d, bat_ufsoc=%d, bat_curr=%d, nac=%d\n", cache.energy, cache.time_to_empty,
		cache.time_to_empty_avg, cache.time_to_full, cache.charge_full, cache.health, cache.temperature, cache.bat_soc,  cache.bat_ufsoc,
		cache.bat_curr, cache.nac);

	if ((memcmp(&di->cache, &cache, sizeof(cache)) != 0) && (gauge_firmware_load_flag == 0)) {
		di->cache = cache;
		power_supply_changed(&di->bat);
	}
	di->last_update = jiffies;
}

static void copy_to_dm_buf_big_endian(struct bq27x00_device_info *di,
	u8 *buf, u8 offset, u8 sz, u32 val)
{
	dev_dbg(di->dev, "%s: offset %d sz %d val %d\n",
		__func__, offset, sz, val);
	switch (sz) {
	case 1:
		buf[offset] = (u8) val;
		break;
	case 2:
		put_unaligned_be16((u16) val, &buf[offset]);
		break;
	case 4:
		put_unaligned_be32(val, &buf[offset]);
		break;
	default:
		dev_err(di->dev, "%s: bad size for dm parameter - %d",
			__func__, sz);
		break;
	}
}

static int rom_mode_gauge_init_completed(struct bq27x00_device_info *di)
{
	dev_dbg(di->dev, "%s:\n", __func__);
	return control_cmd_read(di, CONTROL_STATUS_SUBCMD) &
		BQ274XX_CTRL_STATUS_INITCOMP;
}

static bool rom_mode_gauge_dm_initialized(struct bq27x00_device_info *di)
{
	u16 flags;
	flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
	dev_dbg(di->dev, "%s: flags - 0x%04x\n", __func__, flags);
	if (flags & BQ274XX_FLAG_ITPOR)
		return false;
	else
		return true;
}

#define INITCOMP_TIMEOUT_MS		10000

/*dump gauge's data flash content */
static void rom_mode_gauge_dump_df(struct bq27x00_device_info *di)
{
	int i;
	u8 subclass, offset;
	u8 buf[32];
	struct dm_reg *dm_reg;
		dev_err(di->dev, "%s:\n", __func__);
	/*
	while (!rom_mode_gauge_init_completed(di) && timeout > 0) {
		msleep(100);
		timeout -= 100;
	}
	if (timeout <= 0) {
		dev_err(di->dev, "%s: INITCOMP not set after %d seconds\n",
			__func__, INITCOMP_TIMEOUT_MS/100);
		return;
	}
	*/
	if (!di->dm_regs || !di->dm_regs_count) {
		dev_err(di->dev, "%s: Data not available for DM initialization\n",
			__func__);
		return;
	}
	dev_dbg(di->dev, "dump gauge firmware data flash %s:\n", __func__);
	for (i = 0; i < di->dm_regs_count; i++) {
		dm_reg = &di->dm_regs[i];
		subclass = dm_reg->subclass;
		offset = dm_reg->offset;
		read_dm_block(di, dm_reg->subclass, dm_reg->offset, buf);
		}
}

static void rom_mode_gauge_dm_init(struct bq27x00_device_info *di)
{
	int i;
	int timeout = INITCOMP_TIMEOUT_MS;
	u8 subclass, offset;
	u32 blk_number;
	u32 blk_number_prev = 0;
	u8 buf[32];
	bool buf_valid = false;
	struct dm_reg *dm_reg;
	dev_dbg(di->dev, "%s:\n", __func__);
	while (!rom_mode_gauge_init_completed(di) && timeout > 0) {
		msleep(100);
		timeout -= 100;
	}
	if (timeout <= 0) {
		dev_err(di->dev, "%s: INITCOMP not set after %d seconds\n",
			__func__, INITCOMP_TIMEOUT_MS/100);
		return;
	}
	if (!di->dm_regs || !di->dm_regs_count) {
		dev_err(di->dev, "%s: Data not available for DM initialization\n",
			__func__);
		return;
	}
	enter_cfg_update_mode(di);
	for (i = 0; i < di->dm_regs_count; i++) {
		dm_reg = &di->dm_regs[i];
		subclass = dm_reg->subclass;
		offset = dm_reg->offset;
		/*
		 * Create a composite block number to see if the subsequent
		 * register also belongs to the same 32 btye block in the DM
		 */
		blk_number = subclass << 8;
		blk_number |= offset >> 5;
		if (blk_number == blk_number_prev) {
			copy_to_dm_buf_big_endian(di, buf, offset,
				dm_reg->len, dm_reg->data);
		} else {
			if (buf_valid)
				update_dm_block(di, blk_number_prev >> 8,
					(blk_number_prev << 5) & 0xFF , buf);
			else
				buf_valid = true;
			read_dm_block(di, dm_reg->subclass, dm_reg->offset,
				buf);
			copy_to_dm_buf_big_endian(di, buf, offset,
				dm_reg->len, dm_reg->data);
		}
		blk_number_prev = blk_number;
	}
	/* Last buffer to be written */
	if (buf_valid)
		update_dm_block(di, subclass, offset, buf);
	exit_cfg_update_mode(di);
}

static void bq27520_dfdump_work(struct work_struct *work)
{
		struct bq27x00_device_info *di = container_of(work,
			struct bq27x00_device_info, dumpdf_work.work);

		dev_dbg(di->dev, "BQ27520 dump gauge data flash:\n");

		rom_mode_gauge_dump_df(di);
}

static int dump_register_fgadc(void *data);

static void bq27x00_battery_poll(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, work.work);
	if (((di->chip == BQ274XX) || (di->chip == BQ276XX)) &&
		!rom_mode_gauge_dm_initialized(di)) {
		rom_mode_gauge_dm_init(di);
	}
	bq27x00_update(di);
	dump_register_fgadc(di);
	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&di->work, poll_interval * HZ);
	}
}

/*
 * Return the battery average current in ¦ÌA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int curr;
	int flags;
	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}
	if (di->chip == BQ27200) {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (flags & BQ27200_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}
		val->intval = curr * 3570 / BQ27200_RS;
	} else {
		/* Other gauges return signed value */
		val->intval = (int)((s16)curr) * 1000;
	}
	return 0;
}

static int  bq27x00_read_battery_current(struct bq27x00_device_info *di)
{
	int curr;
	int flags;
	int value;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}
	if (di->chip == BQ27200) {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (flags & BQ27200_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}
		value = curr * 3570 / BQ27200_RS;
	} else {
		/* Other gauges return signed value */
		value = (int)((s16)curr) * 1000;
	}
	return value;
}

/* use platform battery status instead of fuelgauge*/
/*
static int bq27x00_battery_status(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int status;
	if (di->chip == BQ27200) {
		if (di->cache.flags & BQ27200_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27200_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if (power_supply_am_i_supplied(&di->bat))
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		if (di->cache.flags & BQ27XXX_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27XXX_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	}
	val->intval = status;
	return 0;
}
*/
static int bq27x00_battery_capacity_level(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int level;
	if (di->chip == BQ27200) {
		if (di->cache.flags & BQ27200_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27200_FLAG_EDV1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27200_FLAG_EDVF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else {
		if (di->cache.flags & BQ27XXX_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27XXX_FLAG_SOC1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27XXX_FLAG_SOCF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}
	val->intval = level;
	return 0;
}

/*
 * Return the battery Voltage in millivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int volt;
	if (gauge_firmware_load_flag == 1) {
		val->intval = 4000 * 1000;
		return 0;
	}
	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt;
	}
	val->intval = volt * 1000;
	return 0;
}

static int bq27x00_read_battery_voltage(struct bq27x00_device_info *di)
{
	int volt;
	if (gauge_firmware_load_flag == 1) {
		volt = 4000 * 1000;
		return volt;
	}
	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt;
	}
	volt = volt * 1000;
	return volt;
}

static int bq27x00_simple_value(int value,
	union power_supply_propval *val)
{
	if (value < 0)
		return value;
	val->intval = value;
	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);

static int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);
	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27x00_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);
	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0 && gauge_firmware_load_flag == 0)
		return -ENODEV;
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (gauge_firmware_load_flag == 1) {
				val->intval = 1;
		} else {
				val->intval = di->cache.flags < 0 ? 0 : 1;
			}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27x00_battery_current(di, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27x00_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = bq27x00_battery_capacity_level(di, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_simple_value(di->cache.temperature, val);
		if (ret == 0)
			val->intval -= 2731;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_empty, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27x00_simple_value(di->cache.time_to_empty_avg, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_full, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_simple_value(bq27x00_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27x00_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_simple_value(di->charge_design_full, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27x00_simple_value(di->cache.cycle_count, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_simple_value(di->cache.energy, val);
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		ret = bq27x00_simple_value(di->cache.power_avg, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = bq27x00_simple_value(di->cache.health, val);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);
	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static void __init set_properties_array(struct bq27x00_device_info *di,
	enum power_supply_property *props, int num_props)
{
	int tot_sz = num_props * sizeof(enum power_supply_property);
	di->bat.properties = devm_kzalloc(di->dev, tot_sz, GFP_KERNEL);
	if (di->bat.properties) {
		memcpy(di->bat.properties, props, tot_sz);
		di->bat.num_properties = num_props;
	} else {
		di->bat.num_properties = 0;
	}
}

static void bq27x00_dl_fw(struct work_struct *work);

static int __init bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	if (di->chip == BQ274XX) {
		set_properties_array(di, bq274xx_battery_props,
			ARRAY_SIZE(bq274xx_battery_props));
	} else if (di->chip == BQ276XX) {
		set_properties_array(di, bq276xx_battery_props,
			ARRAY_SIZE(bq276xx_battery_props));
	} else if (di->chip == BQ27520) {
		set_properties_array(di, bq27520_battery_props,
			ARRAY_SIZE(bq27520_battery_props));
	} else if (di->chip == BQ2753X) {
		set_properties_array(di, bq2753x_battery_props,
			ARRAY_SIZE(bq2753x_battery_props));
	} else if (di->chip == BQ27542) {
		set_properties_array(di, bq27542_battery_props,
			ARRAY_SIZE(bq27542_battery_props));
	} else if (di->chip == BQ27545) {
		set_properties_array(di, bq27545_battery_props,
			ARRAY_SIZE(bq27545_battery_props));
	} else {
		set_properties_array(di, bq27x00_battery_props,
			ARRAY_SIZE(bq27x00_battery_props));
	}
	di->bat.get_property = bq27x00_battery_get_property;
	di->bat.external_power_changed = bq27x00_external_power_changed;
	INIT_DELAYED_WORK(&di->work, bq27x00_battery_poll);
	INIT_DELAYED_WORK(&di->fw_dl_work, bq27x00_dl_fw);
	INIT_DELAYED_WORK(&di->dumpdf_work, bq27520_dfdump_work);
	mutex_init(&di->lock);
	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}
	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);
	bq27x00_update(di);
	return 0;
}

static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{
	/*
	 * power_supply_unregister call bq27x00_battery_get_property which
	 * call bq27x00_battery_poll.
	 * Make sure that bq27x00_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;
	cancel_delayed_work_sync(&di->work);
	power_supply_unregister(&di->bat);
	mutex_destroy(&di->lock);
}

/* i2c specific code */
/*#ifdef CONFIG_BATTERY_BQ27X00_I2C*/
/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);
static int bq27xxx_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;
	if (!client->adapter)
		return -ENODEV;
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;
	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];
	return ret;
}

static int bq27xxx_write_i2c(struct bq27x00_device_info *di, u8 reg, int value, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	unsigned char data[4];
	int ret;
	if (!client->adapter)
		return -ENODEV;
	data[0] = reg;
	if (single) {
		data[1] = (unsigned char)value;
		msg.len = 2;
	} else {
		put_unaligned_le16(value, &data[1]);
		msg.len = 3;
	}
	msg.buf = data;
	msg.addr = client->addr;
	msg.flags = 0;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		return ret;
	return 0;
}

static int bq27xxx_read_i2c_blk(struct bq27x00_device_info *di, u8 reg,
	u8 *data, u8 len)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	int ret;
	if (!client->adapter)
		return -ENODEV;
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = 1;
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	msg[1].len = len;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;
	return ret;
}

static int bq27xxx_write_i2c_blk(struct bq27x00_device_info *di, u8 reg,
	u8 *data, u8 sz)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	int ret;
	u8 buf[128];       /*increase the array size avoid panic when load firmware*/
	if (!client->adapter)
		return -ENODEV;
	buf[0] = reg;
	memcpy(&buf[1], data, sz);
	msg.buf = buf;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sz + 1;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		return ret;
	return 0;
}

 int bq27x00_battery_reset(struct bq27x00_device_info *di)
{
	dev_info(di->dev, "Gas Gauge Reset\n");
	bq27xxx_write(di, BQ27XXX_REG_CTRL, RESET_SUBCMD, false);
	msleep(10);
	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_fw_version(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, FW_VER_SUBCMD, false);
	msleep(10);
	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_device_type(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, DEV_TYPE_SUBCMD, false);
	msleep(10);
	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_dataflash_version(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, DF_VER_SUBCMD, false);
	msleep(10);
	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int read_battery_id(void *data)
{
	int value;
	IMM_GetOneChannelValue_Cali(1, &value);
	value /= 1000;
	*(int *) data = value;
	return value;
}

static ssize_t show_firmware_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int ver;
	ver = bq27x00_battery_read_fw_version(di);
	return snprintf(buf, sizeof(int), "%d\n", ver);
}

static ssize_t show_dataflash_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int ver;
	ver = bq27x00_battery_read_dataflash_version(di);
	return snprintf(buf, sizeof(int), "%d\n", ver);
}

static ssize_t show_device_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int dev_type;
	dev_type = bq27x00_battery_read_device_type(di);
	return snprintf(buf, sizeof(int), "%d\n", dev_type);
}

static ssize_t show_reset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	bq27x00_battery_reset(di);
	return snprintf(buf, 5, "okay\n");
}

static ssize_t show_battid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	 int value;
	read_battery_id(&value);
	return snprintf(buf, sizeof(long int), "%d\n", value);
}

static DEVICE_ATTR(fw_version, S_IRUGO, show_firmware_version, NULL);
static DEVICE_ATTR(df_version, S_IRUGO, show_dataflash_version, NULL);
static DEVICE_ATTR(device_type, S_IRUGO, show_device_type, NULL);
static DEVICE_ATTR(reset, S_IRUGO, show_reset, NULL);
static DEVICE_ATTR(battid, S_IRUGO, show_battid, NULL);

static struct attribute *bq27x00_attributes[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_df_version.attr,
	&dev_attr_device_type.attr,
	&dev_attr_reset.attr,
	&dev_attr_battid.attr,
	NULL
};
static const struct attribute_group bq27x00_attr_group = {
	.attrs = bq27x00_attributes,
};

/* load fuelgauge firmware code */

#define isxdigit(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'f') \
			 || ('A' <= (c) && (c) <= 'F'))

#define isdigit(c)	('0' <= (c) && (c) <= '9')
#define islower(c)	('a' <= (c) && (c) <= 'z')
#define toupper(c)	(islower(c) ? ((c) - 'a' + 'A') : (c))

#define BQGAUGE_I2C_ROM_ADDR    (0x16 >> 1)
#define BQGAUGE_I2C_DEV_ADDR    (0xAA >> 1)

#define I2C_MAX_TRANSFER_LEN	128
#define MAX_ASC_PER_LINE		400
#define FIRMWARE_FILE_SIZE	(3301*400)

static int bq27x00_atoi(const char *s)
{
	int k = 0;

	k = 0;
	while (*s != '\0' && *s >= '0' && *s <= '9') {
		k = 10 * k + (*s - '0');
		s++;
	}
	return k;
}

static unsigned long bq27x00_strtoul(const char *cp, unsigned int base)
{
	unsigned long result = 0, value;

	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
					? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}

	return result;
}

static int bq27x00_firmware_program(struct bq27x00_device_info *di,
		const unsigned char *pgm_data, unsigned int filelen)
{
	unsigned int i = 0, j = 0, ulDelay = 0, ulReadNum = 0;
	unsigned int ulCounter = 0, ulLineLen = 0;
	unsigned char temp = 0;
	unsigned char *p_cur;
	unsigned char pBuf[MAX_ASC_PER_LINE] = { 0 };
	unsigned char p_src[I2C_MAX_TRANSFER_LEN] = { 0 };
	unsigned char p_dst[I2C_MAX_TRANSFER_LEN] = { 0 };
	unsigned char ucTmpBuf[16] = { 0 };
	struct i2c_client *client = to_i2c_client(di->dev);

	client->addr = BQGAUGE_I2C_ROM_ADDR;

firmware_program_begin:
	if (ulCounter > 10)	{
		client->addr = BQGAUGE_I2C_DEV_ADDR;
		return -ENODATA;
	}

	p_cur = (unsigned char *)pgm_data;

	while (1) {
		if ((p_cur - pgm_data) >= filelen) {
			dev_info(di->dev, "Firmware download success, quit\n");
			break;
		}

		while (*p_cur == '\r' || *p_cur == '\n')
			p_cur++;

		i = 0;
		ulLineLen = 0;

		memset(p_src, 0x00, sizeof(p_src));
		memset(p_dst, 0x00, sizeof(p_dst));
		memset(pBuf, 0x00, sizeof(pBuf));
		while (i < MAX_ASC_PER_LINE)	{
			temp = *p_cur++;
			i++;
			if (('\r' == temp) || ('\n' == temp))
				break;
			if (' ' != temp)
				pBuf[ulLineLen++] = temp;
		}

		/* skip the comments line */
		if (pBuf[0] == ';')
			continue;

		p_src[0] = pBuf[0];
		p_src[1] = pBuf[1];

		if (('W' == p_src[0]) || ('C' == p_src[0]))	{
			for (i = 2, j = 0; i < ulLineLen; i += 2, j++) {
				memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
				memcpy(ucTmpBuf, pBuf+i, 2);
				p_src[2+j] = bq27x00_strtoul(ucTmpBuf, 16);
			}

			temp = (ulLineLen - 2)/2;
			ulLineLen = temp + 2;
		} else if ('X' == p_src[0]) {
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+2, ulLineLen-2);
			ulDelay = bq27x00_atoi(ucTmpBuf);
		} else if ('R' == p_src[0]) {
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+2, 2);
			p_src[2] = bq27x00_strtoul(ucTmpBuf, 16);
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+4, 2);
			p_src[3] = bq27x00_strtoul(ucTmpBuf, 16);
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+6, ulLineLen-6);
			ulReadNum = bq27x00_atoi(ucTmpBuf);
		}

		if (':' == p_src[1]) {
			switch (p_src[0]) {
			case 'W':
#if 0
				dev_info(di->dev, "W: ");
				for (i = 0; i < ulLineLen-4; i++)
					dev_info(di->dev, "%x ", p_src[4+i]);
				dev_info(di->dev, "\n");
#endif
				client->addr = p_src[2] >> 1;
				if (bq27xxx_write_i2c_blk(di,
							p_src[3],
							&p_src[4],
							ulLineLen-4) < 0)
					dev_err(di->dev,
							"Err: len = %d, reg= %02x\n",
							ulLineLen-4,
							p_src[3]);
			break;
			case 'R':
				client->addr = p_src[2] >> 1;
				if (bq27xxx_read_i2c_blk(di, p_src[3],
							p_dst, ulReadNum) < 0) {
					dev_err(di->dev, "bq_read_i2c_blk failed\n");
				}
			break;
			case 'C':
				client->addr = p_src[2] >> 1;
				if (bq27xxx_read_i2c_blk(di, p_src[3], p_dst, ulLineLen-4) < 0 ||
					memcmp(p_dst, &p_src[4], ulLineLen-4) != 0) {
						ulCounter++;
						dev_err(di->dev, "Comparing failed\n");
						goto firmware_program_begin;
				}
				break;
			case 'X':
				mdelay(ulDelay);
				break;

			default:
				dev_err(di->dev, "image file format is not correct!\n");
				client->addr = BQGAUGE_I2C_DEV_ADDR;
				return -ENODATA;
			}
		}
	}

	client->addr = BQGAUGE_I2C_DEV_ADDR;

	return 0;
}

static int bq27x00_firmware_download(struct bq27x00_device_info *di,
		const unsigned char *pgm_data, unsigned int len)
{
	int ret = 0;
	/*
	  Remove this step because it's provided in .dffs file.
	   Otherwize there will be an error when write data to
	   normal mode address 0x55.
	*/

	/*Enter Rom Mode */
	/*
	ret = bq_write_i2c_word(di,
			BQ27520_REG_CONTRL, BQ27520_SUBCMD_ENTER_ROM);
	if (0 > ret) {
		dev_err(di->dev, "Enter rom mode failed\n");
		return ret;
	}
	mdelay(10);
	*/

	/*program bqfs*/
	ret = bq27x00_firmware_program(di, pgm_data, len);
	if (0 != ret)
		dev_err(di->dev, "bq27x00_firmware_program failed\n");

	return ret;
}

static int bq27x00_update_firmware(struct bq27x00_device_info *di,
		const char *pFilePath)
{
	char *buf;
	struct file *filp;
	struct inode *inode = NULL;
	mm_segment_t oldfs;
	unsigned int length;
	int ret = 0;

	/* open file */
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(pFilePath, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		dev_err(di->dev, "filp_open failed\n");
		set_fs(oldfs);
		return -ENODATA;
	}

	if (!filp->f_op) {
		dev_err(di->dev, "File Operation Method Error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -ENODATA;
	}
	inode = filp->f_path.dentry->d_inode;
	if (!inode)	{
		dev_err(di->dev, "Get inode from filp failed\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -ENODATA;
	}

	/* file's size */
	length = i_size_read(inode->i_mapping->host);
	dev_info(di->dev, "bq27x00 firmware image size=%d\n", length);
	if (!(length > 0 && length < FIRMWARE_FILE_SIZE)) {
		dev_err(di->dev, "Get file size error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -ENODATA;
	}

	/* allocation buff size */
	buf = vmalloc(length+(length%2));       /* buf size if even */
	if (!buf) {
		dev_err(di->dev, "Alloctation memory failed\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -ENODATA;
	}

	/* read data */
	if (filp->f_op->read(filp, buf, length, &filp->f_pos) != length) {
		dev_err(di->dev, "File read error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		vfree(buf);
		return -ENODATA;
	}

	ret = bq27x00_firmware_download(di, (const unsigned char *)buf, length);

	filp_close(filp, NULL);
	set_fs(oldfs);
	vfree(buf);

	return ret;
}

#define CURRENT_FW_DF_VER	0x0006

#define CAPPU_PROFILE			"/system/etc/cappu1.dffs"

static void bq27x00_dl_fw(struct work_struct *work)
{
	char *path;
	int ret = 0, df_ver = 0;
	/*
	int batt_ver = 0;
	struct ps_batt_chg_prof chrg_profile;
	struct ps_pse_mod_prof *bprof;
	*/
	struct bq27x00_device_info *di = container_of(work,
		struct bq27x00_device_info, fw_dl_work.work);

	path = CAPPU_PROFILE;
/*
	if (get_batt_prop(&chrg_profile)) {
		pr_err("%s:Error in getting charge profile\n", __func__);
		schedule_delayed_work(&di->fw_dl_work, 60 * HZ);
		return;
	}

	bprof = (struct ps_pse_mod_prof *) chrg_profile.batt_prof;
	if (!memcmp(bprof->batt_id, BATT_ID_LG, 2)) {
		pr_info("LG battery\n");
		batt_ver = CURRENT_LG_FW_DF_VER;
		path = LG_PROFILE;
	} else if (!memcmp(bprof->batt_id, BATT_ID_SS, 2)) {
		pr_info("SS battery\n");
		batt_ver = CURRENT_SS_FW_DF_VER;
		path = SS_PROFILE;
	} else {
	*/
		/* if we can't get batt id ,use lg profile instead */
/*
		pr_info("UNKNOWN battery\n");
		batt_ver = CURRENT_LG_FW_DF_VER;
		path = LG_PROFILE;
	}
*/
	df_ver = bq27x00_battery_read_dataflash_version(di);
	pr_info("df ver %x\n", df_ver);

	if (df_ver == CURRENT_FW_DF_VER) {
		gauge_firmware_load_flag = 0;
		return;
	} else {
		gauge_firmware_load_flag = 1;
	}

	pr_info("Update firmware\n");
	ret = bq27x00_update_firmware(di, path);
	msleep(3000);

	if (ret == 0) {
		gauge_firmware_load_flag = 0;
		dev_info(di->dev, "Update firmware finished...\n");
	} else {
		gauge_firmware_load_flag = 1;
		dev_info(di->dev, "Update firmware failed...\n");
		schedule_delayed_work(&di->fw_dl_work, 10 * HZ);
	}
}
/*fuelgauge firmware download end */

static int __init bq27x00_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	int num;
	int retval = 0;
	u8 *regs;

	/* Get new ID for the new battery device */
	/*retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;*/

	mutex_lock(&battery_mutex);
	num = idr_alloc(&battery_id, client, 0, 0, GFP_KERNEL);
	/*retval = idr_get_new(&battery_id, client, &num);*/
	mutex_unlock(&battery_mutex);

	if (num < 0)
		return num;

	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->bat.name = name;
	di->bus.read = &bq27xxx_read_i2c;
	di->bus.write = &bq27xxx_write_i2c;
	di->bus.blk_read = bq27xxx_read_i2c_blk;
	di->bus.blk_write = bq27xxx_write_i2c_blk;
	di->dm_regs = NULL;
	di->dm_regs_count = 0;
	if (di->chip == BQ27200)
		regs = bq27200_regs;
	else if (di->chip == BQ27500)
		regs = bq27500_regs;
	else if (di->chip == BQ27520) {
		regs = bq27520_regs;
		di->dm_regs = bq27520_dm_regs;
		di->dm_regs_count = ARRAY_SIZE(bq27520_dm_regs);
	} else if (di->chip == BQ2753X)
		regs = bq2753x_regs;
	else if (di->chip == BQ27542 || di->chip == BQ27545)
		regs = bq2754x_regs;
	else if (di->chip == BQ274XX) {
		regs = bq274xx_regs;
		di->dm_regs = bq274xx_dm_regs;
		di->dm_regs_count = ARRAY_SIZE(bq274xx_dm_regs);
	} else if (di->chip == BQ276XX) {
		/* commands are same as bq274xx, only DM is different */
		regs = bq276xx_regs;
		di->dm_regs = bq276xx_dm_regs;
		di->dm_regs_count = ARRAY_SIZE(bq276xx_dm_regs);
	} else {
		dev_err(&client->dev,
			"Unexpected gas gague: %d\n", di->chip);
		regs = bq27520_regs;
	}

	memcpy(di->regs, regs, NUM_REGS);
	di->df_ver = bq27x00_battery_read_dataflash_version(di);
	if (di->df_ver <  0) {
		client->addr = BQGAUGE_I2C_DEV_ADDR;
		di->df_ver = bq27x00_battery_read_dataflash_version(di);
		gauge_firmware_load_flag = 1;
	}
	di->fw_ver = bq27x00_battery_read_fw_version(di);
	dev_info(&client->dev, "Gas Guage fw version is 0x%04x, Data Flash  version is 0x%04x\n", di->fw_ver, di->df_ver);
	retval = bq27x00_powersupply_init(di);
	if (retval)
		goto batt_failed_3;

	bq27x00_di = di;

	/* Schedule a polling after about 1 min */
	schedule_delayed_work(&di->work, 60 * HZ);
	schedule_delayed_work(&di->fw_dl_work, 20 * HZ);
	schedule_delayed_work(&di->dumpdf_work, 90 * HZ);
	i2c_set_clientdata(client, di);
	retval = sysfs_create_group(&client->dev.kobj, &bq27x00_attr_group);
	if (retval)
		dev_err(&client->dev, "could not create sysfs files\n");
	return 0;
batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);
	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);
	bq27x00_powersupply_unregister(di);
	kfree(di->bat.name);
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);
	kfree(di);
	return 0;
}

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27200", BQ27200 },
	{ "bq27500", BQ27500 },
	{ "bq27520", BQ27520 },
	{ "bq274xx", BQ274XX },
	{ "bq276xx", BQ276XX },
	{ "bq2753x", BQ2753X },
	{ "bq27542", BQ27542 },
	{ "bq27545", BQ27545 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq27x00_id);

#ifdef CONFIG_OF
static const struct of_device_id bq27520_id[] = {
	{.compatible = "ti,bq27520"},
};

MODULE_DEVICE_TABLE(of, bq27520_id);
#endif

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
};

static inline int __init bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");
	return ret;
}

static inline void __exit bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

/*
#else
static inline int bq27x00_battery_i2c_init(void) { return 0; }
static inline void bq27x00_battery_i2c_exit(void) {};
#endif
*/

/* platform specific code */
#ifdef CONFIG_BATTERY_BQ27X00_PLATFORM
static int bq27000_read_platform(struct bq27x00_device_info *di, u8 reg,
			bool single)
{
	struct device *dev = di->dev;
	struct bq27000_platform_data *pdata = dev->platform_data;
	unsigned int timeout = 3;
	int upper, lower;
	int temp;
	if (!single) {
		/* Make sure the value has not changed in between reading the
		 * lower and the upper part */
		upper = pdata->read(dev, reg + 1);
		do {
			temp = upper;
			if (upper < 0)
				return upper;
			lower = pdata->read(dev, reg);
			if (lower < 0)
				return lower;
			upper = pdata->read(dev, reg + 1);
		} while (temp != upper && --timeout);
		if (timeout == 0)
			return -EIO;
		return (upper << 8) | lower;
	}
	return pdata->read(dev, reg);
}
static int __devinit bq27000_battery_probe(struct platform_device *pdev)
{
	struct bq27x00_device_info *di;
	struct bq27000_platform_data *pdata = pdev->dev.platform_data;
	int ret;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data supplied\n");
		return -EINVAL;
	}
	if (!pdata->read) {
		dev_err(&pdev->dev, "no hdq read callback supplied\n");
		return -EINVAL;
	}
	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&pdev->dev, "failed to allocate device info data\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, di);
	di->dev = &pdev->dev;
	di->chip = BQ27200;
	di->bat.name = pdata->name ?: dev_name(&pdev->dev);
	di->bus.read = &bq27000_read_platform;
	ret = bq27x00_powersupply_init(di);
	if (ret)
		goto err_free;
	return 0;
err_free:
	kfree(di);
	return ret;
}
static int __devexit bq27000_battery_remove(struct platform_device *pdev)
{
	struct bq27x00_device_info *di = platform_get_drvdata(pdev);
	bq27x00_powersupply_unregister(di);
	kfree(di);
	return 0;
}
static struct platform_driver __initdata bq27000_battery_driver = {
	.probe	= bq27000_battery_probe,
	.remove = __devexit_p(bq27000_battery_remove),
	.driver = {
		.name = "bq27000-battery",
		.owner = THIS_MODULE,
	},
};
static inline int bq27x00_battery_platform_init(void)
{
	int ret = platform_driver_register(&bq27000_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27200 platform driver\n");
	return ret;
}
static inline void bq27x00_battery_platform_exit(void)
{
	platform_driver_unregister(&bq27000_battery_driver);
}
#else
static inline int bq27x00_battery_platform_init(void) { return 0; }
static inline void bq27x00_battery_platform_exit(void) {};
#endif
/*
 * Module stuff
 */

static int  fgauge_initialization(void *data)
{

	int ret;

	bat_meter_data = (struct mt_battery_meter_custom_data *) data;
	ret = bq27x00_battery_read_fw_version(bq27x00_di);
	printk("Guage fw version is 0x%04x\n", ret);
	ret = bq27x00_battery_read_device_type(bq27x00_di);
	printk("Guage device type is 0x%04x\n", ret);
	ret = bq27x00_battery_read_dataflash_version(bq27x00_di);
	printk("Guage dataflash version is 0x%04x\n", ret);

	return 0;
}

static int  fgauge_read_current(void *data)
{
	int value;
	value = bq27x00_read_battery_current(bq27x00_di);
	*(int *) data = value/1000;
	return value/1000;
}

static int fgauge_read_current_sign(void *data)
{
	return 1;
}

static int fgauge_read_columb(void *data)
{
	int soc;
	int ufsoc;
	soc = bq27x00_battery_read_soc(bq27x00_di);
	ufsoc = bq27x00_battery_read_ufsoc(bq27x00_di);
	printk("BQ27520 soc = %d and ufsoc = %d\n", soc, ufsoc);
	*(int *) data = soc;
	return soc;
}

static int fgauge_hw_reset(void *data)
{
	bq27x00_battery_reset(bq27x00_di);
	printk("BQ27520 send reset command\n");
	return 0;
}

static int read_adc_v_bat_sense(void *data)
{
	int value;
	value = PMIC_IMM_GetOneChannelValue(1, *(int *) (data), 1);
	*(int *) data = value;
	return value;
}

static int read_adc_v_i_sense(void *data)
{
	int value;
	value =  PMIC_IMM_GetOneChannelValue(0, *(int *) (data), 1);
	*(int *) data = value;
	return value;
}

static int read_adc_v_bat_temp(void *data)
{
	int value;
	if (gauge_firmware_load_flag == 1) {
	printk("firmware is not ready\n");
		*(int *) data = 25;
		return 25;
	}
	value = bq27x00_battery_read_temperature(bq27x00_di);
	value = (value - 2731)/10;
	*(int *) data = value;
	return value;
}

static int read_adc_v_charger(void *data)
{
	int  val;
	val = PMIC_IMM_GetOneChannelValue(2, *(s32 *) (data), 1);
		val = val / 100;
	*(int *) data = val;
	return val;
}

static int read_hw_ocv(void *data)
{
	int value;
	value =  bq27x00_read_battery_voltage(bq27x00_di);
	*(int *) data = value/1000;
	return value/1000;
}

static int dump_register_fgadc(void *data)
{
	int i;
	int control_status;
	unsigned int reg1[60];
	unsigned int reg2[10];

	if (bq27x00_di == NULL)
		return -EINVAL;

	control_status = control_cmd_read(bq27x00_di, CONTROL_STATUS_SUBCMD);
	printk(" BQ27x00  fuelgauge control status value %d\n", control_status);

	/* a3:01: means a3 product version 01 debug log */
	printk("dump BQ27x00  fuelgauge regs for debug from 1-0x3B:\n");
	for (i = 1; i <= 0x17; i++) {
		reg1[2*i] =  bq27x00_di->bus.read(bq27x00_di, 2*i, false);
		if (reg1[2*i] < 0)
			printk("BQ2589X_REG_0x%02x read fail\n", reg1[2*i]);
		printk("reg[0x%02x]:%d ", 2*i, reg1[2*i]);
	}
	printk("\n");
	for (i = 0x62; i <= 0x75; i++) {
		if (i%2 == 0) {
			reg2[i] =  bq27x00_di->bus.read(bq27x00_di, i, false);
			if (reg2[i] < 0)
				printk("BQ2589X_REG_0x%02x read fail\n", reg2[i]);
			printk("reg[0x%02x]:%d ", i, reg2[i]);
		}
	}
	printk("\n");

	return 0;
}

static s32(*const bm_func[BATTERY_METER_CMD_NUMBER]) (void *data) = {
	fgauge_initialization,
	fgauge_read_current,
	fgauge_read_current_sign,
	fgauge_read_columb,
	fgauge_hw_reset,
	read_adc_v_bat_sense,
	read_adc_v_i_sense,
	read_adc_v_bat_temp,
	read_adc_v_charger,
	read_hw_ocv,
	dump_register_fgadc};

s32 bm_ctrl_cmd(int cmd, void *data)
{
	s32 status;

	if ((cmd < BATTERY_METER_CMD_NUMBER) && (bm_func[cmd] != NULL))
		status = bm_func[cmd] (data);
	else
		return -EINVAL;

	return status;
}

static int __init bq27x00_battery_init(void)
{
	int ret;
	ret = bq27x00_battery_i2c_init();
	if (ret)
		return ret;
	ret = bq27x00_battery_platform_init();
	if (ret)
		bq27x00_battery_i2c_exit();
	return ret;
}

module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_platform_exit();
	bq27x00_battery_i2c_exit();
}

module_exit(bq27x00_battery_exit);
MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
