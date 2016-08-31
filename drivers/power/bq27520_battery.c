/*
 *  bq27520_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *  Nathan Zhang <nathanz@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/pm.h>
#include <linux/jiffies.h>
#include <linux/regmap.h>
#include <linux/power/bq27520_battery.h>

#define BQ27520_DELAY			(30*HZ)
#define BQ27520_DEVICE_ID		0x0520

/* Bq27520 standard data commands */
#define BQ27520_STDCMD_CNTL		0x00
#define BQ27520_STDCMD_AR		0x02
#define BQ27520_STDCMD_ARTTE		0x04
#define BQ27520_STDCMD_TEMP		0x06
#define BQ27520_STDCMD_VOLT		0x08
#define BQ27520_STDCMD_FLAGS		0x0A
#define BQ27520_STDCMD_NAC		0x0C
#define BQ27520_STDCMD_FAC		0x0e
#define BQ27520_STDCMD_RM		0x10
#define BQ27520_STDCMD_FCC		0x12
#define BQ27520_STDCMD_AI		0x14
#define BQ27520_STDCMD_TTE		0x16
#define BQ27520_STDCMD_SI		0x18
#define BQ27520_STDCMD_STTE		0x1a
#define BQ27520_STDCMD_SOH		0x1c
#define BQ27520_STDCMD_CC		0x1e
#define BQ27520_STDCMD_SOC		0x20
#define BQ27520_STDCMD_DC		0x2e

/* Control subcommands */
#define BQ27520_SUBCMD_CTNL_STATUS	0x0000
#define BQ27520_SUBCMD_DEVCIE_TYPE	0x0001
#define BQ27520_SUBCMD_FW_VER		0x0002
#define BQ27520_SUBCMD_PREV_MACW	0x0007
#define BQ27520_SUBCMD_CHEM_ID		0x0008
#define BQ27520_SUBCMD_OCV		0x000c
#define BQ27520_SUBCMD_BAT_INS		0x000d
#define BQ27520_SUBCMD_BAT_REM		0x000e
#define BQ27520_SUBCMD_SET_HIB		0x0011
#define BQ27520_SUBCMD_CLR_HIB		0x0012
#define BQ27520_SUBCMD_SET_SLP		0x0013
#define BQ27520_SUBCMD_CLR_SLP		0x0014
#define BQ27520_SUBCMD_SEALED		0x0020
#define BQ27520_SUBCMD_ENABLE_IT	0x0021
#define BQ27520_SUBCMD_RESET		0x0041

/* Flags bit */
#define BQ27520_FLAGS_DSG		(1<<0)
#define BQ27520_FLAGS_SYSDOWN		(1<<1)
#define BQ27520_FLAGS_BAT_DET		(1<<3)
#define BQ27520_FLAGS_CHG		(1<<8)
#define BQ27520_FLAGS_FC		(1<<9)
#define BQ27520_FLAGS_OTD		(1<<14)
#define BQ27520_FLAGS_OTC		(1<<15)

#define BQ27520_BATTERY_LOW		15
#define BQ27520_BATTERY_FULL		100

#define BQ27520_MAX_REGS		0x7F

struct bq27520_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct bq27520_platform_data	*pdata;
	struct battery_gauge_dev	*bg_dev;
	struct regmap			*regmap;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;

	int full_capacity;
	int design_energy;
	int taper_rate;
	int terminate_voltage;
	int v_chg_term;
	int lasttime_soc;
	int lasttime_status;
	int shutdown_complete;
	int charge_complete;
	struct mutex mutex;
};

struct bq27520_chip *bq27520_data;

static const struct regmap_config bq27520_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= BQ27520_MAX_REGS,
};

static u8 bq27520_read_byte(struct i2c_client *client, u8 reg)
{
	int ret;
	u8 val;

	struct bq27520_chip *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	ret = regmap_raw_read(chip->regmap, reg, (u8 *) &val, sizeof(val));
	if (ret < 0) {
		dev_err(&client->dev, "error reading reg: 0x%x\n", reg);
		mutex_unlock(&chip->mutex);
		return ret;
	}

	mutex_unlock(&chip->mutex);
	return val;
}

