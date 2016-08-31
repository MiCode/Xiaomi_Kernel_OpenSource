/*
 * drivers/power/tps80031_battery_gauge.c
 *
 * Gas Gauge driver for TI's tps80031
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/err.h>
#include <linux/regulator/machine.h>
#include <linux/mutex.h>

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <linux/mfd/core.h>
#include <linux/mfd/tps80031.h>
#include <linux/tps80031-charger.h>

#define CHARGERUSB_CINLIMIT	0xee
#define CONTROLLER_STAT1	0xe3
#define LINEAR_CHARGE_STS	0xde
#define STS_HW_CONDITIONS	0x21
#define TOGGLE1			0x90
#define TOGGLE1_FGS		BIT(5)
#define TOGGLE1_GPADCR		BIT(1)
#define GPCH0_LSB		0x3b
#define GPCH0_MSB		0x3c
#define GPCH0_MSB_COLLISION_GP	BIT(4)
#define GPSELECT_ISB		0x35
#define GPADC_CTRL		0x2e
#define MISC1			0xe4
#define CTRL_P1			0x36
#define CTRL_P1_SP1		BIT(3)
#define CTRL_P1_EOCRT		BIT(2)
#define CTRL_P1_EOCP1		BIT(1)
#define CTRL_P1_BUSY		BIT(0)
#define FG_REG_00		0xc0
#define FG_REG_00_CC_CAL_EN	BIT(1)
#define FG_REG_00_CC_AUTOCLEAR	BIT(2)
#define FG_REG_01		0xc1	/* CONV_NR (unsigned) 0 - 7 */
#define FG_REG_02		0xc2	/* CONV_NR (unsigned) 8 - 15 */
#define FG_REG_03		0xc3	/* CONV_NR (unsigned) 16 - 23 */
#define FG_REG_04		0xc4	/* ACCM	(signed) 0 - 7 */
#define FG_REG_05		0xc5	/* ACCM	(signed) 8 - 15 */
#define FG_REG_06		0xc6	/* ACCM	(signed) 16 - 23 */
#define FG_REG_07		0xc7	/* ACCM	(signed) 24 - 31 */
#define FG_REG_08		0xc8	/* OFFSET (signed) 0 - 7 */
#define FG_REG_09		0xc9	/* OFFSET (signed) 8 - 9 */
#define FG_REG_10		0xca	/* LAST_READ (signed) 0 - 7 */
#define FG_REG_11		0xcb	/* LAST_READ (signed) 8 - 13 */

#define TPS80031_VBUS_DET	BIT(2)
#define TPS80031_VAC_DET	BIT(3)
#define TPS80031_STS_VYSMIN_HI	BIT(4)
#define END_OF_CHARGE		BIT(5)

#define DRIVER_VERSION		"1.1.0"
#define BATTERY_POLL_PERIOD	30000

static int tps80031_temp_table[] = {
	/* adc code for temperature in degree C */
	929, 925, /* -2, -1 */
	920, 917, 912, 908, 904, 899, 895, 890, 885, 880, /* 00 - 09 */
	875, 869, 864, 858, 853, 847, 841, 835, 829, 823, /* 10 - 19 */
	816, 810, 804, 797, 790, 783, 776, 769, 762, 755, /* 20 - 29 */
	748, 740, 732, 725, 718, 710, 703, 695, 687, 679, /* 30 - 39 */
	671, 663, 655, 647, 639, 631, 623, 615, 607, 599, /* 40 - 49 */
	591, 583, 575, 567, 559, 551, 543, 535, 527, 519, /* 50 - 59 */
	511, 504, 496 /* 60 - 62 */
};

struct tps80031_device_info {
	struct device		*dev;
	struct i2c_client	*client;
	struct power_supply	bat;
	struct power_supply	ac;
	struct power_supply	usb;
	struct timer_list	battery_poll_timer;
	uint32_t vsys;
	uint8_t usb_online;
	uint8_t ac_online;
	uint8_t usb_status;
	uint8_t capacity_sts;
	uint8_t health;
	uint8_t sys_vlow_intr;
	uint8_t fg_calib_intr;
	int16_t fg_offset;
	struct mutex adc_lock;
};

static enum power_supply_property tps80031_bat_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};

static enum power_supply_property tps80031_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property tps80031_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int tps80031_reg_read(struct tps80031_device_info *di, int sid, int reg,
					uint8_t *val)
{
	int ret;

	ret = tps80031_read(di->dev->parent, sid, reg, val);
	if (ret < 0)
		dev_err(di->dev, "Failed read register 0x%02x\n",
					reg);
	return ret;
}

