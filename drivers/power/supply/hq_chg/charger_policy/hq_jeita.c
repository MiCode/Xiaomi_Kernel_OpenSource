// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "hq_jeita: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "hq_jeita.h"
#include "../hq_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG][jeita]"
#endif


#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct step_jeita_cfg {
	int low_threshold;
	int high_threshold;
	int value;
};

struct hq_jeita_info {
	struct device		*dev;
	ktime_t			jeita_last_update_time;
	bool			config_is_read;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			get_config_retry_count;
	int			jeita_index;
	int			fv;
	int			fv_offset_15_to_35;
	int			fv_offset_35_to_48;
	int			iterm;
	int			iterm_ffc_nvt_15_to_35;
	int			iterm_ffc_swd_15_to_35;
	int			iterm_ffc_gy_15_to_35;
	int			iterm_ffc_swd_35_to_48;
	int			iterm_ffc_nvt_35_to_48;
	int			iterm_ffc_gy_35_to_48;
	int			iterm_ffc_35_to_48;
	int			iterm_curr;
	int			under_4200_curr_offset;

	bool		pd_verifed;
	bool		is_charge_done;

	struct wakeup_source	*hq_jeita_ws;
	struct step_jeita_cfg step_chg_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg step_chg_batcycle_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg jeita_fv_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg jeita_fcc_cfg[STEP_JEITA_TUPLE_NUM];

	/* voter add here */
	struct votable *main_fcc_votable;
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *total_fcc_votable;

	/* psy add here */
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*verify_psy;
	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	struct adapter_device *pd_adapter;
	#endif
	struct fuel_gauge_dev *fuel_gauge;
	struct charger_dev *charger;

	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;

	struct notifier_block nb;
};

static struct hq_jeita_info *the_chip;
static bool warm_stop_charge;

bool get_warm_stop_charge_state(void)
{
	return warm_stop_charge;
}
EXPORT_SYMBOL(get_warm_stop_charge_state);

static bool is_batt_veryfy_available(struct hq_jeita_info *chip)
{
	if (!chip->verify_psy)
		chip->verify_psy = power_supply_get_by_name("batt_verify");

	if (!chip->verify_psy)
		return false;

	return true;
}

