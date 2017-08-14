/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "QCOM-STEPCHG: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include "step-chg-jeita.h"

#define MAX_STEP_CHG_ENTRIES	8
#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define STATUS_CHANGE_VOTER	"STATUS_CHANGE_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct step_chg_data {
	u32 vbatt_soc_low;
	u32 vbatt_soc_high;
	u32 fcc_ua;
};

struct step_chg_cfg {
	u32 psy_prop;
	char *prop_name;
	struct step_chg_data cfg[MAX_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	ktime_t			last_update_time;
	bool			step_chg_enable;

	struct votable		*fcc_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	struct delayed_work	status_change_work;
	struct notifier_block	nb;
};

static struct step_chg_info *the_chip;

/*
 * Step Charging Configuration
 * Update the table based on the battery profile
 * Supports VBATT and SOC based source
 */
static struct step_chg_cfg step_chg_config = {
	.psy_prop  = POWER_SUPPLY_PROP_VOLTAGE_NOW,
	.prop_name = "VBATT",
	.cfg	 = {
		/* VBAT_LOW	VBAT_HIGH	FCC */
		{3600000,	4000000,	3000000},
		{4000000,	4200000,	2800000},
		{4200000,	4400000,	2000000},
	},
/*
 *	SOC STEP-CHG configuration example.
 *
 *	.psy_prop = POWER_SUPPLY_PROP_CAPACITY,
 *	.prop_name = "SOC",
 *	.cfg	= {
 *		//SOC_LOW	SOC_HIGH	FCC
 *		{20,		70,		3000000},
 *		{70,		90,		2750000},
 *		{90,		100,		2500000},
 *	},
 */
};

static bool is_batt_available(struct step_chg_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static int get_fcc(int threshold)
{
	int i;

	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		if (is_between(step_chg_config.cfg[i].vbatt_soc_low,
			step_chg_config.cfg[i].vbatt_soc_high, threshold))
			return step_chg_config.cfg[i].fcc_ua;

	return -ENODATA;
}

static int handle_step_chg_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED, &pval);
	if (rc < 0)
		chip->step_chg_enable = 0;
	else
		chip->step_chg_enable = pval.intval;

	if (!chip->step_chg_enable) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		return 0;
	}

	rc = power_supply_get_property(chip->batt_psy,
				step_chg_config.psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				step_chg_config.prop_name, rc);
		return rc;
	}

	chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		return -EINVAL;

	fcc_ua = get_fcc(pval.intval);
	if (fcc_ua < 0) {
		/* remove the vote if no step-based fcc is found */
		vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		return 0;
	}

	vote(chip->fcc_votable, STEP_CHG_VOTER, true, fcc_ua);

	pr_debug("%s = %d Step-FCC = %duA\n",
		step_chg_config.prop_name, pval.intval, fcc_ua);

	return 0;
}

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */
static void status_change_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, status_change_work.work);
	int rc = 0;
	u64 elapsed_us;

	elapsed_us = ktime_us_delta(ktime_get(), chip->last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto release_ws;

	if (!is_batt_available(chip))
		goto release_ws;

	rc = handle_step_chg_config(chip);
	if (rc < 0)
		goto release_ws;

	chip->last_update_time = ktime_get();

release_ws:
	__pm_relax(chip->step_chg_ws);
}

static int step_chg_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct step_chg_info *chip = container_of(nb, struct step_chg_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)) {
		__pm_stay_awake(chip->step_chg_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int step_chg_register_notifier(struct step_chg_info *chip)
{
	int rc;

	chip->nb.notifier_call = step_chg_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

int qcom_step_chg_init(bool step_chg_enable)
{
	int rc;
	struct step_chg_info *chip;

	if (the_chip) {
		pr_err("Already initialized\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->step_chg_ws = wakeup_source_register("qcom-step-chg");
	if (!chip->step_chg_ws) {
		rc = -EINVAL;
		goto cleanup;
	}

	chip->step_chg_enable = step_chg_enable;

	if (step_chg_enable && (!step_chg_config.psy_prop ||
				!step_chg_config.prop_name)) {
		/* fail if step-chg configuration is invalid */
		pr_err("Step-chg configuration not defined - fail\n");
		return -ENODATA;
	}

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	rc = step_chg_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	the_chip = chip;

	if (step_chg_enable)
		pr_info("Step charging enabled. Using %s source\n",
				step_chg_config.prop_name);

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->step_chg_ws);
cleanup:
	kfree(chip);
	return rc;
}

void qcom_step_chg_deinit(void)
{
	struct step_chg_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->step_chg_ws);
	the_chip = NULL;
	kfree(chip);
}
