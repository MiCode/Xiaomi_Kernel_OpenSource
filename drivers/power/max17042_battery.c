/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>

/* Status register bits */
#define STATUS_POR_BIT         (1 << 1)
#define STATUS_BST_BIT         (1 << 3)
#define STATUS_VMN_BIT         (1 << 8)
#define STATUS_TMN_BIT         (1 << 9)
#define STATUS_SMN_BIT         (1 << 10)
#define STATUS_BI_BIT          (1 << 11)
#define STATUS_VMX_BIT         (1 << 12)
#define STATUS_TMX_BIT         (1 << 13)
#define STATUS_SMX_BIT         (1 << 14)
#define STATUS_BR_BIT          (1 << 15)

/* Interrupt mask bits */
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define MODEL_UNLOCK1	0X0059
#define MODEL_UNLOCK2	0X00C4
#define MODEL_LOCK1		0X0000
#define MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

#define MAX17042_IC_VERSION	0x0092
#define MAX17047_IC_VERSION	0x00AC	/* same for max17050 */
#define MAX17047_DELAY		60000
#define MAX17047_LOW_BATT_DELAY 20000

#define SAMSUNG_ID_MIN	500
#define SAMSUNG_ID_MAX	700
#define LG_ID_MIN	850
#define LG_ID_MAX	1100
#define SONY_ID_MIN	100
#define SONY_ID_MAX	300

struct max17042_chip {
	struct i2c_client *client;
	struct power_supply battery;
	enum max170xx_chip_type chip_type;
	struct max17042_platform_data *pdata;
	struct delayed_work work;
	int    init_complete;
	int shutdown_complete;
	int status;
	int cap;
};
struct max17042_chip *tmp_chip;
struct i2c_client *temp_client;

static int debug_mask = 0;	/* for debug */

extern void max77665_charger_temp_control(int batt_temp);
extern int ina230_set_threshold(int mA, int vbus);

static int is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}

static int max17042_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = 0;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (chip && chip->shutdown_complete)
		return -ENODEV;

	ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17042_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = 0;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (chip && chip->shutdown_complete)
		return -ENODEV;

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void max17042_set_reg(struct i2c_client *client,
			     struct max17042_reg_data *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
		max17042_write_reg(client, data[i].addr, data[i].data);
}

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_REG,
	POWER_SUPPLY_PROP_VAL,
	POWER_SUPPLY_PROP_TGAIN,
	POWER_SUPPLY_PROP_TOFF,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_STATUS,
};

int maxim_get_temp(int *deci_celsius)
{
	int ret = -ENODEV;
	s16 temp;

	*deci_celsius = -2732;
	if (temp_client == NULL)
		return ret;

	ret = max17042_read_reg(temp_client, MAX17042_TEMP);
	if (ret < 0)
		return ret;

	temp = ret & 0xFFFF;
	/* The value is converted into deci-centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */
	*deci_celsius = temp * 10 / 256;
	return 0;
}
EXPORT_SYMBOL_GPL(maxim_get_temp);

int maxim_get_batt_voltage_avg(int *vbatt_avg)
{
	int ret = -ENODEV;

	if (temp_client == NULL)
		return ret;

	ret = max17042_read_reg(temp_client, MAX17042_AvgVCELL);
	if (ret < 0)
		return ret;

	*vbatt_avg = ret * 625 / 8;

	return 0;
}

EXPORT_SYMBOL_GPL(maxim_get_batt_voltage_avg);

static int max17042_property_is_writeable(struct power_supply *psy,
					  enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_REG:
	case POWER_SUPPLY_PROP_VAL:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		return 1;
	default:
		break;
	}

	return 0;
}

