/* Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "SMB349 %s: " fmt, __func__
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

/* Config/Control registers */
#define CHG_CURRENT_CTRL_REG		0x0
#define CHG_OTH_CURRENT_CTRL_REG	0x1
#define VARIOUS_FUNC_REG		0x2
#define CHG_CTRL_REG			0x4
#define CHG_PIN_EN_CTRL_REG		0x6
#define THERM_A_CTRL_REG		0x7
#define FAULT_INT_REG			0xC
#define STATUS_INT_REG			0xD

/* Command registers */
#define CMD_A_REG			0x30
#define CMD_B_REG			0x31

/* IRQ status registers */
#define IRQ_A_REG			0x35
#define IRQ_B_REG			0x36
#define IRQ_C_REG			0x37
#define IRQ_D_REG			0x38
#define IRQ_E_REG			0x39
#define IRQ_F_REG			0x3A

/* Status registers */
#define STATUS_C_REG			0x3D
#define STATUS_D_REG			0x3E
#define STATUS_E_REG			0x3F

/* Config bits */
#define CMD_A_CHG_ENABLE_BIT			BIT(1)
#define CMD_A_VOLATILE_W_PERM_BIT		BIT(7)
#define CMD_A_CHG_SUSP_EN_BIT			BIT(2)
#define CMD_A_CHG_SUSP_EN_MASK			BIT(2)
#define CMD_A_OTG_ENABLE_BIT			BIT(4)
#define CMD_A_OTG_ENABLE_MASK			BIT(4)
#define CMD_B_CHG_HC_ENABLE_BIT			BIT(0)
#define CMD_B_CHG_USB3_ENABLE_BIT		BIT(2)
#define CMD_B_CHG_USB_500_900_ENABLE_BIT	BIT(1)
#define CHG_CTRL_AUTO_RECHARGE_ENABLE_BIT	0x0
#define CHG_CTRL_CURR_TERM_END_CHG_BIT		0x0
#define CHG_CTRL_BATT_MISSING_DET_THERM_IO	(BIT(5) | BIT(4))
#define CHG_CTRL_AUTO_RECHARGE_MASK		BIT(7)
#define CHG_CTRL_CURR_TERM_END_MSAK		BIT(6)
#define CHG_CTRL_BATT_MISSING_DET_MASK		(BIT(5) | BIT(4))
#define CHG_OTH_CTRL_USB_2_3_REG_CTRL_BIT	0
#define CHG_OTH_CTRL_USB_2_3_PIN_REG_MASK	BIT(1)
#define CHG_PIN_CTRL_USBCS_REG_BIT		0x0
#define CHG_PIN_CTRL_CHG_EN_LOW_PIN_BIT		(BIT(5) | BIT(6))
#define CHG_PIN_CTRL_CHG_EN_LOW_REG_BIT		0x0
#define CHG_PIN_CTRL_CHG_EN_MASK		(BIT(5) | BIT(6))
#define CHG_PIN_CTRL_USBCS_REG_MASK		BIT(4)
#define CHG_PIN_CTRL_APSD_IRQ_BIT		BIT(1)
#define CHG_PIN_CTRL_APSD_IRQ_MASK		BIT(1)
#define CHG_PIN_CTRL_CHG_ERR_IRQ_BIT		BIT(2)
#define CHG_PIN_CTRL_CHG_ERR_IRQ_MASK		BIT(2)
#define VARIOUS_FUNC_USB_SUSP_EN_REG_BIT	BIT(7)
#define VARIOUS_FUNC_USB_SUSP_MASK		BIT(7)
#define VARIOUS_FUNC_APSD_EN_BIT		BIT(2)
#define VARIOUS_FUNC_APSD_MASK			BIT(2)
#define FAULT_INT_HOT_COLD_HARD_BIT		BIT(7)
#define FAULT_INT_HOT_COLD_SOFT_BIT		BIT(6)
#define FAULT_INT_INPUT_UV_BIT			BIT(2)
#define FAULT_INT_AICL_COMPLETE_BIT		BIT(1)
#define STATUS_INT_CHG_TIMEOUT_BIT		BIT(7)
#define STATUS_INT_OTG_DETECT_BIT		BIT(6)
#define STATUS_INT_BATT_OV_BIT			BIT(5)
#define STATUS_INT_TERM_TAPER_BIT		BIT(4)
#define STATUS_INT_FAST_CHG_BIT			BIT(3)
#define STATUS_INT_MISSING_BATT_BIT		BIT(1)
#define STATUS_INT_LOW_BATT_BIT			BIT(0)
#define THERM_A_THERM_MONITOR_EN_BIT		0x0
#define THERM_A_THERM_MONITOR_EN_MASK		BIT(4)

