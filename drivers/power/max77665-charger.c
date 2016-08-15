/*
 * max77665-charger.c - Battery charger driver
 *
 *  Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *  Syed Rafiuddin <srafiuddin@nvidia.com>
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
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/alarmtimer.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/mfd/max77665.h>
#include <linux/max77665-charger.h>
#include <linux/power/max17042_battery.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

/* IDBEN register define for MUIC
 * it's better to put it in extcon-max77665.c
 * but since we don't use extcon-77665 drvier, so just define it here for simple */
#define MAX77665_MUIC_REG_CONTROL1		0x0c
#define CONTROL1_IDB_EN_MASK			0x80

#define CHARGER_TYPE_DETECTION_DEBOUNCE_TIME_MS 50
#define UNPLUG_CHECK_WAIT_PERIOD_MS	500
#define CHARGER_MONITOR_WAIT_PERIOD_MS	60000

/* The temperature range for charging termination */
#define MAX77665_LOW_TEMP_TERMINAL 0
#define MAX77665_HIGH_TEMP_TERMINAL 60
#define batt_temp_is_valid(val) \
(val > MAX77665_LOW_TEMP_TERMINAL && val < MAX77665_HIGH_TEMP_TERMINAL)
#define batt_temp_is_cool(val)	(val <= charger->plat_data->cool_temp)
#define batt_temp_is_warm(val)	(val >= charger->plat_data->warm_temp)
#define batt_temp_is_normal(val) \
(val > charger->plat_data->cool_temp && val < charger->plat_data->warm_temp)

/* 35 mV */
#define MAX77665_CHG_SOFT_RSTRT 35

#define USB_WALL_THRESHOLD_MA 500

/* fast charge current in mA */
static const uint32_t chg_cc[]  = {
	0, 33, 66, 99, 133, 166, 199, 233, 266, 299,
	333, 366, 399, 432, 466, 499, 532, 566, 599, 632,
	666, 699, 732, 765, 799, 832, 865, 899, 932, 965,
	999, 1032, 1065, 1098, 1132, 1165, 1198, 1232, 1265,
	1298, 1332, 1365, 1398, 1421, 1465, 1498, 1531, 1565,
	1598, 1631, 1665, 1698, 1731, 1764, 1798, 1831, 1864,
	1898, 1931, 1964, 1998, 2031, 2064, 2097
};

/* primary charge termination voltage in mV */
static const uint32_t chg_cv_prm[] = {
	3650, 3675, 3700, 3725, 3750,
	3775, 3800, 3825, 3850, 3875,
	3900, 3925, 3950, 3975, 4000,
	4025, 4050, 4075, 4100, 4125,
	4150, 4175, 4200, 4225, 4250,
	4275, 4300, 4325, 4340, 4350,
	4375, 4400
};

static int max77665_bat_to_sys_oc_thres[] = {
	0, 3000, 3250, 3500, 3750, 4000, 4250, 4500
};

struct max77665_charger {
	enum max77665_mode mode;
	struct device		*dev;
	int			irq;
	struct power_supply ac;
	struct power_supply usb;
	struct max77665_charger_plat_data *plat_data;
	struct mutex current_limit_mutex;
	int max_current_mA;
	uint8_t ac_online;
	uint8_t usb_online;
	uint8_t num_cables;
	struct extcon_dev *edev;
	struct extcon_dev *pmu_edev;
	struct alarm wdt_alarm;
	struct delayed_work wdt_ack_work;
	struct delayed_work unplug_check_work;
	struct delayed_work charger_monitor_work;
	struct delayed_work invalid_charger_check_work;
	struct wake_lock chg_wake_lock;
	struct wake_lock wdt_wake_lock;
	unsigned int oc_count;
	unsigned int fast_chg_cc;
	unsigned int term_volt;
};

static enum power_supply_property max77665_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

struct max77665_charger *the_charger;
static struct notifier_block charger_nb;
/* The maximum usb input allowed by usb connector is 1800mA */
static unsigned int usb_max_current = 1800;
static unsigned int usb_target_ma;
/* Charger control for factory test */
static int charging_disabled;
/* Indicate if the usb is charging */
static bool usb_cable_is_online;
/* Indicate if the usb is present */
static bool usb_cable_is_present;

static int max77665_write_reg(struct max77665_charger *charger,
	uint8_t reg, int value)
{
	int ret = 0;
	struct device *dev = charger->dev;

	if ((value < 0) || (value > 0xFF))
		return -EINVAL;

	ret = max77665_write(dev->parent, MAX77665_I2C_SLAVE_PMIC, reg, value);
	if (ret < 0)
		dev_err(dev, "Failed to write to reg 0x%x\n", reg);
	return ret;
}

static int max77665_read_reg(struct max77665_charger *charger,
	uint8_t reg, uint32_t *value)
{
	int ret = 0;
	uint8_t read = 0;
	struct device *dev = charger->dev;

	ret = max77665_read(dev->parent, MAX77665_I2C_SLAVE_PMIC, reg, &read);
	if (0 > ret)
		dev_err(dev, "Failed to read register 0x%x\n", reg);
	else
		*value = read;

	return ret;
}

static int max77665_update_reg(struct max77665_charger *charger,
	uint8_t reg, int value)
{
	int ret = 0;
	int read_val;

	ret = max77665_read_reg(charger, reg, &read_val);
	if (ret)
		return ret;

	ret = max77665_write_reg(charger, reg, read_val | value);
	return ret;
}

/* Convert current to register value using lookup table */
static int convert_to_reg(struct device *dev, char *tbl_name,
		const unsigned int *tbl, size_t size, unsigned int val)
{
	size_t i;

	if ((val < tbl[0]) || (val > tbl[size - 1])) {
		dev_err(dev, "%d is not in %s table\n", val,  tbl_name);
		return -EINVAL;
	}

	for (i = 0; i < size - 1; i++)
		if ((tbl[i] <= val) && (val < tbl[i + 1]))
			break;
	return i;
}
#define CONVERT_TO_REG(table, val)	\
	convert_to_reg(charger->dev, #table, table, ARRAY_SIZE(table), val)

int max77665_set_max_input_current(struct max77665_charger *charger, int mA)
{
	int ret;

	ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_09,
			mA / CURRENT_STEP_mA);
	if (ret < 0)
		dev_err(charger->dev, "failed to set %dmA charging\n", mA);
	return 0;
}

