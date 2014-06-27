/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "SMBCHG: %s: " fmt, __func__

#include <linux/spmi.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/spmi.h>
#include <linux/printk.h>

/* Mask/Bit helpers */
#define _SMB_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))
/* Config registers */
struct smbchg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct parallel_usb_cfg {
	struct power_supply		*psy;
	int				min_current_thr_ma;
	int				current_max_ma;
	bool				avail;
};

struct smbchg_chip {
	struct device			*dev;
	struct spmi_device		*spmi;

	/* peripheral register address bases */
	u16				chgr_base;
	u16				bat_if_base;
	u16				usb_chgpth_base;
	u16				dc_chgpth_base;
	u16				otg_base;
	u16				misc_base;

	int				fake_battery_soc;

	/* configuration parameters */
	int				iterm_ma;
	int				usb_max_current_ma;
	int				dc_max_current_ma;
	int				usb_target_current_ma;
	int				dc_target_current_ma;
	int				vfloat_mv;
	int				resume_delta_mv;
	int				safety_time;
	int				prechg_safety_time;
	int				bmd_pin_src;
	bool				iterm_disabled;
	bool				bmd_algo_disabled;
	bool				soft_vfloat_comp_disabled;
	bool				chg_enabled;
	bool				low_icl_wa_on;
	struct parallel_usb_cfg		parallel;

	/* status variables */
	int				usb_suspended;
	int				dc_suspended;
	bool				usb_online;
	bool				dc_present;
	bool				usb_present;
	bool				batt_present;
	bool				chg_done_batt_full;
	bool				otg_retries;

	/* jeita and temperature */
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;
	unsigned int			thermal_levels;
	unsigned int			therm_lvl_sel;
	unsigned int			*thermal_mitigation;

	/* irqs */
	int				batt_hot_irq;
	int				batt_warm_irq;
	int				batt_cool_irq;
	int				batt_cold_irq;
	int				batt_missing_irq;
	int				vbat_low_irq;
	int				chg_hot_irq;
	int				chg_term_irq;
	int				taper_irq;
	int				recharge_irq;
	int				fastchg_irq;
	int				safety_timeout_irq;
	int				power_ok_irq;
	int				dcin_uv_irq;
	int				usbin_uv_irq;
	int				src_detect_irq;
	int				otg_fail_irq;
	int				otg_oc_irq;
	int				aicl_done_irq;
	int				usbid_change_irq;
	int				chg_inhibit_irq;
	int				chg_error_irq;

	/* psy */
	struct power_supply		*usb_psy;
	struct power_supply		batt_psy;
	struct power_supply		dc_psy;
	struct power_supply		*bms_psy;
	int				dc_psy_type;
	const char			*bms_psy_name;
	const char			*battery_psy_name;
	bool				psy_registered;

	struct smbchg_regulator		otg_vreg;
	struct delayed_work		wireless_insertion_work;
	struct work_struct		usb_set_online_work;
	spinlock_t			sec_access_lock;
	struct mutex			current_change_lock;
	struct mutex			usb_set_online_lock;
	struct mutex			usb_en_lock;
	struct mutex			dc_en_lock;
};

static int smbchg_read(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "spmi read failed addr=0x%02x sid=0x%02x rc=%d\n",
				addr, spmi->sid, rc);
		return rc;
	}
	return 0;
}

/*
 * Writes an arbitrary number of bytes to a specified register
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_write(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "write failed addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_masked_write_raw(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi read failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;

	pr_debug("addr = 0x%x writing 0x%x\n", base, reg);

	rc = smbchg_write(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi write failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * This function holds a spin lock to ensure secure access register writes goes
 * through. If the secure access unlock register is armed, any old register
 * write can unarm the secure access unlock, causing the next write to fail.
 *
 * Note: do not use this for sec_access registers. Instead use the function
 * below: smbchg_sec_masked_write
 */
static int smbchg_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
								u8 val)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	rc = smbchg_masked_write_raw(chip, base, mask, val);
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);

	return rc;
}

/*
 * Unlocks sec access and writes to the register specified.
 *
 * This function holds a spin lock to exclude other register writes while
 * the two writes are taking place.
 */
#define SEC_ACCESS_OFFSET	0xD0
#define SEC_ACCESS_VALUE	0xA5
#define PERIPHERAL_MASK		0xFF
static int smbchg_sec_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	unsigned long flags;
	int rc;
	u16 peripheral_base = base & (~PERIPHERAL_MASK);

	spin_lock_irqsave(&chip->sec_access_lock, flags);

	rc = smbchg_masked_write_raw(chip, peripheral_base + SEC_ACCESS_OFFSET,
				SEC_ACCESS_VALUE, SEC_ACCESS_VALUE);
	if (rc) {
		dev_err(chip->dev, "Unable to unlock sec_access: %d", rc);
		goto out;
	}

	rc = smbchg_masked_write_raw(chip, base, mask, val);

out:
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);
	return rc;
}

#define RID_STS				0xB
#define RID_MASK			0xF
static bool is_otg_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RID_STS, 1);
	if (rc < 0) {
		pr_err("Couldn't read usb rid status rc = %d\n", rc);
		return false;
	}

	pr_debug("RID_STS = %02x\n", reg);

	return (reg & RID_MASK) == 0;
}

#define USBIN_9V			BIT(5)
#define USBIN_UNREG			BIT(4)
#define USBIN_LV			BIT(3)
#define DCIN_9V				BIT(2)
#define DCIN_UNREG			BIT(1)
#define DCIN_LV				BIT(0)
#define INPUT_STS			0x0D
static bool is_dc_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + INPUT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
		return false;
	}

	return !!(reg & (DCIN_9V | DCIN_UNREG | DCIN_LV));
}

#define RT_STS			0x10
#define USBIN_UV_BIT		0x0
#define USBIN_OV_BIT		0x1
static bool is_usb_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		return false;
	}
	if ((reg & USBIN_UV_BIT) || (reg & USBIN_OV_BIT))
		return false;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + INPUT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
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
	"NONE",		/* bit 8 error case */
};

#define BITS_PER_REG	8
/* helper to return the string of USB type */
static char *get_usb_type_name(u8 type_reg)
{
	unsigned long type = type_reg;

	return usb_type_str[find_first_bit(&type, BITS_PER_REG)];
}

static enum power_supply_type usb_type_enum[] = {
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 0 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 1 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 2 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 3 */
	POWER_SUPPLY_TYPE_USB,		/* bit 4 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 5 */
	POWER_SUPPLY_TYPE_USB_DCP,	/* bit 6 */
	POWER_SUPPLY_TYPE_USB_CDP,	/* bit 7 */
	POWER_SUPPLY_TYPE_USB,		/* bit 8 error case, report SDP */
};

/* helper to return enum power_supply_type of USB type */
static enum power_supply_type get_usb_supply_type(u8 type_reg)
{
	unsigned long type = type_reg;

	return usb_type_enum[find_first_bit(&type, BITS_PER_REG)];
}

static enum power_supply_property smbchg_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
};

#define CHGR_STS			0x0E
#define BATT_LESS_THAN_2V		BIT(4)
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)
static int get_prop_batt_status(struct smbchg_chip *chip)
{
	int rc, status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0, chg_type;

	if (chip->chg_done_batt_full)
		return POWER_SUPPLY_STATUS_FULL;

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
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
	pr_debug("CHGR_STS = 0x%02x\n", reg);
	return status;
}

