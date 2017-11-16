/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/pmic-voter.h>
#include <linux/delay.h>

#define QNOVO_REVISION1		0x00
#define QNOVO_REVISION2		0x01
#define QNOVO_PERPH_TYPE	0x04
#define QNOVO_PERPH_SUBTYPE	0x05
#define QNOVO_PTTIME_STS	0x07
#define QNOVO_PTRAIN_STS	0x08
#define QNOVO_ERROR_STS		0x09
#define QNOVO_ERROR_BIT		BIT(0)
#define QNOVO_ERROR_STS2	0x0A
#define QNOVO_ERROR_CHARGING_DISABLED	BIT(1)
#define QNOVO_INT_RT_STS	0x10
#define QNOVO_INT_SET_TYPE	0x11
#define QNOVO_INT_POLARITY_HIGH	0x12
#define QNOVO_INT_POLARITY_LOW	0x13
#define QNOVO_INT_LATCHED_CLR	0x14
#define QNOVO_INT_EN_SET	0x15
#define QNOVO_INT_EN_CLR	0x16
#define QNOVO_INT_LATCHED_STS	0x18
#define QNOVO_INT_PENDING_STS	0x19
#define QNOVO_INT_MID_SEL	0x1A
#define QNOVO_INT_PRIORITY	0x1B
#define QNOVO_PE_CTRL		0x40
#define QNOVO_PREST1_CTRL	0x41
#define QNOVO_PPULS1_LSB_CTRL	0x42
#define QNOVO_PPULS1_MSB_CTRL	0x43
#define QNOVO_NREST1_CTRL	0x44
#define QNOVO_NPULS1_CTRL	0x45
#define QNOVO_PPCNT_CTRL	0x46
#define QNOVO_VLIM1_LSB_CTRL	0x47
#define QNOVO_VLIM1_MSB_CTRL	0x48
#define QNOVO_PTRAIN_EN		0x49
#define QNOVO_PTRAIN_EN_BIT	BIT(0)
#define QNOVO_PE_CTRL2		0x4A
#define QNOVO_PREST2_LSB_CTRL	0x50
#define QNOVO_PREST2_MSB_CTRL	0x51
#define QNOVO_PPULS2_LSB_CTRL	0x52
#define QNOVO_PPULS2_MSB_CTRL	0x53
#define QNOVO_NREST2_CTRL	0x54
#define QNOVO_NPULS2_CTRL	0x55
#define QNOVO_VLIM2_LSB_CTRL	0x56
#define QNOVO_VLIM2_MSB_CTRL	0x57
#define QNOVO_PVOLT1_LSB	0x60
#define QNOVO_PVOLT1_MSB	0x61
#define QNOVO_PCUR1_LSB		0x62
#define QNOVO_PCUR1_MSB		0x63
#define QNOVO_PVOLT2_LSB	0x70
#define QNOVO_PVOLT2_MSB	0x71
#define QNOVO_RVOLT2_LSB	0x72
#define QNOVO_RVOLT2_MSB	0x73
#define QNOVO_PCUR2_LSB		0x74
#define QNOVO_PCUR2_MSB		0x75
#define QNOVO_SCNT		0x80
#define QNOVO_VMAX_LSB		0x90
#define QNOVO_VMAX_MSB		0x91
#define QNOVO_SNUM		0x92

/* Registers ending in 0 imply external rsense */
#define QNOVO_IADC_OFFSET_0	0xA0
#define QNOVO_IADC_OFFSET_1	0xA1
#define QNOVO_IADC_GAIN_0	0xA2
#define QNOVO_IADC_GAIN_1	0xA3
#define QNOVO_VADC_OFFSET	0xA4
#define QNOVO_VADC_GAIN		0xA5
#define QNOVO_IADC_GAIN_2	0xA6
#define QNOVO_SPARE		0xA7
#define QNOVO_STRM_CTRL		0xA8
#define QNOVO_IADC_OFFSET_OVR_VAL	0xA9
#define QNOVO_IADC_OFFSET_OVR		0xAA

#define QNOVO_DISABLE_CHARGING		0xAB
#define ERR_SWITCHER_DISABLED		BIT(7)
#define ERR_JEITA_SOFT_CONDITION	BIT(6)
#define ERR_BAT_OV			BIT(5)
#define ERR_CV_MODE			BIT(4)
#define ERR_BATTERY_MISSING		BIT(3)
#define ERR_SAFETY_TIMER_EXPIRED	BIT(2)
#define ERR_CHARGING_DISABLED		BIT(1)
#define ERR_JEITA_HARD_CONDITION	BIT(0)

#define QNOVO_TR_IADC_OFFSET_0	0xF1
#define QNOVO_TR_IADC_OFFSET_1	0xF2

#define DRV_MAJOR_VERSION	1
#define DRV_MINOR_VERSION	0

#define IADC_LSB_NA	2441400
#define VADC_LSB_NA	1220700
#define GAIN_LSB_FACTOR	976560

#define USER_VOTER		"user_voter"
#define SHUTDOWN_VOTER		"user_voter"
#define OK_TO_QNOVO_VOTER	"ok_to_qnovo_voter"

#define QNOVO_VOTER		"qnovo_voter"
#define FG_AVAILABLE_VOTER	"FG_AVAILABLE_VOTER"
#define QNOVO_OVERALL_VOTER	"QNOVO_OVERALL_VOTER"
#define QNI_PT_VOTER		"QNI_PT_VOTER"
#define ESR_VOTER		"ESR_VOTER"

#define HW_OK_TO_QNOVO_VOTER	"HW_OK_TO_QNOVO_VOTER"
#define CHG_READY_VOTER		"CHG_READY_VOTER"
#define USB_READY_VOTER		"USB_READY_VOTER"
#define DC_READY_VOTER		"DC_READY_VOTER"

#define PT_RESTART_VOTER	"PT_RESTART_VOTER"

#define CLASS_ATTR_IDX_RO(_name, _func)  \
static ssize_t _name##_show(struct class *c, struct class_attribute *attr, \
			char *ubuf) \
{ \
	return _func##_show(c, attr, ubuf); \
}; \
static CLASS_ATTR_RO(_name)