u8 max17042_reg = 0;
int max17042_curr_avg;
static int max17042_set_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 const union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
						  struct max17042_chip,
						  battery);
	static ktime_t ktime_last;
	static u64 mAh_last;
	ktime_t ktime;
	int64_t ns;
	uint64_t mAh;
	uint32_t qh, ql;

	if (!chip->init_complete)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_REG:
		if (val->intval >= 0 && val->intval <= 0xff)
			max17042_reg = val->intval;

		break;
	case POWER_SUPPLY_PROP_VAL:
		max17042_write_reg(chip->client, max17042_reg, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == 1) {
			max17042_curr_avg = 0;
			qh = max17042_read_reg(chip->client, MAX17042_QH);
			ql = max17042_read_reg(chip->client, MAX17042_QL);
			mAh_last = (u64) ((ql & 0xFFFF) | (qh & 0xFFFF) << 16);
			ktime_last = ktime_get_boottime();
			dev_dbg(&chip->client->dev,
				"max17042 mAh%llx ns%lld\n", mAh_last,
				ktime_to_ns(ktime_last));
		} else if (val->intval == 0) {
			qh = max17042_read_reg(chip->client, MAX17042_QH);
			ql = max17042_read_reg(chip->client, MAX17042_QL);
			mAh = (u64) ((ql & 0xFFFF) | (qh & 0xFFFF) << 16);
			ktime = ktime_get_boottime();
			dev_dbg(&chip->client->dev,
				"max17042 now mAh%llx ns%lld\n", mAh,
				ktime_to_ns(ktime));
			if (mAh_last >= mAh) {
				mAh = mAh_last - mAh;
				mAh = mAh * 7630 * 3600;
				ktime = ktime_sub(ktime, ktime_last);
				ns = ktime_to_ns(ktime);
				ns = ns + (1000000 - 1);
				do_div(ns, 1000000);
				do_div(mAh, (uint32_t) ns);
				mAh = mAh + (1000 - 1);
				do_div(mAh, 1000);
				max17042_curr_avg = mAh;
			}
			dev_dbg(&chip->client->dev,
				"max17042 curr_avg %d\n", max17042_curr_avg);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void max17042_update_status(int status)
{
	if (!tmp_chip) {
		WARN_ON(1);
		return;
	}

	tmp_chip->status = status;
	power_supply_changed(&tmp_chip->battery);
}
EXPORT_SYMBOL_GPL(max17042_update_status);

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	int ret, temp;

	if (!chip->init_complete)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if ((ret = maxim_get_temp(&temp)) < 0 ||
		    temp < MIN_TEMP || temp > MAX_TEMP) {
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
			return ret;
		}

		if (temp > 550)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (temp < 0)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = max17042_read_reg(chip->client, MAX17042_STATUS);
		if (ret < 0)
			return ret;

		if (ret & MAX17042_STATUS_BattAbsent)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = max17042_read_reg(chip->client, MAX17042_Cycles);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = max17042_read_reg(chip->client, MAX17042_MinMaxVolt);
		if (ret < 0)
			return ret;

		val->intval = ret >> 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (chip->chip_type == MAX17042)
			ret = max17042_read_reg(chip->client, MAX17042_V_empty);
		else
			ret = max17042_read_reg(chip->client, MAX17047_V_empty);
		if (ret < 0)
			return ret;

		val->intval = ret >> 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = max17042_read_reg(chip->client, MAX17042_VCELL);
		if (ret < 0)
			return ret;

		val->intval = ret * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = max17042_read_reg(chip->client, MAX17042_AvgVCELL);
		if (ret < 0)
			return ret;

		val->intval = ret * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = max17042_read_reg(chip->client, MAX17042_OCVInternal);
		if (ret < 0)
			return ret;

		val->intval = ret * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = max17042_read_reg(chip->client, MAX17042_RepSOC);
		if (ret < 0) {
			dev_err(&chip->client->dev, "ERROR, Force SOC to 1\n");
			val->intval = 1;
			return 0;
		}
		/* round up with 103% */
		ret = (ret & 0xFF00) >> 8;
		ret = (ret * 103 + 99) / 100;
		if (ret > 100)
			ret = 100;
		val->intval = ret;
		chip->cap = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = max17042_read_reg(chip->client, MAX17042_FullCAP);
		if (ret < 0)
			return ret;

		val->intval = ret * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = max17042_read_reg(chip->client, MAX17042_QH);
		if (ret < 0)
			return ret;

		val->intval = ret * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = max17042_read_reg(chip->client, MAX17042_TEMP);
		if (ret < 0) {
			dev_err(&chip->client->dev, "ERROR, Force temp to 0\n");
			val->intval = 0;
			return 0;
		}

		val->intval = ret;
		/* The value is signed. */
		if (val->intval & 0x8000) {
			val->intval = (0x7fff & ~val->intval) + 1;
			val->intval *= -1;
		}
		/* The value is converted into deci-centigrade scale */
		/* Units of LSB = 1 / 256 degree Celsius */
		val->intval = val->intval * 10 / 256;
		if (val->intval >= 680) {
			printk(KERN_WARNING
				"%s: reporting temp %d, forced to 400\n",
				__func__, val->intval);
			val->intval = 400;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->pdata->enable_current_sense) {
			ret = max17042_read_reg(chip->client, MAX17042_Current);
			if (ret < 0)
				return ret;

			val->intval = ret;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
			/* Reverse, P for discharging, N for charging */
			val->intval *= -1;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->pdata->enable_current_sense) {
			ret = max17042_read_reg(chip->client,
						MAX17042_AvgCurrent);
			if (ret < 0)
				return ret;

			val->intval = ret;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
			if (max17042_curr_avg)
				val->intval = max17042_curr_avg;
		} else {
			return -EINVAL;
		}
		break;

	case POWER_SUPPLY_PROP_REG:
		val->intval = max17042_reg;
		break;
	case POWER_SUPPLY_PROP_VAL:
		ret = max17042_read_reg(chip->client, max17042_reg);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TGAIN:
		ret = max17042_read_reg(chip->client, MAX17042_TGAIN);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TOFF:
		ret = max17042_read_reg(chip->client, MAX17042_TOFF);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->status)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;

		if (chip->status && chip->cap >= 100)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17042_write_verify_reg(struct i2c_client *client,
				u8 reg, u16 value)
{
	int retries = 8;
	int ret;
	u16 read_value;

	do {
		ret = max17042_write_reg(client, reg, value);
		read_value =  max17042_read_reg(client, reg);
		if (read_value != value) {
			ret = -EIO;
			retries--;
		}
	} while (retries && read_value != value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static inline void max17042_override_por(
	struct i2c_client *client, u8 reg, u16 value)
{
	if (value)
		max17042_write_reg(client, reg, value);
}

static inline void max10742_unlock_model(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	max17042_write_reg(client, MAX17042_MLOCKReg1, MODEL_UNLOCK1);
	max17042_write_reg(client, MAX17042_MLOCKReg2, MODEL_UNLOCK2);
}

static inline void max10742_lock_model(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	max17042_write_reg(client, MAX17042_MLOCKReg1, MODEL_LOCK1);
	max17042_write_reg(client, MAX17042_MLOCKReg2, MODEL_LOCK2);
}

static inline void max17042_write_model_data(struct max17042_chip *chip,
					u8 addr, int size)
{
	struct i2c_client *client = chip->client;
	int i;
	for (i = 0; i < size; i++)
		max17042_write_reg(client, addr + i,
				chip->pdata->config_data->cell_char_tbl[i]);
}

static inline void max17042_read_model_data(struct max17042_chip *chip,
					u8 addr, u16 *data, int size)
{
	struct i2c_client *client = chip->client;
	int i;

	for (i = 0; i < size; i++)
		data[i] = max17042_read_reg(client, addr + i);
}

static inline int max17042_model_data_compare(struct max17042_chip *chip,
					u16 *data1, u16 *data2, int size)
{
	int i;

	if (memcmp(data1, data2, size * sizeof(u16))) {
		dev_err(&chip->client->dev, "%s compare failed\n", __func__);
		for (i = 0; i < size; i++)
			dev_info(&chip->client->dev, "0x%x, 0x%x",
				data1[i], data2[i]);
		dev_info(&chip->client->dev, "\n");
		return -EINVAL;
	}
	return 0;
}

static int max17042_init_model(struct max17042_chip *chip)
{
	int ret;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_write_model_data(chip, MAX17042_MODELChrTbl,
				table_size);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);

	ret = max17042_model_data_compare(
		chip,
		chip->pdata->config_data->cell_char_tbl,
		temp_data,
		table_size);

	max10742_lock_model(chip);
	kfree(temp_data);

	return ret;
}

static int max17042_verify_model_lock(struct max17042_chip *chip)
{
	int i;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;
	int ret = 0;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);
	for (i = 0; i < table_size; i++)
		if (temp_data[i])
			ret = -EINVAL;

	kfree(temp_data);
	return ret;
}