static int tps80031_reg_write(struct tps80031_device_info *di, int sid, int reg,
					uint8_t val)
{
	int ret;

	ret = tps80031_write(di->dev->parent, sid, reg, val);
	if (ret < 0)
		dev_err(di->dev, "Failed write register 0x%02x\n",
					reg);
	return ret;
}

static int tps80031_battery_capacity(struct tps80031_device_info *di,
			union power_supply_propval *val)
{
	uint8_t hwsts;
	int ret;

	ret = tps80031_reg_read(di, TPS80031_SLAVE_ID2, LINEAR_CHARGE_STS, &hwsts);
	if (ret < 0)
		return ret;

	di->capacity_sts = di->vsys;
	if (hwsts & END_OF_CHARGE)
		di->capacity_sts = 100;

	if (di->sys_vlow_intr) {
		di->capacity_sts = 10;
		di->sys_vlow_intr = 0;
	}

	if (di->capacity_sts <= 10)
		di->health = POWER_SUPPLY_HEALTH_DEAD;
	else
		di->health = POWER_SUPPLY_HEALTH_GOOD;

	return  di->capacity_sts;
}

static int tps80031_battery_voltage(struct tps80031_device_info *di,
			union power_supply_propval *val)
{
	int voltage;

	voltage = tps80031_gpadc_conversion(SYSTEM_SUPPLY);
	if (voltage < 0)
		return voltage;
	voltage = ((voltage * 1000) / 4) * 5;

	if (voltage < 3700000)
		di->vsys = 10;
	else if (voltage > 3700000 && voltage <= 3800000)
		di->vsys = 20;
	else if (voltage > 3800000 && voltage <= 3900000)
		di->vsys = 50;
	else if (voltage > 3900000 && voltage <= 4000000)
		di->vsys = 75;
	else if (voltage >= 4000000)
		di->vsys = 90;

	return voltage;
}

static int tps80031_battery_charge_now(struct tps80031_device_info *di,
			union power_supply_propval *val)
{
	int charge;

	charge = tps80031_gpadc_conversion(BATTERY_CHARGING_CURRENT);
	if (charge < 0)
		return charge;
	charge = charge * 78125 / 40;

	return charge;
}

static int tps80031_battery_charge_counter(struct tps80031_device_info *di,
			union power_supply_propval *val)
{
	int retval, ret;
	uint32_t cnt_byte;
	uint32_t acc_byte;

	/* check if calibrated */
	if (di->fg_calib_intr == 0)
		return 0;

	/* get current accumlator */
	ret = tps80031_reads(di->dev->parent, TPS80031_SLAVE_ID2, FG_REG_04, 4,
							(uint8_t *) &acc_byte);
	if (ret < 0)
		return ret;
	/* counter value is mAs, need report uAh */
	retval = (int32_t) acc_byte / 18 * 5;

	/* get counter */
	ret = tps80031_reads(di->dev->parent, TPS80031_SLAVE_ID2, FG_REG_01, 3,
							(uint8_t *) &cnt_byte);
	if (ret < 0)
		return ret;
	/* we need calibrate the offset current in uAh*/
	retval = retval - (di->fg_offset / 4 * cnt_byte);

	/* @todo, counter value will overflow if battery get continuously
	 * charged discharged for more than 108Ah using 250mS integration
	 * period althrough it is hightly impossible.
	 */

	return retval;
}

static int tps80031_battery_temp(struct tps80031_device_info *di,
					union power_supply_propval *val)
{
	int adc_code, temp;

	adc_code = tps80031_gpadc_conversion(BATTERY_TEMPERATURE);
	if (adc_code < 0)
		return adc_code;

	for (temp = 0; temp < ARRAY_SIZE(tps80031_temp_table); temp++) {
		if (adc_code >= tps80031_temp_table[temp])
			break;
	}
	/* first 2 values are for negative temperature */
	val->intval = (temp - 2) * 10; /* in tenths of degree Celsius */

	return  val->intval;
}

#define to_tps80031_device_info_bat(x) container_of((x), \
				struct tps80031_device_info, bat);