#define CLASS_ATTR_IDX_RW(_name, _func)  \
static ssize_t _name##_show(struct class *c, struct class_attribute *attr, \
			char *ubuf) \
{ \
	return _func##_show(c, attr, ubuf); \
}; \
static ssize_t _name##_store(struct class *c, struct class_attribute *attr, \
			const char *ubuf, size_t count) \
{ \
	return _func##_store(c, attr, ubuf, count); \
}; \
static CLASS_ATTR_RW(_name)

struct qnovo_dt_props {
	bool			external_rsense;
	struct device_node	*revid_dev_node;
	bool			enable_for_dc;
};

struct qnovo {
	int			base;
	struct mutex		write_lock;
	struct regmap		*regmap;
	struct qnovo_dt_props	dt;
	struct device		*dev;
	struct votable		*disable_votable;
	struct votable		*pt_dis_votable;
	struct votable		*not_ok_to_qnovo_votable;
	struct votable		*chg_ready_votable;
	struct votable		*awake_votable;
	struct class		qnovo_class;
	struct pmic_revid_data	*pmic_rev_id;
	u32			wa_flags;
	s64			external_offset_nA;
	s64			internal_offset_nA;
	s64			offset_nV;
	s64			external_i_gain_mega;
	s64			internal_i_gain_mega;
	s64			v_gain_mega;
	struct notifier_block	nb;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct work_struct	status_change_work;
	int			fv_uV_request;
	int			fcc_uA_request;
	int			usb_present;
	int			dc_present;
	struct delayed_work	usb_debounce_work;
	struct delayed_work	dc_debounce_work;

	struct delayed_work	ptrain_restart_work;
};

static int debug_mask;
module_param_named(debug_mask, debug_mask, int, 0600);

#define qnovo_dbg(chip, reason, fmt, ...)				\
	do {								\
		if (debug_mask & (reason))				\
			dev_info(chip->dev, fmt, ##__VA_ARGS__);	\
		else							\
			dev_dbg(chip->dev, fmt, ##__VA_ARGS__);		\
	} while (0)

static bool is_secure(struct qnovo *chip, int addr)
{
	/* assume everything above 0x40 is secure */
	return (bool)(addr >= 0x40);
}

static int qnovo_read(struct qnovo *chip, u16 addr, u8 *buf, int len)
{
	return regmap_bulk_read(chip->regmap, chip->base + addr, buf, len);
}

static int qnovo_masked_write(struct qnovo *chip, u16 addr, u8 mask, u8 val)
{
	int rc = 0;

	mutex_lock(&chip->write_lock);
	if (is_secure(chip, addr)) {
		rc = regmap_write(chip->regmap,
				((chip->base + addr) & ~(0xFF)) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_update_bits(chip->regmap, chip->base + addr, mask, val);

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static int qnovo_write(struct qnovo *chip, u16 addr, u8 *buf, int len)
{
	int i, rc = 0;
	bool is_start_secure, is_end_secure;

	is_start_secure = is_secure(chip, addr);
	is_end_secure = is_secure(chip, addr + len);

	if (!is_start_secure && !is_end_secure) {
		mutex_lock(&chip->write_lock);
		rc = regmap_bulk_write(chip->regmap, chip->base + addr,
					buf, len);
		goto unlock;
	}

	mutex_lock(&chip->write_lock);
	for (i = addr; i < addr + len; i++) {
		if (is_secure(chip, i)) {
			rc = regmap_write(chip->regmap,
				((chip->base + i) & ~(0xFF)) | 0xD0, 0xA5);
			if (rc < 0)
				goto unlock;
		}
		rc = regmap_write(chip->regmap, chip->base + i, buf[i - addr]);
		if (rc < 0)
			goto unlock;
	}

unlock:
	mutex_unlock(&chip->write_lock);
	return rc;
}

static bool is_batt_available(struct qnovo *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_fg_available(struct qnovo *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_usb_available(struct qnovo *chip)
{
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy)
		return false;

	return true;
}

static bool is_dc_available(struct qnovo *chip)
{
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");

	if (!chip->dc_psy)
		return false;

	return true;
}

static int qnovo_batt_psy_update(struct qnovo *chip, bool disable)
{
	union power_supply_propval pval = {0};
	int rc = 0;

	if (!is_batt_available(chip))
		return -EINVAL;

	if (chip->fv_uV_request != -EINVAL) {
		pval.intval = disable ? -EINVAL : chip->fv_uV_request;
		rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_QNOVO,
			&pval);
		if (rc < 0) {
			pr_err("Couldn't set prop qnovo_fv rc = %d\n", rc);
			return -EINVAL;
		}
	}

	if (chip->fcc_uA_request != -EINVAL) {
		pval.intval = disable ? -EINVAL : chip->fcc_uA_request;
		rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_QNOVO,
			&pval);
		if (rc < 0) {
			pr_err("Couldn't set prop qnovo_fcc rc = %d\n", rc);
			return -EINVAL;
		}
	}

	return rc;
}

static int qnovo_disable_cb(struct votable *votable, void *data, int disable,
					const char *client)
{
	struct qnovo *chip = data;
	union power_supply_propval pval = {0};
	int rc;

	if (!is_batt_available(chip))
		return -EINVAL;

	pval.intval = !disable;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE,
			&pval);
	if (rc < 0) {
		pr_err("Couldn't set prop qnovo_enable rc = %d\n", rc);
		return -EINVAL;
	}

	/*
	 * fg must be available for enable FG_AVAILABLE_VOTER
	 * won't enable it otherwise
	 */

	if (is_fg_available(chip))
		power_supply_set_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE,
				&pval);

	vote(chip->pt_dis_votable, QNOVO_OVERALL_VOTER, disable, 0);
	rc = qnovo_batt_psy_update(chip, disable);
	return rc;
}

static int pt_dis_votable_cb(struct votable *votable, void *data, int disable,
					const char *client)
{
	struct qnovo *chip = data;
	int rc;

	if (disable) {
		cancel_delayed_work_sync(&chip->ptrain_restart_work);
		vote(chip->awake_votable, PT_RESTART_VOTER, false, 0);
	}

	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT,
				 (bool)disable ? 0 : QNOVO_PTRAIN_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't %s pulse train rc=%d\n",
			(bool)disable ? "disable" : "enable", rc);
		return rc;
	}

	if (!disable) {
		vote(chip->awake_votable, PT_RESTART_VOTER, true, 0);
		schedule_delayed_work(&chip->ptrain_restart_work,
				msecs_to_jiffies(20));
	}

	return 0;
}

