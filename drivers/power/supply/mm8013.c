// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#include "mtk_battery.h"

#define DRIVER_VERSION			"1.0.0"
#define REG_CTRL_0			0x00
#define REG_TEMPERATURE			0x06
#define REG_VOLTAGE			0x08
#define REG_CURRENT			0x14
#define REG_RSOC			0x2c
#define REG_BLOCKDATAOFFSET		0x3e
#define REG_BLOCKDATA			0x40
#define BATTERY_QMAX			7500
#define BATTERY_CV			4350

#define MM8013C_DEFAULT_SOC_VALUE		51
#define MM8013C_DEFAULT_TEMP_VALUE		260
#define MM8013C_DEFAULT_VOLAGE_VALUE		3900
#define MM8013C_DEFAULT_CURRENT_VALUE		500

struct battery_info {
	int status;
	int health;
	int present;
	int technology;
	int cycle_count;
	int capacity;
	int current_now;
	int current_avg;
	int voltage_now;
	int charger_full;
	int charger_counter;
	int battery_temp;
	int capacoty_level;
	int time_to_full_now;
	int charger_full_design;
	int constant_charge_voltage;
};

struct mm8013_chip {
	struct device *dev;
	struct i2c_client *client;
	struct power_supply_desc battery;
	struct power_supply *mm8013_psy;
	struct power_supply *chg_psy;
	struct battery_info bat_data;
	struct delayed_work work;
	bool is_probe_done;
};

struct mm8013_chip *chip;
bool has_8013;

static int mm8013_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = 0;

	if (client == NULL) {
		pr_info("%s:client is NULL!!\n", __func__);

		return -1;
	}
	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);
	msleep(20);

	return ret;
}

int mm8013_soc(int *val)
{
	int soc = 0;

	if (has_8013) {
		soc = mm8013_read_reg(chip->client, REG_RSOC);
		*val = soc;
	} else {
		*val =	MM8013C_DEFAULT_SOC_VALUE;
	}

	return 0;
}
EXPORT_SYMBOL(mm8013_soc);

int mm8013_voltage(int *val)
{
	int volt = 0;

	if (has_8013) {
		volt = mm8013_read_reg(chip->client, REG_VOLTAGE);
		*val = volt;
	} else {
		*val = MM8013C_DEFAULT_VOLAGE_VALUE;
	}

	return 0;
}
EXPORT_SYMBOL(mm8013_voltage);

int mm8013_current(int *val)
{
	int curr = 0;

	if (has_8013) {
		curr = mm8013_read_reg(chip->client, REG_CURRENT);
		if (curr > 32767)
			curr -= 65536;
		*val = curr;
	} else {
		*val =	MM8013C_DEFAULT_CURRENT_VALUE;
	}

	return 0;
}
EXPORT_SYMBOL(mm8013_current);

int mm8013_temperature(int *val)
{
	int temp = 0;

	if (has_8013) {
		temp = mm8013_read_reg(chip->client, REG_TEMPERATURE);
		*val = temp - 2731;
	} else
		*val =	MM8013C_DEFAULT_TEMP_VALUE;

	return 0;
}
EXPORT_SYMBOL(mm8013_temperature);

static int mm8013_checkdevice(void)
{
	int ret = 0;
	int count = 3;

	while (count--) {
		ret = mm8013_read_reg(chip->client, REG_CTRL_0);
		if (ret > 0)
			break;
	}

	return ret;
}

static enum power_supply_property mm8013_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

