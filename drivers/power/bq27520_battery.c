/*
 * bqGauge battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
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
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__
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
#include <linux/acpi.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/power/battery_id.h>
#include "power_supply_charger.h"
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>


#define DRIVER_VERSION            "1.3.0"
#define CURRENT_FW_VER		  0x0328
#define I2C_RETRY_CNT    3
#define BQGAUGE_I2C_ROM_ADDR    (0x16 >> 1)
#define BQGAUGE_I2C_DEV_ADDR    (0xAA >> 1)

#define I2C_MAX_TRANSFER_LEN	128
#define MAX_ASC_PER_LINE		400
#define FIRMWARE_FILE_SIZE	(3301*400)

#define BQ27520_SOCINT_NAME		"bq27520_gpio_int"

struct bq27520_device_info;

struct bqGauge_Device {
	/* interface to report property request from host */
	void (*updater)(struct bq27520_device_info *di);
	int (*read_fw_ver)(struct bq27520_device_info *di);
	int (*read_status)(struct bq27520_device_info *di);
	int (*read_fcc)(struct bq27520_device_info *di);
	int (*read_designcap)(struct bq27520_device_info *di);
	int (*read_rsoc)(struct bq27520_device_info *di);
	int (*read_temperature)(struct bq27520_device_info *di);
	int (*read_cyclecount)(struct bq27520_device_info *di);
	int (*read_timetoempty)(struct bq27520_device_info *di);
	int (*read_timetofull)(struct bq27520_device_info *di);
	int (*read_health)(struct bq27520_device_info *di);
	int (*read_voltage)(struct bq27520_device_info *di);
	int (*read_current)(struct bq27520_device_info *di);
	int (*read_capacity_level)(struct bq27520_device_info *di);
	int (*read_control_reg)(struct bq27520_device_info *di);
	int (*read_fac)(struct bq27520_device_info *di);
	int (*read_rc)(struct bq27520_device_info *di);
	int (*read_dc)(struct bq27520_device_info *di);
	int (*read_opconfig)(struct bq27520_device_info *di);
};


enum bqGauge_chip {BQ27520, BQ27320};

static DEFINE_MUTEX(battery_mutex);
static DEFINE_IDR(battery_id);

struct bq27520_reg_cache {
	int temperature;
	int voltage;
	int currentI;
	int time_to_empty;
	int time_to_full;
	int charge_full;
	int charge_design_full;
	int cycle_count;
	int rsoc;
	int flags;
	int health;
	int ext_set_rsoc;
};

struct bq27520_device_info {
	struct  device        *dev;
	struct  bqGauge_Device *gauge;

	int     id;
	int     soc_int_irq;
	enum    bqGauge_chip    chip;

	int     fw_ver;	/* format : AABBCCDD: AABB version, CCDD build number */
	int     df_ver;

	struct  bq27520_reg_cache cache;

	unsigned long last_update;

	struct  delayed_work work;
	struct  delayed_work fw_dl_work;
	struct  delayed_work soc_work;
	struct  power_supply bat;
	struct  power_supply *usb_psy;

	struct  mutex lock;
};

static enum power_supply_property bq27520_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static unsigned int poll_interval = 60;
module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - 0 disables polling");

static unsigned int debug_mask;
module_param(debug_mask, uint, 0644);
MODULE_PARM_DESC(debug_mask, "debug switch");

/* common routines for bq I2C gauge */
static int bq_read_i2c_word(struct bq27520_device_info *di, u8 reg)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;
	int i = 0;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	msg[1].len = 2;

	mutex_lock(&battery_mutex);
	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (ret >= 0)
			break;
		msleep(20);
	}
	mutex_unlock(&battery_mutex);
	if (ret < 0)
		return ret;

	ret = get_unaligned_le16(data);

	return ret;
}

static int bq_write_i2c_word(struct bq27520_device_info *di, u8 reg, int value)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	unsigned char data[4];
	int ret;
	int i = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = reg;
	put_unaligned_le16(value, &data[1]);

	msg.len = 3;
	msg.buf = data;
	msg.addr = client->addr;
	msg.flags = 0;

	mutex_lock(&battery_mutex);
	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;
		msleep(20);
	}
	mutex_unlock(&battery_mutex);
	if (ret < 0)
		return ret;

	return 0;
}

