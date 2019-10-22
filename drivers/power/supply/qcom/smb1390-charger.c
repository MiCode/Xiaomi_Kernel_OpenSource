/* Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#define pr_fmt(fmt) "SMB1390: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/iio/consumer.h>
#include <linux/workqueue.h>

#define CORE_STATUS1_REG		0x1006
#define WIN_OV_BIT			BIT(0)
#define WIN_UV_BIT			BIT(1)
#define EN_PIN_OUT_BIT			BIT(2)
#define LCM_AUTO_BIT			BIT(3)
#define LCM_PIN_BIT			BIT(4)
#define ILIM_BIT			BIT(5)
#define TEMP_ALARM_BIT			BIT(6)
#define VPH_OV_SOFT_BIT			BIT(7)

#define CORE_STATUS2_REG		0x1007
#define SWITCHER_HOLD_OFF_BIT		BIT(0)
#define VPH_OV_HARD_BIT			BIT(1)
#define TSD_BIT				BIT(2)
#define IREV_BIT			BIT(3)
#define IOC_BIT				BIT(4)
#define VIN_UV_BIT			BIT(5)
#define VIN_OV_BIT			BIT(6)
#define EN_PIN_OUT2_BIT			BIT(7)

#define CORE_STATUS3_REG		0x1008
#define EN_SL_BIT			BIT(0)
#define IIN_REF_SS_DONE_BIT		BIT(1)
#define FLYCAP_SS_DONE_BIT		BIT(2)
#define SL_DETECTED_BIT			BIT(3)

#define CORE_INT_RT_STS_REG		0x1010
#define SWITCHER_OFF_WINDOW_STS_BIT	BIT(0)
#define SWITCHER_OFF_FAULT_STS_BIT	BIT(1)
#define TSD_STS_BIT			BIT(2)
#define IREV_STS_BIT			BIT(3)
#define VPH_OV_HARD_STS_BIT		BIT(4)
#define VPH_OV_SOFT_STS_BIT		BIT(5)
#define ILIM_STS_BIT			BIT(6)
#define TEMP_ALARM_STS_BIT		BIT(7)

#define CORE_CONTROL1_REG		0x1020
#define CMD_EN_SWITCHER_BIT		BIT(0)
#define CMD_EN_SL_BIT			BIT(1)

#define CORE_FTRIM_ILIM_REG		0x1030
#define CFG_ILIM_MASK			GENMASK(4, 0)

#define CORE_FTRIM_LVL_REG		0x1033
#define CFG_WIN_HI_MASK			GENMASK(3, 2)
#define WIN_OV_LVL_1000MV		0x08
#define WIN_OV_LVL_1200MV		0x0C

#define CORE_FTRIM_CTRL			0x1031
#define CFG_TEMP_ALERT_LVL_MASK		GENMASK(6, 5)
#define TEMP_ALERT_LVL_125C		0x00
#define TEMP_ALERT_LVL_110C		0x10
#define TEMP_ALERT_LVL_90C		0x20
#define TEMP_ALERT_LVL_80C		0x30
#define CFG_EN_SL_MASK			BIT(1)
#define ENABLE_SLAVE_FROM_PIN		0x2
#define ENABLE_SLAVE_FORM_CMD		0x0

#define CORE_FTRIM_MISC_REG		0x1034
#define TR_IREV_BIT			BIT(1)
#define TR_WIN_1P5X_BIT			BIT(0)
#define WINDOW_DETECTION_DELTA_X1P0	0
#define WINDOW_DETECTION_DELTA_X1P5	1
#define IREV_THREAD_200MA		0
#define IREV_THREAD_300MA		1

#define CORE_FTRIM_SL			0x1036
#define TR_EN_SL_DRIVER_MASK		BIT(5)
#define SYNC_DRV_DISABLED		0x00
#define SYNC_DRV_ENABLED		0x20

#define CORE_ATEST1_SEL_REG		0x10E2
#define ATEST1_OUTPUT_ENABLE_BIT	BIT(7)
#define ATEST1_SEL_MASK			GENMASK(6, 0)
#define ISNS_INT_VAL			0x09

#define CORE_VERSION_REG		0x9F1

#define BATT_PROFILE_VOTER	"BATT_PROFILE_VOTER"
#define CP_VOTER		"CP_VOTER"
#define USER_VOTER		"USER_VOTER"
#define ILIM_VOTER		"ILIM_VOTER"
#define ICL_VOTER		"ICL_VOTER"
#define ICL_CHANGE_VOTER	"ICL_CHANGE_VOTER"
#define TAPER_END_VOTER		"TAPER_END_VOTER"
#define WIRELESS_VOTER		"WIRELESS_VOTER"
#define SRC_VOTER		"SRC_VOTER"
#define SWITCHER_TOGGLE_VOTER	"SWITCHER_TOGGLE_VOTER"
#define SOC_LEVEL_VOTER		"SOC_LEVEL_VOTER"

enum cp_role {
	CP_MASTER = 0,
	CP_SLAVE,
};

enum {
	SWITCHER_OFF_WINDOW_IRQ = 0,
	SWITCHER_OFF_FAULT_IRQ,
	TSD_IRQ,
	IREV_IRQ,
	VPH_OV_HARD_IRQ,
	VPH_OV_SOFT_IRQ,
	ILIM_IRQ,
	TEMP_ALARM_IRQ,
	NUM_IRQS,
};

enum {
	ISNS_MODE_OFF = 0,
	ISNS_MODE_ACTIVE,
	ISNS_MODE_STANDBY,
};

struct smb1390_iio {
	struct iio_channel	*die_temp_chan;
};

struct smb1390 {
	struct device		*dev;
	struct regmap		*regmap;
	struct notifier_block	nb;
	struct class		cp_class;
	struct wakeup_source	*cp_ws;

	/* work structs */
	struct work_struct	status_change_work;
	struct work_struct	taper_work;

	/* mutexes */
	spinlock_t		status_change_lock;
	struct mutex		die_chan_lock;

	/* votables */
	struct votable		*disable_votable;
	struct votable		*ilim_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*cp_awake_votable;

	/* power supplies */
	struct power_supply	*cps_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*dc_psy;

	int			irqs[NUM_IRQS];
	bool			status_change_running;
	bool			taper_work_running;
	bool			taper_early_trigger;
	struct smb1390_iio	iio;
	int			irq_status;

	enum cp_role		cp_role;
	bool			ok_to_dual;
	int			dual_thld;
	struct delayed_work     reg_work;
	int			taper_entry_fv;
	u32			max_cutoff_soc;
};

