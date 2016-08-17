/*
 * Gas Gauge driver for SBS Compliant Batteries
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include <linux/power/sbs-battery.h>

enum {
	REG_MANUFACTURER_DATA,
	REG_TEMPERATURE,
	REG_VOLTAGE,
	REG_CURRENT,
	REG_CAPACITY,
	REG_TIME_TO_EMPTY,
	REG_TIME_TO_FULL,
	REG_STATUS,
	REG_CYCLE_COUNT,
	REG_SERIAL_NUMBER,
	REG_REMAINING_CAPACITY,
	REG_REMAINING_CAPACITY_CHARGE,
	REG_FULL_CHARGE_CAPACITY,
	REG_FULL_CHARGE_CAPACITY_CHARGE,
	REG_DESIGN_CAPACITY,
	REG_DESIGN_CAPACITY_CHARGE,
	REG_DESIGN_VOLTAGE,
};

/* Battery Mode defines */
#define BATTERY_MODE_OFFSET		0x03
#define BATTERY_MODE_MASK		0x8000
enum sbs_battery_mode {
	BATTERY_MODE_AMPS,
	BATTERY_MODE_WATTS
};

/* manufacturer access defines */
#define MANUFACTURER_ACCESS_STATUS	0x0006
#define MANUFACTURER_ACCESS_SLEEP	0x0011

/* battery status value bits */
#define BATTERY_DISCHARGING		0x40
#define BATTERY_FULL_CHARGED		0x20
#define BATTERY_FULL_DISCHARGED		0x10

#define SBS_DATA(_psp, _addr, _min_value, _max_value) { \
	.psp = _psp, \
	.addr = _addr, \
	.min_value = _min_value, \
	.max_value = _max_value, \
}
struct i2c_client *tclient = NULL;
int battery_detect = 1;

static const struct chip_data {
	enum power_supply_property psp;
	u8 addr;
	int min_value;
	int max_value;
} sbs_data[] = {
	[REG_MANUFACTURER_DATA] =
		SBS_DATA(POWER_SUPPLY_PROP_PRESENT, 0x00, 0, 65535),
	[REG_TEMPERATURE] =
		SBS_DATA(POWER_SUPPLY_PROP_TEMP, 0x08, 0, 65535),
	[REG_VOLTAGE] =
		SBS_DATA(POWER_SUPPLY_PROP_VOLTAGE_NOW, 0x09, 0, 20000),
	[REG_CURRENT] =
		SBS_DATA(POWER_SUPPLY_PROP_CURRENT_NOW, 0x0A, -32768, 32767),
	[REG_CAPACITY] =
		SBS_DATA(POWER_SUPPLY_PROP_CAPACITY, 0x0D, 0, 100),
	[REG_REMAINING_CAPACITY] =
		SBS_DATA(POWER_SUPPLY_PROP_ENERGY_NOW, 0x0F, 0, 65535),
	[REG_REMAINING_CAPACITY_CHARGE] =
		SBS_DATA(POWER_SUPPLY_PROP_CHARGE_NOW, 0x0F, 0, 65535),
	[REG_FULL_CHARGE_CAPACITY] =
		SBS_DATA(POWER_SUPPLY_PROP_ENERGY_FULL, 0x10, 0, 65535),
	[REG_FULL_CHARGE_CAPACITY_CHARGE] =
		SBS_DATA(POWER_SUPPLY_PROP_CHARGE_FULL, 0x10, 0, 65535),
	[REG_TIME_TO_EMPTY] =
		SBS_DATA(POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG, 0x12, 0, 65535),
	[REG_TIME_TO_FULL] =
		SBS_DATA(POWER_SUPPLY_PROP_TIME_TO_FULL_AVG, 0x13, 0, 65535),
	[REG_STATUS] =
		SBS_DATA(POWER_SUPPLY_PROP_STATUS, 0x16, 0, 65535),
	[REG_CYCLE_COUNT] =
		SBS_DATA(POWER_SUPPLY_PROP_CYCLE_COUNT, 0x17, 0, 65535),
	[REG_DESIGN_CAPACITY] =
		SBS_DATA(POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, 0x18, 0, 65535),
	[REG_DESIGN_CAPACITY_CHARGE] =
		SBS_DATA(POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, 0x18, 0, 65535),
	[REG_DESIGN_VOLTAGE] =
		SBS_DATA(POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, 0x19, 0, 65535),
	[REG_SERIAL_NUMBER] =
		SBS_DATA(POWER_SUPPLY_PROP_SERIAL_NUMBER, 0x1C, 0, 65535),
};