int max77665_get_max_input_current(struct max77665_charger *charger, int *mA)
{
	int ret;
	uint32_t val;

	ret = max77665_read_reg(charger, MAX77665_CHG_CNFG_09, &val);
	if (0 > ret)
		dev_err(charger->dev, "failed to get charging current\n");
	val &= 0x7F;
	*mA = max_t(int, MIN_CURRENT_LIMIT_mA, val * CURRENT_STEP_mA);
	return ret;
}

static int is_usb_chg_plugged_in(struct max77665_charger *charger)
{
	int ret;
	int status, dtls_00, dtls_01;

	ret = max77665_read_reg(charger, MAX77665_CHG_INT_OK, &status);
	if (ret < 0) {
		dev_err(charger->dev, "%s read status ret %d\n", __func__, ret);
		return false;
	}

	ret = max77665_read_reg(charger, MAX77665_CHG_DTLS_00, &dtls_00);
	if (ret < 0) {
		dev_err(charger->dev, "%s read dtls00 ret %d\n", __func__, ret);
		return false;
	}

	ret = max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &dtls_01);
	if (ret < 0) {
		dev_err(charger->dev, "%s read dtls01 ret %d\n", __func__, ret);
		return false;
	}

	if ((status & CHGIN_BIT) &&
	    (CHGIN_DTLS_MASK(dtls_00) == CHGIN_DTLS_VALID))
		return true;

	if (!(status & CHGIN_BIT) && charging_is_on(dtls_01))
		return true;

	return false;
}

static int max77665_charger_set_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 const union power_supply_propval *val)
{
	struct max77665_charger *chip;

	if (!strcmp(psy->name, "ac"))
		chip = container_of(psy, struct max77665_charger, ac);
	else
		chip = container_of(psy, struct max77665_charger, usb);

	if (psp == POWER_SUPPLY_PROP_CURRENT_MAX)
		/* passed value is uA */
		return max77665_set_max_input_current(chip, val->intval / 1000);

	return -EINVAL;
}

static int max77665_charger_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	int online;
	int ret;
	struct max77665_charger *charger;

	if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
		charger = container_of(psy, struct max77665_charger, ac);
		online = charger->ac_online;
	} else if (psy->type == POWER_SUPPLY_TYPE_USB) {
		charger = container_of(psy, struct max77665_charger, usb);
		online = charger->usb_online;
	} else {
		return -EINVAL;
	}

	ret = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = max77665_get_max_input_current(charger, &val->intval);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int max77665_charger_property_is_writeable(struct power_supply *psy,
					enum power_supply_property
					psp)
{
	if (psp == POWER_SUPPLY_PROP_CURRENT_MAX)
		return 1;
	return 0;
}

static int max77665_enable_write(struct max77665_charger *charger, bool access)
{
	int ret = 0;

	if (access)
		/* enable write acces to registers */
		ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_06, 0x0c);
	else
		/* Disable write acces to registers */
		ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_06, 0x00);

	if (ret < 0)
		dev_err(charger->dev, "failed to %s write acess\n",
				access ? "eanble" : "disable");
	return ret;
}

static bool max77665_check_charging_ok(struct max77665_charger *charger)
{
	uint32_t status, byp_dtls;
	int ret;

	/* check charging status is OK */
	ret = max77665_read_reg(charger, MAX77665_CHG_INT_OK, &status);
	if (ret < 0) {
		dev_err(charger->dev, "%s read %x error\n",
			__func__, MAX77665_CHG_INT_OK);
		return false;
	}

	/* check voltage regulation loop */
	ret = max77665_read_reg(charger, MAX77665_CHG_DTLS_02, &byp_dtls);
	if (ret < 0) {
		dev_err(charger->dev, "%s read %x error\n",
			__func__, MAX77665_CHG_DTLS_02);
		return false;
	}

	if (!(status & BYP_BIT) && (BYP_DTLS_MASK(byp_dtls) != BYP_DTLS_VALID))
		return false;

	return true;
}

static void max77665_display_charger_status(struct max77665_charger *charger,
		uint32_t status)
{
	int i;
	uint32_t val;
	bool ok;
	int bits[] = { BYP_BIT, DETBAT_BIT, BAT_BIT, CHG_BIT, CHGIN_BIT };
	char *info[] = {
		"bypass", "main battery presence", "battery",
		"charger", "charging input"
	};

	ok = true;
	for (i = 0; i < ARRAY_SIZE(bits); i++) {
		if (0 == (status & bits[i])) {
			ok = false;
			dev_dbg(charger->dev, "%s is not OK\n", info[i]);
		}
	}

	if (ok == false) {
		max77665_read_reg(charger, MAX77665_CHG_DTLS_00, &val);
		dev_info(charger->dev, "chg_details_00 is %x\n", val);

		max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &val);
		dev_info(charger->dev, "chg_details_01 is %x\n", val);

		max77665_read_reg(charger, MAX77665_CHG_DTLS_02, &val);
		dev_info(charger->dev, "chg_details_02 is %x\n", val);
	}
}

static int max77665_handle_charger_status(struct max77665_charger *charger,
		uint32_t status)
{
	uint32_t val;

	max77665_display_charger_status(charger, status);

	max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &val);
	if (!(status & BAT_BIT)) {
		if (BAT_DTLS_MASK(val) == BAT_DTLS_OVERCURRENT)
			charger->oc_count++;
	}

	return 0;
}

static int max77665_set_charger_mode(struct max77665_charger *charger,
		enum max77665_mode mode)
{
	int ret;
	int flags;

	charger->mode = mode;
	ret = max77665_enable_write(charger, true);
	if (ret < 0)
		return ret;