static int bq_read_i2c_blk(struct bq27520_device_info *di,
		u8 reg, u8 *data, u8 len)
{

	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	int ret;
	int i = 0;

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

	mutex_lock(&battery_mutex);
	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (ret >= 0)
			break;
		msleep(20);
	}
	mutex_unlock(&battery_mutex);

	if (ret < 0)
		return ret;

	return ret;
}

static int bq_write_i2c_blk(struct bq27520_device_info *di,
		u8 reg, u8 *data, u8 sz)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg;
	int ret;
	int i = 0;
	u8 buf[200];

	if (!client->adapter)
		return -ENODEV;

	buf[0] = reg;
	memcpy(&buf[1], data, sz);

	msg.buf = buf;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sz + 1;

	mutex_lock(&battery_mutex);
	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;
		msleep(20);
	}
	mutex_unlock(&battery_mutex);
	if (ret < 0)
		return ret;

	return 0;
}

/* bq27520 device stuff */
#define BQ27520_REG_CONTRL          0x00
#define BQ27520_REG_FLAGS           0x0A
#define BQ27520_REG_FCC             0x12
#define BQ27520_REG_RSOC            0x20
#define BQ27520_REG_DESIGNCAP       0x2E
#define BQ27520_REG_TEMP            0x06
#define BQ27520_REG_CYCLECNT        0x1E
#define BQ27520_REG_TTE             0x16
#define BQ27520_REG_VOLT            0x08
#define BQ27520_REG_CURRENT         0x14
#define BQ27520_REG_OPSTATUS        0x2C

#define BQ27520_REG_UFRM            0x6c
#define BQ27520_REG_UFFCC            0x70
#define BQ27520_REG_UFSOC            0x74

#define BQ27520_REG_FAC				0x0E
#define BQ27520_REG_RC				0x10

#define BQ27520_REG_DFCLASS         0x3E
#define BQ27520_REG_DFBLOCK         0x3F
#define BQ27520_REG_BLOCKDATA       0x40

#define BQ27520_REG_BLKCHKSUM       0x60
#define BQ27520_REG_BLKDATCTL       0x61

#define BQ27520_SECURITY_SEALED     0x03
#define BQ27520_SECURITY_UNSEALED   0x02
#define BQ27520_SECURITY_FA         0x01
#define BQ27520_SECURITY_MASK       0x03


#define BQ27520_UNSEAL_KEY          0x36720414
#define BQ27520_FA_KEY              0xFFFFFFFF


#define BQ27520_FLAG_DSG            BIT(0)
#define BQ27520_FLAG_OTC            BIT(15)
#define BQ27520_FLAG_OTD            BIT(14)
#define BQ27520_FLAG_FC             BIT(9)

#define BQ27520_SUBCMD_FLAGS        0x0000
#define BQ27520_SUBCMD_FWVER        0x0002
#define BQ27520_SUBCMD_DFVER        0x001F
#define BQ27520_SUBCMD_ENTER_ROM    0x0F00
#define BQ27520_SUBCMD_SEAL         0x0020
#define BQ27520_SUBCMD_RESET        0x0021

static int bq27520_read_fw_version(struct bq27520_device_info *di)
{
	int ret;

	ret = bq_write_i2c_word(di, BQ27520_REG_CONTRL, BQ27520_SUBCMD_FWVER);
	if (ret < 0) {
		dev_err(di->dev, "Failed to send read fw version command\n");
		return ret;
	}
	mdelay(2);
	ret = bq_read_i2c_word(di, BQ27520_REG_CONTRL);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read read fw version\n");
		return ret;
	}

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG FW Ver(%x): %x\n", BQ27520_REG_CONTRL, ret);


	pr_info("firmware ver is 0x%04x", ret);
	return ret;
}

static int bq27520_read_df_version(struct bq27520_device_info *di)
{
	int ret;

	ret = bq_write_i2c_word(di, BQ27520_REG_CONTRL, BQ27520_SUBCMD_DFVER);
	if (ret < 0) {
		dev_err(di->dev, "Failed to send read fw version command\n");
		return ret;
	}
	mdelay(2);
	ret = bq_read_i2c_word(di, BQ27520_REG_CONTRL);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read read fw version\n");
		return ret;
	}

	pr_info("data flash ver is 0x%04x", ret);
	return ret;
}


static int bq27520_read_current(struct bq27520_device_info *);