static void max17042_write_config_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_write_reg(chip->client, MAX17042_CONFIG, config->config);
	max17042_write_reg(chip->client, MAX17042_LearnCFG, config->learn_cfg);
	max17042_write_reg(chip->client, MAX17042_FilterCFG,
			config->filter_cfg);
	max17042_write_reg(chip->client, MAX17042_RelaxCFG, config->relax_cfg);
	if (chip->chip_type == MAX17047)
		max17042_write_reg(chip->client, MAX17047_FullSOCThr,
						config->full_soc_thresh);
}

static void  max17042_write_custom_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_write_verify_reg(chip->client, MAX17042_RCOMP0,
				config->rcomp0);
	max17042_write_verify_reg(chip->client, MAX17042_TempCo,
				config->tcompc0);
	max17042_write_verify_reg(chip->client, MAX17042_ICHGTerm,
				config->ichgt_term);
	if (chip->chip_type == MAX17042) {
		max17042_write_reg(chip->client, MAX17042_EmptyTempCo,
					config->empty_tempco);
		max17042_write_verify_reg(chip->client, MAX17042_K_empty0,
					config->kempty0);
	} else {
		max17042_write_verify_reg(chip->client, MAX17047_V_empty,
					  config->vempty);
		max17042_write_verify_reg(chip->client, MAX17047_QRTbl00,
						config->qrtbl00);
		max17042_write_verify_reg(chip->client, MAX17047_QRTbl10,
						config->qrtbl10);
		max17042_write_verify_reg(chip->client, MAX17047_QRTbl20,
						config->qrtbl20);
		max17042_write_verify_reg(chip->client, MAX17047_QRTbl30,
						config->qrtbl30);
	}

	/* Tgain and Toffset */
	max17042_write_verify_reg(chip->client, MAX17042_TGAIN, config->tgain);
	max17042_write_verify_reg(chip->client, MAX17042_TOFF, config->toff);
}