static int bq27520_write_byte(struct i2c_client *client, u8 reg, u8 value)
{
	struct bq27520_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	ret = regmap_write(chip->regmap, reg, value);
	if (ret < 0)
	    dev_err(&client->dev, "%s(): Failed in writing register 0x%02x err %d\n",
				__func__, reg, ret);

	mutex_unlock(&chip->mutex);
	return ret;
}

static u16 bq27520_read_word(struct i2c_client *client, u8 reg)
{
	u16 val;
	u8 val_lsb, val_msb;

	struct bq27520_chip *chip = i2c_get_clientdata(client);

	val_lsb = bq27520_read_byte(chip->client, reg);
	if (val_lsb < 0)
		return -EINVAL;
	val_msb = bq27520_read_byte(chip->client, reg + 1);
	if (val_msb < 0)
		return -EINVAL;
	val = (((u16)val_msb << 8) & 0xff00) + val_lsb;

	return val;
}

static u16 bq27520_write_word(struct i2c_client *client, u8 reg, u16 value)
{
	int ret;
	u8 val_lsb, val_msb;

	struct bq27520_chip *chip = i2c_get_clientdata(client);

	val_lsb = (u8)(value & 0xff);
	val_msb = (u8)((value & 0xff00) >> 8);

	ret = bq27520_write_byte(chip->client, reg, val_lsb);
	if (ret < 0)
		return -EINVAL;
	ret = bq27520_write_byte(chip->client, reg + 1, val_msb);
	if (ret < 0)
		return -EINVAL;

	return ret;
}

static int bq27520_get_battery_voltage(struct bq27520_chip *chip)
{
	int val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_VOLT);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}

	return val;
}

static int bq27520_get_battery_temperature(struct bq27520_chip *chip)
{
	int val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_TEMP);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}

	return val;
}

static int bq27520_get_battery_present(struct bq27520_chip *chip)
{
	 int val;

	 val = bq27520_read_byte(chip->client, BQ27520_STDCMD_FLAGS);
	 if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	 }

	  if (val & BQ27520_FLAGS_BAT_DET)
		return true;
	  else
		return false;
}

static int bq27520_get_battery_state_of_health(struct bq27520_chip *chip)
{
	int val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_STTE);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}
	return val;
}

static int bq27520_get_battery_avg_current(struct bq27520_chip *chip)
{
	int val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_AI);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}
	return val;
}

static int bq27520_get_battery_state_of_charge(struct bq27520_chip *chip)
{
	int val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_SOC);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}
	return val;
}

static void bq27520_work(struct work_struct *work)
{
	struct bq27520_chip *chip;
	int val;

	chip = container_of(work, struct bq27520_chip, work.work);

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_VOLT);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->vcell = val;

	val = bq27520_read_word(chip->client, BQ27520_STDCMD_SOC);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->soc = val;
	if (chip->soc == 0)
		chip->soc = 1;
	if (chip->soc >= BQ27520_BATTERY_FULL && chip->charge_complete != 1)
		chip->soc = BQ27520_BATTERY_FULL-1;

	if (chip->status == POWER_SUPPLY_STATUS_FULL && chip->charge_complete) {
		chip->soc = BQ27520_BATTERY_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < BQ27520_BATTERY_LOW) {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	} else {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	if (chip->soc != chip->lasttime_soc ||
		chip->status != chip->lasttime_status) {
		chip->lasttime_soc = chip->soc;
		power_supply_changed(&chip->battery);
	}

	schedule_delayed_work(&chip->work, BQ27520_DELAY);
}

static enum power_supply_property bq27520_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
};

static int bq27520_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bq27520_chip *chip = container_of(psy,
				struct bq27520_chip, battery);
	int temperature;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq27520_get_battery_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		if (chip->soc == 15)
			dev_warn(&chip->client->dev,
			"\nSystem Running low on battery - 15 percent\n");
		if (chip->soc == 10)
			dev_warn(&chip->client->dev,
			"\nSystem Running low on battery - 10 percent\n");
		if (chip->soc == 5)
			dev_warn(&chip->client->dev,
			"\nSystem Running low on battery - 5 percent\n");
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		temperature = bq27520_get_battery_temperature(chip);
		val->intval = temperature / 10;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq27520_update_battery_status(struct battery_gauge_dev *bg_dev,
		enum battery_charger_status status)
{
	struct bq27520_chip *chip = battery_gauge_get_drvdata(bg_dev);