static int not_ok_to_qnovo_cb(struct votable *votable, void *data,
					int not_ok_to_qnovo,
					const char *client)
{
	struct qnovo *chip = data;

	vote(chip->disable_votable, OK_TO_QNOVO_VOTER, not_ok_to_qnovo, 0);
	if (not_ok_to_qnovo)
		vote(chip->disable_votable, USER_VOTER, true, 0);

	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return 0;
}

static int chg_ready_cb(struct votable *votable, void *data, int ready,
					const char *client)
{
	struct qnovo *chip = data;

	vote(chip->not_ok_to_qnovo_votable, CHG_READY_VOTER, !ready, 0);

	return 0;
}

static int awake_cb(struct votable *votable, void *data, int awake,
					const char *client)
{
	struct qnovo *chip = data;

	if (awake)
		pm_stay_awake(chip->dev);
	else
		pm_relax(chip->dev);

	return 0;
}

static int qnovo_parse_dt(struct qnovo *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "reg", &chip->base);
	if (rc < 0) {
		pr_err("Couldn't read base rc = %d\n", rc);
		return rc;
	}

	chip->dt.external_rsense = of_property_read_bool(node,
			"qcom,external-rsense");

	chip->dt.revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!chip->dt.revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}
	chip->dt.enable_for_dc = of_property_read_bool(node,
					"qcom,enable-for-dc");

	return 0;
}

enum {
	VER = 0,
	OK_TO_QNOVO,
	QNOVO_ENABLE,
	PT_ENABLE,
	FV_REQUEST,
	FCC_REQUEST,
	PE_CTRL_REG,
	PE_CTRL2_REG,
	PTRAIN_STS_REG,
	INT_RT_STS_REG,
	ERR_STS2_REG,
	PREST1,
	PPULS1,
	NREST1,
	NPULS1,
	PPCNT,
	VLIM1,
	PVOLT1,
	PCUR1,
	PTTIME,
	PREST2,
	PPULS2,
	NREST2,
	NPULS2,
	VLIM2,
	PVOLT2,
	RVOLT2,
	PCUR2,
	SCNT,
	VMAX,
	SNUM,
	VBATT,
	IBATT,
	BATTTEMP,
	BATTSOC,
};

struct param_info {
	char	*name;
	int	start_addr;
	int	num_regs;
	int	reg_to_unit_multiplier;
	int	reg_to_unit_divider;
	int	reg_to_unit_offset;
	int	min_val;
	int	max_val;
	char	*units_str;
};