	if (mode == OFF)
		flags = CHARGER_OFF_OTG_OFF_BUCK_ON_BOOST_OFF;
	if (mode == CHARGER)
		/* enable charging and charging watchdog */
		flags = CHARGER_ON_OTG_OFF_BUCK_ON_BOOST_OFF | WDTEN;
	else if (mode == OTG)
		flags = CHARGER_OFF_OTG_ON_BUCK_OFF_BOOST_ON;

	ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_00, flags);
	if (ret < 0)
		goto error;

	/*
	 * Under regulation loop voltage, the VBUS should be higher then the
	 * Charging Port Undershoot Voltage(4.2v) according the USB charging
	 * spec 1.2
	 */
	max77665_read_reg(charger, MAX77665_CHG_CNFG_12, &flags);
	flags |= VCHGIN_REGULATION_4V3;
	ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_12, flags);
	if (ret < 0)
		goto error;

	/*
	 * Set to max current in theory. If the the charger has less current
	 * capability, we will calibrated the current inside the charging error
	 * IRQs handler.
	 */
	ret = max77665_set_max_input_current(charger, charger->max_current_mA);
	dev_info(charger->dev, "max input current %sset to %dmA\n",
			(ret == 0) ? "" : "failed ", charger->max_current_mA);
error:
	return max77665_enable_write(charger, false);
}

static int max77665_charger_init(struct max77665_charger *charger)
{
	int ret = 0;
	int val;

	ret = max77665_enable_write(charger, true);
	if (ret < 0)
		goto error;

	val = FAST_CHARGE_DURATION_8HR | CHARGER_RESTART_THRESHOLD_100mV |
						LOW_BATTERY_PREQ_ENABLE;
	ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_01, val);
	if (ret < 0) {
		dev_err(charger->dev, "Failed in writing register 0x%x\n",
			MAX77665_CHG_CNFG_01);
		goto error;
	}

	/* TOPOFF TO_ITH and TO_TIME setting */
	val = TOPOFF_TO_ITH_100MA | TOPOFF_TO_TIME_0MIN;
	ret = max77665_write_reg(charger, MAX77665_CHG_CNFG_03, val);
	if (ret < 0) {
		dev_err(charger->dev, "Failed in writing register 0x%x\n",
			MAX77665_CHG_CNFG_03);
		goto error;
	}

	if (charger->fast_chg_cc >= 0) {
		val = CONVERT_TO_REG(chg_cc, charger->fast_chg_cc);
		ret = max77665_update_bits(charger->dev->parent,
					MAX77665_I2C_SLAVE_PMIC,
					MAX77665_CHG_CNFG_02, CHG_CC_MASK,
					val);
		if (ret < 0) {
			dev_err(charger->dev, "Failed writing register 0x%x\n",
				MAX77665_CHG_CNFG_02);
			goto error;
		}
	}

	if (charger->term_volt) {
		val = CONVERT_TO_REG(chg_cv_prm, charger->term_volt);
		ret = max77665_update_bits(charger->dev->parent,
					MAX77665_I2C_SLAVE_PMIC,
					MAX77665_CHG_CNFG_04,
					CHG_CV_PRM_MASK, val);
		if (ret < 0) {
			dev_err(charger->dev, "Failed writing to reg:0x%x\n",
				MAX77665_CHG_CNFG_04);
			goto error;
		}
	}

error:
	ret = max77665_enable_write(charger, false);
	return ret;
}

static void max77665_charger_disable_wdt(struct max77665_charger *charger)
{
	cancel_delayed_work_sync(&charger->wdt_ack_work);
	alarm_cancel(&charger->wdt_alarm);
}

static int max77665_disable_charger(struct max77665_charger *charger,
					struct extcon_dev *edev)
{
	int ret;

	if (usb_cable_is_online == 0) {
		dev_info(charger->dev, "%s usb is not online\n", __func__);
		return 0;
	}

	/* Keep the chgin limit to 200mA for bringup when battery is depleted */
	charger->max_current_mA = 200;
	ret = max77665_set_charger_mode(charger, OFF);
	if (ret < 0)
		dev_err(charger->dev, "failed to disable charging");

	max77665_charger_disable_wdt(charger);

	__cancel_delayed_work(&charger->charger_monitor_work);

	__cancel_delayed_work(&charger->unplug_check_work);
	if (charger->plat_data->update_status)
		charger->plat_data->update_status(0);

	usb_cable_is_online = 0;
	usb_target_ma = 0;
	charger->ac_online = 0;
	charger->usb_online = 0;
	power_supply_changed(&charger->usb);
	power_supply_changed(&charger->ac);

	wake_unlock(&charger->chg_wake_lock);

	return ret;
}

static int max77665_enable_charger(struct max77665_charger *charger,
					struct extcon_dev *edev)
{
	int ret = -EINVAL;
	int ilim, batt_temp;
	enum max77665_mode mode;

	if (charging_disabled)
		return 0;

	if (usb_cable_is_online) {
		dev_info(charger->dev, "usb also online, disabled first\n");
		max77665_disable_charger(charger, edev);
	}

	ret = maxim_get_temp(&batt_temp);
	if (ret < 0) {
		dev_err(charger->dev, "%s failed in reading temp\n", __func__);
		return ret;
	} else if (!batt_temp_is_valid(batt_temp / 10)) {
		dev_err(charger->dev, "temp%d invalid \n", batt_temp);
		return -EINVAL;
	}

	charger->usb_online = 0;
	charger->ac_online = 0;

	if (charger->plat_data->update_status)
		charger->plat_data->update_status(0);

	mode = CHARGER;
	if (true == extcon_get_cable_state(edev, "USB-Host")) {
		mode = OTG;
		charger->max_current_mA = 0;
	} else if (true == extcon_get_cable_state(edev, "USB")) {
		mode = CHARGER;
		charger->usb_online = 1;
		charger->max_current_mA = 500;
	} else if (true == extcon_get_cable_state(edev, "Charge-downstream")) {
		mode = CHARGER;
		charger->usb_online = 1;
		charger->max_current_mA = 1500;
	} else if (true == extcon_get_cable_state(edev, "TA")) {
		mode = CHARGER;
		charger->ac_online = 1;
		charger->max_current_mA = 2000;
	} else if (true == extcon_get_cable_state(edev, "Fast-charger")) {
		mode = CHARGER;
		charger->ac_online = 1;
		charger->max_current_mA = 2200;
	} else if (true == extcon_get_cable_state(edev, "Slow-charger")) {
		mode = CHARGER;
		charger->ac_online = 1;
		charger->max_current_mA = 500;
	} else if (usb_cable_is_present) {
		/* Invalid charger */
		mode = CHARGER;
		charger->usb_online = 1;
		charger->max_current_mA = 500;
	} else {
		/* no cable connected */
		mode = OFF;
		charger->ac_online = 0;
		charger->usb_online = 0;
		goto done;
	}

