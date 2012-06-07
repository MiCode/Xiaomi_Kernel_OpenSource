/* Quanta I2C Battery Driver
 *
 * Copyright (C) 2009 Quanta Computer Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 *
 *  The Driver with I/O communications via the I2C Interface for ST15 platform.
 *  And it is only working on the nuvoTon WPCE775x Embedded Controller.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/wpce775x.h>
#include <linux/delay.h>

#include "qci_battery.h"

#define QCIBAT_DEFAULT_CHARGE_FULL_CAPACITY 2200 /* 2200 mAh */
#define QCIBAT_DEFAULT_CHARGE_FULL_DESIGN   2200
#define QCIBAT_DEFAULT_VOLTAGE_DESIGN      10800 /* 10.8 V */
#define QCIBAT_STRING_SIZE 16

/* General structure to hold the driver data */
struct i2cbat_drv_data {
	struct i2c_client *bi2c_client;
	struct work_struct work;
	unsigned int qcibat_irq;
	unsigned int qcibat_gpio;
	u8 battery_state;
	u8 battery_dev_name[QCIBAT_STRING_SIZE];
	u8 serial_number[QCIBAT_STRING_SIZE];
	u8 manufacturer_name[QCIBAT_STRING_SIZE];
	unsigned int charge_full;
	unsigned int charge_full_design;
	unsigned int voltage_full_design;
	unsigned int energy_full;
};

static struct i2cbat_drv_data context;
static struct mutex qci_i2c_lock;
static struct mutex qci_transaction_lock;
/*********************************************************************
 *		Power
 *********************************************************************/

static int get_bat_info(u8 ec_data)
{
	u8 byte_read;

	mutex_lock(&qci_i2c_lock);
	i2c_smbus_write_byte(context.bi2c_client, ec_data);
	byte_read = i2c_smbus_read_byte(context.bi2c_client);
	mutex_unlock(&qci_i2c_lock);
	return byte_read;
}

static int qci_ac_get_prop(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (get_bat_info(ECRAM_POWER_SOURCE) & EC_FLAG_ADAPTER_IN)
			val->intval =  EC_ADAPTER_PRESENT;
		else
			val->intval =  EC_ADAPTER_NOT_PRESENT;
	break;
	default:
		ret = -EINVAL;
	break;
	}
	return ret;
}

static enum power_supply_property qci_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property qci_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
};

static int read_data_from_battery(u8 smb_cmd, u8 smb_prtcl)
{
	if (context.battery_state & MAIN_BATTERY_STATUS_BAT_IN)	{
		mutex_lock(&qci_i2c_lock);
		i2c_smbus_write_byte_data(context.bi2c_client,
					  ECRAM_SMB_STS, 0);
		i2c_smbus_write_byte_data(context.bi2c_client, ECRAM_SMB_ADDR,
					  BATTERY_SLAVE_ADDRESS);
		i2c_smbus_write_byte_data(context.bi2c_client,
					  ECRAM_SMB_CMD, smb_cmd);
		i2c_smbus_write_byte_data(context.bi2c_client,
					  ECRAM_SMB_PRTCL, smb_prtcl);
		mutex_unlock(&qci_i2c_lock);
		msleep(100);
		return get_bat_info(ECRAM_SMB_STS);
	} else
		return SMBUS_DEVICE_NOACK;
}

static int qbat_get_status(union power_supply_propval *val)
{
	int status;

	status = get_bat_info(ECRAM_BATTERY_STATUS);

	if ((status & MAIN_BATTERY_STATUS_BAT_IN) == 0x0)
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
	else if (status & MAIN_BATTERY_STATUS_BAT_CHARGING)
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
	else if (status & MAIN_BATTERY_STATUS_BAT_FULL)
		val->intval = POWER_SUPPLY_STATUS_FULL;
	else if (status & MAIN_BATTERY_STATUS_BAT_DISCHRG)
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	else
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return 0;
}

static int qbat_get_present(union power_supply_propval *val)
{
	if (context.battery_state & MAIN_BATTERY_STATUS_BAT_IN)
		val->intval = EC_BAT_PRESENT;
	else
		val->intval = EC_BAT_NOT_PRESENT;
	return 0;
}

static int qbat_get_health(union power_supply_propval *val)
{
	u8 health;

	health = get_bat_info(ECRAM_CHARGER_ALARM);
	if (!(context.battery_state & MAIN_BATTERY_STATUS_BAT_IN))
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	else if (health & ALARM_OVER_TEMP)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (health & ALARM_REMAIN_CAPACITY)
		val->intval = POWER_SUPPLY_HEALTH_DEAD;
	else if (health & ALARM_OVER_CHARGE)
		val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	return 0;
}

