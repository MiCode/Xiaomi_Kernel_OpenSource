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
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

#define SMB135X_BITS_PER_REG	8

/* Mask/Bit helpers */
#define _SMB135X_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB135X_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB135X_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))

/* Config registers */
#define CFG_4_REG			0x04
#define CHG_INHIBIT_MASK		SMB135X_MASK(7, 6)
#define CHG_INHIBIT_50MV_VAL		0x00
#define CHG_INHIBIT_100MV_VAL		0x40
#define CHG_INHIBIT_200MV_VAL		0x80
#define CHG_INHIBIT_300MV_VAL		0xC0

#define CFG_5_REG			0x05
#define RECHARGE_200MV_BIT		BIT(2)
#define USB_2_3_BIT			BIT(5)

#define CFG_C_REG			0x0C
#define USBIN_INPUT_MASK		SMB135X_MASK(4, 0)

#define CFG_D_REG			0x0D

#define CFG_E_REG			0x0E
#define POLARITY_100_500_BIT		BIT(2)
#define USB_CTRL_BY_PIN_BIT		BIT(1)

#define CFG_10_REG			0x11
#define DCIN_INPUT_MASK			SMB135X_MASK(4, 0)

#define CFG_11_REG			0x11
#define PRIORITY_BIT			BIT(7)

#define USBIN_DCIN_CFG_REG		0x12
#define USBIN_SUSPEND_VIA_COMMAND_BIT	BIT(6)

#define CFG_14_REG			0x14
#define CHG_EN_BY_PIN_BIT			BIT(7)
#define CHG_EN_ACTIVE_LOW_BIT		BIT(6)
#define PRE_TO_FAST_REQ_CMD_BIT		BIT(5)
#define DISABLE_CURRENT_TERM_BIT	BIT(3)
#define DISABLE_AUTO_RECHARGE_BIT	BIT(2)
#define EN_CHG_INHIBIT_BIT		BIT(0)

#define CHG_16_REG			0x16
#define SAFETY_TIME_EN_BIT		BIT(4)
#define SAFETY_TIME_MINUTES_MASK	SMB135X_MASK(3, 2)
#define SAFETY_TIME_MINUTES_SHIFT	2

#define CHG_17_REG			0x17
#define CHG_STAT_DISABLE_BIT		BIT(0)
#define CHG_STAT_ACTIVE_HIGH_BIT	BIT(1)
#define CHG_STAT_IRQ_ONLY_BIT		BIT(4)

#define VFLOAT_REG			0x1E

/* Irq Config registers */
#define IRQ_CFG_REG			0x07
#define IRQ_BAT_HOT_COLD_HARD_BIT	BIT(7)
#define IRQ_BAT_HOT_COLD_SOFT_BIT	BIT(6)
#define IRQ_INTERNAL_TEMPERATURE_BIT	BIT(0)

#define IRQ2_CFG_REG			0x08
#define IRQ2_SAFETY_TIMER_BIT		BIT(7)
#define IRQ2_CHG_ERR_BIT		BIT(6)
#define IRQ2_CHG_PHASE_CHANGE_BIT	BIT(4)
#define IRQ2_CHG_INHIBIT_BIT		BIT(3)
#define IRQ2_POWER_OK_BIT		BIT(2)
#define IRQ2_BATT_MISSING_BIT		BIT(1)
#define IRQ2_VBAT_LOW_BIT		BIT(0)

#define IRQ3_CFG_REG			0x09
#define IRQ3_SRC_DETECT_BIT		BIT(2)

/* Command Registers */
#define CMD_I2C_REG			0x40
#define ALLOW_VOLATILE_BIT		BIT(6)

#define CMD_INPUT_LIMIT			0x41
#define USB_SHUTDOWN_BIT		BIT(6)
#define DC_SHUTDOWN_BIT			BIT(5)
#define USE_REGISTER_FOR_CURRENT	BIT(2)
#define USB_100_500_AC_MASK		SMB135X_MASK(1, 0)
#define USB_100_VAL			0x02
#define USB_500_VAL			0x00
#define USB_AC_VAL			0x01

#define CMD_CHG_REG			0x42
#define CMD_CHG_EN			BIT(1)
#define OTG_EN				BIT(0)

/* Status registers */
#define STATUS_1_REG			0x47
#define USING_USB_BIT			BIT(1)
#define USING_DC_BIT			BIT(0)

#define STATUS_4_REG			0x4A
#define BATT_NET_CHG_CURRENT_BIT	BIT(7)
#define BATT_LESS_THAN_2V		BIT(4)
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB135X_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)