/* IRQ status bits */
#define IRQ_A_TEMP_HARD_LIMIT			(BIT(6) | BIT(4))
#define IRQ_A_TEMP_SOFT_LIMIT			(BIT(2) | BIT(0))
#define IRQ_B_BATT_MISSING_BIT			BIT(4)
#define IRQ_B_BATT_LOW_BIT			BIT(2)
#define IRQ_B_BATT_OV_BIT			BIT(6)
#define IRQ_B_PRE_FAST_CHG_BIT			BIT(0)
#define IRQ_C_TERM_TAPER_CHG_BIT		(BIT(0) | BIT(2))
#define IRQ_C_INT_OVER_TEMP_BIT			BIT(6)
#define IRQ_D_CHG_TIMEOUT_BIT			(BIT(0) | BIT(2))
#define IRQ_D_AICL_DONE_BIT			BIT(4)
#define IRQ_D_APSD_COMPLETE			BIT(6)
#define IRQ_E_INPUT_UV_BIT			BIT(0)
#define IRQ_E_INPUT_OV_BIT			BIT(2)
#define IRQ_E_AFVC_ACTIVE                       BIT(4)
#define IRQ_F_OTG_VALID_BIT			BIT(2)
#define IRQ_F_OTG_BATT_FAIL_BIT			BIT(4)
#define IRQ_F_OTG_OC_BIT			BIT(6)
#define IRQ_F_POWER_OK				BIT(0)

/* Status  bits */
#define STATUS_C_CHARGING_MASK			(BIT(1) | BIT(2))
#define STATUS_C_FAST_CHARGING			BIT(2)
#define STATUS_C_PRE_CHARGING			BIT(1)
#define STATUS_C_TAPER_CHARGING			(BIT(2) | BIT(1))
#define STATUS_C_CHG_ERR_STATUS_BIT		BIT(6)
#define STATUS_C_CHG_ENABLE_STATUS_BIT		BIT(0)
#define STATUS_D_PORT_OTHER			BIT(0)
#define STATUS_D_PORT_SDP			BIT(1)
#define STATUS_D_PORT_DCP			BIT(2)
#define STATUS_D_PORT_CDP			BIT(3)
#define STATUS_D_PORT_ACA_A			BIT(4)
#define STATUS_D_PORT_ACA_B			BIT(5)
#define STATUS_D_PORT_ACA_C			BIT(6)
#define STATUS_D_PORT_ACA_DOCK			BIT(7)

/* constants */
#define USB2_MIN_CURRENT_MA		100
#define USB2_MAX_CURRENT_MA		500
#define USB3_MAX_CURRENT_MA		900
#define AC_CHG_CURRENT_MASK		0x0F
#define SMB349_IRQ_REG_COUNT		6
#define SMB349_FAST_CHG_MIN_MA		1000
#define SMB349_FAST_CHG_STEP_MA		200
#define SMB349_FAST_CHG_MAX_MA		4000
#define SMB349_FAST_CHG_SHIFT		4
#define SMB_FAST_CHG_CURRENT_MASK	0xF0
#define SMB349_DEFAULT_BATT_CAPACITY	50

struct smb349_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct smb349_charger {
	struct i2c_client	*client;
	struct device		*dev;

	int			chg_valid_gpio;
	int			chg_valid_act_low;
	int			chg_present;
	bool			chg_autonomous_mode;
	bool			disable_apsd;
	bool			enable_chg_type_detection;
	bool			battery_missing;
	bool			supply_update;
	const char		*bms_psy_name;

	int			charging_disabled;
	int			fastchg_current_max_ma;

	struct power_supply	*usb_psy;
	struct power_supply	*bms_psy;
	struct power_supply	batt_psy;

	struct smb349_regulator	otg_vreg;

	struct dentry		*debug_root;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			prev_val;
	u8			val;
	int (*smb_irq[4])(struct smb349_charger *smb, u8 status);
};

static struct irq_handler_info handlers[SMB349_IRQ_REG_COUNT];

struct chg_current_map {
	int	chg_current_ma;
	u8	value;
};

static int chg_current[] = {
	500, 900, 1000, 1100, 1200, 1300, 1500, 1600,
	1700, 1800, 2000, 2200, 2400, 2500, 3000, 3500,
};

