/* Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * High Level description:
 * http://www.ti.com/lit/ds/symlink/bq28400.pdf
 * Thechnical Reference:
 * http://www.ti.com/lit/ug/sluu431/sluu431.pdf
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/printk.h>

#define BQ28400_NAME "bq28400"
#define BQ28400_REV  "1.0"

/* SBS Commands (page 63)  */

#define SBS_MANUFACTURER_ACCESS		0x00
#define SBS_BATTERY_MODE		0x03
#define SBS_TEMPERATURE			0x08
#define SBS_VOLTAGE			0x09
#define SBS_CURRENT			0x0A
#define SBS_AVG_CURRENT			0x0B
#define SBS_MAX_ERROR			0x0C
#define SBS_RSOC			0x0D	/* Relative State Of Charge */
#define SBS_REMAIN_CAPACITY		0x0F
#define SBS_FULL_CAPACITY		0x10
#define SBS_CHG_CURRENT			0x14
#define SBS_CHG_VOLTAGE			0x15
#define SBS_BATTERY_STATUS		0x16
#define SBS_CYCLE_COUNT			0x17
#define SBS_DESIGN_CAPACITY		0x18
#define SBS_DESIGN_VOLTAGE		0x19
#define SBS_SPEC_INFO			0x1A
#define SBS_MANUFACTURE_DATE		0x1B
#define SBS_SERIAL_NUMBER		0x1C
#define SBS_MANUFACTURER_NAME		0x20
#define SBS_DEVICE_NAME			0x21
#define SBS_DEVICE_CHEMISTRY		0x22
#define SBS_MANUFACTURER_DATA		0x23
#define SBS_AUTHENTICATE		0x2F
#define SBS_CELL_VOLTAGE1		0x3E
#define SBS_CELL_VOLTAGE2		0x3F

/* Extended SBS Commands (page 71)  */

#define SBS_FET_CONTROL			0x46
#define SBS_SAFETY_ALERT		0x50
#define SBS_SAFETY_STATUS		0x51
#define SBS_PE_ALERT			0x52
#define SBS_PE_STATUS			0x53
#define SBS_OPERATION_STATUS		0x54
#define SBS_CHARGING_STATUS		0x55
#define SBS_FET_STATUS			0x56
#define SBS_PACK_VOLTAGE		0x5A
#define SBS_TS0_TEMPERATURE		0x5E
#define SBS_FULL_ACCESS_KEY		0x61
#define SBS_PF_KEY			0x62
#define SBS_AUTH_KEY3			0x63
#define SBS_AUTH_KEY2			0x64
#define SBS_AUTH_KEY1			0x65
#define SBS_AUTH_KEY0			0x66
#define SBS_MANUFACTURER_INFO		0x70
#define SBS_SENSE_RESISTOR		0x71
#define SBS_TEMP_RANGE			0x72

/* SBS Sub-Commands (16 bits) */
/* SBS_MANUFACTURER_ACCESS CMD */
#define SUBCMD_DEVICE_TYPE		0x01
#define SUBCMD_FIRMWARE_VERSION		0x02
#define SUBCMD_HARDWARE_VERSION		0x03
#define SUBCMD_DF_CHECKSUM		0x04
#define SUBCMD_EDV			0x05
#define SUBCMD_CHEMISTRY_ID		0x08

/* SBS_CHARGING_STATUS */
#define CHG_STATUS_BATTERY_DEPLETED	BIT(0)
#define CHG_STATUS_OVERCHARGE		BIT(1)
#define CHG_STATUS_OVERCHARGE_CURRENT	BIT(2)
#define CHG_STATUS_OVERCHARGE_VOLTAGE	BIT(3)
#define CHG_STATUS_CELL_BALANCING	BIT(6)
#define CHG_STATUS_HOT_TEMP_CHARGING	BIT(8)
#define CHG_STATUS_STD1_TEMP_CHARGING	BIT(9)
#define CHG_STATUS_STD2_TEMP_CHARGING	BIT(10)
#define CHG_STATUS_LOW_TEMP_CHARGING	BIT(11)
#define CHG_STATUS_PRECHARGING_EXIT	BIT(13)
#define CHG_STATUS_SUSPENDED		BIT(14)
#define CHG_STATUS_DISABLED		BIT(15)