static struct param_info params[] = {
	[PT_ENABLE] = {
		.name			= "PT_ENABLE",
		.start_addr		= QNOVO_PTRAIN_EN,
		.num_regs		= 1,
		.units_str		= "",
	},
	[FV_REQUEST] = {
		.units_str		= "uV",
	},
	[FCC_REQUEST] = {
		.units_str		= "uA",
	},
	[PE_CTRL_REG] = {
		.name			= "CTRL_REG",
		.start_addr		= QNOVO_PE_CTRL,
		.num_regs		= 1,
		.units_str		= "",
	},
	[PE_CTRL2_REG] = {
		.name			= "PE_CTRL2_REG",
		.start_addr		= QNOVO_PE_CTRL2,
		.num_regs		= 1,
		.units_str		= "",
	},
	[PTRAIN_STS_REG] = {
		.name			= "PTRAIN_STS",
		.start_addr		= QNOVO_PTRAIN_STS,
		.num_regs		= 1,
		.units_str		= "",
	},
	[INT_RT_STS_REG] = {
		.name			= "INT_RT_STS",
		.start_addr		= QNOVO_INT_RT_STS,
		.num_regs		= 1,
		.units_str		= "",
	},
	[ERR_STS2_REG] = {
		.name			= "RAW_CHGR_ERR",
		.start_addr		= QNOVO_ERROR_STS2,
		.num_regs		= 1,
		.units_str		= "",
	},
	[PREST1] = {
		.name			= "PREST1",
		.start_addr		= QNOVO_PREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 255,
		.units_str		= "mS",
	},
	[PPULS1] = {
		.name			= "PPULS1",
		.start_addr		= QNOVO_PPULS1_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1600, /* converts to uC */
		.reg_to_unit_divider	= 1,
		.min_val		= 30000,
		.max_val		= 65535000,
		.units_str		= "uC",
	},
	[NREST1] = {
		.name			= "NREST1",
		.start_addr		= QNOVO_NREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 255,
		.units_str		= "mS",
	},
	[NPULS1] = {
		.name			= "NPULS1",
		.start_addr		= QNOVO_NPULS1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 255,
		.units_str		= "mS",
	},
	[PPCNT] = {
		.name			= "PPCNT",
		.start_addr		= QNOVO_PPCNT_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.min_val		= 1,
		.max_val		= 255,
		.units_str		= "pulses",
	},
	[VLIM1] = {
		.name			= "VLIM1",
		.start_addr		= QNOVO_VLIM1_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 2200000,
		.max_val		= 4500000,
		.units_str		= "uV",
	},
	[PVOLT1] = {
		.name			= "PVOLT1",
		.start_addr		= QNOVO_PVOLT1_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[PCUR1] = {
		.name			= "PCUR1",
		.start_addr		= QNOVO_PCUR1_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1220700, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[PTTIME] = {
		.name			= "PTTIME",
		.start_addr		= QNOVO_PTTIME_STS,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 2,
		.reg_to_unit_divider	= 1,
		.units_str		= "S",
	},
	[PREST2] = {
		.name			= "PREST2",
		.start_addr		= QNOVO_PREST2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 65535,
		.units_str		= "mS",
	},
	[PPULS2] = {
		.name			= "PPULS2",
		.start_addr		= QNOVO_PPULS2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1600, /* converts to uC */
		.reg_to_unit_divider	= 1,
		.min_val		= 30000,
		.max_val		= 65535000,
		.units_str		= "uC",
	},
	[NREST2] = {
		.name			= "NREST2",
		.start_addr		= QNOVO_NREST2_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.reg_to_unit_offset	= -5,
		.min_val		= 5,
		.max_val		= 255,
		.units_str		= "mS",
	},
	[NPULS2] = {
		.name			= "NPULS2",
		.start_addr		= QNOVO_NPULS2_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 255,
		.units_str		= "mS",
	},
	[VLIM2] = {
		.name			= "VLIM2",
		.start_addr		= QNOVO_VLIM2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 2200000,
		.max_val		= 4500000,
		.units_str		= "uV",
	},
	[PVOLT2] = {
		.name			= "PVOLT2",
		.start_addr		= QNOVO_PVOLT2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[RVOLT2] = {
		.name			= "RVOLT2",
		.start_addr		= QNOVO_RVOLT2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350,
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[PCUR2] = {
		.name			= "PCUR2",
		.start_addr		= QNOVO_PCUR2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1220700, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[SCNT] = {
		.name			= "SCNT",
		.start_addr		= QNOVO_SCNT,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 255,
		.units_str		= "pulses",
	},
	[VMAX] = {
		.name			= "VMAX",
		.start_addr		= QNOVO_VMAX_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 814000, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[SNUM] = {
		.name			= "SNUM",
		.start_addr		= QNOVO_SNUM,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.units_str		= "pulses",
	},
	[VBATT]	= {
		.name			= "POWER_SUPPLY_PROP_VOLTAGE_NOW",
		.start_addr		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
		.units_str		= "uV",
	},
	[IBATT]	= {
		.name			= "POWER_SUPPLY_PROP_CURRENT_NOW",
		.start_addr		= POWER_SUPPLY_PROP_CURRENT_NOW,
		.units_str		= "uA",
	},
	[BATTTEMP] = {
		.name			= "POWER_SUPPLY_PROP_TEMP",
		.start_addr		= POWER_SUPPLY_PROP_TEMP,
		.units_str		= "uV",
	},
	[BATTSOC] = {
		.name			= "POWER_SUPPLY_PROP_CAPACITY",
		.start_addr		= POWER_SUPPLY_PROP_CAPACITY,
		.units_str		= "%",
	},
};

static struct attribute *qnovo_class_attrs[];

static ssize_t version_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d.%d\n",
			DRV_MAJOR_VERSION, DRV_MINOR_VERSION);
}
static CLASS_ATTR_RO(version);

static ssize_t ok_to_qnovo_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int val = get_effective_result(chip->not_ok_to_qnovo_votable);

	return snprintf(buf, PAGE_SIZE, "%d\n", !val);
}
static CLASS_ATTR_RO(ok_to_qnovo);

static ssize_t qnovo_enable_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int val = get_effective_result(chip->disable_votable);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", !val);
}

static ssize_t qnovo_enable_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	unsigned long val;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	vote(chip->disable_votable, USER_VOTER, !val, 0);

	return count;
}
static CLASS_ATTR_RW(qnovo_enable);

static ssize_t pt_enable_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int val = get_effective_result(chip->pt_dis_votable);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", !val);
}

static ssize_t pt_enable_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	unsigned long val;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	/* val being 0, userspace wishes to disable pt so vote true */
	vote(chip->pt_dis_votable, QNI_PT_VOTER, val ? false : true, 0);

	return count;
}
static CLASS_ATTR_RW(pt_enable);


static ssize_t val_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int i = &attr->attr - *qnovo_class_attrs;
	int val = 0;

	if (i == FV_REQUEST)
		val = chip->fv_uV_request;

	if (i == FCC_REQUEST)
		val = chip->fcc_uA_request;

	return snprintf(ubuf, PAGE_SIZE, "%d\n", val);
}

static ssize_t val_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int i = &attr->attr - *qnovo_class_attrs;
	unsigned long val;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	if (i == FV_REQUEST)
		chip->fv_uV_request = val;

	if (i == FCC_REQUEST)
		chip->fcc_uA_request = val;

	if (!get_effective_result(chip->disable_votable))
		qnovo_batt_psy_update(chip, false);

	return count;
}

static ssize_t reg_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	int rc;

	rc = qnovo_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	regval = buf[1] << 8 | buf[0];

	return snprintf(ubuf, PAGE_SIZE, "0x%04x\n", regval);
}

static ssize_t reg_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	unsigned long val;
	int rc;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	buf[0] = val & 0xFF;
	buf[1] = (val >> 8) & 0xFF;

	rc = qnovo_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	return count;
}

static ssize_t time_show(struct class *c, struct class_attribute *attr,
		char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	int val;
	int rc;

	rc = qnovo_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	regval = buf[1] << 8 | buf[0];

	val = ((regval * params[i].reg_to_unit_multiplier)
			/ params[i].reg_to_unit_divider)
		- params[i].reg_to_unit_offset;

	return snprintf(ubuf, PAGE_SIZE, "%d\n", val);
}

static ssize_t time_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	unsigned long val;
	int rc;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	if (val < params[i].min_val || val > params[i].max_val) {
		pr_err("Out of Range %d%s for %s\n", (int)val,
				params[i].units_str,
				params[i].name);
		return -ERANGE;
	}

	regval = (((int)val + params[i].reg_to_unit_offset)
			* params[i].reg_to_unit_divider)
		/ params[i].reg_to_unit_multiplier;
	buf[0] = regval & 0xFF;
	buf[1] = (regval >> 8) & 0xFF;

	rc = qnovo_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t current_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	int comp_val_uA;
	s64 regval_nA;
	s64 gain, offset_nA, comp_val_nA;

	rc = qnovo_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	if (buf[1] & BIT(5))
		buf[1] |= GENMASK(7, 6);

	regval_nA = (s16)(buf[1] << 8 | buf[0]);
	regval_nA = div_s64(regval_nA * params[i].reg_to_unit_multiplier,
					params[i].reg_to_unit_divider)
			- params[i].reg_to_unit_offset;

	if (chip->dt.external_rsense) {
		offset_nA = chip->external_offset_nA;
		gain = chip->external_i_gain_mega;
	} else {
		offset_nA = chip->internal_offset_nA;
		gain = chip->internal_i_gain_mega;
	}

	comp_val_nA = div_s64(regval_nA * gain, 1000000) - offset_nA;
	comp_val_uA = div_s64(comp_val_nA, 1000);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", comp_val_uA);
}

