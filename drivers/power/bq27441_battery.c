/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/power/bq27441_battery.h>

#define BQ27441_DELAY			(30*HZ)

#define BQ27441_CONTROL_STATUS		0x0000
#define BQ27441_DEVICE_TYPE		0x0001
#define BQ27441_FW_VERSION		0x0002
#define BQ27441_DM_CODE			0x0004
#define BQ27441_PREV_MACWRITE		0x0007
#define BQ27441_CHEM_ID			0x0008
#define BQ27441_BAT_INSERT		0x000C
#define BQ27441_BAT_REMOVE		0x000D
#define BQ27441_SET_HIBERNATE		0x0011
#define BQ27441_CLEAR_HIBERNATE		0x0012
#define BQ27441_SET_CFGUPDATE		0x0013
#define BQ27441_SHUTDOWN_ENABLE		0x001B
#define BQ27441_SHUTDOWN		0x001C
#define BQ27441_SEALED			0x0020
#define BQ27441_PULSE_SOC_INT		0x0023
#define BQ27441_RESET			0x0041
#define BQ27441_SOFT_RESET		0x0042

#define BQ27441_CONTROL_1		0x00
#define BQ27441_CONTROL_2		0x01
#define BQ27441_TEMPERATURE		0x02
#define BQ27441_VOLTAGE			0x04
#define BQ27441_FLAGS			0x06
#define BQ27441_NOMINAL_AVAIL_CAPACITY	0x08
#define BQ27441_FULL_AVAIL_CAPACITY	0x0a
#define BQ27441_REMAINING_CAPACITY	0x0c
#define BQ27441_FULL_CHG_CAPACITY	0x0e
#define BQ27441_AVG_CURRENT		0x10
#define BQ27441_STANDBY_CURRENT		0x12
#define BQ27441_MAXLOAD_CURRENT		0x14
#define BQ27441_AVERAGE_POWER		0x18
#define BQ27441_STATE_OF_CHARGE		0x1c
#define BQ27441_INT_TEMPERATURE		0x1e
#define BQ27441_STATE_OF_HEALTH		0x20

#define BQ27441_BLOCK_DATA_CHECKSUM	0x60
#define BQ27441_BLOCK_DATA_CONTROL	0x61
#define BQ27441_DATA_BLOCK_CLASS	0x3E
#define BQ27441_DATA_BLOCK		0x3F

#define BQ27441_DESIGN_CAPACITY_1	0x4A
#define BQ27441_DESIGN_CAPACITY_2	0x4B
#define BQ27441_DESIGN_ENERGY_1		0x4C
#define BQ27441_DESIGN_ENERGY_2		0x4D
#define BQ27441_TAPER_RATE_1		0x5B
#define BQ27441_TAPER_RATE_2		0x5C
#define BQ27441_TERMINATE_VOLTAGE_1	0x50
#define BQ27441_TERMINATE_VOLTAGE_2	0x51
#define BQ27441_V_CHG_TERM_1		0x41
#define BQ27441_V_CHG_TERM_2		0x42
#define BQ27441_BATTERY_LOW		15
#define BQ27441_BATTERY_FULL		100

#define BQ27441_MAX_REGS		0x7F

struct bq27441_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct bq27441_platform_data	*pdata;
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

struct bq27441_chip *bq27441_data;

static const struct regmap_config bq27441_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= BQ27441_MAX_REGS,
};