struct smb_irq {
	const char		*name;
	const irq_handler_t	handler;
	const bool		wake;
};

static const struct smb_irq smb_irqs[];

static int smb1390_read(struct smb1390 *chip, int reg, int *val)
{
	int rc;

	rc = regmap_read(chip->regmap, reg, val);
	if (rc < 0)
		pr_err("Couldn't read 0x%04x\n", reg);

	return rc;
}

static int smb1390_masked_write(struct smb1390 *chip, int reg, int mask,
				int val)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(chip->regmap, reg, mask, val);
	if (rc < 0)
		pr_err("Couldn't write 0x%02x to 0x%04x with mask 0x%02x\n",
		       val, reg, mask);

	return rc;
}

static bool is_psy_voter_available(struct smb1390 *chip)
{
	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			pr_debug("Couldn't find battery psy\n");
			return false;
		}
	}

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			pr_debug("Couldn't find usb psy\n");
			return false;
		}
	}

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			pr_debug("Couldn't find dc psy\n");
			return false;
		}
	}

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			pr_debug("Couldn't find FCC votable\n");
			return false;
		}
	}

	if (!chip->fv_votable) {
		chip->fv_votable = find_votable("FV");
		if (!chip->fv_votable) {
			pr_debug("Couldn't find FV votable\n");
			return false;
		}
	}

	return true;
}

static void cp_toggle_switcher(struct smb1390 *chip)
{
	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, true, 0);

	/* Delay for toggling switcher */
	usleep_range(20, 30);

	vote(chip->disable_votable, SWITCHER_TOGGLE_VOTER, false, 0);
}

static int smb1390_is_batt_soc_valid(struct smb1390 *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!chip->batt_psy)
		goto out;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_err("Couldn't get CAPACITY rc=%d\n", rc);
		goto out;
	}

	if (pval.intval >= chip->max_cutoff_soc)
		return false;

out:
	return true;
}

static irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb1390 *chip = data;

	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return IRQ_HANDLED;
}

static const struct smb_irq smb_irqs[] = {
	[SWITCHER_OFF_WINDOW_IRQ] = {
		.name		= "switcher-off-window",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[SWITCHER_OFF_FAULT_IRQ] = {
		.name		= "switcher-off-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[TSD_IRQ] = {
		.name		= "tsd-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[IREV_IRQ] = {
		.name		= "irev-fault",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[VPH_OV_HARD_IRQ] = {
		.name		= "vph-ov-hard",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[VPH_OV_SOFT_IRQ] = {
		.name		= "vph-ov-soft",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[ILIM_IRQ] = {
		.name		= "ilim",
		.handler	= default_irq_handler,
		.wake		= true,
	},
	[TEMP_ALARM_IRQ] = {
		.name		= "temp-alarm",
		.handler	= default_irq_handler,
		.wake		= true,
	},
};

/* SYSFS functions for reporting smb1390 charge pump state */
static ssize_t stat1_show(struct class *c, struct class_attribute *attr,
			 char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	int rc, val;

	rc = smb1390_read(chip, CORE_STATUS1_REG, &val);
	if (rc < 0)
		return -EINVAL;
	/*pr_info("smb1390 CORE_STATUS1_REG: 0x%x\n", val);*/
	return snprintf(buf, PAGE_SIZE, "%x\n", val);
}
static CLASS_ATTR_RO(stat1);

static ssize_t stat2_show(struct class *c, struct class_attribute *attr,
			 char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	int rc, val;

	rc = smb1390_read(chip, CORE_STATUS2_REG, &val);
	if (rc < 0)
		return -EINVAL;
	/*pr_info("smb1390 CORE_STATUS2_REG: 0x%x\n", val);*/
	return snprintf(buf, PAGE_SIZE, "%x\n", val);
}
static CLASS_ATTR_RO(stat2);

static ssize_t model_name_show(struct class *c, struct class_attribute *attr,
			 char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	int rc, val;

	rc = smb1390_read(chip, CORE_STATUS1_REG, &val);
	if (rc < 0)
		return snprintf(buf, PAGE_SIZE, "%s\n", "unknown");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "smb1390");
}
static CLASS_ATTR_RO(model_name);

static ssize_t enable_show(struct class *c, struct class_attribute *attr,
			   char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			!get_effective_result(chip->disable_votable));
}

static ssize_t enable_store(struct class *c, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	pr_info("enable smb1390: %d\n", val);
	vote(chip->disable_votable, USER_VOTER, !val, 0);
	return count;
}
static CLASS_ATTR_RW(enable);

static ssize_t cp_irq_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	int rc, val;

	rc = smb1390_read(chip, CORE_INT_RT_STS_REG, &val);
	if (rc < 0)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%x\n", val);
}
static CLASS_ATTR_RO(cp_irq);

static ssize_t thld2_show(struct class *c, struct class_attribute *attr,
			   char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->dual_thld);
}

