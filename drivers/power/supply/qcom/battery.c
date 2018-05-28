/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "QCOM-BATT: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/printk.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include <linux/workqueue.h>
#include "battery.h"

#define DRV_MAJOR_VERSION	1
#define DRV_MINOR_VERSION	0

#define CHG_STATE_VOTER			"CHG_STATE_VOTER"
#define TAPER_STEPPER_VOTER		"TAPER_STEPPER_VOTER"
#define TAPER_END_VOTER			"TAPER_END_VOTER"
#define PL_TAPER_EARLY_BAD_VOTER	"PL_TAPER_EARLY_BAD_VOTER"
#define PARALLEL_PSY_VOTER		"PARALLEL_PSY_VOTER"
#define PL_HW_ABSENT_VOTER		"PL_HW_ABSENT_VOTER"
#define PL_VOTER			"PL_VOTER"
#define RESTRICT_CHG_VOTER		"RESTRICT_CHG_VOTER"
#define ICL_CHANGE_VOTER		"ICL_CHANGE_VOTER"
#define PL_INDIRECT_VOTER		"PL_INDIRECT_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"
#define PL_FCC_LOW_VOTER		"PL_FCC_LOW_VOTER"
#define ICL_LIMIT_VOTER			"ICL_LIMIT_VOTER"
#define FCC_STEPPER_VOTER		"FCC_STEPPER_VOTER"

struct pl_data {
	int			pl_mode;
	int			pl_batfet_mode;
	int			pl_min_icl_ua;
	int			slave_pct;
	int			slave_fcc_ua;
	int			main_fcc_ua;
	int			restricted_current;
	bool			restricted_charging_enabled;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*pl_disable_votable;
	struct votable		*pl_awake_votable;
	struct votable		*hvdcp_hw_inov_dis_votable;
	struct votable		*usb_icl_votable;
	struct votable		*pl_enable_votable_indirect;
	struct delayed_work	status_change_work;
	struct work_struct	pl_disable_forever_work;
	struct work_struct	pl_taper_work;
	struct delayed_work	pl_awake_work;
	struct delayed_work	fcc_stepper_work;
	bool			taper_work_running;
	struct power_supply	*main_psy;
	struct power_supply	*pl_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	int			charge_type;
	int			total_settled_ua;
	int			pl_settled_ua;
	int			pl_fcc_max;
	int			fcc_stepper_enable;
	int			main_step_fcc_dir;
	int			main_step_fcc_count;
	int			main_step_fcc_residual;
	int			parallel_step_fcc_dir;
	int			parallel_step_fcc_count;
	int			parallel_step_fcc_residual;
	int			step_fcc;
	u32			wa_flags;
	struct class		qcom_batt_class;
	struct wakeup_source	*pl_ws;
	struct notifier_block	nb;
	bool			pl_disable;
	int			taper_entry_fv;
};

struct pl_data *the_chip;

enum print_reason {
	PR_PARALLEL	= BIT(0),
};

enum {
	AICL_RERUN_WA_BIT	= BIT(0),
	FORCE_INOV_DISABLE_BIT	= BIT(1),
};

static int debug_mask;
module_param_named(debug_mask, debug_mask, int, 0600);

#define pl_dbg(chip, reason, fmt, ...)				\
	do {								\
		if (debug_mask & (reason))				\
			pr_info(fmt, ##__VA_ARGS__);	\
		else							\
			pr_debug(fmt, ##__VA_ARGS__);		\
	} while (0)

#define IS_USBIN(mode)	((mode == POWER_SUPPLY_PL_USBIN_USBIN) \
			|| (mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT))
enum {
	VER = 0,
	SLAVE_PCT,
	RESTRICT_CHG_ENABLE,
	RESTRICT_CHG_CURRENT,
};

/*******
 * ICL *
 ********/
static int get_settled_split(struct pl_data *chip, int *main_icl_ua,
				int *slave_icl_ua, int *total_settled_icl_ua)
{
	int slave_icl_pct, total_current_ua;
	int slave_ua = 0, main_settled_ua = 0;
	union power_supply_propval pval = {0, };
	int rc, total_settled_ua = 0;

	if (!IS_USBIN(chip->pl_mode))
		return -EINVAL;

	if (!chip->main_psy)
		return -EINVAL;

	if (!get_effective_result_locked(chip->pl_disable_votable)) {
		/* read the aicl settled value */
		rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
		if (rc < 0) {
			pr_err("Couldn't get aicl settled value rc=%d\n", rc);
			return rc;
		}
		main_settled_ua = pval.intval;
		slave_icl_pct = max(0, chip->slave_pct);
		slave_ua = ((main_settled_ua + chip->pl_settled_ua)
						* slave_icl_pct) / 100;
		total_settled_ua = main_settled_ua + chip->pl_settled_ua;
	}

	total_current_ua = get_effective_result_locked(chip->usb_icl_votable);
	if (total_current_ua < 0) {
		if (!chip->usb_psy)
			chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			pr_err("Couldn't get usbpsy while splitting settled\n");
			return -ENOENT;
		}
		/* no client is voting, so get the total current from charger */
		rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_HW_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't get max current rc=%d\n", rc);
			return rc;
		}
		total_current_ua = pval.intval;
	}

	*main_icl_ua = total_current_ua - slave_ua;
	*slave_icl_ua = slave_ua;
	*total_settled_icl_ua = total_settled_ua;

	pl_dbg(chip, PR_PARALLEL,
		"Split total_current_ua=%d total_settled_ua=%d main_settled_ua=%d slave_ua=%d\n",
		total_current_ua, total_settled_ua, main_settled_ua, slave_ua);

	return 0;
}

static int validate_parallel_icl(struct pl_data *chip, bool *disable)
{
	int rc = 0;
	int main_ua = 0, slave_ua = 0, total_settled_ua = 0;

	if (!IS_USBIN(chip->pl_mode)
		|| get_effective_result_locked(chip->pl_disable_votable))
		return 0;

	rc = get_settled_split(chip, &main_ua, &slave_ua, &total_settled_ua);
	if (rc < 0) {
		pr_err("Couldn't  get split current rc=%d\n", rc);
		return rc;
	}

	if (slave_ua < chip->pl_min_icl_ua)
		*disable = true;
	else
		*disable = false;

	return 0;
}