static void max17042_update_capacity_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	/* Based on the spec, VF_FULLCap is used to load */
	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
				config->design_cap);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
				config->design_cap);
}

static void max17042_reset_vfsoc0_reg(struct max17042_chip *chip)
{
	u16 vfSoc;

	vfSoc = max17042_read_reg(chip->client, MAX17042_VFSOC);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(chip->client, MAX17042_VFSOC0, vfSoc);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_LOCK);
}

static void max17042_load_new_capacity_params(struct max17042_chip *chip)
{
	u16 rep_cap, dq_acc, vfSoc;
	u32 rem_cap;

	struct max17042_config_data *config = chip->pdata->config_data;

	vfSoc = max17042_read_reg(chip->client, MAX17042_VFSOC);

	/* fg_vfSoc needs to shifted by 8 bits to get the
	 * perc in 1% accuracy, to get the right rem_cap multiply
	 * full_cap0, fg_vfSoc and devide by 100
	 */
	rem_cap = ((vfSoc >> 8) * config->design_cap) / 100;
	max17042_write_verify_reg(chip->client, MAX17042_RemCap, (u16)rem_cap);

	rep_cap = (u16)rem_cap;
	max17042_write_verify_reg(chip->client, MAX17042_RepCap, rep_cap);

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = config->design_cap / dQ_ACC_DIV;
	max17042_write_verify_reg(chip->client, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(chip->client, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
			config->design_cap);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
			config->design_cap);
	/* Update SOC register with new SOC */
	max17042_write_reg(chip->client, MAX17042_RepSOC, vfSoc);
}

