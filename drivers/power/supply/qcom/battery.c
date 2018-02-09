/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#define DRV_MAJOR_VERSION	1
#define DRV_MINOR_VERSION	0

#define CHG_STATE_VOTER			"CHG_STATE_VOTER"
#define TAPER_END_VOTER			"TAPER_END_VOTER"
#define PL_TAPER_EARLY_BAD_VOTER	"PL_TAPER_EARLY_BAD_VOTER"
#define PARALLEL_PSY_VOTER		"PARALLEL_PSY_VOTER"
#define PL_HW_ABSENT_VOTER		"PL_HW_ABSENT_VOTER"
#define PL_VOTER			"PL_VOTER"
#define RESTRICT_CHG_VOTER		"RESTRICT_CHG_VOTER"
#define ICL_CHANGE_VOTER		"ICL_CHANGE_VOTER"
#define PL_INDIRECT_VOTER		"PL_INDIRECT_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"

struct pl_data {
	int			pl_mode;
	int			slave_pct;
	int			taper_pct;
	int			slave_fcc_ua;
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
	struct delayed_work	pl_taper_work;
	struct power_supply	*main_psy;
	struct power_supply	*pl_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	int			charge_type;
	int			total_settled_ua;
	int			pl_settled_ua;
	struct class		qcom_batt_class;
	struct wakeup_source	*pl_ws;
	struct notifier_block	nb;
};

struct pl_data *the_chip;

enum print_reason {
	PR_PARALLEL	= BIT(0),
	PR_OEM		= BIT(1),
};

static int debug_mask;
module_param_named(debug_mask, debug_mask, int, S_IRUSR | S_IWUSR);

#define pl_dbg(chip, reason, fmt, ...)				\
	do {								\
		if (debug_mask & (reason))				\
			pr_info(fmt, ##__VA_ARGS__);	\
		else							\
			pr_debug(fmt, ##__VA_ARGS__);		\
	} while (0)

enum {
	VER = 0,
	SLAVE_PCT,
	RESTRICT_CHG_ENABLE,
	RESTRICT_CHG_CURRENT,
};

/*******
 * ICL *
********/
static void split_settled(struct pl_data *chip)
{
	int slave_icl_pct, total_current_ua;
	int slave_ua = 0, main_settled_ua = 0;
	union power_supply_propval pval = {0, };
	int rc, total_settled_ua = 0;

	if ((chip->pl_mode != POWER_SUPPLY_PL_USBIN_USBIN)
		&& (chip->pl_mode != POWER_SUPPLY_PL_USBIN_USBIN_EXT))
		return;

	if (!chip->main_psy)
		return;

	if (!get_effective_result_locked(chip->pl_disable_votable)) {
		/* read the aicl settled value */
		rc = power_supply_get_property(chip->main_psy,
			       POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &pval);
		if (rc < 0) {
			pr_err("Couldn't get aicl settled value rc=%d\n", rc);
			return;
		}
		main_settled_ua = pval.intval;
		/* slave gets 10 percent points less for ICL */
		slave_icl_pct = max(0, chip->slave_pct - 10);
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
			return;
		}
		/* no client is voting, so get the total current from charger */
		rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_HW_CURRENT_MAX, &pval);
		if (rc < 0) {
			pr_err("Couldn't get max current rc=%d\n", rc);
			return;
		}
		total_current_ua = pval.intval;
	}

	pval.intval = total_current_ua - slave_ua;
	/* Set ICL on main charger */
	rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
	if (rc < 0) {
		pr_err("Couldn't change slave suspend state rc=%d\n", rc);
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

	chip->total_settled_ua = total_settled_ua;
	chip->pl_settled_ua = slave_ua;

	pl_dbg(chip, PR_PARALLEL,
		"Split total_current_ua=%d main_settled_ua=%d slave_ua=%d\n",
		total_current_ua, main_settled_ua, slave_ua);
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
	struct pl_data *chip = container_of(c, struct pl_data,
			qcom_batt_class);
	unsigned long val;

	if (kstrtoul(ubuf, 10, &val))
		return -EINVAL;

	pl_dbg(chip, PR_OEM, "Parallel CT %ld\n", val);
	if (val >= 50 && val <= 100)
		chip->slave_pct = 50;
	else
		chip->slave_pct = val;
	rerun_election(chip->fcc_votable);
	rerun_election(chip->fv_votable);
	split_settled(chip);

	return count;
}

/**********************
* RESTICTED CHARGIGNG *
***********************/
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
	[SLAVE_PCT]		= __ATTR(parallel_pct, S_IRUGO | S_IWUSR,
					slave_pct_show, slave_pct_store),
	[RESTRICT_CHG_ENABLE]	= __ATTR(restricted_charging, S_IRUGO | S_IWUSR,
					restrict_chg_show, restrict_chg_store),
	[RESTRICT_CHG_CURRENT]	= __ATTR(restricted_current, S_IRUGO | S_IWUSR,
					restrict_cur_show, restrict_cur_store),
	__ATTR_NULL,
};