/* SBS_FET_STATUS */
#define FET_STATUS_DISCHARGE		BIT(1)
#define FET_STATUS_CHARGE		BIT(2)
#define FET_STATUS_PRECHARGE		BIT(3)

/* SBS_BATTERY_STATUS */
#define BAT_STATUS_SBS_ERROR		0x0F
#define BAT_STATUS_EMPTY		BIT(4)
#define BAT_STATUS_FULL			BIT(5)
#define BAT_STATUS_DISCHARGING		BIT(6)
#define BAT_STATUS_OVER_TEMPERATURE	BIT(12)
#define BAT_STATUS_OVER_CHARGED		BIT(15)

#define ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN   (-2731)
#define BQ_TERMINATION_CURRENT_MA	200

#define BQ_MAX_STR_LEN	32

struct bq28400_device {
	struct i2c_client	*client;
	struct delayed_work	periodic_user_space_update_work;
	struct dentry		*dent;
	struct power_supply	batt_psy;
	struct power_supply	*dc_psy;
	bool			is_charging_enabled;
	u32			temp_cold;	/* in degree celsius */
	u32			temp_hot;	/* in degree celsius */
};

static struct bq28400_device *bq28400_dev;

static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

struct debug_reg {
	char	*name;
	u8	reg;
	u16	subcmd;
};

static int fake_battery = -EINVAL;
module_param(fake_battery, int, 0644);

#define BQ28400_DEBUG_REG(x) {#x, SBS_##x, 0}
#define BQ28400_DEBUG_SUBREG(x, y) {#y, SBS_##x, SUBCMD_##y}

/* Note: Some register can be read only in Unsealed mode */
static struct debug_reg bq28400_debug_regs[] = {
	BQ28400_DEBUG_REG(MANUFACTURER_ACCESS),
	BQ28400_DEBUG_REG(BATTERY_MODE),
	BQ28400_DEBUG_REG(TEMPERATURE),
	BQ28400_DEBUG_REG(VOLTAGE),
	BQ28400_DEBUG_REG(CURRENT),
	BQ28400_DEBUG_REG(AVG_CURRENT),
	BQ28400_DEBUG_REG(MAX_ERROR),
	BQ28400_DEBUG_REG(RSOC),
	BQ28400_DEBUG_REG(REMAIN_CAPACITY),
	BQ28400_DEBUG_REG(FULL_CAPACITY),
	BQ28400_DEBUG_REG(CHG_CURRENT),
	BQ28400_DEBUG_REG(CHG_VOLTAGE),
	BQ28400_DEBUG_REG(BATTERY_STATUS),
	BQ28400_DEBUG_REG(CYCLE_COUNT),
	BQ28400_DEBUG_REG(DESIGN_CAPACITY),
	BQ28400_DEBUG_REG(DESIGN_VOLTAGE),
	BQ28400_DEBUG_REG(SPEC_INFO),
	BQ28400_DEBUG_REG(MANUFACTURE_DATE),
	BQ28400_DEBUG_REG(SERIAL_NUMBER),
	BQ28400_DEBUG_REG(MANUFACTURER_NAME),
	BQ28400_DEBUG_REG(DEVICE_NAME),
	BQ28400_DEBUG_REG(DEVICE_CHEMISTRY),
	BQ28400_DEBUG_REG(MANUFACTURER_DATA),
	BQ28400_DEBUG_REG(AUTHENTICATE),
	BQ28400_DEBUG_REG(CELL_VOLTAGE1),
	BQ28400_DEBUG_REG(CELL_VOLTAGE2),
	BQ28400_DEBUG_REG(SAFETY_ALERT),
	BQ28400_DEBUG_REG(SAFETY_STATUS),
	BQ28400_DEBUG_REG(PE_ALERT),
	BQ28400_DEBUG_REG(PE_STATUS),
	BQ28400_DEBUG_REG(OPERATION_STATUS),
	BQ28400_DEBUG_REG(CHARGING_STATUS),
	BQ28400_DEBUG_REG(FET_STATUS),
	BQ28400_DEBUG_REG(FULL_ACCESS_KEY),
	BQ28400_DEBUG_REG(PF_KEY),
	BQ28400_DEBUG_REG(MANUFACTURER_INFO),
	BQ28400_DEBUG_REG(SENSE_RESISTOR),
	BQ28400_DEBUG_REG(TEMP_RANGE),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, DEVICE_TYPE),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, FIRMWARE_VERSION),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, HARDWARE_VERSION),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, DF_CHECKSUM),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, EDV),
	BQ28400_DEBUG_SUBREG(MANUFACTURER_ACCESS, CHEMISTRY_ID),
};

