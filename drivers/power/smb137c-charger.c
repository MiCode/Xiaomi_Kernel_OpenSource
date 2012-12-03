/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

struct smb137c_chip {
	struct i2c_client	*client;
	struct power_supply	psy;
	struct power_supply	*usb_psy;
	struct mutex		lock;
	int			charge_current_limit_ua;
	int			input_current_limit_ua;
	int			term_current_ua;
	bool			charging_enabled;
	bool			otg_mode_enabled;
	bool			charging_allowed;
	bool			usb_suspend_enabled;
};

struct input_current_config {
	int			current_limit_ua;
	u8			cmd_b_reg;
	u8			var_func_reg;
	u8			input_cur_reg;
};

struct term_current_config {
	int			term_current_ua;
	u8			charge_cur_reg;
};

#define INPUT_CURRENT(_current_limit_ua, _cmd_b_reg, _var_func_reg, \
			_input_cur_reg) \
	{ \
		.current_limit_ua	= _current_limit_ua, \
		.cmd_b_reg		= _cmd_b_reg, \
		.var_func_reg		= _var_func_reg, \
		.input_cur_reg		= _input_cur_reg, \
	}

#define CHARGE_CURRENT_REG		0x00
#define CHARGE_CURRENT_FAST_CHG_MASK	0xE0
#define CHARGE_CURRENT_FAST_CHG_SHIFT	5
#define CHARGE_CURRENT_PRE_CHG_MASK	0x18
#define CHARGE_CURRENT_PRE_CHG_SHIFT	3
#define CHARGE_CURRENT_TERM_CUR_MASK	0x06

#define INPUT_CURRENT_REG		0x01
#define INPUT_CURRENT_LIMIT_MASK	0xE0

#define FLOAT_VOLTAGE_REG		0x02
#define FLOAT_VOLTAGE_MASK		0x7F
#define FLOAT_VOLTAGE_SHIFT		0

#define CTRL_A_REG			0x03
#define CTRL_A_AUTO_RECHARGE_MASK	0x80
#define CTRL_A_AUTO_RECHARGE_ENABLED	0x00
#define CTRL_A_AUTO_RECHARGE_DISABLED	0x80
#define CTRL_A_TERM_CUR_MASK		0x40
#define CTRL_A_TERM_CUR_ENABLED		0x00
#define CTRL_A_TERM_CUR_DISABLED	0x40
#define CTRL_A_THRESH_VOLTAGE_MASK	0x38
#define CTRL_A_THRESH_VOLTAGE_SHIFT	3
#define CTRL_A_VOUTL_MASK		0x02
#define CTRL_A_VOUTL_4250MV		0x00
#define CTRL_A_VOUTL_4460MV		0x02
#define CTRL_A_THERM_MONITOR_MASK	0x01
#define CTRL_A_THERM_MONITOR_ENABLED	0x01
#define CTRL_A_THERM_MONITOR_DISABLED	0x00

#define PIN_CTRL_REG			0x05
#define PIN_CTRL_DEAD_BATT_CHG_MASK	0x80
#define PIN_CTRL_DEAD_BATT_CHG_ENABLED	0x80
#define PIN_CTRL_DEAD_BATT_CHG_DISABLED	0x00
#define PIN_CTRL_OTG_LBR_MASK		0x20
#define PIN_CTRL_OTG			0x00
#define PIN_CTRL_LBR			0x20
#define PIN_CTRL_USB_CUR_LIMIT_MASK	0x10
#define PIN_CTRL_USB_CUR_LIMIT_REG	0x00
#define PIN_CTRL_USB_CUR_LIMIT_PIN	0x10
#define PIN_CTRL_CHG_EN_MASK		0x0C
#define PIN_CTRL_CHG_EN_REG_LOW		0x00
#define PIN_CTRL_CHG_EN_REG_HIGH	0x04
#define PIN_CTRL_CHG_EN_PIN_LOW		0x08
#define PIN_CTRL_CHG_EN_PIN_HIGH	0x0C
#define PIN_CTRL_OTG_CTRL_MASK		0x02
#define PIN_CTRL_OTG_CTRL_REG		0x00
#define PIN_CTRL_OTG_CTRL_PIN		0x02

#define OTG_CTRL_REG			0x06
#define OTG_CTRL_BMD_MASK		0x80
#define OTG_CTRL_BMD_ENABLED		0x80
#define OTG_CTRL_BMD_DISABLED		0x00
#define OTG_CTRL_AUTO_RECHARGE_MASK	0x40
#define OTG_CTRL_AUTO_RECHARGE_75MV	0x00
#define OTG_CTRL_AUTO_RECHARGE_120MV	0x40

#define TEMP_MON_REG			0x08
#define TEMP_MON_THERM_CURRENT_MASK	0xC0
#define TEMP_MON_THERM_CURRENT_SHIFT	6
#define TEMP_MON_TEMP_LOW_MASK		0x38
#define TEMP_MON_TEMP_LOW_SHIFT		3
#define TEMP_MON_TEMP_HIGH_MASK		0x07
#define TEMP_MON_TEMP_HIGH_SHIFT	0

#define SAFETY_TIMER_REG		0x09
#define SAFETY_TIMER_RELOAD_MASK	0x40
#define SAFETY_TIMER_RELOAD_ENABLED	0x40
#define SAFETY_TIMER_RELOAD_DISABLED	0x00
#define SAFETY_TIMER_CHG_TIMEOUT_MASK	0x0C
#define SAFETY_TIMER_CHG_TIMEOUT_SHIFT	2
#define SAFETY_TIMER_PRE_CHG_TIME_MASK	0x03
#define SAFETY_TIMER_PRE_CHG_TIME_SHIFT	0