static void split_settled(struct pl_data *chip)
{
	union power_supply_propval pval = {0, };
	int rc, main_ua, slave_ua, total_settled_ua;

	rc = get_settled_split(chip, &main_ua, &slave_ua, &total_settled_ua);
	if (rc < 0) {
		pr_err("Couldn't  get split current rc=%d\n", rc);
		return;
	}

	/*
	 * If there is an increase in slave share
	 * (Also handles parallel enable case)
	 *	Set Main ICL then slave ICL
	 * else
	 * (Also handles parallel disable case)
	 *	Set slave ICL then main ICL.
	 */
	if (slave_ua > chip->pl_settled_ua) {
		pval.intval = main_ua;
		/* Set ICL on main charger */
		rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't change slave suspend state rc=%d\n",
					rc);
			return;
		}

		/* set parallel's ICL  could be 0mA when pl is disabled */
		pval.intval = slave_ua;
		rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set parallel icl, rc=%d\n", rc);
			return;
		}
	} else {
		/* set parallel's ICL  could be 0mA when pl is disabled */
		pval.intval = slave_ua;
		rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set parallel icl, rc=%d\n", rc);
			return;
		}

		pval.intval = main_ua;
		/* Set ICL on main charger */
		rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't change slave suspend state rc=%d\n",
					rc);
			return;
		}
	}

	chip->total_settled_ua = total_settled_ua;
	chip->pl_settled_ua = slave_ua;
}

static ssize_t version_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d.%d\n",
			DRV_MAJOR_VERSION, DRV_MINOR_VERSION);
}

/*************
 * SLAVE PCT *
 **************/
static ssize_t slave_pct_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", chip->slave_pct);
}

static ssize_t slave_pct_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct pl_data *chip = container_of(c, struct pl_data, qcom_batt_class);
	int rc;
	unsigned long val;
	bool disable = false;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	chip->slave_pct = val;

	rc = validate_parallel_icl(chip, &disable);
	if (rc < 0)
		return rc;

	vote(chip->pl_disable_votable, ICL_LIMIT_VOTER, disable, 0);
	rerun_election(chip->fcc_votable);
	rerun_election(chip->fv_votable);
	split_settled(chip);

	return count;
}

/************************
 * RESTRICTED CHARGIGNG *
 ************************/
static ssize_t restrict_chg_show(struct class *c, struct class_attribute *attr,
		char *ubuf)
{
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);

	return snprintf(ubuf, PAGE_SIZE, "%d\n",
			chip->restricted_charging_enabled);
}

static ssize_t restrict_chg_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);
	unsigned long val;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	if (chip->restricted_charging_enabled == !!val)
		goto no_change;

	chip->restricted_charging_enabled = !!val;

	/* disable parallel charger in case of restricted charging */
	vote(chip->pl_disable_votable, RESTRICT_CHG_VOTER,
				chip->restricted_charging_enabled, 0);

	vote(chip->fcc_votable, RESTRICT_CHG_VOTER,
				chip->restricted_charging_enabled,
				chip->restricted_current);

no_change:
	return count;
}

static ssize_t restrict_cur_show(struct class *c, struct class_attribute *attr,
			char *ubuf)
{
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", chip->restricted_current);
}

static ssize_t restrict_cur_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);
	unsigned long val;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	chip->restricted_current = val;

	vote(chip->fcc_votable, RESTRICT_CHG_VOTER,
				chip->restricted_charging_enabled,
				chip->restricted_current);

	return count;
}

static struct class_attribute pl_attributes[] = {
	[VER]			= __ATTR_RO(version),
	[SLAVE_PCT]		= __ATTR(parallel_pct, 0644,
					slave_pct_show, slave_pct_store),
	[RESTRICT_CHG_ENABLE]	= __ATTR(restricted_charging, 0644,
					restrict_chg_show, restrict_chg_store),
	[RESTRICT_CHG_CURRENT]	= __ATTR(restricted_current, 0644,
					restrict_cur_show, restrict_cur_store),
	__ATTR_NULL,
};

/*********
 *  FCC  *
 **********/
#define EFFICIENCY_PCT	80
#define FCC_STEP_SIZE_UA 100000
#define FCC_STEP_UPDATE_DELAY_MS 1000
#define STEP_UP 1
#define STEP_DOWN -1
static void get_fcc_split(struct pl_data *chip, int total_ua,
			int *master_ua, int *slave_ua)
{
	int rc, effective_total_ua, slave_limited_ua, hw_cc_delta_ua = 0,
		icl_ua, adapter_uv, bcl_ua;
	union power_supply_propval pval = {0, };

	rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_FCC_DELTA, &pval);
	if (rc < 0)
		hw_cc_delta_ua = 0;
	else
		hw_cc_delta_ua = pval.intval;

	bcl_ua = INT_MAX;
	if (chip->pl_mode == POWER_SUPPLY_PL_USBMID_USBMID) {
		rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
		if (rc < 0) {
			pr_err("Couldn't get aicl settled value rc=%d\n", rc);
			return;
		}
		icl_ua = pval.intval;

		rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED, &pval);
		if (rc < 0) {
			pr_err("Couldn't get adaptive voltage rc=%d\n", rc);
			return;
		}
		adapter_uv = pval.intval;

		bcl_ua = div64_s64((s64)icl_ua * adapter_uv * EFFICIENCY_PCT,
			(s64)get_effective_result(chip->fv_votable) * 100);
	}

	effective_total_ua = max(0, total_ua + hw_cc_delta_ua);
	slave_limited_ua = min(effective_total_ua, bcl_ua);
	*slave_ua = (slave_limited_ua * chip->slave_pct) / 100;
	*slave_ua = min(*slave_ua, chip->pl_fcc_max);

	/*
	 * In stacked BATFET configuration charger's current goes
	 * through main charger's BATFET, keep the main charger's FCC
	 * to the votable result.
	 */
	if (chip->pl_batfet_mode == POWER_SUPPLY_PL_STACKED_BATFET)
		*master_ua = max(0, total_ua);
	else
		*master_ua = max(0, total_ua - *slave_ua);
}