static int tps80031_bat_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct tps80031_device_info *di = to_tps80031_device_info_bat(psy);

	switch (psp) {

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->health;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval =  tps80031_battery_capacity(di, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval =  tps80031_battery_charge_now(di, val);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval =  tps80031_battery_voltage(di, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval =  tps80031_battery_charge_counter(di, val);
		break;

	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->usb_status;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = tps80031_battery_temp(di, val);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

#define to_tps80031_device_info_usb(x) container_of((x), \
				struct tps80031_device_info, usb);

static int tps80031_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct tps80031_device_info *di = to_tps80031_device_info_usb(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->usb_online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define to_tps80031_device_info_ac(x) container_of((x), \
				struct tps80031_device_info, ac);

static int tps80031_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct tps80031_device_info *di = to_tps80031_device_info_ac(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->ac_online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static irqreturn_t tps80031_sys_vlow(int irq, void *data)
{
	struct tps80031_device_info *di = data;

	di->sys_vlow_intr = 1;
	power_supply_changed(&di->bat);
	return IRQ_HANDLED;
}

static irqreturn_t tps80031_fg_calibrated(int irq, void *data)
{
	struct tps80031_device_info *di = data;
	uint8_t acc_byte0;
	uint8_t acc_byte1;
	int ret;

	ret = tps80031_reg_read(di, TPS80031_SLAVE_ID2, FG_REG_08, &acc_byte0);
	if (ret < 0)
		return IRQ_HANDLED;
	ret = tps80031_reg_read(di, TPS80031_SLAVE_ID2, FG_REG_09, &acc_byte1);
	if (ret < 0)
		return IRQ_HANDLED;
	/* sign extension */
	if (acc_byte1 & 0x02)
		acc_byte1 = acc_byte1 | 0xFC;
	else
		acc_byte1 = acc_byte1 & 0x03;

	di->fg_offset = (int16_t) ((acc_byte1 << 8) | acc_byte0);
	/* fuel gauge auto calibration finished */
	di->fg_calib_intr = 1;
	return IRQ_HANDLED;
}

static int tps80031_fg_start_gas_gauge(struct tps80031_device_info *di)
{
	int ret = 0;
	di->fg_calib_intr = 0;

	/* start gas gauge */
	ret = tps80031_reg_write(di, TPS80031_SLAVE_ID2, TOGGLE1, 0x20);
	if (ret < 0)
		return ret;
	/* set ADC update time to 3.9ms and start calibration */
	ret = tps80031_reg_write(di, TPS80031_SLAVE_ID2, FG_REG_00, FG_REG_00_CC_CAL_EN);
	if (ret < 0)
		return ret;
	return ret;
}

void tps80031_battery_status(enum charging_states status, void *data)
{
	struct tps80031_device_info *di = data;
	int ret;
	uint8_t retval;

	if ((status == charging_state_charging_in_progress)) {
		di->usb_status	= POWER_SUPPLY_STATUS_CHARGING;
		di->health	= POWER_SUPPLY_HEALTH_GOOD;
		ret = tps80031_reg_read(di, TPS80031_SLAVE_ID2,
				CHARGERUSB_CINLIMIT, &retval);
		if (ret < 0) {
			di->ac_online = 0;
			di->usb_online = 0;
		}
		if (retval == 0x9) {
			di->ac_online = 0;
			di->usb_online = 1;
		} else {
			di->usb_online = 0;
			di->ac_online = 1;
		}
	} else if (status == charging_state_charging_stopped) {
		di->usb_status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->ac_online = 0;
		di->usb_online = 0;
	} else if (status == charging_state_charging_completed) {
		di->usb_status = POWER_SUPPLY_STATUS_FULL;
	}
	power_supply_changed(&di->usb);
	power_supply_changed(&di->bat);
	power_supply_changed(&di->ac);
}

static void battery_poll_timer_func(unsigned long pdi)
{
	struct tps80031_device_info *di = (void *)pdi;
	power_supply_changed(&di->bat);
	mod_timer(&di->battery_poll_timer,
		jiffies + msecs_to_jiffies(BATTERY_POLL_PERIOD));
}

static int tps80031_battery_probe(struct platform_device *pdev)
{
	int ret;
	uint8_t retval;
	struct device *dev = &pdev->dev;
	struct tps80031_device_info *di;
	struct tps80031_platform_data *tps80031_pdata;
	struct tps80031_bg_platform_data *pdata;

	tps80031_pdata = dev_get_platdata(pdev->dev.parent);
	if (!tps80031_pdata) {
		dev_err(&pdev->dev, "no tps80031 platform_data specified\n");
		return -EINVAL;
	}

	pdata = tps80031_pdata->bg_pdata;
	if (!pdata) {
		dev_err(&pdev->dev, "no battery_gauge platform data\n");
		return -EINVAL;
	}

	di = devm_kzalloc(&pdev->dev, sizeof *di, GFP_KERNEL);
	if (!di) {
		dev_err(dev->parent, "failed to allocate device info data\n");
		return -ENOMEM;
	}

	if (!pdata->battery_present) {
		dev_err(dev, "%s() No battery detected, exiting..\n",
				__func__);
		return -ENODEV;
	}

	di->dev =  &pdev->dev;

	ret = tps80031_reg_read(di, TPS80031_SLAVE_ID2, CONTROLLER_STAT1, &retval);
	if (ret < 0)
		return ret;

	if ((retval & TPS80031_VAC_DET) | (retval & TPS80031_VBUS_DET)) {
		di->usb_status = POWER_SUPPLY_STATUS_CHARGING;
		di->usb_online = 1;
	} else {
		di->usb_status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->usb_online = 0;
	}

	di->capacity_sts = 50;
	di->health = POWER_SUPPLY_HEALTH_GOOD;

	di->bat.name		= "tps80031-bat";
	di->bat.type		= POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties	= tps80031_bat_props;
	di->bat.num_properties	= ARRAY_SIZE(tps80031_bat_props);
	di->bat.get_property	= tps80031_bat_get_property;

	ret = power_supply_register(dev->parent, &di->bat);
	if (ret) {
		dev_err(dev->parent, "failed to register bat power supply\n");
		return ret;
	}

	di->usb.name		= "tps80031-usb";
	di->usb.type		= POWER_SUPPLY_TYPE_USB;
	di->usb.properties	= tps80031_usb_props;
	di->usb.num_properties	= ARRAY_SIZE(tps80031_usb_props);
	di->usb.get_property	= tps80031_usb_get_property;

	ret = power_supply_register(dev->parent, &di->usb);
	if (ret) {
		dev_err(dev->parent, "failed to register ac power supply\n");
		goto power_supply_fail2;
	}

	di->ac.name		= "tps80031-ac";
	di->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	di->ac.properties	= tps80031_ac_props;
	di->ac.num_properties	= ARRAY_SIZE(tps80031_ac_props);
	di->ac.get_property	= tps80031_ac_get_property;

	ret = power_supply_register(dev->parent, &di->ac);
	if (ret) {
		dev_err(dev->parent, "failed to register ac power supply\n");
		goto power_supply_fail1;
	}

	dev_set_drvdata(&pdev->dev, di);

	ret = register_charging_state_callback(tps80031_battery_status, di);
	if (ret < 0)
		goto power_supply_fail0;

	ret = request_threaded_irq(pdata->irq_base + TPS80031_INT_SYS_VLOW,
				NULL, tps80031_sys_vlow,
					IRQF_ONESHOT, "tps80031_sys_vlow", di);
	if (ret < 0) {
		dev_err(dev->parent, "request IRQ %d fail\n", pdata->irq_base);
		goto power_supply_fail0;
	}

	ret = request_threaded_irq(pdata->irq_base + TPS80031_INT_CC_AUTOCAL,
				NULL, tps80031_fg_calibrated, IRQF_ONESHOT,
				"tps80031_fuel_gauge_calibration", di);
	if (ret < 0) {
		dev_err(dev->parent, "request IRQ %d fail\n", pdata->irq_base);
		goto irq_fail2;
	}
	setup_timer(&di->battery_poll_timer,
		battery_poll_timer_func, (unsigned long) di);
	mod_timer(&di->battery_poll_timer,
		jiffies + msecs_to_jiffies(BATTERY_POLL_PERIOD));

	ret = tps80031_fg_start_gas_gauge(di);
	if (ret < 0) {
		dev_err(dev->parent, "failed to start fuel-gauge\n");
		goto irq_fail1;
	}
	dev_info(dev->parent, "support ver. %s enabled\n", DRIVER_VERSION);

	return ret;

irq_fail1:
	free_irq(pdata->irq_base + TPS80031_INT_CC_AUTOCAL, di);
irq_fail2:
	free_irq(pdata->irq_base + TPS80031_INT_SYS_VLOW, di);
power_supply_fail0:
	power_supply_unregister(&di->ac);
power_supply_fail1:
	power_supply_unregister(&di->usb);
power_supply_fail2:
	power_supply_unregister(&di->bat);
	return ret;
}

static int tps80031_battery_remove(struct platform_device *pdev)
{
	struct tps80031_device_info *di = dev_get_drvdata(&pdev->dev);

	power_supply_unregister(&di->bat);
	power_supply_unregister(&di->usb);
	power_supply_unregister(&di->ac);

	return 0;
}

static struct platform_driver tps80031_battery_driver = {
	.driver	= {
		.name	= "tps80031-battery-gauge",
		.owner	= THIS_MODULE,
	},
	.probe	= tps80031_battery_probe,
	.remove = tps80031_battery_remove,
};

static int __init tps80031_battery_init(void)
{
	return platform_driver_register(&tps80031_battery_driver);
}

static void __exit tps80031_battery_exit(void)
{
	platform_driver_unregister(&tps80031_battery_driver);
}

module_init(tps80031_battery_init);
module_exit(tps80031_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com> ");
MODULE_DESCRIPTION("tps80031 battery gauge driver");