#define VAR_FUNC_REG			0x0C
#define VAR_FUNC_USB_MODE_MASK		0x80
#define VAR_FUNC_USB_SUSPEND_CTRL_MASK	0x20
#define VAR_FUNC_USB_SUSPEND_CTRL_REG	0x00
#define VAR_FUNC_USB_SUSPEND_CTRL_PIN	0x20
#define VAR_FUNC_BMD_MASK		0x0C
#define VAR_FUNC_BMD_DISABLED		0x00
#define VAR_FUNC_BMD_ALGO_PERIODIC	0x04
#define VAR_FUNC_BMD_ALGO		0x08
#define VAR_FUNC_BMD_THERM		0x0C

#define CMD_A_REG			0x30
#define CMD_A_VOLATILE_WRITE_MASK	0x80
#define CMD_A_VOLATILE_WRITE_ALLOW	0x80
#define CMD_A_VOLATILE_WRITE_DISALLOW	0x00
#define CMD_A_FAST_CHG_MASK		0x40
#define CMD_A_FAST_CHG_ALLOW		0x40
#define CMD_A_FAST_CHG_DISALLOW		0x00
#define CMD_A_OTG_MASK			0x10
#define CMD_A_OTG_ENABLED		0x10
#define CMD_A_OTG_DISABLED		0x00
#define CMD_A_USB_SUSPEND_MASK		0x04
#define CMD_A_USB_SUSPEND_DISABLED	0x00
#define CMD_A_USB_SUSPEND_ENABLED	0x04
#define CMD_A_CHARGING_MASK		0x02
#define CMD_A_CHARGING_ENABLED		0x00
#define CMD_A_CHARGING_DISABLED		0x02

#define CMD_B_REG			0x31
#define CMD_B_USB_MODE_MASK		0x03

#define DEV_ID_REG			0x33
#define DEV_ID_PART_MASK		0x80
#define DEV_ID_PART_SMB137C		0x00
#define DEV_ID_GUI_REV_MASK		0x70
#define DEV_ID_GUI_REV_SHIFT		4
#define DEV_ID_SILICON_REV_MASK		0x0F
#define DEV_ID_SILICON_REV_SHIFT	0

#define IRQ_STAT_A_REG			0x35
#define IRQ_STAT_A_BATT_HOT		0x40
#define IRQ_STAT_A_BATT_COLD		0x10

#define IRQ_STAT_B_REG			0x36
#define IRQ_STAT_B_BATT_OVERVOLT	0x40
#define IRQ_STAT_B_BATT_MISSING		0x10
#define IRQ_STAT_B_BATT_UNDERVOLT	0x04

#define STAT_C_REG			0x3D
#define STAT_C_CHG_ERROR		0x40
#define STAT_C_VBATT_LEVEL_BELOW_2P1V	0x10
#define STAT_C_CHG_STAT_MASK		0x06
#define STAT_C_CHG_STAT_SHIFT		1
#define STAT_C_CHG_ENABLED		0x01

/* Charge status register values */
enum smb137c_charge_status {
	CHARGE_STAT_NO_CHG	= 0,
	CHARGE_STAT_PRE_CHG	= 1,
	CHARGE_STAT_FAST_CHG	= 2,
	CHARGE_STAT_TAPER_CHG	= 3,
};

#define PRE_CHARGE_CURRENT_MIN_UA	50000
#define PRE_CHARGE_CURRENT_MAX_UA	200000
#define PRE_CHARGE_CURRENT_STEP_UA	50000

#define FLOAT_VOLTAGE_MIN_UV		3460000
#define FLOAT_VOLTAGE_MAX_UV		4730000
#define FLOAT_VOLTAGE_STEP_UV		10000

#define PRE_CHG_THRESH_VOLTAGE_MIN_UV	2400000
#define PRE_CHG_THRESH_VOLTAGE_MAX_UV	3100000
#define PRE_CHG_THRESH_VOLTAGE_STEP_UV	100000

#define USB_MIN_CURRENT_UA		100000

static int smb137c_read_reg(struct smb137c_chip *chip, u8 reg, u8 *val)
{
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client, reg);
	if (rc < 0) {
		pr_err("i2c_smbus_read_byte_data failed. reg=0x%02X, rc=%d\n",
			reg, rc);
	} else {
		*val = rc;
		rc = 0;
		pr_debug("read(0x%02X)=0x%02X\n", reg, *val);
	}

	return rc;
}

static int smb137c_write_reg(struct smb137c_chip *chip, u8 reg, u8 val)
{
	int rc;

	rc = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (rc < 0)
		pr_err("i2c_smbus_write_byte_data failed. reg=0x%02X, rc=%d\n",
			reg, rc);
	else
		pr_debug("write(0x%02X)=0x%02X\n", reg, val);

	return rc;
}

static int smb137c_masked_write_reg(struct smb137c_chip *chip, u8 reg, u8 mask,
					u8 val)
{
	u8 reg_val;
	int rc;

	pr_debug("masked write(0x%02X), mask=0x%02X, value=0x%02X\n", reg, mask,
		val);

	rc = smb137c_read_reg(chip, reg, &reg_val);
	if (rc < 0)
		return rc;

	val = (reg_val & ~mask) | (val & mask);

	if (val != reg_val)
		rc = smb137c_write_reg(chip, reg, val);

	return rc;
}