static void get_fcc_stepper_params(struct pl_data *chip, int main_fcc_ua,
			int parallel_fcc_ua)
{
	union power_supply_propval pval = {0, };
	int rc;

	/* Read current FCC of main charger */
	rc = power_supply_get_property(chip->main_psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	if (rc < 0) {
		pr_err("Couldn't get main charger current fcc, rc=%d\n", rc);
		return;
	}
	chip->main_fcc_ua = pval.intval;

	chip->main_step_fcc_dir = (main_fcc_ua > pval.intval) ?
				STEP_UP : STEP_DOWN;
	chip->main_step_fcc_count = abs((main_fcc_ua - pval.intval) /
				FCC_STEP_SIZE_UA);
	chip->main_step_fcc_residual = (main_fcc_ua - pval.intval) %
				FCC_STEP_SIZE_UA;

	chip->parallel_step_fcc_dir = (parallel_fcc_ua > chip->slave_fcc_ua) ?
				STEP_UP : STEP_DOWN;
	chip->parallel_step_fcc_count = abs((parallel_fcc_ua -
				chip->slave_fcc_ua) / FCC_STEP_SIZE_UA);
	chip->parallel_step_fcc_residual = (parallel_fcc_ua -
				chip->slave_fcc_ua) % FCC_STEP_SIZE_UA;

	if (chip->parallel_step_fcc_count || chip->parallel_step_fcc_residual
		|| chip->main_step_fcc_count || chip->main_step_fcc_residual)
		chip->step_fcc = 1;

	pr_debug("Main FCC Stepper parameters: main_step_direction: %d, main_step_count: %d, main_residual_fcc: %d\n",
		chip->main_step_fcc_dir, chip->main_step_fcc_count,
		chip->main_step_fcc_residual);
	pr_debug("Parallel FCC Stepper parameters: parallel_step_direction: %d, parallel_step_count: %d, parallel_residual_fcc: %d\n",
		chip->parallel_step_fcc_dir, chip->parallel_step_fcc_count,
		chip->parallel_step_fcc_residual);
}

#define MINIMUM_PARALLEL_FCC_UA		500000
#define PL_TAPER_WORK_DELAY_MS		500
#define TAPER_RESIDUAL_PCT		90
#define TAPER_REDUCTION_UA		200000
static void pl_taper_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work, struct pl_data,
						pl_taper_work);
	union power_supply_propval pval = {0, };
	int rc;
	int eff_fcc_ua;
	int total_fcc_ua, master_fcc_ua, slave_fcc_ua = 0;

	chip->taper_entry_fv = get_effective_result(chip->fv_votable);
	chip->taper_work_running = true;
	while (true) {
		if (get_effective_result(chip->pl_disable_votable)) {
			/*
			 * if parallel's FCC share is low, simply disable
			 * parallel with TAPER_END_VOTER
			 */
			total_fcc_ua = get_effective_result_locked(
					chip->fcc_votable);
			get_fcc_split(chip, total_fcc_ua, &master_fcc_ua,
					&slave_fcc_ua);
			if (slave_fcc_ua <= MINIMUM_PARALLEL_FCC_UA) {
				pl_dbg(chip, PR_PARALLEL, "terminating: parallel's share is low\n");
				vote(chip->pl_disable_votable, TAPER_END_VOTER,
						true, 0);
			} else {
				pl_dbg(chip, PR_PARALLEL, "terminating: parallel disabled\n");
			}
			goto done;
		}

		rc = power_supply_get_property(chip->batt_psy,
				       POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt charge type rc=%d\n", rc);
			goto done;
		}

		chip->charge_type = pval.intval;
		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			eff_fcc_ua = get_effective_result(chip->fcc_votable);
			if (eff_fcc_ua < 0) {
				pr_err("Couldn't get fcc, exiting taper work\n");
				goto done;
			}
			eff_fcc_ua = eff_fcc_ua - TAPER_REDUCTION_UA;
			if (eff_fcc_ua < 0) {
				pr_err("Can't reduce FCC any more\n");
				goto done;
			}

			pl_dbg(chip, PR_PARALLEL, "master is taper charging; reducing FCC to %dua\n",
					eff_fcc_ua);
			vote(chip->fcc_votable, TAPER_STEPPER_VOTER,
					true, eff_fcc_ua);
		} else {
			/*
			 * Due to reduction of float voltage in JEITA condition
			 * taper charging can be initiated at a lower FV. On
			 * removal of JEITA condition, FV readjusts itself.
			 * However, once taper charging is initiated, it doesn't
			 * exits until parallel chaging is disabled due to which
			 * FCC doesn't scale back to its original value, leading
			 * to slow charging thereafter.
			 * Check if FV increases in comparison to FV at which
			 * taper charging was initiated, and if yes, exit taper
			 * charging.
			 */
			if (get_effective_result(chip->fv_votable) >
						chip->taper_entry_fv) {
				pl_dbg(chip, PR_PARALLEL, "Float voltage increased. Exiting taper\n");
				goto done;
			} else {
				pl_dbg(chip, PR_PARALLEL, "master is fast charging; waiting for next taper\n");
			}
		}
		/* wait for the charger state to deglitch after FCC change */
		msleep(PL_TAPER_WORK_DELAY_MS);
	}
done:
	chip->taper_work_running = false;
	vote(chip->fcc_votable, TAPER_STEPPER_VOTER, false, 0);
	vote(chip->pl_awake_votable, TAPER_END_VOTER, false, 0);
}

static int pl_fcc_vote_callback(struct votable *votable, void *data,
			int total_fcc_ua, const char *client)
{
	struct pl_data *chip = data;
	int master_fcc_ua = total_fcc_ua, slave_fcc_ua = 0;

	if (total_fcc_ua < 0)
		return 0;

	if (!chip->main_psy)
		return 0;

	if (chip->pl_mode != POWER_SUPPLY_PL_NONE) {
		get_fcc_split(chip, total_fcc_ua, &master_fcc_ua,
				&slave_fcc_ua);

		if (slave_fcc_ua > MINIMUM_PARALLEL_FCC_UA) {
			vote(chip->pl_disable_votable, PL_FCC_LOW_VOTER,
							false, 0);
		} else {
			vote(chip->pl_disable_votable, PL_FCC_LOW_VOTER,
							true, 0);
		}
	}

	rerun_election(chip->pl_disable_votable);

	return 0;
}