static int smb349_read_reg(struct smb349_charger *chip, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_write_reg(struct smb349_charger *chip, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int smb349_masked_write(struct smb349_charger *chip, int reg,
							u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb349_read_reg(chip, reg, &temp);
	if (rc) {
		dev_err(chip->dev,
			"smb349_read_reg Failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb349_write_reg(chip, reg, temp);
	if (rc) {
		dev_err(chip->dev,
			"smb349_write Failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static int smb349_enable_volatile_writes(struct smb349_charger *chip)
{
	int rc;

	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_VOLATILE_W_PERM_BIT,
						CMD_A_VOLATILE_W_PERM_BIT);
	if (rc)
		dev_err(chip->dev, "Couldn't write VOLATILE_W_PERM_BIT rc=%d\n",
				rc);

	return rc;
}

static int smb349_fastchg_current_set(struct smb349_charger *chip)
{
	u8 temp;

	if ((chip->fastchg_current_max_ma < SMB349_FAST_CHG_MIN_MA) ||
		(chip->fastchg_current_max_ma >  SMB349_FAST_CHG_MAX_MA)) {
		dev_dbg(chip->dev, "bad fastchg current mA=%d asked to set\n",
					chip->fastchg_current_max_ma);
		return -EINVAL;
	}

	temp = (chip->fastchg_current_max_ma - SMB349_FAST_CHG_MIN_MA)
			/ SMB349_FAST_CHG_STEP_MA;

	temp = temp << SMB349_FAST_CHG_SHIFT;
	dev_dbg(chip->dev, "fastchg limit=%d setting %02x\n",
			chip->fastchg_current_max_ma, temp);

	return smb349_masked_write(chip, CHG_CURRENT_CTRL_REG,
				SMB_FAST_CHG_CURRENT_MASK, temp);
}

static int smb349_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb349_charger *chip = rdev_get_drvdata(rdev);

	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_OTG_ENABLE_BIT,
							CMD_A_OTG_ENABLE_BIT);
	if (rc)
		dev_err(chip->dev, "Couldn't enable  OTG mode rc=%d\n", rc);
	return rc;
}

static int smb349_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb349_charger *chip = rdev_get_drvdata(rdev);

	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_OTG_ENABLE_BIT, 0);
	if (rc)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
	return rc;
}

static int smb349_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smb349_charger *chip = rdev_get_drvdata(rdev);

	rc = smb349_read_reg(chip, CMD_A_REG, &reg);
	if (rc) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return  (reg & CMD_A_OTG_ENABLE_BIT) ? 1 : 0;
}

struct regulator_ops smb349_chg_otg_reg_ops = {
	.enable		= smb349_chg_otg_regulator_enable,
	.disable	= smb349_chg_otg_regulator_disable,
	.is_enabled	= smb349_chg_otg_regulator_is_enable,
};

static int smb349_regulator_init(struct smb349_charger *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(chip->dev, chip->dev->of_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &smb349_chg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = chip->dev->of_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
						&chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			rc = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"OTG reg failed, rc=%d\n", rc);
		}
	}

	return rc;
}