static int bq28400_read_reg(struct i2c_client *client, u8 reg)
{
	int val;

	val = i2c_smbus_read_word_data(client, reg);
	if (val < 0)
		pr_err("i2c read fail. reg = 0x%x.ret = %d.\n", reg, val);
	else
		pr_debug("reg = 0x%02X.val = 0x%04X.\n", reg , val);

	return val;
}

static int bq28400_write_reg(struct i2c_client *client, u8 reg, u16 val)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0)
		pr_err("i2c read fail. reg = 0x%x.val = 0x%x.ret = %d.\n",
		       reg, val, ret);
	else
		pr_debug("reg = 0x%02X.val = 0x%02X.\n", reg , val);

	return ret;
}

static int bq28400_read_subcmd(struct i2c_client *client, u8 reg, u16 subcmd)
{
	int ret;
	u8 buf[4];
	u16 val = 0;

	buf[0] = reg;
	buf[1] = subcmd & 0xFF;
	buf[2] = (subcmd >> 8) & 0xFF;

	/* Control sub-command */
	ret = i2c_master_send(client, buf, 3);
	if (ret < 0) {
		pr_err("i2c tx fail. reg = 0x%x.ret = %d.\n", reg, ret);
		return ret;
	}
	udelay(66);

	/* Read Result of subcmd */
	ret = i2c_master_send(client, buf, 1);
	memset(buf, 0xAA, sizeof(buf));
	ret = i2c_master_recv(client, buf, 2);
	if (ret < 0) {
		pr_err("i2c rx fail. reg = 0x%x.ret = %d.\n", reg, ret);
		return ret;
	}
	val = (buf[1] << 8) + buf[0];

	pr_debug("reg = 0x%02X.subcmd = 0x%x.val = 0x%04X.\n",
		 reg , subcmd, val);

	return val;
}

static int bq28400_read_block(struct i2c_client *client, u8 reg,
			      u8 len, u8 *buf)
{
	int ret;
	u32 val;

	ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
	val = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);

	if (ret < 0)
		pr_err("i2c read fail. reg = 0x%x.ret = %d.\n", reg, ret);
	else
		pr_debug("reg = 0x%02X.val = 0x%04X.\n", reg , val);

	return val;
}

/*
 * Read a string from a device.
 * Returns string length on success or error on failure (negative value).
 */
static int bq28400_read_string(struct i2c_client *client, u8 reg, char *str,
			       u8 max_len)
{
	int ret;
	int len;

	ret = bq28400_read_block(client, reg, max_len, str);
	if (ret < 0)
		return ret;

	len = str[0]; /* Actual length */
	if (len > max_len - 2) { /* reduce len byte and null */
		pr_err("len = %d invalid.\n", len);
		return -EINVAL;
	}

	memcpy(&str[0], &str[1], len); /* Move sting to the start */
	str[len] = 0; /* put NULL after actual size */

	pr_debug("len = %d.str = %s.\n", len, str);

	return len;
}

#define BQ28400_INVALID_TEMPERATURE	-999
/*
 * Return the battery temperature in tenths of degree Celsius
 * Or -99.9 C if something fails.
 */
static int bq28400_read_temperature(struct i2c_client *client)
{
	int temp;

	/* temperature resolution 0.1 Kelvin */
	temp = bq28400_read_reg(client, SBS_TEMPERATURE);
	if (temp < 0)
		return BQ28400_INVALID_TEMPERATURE;

	temp = temp + ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN;

	pr_debug("temp = %d C\n", temp/10);

	return temp;
}