#define BAT_PRES_STATUS			0x08
#define BAT_PRES_BIT			BIT(7)
static int get_prop_batt_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->bat_if_base + BAT_PRES_STATUS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

	return !!(reg & BAT_PRES_BIT);
}

static int get_prop_charge_type(struct smbchg_chip *chip)
{
	int rc;
	u8 reg, chg_type;

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (chg_type == BATT_FAST_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

#define DEFAULT_BATT_CAPACITY	50
static int get_prop_batt_capacity(struct smbchg_chip *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;
	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	}

	return DEFAULT_BATT_CAPACITY;
}

static int get_prop_batt_health(struct smbchg_chip *chip)
{
	if (chip->batt_hot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		return POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		return POWER_SUPPLY_HEALTH_COOL;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
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

static int calc_thermal_limited_current(struct smbchg_chip *chip,
						int current_ma)
{
	int therm_ma;

	if (chip->therm_lvl_sel > 0
			&& chip->therm_lvl_sel < (chip->thermal_levels - 1)) {
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = (int)chip->thermal_mitigation[chip->therm_lvl_sel];
		if (therm_ma < current_ma) {
			dev_dbg(chip->dev, "Limiting current due to thermal: %d mA",
					therm_ma);
			return therm_ma;
		}
	}

	return current_ma;
}

#define CMD_IL			0x40
#define USBIN_SUSPEND_BIT	BIT(4)
#define CURRENT_100_MA		100
#define CURRENT_150_MA		150
#define CURRENT_500_MA		500
#define CURRENT_900_MA		900
#define SUSPEND_CURRENT_MA	2
static int smbchg_usb_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USBIN_SUSPEND_BIT, suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set usb suspend rc = %d\n", rc);
	return rc;
}

#define DCIN_SUSPEND_BIT	BIT(3)
static int smbchg_dc_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc = 0;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			DCIN_SUSPEND_BIT, suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set dc suspend rc = %d\n", rc);
	return rc;
}

#define IL_CFG			0xF2
#define DCIN_INPUT_MASK	SMB_MASK(4, 0)
static int smbchg_set_dc_current_max(struct smbchg_chip *chip, int current_ma)
{
	int i;
	u8 dc_cur_val;

	for (i = ARRAY_SIZE(dc_current_table) - 1; i >= 0; i--) {
		if (current_ma >= dc_current_table[i])
			break;
	}

	if (i < 0) {
		dev_err(chip->dev, "Cannot find %dma current_table\n",
				current_ma);
		return -EINVAL;
	}

	chip->dc_max_current_ma = dc_current_table[i];
	dc_cur_val = i & DCIN_INPUT_MASK;

	dev_dbg(chip->dev, "dc current set to %d mA\n",
			chip->dc_max_current_ma);
	return smbchg_sec_masked_write(chip, chip->dc_chgpth_base + IL_CFG,
				DCIN_INPUT_MASK, dc_cur_val);
}

enum enable_reason {
	/* userspace has suspended charging altogether */
	REASON_USER = BIT(0),
	/*
	 * this specific path has been suspended through the power supply
	 * framework
	 */
	REASON_POWER_SUPPLY = BIT(1),
	/*
	 * the usb driver has suspended this path by setting a current limit
	 * of < 2MA
	 */
	REASON_USB = BIT(2),
	/*
	 * when a wireless charger comes online,
	 * the dc path is suspended for a second
	 */
	REASON_WIRELESS = BIT(3),
	/*
	 * the thermal daemon can suspend a charge path when the system
	 * temperature levels rise
	 */
	REASON_THERMAL = BIT(4),
};

static struct power_supply *get_parallel_psy(struct smbchg_chip *chip)
{
	if (chip->parallel.psy)
		return chip->parallel.psy;
	chip->parallel.psy = power_supply_get_by_name("usb-parallel");
	if (!chip->parallel.psy)
		pr_debug("parallel charger not found\n");
	return chip->parallel.psy;
}

#define ICL_STS_1_REG			0x7
#define ICL_STS_2_REG			0x9
#define ICL_STS_MASK			0x1F
#define AICL_STS_BIT			BIT(5)
#define USBIN_SUSPEND_STS_BIT		BIT(3)
#define USBIN_ACTIVE_PWR_SRC_BIT	BIT(1)
static void smbchg_parallel_usb_determine_current(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy;
	int current_limit_ma, parallel_cl_ma, total_current_ma;
	int new_parallel_cl_ma, rc;
	u8 reg;

	parallel_psy = get_parallel_psy(chip);
	if (!parallel_psy)
		return;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_2_REG, 1);
	if (rc) {
		pr_err("Could not read usb icl sts 2: %d\n", rc);
		return;
	}

	/*
	 * If the usbin is suspended or not the active power src, then this
	 * was triggered from DCIN AICL. Retrun silently if this is the case.
	 */
	if (!!(reg & USBIN_SUSPEND_STS_BIT) ||
				!(reg & USBIN_ACTIVE_PWR_SRC_BIT)) {
		pr_debug("USB not active power source: %02x\n", reg);
		return;
	}

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_1_REG, 1);
	if (rc) {
		pr_err("Could not read usb icl sts 1: %d\n", rc);
		return;
	}

	reg &= ICL_STS_MASK;
	if (reg >= ARRAY_SIZE(usb_current_table)) {
		pr_warn("invalid AICL value: %02x\n", reg);
		return;
	}

	parallel_cl_ma = chip->parallel.current_max_ma;
	if (parallel_cl_ma <= SUSPEND_CURRENT_MA)
		parallel_cl_ma = 0;

	current_limit_ma = usb_current_table[reg];
	total_current_ma = current_limit_ma + parallel_cl_ma;

	/*
	 * if the total available current is less than the minimum threshold
	 * to enable parallel charging, set the current limit to 0 to disable
	 * the parallel charge path.
	 *
	 * otherwise, set the parallel charge path's input current limit (ICL)
	 * to the total current / 2
	 */
	if (total_current_ma <= chip->parallel.min_current_thr_ma)
		new_parallel_cl_ma = 0;
	else
		new_parallel_cl_ma = total_current_ma / 2;

	if (new_parallel_cl_ma == 0)
		new_parallel_cl_ma = SUSPEND_CURRENT_MA;
	if (new_parallel_cl_ma < chip->parallel.current_max_ma
			|| chip->parallel.current_max_ma <= SUSPEND_CURRENT_MA)
		chip->parallel.current_max_ma = new_parallel_cl_ma;
	pr_debug("ICL at %d. Setting Parallel ICL at %d\n",
			current_limit_ma, chip->parallel.current_max_ma);

	mutex_lock(&chip->usb_en_lock);
	if (!chip->usb_suspended)
		power_supply_set_current_limit(parallel_psy,
					chip->parallel.current_max_ma * 1000);
	mutex_unlock(&chip->usb_en_lock);
}

