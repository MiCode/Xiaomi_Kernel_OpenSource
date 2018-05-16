/* Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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
#include <linux/pmic-voter.h>
#include <linux/delay.h>

#define QNOVO_PE_CTRL			0x45
#define QNOVO_PTRAIN_EN_BIT		BIT(7)

#define QNOVO_NREST1_CTRL		0x4A
#define QNOVO_NPULS1_CTRL		0x4B
#define QNOVO_PREST1_CTRL		0x4C
#define QNOVO_NREST2_CTRL		0x4D
#define QNOVO_NPULS2_CTRL		0x4E
#define QNOVO_NREST3_CTRL		0x4F
#define QNOVO_NPULS3_CTRL		0x50
#define QNOVO_ERROR_MASK		0x51
#define QNOVO_VLIM1_LSB_CTRL		0x52
#define QNOVO_VLIM1_MSB_CTRL		0x53
#define QNOVO_VLIM2_LSB_CTRL		0x54
#define QNOVO_VLIM2_MSB_CTRL		0x55
#define QNOVO_PVOLT1_LSB		0x56
#define QNOVO_PVOLT1_MSB		0x57
#define QNOVO_RVOLT2_LSB		0x58
#define QNOVO_RVOLT2_MSB		0x59
#define QNOVO_PVOLT2_LSB		0x5A
#define QNOVO_PVOLT2_MSB		0x5B
#define QNOVO_PCURR1_LSB		0x5C
#define QNOVO_PCURR1_MSB		0x5D
#define QNOVO_PCURR2_LSB		0x5E
#define QNOVO_PCURR2_MSB		0x5F
#define QNOVO_PCURR1_SUM_LSB		0x60
#define QNOVO_PCURR1_SUM_MSB		0x61
#define QNOVO_PCURR1_TERMINAL_LSB	0x62
#define QNOVO_PCURR1_TERMINAL_MSB	0x63
#define QNOVO_PTTIME_LSB		0x64
#define QNOVO_PTTIME_MSB		0x65
#define QNOVO_PPCNT			0x66
#define QNOVO_PPCNT_MAX_CTRL		0x67
#define QNOVO_RVOLT3_VMAX_LSB		0x68
#define QNOVO_RVOLT3_VMAX_MSB		0x69
#define QNOVO_RVOLT3_VMAX_SNUM		0x6A
#define QNOVO_PTTIME_MAX_LSB		0x6C
#define QNOVO_PTTIME_MAX_MSB		0x6D
#define QNOVO_PHASE			0x6E
#define QNOVO_P2_TICK			0x6F
#define QNOVO_PTRAIN_STS		0x70
#define QNOVO_ERROR_STS			0x71

/* QNOVO_ERROR_STS */
#define ERR_CHARGING_DISABLED		BIT(6)
#define ERR_JEITA_HARD_CONDITION	BIT(5)
#define ERR_JEITA_SOFT_CONDITION	BIT(4)
#define ERR_CV_MODE			BIT(3)
#define ERR_SAFETY_TIMER_EXPIRED	BIT(2)
#define ERR_BAT_OV			BIT(1)
#define ERR_BATTERY_MISSING		BIT(0)

#define DRV_MAJOR_VERSION	1
#define DRV_MINOR_VERSION	1

#define USER_VOTER		"user_voter"
#define SHUTDOWN_VOTER		"user_voter"
#define OK_TO_QNOVO_VOTER	"ok_to_qnovo_voter"

#define QNOVO_VOTER		"qnovo_voter"
#define QNOVO_OVERALL_VOTER	"QNOVO_OVERALL_VOTER"
#define QNI_PT_VOTER		"QNI_PT_VOTER"

#define HW_OK_TO_QNOVO_VOTER	"HW_OK_TO_QNOVO_VOTER"
#define CHG_READY_VOTER		"CHG_READY_VOTER"
#define USB_READY_VOTER		"USB_READY_VOTER"

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