/*
 * Return the battery Voltage in milivolts 0..20 V
 * Or < 0 if something fails.
 */
static int bq28400_read_voltage(struct i2c_client *client)
{
	int mvolt = 0;

	mvolt = bq28400_read_reg(client, SBS_VOLTAGE);
	if (mvolt < 0)
		return mvolt;

	pr_debug("volt = %d mV.\n", mvolt);

	return mvolt;
}

/*
 * Return the battery Current in miliamps
 * Or 0 if something fails.
 * Positive current indicates charging
 * Negative current indicates discharging.
 * Current-now is calculated every second.
 */
static int bq28400_read_current(struct i2c_client *client)
{
	s16 current_ma = 0;

	current_ma = bq28400_read_reg(client, SBS_CURRENT);

	pr_debug("current = %d mA.\n", current_ma);

	return current_ma;
}

/*
 * Return the Average battery Current in miliamps
 * Or 0 if something fails.
 * Positive current indicates charging
 * Negative current indicates discharging.
 * Average Current is the rolling 1 minute average current.
 */
static int bq28400_read_avg_current(struct i2c_client *client)
{
	s16 current_ma = 0;

	current_ma = bq28400_read_reg(client, SBS_AVG_CURRENT);

	pr_debug("avg_current=%d mA.\n", current_ma);

	return current_ma;
}

/*
 * Return the battery Relative-State-Of-Charge 0..100 %
 * Or negative value if something fails.
 */
static int bq28400_read_rsoc(struct i2c_client *client)
{
	int percentage = 0;

	if (fake_battery != -EINVAL) {
		pr_debug("Reporting Fake SOC = %d\n", fake_battery);
		return fake_battery;
	}

	/* This register is only 1 byte */
	percentage = i2c_smbus_read_byte_data(client, SBS_RSOC);

	if (percentage < 0) {
		pr_err("I2C failure when reading rsoc.\n");
		return percentage;
	}

	pr_debug("percentage = %d.\n", percentage);

	return percentage;
}

/*
 * Return the battery Capacity in mAh.
 * Or 0 if something fails.
 */
static int bq28400_read_full_capacity(struct i2c_client *client)
{
	int capacity = 0;

	capacity = bq28400_read_reg(client, SBS_FULL_CAPACITY);
	if (capacity < 0)
		return 0;

	pr_debug("full-capacity = %d mAh.\n", capacity);

	return capacity;
}

/*
 * Return the battery Capacity in mAh.
 * Or 0 if something fails.
 */
static int bq28400_read_remain_capacity(struct i2c_client *client)
{
	int capacity = 0;

	capacity = bq28400_read_reg(client, SBS_REMAIN_CAPACITY);
	if (capacity < 0)
		return 0;

	pr_debug("remain-capacity = %d mAh.\n", capacity);

	return capacity;
}

static int bq28400_enable_charging(struct bq28400_device *bq28400_dev,
				   bool enable)
{
	int ret;
	static bool is_charging_enabled;

	if (bq28400_dev->dc_psy == NULL) {
		bq28400_dev->dc_psy = power_supply_get_by_name("dc");
		if (bq28400_dev->dc_psy == NULL) {
			pr_err("fail to get dc-psy.\n");
			return -ENODEV;
		}
	}

	if (is_charging_enabled == enable) {
		pr_debug("Charging enable already = %d.\n", enable);
		return 0;
	}

	ret = power_supply_set_online(bq28400_dev->dc_psy, enable);
	if (ret < 0) {
		pr_err("fail to set dc-psy online to %d.\n", enable);
		return ret;
	}

	is_charging_enabled = enable;

	pr_debug("Charging enable = %d.\n", enable);

	return 0;
}