	/* Hold wakelock for i2c access */
	wake_lock(&charger->chg_wake_lock);

	usb_cable_is_online = 1;

	if (charger->max_current_mA > usb_max_current) {
		dev_info(charger->dev, "%d exceed maximum, revert to %d\n",
			 charger->max_current_mA, usb_max_current);
		charger->max_current_mA = usb_max_current;
	}

	if (charger->ac_online)
		usb_target_ma = charger->max_current_mA;
	if (usb_target_ma > USB_WALL_THRESHOLD_MA)
		charger->max_current_mA = USB_WALL_THRESHOLD_MA;

	ret = max77665_set_charger_mode(charger, mode);
	if (ret < 0) {
		dev_err(charger->dev, "failed to set device to charger mode\n");
		goto done;
	}

	/* Charging process monitor */
	schedule_delayed_work(&charger->charger_monitor_work,
					round_jiffies_relative(msecs_to_jiffies
					(CHARGER_MONITOR_WAIT_PERIOD_MS)));

	/* The cable is connneted */
	schedule_delayed_work(&charger->unplug_check_work,
					round_jiffies_relative(msecs_to_jiffies
					(UNPLUG_CHECK_WAIT_PERIOD_MS)));

	/* set the charging watchdog timer */
	alarm_start(&charger->wdt_alarm, ktime_add(ktime_get_boottime(),
						ktime_set
						(MAX77665_WATCHDOG_TIMER_PERIOD_S
						/ 2, 0)));

	if (charger->plat_data->update_status) {
		ret = max77665_get_max_input_current(charger, &ilim);
		if (ret < 0)
			goto done;
		if (mode == CHARGER)
			charger->plat_data->update_status(1);
	}

done:
	if (charger->usb_online)
		power_supply_changed(&charger->usb);
	if (charger->ac_online)
		power_supply_changed(&charger->ac);

	return ret;
}

static void charger_extcon_handle_notifier(struct work_struct *w)
{
	struct max77665_charger_cable *cable = container_of(to_delayed_work(w),
							struct
							max77665_charger_cable,
							extcon_notifier_work);
	struct max77665_charger *charger = cable->charger;

	mutex_lock(&charger->current_limit_mutex);
	if (cable->event == 0)
		max77665_disable_charger(charger, cable->extcon_dev->edev);
	else if (cable->event == 1)
		max77665_enable_charger(charger, cable->extcon_dev->edev);
	mutex_unlock(&charger->current_limit_mutex);

}

static int max77665_reset_charger(struct max77665_charger *charger,
				struct extcon_dev *edev)
{
	int ret;

	mutex_lock(&charger->current_limit_mutex);

	ret = max77665_disable_charger(charger, charger->edev);
	if (ret < 0)
		goto error;

	ret = max77665_enable_charger(charger, charger->edev);
	if (ret < 0)
		goto error;
      error:
	mutex_unlock(&charger->current_limit_mutex);
	return 0;
}

static void max77665_charger_wdt_ack_work_handler(struct work_struct *w)
{
	struct max77665_charger *charger = container_of(to_delayed_work(w),
							struct max77665_charger,
							wdt_ack_work);

	if (0 > max77665_update_reg(charger, MAX77665_CHG_CNFG_06, WDTCLR))
		dev_err(charger->dev, "fail to ack charging WDT\n");

	alarm_start(&charger->wdt_alarm,
			ktime_add(ktime_get_boottime(), ktime_set(30, 0)));
	wake_unlock(&charger->wdt_wake_lock);
}

static enum alarmtimer_restart max77665_charger_wdt_timer(struct alarm *alarm,
		ktime_t now)
{
	struct max77665_charger *charger =
		container_of(alarm, struct max77665_charger, wdt_alarm);

	wake_lock(&charger->wdt_wake_lock);
	schedule_delayed_work(&charger->wdt_ack_work, 0);
	return ALARMTIMER_NORESTART;
}

static void invalid_charger_check_worker(struct work_struct *work)
{
	struct max77665_charger *charger = container_of(to_delayed_work(work),
							struct max77665_charger,
							invalid_charger_check_work);

	mutex_lock(&charger->current_limit_mutex);
	/* usb cable is plugged, but not charging */
	if (usb_cable_is_present && !usb_cable_is_online) {
		dev_info(charger->dev, "invalid charger attached\n");
		max77665_enable_charger(charger, charger->edev);
	}

	/* usb cable is not present, but charging */
	if (!usb_cable_is_present && usb_cable_is_online) {
		dev_info(charger->dev, "invalid charger detached\n");
		max77665_disable_charger(charger, charger->edev);
	}

	mutex_unlock(&charger->current_limit_mutex);
}

static int charger_pmu_extcon_notifier(struct notifier_block *self,
					unsigned long event, void *ptr)
{
	if (!the_charger)
		return NOTIFY_BAD;

	if (extcon_get_cable_state(the_charger->pmu_edev, "USB"))
		usb_cable_is_present = 1;
	else
		usb_cable_is_present = 0;

	dev_info(the_charger->dev, "usb cable is %d\n", usb_cable_is_present);

	__cancel_delayed_work(&the_charger->invalid_charger_check_work);
	schedule_delayed_work(&the_charger->invalid_charger_check_work,
					msecs_to_jiffies((usb_cable_is_present ? 2000 :
					1000)));

	return NOTIFY_DONE;
}