#define STATUS_5_REG			0x4B
#define CDP_BIT				BIT(7)
#define DCP_BIT				BIT(6)
#define OTHER_BIT			BIT(5)
#define SDP_BIT				BIT(4)
#define ACA_A_BIT			BIT(3)
#define ACA_B_BIT			BIT(2)
#define ACA_C_BIT			BIT(1)
#define ACA_DOCK_BIT			BIT(0)

#define STATUS_8_REG			0x4E
#define USBIN_9V			BIT(5)
#define USBIN_UNREG			BIT(4)
#define USBIN_LV			BIT(3)
#define DCIN_9V				BIT(2)
#define DCIN_UNREG			BIT(1)
#define DCIN_LV				BIT(0)

#define STATUS_9_REG			0x4F
#define REV_MASK			SMB135X_MASK(3, 0)

/* Irq Status registers */
#define IRQ_A_REG			0x50
#define IRQ_A_HOT_HARD_BIT		BIT(6)
#define IRQ_A_COLD_HARD_BIT		BIT(4)
#define IRQ_A_HOT_SOFT_BIT		BIT(2)
#define IRQ_A_COLD_SOFT_BIT		BIT(0)

#define IRQ_B_REG			0x51
#define IRQ_B_BATT_TERMINAL_BIT		BIT(6)
#define IRQ_B_BATT_MISSING_BIT		BIT(4)
#define IRQ_B_VBAT_LOW_BIT		BIT(2)
#define IRQ_B_TEMPERATURE_BIT		BIT(0)

#define IRQ_C_REG			0x52
#define IRQ_C_TERM_BIT			BIT(0)

#define IRQ_D_REG			0x53
#define IRQ_D_TIMEOUT_BIT		BIT(2)

#define IRQ_E_REG			0x54
#define IRQ_F_REG			0x55
#define IRQ_F_POWER_OK_BIT		BIT(0)

#define IRQ_G_REG			0x56
#define IRQ_G_SRC_DETECT_BIT		BIT(6)

static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

struct smb135x_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct smb135x_chg {
	struct i2c_client		*client;
	struct device			*dev;

	bool				chg_enabled;

	bool				usb_present;
	bool				dc_present;

	int				vfloat_mv;
	int				safety_time;
	int				resume_delta_mv;
	struct dentry			*debug_root;
	int				usb_current_arr_size;

	/* psy */
	struct power_supply		*usb_psy;
	struct power_supply		batt_psy;
	struct power_supply		dc_psy;
	enum power_supply_type		dc_psy_type;
	int				dc_psy_ma;

	/* status tracking */
	bool				chg_done_batt_full;
	bool				batt_present;
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;

	bool				usb_suspended;
	bool				dc_suspended;

	u32				peek_poke_address;
	struct smb135x_regulator	otg_vreg;
	int				skip_writes;
};

static int smb135x_read(struct smb135x_chg *chip, int reg,
				u8 *val)
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

static int smb135x_write(struct smb135x_chg *chip, int reg,
						u8 val)
{
	s32 ret;

	if (chip->skip_writes)
		return 0;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	pr_debug("Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int smb135x_masked_write(struct smb135x_chg *chip, int reg,
						u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb135x_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb135x_write(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static int read_version(struct smb135x_chg *chip, u8 *version)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, STATUS_9_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 9 rc = %d\n", rc);
		return rc;
	}
	*version = (reg & REV_MASK);
	return 0;
}

static bool is_dc_present(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, STATUS_8_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return false;
	}

	return !!(reg & (DCIN_9V | DCIN_UNREG | DCIN_LV));
}

static bool is_usb_present(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, STATUS_8_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return false;
	}

	return !!(reg & (USBIN_9V | USBIN_UNREG | USBIN_LV));
}

static char *usb_type_str[] = {
	"ACA_DOCK",	/* bit 0 */
	"ACA_C",	/* bit 1 */
	"ACA_B",	/* bit 2 */
	"ACA_A",	/* bit 3 */
	"SDP",		/* bit 4 */
	"OTHER",	/* bit 5 */
	"DCP",		/* bit 6 */
	"CDP",		/* bit 7 */
	"NONE",		/* bit 8  error case */
};

/* helper to return the string of USB type */
static char *get_usb_type_name(u8 stat_5)
{
	unsigned long stat = stat_5;

	return usb_type_str[find_first_bit(&stat, SMB135X_BITS_PER_REG)];
}

static enum power_supply_type usb_type_enum[] = {
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 0 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 1 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 2 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 3 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 5 */
	POWER_SUPPLY_TYPE_USB,		/* bit 4 */
	POWER_SUPPLY_TYPE_USB_DCP,	/* bit 6 */
	POWER_SUPPLY_TYPE_USB_CDP,	/* bit 7 */
	POWER_SUPPLY_TYPE_USB,		/* bit 8 error case, report SDP */
};