static void fcc_stepper_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work, struct pl_data,
			fcc_stepper_work.work);
	union power_supply_propval pval = {0, };
	int reschedule_ms = 0, rc = 0, charger_present = 0;
	int main_fcc = chip->main_fcc_ua;
	int parallel_fcc = chip->slave_fcc_ua;

	/* Check whether USB is present or not */
	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0)
		pr_err("Couldn't get USB Present status, rc=%d\n", rc);

	charger_present = pval.intval;

	/*Check whether DC charger is present or not */
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");
	if (chip->dc_psy) {
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			pr_err("Couldn't get DC Present status, rc=%d\n", rc);

		charger_present |= pval.intval;
	}

	/*
	 * If USB is not present, then set parallel FCC to min value and
	 * main FCC to the effective value of FCC votable and exit.
	 */
	if (!charger_present) {
		/* Disable parallel */
		parallel_fcc = 0;

		if (chip->pl_psy) {
			pval.intval = 1;
			rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
			if (rc < 0) {
				pr_err("Couldn't change slave suspend state rc=%d\n",
					rc);
				goto out;
			}

			chip->pl_disable = true;
			power_supply_changed(chip->pl_psy);
		}

		main_fcc = get_effective_result_locked(chip->fcc_votable);
		pval.intval = main_fcc;
		rc = power_supply_set_property(chip->main_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set main charger fcc, rc=%d\n", rc);
			goto out;
		}

		goto stepper_exit;
	}

	if (chip->main_step_fcc_count) {
		main_fcc += (FCC_STEP_SIZE_UA * chip->main_step_fcc_dir);
		chip->main_step_fcc_count--;
		reschedule_ms = FCC_STEP_UPDATE_DELAY_MS;
	} else if (chip->main_step_fcc_residual) {
		main_fcc += chip->main_step_fcc_residual;
		chip->main_step_fcc_residual = 0;
	}

	if (chip->parallel_step_fcc_count) {
		parallel_fcc += (FCC_STEP_SIZE_UA *
			chip->parallel_step_fcc_dir);
		chip->parallel_step_fcc_count--;
		reschedule_ms = FCC_STEP_UPDATE_DELAY_MS;
	} else if (chip->parallel_step_fcc_residual) {
		parallel_fcc += chip->parallel_step_fcc_residual;
		chip->parallel_step_fcc_residual = 0;
	}

	if (parallel_fcc < chip->slave_fcc_ua) {
		/* Set parallel FCC */
		if (chip->pl_psy && !chip->pl_disable) {
			if (parallel_fcc < MINIMUM_PARALLEL_FCC_UA) {
				pval.intval = 1;
				rc = power_supply_set_property(chip->pl_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
				if (rc < 0) {
					pr_err("Couldn't change slave suspend state rc=%d\n",
						rc);
					goto out;
				}

				if (IS_USBIN(chip->pl_mode))
					split_settled(chip);

				parallel_fcc = 0;
				chip->parallel_step_fcc_count = 0;
				chip->parallel_step_fcc_residual = 0;
				chip->total_settled_ua = 0;
				chip->pl_settled_ua = 0;
				chip->pl_disable = true;
				power_supply_changed(chip->pl_psy);
			} else {
				/* Set Parallel FCC */
				pval.intval = parallel_fcc;
				rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
					&pval);
				if (rc < 0) {
					pr_err("Couldn't set parallel charger fcc, rc=%d\n",
						rc);
					goto out;
				}
			}
		}

		/* Set main FCC */
		pval.intval = main_fcc;
		rc = power_supply_set_property(chip->main_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set main charger fcc, rc=%d\n", rc);
			goto out;
		}
	} else {
		/* Set main FCC */
		pval.intval = main_fcc;
		rc = power_supply_set_property(chip->main_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set main charger fcc, rc=%d\n", rc);
			goto out;
		}

		/* Set parallel FCC */
		if (chip->pl_psy) {
			pval.intval = parallel_fcc;
			rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
			if (rc < 0) {
				pr_err("Couldn't set parallel charger fcc, rc=%d\n",
					rc);
				goto out;
			}

			/*
			 * Enable parallel charger only if it was disabled
			 * earlier and configured slave fcc is greater than or
			 * equal to minimum parallel FCC value.
			 */
			if (chip->pl_disable && parallel_fcc
					>= MINIMUM_PARALLEL_FCC_UA) {
				pval.intval = 0;
				rc = power_supply_set_property(chip->pl_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
				if (rc < 0) {
					pr_err("Couldn't change slave suspend state rc=%d\n",
						rc);
					goto out;
				}

				if (IS_USBIN(chip->pl_mode))
					split_settled(chip);

				chip->pl_disable = false;
				power_supply_changed(chip->pl_psy);
			}
		}
	}

stepper_exit:
	chip->main_fcc_ua = main_fcc;
	chip->slave_fcc_ua = parallel_fcc;

	if (reschedule_ms) {
		schedule_delayed_work(&chip->fcc_stepper_work,
				msecs_to_jiffies(reschedule_ms));
		pr_debug("Rescheduling FCC_STEPPER work\n");
		return;
	}
out:
	chip->step_fcc = 0;
	vote(chip->pl_awake_votable, FCC_STEPPER_VOTER, false, 0);
}