static ssize_t thld2_store(struct class *c, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	chip->dual_thld = val;
	return count;
}
static CLASS_ATTR_RW(thld2);

static ssize_t toggle_switcher_store(struct class *c,
			struct class_attribute *attr, const char *buf,
			size_t count)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val)
		cp_toggle_switcher(chip);

	return count;
}
static CLASS_ATTR_WO(toggle_switcher);

static ssize_t die_temp_show(struct class *c, struct class_attribute *attr,
			     char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	int die_temp_deciC = 0;
	int rc;

	mutex_lock(&chip->die_chan_lock);
	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&die_temp_deciC);
	mutex_unlock(&chip->die_chan_lock);

	if (rc < 0) {
		pr_err("Couldn't read die chan, rc = %d\n", rc);
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", die_temp_deciC / 100);
}
static CLASS_ATTR_RO(die_temp);

static int isns_mode_control(struct smb1390 *chip, int role)
{
	int rc;
	u8 val;

	switch (role) {
	case ISNS_MODE_ACTIVE:
		val = ATEST1_OUTPUT_ENABLE_BIT | ISNS_INT_VAL;
		break;
	case ISNS_MODE_STANDBY:
		val = ATEST1_OUTPUT_ENABLE_BIT;
		break;
	case ISNS_MODE_OFF:
	default:
		val = 0;
		break;
	}

	rc = smb1390_masked_write(chip, CORE_ATEST1_SEL_REG,
				ATEST1_OUTPUT_ENABLE_BIT | ATEST1_SEL_MASK,
				val);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);

	return rc;
}

static bool is_cps_available(struct smb1390 *chip)
{
	if (!chip->cps_psy)
		chip->cps_psy = power_supply_get_by_name("cps");

	if (!chip->cps_psy)
		return false;

	return true;
}

static ssize_t isns1_show(struct class *c, struct class_attribute *attr,
			     char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	union power_supply_propval pval = {0, };
	int isns_ma = 0, temp = 0;
	int rc;

	mutex_lock(&chip->die_chan_lock);
	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_STANDBY;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0) {
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
			goto unlock;
		}
	}

	rc = isns_mode_control(chip, ISNS_MODE_ACTIVE);
	if (rc < 0)
		goto unlock;

	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&temp);
	if (rc < 0) {
		pr_err("Couldn't read die chan for isns, rc = %d\n", rc);
		goto unlock;
	}

	rc = isns_mode_control(chip, ISNS_MODE_OFF);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);

	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_OFF;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0)
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
	}

unlock:
	mutex_unlock(&chip->die_chan_lock);

	if (rc < 0)
		return -EINVAL;

	/* ISNS = 2 * (1496 - 1390_therm_input * 0.00356) * 1000 uA */
	isns_ma = (1496 * 1000 - div_s64((s64)temp * 3560, 1000)) * 2;
	printk("%s isns_ma1:%d\n", __func__, isns_ma);
	return snprintf(buf, PAGE_SIZE, "%d\n", isns_ma);
}
static CLASS_ATTR_RO(isns1);

static ssize_t isns2_show(struct class *c, struct class_attribute *attr,
			     char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);
	union power_supply_propval pval = {0, };
	int isns_ma = 0, temp = 0;
	int rc;

	if (!is_cps_available(chip))
		return snprintf(buf, PAGE_SIZE, "%d\n", isns_ma);

	mutex_lock(&chip->die_chan_lock);

	rc = isns_mode_control(chip, ISNS_MODE_STANDBY);
	if (rc < 0)
		goto unlock;

	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_ACTIVE;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0) {
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
			goto unlock;
		}
	}

	rc = iio_read_channel_processed(chip->iio.die_temp_chan,
			&temp);
	if (rc < 0) {
		pr_err("Couldn't read die chan for isns, rc = %d\n", rc);
		goto unlock;
	}

	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_OFF;
		rc = power_supply_set_property(chip->cps_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0)
			pr_err("Couldn't change slave charging state rc=%d\n",
				rc);
	}

	rc = isns_mode_control(chip, ISNS_MODE_OFF);
	if (rc < 0)
		pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);