static enum power_supply_property sbs_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

struct sbs_info {
	struct i2c_client		*client;
	struct power_supply		power_supply;
	struct sbs_platform_data	plat_data;
	bool				gpio_detect;
	bool				enable_detection;
	int				irq;
	int				last_state;
	int				poll_time;
	struct delayed_work		work;
	int				battery_status;
	int				battery_health;
	int				battery_present;
	int				battery_cycle_count;
	int				battery_voltage_now;
	int				battery_current_now;
	int				battery_capacity;
	int				battery_temp;
	int				battery_time_to_empty;
	int				battery_time_to_full;
	char				battery_serial_num[5];
	int				battery_voltage_max;
	int				battery_energy_now;
	int				battery_energy_full;
	int				battery_energy_design;
	int				battery_charge_now;
	int				battery_charge_full;
	int				battery_charge_design;
	int				ignore_changes;
	int				shutdown_complete;
	struct mutex			mutex;
};
struct sbs_info *tchip;

static int sbs_read_word_data(struct i2c_client *client, u8 address)
{
	struct sbs_info *chip = i2c_get_clientdata(client);
	s32 ret = 0;
	int retries = 1;

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}
	retries = max(chip->plat_data.i2c_retry_count + 1, 1);

	while (retries > 0) {
		ret = i2c_smbus_read_word_data(client, address);
		if (ret >= 0)
			break;
		retries--;
	}

	if (ret < 0) {
		dev_dbg(&client->dev,
			"%s: i2c read at address 0x%x failed\n",
			__func__, address);
		mutex_unlock(&chip->mutex);
		return ret;
	}
	mutex_unlock(&chip->mutex);

	return le16_to_cpu(ret);
}

static int sbs_write_word_data(struct i2c_client *client, u8 address,
	u16 value)
{
	struct sbs_info *chip = i2c_get_clientdata(client);
	s32 ret = 0;
	int retries = 1;

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	retries = max(chip->plat_data.i2c_retry_count + 1, 1);

	while (retries > 0) {
		ret = i2c_smbus_write_word_data(client, address,
			le16_to_cpu(value));
		if (ret >= 0)
			break;
		retries--;
	}

	if (ret < 0) {
		dev_dbg(&client->dev,
			"%s: i2c write to address 0x%x failed\n",
			__func__, address);
		mutex_unlock(&chip->mutex);
		return ret;
	}
	mutex_unlock(&chip->mutex);

	return 0;
}

static int sbs_get_battery_presence_and_health(
	struct i2c_client *client, enum power_supply_property psp,
	int *val)
{
	s32 ret;
	struct sbs_info *chip = i2c_get_clientdata(client);

	if (psp == POWER_SUPPLY_PROP_PRESENT &&
		chip->gpio_detect) {
		ret = gpio_get_value(chip->plat_data.battery_detect);
		if (ret == chip->plat_data.battery_detect_present)
			*val = 1;
		else
			*val = 0;
		return ret;
	}

	/* Write to ManufacturerAccess with
	 * ManufacturerAccess command and then
	 * read the status */
	ret = sbs_write_word_data(client, sbs_data[REG_MANUFACTURER_DATA].addr,
					MANUFACTURER_ACCESS_STATUS);
	if (ret < 0) {
		if (psp == POWER_SUPPLY_PROP_PRESENT)
			*val = 0; /* battery removed */
		return ret;
	}

	ret = sbs_read_word_data(client, sbs_data[REG_MANUFACTURER_DATA].addr);
	if (ret < 0)
		return ret;

	if (ret < sbs_data[REG_MANUFACTURER_DATA].min_value ||
	    ret > sbs_data[REG_MANUFACTURER_DATA].max_value) {
		*val = 0;
		return 0;
	}

	/* Mask the upper nibble of 2nd byte and
	 * lower byte of response then
	 * shift the result by 8 to get status*/
	ret &= 0x0F00;
	ret >>= 8;
	if (psp == POWER_SUPPLY_PROP_PRESENT) {
		if (ret == 0x0F)
			/* battery removed */
			*val = 0;
		else
			*val = 1;
	} else if (psp == POWER_SUPPLY_PROP_HEALTH) {
		if (ret == 0x09)
			*val = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		else if (ret == 0x0B)
			*val = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (ret == 0x0C)
			*val = POWER_SUPPLY_HEALTH_DEAD;
		else
			*val = POWER_SUPPLY_HEALTH_GOOD;
	}

	return 0;
}