#define PARALLEL_FLOAT_VOLTAGE_DELTA_UV 50000
static int pl_fv_vote_callback(struct votable *votable, void *data,
			int fv_uv, const char *client)
{
	struct pl_data *chip = data;
	union power_supply_propval pval = {0, };
	int rc = 0;

	if (fv_uv < 0)
		return 0;

	if (!chip->main_psy)
		return 0;

	pval.intval = fv_uv;

	rc = power_supply_set_property(chip->main_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc < 0) {
		pr_err("Couldn't set main fv, rc=%d\n", rc);
		return rc;
	}

	if (chip->pl_mode != POWER_SUPPLY_PL_NONE) {
		pval.intval += PARALLEL_FLOAT_VOLTAGE_DELTA_UV;
		rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't set float on parallel rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

#define ICL_STEP_UA	25000
#define PL_DELAY_MS     3000
static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ua, const char *client)
{
	int rc;
	struct pl_data *chip = data;
	union power_supply_propval pval = {0, };
	bool rerun_aicl = false;

	if (!chip->main_psy)
		return 0;

	if (client == NULL)
		icl_ua = INT_MAX;

	/*
	 * Disable parallel for new ICL vote - the call to split_settled will
	 * ensure that all the input current limit gets assigned to the main
	 * charger.
	 */
	vote(chip->pl_disable_votable, ICL_CHANGE_VOTER, true, 0);

	/*
	 * if (ICL < 1400)
	 *	disable parallel charger using USBIN_I_VOTER
	 * else
	 *	instead of re-enabling here rely on status_changed_work
	 *	(triggered via AICL completed or scheduled from here to
	 *	unvote USBIN_I_VOTER) the status_changed_work enables
	 *	USBIN_I_VOTER based on settled current.
	 */
	if (icl_ua <= 1400000)
		vote(chip->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	else
		schedule_delayed_work(&chip->status_change_work,
						msecs_to_jiffies(PL_DELAY_MS));

	/* rerun AICL */
	/* get the settled current */
	rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
			       &pval);
	if (rc < 0) {
		pr_err("Couldn't get aicl settled value rc=%d\n", rc);
		return rc;
	}

	/* rerun AICL if new ICL is above settled ICL */
	if (icl_ua > pval.intval)
		rerun_aicl = true;

	if (rerun_aicl && (chip->wa_flags & AICL_RERUN_WA_BIT)) {
		/* set a lower ICL */
		pval.intval = max(pval.intval - ICL_STEP_UA, ICL_STEP_UA);
		power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX,
				&pval);
	}

	/* set the effective ICL */
	pval.intval = icl_ua;
	power_supply_set_property(chip->main_psy,
			POWER_SUPPLY_PROP_CURRENT_MAX,
			&pval);

	vote(chip->pl_disable_votable, ICL_CHANGE_VOTER, false, 0);

	return 0;
}

static void pl_disable_forever_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work,
			struct pl_data, pl_disable_forever_work);

	/* Disable Parallel charger forever */
	vote(chip->pl_disable_votable, PL_HW_ABSENT_VOTER, true, 0);

	/* Re-enable autonomous mode */
	if (chip->hvdcp_hw_inov_dis_votable)
		vote(chip->hvdcp_hw_inov_dis_votable, PL_VOTER, false, 0);
}

static void pl_awake_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work,
			struct pl_data, pl_awake_work.work);

	vote(chip->pl_awake_votable, PL_VOTER, false, 0);
}

static bool is_main_available(struct pl_data *chip)
{
	if (chip->main_psy)
		return true;

	chip->main_psy = power_supply_get_by_name("main");

	return !!chip->main_psy;
}