/*
 * Block write all the override values coming from platform data.
 * This function MUST be called before the POR initialization proceedure
 * specified by maxim.
 */
static inline void max17042_override_por_values(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por(client, MAX17042_TGAIN, config->tgain);
	max17042_override_por(client, MAX17042_TOFF, config->toff);
	max17042_override_por(client, MAX17042_CGAIN, config->cgain);
	max17042_override_por(client, MAX17042_COFF, config->coff);

	max17042_override_por(client, MAX17042_VALRT_Th, config->valrt_thresh);
	max17042_override_por(client, MAX17042_TALRT_Th, config->talrt_thresh);
	max17042_override_por(client, MAX17042_SALRT_Th,
			config->soc_alrt_thresh);
	max17042_override_por(client, MAX17042_CONFIG, config->config);
	max17042_override_por(client, MAX17042_SHDNTIMER, config->shdntimer);

	max17042_override_por(client, MAX17042_DesignCap, config->design_cap);
	max17042_override_por(client, MAX17042_ICHGTerm, config->ichgt_term);

	max17042_override_por(client, MAX17042_AtRate, config->at_rate);
	max17042_override_por(client, MAX17042_LearnCFG, config->learn_cfg);
	max17042_override_por(client, MAX17042_FilterCFG, config->filter_cfg);
	max17042_override_por(client, MAX17042_RelaxCFG, config->relax_cfg);
	max17042_override_por(client, MAX17042_MiscCFG, config->misc_cfg);
	max17042_override_por(client, MAX17042_MaskSOC, config->masksoc);

	max17042_override_por(client, MAX17042_FullCAP, config->fullcap);
	max17042_override_por(client, MAX17042_FullCAPNom, config->fullcapnom);
	if (chip->chip_type == MAX17042)
		max17042_override_por(client, MAX17042_SOC_empty,
						config->socempty);
	max17042_override_por(client, MAX17042_LAvg_empty, config->lavg_empty);
	max17042_override_por(client, MAX17042_dQacc, config->dqacc);
	max17042_override_por(client, MAX17042_dPacc, config->dpacc);

	if (chip->chip_type == MAX17042)
		max17042_override_por(client, MAX17042_V_empty, config->vempty);
	else
		max17042_override_por(client, MAX17047_V_empty, config->vempty);
	max17042_override_por(client, MAX17042_TempNom, config->temp_nom);
	max17042_override_por(client, MAX17042_TempLim, config->temp_lim);
	max17042_override_por(client, MAX17042_FCTC, config->fctc);
	max17042_override_por(client, MAX17042_RCOMP0, config->rcomp0);
	max17042_override_por(client, MAX17042_TempCo, config->tcompc0);
	if (chip->chip_type) {
		max17042_override_por(client, MAX17042_EmptyTempCo,
					config->empty_tempco);
		max17042_override_por(client, MAX17042_K_empty0,
					config->kempty0);
	}
}

static void max17042_dump_registers(struct max17042_chip *chip)
{
	int i;

	for (i = 0; i <= 0x4f; i++) {
		if (i % 16 == 0)
			printk("\nmax17042 %02x ", i);
		printk(" %04x", max17042_read_reg(chip->client, i));
	}
	printk("\n");

	for (i = 0xe0; i <= 0xff; i++) {
		if (i % 16 == 0)
			printk("\nmax17042 %02x ", i);
		printk(" %04x", max17042_read_reg(chip->client, i));
	}
	printk("\n");
}

static int set_debug_status_param(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_info("max17042 set debug to %d\n", debug_mask);
	if (tmp_chip && debug_mask)
		max17042_dump_registers(tmp_chip);

	return 0;
}