	if (status == BATTERY_CHARGING) {
		chip->charge_complete = 0;
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (status == BATTERY_CHARGING_DONE) {
		chip->charge_complete = 1;
		chip->soc = BQ27520_BATTERY_FULL;
		chip->status = POWER_SUPPLY_STATUS_FULL;
		power_supply_changed(&chip->battery);
		return 0;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
		chip->charge_complete = 0;
	}
	chip->lasttime_status = chip->status;
	power_supply_changed(&chip->battery);
	return 0;
}

static struct battery_gauge_ops bq27520_bg_ops = {
	.update_battery_status = bq27520_update_battery_status,
};

static struct battery_gauge_info bq27520_bgi = {
	.cell_id = 0,
	.bg_ops = &bq27520_bg_ops,
};

static int bq27520_initialize(struct bq27520_chip *chip)
{
	int ret;
	u16 u16Dev_type, u16Fw_ver, u16Chem_id;
	int u32Vol, u32Temp, u32Soc, u32Soh, u32Rmcur;
	bool bat;


	ret = bq27520_write_word(chip->client, BQ27520_STDCMD_CNTL, BQ27520_SUBCMD_DEVCIE_TYPE);
	if (ret < 0)
		goto fail;
	u16Dev_type = bq27520_read_word(chip->client, BQ27520_STDCMD_CNTL);
	if (u16Dev_type < 0)
		goto fail;
	dev_info(&chip->client->dev, "DeviceType = 0x%x\n", u16Dev_type);


	ret = bq27520_write_word(chip->client, BQ27520_STDCMD_CNTL, BQ27520_SUBCMD_FW_VER);
	if (ret < 0)
		goto fail;
	u16Fw_ver = bq27520_read_word(chip->client, BQ27520_STDCMD_CNTL);
	if (u16Fw_ver < 0)
		goto fail;
	dev_info(&chip->client->dev, "FirmwareVersion = 0x%x\n", u16Fw_ver);


	ret = bq27520_write_word(chip->client, BQ27520_STDCMD_CNTL, BQ27520_SUBCMD_CHEM_ID);
	if (ret < 0)
		goto fail;
	u16Chem_id = bq27520_read_word(chip->client, BQ27520_STDCMD_CNTL);
	if (u16Chem_id < 0)
		goto fail;
	dev_info(&chip->client->dev, "CHEM_ID = 0x%x\n", u16Chem_id);


	bat = bq27520_get_battery_present(chip);
	if (true == bat)
		dev_info(&chip->client->dev, "Battery is detected !\n");
	else {
		dev_info(&chip->client->dev, "Battery is NOT detected !\n");
		 goto fail;
	 }


	u32Soc = bq27520_get_battery_state_of_charge(chip);
	if (ret < 0)
		goto fail;
	dev_info(&chip->client->dev, "StateOfCharge = %d\n", u32Soc);


	u32Soh = bq27520_get_battery_state_of_health(chip);
	if (u32Soh < 0)
		goto fail;
	dev_info(&chip->client->dev, "StateOfHealth = %d\n", u32Soh);


	u32Rmcur = bq27520_get_battery_avg_current(chip);
	if (u32Rmcur < 0)
		goto fail;
	dev_info(&chip->client->dev, "BatteryAverageCurrent = %d\n", u32Rmcur);


	u32Vol = bq27520_get_battery_voltage(chip);
	if (u32Vol < 0)
		goto fail;
	dev_info(&chip->client->dev, "BatteryVoltage = %d\n", u32Vol);


	u32Temp = bq27520_get_battery_temperature(chip);
	if (u32Temp < 0)
		goto fail;
	dev_info(&chip->client->dev, "BatteryTemperature = %d\n", u32Temp);

	return 0;
fail:
	return -EIO;
}

static int bq27520_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct bq27520_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	chip->pdata = client->dev.platform_data;
	if (!chip->pdata)
		return -ENODATA;