static int mm8013_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret  = 0;
	int ui_soc;
	union power_supply_propval online, status;
	union power_supply_propval tmp_val;

	if (has_8013) {
		switch (psp) {
		case POWER_SUPPLY_PROP_CAPACITY:
			ret = mm8013_soc(&val->intval);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			ret = mm8013_voltage(&val->intval);
			val->intval = ((val->intval) * 1000);
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			ret = mm8013_current(&val->intval);
			val->intval = ((val->intval) * 1000);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			ret = mm8013_temperature(&val->intval);
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			ret = mm8013_voltage(&val->intval);
			if (val->intval <= 2000)
				val->intval = 0;
			else
				val->intval = 1;
			break;
		/* mtk add */
		case POWER_SUPPLY_PROP_STATUS:
			if (IS_ERR_OR_NULL(chip->chg_psy)) {
				chip->chg_psy =
					devm_power_supply_get_by_phandle(chip->dev, "charger");
				pr_info("[%s]: get charger phandle fail\n", __func__);
				val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			} else {
				ret = power_supply_get_property(chip->chg_psy,
					POWER_SUPPLY_PROP_ONLINE, &online);

				ret = power_supply_get_property(chip->chg_psy,
					POWER_SUPPLY_PROP_STATUS, &status);

				if (!online.intval)
					val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				else
					val->intval  = status.intval;
			}
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			val->intval = 1;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval = BATTERY_QMAX * 1000;
			break;
		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = mm8013_soc(&val->intval);
			val->intval = (val->intval) * ((BATTERY_QMAX * 1000) / 100);
			break;
		case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
			mm8013_soc(&(tmp_val.intval));
			val->intval = check_cap_level(tmp_val.intval);
			break;
		case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
			mm8013_soc(&tmp_val.intval);
			ui_soc = tmp_val.intval;
			ret = check_cap_level(ui_soc);
			if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) ||
				(ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN))
				val->intval = 0;
			else {
				int q_max_now = BATTERY_QMAX;
				int remain_ui = 100 - ui_soc;
				int remain_mah = remain_ui * q_max_now / 100;
				int current_now = 0;
				int time_to_full = 0;

				mm8013_current(&tmp_val.intval);
				current_now = tmp_val.intval;
				if (current_now != 0)
					time_to_full = remain_mah * 3600 / current_now;
					pr_info("time_to_full:%d, remain:ui:%d mah:%d, current_now:%d, qmax:%d\n",
						time_to_full, remain_ui, remain_mah,
						current_now, q_max_now);
					val->intval = abs(time_to_full);
				}
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			val->intval = BATTERY_CV;
			break;
		default:
			return -EINVAL;
		}
	} else {
		pr_info("failed: %s!\n", __func__);
		val->intval = -99;
	}

	return ret;
}

static void mm8013_external_power_changed(struct power_supply *psy)
{
	if (chip->is_probe_done == false) {
		pr_info("[%s]mm8013 probe is not rdy:%d\n",
			__func__, chip->is_probe_done);
		return;
	}

	power_supply_changed(chip->mm8013_psy);
}

static int	mm8013_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct device *cdev = &client->dev;
	int ret;
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {0};

	has_8013 = false;
	pr_info("%s start!\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		pr_info("failed: %s smbus data not supported!\n", __func__);
		return -EIO;
	}

	chip = devm_kzalloc(cdev, sizeof(struct mm8013_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;

	i2c_set_clientdata(client, chip);

	ret = mm8013_checkdevice();
	if (ret < 0)
		pr_info("failed to access\n");
	else
		has_8013 = true;

	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;

	psy_cfg.drv_data = chip;
	psy_desc->name = "battery";
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = mm8013_battery_props;
	psy_desc->num_properties = ARRAY_SIZE(mm8013_battery_props);
	psy_desc->get_property = mm8013_get_property;
	psy_desc->external_power_changed = mm8013_external_power_changed;
	psy_desc->set_property = NULL;

	chip->mm8013_psy = power_supply_register(&client->dev, psy_desc, &psy_cfg);
	if (IS_ERR(chip->mm8013_psy)) {
		ret = PTR_ERR(chip->mm8013_psy);
		pr_info("failed to register battery: %d\n", ret);

		return ret;
	}

	pr_info("%s success!\n", __func__);
	chip->is_probe_done = true;

	return 0;
}

static	struct i2c_device_id mm8013_id_table[] = {
	{ "mm8013", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mm8013_id_table);

static const struct of_device_id mm8013_match_table[] = {
	{ .compatible = "nvt,mm8013",},
	{},
};

static struct i2c_driver mm8013_i2c_driver = {
	.driver    = {
	.name  = "mm8013",
	.owner = THIS_MODULE,
	.of_match_table = mm8013_match_table,
	},
	.probe	   = mm8013_probe,
	.id_table  = mm8013_id_table,
};

static int __init mm8013_i2c_init(void)
{
	return i2c_add_driver(&mm8013_i2c_driver);
}
static void __exit mm8013_i2c_exit(void)
{
	i2c_del_driver(&mm8013_i2c_driver);
}

module_init(mm8013_i2c_init);
module_exit(mm8013_i2c_exit);
MODULE_DESCRIPTION("I2c bus driver for mm8013x gauge");
MODULE_LICENSE("GPL v2");
