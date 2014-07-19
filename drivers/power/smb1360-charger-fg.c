/* Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "SMB:%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/qpnp/qpnp-adc.h>

#define _SMB1360_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB1360_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB1360_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))

/* Charger Registers */
#define CFG_BATT_CHG_REG		0x00
#define CHG_ITERM_MASK			SMB1360_MASK(2, 0)
#define CHG_ITERM_25MA			0x0
#define CHG_ITERM_200MA			0x7
#define RECHG_MV_MASK			SMB1360_MASK(6, 5)
#define RECHG_MV_SHIFT			5

#define CFG_BATT_CHG_ICL_REG		0x05
#define AC_INPUT_ICL_PIN_BIT		BIT(7)
#define AC_INPUT_PIN_HIGH_BIT		BIT(6)
#define RESET_STATE_USB_500		BIT(5)
#define INPUT_CURR_LIM_MASK		SMB1360_MASK(3, 0)
#define INPUT_CURR_LIM_300MA		0x0

#define CFG_GLITCH_FLT_REG		0x06
#define AICL_ENABLED_BIT		BIT(0)
#define INPUT_UV_GLITCH_FLT_20MS_BIT	BIT(7)

#define CFG_CHG_MISC_REG		0x7
#define CHG_EN_BY_PIN_BIT		BIT(7)
#define CHG_EN_ACTIVE_LOW_BIT		BIT(6)
#define PRE_TO_FAST_REQ_CMD_BIT		BIT(5)
#define CHG_CURR_TERM_DIS_BIT		BIT(3)
#define CFG_AUTO_RECHG_DIS_BIT		BIT(2)
#define CFG_CHG_INHIBIT_EN_BIT		BIT(0)

#define CFG_STAT_CTRL_REG		0x09
#define CHG_STAT_IRQ_ONLY_BIT		BIT(4)
#define CHG_TEMP_CHG_ERR_BLINK_BIT	BIT(3)
#define CHG_STAT_ACTIVE_HIGH_BIT	BIT(1)
#define CHG_STAT_DISABLE_BIT		BIT(0)

#define CFG_SFY_TIMER_CTRL_REG		0x0A
#define SAFETY_TIME_EN_BIT		BIT(4)
#define SAFETY_TIME_MINUTES_SHIFT	2
#define SAFETY_TIME_MINUTES_MASK	SMB1360_MASK(3, 2)

#define CFG_BATT_MISSING_REG		0x0D
#define BATT_MISSING_SRC_THERM_BIT	BIT(1)

#define CFG_FG_BATT_CTRL_REG		0x0E
#define BATT_ID_ENABLED_BIT		BIT(5)
#define CHG_BATT_ID_FAIL		BIT(4)
#define BATT_ID_FAIL_SELECT_PROFILE	BIT(3)
#define BATT_PROFILE_SELECT_MASK	SMB1360_MASK(3, 0)
#define BATT_PROFILEA_MASK		0x0
#define BATT_PROFILEB_MASK		0xF

#define IRQ_CFG_REG			0x0F
#define IRQ_BAT_HOT_COLD_HARD_BIT	BIT(7)
#define IRQ_BAT_HOT_COLD_SOFT_BIT	BIT(6)
#define IRQ_DCIN_UV_BIT			BIT(2)
#define IRQ_INTERNAL_TEMPERATURE_BIT	BIT(0)

#define IRQ2_CFG_REG			0x10
#define IRQ2_SAFETY_TIMER_BIT		BIT(7)
#define IRQ2_CHG_ERR_BIT		BIT(6)
#define IRQ2_CHG_PHASE_CHANGE_BIT	BIT(4)
#define IRQ2_POWER_OK_BIT		BIT(2)
#define IRQ2_BATT_MISSING_BIT		BIT(1)
#define IRQ2_VBAT_LOW_BIT		BIT(0)

#define IRQ3_CFG_REG			0x11
#define IRQ3_SOC_CHANGE_BIT		BIT(4)
#define IRQ3_SOC_MIN_BIT		BIT(3)
#define IRQ3_SOC_MAX_BIT		BIT(2)
#define IRQ3_SOC_EMPTY_BIT		BIT(1)
#define IRQ3_SOC_FULL_BIT		BIT(0)

#define CHG_CURRENT_REG			0x13
#define FASTCHG_CURR_MASK		SMB1360_MASK(4, 2)
#define FASTCHG_CURR_SHIFT		2

#define BATT_CHG_FLT_VTG_REG		0x15
#define VFLOAT_MASK			SMB1360_MASK(6, 0)

#define SHDN_CTRL_REG			0x1A
#define SHDN_CMD_USE_BIT		BIT(1)
#define SHDN_CMD_POLARITY_BIT		BIT(2)

/* Command Registers */
#define CMD_I2C_REG			0x40
#define ALLOW_VOLATILE_BIT		BIT(6)
#define FG_ACCESS_ENABLED_BIT		BIT(5)
#define FG_RESET_BIT			BIT(4)
#define CYCLE_STRETCH_CLEAR_BIT		BIT(3)

#define CMD_IL_REG			0x41
#define USB_CTRL_MASK			SMB1360_MASK(1 , 0)
#define USB_100_BIT			0x01
#define USB_500_BIT			0x00
#define USB_AC_BIT			0x02
#define SHDN_CMD_BIT			BIT(7)

#define CMD_CHG_REG			0x42
#define CMD_CHG_EN			BIT(1)
#define CMD_OTG_EN_BIT			BIT(0)

/* Status Registers */
#define STATUS_3_REG			0x4B
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB1360_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)

#define STATUS_4_REG			0x4C
#define CYCLE_STRETCH_ACTIVE_BIT	BIT(5)

#define REVISION_CTRL_REG		0x4F
#define DEVICE_REV_MASK			SMB1360_MASK(3, 0)

/* IRQ Status Registers */
#define IRQ_A_REG			0x50
#define IRQ_A_HOT_HARD_BIT		BIT(6)
#define IRQ_A_COLD_HARD_BIT		BIT(4)
#define IRQ_A_HOT_SOFT_BIT		BIT(2)
#define IRQ_A_COLD_SOFT_BIT		BIT(0)

#define IRQ_B_REG			0x51
#define IRQ_B_BATT_TERMINAL_BIT		BIT(6)
#define IRQ_B_BATT_MISSING_BIT		BIT(4)

#define IRQ_C_REG			0x52
#define IRQ_C_CHG_TERM			BIT(0)

#define IRQ_D_REG			0x53
#define IRQ_E_REG			0x54
#define IRQ_E_USBIN_UV_BIT		BIT(0)

#define IRQ_F_REG			0x55

#define IRQ_G_REG			0x56

#define IRQ_H_REG			0x57
#define IRQ_I_REG			0x58
#define FG_ACCESS_ALLOWED_BIT		BIT(0)
#define BATT_ID_RESULT_BIT		SMB1360_MASK(6, 4)
#define BATT_ID_SHIFT			4

/* FG registers - IRQ config register */
#define SOC_MAX_REG			0x24
#define SOC_MIN_REG			0x25
#define VTG_EMPTY_REG			0x26
#define SOC_DELTA_REG			0x28
#define VTG_MIN_REG			0x2B

/* FG SHADOW registers */
#define SHDW_FG_ESR_ACTUAL		0x20
#define SHDW_FG_BATT_STATUS		0x60
#define BATTERY_PROFILE_BIT		BIT(0)

#define SHDW_FG_MSYS_SOC		0x61
#define SHDW_FG_CAPACITY		0x62
#define SHDW_FG_VTG_NOW			0x69
#define SHDW_FG_CURR_NOW		0x6B
#define SHDW_FG_BATT_TEMP		0x6D

#define CC_TO_SOC_COEFF			0xBA
#define NOMINAL_CAPACITY_REG		0xBC

#define FG_I2C_CFG_MASK			SMB1360_MASK(2, 1)
#define FG_CFG_I2C_ADDR			0x2
#define FG_PROFILE_A_ADDR		0x4
#define FG_PROFILE_B_ADDR		0x6

/* Constants */
#define CURRENT_100_MA			100
#define CURRENT_500_MA			500
#define MAX_8_BITS			255

#define SMB1360_REV_1			0x01

enum {
	WRKRND_FG_CONFIG_FAIL = BIT(0),
	WRKRND_BATT_DET_FAIL = BIT(1),
	WRKRND_USB100_FAIL = BIT(2),
};

enum {
	USER	= BIT(0),
};

enum fg_i2c_access_type {
	FG_ACCESS_CFG = 0x1,
	FG_ACCESS_PROFILE_A = 0x2,
	FG_ACCESS_PROFILE_B = 0x3
};

enum {
	BATTERY_PROFILE_A,
	BATTERY_PROFILE_B,
	BATTERY_PROFILE_MAX,
};

struct smb1360_otg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct smb1360_chip {
	struct i2c_client		*client;
	struct device			*dev;
	u8				revision;
	unsigned short			default_i2c_addr;
	unsigned short			fg_i2c_addr;
	bool				pulsed_irq;

	/* configuration data - charger */
	int				fake_battery_soc;
	bool				batt_id_disabled;
	bool				charging_disabled;
	bool				recharge_disabled;
	bool				chg_inhibit_disabled;
	bool				iterm_disabled;
	bool				shdn_after_pwroff;
	int				iterm_ma;
	int				vfloat_mv;
	int				safety_time;
	int				resume_delta_mv;
	u32				default_batt_profile;
	unsigned int			thermal_levels;
	unsigned int			therm_lvl_sel;
	unsigned int			*thermal_mitigation;

	/* configuration data - fg */
	int				soc_max;
	int				soc_min;
	int				delta_soc;
	int				voltage_min_mv;
	int				voltage_empty_mv;
	int				batt_capacity_mah;
	int				cc_soc_coeff;

	/* status tracking */
	bool				usb_present;
	bool				batt_present;
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;
	bool				batt_full;
	bool				resume_completed;
	bool				irq_waiting;
	bool				empty_soc;
	int				workaround_flags;
	u8				irq_cfg_mask[3];
	int				usb_psy_ma;
	int				charging_disabled_status;
	u32				connected_rid;
	u32				profile_rid[BATTERY_PROFILE_MAX];

	u32				peek_poke_address;
	u32				fg_access_type;
	u32				fg_peek_poke_address;
	int				skip_writes;
	int				skip_reads;
	struct dentry			*debug_root;

	struct qpnp_vadc_chip		*vadc_dev;
	struct power_supply		*usb_psy;
	struct power_supply		batt_psy;
	struct smb1360_otg_regulator	otg_vreg;
	struct mutex			irq_complete;
	struct mutex			charging_disable_lock;
	struct mutex			current_change_lock;
	struct mutex			read_write_lock;
};

static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