static bool is_batt_available(struct pl_data *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static int pl_disable_vote_callback(struct votable *votable,
		void *data, int pl_disable, const char *client)
{
	struct pl_data *chip = data;
	union power_supply_propval pval = {0, };
	int master_fcc_ua = 0, total_fcc_ua = 0, slave_fcc_ua = 0;
	int rc = 0;
	bool disable = false;

	if (!is_main_available(chip))
		return -ENODEV;

	if (!is_batt_available(chip))
		return -ENODEV;

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_err("Couldn't get usb psy\n");
		return -ENODEV;
	}

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE, &pval);
	if (rc < 0) {
		pr_err("Couldn't read FCC step update status, rc=%d\n", rc);
		return rc;
	}
	chip->fcc_stepper_enable = pval.intval;
	pr_debug("FCC Stepper %s\n", pval.intval ? "enabled" : "disabled");

	if (chip->fcc_stepper_enable) {
		cancel_delayed_work_sync(&chip->fcc_stepper_work);
		vote(chip->pl_awake_votable, FCC_STEPPER_VOTER, false, 0);
	}

	total_fcc_ua = get_effective_result_locked(chip->fcc_votable);

	if (chip->pl_mode != POWER_SUPPLY_PL_NONE && !pl_disable) {
		/* keep system awake to talk to slave charger through i2c */
		cancel_delayed_work_sync(&chip->pl_awake_work);
		vote(chip->pl_awake_votable, PL_VOTER, true, 0);

		rc = validate_parallel_icl(chip, &disable);
		if (rc < 0)
			return rc;

		if (disable) {
			pr_info("Parallel ICL is less than min ICL(%d), skipping parallel enable\n",
					chip->pl_min_icl_ua);
			return 0;
		}

		 /* enable parallel charging */
		rc = power_supply_get_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc == -ENODEV) {
			/*
			 * -ENODEV is returned only if parallel chip
			 * is not present in the system.
			 * Disable parallel charger forever.
			 */
			schedule_work(&chip->pl_disable_forever_work);
			return rc;
		}

		rerun_election(chip->fv_votable);

		get_fcc_split(chip, total_fcc_ua, &master_fcc_ua,
				&slave_fcc_ua);

		if (chip->fcc_stepper_enable) {
			get_fcc_stepper_params(chip, master_fcc_ua,
					slave_fcc_ua);
			if (chip->step_fcc) {
				vote(chip->pl_awake_votable, FCC_STEPPER_VOTER,
					true, 0);
				schedule_delayed_work(&chip->fcc_stepper_work,
					0);
				}
		} else {
			/*
			 * If there is an increase in slave share
			 * (Also handles parallel enable case)
			 *	Set Main ICL then slave FCC
			 * else
			 * (Also handles parallel disable case)
			 *	Set slave ICL then main FCC.
			 */
			if (slave_fcc_ua > chip->slave_fcc_ua) {
				pval.intval = master_fcc_ua;
				rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
					&pval);
				if (rc < 0) {
					pr_err("Could not set main fcc, rc=%d\n",
						rc);
					return rc;
				}

				pval.intval = slave_fcc_ua;
				rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
					&pval);
				if (rc < 0) {
					pr_err("Couldn't set parallel fcc, rc=%d\n",
						rc);
					return rc;
				}

				chip->slave_fcc_ua = slave_fcc_ua;
			} else {
				pval.intval = slave_fcc_ua;
				rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
					&pval);
				if (rc < 0) {
					pr_err("Couldn't set parallel fcc, rc=%d\n",
						rc);
					return rc;
				}

				chip->slave_fcc_ua = slave_fcc_ua;

				pval.intval = master_fcc_ua;
				rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
					&pval);
				if (rc < 0) {
					pr_err("Could not set main fcc, rc=%d\n",
						rc);
					return rc;
				}
			}

			/*
			 * Enable will be called with a valid pl_psy always. The
			 * PARALLEL_PSY_VOTER keeps it disabled unless a pl_psy
			 * is seen.
			 */
			pval.intval = 0;
			rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
			if (rc < 0)
				pr_err("Couldn't change slave suspend state rc=%d\n",
					rc);

			if (IS_USBIN(chip->pl_mode))
				split_settled(chip);
		}

		/*
		 * we could have been enabled while in taper mode,
		 *  start the taper work if so
		 */
		rc = power_supply_get_property(chip->batt_psy,
				       POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt charge type rc=%d\n", rc);
		} else {
			if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER
				&& !chip->taper_work_running) {
				pl_dbg(chip, PR_PARALLEL,
					"pl enabled in Taper scheduing work\n");
				vote(chip->pl_awake_votable, TAPER_END_VOTER,
						true, 0);
				queue_work(system_long_wq,
						&chip->pl_taper_work);
			}
		}

		pl_dbg(chip, PR_PARALLEL, "master_fcc=%d slave_fcc=%d distribution=(%d/%d)\n",
			master_fcc_ua, slave_fcc_ua,
			(master_fcc_ua * 100) / total_fcc_ua,
			(slave_fcc_ua * 100) / total_fcc_ua);
	} else {
		if (!chip->fcc_stepper_enable) {
			if (IS_USBIN(chip->pl_mode))
				split_settled(chip);

			/* pl_psy may be NULL while in the disable branch */
			if (chip->pl_psy) {
				pval.intval = 1;
				rc = power_supply_set_property(chip->pl_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
				if (rc < 0)
					pr_err("Couldn't change slave suspend state rc=%d\n",
						rc);
			}

			/* main psy gets all share */
			pval.intval = total_fcc_ua;
			rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
			if (rc < 0) {
				pr_err("Could not set main fcc, rc=%d\n", rc);
				return rc;
			}

			/* reset parallel FCC */
			chip->slave_fcc_ua = 0;
			chip->total_settled_ua = 0;
			chip->pl_settled_ua = 0;
		} else {
			get_fcc_stepper_params(chip, total_fcc_ua, 0);
			if (chip->step_fcc) {
				vote(chip->pl_awake_votable, FCC_STEPPER_VOTER,
					true, 0);
				schedule_delayed_work(&chip->fcc_stepper_work,
					0);
			}
		}

		rerun_election(chip->fv_votable);

		cancel_delayed_work_sync(&chip->pl_awake_work);
		schedule_delayed_work(&chip->pl_awake_work,
						msecs_to_jiffies(5000));
	}

	/* notify parallel state change */
	if (chip->pl_psy && (chip->pl_disable != pl_disable)
				&& !chip->fcc_stepper_enable) {
		power_supply_changed(chip->pl_psy);
		chip->pl_disable = (bool)pl_disable;
	}

	pl_dbg(chip, PR_PARALLEL, "parallel charging %s\n",
		   pl_disable ? "disabled" : "enabled");

	return 0;
}

static int pl_enable_indirect_vote_callback(struct votable *votable,
			void *data, int pl_enable, const char *client)
{
	struct pl_data *chip = data;

	vote(chip->pl_disable_votable, PL_INDIRECT_VOTER, !pl_enable, 0);

	return 0;
}

static int pl_awake_vote_callback(struct votable *votable,
			void *data, int awake, const char *client)
{
	struct pl_data *chip = data;

	if (awake)
		__pm_stay_awake(chip->pl_ws);
	else
		__pm_relax(chip->pl_ws);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static bool is_parallel_available(struct pl_data *chip)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (chip->pl_psy)
		return true;

	chip->pl_psy = power_supply_get_by_name("parallel");
	if (!chip->pl_psy)
		return false;

	rc = power_supply_get_property(chip->pl_psy,
			       POWER_SUPPLY_PROP_PARALLEL_MODE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get parallel mode from parallel rc=%d\n",
				rc);
		return false;
	}
	/*
	 * Note that pl_mode will be updated to anything other than a _NONE
	 * only after pl_psy is found. IOW pl_mode != _NONE implies that
	 * pl_psy is present and valid.
	 */
	chip->pl_mode = pval.intval;

	/* Disable autonomous votage increments for USBIN-USBIN */
	if (IS_USBIN(chip->pl_mode)
			&& (chip->wa_flags & FORCE_INOV_DISABLE_BIT)) {
		if (!chip->hvdcp_hw_inov_dis_votable)
			chip->hvdcp_hw_inov_dis_votable =
					find_votable("HVDCP_HW_INOV_DIS");
		if (chip->hvdcp_hw_inov_dis_votable)
			/* Read current pulse count */
			vote(chip->hvdcp_hw_inov_dis_votable, PL_VOTER,
					true, 0);
		else
			return false;
	}

	rc = power_supply_get_property(chip->pl_psy,
		       POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get parallel batfet mode rc=%d\n",
				rc);
		return false;
	}
	chip->pl_batfet_mode = pval.intval;

	pval.intval = 0;
	power_supply_get_property(chip->pl_psy, POWER_SUPPLY_PROP_MIN_ICL,
					&pval);
	chip->pl_min_icl_ua = pval.intval;

	chip->pl_fcc_max = INT_MAX;
	rc = power_supply_get_property(chip->pl_psy,
			POWER_SUPPLY_PROP_PARALLEL_FCC_MAX, &pval);
	if (!rc)
		chip->pl_fcc_max = pval.intval;

	vote(chip->pl_disable_votable, PARALLEL_PSY_VOTER, false, 0);

	return true;
}