module_param_call(debug, set_debug_status_param, param_get_uint,
		  &debug_mask, 0644);

static int max17042_init_chip(struct max17042_chip *chip)
{
	int ret;
	int val;

	dev_info(&chip->client->dev, "%s\n", __func__);

	if (debug_mask)
		max17042_dump_registers(chip);

	max17042_override_por_values(chip);
	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	/* Initialize configaration */
	max17042_write_config_regs(chip);

	/* write cell characterization data */
	ret = max17042_init_model(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s init failed\n",
			__func__);
		return -EIO;
	}
	max17042_verify_model_lock(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s lock verify failed\n",
			__func__);
		return -EIO;
	}
	/* write custom parameters */
	max17042_write_custom_regs(chip);

	/* update capacity params */
	max17042_update_capacity_regs(chip);

	/* delay must be atleast 350mS to allow VFSOC
	 * to be calculated from the new configuration
	 */
	msleep(350);

	/* reset vfsoc0 reg */
	max17042_reset_vfsoc0_reg(chip);

	/* Advance to Coulomb Counter Mode */
	max17042_write_verify_reg(chip->client, MAX17042_Cycles, 0x0060);

	/* load new capacity params */
	max17042_load_new_capacity_params(chip);

	/* Init complete, Clear the POR bit */
	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	max17042_write_reg(chip->client, MAX17042_STATUS,
			val & (~STATUS_POR_BIT));

	if (debug_mask)
		max17042_dump_registers(chip);

	return 0;
}

static void max17042_set_soc_threshold(struct max17042_chip *chip, u16 off)
{
	u16 soc, soc_tr;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'off' perc change in the soc
	 */
	soc = max17042_read_reg(chip->client, MAX17042_RepSOC) >> 8;
	/* Alert if soc is below 10% */
	if (soc <= 10)
		soc_tr = soc | 0xff00;
	max17042_write_reg(chip->client, MAX17042_SALRT_Th, soc_tr);
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;
	u16 val;

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	dev_info(&chip->client->dev, "INTR sts %x\n", val);
	if ((val & STATUS_INTR_SOCMIN_BIT) || (val & STATUS_INTR_SOCMAX_BIT)) {
		max17042_set_soc_threshold(chip, 1);
	}

	/* Clear the status for next event */
	max17042_write_reg(chip->client, MAX17042_STATUS, 0);

	power_supply_changed(&chip->battery);
	return IRQ_HANDLED;
}