static void smbchg_parallel_usb_en(struct smbchg_chip *chip, bool enable)
{
	struct power_supply *parallel_psy;

	parallel_psy = get_parallel_psy(chip);
	if (!parallel_psy)
		return;

	power_supply_set_current_limit(parallel_psy,
			enable ? chip->parallel.current_max_ma * 1000
			: (SUSPEND_CURRENT_MA * 1000));
	pr_debug("parallel charger %s\n",
			enable ? "unsuspended" : "suspended");
}

static void smbchg_usb_update_online_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				usb_set_online_work);
	bool user_enabled = (chip->usb_suspended & REASON_USER) == 0;
	int online = user_enabled && chip->usb_present;

	mutex_lock(&chip->usb_set_online_lock);
	if (chip->usb_online != online) {
		power_supply_set_online(chip->usb_psy, online);
		chip->usb_online = online;
	}
	mutex_unlock(&chip->usb_set_online_lock);
}

static int smbchg_usb_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	int rc = 0, suspended;

	pr_debug("usb charging %s, suspended = %02x, enable = %d, reason = %02x\n",
			chip->usb_suspended == 0 ? "enabled"
			: "suspended", chip->usb_suspended, enable, reason);
	mutex_lock(&chip->usb_en_lock);
	if (!enable)
		suspended = chip->usb_suspended | reason;
	else
		suspended = chip->usb_suspended & (~reason);

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->usb_suspended)
		goto out;

	if (chip->parallel.avail)
		smbchg_parallel_usb_en(chip, suspended == 0);

	rc = smbchg_usb_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set usb suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

	pr_debug("usb charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
out:
	chip->usb_suspended = suspended;
	mutex_unlock(&chip->usb_en_lock);
	return rc;
}

static int smbchg_dc_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	int rc = 0, suspended;

	pr_debug("dc charging %s, suspended = %02x, enable = %d, reason = %02x\n",
			chip->dc_suspended == 0 ? "enabled"
			: "suspended", chip->dc_suspended, enable, reason);
	mutex_lock(&chip->dc_en_lock);
	if (!enable)
		suspended = chip->dc_suspended | reason;
	else
		suspended = chip->dc_suspended & ~reason;

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->dc_suspended)
		goto out;

	rc = smbchg_dc_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set dc suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

	if (chip->psy_registered)
		power_supply_changed(&chip->dc_psy);
	pr_debug("dc charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
out:
	chip->dc_suspended = suspended;
	mutex_unlock(&chip->dc_en_lock);
	return rc;
}

#define CHGPTH_CFG		0xF4
#define CFG_USB_2_3_SEL_BIT	BIT(7)
#define CFG_USB_2		0
#define CFG_USB_3		BIT(7)
#define USBIN_INPUT_MASK	SMB_MASK(4, 0)
#define USBIN_MODE_CHG_BIT	BIT(0)
#define USBIN_LIMITED_MODE	0
#define USBIN_HC_MODE		BIT(0)
#define USB51_MODE_BIT		BIT(1)
#define USB51_100MA		0
#define USB51_500MA		BIT(1)
static int smbchg_set_high_usb_chg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);

		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);
		else
			chip->usb_max_current_ma = 150;
		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + IL_CFG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
				USBIN_MODE_CHG_BIT, USBIN_HC_MODE);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);
	chip->usb_max_current_ma = usb_current_table[i];
	return rc;
}

/* if APSD results are used
 *	if SDP is detected it will look at 500mA setting
 *		if set it will draw 500mA
 *		if unset it will draw 100mA
 *	if CDP/DCP it will look at 0x0C setting
 *		i.e. values in 0x41[1, 0] does not matter
 */
static int smbchg_set_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	int rc;

	if (!chip->batt_present) {
		pr_info_ratelimited("Ignoring usb current->%d, battery is absent\n",
				current_ma);
		return 0;
	}
	pr_debug("USB current_ma = %d\n", current_ma);

	if (current_ma == SUSPEND_CURRENT_MA) {
		/* suspend the usb if current set to 2mA */
		rc = smbchg_usb_en(chip, false, REASON_USB);
		chip->usb_max_current_ma = 0;
		goto out;
	} else {
		rc = smbchg_usb_en(chip, true, REASON_USB);
	}

	if (chip->low_icl_wa_on) {
		chip->usb_max_current_ma = current_ma;
		pr_debug("low_icl_wa on, ignoring the usb current setting\n");
		goto out;
	}
	if (current_ma < CURRENT_150_MA) {
		/* force 100mA */
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 100;
		goto out;
	}
	/* specific current values */
	if (current_ma == CURRENT_150_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 150;
		goto out;
	}
	if (current_ma == CURRENT_500_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 500;
		goto out;
	}
	if (current_ma == CURRENT_900_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 900;
		goto out;
	}

	rc = smbchg_set_high_usb_chg_current(chip, current_ma);
out:
	pr_debug("usb current set to %d mA\n", chip->usb_max_current_ma);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	return rc;
}

static int smbchg_low_icl_wa_check(struct smbchg_chip *chip)
{
	int rc = 0;
	bool enable = (get_prop_batt_status(chip)
		!= POWER_SUPPLY_STATUS_CHARGING);

	mutex_lock(&chip->current_change_lock);
	pr_debug("low icl %s -> %s\n", chip->low_icl_wa_on ? "on" : "off",
			enable ? "on" : "off");
	if (enable == chip->low_icl_wa_on)
		goto out;

	chip->low_icl_wa_on = enable;
	if (enable) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc)
			pr_err("could not set low current limit: %d\n", rc);
	} else {
		rc = smbchg_set_usb_current_max(chip, chip->usb_max_current_ma);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

/*
 * set the dc charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_dc_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	current_ma = calc_thermal_limited_current(chip, current_ma);
	return smbchg_set_dc_current_max(chip, current_ma);
}

/*
 * set the usb charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	current_ma = calc_thermal_limited_current(chip, current_ma);
	return smbchg_set_usb_current_max(chip, current_ma);
}

static int smbchg_system_temp_level_set(struct smbchg_chip *chip,
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
		/*
		 * Disable charging if highest value selected by
		 * setting the DC and USB path in suspend
		 */
		rc = smbchg_dc_en(chip, false, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
			goto out;
		}
		rc = smbchg_usb_en(chip, false, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
			goto out;
		}
		goto out;
	}

	rc = smbchg_set_thermal_limited_usb_current_max(chip,
					chip->usb_target_current_ma);
	rc = smbchg_set_thermal_limited_dc_current_max(chip,
					chip->dc_target_current_ma);

	if (prev_therm_lvl == chip->thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the DC and USB path
		 * out of suspend.
		 */
		rc = smbchg_dc_en(chip, true, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
			goto out;
		}
		rc = smbchg_usb_en(chip, true, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
			goto out;
		}
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smbchg_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smbchg_usb_en(chip, val->intval, REASON_USER);
		smbchg_dc_en(chip, val->intval, REASON_USER);
		chip->chg_enabled = val->intval;
		schedule_work(&chip->usb_set_online_work);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smbchg_system_temp_level_set(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smbchg_battery_is_writeable(struct power_supply *psy,
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

static int smbchg_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->therm_lvl_sel;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smbchg_dc_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
};

static int smbchg_dc_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return smbchg_dc_en(chip, val->intval, REASON_POWER_SUPPLY);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smbchg_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);
	bool user_enabled = (chip->dc_suspended & REASON_USER) == 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_dc_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->dc_suspended == 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* return if dc is charging the battery */
		val->intval = user_enabled && chip->dc_present;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smbchg_dc_is_writeable(struct power_supply *psy,
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

static void smbchg_external_power_changed(struct power_supply *psy)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0;

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
	if (rc < 0)
		dev_dbg(chip->dev,
			"could not read USB charging enabled, rc=%d\n", rc);
	else
		smbchg_usb_en(chip, prop.intval, REASON_POWER_SUPPLY);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;
	pr_debug("current_limit = %d\n", current_limit);

	mutex_lock(&chip->current_change_lock);
	chip->usb_target_current_ma = current_limit;
	rc = smbchg_set_thermal_limited_usb_current_max(chip, current_limit);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set usb current rc = %d\n", rc);
	mutex_unlock(&chip->current_change_lock);

	power_supply_changed(&chip->batt_psy);
}

#define VFLOAT_CFG_REG			0xF4
#define MIN_FLOAT_MV			3600
#define MAX_FLOAT_MV			4500
#define VFLOAT_MASK			SMB_MASK(5, 0)

#define MID_RANGE_FLOAT_MV_MIN		3600
#define MID_RANGE_FLOAT_MIN_VAL		0x05
#define MID_RANGE_FLOAT_STEP_MV		20

#define HIGH_RANGE_FLOAT_MIN_MV		4340
#define HIGH_RANGE_FLOAT_MIN_VAL	0x2A
#define HIGH_RANGE_FLOAT_STEP_MV	10

#define VHIGH_RANGE_FLOAT_MIN_MV	4400
#define VHIGH_RANGE_FLOAT_MIN_VAL	0x2E
#define VHIGH_RANGE_FLOAT_STEP_MV	20
static int smbchg_float_voltage_set(struct smbchg_chip *chip, int vfloat_mv)
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

	return smbchg_sec_masked_write(chip, chip->chgr_base + VFLOAT_CFG_REG,
			VFLOAT_MASK, temp);
}