static int smb137c_enable_charging(struct smb137c_chip *chip)
{
	int rc = 0;

	chip->charging_allowed = true;

	if (!chip->charging_enabled && chip->charge_current_limit_ua > 0) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG,
			CMD_A_CHARGING_MASK, CMD_A_CHARGING_ENABLED);
		if (!rc)
			chip->charging_enabled = true;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static int smb137c_disable_charging(struct smb137c_chip *chip)
{
	int rc = 0;

	chip->charging_allowed = false;

	if (chip->charging_enabled) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG,
			CMD_A_CHARGING_MASK, CMD_A_CHARGING_DISABLED);
		if (!rc)
			chip->charging_enabled = false;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static int smb137c_enable_otg_mode(struct smb137c_chip *chip)
{
	int rc = 0;

	if (!chip->otg_mode_enabled) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG, CMD_A_OTG_MASK,
					CMD_A_OTG_ENABLED);
		if (!rc)
			chip->otg_mode_enabled = true;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static int smb137c_disable_otg_mode(struct smb137c_chip *chip)
{
	int rc = 0;

	if (chip->otg_mode_enabled) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG, CMD_A_OTG_MASK,
					CMD_A_OTG_DISABLED);
		if (!rc)
			chip->otg_mode_enabled = false;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static int smb137c_enable_usb_suspend(struct smb137c_chip *chip)
{
	int rc = 0;

	if (!chip->usb_suspend_enabled) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG,
			CMD_A_USB_SUSPEND_MASK, CMD_A_USB_SUSPEND_ENABLED);
		if (!rc)
			chip->usb_suspend_enabled = true;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static int smb137c_disable_usb_suspend(struct smb137c_chip *chip)
{
	int rc = 0;

	if (chip->input_current_limit_ua > 0 && chip->usb_suspend_enabled) {
		rc = smb137c_masked_write_reg(chip, CMD_A_REG,
			CMD_A_USB_SUSPEND_MASK, CMD_A_USB_SUSPEND_DISABLED);
		if (!rc)
			chip->usb_suspend_enabled = false;
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s\n", __func__);

	return rc;
}

static struct input_current_config supported_input_current[] = {
	INPUT_CURRENT(100000,  0x00, 0x00, 0x00),
	INPUT_CURRENT(150000,  0x00, 0x80, 0x00),
	INPUT_CURRENT(500000,  0x02, 0x00, 0x00),
	INPUT_CURRENT(700000,  0x01, 0x00, 0x00),
	INPUT_CURRENT(800000,  0x01, 0x00, 0x20),
	INPUT_CURRENT(900000,  0x01, 0x00, 0x40),
	INPUT_CURRENT(1000000, 0x01, 0x00, 0x60),
	INPUT_CURRENT(1100000, 0x01, 0x00, 0x80),
	INPUT_CURRENT(1200000, 0x01, 0x00, 0xA0),
	INPUT_CURRENT(1300000, 0x01, 0x00, 0xC0),
	INPUT_CURRENT(1500000, 0x01, 0x00, 0xE0),
};

static int smb137c_set_usb_input_current_limit(struct smb137c_chip *chip,
					int current_limit_ua)
{
	struct input_current_config *config = NULL;
	int rc = 0;
	int i;

	for (i = ARRAY_SIZE(supported_input_current) - 1; i >= 0; i--) {
		if (current_limit_ua
		    >= supported_input_current[i].current_limit_ua) {
			config = &supported_input_current[i];
			break;
		}
	}

	if (config) {
		if (chip->input_current_limit_ua != config->current_limit_ua) {
			rc = smb137c_masked_write_reg(chip, INPUT_CURRENT_REG,
			       INPUT_CURRENT_LIMIT_MASK, config->input_cur_reg);
			if (rc)
				return rc;

			rc = smb137c_masked_write_reg(chip, VAR_FUNC_REG,
				VAR_FUNC_USB_MODE_MASK, config->var_func_reg);
			if (rc)
				return rc;

			rc = smb137c_masked_write_reg(chip, CMD_B_REG,
				CMD_B_USB_MODE_MASK, config->cmd_b_reg);
			if (rc)
				return rc;

			chip->input_current_limit_ua = config->current_limit_ua;
		}

		rc = smb137c_disable_usb_suspend(chip);
	} else {
		chip->input_current_limit_ua = 0;

		rc = smb137c_enable_usb_suspend(chip);
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
			chip->input_current_limit_ua);

	return rc;
}

static int fast_charge_current_ua[] = {
	 500000,
	 650000,
	 750000,
	 850000,
	 950000,
	1100000,
	1300000,
	1500000,
};

static int smb137c_set_charge_current_limit(struct smb137c_chip *chip,
					int current_limit_ua)
{
	int fast_charge_limit_ua = 0;
	int rc = 0;
	u8 val = 0;
	int i;

	for (i = ARRAY_SIZE(fast_charge_current_ua) - 1; i >= 0; i--) {
		if (current_limit_ua >= fast_charge_current_ua[i]) {
			val = i << CHARGE_CURRENT_FAST_CHG_SHIFT;
			fast_charge_limit_ua = fast_charge_current_ua[i];
			break;
		}
	}

	if (fast_charge_limit_ua
	    && chip->charge_current_limit_ua != fast_charge_limit_ua)
		rc = smb137c_masked_write_reg(chip, CHARGE_CURRENT_REG,
					CHARGE_CURRENT_FAST_CHG_MASK, val);
	else if (fast_charge_limit_ua == 0)
		rc = smb137c_disable_charging(chip);

	chip->charge_current_limit_ua = fast_charge_limit_ua;

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
			fast_charge_limit_ua);

	return rc;
}