unlock:
	mutex_unlock(&chip->die_chan_lock);

	if (rc < 0)
		return -EINVAL;

	/* ISNS = 2 * (1496 - 1390_therm_input * 0.00356) * 1000 uA */
	isns_ma = (1496 * 1000 - div_s64((s64)temp * 3560, 1000)) * 2;
	printk("%s isns_ma2:%d\n", __func__, isns_ma);
	return snprintf(buf, PAGE_SIZE, "%d\n", isns_ma);
}
static CLASS_ATTR_RO(isns2);

static ssize_t ilim_show(struct class *c, struct class_attribute *attr,
			     char *buf)
{
	struct smb1390 *chip = container_of(c, struct smb1390, cp_class);

	return snprintf(buf, PAGE_SIZE, "%d\n",
				get_effective_result(chip->ilim_votable));
}
static CLASS_ATTR_RO(ilim);

static struct attribute *cp_class_attrs[] = {
	&class_attr_stat1.attr,
	&class_attr_stat2.attr,
	&class_attr_enable.attr,
	&class_attr_cp_irq.attr,
	&class_attr_thld2.attr,
	&class_attr_toggle_switcher.attr,
	&class_attr_die_temp.attr,
	&class_attr_isns1.attr,
	&class_attr_isns2.attr,
	&class_attr_ilim.attr,
	&class_attr_model_name.attr,
	NULL,
};
ATTRIBUTE_GROUPS(cp_class);

static int smb1390_enable_switcher(struct smb1390 *chip, bool enable)
{
	int rc;
	u8 mask, val;

	rc = smb1390_masked_write(chip, CORE_FTRIM_CTRL,
			CFG_TEMP_ALERT_LVL_MASK,
			TEMP_ALERT_LVL_125C);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	rc = smb1390_masked_write(chip, CORE_FTRIM_SL, TR_EN_SL_DRIVER_MASK, SYNC_DRV_ENABLED);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	mask = CMD_EN_SWITCHER_BIT | CMD_EN_SL_BIT;
	val = chip->ok_to_dual ? mask : CMD_EN_SWITCHER_BIT;
	rc = smb1390_masked_write(chip, CORE_CONTROL1_REG, mask,
			enable ? val : 0);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	return rc;
}

/* voter callbacks */
static int smb1390_disable_vote_cb(struct votable *votable, void *data,
				  int disable, const char *client)
{
	struct smb1390 *chip = data;
	int rc = 0;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	if (disable) {
		rc = smb1390_enable_switcher(chip, false);
		if (rc < 0)
			return rc;

		vote(chip->cp_awake_votable, CP_VOTER, false, 0);
	} else {
		vote(chip->cp_awake_votable, CP_VOTER, true, 0);
		rc = smb1390_enable_switcher(chip, true);
		if (rc < 0)
			return rc;
	}

	/* charging may have been disabled by ILIM; send uevent */
	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return rc;
}

static int smb1390_set_ilim(struct smb1390 *chip, int ilim_ua)
{
	int rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_ILIM_REG,
			CFG_ILIM_MASK, ilim_ua);
	if (rc < 0)
		pr_err("Failed to write ILIM Register, rc=%d\n", rc);

	return rc;
}

static int smb1390_ilim_vote_cb(struct votable *votable, void *data,
			      int ilim_uA, const char *client)
{
	union power_supply_propval pval = {0, };
	struct smb1390 *chip = data;
	bool ok_to_dual;
	int rc = 0;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	/* ILIM should always have at least one active vote */
	if (!client) {
		pr_err("Client missing\n");
		return -EINVAL;
	}

	/* ILIM less than 0.75A is not accurate; disable charging */
	if (ilim_uA < 750000) {
		pr_info("ILIM %duA is too low to allow charging\n", ilim_uA);
		vote(chip->disable_votable, ILIM_VOTER, true, 0);
		ok_to_dual = false;
	} else {
		/* hysterysis for dual smb1390 */
		ok_to_dual = is_cps_available(chip) && (ilim_uA > chip->dual_thld);
		if (ok_to_dual) {
			ilim_uA /= 2;

			pval.intval = DIV_ROUND_CLOSEST(ilim_uA - 500000,
					100000);
			rc = power_supply_set_property(chip->cps_psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
					&pval);
			if (rc < 0)
				pr_err("Couldn't change slave ilim  rc=%d\n",
						rc);
		}

		rc = smb1390_set_ilim(chip,
				DIV_ROUND_CLOSEST(ilim_uA - 500000, 100000));
		if (rc < 0)
			pr_err("Couldn't change SMB1390 ilim  rc=%d\n", rc);
		pr_info("%s ilim_uA:%d:M_1390:%d,S_1390:%d\n", __func__, ilim_uA,
				DIV_ROUND_CLOSEST(ilim_uA - 500000, 100000), pval.intval);

		vote(chip->disable_votable, ILIM_VOTER, false, 0);
	}

	if (ok_to_dual != chip->ok_to_dual) {
		chip->ok_to_dual = ok_to_dual;
		rerun_election(chip->disable_votable);
	}

	/* notify userspace of ILIM change */
	kobject_uevent(&chip->dev->kobj, KOBJ_CHANGE);
	return rc;
}