#define CMD_CHG_REG	0x42
#define OTG_EN		BIT(0)
static int smbchg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	chip->otg_retries = 0;
	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, OTG_EN);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
	pr_debug("Enabling OTG Boost\n");
	return rc;
}

static int smbchg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
	pr_debug("Disabling OTG Boost\n");
	return rc;
}

static int smbchg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_read(chip, &reg, chip->bat_if_base + CMD_CHG_REG, 1);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return (reg & OTG_EN) ? 1 : 0;
}

struct regulator_ops smbchg_otg_reg_ops = {
	.enable		= smbchg_otg_regulator_enable,
	.disable	= smbchg_otg_regulator_disable,
	.is_enabled	= smbchg_otg_regulator_is_enable,
};

static int smbchg_regulator_init(struct smbchg_chip *chip)
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
		chip->otg_vreg.rdesc.ops = &smbchg_otg_reg_ops;
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

static void smbchg_regulator_deinit(struct smbchg_chip *chip)
{
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
}

static void wireless_insertion_work(struct work_struct *work)
{
	struct smbchg_chip *chip =
		container_of(work, struct smbchg_chip,
				wireless_insertion_work.work);

	/* unsuspend dc */
	smbchg_dc_en(chip, false, REASON_WIRELESS);
}

#define HOT_BAT_HARD_BIT	BIT(0)
#define HOT_BAT_SOFT_BIT	BIT(1)
#define COLD_BAT_HARD_BIT	BIT(2)
#define COLD_BAT_SOFT_BIT	BIT(3)
#define BAT_OV_BIT		BIT(4)
#define BAT_LOW_BIT		BIT(5)
#define BAT_MISSING_BIT		BIT(6)
#define BAT_TERM_MISSING_BIT	BIT(7)
static irqreturn_t batt_hot_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_hot = !!(reg & HOT_BAT_HARD_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	smbchg_low_icl_wa_check(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cold_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cold = !!(reg & COLD_BAT_HARD_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	smbchg_low_icl_wa_check(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_warm_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_warm = !!(reg & HOT_BAT_SOFT_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cool_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cool = !!(reg & COLD_BAT_SOFT_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_pres_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_present = !(reg & BAT_MISSING_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t vbat_low_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("vbat low\n");
	return IRQ_HANDLED;
}

static irqreturn_t chg_error_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_debug("chg-error triggered\n");
	smbchg_low_icl_wa_check(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fastchg_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_debug("p2f triggered\n");
	smbchg_low_icl_wa_check(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t chg_hot_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("chg hot\n");
	return IRQ_HANDLED;
}

#define CHG_INHIBIT_BIT		BIT(1)
#define BAT_TAPER_MODE_BIT	BIT(6)
#define BAT_TCC_REACHED_BIT	BIT(7)
static irqreturn_t chg_term_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	chip->chg_done_batt_full = !!(reg & BAT_TCC_REACHED_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t taper_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_debug("triggered: 0x%02x\n", reg);
	return IRQ_HANDLED;
}

static irqreturn_t recharge_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t safety_timeout_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_warn_ratelimited("safety timeout rt_stat = 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

/**
 * power_ok_handler() - called when the switcher turns on or turns off
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating switcher turning on or off
 */
static irqreturn_t power_ok_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_debug("triggered: 0x%02x\n", reg);
	return IRQ_HANDLED;
}

/**
 * dcin_uv_handler() - called when the dc voltage crosses the uv threshold
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating whether dc voltage is uv
 */
#define DCIN_UNSUSPEND_DELAY_MS		1000
static irqreturn_t dcin_uv_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool dc_present = is_dc_present(chip);

	pr_debug("chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

	if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;
		if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS) {
			cancel_delayed_work_sync(
				&chip->wireless_insertion_work);
			smbchg_dc_en(chip, true, REASON_WIRELESS);
		}
	}

	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS)
			schedule_delayed_work(&chip->wireless_insertion_work,
				msecs_to_jiffies(DCIN_UNSUSPEND_DELAY_MS));
	}
	if (chip->psy_registered)
		power_supply_changed(&chip->dc_psy);

	return IRQ_HANDLED;
}

static void handle_usb_removal(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy;

	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n",
				POWER_SUPPLY_TYPE_UNKNOWN);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		schedule_work(&chip->usb_set_online_work);
	}
	if (chip->parallel.avail) {
		parallel_psy = get_parallel_psy(chip);
		if (parallel_psy) {
			power_supply_set_present(parallel_psy, false);
			chip->parallel.current_max_ma = SUSPEND_CURRENT_MA;
			disable_irq_wake(chip->aicl_done_irq);
		}
	}
}

#define IDEV_STS	0x8
static void handle_usb_insertion(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy;
	u8 reg = 0;
	int rc;
	char *usb_type_name = "null";
	enum power_supply_type usb_supply_type;

	/* usb inserted */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
	usb_type_name = get_usb_type_name(reg);
	usb_supply_type = get_usb_supply_type(reg);
	pr_debug("inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);
	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n", usb_supply_type);
		power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		schedule_work(&chip->usb_set_online_work);
	}
	if (chip->parallel.avail) {
		chip->parallel.current_max_ma = SUSPEND_CURRENT_MA;
		parallel_psy = get_parallel_psy(chip);
		if (parallel_psy) {
			power_supply_set_present(parallel_psy, true);
			enable_irq_wake(chip->aicl_done_irq);
		}
	}
}

/**
 * usbin_uv_handler() - this is called when USB charger is removed
 * @chip: pointer to smbchg_chip chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t usbin_uv_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

	pr_debug("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);
	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}
	return IRQ_HANDLED;
}

/**
 * src_detect_handler() - this is called when USB charger type is detected, use
 *			it for handling USB charger insertion
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t src_detect_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

	pr_debug("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	}

	return IRQ_HANDLED;
}

/**
 * otg_oc_handler() - called when the usb otg goes over current
 */
#define NUM_OTG_RETRIES			1
static irqreturn_t otg_oc_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_debug("triggered\n");
	/*
	 * Due to a HW bug in the PMI8994 charger, the current inrush that
	 * occurs when connecting certain OTG devices can cause the OTG
	 * overcurrent protection to trip.
	 *
	 * The work around is to try reenabling the OTG when getting an
	 * overcurrent interrupt once.
	 */
	if (chip->otg_retries < NUM_OTG_RETRIES) {
		chip->otg_retries += 1;
		pr_debug("Retrying OTG enable. Try #%d\n", chip->otg_retries);
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, 0);
		msleep(20);
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, OTG_EN);
	}
	return IRQ_HANDLED;
}