static int smb349_hw_init(struct smb349_charger *chip)
{
	int rc;
	u8 reg = 0, mask = 0;

	/*
	 * If the charger is pre-configured for autonomous operation,
	 * do not apply additonal settings
	 */
	if (chip->chg_autonomous_mode) {
		dev_dbg(chip->dev, "Charger configured for autonomous mode\n");
		return 0;
	}

	rc = smb349_enable_volatile_writes(chip);
	if (rc) {
		dev_err(chip->dev, "Couldn't configure volatile writes rc=%d\n",
				rc);
		return rc;
	}

	/* setup defaults for CHG_CNTRL_REG */
	reg = CHG_CTRL_AUTO_RECHARGE_ENABLE_BIT |
		CHG_CTRL_CURR_TERM_END_CHG_BIT |
		CHG_CTRL_BATT_MISSING_DET_THERM_IO;
	mask = CHG_CTRL_AUTO_RECHARGE_MASK | CHG_CTRL_CURR_TERM_END_MSAK |
						CHG_CTRL_BATT_MISSING_DET_MASK;
	rc = smb349_masked_write(chip, CHG_CTRL_REG, mask, reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't set CHG_CTRL_REG rc=%d\n", rc);
		return rc;
	}
	/* setup defaults for PIN_CTRL_REG */
	reg = CHG_PIN_CTRL_USBCS_REG_BIT | CHG_PIN_CTRL_CHG_EN_LOW_REG_BIT |
		CHG_PIN_CTRL_APSD_IRQ_BIT | CHG_PIN_CTRL_CHG_ERR_IRQ_BIT;
	mask = CHG_PIN_CTRL_CHG_EN_MASK | CHG_PIN_CTRL_USBCS_REG_MASK |
		CHG_PIN_CTRL_APSD_IRQ_MASK | CHG_PIN_CTRL_CHG_ERR_IRQ_MASK;
	rc = smb349_masked_write(chip, CHG_PIN_EN_CTRL_REG, mask, reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't set CHG_PIN_EN_CTRL_REG rc=%d\n",
				rc);
		return rc;
	}
	/* setup USB 2.0/3.0 detection mechanism */
	rc = smb349_masked_write(chip, CHG_OTH_CURRENT_CTRL_REG,
					CHG_OTH_CTRL_USB_2_3_PIN_REG_MASK,
					CHG_OTH_CTRL_USB_2_3_REG_CTRL_BIT);
	if (rc) {
		dev_err(chip->dev, "Couldn't set USB_2_3_REG_CTRL_BIT rc=%d\n",
				rc);
		return rc;
	}
	/* setup USB suspend and APSD  */
	reg = VARIOUS_FUNC_USB_SUSP_EN_REG_BIT;
	if (!chip->disable_apsd)
		reg |= VARIOUS_FUNC_APSD_EN_BIT;
	mask = VARIOUS_FUNC_USB_SUSP_MASK | VARIOUS_FUNC_APSD_MASK;
	rc = smb349_masked_write(chip, VARIOUS_FUNC_REG, mask, reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't set VARIOUS_FUNC_REG rc=%d\n",
				rc);
		return rc;
	}
	/* Fault and Status IRQ configuration */
	reg = FAULT_INT_HOT_COLD_HARD_BIT | FAULT_INT_HOT_COLD_SOFT_BIT
		| FAULT_INT_INPUT_UV_BIT | FAULT_INT_AICL_COMPLETE_BIT;
	rc = smb349_write_reg(chip, FAULT_INT_REG, reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't set FAULT_INT_REG rc=%d\n", rc);
		return rc;
	}
	reg = STATUS_INT_CHG_TIMEOUT_BIT | STATUS_INT_OTG_DETECT_BIT |
		STATUS_INT_BATT_OV_BIT | STATUS_INT_TERM_TAPER_BIT |
		STATUS_INT_FAST_CHG_BIT | STATUS_INT_LOW_BATT_BIT |
		STATUS_INT_MISSING_BATT_BIT;
	rc = smb349_write_reg(chip, STATUS_INT_REG, reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't set STATUS_INT_REG rc=%d\n", rc);
		return rc;
	}
	/* setup THERM Monitor */
	rc = smb349_masked_write(chip, THERM_A_CTRL_REG,
		THERM_A_THERM_MONITOR_EN_MASK, THERM_A_THERM_MONITOR_EN_BIT);
	if (rc) {
		dev_err(chip->dev, "Couldn't set THERM_A_CTRL_REG rc=%d\n",
				rc);
		return rc;
	}
	/* set the fast charge current limit */
	rc = smb349_fastchg_current_set(chip);
	if (rc) {
		dev_err(chip->dev, "Couldn't set fastchg current rc=%d\n", rc);
		return rc;
	}
	/* enable/disable charging */
	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_CHG_ENABLE_BIT,
			chip->charging_disabled ? 0 : CMD_A_CHG_ENABLE_BIT);
	if (rc) {
		dev_err(chip->dev, "Unable to %s charging. rc=%d\n",
			chip->charging_disabled ? "disable" : "enable", rc);
	}

	return rc;
}

static enum power_supply_property smb349_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb349_get_prop_batt_status(struct smb349_charger *chip)
{
	int rc;
	int status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0;

	rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if ((reg & (STATUS_C_CHARGING_MASK |
			STATUS_C_CHG_ENABLE_STATUS_BIT)) &&
			!(reg & STATUS_C_CHG_ERR_STATUS_BIT))
		status = POWER_SUPPLY_STATUS_CHARGING;

	dev_dbg(chip->dev, "%s: STATUS_C_REG=%x\n", __func__, reg);

	return status;
}

static int smb349_get_prop_batt_present(struct smb349_charger *chip)
{
	return !chip->battery_missing;
}

static int smb349_get_prop_batt_capacity(struct smb349_charger *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	}

	return SMB349_DEFAULT_BATT_CAPACITY;
}