static int smb137c_get_charge_current_limit(struct smb137c_chip *chip)
{
	int fast_charge_limit_ua = 0;
	u8 val = 0;
	int rc, i;

	rc = smb137c_read_reg(chip, CHARGE_CURRENT_REG, &val);
	if (rc)
		return rc;

	i = (val & CHARGE_CURRENT_FAST_CHG_MASK)
		>> CHARGE_CURRENT_FAST_CHG_SHIFT;

	if (i >= 0 && i < ARRAY_SIZE(fast_charge_current_ua))
		fast_charge_limit_ua = fast_charge_current_ua[i];

	dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
		fast_charge_limit_ua);

	return fast_charge_limit_ua;
}

static struct term_current_config term_current_ua[] = {
	{ 35000, 0x06},
	{ 50000, 0x00},
	{100000, 0x02},
	{150000, 0x04},
};

static int smb137c_set_term_current(struct smb137c_chip *chip,
					int current_limit_ua)
{
	int term_current_limit_ua = 0;
	int rc = 0;
	u8 val = 0;
	int i;

	for (i = ARRAY_SIZE(term_current_ua) - 1; i >= 0; i--) {
		if (current_limit_ua >= term_current_ua[i].term_current_ua) {
			val = term_current_ua[i].charge_cur_reg;
			term_current_limit_ua
				= term_current_ua[i].term_current_ua;
			break;
		}
	}

	if (term_current_limit_ua) {
		rc = smb137c_masked_write_reg(chip, CHARGE_CURRENT_REG,
				CHARGE_CURRENT_TERM_CUR_MASK, val);
		if (rc)
			return rc;
		rc = smb137c_masked_write_reg(chip, CTRL_A_REG,
				CTRL_A_TERM_CUR_MASK, CTRL_A_TERM_CUR_ENABLED);
	} else {
		rc = smb137c_masked_write_reg(chip, CTRL_A_REG,
				CTRL_A_TERM_CUR_MASK, CTRL_A_TERM_CUR_DISABLED);
	}

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
			term_current_limit_ua);

	return rc;
}

static int smb137c_set_pre_charge_current_limit(struct smb137c_chip *chip,
					int current_limit_ua)
{
	int setpoint, rc;
	u8 val;

	if (current_limit_ua < PRE_CHARGE_CURRENT_MIN_UA ||
	    current_limit_ua > PRE_CHARGE_CURRENT_MAX_UA) {
		dev_err(&chip->client->dev, "%s: current limit out of bounds: %d\n",
			__func__, current_limit_ua);
		return -EINVAL;
	}

	setpoint = (current_limit_ua - PRE_CHARGE_CURRENT_MIN_UA)
			/ PRE_CHARGE_CURRENT_STEP_UA;
	val = setpoint << CHARGE_CURRENT_PRE_CHG_SHIFT;

	rc = smb137c_masked_write_reg(chip, CHARGE_CURRENT_REG,
			CHARGE_CURRENT_PRE_CHG_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
			setpoint * PRE_CHARGE_CURRENT_STEP_UA
			+ PRE_CHARGE_CURRENT_MIN_UA);

	return rc;
}

static int smb137c_set_float_voltage(struct smb137c_chip *chip, int voltage_uv)
{
	int setpoint, rc;
	u8 val;

	if (voltage_uv < FLOAT_VOLTAGE_MIN_UV ||
	    voltage_uv > FLOAT_VOLTAGE_MAX_UV) {
		dev_err(&chip->client->dev, "%s: voltage out of bounds: %d\n",
			__func__, voltage_uv);
		return -EINVAL;
	}

	setpoint = (voltage_uv - FLOAT_VOLTAGE_MIN_UV) / FLOAT_VOLTAGE_STEP_UV;
	val = setpoint << FLOAT_VOLTAGE_SHIFT;

	rc = smb137c_masked_write_reg(chip, FLOAT_VOLTAGE_REG,
			FLOAT_VOLTAGE_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: voltage=%d uV\n", __func__,
		       setpoint * FLOAT_VOLTAGE_STEP_UV + FLOAT_VOLTAGE_MIN_UV);

	return rc;
}

static int smb137c_set_pre_charge_threshold_voltage(struct smb137c_chip *chip,
							int voltage_uv)
{
	int setpoint, rc;
	u8 val;

	if (voltage_uv < PRE_CHG_THRESH_VOLTAGE_MIN_UV ||
	    voltage_uv > PRE_CHG_THRESH_VOLTAGE_MAX_UV) {
		dev_err(&chip->client->dev, "%s: voltage out of bounds: %d\n",
			__func__, voltage_uv);
		return -EINVAL;
	}

	setpoint = (voltage_uv - PRE_CHG_THRESH_VOLTAGE_MIN_UV)
			/ PRE_CHG_THRESH_VOLTAGE_STEP_UV;
	val = setpoint << CTRL_A_THRESH_VOLTAGE_SHIFT;

	rc = smb137c_masked_write_reg(chip, CTRL_A_REG,
			CTRL_A_THRESH_VOLTAGE_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: voltage=%d uV\n", __func__,
		       setpoint * PRE_CHG_THRESH_VOLTAGE_STEP_UV
		       + PRE_CHG_THRESH_VOLTAGE_MIN_UV);

	return rc;
}