static int max17042_verify_model_update(struct max17042_chip *chip)
{
	int ret = -EINVAL;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u16 *temp_data;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				 table_size);
	max10742_lock_model(chip);

	ret = max17042_model_data_compare(chip,
					  chip->pdata->config_data->
					  cell_char_tbl, temp_data, table_size);
	if (ret < 0) {
		dev_info(&chip->client->dev, "model need update\n");
		ret = 1;
	}

	kfree(temp_data);

	return ret;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
						  struct max17042_chip,
						  work.work);
	union power_supply_propval val;
	int soc, ocv, voltage, current_now, temp;
	int reg;

	/* Check if model needs update periodically, only check POR bit
	 * as frequent unlock/lock model will impact the internal firmware
	 */
	reg = max17042_read_reg(chip->client, MAX17042_STATUS);
	if ((reg & STATUS_POR_BIT)) {
		dev_info(&chip->client->dev, "%s status:%x\n", __func__, reg);
		if (chip->pdata->enable_por_init && chip->pdata->config_data) {
			if (max17042_init_chip(chip) < 0)
				dev_err(&chip->client->dev, "%s failed\n",
					__func__);

			max17042_write_reg(chip->client, MAX17042_STATUS, 0);

			max17042_set_soc_threshold(chip, 1);

			max17042_write_reg(chip->client,
					MAX17042_TALRT_Th, 0x7f80);

			reg = max17042_read_reg(chip->client, MAX17042_CONFIG);
			reg |= CONFIG_ALRT_BIT_ENBL | 0x700;
			max17042_write_reg(chip->client, MAX17042_CONFIG, reg);
		}
	}

	power_supply_changed(&chip->battery);

	/* Dump the battery info */
	max17042_get_property(&chip->battery, POWER_SUPPLY_PROP_CAPACITY, &val);
	soc = val.intval;

	max17042_get_property(&chip->battery,
			 POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	ocv = val.intval;

	max17042_get_property(&chip->battery,
			 POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	voltage = val.intval;

	max17042_get_property(&chip->battery,
			 POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	current_now = val.intval;

	maxim_get_temp(&temp);
	dev_info(&chip->client->dev, "SOC %d OCV %d VOL %d CURR %d TEMP %d\n",
		 soc, ocv, voltage, current_now, temp);

	if (debug_mask)
		max17042_dump_registers(chip);

	/* Temperature control */
	max77665_charger_temp_control(temp / 10);

	ocv = ocv / 1000;
	if (ocv > 3900)
		/* 4000mA current threshold, vbus monitor disabled */
		ina230_set_threshold(4000, 0);
	else
		/* current monitor disabled, 3400mV threshold */
		ina230_set_threshold(0, 3400);

	schedule_delayed_work(&chip->work,
			      (chip->cap >
			       20) ? MAX17047_DELAY : MAX17047_LOW_BATT_DELAY);
}

#ifdef CONFIG_OF
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop;
	struct max17042_platform_data *pdata;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	/*
	 * Require current sense resistor value to be specified for
	 * current-sense functionality to be enabled at all.
	 */
	if (of_property_read_u32(np, "maxim,rsns-microohm", &prop) == 0) {
		pdata->r_sns = prop;
		pdata->enable_current_sense = true;
	}

	return pdata;
}
#else
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	return dev->platform_data;
}
#endif