static int input_current_limit[] = {
	300, 400, 450, 500, 600, 700, 800, 850, 900,
	950, 1000, 1100, 1200, 1300, 1400, 1500,
};

static int fastchg_current[] = {
	450, 600, 750, 900, 1050, 1200, 1350, 1500,
};

static int is_between(int value, int left, int right)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}

static int bound(int val, int min, int max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;

	return val;
}

static int __smb1360_read(struct smb1360_chip *chip, int reg,
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
	pr_debug("Reading 0x%02x=0x%02x\n", reg, *val);

	return 0;
}

static int __smb1360_write(struct smb1360_chip *chip, int reg,
						u8 val)
{
	s32 ret;

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

static int smb1360_read(struct smb1360_chip *chip, int reg,
				u8 *val)
{
	int rc;

	if (chip->skip_reads) {
		*val = 0;
		return 0;
	}
	mutex_lock(&chip->read_write_lock);
	rc = __smb1360_read(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb1360_write(struct smb1360_chip *chip, int reg,
						u8 val)
{
	int rc;

	if (chip->skip_writes)
		return 0;

	mutex_lock(&chip->read_write_lock);
	rc = __smb1360_write(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb1360_fg_read(struct smb1360_chip *chip, int reg,
				u8 *val)
{
	int rc;

	if (chip->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&chip->read_write_lock);
	chip->client->addr = chip->fg_i2c_addr;
	rc = __smb1360_read(chip, reg, val);
	chip->client->addr = chip->default_i2c_addr;
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb1360_fg_write(struct smb1360_chip *chip, int reg,
						u8 val)
{
	int rc;

	if (chip->skip_writes)
		return 0;

	mutex_lock(&chip->read_write_lock);
	chip->client->addr = chip->fg_i2c_addr;
	rc = __smb1360_write(chip, reg, val);
	chip->client->addr = chip->default_i2c_addr;
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb1360_read_bytes(struct smb1360_chip *chip, int reg,
						u8 *val, u8 bytes)
{
	s32 rc;

	if (chip->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&chip->read_write_lock);
	rc = i2c_smbus_read_i2c_block_data(chip->client, reg, bytes, val);
	if (rc < 0)
		dev_err(chip->dev,
			"i2c read fail: can't read %d bytes from %02x: %d\n",
							bytes, reg, rc);
	mutex_unlock(&chip->read_write_lock);

	return (rc < 0) ? rc : 0;
}

static int smb1360_write_bytes(struct smb1360_chip *chip, int reg,
						u8 *val, u8 bytes)
{
	s32 rc;

	if (chip->skip_writes) {
		*val = 0;
		return 0;
	}

	mutex_lock(&chip->read_write_lock);
	rc = i2c_smbus_write_i2c_block_data(chip->client, reg, bytes, val);
	if (rc < 0)
		dev_err(chip->dev,
			"i2c write fail: can't read %d bytes from %02x: %d\n",
							bytes, reg, rc);
	mutex_unlock(&chip->read_write_lock);

	return (rc < 0) ? rc : 0;
}

static int smb1360_masked_write(struct smb1360_chip *chip, int reg,
						u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	if (chip->skip_writes || chip->skip_reads)
		return 0;

	mutex_lock(&chip->read_write_lock);
	rc = __smb1360_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __smb1360_write(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

#define EXPONENT_MASK		0xF800
#define MANTISSA_MASK		0x3FF
#define SIGN_MASK		0x400
#define EXPONENT_SHIFT		11
#define MICRO_UNIT		1000000ULL
static int64_t float_decode(u16 reg)
{
	int64_t final_val, exponent_val, mantissa_val;
	int exponent, mantissa, n;
	bool sign;

	exponent = (reg & EXPONENT_MASK) >> EXPONENT_SHIFT;
	mantissa = (reg & MANTISSA_MASK);
	sign = !!(reg & SIGN_MASK);

	pr_debug("exponent=%d mantissa=%d sign=%d\n", exponent, mantissa, sign);

	mantissa_val = mantissa * MICRO_UNIT;

	n = exponent - 15;
	if (n < 0)
		exponent_val = MICRO_UNIT >> -n;
	else
		exponent_val = MICRO_UNIT << n;

	n = n - 10;
	if (n < 0)
		mantissa_val >>= -n;
	else
		mantissa_val <<= n;

	final_val = exponent_val + mantissa_val;

	if (sign)
		final_val *= -1;

	return final_val;
}

static int smb1360_enable_fg_access(struct smb1360_chip *chip)
{
	int rc;
	u8 reg = 0, timeout = 50;

	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT,
							FG_ACCESS_ENABLED_BIT);
	if (rc) {
		pr_err("Couldn't enable FG access rc=%d\n", rc);
		return rc;
	}

	while (timeout) {
		/* delay for FG access to be granted */
		msleep(200);
		rc = smb1360_read(chip, IRQ_I_REG, &reg);
		if (rc)
			pr_err("Could't read IRQ_I_REG rc=%d\n", rc);
		else if (reg & FG_ACCESS_ALLOWED_BIT)
			break;
		timeout--;
	}

	pr_debug("timeout=%d\n", timeout);

	if (!timeout)
		return -EBUSY;

	return 0;
}

static int smb1360_disable_fg_access(struct smb1360_chip *chip)
{
	int rc;

	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT, 0);
	if (rc)
		pr_err("Couldn't disable FG access rc=%d\n", rc);

	return rc;
}

static int smb1360_enable_volatile_writes(struct smb1360_chip *chip)
{
	int rc;

	rc = smb1360_masked_write(chip, CMD_I2C_REG,
		ALLOW_VOLATILE_BIT, ALLOW_VOLATILE_BIT);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set VOLATILE_W_PERM_BIT rc=%d\n", rc);

	return rc;
}

#define TRIM_1C_REG		0x1C
#define CHECK_USB100_GOOD_BIT	BIT(6)
static bool is_usb100_broken(struct smb1360_chip *chip)
{
	int rc;
	u8 reg;

	rc = smb1360_read(chip, TRIM_1C_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read trim 1C reg rc = %d\n", rc);
		return rc;
	}
	return !!(reg & CHECK_USB100_GOOD_BIT);
}

static int read_revision(struct smb1360_chip *chip, u8 *revision)
{
	int rc;

	*revision = 0;
	rc = smb1360_read(chip, REVISION_CTRL_REG, revision);
	if (rc)
		dev_err(chip->dev, "Couldn't read REVISION_CTRL_REG rc=%d", rc);

	*revision &= DEVICE_REV_MASK;

	return rc;
}

#define MIN_FLOAT_MV		3460
#define MAX_FLOAT_MV		4730
#define VFLOAT_STEP_MV		10
static int smb1360_float_voltage_set(struct smb1360_chip *chip, int vfloat_mv)
{
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	temp = (vfloat_mv - MIN_FLOAT_MV) / VFLOAT_STEP_MV;

	return smb1360_masked_write(chip, BATT_CHG_FLT_VTG_REG,
				VFLOAT_MASK, temp);
}

#define MIN_RECHG_MV		50
#define MAX_RECHG_MV		300
static int smb1360_recharge_threshold_set(struct smb1360_chip *chip,
							int resume_mv)
{
	u8 temp;

	if ((resume_mv < MIN_RECHG_MV) || (resume_mv > MAX_RECHG_MV)) {
		dev_err(chip->dev, "bad rechg_thrsh =%d asked to set\n",
							resume_mv);
		return -EINVAL;
	}

	temp = resume_mv / 100;

	return smb1360_masked_write(chip, CFG_BATT_CHG_REG,
		RECHG_MV_MASK, temp << RECHG_MV_SHIFT);
}

static int __smb1360_charging_disable(struct smb1360_chip *chip, bool disable)
{
	int rc;

	rc = smb1360_masked_write(chip, CMD_CHG_REG,
			CMD_CHG_EN, disable ? 0 : CMD_CHG_EN);
	if (rc < 0)
		pr_err("Couldn't set CHG_ENABLE_BIT disable=%d rc = %d\n",
							disable, rc);
	else
		pr_debug("CHG_EN status=%d\n", !disable);

	return rc;
}

static int smb1360_charging_disable(struct smb1360_chip *chip, int reason,
								int disable)
{
	int rc = 0;
	int disabled;

	mutex_lock(&chip->charging_disable_lock);

	disabled = chip->charging_disabled_status;

	pr_debug("reason=%d requested_disable=%d disabled_status=%d\n",
					reason, disable, disabled);

	if (disable == true)
		disabled |= reason;
	else
		disabled &= ~reason;

	if (disabled)
		rc = __smb1360_charging_disable(chip, true);
	else
		rc = __smb1360_charging_disable(chip, false);

	if (rc)
		pr_err("Couldn't disable charging for reason=%d rc=%d\n",
							rc, reason);
	else
		chip->charging_disabled_status = disabled;

	mutex_unlock(&chip->charging_disable_lock);

	return rc;
}

static enum power_supply_property smb1360_battery_properties[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
};

static int smb1360_get_prop_batt_present(struct smb1360_chip *chip)
{
	return chip->batt_present;
}

static int smb1360_get_prop_batt_status(struct smb1360_chip *chip)
{
	int rc;
	u8 reg = 0, chg_type;

	if (chip->batt_full)
		return POWER_SUPPLY_STATUS_FULL;

	rc = smb1360_read(chip, STATUS_3_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_3_REG rc=%d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	pr_debug("STATUS_3_REG = %x\n", reg);

	if (reg & CHG_HOLD_OFF_BIT)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else
		return POWER_SUPPLY_STATUS_CHARGING;
}

static int smb1360_get_prop_charging_status(struct smb1360_chip *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb1360_read(chip, STATUS_3_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_3_REG rc=%d\n", rc);
		return 0;
	}

	return (reg & CHG_EN_BIT) ? 1 : 0;
}

static int smb1360_get_prop_charge_type(struct smb1360_chip *chip)
{
	int rc;
	u8 reg = 0;
	u8 chg_type;

	rc = smb1360_read(chip, STATUS_3_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_3_REG rc=%d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if ((chg_type == BATT_FAST_CHG_VAL) ||
			(chg_type == BATT_TAPER_CHG_VAL))
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb1360_get_prop_batt_health(struct smb1360_chip *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->batt_hot)
		ret.intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		ret.intval = POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		ret.intval = POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		ret.intval = POWER_SUPPLY_HEALTH_COOL;
	else
		ret.intval = POWER_SUPPLY_HEALTH_GOOD;

	return ret.intval;
}

static int smb1360_get_prop_batt_capacity(struct smb1360_chip *chip)
{
	u8 reg;
	u32 temp = 0;
	int rc, soc = 0;

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	if (chip->empty_soc) {
		pr_debug("empty_soc\n");
		return 0;
	}

	rc = smb1360_read(chip, SHDW_FG_MSYS_SOC, &reg);
	if (rc) {
		pr_err("Failed to read FG_MSYS_SOC rc=%d\n", rc);
		return rc;
	}
	soc = (100 * reg) / MAX_8_BITS;

	temp = (100 * reg) % MAX_8_BITS;
	if (temp > (MAX_8_BITS / 2))
		soc += 1;

	pr_debug("msys_soc_reg=0x%02x, fg_soc=%d batt_full = %d\n", reg,
						soc, chip->batt_full);

	return chip->batt_full ? 100 : bound(soc, 0, 100);
}

static int smb1360_get_prop_chg_full_design(struct smb1360_chip *chip)
{
	u8 reg[2];
	int rc, fcc_mah = 0;

	rc = smb1360_read_bytes(chip, SHDW_FG_CAPACITY, reg, 2);
	if (rc) {
		pr_err("Failed to read SHDW_FG_CAPACITY rc=%d\n", rc);
		return rc;
	}
	fcc_mah = (reg[1] << 8) | reg[0];

	pr_debug("reg[0]=0x%02x reg[1]=0x%02x fcc_mah=%d\n",
				reg[0], reg[1], fcc_mah);

	return fcc_mah * 1000;
}

static int smb1360_get_prop_batt_temp(struct smb1360_chip *chip)
{
	u8 reg[2];
	int rc, temp = 0;

	rc = smb1360_read_bytes(chip, SHDW_FG_BATT_TEMP, reg, 2);
	if (rc) {
		pr_err("Failed to read SHDW_FG_BATT_TEMP rc=%d\n", rc);
		return rc;
	}

	temp = (reg[1] << 8) | reg[0];
	temp = div_u64(temp * 625, 10000UL);	/* temperature in kelvin */
	temp = (temp - 273) * 10;		/* temperature in decideg */

	pr_debug("reg[0]=0x%02x reg[1]=0x%02x temperature=%d\n",
					reg[0], reg[1], temp);

	return temp;
}

static int smb1360_get_prop_voltage_now(struct smb1360_chip *chip)
{
	u8 reg[2];
	int rc, temp = 0;

	rc = smb1360_read_bytes(chip, SHDW_FG_VTG_NOW, reg, 2);
	if (rc) {
		pr_err("Failed to read SHDW_FG_VTG_NOW rc=%d\n", rc);
		return rc;
	}

	temp = (reg[1] << 8) | reg[0];
	temp = div_u64(temp * 5000, 0x7FFF);

	pr_debug("reg[0]=0x%02x reg[1]=0x%02x voltage=%d\n",
				reg[0], reg[1], temp * 1000);

	return temp * 1000;
}

static int smb1360_get_prop_batt_resistance(struct smb1360_chip *chip)
{
	u8 reg[2];
	u16 temp;
	int rc;
	int64_t resistance;

	rc = smb1360_read_bytes(chip, SHDW_FG_ESR_ACTUAL, reg, 2);
	if (rc) {
		pr_err("Failed to read FG_ESR_ACTUAL rc=%d\n", rc);
		return rc;
	}
	temp = (reg[1] << 8) | reg[0];

	resistance = float_decode(temp) * 2;

	pr_debug("reg=0x%02x resistance=%lld\n", temp, resistance);

	/* resistance in uohms */
	return resistance;
}

static int smb1360_get_prop_current_now(struct smb1360_chip *chip)
{
	u8 reg[2];
	int rc, temp = 0;

	rc = smb1360_read_bytes(chip, SHDW_FG_CURR_NOW, reg, 2);
	if (rc) {
		pr_err("Failed to read SHDW_FG_CURR_NOW rc=%d\n", rc);
		return rc;
	}

	temp = ((s8)reg[1] << 8) | reg[0];
	temp = div_s64(temp * 2500, 0x7FFF);

	pr_debug("reg[0]=0x%02x reg[1]=0x%02x current=%d\n",
				reg[0], reg[1], temp * 1000);

	return temp * 1000;
}

static int smb1360_set_minimum_usb_current(struct smb1360_chip *chip)
{
	int rc = 0;

	pr_debug("set USB current to minimum\n");
	/* set input current limit to minimum (300mA)*/
	rc = smb1360_masked_write(chip, CFG_BATT_CHG_ICL_REG,
					INPUT_CURR_LIM_MASK,
					INPUT_CURR_LIM_300MA);
	if (rc)
		pr_err("Couldn't set ICL mA rc=%d\n", rc);

	if (!(chip->workaround_flags & WRKRND_USB100_FAIL)) {
		rc = smb1360_masked_write(chip, CMD_IL_REG,
				USB_CTRL_MASK, USB_100_BIT);
		if (rc)
			pr_err("Couldn't configure for USB100 rc=%d\n", rc);
	}

	return rc;
}

static int smb1360_set_appropriate_usb_current(struct smb1360_chip *chip)
{
	int rc = 0, i, therm_ma, current_ma;
	int path_current = chip->usb_psy_ma;

	/*
	 * If battery is absent do not modify the current at all, these
	 * would be some appropriate values set by the bootloader or default
	 * configuration and since it is the only source of power we should
	 * not change it
	 */
	if (!chip->batt_present) {
		pr_debug("ignoring current request since battery is absent\n");
		return 0;
	}

	if (chip->therm_lvl_sel > 0
			&& chip->therm_lvl_sel < (chip->thermal_levels - 1))
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = chip->thermal_mitigation[chip->therm_lvl_sel];
	else
		therm_ma = path_current;

	current_ma = min(therm_ma, path_current);

	if (current_ma <= 2) {
		/*
		 * SMB1360 does not support USB suspend -
		 * so set the current-limit to minimum in suspend.
		 */
		pr_debug("current_ma=%d <= 2 set USB current to minimum\n",
								current_ma);
		rc = smb1360_set_minimum_usb_current(chip);
		if (rc < 0)
			pr_err("Couldn't to set minimum USB current rc = %d\n",
								rc);

		return rc;
	}

	for (i = ARRAY_SIZE(input_current_limit) - 1; i >= 0; i--) {
		if (input_current_limit[i] <= current_ma)
			break;
	}
	if (i < 0) {
		pr_debug("Couldn't find ICL mA rc=%d\n", rc);
		i = 0;
	}
	/* set input current limit */
	rc = smb1360_masked_write(chip, CFG_BATT_CHG_ICL_REG,
					INPUT_CURR_LIM_MASK, i);
	if (rc)
		pr_err("Couldn't set ICL mA rc=%d\n", rc);

	pr_debug("ICL set to = %d\n", input_current_limit[i]);

	if ((current_ma <= CURRENT_100_MA) &&
		(chip->workaround_flags & WRKRND_USB100_FAIL)) {
		pr_debug("usb100 not supported\n");
		current_ma = CURRENT_500_MA;
	}

	if (current_ma <= CURRENT_100_MA) {
		/* USB 100 */
		rc = smb1360_masked_write(chip, CMD_IL_REG,
				USB_CTRL_MASK, USB_100_BIT);
		if (rc)
			pr_err("Couldn't configure for USB100 rc=%d\n", rc);
		pr_debug("Setting USB 100\n");
	} else if (current_ma <= CURRENT_500_MA) {
		/* USB 500 */
		rc = smb1360_masked_write(chip, CMD_IL_REG,
				USB_CTRL_MASK, USB_500_BIT);
		if (rc)
			pr_err("Couldn't configure for USB500 rc=%d\n", rc);
		pr_debug("Setting USB 500\n");
	} else {
		/* USB AC */
		for (i = ARRAY_SIZE(fastchg_current) - 1; i >= 0; i--) {
			if (fastchg_current[i] <= current_ma)
				break;
		}
		if (i < 0) {
			pr_debug("Couldn't find fastchg mA rc=%d\n", rc);
			i = 0;
		}
		/* set fastchg limit */
		rc = smb1360_masked_write(chip, CHG_CURRENT_REG,
			FASTCHG_CURR_MASK, i << FASTCHG_CURR_SHIFT);
		if (rc)
			pr_err("Couldn't set fastchg mA rc=%d\n", rc);

		rc = smb1360_masked_write(chip, CMD_IL_REG,
				USB_CTRL_MASK, USB_AC_BIT);
		if (rc)
			pr_err("Couldn't configure for USB AC rc=%d\n", rc);

		pr_debug("fast-chg current set to = %d\n", fastchg_current[i]);
	}

	return rc;
}

static int smb1360_system_temp_level_set(struct smb1360_chip *chip,
							int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->thermal_mitigation) {
		pr_err("Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		pr_err("Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->thermal_levels) {
		pr_err("Unsupported level selected %d forcing %d\n", lvl_sel,
				chip->thermal_levels - 1);
		lvl_sel = chip->thermal_levels - 1;
	}

	if (lvl_sel == chip->therm_lvl_sel)
		return 0;

	mutex_lock(&chip->current_change_lock);
	prev_therm_lvl = chip->therm_lvl_sel;
	chip->therm_lvl_sel = lvl_sel;

	if (chip->therm_lvl_sel == (chip->thermal_levels - 1)) {
		rc = smb1360_set_minimum_usb_current(chip);
		if (rc)
			pr_err("Couldn't set USB current to minimum rc = %d\n",
							rc);
	} else {
		rc = smb1360_set_appropriate_usb_current(chip);
		if (rc)
			pr_err("Couldn't set USB current rc = %d\n", rc);
	}

	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smb1360_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smb1360_chip *chip = container_of(psy,
				struct smb1360_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb1360_charging_disable(chip, USER, !val->intval);
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		pr_info("fake_soc set to %d\n", chip->fake_battery_soc);
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smb1360_system_temp_level_set(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb1360_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smb1360_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb1360_chip *chip = container_of(psy,
				struct smb1360_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb1360_get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb1360_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb1360_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = smb1360_get_prop_charging_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb1360_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smb1360_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = smb1360_get_prop_chg_full_design(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = smb1360_get_prop_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = smb1360_get_prop_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = smb1360_get_prop_batt_resistance(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = smb1360_get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->therm_lvl_sel;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void smb1360_external_power_changed(struct power_supply *psy)
{
	struct smb1360_chip *chip = container_of(psy,
				struct smb1360_chip, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0;

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	pr_debug("current_limit = %d\n", current_limit);

	if (chip->usb_psy_ma != current_limit) {
		mutex_lock(&chip->current_change_lock);
		chip->usb_psy_ma = current_limit;
		rc = smb1360_set_appropriate_usb_current(chip);
		if (rc < 0)
			pr_err("Couldn't set usb current rc = %d\n", rc);
		mutex_unlock(&chip->current_change_lock);
	}

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc < 0)
		pr_err("could not read USB ONLINE property, rc=%d\n", rc);

	/* update online property */
	rc = 0;
	if (chip->usb_present && !chip->charging_disabled_status
					&& chip->usb_psy_ma != 0) {
		if (prop.intval == 0)
			rc = power_supply_set_online(chip->usb_psy, true);
	} else {
		if (prop.intval == 1)
			rc = power_supply_set_online(chip->usb_psy, false);
	}
	if (rc < 0)
		pr_err("could not set usb online, rc=%d\n", rc);
}

static int hot_hard_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_hot = !!rt_stat;
	return 0;
}

static int cold_hard_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cold = !!rt_stat;
	return 0;
}

static int hot_soft_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_warm = !!rt_stat;
	return 0;
}

static int cold_soft_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cool = !!rt_stat;
	return 0;
}

static int battery_missing_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_present = !rt_stat;
	return 0;
}

static int vbat_low_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("vbat low\n");

	return 0;
}

static int chg_hot_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_warn_ratelimited("chg hot\n");
	return 0;
}

static int chg_term_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_full = !!rt_stat;
	return 0;
}

static int usbin_uv_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	bool usb_present = !rt_stat;

	pr_debug("chip->usb_present = %d usb_present = %d\n",
				chip->usb_present, usb_present);
	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		power_supply_set_present(chip->usb_psy, usb_present);
	}

	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		power_supply_set_present(chip->usb_psy, usb_present);
	}

	return 0;
}

static int chg_inhibit_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	/*
	 * charger is inserted when the battery voltage is high
	 * so h/w won't start charging just yet. Treat this as
	 * battery full
	 */
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_full = !!rt_stat;
	return 0;
}

static int delta_soc_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("SOC changed! - rt_stat = 0x%02x\n", rt_stat);

	return 0;
}

static int min_soc_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("SOC dropped below min SOC\n");

	return 0;
}

static int empty_soc_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	if (rt_stat) {
		pr_warn_ratelimited("SOC is 0\n");
		chip->empty_soc = true;
		pm_stay_awake(chip->dev);
	} else {
		chip->empty_soc = false;
		pm_relax(chip->dev);
	}

	return 0;
}