static int smb137c_set_recharge_threshold_voltage(struct smb137c_chip *chip,
							int voltage_uv)
{
	int rc;
	u8 val;

	if (voltage_uv == 75000) {
		val = OTG_CTRL_AUTO_RECHARGE_75MV;
	} else if (voltage_uv == 120000) {
		val = OTG_CTRL_AUTO_RECHARGE_120MV;
	} else {
		dev_err(&chip->client->dev, "%s: voltage out of bounds: %d\n",
			__func__, voltage_uv);
		return -EINVAL;
	}

	rc = smb137c_masked_write_reg(chip, OTG_CTRL_REG,
			OTG_CTRL_AUTO_RECHARGE_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: voltage=%d uV\n", __func__,
		       voltage_uv);

	return rc;
}

static int smb137c_set_system_voltage(struct smb137c_chip *chip, int voltage_uv)
{
	int rc;
	u8 val;

	if (voltage_uv == 4250000) {
		val = CTRL_A_VOUTL_4250MV;
	} else if (voltage_uv == 4460000) {
		val = CTRL_A_VOUTL_4460MV;
	} else {
		dev_err(&chip->client->dev, "%s: voltage out of bounds: %d\n",
			__func__, voltage_uv);
		return -EINVAL;
	}

	rc = smb137c_masked_write_reg(chip, CTRL_A_REG, CTRL_A_VOUTL_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: voltage=%d uV\n", __func__,
		       voltage_uv);

	return rc;
}

static int charging_timeout[] = {
	 382,
	 764,
	 1527,
};

static int smb137c_set_charging_timeout(struct smb137c_chip *chip, int timeout)
{
	int timeout_chosen = 0;
	u8 val = 3 << SAFETY_TIMER_CHG_TIMEOUT_SHIFT;
	int rc, i;

	for (i = ARRAY_SIZE(charging_timeout) - 1; i >= 0; i--) {
		if (timeout >= charging_timeout[i]) {
			val = i << SAFETY_TIMER_CHG_TIMEOUT_SHIFT;
			timeout_chosen = charging_timeout[i];
			break;
		}
	}

	rc = smb137c_masked_write_reg(chip, SAFETY_TIMER_REG,
			SAFETY_TIMER_CHG_TIMEOUT_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: timeout=%d min\n", __func__,
			timeout_chosen);

	return rc;
}

static int pre_charge_timeout[] = {
	 48,
	 95,
	 191,
};

static int smb137c_set_pre_charge_timeout(struct smb137c_chip *chip,
					int timeout)
{
	int timeout_chosen = 0;
	u8 val = 3 << SAFETY_TIMER_PRE_CHG_TIME_SHIFT;
	int rc, i;

	for (i = ARRAY_SIZE(pre_charge_timeout) - 1; i >= 0; i--) {
		if (timeout >= pre_charge_timeout[i]) {
			val = i << SAFETY_TIMER_PRE_CHG_TIME_SHIFT;
			timeout_chosen = pre_charge_timeout[i];
			break;
		}
	}

	rc = smb137c_masked_write_reg(chip, SAFETY_TIMER_REG,
			SAFETY_TIMER_PRE_CHG_TIME_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: timeout=%d min\n", __func__,
			timeout_chosen);

	return rc;
}

static int thermistor_current[] = {
	100,
	40,
	20,
	10,
};

static int smb137c_set_thermistor_current(struct smb137c_chip *chip,
					int current_ua)
{
	bool found = false;
	u8 val = 0;
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(thermistor_current); i++) {
		if (current_ua == thermistor_current[i]) {
			found = true;
			val = i << TEMP_MON_THERM_CURRENT_SHIFT;
		}
	}

	if (!found) {
		dev_err(&chip->client->dev, "%s: current out of bounds: %d\n",
			__func__, current_ua);
		return -EINVAL;
	}

	rc = smb137c_masked_write_reg(chip, TEMP_MON_REG,
			TEMP_MON_THERM_CURRENT_MASK, val);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: current=%d uA\n", __func__,
			current_ua);

	return 0;
}

static int smb137c_set_temperature_low_limit(struct smb137c_chip *chip,
					int value)
{
	int rc;

	if (value < 0 || value > 7) {
		dev_err(&chip->client->dev, "%s: temperature value out of bounds: %d\n",
			__func__, value);
		return -EINVAL;
	}

	rc = smb137c_masked_write_reg(chip, TEMP_MON_REG,
		TEMP_MON_TEMP_LOW_MASK, value << TEMP_MON_TEMP_LOW_SHIFT);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: temperature value=%d\n",
			__func__, value);

	return rc;
}

static int smb137c_set_temperature_high_limit(struct smb137c_chip *chip,
					int value)
{
	int rc;

	if (value < 0 || value > 7) {
		dev_err(&chip->client->dev, "%s: temperature value out of bounds: %d\n",
			__func__, value);
		return -EINVAL;
	}

	rc = smb137c_masked_write_reg(chip, TEMP_MON_REG,
		TEMP_MON_TEMP_HIGH_MASK, value << TEMP_MON_TEMP_HIGH_SHIFT);

	if (!rc)
		dev_dbg(&chip->client->dev, "%s: temperature value=%d\n",
			__func__, value);

	return rc;
}