	chip->full_capacity = 1200;

	if (chip->pdata->full_capacity)
		chip->full_capacity = chip->pdata->full_capacity;
	if (chip->pdata->full_energy)
		chip->design_energy = chip->pdata->full_energy;
	if (chip->pdata->taper_rate)
		chip->taper_rate = chip->pdata->taper_rate;
	if (chip->pdata->terminate_voltage)
		chip->terminate_voltage = chip->pdata->terminate_voltage;
	if (chip->pdata->v_at_chg_term)
		chip->v_chg_term = chip->pdata->v_at_chg_term;

	dev_info(&client->dev, "Battery capacity is %d\n", chip->full_capacity);

	bq27520_data = chip;
	mutex_init(&chip->mutex);
	chip->shutdown_complete = 0;
	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= bq27520_get_property;
	chip->battery.properties	= bq27520_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(bq27520_battery_props);
	chip->status			= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->lasttime_status		= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->charge_complete		= 0;

	chip->regmap = devm_regmap_init_i2c(client, &bq27520_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto error;
	}

	ret = bq27520_initialize(chip);
	if (ret < 0) {
		dev_err(&client->dev, "chip init failed - %d\n", ret);
		goto error;
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}

	bq27520_bgi.tz_name = chip->pdata->tz_name;

	chip->bg_dev = battery_gauge_register(&client->dev, &bq27520_bgi,
				chip);
	if (IS_ERR(chip->bg_dev)) {
		ret = PTR_ERR(chip->bg_dev);
		dev_err(&client->dev, "battery gauge register failed: %d\n",
			ret);
		goto bg_err;
	}

	INIT_DEFERRABLE_WORK(&chip->work, bq27520_work);
	schedule_delayed_work(&chip->work, 0);

	return 0;
bg_err:
	power_supply_unregister(&chip->battery);
error:
	mutex_destroy(&chip->mutex);

	return ret;
}

static int bq27520_remove(struct i2c_client *client)
{
	struct bq27520_chip *chip = i2c_get_clientdata(client);

	battery_gauge_unregister(chip->bg_dev);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work_sync(&chip->work);
	mutex_destroy(&chip->mutex);

	return 0;
}

static void bq27520_shutdown(struct i2c_client *client)
{
	struct bq27520_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->work);
	mutex_lock(&chip->mutex);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);

}

#ifdef CONFIG_PM_SLEEP
static int bq27520_suspend(struct device *dev)
{
	struct bq27520_chip *chip = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&chip->work);
	return 0;
}

static int bq27520_resume(struct device *dev)
{
	struct bq27520_chip *chip = dev_get_drvdata(dev);
	schedule_delayed_work(&chip->work, BQ27520_DELAY);
	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(bq27520_pm_ops, bq27520_suspend, bq27520_resume);

#ifdef CONFIG_OF
static const struct of_device_id bq27520_dt_match[] = {
	{ .compatible = "ti,bq27520" },
	{ },
};
MODULE_DEVICE_TABLE(of, bq27520_dt_match);
#endif

static const struct i2c_device_id bq27520_id[] = {
	{ "bq27520", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq27520_id);

static struct i2c_driver bq27520_i2c_driver = {
	.driver	= {
		.name	= "bq27520",
		.of_match_table = of_match_ptr(bq27520_dt_match),
		.pm = &bq27520_pm_ops,
	},
	.probe		= bq27520_probe,
	.remove		= bq27520_remove,
	.id_table	= bq27520_id,
	.shutdown	= bq27520_shutdown,
};

static int __init bq27520_init(void)
{
	return i2c_add_driver(&bq27520_i2c_driver);
}
fs_initcall_sync(bq27520_init);

static void __exit bq27520_exit(void)
{
	i2c_del_driver(&bq27520_i2c_driver);
}
module_exit(bq27520_exit);

MODULE_AUTHOR("Nathan Zhang<nathanz@nvidia.com>");
MODULE_DESCRIPTION("BQ27520 Fuel Gauge");
MODULE_LICENSE("GPL");