static int sbs_get_property_index(struct i2c_client *client,
	enum power_supply_property psp)
{
	int count;
	for (count = 0; count < ARRAY_SIZE(sbs_data); count++)
		if (psp == sbs_data[count].psp)
			return count;

	dev_warn(&client->dev,
		"%s: Invalid Property - %d\n", __func__, psp);

	return -EINVAL;
}

static int sbs_get_battery_property(struct i2c_client *client,
	enum power_supply_property psp,
	int *val)
{
	int reg_offset;
	s32 ret;

	reg_offset = sbs_get_property_index(client, psp);

	ret = sbs_read_word_data(client, sbs_data[reg_offset].addr);
	if (ret < 0)
		return ret;

	/* returned values are 16 bit */
	if (sbs_data[reg_offset].min_value < 0)
		ret = (s16)ret;

	if (ret >= sbs_data[reg_offset].min_value &&
	    ret <= sbs_data[reg_offset].max_value) {
		*val = ret;
		if (psp != POWER_SUPPLY_PROP_STATUS)
			return 0;

		if (ret & BATTERY_FULL_CHARGED)
			*val = POWER_SUPPLY_STATUS_FULL;
		else if (ret & BATTERY_FULL_DISCHARGED)
			*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (ret & BATTERY_DISCHARGING)
			*val = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			*val = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (psp == POWER_SUPPLY_PROP_STATUS)
			*val = POWER_SUPPLY_STATUS_UNKNOWN;
		else
			*val = 0;
	}

	return 0;
}

static void  sbs_unit_adjustment(struct i2c_client *client,
	enum power_supply_property psp, union power_supply_propval *val)
{
#define BASE_UNIT_CONVERSION		1000
#define BATTERY_MODE_CAP_MULT_WATT	(10 * BASE_UNIT_CONVERSION)
#define TIME_UNIT_CONVERSION		60
#define TEMP_KELVIN_TO_CELSIUS		2731
	switch (psp) {
	case POWER_SUPPLY_PROP_ENERGY_NOW:
	case POWER_SUPPLY_PROP_ENERGY_FULL:
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		/* sbs provides energy in units of 10mWh.
		 * Convert to µWh
		 */
		val->intval *= BATTERY_MODE_CAP_MULT_WATT;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval *= BASE_UNIT_CONVERSION;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		/* sbs provides battery temperature in 0.1K
		 * so convert it to 0.1°C
		 */
		val->intval -= TEMP_KELVIN_TO_CELSIUS;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		/* sbs provides time to empty and time to full in minutes.
		 * Convert to seconds
		 */
		val->intval *= TIME_UNIT_CONVERSION;
		break;

	default:
		dev_dbg(&client->dev,
			"%s: no need for unit conversion %d\n", __func__, psp);
	}
}

static enum sbs_battery_mode sbs_set_battery_mode(struct i2c_client *client,
	enum sbs_battery_mode mode)
{
	int ret, original_val;

	original_val = sbs_read_word_data(client, BATTERY_MODE_OFFSET);
	if (original_val < 0)
		return original_val;

	if ((original_val & BATTERY_MODE_MASK) == mode)
		return mode;

	if (mode == BATTERY_MODE_AMPS)
		ret = original_val & ~BATTERY_MODE_MASK;
	else
		ret = original_val | BATTERY_MODE_MASK;