static int charge_status_type_map[] = {
	[CHARGE_STAT_NO_CHG]	= POWER_SUPPLY_CHARGE_TYPE_NONE,
	[CHARGE_STAT_PRE_CHG]	= POWER_SUPPLY_CHARGE_TYPE_TRICKLE,
	[CHARGE_STAT_FAST_CHG]	= POWER_SUPPLY_CHARGE_TYPE_FAST,
	[CHARGE_STAT_TAPER_CHG]	= POWER_SUPPLY_CHARGE_TYPE_FAST,
};

static const char * const charge_status_name[] = {
	[CHARGE_STAT_NO_CHG]	= "none",
	[CHARGE_STAT_PRE_CHG]	= "pre-charge",
	[CHARGE_STAT_FAST_CHG]	= "fast-charge",
	[CHARGE_STAT_TAPER_CHG]	= "taper-charge",
};

static int smb137c_get_property_status(struct smb137c_chip *chip)
{
	int status = POWER_SUPPLY_STATUS_DISCHARGING;
	enum smb137c_charge_status charging_status;
	bool charging_enabled;
	bool charging_error;
	int rc;
	u8 val;

	rc = smb137c_read_reg(chip, STAT_C_REG, &val);
	if (rc)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	charging_enabled = val & STAT_C_CHG_ENABLED;
	charging_error = val & STAT_C_CHG_ERROR;
	charging_status = (val & STAT_C_CHG_STAT_MASK) >> STAT_C_CHG_STAT_SHIFT;

	if (charging_enabled && !charging_error
	    && charging_status != CHARGE_STAT_NO_CHG)
		status = POWER_SUPPLY_STATUS_CHARGING;

	dev_dbg(&chip->client->dev, "%s: status=%s\n", __func__,
		(status == POWER_SUPPLY_STATUS_CHARGING ? "charging"
			: "discharging"));

	return status;
}

static int smb137c_get_property_battery_present(struct smb137c_chip *chip)
{
	int rc;
	u8 val;

	rc = smb137c_read_reg(chip, IRQ_STAT_B_REG, &val);
	if (rc || (val & IRQ_STAT_B_BATT_MISSING))
		return 0;

	/* Treat battery voltage less than 2.1 V as battery not present. */
	rc = smb137c_read_reg(chip, STAT_C_REG, &val);
	if (rc || (val & STAT_C_VBATT_LEVEL_BELOW_2P1V))
		return 0;

	return 1;
}

static int smb137c_get_property_battery_health(struct smb137c_chip *chip)
{
	int rc;
	u8 val;

	/* The health of a disconnected battery is unknown. */
	if (!smb137c_get_property_battery_present(chip))
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	rc = smb137c_read_reg(chip, IRQ_STAT_B_REG, &val);
	if (rc)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (val & IRQ_STAT_B_BATT_OVERVOLT)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else if (val & IRQ_STAT_B_BATT_UNDERVOLT)
		return POWER_SUPPLY_HEALTH_DEAD;

	rc = smb137c_read_reg(chip, IRQ_STAT_A_REG, &val);
	if (rc)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (val & IRQ_STAT_A_BATT_HOT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (val & IRQ_STAT_A_BATT_COLD)
		return POWER_SUPPLY_HEALTH_COLD;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static int smb137c_get_property_charge_type(struct smb137c_chip *chip)
{
	enum smb137c_charge_status status;
	int charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	bool charging_enabled;
	bool charging_error;
	int rc;
	u8 val;

	rc = smb137c_read_reg(chip, STAT_C_REG, &val);
	if (rc)
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	charging_enabled = val & STAT_C_CHG_ENABLED;
	charging_error = val & STAT_C_CHG_ERROR;
	status = (val & STAT_C_CHG_STAT_MASK) >> STAT_C_CHG_STAT_SHIFT;

	if (!charging_enabled) {
		dev_dbg(&chip->client->dev, "%s: not charging\n", __func__);
	} else if (charging_error) {
		dev_warn(&chip->client->dev, "%s: charger error detected\n",
			__func__);
	} else {
		charge_type = charge_status_type_map[status];
	}

	dev_dbg(&chip->client->dev, "%s: charging status=%s\n", __func__,
		charge_status_name[status]);

	return charge_type;
}

static enum power_supply_property smb137c_power_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int smb137c_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}

static int smb137c_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct smb137c_chip *chip = container_of(psy, struct smb137c_chip, psy);

	mutex_lock(&chip->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (val->intval)
			smb137c_enable_charging(chip);
		else
			smb137c_disable_charging(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		smb137c_set_charge_current_limit(chip, val->intval);
		break;
	default:
		mutex_unlock(&chip->lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->lock);

	power_supply_changed(&chip->psy);
	return 0;
}

static int smb137c_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct smb137c_chip *chip = container_of(psy, struct smb137c_chip, psy);

	mutex_lock(&chip->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb137c_get_property_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb137c_get_property_battery_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb137c_get_property_battery_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->charging_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb137c_get_property_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->charge_current_limit_ua;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "SMB137C";
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Summit Microelectronics";
		break;
	default:
		mutex_unlock(&chip->lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->lock);

	return 0;
}

static void smb137c_external_power_changed(struct power_supply *psy)
{
	struct smb137c_chip *chip = container_of(psy, struct smb137c_chip, psy);
	union power_supply_propval prop = {0,};
	int scope = POWER_SUPPLY_SCOPE_DEVICE;
	int current_limit = 0;
	int online = 0;
	int rc;

	mutex_lock(&chip->lock);
	dev_dbg(&chip->client->dev, "%s: start\n", __func__);

	rc = chip->usb_psy->get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc)
		dev_err(&chip->client->dev, "%s: could not read USB online property, rc=%d\n",
			__func__, rc);
	else
		online = prop.intval;

	rc = chip->usb_psy->get_property(chip->usb_psy, POWER_SUPPLY_PROP_SCOPE,
					&prop);
	if (rc)
		dev_err(&chip->client->dev, "%s: could not read USB scope property, rc=%d\n",
			__func__, rc);
	else
		scope = prop.intval;

	rc = chip->usb_psy->get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc)
		dev_err(&chip->client->dev, "%s: could not read USB current_max property, rc=%d\n",
			__func__, rc);
	else
		current_limit = prop.intval;

	if (scope == POWER_SUPPLY_SCOPE_SYSTEM) {
		/* USB host mode */
		smb137c_disable_charging(chip);
		smb137c_enable_otg_mode(chip);
	} else if (online) {
		/* USB online in device mode */
		smb137c_set_usb_input_current_limit(chip, current_limit);
		smb137c_enable_charging(chip);
		smb137c_disable_otg_mode(chip);
	} else {
		/* USB offline */
		smb137c_disable_charging(chip);
		smb137c_disable_otg_mode(chip);
		smb137c_set_usb_input_current_limit(chip,
			min(current_limit, USB_MIN_CURRENT_UA));
	}

	dev_dbg(&chip->client->dev, "%s: end\n", __func__);
	mutex_unlock(&chip->lock);

	power_supply_changed(&chip->psy);
}