static int __devinit max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int ret;
	int reg;
	int temp;
	int battery_id;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	temp_client = client;
	chip->pdata = max17042_get_pdata(&client->dev);
	if (!chip->pdata) {
		dev_err(&client->dev, "no platform data provided\n");
		return -EINVAL;
	}
	i2c_set_clientdata(client, chip);

	ret = max17042_read_reg(chip->client, MAX17042_DevName);
	if (ret == MAX17042_IC_VERSION) {
		dev_info(&client->dev, "chip type max17042 detected\n");
		chip->chip_type = MAX17042;
	} else if (ret == MAX17047_IC_VERSION) {
		dev_info(&client->dev, "chip type max17047/50 detected\n");
		chip->chip_type = MAX17047;
	} else {
		dev_err(&client->dev, "device version mismatch: %x\n", ret);
		return -EIO;
	}

	chip->battery.name		= "max170xx_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.set_property = max17042_set_property;
	chip->battery.property_is_writeable = max17042_property_is_writeable;
	chip->battery.properties = max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	/* When current is not measured,
	 * CURRENT_NOW and CURRENT_AVG properties should be invisible. */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 2;

	if (chip->pdata->r_sns == 0)
		chip->pdata->r_sns = MAX17042_DEFAULT_SNS_RESISTOR;

	if (chip->pdata->init_data)
		max17042_set_reg(client, chip->pdata->init_data,
				chip->pdata->num_init_data);

	if (!chip->pdata->enable_current_sense) {
		max17042_write_reg(client, MAX17042_CGAIN, 0x0000);
		max17042_write_reg(client, MAX17042_MiscCFG, 0x0003);
		max17042_write_reg(client, MAX17042_LearnCFG, 0x0007);
	}

	battery_id = palmas_gpadc_read_physical(PALMAS_ADC_CH_IN0);

	if (battery_id < 0) {
		dev_err(&client->dev, "can't read batter id err = %d\n",
			battery_id);
		chip->pdata->config_data = &pisces_conf_data_samsung;
	} else if (is_between(SAMSUNG_ID_MIN, SAMSUNG_ID_MAX, battery_id)) {
		dev_info(&client->dev, "samsung battery IC %d\n", battery_id);
		chip->pdata->config_data = &pisces_conf_data_samsung;
	} else if (is_between(LG_ID_MIN, LG_ID_MAX, battery_id)) {
		dev_info(&client->dev, "lg battery IC %d\n", battery_id);
		chip->pdata->config_data = &pisces_conf_data_lg;
	} else if (is_between(SONY_ID_MIN, SONY_ID_MAX, battery_id)) {
		dev_info(&client->dev, "sony battery IC %d\n", battery_id);
		chip->pdata->config_data = &pisces_conf_data_sony;
	} else {
		dev_warn(&client->dev, "invalid battery id %d, using default\n",
			 battery_id);
		chip->pdata->config_data = &pisces_conf_data_samsung;
	}

	/* Update model if POR or model data is updated during initialization */
	reg = max17042_read_reg(chip->client, MAX17042_STATUS);
	if ((reg & STATUS_POR_BIT) || max17042_verify_model_update(chip) == 1) {
		dev_info(&client->dev, "status:%x\n", reg);
		if (chip->pdata->enable_por_init && chip->pdata->config_data) {
			ret = max17042_init_chip(chip);
			if (ret)
				return ret;
		}
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
						max17042_thread_handler,
						IRQF_TRIGGER_FALLING,
						chip->battery.name, chip);
		if (!ret) {
			max17042_write_reg(client, MAX17042_STATUS, 0);

			max17042_write_reg(client, MAX17042_TALRT_Th, 0x7f80);

			max17042_set_soc_threshold(chip, 1);

			reg = max17042_read_reg(client, MAX17042_CONFIG);
			/* Clear Alert by software */
			reg |= CONFIG_ALRT_BIT_ENBL | 0x7000;
			max17042_write_reg(client, MAX17042_CONFIG, reg);

		} else {
			client->irq = 0;
			dev_err(&client->dev, "%s(): cannot get IRQ\n",
				__func__);
		}
	}

	if (!chip->pdata->is_battery_present) {
		dev_err(&client->dev, "Battery not detected exiting driver\n");
		return -ENODEV;
	}

	/* Check for battery presence */
	ret = maxim_get_temp(&temp);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading temperaure: %d\n", ret);
		return -ENODEV;
	} else if ((temp < MIN_TEMP) || (temp > MAX_TEMP)) {
		dev_err(&client->dev, "Battery temperaure abnormal, exit\n");
		return -ENODEV;
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	tmp_chip = chip;
	chip->init_complete = 1;
	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17042_init_worker);
	schedule_delayed_work(&chip->work, 10000);

	return 0;
}

static int __devexit max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		free_irq(client->irq, chip);
	power_supply_unregister(&chip->battery);
	return 0;
}

static void max17042_shutdown(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		disable_irq(client->irq);

	cancel_delayed_work_sync(&chip->work);

	chip->shutdown_complete = 1;
}

#ifdef CONFIG_PM
static int max17042_suspend(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	/*
	 * disable the irq and enable irq_wake
	 * capability to the interrupt line.
	 */
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}

	return 0;
}

static int max17042_resume(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
	}

	return 0;
}

static const struct dev_pm_ops max17042_pm_ops = {
	.suspend	= max17042_suspend,
	.resume		= max17042_resume,
};

#define MAX17042_PM_OPS (&max17042_pm_ops)
#else
#define MAX17042_PM_OPS NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id max17042_dt_match[] = {
	{ .compatible = "maxim,max17042" },
	{ .compatible = "maxim,max17047" },
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17042_dt_match);
#endif

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ "max17047", 1 },
	{ "max17050", 2 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
		.of_match_table = of_match_ptr(max17042_dt_match),
		.pm	= MAX17042_PM_OPS,
	},
	.probe		= max17042_probe,
	.remove		= __devexit_p(max17042_remove),
	.id_table	= max17042_id,
	.shutdown	= max17042_shutdown,
};

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}
module_init(max17042_init);

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}
module_exit(max17042_exit);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