static void handle_main_charge_type(struct pl_data *chip)
{
	union power_supply_propval pval = {0, };
	int rc;

	rc = power_supply_get_property(chip->batt_psy,
			       POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return;
	}

	/* not fast/not taper state to disables parallel */
	if ((pval.intval != POWER_SUPPLY_CHARGE_TYPE_FAST)
		&& (pval.intval != POWER_SUPPLY_CHARGE_TYPE_TAPER)) {
		vote(chip->pl_disable_votable, CHG_STATE_VOTER, true, 0);
		chip->charge_type = pval.intval;
		return;
	}

	/* handle taper charge entry */
	if (chip->charge_type == POWER_SUPPLY_CHARGE_TYPE_FAST
		&& (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER)) {
		chip->charge_type = pval.intval;
		if (!chip->taper_work_running) {
			pl_dbg(chip, PR_PARALLEL, "taper entry scheduling work\n");
			vote(chip->pl_awake_votable, TAPER_END_VOTER, true, 0);
			queue_work(system_long_wq, &chip->pl_taper_work);
		}
		return;
	}

	/* handle fast/taper charge entry */
	if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER
			|| pval.intval == POWER_SUPPLY_CHARGE_TYPE_FAST) {
		/*
		 * Undo parallel charging termination if entered taper in
		 * reduced float voltage condition due to jeita mitigation.
		 */
		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_FAST &&
			(chip->taper_entry_fv <
			get_effective_result(chip->fv_votable))) {
			vote(chip->pl_disable_votable, TAPER_END_VOTER,
				false, 0);
		}
		pl_dbg(chip, PR_PARALLEL, "chg_state enabling parallel\n");
		vote(chip->pl_disable_votable, CHG_STATE_VOTER, false, 0);
		chip->charge_type = pval.intval;
		return;
	}

	/* remember the new state only if it isn't any of the above */
	chip->charge_type = pval.intval;
}

#define MIN_ICL_CHANGE_DELTA_UA		300000
static void handle_settled_icl_change(struct pl_data *chip)
{
	union power_supply_propval pval = {0, };
	int new_total_settled_ua;
	int rc;
	int main_settled_ua;
	int main_limited;
	int total_current_ua;
	bool disable = false;

	total_current_ua = get_effective_result_locked(chip->usb_icl_votable);

	/*
	 * call aicl split only when USBIN_USBIN and enabled
	 * and if aicl changed
	 */
	rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
			       &pval);
	if (rc < 0) {
		pr_err("Couldn't get aicl settled value rc=%d\n", rc);
		return;
	}
	main_settled_ua = pval.intval;

	rc = power_supply_get_property(chip->batt_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
			       &pval);
	if (rc < 0) {
		pr_err("Couldn't get aicl settled value rc=%d\n", rc);
		return;
	}
	main_limited = pval.intval;

	if ((main_limited && (main_settled_ua + chip->pl_settled_ua) < 1400000)
			|| (main_settled_ua == 0)
			|| ((total_current_ua >= 0) &&
				(total_current_ua <= 1400000)))
		vote(chip->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	else
		vote(chip->pl_enable_votable_indirect, USBIN_I_VOTER, true, 0);

	rerun_election(chip->fcc_votable);

	if (IS_USBIN(chip->pl_mode)) {
		/*
		 * call aicl split only when USBIN_USBIN and enabled
		 * and if settled current has changed by more than 300mA
		 */

		new_total_settled_ua = main_settled_ua + chip->pl_settled_ua;
		pl_dbg(chip, PR_PARALLEL,
			"total_settled_ua=%d settled_ua=%d new_total_settled_ua=%d\n",
			chip->total_settled_ua, pval.intval,
			new_total_settled_ua);

		/* If ICL change is small skip splitting */
		if (abs(new_total_settled_ua - chip->total_settled_ua)
						> MIN_ICL_CHANGE_DELTA_UA) {
			rc = validate_parallel_icl(chip, &disable);
			if (rc < 0)
				return;

			vote(chip->pl_disable_votable, ICL_LIMIT_VOTER,
						disable, 0);
			if (!get_effective_result_locked(
						chip->pl_disable_votable))
				split_settled(chip);
		}
	}
}

static void handle_parallel_in_taper(struct pl_data *chip)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (get_effective_result_locked(chip->pl_disable_votable))
		return;

	if (!chip->pl_psy)
		return;

	rc = power_supply_get_property(chip->pl_psy,
			       POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (rc < 0) {
		pr_err("Couldn't get pl charge type rc=%d\n", rc);
		return;
	}

	/*
	 * if parallel is seen in taper mode ever, that is an anomaly and
	 * we disable parallel charger
	 */
	if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		vote(chip->pl_disable_votable, PL_TAPER_EARLY_BAD_VOTER,
				true, 0);
		return;
	}
}

static void handle_usb_change(struct pl_data *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_err("Couldn't get usbpsy\n");
		return;
	}

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Couldn't get present from USB rc=%d\n", rc);
		return;
	}

	if (!pval.intval) {
		/* USB removed: remove all stale votes */
		vote(chip->pl_disable_votable, TAPER_END_VOTER, false, 0);
		vote(chip->pl_disable_votable, PL_TAPER_EARLY_BAD_VOTER,
				false, 0);
		vote(chip->pl_disable_votable, ICL_LIMIT_VOTER, false, 0);
	}
}

static void status_change_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work,
			struct pl_data, status_change_work.work);

	if (!chip->main_psy && is_main_available(chip)) {
		/*
		 * re-run election for FCC/FV/ICL once main_psy
		 * is available to ensure all votes are reflected
		 * on hardware
		 */
		rerun_election(chip->usb_icl_votable);
		rerun_election(chip->fcc_votable);
		rerun_election(chip->fv_votable);
	}

	if (!chip->main_psy)
		return;

	if (!is_batt_available(chip))
		return;

	is_parallel_available(chip);

	handle_usb_change(chip);
	handle_main_charge_type(chip);
	handle_settled_icl_change(chip);
	handle_parallel_in_taper(chip);
}