/* helper to return enum power_supply_type of USB type */
static enum power_supply_type get_usb_supply_type(u8 stat_5)
{
	unsigned long stat = stat_5;

	return usb_type_enum[find_first_bit(&stat, SMB135X_BITS_PER_REG)];
}

static enum power_supply_property smb135x_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int smb135x_get_prop_batt_status(struct smb135x_chg *chip)
{
	int rc;
	int status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0;
	u8 chg_type;

	if (chip->chg_done_batt_full)
		return POWER_SUPPLY_STATUS_FULL;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read STATUS_4_REG rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (reg & CHG_HOLD_OFF_BIT) {
		/*
		 * when chg hold off happens the battery is
		 * not charging
		 */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		goto out;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

	if (chg_type == BATT_NOT_CHG_VAL)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else
		status = POWER_SUPPLY_STATUS_CHARGING;
out:
	pr_debug("STATUS_4_REG=%x\n", reg);
	return status;
}

static int smb135x_get_prop_batt_present(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0)
		return 0;

	/* treat battery gone if less than 2V */
	if (reg & BATT_LESS_THAN_2V)
		return 0;

	return chip->batt_present;
}

static int smb135x_get_charging_status(struct smb135x_chg *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0)
		return 0;

	return (reg & CHG_EN_BIT) ? 1 : 0;
}

static int smb135x_get_prop_charge_type(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;
	u8 chg_type;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0)
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (chg_type == BATT_FAST_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb135x_enable_volatile_writes(struct smb135x_chg *chip)
{
	int rc;

	rc = smb135x_masked_write(chip, CMD_I2C_REG,
			ALLOW_VOLATILE_BIT, ALLOW_VOLATILE_BIT);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set VOLATILE_W_PERM_BIT rc=%d\n", rc);

	return rc;
}

int usb_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
	2050,
	2100,
	2300,
	2400,
	2500,
	3000
};

int dc_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
};

#define CURRENT_100_MA		100
#define CURRENT_150_MA		150
#define CURRENT_500_MA		500
#define CURRENT_900_MA		900
#define SUSPEND_CURRENT_MA	2

static int __smb135x_usb_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc;

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
			USB_SHUTDOWN_BIT, suspend ? USB_SHUTDOWN_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
	return rc;
}

static int smb135x_usb_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc = 0;

	chip->usb_suspended = suspend;
	if (chip->chg_enabled)
		rc = __smb135x_usb_suspend(chip, suspend);

	return rc;
}

static int __smb135x_dc_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc = 0;

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
			DC_SHUTDOWN_BIT, suspend ? DC_SHUTDOWN_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
	return rc;
}

static int smb135x_dc_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc = 0;

	chip->dc_suspended = suspend;
	if (chip->chg_enabled)
		rc = __smb135x_dc_suspend(chip, suspend);

	return rc;
}

static int smb135x_set_high_usb_chg_current(struct smb135x_chg *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = chip->usb_current_arr_size - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);
		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smb135x_masked_write(chip, CFG_C_REG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
					USB_100_500_AC_MASK, USB_AC_VAL);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);
	return rc;
}

#define MAX_VERSION			0xF
#define USB_100_PROBLEM_VERSION		0x2
/* if APSD results are used
 *	if SDP is detected it will look at 500mA setting
 *		if set it will draw 500mA
 *		if unset it will draw 100mA
 *	if CDP/DCP it will look at 0x0C setting
 *		i.e. values in 0x41[1, 0] does not matter
 */
static int smb135x_set_usb_chg_current(struct smb135x_chg *chip,
							int current_ma)
{
	int rc;
	u8 version = MAX_VERSION;

	pr_debug("USB current_ma = %d\n", current_ma);

	rc = read_version(chip, &version);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read version rc = %d\n", rc);

	if (version <= USB_100_PROBLEM_VERSION
		&& (current_ma == CURRENT_100_MA
			|| current_ma == CURRENT_150_MA)) {
		pr_info("version = 0x%02x USB requested = %dmA using %dmA\n",
			version, current_ma, CURRENT_500_MA);
		current_ma = CURRENT_500_MA;
	}

	if (current_ma <= SUSPEND_CURRENT_MA) {
		/* force suspend bit */
		rc = smb135x_usb_suspend(chip, true);
		goto out;
	}
	if (current_ma < CURRENT_150_MA) {
		/* force 100mA */
		rc = smb135x_masked_write(chip, CFG_5_REG, USB_2_3_BIT, 0);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		rc |= smb135x_usb_suspend(chip, false);
		goto out;
	}
	/* specific current values */
	if (current_ma == CURRENT_150_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		rc |= smb135x_usb_suspend(chip, false);
		goto out;
	}
	if (current_ma == CURRENT_500_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG, USB_2_3_BIT, 0);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_500_VAL);
		rc |= smb135x_usb_suspend(chip, false);
		goto out;
	}
	if (current_ma == CURRENT_900_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_500_VAL);
		rc |= smb135x_usb_suspend(chip, false);
		goto out;
	}

	rc = smb135x_set_high_usb_chg_current(chip, current_ma);
	rc |= smb135x_usb_suspend(chip, false);