static int charger_extcon_notifier(struct notifier_block *self,
					unsigned long event, void *ptr)
{
	struct max77665_charger_cable *cable = container_of(self,
							struct
							max77665_charger_cable,
							nb);

	if (the_charger)
		dev_info(the_charger->dev, "event %lu\n", event);

	cable->event = event;
	__cancel_delayed_work(&cable->extcon_notifier_work);
	schedule_delayed_work(&cable->extcon_notifier_work,
					msecs_to_jiffies
					(CHARGER_TYPE_DETECTION_DEBOUNCE_TIME_MS));

	return NOTIFY_DONE;
}

void max77665_charger_vbus_draw(unsigned int mA)
{
	pr_info("%s Enter charger:%d\n", __func__, mA);
}

EXPORT_SYMBOL(max77665_charger_vbus_draw);

/* Stop/start charging process based on the battery temperature */
void max77665_charger_temp_control(int batt_temp)
{
	if (the_charger == NULL)
		return;

	if ((the_charger->ac_online || the_charger->usb_online) &&
				!batt_temp_is_valid(batt_temp)) {
				dev_info(the_charger->dev, "T%d disable charging\n", batt_temp);
		max77665_disable_charger(the_charger, the_charger->edev);
	}

	if (!(the_charger->ac_online || the_charger->usb_online) &&
				batt_temp_is_valid(batt_temp) &&
				usb_cable_is_present && is_usb_chg_plugged_in(the_charger)) {
		dev_info(the_charger->dev, "T%d enable charging\n", batt_temp);
		max77665_enable_charger(the_charger, the_charger->edev);
	}

}

EXPORT_SYMBOL(max77665_charger_temp_control);

static int max77665_update_charger_status(struct max77665_charger *charger)
{
	int ret;
	uint32_t read_val, status;

	mutex_lock(&charger->current_limit_mutex);

	ret = max77665_read_reg(charger, MAX77665_CHG_INT, &read_val);
	if (ret < 0 || read_val == 0)
		goto error;

	ret = max77665_read_reg(charger, MAX77665_CHG_INT_OK, &status);
	if (ret < 0)
		goto error;

	dev_info(charger->dev, "INT %02x STATUS %02x\n", read_val, status);
	if (charger->plat_data->is_battery_present)
		max77665_handle_charger_status(charger, status);

error:
	mutex_unlock(&charger->current_limit_mutex);
	return ret;
}

static irqreturn_t max77665_charger_irq_handler(int irq, void *data)
{
	struct max77665_charger *charger = data;

	max77665_update_charger_status(charger);
	return IRQ_HANDLED;
}