static int full_soc_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	if (rt_stat)
		pr_debug("SOC is 100\n");

	return 0;
}

static int fg_access_allowed_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("stat=%d\n", !!rt_stat);

	return 0;
}

static int batt_id_complete_handler(struct smb1360_chip *chip, u8 rt_stat)
{
	pr_debug("batt_id = %x\n", (rt_stat & BATT_ID_RESULT_BIT)
						>> BATT_ID_SHIFT);

	return 0;
}

struct smb_irq_info {
	const char		*name;
	int			(*smb_irq)(struct smb1360_chip *chip,
							u8 rt_stat);
	int			high;
	int			low;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	struct smb_irq_info	irq_info[4];
};

static struct irq_handler_info handlers[] = {
	{IRQ_A_REG, 0, 0,
		{
			{
				.name		= "cold_soft",
				.smb_irq	= cold_soft_handler,
			},
			{
				.name		= "hot_soft",
				.smb_irq	= hot_soft_handler,
			},
			{
				.name		= "cold_hard",
				.smb_irq	= cold_hard_handler,
			},
			{
				.name		= "hot_hard",
				.smb_irq	= hot_hard_handler,
			},
		},
	},
	{IRQ_B_REG, 0, 0,
		{
			{
				.name		= "chg_hot",
				.smb_irq	= chg_hot_handler,
			},
			{
				.name		= "vbat_low",
				.smb_irq	= vbat_low_handler,
			},
			{
				.name		= "battery_missing",
				.smb_irq	= battery_missing_handler,
			},
			{
				.name		= "battery_missing",
				.smb_irq	= battery_missing_handler,
			},
		},
	},
	{IRQ_C_REG, 0, 0,
		{
			{
				.name		= "chg_term",
				.smb_irq	= chg_term_handler,
			},
			{
				.name		= "taper",
			},
			{
				.name		= "recharge",
			},
			{
				.name		= "fast_chg",
			},
		},
	},
	{IRQ_D_REG, 0, 0,
		{
			{
				.name		= "prechg_timeout",
			},
			{
				.name		= "safety_timeout",
			},
			{
				.name		= "aicl_done",
			},
			{
				.name		= "battery_ov",
			},
		},
	},
	{IRQ_E_REG, 0, 0,
		{
			{
				.name		= "usbin_uv",
				.smb_irq	= usbin_uv_handler,
			},
			{
				.name		= "usbin_ov",
			},
			{
				.name		= "unused",
			},
			{
				.name		= "chg_inhibit",
				.smb_irq	= chg_inhibit_handler,
			},
		},
	},
	{IRQ_F_REG, 0, 0,
		{
			{
				.name		= "power_ok",
			},
			{
				.name		= "unused",
			},
			{
				.name		= "otg_fail",
			},
			{
				.name		= "otg_oc",
			},
		},
	},
	{IRQ_G_REG, 0, 0,
		{
			{
				.name		= "delta_soc",
				.smb_irq	= delta_soc_handler,
			},
			{
				.name		= "chg_error",
			},
			{
				.name		= "wd_timeout",
			},
			{
				.name		= "unused",
			},
		},
	},
	{IRQ_H_REG, 0, 0,
		{
			{
				.name		= "min_soc",
				.smb_irq	= min_soc_handler,
			},
			{
				.name		= "max_soc",
			},
			{
				.name		= "empty_soc",
				.smb_irq	= empty_soc_handler,
			},
			{
				.name		= "full_soc",
				.smb_irq	= full_soc_handler,
			},
		},
	},
	{IRQ_I_REG, 0, 0,
		{
			{
				.name		= "fg_access_allowed",
				.smb_irq	= fg_access_allowed_handler,
			},
			{
				.name		= "fg_data_recovery",
			},
			{
				.name		= "batt_id_complete",
				.smb_irq	= batt_id_complete_handler,
			},
		},
	},
};

