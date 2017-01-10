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
#include "pmic-voter.h"

#define QNOVO_REVISION1		0x00
#define QNOVO_REVISION2		0x01
#define QNOVO_PERPH_TYPE	0x04
#define QNOVO_PERPH_SUBTYPE	0x05
#define QNOVO_PTTIME_STS	0x07
#define QNOVO_PTRAIN_STS	0x08
#define QNOVO_ERROR_STS		0x09
#define QNOVO_ERROR_BIT		BIT(0)
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

#define QNOVO_TR_IADC_OFFSET_0	0xF1
#define QNOVO_TR_IADC_OFFSET_1	0xF2

#define DRV_MAJOR_VERSION	1
#define DRV_MINOR_VERSION	0

#define IADC_LSB_NA	2441400
#define VADC_LSB_NA	1220700
#define GAIN_LSB_FACTOR	976560

#define USER_VOTER		"user_voter"
#define OK_TO_QNOVO_VOTER	"ok_to_qnovo_voter"

#define QNOVO_VOTER		"qnovo_voter"

struct qnovo_dt_props {
	bool			external_rsense;
	struct device_node	*revid_dev_node;
};

enum {
	QNOVO_ERASE_OFFSET_WA_BIT	= BIT(0),
	QNOVO_NO_ERR_STS_BIT		= BIT(1),
};

struct chg_props {
	bool		charging;
	bool		usb_online;
	bool		dc_online;
};

struct chg_status {
	bool		ok_to_qnovo;
};

struct qnovo {
	int			base;
	struct mutex		write_lock;
	struct regmap		*regmap;
	struct qnovo_dt_props	dt;
	struct device		*dev;
	struct votable		*disable_votable;
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
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct chg_props	cp;
	struct chg_status	cs;
	struct work_struct	status_change_work;
	int			fv_uV_request;
	int			fcc_uA_request;
	struct votable		*fcc_max_votable;
	struct votable		*fv_votable;
};

static int debug_mask;
module_param_named(debug_mask, debug_mask, int, S_IRUSR | S_IWUSR);

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

static int qnovo_disable_cb(struct votable *votable, void *data, int disable,
					const char *client)
{
	struct qnovo *chip = data;
	int rc = 0;

	if (disable) {
		if (chip->fv_uV_request != -EINVAL) {
			if (chip->fv_votable)
				vote(chip->fv_votable, QNOVO_VOTER, false, 0);
		}
		if (chip->fcc_uA_request != -EINVAL) {
			if (chip->fcc_max_votable)
				vote(chip->fcc_max_votable, QNOVO_VOTER,
						false, 0);
		}
	}

	rc = qnovo_masked_write(chip, QNOVO_PTRAIN_EN, QNOVO_PTRAIN_EN_BIT,
				 disable ? 0 : QNOVO_PTRAIN_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't %s pulse train rc=%d\n",
			disable ? "disable" : "enable", rc);
		return rc;
	}

	if (!disable) {
		if (chip->fv_uV_request != -EINVAL) {
			if (!chip->fv_votable)
				chip->fv_votable = find_votable("FV");
			if (chip->fv_votable)
				vote(chip->fv_votable, QNOVO_VOTER,
						true, chip->fv_uV_request);
		}
		if (chip->fcc_uA_request != -EINVAL) {
			if (!chip->fcc_max_votable)
				chip->fcc_max_votable = find_votable("FCC_MAX");
			if (chip->fcc_max_votable)
				vote(chip->fcc_max_votable, QNOVO_VOTER,
						true, chip->fcc_uA_request);
		}
	}

	return rc;
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

	return 0;
}