/**
 * otg_fail_handler() - called when the usb otg fails
 * (when vbat < OTG UVLO threshold)
 */
static irqreturn_t otg_fail_handler(int irq, void *_chip)
{
	pr_debug("triggered\n");
	return IRQ_HANDLED;
}

/**
 * aicl_done_handler() - called when the usb AICL algorithm is finished
 *			and a current is set.
 */
static irqreturn_t aicl_done_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

	pr_debug("aicl_done triggered\n");
	if (chip->parallel.avail && usb_present)
		smbchg_parallel_usb_determine_current(chip);
	return IRQ_HANDLED;
}

/**
 * usbid_change_handler() - called when the usb RID changes.
 * This is used mostly for detecting OTG
 */
static irqreturn_t usbid_change_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool otg_present;

	pr_debug("triggered\n");

	/*
	 * After the falling edge of the usbid change interrupt occurs,
	 * there may still be some time before the ADC conversion for USB RID
	 * finishes in the fuel gauge.
	 *
	 * Sleep for a bit to wait for the conversion to finish and the USB RID
	 * status register to be updated before trying to detect OTG insertions.
	 */
	usleep_range(5000, 20000);
	otg_present = is_otg_present(chip);
	if (chip->usb_psy)
		power_supply_set_usb_otg(chip->usb_psy, otg_present ? 1 : 0);
	if (otg_present)
		pr_debug("OTG detected\n");

	return IRQ_HANDLED;
}