static void charger_monitor_worker(struct work_struct *work)
{
	struct max77665_charger *charger = container_of(to_delayed_work(work),
							struct max77665_charger,
							charger_monitor_work);
	uint32_t dtls_01;
	int vbatt, batt_temp;
	int mA, voltage;
	int ret;

	mutex_lock(&charger->current_limit_mutex);

	if (charging_disabled) {
		mutex_unlock(&charger->current_limit_mutex);
		return;
	}

	if (!usb_cable_is_online) {
		mutex_unlock(&charger->current_limit_mutex);
		return;
	}

	/* Temperature monitor */
	ret = maxim_get_temp(&batt_temp);
	if (ret < 0) {
		dev_err(charger->dev, "failed in reading batt temperaure\n");
		goto out;
	}

	batt_temp = batt_temp / 10;
	/* Is the battery cool */
	mA = charger->plat_data->cool_bat_chg_current;
	voltage = charger->plat_data->cool_bat_voltage;
	if (batt_temp_is_cool(batt_temp) &&
				(mA != charger->fast_chg_cc || voltage != charger->term_volt)) {
		dev_info(charger->dev, "T%d I%d V%d\n", batt_temp, mA, voltage);
		charger->fast_chg_cc = mA;
		charger->term_volt = voltage;
		max77665_charger_init(charger);
	}

	/* Is the battery warm */
	mA = charger->plat_data->warm_bat_chg_current;
	voltage = charger->plat_data->warm_bat_voltage;
	if (batt_temp_is_warm(batt_temp) &&
				(mA != charger->fast_chg_cc || voltage != charger->term_volt)) {
		dev_info(charger->dev, "T%d I%d V%d\n", batt_temp, mA, voltage);
		charger->fast_chg_cc = mA;
		charger->term_volt = voltage;
		max77665_charger_init(charger);
	}

	/* Is the battery normal */
	mA = charger->plat_data->fast_chg_cc;
	voltage = charger->plat_data->term_volt;
	if (batt_temp_is_normal(batt_temp) &&
				(mA != charger->fast_chg_cc || voltage != charger->term_volt)) {
		dev_info(charger->dev, "T%d I%d V%d\n", batt_temp, mA, voltage);
		charger->fast_chg_cc = mA;
		charger->term_volt = voltage;
		max77665_charger_init(charger);
	}

	/* Resume charging monitor */
	ret = max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &dtls_01);
	if (ret < 0) {
		dev_err(charger->dev, "%s read dtls01 ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(charger->dev, "dtls_01 %x\n", dtls_01);

	ret = maxim_get_batt_voltage_avg(&vbatt);
	if (ret < 0) {
		dev_err(charger->dev, "%s read batt vol %d\n", __func__, ret);
		goto out;
	}

	vbatt = vbatt / 1000;
	/* Resume charging */
	if (batt_temp_is_valid(batt_temp) &&
				((CHG_DTLS_MASK(dtls_01) == CHG_DTLS_TIME_FAULT_MODE) ||
				(CHG_DTLS_MASK(dtls_01) == CHG_DTLS_DONE_MODE &&
				(charger->term_volt - vbatt) >= MAX77665_CHG_SOFT_RSTRT))) {
		/* Disable the charger, then enable it again */
		dev_info(charger->dev, "%x %d recharging\n", dtls_01, vbatt);
		ret = max77665_set_charger_mode(charger, OFF);
		if (ret < 0)
			dev_err(charger->dev, "failed to disable charging");
		max77665_charger_init(charger);
		ret = max77665_set_charger_mode(charger, CHARGER);
		if (ret < 0)
			dev_err(charger->dev, "failed to enable charging");
	}

out:
	mutex_unlock(&charger->current_limit_mutex);

	schedule_delayed_work(&charger->charger_monitor_work,
				round_jiffies_relative(msecs_to_jiffies
				(CHARGER_MONITOR_WAIT_PERIOD_MS)));
}

static void unplug_check_worker(struct work_struct *work)
{
	struct max77665_charger *charger = container_of(to_delayed_work(work),
							struct max77665_charger,
							unplug_check_work);
	uint32_t charging_ok, mA;
	static bool modified;

	mutex_lock(&charger->current_limit_mutex);
	if (charging_disabled) {
		dev_info(charger->dev, "charging disabled, exit\n");
		mutex_unlock(&charger->current_limit_mutex);
		return;
	}

	if (usb_cable_is_online == 0) {
		dev_info(charger->dev, "usb not present, exit\n");
		mutex_unlock(&charger->current_limit_mutex);
		return;
	}

	if (charger->usb_online) {
		dev_info(charger->dev, "usb enumerated, exit\n");
		mutex_unlock(&charger->current_limit_mutex);
		return;
	}

	charging_ok = max77665_check_charging_ok(charger);
	mA = charger->max_current_mA;
	if (charging_ok && mA < usb_target_ma) {
		modified = 0;
		charger->max_current_mA = mA = mA + 100;
		max77665_set_max_input_current(charger, mA);
		dev_info(charger->dev, "increase current %d target %d\n",
			 charger->max_current_mA, usb_target_ma);
	} else if (!charging_ok && mA > 200) {
		modified = 0;
		charger->max_current_mA = mA = mA - 20;
		max77665_set_max_input_current(charger, mA);
		usb_target_ma = charger->max_current_mA;
		dev_info(charger->dev, "reduce current %d target %d\n",
				charger->max_current_mA, usb_target_ma);
	} else if (modified == 0 &&
				charging_ok && mA >= 1000 && (mA == usb_target_ma)) {
		modified = 1;
		/* Set 95% of target to avoid overload */
		if (mA >= 1400)
			/* 95% of target */
			mA = DIV_ROUND_UP((mA * 95) / 100, 20) * 20;
		else if (mA >= 1200)
			/* 80% of target */
			mA = DIV_ROUND_UP((mA * 80) / 100, 20) * 20;
		else if (mA >= 1100)
			/* 90% of target */
			mA = DIV_ROUND_UP((mA * 90) / 100, 20) * 20;

		charger->max_current_mA = usb_target_ma = mA;
		max77665_set_max_input_current(charger, mA);
		dev_info(charger->dev, "modify current %d target %d\n",
			 charger->max_current_mA, usb_target_ma);
	}

	mutex_unlock(&charger->current_limit_mutex);

	/* schedule to check again later */
	schedule_delayed_work(&charger->unplug_check_work,
				round_jiffies_relative(msecs_to_jiffies
				(UNPLUG_CHECK_WAIT_PERIOD_MS)));
}

static ssize_t max77665_set_bat_oc_threshold(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	int i;
	int ret;
	int val = 0;
	int n = ARRAY_SIZE(max77665_bat_to_sys_oc_thres);
	char *p = (char *)buf;
	int oc_curr = memparse(p, &p);

	for (i = 0; i < n; ++i) {
		if (oc_curr <= max77665_bat_to_sys_oc_thres[i])
			break;
	}

	val = (i < n) ? i : n - 1;
	ret = max77665_update_bits(charger->dev->parent,
			MAX77665_I2C_SLAVE_PMIC, MAX77665_CHG_CNFG_12,
			BAT_TO_SYS_OVERCURRENT_MASK, val);
	if (ret < 0) {
		dev_err(charger->dev, "CHG_CNFG_12 update failed: %d\n", ret);
		return ret;
	}
	return count;
}

static ssize_t max77665_show_bat_oc_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	uint8_t val = 0;
	int ret;

	ret = max77665_read(charger->dev->parent, MAX77665_I2C_SLAVE_PMIC,
				MAX77665_CHG_CNFG_12, &val);
	if (ret < 0) {
		dev_err(charger->dev, "CHG_CNFG_12 read failed: %d\n", ret);
		return ret;
	}
	val &= BAT_TO_SYS_OVERCURRENT_MASK;
	return sprintf(buf, "%d\n", max77665_bat_to_sys_oc_thres[val]);
}
static DEVICE_ATTR(oc_threshold,  0644,
		max77665_show_bat_oc_threshold, max77665_set_bat_oc_threshold);

static ssize_t max77665_set_battery_oc_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	int ret;
	bool enabled;
	unsigned int val;

	if ((*buf == 'E') || (*buf == 'e')) {
		enabled = true;
	} else if ((*buf == 'D') || (*buf == 'd')) {
		enabled = false;
	} else {
		dev_err(charger->dev, "Illegal option\n");
		return -EINVAL;
	}

	val = (enabled) ? 0x0 : 0x8;
	ret = max77665_update_bits(charger->dev->parent,
			MAX77665_I2C_SLAVE_PMIC,
			MAX77665_CHG_INT_MASK, 0x08, val);
	if (ret < 0) {
		dev_err(charger->dev, "CHG_INT_MASK update failed: %d\n", ret);
		return ret;
	}
	return count;
}

static ssize_t max77665_show_battery_oc_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	uint8_t val = 0;
	int ret;

	ret = max77665_read(charger->dev->parent, MAX77665_I2C_SLAVE_PMIC,
			 MAX77665_CHG_INT_MASK, &val);
	if (ret < 0) {
		dev_err(charger->dev, "CHG_INT_MASK read failed: %d\n", ret);
		return ret;
	}
	if (val & 0x8)
		return sprintf(buf, "disabled\n");
	else
		return sprintf(buf, "enabled\n");
}
static DEVICE_ATTR(oc_state, 0644,
		max77665_show_battery_oc_state, max77665_set_battery_oc_state);

static ssize_t max77665_show_battery_oc_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", charger->oc_count);
}
static DEVICE_ATTR(oc_count, 0444, max77665_show_battery_oc_count, NULL);