/***********
 *  TAPER  *
************/
#define MINIMUM_PARALLEL_FCC_UA		500000
#define PL_TAPER_WORK_DELAY_MS		100
#define TAPER_RESIDUAL_PCT		75
static void pl_taper_work(struct work_struct *work)
{
	struct pl_data *chip = container_of(work, struct pl_data,
						pl_taper_work.work);
	union power_supply_propval pval = {0, };
	int rc;

	/* exit immediately if parallel is disabled */
	if (get_effective_result(chip->pl_disable_votable)) {
		pl_dbg(chip, PR_PARALLEL, "terminating parallel not in progress\n");
		goto done;
	}

	pl_dbg(chip, PR_PARALLEL, "entering parallel taper work slave_fcc = %d\n",
			chip->slave_fcc_ua);
	if (chip->slave_fcc_ua < MINIMUM_PARALLEL_FCC_UA) {
		pl_dbg(chip, PR_PARALLEL, "terminating parallel's share lower than 500mA\n");
		vote(chip->pl_disable_votable, TAPER_END_VOTER, true, 0);
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
		pl_dbg(chip, PR_PARALLEL, "master is taper charging; reducing slave FCC\n");

		vote(chip->pl_awake_votable, TAPER_END_VOTER, true, 0);
		/* Reduce the taper percent by 25 percent */
		chip->taper_pct = chip->taper_pct * TAPER_RESIDUAL_PCT / 100;
		rerun_election(chip->fcc_votable);
		pl_dbg(chip, PR_PARALLEL, "taper entry scheduling work after %d ms\n",
				PL_TAPER_WORK_DELAY_MS);
		schedule_delayed_work(&chip->pl_taper_work,
				msecs_to_jiffies(PL_TAPER_WORK_DELAY_MS));
		return;
	}

	/*
	 * Master back to Fast Charge, get out of this round of taper reduction
	 */
	pl_dbg(chip, PR_PARALLEL, "master is fast charging; waiting for next taper\n");

done:
	vote(chip->pl_awake_votable, TAPER_END_VOTER, false, 0);
}

/*********
 *  FCC  *
**********/
#define EFFICIENCY_PCT	80
static void split_fcc(struct pl_data *chip, int total_ua,
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
	*slave_ua = (*slave_ua * chip->taper_pct) / 100;
	/*
	 * In USBIN_USBIN configuration with internal rsense parallel
	 * charger's current goes through main charger's BATFET, keep
	 * the main charger's FCC to the votable result.
	 */
	if (chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN)
		*master_ua = max(0, total_ua);
	else
		*master_ua = max(0, total_ua - *slave_ua);
}