#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BATT_ID_LATCHED_MASK	0x08
#define BATT_ID_STATUS_MASK	0x07
#define BITS_PER_IRQ		2
static irqreturn_t smb1360_stat_handler(int irq, void *dev_id)
{
	struct smb1360_chip *chip = dev_id;
	int i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat, prev_rt_stat, irq_latched_mask, irq_status_mask;
	int rc;
	int handler_count = 0;

	mutex_lock(&chip->irq_complete);
	chip->irq_waiting = true;
	if (!chip->resume_completed) {
		dev_dbg(chip->dev, "IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}
	chip->irq_waiting = false;

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb1360_read(chip, handlers[i].stat_reg,
					&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}

		for (j = 0; j < ARRAY_SIZE(handlers[i].irq_info); j++) {
			if (handlers[i].stat_reg == IRQ_I_REG && j == 2) {
				irq_latched_mask = BATT_ID_LATCHED_MASK;
				irq_status_mask = BATT_ID_STATUS_MASK;
			} else {
				irq_latched_mask = IRQ_LATCHED_MASK;
				irq_status_mask = IRQ_STATUS_MASK;
			}
			triggered = handlers[i].val
			       & (irq_latched_mask << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (irq_status_mask << (j * BITS_PER_IRQ));
			prev_rt_stat = handlers[i].prev_val
				& (irq_status_mask << (j * BITS_PER_IRQ));
			changed = prev_rt_stat ^ rt_stat;

			if (triggered || changed)
				rt_stat ? handlers[i].irq_info[j].high++ :
						handlers[i].irq_info[j].low++;

			if ((triggered || changed)
				&& handlers[i].irq_info[j].smb_irq != NULL) {
				handler_count++;
				rc = handlers[i].irq_info[j].smb_irq(chip,
								rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	pr_debug("handler count = %d\n", handler_count);
	if (handler_count)
		power_supply_changed(&chip->batt_psy);

	mutex_unlock(&chip->irq_complete);

	return IRQ_HANDLED;
}

static int show_irq_count(struct seq_file *m, void *data)
{
	int i, j, total = 0;

	for (i = 0; i < ARRAY_SIZE(handlers); i++)
		for (j = 0; j < 4; j++) {
			if (!handlers[i].irq_info[j].name)
				continue;
			seq_printf(m, "%s=%d\t(high=%d low=%d)\n",
						handlers[i].irq_info[j].name,
						handlers[i].irq_info[j].high
						+ handlers[i].irq_info[j].low,
						handlers[i].irq_info[j].high,
						handlers[i].irq_info[j].low);
			total += (handlers[i].irq_info[j].high
					+ handlers[i].irq_info[j].low);
		}

	seq_printf(m, "\n\tTotal = %d\n", total);

	return 0;
}

static int irq_count_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

	return single_open(file, show_irq_count, chip);
}

static const struct file_operations irq_count_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= irq_count_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct smb1360_chip *chip = data;
	int rc;
	u8 temp;

	rc = smb1360_read(chip, chip->peek_poke_address, &temp);
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
	struct smb1360_chip *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = smb1360_write(chip, chip->peek_poke_address, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			chip->peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int smb1360_select_fg_i2c_address(struct smb1360_chip *chip)
{
	unsigned short addr = chip->default_i2c_addr << 0x1;

	switch (chip->fg_access_type) {
	case FG_ACCESS_CFG:
		addr = (addr & ~FG_I2C_CFG_MASK) | FG_CFG_I2C_ADDR;
		break;
	case FG_ACCESS_PROFILE_A:
		addr = (addr & ~FG_I2C_CFG_MASK) | FG_PROFILE_A_ADDR;
		break;
	case FG_ACCESS_PROFILE_B:
		addr = (addr & ~FG_I2C_CFG_MASK) | FG_PROFILE_B_ADDR;
		break;
	default:
		pr_err("Invalid FG access type=%d\n", chip->fg_access_type);
		return -EINVAL;
	}

	chip->fg_i2c_addr = addr >> 0x1;
	pr_debug("FG_access_type=%d fg_i2c_addr=%x\n", chip->fg_access_type,
							chip->fg_i2c_addr);

	return 0;
}

static int fg_get_reg(void *data, u64 *val)
{
	struct smb1360_chip *chip = data;
	int rc;
	u8 temp;

	rc = smb1360_select_fg_i2c_address(chip);
	if (rc) {
		pr_err("Unable to set FG access I2C address\n");
		return -EINVAL;
	}

	rc = smb1360_fg_read(chip, chip->fg_peek_poke_address, &temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't read reg %x rc = %d\n",
			chip->fg_peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int fg_set_reg(void *data, u64 val)
{
	struct smb1360_chip *chip = data;
	int rc;
	u8 temp;

	rc = smb1360_select_fg_i2c_address(chip);
	if (rc) {
		pr_err("Unable to set FG access I2C address\n");
		return -EINVAL;
	}

	temp = (u8) val;
	rc = smb1360_fg_write(chip, chip->fg_peek_poke_address, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			chip->fg_peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fg_poke_poke_debug_ops, fg_get_reg,
				fg_set_reg, "0x%02llx\n");

#define LAST_CNFG_REG	0x17
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smb1360_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb1360_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

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
	struct smb1360_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb1360_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cmd_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

	return single_open(file, show_cmd_regs, chip);
}

static const struct file_operations cmd_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cmd_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_STATUS_REG	0x48
#define LAST_STATUS_REG		0x4B
static int show_status_regs(struct seq_file *m, void *data)
{
	struct smb1360_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb1360_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int status_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

	return single_open(file, show_status_regs, chip);
}

static const struct file_operations status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= status_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_IRQ_REG		0x50
#define LAST_IRQ_REG		0x58
static int show_irq_stat_regs(struct seq_file *m, void *data)
{
	struct smb1360_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_IRQ_REG; addr <= LAST_IRQ_REG; addr++) {
		rc = smb1360_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int irq_stat_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

	return single_open(file, show_irq_stat_regs, chip);
}

static const struct file_operations irq_stat_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= irq_stat_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int data_8(u8 *reg)
{
	return reg[0];
}
static int data_16(u8 *reg)
{
	return (reg[1] << 8) | reg[0];
}
static int data_24(u8 *reg)
{
	return  (reg[2] << 16) | (reg[1] << 8) | reg[0];
}
static int data_28(u8 *reg)
{
	return  ((reg[3] & 0xF) << 24) | (reg[2] << 16) |
					(reg[1] << 8) | reg[0];
}
static int data_32(u8 *reg)
{
	return  (reg[3]  << 24) | (reg[2] << 16) |
				(reg[1] << 8) | reg[0];
}

struct fg_regs {
	int index;
	int length;
	char *param_name;
	int (*calc_func) (u8 *);
};

static struct fg_regs fg_scratch_pad[] = {
	{0, 2, "v_current_predicted", data_16},
	{2, 2, "v_cutoff_predicted", data_16},
	{4, 2, "v_full_predicted", data_16},
	{6, 2, "ocv_estimate", data_16},
	{8, 2, "rslow_drop", data_16},
	{10, 2, "voltage_old", data_16},
	{12, 2, "current_old", data_16},
	{14, 4, "current_average_full", data_32},
	{18, 2, "temperature", data_16},
	{20, 2, "temp_last_track", data_16},
	{22, 2, "ESR_nominal", data_16},
	{26, 2, "Rslow", data_16},
	{28, 2, "counter_imptr", data_16},
	{30, 2, "counter_pulse", data_16},
	{32, 1, "IRQ_delta_prev", data_8},
	{33, 1, "cap_learning_counter", data_8},
	{34, 4, "Vact_int_error", data_24},
	{38, 3, "SOC_cutoff", data_24},
	{41, 3, "SOC_full", data_24},
	{44, 3, "SOC_auto_rechrge_temp", data_24},
	{47, 3, "Battery_SOC", data_24},
	{50, 4, "CC_SOC", data_28},
	{54, 2, "SOC_filtered", data_16},
	{56, 2, "SOC_Monotonic", data_16},
	{58, 2, "CC_SOC_coeff", data_16},
	{60, 2, "nominal_capacity", data_16},
	{62, 2, "actual_capacity", data_16},
	{68, 1, "temperature_counter", data_8},
	{69, 3, "Vbatt_filtered", data_24},
	{72, 3, "Ibatt_filtered", data_24},
	{75, 2, "Current_CC_shadow", data_16},
	{79, 2, "Ibatt_standby", data_16},
	{82, 1, "Auto_recharge_SOC_threshold", data_8},
	{83, 2, "System_cutoff_voltage", data_16},
	{85, 2, "System_CC_to_CV_voltage", data_16},
	{87, 2, "System_term_current", data_16},
	{89, 2, "System_fake_term_current", data_16},
};

static struct fg_regs fg_cfg[] = {
	{0, 2, "ESR_actual", data_16},
	{4, 1, "IRQ_SOC_max", data_8},
	{5, 1, "IRQ_SOC_min", data_8},
	{6, 1, "IRQ_volt_empty", data_8},
	{7, 1, "Temp_external", data_8},
	{8, 1, "IRQ_delta_threshold", data_8},
	{9, 1, "JIETA_soft_cold", data_8},
	{10, 1, "JIETA_soft_hot", data_8},
	{11, 1, "IRQ_volt_min", data_8},
	{14, 2, "ESR_sys_replace", data_16},
};

static struct fg_regs fg_shdw[] = {
	{0, 1, "Latest_battery_info", data_8},
	{1, 1, "Latest_Msys_SOC", data_8},
	{2, 2, "Battery_capacity", data_16},
	{4, 2, "Rslow_drop", data_16},
	{6, 1, "Latest_SOC", data_8},
	{7, 1, "Latest_Cutoff_SOC", data_8},
	{8, 1, "Latest_full_SOC", data_8},
	{9, 2, "Voltage_shadow", data_16},
	{11, 2, "Current_shadow", data_16},
	{13, 2, "Latest_temperature", data_16},
	{15, 1, "Latest_system_sbits", data_8},
};

#define FIRST_FG_CFG_REG		0x20
#define LAST_FG_CFG_REG			0x2F
#define FIRST_FG_SHDW_REG		0x60
#define LAST_FG_SHDW_REG		0x6F
#define FG_SCRATCH_PAD_MAX		90
#define FG_SCRATCH_PAD_BASE_REG		0x80
#define SMB1360_I2C_READ_LENGTH		32

static int smb1360_check_cycle_stretch(struct smb1360_chip *chip)
{
	int rc = 0;
	u8 reg;

	rc = smb1360_read(chip, STATUS_4_REG, &reg);
	if (rc) {
		pr_err("Unable to read status regiseter\n");
	} else if (reg & CYCLE_STRETCH_ACTIVE_BIT) {
		/* clear cycle stretch */
		rc = smb1360_masked_write(chip, CMD_I2C_REG,
			CYCLE_STRETCH_CLEAR_BIT, CYCLE_STRETCH_CLEAR_BIT);
		if (rc)
			pr_err("Unable to clear cycle stretch\n");
	}

	return rc;
}

static int show_fg_regs(struct seq_file *m, void *data)
{
	struct smb1360_chip *chip = m->private;
	int rc, i , j, rem_length;
	u8 reg[FG_SCRATCH_PAD_MAX];

	rc = smb1360_check_cycle_stretch(chip);
	if (rc)
		pr_err("Unable to check cycle-stretch\n");

	rc = smb1360_enable_fg_access(chip);
	if (rc) {
		pr_err("Couldn't request FG access rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < (FG_SCRATCH_PAD_MAX / SMB1360_I2C_READ_LENGTH); i++) {
		j = i * SMB1360_I2C_READ_LENGTH;
		rc = smb1360_read_bytes(chip, FG_SCRATCH_PAD_BASE_REG + j,
					&reg[j], SMB1360_I2C_READ_LENGTH);
		if (rc) {
			pr_err("Couldn't read scratch registers rc=%d\n", rc);
			break;
		}
	}

	j = i * SMB1360_I2C_READ_LENGTH;
	rem_length = FG_SCRATCH_PAD_MAX % SMB1360_I2C_READ_LENGTH;
	if (rem_length) {
		rc = smb1360_read_bytes(chip, FG_SCRATCH_PAD_BASE_REG + j,
						&reg[j], rem_length);
		if (rc)
			pr_err("Couldn't read scratch registers rc=%d\n", rc);
	}

	rc = smb1360_disable_fg_access(chip);
	if (rc) {
		pr_err("Couldn't disable FG access rc=%d\n", rc);
		return rc;
	}

	rc = smb1360_check_cycle_stretch(chip);
	if (rc)
		pr_err("Unable to check cycle-stretch\n");


	seq_puts(m, "FG scratch-pad registers\n");
	for (i = 0; i < ARRAY_SIZE(fg_scratch_pad); i++)
		seq_printf(m, "\t%s = %x\n", fg_scratch_pad[i].param_name,
		fg_scratch_pad[i].calc_func(&reg[fg_scratch_pad[i].index]));

	rem_length = LAST_FG_CFG_REG - FIRST_FG_CFG_REG + 1;
	rc = smb1360_read_bytes(chip, FIRST_FG_CFG_REG,
					&reg[0], rem_length);
	if (rc)
		pr_err("Couldn't read config registers rc=%d\n", rc);

	seq_puts(m, "FG config registers\n");
	for (i = 0; i < ARRAY_SIZE(fg_cfg); i++)
		seq_printf(m, "\t%s = %x\n", fg_cfg[i].param_name,
				fg_cfg[i].calc_func(&reg[fg_cfg[i].index]));

	rem_length = LAST_FG_SHDW_REG - FIRST_FG_SHDW_REG + 1;
	rc = smb1360_read_bytes(chip, FIRST_FG_SHDW_REG,
					&reg[0], rem_length);
	if (rc)
		pr_err("Couldn't read shadow registers rc=%d\n", rc);

	seq_puts(m, "FG shadow registers\n");
	for (i = 0; i < ARRAY_SIZE(fg_shdw); i++)
		seq_printf(m, "\t%s = %x\n", fg_shdw[i].param_name,
				fg_shdw[i].calc_func(&reg[fg_shdw[i].index]));

	return rc;
}

static int fg_regs_open(struct inode *inode, struct file *file)
{
	struct smb1360_chip *chip = inode->i_private;

	return single_open(file, show_fg_regs, chip);
}

static const struct file_operations fg_regs_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= fg_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int smb1360_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb1360_chip *chip = rdev_get_drvdata(rdev);

	rc = smb1360_masked_write(chip, CMD_CHG_REG, CMD_OTG_EN_BIT,
						CMD_OTG_EN_BIT);
	if (rc)
		pr_err("Couldn't enable  OTG mode rc=%d\n", rc);

	return rc;
}

static int smb1360_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb1360_chip *chip = rdev_get_drvdata(rdev);

	rc = smb1360_masked_write(chip, CMD_CHG_REG, CMD_OTG_EN_BIT, 0);
	if (rc)
		pr_err("Couldn't disable OTG mode rc=%d\n", rc);

	return rc;
}

static int smb1360_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	u8 reg = 0;
	int rc = 0;
	struct smb1360_chip *chip = rdev_get_drvdata(rdev);

	rc = smb1360_read(chip, CMD_CHG_REG, &reg);
	if (rc) {
		pr_err("Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return  (reg & CMD_OTG_EN_BIT) ? 1 : 0;
}

struct regulator_ops smb1360_otg_reg_ops = {
	.enable		= smb1360_otg_regulator_enable,
	.disable	= smb1360_otg_regulator_disable,
	.is_enabled	= smb1360_otg_regulator_is_enable,
};

static int smb1360_regulator_init(struct smb1360_chip *chip)
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
		chip->otg_vreg.rdesc.ops = &smb1360_otg_reg_ops;
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

static int smb1360_check_batt_profile(struct smb1360_chip *chip)
{
	int rc, i, timeout = 50;
	u8 reg = 0, loaded_profile, new_profile = 0, bid_mask;

	if (!chip->connected_rid) {
		pr_debug("Skip batt-profile loading connected_rid=%d\n",
						chip->connected_rid);
		return 0;
	}

	rc = smb1360_read(chip, SHDW_FG_BATT_STATUS, &reg);
	if (rc) {
		pr_err("Couldn't read FG_BATT_STATUS rc=%d\n", rc);
		goto fail_profile;
	}

	loaded_profile = !!(reg & BATTERY_PROFILE_BIT) ?
			BATTERY_PROFILE_B : BATTERY_PROFILE_A;

	pr_debug("fg_batt_status=%x loaded_profile=%d\n", reg, loaded_profile);

	for (i = 0; i < BATTERY_PROFILE_MAX; i++) {
		pr_debug("profile=%d profile_rid=%d connected_rid=%d\n", i,
						chip->profile_rid[i],
						chip->connected_rid);
		if (abs(chip->profile_rid[i] - chip->connected_rid) <
				(div_u64(chip->connected_rid, 10)))
			break;
	}

	if (i == BATTERY_PROFILE_MAX) {
		pr_err("None of the battery-profiles match the connected-RID\n");
		return 0;
	} else {
		if (i == loaded_profile) {
			pr_debug("Loaded Profile-RID == connected-RID\n");
			return 0;
		} else {
			new_profile = (loaded_profile == BATTERY_PROFILE_A) ?
					BATTERY_PROFILE_B : BATTERY_PROFILE_A;
			bid_mask = (new_profile == BATTERY_PROFILE_A) ?
					BATT_PROFILEA_MASK : BATT_PROFILEB_MASK;
			pr_info("Loaded Profile-RID != connected-RID, switch-profile old_profile=%d new_profile=%d\n",
						loaded_profile, new_profile);
		}
	}

	/* set the BID mask */
	rc = smb1360_masked_write(chip, CFG_FG_BATT_CTRL_REG,
				BATT_PROFILE_SELECT_MASK, bid_mask);
	if (rc) {
		pr_err("Couldn't reset battery-profile rc=%d\n", rc);
		goto fail_profile;
	}

	/* enable FG access */
	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT,
							FG_ACCESS_ENABLED_BIT);
	if (rc) {
		pr_err("Couldn't enable FG access rc=%d\n", rc);
		goto fail_profile;
	}

	while (timeout) {
		/* delay for FG access to be granted */
		msleep(100);
		rc = smb1360_read(chip, IRQ_I_REG, &reg);
		if (rc) {
			pr_err("Could't read IRQ_I_REG rc=%d\n", rc);
			goto restore_fg;
		}
		if (reg & FG_ACCESS_ALLOWED_BIT)
			break;
		timeout--;
	}
	if (!timeout) {
		pr_err("FG access timed-out\n");
		rc = -EAGAIN;
		goto restore_fg;
	}

	/* delay after handshaking for profile-switch to continue */
	msleep(1500);

	/* reset FG */
	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_RESET_BIT,
						FG_RESET_BIT);
	if (rc) {
		pr_err("Couldn't reset FG rc=%d\n", rc);
		goto restore_fg;
	}

	/* un-reset FG */
	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_RESET_BIT, 0);
	if (rc) {
		pr_err("Couldn't un-reset FG rc=%d\n", rc);
		goto restore_fg;
	}

	/*  disable FG access */
	rc = smb1360_masked_write(chip, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT, 0);
	if (rc) {
		pr_err("Couldn't disable FG access rc=%d\n", rc);
		goto restore_fg;
	}

	timeout = 10;
	while (timeout) {
		/* delay for profile to change */
		msleep(500);
		rc = smb1360_read(chip, SHDW_FG_BATT_STATUS, &reg);
		if (rc) {
			pr_err("Could't read FG_BATT_STATUS rc=%d\n", rc);
			goto restore_fg;
		}

		reg = !!(reg & BATTERY_PROFILE_BIT);
		if (reg == new_profile) {
			pr_info("New profile=%d loaded\n", new_profile);
			break;
		}
		timeout--;
	}

	if (!timeout) {
		pr_err("New profile could not be loaded\n");
		return -EBUSY;
	}

	return 0;

restore_fg:
	smb1360_masked_write(chip, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT, 0);
fail_profile:
	return rc;
}

static int determine_initial_status(struct smb1360_chip *chip)
{
	int rc;
	u8 reg = 0;

	/*
	 * It is okay to read the IRQ status as the irq's are
	 * not registered yet.
	 */
	chip->batt_present = true;
	rc = smb1360_read(chip, IRQ_B_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read IRQ_B_REG rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_B_BATT_TERMINAL_BIT || reg & IRQ_B_BATT_MISSING_BIT)
		chip->batt_present = false;

	rc = smb1360_read(chip, IRQ_C_REG, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read IRQ_C_REG rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_C_CHG_TERM)
		chip->batt_full = true;

	rc = smb1360_read(chip, IRQ_A_REG, &reg);
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

	rc = smb1360_read(chip, IRQ_E_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq E rc = %d\n", rc);
		return rc;
	}
	chip->usb_present = (reg & IRQ_E_USBIN_UV_BIT) ? false : true;
	power_supply_set_present(chip->usb_psy, chip->usb_present);

	return 0;
}

static int smb1360_fg_config(struct smb1360_chip *chip)
{
	int rc = 0, temp, fcc_mah;
	u8 reg = 0, reg2[2];

	/*
	 * The below IRQ thresholds are not accessible in REV_1
	 * of SMB1360.
	 */
	if (!(chip->workaround_flags & WRKRND_FG_CONFIG_FAIL)) {
		if (chip->delta_soc != -EINVAL) {
			reg = DIV_ROUND_UP(chip->delta_soc * MAX_8_BITS, 100);
			pr_debug("delta_soc=%d reg=%x\n", chip->delta_soc, reg);
			rc = smb1360_write(chip, SOC_DELTA_REG, reg);
			if (rc) {
				dev_err(chip->dev, "Couldn't write to SOC_DELTA_REG rc=%d\n",
						rc);
				return rc;
			}
		}

		if (chip->soc_min != -EINVAL) {
			if (is_between(chip->soc_min, 0, 100)) {
				reg = DIV_ROUND_UP(chip->soc_min * MAX_8_BITS,
									100);
				pr_debug("soc_min=%d reg=%x\n",
						chip->soc_min, reg);
				rc = smb1360_write(chip, SOC_MIN_REG, reg);
				if (rc) {
					dev_err(chip->dev, "Couldn't write to SOC_MIN_REG rc=%d\n",
							rc);
					return rc;
				}
			}
		}

		if (chip->soc_max != -EINVAL) {
			if (is_between(chip->soc_max, 0, 100)) {
				reg = DIV_ROUND_UP(chip->soc_max * MAX_8_BITS,
									100);
				pr_debug("soc_max=%d reg=%x\n",
						chip->soc_max, reg);
				rc = smb1360_write(chip, SOC_MAX_REG, reg);
				if (rc) {
					dev_err(chip->dev, "Couldn't write to SOC_MAX_REG rc=%d\n",
							rc);
					return rc;
				}
			}
		}

		if (chip->voltage_min_mv != -EINVAL) {
			temp = (chip->voltage_min_mv - 2500) * MAX_8_BITS;
			reg = DIV_ROUND_UP(temp, 2500);
			pr_debug("voltage_min=%d reg=%x\n",
					chip->voltage_min_mv, reg);
			rc = smb1360_write(chip, VTG_MIN_REG, reg);
			if (rc) {
				dev_err(chip->dev, "Couldn't write to VTG_MIN_REG rc=%d\n",
							rc);
				return rc;
			}
		}

		if (chip->voltage_empty_mv != -EINVAL) {
			temp = (chip->voltage_empty_mv - 2500) * MAX_8_BITS;
			reg = DIV_ROUND_UP(temp, 2500);
			pr_debug("voltage_empty=%d reg=%x\n",
					chip->voltage_empty_mv, reg);
			rc = smb1360_write(chip, VTG_EMPTY_REG, reg);
			if (rc) {
				dev_err(chip->dev, "Couldn't write to VTG_EMPTY_REG rc=%d\n",
							rc);
				return rc;
			}
		}
	}

	/* scratch-pad register config */
	if (chip->batt_capacity_mah != -EINVAL) {
		rc = smb1360_enable_fg_access(chip);
		if (rc) {
			pr_err("Couldn't enable FG access rc=%d\n", rc);
			return rc;
		}
		rc = smb1360_read_bytes(chip, NOMINAL_CAPACITY_REG,
							reg2, 2);
		if (rc) {
			pr_err("Failed to read NOM CAPACITY rc=%d\n",
								rc);
			goto disable_fg;
		}
		fcc_mah = (reg2[1] << 8) | reg2[0];
		if (fcc_mah == chip->batt_capacity_mah) {
			pr_debug("battery capacity correct\n");
			goto disable_fg;
		}
		/* Update the battery capacity */
		reg2[1] = (chip->batt_capacity_mah & 0xFF00) >> 8;
		reg2[0] = (chip->batt_capacity_mah & 0xFF);
		rc = smb1360_write_bytes(chip, NOMINAL_CAPACITY_REG,
							reg2, 2);
		if (rc) {
			pr_err("Couldn't write batt-capacity rc=%d\n",
								rc);
			goto disable_fg;
		}
		/* Update CC to SOC COEFF */
		if (chip->cc_soc_coeff != -EINVAL) {
			reg2[1] = (chip->cc_soc_coeff & 0xFF00) >> 8;
			reg2[0] = (chip->cc_soc_coeff & 0xFF);
			rc = smb1360_write_bytes(chip, CC_TO_SOC_COEFF,
							reg2, 2);
			if (rc) {
				pr_err("Couldn't write cc_soc_coeff rc=%d\n",
									rc);
				goto disable_fg;
			}
		}
disable_fg:
		/* disable FG access */
		smb1360_disable_fg_access(chip);
	}

	return rc;
}

static void smb1360_check_feature_support(struct smb1360_chip *chip)
{

	if (is_usb100_broken(chip)) {
		pr_debug("USB100 is not supported\n");
		chip->workaround_flags |= WRKRND_USB100_FAIL;
	}

	/*
	 * FG Configuration
	 *
	 * The REV_1 of the chip does not allow access to
	 * FG config registers (20-2FH). Set the workaround flag.
	 * Also, the battery detection does not work when the DCIN is absent,
	 * add a workaround flag for it.
	*/
	if (chip->revision == SMB1360_REV_1) {
		pr_debug("FG config and Battery detection is not supported\n");
		chip->workaround_flags |=
			WRKRND_FG_CONFIG_FAIL | WRKRND_BATT_DET_FAIL;
	}
}

static int smb1360_enable(struct smb1360_chip *chip, bool enable)
{
	int rc = 0;
	u8 val, shdn_status, shdn_cmd_en, shdn_cmd_polar;

	rc = smb1360_read(chip, SHDN_CTRL_REG, &val);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read 0x1A reg rc = %d\n", rc);
		return rc;
	}
	shdn_cmd_en = val & SHDN_CMD_USE_BIT;
	shdn_cmd_polar = !!(val & SHDN_CMD_POLARITY_BIT);
	shdn_status = val & SHDN_CMD_BIT;

	val = (shdn_cmd_polar ^ enable) ? SHDN_CMD_BIT : 0;

	if (shdn_cmd_en) {
		if (shdn_status != val) {
			rc = smb1360_masked_write(chip, CMD_IL_REG,
					SHDN_CMD_BIT, val);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't shutdown smb1360 rc = %d\n",
									rc);
				return rc;
			}
		}
	} else {
		dev_dbg(chip->dev, "SMB not configured for CMD based shutdown\n");
	}

	return rc;
}

static inline int smb1360_poweroff(struct smb1360_chip *chip)
{
	pr_debug("power off smb1360\n");
	return smb1360_enable(chip, false);
}

static inline int smb1360_poweron(struct smb1360_chip *chip)
{
	pr_debug("power on smb1360\n");
	return smb1360_enable(chip, true);
}

static int smb1360_hw_init(struct smb1360_chip *chip)
{
	int rc;
	int i;
	u8 reg, mask;

	smb1360_check_feature_support(chip);

	rc = smb1360_enable_volatile_writes(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't configure for volatile rc = %d\n",
				rc);
		return rc;
	}

	if (chip->shdn_after_pwroff) {
		rc = smb1360_poweron(chip);
		if (rc < 0) {
			pr_err("smb1360 power on failed\n");
			return rc;
		}
	}

	rc = smb1360_check_batt_profile(chip);
	if (rc)
		pr_err("Unable to modify battery profile\n");

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast
	 */
	rc = smb1360_masked_write(chip, CFG_CHG_MISC_REG,
					CHG_EN_BY_PIN_BIT
					| CHG_EN_ACTIVE_LOW_BIT
					| PRE_TO_FAST_REQ_CMD_BIT,
					0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set CFG_CHG_MISC_REG rc=%d\n", rc);
		return rc;
	}

	/* USB/AC pin settings */
	rc = smb1360_masked_write(chip, CFG_BATT_CHG_ICL_REG,
					AC_INPUT_ICL_PIN_BIT
					| AC_INPUT_PIN_HIGH_BIT
					| RESET_STATE_USB_500,
					AC_INPUT_PIN_HIGH_BIT
					| RESET_STATE_USB_500);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set CFG_BATT_CHG_ICL_REG rc=%d\n",
				rc);
		return rc;
	}

	/* AICL enable and set input-uv glitch flt to 20ms*/
	reg = AICL_ENABLED_BIT | INPUT_UV_GLITCH_FLT_20MS_BIT;
	rc = smb1360_masked_write(chip, CFG_GLITCH_FLT_REG, reg, reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set CFG_GLITCH_FLT_REG rc=%d\n",
				rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smb1360_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			if (chip->iterm_ma < 25)
				reg = CHG_ITERM_25MA;
			else if (chip->iterm_ma > 200)
				reg = CHG_ITERM_200MA;
			else
				reg = DIV_ROUND_UP(chip->iterm_ma, 25) - 1;

			rc = smb1360_masked_write(chip, CFG_BATT_CHG_REG,
						CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}

			rc = smb1360_masked_write(chip, CFG_CHG_MISC_REG,
					CHG_CURR_TERM_DIS_BIT, 0);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't enable iterm rc = %d\n", rc);
				return rc;
			}
		}
	} else  if (chip->iterm_disabled) {
		rc = smb1360_masked_write(chip, CFG_CHG_MISC_REG,
						CHG_CURR_TERM_DIS_BIT,
						CHG_CURR_TERM_DIS_BIT);
		if (rc) {
			dev_err(chip->dev, "Couldn't set iterm rc = %d\n",
								rc);
			return rc;
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		if (chip->safety_time == 0) {
			/* safety timer disabled */
			rc = smb1360_masked_write(chip, CFG_SFY_TIMER_CTRL_REG,
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
			rc = smb1360_masked_write(chip, CFG_SFY_TIMER_CTRL_REG,
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

	/* configure resume threshold, auto recharge and charge inhibit */
	if (chip->resume_delta_mv != -EINVAL) {
		if (chip->recharge_disabled && chip->chg_inhibit_disabled) {
			dev_err(chip->dev, "Error: Both recharge_disabled and recharge_mv set\n");
			return -EINVAL;
		} else {
			rc = smb1360_recharge_threshold_set(chip,
						chip->resume_delta_mv);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set rechg thresh rc = %d\n",
									rc);
				return rc;
			}
		}
	}

	rc = smb1360_masked_write(chip, CFG_CHG_MISC_REG,
					CFG_AUTO_RECHG_DIS_BIT,
					chip->recharge_disabled ?
					CFG_AUTO_RECHG_DIS_BIT : 0);
	if (rc) {
		dev_err(chip->dev, "Couldn't set rechg-cfg rc = %d\n", rc);
		return rc;
	}
	rc = smb1360_masked_write(chip, CFG_CHG_MISC_REG,
					CFG_CHG_INHIBIT_EN_BIT,
					chip->chg_inhibit_disabled ?
					0 : CFG_CHG_INHIBIT_EN_BIT);
	if (rc) {
		dev_err(chip->dev, "Couldn't set chg_inhibit rc = %d\n", rc);
		return rc;
	}

	/* battery missing detection */
	rc = smb1360_masked_write(chip, CFG_BATT_MISSING_REG,
				BATT_MISSING_SRC_THERM_BIT,
				BATT_MISSING_SRC_THERM_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set batt_missing config = %d\n",
									rc);
		return rc;
	}

	/* interrupt enabling - active low */
	if (chip->client->irq) {
		mask = CHG_STAT_IRQ_ONLY_BIT
			| CHG_STAT_ACTIVE_HIGH_BIT
			| CHG_STAT_DISABLE_BIT
			| CHG_TEMP_CHG_ERR_BLINK_BIT;

		if (!chip->pulsed_irq)
			reg = CHG_STAT_IRQ_ONLY_BIT;
		else
			reg = CHG_TEMP_CHG_ERR_BLINK_BIT;
		rc = smb1360_masked_write(chip, CFG_STAT_CTRL_REG, mask, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq config rc = %d\n",
					rc);
			return rc;
		}

		/* enabling only interesting interrupts */
		rc = smb1360_write(chip, IRQ_CFG_REG,
				IRQ_BAT_HOT_COLD_HARD_BIT
				| IRQ_BAT_HOT_COLD_SOFT_BIT
				| IRQ_INTERNAL_TEMPERATURE_BIT
				| IRQ_DCIN_UV_BIT);
		if (rc) {
			dev_err(chip->dev, "Couldn't set irq1 config rc = %d\n",
					rc);
			return rc;
		}

		rc = smb1360_write(chip, IRQ2_CFG_REG,
				IRQ2_SAFETY_TIMER_BIT
				| IRQ2_CHG_ERR_BIT
				| IRQ2_CHG_PHASE_CHANGE_BIT
				| IRQ2_POWER_OK_BIT
				| IRQ2_BATT_MISSING_BIT
				| IRQ2_VBAT_LOW_BIT);
		if (rc) {
			dev_err(chip->dev, "Couldn't set irq2 config rc = %d\n",
					rc);
			return rc;
		}

		rc = smb1360_write(chip, IRQ3_CFG_REG,
				IRQ3_SOC_CHANGE_BIT
				| IRQ3_SOC_MIN_BIT
				| IRQ3_SOC_MAX_BIT
				| IRQ3_SOC_EMPTY_BIT
				| IRQ3_SOC_FULL_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq3 enable rc = %d\n",
					rc);
			return rc;
		}
	}

	/* batt-id configuration */
	if (chip->batt_id_disabled) {
		mask = BATT_ID_ENABLED_BIT | CHG_BATT_ID_FAIL
				| BATT_ID_FAIL_SELECT_PROFILE;
		reg = CHG_BATT_ID_FAIL | BATT_ID_FAIL_SELECT_PROFILE;
		rc = smb1360_masked_write(chip, CFG_FG_BATT_CTRL_REG,
						mask, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set batt_id_reg rc = %d\n",
					rc);
			return rc;
		}
	}


	rc = smb1360_fg_config(chip);
	if (rc < 0) {
		pr_err("Couldn't configure FG rc=%d\n", rc);
		return rc;
	}

	rc = smb1360_charging_disable(chip, USER, !!chip->charging_disabled);
	if (rc)
		dev_err(chip->dev, "Couldn't '%s' charging rc = %d\n",
			chip->charging_disabled ? "disable" : "enable", rc);

	return rc;
}