static u16 bq27441_read_word(struct i2c_client *client, u8 reg)
{
	int ret;
	u16 val;

	struct bq27441_chip *chip = i2c_get_clientdata(client);

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

static u8 bq27441_read_byte(struct i2c_client *client, u8 reg)
{
	int ret;
	u8 val;

	struct bq27441_chip *chip = i2c_get_clientdata(client);

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


static int bq27441_write_byte(struct i2c_client *client, u8 reg, u8 value)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	ret = regmap_write(chip->regmap, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register"
					"0x%02x err %d\n", __func__, reg, ret);

	mutex_unlock(&chip->mutex);
	return ret;
}

static void bq27441_work(struct work_struct *work)
{
	struct bq27441_chip *chip;
	int val;

	chip = container_of(work, struct bq27441_chip, work.work);

	val = bq27441_read_word(chip->client, BQ27441_VOLTAGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->vcell = val;

	val = bq27441_read_word(chip->client, BQ27441_STATE_OF_CHARGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->soc = val;
	if (chip->soc >= BQ27441_BATTERY_FULL && chip->charge_complete != 1)
		chip->soc = BQ27441_BATTERY_FULL-1;

	if (chip->status == POWER_SUPPLY_STATUS_FULL && chip->charge_complete) {
		chip->soc = BQ27441_BATTERY_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < BQ27441_BATTERY_LOW) {
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

	schedule_delayed_work(&chip->work, BQ27441_DELAY);
}

static int bq27441_initialize(struct bq27441_chip *chip)
{
	struct i2c_client *client = chip->client;
	int old_csum;
	int temp;
	int new_csum;
	int old_des_cap;
	int old_des_energy;
	int old_taper_rate;
	int old_terminate_voltage;
	int old_v_chg_term;
	int old_des_cap_lsb;
	int old_des_cap_msb;
	int old_taper_rate_lsb;
	int old_taper_rate_msb;
	int old_des_energy_lsb;
	int old_des_energy_msb;
	int old_terminate_voltage_lsb;
	int old_terminate_voltage_msb;
	int old_v_chg_term_msb;
	int old_v_chg_term_lsb;

	unsigned long timeout = jiffies + HZ;
	int ret;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;
	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x13);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	while (!(bq27441_read_byte(client, BQ27441_FLAGS) & 0x10)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&chip->client->dev,
					"timeout waiting for cfg update\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 0x52);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x00);
	if (ret < 0)
		goto fail;

	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	old_des_cap = bq27441_read_word(client, BQ27441_DESIGN_CAPACITY_1);
	old_des_cap_msb = old_des_cap & 0xFF;
	old_des_cap_lsb = (old_des_cap & 0xFF00) >> 8;

	old_des_energy = bq27441_read_word(client, BQ27441_DESIGN_ENERGY_1);
	old_des_energy_msb = old_des_energy & 0xFF;
	old_des_energy_lsb = (old_des_energy & 0xFF00) >> 8;

	old_taper_rate = bq27441_read_word(client, BQ27441_TAPER_RATE_1);
	old_taper_rate_msb = old_taper_rate & 0xFF;
	old_taper_rate_lsb = (old_taper_rate & 0xFF00) >> 8;

	old_terminate_voltage = bq27441_read_word(client,
						BQ27441_TERMINATE_VOLTAGE_1);
	old_terminate_voltage_msb = old_terminate_voltage & 0xFF;
	old_terminate_voltage_lsb = (old_terminate_voltage & 0xFF00) >> 8;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_CAPACITY_1,
					(chip->full_capacity & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_CAPACITY_2,
					chip->full_capacity & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_ENERGY_1,
					(chip->design_energy & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_ENERGY_2,
					chip->design_energy & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TAPER_RATE_1,
					(chip->taper_rate & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TAPER_RATE_2,
					chip->taper_rate & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TERMINATE_VOLTAGE_1,
				(chip->terminate_voltage & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TERMINATE_VOLTAGE_2,
					chip->terminate_voltage & 0xFF);
	if (ret < 0)
		goto fail;

	temp = (255 - old_csum - old_des_cap_lsb - old_des_cap_msb
		- old_des_energy_lsb - old_des_energy_msb
		- old_taper_rate_lsb - old_taper_rate_msb
		- old_terminate_voltage_lsb - old_terminate_voltage_msb);

	new_csum = 255 - ((temp + (chip->full_capacity & 0xFF)
				+ ((chip->full_capacity & 0xFF00) >> 8)
				+ (chip->design_energy & 0xFF)
				+ ((chip->design_energy & 0xFF00) >> 8)
				+ (chip->taper_rate & 0xFF)
				+ ((chip->taper_rate & 0xFF00) >> 8)
				+ (chip->terminate_voltage & 0xFF)
				+ ((chip->terminate_voltage & 0xFF00) >> 8)
				));

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM, new_csum);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 0x52);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x01);
	if (ret < 0)
		goto fail;

	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	old_v_chg_term = bq27441_read_word(client, BQ27441_V_CHG_TERM_1);
	old_v_chg_term_msb = old_v_chg_term & 0xFF;
	old_v_chg_term_lsb = (old_v_chg_term & 0xFF00) >> 8;

	ret = bq27441_write_byte(client, BQ27441_V_CHG_TERM_1,
					(chip->v_chg_term & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_V_CHG_TERM_2,
					chip->v_chg_term & 0xFF);
	if (ret < 0)
		goto fail;

	temp = (255 - old_csum - old_v_chg_term_lsb - old_v_chg_term_msb) % 256;
	new_csum = 255 - ((temp
				+ (chip->v_chg_term & 0xFF)
				+ ((chip->v_chg_term & 0xFF00) >> 8)
				) % 256);

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM, new_csum);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x42);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	while (!(bq27441_read_byte(client, BQ27441_FLAGS) & 0x10)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&chip->client->dev,
					"timeout waiting for cfg update\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x20);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	return -EIO;
}

static int bq27441_get_temperature(struct bq27441_chip *chip)
{
	int val;

	val = bq27441_read_word(chip->client, BQ27441_TEMPERATURE);
	if (val < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
		return -EINVAL;
	}
	return val;
}

static enum power_supply_property bq27441_battery_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
};

static int bq27441_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bq27441_chip *chip = container_of(psy,
				struct bq27441_chip, battery);
	int temperature;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		battery_gauge_record_voltage_value(chip->bg_dev, chip->vcell);
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
		battery_gauge_record_capacity_value(chip->bg_dev, chip->soc);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		temperature = bq27441_get_temperature(chip);
		val->intval = temperature / 10;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq27441_update_battery_status(struct battery_gauge_dev *bg_dev,
		enum battery_charger_status status)
{
	struct bq27441_chip *chip = battery_gauge_get_drvdata(bg_dev);

	if (status == BATTERY_CHARGING) {
		chip->charge_complete = 0;
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (status == BATTERY_CHARGING_DONE) {
		chip->charge_complete = 1;
		chip->soc = BQ27441_BATTERY_FULL;
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

static struct battery_gauge_ops bq27441_bg_ops = {
	.update_battery_status = bq27441_update_battery_status,
};

static struct battery_gauge_info bq27441_bgi = {
	.cell_id = 0,
	.bg_ops = &bq27441_bg_ops,
};

static void of_bq27441_parse_platform_data(struct i2c_client *client,
				struct bq27441_platform_data *pdata)
{
	u32 tmp;
	char const *pstr;
	struct device_node *np = client->dev.of_node;

	if (!of_property_read_u32(np, "ti,design-capacity", &tmp))
		pdata->full_capacity = (unsigned long)tmp;

	if (!of_property_read_u32(np, "ti,design-energy", &tmp))
		pdata->full_energy = (unsigned long)tmp;

	if (!of_property_read_u32(np, "ti,taper-rate", &tmp))
		pdata->taper_rate = (unsigned long)tmp;

	if (!of_property_read_u32(np, "ti,terminate-voltage", &tmp))
		pdata->terminate_voltage = (unsigned long)tmp;

	if (!of_property_read_u32(np, "ti,v-at-chg-term", &tmp))
		pdata->v_at_chg_term = (unsigned long)tmp;

	if (!of_property_read_string(np, "ti,tz-name", &pstr))
		pdata->tz_name = pstr;
	else
		dev_err(&client->dev, "Failed to read tz-name\n");
}

static int bq27441_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct bq27441_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	if (client->dev.of_node) {
		chip->pdata = devm_kzalloc(&client->dev,
					sizeof(*chip->pdata), GFP_KERNEL);
		if (!chip->pdata)
			return -ENOMEM;
		of_bq27441_parse_platform_data(client, chip->pdata);
	} else {
		chip->pdata = client->dev.platform_data;
	}

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

	bq27441_data = chip;
	mutex_init(&chip->mutex);
	chip->shutdown_complete = 0;
	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= bq27441_get_property;
	chip->battery.properties	= bq27441_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(bq27441_battery_props);
	chip->status			= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->lasttime_status		= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->charge_complete		= 0;

	chip->regmap = devm_regmap_init_i2c(client, &bq27441_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto error;
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}

	bq27441_bgi.tz_name = chip->pdata->tz_name;

	chip->bg_dev = battery_gauge_register(&client->dev, &bq27441_bgi,
				chip);
	if (IS_ERR(chip->bg_dev)) {
		ret = PTR_ERR(chip->bg_dev);
		dev_err(&client->dev, "battery gauge register failed: %d\n",
			ret);
		goto bg_err;
	}

	ret = bq27441_initialize(chip);
	if (ret < 0)
		dev_err(&client->dev, "chip init failed - %d\n", ret);

	INIT_DEFERRABLE_WORK(&chip->work, bq27441_work);
	schedule_delayed_work(&chip->work, 0);

	battery_gauge_record_snapshot_values(chip->bg_dev,
				jiffies_to_msecs(BQ27441_DELAY/2));

	return 0;
bg_err:
	power_supply_unregister(&chip->battery);
error:
	mutex_destroy(&chip->mutex);

	return ret;
}

static int bq27441_remove(struct i2c_client *client)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);

	battery_gauge_unregister(chip->bg_dev);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work_sync(&chip->work);
	mutex_destroy(&chip->mutex);

	return 0;
}

static void bq27441_shutdown(struct i2c_client *client)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->work);
	mutex_lock(&chip->mutex);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);

}