static ssize_t max77665_show_battery_charging_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);
	char *state_string[] = {
		"PREQUALIFICATION MODE",		/* 0X0 */
		"FAST CONSTANT CURRENT MODE",		/* 0X1 */
		"FAST CONSTANT VOLTAGE MODE",		/* 0X2 */
		"TOP OFF MODE",				/* 0X3 */
		"DONE MODE",				/* 0X4 */
		"HIGH TEMP CHG MODE",			/* 0X5 */
		"TIME FAULT MODE",			/* 0X6 */
		"THERMISTOR SUSPEND FAULT MODE",	/* 0X7 */
		"CHG OFF INPUT INVALID",		/* 0X8 */
		"RSVD",					/* 0X9 */
		"CHG OFF JUNCT TEMP",			/* 0XA */
		"CHG OFF WDT EXPIRE",			/* 0XB */
	};
	int val;

	if (0 > max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &val))
		return -EINVAL;

	val = CHG_DTLS_MASK(val);
	if (val >= ARRAY_SIZE(state_string))
		return -EINVAL;

	return sprintf(buf, "%s(%d)\n", state_string[val], val);
}
static DEVICE_ATTR(charging_state, 0444,
		max77665_show_battery_charging_state, NULL);

static struct attribute *max77665_chg_attributes[] = {
	&dev_attr_oc_threshold.attr,
	&dev_attr_oc_state.attr,
	&dev_attr_oc_count.attr,
	&dev_attr_charging_state.attr,
	NULL,
};

static const struct attribute_group max77665_chg_attr_group = {
	.attrs = max77665_chg_attributes,
};

static int max77665_add_sysfs_entry(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &max77665_chg_attr_group);
}
static void max77665_remove_sysfs_entry(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &max77665_chg_attr_group);
}

static __devinit int max77665_battery_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint8_t j;
	uint32_t read_val;
	struct max77665_charger *charger;
	int temp;

	charger = devm_kzalloc(&pdev->dev, sizeof(*charger), GFP_KERNEL);
	if (!charger) {
		dev_err(&pdev->dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	mutex_init(&charger->current_limit_mutex);

	charger->dev = &pdev->dev;

	charger->plat_data = pdev->dev.platform_data;
	charger->fast_chg_cc = charger->plat_data->fast_chg_cc;
	charger->term_volt = charger->plat_data->term_volt;
	dev_set_drvdata(&pdev->dev, charger);

	if (charger->plat_data->is_battery_present) {
		wake_lock_init(&charger->wdt_wake_lock, WAKE_LOCK_SUSPEND,
				"max77665-charger-wdt");
		wake_lock_init(&charger->chg_wake_lock, WAKE_LOCK_SUSPEND,
				"max77665-charger");
		alarm_init(&charger->wdt_alarm, ALARM_BOOTTIME,
				max77665_charger_wdt_timer);
		INIT_DELAYED_WORK(&charger->wdt_ack_work,
				max77665_charger_wdt_ack_work_handler);
		INIT_DELAYED_WORK(&charger->unplug_check_work,
				unplug_check_worker);
		INIT_DELAYED_WORK(&charger->charger_monitor_work,
				charger_monitor_worker);
		INIT_DELAYED_WORK(&charger->invalid_charger_check_work,
				invalid_charger_check_worker);

		/* modify OTP setting of input current limit to 200ma */
		ret = max77665_set_max_input_current(charger, 200);
		if (ret < 0)
			goto remove_charging;

		dev_info(&pdev->dev, "Initializing battery charger code\n");

		/* check for battery presence */
		ret =
		    max77665_read_reg(charger, MAX77665_CHG_DTLS_01, &read_val);
		if (ret < 0)
			goto remove_charging;
		if (BAT_DTLS_NO_BATTERY == BAT_DTLS_MASK(read_val)) {
			dev_err(&pdev->dev,
				"Battery not detected, exiting driver\n");
			goto remove_charging;
		}

		/* differentiate between E1236 and E1587 */
		ret = maxim_get_temp(&temp);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed in reading temperaure\n");
			goto remove_charging;
		} else if ((temp < MIN_TEMP) || (temp > MAX_TEMP)) {
			dev_err(&pdev->dev,
				"E1236 detected exiting driver....\n");
			goto remove_charging;
		}

		charger->ac.name = "ac";
		charger->ac.type = POWER_SUPPLY_TYPE_MAINS;
		charger->ac.get_property = max77665_charger_get_property;
		charger->ac.set_property = max77665_charger_set_property;
		charger->ac.properties = max77665_charger_props;
		charger->ac.num_properties = ARRAY_SIZE(max77665_charger_props);
		charger->ac.property_is_writeable =
		    max77665_charger_property_is_writeable;
		ret = power_supply_register(charger->dev, &charger->ac);
		if (ret) {
			dev_err(charger->dev,
				"failed: power supply register\n");
			return ret;
		}

		charger->usb = charger->ac;
		charger->usb.name = "usb";
		charger->usb.type = POWER_SUPPLY_TYPE_USB;
		ret = power_supply_register(charger->dev, &charger->usb);
		if (ret) {
			dev_err(charger->dev,
				"failed: power supply register\n");
			goto pwr_sply_error;
		}

		for (j = 0; j < charger->plat_data->num_cables; j++) {
			struct max77665_charger_cable *cable =
			    &charger->plat_data->cables[j];
			cable->extcon_dev = devm_kzalloc(&pdev->dev,
							 sizeof(struct
								extcon_specific_cable_nb),
							 GFP_KERNEL);
			if (!cable->extcon_dev) {
				dev_err(&pdev->dev,
					"failed to allocate memory for extcon dev\n");
				goto chrg_error;
			}

			INIT_DELAYED_WORK(&cable->extcon_notifier_work,
					  charger_extcon_handle_notifier);

			cable->charger = charger;
			cable->nb.notifier_call = charger_extcon_notifier;

			ret = extcon_register_interest(cable->extcon_dev,
						charger->plat_data->
						extcon_name, cable->name,
						&cable->nb);
			if (ret < 0)
				dev_err(charger->dev,
					"Cannot register for cable: %s\n",
					cable->name);
		}

		charger->edev =
		    extcon_get_extcon_dev(charger->plat_data->extcon_name);
		if (!charger->edev) {
			dev_err(charger->dev, "Can't get %s extcon dev\n",
				charger->plat_data->extcon_name);
			goto chrg_error;
		}

		/* Register PMU extcon notifier */
		charger->pmu_edev =
		    extcon_get_extcon_dev(charger->plat_data->pmu_extcon_name);
		if (!charger->pmu_edev) {
			dev_err(charger->dev, "Can't get %s extcon dev\n",
				charger->plat_data->pmu_extcon_name);
			goto chrg_error;
		}

		charger_nb.notifier_call = charger_pmu_extcon_notifier;
		extcon_register_notifier(charger->pmu_edev, &charger_nb);
	}

	charger->irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(charger->irq, NULL,
			max77665_charger_irq_handler, 0, "charger_irq",
			charger);
	if (ret) {
		dev_err(&pdev->dev, "failed: irq request error :%d)\n", ret);
		goto chrg_error;
	}
	/* FIXME
	 * Some unexpected interrupts triggered during charing, so temperarily
	 * mask all the interrupt except OC interrupt, and use unplug check
	 * work to handle voltage regulation loop.
	 */
	max77665_write_reg(charger, MAX77665_CHG_INT_MASK, 0xef);

	ret = max77665_add_sysfs_entry(&pdev->dev);
	if (ret < 0) {
		dev_err(charger->dev, "sysfs create failed %d\n", ret);
		goto free_irq;
	}

	if (charger->plat_data->is_battery_present) {
		ret = max77665_charger_init(charger);
		if (ret < 0) {
			dev_err(charger->dev, "failed to initialize charger\n");
			goto remove_sysfs;
		}
	}

	/* Set OC threshold */
	ret = max77665_update_bits(charger->dev->parent,
		MAX77665_I2C_SLAVE_PMIC, MAX77665_CHG_CNFG_12,
		BAT_TO_SYS_OVERCURRENT_MASK, charger->plat_data->oc_alert);
	if (ret < 0) {
		dev_err(charger->dev, "CHG_CNFG_12 update failed: %d\n", ret);
		goto remove_sysfs;
	}

	the_charger = charger;

	if (charger->plat_data->is_battery_present) {
		/* reset the charging in case cable is already inserted */
		ret = max77665_reset_charger(charger, charger->edev);
		if (ret < 0)
			goto chrg_error;
	}

	/* Check if usb cable is present or not, should be called after
	 * max77665_reset_charger() to avoid invalid chg detected incorrectly
	 */
	charger_pmu_extcon_notifier(NULL, 0, NULL);

	dev_info(&pdev->dev, "%s() get success\n", __func__);
	return 0;

remove_sysfs:
	max77665_remove_sysfs_entry(&pdev->dev);
free_irq:
	free_irq(charger->irq, charger);
chrg_error:
	if (charger->plat_data->is_battery_present)
		power_supply_unregister(&charger->usb);
pwr_sply_error:
	if (charger->plat_data->is_battery_present)
		power_supply_unregister(&charger->ac);
remove_charging:
	mutex_destroy(&charger->current_limit_mutex);
	if (charger->plat_data->is_battery_present)
		wake_lock_destroy(&charger->wdt_wake_lock);
	return ret;
}