static bool is_batt_available(struct hq_jeita_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct hq_jeita_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_input_present(struct hq_jeita_info *chip)
{
	int rc = 0, input_present = 0;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0)
			hq_err("Couldn't read USB Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

static int hq_step_jeita_get_index(struct step_jeita_cfg *cfg, int value)
{
	int new_index = 0, i = 0;
	int index = 0;

	if (value < cfg[0].low_threshold) {
		index = 0;
		return index;
	}

	if (value > cfg[STEP_JEITA_TUPLE_NUM - 1].high_threshold)
		new_index = STEP_JEITA_TUPLE_NUM - 1;

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}
	index = new_index;

	return index;
}

static void judge_warm_stop_charge(struct hq_jeita_info *chip, int temp, int vbat)
{
	if (temp >= TEMP_LEVEL_48 && vbat >= TEMP_48_TO_58_VOL)
		warm_stop_charge = true;

	if (warm_stop_charge) {
		if (temp < (TEMP_LEVEL_48 - WARM_RECHG_TEMP_OFFSET) || vbat < (TEMP_48_TO_58_VOL - WARM_RECHG_VOLT_OFFSET))
			warm_stop_charge = false;
	}
}

static int handle_jeita(struct hq_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int ret = 0;
	int i = 0;
	int temp_now, vol_now, curr_now, batt_id;
	int curr_offset = 0;
	static bool cold_curr_lmt = false;

	if (!is_batt_available(chip)) {
		pr_err("failed to get batt psy\n");
		return 0;
	}

	if (!is_batt_veryfy_available(chip)) {
		pr_err("failed to get verify_psy\n");
		return 0;
	}

	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	chip->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chip->pd_adapter)
		hq_err("failed to pd_adapter\n");
	else
		chip->pd_verifed = chip->pd_adapter->verifed;
	#endif
	if (chip->fuel_gauge == NULL)
		chip->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (IS_ERR_OR_NULL(chip->fuel_gauge)) {
		hq_err("failed to get fuel_gauge\n");
	}

	if (chip->charger == NULL)
		chip->charger = charger_find_dev_by_name("primary_chg");
	if (IS_ERR_OR_NULL(chip->charger)) {
		hq_err("failed to get main charger\n");
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0) {
		hq_err("Couldn't read status, ret=%d\n", ret);
	}
	if (pval.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
		cold_curr_lmt = false;
		return 0;
	}

	ret = power_supply_get_property(chip->verify_psy, POWER_SUPPLY_PROP_TYPE, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt_id, ret=%d\n", ret);
	}
	batt_id = 0;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt temp, ret=%d\n", ret);
	}
	temp_now = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt voltage_now, ret=%d\n", ret);
	}
	vol_now = pval.intval / 1000;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt current_now, ret=%d\n", ret);
	}
	curr_now = pval.intval / -1000;

	if (temp_now < 0) {
		charger_set_rechg_volt(chip->charger, LOW_TEMP_RECHG_OFFSET);
	} else {
		charger_set_rechg_volt(chip->charger, NOR_TEMP_RECHG_OFFSET);
	}

	judge_warm_stop_charge(chip, temp_now, vol_now);
	charger_is_charge_done(chip->charger, &(chip->is_charge_done));

	if (temp_now < TEMP_LEVEL_NEGATIVE_10 || temp_now >= TEMP_LEVEL_58) {
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		hq_info("temp_now < TEMP_LEVEL_NEGATIVE_10 || temp_now >= TEMP_LEVEL_58, stop charge");
		return 0;
	}

	if (warm_stop_charge) {
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		hq_info("warm_stop_charge = true, stop charge");
		return 0;
	}

	chip->jeita_index = hq_step_jeita_get_index(chip->jeita_fcc_cfg, temp_now);
	//vbat < 4.25V and 15 < temp <45，ichg = 6A
	if ((chip->jeita_index == INDEX_15_to_35 || chip->jeita_index == INDEX_35_to_48) && (vol_now < NAGETIVE_10_TO_0_VOL_4200)) {
		if (cold_curr_lmt) { //if limited,need vbat below 4.15V,than set ichg = 6A
			if ((vol_now < (NAGETIVE_10_TO_0_VOL_4200 - COLD_RECHG_VOLT_OFFSET))) {
				curr_offset = chip->under_4200_curr_offset;
				cold_curr_lmt = false;
			}
		} else
			curr_offset = chip->under_4200_curr_offset;
	} else { //vbat >= 4.25V, ichg = 5.4A
		curr_offset = 0;
		cold_curr_lmt = true;
	}


	/*FFC: fcc*/
	if(chip->pd_verifed) {
		if (chip->jeita_index == INDEX_15_to_35 || chip->jeita_index == INDEX_35_to_48) {
			ret = power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
			if (ret < 0) {
				pr_err("%s failed to get cycle_count prop", __func__);
				return 0;
			}
			pr_info("%s battery cycle is %d\n", __func__,pval.intval);
			if ( pval.intval <= 800 ) {
				for (i = 0; i < STEP_CYCLE_TUPLE_NUM; i++) {
					if (chip->step_chg_cfg[i].low_threshold <= vol_now && vol_now <= chip->step_chg_cfg[i].high_threshold) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, chip->step_chg_cfg[i].value);
						break;
					}
				}
			} else {
				for (i = 0; i < STEP_CYCLE_TUPLE_NUM; i++) {
					if (chip->step_chg_batcycle_cfg[i].low_threshold <= vol_now && vol_now <= chip->step_chg_batcycle_cfg[i].high_threshold) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, chip->step_chg_batcycle_cfg[i].value);
						break;
					}
				}
			}
		} else
			vote(chip->total_fcc_votable, JEITA_VOTER, true, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset));
	} else
		vote(chip->total_fcc_votable, JEITA_VOTER, true, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset));

	chip->iterm_curr = chip->iterm;
	chip->fv = chip->jeita_fv_cfg[chip->jeita_index].value;

	/*FFC: iterm + fv*/
	if (chip->pd_verifed && (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done)) {
		if (chip->jeita_index == INDEX_15_to_35) {
			if (batt_id == 0)
				chip->iterm_curr = chip->iterm_ffc_nvt_15_to_35;
			if (batt_id == 1)
				chip->iterm_curr = chip->iterm_ffc_swd_15_to_35;
			if (batt_id == 2)
				chip->iterm_curr = chip->iterm_ffc_gy_15_to_35;
			chip->fv += chip->fv_offset_15_to_35;
			chip->fv = (chip->fv - TERM_DELTA_CV); //To prevent excessive in the battery cell voltage, subtract 8mV
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, true);
			#endif
		} else if (chip->jeita_index == INDEX_35_to_48) {
			if (batt_id == 0)
				chip->iterm_curr = chip->iterm_ffc_nvt_35_to_48;
			if (batt_id == 1)
				chip->iterm_curr = chip->iterm_ffc_swd_35_to_48;
			if (batt_id == 2)
				chip->iterm_curr = chip->iterm_ffc_gy_35_to_48;
			chip->fv += chip->fv_offset_35_to_48;
			chip->fv = (chip->fv - TERM_DELTA_CV); //To prevent excessive in the battery cell voltage, subtract 8mV
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, true);
			#endif
		}
		#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
		else {
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, false);
		}
		#endif

		if (chip->is_charge_done)
			adapter_set_cap(g_policy->adapter, g_policy->cap_nr, 5000, 2000);
	}

	vote(chip->iterm_votable, ITER_VOTER, true, chip->iterm_curr);
	vote(chip->fv_votable, JEITA_VOTER, true, chip->fv);

	hq_info("pd_verifed = %d, cp_charge_done = %d, jeita_index = %d, curr_offset = %d, is_charge_done = %d \n",
				chip->pd_verifed, g_policy->cp_charge_done, chip->jeita_index, curr_offset, chip->is_charge_done);
	hq_info("step_chg_fcc = %d, jeita_fcc = %d, jeita_fv = %d, jeita_iterm = %d, batt_id = %d\n",
 				chip->step_chg_cfg[i].value, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset), chip->fv, chip->iterm_curr, batt_id);
	return 0;
}