static int qbat_get_voltage_avg(union power_supply_propval *val)
{
	val->intval = (get_bat_info(ECRAM_BATTERY_VOLTAGE_MSB) << 8 |
		       get_bat_info(ECRAM_BATTERY_VOLTAGE_LSB)) * 1000;
	return 0;
}

static int qbat_get_current_avg(union power_supply_propval *val)
{
	val->intval = (get_bat_info(ECRAM_BATTERY_CURRENT_MSB) << 8 |
		       get_bat_info(ECRAM_BATTERY_CURRENT_LSB));
	return 0;
}

static int qbat_get_capacity(union power_supply_propval *val)
{
	if (!(context.battery_state & MAIN_BATTERY_STATUS_BAT_IN))
		val->intval = 0xFF;
	else
		val->intval = get_bat_info(ECRAM_BATTERY_CAPACITY);
	return 0;
}

static int qbat_get_temp_avg(union power_supply_propval *val)
{
	int temp;
	int rc = 0;

	if (!(context.battery_state & MAIN_BATTERY_STATUS_BAT_IN)) {
		val->intval = 0xFFFF;
		rc = -ENODATA;
	} else {
		temp = (get_bat_info(ECRAM_BATTERY_TEMP_MSB) << 8) |
			get_bat_info(ECRAM_BATTERY_TEMP_LSB);
		val->intval = (temp - 2730) / 10;
	}
	return rc;
}

static int qbat_get_charge_full_design(union power_supply_propval *val)
{
	val->intval = context.charge_full_design;
	return 0;
}

static int qbat_get_charge_full(union power_supply_propval *val)
{
	val->intval = context.charge_full;
	return 0;
}

static int qbat_get_charge_counter(union power_supply_propval *val)
{
	u16 charge = 0;
	int rc = 0;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_CYCLE_COUNT,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		charge = get_bat_info(ECRAM_SMB_DATA1);
		charge = charge << 8;
		charge |= get_bat_info(ECRAM_SMB_DATA0);
	} else
		rc = -ENODATA;
	mutex_unlock(&qci_transaction_lock);
	val->intval = charge;
	return rc;
}

static int qbat_get_time_empty_avg(union power_supply_propval *val)
{
	u16 avg = 0;
	int rc = 0;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_AVERAGE_TIME_TO_EMPTY,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		avg = get_bat_info(ECRAM_SMB_DATA1);
		avg = avg << 8;
		avg |= get_bat_info(ECRAM_SMB_DATA0);
	} else
		rc = -ENODATA;
	mutex_unlock(&qci_transaction_lock);
	val->intval = avg;
	return rc;
}

static int qbat_get_time_full_avg(union power_supply_propval *val)
{
	u16 avg = 0;
	int rc = 0;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_AVERAGE_TIME_TO_FULL,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		avg = get_bat_info(ECRAM_SMB_DATA1);
		avg = avg << 8;
		avg |= get_bat_info(ECRAM_SMB_DATA0);
	} else
		rc = -ENODATA;
	mutex_unlock(&qci_transaction_lock);
	val->intval = avg;
	return rc;
}

static int qbat_get_model_name(union power_supply_propval *val)
{
	unsigned char i, size;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_DEVICE_NAME,
				   SMBUS_READ_BLOCK_PRTCL) == SMBUS_DONE) {
		size = min(get_bat_info(ECRAM_SMB_BCNT), QCIBAT_STRING_SIZE);
		for (i = 0; i < size; i++) {
			context.battery_dev_name[i] =
				get_bat_info(ECRAM_SMB_DATA_START + i);
		}
		val->strval = context.battery_dev_name;
	} else
		val->strval = "Unknown";
	mutex_unlock(&qci_transaction_lock);
	return 0;
}

static int qbat_get_manufacturer_name(union power_supply_propval *val)
{
	val->strval = context.manufacturer_name;
	return 0;
}

static int qbat_get_serial_number(union power_supply_propval *val)
{
	val->strval = context.serial_number;
	return 0;
}

static int qbat_get_technology(union power_supply_propval *val)
{
	val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	return 0;
}