static int smb_parse_batt_id(struct smb1360_chip *chip)
{
	int rc = 0, rpull = 0, vref = 0;
	int64_t denom, batt_id_uv;
	struct device_node *node = chip->dev->of_node;
	struct qpnp_vadc_result result;

	chip->vadc_dev = qpnp_get_vadc(chip->dev, "smb1360");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("vadc not found - defer rc=%d\n", rc);
		else
			pr_err("vadc property missing, rc=%d\n", rc);

		return rc;
	}

	rc = of_property_read_u32(node, "qcom,profile-a-rid-kohm",
						&chip->profile_rid[0]);
	if (rc < 0) {
		pr_err("Couldn't read profile-a-rid-kohm rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,profile-b-rid-kohm",
						&chip->profile_rid[1]);
	if (rc < 0) {
		pr_err("Couldn't read profile-b-rid-kohm rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,batt-id-vref-uv", &vref);
	if (rc < 0) {
		pr_err("Couldn't read batt-id-vref-uv rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,batt-id-rpullup-kohm", &rpull);
	if (rc < 0) {
		pr_err("Couldn't read batt-id-rpullup-kohm rc=%d\n", rc);
		return rc;
	}

	/* read battery ID */
	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX2_BAT_ID, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					LR_MUX2_BAT_ID, rc);
		return rc;
	}
	batt_id_uv = result.physical;

	if (batt_id_uv == 0) {
		/* vadc not correct or batt id line grounded, report 0 kohms */
		pr_err("batt_id_uv = 0, batt-id grounded using same profile\n");
		return 0;
	}

	denom = div64_s64(vref * 1000000LL, batt_id_uv) - 1000000LL;
	if (denom == 0) {
		/* batt id connector might be open, return 0 kohms */
		return 0;
	}
	chip->connected_rid = div64_s64(rpull * 1000000LL + denom/2, denom);

	pr_debug("batt_id_voltage = %lld, connected_rid = %d\n",
			batt_id_uv, chip->connected_rid);

	return 0;
}