static int qnovo_check_chg_version(struct qnovo *chip)
{
	int rc;

	chip->pmic_rev_id = get_revid_data(chip->dt.revid_dev_node);
	if (IS_ERR(chip->pmic_rev_id)) {
		rc = PTR_ERR(chip->pmic_rev_id);
		if (rc != -EPROBE_DEFER)
			pr_err("Unable to get pmic_revid rc=%d\n", rc);
		return rc;
	}

	if ((chip->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE)
		   && (chip->pmic_rev_id->rev4 < PMI8998_V2P0_REV4)) {
		chip->wa_flags |= QNOVO_ERASE_OFFSET_WA_BIT;
		chip->wa_flags |= QNOVO_NO_ERR_STS_BIT;
	}

	return 0;
}

enum {
	VER = 0,
	OK_TO_QNOVO,
	ENABLE,
	FV_REQUEST,
	FCC_REQUEST,
	PE_CTRL_REG,
	PE_CTRL2_REG,
	PTRAIN_STS_REG,
	INT_RT_STS_REG,
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
	[PREST1] = {
		.name			= "PREST1",
		.start_addr		= QNOVO_PREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 1275,
		.units_str		= "mS",
	},
	[PPULS1] = {
		.name			= "PPULS1",
		.start_addr		= QNOVO_PPULS1_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1600, /* converts to uC */
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 104856000,
		.units_str		= "uC",
	},
	[NREST1] = {
		.name			= "NREST1",
		.start_addr		= QNOVO_NREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 1275,
		.units_str		= "mS",
	},
	[NPULS1] = {
		.name			= "NPULS1",
		.start_addr		= QNOVO_NPULS1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 1275,
		.units_str		= "mS",
	},
	[PPCNT] = {
		.name			= "PPCNT",
		.start_addr		= QNOVO_PPCNT_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.units_str		= "pulses",
	},
	[VLIM1] = {
		.name			= "VLIM1",
		.start_addr		= QNOVO_VLIM1_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 5000000,
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
		.min_val		= 5,
		.max_val		= 1275,
		.units_str		= "S",
	},
	[PREST2] = {
		.name			= "PREST2",
		.start_addr		= QNOVO_PREST2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 327675,
		.units_str		= "mS",
	},
	[PPULS2] = {
		.name			= "PPULS2",
		.start_addr		= QNOVO_PPULS2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1600, /* converts to uC */
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 104856000,
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
		.max_val		= 1280,
		.units_str		= "mS",
	},
	[NPULS2] = {
		.name			= "NPULS2",
		.start_addr		= QNOVO_NPULS2_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 5,
		.reg_to_unit_divider	= 1,
		.min_val		= 5,
		.max_val		= 1275,
		.units_str		= "mS",
	},
	[VLIM2] = {
		.name			= "VLIM1",
		.start_addr		= QNOVO_VLIM2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 610350, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 0,
		.max_val		= 5000000,
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

static struct class_attribute qnovo_attributes[];

static ssize_t version_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d.%d\n",
			DRV_MAJOR_VERSION, DRV_MINOR_VERSION);
}

static ssize_t ok_to_qnovo_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->cs.ok_to_qnovo);
}

static ssize_t enable_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int val;

	val = get_client_vote(chip->disable_votable, USER_VOTER);
	return snprintf(ubuf, PAGE_SIZE, "%d\n", val);
}

static ssize_t enable_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	unsigned long val;
	bool disable;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	disable = !val;

	vote(chip->disable_votable, USER_VOTER, disable, 0);
	return count;
}

static ssize_t val_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int i = attr - qnovo_attributes;
	int val = 0;

	if (i == FV_REQUEST)
		val = chip->fv_uV_request;

	if (i == FCC_REQUEST)
		val = chip->fcc_uA_request;

	return snprintf(ubuf, PAGE_SIZE, "%d%s\n", val, params[i].units_str);
}

static ssize_t val_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int i = attr - qnovo_attributes;
	unsigned long val;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	if (i == FV_REQUEST)
		chip->fv_uV_request = val;

	if (i == FCC_REQUEST)
		chip->fcc_uA_request = val;

	return count;
}

static ssize_t reg_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	int i = attr - qnovo_attributes;
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

	return snprintf(ubuf, PAGE_SIZE, "0x%04x%s\n",
			regval, params[i].units_str);
}