static irqreturn_t chg_inhibit_handler(int irq, void *_chip)
{
	/*
	 * charger is inserted when the battery voltage is high
	 * so h/w won't start charging just yet. Treat this as
	 * battery full
	 */
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	chip->chg_done_batt_full = !!(reg & CHG_INHIBIT_BIT);
	pr_debug("triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static int determine_initial_status(struct smbchg_chip *chip)
{
	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */

	batt_pres_handler(0, chip);
	batt_hot_handler(0, chip);
	batt_warm_handler(0, chip);
	batt_cool_handler(0, chip);
	batt_cold_handler(0, chip);
	chg_term_handler(0, chip);
	usbid_change_handler(0, chip);

	chip->usb_present = is_usb_present(chip);
	chip->dc_present = is_dc_present(chip);

	if (chip->usb_present)
		handle_usb_insertion(chip);
	else
		handle_usb_removal(chip);

	if (chip->dc_psy_type != -EINVAL) {
		if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS) {
			/*
			 * put the dc path in suspend state if it is powered
			 * by wireless charger
			 */
			if (chip->dc_present)
				smbchg_dc_en(chip, false, REASON_WIRELESS);
			else
				smbchg_dc_en(chip, true, REASON_WIRELESS);
		}
	}
	return 0;
}

static int prechg_time[] = {
	24,
	48,
	96,
	192,
};
static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

#define CHGR_CFG1			0xFB
#define RECHG_THRESHOLD_SRC_BIT		BIT(1)
#define TERM_I_SRC_BIT			BIT(2)
#define CHGR_CFG2			0xFC
#define CHG_INHIB_CFG_REG		0xF7
#define CHG_INHIBIT_50MV_VAL		0x00
#define CHG_INHIBIT_100MV_VAL		0x01
#define CHG_INHIBIT_200MV_VAL		0x02
#define CHG_INHIBIT_300MV_VAL		0x03
#define CHG_INHIBIT_MASK		0x03
#define USE_REGISTER_FOR_CURRENT	BIT(2)
#define CHG_EN_SRC_BIT			BIT(7)
#define CHG_EN_COMMAND_BIT		BIT(6)
#define P2F_CHG_TRAN			BIT(5)
#define I_TERM_BIT			BIT(3)
#define AUTO_RECHG_BIT			BIT(2)
#define CHARGER_INHIBIT_BIT		BIT(0)
#define CFG_TCC_REG			0xF9
#define CHG_ITERM_50MA			0x1
#define CHG_ITERM_100MA			0x2
#define CHG_ITERM_150MA			0x3
#define CHG_ITERM_200MA			0x4
#define CHG_ITERM_250MA			0x5
#define CHG_ITERM_300MA			0x0
#define CHG_ITERM_500MA			0x6
#define CHG_ITERM_600MA			0x7
#define CHG_ITERM_MASK			SMB_MASK(2, 0)
#define USBIN_SUSPEND_SRC_BIT		BIT(6)
#define USB51_COMMAND_POL		BIT(2)
#define USB51AC_CTRL			BIT(1)
#define SFT_CFG				0xFD
#define SFT_EN_MASK			SMB_MASK(5, 4)
#define SFT_TO_MASK			SMB_MASK(3, 2)
#define PRECHG_SFT_TO_MASK		SMB_MASK(1, 0)
#define SFT_TIMER_DISABLE_BIT		BIT(5)
#define PRECHG_SFT_TIMER_DISABLE_BIT	BIT(4)
#define SAFETY_TIME_MINUTES_SHIFT	2
#define BM_CFG				0xF3
#define BATT_MISSING_ALGO_BIT		BIT(2)
#define BMD_PIN_SRC_MASK		SMB_MASK(1, 0)
#define PIN_SRC_SHIFT			0
#define CHGR_CFG			0xFF
#define RCHG_LVL_BIT			BIT(0)
#define CFG_AFVC			0xF5
#define VFLOAT_COMP_ENABLE_MASK		SMB_MASK(2, 0)
#define TR_RID_REG			0xFA
#define FG_INPUT_FET_DELAY_BIT		BIT(3)
#define TRIM_OPTIONS_7_0		0xF6
#define INPUT_MISSING_POLLER_EN_BIT	BIT(3)
static int smbchg_hw_init(struct smbchg_chip *chip)
{
	int rc, i;
	u8 reg;

	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + TR_RID_REG,
			FG_INPUT_FET_DELAY_BIT, FG_INPUT_FET_DELAY_BIT);
	if (rc < 0) {
		pr_err("Couldn't disable fg input fet delay rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_OPTIONS_7_0,
			INPUT_MISSING_POLLER_EN_BIT, 0);
	if (rc < 0) {
		pr_err("Couldn't disable input missing poller rc=%d\n", rc);
		return rc;
	}

	/*
	 * force using current from the register i.e. ignore auto
	 * power source detect (APSD) mA ratings
	 */
	reg = USE_REGISTER_FOR_CURRENT;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USE_REGISTER_FOR_CURRENT, USE_REGISTER_FOR_CURRENT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set input limit cmd rc=%d\n", rc);
		return rc;
	}

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast, enable current termination, enable
	 * auto recharge, enable chg inhibition
	 */
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
			CHG_EN_SRC_BIT | CHG_EN_COMMAND_BIT | P2F_CHG_TRAN
			| I_TERM_BIT | AUTO_RECHG_BIT | CHARGER_INHIBIT_BIT,
			CHARGER_INHIBIT_BIT | CHG_EN_COMMAND_BIT
			| (chip->iterm_disabled ? I_TERM_BIT : 0));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}

	/*
	 * use the analog sensors instead of the fuelgauge adcs so that
	 * tcc detection works without trimmed parts
	 */
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG1,
			TERM_I_SRC_BIT | RECHG_THRESHOLD_SRC_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}

	/*
	 * control USB suspend via command bits and set correct 100/500mA
	 * polarity on the usb current
	 */
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + CHGPTH_CFG,
		USBIN_SUSPEND_SRC_BIT | USB51_COMMAND_POL | USB51AC_CTRL,
		USBIN_SUSPEND_SRC_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smbchg_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
		pr_debug("set vfloat to %d\n", chip->vfloat_mv);
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			if (chip->iterm_ma <= 50)
				reg = CHG_ITERM_50MA;
			else if (chip->iterm_ma <= 100)
				reg = CHG_ITERM_100MA;
			else if (chip->iterm_ma <= 150)
				reg = CHG_ITERM_150MA;
			else if (chip->iterm_ma <= 200)
				reg = CHG_ITERM_200MA;
			else if (chip->iterm_ma <= 250)
				reg = CHG_ITERM_250MA;
			else if (chip->iterm_ma <= 300)
				reg = CHG_ITERM_300MA;
			else if (chip->iterm_ma <= 500)
				reg = CHG_ITERM_500MA;
			else
				reg = CHG_ITERM_600MA;

			rc = smbchg_sec_masked_write(chip,
					chip->chgr_base + CFG_TCC_REG,
					CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}
			pr_debug("set tcc (%d) to 0x%02x\n", chip->iterm_ma,
					reg);
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		reg = (chip->safety_time > 0 ? 0 : SFT_TIMER_DISABLE_BIT) |
			(chip->prechg_safety_time > 0
			? 0 : PRECHG_SFT_TIMER_DISABLE_BIT);

		for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
			if (chip->safety_time <= chg_time[i]) {
				reg |= i << SAFETY_TIME_MINUTES_SHIFT;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(prechg_time); i++) {
			if (chip->prechg_safety_time <= prechg_time[i]) {
				reg |= i;
				break;
			}
		}

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + SFT_CFG,
				SFT_EN_MASK | SFT_TO_MASK |
				(chip->prechg_safety_time > 0
				? PRECHG_SFT_TO_MASK : 0), reg);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set safety timer rc = %d\n",
				rc);
			return rc;
		}
	}

	/* battery missing detection */
	rc = smbchg_sec_masked_write(chip, chip->bat_if_base + BM_CFG,
			BATT_MISSING_ALGO_BIT
			| (chip->bmd_pin_src > 0 ? BMD_PIN_SRC_MASK : 0),
			(chip->bmd_algo_disabled ? BATT_MISSING_ALGO_BIT : 0)
			| (chip->bmd_pin_src << PIN_SRC_SHIFT));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set batt_missing config = %d\n",
									rc);
		return rc;
	}

	smbchg_low_icl_wa_check(chip);

	/*
	 * The charger needs 20 milliseconds to go into battery supplementary
	 * mode. Sleep here until we are sure it takes into effect.
	 */
	msleep(20);
	smbchg_usb_en(chip, chip->chg_enabled, REASON_USER);
	smbchg_dc_en(chip, chip->chg_enabled, REASON_USER);
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

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + CHG_INHIB_CFG_REG,
				CHG_INHIBIT_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set inhibit val rc = %d\n",
					rc);
			return rc;
		}

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + CHGR_CFG,
				RCHG_LVL_BIT, (chip->resume_delta_mv < 200)
				? 0 : RCHG_LVL_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set recharge rc = %d\n",
					rc);
			return rc;
		}
	}

	/* DC path current settings */
	if (chip->dc_psy_type != -EINVAL)
		return smbchg_set_thermal_limited_dc_current_max(chip,
						chip->dc_target_current_ma);

	/*
	 * on some devices the battery is powered via external sources which
	 * could raise its voltage above the float voltage. smbchargers go
	 * in to reverse boost in such a situation and the workaround is to
	 * disable float voltage compensation (note that the battery will appear
	 * hot/cold when powered via external source).
	 */
	if (chip->soft_vfloat_comp_disabled) {
		rc |= smbchg_sec_masked_write(chip, chip->chgr_base + CFG_AFVC,
				VFLOAT_COMP_ENABLE_MASK, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't disable soft vfloat rc = %d\n",
					rc);
			return rc;
		}
	}

	/*
	 * Allow the smb charger to enable charging based on the command
	 * register rather than the enable pin.
	 */
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
			CHG_EN_SRC_BIT,
			CHG_EN_SRC_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't switch to cmd register for enable = %d\n",
				rc);
		return rc;
	}

	return rc;
}

static struct of_device_id smbchg_match_table[] = {
	{
		.compatible	= "qcom,qpnp-smbcharger",
		.data		= (void *)ARRAY_SIZE(usb_current_table),
	},
	{ },
};

enum bpd_type {
	BPD_TYPE_BAT_NONE,
	BPD_TYPE_BAT_ID,
	BPD_TYPE_BAT_THM,
	BPD_TYPE_BAT_THM_BAT_ID,
};

static const char * const bpd_label[] = {
	[BPD_TYPE_BAT_NONE]		= "bpd_none",
	[BPD_TYPE_BAT_ID]		= "bpd_id",
	[BPD_TYPE_BAT_THM]		= "bpd_thm",
	[BPD_TYPE_BAT_THM_BAT_ID]	= "bpd_thm_id",
};

static inline int get_bpd(const char *name)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(bpd_label); i++) {
		if (strcmp(bpd_label[i], name) == 0)
			return i;
	}
	return -EINVAL;
}