static int __devinit smb137c_set_register_defaults(struct smb137c_chip *chip)
{
	int rc;
	u8 val, mask;

	/* Allow volatile register writes. */
	rc = smb137c_masked_write_reg(chip, CMD_A_REG,
			CMD_A_VOLATILE_WRITE_MASK, CMD_A_VOLATILE_WRITE_ALLOW);
	if (rc)
		return rc;

	/* Do not reset register values on USB reinsertion. */
	rc = smb137c_masked_write_reg(chip, SAFETY_TIMER_REG,
			SAFETY_TIMER_RELOAD_MASK, SAFETY_TIMER_RELOAD_DISABLED);
	if (rc)
		return rc;

	/* Set various default control parameters. */
	val = PIN_CTRL_DEAD_BATT_CHG_ENABLED | PIN_CTRL_OTG
		| PIN_CTRL_USB_CUR_LIMIT_REG | PIN_CTRL_CHG_EN_REG_LOW
		| PIN_CTRL_OTG_CTRL_REG;
	mask = PIN_CTRL_DEAD_BATT_CHG_MASK | PIN_CTRL_OTG_LBR_MASK
		| PIN_CTRL_USB_CUR_LIMIT_MASK | PIN_CTRL_CHG_EN_MASK
		| PIN_CTRL_OTG_CTRL_MASK;
	rc = smb137c_masked_write_reg(chip, PIN_CTRL_REG, mask, val);
	if (rc)
		return rc;

	/* Disable charging, disable OTG mode, and allow fast-charge current. */
	val = CMD_A_CHARGING_DISABLED | CMD_A_OTG_DISABLED
		| CMD_A_FAST_CHG_ALLOW;
	mask = CMD_A_CHARGING_MASK | CMD_A_OTG_MASK | CMD_A_FAST_CHG_MASK;
	rc = smb137c_masked_write_reg(chip, CMD_A_REG, mask, val);
	if (rc)
		return rc;

	/* Enable auto recharging and full-time THERM monitor. */
	val = CTRL_A_AUTO_RECHARGE_ENABLED | CTRL_A_THERM_MONITOR_ENABLED;
	mask = CTRL_A_AUTO_RECHARGE_MASK | CTRL_A_THERM_MONITOR_MASK;
	rc = smb137c_masked_write_reg(chip, CTRL_A_REG, mask, val);
	if (rc)
		return rc;

	/* Use register value instead of pin to control USB suspend. */
	rc = smb137c_masked_write_reg(chip, VAR_FUNC_REG,
		VAR_FUNC_USB_SUSPEND_CTRL_MASK, VAR_FUNC_USB_SUSPEND_CTRL_REG);
	if (rc)
		return rc;

	return rc;
}