static int smb1390_awake_vote_cb(struct votable *votable, void *data,
				int awake, const char *client)
{
	struct smb1390 *chip = data;

	if (awake)
		__pm_stay_awake(chip->cp_ws);
	else
		__pm_relax(chip->cp_ws);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static int smb1390_notifier_cb(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct smb1390 *chip = container_of(nb, struct smb1390, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0
				|| strcmp(psy->desc->name, "usb") == 0
				|| strcmp(psy->desc->name, "main") == 0) {
		spin_lock_irqsave(&chip->status_change_lock, flags);
		if (!chip->status_change_running) {
			chip->status_change_running = true;
			pm_stay_awake(chip->dev);
			schedule_work(&chip->status_change_work);
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);
	}

	return NOTIFY_OK;
}


#define TAPER_CAPACITY_THR		55
#define TAPER_CAPCITY_DELTA		1
#define BATT_COOL_THR		220
static void smb1390_status_change_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390,
					    status_change_work);
	union power_supply_propval pval = {0, };
	int capacity, batt_temp, charge_type;
	int max_fcc_ma, rc;

	if (!is_psy_voter_available(chip))
		goto out;

	vote(chip->disable_votable, SOC_LEVEL_VOTER,
			smb1390_is_batt_soc_valid(chip) ? false : true, 0);

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_SMB_EN_MODE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		goto out;
	}

	if (pval.intval == POWER_SUPPLY_CHARGER_SEC_CP) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
		if (rc < 0) {
			pr_err("Couldn't get cp reason rc=%d\n", rc);
			goto out;
		}

		vote(chip->disable_votable, SRC_VOTER, false, 0);

		/*
		 * ILIM is set based on the primary chargers AICL result. This
		 * ensures VBUS does not collapse due to the current drawn via
		 * MID.
		 */
		if (pval.intval == POWER_SUPPLY_CP_WIRELESS) {
			vote(chip->ilim_votable, ICL_VOTER, false, 0);
			vote(chip->ilim_votable, ICL_CHANGE_VOTER, false, 0);
			rc = power_supply_get_property(chip->dc_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
			if (rc < 0)
				pr_err("Couldn't get dc icl rc=%d\n", rc);
			else
				vote(chip->ilim_votable, WIRELESS_VOTER, true,
								pval.intval);
		} else { /* QC3 or PPS */
			vote(chip->ilim_votable, WIRELESS_VOTER, false, 0);
			rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
			if (rc < 0)
				pr_err("Couldn't get usb icl rc=%d\n", rc);
			else {
				vote(chip->ilim_votable, ICL_VOTER, true,
								pval.intval);
			}
		}

		/*
		 * Remove SMB1390 Taper condition disable vote if float voltage
		 * increased in comparison to voltage at which it entered taper.
		 */
		if (chip->taper_entry_fv <
				get_effective_result(chip->fv_votable))
			vote(chip->disable_votable, TAPER_END_VOTER, false, 0);

		/*
		 * all votes that would result in disabling the charge pump have
		 * been cast; ensure the charhe pump is still enabled before
		 * continuing.
		 */
		if (get_effective_result(chip->disable_votable))
			goto out;


		rc = power_supply_get_property(chip->batt_psy,
			       POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt capacity rc=%d\n", rc);
			goto out;
		}
		capacity = pval.intval;

		rc = power_supply_get_property(chip->batt_psy,
			       POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt temp rc=%d\n", rc);
			goto out;
		}
		batt_temp = pval.intval;

		rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
			goto out;
		}
		charge_type = pval.intval;

		pr_info("capacity:%d, batt_temp:%d, charge_type:%d\n",
				capacity, batt_temp, charge_type);

		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
		} else if (pval.intval ==
				POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			/*
			 * mutual exclusion is already guaranteed by
			 * chip->status_change_running
			 */
			if (!chip->taper_work_running) {
				chip->taper_work_running = true;
				queue_work(system_long_wq,
					   &chip->taper_work);
			}
		}
	} else {
		vote(chip->disable_votable, SRC_VOTER, true, 0);
		vote(chip->disable_votable, TAPER_END_VOTER, false, 0);
		max_fcc_ma = get_client_vote(chip->fcc_votable,
				BATT_PROFILE_VOTER);
		vote(chip->fcc_votable, CP_VOTER,
				max_fcc_ma > 0 ? true : false, max_fcc_ma);
		vote(chip->disable_votable, SOC_LEVEL_VOTER, true, 0);
	}

out:
	pm_relax(chip->dev);
	chip->status_change_running = false;
}