static ssize_t reg_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	int i = attr - qnovo_attributes;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	unsigned long val;
	int rc;

	if (kstrtoul(ubuf, 16, &val))
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
	int i = attr - qnovo_attributes;
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

	return snprintf(ubuf, PAGE_SIZE, "%d%s\n", val, params[i].units_str);
}

static ssize_t time_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = attr - qnovo_attributes;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	unsigned long val;
	int rc;

	if (kstrtoul(ubuf, 10, &val))
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
	int i = attr - qnovo_attributes;
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
	regval_nA = buf[1] << 8 | buf[0];
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

	comp_val_nA = div_s64(regval_nA * gain, 1000000) + offset_nA;
	comp_val_uA = comp_val_nA / 1000;

	return snprintf(ubuf, PAGE_SIZE, "%d%s\n",
			comp_val_uA, params[i].units_str);
}

static ssize_t voltage_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	int i = attr - qnovo_attributes;
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
	comp_val_uV = comp_val_nV / 1000;

	return snprintf(ubuf, PAGE_SIZE, "%d%s\n",
				comp_val_uV, params[i].units_str);
}

static ssize_t voltage_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = attr - qnovo_attributes;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	unsigned long val_uV;
	s64 regval_nV;
	s64 gain, offset_nV;

	if (kstrtoul(ubuf, 10, &val_uV))
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
	int i = attr - qnovo_attributes;
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
	return snprintf(ubuf, PAGE_SIZE, "%d%s\n",
			comp_val_uC, params[i].units_str);
}

static ssize_t coulomb_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	int i = attr - qnovo_attributes;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc;
	unsigned long val_uC;
	s64 regval;
	s64 gain;

	if (kstrtoul(ubuf, 10, &val_uC))
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
	int i = attr - qnovo_attributes;
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	int rc = -EINVAL;
	int prop = params[i].start_addr;
	union power_supply_propval pval = {0};

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return -EINVAL;

	rc = power_supply_get_property(chip->batt_psy, prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read battery prop %s rc = %d\n",
				params[i].name, rc);
		return -EINVAL;
	}

	return snprintf(ubuf, PAGE_SIZE, "%d%s\n",
			pval.intval, params[i].units_str);
}