out:
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	return rc;
}

static int smb135x_charging(struct smb135x_chg *chip, int enable)
{
	int rc = 0;

	pr_debug("charging enable = %d\n", enable);

	rc = smb135x_masked_write(chip, CMD_CHG_REG,
			CMD_CHG_EN, enable ? CMD_CHG_EN : 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set CHG_ENABLE_BIT enable = %d rc = %d\n",
			enable, rc);
		return rc;
	}
	chip->chg_enabled = enable;

	/* manage the shutdown bits */
	if (enable) {
		/* restore the suspended status */
		rc = smb135x_dc_suspend(chip, chip->dc_suspended);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend to %d rc = %d\n",
				chip->dc_suspended, rc);
			return rc;
		}
		rc = smb135x_usb_suspend(chip, chip->usb_suspended);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend to %d rc = %d\n",
				chip->usb_suspended, rc);
			return rc;
		}
	} else {
		/* force the shutdown bits */
		rc = __smb135x_dc_suspend(chip, true);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend rc = %d\n", rc);
			return rc;
		}
		rc = __smb135x_usb_suspend(chip, true);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend rc = %d\n", rc);
			return rc;
		}
	}

	pr_debug("charging %s\n",
			enable ?  "enabled" : "disabled running from batt");
	return rc;
}

static int smb135x_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb135x_charging(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb135x_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smb135x_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb135x_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb135x_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = smb135x_get_charging_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb135x_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smb135x_dc_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb135x_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		/* return if dc is charging the battery */
		val->intval = is_dc_present(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void smb135x_external_power_changed(struct power_supply *psy)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0, online = 0;

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB online property, rc=%d\n", rc);
	else
		online = prop.intval;

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	pr_debug("online = %d, current_limit = %d\n", online, current_limit);
	if (online) {
		rc = smb135x_set_usb_chg_current(chip, current_limit);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set usb current rc = %d\n",
					rc);
	} else {
		/*
		 * reset the current to 100mA to prevent drawing
		 * a potentially high current at next USB insertion event
		 */
		rc = smb135x_set_usb_chg_current(chip, CURRENT_100_MA);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set usb current rc = %d\n",
					rc);
	}
}

#define MIN_FLOAT_MV	3600
#define MAX_FLOAT_MV	4500

#define MID_RANGE_FLOAT_MV_MIN		3600
#define MID_RANGE_FLOAT_MIN_VAL		0x05
#define MID_RANGE_FLOAT_STEP_MV		20

#define HIGH_RANGE_FLOAT_MIN_MV		4340
#define HIGH_RANGE_FLOAT_MIN_VAL	0x2A
#define HIGH_RANGE_FLOAT_STEP_MV	10

#define VHIGH_RANGE_FLOAT_MIN_MV	4400
#define VHIGH_RANGE_FLOAT_MIN_VAL	0x2E
#define VHIGH_RANGE_FLOAT_STEP_MV	20
static int smb135x_float_voltage_set(struct smb135x_chg *chip, int vfloat_mv)
{
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	if (vfloat_mv <= HIGH_RANGE_FLOAT_MIN_MV) {
		/* mid range */
		temp = MID_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - MID_RANGE_FLOAT_MV_MIN)
				/ MID_RANGE_FLOAT_STEP_MV;
	} else if (vfloat_mv <= VHIGH_RANGE_FLOAT_MIN_MV) {
		/* high range */
		temp = HIGH_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - HIGH_RANGE_FLOAT_MIN_MV)
				/ HIGH_RANGE_FLOAT_STEP_MV;
	} else {
		/* very high range */
		temp = VHIGH_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - VHIGH_RANGE_FLOAT_MIN_MV)
				/ VHIGH_RANGE_FLOAT_STEP_MV;
	}

	return smb135x_write(chip, VFLOAT_REG, temp);
}

static int smb135x_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);

	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, OTG_EN);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't enable  OTG mode rc=%d\n", rc);
	return rc;
}

static int smb135x_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);

	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
	return rc;
}

static int smb135x_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);

	rc = smb135x_read(chip, CMD_CHG_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return  (reg & OTG_EN) ? 1 : 0;
}