static void status_change_work(struct work_struct *work)
{
	struct hq_jeita_info*chip = container_of(work,
			struct hq_jeita_info, status_change_work.work);
	int rc = 0;

	if (!is_batt_available(chip)|| !is_bms_available(chip))
		goto exit_work;

	rc = handle_jeita(chip);
	if (rc < 0)
		hq_err("Couldn't handle sw jeita rc = %d\n", rc);

	if (! is_input_present(chip)) {
		vote(chip->main_icl_votable, JEITA_VOTER, false, 0);
	}

exit_work:
	__pm_relax(chip->hq_jeita_ws);
}

static int jeita_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct hq_jeita_info*chip = container_of(nb, struct hq_jeita_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->hq_jeita_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int jeita_register_notifier(struct hq_jeita_info *chip)
{
	int rc;

	chip->nb.notifier_call = jeita_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		hq_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static bool hq_parse_jeita_dt(struct device_node *node, struct hq_jeita_info *chip)
{
	int total_length = 0;
	bool ret = 0;
	int i = 0;

	total_length = of_property_count_elems_of_size(node, "jeita_fcc_cfg", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of jeita_fcc_cfg\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_fcc_cfg", (u32 *)chip->jeita_fcc_cfg, total_length);
	if (ret) {
		hq_err("failed to parse jeita_fcc_cfg\n");
		return 0;
	}

	total_length = of_property_count_elems_of_size(node, "jeita_fv_cfg", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of jeita_fv_cfg\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_fv_cfg", (u32 *)chip->jeita_fv_cfg, total_length);
	if (ret) {
		hq_err("failed to parse jeita_fv_cfg\n");
		return 0;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
		hq_info("[jeita_fcc_cfg]%d %d %d [jeita_fv_cfg]%d %d %d\n",
					chip->jeita_fcc_cfg[i].low_threshold, chip->jeita_fcc_cfg[i].high_threshold, chip->jeita_fcc_cfg[i].value,
					chip->jeita_fv_cfg[i].low_threshold, chip->jeita_fv_cfg[i].high_threshold, chip->jeita_fv_cfg[i].value);

	ret |= of_property_read_u32(node, "iterm", &chip->iterm);
	ret |= of_property_read_u32(node, "iterm_ffc_nvt_15_to_35", &chip->iterm_ffc_nvt_15_to_35);
	ret |= of_property_read_u32(node, "iterm_ffc_swd_15_to_35", &chip->iterm_ffc_swd_15_to_35);
	ret |= of_property_read_u32(node, "iterm_ffc_gy_15_to_35", &chip->iterm_ffc_gy_15_to_35);
	ret |= of_property_read_u32(node, "iterm_ffc_nvt_35_to_48", &chip->iterm_ffc_nvt_35_to_48);
	ret |= of_property_read_u32(node, "iterm_ffc_swd_35_to_48", &chip->iterm_ffc_swd_35_to_48);
	ret |= of_property_read_u32(node, "iterm_ffc_gy_35_to_48", &chip->iterm_ffc_gy_35_to_48);
	ret |= of_property_read_u32(node, "iterm_ffc_35_to_48", &chip->iterm_ffc_35_to_48);
	ret |= of_property_read_u32(node, "under_4200_curr_offset", &chip->under_4200_curr_offset);
	ret |= of_property_read_u32(node, "fv_offset_15_to_35", &chip->fv_offset_15_to_35);
	ret |= of_property_read_u32(node, "fv_offset_35_to_48", &chip->fv_offset_35_to_48);

	return !ret;
}

static bool hq_parse_ffc_dt(struct device_node *node, struct hq_jeita_info *chip)
{
	int total_length = 0;
	int i = 0;
	bool ret = 0;

	total_length = of_property_count_elems_of_size(node, "step_chg_cfg_cycle", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of step_chg_cfg_cycle\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "step_chg_cfg_cycle", (u32 *)chip->step_chg_cfg, total_length);
	if (ret)
	{
		hq_err("failed to parse step_chg_cfg_cycle\n");
		return false;
	}

	for (i = 0; i < STEP_CYCLE_TUPLE_NUM; i++)
		hq_info("[STEP_CHG] [step_chg_cfg_cycle]%d %d %d\n",
					chip->step_chg_cfg[i].low_threshold, chip->step_chg_cfg[i].high_threshold, chip->step_chg_cfg[i].value);

	total_length = of_property_count_elems_of_size(node, "step_chg_cfg_bat_cycle", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of step_chg_cfg_bat_cycle\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "step_chg_cfg_bat_cycle", (u32 *)chip->step_chg_batcycle_cfg, total_length);
	if (ret)
	{
		hq_err("failed to parse step_chg_cfg_bat_cycle\n");
		return false;
	}

	for (i = 0; i < STEP_CYCLE_TUPLE_NUM; i++)
		hq_info("[STEP_CHG] [step_chg_cfg_bat_cycle]%d %d %d\n",
					chip->step_chg_batcycle_cfg[i].low_threshold, chip->step_chg_batcycle_cfg[i].high_threshold, chip->step_chg_batcycle_cfg[i].value);

	return !ret;
}

int hq_jeita_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct device_node *step_jeita_node = NULL;
	struct hq_jeita_info *chip = NULL;

	int rc = 0;

	if (node) {
		step_jeita_node = of_find_node_by_name(node, "step_jeita");
	}

	if (the_chip) {
		hq_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->hq_jeita_ws = wakeup_source_register(dev, "hq_jeita");
	if (!chip->hq_jeita_ws)
		return -EINVAL;

	chip->dev = dev;

	rc = hq_parse_jeita_dt(step_jeita_node, chip);
	if(!rc)
		hq_err("hq_parse_jeita_dt failed\n");

	rc = hq_parse_ffc_dt(step_jeita_node, chip);
	if(!rc)
		hq_err("hq_parse_ffc_dt failed\n");

	chip->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!chip->total_fcc_votable)
		hq_err("find TOTAL_FCC voltable failed\n");

	chip->fv_votable = find_votable("MAIN_FV");
	if (!chip->fv_votable)
		hq_err("find MAIN_FV voltable failed\n");

	chip->iterm_votable = find_votable("MAIN_ITERM");
	if (!chip->iterm_votable)
		hq_err("find MAIN_FV voltable failed\n");

	chip->charger = charger_find_dev_by_name("primary_chg");
	if (chip->charger == NULL) {
		hq_err("failed get charger\n");
	}

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	rc = jeita_register_notifier(chip);
	if (rc < 0) {
		hq_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	the_chip = chip;
	hq_info("hq_jeita_init success\n");

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->hq_jeita_ws);
	return rc;
}

void hq_jeita_deinit(void)
{
	struct hq_jeita_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->hq_jeita_ws);
	the_chip = NULL;
}
/*
 * HQ jeita Release Note
 * 1.0.0
 * (1)  xxxxxx
 */