static int pl_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct pl_data *chip = container_of(nb, struct pl_data, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "parallel") == 0)
	    || (strcmp(psy->desc->name, "battery") == 0)
	    || (strcmp(psy->desc->name, "main") == 0))
		schedule_delayed_work(&chip->status_change_work, 0);

	return NOTIFY_OK;
}

static int pl_register_notifier(struct pl_data *chip)
{
	int rc;

	chip->nb.notifier_call = pl_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int pl_determine_initial_status(struct pl_data *chip)
{
	status_change_work(&chip->status_change_work.work);
	return 0;
}

static void pl_config_init(struct pl_data *chip, int smb_version)
{
	switch (smb_version) {
	case PMI8998_SUBTYPE:
	case PM660_SUBTYPE:
		chip->wa_flags = AICL_RERUN_WA_BIT | FORCE_INOV_DISABLE_BIT;
		break;
	case PMI632_SUBTYPE:
		break;
	default:
		break;
	}
}

#define DEFAULT_RESTRICTED_CURRENT_UA	1000000
int qcom_batt_init(int smb_version)
{
	struct pl_data *chip;
	int rc = 0;

	/* initialize just once */
	if (the_chip) {
		pr_err("was initialized earlier. Failing now\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->slave_pct = 50;
	pl_config_init(chip, smb_version);
	chip->restricted_current = DEFAULT_RESTRICTED_CURRENT_UA;

	chip->pl_ws = wakeup_source_register("qcom-battery");
	if (!chip->pl_ws)
		goto cleanup;

	chip->fcc_votable = create_votable("FCC", VOTE_MIN,
					pl_fcc_vote_callback,
					chip);
	if (IS_ERR(chip->fcc_votable)) {
		rc = PTR_ERR(chip->fcc_votable);
		goto release_wakeup_source;
	}

	chip->fv_votable = create_votable("FV", VOTE_MIN,
					pl_fv_vote_callback,
					chip);
	if (IS_ERR(chip->fv_votable)) {
		rc = PTR_ERR(chip->fv_votable);
		goto destroy_votable;
	}

	chip->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					chip);
	if (IS_ERR(chip->usb_icl_votable)) {
		rc = PTR_ERR(chip->usb_icl_votable);
		goto destroy_votable;
	}

	chip->pl_disable_votable = create_votable("PL_DISABLE", VOTE_SET_ANY,
					pl_disable_vote_callback,
					chip);
	if (IS_ERR(chip->pl_disable_votable)) {
		rc = PTR_ERR(chip->pl_disable_votable);
		goto destroy_votable;
	}
	vote(chip->pl_disable_votable, CHG_STATE_VOTER, true, 0);
	vote(chip->pl_disable_votable, TAPER_END_VOTER, false, 0);
	vote(chip->pl_disable_votable, PARALLEL_PSY_VOTER, true, 0);

	chip->pl_awake_votable = create_votable("PL_AWAKE", VOTE_SET_ANY,
					pl_awake_vote_callback,
					chip);
	if (IS_ERR(chip->pl_awake_votable)) {
		rc = PTR_ERR(chip->pl_disable_votable);
		goto destroy_votable;
	}

	chip->pl_enable_votable_indirect = create_votable("PL_ENABLE_INDIRECT",
					VOTE_SET_ANY,
					pl_enable_indirect_vote_callback,
					chip);
	if (IS_ERR(chip->pl_enable_votable_indirect)) {
		rc = PTR_ERR(chip->pl_enable_votable_indirect);
		return rc;
	}

	vote(chip->pl_disable_votable, PL_INDIRECT_VOTER, true, 0);

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_WORK(&chip->pl_taper_work, pl_taper_work);
	INIT_WORK(&chip->pl_disable_forever_work, pl_disable_forever_work);
	INIT_DELAYED_WORK(&chip->pl_awake_work, pl_awake_work);
	INIT_DELAYED_WORK(&chip->fcc_stepper_work, fcc_stepper_work);

	rc = pl_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto unreg_notifier;
	}

	rc = pl_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n", rc);
		goto unreg_notifier;
	}

	chip->pl_disable = true;
	chip->qcom_batt_class.name = "qcom-battery",
	chip->qcom_batt_class.owner = THIS_MODULE,
	chip->qcom_batt_class.class_attrs = pl_attributes;

	rc = class_register(&chip->qcom_batt_class);
	if (rc < 0) {
		pr_err("couldn't register pl_data sysfs class rc = %d\n", rc);
		goto unreg_notifier;
	}

	the_chip = chip;

	return 0;

unreg_notifier:
	power_supply_unreg_notifier(&chip->nb);
destroy_votable:
	destroy_votable(chip->pl_enable_votable_indirect);
	destroy_votable(chip->pl_awake_votable);
	destroy_votable(chip->pl_disable_votable);
	destroy_votable(chip->fv_votable);
	destroy_votable(chip->fcc_votable);
	destroy_votable(chip->usb_icl_votable);
release_wakeup_source:
	wakeup_source_unregister(chip->pl_ws);
cleanup:
	kfree(chip);
	return rc;
}

void qcom_batt_deinit(void)
{
	struct pl_data *chip = the_chip;

	if (chip == NULL)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	cancel_work_sync(&chip->pl_taper_work);
	cancel_work_sync(&chip->pl_disable_forever_work);
	cancel_delayed_work_sync(&chip->pl_awake_work);
	cancel_delayed_work_sync(&chip->fcc_stepper_work);

	power_supply_unreg_notifier(&chip->nb);
	destroy_votable(chip->pl_enable_votable_indirect);
	destroy_votable(chip->pl_awake_votable);
	destroy_votable(chip->pl_disable_votable);
	destroy_votable(chip->fv_votable);
	destroy_votable(chip->fcc_votable);
	wakeup_source_unregister(chip->pl_ws);
	the_chip = NULL;
	kfree(chip);
}