static int smb_parse_dt(struct smb1360_chip *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	if (of_property_read_bool(node, "qcom,batt-profile-select")) {
		rc = smb_parse_batt_id(chip);
		if (rc < 0) {
			if (rc != -EPROBE_DEFER)
				pr_err("Unable to parse batt-id rc=%d\n", rc);
			return rc;
		}
	}

	chip->pulsed_irq = of_property_read_bool(node, "qcom,stat-pulsed-irq");

	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc < 0)
		chip->vfloat_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,charging-timeout",
						&chip->safety_time);
	if (rc < 0)
		chip->safety_time = -EINVAL;

	if (!rc && (chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout %d\n",
						chip->safety_time);
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,recharge-thresh-mv",
						&chip->resume_delta_mv);
	if (rc < 0)
		chip->resume_delta_mv = -EINVAL;

	chip->recharge_disabled = of_property_read_bool(node,
						"qcom,recharge-disabled");

	rc = of_property_read_u32(node, "qcom,iterm-ma", &chip->iterm_ma);
	if (rc < 0)
		chip->iterm_ma = -EINVAL;

	chip->iterm_disabled = of_property_read_bool(node,
						"qcom,iterm-disabled");

	chip->chg_inhibit_disabled = of_property_read_bool(node,
						"qcom,chg-inhibit-disabled");

	chip->charging_disabled = of_property_read_bool(node,
						"qcom,charging-disabled");

	chip->batt_id_disabled = of_property_read_bool(node,
						"qcom,batt-id-disabled");

	chip->shdn_after_pwroff = of_property_read_bool(node,
						"qcom,shdn-after-pwroff");

	if (of_find_property(node, "qcom,thermal-mitigation",
					&chip->thermal_levels)) {
		chip->thermal_mitigation = devm_kzalloc(chip->dev,
					chip->thermal_levels,
						GFP_KERNEL);

		if (chip->thermal_mitigation == NULL) {
			pr_err("thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chip->thermal_mitigation, chip->thermal_levels);
		if (rc) {
			pr_err("Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	/* fg params */
	rc = of_property_read_u32(node, "qcom,fg-delta-soc", &chip->delta_soc);
	if (rc < 0)
		chip->delta_soc = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-soc-max", &chip->soc_max);
	if (rc < 0)
		chip->soc_max = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-soc-min", &chip->soc_min);
	if (rc < 0)
		chip->soc_min = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-voltage-min-mv",
					&chip->voltage_min_mv);
	if (rc < 0)
		chip->voltage_min_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-voltage-empty-mv",
					&chip->voltage_empty_mv);
	if (rc < 0)
		chip->voltage_empty_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-batt-capacity-mah",
					&chip->batt_capacity_mah);
	if (rc < 0)
		chip->batt_capacity_mah = -EINVAL;

	rc = of_property_read_u32(node, "qcom,fg-cc-soc-coeff",
					&chip->cc_soc_coeff);
	if (rc < 0)
		chip->cc_soc_coeff = -EINVAL;

	return 0;
}

static int smb1360_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	u8 reg;
	int rc;
	struct smb1360_chip *chip;
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
	chip->fake_battery_soc = -EINVAL;
	mutex_init(&chip->read_write_lock);

	/* probe the device to check if its actually connected */
	rc = smb1360_read(chip, CFG_BATT_CHG_REG, &reg);
	if (rc) {
		pr_err("Failed to detect SMB1360, device may be absent\n");
		return -ENODEV;
	}

	rc = read_revision(chip, &chip->revision);
	if (rc)
		dev_err(chip->dev, "Couldn't read revision rc = %d\n", rc);

	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&client->dev, "Unable to parse DT nodes\n");
		return rc;
	}

	device_init_wakeup(chip->dev, 1);
	i2c_set_clientdata(client, chip);
	chip->resume_completed = true;
	mutex_init(&chip->irq_complete);
	mutex_init(&chip->charging_disable_lock);
	mutex_init(&chip->current_change_lock);
	chip->default_i2c_addr = client->addr;

	pr_debug("default_i2c_addr=%x\n", chip->default_i2c_addr);

	rc = smb1360_regulator_init(chip);
	if  (rc) {
		dev_err(&client->dev,
			"Couldn't initialize smb349 ragulator rc=%d\n", rc);
		return rc;
	}

	rc = smb1360_hw_init(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		goto fail_hw_init;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to determine init status rc = %d\n", rc);
		goto fail_hw_init;
	}

	chip->batt_psy.name		= "battery";
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smb1360_battery_get_property;
	chip->batt_psy.set_property	= smb1360_battery_set_property;
	chip->batt_psy.properties	= smb1360_battery_properties;
	chip->batt_psy.num_properties  = ARRAY_SIZE(smb1360_battery_properties);
	chip->batt_psy.external_power_changed = smb1360_external_power_changed;
	chip->batt_psy.property_is_writeable = smb1360_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		goto fail_hw_init;
	}

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				smb1360_stat_handler, IRQF_ONESHOT,
				"smb1360_stat_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev,
				"request_irq for irq=%d  failed rc = %d\n",
				client->irq, rc);
			goto unregister_batt_psy;
		}
		enable_irq_wake(client->irq);
	}

	chip->debug_root = debugfs_create_dir("smb1360", NULL);
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

		ent = debugfs_create_file("irq_status", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &irq_stat_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create irq_stat debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("cmd_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cmd_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cmd debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("fg_regs",
				S_IFREG | S_IRUGO, chip->debug_root, chip,
					  &fg_regs_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create fg_scratch_pad debug file rc = %d\n",
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

		ent = debugfs_create_x32("fg_address",
					S_IFREG | S_IWUSR | S_IRUGO,
					chip->debug_root,
					&(chip->fg_peek_poke_address));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create address debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("fg_data",
					S_IFREG | S_IWUSR | S_IRUGO,
					chip->debug_root, chip,
					&fg_poke_poke_debug_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("fg_access_type",
					S_IFREG | S_IWUSR | S_IRUGO,
					chip->debug_root,
					&(chip->fg_access_type));
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

		ent = debugfs_create_x32("skip_reads",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_reads));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("irq_count", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &irq_count_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create count debug file rc = %d\n",
				rc);
	}

	dev_info(chip->dev, "SMB1360 revision=0x%x probe success! batt=%d usb=%d soc=%d\n",
			chip->revision,
			smb1360_get_prop_batt_present(chip),
			chip->usb_present,
			smb1360_get_prop_batt_capacity(chip));

	return 0;

unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
fail_hw_init:
	regulator_unregister(chip->otg_vreg.rdev);
	return rc;
}