static struct class_attribute qnovo_attributes[] = {
	[VER]			= __ATTR_RO(version),
	[OK_TO_QNOVO]		= __ATTR_RO(ok_to_qnovo),
	[ENABLE]		= __ATTR(enable, S_IRUGO | S_IWUSR,
					enable_show, enable_store),
	[FV_REQUEST]		= __ATTR(fv_uV_request, S_IRUGO | S_IWUSR,
					val_show, val_store),
	[FCC_REQUEST]		= __ATTR(fcc_uA_request, S_IRUGO | S_IWUSR,
					val_show, val_store),
	[PE_CTRL_REG]		= __ATTR(PE_CTRL_REG, S_IRUGO | S_IWUSR,
					reg_show, reg_store),
	[PE_CTRL2_REG]		= __ATTR(PE_CTRL2_REG, S_IRUGO | S_IWUSR,
					reg_show, reg_store),
	[PTRAIN_STS_REG]	= __ATTR(PTRAIN_STS_REG, S_IRUGO | S_IWUSR,
					reg_show, reg_store),
	[INT_RT_STS_REG]	= __ATTR(INT_RT_STS_REG, S_IRUGO | S_IWUSR,
					reg_show, reg_store),
	[PREST1]		= __ATTR(PREST1_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[PPULS1]		= __ATTR(PPULS1_uC, S_IRUGO | S_IWUSR,
					coulomb_show, coulomb_store),
	[NREST1]		= __ATTR(NREST1_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[NPULS1]		= __ATTR(NPULS1_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[PPCNT]			= __ATTR(PPCNT, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[VLIM1]			= __ATTR(VLIM1_uV, S_IRUGO | S_IWUSR,
					voltage_show, voltage_store),
	[PVOLT1]		= __ATTR(PVOLT1_uV, S_IRUGO,
					voltage_show, NULL),
	[PCUR1]			= __ATTR(PCUR1_uA, S_IRUGO,
					current_show, NULL),
	[PTTIME]		= __ATTR(PTTIME_S, S_IRUGO,
					time_show, NULL),
	[PREST2]		= __ATTR(PREST2_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[PPULS2]		= __ATTR(PPULS2_mS, S_IRUGO | S_IWUSR,
					coulomb_show, coulomb_store),
	[NREST2]		= __ATTR(NREST2_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[NPULS2]		= __ATTR(NPULS2_mS, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[VLIM2]			= __ATTR(VLIM2_uV, S_IRUGO | S_IWUSR,
					voltage_show, voltage_store),
	[PVOLT2]		= __ATTR(PVOLT2_uV, S_IRUGO,
					voltage_show, NULL),
	[RVOLT2]		= __ATTR(RVOLT2_uV, S_IRUGO,
					voltage_show, NULL),
	[PCUR2]			= __ATTR(PCUR2_uA, S_IRUGO,
					current_show, NULL),
	[SCNT]			= __ATTR(SCNT, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[VMAX]			= __ATTR(VMAX_uV, S_IRUGO,
					voltage_show, NULL),
	[SNUM]			= __ATTR(SNUM, S_IRUGO | S_IWUSR,
					time_show, time_store),
	[VBATT]			= __ATTR(VBATT_uV, S_IRUGO,
					batt_prop_show, NULL),
	[IBATT]			= __ATTR(IBATT_uA, S_IRUGO,
					batt_prop_show, NULL),
	[BATTTEMP]		= __ATTR(BATTTEMP_deciDegC, S_IRUGO,
					batt_prop_show, NULL),
	[BATTSOC]		= __ATTR(BATTSOC, S_IRUGO,
					batt_prop_show, NULL),
	__ATTR_NULL,
};

static void get_chg_props(struct qnovo *chip, struct chg_props *cp)
{
	union power_supply_propval pval;
	u8 val = 0;
	int rc;

	cp->charging = true;
	rc = qnovo_read(chip, QNOVO_ERROR_STS, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't read error sts rc = %d\n", rc);
		cp->charging = false;
	} else {
		cp->charging = (!(val & QNOVO_ERROR_BIT));
	}

	if (chip->wa_flags & QNOVO_NO_ERR_STS_BIT) {
		/*
		 * on v1.0 and v1.1 pmic's force charging to true
		 * if things are not good to charge s/w gets a PTRAIN_DONE
		 * interrupt
		 */
		cp->charging = true;
	}

	cp->usb_online = false;
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0)
			pr_err("Couldn't read usb online rc = %d\n", rc);
		else
			cp->usb_online = (bool)pval.intval;
	}

	cp->dc_online = false;
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");
	if (chip->dc_psy) {
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0)
			pr_err("Couldn't read dc online rc = %d\n", rc);
		else
			cp->dc_online = (bool)pval.intval;
	}
}

static void get_chg_status(struct qnovo *chip, const struct chg_props *cp,
				struct chg_status *cs)
{
	cs->ok_to_qnovo = false;

	if (cp->charging &&
		(cp->usb_online || cp->dc_online))
		cs->ok_to_qnovo = true;
}

static void status_change_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
			struct qnovo, status_change_work);
	bool notify_uevent = false;
	struct chg_props cp;
	struct chg_status cs;

	get_chg_props(chip, &cp);
	get_chg_status(chip, &cp, &cs);

	if (cs.ok_to_qnovo ^ chip->cs.ok_to_qnovo) {
		/*
		 * when it is not okay to Qnovo charge, disable both voters,
		 * so that when it becomes okay to Qnovo charge the user voter
		 * has to specifically enable its vote to being Qnovo charging
		 */
		if (!cs.ok_to_qnovo) {
			vote(chip->disable_votable, OK_TO_QNOVO_VOTER, 1, 0);
			vote(chip->disable_votable, USER_VOTER, 1, 0);
		} else {
			vote(chip->disable_votable, OK_TO_QNOVO_VOTER, 0, 0);
		}
		notify_uevent = true;
	}

	memcpy(&chip->cp, &cp, sizeof(struct chg_props));
	memcpy(&chip->cs, &cs, sizeof(struct chg_status));

	if (notify_uevent)
		kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
}

static int qnovo_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct qnovo *chip = container_of(nb, struct qnovo, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;
	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0))
		schedule_work(&chip->status_change_work);

	return NOTIFY_OK;
}

static irqreturn_t handle_ptrain_done(int irq, void *data)
{
	struct qnovo *chip = data;

	/* disable user voter here */
	vote(chip->disable_votable, USER_VOTER, 0, 0);
	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return IRQ_HANDLED;
}

static int qnovo_hw_init(struct qnovo *chip)
{
	int rc;
	u8 iadc_offset_external, iadc_offset_internal;
	u8 iadc_gain_external, iadc_gain_internal;
	u8 vadc_offset, vadc_gain;
	u8 buf[2] = {0, 0};

	vote(chip->disable_votable, USER_VOTER, 1, 0);

	rc = qnovo_read(chip, QNOVO_IADC_OFFSET_0, &iadc_offset_external, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc exernal offset rc = %d\n", rc);
		return rc;
	}

	rc = qnovo_read(chip, QNOVO_IADC_OFFSET_1, &iadc_offset_internal, 1);
	if (rc < 0) {
		pr_err("Couldn't read iadc internal offset rc = %d\n", rc);
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

	chip->external_offset_nA = (s64)iadc_offset_external * IADC_LSB_NA;
	chip->internal_offset_nA = (s64)iadc_offset_internal * IADC_LSB_NA;
	chip->offset_nV = (s64)vadc_offset * VADC_LSB_NA;
	chip->external_i_gain_mega
		= 1000000000 + (s64)iadc_gain_external * GAIN_LSB_FACTOR;
	chip->external_i_gain_mega
		= div_s64(chip->external_i_gain_mega, 1000);
	chip->internal_i_gain_mega
		= 1000000000 + (s64)iadc_gain_internal * GAIN_LSB_FACTOR;
	chip->internal_i_gain_mega
		= div_s64(chip->internal_i_gain_mega, 1000);
	chip->v_gain_mega = 1000000000 + (s64)vadc_gain * GAIN_LSB_FACTOR;
	chip->v_gain_mega = div_s64(chip->v_gain_mega, 1000);

	if (chip->wa_flags & QNOVO_ERASE_OFFSET_WA_BIT) {
		rc = qnovo_write(chip, QNOVO_TR_IADC_OFFSET_0, buf, 2);
		if (rc < 0) {
			pr_err("Couldn't erase offset rc = %d\n", rc);
			return rc;
		}
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

	rc = qnovo_check_chg_version(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't check version rc=%d\n", rc);
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

	INIT_WORK(&chip->status_change_work, status_change_work);

	rc = qnovo_hw_init(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto destroy_votable;
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
	chip->qnovo_class.class_attrs = qnovo_attributes;

	rc = class_register(&chip->qnovo_class);
	if (rc < 0) {
		pr_err("couldn't register qnovo sysfs class rc = %d\n", rc);
		goto unreg_notifier;
	}

	return rc;

unreg_notifier:
	power_supply_unreg_notifier(&chip->nb);
destroy_votable:
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
	destroy_votable(chip->disable_votable);
	platform_set_drvdata(pdev, NULL);
	return 0;
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
};
module_platform_driver(qnovo_driver);

MODULE_DESCRIPTION("QPNP Qnovo Driver");
MODULE_LICENSE("GPL v2");