	ret = sbs_write_word_data(client, BATTERY_MODE_OFFSET, ret);
	if (ret < 0)
		return ret;

	return original_val & BATTERY_MODE_MASK;
}



static int sbs_get_battery_capacity(struct i2c_client *client,
	 enum power_supply_property psp, int *val)
{
	s32 ret;
	int reg_offset;
	enum sbs_battery_mode mode = BATTERY_MODE_WATTS;

	reg_offset = sbs_get_property_index(client, psp);

	if (power_supply_is_amp_property(psp))
		mode = BATTERY_MODE_AMPS;

	mode = sbs_set_battery_mode(client, mode);
	if (mode < 0)
		return mode;

	ret = sbs_read_word_data(client, sbs_data[reg_offset].addr);
	if (ret < 0)
		return ret;

	if (psp == POWER_SUPPLY_PROP_CAPACITY) {
		/* sbs spec says that this can be >100 %
		* even if max value is 100 % */
		*val = min(ret, 100);
	} else
		*val = ret;

	ret = sbs_set_battery_mode(client, mode);
	if (ret < 0)
		return ret;

	return 0;
}

static int sbs_get_battery_serial_number(struct i2c_client *client,
	const char *strval)
{
	int ret;

	ret = sbs_read_word_data(client, sbs_data[REG_SERIAL_NUMBER].addr);
	if (ret < 0)
		return ret;

	ret = sprintf(strval, "%04x", ret);

	return 0;
}


static int sbs_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct sbs_info *chip = container_of(psy,
				struct sbs_info, power_supply);
	struct i2c_client *client = chip->client;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->battery_present;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->battery_health;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = chip->battery_energy_now;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = chip->battery_energy_full;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = chip->battery_energy_design;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = chip->battery_charge_now;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = chip->battery_charge_full;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->battery_charge_design;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->battery_capacity;
		break;

	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = chip->battery_serial_num;
		break;

	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->battery_status;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = chip->battery_cycle_count;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->battery_voltage_now;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->battery_current_now;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->battery_temp;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		val->intval = chip->battery_time_to_empty;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		val->intval = chip->battery_time_to_full;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->battery_voltage_max;
		break;

	default:
		dev_err(&client->dev,
			"%s: INVALID property\n", __func__);
		return -EINVAL;
	}

	sbs_unit_adjustment(client, psp, val);

	/* battery not present, so return NODATA for properties */
	if (!chip->battery_present)
		return -ENODATA;


	return 0;
}

static irqreturn_t sbs_irq(int irq, void *devid)
{
	struct power_supply *battery = devid;

	power_supply_changed(battery);

	return IRQ_HANDLED;
}

static void sbs_external_power_changed(struct power_supply *psy)
{
	struct sbs_info *chip;

	chip = container_of(psy, struct sbs_info, power_supply);

	if (chip->ignore_changes > 0) {
		chip->ignore_changes--;
		return;
	}

	/* cancel outstanding work */
	cancel_delayed_work_sync(&chip->work);

	schedule_delayed_work(&chip->work, HZ);
	chip->poll_time = chip->plat_data.poll_retry_count;
}

void sbs_update(void)
{
	int ret;

	if (tchip != NULL) {
		ret = sbs_read_word_data(tchip->client,
				sbs_data[REG_STATUS].addr);
		/* if the read failed, give up on this work */
		if (ret < 0) {
			tchip->poll_time = 0;
			return;
		}

		if (ret & BATTERY_FULL_CHARGED)
			ret = POWER_SUPPLY_STATUS_FULL;
		else if (ret & BATTERY_FULL_DISCHARGED)
			ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (ret & BATTERY_DISCHARGING)
			ret = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;

		if (tchip->last_state != ret) {
			tchip->poll_time = 0;
			power_supply_changed(&tchip->power_supply);
			return;
		}
	}
}
EXPORT_SYMBOL_GPL(sbs_update);