struct regulator_ops smb135x_chg_otg_reg_ops = {
	.enable		= smb135x_chg_otg_regulator_enable,
	.disable	= smb135x_chg_otg_regulator_disable,
	.is_enabled	= smb135x_chg_otg_regulator_is_enable,
};

static int smb135x_regulator_init(struct smb135x_chg *chip)
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
		chip->otg_vreg.rdesc.ops = &smb135x_chg_otg_reg_ops;
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

static int hot_hard_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_hot = !!rt_stat;
	return 0;
}
static int cold_hard_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cold = !!rt_stat;
	return 0;
}
static int hot_soft_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_warm = !!rt_stat;
	return 0;
}
static int cold_soft_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cool = !!rt_stat;
	return 0;
}
static int battery_missing_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_present = !!rt_stat;
	return 0;
}
static int vbat_low_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("vbat low\n");
	return 0;
}
static int chg_hot_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("chg hot\n");
	return 0;
}
static int chg_term_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->chg_done_batt_full = !!rt_stat;
	return 0;
}

static int taper_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int recharge_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int safety_timeout_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("safety timeout rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

/**
 * power_ok_handler() - called  when any charger is inserted or removed,
 *		use it to handle dc charger insertion and removal
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static int power_ok_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	bool dc_present = is_dc_present(chip);

	pr_debug("chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

	if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;

		if (chip->dc_psy_type != -EINVAL)
			power_supply_set_online(&chip->dc_psy,
							chip->dc_present);
	}

	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		if (chip->dc_psy_type != -EINVAL)
			power_supply_set_online(&chip->dc_psy,
							chip->dc_present);
	}

	return 0;
}

static int handle_usb_removal(struct smb135x_chg *chip)
{
	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n",
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	}
	return 0;
}

static int handle_usb_insertion(struct smb135x_chg *chip)
{
	u8 reg;
	int rc;
	char *usb_type_name = "null";
	enum power_supply_type usb_supply_type;

	/* usb inserted */
	rc = smb135x_read(chip, STATUS_5_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return rc;
	}
	usb_type_name = get_usb_type_name(reg);
	usb_supply_type = get_usb_supply_type(reg);
	pr_debug("inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);
	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n", usb_supply_type);
		power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	}
	return 0;
}

/**
 * src_detect_handler() - this is called when USB charger type is detected, use
 *			it for handling USB charger insertion and removal
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static int src_detect_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	bool usb_present = is_usb_present(chip);

	pr_debug("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);
	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}

	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	}

	return 0;
}

static int chg_inhibit_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	/*
	 * charger is inserted when the battery voltage is high
	 * so h/w won't start charging just yet. Treat this as
	 * battery full
	 */
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->chg_done_batt_full = !!rt_stat;
	return 0;
}

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	int (*smb_irq[4])(struct smb135x_chg *chip, u8 rt_stat);
};

static struct irq_handler_info handlers[] = {
	{IRQ_A_REG, 0, 0,
		{cold_soft_handler, hot_soft_handler,
			cold_hard_handler, hot_hard_handler
		}
	},
	{IRQ_B_REG, 0, 0,
		{chg_hot_handler, vbat_low_handler,
			battery_missing_handler, battery_missing_handler
		}
	},
	{IRQ_C_REG, 0, 0,
		{chg_term_handler, taper_handler, recharge_handler, NULL}
	},
	{IRQ_D_REG, 0, 0,
		{NULL, safety_timeout_handler, NULL, NULL}
	},
	{IRQ_E_REG, 0, 0,
		{NULL, NULL, NULL, NULL}
	},
	{IRQ_F_REG, 0, 0,
		{power_ok_handler, NULL, NULL, NULL, }
	},
	{IRQ_G_REG, 0, 0,
		{chg_inhibit_handler, NULL, NULL, src_detect_handler}
	},
};

#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BITS_PER_IRQ		2
static irqreturn_t smb135x_chg_stat_handler(int irq, void *dev_id)
{
	struct smb135x_chg *chip = dev_id;
	int i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat, prev_rt_stat;
	int rc;
	int handler_count = 0;

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb135x_read(chip, handlers[i].stat_reg,
						&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}

		for (j = 0; j < ARRAY_SIZE(handlers[i].smb_irq); j++) {
			triggered = handlers[i].val
			       & (IRQ_LATCHED_MASK << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			prev_rt_stat = handlers[i].prev_val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = prev_rt_stat ^ rt_stat;

			if ((triggered || changed)
				&& handlers[i].smb_irq[j] != NULL) {
				handler_count++;
				rc = handlers[i].smb_irq[j](chip, rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	pr_debug("handler count = %d\n", handler_count);
	if (handler_count) {
		pr_debug("batt psy changed\n");
		power_supply_changed(&chip->batt_psy);
		if (chip->usb_psy) {
			pr_debug("usb psy changed\n");
			power_supply_changed(chip->usb_psy);
		}
		if (chip->dc_psy_type != -EINVAL) {
			pr_debug("dc psy changed\n");
			power_supply_changed(&chip->dc_psy);
		}
	}

	return IRQ_HANDLED;
}

#define LAST_CNFG_REG	0x1F
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cnfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_CMD_REG	0x40
#define LAST_CMD_REG	0x42
static int show_cmd_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cmd_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_cmd_regs, chip);
}

static const struct file_operations cmd_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cmd_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_STATUS_REG	0x46
#define LAST_STATUS_REG		0x56
static int show_status_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int status_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_status_regs, chip);
}