static int smb349_get_prop_charge_type(struct smb349_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	dev_dbg(chip->dev, "%s: STATUS_C_REG=%x\n", __func__, reg);

	reg &= STATUS_C_CHARGING_MASK;

	if ((reg == STATUS_C_FAST_CHARGING) || (reg == STATUS_C_TAPER_CHARGING))
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (reg == STATUS_C_PRE_CHARGING)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb349_get_charging_status(struct smb349_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb349_read_reg(chip, STATUS_C_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n", rc);
		return 0;
	}

	return (reg & STATUS_C_CHG_ENABLE_STATUS_BIT) ? 1 : 0;
}

static int smb349_charging(struct smb349_charger *chip, int enable)
{
	int rc = 0;

	if (chip->chg_autonomous_mode) {
		dev_dbg(chip->dev, "%s: Charger in autonmous mode\n", __func__);
		return 0;
	}

	dev_dbg(chip->dev, "%s: charging enable = %d\n", __func__, enable);

	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_CHG_ENABLE_BIT,
					enable ? CMD_A_CHG_ENABLE_BIT : 0);
	if (rc)
		dev_err(chip->dev, "Couldn't enable = %d rc = %d\n",
				enable, rc);
	else
		chip->charging_disabled = !enable;

	return rc;
}

static int smb349_set_usb_chg_current(struct smb349_charger *chip,
							int current_ma)
{
	int i, rc = 0;
	u8 reg = 0, mask = 0;

	dev_dbg(chip->dev, "%s: USB current_ma = %d\n", __func__, current_ma);

	if (chip->chg_autonomous_mode) {
		dev_dbg(chip->dev, "%s: Charger in autonmous mode\n", __func__);
		return 0;
	}

	/* Only set suspend bit when chg present and current_ma = 2 */
	if (current_ma == 2 && chip->chg_present) {
		rc = smb349_masked_write(chip, CMD_A_REG,
			CMD_A_CHG_SUSP_EN_MASK, CMD_A_CHG_SUSP_EN_BIT);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't suspend rc = %d\n", rc);

		return rc;
	}

	if (current_ma <= 2)
		current_ma = USB2_MIN_CURRENT_MA;

	if (current_ma == USB2_MIN_CURRENT_MA) {
		/* USB 2.0 - 100mA */
		reg &= ~CMD_B_CHG_USB3_ENABLE_BIT;
		reg &= ~CMD_B_CHG_USB_500_900_ENABLE_BIT;
	} else if (current_ma == USB2_MAX_CURRENT_MA) {
		/* USB 2.0 - 500mA */
		reg &= ~CMD_B_CHG_USB3_ENABLE_BIT;
		reg |= CMD_B_CHG_USB_500_900_ENABLE_BIT;
	} else if (current_ma == USB3_MAX_CURRENT_MA) {
		/* USB 3.0 - 900mA */
		reg |= CMD_B_CHG_USB3_ENABLE_BIT;
		reg |= CMD_B_CHG_USB_500_900_ENABLE_BIT;
	} else if (current_ma > USB2_MAX_CURRENT_MA) {
		/* HC mode  - if none of the above */
		reg |= CMD_B_CHG_HC_ENABLE_BIT;

		for (i = ARRAY_SIZE(chg_current) - 1; i >= 0; i--) {
			if (chg_current[i] <= current_ma)
				break;
		}
		if (i < 0) {
			dev_err(chip->dev, "Cannot find %dmA\n", current_ma);
			i = 0;
		}

		rc = smb349_masked_write(chip, CHG_CURRENT_CTRL_REG,
						AC_CHG_CURRENT_MASK, i);
		if (rc)
			dev_err(chip->dev, "Couldn't set input mA rc=%d\n", rc);
	}

	mask = CMD_B_CHG_HC_ENABLE_BIT | CMD_B_CHG_USB3_ENABLE_BIT |
				CMD_B_CHG_USB_500_900_ENABLE_BIT;
	rc = smb349_masked_write(chip, CMD_B_REG, mask, reg);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set charging mode rc = %d\n", rc);

	/* unset the susp bit here */
	rc = smb349_masked_write(chip, CMD_A_REG, CMD_A_CHG_SUSP_EN_MASK, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable suspend rc = %d\n", rc);

	return rc;
}

static int
smb349_batt_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		break;
	}

	return 0;
}