static int sbs_update_health_properties(struct sbs_info *chip)
{
	int ret;

	ret = sbs_get_battery_presence_and_health(chip->client,
				POWER_SUPPLY_PROP_PRESENT,
				&chip->battery_present);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery presence info\n");
		return ret;
	}

	ret = sbs_get_battery_presence_and_health(chip->client,
				POWER_SUPPLY_PROP_HEALTH,
				&chip->battery_health);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery health info\n");
		return ret;
	}

	return 0;
}

static int sbs_update_capacity_properties(struct sbs_info *chip)
{
	int ret;

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_ENERGY_NOW,
				&chip->battery_energy_now);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery energy now info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_ENERGY_FULL,
				&chip->battery_energy_full);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery full energy info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
				&chip->battery_energy_design);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery design energy info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_CHARGE_NOW,
				&chip->battery_charge_now);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery charge info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_CHARGE_FULL,
				&chip->battery_charge_full);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery full charge info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
				&chip->battery_charge_design);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery design charge info\n");
		return ret;
	}

	ret = sbs_get_battery_capacity(chip->client,
				POWER_SUPPLY_PROP_CAPACITY,
				&chip->battery_capacity);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery capacity info\n");
		return ret;
	}

	return 0;
}

static int sbs_update_serial_number_property(struct sbs_info *chip)
{
	int ret;
	ret = sbs_get_battery_serial_number(chip->client,
					chip->battery_serial_num);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery serial number info\n");
		return ret;
	}

	return 0;
}

static int sbs_update_battery_properties(struct sbs_info *chip)
{
	int ret;

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_STATUS,
				&chip->battery_status);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery status info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_CYCLE_COUNT,
				&chip->battery_cycle_count);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery cycle count info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_VOLTAGE_NOW,
				&chip->battery_voltage_now);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery voltage info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_CURRENT_NOW,
				&chip->battery_current_now);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery current now info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_TEMP,
				&chip->battery_temp);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery temperature info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
				&chip->battery_time_to_empty);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery time to empty info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
				&chip->battery_time_to_full);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery presence info\n");
		return ret;
	}

	ret = sbs_get_battery_property(chip->client,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
				&chip->battery_voltage_max);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to get battery presence info\n");
		return ret;
	}

	return 0;
}

static int sbs_delayed_work(struct work_struct *work)
{
	struct sbs_info *chip;
	int ret;
	chip = container_of(work, struct sbs_info, work.work);

	ret = sbs_update_health_properties(chip);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to update health properties\n");
		goto battery_detect;
	}

	ret = sbs_update_capacity_properties(chip);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to update capacity properties\n");
		goto battery_detect;
	}

	ret = sbs_update_serial_number_property(chip);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to update serial number property\n");
		goto battery_detect;
	}

	ret = sbs_update_battery_properties(chip);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to update battery properties\n");
	}

battery_detect:
	if (chip->enable_detection) {
		if (!chip->gpio_detect &&
			chip->battery_present != (ret >= 0)) {
			chip->battery_present = (ret >= 0);
		}
	}

	power_supply_changed(&chip->power_supply);
	schedule_delayed_work(&chip->work, HZ*2);

	return ret;
}

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>

static const struct of_device_id sbs_dt_ids[] = {
	{ .compatible = "sbs,sbs-battery" },
	{ .compatible = "ti,bq20z75" },
	{ }
};
MODULE_DEVICE_TABLE(of, sbs_dt_ids);

static struct sbs_platform_data *sbs_of_populate_pdata(
		struct i2c_client *client)
{
	struct device_node *of_node = client->dev.of_node;
	struct sbs_platform_data *pdata = client->dev.platform_data;
	enum of_gpio_flags gpio_flags;
	int rc;
	u32 prop;

	/* if platform data is set, honor it */
	if (pdata)
		return pdata;

	/* verify this driver matches this device */
	if (!of_node)
		return NULL;


	/* first make sure at least one property is set, otherwise
	 * it won't change behavior from running without pdata.
	 */
	if (!of_get_property(of_node, "sbs,i2c-retry-count", NULL) &&
		!of_get_property(of_node, "sbs,poll-retry-count", NULL) &&
		!of_get_property(of_node, "sbs,battery-detect-gpios", NULL))
		goto of_out;