static int qbat_get_energy_now(union power_supply_propval *val)
{
	if (!(get_bat_info(ECRAM_BATTERY_STATUS) & MAIN_BATTERY_STATUS_BAT_IN))
		val->intval = 0;
	else
		val->intval = (get_bat_info(ECRAM_BATTERY_CAPACITY) *
			       context.energy_full) / 100;
	return 0;
}

static int qbat_get_energy_full(union power_supply_propval *val)
{
	val->intval = context.energy_full;
	return 0;
}

static int qbat_get_energy_empty(union power_supply_propval *val)
{
	val->intval = 0;
	return 0;
}

static void qbat_init_get_charge_full(void)
{
	u16 charge = QCIBAT_DEFAULT_CHARGE_FULL_CAPACITY;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_FULL_CAPACITY,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		charge = get_bat_info(ECRAM_SMB_DATA1);
		charge = charge << 8;
		charge |= get_bat_info(ECRAM_SMB_DATA0);
	}
	mutex_unlock(&qci_transaction_lock);
	context.charge_full = charge;
}

static void qbat_init_get_charge_full_design(void)
{
	u16 charge = QCIBAT_DEFAULT_CHARGE_FULL_DESIGN;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_DESIGN_CAPACITY,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		charge = get_bat_info(ECRAM_SMB_DATA1);
		charge = charge << 8;
		charge |= get_bat_info(ECRAM_SMB_DATA0);
	}
	mutex_unlock(&qci_transaction_lock);
	context.charge_full_design = charge;
}

static void qbat_init_get_voltage_full_design(void)
{
	u16 voltage = QCIBAT_DEFAULT_VOLTAGE_DESIGN;

	mutex_lock(&qci_transaction_lock);
	if (read_data_from_battery(BATTERY_DESIGN_VOLTAGE,
				   SMBUS_READ_WORD_PRTCL) == SMBUS_DONE) {
		voltage = get_bat_info(ECRAM_SMB_DATA1);
		voltage = voltage << 8;
		voltage |= get_bat_info(ECRAM_SMB_DATA0);
	}
	mutex_unlock(&qci_transaction_lock);
	context.voltage_full_design = voltage;
}

static void qbat_init_get_manufacturer_name(void)
{
	u8 size;
	u8 i;
	int rc;

	mutex_lock(&qci_transaction_lock);
	rc = read_data_from_battery(BATTERY_MANUFACTURE_NAME,
				    SMBUS_READ_BLOCK_PRTCL);
	if (rc == SMBUS_DONE) {
		size = min(get_bat_info(ECRAM_SMB_BCNT), QCIBAT_STRING_SIZE);
		for (i = 0; i < size; i++) {
			context.manufacturer_name[i] =
				get_bat_info(ECRAM_SMB_DATA_START + i);
		}
	} else
		strcpy(context.manufacturer_name, "Unknown");
	mutex_unlock(&qci_transaction_lock);
}

static void qbat_init_get_serial_number(void)
{
	u8 size;
	u8 i;
	int rc;

	mutex_lock(&qci_transaction_lock);
	rc = read_data_from_battery(BATTERY_SERIAL_NUMBER,
				    SMBUS_READ_BLOCK_PRTCL);
	if (rc == SMBUS_DONE) {
		size = min(get_bat_info(ECRAM_SMB_BCNT), QCIBAT_STRING_SIZE);
		for (i = 0; i < size; i++) {
			context.serial_number[i] =
				get_bat_info(ECRAM_SMB_DATA_START + i);
		}
	} else
		strcpy(context.serial_number, "Unknown");
	mutex_unlock(&qci_transaction_lock);
}

static void init_battery_stats(void)
{
	int i;

	context.battery_state = get_bat_info(ECRAM_BATTERY_STATUS);
	if (!(context.battery_state & MAIN_BATTERY_STATUS_BAT_IN))
		return;
	/* EC bug? needs some initial priming */
	for (i = 0; i < 5; i++) {
		read_data_from_battery(BATTERY_DESIGN_CAPACITY,
				       SMBUS_READ_WORD_PRTCL);
	}

	qbat_init_get_charge_full_design();
	qbat_init_get_charge_full();
	qbat_init_get_voltage_full_design();

	context.energy_full = context.voltage_full_design *
		context.charge_full;

	qbat_init_get_serial_number();
	qbat_init_get_manufacturer_name();
}

/*********************************************************************
 *		Battery properties
 *********************************************************************/