static int smb1360_remove(struct i2c_client *client)
{
	struct smb1360_chip *chip = i2c_get_clientdata(client);

	regulator_unregister(chip->otg_vreg.rdev);
	power_supply_unregister(&chip->batt_psy);
	mutex_destroy(&chip->charging_disable_lock);
	mutex_destroy(&chip->current_change_lock);
	mutex_destroy(&chip->read_write_lock);
	mutex_destroy(&chip->irq_complete);
	debugfs_remove_recursive(chip->debug_root);

	return 0;
}

static int smb1360_suspend(struct device *dev)
{
	int i, rc;
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1360_chip *chip = i2c_get_clientdata(client);

	/* Save the current IRQ config */
	for (i = 0; i < 3; i++) {
		rc = smb1360_read(chip, IRQ_CFG_REG + i,
					&chip->irq_cfg_mask[i]);
		if (rc)
			pr_err("Couldn't save irq cfg regs rc=%d\n", rc);
	}

	/* enable only important IRQs */
	rc = smb1360_write(chip, IRQ_CFG_REG, IRQ_DCIN_UV_BIT);
	if (rc < 0)
		pr_err("Couldn't set irq_cfg rc=%d\n", rc);

	rc = smb1360_write(chip, IRQ2_CFG_REG, IRQ2_BATT_MISSING_BIT
						| IRQ2_VBAT_LOW_BIT
						| IRQ2_POWER_OK_BIT);
	if (rc < 0)
		pr_err("Couldn't set irq2_cfg rc=%d\n", rc);

	rc = smb1360_write(chip, IRQ3_CFG_REG, IRQ3_SOC_FULL_BIT
					| IRQ3_SOC_MIN_BIT
					| IRQ3_SOC_EMPTY_BIT);
	if (rc < 0)
		pr_err("Couldn't set irq3_cfg rc=%d\n", rc);

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);

	return 0;
}