#define DC_MA_MIN 300
#define DC_MA_MAX 2000
#define OF_PROP_READ(chip, prop, dt_property, retval, optional)		\
do {									\
	if (retval)							\
		break;							\
	if (optional)							\
		prop = -EINVAL;						\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," dt_property	,	\
					&prop);				\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		pr_err("Error reading " #dt_property			\
				" property rc = %d\n", rc);		\
} while (0)

static int smb_parse_dt(struct smbchg_chip *chip)
{
	int rc = 0;
	struct device_node *node = chip->dev->of_node;
	const char *dc_psy_type, *bpd;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	/* read optional u32 properties */
	OF_PROP_READ(chip, chip->iterm_ma, "iterm-ma", rc, 1);
	OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv", rc, 1);
	OF_PROP_READ(chip, chip->safety_time, "charging-timeout-mins", rc, 1);
	OF_PROP_READ(chip, chip->prechg_safety_time, "precharging-timeout-mins",
			rc, 1);
	if (chip->safety_time != -EINVAL &&
		(chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout-mins %d\n",
						chip->safety_time);
		return -EINVAL;
	}
	if (chip->prechg_safety_time != -EINVAL &&
		(chip->prechg_safety_time >
		 prechg_time[ARRAY_SIZE(prechg_time) - 1])) {
		dev_err(chip->dev, "Bad precharging-timeout-mins %d\n",
						chip->prechg_safety_time);
		return -EINVAL;
	}
	OF_PROP_READ(chip, chip->resume_delta_mv, "resume-delta-mv", rc, 1);
	OF_PROP_READ(chip, chip->parallel.min_current_thr_ma,
			"parallel-usb-min-current-ma", rc, 1);
	if (chip->parallel.min_current_thr_ma != -EINVAL)
		chip->parallel.avail = true;

	/* read boolean configuration properties */
	chip->bmd_algo_disabled = of_property_read_bool(node,
						"qcom,bmd-algo-disabled");
	chip->iterm_disabled = of_property_read_bool(node,
						"qcom,iterm-disabled");
	chip->soft_vfloat_comp_disabled = of_property_read_bool(node,
					"qcom,soft-vfloat-comp-disabled");
	chip->chg_enabled = !(of_property_read_bool(node,
						"qcom,charging-disabled"));

	/* parse the battery missing detection pin source */
	rc = of_property_read_string(chip->spmi->dev.of_node,
		"qcom,bmd-pin-src", &bpd);
	if (rc) {
		/* Select BAT_THM as default BPD scheme */
		chip->bmd_pin_src = BPD_TYPE_BAT_THM;
		rc = 0;
	} else {
		chip->bmd_pin_src = get_bpd(bpd);
		if (chip->bmd_pin_src < 0) {
			pr_err("failed to determine bpd schema %d\n", rc);
			return rc;
		}
	}

	/* parse the dc power supply configuration */
	rc = of_property_read_string(node, "qcom,dc-psy-type", &dc_psy_type);
	if (rc) {
		chip->dc_psy_type = -EINVAL;
		rc = 0;
	} else {
		if (strcmp(dc_psy_type, "Mains") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_MAINS;
		else if (strcmp(dc_psy_type, "Wireless") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIRELESS;
	}
	if (chip->dc_psy_type != -EINVAL) {
		OF_PROP_READ(chip, chip->dc_target_current_ma,
				"dc-psy-ma", rc, 0);
		if (rc)
			return rc;
		if (chip->dc_target_current_ma < DC_MA_MIN
				|| chip->dc_target_current_ma > DC_MA_MAX) {
			dev_err(chip->dev, "Bad dc mA %d\n",
					chip->dc_target_current_ma);
			return -EINVAL;
		}
	}

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,battery-psy-name",
						&chip->battery_psy_name);
	if (rc)
		chip->battery_psy_name = "battery";

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

	return 0;
}

#define SUBTYPE_REG			0x5
#define SMBCHG_CHGR_SUBTYPE		0x1
#define SMBCHG_OTG_SUBTYPE		0x8
#define SMBCHG_BAT_IF_SUBTYPE		0x3
#define SMBCHG_USB_CHGPTH_SUBTYPE	0x4
#define SMBCHG_DC_CHGPTH_SUBTYPE	0x5
#define SMBCHG_MISC_SUBTYPE		0x7
#define REQUEST_IRQ(chip, resource, irq_num, irq_name, irq_handler, flags, rc)\
do {									\
	irq_num = spmi_get_irq_byname(chip->spmi,			\
					resource, irq_name);		\
	if (irq_num < 0) {						\
		dev_err(chip->dev, "Unable to get " irq_name " irq\n");	\
		return -ENXIO;						\
	}								\
	rc = devm_request_threaded_irq(chip->dev,			\
			irq_num, NULL, irq_handler, flags, irq_name,	\
			chip);						\
	if (rc < 0) {							\
		dev_err(chip->dev, "Unable to request " irq_name " irq: %d\n",\
				rc);					\
		return -ENXIO;						\
	}								\
} while (0)

static int smbchg_request_irqs(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
							| IRQF_ONESHOT;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->chg_error_irq,
				"chg-error", chg_error_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->taper_irq,
				"chg-taper-thr", taper_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->chg_term_irq,
				"chg-tcc-thr", chg_term_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->chg_inhibit_irq,
				"chg-inhibit", chg_inhibit_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->recharge_irq,
				"chg-rechg-thr", recharge_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->fastchg_irq,
				"chg-p2f-thr", fastchg_handler, flags, rc);
			enable_irq_wake(chip->chg_term_irq);
			enable_irq_wake(chip->chg_error_irq);
			enable_irq_wake(chip->fastchg_irq);
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->batt_hot_irq,
				"batt-hot", batt_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_warm_irq,
				"batt-warm", batt_warm_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cool_irq,
				"batt-cool", batt_cool_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cold_irq,
				"batt-cold", batt_cold_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_missing_irq,
				"batt-missing", batt_pres_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->vbat_low_irq,
				"batt-low", vbat_low_handler, flags, rc);
			enable_irq_wake(chip->batt_hot_irq);
			enable_irq_wake(chip->batt_warm_irq);
			enable_irq_wake(chip->batt_cool_irq);
			enable_irq_wake(chip->batt_cold_irq);
			enable_irq_wake(chip->batt_missing_irq);
			enable_irq_wake(chip->vbat_low_irq);
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->usbin_uv_irq,
				"usbin-uv", usbin_uv_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->src_detect_irq,
				"usbin-src-det",
				src_detect_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_fail_irq,
				"otg-fail", otg_fail_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_oc_irq,
				"otg-oc", otg_oc_handler,
				(IRQF_TRIGGER_RISING | IRQF_ONESHOT), rc);
			REQUEST_IRQ(chip, spmi_resource, chip->aicl_done_irq,
				"aicl-done",
				aicl_done_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource,
				chip->usbid_change_irq, "usbid-change",
				usbid_change_handler,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), rc);
			enable_irq_wake(chip->usbin_uv_irq);
			enable_irq_wake(chip->src_detect_irq);
			enable_irq_wake(chip->otg_fail_irq);
			enable_irq_wake(chip->otg_oc_irq);
			enable_irq_wake(chip->usbid_change_irq);
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->dcin_uv_irq,
				"dcin-uv", dcin_uv_handler, flags, rc);
			enable_irq_wake(chip->dcin_uv_irq);
			break;
		case SMBCHG_MISC_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->power_ok_irq,
				"power-ok", power_ok_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->chg_hot_irq,
				"temp-shutdown", chg_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource,
				chip->safety_timeout_irq,
				"safety-timeout",
				safety_timeout_handler, flags, rc);
			enable_irq_wake(chip->chg_hot_irq);
			enable_irq_wake(chip->safety_timeout_irq);
			break;
		case SMBCHG_OTG_SUBTYPE:
			break;
		}
	}

	return rc;
}