	pdata = devm_kzalloc(&client->dev, sizeof(struct sbs_platform_data),
				GFP_KERNEL);
	if (!pdata)
		goto of_out;

	rc = of_property_read_u32(of_node, "sbs,i2c-retry-count", &prop);
	if (!rc)
		pdata->i2c_retry_count = prop;

	rc = of_property_read_u32(of_node, "sbs,poll-retry-count", &prop);
	if (!rc)
		pdata->poll_retry_count = prop;

	if (!of_get_property(of_node, "sbs,battery-detect-gpios", NULL)) {
		pdata->battery_detect = -1;
		goto of_out;
	}

	pdata->battery_detect = of_get_named_gpio_flags(of_node,
			"sbs,battery-detect-gpios", 0, &gpio_flags);

	if (gpio_flags & OF_GPIO_ACTIVE_LOW)
		pdata->battery_detect_present = 0;
	else
		pdata->battery_detect_present = 1;

of_out:
	return pdata;
}
#else
#define sbs_dt_ids NULL
static struct sbs_platform_data *sbs_of_populate_pdata(
	struct i2c_client *client)
{
	return client->dev.platform_data;
}
#endif

int sbs_battery_detect(void)
{
	if (tclient != NULL && battery_detect)
		return sbs_read_word_data(tclient,
			sbs_data[REG_SERIAL_NUMBER].addr);
	return -EINVAL;

}
EXPORT_SYMBOL_GPL(sbs_battery_detect);

static int __devinit sbs_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct sbs_info *chip;
	struct sbs_platform_data *pdata = client->dev.platform_data;
	int rc;
	int irq;
	char *name;

	name = kasprintf(GFP_KERNEL, "sbs-%s", dev_name(&client->dev));
	if (!name) {
		dev_err(&client->dev, "Failed to allocate device name\n");
		return -ENOMEM;
	}

	chip = kzalloc(sizeof(struct sbs_info), GFP_KERNEL);
	if (!chip) {
		rc = -ENOMEM;
		goto exit_free_name;
	}

	chip->client = client;
	tclient = client;
	chip->enable_detection = false;
	chip->gpio_detect = false;
	chip->power_supply.name = name;
	chip->power_supply.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->power_supply.properties = sbs_properties;
	chip->power_supply.num_properties = ARRAY_SIZE(sbs_properties);
	chip->power_supply.get_property = sbs_get_property;
	/* ignore first notification of external change, it is generated
	 * from the power_supply_register call back
	 */
	chip->ignore_changes = 1;
	chip->last_state = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->power_supply.external_power_changed = sbs_external_power_changed;

	tchip = chip;

	pdata = sbs_of_populate_pdata(client);
	if (pdata) {
		chip->gpio_detect = gpio_is_valid(pdata->battery_detect);
		memcpy(&chip->plat_data, pdata, sizeof(struct sbs_platform_data));
	}
	chip->poll_time = chip->plat_data.poll_retry_count;

	i2c_set_clientdata(client, chip);

	/* Probing for the presence of the sbs */

	mutex_init(&chip->mutex);

	rc = sbs_battery_detect();
	if (rc < 0) {
		dev_err(&client->dev,
			"%s: not responding\n", __func__);
		rc = -ENODEV;
		battery_detect = 0;
		goto exit_mem_free;
	}

	if (!chip->gpio_detect)
		goto skip_gpio;

	rc = gpio_request(pdata->battery_detect, dev_name(&client->dev));
	if (rc) {
		dev_warn(&client->dev, "Failed to request gpio: %d\n", rc);
		chip->gpio_detect = false;
		goto skip_gpio;
	}

	rc = gpio_direction_input(pdata->battery_detect);
	if (rc) {
		dev_warn(&client->dev, "Failed to get gpio as input: %d\n", rc);
		gpio_free(pdata->battery_detect);
		chip->gpio_detect = false;
		goto skip_gpio;
	}

	irq = gpio_to_irq(pdata->battery_detect);
	if (irq <= 0) {
		dev_warn(&client->dev, "Failed to get gpio as irq: %d\n", irq);
		gpio_free(pdata->battery_detect);
		chip->gpio_detect = false;
		goto skip_gpio;
	}

	rc = request_irq(irq, sbs_irq,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		dev_name(&client->dev), &chip->power_supply);
	if (rc) {
		dev_warn(&client->dev, "Failed to request irq: %d\n", rc);
		gpio_free(pdata->battery_detect);
		chip->gpio_detect = false;
		goto skip_gpio;
	}

	chip->irq = irq;

	chip->shutdown_complete = 0;