static void smb1390_taper_work(struct work_struct *work)
{
	struct smb1390 *chip = container_of(work, struct smb1390, taper_work);
	union power_supply_propval pval = {0, };
	int rc, fcc_uA;
	int capacity;

	if (!is_psy_voter_available(chip))
		goto out;

	do {

		fcc_uA = get_effective_result(chip->fcc_votable);
		if (fcc_uA < 2000000)
			break;

		fcc_uA = get_client_vote(chip->fcc_votable, CP_VOTER) - 100000;
		pr_info("taper work reducing FCC to %duA\n", fcc_uA);
		vote(chip->fcc_votable, CP_VOTER, true, fcc_uA);

		msleep(500);

		chip->taper_entry_fv = get_effective_result(chip->fv_votable);
		rc = power_supply_get_property(chip->batt_psy,
			       POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt capacity rc=%d\n", rc);
			goto out;
		}
		capacity = pval.intval;
		/*
		 * if capacity is lower than 55% - 1%(delta)  and taper charge comes
		 * we think it is a early taper, normaly due to battery temperature is low
		 * such as 10 to 22 degree, battery esr is high and high current charging
		 * (charge pump is working during 10 to 22 degree, not working below 10)
		 */
		if ((capacity < (TAPER_CAPACITY_THR - TAPER_CAPCITY_DELTA))
				&& !chip->taper_early_trigger)
			chip->taper_early_trigger = true;

		rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get charge type rc=%d\n", rc);
			goto out;
		}

		if (get_effective_result(chip->fv_votable) >
						chip->taper_entry_fv) {
			pr_debug("Float voltage increased. Exiting taper\n");
			goto out;
		} else {
			chip->taper_entry_fv =
					get_effective_result(chip->fv_votable);
		}

		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			fcc_uA = get_client_vote(chip->fcc_votable, CP_VOTER)
								- 100000;
			pr_debug("taper work reducing FCC to %duA\n", fcc_uA);
			vote(chip->fcc_votable, CP_VOTER, true, fcc_uA);

			if (fcc_uA < 2000000) {
				vote(chip->disable_votable, TAPER_END_VOTER,
								true, 0);
				goto out;
			}
		} else {
			pr_debug("In fast charging. Wait for next taper\n");
		}

		msleep(500);
	} while (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER);

out:
	pr_info("taper work exit\n");
	vote(chip->fcc_votable, CP_VOTER, false, 0);
	chip->taper_work_running = false;
}

static int smb1390_parse_dt(struct smb1390 *chip)
{
	int rc;

	rc = of_property_match_string(chip->dev->of_node, "io-channel-names",
			"cp_die_temp");
	if (rc >= 0) {
		chip->iio.die_temp_chan =
			iio_channel_get(chip->dev, "cp_die_temp");
		if (IS_ERR(chip->iio.die_temp_chan)) {
			rc = PTR_ERR(chip->iio.die_temp_chan);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"cp_die_temp channel unavailable %ld\n",
					rc);
			chip->iio.die_temp_chan = NULL;
			return rc;
		}
	}

	chip->max_cutoff_soc = 85; /* 85% */
	of_property_read_u32(chip->dev->of_node, "qcom,max-cutoff-soc",
			&chip->max_cutoff_soc);

	return rc;
}

static void smb1390_release_channels(struct smb1390 *chip)
{
	if (!IS_ERR_OR_NULL(chip->iio.die_temp_chan))
		iio_channel_release(chip->iio.die_temp_chan);
}

static int smb1390_create_votables(struct smb1390 *chip)
{
	chip->cp_awake_votable = create_votable("CP_AWAKE", VOTE_SET_ANY,
			smb1390_awake_vote_cb, chip);
	if (IS_ERR(chip->cp_awake_votable))
		return PTR_ERR(chip->cp_awake_votable);

	chip->disable_votable = create_votable("CP_DISABLE",
			VOTE_SET_ANY, smb1390_disable_vote_cb, chip);
	if (IS_ERR(chip->disable_votable))
		return PTR_ERR(chip->disable_votable);

	chip->ilim_votable = create_votable("CP_ILIM",
			VOTE_MIN, smb1390_ilim_vote_cb, chip);
	if (IS_ERR(chip->ilim_votable))
		return PTR_ERR(chip->ilim_votable);

	return 0;
}

static void smb1390_destroy_votables(struct smb1390 *chip)
{
	destroy_votable(chip->disable_votable);
	destroy_votable(chip->ilim_votable);
	destroy_votable(chip->cp_awake_votable);
}

static int smb1390_init_hw(struct smb1390 *chip)
{
	int rc;

	/*
	 * charge pump is initially disabled; this indirectly votes to allow
	 * traditional parallel charging if present
	 */
	vote(chip->disable_votable, USER_VOTER, true, 0);
	/* keep charge pump disabled if SOC is above threshold */
	vote(chip->disable_votable, SOC_LEVEL_VOTER,
			smb1390_is_batt_soc_valid(chip) ? false : true, 0);

	/*
	 * Improve ILIM accuracy:
	 *  - Configure window (Vin - 2Vout) OV level to 1000mV
	 *  - Configure VOUT tracking value to 1.0
	 */
	rc = smb1390_masked_write(chip, CORE_FTRIM_LVL_REG,
			CFG_WIN_HI_MASK, WIN_OV_LVL_1200MV);
	if (rc < 0)
		return rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_MISC_REG,
			TR_WIN_1P5X_BIT | TR_IREV_BIT,
			WINDOW_DETECTION_DELTA_X1P5 | IREV_THREAD_200MA);
	if (rc < 0)
		return rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_CTRL,
			CFG_TEMP_ALERT_LVL_MASK,
			TEMP_ALERT_LVL_125C);
	if (rc < 0)
		return rc;

	rc = smb1390_masked_write(chip, CORE_FTRIM_SL, TR_EN_SL_DRIVER_MASK, SYNC_DRV_ENABLED);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	rc = smb1390_masked_write(chip, 0x1032, 0xff, 0x03);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	chip->ok_to_dual = false;
	chip->dual_thld = 1300000;

	return 0;
}