static ssize_t voltage_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	int comp_val_uV;
	s64 regval_nV;
	s64 gain, offset_nV, comp_val_nV;

	rc = qnovo_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	regval_nV = buf[1] << 8 | buf[0];
	regval_nV = div_s64(regval_nV * params[i].reg_to_unit_multiplier,
					params[i].reg_to_unit_divider)
			- params[i].reg_to_unit_offset;

	offset_nV = chip->offset_nV;
	gain = chip->v_gain_mega;

	comp_val_nV = div_s64(regval_nV * gain, 1000000) + offset_nV;
	comp_val_uV = div_s64(comp_val_nV, 1000);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", comp_val_uV);
}

static ssize_t voltage_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	unsigned long val_uV;
	s64 regval_nV;
	s64 gain, offset_nV;

	if (kstrtoul(ubuf, 0, &val_uV))
		return -EINVAL;

	if (val_uV < params[i].min_val || val_uV > params[i].max_val) {
		pr_err("Out of Range %d%s for %s\n", (int)val_uV,
				params[i].units_str,
				params[i].name);
		return -ERANGE;
	}

	offset_nV = chip->offset_nV;
	gain = chip->v_gain_mega;

	regval_nV = (s64)val_uV * 1000 - offset_nV;
	regval_nV = div_s64(regval_nV * 1000000, gain);

	regval_nV = div_s64((regval_nV + params[i].reg_to_unit_offset)
			* params[i].reg_to_unit_divider,
			params[i].reg_to_unit_multiplier);
	buf[0] = regval_nV & 0xFF;
	buf[1] = ((u64)regval_nV >> 8) & 0xFF;

	rc = qnovo_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t coulomb_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	int comp_val_uC;
	s64 regval_uC, gain;

	rc = qnovo_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	regval_uC = buf[1] << 8 | buf[0];
	regval_uC = div_s64(regval_uC * params[i].reg_to_unit_multiplier,
					params[i].reg_to_unit_divider)
			- params[i].reg_to_unit_offset;

	if (chip->dt.external_rsense)
		gain = chip->external_i_gain_mega;
	else
		gain = chip->internal_i_gain_mega;

	comp_val_uC = div_s64(regval_uC * gain, 1000000);
	return snprintf(ubuf, PAGE_SIZE, "%d\n", comp_val_uC);
}

static ssize_t coulomb_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	unsigned long val_uC;
	s64 regval;
	s64 gain;

	if (kstrtoul(ubuf, 0, &val_uC))
		return -EINVAL;

	if (val_uC < params[i].min_val || val_uC > params[i].max_val) {
		pr_err("Out of Range %d%s for %s\n", (int)val_uC,
				params[i].units_str,
				params[i].name);
		return -ERANGE;
	}

	if (chip->dt.external_rsense)
		gain = chip->external_i_gain_mega;
	else
		gain = chip->internal_i_gain_mega;

	regval = div_s64((s64)val_uC * 1000000, gain);

	regval = div_s64((regval + params[i].reg_to_unit_offset)
			* params[i].reg_to_unit_divider,
			params[i].reg_to_unit_multiplier);

	buf[0] = regval & 0xFF;
	buf[1] = ((u64)regval >> 8) & 0xFF;

	rc = qnovo_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t batt_prop_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	int i = &attr->attr - *qnovo_class_attrs;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int rc = -EINVAL;
	int prop = params[i].start_addr;
	union power_supply_propval pval = {0};

	if (!is_batt_available(chip))
		return -EINVAL;

	rc = power_supply_get_property(chip->batt_psy, prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read battery prop %s rc = %d\n",
				params[i].name, rc);
		return -EINVAL;
	}

	return snprintf(ubuf, PAGE_SIZE, "%d\n", pval.intval);
}

CLASS_ATTR_IDX_RW(fv_uV_request, val);
CLASS_ATTR_IDX_RW(fcc_uA_request, val);
CLASS_ATTR_IDX_RW(PE_CTRL_REG, reg);
CLASS_ATTR_IDX_RW(PE_CTRL2_REG, reg);
CLASS_ATTR_IDX_RO(PTRAIN_STS_REG, reg);
CLASS_ATTR_IDX_RO(INT_RT_STS_REG, reg);
CLASS_ATTR_IDX_RO(ERR_STS2_REG, reg);
CLASS_ATTR_IDX_RW(PREST1_mS, time);
CLASS_ATTR_IDX_RW(PPULS1_uC, coulomb);
CLASS_ATTR_IDX_RW(NREST1_mS, time);
CLASS_ATTR_IDX_RW(NPULS1_mS, time);
CLASS_ATTR_IDX_RW(PPCNT, time);
CLASS_ATTR_IDX_RW(VLIM1_uV, voltage);
CLASS_ATTR_IDX_RO(PVOLT1_uV, voltage);
CLASS_ATTR_IDX_RO(PCUR1_uA, current);
CLASS_ATTR_IDX_RO(PTTIME_S, time);
CLASS_ATTR_IDX_RW(PREST2_mS, time);
CLASS_ATTR_IDX_RW(PPULS2_uC, coulomb);
CLASS_ATTR_IDX_RW(NREST2_mS, time);
CLASS_ATTR_IDX_RW(NPULS2_mS, time);
CLASS_ATTR_IDX_RW(VLIM2_uV, voltage);
CLASS_ATTR_IDX_RO(PVOLT2_uV, voltage);
CLASS_ATTR_IDX_RO(RVOLT2_uV, voltage);
CLASS_ATTR_IDX_RO(PCUR2_uA, current);
CLASS_ATTR_IDX_RW(SCNT, time);
CLASS_ATTR_IDX_RO(VMAX_uV, voltage);
CLASS_ATTR_IDX_RO(SNUM, time);
CLASS_ATTR_IDX_RO(VBATT_uV, batt_prop);
CLASS_ATTR_IDX_RO(IBATT_uA, batt_prop);
CLASS_ATTR_IDX_RO(BATTTEMP_deciDegC, batt_prop);
CLASS_ATTR_IDX_RO(BATTSOC, batt_prop);