skip_gpio:

	sbs_update_health_properties(chip);
	sbs_update_capacity_properties(chip);
	sbs_update_serial_number_property(chip);
	sbs_update_battery_properties(chip);

	rc = power_supply_register(&client->dev, &chip->power_supply);
	if (rc) {
		dev_err(&client->dev,
			"%s: Failed to register power supply\n", __func__);
		goto exit_psupply;
	}

	dev_info(&client->dev,
		"%s: battery gas gauge device registered\n", client->name);

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, sbs_delayed_work);
	schedule_delayed_work(&chip->work, HZ);

	chip->enable_detection = true;

	return 0;

exit_psupply:
	if (chip->irq)
		free_irq(chip->irq, &chip->power_supply);
	if (chip->gpio_detect)
		gpio_free(pdata->battery_detect);

exit_mem_free:
	mutex_destroy(&chip->mutex);
	kfree(chip);

exit_free_name:
	kfree(name);

	return rc;
}

static int __devexit sbs_remove(struct i2c_client *client)
{
	struct sbs_info *chip = i2c_get_clientdata(client);

	if (chip->irq)
		free_irq(chip->irq, &chip->power_supply);
	if (chip->gpio_detect)
		gpio_free(chip->plat_data.battery_detect);

	power_supply_unregister(&chip->power_supply);

	cancel_delayed_work_sync(&chip->work);

	mutex_destroy(&chip->mutex);

	kfree(chip->power_supply.name);
	kfree(chip);
	chip = NULL;

	return 0;
}

static void sbs_shutdown(struct i2c_client *client)
{
	struct sbs_info *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mutex);
	if (chip->irq)
		disable_irq(chip->irq);

	if (chip->gpio_detect)
		gpio_free(chip->plat_data.battery_detect);

	cancel_delayed_work_sync(&chip->work);
	chip->shutdown_complete = 1;

	mutex_unlock(&chip->mutex);
}

#if defined CONFIG_PM
static int sbs_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct sbs_info *chip = i2c_get_clientdata(i2c);
	s32 ret;

	cancel_delayed_work_sync(&chip->work);

	/* write to manufacturer access with sleep command */
	ret = sbs_write_word_data(chip->client,
		sbs_data[REG_MANUFACTURER_DATA].addr,
		MANUFACTURER_ACCESS_SLEEP);
	if (chip->battery_present && ret < 0)
		return ret;

	return 0;
}

static int sbs_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct sbs_info *chip = i2c_get_clientdata(i2c);

	schedule_delayed_work(&chip->work, HZ);
	return 0;
}
#else
#define sbs_suspend		NULL
#define sbs_resume		NULL
#endif

static const struct i2c_device_id sbs_id[] = {
	{ "bq20z75", 0 },
	{ "sbs-battery", 1 },
	{}
};
MODULE_DEVICE_TABLE(i2c, sbs_id);

static const struct dev_pm_ops sbs_battery_pm_ops = {
	.suspend = sbs_suspend,
	.resume = sbs_resume,
};

static struct i2c_driver sbs_battery_driver = {
	.probe		= sbs_probe,
	.remove		= __devexit_p(sbs_remove),
	.id_table	= sbs_id,
	.shutdown	= sbs_shutdown,
	.driver = {
		.name	= "sbs-battery",
		.of_match_table = sbs_dt_ids,
		.pm = &sbs_battery_pm_ops,
	},
};
module_i2c_driver(sbs_battery_driver);

MODULE_DESCRIPTION("SBS battery monitor driver");
MODULE_LICENSE("GPL");