static int bq28400_get_prop_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int rsoc;
	s16 current_ma = 0;
	u16 battery_status;
	int temperature;
	struct bq28400_device *dev = i2c_get_clientdata(client);

	battery_status = bq28400_read_reg(client, SBS_BATTERY_STATUS);
	rsoc = bq28400_read_rsoc(client);
	current_ma = bq28400_read_current(client);
	temperature = bq28400_read_temperature(client);
	temperature = temperature / 10; /* in degree celsius */

	if (battery_status & BAT_STATUS_EMPTY)
		pr_debug("Battery report Empty.\n");

	/* Battery may report FULL before rsoc is 100%
	 * for protection and cell-balancing.
	 * The FULL report may remain when rsoc drops from 100%.
	 * If battery is full but DC-Jack is removed then report discahrging.
	 */
	if (battery_status & BAT_STATUS_FULL) {
		pr_debug("Battery report Full.\n");
		bq28400_enable_charging(bq28400_dev, false);
		if (current_ma < 0)
			return POWER_SUPPLY_STATUS_DISCHARGING;
		return POWER_SUPPLY_STATUS_FULL;
	}

	if (rsoc == 100) {
		bq28400_enable_charging(bq28400_dev, false);
		pr_debug("Full.\n");
		return POWER_SUPPLY_STATUS_FULL;
	}

	/* Enable charging when battery is not full and temperature is ok */
	if ((temperature > dev->temp_cold) && (temperature < dev->temp_hot))
		bq28400_enable_charging(bq28400_dev, true);
	else
		bq28400_enable_charging(bq28400_dev, false);

	/*
	* Positive current indicates charging
	* Negative current indicates discharging.
	* Charging is stopped at termination-current.
	*/
	if (current_ma < 0) {
		pr_debug("Discharging.\n");
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else if (current_ma > BQ_TERMINATION_CURRENT_MA) {
		pr_debug("Charging.\n");
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		pr_debug("Not Charging.\n");
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	return status;
}

static int bq28400_get_prop_charge_type(struct i2c_client *client)
{
	u16 battery_status;
	u16 chg_status;
	u16 fet_status;

	battery_status = bq28400_read_reg(client, SBS_BATTERY_STATUS);
	chg_status = bq28400_read_reg(client, SBS_CHARGING_STATUS);
	fet_status = bq28400_read_reg(client, SBS_FET_STATUS);

	if (battery_status & BAT_STATUS_DISCHARGING) {
		pr_debug("Discharging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (fet_status & FET_STATUS_PRECHARGE) {
		pr_debug("Pre-Charging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	}

	if (chg_status & CHG_STATUS_HOT_TEMP_CHARGING) {
		pr_debug("Hot-Temp-Charging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	if (chg_status & CHG_STATUS_LOW_TEMP_CHARGING) {
		pr_debug("Low-Temp-Charging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	if (chg_status & CHG_STATUS_STD1_TEMP_CHARGING) {
		pr_debug("STD1-Temp-Charging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	if (chg_status & CHG_STATUS_STD2_TEMP_CHARGING) {
		pr_debug("STD2-Temp-Charging.\n");
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	if (chg_status & CHG_STATUS_BATTERY_DEPLETED)
		pr_debug("battery_depleted.\n");

	if (chg_status & CHG_STATUS_CELL_BALANCING)
		pr_debug("cell_balancing.\n");

	if (chg_status & CHG_STATUS_OVERCHARGE) {
		pr_err("overcharge fault.\n");
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (chg_status & CHG_STATUS_SUSPENDED) {
		pr_info("Suspended.\n");
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (chg_status & CHG_STATUS_DISABLED) {
		pr_info("Disabled.\n");
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
}

static bool bq28400_get_prop_present(struct i2c_client *client)
{
	int val;

	val = bq28400_read_reg(client, SBS_BATTERY_STATUS);

	/* If the bq28400 is inside the battery pack
	 * then when battery is removed the i2c transfer will fail.
	 */

	if (val < 0)
		return false;

	/* TODO - support when bq28400 is not embedded in battery pack */

	return true;
}

/*
 * User sapce read the battery info.
 * Get data online via I2C from the battery gauge.
 */
static int bq28400_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	int ret = 0;
	struct bq28400_device *dev = container_of(psy,
						  struct bq28400_device,
						  batt_psy);
	struct i2c_client *client = dev->client;
	static char str[BQ_MAX_STR_LEN];

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq28400_get_prop_status(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq28400_get_prop_charge_type(client);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq28400_get_prop_present(client);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq28400_read_voltage(client);
		val->intval *= 1000; /* mV to uV */
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq28400_read_rsoc(client);
		if (val->intval < 0)
			ret = -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Positive current indicates drawing */
		val->intval = -bq28400_read_current(client);
		val->intval *= 1000; /* mA to uA */
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		/* Positive current indicates drawing */
		val->intval = -bq28400_read_avg_current(client);
		val->intval *= 1000; /* mA to uA */
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq28400_read_temperature(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = bq28400_read_full_capacity(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = bq28400_read_remain_capacity(client);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		bq28400_read_string(client, SBS_DEVICE_NAME, str, 20);
		val->strval = str;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		bq28400_read_string(client, SBS_MANUFACTURER_NAME, str, 20);
		val->strval = str;
		break;
	default:
		pr_err(" psp %d Not supoprted.\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bq28400_set_reg(void *data, u64 val)
{
	struct debug_reg *dbg = data;
	u8 reg = dbg->reg;
	int ret;
	struct i2c_client *client = bq28400_dev->client;

	ret = bq28400_write_reg(client, reg, val);

	return ret;
}

static int bq28400_get_reg(void *data, u64 *val)
{
	struct debug_reg *dbg = data;
	u8 reg = dbg->reg;
	u16 subcmd = dbg->subcmd;
	int ret;
	struct i2c_client *client = bq28400_dev->client;

	if (subcmd)
		ret = bq28400_read_subcmd(client, reg, subcmd);
	else
		ret = bq28400_read_reg(client, reg);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, bq28400_get_reg, bq28400_set_reg,
			"0x%04llx\n");

static int bq28400_create_debugfs_entries(struct bq28400_device *bq28400_dev)
{
	int i;

	bq28400_dev->dent = debugfs_create_dir(BQ28400_NAME, NULL);
	if (IS_ERR(bq28400_dev->dent)) {
		pr_err("bq28400 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(bq28400_debug_regs) ; i++) {
		char *name = bq28400_debug_regs[i].name;
		struct dentry *file;
		void *data = &bq28400_debug_regs[i];

		file = debugfs_create_file(name, 0644, bq28400_dev->dent,
					   data, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static int bq28400_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	pr_debug("psp = %d.val = %d.\n", psp, val->intval);

	return -EINVAL;
}

static void bq28400_external_power_changed(struct power_supply *psy)
{
	pr_debug("Notify power_supply_changed.\n");

	/* The battery gauge monitors the current and voltage every 1 second.
	 * Therefore a delay from the time that the charger start/stop charging
	 * until the battery gauge detects it.
	 */
	msleep(1000);
	/* Update LEDs and notify uevents */
	power_supply_changed(&bq28400_dev->batt_psy);
}

static int bq28400_register_psy(struct bq28400_device *bq28400_dev)
{
	int ret;

	bq28400_dev->batt_psy.name = "battery";
	bq28400_dev->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	bq28400_dev->batt_psy.num_supplicants = 0;
	bq28400_dev->batt_psy.properties = pm_power_props;
	bq28400_dev->batt_psy.num_properties = ARRAY_SIZE(pm_power_props);
	bq28400_dev->batt_psy.get_property = bq28400_get_property;
	bq28400_dev->batt_psy.set_property = bq28400_set_property;
	bq28400_dev->batt_psy.external_power_changed =
		bq28400_external_power_changed;

	ret = power_supply_register(&bq28400_dev->client->dev,
				&bq28400_dev->batt_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

/**
 * Update userspace every 1 minute.
 * Normally it takes more than 120 minutes (two hours) to
 * charge/discahrge the battery,
 * so updating every 1 minute should be enough for 1% change
 * detection.
 * Any immidiate change detected by the DC charger is notified
 * by the bq28400_external_power_changed callback, which notify
 * the user space.
 */
static void bq28400_periodic_user_space_update_worker(struct work_struct *work)
{
	u32 delay_msec = 60*1000;

	pr_debug("Notify user space.\n");

	/* Notify user space via kobject_uevent change notification */
	power_supply_changed(&bq28400_dev->batt_psy);

	schedule_delayed_work(&bq28400_dev->periodic_user_space_update_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (delay_msec)));
}

static int bq28400_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int ret = 0;
	struct device_node *dev_node = client->dev.of_node;

	if (dev_node == NULL) {
		pr_err("Device Tree node doesn't exist.\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err(" i2c func fail.\n");
		return -EIO;
	}

	if (bq28400_read_reg(client, SBS_BATTERY_STATUS) < 0) {
		pr_err("Device doesn't exist.\n");
		return -ENODEV;
	}

	bq28400_dev = kzalloc(sizeof(*bq28400_dev), GFP_KERNEL);
	if (!bq28400_dev) {
		pr_err(" alloc fail.\n");
		return -ENOMEM;
	}

	/* Note: Lithium-ion battery normal temperature range 0..40 C */
	ret = of_property_read_u32(dev_node, "ti,temp-cold",
				   &(bq28400_dev->temp_cold));
	if (ret) {
		pr_err("Unable to read cold temperature. ret=%d.\n", ret);
		goto err_dev_node;
	}
	pr_debug("cold temperature limit = %d C.\n", bq28400_dev->temp_cold);

	ret = of_property_read_u32(dev_node, "ti,temp-hot",
				   &(bq28400_dev->temp_hot));
	if (ret) {
		pr_err("Unable to read hot temperature. ret=%d.\n", ret);
		goto err_dev_node;
	}
	pr_debug("hot temperature limit = %d C.\n", bq28400_dev->temp_hot);

	bq28400_dev->client = client;
	i2c_set_clientdata(client, bq28400_dev);

	ret = bq28400_register_psy(bq28400_dev);
	if (ret) {
		pr_err(" bq28400_register_psy fail.\n");
		goto err_register_psy;
	}

	ret = bq28400_create_debugfs_entries(bq28400_dev);
	if (ret) {
		pr_err(" bq28400_create_debugfs_entries fail.\n");
		goto err_debugfs;
	}

	INIT_DELAYED_WORK(&bq28400_dev->periodic_user_space_update_work,
			  bq28400_periodic_user_space_update_worker);

	schedule_delayed_work(&bq28400_dev->periodic_user_space_update_work,
			      msecs_to_jiffies(1000));

	pr_debug("Device is ready.\n");

	return 0;

err_debugfs:
	if (bq28400_dev->dent)
		debugfs_remove_recursive(bq28400_dev->dent);
	power_supply_unregister(&bq28400_dev->batt_psy);
err_register_psy:
err_dev_node:
	kfree(bq28400_dev);
	bq28400_dev = NULL;

	pr_info("FAIL.\n");

	return ret;
}

static int bq28400_remove(struct i2c_client *client)
{
	struct bq28400_device *bq28400_dev = i2c_get_clientdata(client);

	power_supply_unregister(&bq28400_dev->batt_psy);
	if (bq28400_dev->dent)
		debugfs_remove_recursive(bq28400_dev->dent);
	kfree(bq28400_dev);
	bq28400_dev = NULL;

	return 0;
}

static const struct of_device_id bq28400_match[] = {
	{ .compatible = "ti,bq28400-battery" },
	{ .compatible = "ti,bq30z55-battery" },
	{ },
	};

static const struct i2c_device_id bq28400_id[] = {
	{BQ28400_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, bq28400_id);

static struct i2c_driver bq28400_driver = {
	.driver	= {
		.name	= BQ28400_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(bq28400_match),
	},
	.probe		= bq28400_probe,
	.remove		= bq28400_remove,
	.id_table	= bq28400_id,
};

static int __init bq28400_init(void)
{
	pr_info(" bq28400 driver rev %s.\n", BQ28400_REV);

	return i2c_add_driver(&bq28400_driver);
}
module_init(bq28400_init);

static void __exit bq28400_exit(void)
{
	return i2c_del_driver(&bq28400_driver);
}
module_exit(bq28400_exit);

MODULE_DESCRIPTION("Driver for BQ28400 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" BQ28400_NAME);