static int bq27520_read_status(struct bq27520_device_info *di)
{
	int flags;
	int status;
	int curr;

	curr = bq27520_read_current(di);
	mdelay(2);
	flags = bq_read_i2c_word(di, BQ27520_REG_FLAGS);
	if (flags < 0) {
		dev_err(di->dev, "Failed to read status:%d\n", flags);
		flags = di->cache.flags;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG FLAGS(%x): %x\n", BQ27520_REG_FLAGS, flags);
	di->cache.flags = flags;

	if (flags & BQ27520_FLAG_DSG)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else if (flags & BQ27520_FLAG_FC)
		status = POWER_SUPPLY_STATUS_FULL;
	else if (curr < 0)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return status;
}


static int bq27520_read_fcc(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_FCC);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read FullChargeCapacity:%d\n", ret);
		ret = di->cache.charge_full;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG FCC(%x): %x\n", BQ27520_REG_FCC, ret);
	di->cache.charge_full = ret;

	return ret * 1000;
}

static int bq27520_read_designcapacity(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_DESIGNCAP);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read FullChargeCapacity:%d\n", ret);
		ret = di->cache.charge_design_full;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG DC(%x): %x\n", BQ27520_REG_DESIGNCAP, ret);
	di->cache.charge_design_full = ret;

	return ret * 1000;
}

static int bq27520_read_rsoc(struct bq27520_device_info *di)
{
	int ret, ufsoc;
	ret = bq_read_i2c_word(di, BQ27520_REG_RSOC);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read RSOC:%d\n", ret);
		ret = di->cache.rsoc;
	}
	if (100 == ret) {
		if (bq27520_read_current(di) > 300000) {
			ufsoc = bq_read_i2c_word(di, BQ27520_REG_UFSOC);
			printk("BQ27520 ufsoc is %d \n", ufsoc);
			if (ufsoc <= 95)
				bq_write_i2c_word(di, BQ27520_REG_CONTRL, BQ27520_SUBCMD_RESET);
		}
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG SOC(%x): %x\n", BQ27520_REG_RSOC, ret);
	di->cache.rsoc = ret;

	return ret;
}

static int bq27520_read_temperature(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_TEMP);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read TEMP:%d\n", ret);
		ret = di->cache.temperature;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG TEMP(%x): %x\n", BQ27520_REG_TEMP, ret);
	di->cache.temperature = ret;

	return ret;
}

static int bq27520_read_cyclecount(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_CYCLECNT);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read CycleCount:%d\n", ret);
		ret = di->cache.cycle_count;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG CC(%x): %x\n", BQ27520_REG_CYCLECNT, ret);
	di->cache.cycle_count = ret;

	return ret;
}

static int bq27520_read_timetoempty(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_TTE);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read TimeToEmpty:%d\n", ret);
		ret = di->cache.time_to_empty;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG TTE(%x): %x\n", BQ27520_REG_TTE, ret);
	di->cache.time_to_empty = ret;

	return ret;
}

static int bq27520_read_health(struct bq27520_device_info *di)
{
	int flags;
	int status;
	flags = bq_read_i2c_word(di, BQ27520_REG_FLAGS);
	if (flags < 0) {
		dev_err(di->dev, "Failed to read BatteryStatus:%d\n", flags);
		flags = di->cache.flags;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG FLAGS(%x): %x\n", BQ27520_REG_FLAGS, flags);
	di->cache.flags = flags;
	if (flags & (BQ27520_FLAG_OTC | BQ27520_FLAG_OTD))
		status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		status = POWER_SUPPLY_HEALTH_GOOD;

	return status;
}

static int bq27520_read_voltage(struct bq27520_device_info *di)
{

	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_VOLT);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read Voltage:%d\n", ret);
		ret = di->cache.voltage;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG VOLT(%x): %x\n", BQ27520_REG_VOLT, ret);
	di->cache.voltage = ret;

	return ret * 1000;
}

static int bq27520_read_current(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_CURRENT);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read Curent:%d\n", ret);
		ret = di->cache.currentI;
	}
	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG CURR(%x): %x\n", BQ27520_REG_CURRENT, ret);
	di->cache.currentI = ret;

	return (int)((s16)ret) * -1000;
}

static int bq27520_read_capacity_level(struct bq27520_device_info *di)
{
	int level;
	int rsoc;
	int flags;

	rsoc = bq27520_read_rsoc(di);
	flags = bq27520_read_status(di);
	if (rsoc > 95)
		level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (flags & 0x02) /* SYSDOWN */
		level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (flags & 0x04)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else
		level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

	return level;
}