static int qbat_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = qbat_get_status(val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = qbat_get_present(val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = qbat_get_health(val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		ret = qbat_get_manufacturer_name(val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = qbat_get_technology(val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = qbat_get_voltage_avg(val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = qbat_get_current_avg(val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = qbat_get_capacity(val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = qbat_get_temp_avg(val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = qbat_get_charge_full_design(val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = qbat_get_charge_full(val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = qbat_get_charge_counter(val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = qbat_get_time_empty_avg(val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		ret = qbat_get_time_full_avg(val);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		ret = qbat_get_model_name(val);
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		ret = qbat_get_serial_number(val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = qbat_get_energy_now(val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		ret = qbat_get_energy_full(val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		ret = qbat_get_energy_empty(val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*********************************************************************
 *		Initialisation
 *********************************************************************/

static struct power_supply qci_ac = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = qci_ac_props,
	.num_properties = ARRAY_SIZE(qci_ac_props),
	.get_property = qci_ac_get_prop,
};

static struct power_supply qci_bat = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = qci_bat_props,
	.num_properties = ARRAY_SIZE(qci_bat_props),
	.get_property = qbat_get_property,
	.use_for_apm = 1,
};

static irqreturn_t qbat_interrupt(int irq, void *dev_id)
{
	struct i2cbat_drv_data *ibat_drv_data = dev_id;
	schedule_work(&ibat_drv_data->work);
	return IRQ_HANDLED;
}

static void qbat_work(struct work_struct *_work)
{
	u8 status;

	status = get_bat_info(ECRAM_BATTERY_EVENTS);
	if (status & EC_EVENT_AC) {
		context.battery_state = get_bat_info(ECRAM_BATTERY_STATUS);
		power_supply_changed(&qci_ac);
	}

	if (status & (EC_EVENT_BATTERY | EC_EVENT_CHARGER | EC_EVENT_TIMER)) {
		context.battery_state = get_bat_info(ECRAM_BATTERY_STATUS);
		power_supply_changed(&qci_bat);
		if (status & EC_EVENT_BATTERY)
			init_battery_stats();
	}
}

static struct platform_device *bat_pdev;

static int __init qbat_init(void)
{
	int err = 0;

	mutex_init(&qci_i2c_lock);
	mutex_init(&qci_transaction_lock);

	context.bi2c_client = wpce_get_i2c_client();
	if (context.bi2c_client == NULL)
		return -1;

	i2c_set_clientdata(context.bi2c_client, &context);
	context.qcibat_gpio = context.bi2c_client->irq;

	/*battery device register*/
	bat_pdev = platform_device_register_simple("battery", 0, NULL, 0);
	if (IS_ERR(bat_pdev))
		return PTR_ERR(bat_pdev);

	err = power_supply_register(&bat_pdev->dev, &qci_ac);
	if (err)
		goto ac_failed;

	qci_bat.name = bat_pdev->name;
	err = power_supply_register(&bat_pdev->dev, &qci_bat);
	if (err)
		goto battery_failed;

	/*battery irq configure*/
	INIT_WORK(&context.work, qbat_work);
	err = gpio_request(context.qcibat_gpio, "qci-bat");
	if (err) {
		dev_err(&context.bi2c_client->dev,
			"[BAT] err gpio request\n");
		goto gpio_request_fail;
	}
	context.qcibat_irq = gpio_to_irq(context.qcibat_gpio);
	err = request_irq(context.qcibat_irq, qbat_interrupt,
		IRQF_TRIGGER_FALLING, BATTERY_ID_NAME, &context);
	if (err) {
		dev_err(&context.bi2c_client->dev,
			"[BAT] unable to get IRQ\n");
		goto request_irq_fail;
	}

	init_battery_stats();
	goto success;

request_irq_fail:
	gpio_free(context.qcibat_gpio);

gpio_request_fail:
	power_supply_unregister(&qci_bat);

battery_failed:
	power_supply_unregister(&qci_ac);

ac_failed:
	platform_device_unregister(bat_pdev);

	i2c_set_clientdata(context.bi2c_client, NULL);
success:
	return err;
}

static void __exit qbat_exit(void)
{
	free_irq(context.qcibat_irq, &context);
	gpio_free(context.qcibat_gpio);
	power_supply_unregister(&qci_bat);
	power_supply_unregister(&qci_ac);
	platform_device_unregister(bat_pdev);
	i2c_set_clientdata(context.bi2c_client, NULL);
}

late_initcall(qbat_init);
module_exit(qbat_exit);

MODULE_AUTHOR("Quanta Computer Inc.");
MODULE_DESCRIPTION("Quanta Embedded Controller I2C Battery Driver");
MODULE_LICENSE("GPL v2");