static struct attribute *qnovo_class_attrs[] = {
	[VER]			= &class_attr_version.attr,
	[OK_TO_QNOVO]		= &class_attr_ok_to_qnovo.attr,
	[QNOVO_ENABLE]		= &class_attr_qnovo_enable.attr,
	[PT_ENABLE]		= &class_attr_pt_enable.attr,
	[FV_REQUEST]		= &class_attr_fv_uV_request.attr,
	[FCC_REQUEST]		= &class_attr_fcc_uA_request.attr,
	[PE_CTRL_REG]		= &class_attr_PE_CTRL_REG.attr,
	[PE_CTRL2_REG]		= &class_attr_PE_CTRL2_REG.attr,
	[PTRAIN_STS_REG]	= &class_attr_PTRAIN_STS_REG.attr,
	[INT_RT_STS_REG]	= &class_attr_INT_RT_STS_REG.attr,
	[ERR_STS2_REG]		= &class_attr_ERR_STS2_REG.attr,
	[PREST1]		= &class_attr_PREST1_mS.attr,
	[PPULS1]		= &class_attr_PPULS1_uC.attr,
	[NREST1]		= &class_attr_NREST1_mS.attr,
	[NPULS1]		= &class_attr_NPULS1_mS.attr,
	[PPCNT]			= &class_attr_PPCNT.attr,
	[VLIM1]			= &class_attr_VLIM1_uV.attr,
	[PVOLT1]		= &class_attr_PVOLT1_uV.attr,
	[PCUR1]			= &class_attr_PCUR1_uA.attr,
	[PTTIME]		= &class_attr_PTTIME_S.attr,
	[PREST2]		= &class_attr_PREST2_mS.attr,
	[PPULS2]		= &class_attr_PPULS2_uC.attr,
	[NREST2]		= &class_attr_NREST2_mS.attr,
	[NPULS2]		= &class_attr_NPULS2_mS.attr,
	[VLIM2]			= &class_attr_VLIM2_uV.attr,
	[PVOLT2]		= &class_attr_PVOLT2_uV.attr,
	[RVOLT2]		= &class_attr_RVOLT2_uV.attr,
	[PCUR2]			= &class_attr_PCUR2_uA.attr,
	[SCNT]			= &class_attr_SCNT.attr,
	[VMAX]			= &class_attr_VMAX_uV.attr,
	[SNUM]			= &class_attr_SNUM.attr,
	[VBATT]			= &class_attr_VBATT_uV.attr,
	[IBATT]			= &class_attr_IBATT_uA.attr,
	[BATTTEMP]		= &class_attr_BATTTEMP_deciDegC.attr,
	[BATTSOC]		= &class_attr_BATTSOC.attr,
	NULL,
};
ATTRIBUTE_GROUPS(qnovo_class);

static int qnovo_update_status(struct qnovo *chip)
{
	u8 val = 0;
	int rc;
	bool hw_ok_to_qnovo;

	rc = qnovo_read(chip, QNOVO_ERROR_STS2, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't read error sts rc = %d\n", rc);
		hw_ok_to_qnovo = false;
	} else {
		/*
		 * For CV mode keep qnovo enabled, userspace is expected to
		 * disable it after few runs
		 */
		hw_ok_to_qnovo = (val == ERR_CV_MODE || val == 0) ?
			true : false;
	}

	vote(chip->not_ok_to_qnovo_votable, HW_OK_TO_QNOVO_VOTER,
					!hw_ok_to_qnovo, 0);
	return 0;
}

static void usb_debounce_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
				struct qnovo, usb_debounce_work.work);

	vote(chip->chg_ready_votable, USB_READY_VOTER, true, 0);
	vote(chip->awake_votable, USB_READY_VOTER, false, 0);
}

static void dc_debounce_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
				struct qnovo, dc_debounce_work.work);

	vote(chip->chg_ready_votable, DC_READY_VOTER, true, 0);
	vote(chip->awake_votable, DC_READY_VOTER, false, 0);
}

#define DEBOUNCE_MS 15000  /* 15 seconds */
static void status_change_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
			struct qnovo, status_change_work);
	union power_supply_propval pval;
	bool usb_present = false, dc_present = false;
	int rc;

	if (is_fg_available(chip))
		vote(chip->disable_votable, FG_AVAILABLE_VOTER, false, 0);

	if (is_usb_available(chip)) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		usb_present = (rc < 0) ? 0 : pval.intval;
	}

	if (chip->usb_present && !usb_present) {
		/* removal */
		chip->usb_present = 0;
		cancel_delayed_work_sync(&chip->usb_debounce_work);
		vote(chip->awake_votable, USB_READY_VOTER, false, 0);
		vote(chip->chg_ready_votable, USB_READY_VOTER, false, 0);
	} else if (!chip->usb_present && usb_present) {
		/* insertion */
		chip->usb_present = 1;
		vote(chip->awake_votable, USB_READY_VOTER, true, 0);
		schedule_delayed_work(&chip->usb_debounce_work,
				msecs_to_jiffies(DEBOUNCE_MS));
	}

	if (is_dc_available(chip)) {
		rc = power_supply_get_property(chip->dc_psy,
			POWER_SUPPLY_PROP_PRESENT,
			&pval);
		dc_present = (rc < 0) ? 0 : pval.intval;
	}

	if (usb_present)
		dc_present = 0;

	/* disable qnovo for dc path by forcing dc_present = 0 always */
	if (!chip->dt.enable_for_dc)
		dc_present = 0;

	if (chip->dc_present && !dc_present) {
		/* removal */
		chip->dc_present = 0;
		cancel_delayed_work_sync(&chip->dc_debounce_work);
		vote(chip->awake_votable, DC_READY_VOTER, false, 0);
		vote(chip->chg_ready_votable, DC_READY_VOTER, false, 0);
	} else if (!chip->dc_present && dc_present) {
		/* insertion */
		chip->dc_present = 1;
		vote(chip->awake_votable, DC_READY_VOTER, true, 0);
		schedule_delayed_work(&chip->dc_debounce_work,
				msecs_to_jiffies(DEBOUNCE_MS));
	}

	qnovo_update_status(chip);
}