static int pl_fcc_vote_callback(struct votable *votable, void *data,
			int total_fcc_ua, const char *client)
{
	struct pl_data *chip = data;
	union power_supply_propval pval = {0, };
	int rc, master_fcc_ua = total_fcc_ua, slave_fcc_ua = 0;

	if (total_fcc_ua < 0)
		return 0;

	if (!chip->main_psy)
		return 0;

	if (chip->pl_mode == POWER_SUPPLY_PL_NONE
	    || get_effective_result_locked(chip->pl_disable_votable)) {
		pval.intval = total_fcc_ua;
		rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
		if (rc < 0)
			pr_err("Couldn't set main fcc, rc=%d\n", rc);
		return rc;
	}

	if (chip->pl_mode != POWER_SUPPLY_PL_NONE) {
		split_fcc(chip, total_fcc_ua, &master_fcc_ua, &slave_fcc_ua);

		pval.intval = slave_fcc_ua;
		rc = power_supply_set_property(chip->pl_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
		if (rc < 0) {
			pr_err("Couldn't set parallel fcc, rc=%d\n", rc);
			return rc;
		}

		chip->slave_fcc_ua = slave_fcc_ua;

		pval.intval = master_fcc_ua;
		rc = power_supply_set_property(chip->main_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&pval);
		if (rc < 0) {
			pr_err("Could not set main fcc, rc=%d\n", rc);
			return rc;
		}
	}

	pl_dbg(chip, PR_OEM, "master_fcc=%d slave_fcc=%d distribution=(%d/%d)\n",
		   master_fcc_ua, slave_fcc_ua,
		   (master_fcc_ua * 100) / total_fcc_ua,
		   (slave_fcc_ua * 100) / total_fcc_ua);

	return 0;
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

	pr_info("%s: set icl %d\n", __func__, icl_ua);

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

	if (rerun_aicl) {
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

static int pl_disable_vote_callback(struct votable *votable,
		void *data, int pl_disable, const char *client)
{
	struct pl_data *chip = data;
	union power_supply_propval pval = {0, };
	int rc;

	chip->taper_pct = 100;
	chip->total_settled_ua = 0;
	chip->pl_settled_ua = 0;

	if (!pl_disable) { /* enable */
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
		rerun_election(chip->fcc_votable);
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

		if ((chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN)
			|| (chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT))
			split_settled(chip);
		/*
		 * we could have been enabled while in taper mode,
		 *  start the taper work if so
		 */
		rc = power_supply_get_property(chip->batt_psy,
				       POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Couldn't get batt charge type rc=%d\n", rc);
		} else {
			if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
				pl_dbg(chip, PR_PARALLEL,
					"pl enabled in Taper scheduing work\n");
				schedule_delayed_work(&chip->pl_taper_work, 0);
			}
		}
	} else {
		if ((chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN)
			|| (chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT))
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
		rerun_election(chip->fcc_votable);
		rerun_election(chip->fv_votable);
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
	if ((chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN)
		|| (chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT)) {
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
		chip->taper_pct = 100;
		vote(chip->pl_disable_votable, TAPER_END_VOTER, false, 0);
		vote(chip->pl_disable_votable, PL_TAPER_EARLY_BAD_VOTER,
				false, 0);
		chip->charge_type = pval.intval;
		return;
	}

	/* handle taper charge entry */
	if (chip->charge_type == POWER_SUPPLY_CHARGE_TYPE_FAST
		&& (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER)) {
		chip->charge_type = pval.intval;
		pl_dbg(chip, PR_PARALLEL, "taper entry scheduling work\n");
		schedule_delayed_work(&chip->pl_taper_work, 0);
		return;
	}

	/* handle fast/taper charge entry */
	if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER
			|| pval.intval == POWER_SUPPLY_CHARGE_TYPE_FAST) {
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


	if (get_effective_result(chip->pl_disable_votable))
		return;

	if (chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN
			|| chip->pl_mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT) {
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
						> MIN_ICL_CHANGE_DELTA_UA)
			split_settled(chip);
	} else {
		rerun_election(chip->fcc_votable);
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

#define DEFAULT_RESTRICTED_CURRENT_UA	1000000
int qcom_batt_init(void)
{
	struct pl_data *chip;
	int rc = 0;

	/* initialize just once */
	if (the_chip) {
		pr_err("was initialized earlier Failing now\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->slave_pct = 50;
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

	chip->fv_votable = create_votable("FV", VOTE_MAX,
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
	INIT_DELAYED_WORK(&chip->pl_taper_work, pl_taper_work);
	INIT_WORK(&chip->pl_disable_forever_work, pl_disable_forever_work);

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
	cancel_delayed_work_sync(&chip->pl_taper_work);
	cancel_work_sync(&chip->pl_disable_forever_work);

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