static int smb1390_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb_irqs); i++) {
		if (strcmp(smb_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb1390_request_interrupt(struct smb1390 *chip,
				struct device_node *node,
				const char *irq_name)
{
	int rc = 0, irq, irq_index;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb1390_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb_irqs[irq_index].handler)
		return 0;

	rc = devm_request_threaded_irq(chip->dev, irq, NULL,
				smb_irqs[irq_index].handler,
				IRQF_ONESHOT, irq_name, chip);
	if (rc < 0) {
		pr_err("Couldn't request irq %d rc=%d\n", irq, rc);
		return rc;
	}

	chip->irqs[irq_index] = irq;
	if (smb_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb1390_request_interrupts(struct smb1390 *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smb1390_request_interrupt(chip, child, name);
			if (rc < 0) {
				pr_err("Couldn't request interrupt %s rc=%d\n",
					name, rc);
				return rc;
			}
		}
	}

	return rc;
}

static const struct of_device_id match_table[] = {
	{
		.compatible = "qcom,smb1390-charger",
		.data = (void *) CP_MASTER
	},
	{
		.compatible = "qcom,smb1390-slave",
		.data = (void *) CP_SLAVE
	},
	{ },
};

static int smb1390_master_probe(struct smb1390 *chip)
{
	int rc;

	spin_lock_init(&chip->status_change_lock);
	mutex_init(&chip->die_chan_lock);

	INIT_WORK(&chip->status_change_work, smb1390_status_change_work);
	INIT_WORK(&chip->taper_work, smb1390_taper_work);

	rc = smb1390_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto out_work;
	}

	chip->cp_ws = wakeup_source_register("qcom-chargepump");
	if (!chip->cp_ws)
		return rc;

	rc = smb1390_create_votables(chip);
	if (rc < 0) {
		pr_err("Couldn't create votables rc=%d\n", rc);
		goto out_work;
	}

	rc = smb1390_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't init hardware rc=%d\n", rc);
		goto out_votables;
	}

	chip->nb.notifier_call = smb1390_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc=%d\n", rc);
		goto out_votables;
	}

	chip->cp_class.name = "charge_pump";
	chip->cp_class.owner = THIS_MODULE;
	chip->cp_class.class_groups = cp_class_groups;
	rc = class_register(&chip->cp_class);
	if (rc < 0) {
		pr_err("Couldn't register charge_pump sysfs class rc=%d\n", rc);
		goto out_notifier;

	}

	rc = smb1390_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto out_class;
	}

	return 0;

out_class:
	class_unregister(&chip->cp_class);
out_notifier:
	power_supply_unreg_notifier(&chip->nb);
out_votables:
	smb1390_destroy_votables(chip);
out_work:
	cancel_work(&chip->taper_work);
	cancel_work(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	return rc;
}

static enum power_supply_property smb1390_cps_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb1390_cps_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc;
	int status;

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		rc = smb1390_read(chip, CORE_STATUS1_REG, &status);
		if (rc < 0)
			val->strval = "unknown";
		else
			val->strval = "smb1390";
		break;
	default:
		break;
	}

	return 0;
}

static int smb1390_cps_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb1390 *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		rc = smb1390_set_ilim(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		rc = isns_mode_control(chip, val->intval);
		break;
	default:
		break;
	}

	return 0;
};

static const struct power_supply_desc cps_psy_desc = {
	.name = "cps",
	.type = POWER_SUPPLY_TYPE_PARALLEL,
	.properties = smb1390_cps_props,
	.num_properties = ARRAY_SIZE(smb1390_cps_props),
	.get_property = smb1390_cps_get_prop,
	.set_property = smb1390_cps_set_prop,
};

static int smb1390_init_cps_psy(struct smb1390 *chip)
{
	struct power_supply_config cps_cfg = {};

	cps_cfg.drv_data = chip;
	cps_cfg.of_node = chip->dev->of_node;
	chip->cps_psy = devm_power_supply_register(chip->dev,
						  &cps_psy_desc,
						  &cps_cfg);
	if (IS_ERR(chip->cps_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chip->cps_psy);
	}

	return 0;
}

static int smb1390_slave_probe(struct smb1390 *chip)
{
	int stat, rc;

	/* a "hello" read to test the presence of the slave PMIC */
	rc = smb1390_read(chip, CORE_STATUS1_REG, &stat);
	if (rc < 0)
		return -EINVAL;

	rc = smb1390_masked_write(chip, CORE_FTRIM_CTRL,
			CFG_TEMP_ALERT_LVL_MASK,
			TEMP_ALERT_LVL_125C);
	if (rc < 0)
		return rc;

	rc = smb1390_init_cps_psy(chip);
	if (rc < 0)
		pr_err("Couldn't initialize cps psy rc=%d\n", rc);

	rc = smb1390_masked_write(chip, CORE_FTRIM_MISC_REG,
			TR_WIN_1P5X_BIT | TR_IREV_BIT,
			WINDOW_DETECTION_DELTA_X1P5 | IREV_THREAD_200MA);
	if (rc < 0)
		return rc;

	rc = smb1390_masked_write(chip, 0x1032, 0xff, 0x03);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	/*fix the leakage current for smb1390*/
	rc = smb1390_masked_write(chip, 0x1037, 0xff, 0x80);
	if (rc < 0)
		pr_err("Couldn't write CORE_CONTROL1_REG, rc=%d\n", rc);

	return rc;
}