static void ptrain_restart_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
				struct qnovo, ptrain_restart_work.work);
	u8 pt_t1, pt_t2;
	int rc;
	u8 pt_en;

	rc = qnovo_read(chip, QNOVO_PTRAIN_EN, &pt_en, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTRAIN_EN rc = %d\n",
				rc);
		goto clean_up;
	}

	if (!pt_en) {
		rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN,
				QNOVO_PTRAIN_EN_BIT, QNOVO_PTRAIN_EN_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't enable pulse train rc=%d\n",
					rc);
			goto clean_up;
		}
		/* sleep 20ms for the pulse trains to restart and settle */
		msleep(20);
	}

	rc = qnovo_read(chip, QNOVO_PTTIME_STS, &pt_t1, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTTIME_STS rc = %d\n",
			rc);
		goto clean_up;
	}

	/* pttime increments every 2 seconds */
	msleep(2100);

	rc = qnovo_read(chip, QNOVO_PTTIME_STS, &pt_t2, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read QNOVO_PTTIME_STS rc = %d\n",
			rc);
		goto clean_up;
	}

	if (pt_t1 != pt_t2)
		goto clean_up;

	/* Toggle pt enable to restart pulse train */
	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable pulse train rc=%d\n", rc);
		goto clean_up;
	}
	msleep(1000);
	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT,
				QNOVO_PTRAIN_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable pulse train rc=%d\n", rc);
		goto clean_up;
	}

clean_up:
	vote(chip->awake_votable, PT_RESTART_VOTER, false, 0);
}

static int qnovo_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct qnovo *chip = container_of(nb, struct qnovo, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0
		|| strcmp(psy->desc->name, "bms") == 0
		|| strcmp(psy->desc->name, "usb") == 0
		|| strcmp(psy->desc->name, "dc") == 0)
		schedule_work(&chip->status_change_work);

	return NOTIFY_OK;
}

static irqreturn_t handle_ptrain_done(int irq, void *data)
{
	struct qnovo *chip = data;
	union power_supply_propval pval = {0};

	qnovo_update_status(chip);

	/*
	 * hw resets pt_en bit once ptrain_done triggers.
	 * vote on behalf of QNI to disable it such that
	 * once QNI enables it, the votable state changes
	 * and the callback that sets it is indeed invoked
	 */
	vote(chip->pt_dis_votable, QNI_PT_VOTER, true, 0);

	vote(chip->pt_dis_votable, ESR_VOTER, true, 0);
	if (is_fg_available(chip)
		&& !get_client_vote(chip->disable_votable, USER_VOTER)
		&& !get_effective_result(chip->not_ok_to_qnovo_votable))
		power_supply_set_property(chip->bms_psy,
				POWER_SUPPLY_PROP_RESISTANCE,
				&pval);

	vote(chip->pt_dis_votable, ESR_VOTER, false, 0);
	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return IRQ_HANDLED;
}