static int bq27520_read_control_reg(struct bq27520_device_info *di)
{
	int ret;

	ret = bq_write_i2c_word(di, BQ27520_REG_CONTRL, BQ27520_SUBCMD_FLAGS);
	if (ret < 0) {
		dev_err(di->dev, "Failed to send read control status command\n");
		return ret;
	}
	mdelay(2);
	ret = bq_read_i2c_word(di, BQ27520_REG_CONTRL);
	if (ret < 0) {
		dev_err(di->dev, "Failed to read read fw version\n");
		return ret;
	}

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG Control(%x): %x\n", BQ27520_REG_CONTRL, ret);
	return ret;
}

static int bq27520_read_fac(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_FAC);
	if (ret < 0)
		dev_err(di->dev, "Failed to read FAC:%d\n", ret);

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG FAC(%x): %x\n", BQ27520_REG_FAC, ret);

	return ret;
}

static int bq27520_read_rc(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_RC);
	if (ret < 0)
		dev_err(di->dev, "Failed to read RC:%d\n", ret);

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG RC(%x): %x\n", BQ27520_REG_RC, ret);

	return ret;
}

static int bq27520_read_dc(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_DESIGNCAP);
	if (ret < 0)
		dev_err(di->dev, "Failed to read DC:%d\n", ret);

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG DC(%x): %x\n", BQ27520_REG_DESIGNCAP, ret);

	return ret;
}

static int bq27520_read_opconfig(struct bq27520_device_info *di)
{
	int ret;
	ret = bq_read_i2c_word(di, BQ27520_REG_OPSTATUS);
	if (ret < 0)
		dev_err(di->dev, "Failed to read OpConfig:%d\n", ret);

	if (debug_mask)
		printk(KERN_ERR "BQ27520-REG OpConfig(%x): %x\n", BQ27520_REG_OPSTATUS, ret);

	return ret;
}

static struct bqGauge_Device bqGauge_27520 = {
	.updater            = NULL,
	.read_fw_ver        = bq27520_read_fw_version,
	.read_status        = bq27520_read_status,
	.read_fcc           = bq27520_read_fcc,
	.read_designcap     = bq27520_read_designcapacity,
	.read_rsoc          = bq27520_read_rsoc,
	.read_health        = bq27520_read_health,
	.read_voltage       = bq27520_read_voltage,
	.read_current       = bq27520_read_current,
	.read_temperature   = bq27520_read_temperature,
	.read_cyclecount    = bq27520_read_cyclecount,
	.read_timetoempty   = bq27520_read_timetoempty,
	.read_timetofull    = NULL,
	.read_capacity_level = bq27520_read_capacity_level,
	.read_control_reg	= bq27520_read_control_reg,
	.read_fac			= bq27520_read_fac,
	.read_rc			= bq27520_read_rc,
	.read_dc			= bq27520_read_dc,
	.read_opconfig		= bq27520_read_opconfig,
};

#define BQ27520_DEVICE_NAME_CLASSID     48
#define BQ27520_DEVICE_NAME_OFFSET      17
#define BQ27520_DEVICE_NAME_LENGTH      7

static void bq27520_refresh(struct bq27520_device_info *di)
{
	struct bq27520_reg_cache cache = {0, };

	if (!di->gauge)
		return;

	cache = di->cache;
	mdelay(2);
	if (debug_mask) {
		if (di->gauge->read_control_reg) {
			di->gauge->read_control_reg(di);
			mdelay(2);
		}

		if (di->gauge->read_fac) {
			di->gauge->read_fac(di);
			mdelay(2);
		}

		if (di->gauge->read_rc) {
			di->gauge->read_rc(di);
			mdelay(2);
		}

		if (di->gauge->read_dc) {
			di->gauge->read_dc(di);
			mdelay(2);
		}

		if (di->gauge->read_opconfig) {
			di->gauge->read_opconfig(di);
			mdelay(2);
		}
	}

	if (di->gauge->read_status) {
		di->gauge->read_status(di);
		mdelay(2);
	}

	if (di->gauge->read_current) {
		di->gauge->read_current(di);
		mdelay(2);
	}

	if (di->gauge->read_voltage) {
		di->gauge->read_voltage(di);
		mdelay(2);
	}

	if (di->gauge->read_temperature) {
		di->gauge->read_temperature(di);
		mdelay(2);
	}

	if (di->gauge->read_timetoempty) {
		di->gauge->read_timetoempty(di);
		mdelay(2);
	}

	if (di->gauge->read_timetofull) {
		di->gauge->read_timetofull(di);
		mdelay(2);
	}

	if (di->gauge->read_fcc) {
		di->gauge->read_fcc(di);
		mdelay(2);
	}

	if (di->gauge->read_rsoc) {
		di->gauge->read_rsoc(di);
		mdelay(2);
	}

	if (memcmp(&di->cache, &cache, sizeof(cache)))
		power_supply_changed(&di->bat);

	di->last_update = jiffies;
}