#define REQUIRE_BASE(chip, base, rc)					\
do {									\
	if (!rc && !chip->base) {					\
		dev_err(chip->dev, "Missing " #base "\n");		\
		rc = -EINVAL;						\
	}								\
} while (0)

static int smbchg_parse_peripherals(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			chip->chgr_base = resource->start;
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			chip->bat_if_base = resource->start;
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			chip->usb_chgpth_base = resource->start;
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			chip->dc_chgpth_base = resource->start;
			break;
		case SMBCHG_MISC_SUBTYPE:
			chip->misc_base = resource->start;
			break;
		case SMBCHG_OTG_SUBTYPE:
			chip->otg_base = resource->start;
			break;
		}
	}

	REQUIRE_BASE(chip, chgr_base, rc);
	REQUIRE_BASE(chip, bat_if_base, rc);
	REQUIRE_BASE(chip, usb_chgpth_base, rc);
	REQUIRE_BASE(chip, dc_chgpth_base, rc);
	REQUIRE_BASE(chip, misc_base, rc);

	return rc;
}

static inline void dump_reg(struct smbchg_chip *chip, u16 addr,
		const char *name)
{
	u8 reg;

	smbchg_read(chip, &reg, addr, 1);
	pr_debug("%s - %04X = %02X\n", name, addr, reg);
}

/* dumps useful registers for debug */
static void dump_regs(struct smbchg_chip *chip)
{
	u16 addr;

	/* charger peripheral */
	for (addr = 0xB; addr <= 0x10; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Status");
	for (addr = 0xF0; addr <= 0xFF; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Config");
	/* battery interface peripheral */
	dump_reg(chip, chip->bat_if_base + RT_STS, "BAT_IF Status");
	dump_reg(chip, chip->bat_if_base + CMD_CHG_REG, "BAT_IF Command");
	for (addr = 0xF0; addr <= 0xFB; addr++)
		dump_reg(chip, chip->bat_if_base + addr, "BAT_IF Config");
	/* usb charge path peripheral */
	for (addr = 0x7; addr <= 0x10; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Status");
	dump_reg(chip, chip->usb_chgpth_base + CMD_IL, "USB Command");
	for (addr = 0xF0; addr <= 0xF5; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Config");
	/* dc charge path peripheral */
	dump_reg(chip, chip->dc_chgpth_base + RT_STS, "DC Status");
	for (addr = 0xF0; addr <= 0xF6; addr++)
		dump_reg(chip, chip->dc_chgpth_base + addr, "DC Config");
	/* misc peripheral */
	dump_reg(chip, chip->misc_base + IDEV_STS, "MISC Status");
	dump_reg(chip, chip->misc_base + RT_STS, "MISC Status");
	for (addr = 0xF0; addr <= 0xF3; addr++)
		dump_reg(chip, chip->misc_base + addr, "MISC CFG");
}

static int smbchg_probe(struct spmi_device *spmi)
{
	int rc;
	struct smbchg_chip *chip;
	struct power_supply *usb_psy;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&spmi->dev, "USB supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&chip->wireless_insertion_work,
					wireless_insertion_work);
	INIT_WORK(&chip->usb_set_online_work, smbchg_usb_update_online_work);
	chip->spmi = spmi;
	chip->dev = &spmi->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;
	chip->usb_online = -EINVAL;
	dev_set_drvdata(&spmi->dev, chip);

	spin_lock_init(&chip->sec_access_lock);
	mutex_init(&chip->current_change_lock);
	mutex_init(&chip->usb_set_online_lock);
	mutex_init(&chip->usb_en_lock);
	mutex_init(&chip->dc_en_lock);

	rc = smbchg_parse_peripherals(chip);
	if (rc) {
		dev_err(chip->dev, "Error parsing DT peripherals: %d\n", rc);
		return rc;
	}
	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to parse DT nodes: %d\n", rc);
		return rc;
	}

	rc = smbchg_regulator_init(chip);
	if (rc) {
		dev_err(&spmi->dev,
			"Couldn't initialize regulator rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_hw_init(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		goto free_regulator;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to determine init status rc = %d\n", rc);
		goto free_regulator;
	}

	chip->batt_psy.name		= chip->battery_psy_name;
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smbchg_battery_get_property;
	chip->batt_psy.set_property	= smbchg_battery_set_property;
	chip->batt_psy.properties	= smbchg_battery_properties;
	chip->batt_psy.num_properties	= ARRAY_SIZE(smbchg_battery_properties);
	chip->batt_psy.external_power_changed = smbchg_external_power_changed;
	chip->batt_psy.property_is_writeable = smbchg_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		goto free_regulator;
	}

	if (chip->dc_psy_type != -EINVAL) {
		chip->dc_psy.name		= "dc";
		chip->dc_psy.type		= chip->dc_psy_type;
		chip->dc_psy.get_property	= smbchg_dc_get_property;
		chip->dc_psy.set_property	= smbchg_dc_set_property;
		chip->dc_psy.property_is_writeable = smbchg_dc_is_writeable;
		chip->dc_psy.properties		= smbchg_dc_properties;
		chip->dc_psy.num_properties = ARRAY_SIZE(smbchg_dc_properties);
		rc = power_supply_register(chip->dev, &chip->dc_psy);
		if (rc < 0) {
			dev_err(&spmi->dev,
				"Unable to register dc_psy rc = %d\n", rc);
			goto unregister_batt_psy;
		}
	}
	chip->psy_registered = true;

	rc = smbchg_request_irqs(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to request irqs rc = %d\n", rc);
		goto unregister_dc_psy;
	}

	power_supply_set_present(chip->usb_psy, chip->usb_present);

	dump_regs(chip);
	dev_info(chip->dev, "SMBCHG successfully probed batt=%d dc = %d usb = %d\n",
			get_prop_batt_present(chip),
			chip->dc_present, chip->usb_present);
	return 0;

unregister_dc_psy:
	power_supply_unregister(&chip->dc_psy);
unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
free_regulator:
	smbchg_regulator_deinit(chip);
	handle_usb_removal(chip);
	return rc;
}

static int smbchg_remove(struct spmi_device *spmi)
{
	struct smbchg_chip *chip = dev_get_drvdata(&spmi->dev);

	cancel_delayed_work_sync(&chip->wireless_insertion_work);
	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);

	power_supply_unregister(&chip->batt_psy);
	smbchg_regulator_deinit(chip);

	return 0;
}

static const struct dev_pm_ops smbchg_pm_ops = {
};

MODULE_DEVICE_TABLE(spmi, smbchg_id);

static struct spmi_driver smbchg_driver = {
	.driver		= {
		.name		= "qpnp-smbcharger",
		.owner		= THIS_MODULE,
		.of_match_table	= smbchg_match_table,
		.pm		= &smbchg_pm_ops,
	},
	.probe		= smbchg_probe,
	.remove		= smbchg_remove,
};

static int __init smbchg_init(void)
{
	return spmi_driver_register(&smbchg_driver);
}

static void __exit smbchg_exit(void)
{
	return spmi_driver_unregister(&smbchg_driver);
}

module_init(smbchg_init);
module_exit(smbchg_exit);

MODULE_DESCRIPTION("QPNP SMB Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qpnp-smbcharger");