static int __devexit max77665_battery_remove(struct platform_device *pdev)
{
	struct max77665_charger *charger = platform_get_drvdata(pdev);

	max77665_remove_sysfs_entry(&pdev->dev);
	free_irq(charger->irq, charger);
	if (charger->plat_data->is_battery_present)
		power_supply_unregister(&charger->ac);
	if (charger->plat_data->is_battery_present)
		power_supply_unregister(&charger->usb);

	return 0;
}
#ifdef CONFIG_PM_SLEEP
static int max77665_suspend(struct device *dev)
{
	return 0;
}
static int max77665_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct max77665_charger *charger = platform_get_drvdata(pdev);
	int ret;

	ret = max77665_update_charger_status(charger);
	if (ret < 0)
		dev_err(charger->dev, "error occured in resume\n");
	return ret;
}

static const struct dev_pm_ops max77665_pm = {
	.suspend = max77665_suspend,
	.resume = max77665_resume,
};
#define MAX77665_PM	(&max77665_pm)
#else
#define MAX77665_PM	NULL
#endif
static struct platform_driver max77665_battery_driver = {
	.driver = {
		.name = "max77665-charger",
		.owner = THIS_MODULE,
		.pm	= MAX77665_PM,
	},
	.probe = max77665_battery_probe,
	.remove = __devexit_p(max77665_battery_remove),

};

static int __init max77665_battery_init(void)
{
	return platform_driver_register(&max77665_battery_driver);
}

static void __exit max77665_battery_exit(void)
{
	platform_driver_unregister(&max77665_battery_driver);
}

static int set_usb_max_current(const char *val, struct kernel_param *kp)
{
	int ret, mA, max;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	max = usb_max_current;
	if (the_charger) {
		pr_warn("setting current max to %d\n", max);
		max77665_get_max_input_current(the_charger, &mA);
		if (mA > max)
			max77665_set_max_input_current(the_charger, max);
		return 0;
	}
	return -EINVAL;
}

module_param_call(usb_max_current, set_usb_max_current,
			param_get_uint, &usb_max_current, 0644);

/**
 * set_disable_status_param -
 *
 * Internal function to disable battery charging and also disable drawing
 * any current from the source. The device is forced to run on a battery
 * after this.
 */
static int set_disable_status_param(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_info("factory set disable param to %d\n", charging_disabled);
	if (the_charger) {
		if (charging_disabled) {
			max77665_disable_charger(the_charger,
						the_charger->edev);
			/* set buck and charger off */
			max77665_write_reg(the_charger, MAX77665_CHG_CNFG_00,
						0);
		} else
			max77665_enable_charger(the_charger, the_charger->edev);

	}

	return 0;
}

module_param_call(disabled, set_disable_status_param, param_get_uint,
			&charging_disabled, 0644);

late_initcall(max77665_battery_init);
module_exit(max77665_battery_exit);

MODULE_DESCRIPTION("MAXIM MAX77665 battery charging driver");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com>");
MODULE_LICENSE("GPL v2");