static const struct file_operations status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= status_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct smb135x_chg *chip = data;
	int rc;
	u8 temp;

	rc = smb135x_read(chip, chip->peek_poke_address, &temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't read reg %x rc = %d\n",
			chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct smb135x_chg *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = smb135x_write(chip, chip->peek_poke_address, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			chip->peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int force_irq_set(void *data, u64 val)
{
	struct smb135x_chg *chip = data;

	smb135x_chg_stat_handler(chip->client->irq, data);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_irq_ops, NULL, force_irq_set, "0x%02llx\n");

#ifdef DEBUG
static void dump_regs(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}
}
#else
static void dump_regs(struct smb135x_chg *chip)
{
}
#endif
static int determine_initial_status(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */

	chip->batt_present = true;
	rc = smb135x_read(chip, IRQ_B_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq b rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_B_BATT_TERMINAL_BIT || reg & IRQ_B_BATT_MISSING_BIT)
		chip->batt_present = false;
	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 4 rc = %d\n", rc);
		return rc;
	}
	/* treat battery gone if less than 2V */
	if (reg & BATT_LESS_THAN_2V)
		chip->batt_present = false;

	rc = smb135x_read(chip, IRQ_A_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq A rc = %d\n", rc);
		return rc;
	}

	if (reg & IRQ_A_HOT_HARD_BIT)
		chip->batt_hot = true;
	if (reg & IRQ_A_COLD_HARD_BIT)
		chip->batt_cold = true;
	if (reg & IRQ_A_HOT_SOFT_BIT)
		chip->batt_warm = true;
	if (reg & IRQ_A_COLD_SOFT_BIT)
		chip->batt_cool = true;

	rc = smb135x_read(chip, IRQ_C_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq A rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_C_TERM_BIT)
		chip->chg_done_batt_full = true;

	chip->usb_present = is_usb_present(chip);
	chip->dc_present = is_dc_present(chip);

	if (chip->usb_present)
		handle_usb_insertion(chip);

	return 0;
}