static void bq27520_battery_poll(struct work_struct *work)
{
	struct bq27520_device_info *di =
		container_of(work, struct bq27520_device_info, work.work);


	bq27520_refresh(di);

	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&di->work, poll_interval * HZ);
	}
}

#define to_bq27520_device_info(x) container_of((x), \
		struct bq27520_device_info, bat);
static int get_battery_status(struct power_supply *psy)
{
	int cnt, status, is_vbus_good;
	union power_supply_propval val = {0, };
	struct power_supply *chrgr_lst[5];
	struct bq27520_device_info *di = to_bq27520_device_info(psy);

	status = di->gauge->read_status(di);

	if (NULL == di->usb_psy)
		di->usb_psy = power_supply_get_by_name("bq2589x_charger");

	if (NULL == di->usb_psy) {
		pr_info("can't not find usb psy\n");
		return status;
	} else
		di->usb_psy->get_property(di->usb_psy,
			POWER_SUPPLY_PROP_VBUS_GOOD, &val);

	is_vbus_good = val.intval;

	if (is_vbus_good > 0 && IS_CHARGING_ENABLED(di->usb_psy)
				&& POWER_SUPPLY_STATUS_FULL != status)
		status = POWER_SUPPLY_STATUS_CHARGING;

	if ((is_vbus_good <= 0) || !IS_CHARGING_ENABLED(di->usb_psy))
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	pr_info_ratelimited("%s Set status=%d vbus good %d for %s\n", __func__,
			status, is_vbus_good, psy->name);

	return status;
}