static int qnovo_hw_init(struct qnovo *chip)
{
	int rc;
	u8 iadc_offset_external, iadc_offset_internal;
	u8 iadc_gain_external, iadc_gain_internal;
	u8 vadc_offset, vadc_gain;
	u8 val;

	vote(chip->chg_ready_votable, USB_READY_VOTER, false, 0);
	vote(chip->chg_ready_votable, DC_READY_VOTER, false, 0);

	vote(chip->disable_votable, USER_VOTER, true, 0);
	vote(chip->disable_votable, FG_AVAILABLE_VOTER, true, 0);

	vote(chip->pt_dis_votable, QNI_PT_VOTER, true, 0);
	vote(chip->pt_dis_votable, QNOVO_OVERALL_VOTER, true, 0);
	vote(chip->pt_dis_votable, ESR_VOTER, false, 0);

	val = 0;
	rc = qnovo_write(chip, QNOVO_STRM_CTRL, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't write iadc bitstream control rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_IADC_OFFSET_0, &iadc_offset_external, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc exernal offset rc = %d\n", rc);
		return rc;
	}

	/* stored as an 8 bit 2's complement signed integer */
	val = -1 * iadc_offset_external;
	rc = qnovo_write(chip, QNOVO_TR_IADC_OFFSET_0, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't write iadc offset rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_IADC_OFFSET_1, &iadc_offset_internal, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc internal offset rc = %d\n", rc);
		return rc;
	}

	/* stored as an 8 bit 2's complement signed integer */
	val = -1 * iadc_offset_internal;
	rc = qnovo_write(chip, QNOVO_TR_IADC_OFFSET_1, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't write iadc offset rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_IADC_GAIN_0, &iadc_gain_external, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc external gain rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_IADC_GAIN_1, &iadc_gain_internal, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc internal gain rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_VADC_OFFSET, &vadc_offset, 1);
	if (rc < 0) {
		pr_err("Couldn't read vadc offset rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_VADC_GAIN, &vadc_gain, 1);
	if (rc < 0) {
		pr_err("Couldn't read vadc external gain rc = %d\n", rc);
		return rc;
	}

	chip->external_offset_nA = (s64)(s8)iadc_offset_external * IADC_LSB_NA;
	chip->internal_offset_nA = (s64)(s8)iadc_offset_internal * IADC_LSB_NA;
	chip->offset_nV = (s64)(s8)vadc_offset * VADC_LSB_NA;
	chip->external_i_gain_mega
		= 1000000000 + (s64)(s8)iadc_gain_external * GAIN_LSB_FACTOR;
	chip->external_i_gain_mega
		= div_s64(chip->external_i_gain_mega, 1000);
	chip->internal_i_gain_mega
		= 1000000000 + (s64)(s8)iadc_gain_internal * GAIN_LSB_FACTOR;
	chip->internal_i_gain_mega
		= div_s64(chip->internal_i_gain_mega, 1000);
	chip->v_gain_mega = 1000000000 + (s64)(s8)vadc_gain * GAIN_LSB_FACTOR;
	chip->v_gain_mega = div_s64(chip->v_gain_mega, 1000);

	/* allow charger error conditions to disable qnovo, CV mode excluded */
	val = ERR_SWITCHER_DISABLED | ERR_JEITA_SOFT_CONDITION | ERR_BAT_OV |
		ERR_BATTERY_MISSING | ERR_SAFETY_TIMER_EXPIRED |
		ERR_CHARGING_DISABLED | ERR_JEITA_HARD_CONDITION;
	rc = qnovo_write(chip, QNOVO_DISABLE_CHARGING, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't write QNOVO_DISABLE_CHARGING rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int qnovo_register_notifier(struct qnovo *chip)
{
	int rc;

	chip->nb.notifier_call = qnovo_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int qnovo_determine_initial_status(struct qnovo *chip)
{
	status_change_work(&chip->status_change_work);
	return 0;
}

static int qnovo_request_interrupts(struct qnovo *chip)
{
	int rc = 0;
	int irq_ptrain_done = of_irq_get_byname(chip->dev->of_node,
						"ptrain-done");

	rc = devm_request_threaded_irq(chip->dev, irq_ptrain_done, NULL,
					handle_ptrain_done,
					IRQF_ONESHOT, "ptrain-done", chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc = %d\n",
					irq_ptrain_done, rc);
		return rc;
	}

	enable_irq_wake(irq_ptrain_done);

	return rc;
}

static int qnovo_probe(struct platform_device *pdev)
{
	struct qnovo *chip;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->fv_uV_request = -EINVAL;
	chip->fcc_uA_request = -EINVAL;
	chip->dev = &pdev->dev;
	mutex_init(&chip->write_lock);

	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	rc = qnovo_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, chip);

	chip->disable_votable = create_votable("QNOVO_DISABLE", VOTE_SET_ANY,
					qnovo_disable_cb, chip);
	if (IS_ERR(chip->disable_votable)) {
		rc = PTR_ERR(chip->disable_votable);
		goto cleanup;
	}

	chip->pt_dis_votable = create_votable("QNOVO_PT_DIS", VOTE_SET_ANY,
					pt_dis_votable_cb, chip);
	if (IS_ERR(chip->pt_dis_votable)) {
		rc = PTR_ERR(chip->pt_dis_votable);
		goto destroy_disable_votable;
	}

	chip->not_ok_to_qnovo_votable = create_votable("QNOVO_NOT_OK",
					VOTE_SET_ANY,
					not_ok_to_qnovo_cb, chip);
	if (IS_ERR(chip->not_ok_to_qnovo_votable)) {
		rc = PTR_ERR(chip->not_ok_to_qnovo_votable);
		goto destroy_pt_dis_votable;
	}

	chip->chg_ready_votable = create_votable("QNOVO_CHG_READY",
					VOTE_SET_ANY,
					chg_ready_cb, chip);
	if (IS_ERR(chip->chg_ready_votable)) {
		rc = PTR_ERR(chip->chg_ready_votable);
		goto destroy_not_ok_to_qnovo_votable;
	}

	chip->awake_votable = create_votable("QNOVO_AWAKE", VOTE_SET_ANY,
					awake_cb, chip);
	if (IS_ERR(chip->awake_votable)) {
		rc = PTR_ERR(chip->awake_votable);
		goto destroy_chg_ready_votable;
	}

	INIT_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->dc_debounce_work, dc_debounce_work);
	INIT_DELAYED_WORK(&chip->usb_debounce_work, usb_debounce_work);
	INIT_DELAYED_WORK(&chip->ptrain_restart_work, ptrain_restart_work);

	rc = qnovo_hw_init(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto destroy_awake_votable;
	}

	rc = qnovo_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto unreg_notifier;
	}

	rc = qnovo_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n", rc);
		goto unreg_notifier;
	}

	rc = qnovo_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto unreg_notifier;
	}
	chip->qnovo_class.name = "qnovo",
	chip->qnovo_class.owner = THIS_MODULE,
	chip->qnovo_class.class_groups = qnovo_class_groups;

	rc = class_register(&chip->qnovo_class);
	if (rc < 0) {
		pr_err("couldn't register qnovo sysfs class rc = %d\n", rc);
		goto unreg_notifier;
	}

	device_init_wakeup(chip->dev, true);

	return rc;

unreg_notifier:
	power_supply_unreg_notifier(&chip->nb);
destroy_awake_votable:
	destroy_votable(chip->awake_votable);
destroy_chg_ready_votable:
	destroy_votable(chip->chg_ready_votable);
destroy_not_ok_to_qnovo_votable:
	destroy_votable(chip->not_ok_to_qnovo_votable);
destroy_pt_dis_votable:
	destroy_votable(chip->pt_dis_votable);
destroy_disable_votable:
	destroy_votable(chip->disable_votable);
cleanup:
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int qnovo_remove(struct platform_device *pdev)
{
	struct qnovo *chip = platform_get_drvdata(pdev);

	class_unregister(&chip->qnovo_class);
	power_supply_unreg_notifier(&chip->nb);
	destroy_votable(chip->chg_ready_votable);
	destroy_votable(chip->not_ok_to_qnovo_votable);
	destroy_votable(chip->pt_dis_votable);
	destroy_votable(chip->disable_votable);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void qnovo_shutdown(struct platform_device *pdev)
{
	struct qnovo *chip = platform_get_drvdata(pdev);

	vote(chip->not_ok_to_qnovo_votable, SHUTDOWN_VOTER, true, 0);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-qnovo", },
	{ },
};

static struct platform_driver qnovo_driver = {
	.driver		= {
		.name		= "qcom,qnovo-driver",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= qnovo_probe,
	.remove		= qnovo_remove,
	.shutdown	= qnovo_shutdown,
};
module_platform_driver(qnovo_driver);

MODULE_DESCRIPTION("QPNP Qnovo Driver");
MODULE_LICENSE("GPL v2");