static int smb135x_hw_init(struct smb135x_chg *chip)
{
	int rc;
	int i;
	u8 reg, mask, version, dc_cur_val;

	rc = smb135x_enable_volatile_writes(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't configure for volatile rc = %d\n",
				rc);
		return rc;
	}

	/*
	 * force using current from the register i.e. ignore auto
	 * power source detect mA ratings
	 */
	rc = read_version(chip, &version);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read version rc=%d\n", rc);
		return rc;
	}

	mask = USE_REGISTER_FOR_CURRENT;
	if (version <= USB_100_PROBLEM_VERSION)
		reg = 0;
	else
		reg = USE_REGISTER_FOR_CURRENT;
	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT, mask, reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set input limit cmd rc=%d\n", rc);
		return rc;
	}

	/* set bit 0 = 100mA bit 1 = 500mA and set register control */
	rc = smb135x_masked_write(chip, CFG_E_REG,
			POLARITY_100_500_BIT | USB_CTRL_BY_PIN_BIT,
			POLARITY_100_500_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usbin cfg rc=%d\n", rc);
		return rc;
	}

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast, enable current termination, enable
	 * auto recharge, enable chg inhibition
	 */
	rc = smb135x_masked_write(chip, CFG_14_REG,
			CHG_EN_BY_PIN_BIT | CHG_EN_ACTIVE_LOW_BIT
			| PRE_TO_FAST_REQ_CMD_BIT | DISABLE_CURRENT_TERM_BIT
			| DISABLE_AUTO_RECHARGE_BIT | EN_CHG_INHIBIT_BIT,
			EN_CHG_INHIBIT_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg 14 rc=%d\n", rc);
		return rc;
	}

	/* control USB suspend via command bits */
	rc = smb135x_masked_write(chip, USBIN_DCIN_CFG_REG,
		USBIN_SUSPEND_VIA_COMMAND_BIT, USBIN_SUSPEND_VIA_COMMAND_BIT);

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smb135x_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		if (chip->safety_time == 0) {
			/* safety timer disabled */
			rc = smb135x_masked_write(chip, CHG_16_REG,
							SAFETY_TIME_EN_BIT, 0);
			if (rc < 0) {
				dev_err(chip->dev,
				"Couldn't disable safety timer rc = %d\n",
				rc);
				return rc;
			}
		} else {
			for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
				if (chip->safety_time <= chg_time[i]) {
					reg = i << SAFETY_TIME_MINUTES_SHIFT;
					break;
				}
			}
			rc = smb135x_masked_write(chip, CHG_16_REG,
				SAFETY_TIME_EN_BIT | SAFETY_TIME_MINUTES_MASK,
				SAFETY_TIME_EN_BIT | reg);
			if (rc < 0) {
				dev_err(chip->dev,
					"Couldn't set safety timer rc = %d\n",
					rc);
				return rc;
			}
		}
	}

	smb135x_charging(chip, chip->chg_enabled);

	/* interrupt enabling - active low */
	if (chip->client->irq) {
		mask = CHG_STAT_IRQ_ONLY_BIT | CHG_STAT_ACTIVE_HIGH_BIT
			| CHG_STAT_DISABLE_BIT;
		reg = CHG_STAT_IRQ_ONLY_BIT;
		rc = smb135x_masked_write(chip, CHG_17_REG, mask, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq config rc = %d\n",
					rc);
			return rc;
		}

		/* enabling only interesting interrupts */
		rc = smb135x_write(chip, IRQ_CFG_REG,
			IRQ_BAT_HOT_COLD_HARD_BIT
			| IRQ_BAT_HOT_COLD_SOFT_BIT
			| IRQ_INTERNAL_TEMPERATURE_BIT);

		rc |= smb135x_write(chip, IRQ2_CFG_REG,
			IRQ2_SAFETY_TIMER_BIT
			| IRQ2_CHG_ERR_BIT
			| IRQ2_CHG_PHASE_CHANGE_BIT
			| IRQ2_CHG_INHIBIT_BIT
			| IRQ2_POWER_OK_BIT
			| IRQ2_BATT_MISSING_BIT
			| IRQ2_VBAT_LOW_BIT);

		rc |= smb135x_write(chip, IRQ3_CFG_REG, IRQ3_SRC_DETECT_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq enable rc = %d\n",
					rc);
			return rc;
		}
	}

	/* resume threshold */
	if (chip->resume_delta_mv != -EINVAL) {
		if (chip->resume_delta_mv < 100)
			reg = CHG_INHIBIT_50MV_VAL;
		else if (chip->resume_delta_mv < 200)
			reg = CHG_INHIBIT_100MV_VAL;
		else if (chip->resume_delta_mv < 300)
			reg = CHG_INHIBIT_200MV_VAL;
		else
			reg = CHG_INHIBIT_300MV_VAL;

		rc = smb135x_masked_write(chip, CFG_4_REG,
						CHG_INHIBIT_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set inhibit val rc = %d\n",
					rc);
			return rc;
		}

		if (chip->resume_delta_mv < 200)
			reg = 0;
		else
			 reg = RECHARGE_200MV_BIT;

		rc = smb135x_masked_write(chip, CFG_5_REG,
						RECHARGE_200MV_BIT, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set recharge  rc = %d\n",
					rc);
			return rc;
		}
	}

	/* DC path current settings */
	if (chip->dc_psy_type != -EINVAL) {
		for (i = ARRAY_SIZE(dc_current_table) - 1; i >= 0; i--) {
			if (chip->dc_psy_ma >= dc_current_table[i])
				break;
		}
		dc_cur_val = i & DCIN_INPUT_MASK;
		rc = smb135x_masked_write(chip, CFG_10_REG,
					DCIN_INPUT_MASK, dc_cur_val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set dc charge current rc = %d\n",
					rc);
			return rc;
		}
	}

	return rc;
}

static struct of_device_id smb135x_match_table[] = {
	{
		.compatible = "qcom,smb1357-charger",
		.data = (void *)ARRAY_SIZE(usb_current_table),
	},
	{
		.compatible = "qcom,smb1359-charger",
		.data = (void *)ARRAY_SIZE(usb_current_table) - 1,
	},
	{ },
};