struct qnovo {
	struct regmap		*regmap;
	struct device		*dev;
	struct mutex		write_lock;
	struct class		qnovo_class;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct notifier_block	nb;
	struct votable		*disable_votable;
	struct votable		*pt_dis_votable;
	struct votable		*not_ok_to_qnovo_votable;
	struct votable		*chg_ready_votable;
	struct votable		*awake_votable;
	struct work_struct	status_change_work;
	struct delayed_work	usb_debounce_work;
	int			base;
	int			fv_uV_request;
	int			fcc_uA_request;
	int			usb_present;
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

static int qnovo5_read(struct qnovo *chip, u16 addr, u8 *buf, int len)
{
	return regmap_bulk_read(chip->regmap, chip->base + addr, buf, len);
}

static int qnovo5_masked_write(struct qnovo *chip, u16 addr, u8 mask, u8 val)
{
	return regmap_update_bits(chip->regmap, chip->base + addr, mask, val);
}

static int qnovo5_write(struct qnovo *chip, u16 addr, u8 *buf, int len)
{
	return regmap_bulk_write(chip->regmap, chip->base + addr, buf, len);
}

static bool is_batt_available(struct qnovo *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
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
	int rc;

	vote(chip->pt_dis_votable, QNOVO_OVERALL_VOTER, disable, 0);
	rc = qnovo_batt_psy_update(chip, disable);
	return rc;
}

static int pt_dis_votable_cb(struct votable *votable, void *data, int disable,
					const char *client)
{
	struct qnovo *chip = data;
	int rc;

	rc = qnovo5_masked_write(chip, QNOVO_PE_CTRL, QNOVO_PTRAIN_EN_BIT,
				 (bool)disable ? 0 : QNOVO_PTRAIN_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't %s pulse train rc=%d\n",
			(bool)disable ? "disable" : "enable", rc);
		return rc;
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

static int qnovo5_parse_dt(struct qnovo *chip)
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
	PTRAIN_STS_REG,
	ERR_STS_REG,
	PREST1,
	NREST1,
	NPULS1,
	PPCNT,
	PPCNT_MAX,
	VLIM1,
	PVOLT1,
	PCURR1,
	PCURR1_SUM,
	PCURR1_TERMINAL,
	PTTIME,
	PTTIME_MAX,
	NREST2,
	NPULS2,
	VLIM2,
	PVOLT2,
	RVOLT2,
	PCURR2,
	NREST3,
	NPULS3,
	RVOLT3_VMAX,
	RVOLT3_VMAX_SNUM,
	VBATT,
	IBATT,
	BATTTEMP,
	BATTSOC,
	MAX_PROP
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
	[PTRAIN_STS_REG] = {
		.name			= "PTRAIN_STS",
		.start_addr		= QNOVO_PTRAIN_STS,
		.num_regs		= 1,
		.units_str		= "",
	},
	[ERR_STS_REG] = {
		.name			= "RAW_CHGR_ERR",
		.start_addr		= QNOVO_ERROR_STS,
		.num_regs		= 1,
		.units_str		= "",
	},
	[PREST1] = {
		.name			= "PREST1",
		.start_addr		= QNOVO_PREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[NREST1] = {
		.name			= "NREST1",
		.start_addr		= QNOVO_NREST1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[NPULS1] = {
		.name			= "NPULS1",
		.start_addr		= QNOVO_NPULS1_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[PPCNT] = {
		.name			= "PPCNT",
		.start_addr		= QNOVO_PPCNT,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.min_val		= 1,
		.max_val		= 255,
		.units_str		= "pulses",
	},
	[PPCNT_MAX] = {
		.name			= "PPCNT_MAX",
		.start_addr		= QNOVO_PPCNT_MAX_CTRL,
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
		.reg_to_unit_multiplier	= 194637, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 2200000,
		.max_val		= 4500000,
		.units_str		= "uV",
	},
	[PVOLT1] = {
		.name			= "PVOLT1",
		.start_addr		= QNOVO_PVOLT1_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 194637, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[PCURR1] = {
		.name			= "PCURR1",
		.start_addr		= QNOVO_PCURR1_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 305185, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[PCURR1_SUM] = {
		.name			= "PCURR1_SUM",
		.start_addr		= QNOVO_PCURR1_SUM_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 305185, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[PCURR1_TERMINAL] = {
		.name			= "PCURR1_TERMINAL",
		.start_addr		= QNOVO_PCURR1_TERMINAL_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 305185, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[PTTIME] = {
		.name			= "PTTIME",
		.start_addr		= QNOVO_PTTIME_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.units_str		= "S",
	},
	[PTTIME_MAX] = {
		.name			= "PTTIME_MAX",
		.start_addr		= QNOVO_PTTIME_MAX_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.units_str		= "S",
	},
	[NREST2] = {
		.name			= "NREST2",
		.start_addr		= QNOVO_NREST2_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[NPULS2] = {
		.name			= "NPULS2",
		.start_addr		= QNOVO_NPULS2_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[VLIM2] = {
		.name			= "VLIM2",
		.start_addr		= QNOVO_VLIM2_LSB_CTRL,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 194637, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.min_val		= 2200000,
		.max_val		= 4500000,
		.units_str		= "uV",
	},
	[PVOLT2] = {
		.name			= "PVOLT2",
		.start_addr		= QNOVO_PVOLT2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 194637, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[RVOLT2] = {
		.name			= "RVOLT2",
		.start_addr		= QNOVO_RVOLT2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 194637,
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[PCURR2] = {
		.name			= "PCURR2",
		.start_addr		= QNOVO_PCURR2_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 305185, /* converts to nA */
		.reg_to_unit_divider	= 1,
		.units_str		= "uA",
	},
	[NREST3] = {
		.name			= "NREST3",
		.start_addr		= QNOVO_NREST3_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[NPULS3] = {
		.name			= "NPULS3",
		.start_addr		= QNOVO_NPULS3_CTRL,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 976650,
		.reg_to_unit_divider	= 1000,
		.min_val		= 0,
		.max_val		= 249135,
		.units_str		= "uS",
	},
	[RVOLT3_VMAX] = {
		.name			= "RVOLT3_VMAX",
		.start_addr		= QNOVO_RVOLT3_VMAX_LSB,
		.num_regs		= 2,
		.reg_to_unit_multiplier	= 194637, /* converts to nV */
		.reg_to_unit_divider	= 1,
		.units_str		= "uV",
	},
	[RVOLT3_VMAX_SNUM] = {
		.name			= "SNUM",
		.start_addr		= QNOVO_RVOLT3_VMAX_SNUM,
		.num_regs		= 1,
		.reg_to_unit_multiplier	= 1,
		.reg_to_unit_divider	= 1,
		.units_str		= "pulses",
	},
	[VBATT] = {
		.name			= "POWER_SUPPLY_PROP_VOLTAGE_NOW",
		.start_addr		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
		.units_str		= "uV",
	},
	[IBATT] = {
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

static int __find_attr_idx(struct attribute *attr)
{
	int i;

	for (i = 0; i < MAX_PROP; i++)
		if (attr == qnovo_class_attrs[i])
			break;

	if (i == MAX_PROP)
		return -EINVAL;

	return i;
}

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
	int i;
	int val = 0;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

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
	int i;
	unsigned long val;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
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
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	int rc, i;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	rc = qnovo5_read(chip, params[i].start_addr, buf, params[i].num_regs);
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
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	unsigned long val;
	int rc, i;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	buf[0] = val & 0xFF;
	buf[1] = (val >> 8) & 0xFF;

	rc = qnovo5_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	return count;
}

static ssize_t time_show(struct class *c, struct class_attribute *attr,
		char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	int val;
	int rc, i;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	rc = qnovo5_read(chip, params[i].start_addr, buf, params[i].num_regs);
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
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	u16 regval;
	unsigned long val;
	int rc, i;

	if (kstrtoul(ubuf, 0, &val))
		return -EINVAL;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
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

	rc = qnovo5_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t current_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc, i, regval_uA;
	s64 regval_nA;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	rc = qnovo5_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	regval_nA = (s16)(buf[1] << 8 | buf[0]);
	regval_nA = div_s64(regval_nA * params[i].reg_to_unit_multiplier,
					params[i].reg_to_unit_divider)
			- params[i].reg_to_unit_offset;

	regval_uA = div_s64(regval_nA, 1000);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", regval_uA);
}

static ssize_t current_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc, i;
	long val_uA;
	s64 regval_nA;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	if (kstrtoul(ubuf, 0, &val_uA))
		return -EINVAL;

	if (val_uA < params[i].min_val || val_uA > params[i].max_val) {
		pr_err("Out of Range %d%s for %s\n", (int)val_uA,
				params[i].units_str,
				params[i].name);
		return -ERANGE;
	}

	regval_nA = (s64)val_uA * 1000;
	regval_nA = div_s64((regval_nA + params[i].reg_to_unit_offset)
			* params[i].reg_to_unit_divider,
			params[i].reg_to_unit_multiplier);
	buf[0] = regval_nA & 0xFF;
	buf[1] = (regval_nA >> 8) & 0xFF;

	rc = qnovo5_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t voltage_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc, i, regval_uV;
	s64 regval_nV;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	rc = qnovo5_read(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't read %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}
	regval_nV = buf[1] << 8 | buf[0];
	regval_nV = div_s64(regval_nV * params[i].reg_to_unit_multiplier,
					params[i].reg_to_unit_divider)
			- params[i].reg_to_unit_offset;

	regval_uV = div_s64(regval_nV, 1000);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", regval_uV);
}

static ssize_t voltage_store(struct class *c, struct class_attribute *attr,
		       const char *ubuf, size_t count)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	u8 buf[2] = {0, 0};
	int rc, i;
	unsigned long val_uV;
	s64 regval_nV;

	if (kstrtoul(ubuf, 0, &val_uV))
		return -EINVAL;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	if (val_uV < params[i].min_val || val_uV > params[i].max_val) {
		pr_err("Out of Range %d%s for %s\n", (int)val_uV,
				params[i].units_str,
				params[i].name);
		return -ERANGE;
	}

	regval_nV = (s64)val_uV * 1000;
	regval_nV = div_s64((regval_nV + params[i].reg_to_unit_offset)
			* params[i].reg_to_unit_divider,
			params[i].reg_to_unit_multiplier);
	buf[0] = regval_nV & 0xFF;
	buf[1] = ((u64)regval_nV >> 8) & 0xFF;

	rc = qnovo5_write(chip, params[i].start_addr, buf, params[i].num_regs);
	if (rc < 0) {
		pr_err("Couldn't write %s rc = %d\n", params[i].name, rc);
		return -EINVAL;
	}

	return count;
}

static ssize_t batt_prop_show(struct class *c, struct class_attribute *attr,
				char *ubuf)
{
	struct qnovo *chip = container_of(c, struct qnovo, qnovo_class);
	union power_supply_propval pval = {0};
	int i, prop, rc = -EINVAL;

	if (!is_batt_available(chip))
		return -EINVAL;

	i = __find_attr_idx(&attr->attr);
	if (i < 0)
		return -EINVAL;

	prop = params[i].start_addr;

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
CLASS_ATTR_IDX_RO(PTRAIN_STS_REG, reg);
CLASS_ATTR_IDX_RO(ERR_STS_REG, reg);
CLASS_ATTR_IDX_RW(PREST1_uS, time);
CLASS_ATTR_IDX_RW(NREST1_uS, time);
CLASS_ATTR_IDX_RW(NPULS1_uS, time);
CLASS_ATTR_IDX_RO(PPCNT, time);
CLASS_ATTR_IDX_RW(PPCNT_MAX, time);
CLASS_ATTR_IDX_RW(VLIM1_uV, voltage);
CLASS_ATTR_IDX_RO(PVOLT1_uV, voltage);
CLASS_ATTR_IDX_RO(PCURR1_uA, current);
CLASS_ATTR_IDX_RO(PCURR1_SUM_uA, current);
CLASS_ATTR_IDX_RW(PCURR1_TERMINAL_uA, current);
CLASS_ATTR_IDX_RO(PTTIME_S, time);
CLASS_ATTR_IDX_RW(PTTIME_MAX_S, time);
CLASS_ATTR_IDX_RW(NREST2_uS, time);
CLASS_ATTR_IDX_RW(NPULS2_uS, time);
CLASS_ATTR_IDX_RW(VLIM2_uV, voltage);
CLASS_ATTR_IDX_RO(PVOLT2_uV, voltage);
CLASS_ATTR_IDX_RO(RVOLT2_uV, voltage);
CLASS_ATTR_IDX_RO(PCURR2_uA, current);
CLASS_ATTR_IDX_RW(NREST3_uS, time);
CLASS_ATTR_IDX_RW(NPULS3_uS, time);
CLASS_ATTR_IDX_RO(RVOLT3_VMAX_uV, voltage);
CLASS_ATTR_IDX_RO(RVOLT3_VMAX_SNUM, time);
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
	[PTRAIN_STS_REG]	= &class_attr_PTRAIN_STS_REG.attr,
	[ERR_STS_REG]		= &class_attr_ERR_STS_REG.attr,
	[PREST1]		= &class_attr_PREST1_uS.attr,
	[NREST1]		= &class_attr_NREST1_uS.attr,
	[NPULS1]		= &class_attr_NPULS1_uS.attr,
	[PPCNT]			= &class_attr_PPCNT.attr,
	[PPCNT_MAX]		= &class_attr_PPCNT_MAX.attr,
	[VLIM1]			= &class_attr_VLIM1_uV.attr,
	[PVOLT1]		= &class_attr_PVOLT1_uV.attr,
	[PCURR1]		= &class_attr_PCURR1_uA.attr,
	[PCURR1_SUM]		= &class_attr_PCURR1_SUM_uA.attr,
	[PCURR1_TERMINAL]	= &class_attr_PCURR1_TERMINAL_uA.attr,
	[PTTIME]		= &class_attr_PTTIME_S.attr,
	[PTTIME_MAX]		= &class_attr_PTTIME_MAX_S.attr,
	[NREST2]		= &class_attr_NREST2_uS.attr,
	[NPULS2]		= &class_attr_NPULS2_uS.attr,
	[VLIM2]			= &class_attr_VLIM2_uV.attr,
	[PVOLT2]		= &class_attr_PVOLT2_uV.attr,
	[RVOLT2]		= &class_attr_RVOLT2_uV.attr,
	[PCURR2]		= &class_attr_PCURR2_uA.attr,
	[NREST3]		= &class_attr_NREST3_uS.attr,
	[NPULS3]		= &class_attr_NPULS3_uS.attr,
	[RVOLT3_VMAX]		= &class_attr_RVOLT3_VMAX_uV.attr,
	[RVOLT3_VMAX_SNUM]	= &class_attr_RVOLT3_VMAX_SNUM.attr,
	[VBATT]			= &class_attr_VBATT_uV.attr,
	[IBATT]			= &class_attr_IBATT_uA.attr,
	[BATTTEMP]		= &class_attr_BATTTEMP_deciDegC.attr,
	[BATTSOC]		= &class_attr_BATTSOC.attr,
	NULL,
};
ATTRIBUTE_GROUPS(qnovo_class);

static int qnovo5_update_status(struct qnovo *chip)
{
	u8 val = 0;
	int rc;
	bool hw_ok_to_qnovo;

	rc = qnovo5_read(chip, QNOVO_ERROR_STS, &val, 1);
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

#define DEBOUNCE_MS 15000  /* 15 seconds */
static void status_change_work(struct work_struct *work)
{
	struct qnovo *chip = container_of(work,
			struct qnovo, status_change_work);
	union power_supply_propval pval;
	bool usb_present = false;
	int rc;

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

	qnovo5_update_status(chip);
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
		|| strcmp(psy->desc->name, "usb") == 0)
		schedule_work(&chip->status_change_work);

	return NOTIFY_OK;
}

static irqreturn_t handle_ptrain_done(int irq, void *data)
{
	struct qnovo *chip = data;

	qnovo5_update_status(chip);

	/*
	 * hw resets pt_en bit once ptrain_done triggers.
	 * vote on behalf of QNI to disable it such that
	 * once QNI enables it, the votable state changes
	 * and the callback that sets it is indeed invoked
	 */
	vote(chip->pt_dis_votable, QNI_PT_VOTER, true, 0);

	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return IRQ_HANDLED;
}

static int qnovo5_hw_init(struct qnovo *chip)
{
	int rc;
	u8 val;

	vote(chip->chg_ready_votable, USB_READY_VOTER, false, 0);

	vote(chip->disable_votable, USER_VOTER, true, 0);

	vote(chip->pt_dis_votable, QNI_PT_VOTER, true, 0);
	vote(chip->pt_dis_votable, QNOVO_OVERALL_VOTER, true, 0);

	/* allow charger error conditions to disable qnovo, CV mode excluded */
	val = ERR_JEITA_SOFT_CONDITION | ERR_BAT_OV |
		ERR_BATTERY_MISSING | ERR_SAFETY_TIMER_EXPIRED |
		ERR_CHARGING_DISABLED | ERR_JEITA_HARD_CONDITION;
	rc = qnovo5_write(chip, QNOVO_ERROR_MASK, &val, 1);
	if (rc < 0) {
		pr_err("Couldn't write QNOVO_ERROR_MASK rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int qnovo5_register_notifier(struct qnovo *chip)
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

static int qnovo5_determine_initial_status(struct qnovo *chip)
{
	status_change_work(&chip->status_change_work);
	return 0;
}

static int qnovo5_request_interrupts(struct qnovo *chip)
{
	int rc = 0;
	int irq_ptrain_done = of_irq_get_byname(chip->dev->of_node,
						"ptrain-done");

	rc = devm_request_threaded_irq(chip->dev, irq_ptrain_done, NULL,
					handle_ptrain_done,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"ptrain-done", chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc = %d\n",
					irq_ptrain_done, rc);
		return rc;
	}

	enable_irq_wake(irq_ptrain_done);

	return rc;
}

static int qnovo5_probe(struct platform_device *pdev)
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

	rc = qnovo5_parse_dt(chip);
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
		chip->disable_votable = NULL;
		goto cleanup;
	}

	chip->pt_dis_votable = create_votable("QNOVO_PT_DIS", VOTE_SET_ANY,
					pt_dis_votable_cb, chip);
	if (IS_ERR(chip->pt_dis_votable)) {
		rc = PTR_ERR(chip->pt_dis_votable);
		chip->pt_dis_votable = NULL;
		goto destroy_disable_votable;
	}

	chip->not_ok_to_qnovo_votable = create_votable("QNOVO_NOT_OK",
					VOTE_SET_ANY,
					not_ok_to_qnovo_cb, chip);
	if (IS_ERR(chip->not_ok_to_qnovo_votable)) {
		rc = PTR_ERR(chip->not_ok_to_qnovo_votable);
		chip->not_ok_to_qnovo_votable = NULL;
		goto destroy_pt_dis_votable;
	}

	chip->chg_ready_votable = create_votable("QNOVO_CHG_READY",
					VOTE_SET_ANY,
					chg_ready_cb, chip);
	if (IS_ERR(chip->chg_ready_votable)) {
		rc = PTR_ERR(chip->chg_ready_votable);
		chip->chg_ready_votable = NULL;
		goto destroy_not_ok_to_qnovo_votable;
	}

	chip->awake_votable = create_votable("QNOVO_AWAKE", VOTE_SET_ANY,
					awake_cb, chip);
	if (IS_ERR(chip->awake_votable)) {
		rc = PTR_ERR(chip->awake_votable);
		chip->awake_votable = NULL;
		goto destroy_chg_ready_votable;
	}

	INIT_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->usb_debounce_work, usb_debounce_work);

	rc = qnovo5_hw_init(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto destroy_awake_votable;
	}

	rc = qnovo5_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto unreg_notifier;
	}

	rc = qnovo5_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n", rc);
		goto unreg_notifier;
	}

	rc = qnovo5_request_interrupts(chip);
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

static int qnovo5_remove(struct platform_device *pdev)
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

static void qnovo5_shutdown(struct platform_device *pdev)
{
	struct qnovo *chip = platform_get_drvdata(pdev);

	vote(chip->not_ok_to_qnovo_votable, SHUTDOWN_VOTER, true, 0);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-qnovo5", },
	{ },
};

static struct platform_driver qnovo5_driver = {
	.driver		= {
		.name		= "qcom,qnovo5-driver",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= qnovo5_probe,
	.remove		= qnovo5_remove,
	.shutdown	= qnovo5_shutdown,
};
module_platform_driver(qnovo5_driver);

MODULE_DESCRIPTION("QPNP Qnovo5 Driver");
MODULE_LICENSE("GPL v2");