static int bq27520_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct bq27520_device_info *di = to_bq27520_device_info(psy);

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27520_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	if (di->gauge == NULL)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_battery_status(psy);
	break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (di->gauge->read_voltage)
			val->intval = di->gauge->read_voltage(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (di->gauge->read_voltage)
			val->intval = di->gauge->read_voltage(di) > 0?1:0;
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (di->gauge->read_current)
			val->intval = di->gauge->read_current(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (di->cache.ext_set_rsoc != -EINVAL) {
			val->intval = di->cache.ext_set_rsoc;
			break;
		}
		if (di->gauge->read_rsoc)
			val->intval = di->gauge->read_rsoc(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_TEMP:
		if (di->gauge->read_temperature)
			val->intval = di->gauge->read_temperature(di)-2731;
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		if (di->gauge->read_timetoempty)
			val->intval = di->gauge->read_timetoempty(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (di->gauge->read_fcc)
			val->intval = di->gauge->read_fcc(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (di->gauge->read_designcap)
			val->intval = di->gauge->read_designcap(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (di->gauge->read_cyclecount)
			val->intval = di->gauge->read_cyclecount(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (di->gauge->read_health)
			val->intval = di->gauge->read_health(di);
		else
			val->intval = -EINVAL;
	break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq27520_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int ret = 0;
	struct bq27520_device_info *di = to_bq27520_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
			di->cache.ext_set_rsoc = val->intval;
	break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int bq27520_battery_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}
static void bq27520_external_power_changed(struct power_supply *psy)
{
	struct bq27520_device_info *di = to_bq27520_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static void set_properties_array(struct bq27520_device_info *di,
		enum power_supply_property *props, int num_props)
{
	int tot_sz = num_props * sizeof(enum power_supply_property);

	di->bat.properties = devm_kzalloc(di->dev, tot_sz, GFP_KERNEL);

	if (di->bat.properties) {
		memcpy(di->bat.properties, props, tot_sz);
		di->bat.num_properties = num_props;
	} else
		di->bat.num_properties = 0;
}

static void bq27520_dl_fw(struct work_struct *work);
static void bq27520_soc_work(struct work_struct *work);
static int bq27520_powersupply_init(struct bq27520_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;

	set_properties_array(di, bq27520_battery_props,
			ARRAY_SIZE(bq27520_battery_props));

	di->bat.get_property = bq27520_get_property;
	di->bat.set_property = bq27520_set_property;
	di->bat.external_power_changed = bq27520_external_power_changed;
	di->bat.property_is_writeable = bq27520_battery_is_writeable;

	INIT_DELAYED_WORK(&di->work, bq27520_battery_poll);
	INIT_DELAYED_WORK(&di->fw_dl_work, bq27520_dl_fw);
	INIT_DELAYED_WORK(&di->soc_work, bq27520_soc_work);
	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	bq27520_refresh(di);

	return 0;
}

static void bq27520_powersupply_unregister(struct bq27520_device_info *di)
{
	/*
	 * power_supply_unregister call bqGauge_battery_get_property which
	 * call bq27520_battery_poll.
	 * Make sure that bq27520_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(&di->bat);

	mutex_destroy(&di->lock);
}

static int bq27520_atoi(const char *s)
{
	int k = 0;

	k = 0;
	while (*s != '\0' && *s >= '0' && *s <= '9') {
		k = 10 * k + (*s - '0');
		s++;
	}
	return k;
}

static unsigned long bq27520_strtoul(const char *cp, unsigned int base)
{
	unsigned long result = 0, value;

	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
					? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}

	return result;
}

static int bq27520_firmware_program(struct bq27520_device_info *di,
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
	if (ulCounter > 10) {
		client->addr = BQGAUGE_I2C_DEV_ADDR;
		return -EPERM;
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
				p_src[2+j] = bq27520_strtoul(ucTmpBuf, 16);
			}

			temp = (ulLineLen - 2)/2;
			ulLineLen = temp + 2;
		} else if ('X' == p_src[0]) {
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+2, ulLineLen-2);
			ulDelay = bq27520_atoi(ucTmpBuf);
		} else if ('R' == p_src[0]) {
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+2, 2);
			p_src[2] = bq27520_strtoul(ucTmpBuf, 16);
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+4, 2);
			p_src[3] = bq27520_strtoul(ucTmpBuf, 16);
			memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
			memcpy(ucTmpBuf, pBuf+6, ulLineLen-6);
			ulReadNum = bq27520_atoi(ucTmpBuf);
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
				if (bq_write_i2c_blk(di,
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
				if (bq_read_i2c_blk(di, p_src[3],
							p_dst, ulReadNum) < 0) {
					dev_err(di->dev, "bq_read_i2c_blk failed\n");
				}
			break;
			case 'C':
				client->addr = p_src[2] >> 1;
				if (bq_read_i2c_blk(
					di, p_src[3],
					p_dst,
					ulLineLen-4) < 0 ||
					memcmp(p_dst,
						&p_src[4],
						ulLineLen-4) != 0) {
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
				return -EPERM;
			}
		}
	}

	client->addr = BQGAUGE_I2C_DEV_ADDR;

	return 0;
}

static int bq27520_firmware_download(struct bq27520_device_info *di,
		const unsigned char *pgm_data, unsigned int len)
{
	int ret = 0;
	/*program bqfs*/
	ret = bq27520_firmware_program(di, pgm_data, len);
	if (0 != ret)
		dev_err(di->dev, "bq27520_firmware_program failed\n");

	return ret;
}

static int bq27520_update_firmware(struct bq27520_device_info *di,
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
		return -EPERM;
	}

	if (!filp->f_op) {
		dev_err(di->dev, "File Operation Method Error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -EPERM;
	}
	inode = filp->f_path.dentry->d_inode;
	if (!inode) {
		dev_err(di->dev, "Get inode from filp failed\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -EPERM;
	}

	/* file's size */
	length = i_size_read(inode->i_mapping->host);
	dev_info(di->dev, "bq27520 firmware image size=%d\n", length);
	if (!(length > 0 && length < FIRMWARE_FILE_SIZE)) {
		dev_err(di->dev, "Get file size error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -EPERM;
	}

	/* allocation buff size */
	buf = vmalloc(length+(length%2));       /* buf size if even */
	if (!buf) {
		dev_err(di->dev, "Alloctation memory failed\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -EPERM;
	}

	/* read data */
	if (filp->f_op->read(filp, buf, length, &filp->f_pos) != length) {
		dev_err(di->dev, "File read error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		vfree(buf);
		return -EPERM;
	}

	ret = bq27520_firmware_download(di, (const unsigned char *)buf, length);

	filp_close(filp, NULL);
	set_fs(oldfs);
	vfree(buf);

	return ret;
}

/* If the system has several batteries we need a different name for each
 * of them...
 */
static ssize_t store_firmware_update(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned char path_image[255];
	struct bq27520_device_info *di = dev_get_drvdata(dev);

	if (di == NULL || NULL == buf || count >= 255 || count == 0
			|| strnchr(buf, count, 0x20))
		return -EPERM;

	memcpy(path_image, buf, count);
	/* replace '\n' with  '\0'  */
	if ((path_image[count-1]) == '\n')
		path_image[count-1] = '\0';
	else
		path_image[count] = '\0';
	ret = bq27520_update_firmware(di, path_image);
	msleep(3000);

	if (ret == 0) {
		dev_info(di->dev, "Update firemware finished...\n");
		ret = count;
	}

	return ret;
}

static ssize_t show_firmware_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27520_device_info *di = dev_get_drvdata(dev);
	int ver = 0;
	if (di->gauge && di->gauge->read_fw_ver)
		ver = di->gauge->read_fw_ver(di);

	return sprintf(buf, "%04x\n", ver);
}

static ssize_t show_device_regs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27520_device_info *di = dev_get_drvdata(dev);

	bq27520_refresh(di);

	return sprintf(buf,
"V:%d, C:%d, T:%d, RSOC:%d, FCC:%d, TTE:%d, Flags:%02X\n",
			di->cache.voltage,
			(s16)di->cache.currentI,
			di->cache.temperature,
			di->cache.rsoc,
			di->cache.charge_full,
			di->cache.time_to_empty,
			di->cache.flags);
}

static DEVICE_ATTR(fw_version, 0664,
		show_firmware_version,
		store_firmware_update);
static DEVICE_ATTR(show_regs, S_IRUGO,
		show_device_regs, NULL);

static struct attribute *bq27520_attributes[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_show_regs.attr,
	NULL
};

#define CURRENT_LG_FW_DF_VER		0x0102
#define CURRENT_SS_FW_DF_VER		0x0202
#define LG_PROFILE			"/system/etc/lg.dffs"
#define SS_PROFILE			"/system/etc/ss.dffs"
static void bq27520_dl_fw(struct work_struct *work)
{
	char *path;
	int ret = 0, df_ver = 0, batt_ver = 0;
	struct ps_batt_chg_prof chrg_profile;
	struct ps_pse_mod_prof *bprof;
	struct bq27520_device_info *di = container_of(work,
		struct bq27520_device_info, fw_dl_work.work);

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
		/* if we can't get batt id ,use lg profile instead */
		pr_info("UNKNOWN battery\n");
		batt_ver = CURRENT_LG_FW_DF_VER;
		path = LG_PROFILE;
	}

	df_ver = bq27520_read_df_version(di);
	pr_info("batt ver %x, df ver %x\n", batt_ver, df_ver);
	if (df_ver == batt_ver)
		return;

	pr_info("Update firmware\n");
	ret = bq27520_update_firmware(di, path);
	msleep(3000);

	if (ret == 0)
		dev_info(di->dev, "Update firmware finished...\n");
	else
		dev_info(di->dev, "Update firmware failed...\n");

}

static void bq27520_soc_work(struct work_struct *work)
{
	struct bq27520_device_info *di = container_of(work,
		struct bq27520_device_info, soc_work.work);

	pr_info("adjust_soc: s %d i %d v %d t %d\n",
			bq27520_read_rsoc(di), bq27520_read_current(di),
			bq27520_read_voltage(di), bq27520_read_temperature(di));

	schedule_delayed_work(&di->soc_work, 20 * HZ);
}
static const struct attribute_group bq27520_attr_group = {
	.attrs = bq27520_attributes,
};

static irqreturn_t bq27520_irq_thread(int irq, void *devid)
{
	int soc;
	union power_supply_propval val = {0, };
	struct bq27520_device_info *di = (struct bq27520_device_info *)devid;

	dev_err(di->dev, "BQ27520 interrupt occurred!\n");

	soc = bq27520_read_rsoc(di);

	if (NULL == di->usb_psy)
		di->usb_psy = power_supply_get_by_name("bq2589x_charger");

	if (NULL == di->usb_psy) {
		pr_info("can't not find usb psy\n");
		return IRQ_HANDLED;
	}

	if (100 == soc)
		val.intval = 300;
	else
		val.intval = 800;
	di->usb_psy->set_property(di->usb_psy, POWER_SUPPLY_PROP_SET_VINDPM, &val);

	return IRQ_HANDLED;
}

static int bq27520_battery_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq27520_device_info *di;
	int retval = 0;
	int num;
	int index;
	struct gpio_desc *gpio;

	if (!client || !id)
		return -EPERM;
	/* Get new ID for the new battery device */
	mutex_lock(&battery_mutex);
	num = idr_alloc(&battery_id, client, 0, 0, GFP_KERNEL);
	mutex_unlock(&battery_mutex);
	if (num < 0)
		return num;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->bat.name = "battery";
	di->usb_psy = power_supply_get_by_name("bq2589x_charger");

	index = 0;
	gpio = devm_gpiod_get_index(&client->dev,
			BQ27520_SOCINT_NAME, index);
	if (IS_ERR(gpio)) {
		di->soc_int_irq = -1;
		dev_err(&client->dev, "Failed to get gpio interrupt\n");
		retval = PTR_ERR(gpio);
		goto batt_failed_3;
	}

	di->soc_int_irq = gpiod_to_irq(gpio);

	if (di->soc_int_irq < 0) {
		dev_err(&client->dev,
				"soc_int_irq GPIO is not available\n");
	} else {
		retval = request_threaded_irq(di->soc_int_irq,
				NULL, bq27520_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "BQ27520", di);
		if (retval) {
			dev_err(&client->dev,
					"Failed to register irq for pin %d\n",
					di->soc_int_irq);
		} else {
			dev_info(&client->dev,
					"Registered irq for pin %d\n",
					di->soc_int_irq);
		}
	}

	if (di->chip == BQ27520)
		di->gauge = &bqGauge_27520;
	else {
		dev_err(&client->dev,
				"Unexpected gas gauge: %d\n", di->chip);
		di->gauge = NULL;
	}

	di->cache.ext_set_rsoc = -EINVAL;

	i2c_set_clientdata(client, di);
	if (di->gauge && di->gauge->read_fw_ver)
		di->fw_ver = di->gauge->read_fw_ver(di);
	else
		di->fw_ver = 0x00;
	dev_info(&client->dev, "Gas Gauge fw version is 0x%04x\n", di->fw_ver);

	retval = bq27520_powersupply_init(di);
	if (retval)
		goto batt_failed_3;

	if (di->gauge && di->gauge->updater)
		di->gauge->updater(di);

	/* Schedule a polling after about 1 min */
	schedule_delayed_work(&di->work, 60 * HZ);
	schedule_delayed_work(&di->soc_work, 20 * HZ);
	schedule_delayed_work(&di->fw_dl_work, 60 * HZ);

	retval = sysfs_create_group(&client->dev.kobj, &bq27520_attr_group);
	if (retval)
		dev_err(&client->dev, "could not create sysfs files\n");
	return 0;

batt_failed_3:
	kfree(di);
batt_failed_2:
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, retval);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27520_battery_remove(struct i2c_client *client)
{
	struct bq27520_device_info *di = i2c_get_clientdata(client);

	bq27520_powersupply_unregister(di);

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);

	return 0;
}

static int bq27520_battery_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct bq27520_device_info *di = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&di->work);

	return 0;
}

static int bq27520_battery_resume(struct i2c_client *client)
{
	struct bq27520_device_info *di = i2c_get_clientdata(client);
	schedule_delayed_work(&di->work, 0);

	return 0;
}

#ifdef CONFIG_ACPI
static struct acpi_device_id bq27520_acpi_match[] = {
	{"TXN27520", BQ27520},
};
MODULE_DEVICE_TABLE(acpi, bq27520_acpi_match);
#endif

static const struct i2c_device_id bq27520_id[] = {
	{ "TXN27520:00", BQ27520 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27520_id);

static struct i2c_driver bq27520_battery_driver = {
	.driver = {
		.name = "bq27520_battery",
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(bq27520_acpi_match),
#endif
	},
	.probe      = bq27520_battery_probe,
	.suspend    = bq27520_battery_suspend,
	.resume     = bq27520_battery_resume,
	.remove     = bq27520_battery_remove,
	.id_table = bq27520_id,
};

static inline int  bq27520_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27520_battery_driver);
	if (ret)
		pr_err("Unable to register bqGauge i2c driver\n");

	return ret;
}

static inline void __exit bq27520_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27520_battery_driver);
}

/*
 * Module stuff
 */
static int  bq27520_battery_init(void)
{
	int ret;

	ret = bq27520_battery_i2c_init();

	return ret;
}
module_init(bq27520_battery_init);

static void bq27520_battery_exit(void)
{
	bq27520_battery_i2c_exit();
}
module_exit(bq27520_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("bqGauge battery monitor driver");
MODULE_LICENSE("GPL");