static int smb349_battery_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct smb349_charger *chip = container_of(psy,
				struct smb349_charger, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb349_charging(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb349_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb349_charger *chip = container_of(psy,
				struct smb349_charger, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb349_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb349_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smb349_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = smb349_get_charging_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb349_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "SMB349";
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define LAST_CNFG_REG	0x13
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smb349_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb349_charger *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cnfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_CMD_REG	0x30
#define LAST_CMD_REG	0x33
static int show_cmd_regs(struct seq_file *m, void *data)
{
	struct smb349_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cmd_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb349_charger *chip = inode->i_private;

	return single_open(file, show_cmd_regs, chip);
}

static const struct file_operations cmd_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cmd_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_STATUS_REG	0x35
#define LAST_STATUS_REG		0x3F
static int show_status_regs(struct seq_file *m, void *data)
{
	struct smb349_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int status_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb349_charger *chip = inode->i_private;

	return single_open(file, show_status_regs, chip);
}

static const struct file_operations status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= status_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef DEBUG
static void dump_regs(struct smb349_charger *chip)
{
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (rc)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (rc)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb349_read_reg(chip, addr, &reg);
		if (rc)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}
}
#else
static void dump_regs(struct smb349_charger *chip)
{
}
#endif

static int apsd_complete(struct smb349_charger *chip, u8 status)
{
	int rc;
	u8 reg = 0;
	enum power_supply_type type = POWER_SUPPLY_TYPE_UNKNOWN;

	/*
	 * If apsd is disabled, charger detection is done by
	 * DCIN UV irq.
	 * status = ZERO - indicates charger removed, handled
	 * by DCIN UV irq
	 */
	if (chip->disable_apsd || status == 0) {
		dev_dbg(chip->dev, "APSD %s, status = %d\n",
			chip->disable_apsd ? "disabled" : "enabled", !!status);
		return 0;
	}

	rc = smb349_read_reg(chip, STATUS_D_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read STATUS D rc = %d\n", rc);
		return rc;
	}

	dev_dbg(chip->dev, "%s: STATUS_D_REG=%x\n", __func__, reg);

	switch (reg) {
	case STATUS_D_PORT_ACA_DOCK:
	case STATUS_D_PORT_ACA_C:
	case STATUS_D_PORT_ACA_B:
	case STATUS_D_PORT_ACA_A:
		type = POWER_SUPPLY_TYPE_USB_ACA;
		break;
	case STATUS_D_PORT_CDP:
		type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case STATUS_D_PORT_DCP:
		type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case STATUS_D_PORT_SDP:
		type = POWER_SUPPLY_TYPE_USB;
		break;
	case STATUS_D_PORT_OTHER:
		type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		type = POWER_SUPPLY_TYPE_USB;
		break;
	}

	chip->chg_present = !!status;

	dev_dbg(chip->dev, "APSD complete. USB type detected=%d chg_present=%d",
						type, chip->chg_present);

	if (chip->enable_chg_type_detection) {
		dev_dbg(chip->dev, "%s updating usb_psy with type=%d", __func__,
				type);
		power_supply_set_supply_type(chip->usb_psy, type);
	}

	 /* SMB is now done sampling the D+/D- lines, indicate USB driver */
	dev_dbg(chip->dev, "%s updating usb_psy present=%d", __func__,
			chip->chg_present);
	power_supply_set_present(chip->usb_psy, chip->chg_present);

	return 0;
}

static int chg_uv(struct smb349_charger *chip, u8 status)
{
	/* use this to detect USB insertion only if !apsd */
	if (chip->disable_apsd && status == 0) {
		chip->chg_present = true;
		dev_dbg(chip->dev, "%s updating usb_psy present=%d",
				__func__, chip->chg_present);
		power_supply_set_present(chip->usb_psy, chip->chg_present);
	}

	if (status != 0) {
		chip->chg_present = false;
		dev_dbg(chip->dev, "%s updating usb_psy present=%d",
				__func__, chip->chg_present);
		if (chip->enable_chg_type_detection)
			power_supply_set_supply_type(chip->usb_psy,
						POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_present(chip->usb_psy, chip->chg_present);
	}

	dev_dbg(chip->dev, "chip->chg_present = %d\n", chip->chg_present);

	return 0;
}

static int fast_chg(struct smb349_charger *chip, u8 status)
{
	chip->supply_update = true;

	return 0;
}

static int chg_term(struct smb349_charger *chip, u8 status)
{
	chip->supply_update = true;

	return 0;
}

static int battery_missing(struct smb349_charger *chip, u8 status)
{
	if (status)
		chip->battery_missing = true;
	else
		chip->battery_missing = false;

	chip->supply_update = true;

	return 0;
}

static struct irq_handler_info handlers[] = {
	{IRQ_A_REG, 0, 0,
		{NULL, NULL, NULL, NULL},
	},
	{IRQ_B_REG, 0, 0,
		{fast_chg, NULL, battery_missing, NULL},
	},
	{IRQ_C_REG, 0, 0,
		{chg_term, NULL, NULL, NULL},
	},
	{IRQ_D_REG, 0, 0,
		{NULL, NULL, NULL, apsd_complete},
	},
	{IRQ_E_REG, 0, 0,
		{chg_uv, NULL, NULL, NULL},
	},
	{IRQ_F_REG, 0, 0,
		{NULL, NULL, NULL, NULL},
	},
};

#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BITS_PER_IRQ		2

static irqreturn_t smb349_chg_stat_handler(int irq, void *dev_id)
{
	struct smb349_charger *chip = dev_id;
	int rc, i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat;

	chip->supply_update = false;

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb349_read_reg(chip, handlers[i].stat_reg,
						&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}

		dev_dbg(chip->dev, "stat_reg=%x, prev_val=%x, val=%x\n",
			handlers[i].stat_reg,
			handlers[i].prev_val, handlers[i].val);

		for (j = 0; j < ARRAY_SIZE(handlers[i].smb_irq); j++) {
			triggered = handlers[i].val
			       & (IRQ_LATCHED_MASK << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = handlers[i].prev_val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = changed ^ rt_stat;

			if ((triggered || changed)
				&& handlers[i].smb_irq[j] != NULL) {
				rc = handlers[i].smb_irq[j](chip, rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	if (chip->supply_update) {
		dev_dbg(chip->dev, "%s updating batt psy\n", __func__);
		power_supply_changed(&chip->batt_psy);
	}

	dump_regs(chip);
	return IRQ_HANDLED;
}

static irqreturn_t smb349_chg_valid_handler(int irq, void *dev_id)
{
	struct smb349_charger *chip = dev_id;
	int present;

	present = gpio_get_value_cansleep(chip->chg_valid_gpio);
	if (present < 0) {
		dev_err(chip->dev, "Couldn't read chg_valid gpio=%d\n",
						chip->chg_valid_gpio);
		return IRQ_HANDLED;
	}
	present ^= chip->chg_valid_act_low;

	dev_dbg(chip->dev, "%s: chg_present = %d\n", __func__, present);

	if (present != chip->chg_present) {
		chip->chg_present = present;
		dev_dbg(chip->dev, "%s updating usb_psy present=%d",
				__func__, chip->chg_present);
		power_supply_set_present(chip->usb_psy, chip->chg_present);
	}

	return IRQ_HANDLED;
}

static void smb349_external_power_changed(struct power_supply *psy)
{
	struct smb349_charger *chip = container_of(psy,
				struct smb349_charger, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0, online = 0;

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc)
		dev_err(chip->dev,
			"Couldn't read USB online property, rc=%d\n", rc);
	else
		online = prop.intval;

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc)
		dev_err(chip->dev,
			"Couldn't read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	dev_dbg(chip->dev, "online = %d, current_limit = %d\n",
						online, current_limit);

	smb349_enable_volatile_writes(chip);
	smb349_set_usb_chg_current(chip, current_limit);

	dev_dbg(chip->dev, "%s updating batt psy\n", __func__);
	power_supply_changed(&chip->batt_psy);
}

static int smb_parse_dt(struct smb349_charger *chip)
{
	int rc;
	enum of_gpio_flags gpio_flags;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	chip->charging_disabled = of_property_read_bool(node,
					"qcom,charging-disabled");

	chip->chg_autonomous_mode = of_property_read_bool(node,
					"qcom,chg-autonomous-mode");

	chip->enable_chg_type_detection = of_property_read_bool(node,
					"qcom,enable-chg-type-detection");

	chip->disable_apsd = of_property_read_bool(node, "qcom,disable-apsd");

	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	chip->chg_valid_gpio = of_get_named_gpio_flags(node,
				"qcom,chg-valid-gpio", 0, &gpio_flags);
	if (!gpio_is_valid(chip->chg_valid_gpio))
		dev_dbg(chip->dev, "Invalid chg-valid-gpio");
	else
		chip->chg_valid_act_low = gpio_flags & OF_GPIO_ACTIVE_LOW;

	rc = of_property_read_u32(node, "qcom,fastchg-current-max-ma",
						&chip->fastchg_current_max_ma);
	if (rc)
		chip->fastchg_current_max_ma = SMB349_FAST_CHG_MAX_MA;

	return 0;
}

static int determine_initial_state(struct smb349_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb349_read_reg(chip, IRQ_B_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read IRQ_B rc = %d\n", rc);
		goto fail_init_status;
	}

	chip->battery_missing = (reg & IRQ_B_BATT_MISSING_BIT) ? true : false;

	rc = smb349_read_reg(chip, IRQ_E_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read IRQ_E rc = %d\n", rc);
		goto fail_init_status;
	}

	if (reg & IRQ_E_INPUT_UV_BIT) {
		chg_uv(chip, 1);
	} else {
		chg_uv(chip, 0);
		apsd_complete(chip, 1);
	}

	return 0;

fail_init_status:
	dev_err(chip->dev, "Couldn't determine intial status\n");
	return rc;
}

static int smb349_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc, irq;
	struct smb349_charger *chip;
	struct power_supply *usb_psy;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB psy not found; deferring probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;

	rc = smb_parse_dt(chip);
	if (rc) {
		dev_err(&client->dev, "Couldn't parse DT nodes rc=%d\n", rc);
		return rc;
	}

	i2c_set_clientdata(client, chip);

	chip->batt_psy.name		= "battery";
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smb349_battery_get_property;
	chip->batt_psy.set_property	= smb349_battery_set_property;
	chip->batt_psy.property_is_writeable =
					smb349_batt_property_is_writeable;
	chip->batt_psy.properties	= smb349_battery_properties;
	chip->batt_psy.num_properties	= ARRAY_SIZE(smb349_battery_properties);
	chip->batt_psy.external_power_changed = smb349_external_power_changed;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev, "Couldn't register batt psy rc=%d\n",
				rc);
		return rc;
	}

	dump_regs(chip);

	rc = smb349_regulator_init(chip);
	if  (rc) {
		dev_err(&client->dev,
			"Couldn't initialize smb349 ragulator rc=%d\n", rc);
		return rc;
	}

	rc = smb349_hw_init(chip);
	if (rc) {
		dev_err(&client->dev,
			"Couldn't intialize hardware rc=%d\n", rc);
		goto fail_smb349_hw_init;
	}

	rc = determine_initial_state(chip);
	if (rc) {
		dev_err(&client->dev,
			"Couldn't determine initial state rc=%d\n", rc);
		goto fail_smb349_hw_init;
	}

	if (gpio_is_valid(chip->chg_valid_gpio)) {
		rc = gpio_request(chip->chg_valid_gpio, "smb349_chg_valid");
		if (rc) {
			dev_err(&client->dev,
				"gpio_request for %d failed rc=%d\n",
				chip->chg_valid_gpio, rc);
			goto fail_smb349_hw_init;
		}
		irq = gpio_to_irq(chip->chg_valid_gpio);
		if (irq < 0) {
			dev_err(&client->dev,
				"Invalid chg_valid irq = %d\n", irq);
			goto fail_chg_valid_irq;
		}
		rc = devm_request_threaded_irq(&client->dev, irq,
				NULL, smb349_chg_valid_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"smb349_chg_valid_irq", chip);
		if (rc) {
			dev_err(&client->dev,
				"Failed request_irq irq=%d, gpio=%d rc=%d\n",
						irq, chip->chg_valid_gpio, rc);
			goto fail_chg_valid_irq;
		}
		smb349_chg_valid_handler(irq, chip);
		enable_irq_wake(irq);
	}

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				smb349_chg_stat_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"smb349_chg_stat_irq", chip);
		if (rc) {
			dev_err(&client->dev,
				"Failed STAT irq=%d request rc = %d\n",
				client->irq, rc);
			goto fail_chg_valid_irq;
		}
		enable_irq_wake(client->irq);
	}

	chip->debug_root = debugfs_create_dir("smb349", NULL);
	if (!chip->debug_root)
		dev_err(chip->dev, "Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_file("config_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cnfg_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cnfg debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("status_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &status_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create status debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("cmd_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cmd_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cmd debug file rc = %d\n",
				rc);
	}

	dump_regs(chip);

	dev_info(chip->dev, "SMB349 successfully probed. charger=%d, batt=%d\n",
			chip->chg_present, smb349_get_prop_batt_present(chip));
	return 0;

fail_chg_valid_irq:
	if (gpio_is_valid(chip->chg_valid_gpio))
		gpio_free(chip->chg_valid_gpio);
fail_smb349_hw_init:
	power_supply_unregister(&chip->batt_psy);
	regulator_unregister(chip->otg_vreg.rdev);
	return rc;
}

static int smb349_charger_remove(struct i2c_client *client)
{
	struct smb349_charger *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->batt_psy);
	if (gpio_is_valid(chip->chg_valid_gpio))
		gpio_free(chip->chg_valid_gpio);

	debugfs_remove_recursive(chip->debug_root);
	return 0;
}

static struct of_device_id smb349_match_table[] = {
	{ .compatible = "qcom,smb349-charger",},
	{ },
};

static const struct i2c_device_id smb349_charger_id[] = {
	{"smb349-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb349_charger_id);

static struct i2c_driver smb349_charger_driver = {
	.driver		= {
		.name		= "smb349-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= smb349_match_table,
	},
	.probe		= smb349_charger_probe,
	.remove		= smb349_charger_remove,
	.id_table	= smb349_charger_id,
};

module_i2c_driver(smb349_charger_driver);

MODULE_DESCRIPTION("SMB349 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb349-charger");