static void smb1390_reg_work(struct work_struct *work)
{
#if 0
	struct smb1390 *chip = container_of(work, struct smb1390, reg_work.work);
	int rc, addr;
	int stat;

	for (addr = 0x1000; addr <= 0x103f; addr++) {
		rc = smb1390_read(chip, addr, &stat);
		printk("%s:%s:0x%x:%x", __func__, (chip->cp_role == CP_SLAVE) ? "Slave":"Master", addr, stat);
	}

	schedule_delayed_work(&chip->reg_work, 10 * HZ);
#endif
}

struct reg_data {
	int reg;
	int mask;
	int val;
};

struct smb1390_version {
	char str[16];
	int num[2];
	struct reg_data reg[11];
};

struct smb1390_version pmc1000h = {
	"PMC1000H",
	{ 0x00, 0x25 },
	{ { 0x1030, 0x80, 0x01 },
	  { 0x1031, 0xFF, 0x72 },
	  { 0x1033, 0x04, 0x01 },
	  { 0x1034, 0x80, 0x00 },
	  { 0x1035, 0xFF, 0xE3 },
	  { 0x1036, 0xFF, 0xA0 },
	  { 0x1037, 0xFF, 0x84 },
	  { 0x1039, 0xFF, 0x30 },
	  { 0x103A, 0xFF, 0x40 },
	  { 0x103B, 0xFF, 0x20 },
	  { 0x103E, 0xFF, 0x00 },
	}
};

static int smb1390_pmc1000h_detect(struct smb1390 *chip)
{
	int version, rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(pmc1000h.num); i++) {
		rc = smb1390_read(chip, CORE_VERSION_REG + i, &version);
		if (rc < 0)
			return -EINVAL;
		pr_info("version[%d]:%x, num[%d]\n", i, version, pmc1000h.num[i]);
		if (version == pmc1000h.num[i])
			continue;
		else
			return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(pmc1000h.reg); i++) {
		rc = smb1390_masked_write(chip, pmc1000h.reg[i].reg,
				pmc1000h.reg[i].mask, pmc1000h.reg[i].val);
		if (rc < 0)
			pr_err("Couldn't set CORE_ATEST1_SEL_REG, rc = %d\n", rc);
	}

	return 0;

}

static int smb1390_probe(struct platform_device *pdev)
{
	struct smb1390 *chip;
	const struct of_device_id *id;
	int rc;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		pr_err("Couldn't get regmap\n");
		return -EINVAL;
	}

	id = of_match_device(of_match_ptr(match_table), chip->dev);
	if (!id) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, chip);

	chip->cp_role = (enum cp_role)id->data;
	switch (chip->cp_role) {
	case CP_MASTER:
		rc = smb1390_master_probe(chip);
		break;
	case CP_SLAVE:
		rc = smb1390_slave_probe(chip);
		break;
	default:
		pr_err("Couldn't find a matching role %d\n", chip->cp_role);
		rc = -EINVAL;
		goto cleanup;
	}

	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't probe SMB1390 rc=%d\n", rc);
		goto cleanup;
	}
	smb1390_pmc1000h_detect(chip);

	INIT_DELAYED_WORK(&chip->reg_work, smb1390_reg_work);

	if (chip->cp_role == CP_MASTER)
		schedule_delayed_work(&chip->reg_work, 15 * HZ);
	else
		schedule_delayed_work(&chip->reg_work, 20 * HZ);

	pr_info("SMB1390 probed %s successfully\n", chip->cp_role ? "Master":"Slave");

	return 0;

cleanup:
	platform_set_drvdata(pdev, NULL);
	return rc;
}


static int smb1390_remove(struct platform_device *pdev)
{
	struct smb1390 *chip = platform_get_drvdata(pdev);

	if (chip->cp_role != CP_MASTER) {
		platform_set_drvdata(pdev, NULL);
		return 0;
	}

	class_unregister(&chip->cp_class);
	power_supply_unreg_notifier(&chip->nb);

	/* explicitly disable charging */
	vote(chip->disable_votable, USER_VOTER, true, 0);
	vote(chip->disable_votable, SOC_LEVEL_VOTER, true, 0);
	cancel_work(&chip->taper_work);
	cancel_work(&chip->status_change_work);
	wakeup_source_unregister(chip->cp_ws);
	smb1390_destroy_votables(chip);
	smb1390_release_channels(chip);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver smb1390_driver = {
	.driver	= {
		.name		= "qcom,smb1390-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe	= smb1390_probe,
	.remove	= smb1390_remove,
};
module_platform_driver(smb1390_driver);

MODULE_DESCRIPTION("SMB1390 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