static int __devinit smb137c_apply_dt_configs(struct smb137c_chip *chip)
{
	struct device *dev = &chip->client->dev;
	struct device_node *node = chip->client->dev.of_node;
	int ret, current_ma, voltage_mv, timeout, value;
	int rc = 0;

	/*
	 * All device tree parameters are optional so it is ok if read calls
	 * fail.
	 */
	ret = of_property_read_u32(node, "summit,chg-current-ma", &current_ma);
	if (ret == 0) {
		rc = smb137c_set_charge_current_limit(chip, current_ma * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set charge current, rc=%d\n",
				__func__, rc);
			return rc;
		}
	} else {
		chip->charge_current_limit_ua
			= smb137c_get_charge_current_limit(chip);
		rc = chip->charge_current_limit_ua;
		if (rc < 0) {
			dev_err(dev, "%s: Failed to get charge current, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,term-current-ma", &current_ma);
	if (ret == 0) {
		rc = smb137c_set_term_current(chip, current_ma * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set termination current, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,pre-chg-current-ma",
					&current_ma);
	if (ret == 0) {
		rc = smb137c_set_pre_charge_current_limit(chip,
				current_ma * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set pre-charge current limit, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,float-voltage-mv",
					&voltage_mv);
	if (ret == 0) {
		rc = smb137c_set_float_voltage(chip, voltage_mv * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set float voltage, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,thresh-voltage-mv",
					&voltage_mv);
	if (ret == 0) {
		rc = smb137c_set_pre_charge_threshold_voltage(chip,
				voltage_mv * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set fast-charge threshold voltage, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,recharge-thresh-mv",
					&voltage_mv);
	if (ret == 0) {
		rc = smb137c_set_recharge_threshold_voltage(chip,
				voltage_mv * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set recharge threshold voltage, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,system-voltage-mv",
					&voltage_mv);
	if (ret == 0) {
		rc = smb137c_set_system_voltage(chip, voltage_mv * 1000);
		if (rc) {
			dev_err(dev, "%s: Failed to set system voltage, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,charging-timeout", &timeout);
	if (ret == 0) {
		rc = smb137c_set_charging_timeout(chip, timeout);
		if (rc) {
			dev_err(dev, "%s: Failed to set charging timeout, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,pre-charge-timeout", &timeout);
	if (ret == 0) {
		rc = smb137c_set_pre_charge_timeout(chip, timeout);
		if (rc) {
			dev_err(dev, "%s: Failed to set pre-charge timeout, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,therm-current-ua", &value);
	if (ret == 0) {
		rc = smb137c_set_thermistor_current(chip, value);
		if (rc) {
			dev_err(dev, "%s: Failed to set thermistor current, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,temperature-min", &value);
	if (ret == 0) {
		rc = smb137c_set_temperature_low_limit(chip, value);
		if (rc) {
			dev_err(dev, "%s: Failed to set low temperature limit, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	ret = of_property_read_u32(node, "summit,temperature-max", &value);
	if (ret == 0) {
		rc = smb137c_set_temperature_high_limit(chip, value);
		if (rc) {
			dev_err(dev, "%s: Failed to set high temperature limit, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	return rc;
}

static int __devinit smb137c_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct smb137c_chip *chip;
	struct device *dev = &client->dev;
	struct device_node *node = client->dev.of_node;
	int rc = 0;
	int gui_rev, silicon_rev;
	u8 dev_id;

	if (!node) {
		dev_err(dev, "%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: SMBUS_BYTE_DATA unsupported\n", __func__);
		return -EIO;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(dev, "%s: devm_kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&chip->lock);
	chip->client = client;
	i2c_set_clientdata(client, chip);

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		dev_dbg(dev, "%s: USB supply not found; deferring charger probe\n",
			__func__);
		return -EPROBE_DEFER;
	}

	rc = smb137c_read_reg(chip, DEV_ID_REG, &dev_id);
	if (rc)
		return rc;

	if ((dev_id & DEV_ID_PART_MASK) != DEV_ID_PART_SMB137C) {
		dev_err(dev, "%s: invalid device ID=0x%02X\n", __func__,
			dev_id);
		return -ENODEV;
	}

	gui_rev = (dev_id & DEV_ID_GUI_REV_MASK) >> DEV_ID_GUI_REV_SHIFT;
	silicon_rev = (dev_id & DEV_ID_SILICON_REV_MASK)
			>> DEV_ID_SILICON_REV_SHIFT;

	rc = smb137c_set_register_defaults(chip);
	if (rc)
		return rc;

	rc = smb137c_apply_dt_configs(chip);
	if (rc)
		return rc;

	chip->psy.name			 = "battery";
	chip->psy.type			 = POWER_SUPPLY_TYPE_BATTERY;
	chip->psy.properties		 = smb137c_power_properties;
	chip->psy.num_properties	 = ARRAY_SIZE(smb137c_power_properties);
	chip->psy.get_property		 = smb137c_power_get_property;
	chip->psy.set_property		 = smb137c_power_set_property;
	chip->psy.property_is_writeable  = smb137c_property_is_writeable;
	chip->psy.external_power_changed = smb137c_external_power_changed;

	rc = power_supply_register(dev, &chip->psy);
	if (rc < 0) {
		dev_err(dev, "%s: power_supply_register failed, rc=%d\n",
						__func__, rc);
		return rc;
	}

	smb137c_external_power_changed(&chip->psy);

	dev_info(dev, "%s: SMB137C charger probed successfully, gui_rev=%d, silicon_rev=%d\n",
		__func__, gui_rev, silicon_rev);

	return rc;
}

static int __devexit smb137c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id smb137c_id[] = {
	{ .name = "smb137c", },
	{},
};
MODULE_DEVICE_TABLE(i2c, smb137c_id);

static const struct of_device_id smb137c_match[] = {
	{ .compatible = "summit,smb137c", },
	{ },
};

static struct i2c_driver smb137c_driver = {
	.driver	= {
		.name		= "smb137c",
		.owner		= THIS_MODULE,
		.of_match_table	= smb137c_match,
	},
	.probe		= smb137c_probe,
	.remove		= __devexit_p(smb137c_remove),
	.id_table	= smb137c_id,
};

static int __init smb137c_init(void)
{
	return i2c_add_driver(&smb137c_driver);
}
module_init(smb137c_init);

static void __exit smb137c_exit(void)
{
	return i2c_del_driver(&smb137c_driver);
}
module_exit(smb137c_exit);

MODULE_DESCRIPTION("SMB137C Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb137c");