#define DC_MA_MIN 300
#define DC_MA_MAX 2000
static int smb_parse_dt(struct smb135x_chg *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;
	const struct of_device_id *match;
	const char *dc_psy_type;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	match = of_match_node(smb135x_match_table, node);
	if (match == NULL) {
		dev_err(chip->dev, "device tree match not found\n");
		return -EINVAL;
	}

	chip->usb_current_arr_size = (int)match->data;

	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc < 0)
		chip->vfloat_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,charging-timeout",
						&chip->safety_time);
	if (rc < 0)
		chip->safety_time = -EINVAL;

	if (!rc &&
		(chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout %d\n",
						chip->safety_time);
		return -EINVAL;
	}

	chip->dc_psy_type = -EINVAL;
	dc_psy_type = of_get_property(node, "qcom,dc-psy-type", NULL);
	if (dc_psy_type) {
		if (strcmp(dc_psy_type, "Mains") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_MAINS;
		else if (strcmp(dc_psy_type, "Wireless") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIRELESS;
	}

	if (chip->dc_psy_type != -EINVAL) {
		rc = of_property_read_u32(node, "qcom,dc-psy-ma",
							&chip->dc_psy_ma);
		if (rc < 0) {
			dev_err(chip->dev,
					"no mA current for dc rc = %d\n", rc);
			return rc;
		}

		if (chip->dc_psy_ma < DC_MA_MIN
				|| chip->dc_psy_ma > DC_MA_MAX) {
			dev_err(chip->dev, "Bad dc mA %d\n", chip->dc_psy_ma);
			return -EINVAL;
		}
	}

	rc = of_property_read_u32(node, "qcom,recharge-thresh-mv",
						&chip->resume_delta_mv);
	if (rc < 0)
		chip->resume_delta_mv = -EINVAL;

	chip->chg_enabled = !(of_property_read_bool(node,
						"qcom,charging-disabled"));

	return 0;
}

static int smb135x_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct smb135x_chg *chip;
	struct power_supply *usb_psy;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB supply not found; defer probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;

	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&client->dev, "Unable to parse DT nodes\n");
		return rc;
	}

	i2c_set_clientdata(client, chip);

	dump_regs(chip);

	rc = smb135x_regulator_init(chip);
	if  (rc) {
		dev_err(&client->dev,
			"Couldn't initialize regulator rc=%d\n", rc);
		return rc;
	}

	rc = smb135x_hw_init(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		return rc;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to determine init status rc = %d\n", rc);
		return rc;
	}

	chip->batt_psy.name		= "battery";
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smb135x_battery_get_property;
	chip->batt_psy.set_property	= smb135x_battery_set_property;
	chip->batt_psy.properties	= smb135x_battery_properties;
	chip->batt_psy.num_properties  = ARRAY_SIZE(smb135x_battery_properties);
	chip->batt_psy.external_power_changed = smb135x_external_power_changed;
	chip->batt_psy.property_is_writeable = smb135x_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		return rc;
	}

	if (chip->dc_psy_type != -EINVAL) {
		chip->dc_psy.name		= "dc";
		chip->dc_psy.type		= chip->dc_psy_type;
		chip->dc_psy.get_property	= smb135x_dc_get_property;
		chip->dc_psy.properties		= smb135x_dc_properties;
		chip->dc_psy.num_properties = ARRAY_SIZE(smb135x_dc_properties);
		rc = power_supply_register(chip->dev, &chip->dc_psy);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to register dc_psy rc = %d\n", rc);
			goto unregister_batt_psy;
		}
	}

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				smb135x_chg_stat_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"smb135x_chg_stat_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev,
				"request_irq for irq=%d  failed rc = %d\n",
				client->irq, rc);
			goto unregister_dc_psy;
		}
		enable_irq_wake(client->irq);
	}

	chip->debug_root = debugfs_create_dir("smb135x", NULL);
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

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->peek_poke_address));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create address debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("force_irq",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &force_irq_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("skip_writes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_writes));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);
	}

	dev_info(chip->dev, "SMB135X successfully probed batt=%d dc = %d usb = %d\n",
			smb135x_get_prop_batt_present(chip),
			chip->dc_present, chip->usb_present);
	return 0;

unregister_dc_psy:
	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);
unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
	return rc;
}

static int smb135x_charger_remove(struct i2c_client *client)
{
	struct smb135x_chg *chip = i2c_get_clientdata(client);

	debugfs_remove_recursive(chip->debug_root);

	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);

	power_supply_unregister(&chip->batt_psy);

	return 0;
}

static const struct i2c_device_id smb135x_charger_id[] = {
	{"smb135x-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb135x_charger_id);

static struct i2c_driver smb135x_charger_driver = {
	.driver		= {
		.name		= "smb135x-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= smb135x_match_table,
	},
	.probe		= smb135x_charger_probe,
	.remove		= smb135x_charger_remove,
	.id_table	= smb135x_charger_id,
};

module_i2c_driver(smb135x_charger_driver);

MODULE_DESCRIPTION("SMB135x Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb135x-charger");