#ifdef CONFIG_PM_SLEEP
static int bq27441_suspend(struct device *dev)
{
	struct bq27441_chip *chip = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&chip->work);
	return 0;
}

static int bq27441_resume(struct device *dev)
{
	struct bq27441_chip *chip = dev_get_drvdata(dev);
	schedule_delayed_work(&chip->work, BQ27441_DELAY);
	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(bq27441_pm_ops, bq27441_suspend, bq27441_resume);

#ifdef CONFIG_OF
static const struct of_device_id bq27441_dt_match[] = {
	{ .compatible = "ti,bq27441" },
	{ },
};
MODULE_DEVICE_TABLE(of, bq27441_dt_match);
#endif

static const struct i2c_device_id bq27441_id[] = {
	{ "bq27441", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq27441_id);

static struct i2c_driver bq27441_i2c_driver = {
	.driver	= {
		.name	= "bq27441",
		.of_match_table = of_match_ptr(bq27441_dt_match),
		.pm = &bq27441_pm_ops,
	},
	.probe		= bq27441_probe,
	.remove		= bq27441_remove,
	.id_table	= bq27441_id,
	.shutdown	= bq27441_shutdown,
};

static int __init bq27441_init(void)
{
	return i2c_add_driver(&bq27441_i2c_driver);
}
fs_initcall_sync(bq27441_init);

static void __exit bq27441_exit(void)
{
	i2c_del_driver(&bq27441_i2c_driver);
}
module_exit(bq27441_exit);

MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_DESCRIPTION("BQ27441 Fuel Gauge");
MODULE_LICENSE("GPL");