static int smb1360_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1360_chip *chip = i2c_get_clientdata(client);

	if (chip->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int smb1360_resume(struct device *dev)
{
	int i, rc;
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1360_chip *chip = i2c_get_clientdata(client);

	/* Restore the IRQ config */
	for (i = 0; i < 3; i++) {
		rc = smb1360_write(chip, IRQ_CFG_REG + i,
					chip->irq_cfg_mask[i]);
		if (rc)
			pr_err("Couldn't restore irq cfg regs rc=%d\n", rc);
	}

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	if (chip->irq_waiting) {
		mutex_unlock(&chip->irq_complete);
		smb1360_stat_handler(client->irq, chip);
		enable_irq(client->irq);
	} else {
		mutex_unlock(&chip->irq_complete);
	}

	return 0;
}

static void smb1360_shutdown(struct i2c_client *client)
{
	int rc;
	struct smb1360_chip *chip = i2c_get_clientdata(client);

	if (chip->shdn_after_pwroff) {
		rc = smb1360_poweroff(chip);
		if (rc)
			pr_err("Couldn't shutdown smb1360, rc = %d\n", rc);
		pr_info("smb1360 power off\n");
	}
}

static const struct dev_pm_ops smb1360_pm_ops = {
	.resume		= smb1360_resume,
	.suspend_noirq	= smb1360_suspend_noirq,
	.suspend	= smb1360_suspend,
};

static struct of_device_id smb1360_match_table[] = {
	{ .compatible = "qcom,smb1360-chg-fg",},
	{ },
};

static const struct i2c_device_id smb1360_id[] = {
	{"smb1360-chg-fg", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb1360_id);

static struct i2c_driver smb1360_driver = {
	.driver		= {
		.name		= "smb1360-chg-fg",
		.owner		= THIS_MODULE,
		.of_match_table	= smb1360_match_table,
		.pm		= &smb1360_pm_ops,
	},
	.probe		= smb1360_probe,
	.remove		= smb1360_remove,
	.shutdown	= smb1360_shutdown,
	.id_table	= smb1360_id,
};

module_i2c_driver(smb1360_driver);

MODULE_DESCRIPTION("SMB1360 Charger and Fuel Gauge");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb1360-chg-fg");
