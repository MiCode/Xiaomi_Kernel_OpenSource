/* Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <linux/of.h>
#include <linux/of_batterydata.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include "step-chg-jeita.h"
#include "smb5-lib.h"

#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"
#define DYNAMIC_FV_VOTER	"DYNAMIC_FV_VOTER"
#define BATT_PROFILE_VOTER	"BATT_PROFILE_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct step_chg_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fcc_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fv_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fv_cfg[MAX_STEP_CHG_ENTRIES];
};

struct dynamic_fv_cfg {
	char			*prop_name;
	struct range_data	fv_cfg[MAX_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	struct device		*dev;
	ktime_t			step_last_update_time;
	ktime_t			jeita_last_update_time;
	ktime_t			dynamic_fv_last_update_time;
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	bool			dynamic_fv_enable;
	bool			jeita_arb_en;
	bool			config_is_read;
	bool			step_chg_cfg_valid;
	bool			sw_jeita_cfg_valid;
	bool			dynamic_fv_cfg_valid;
	bool			soc_based_step_chg;
	bool			ocv_based_step_chg;
	bool			vbat_avg_based_step_chg;
	bool			batt_missing;
	bool			taper_fcc;
	bool			six_pin_battery;
	bool			qc3p5_ffc_batt;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			dynamic_fv_index;
	int			step_index;
	int			get_config_retry_count;

	struct step_chg_cfg	*step_chg_config;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;
	struct dynamic_fv_cfg	*dynamic_fv_config;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*cp_disable_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
	struct notifier_block	nb;
};

static struct step_chg_info *the_chip;

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

#define BATT_HOT_DECIDEGREE_MAX			600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

static bool is_batt_available(struct step_chg_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct step_chg_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_usb_available(struct step_chg_info *chip)
{
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy)
		return false;

	return true;
}

static bool is_input_present(struct step_chg_info *chip)
{
	int rc = 0, input_present = 0;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			pr_err("Couldn't read USB Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");
	if (chip->dc_psy) {
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			pr_err("Couldn't read DC Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		int max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	if (!node || !prop_str || !ranges) {
		pr_err("Invalid parameters passed\n");
		return -EINVAL;
	}

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		pr_err("Count %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		pr_err("%s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;

	if (tuples > MAX_STEP_CHG_ENTRIES) {
		pr_err("too many entries(%d), only %d allowed\n",
				tuples, MAX_STEP_CHG_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		pr_err("Read %s failed, rc=%d", prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			pr_err("%s thresholds should be in ascendant ranges\n",
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				pr_err("%s thresholds should be in ascendant ranges\n",
							prop_str);
				rc = -EINVAL;
				goto clean;
			}
		}

		if (ranges[i].low_threshold > max_threshold)
			ranges[i].low_threshold = max_threshold;
		if (ranges[i].high_threshold > max_threshold)
			ranges[i].high_threshold = max_threshold;
		if (ranges[i].value > max_value)
			ranges[i].value = max_value;
	}

	return rc;
clean:
	memset(ranges, 0, tuples * sizeof(struct range_data));
	return rc;
}
EXPORT_SYMBOL(read_range_data_from_node);

static int get_step_chg_jeita_setting_from_profile(struct step_chg_info *chip)
{
	struct device_node *batt_node, *profile_node;
	u32 max_fv_uv, max_fcc_ma;
	const char *batt_type_str;
	const __be32 *handle;
	int batt_id_ohms, rc;
	union power_supply_propval prop = {0, };

	handle = of_get_property(chip->dev->of_node,
			"qcom,battery-data", NULL);
	if (!handle) {
		pr_debug("ignore getting sw-jeita/step charging settings from profile\n");
		return 0;
	}

	batt_node = of_find_node_by_phandle(be32_to_cpup(handle));
	if (!batt_node) {
		pr_err("Get battery data node failed\n");
		return -EINVAL;
	}

	if (!is_bms_available(chip))
		return -ENODEV;

	power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
	batt_id_ohms = prop.intval;

	/* bms_psy has not yet read the batt_id */
	if (batt_id_ohms < 0)
		return -EBUSY;

	profile_node = of_batterydata_get_best_profile(batt_node,
					batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("Couldn't find profile\n");
		return -ENODATA;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}
	pr_debug("battery: %s detected, getting sw-jeita/step charging settings\n",
					batt_type_str);

	rc = of_property_read_u32(profile_node, "qcom,ffc-max-voltage-uv",
					&max_fv_uv);
	if (rc < 0) {
		pr_err("ffc-max-voltage_uv reading failed, try max-voltage_uv, rc=%d\n", rc);
		rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv", &max_fv_uv);
		if (rc < 0) {
			pr_err("max-voltage_uv reading failed, rc=%d\n", rc);
			return rc;
		}
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		return rc;
	}

	chip->taper_fcc = of_property_read_bool(profile_node, "qcom,taper-fcc");

	chip->soc_based_step_chg =
		of_property_read_bool(profile_node, "qcom,soc-based-step-chg");
	if (chip->soc_based_step_chg) {
		chip->step_chg_config->param.psy_prop =
				POWER_SUPPLY_PROP_CAPACITY;
		chip->step_chg_config->param.prop_name = "SOC";
		chip->step_chg_config->param.hysteresis = 0;
	}

	chip->ocv_based_step_chg =
		of_property_read_bool(profile_node, "qcom,ocv-based-step-chg");
	if (chip->ocv_based_step_chg) {
		chip->step_chg_config->param.psy_prop =
				POWER_SUPPLY_PROP_VOLTAGE_OCV;
		chip->step_chg_config->param.prop_name = "OCV";
		chip->step_chg_config->param.hysteresis = 10000;
		chip->step_chg_config->param.use_bms = true;
	}

	chip->vbat_avg_based_step_chg =
				of_property_read_bool(profile_node,
				"qcom,vbat-avg-based-step-chg");
	if (chip->vbat_avg_based_step_chg) {
		chip->step_chg_config->param.psy_prop =
				POWER_SUPPLY_PROP_VOLTAGE_AVG;
		chip->step_chg_config->param.prop_name = "VBAT_AVG";
		chip->step_chg_config->param.hysteresis = 0;
		chip->step_chg_config->param.use_bms = true;
	}

	chip->step_chg_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,step-chg-ranges",
			chip->step_chg_config->fcc_cfg,
			chip->soc_based_step_chg ? 100 : max_fv_uv,
			max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,step-chg-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->step_chg_cfg_valid = false;
	}

	chip->sw_jeita_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fcc-ranges",
			chip->jeita_fcc_config->fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,jeita-fcc-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}

	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fv-ranges",
			chip->jeita_fv_config->fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fv_uv);
	if (rc < 0) {
		pr_debug("Read qcom,jeita-fv-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}

	chip->dynamic_fv_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,dynamic-fv-ranges",
			chip->dynamic_fv_config->fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fv_uv);
	if (rc < 0) {
		pr_debug("Read qcom,dynamic-fv-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->dynamic_fv_cfg_valid = false;
	}

	chip->six_pin_battery =
		of_property_read_bool(profile_node, "mi,six-pin-battery");

	chip->qc3p5_ffc_batt =
		of_property_read_bool(profile_node, "mi,qc3p5-ffc-battery");

	if (lct_check_hwversion() == INDIA_HWVERSION)
		chip->qc3p5_ffc_batt = false;

	return rc;
}

static void get_config_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, get_config_work.work);
	int i, rc;

	chip->config_is_read = false;
	rc = get_step_chg_jeita_setting_from_profile(chip);

	if (rc < 0) {
		if (rc == -ENODEV || rc == -EBUSY) {
			if (chip->get_config_retry_count++
					< GET_CONFIG_RETRY_COUNT) {
				pr_debug("bms_psy is not ready, retry: %d\n",
						chip->get_config_retry_count);
				goto reschedule;
			}
		}
	}

	chip->config_is_read = true;

	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_err("step-chg-cfg: %duV(SoC) ~ %duV(SoC), %duA\n",
			chip->step_chg_config->fcc_cfg[i].low_threshold,
			chip->step_chg_config->fcc_cfg[i].high_threshold,
			chip->step_chg_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_err("jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %duA\n",
			chip->jeita_fcc_config->fcc_cfg[i].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_err("jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %duV\n",
			chip->jeita_fv_config->fv_cfg[i].low_threshold,
			chip->jeita_fv_config->fv_cfg[i].high_threshold,
			chip->jeita_fv_config->fv_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_debug("dynamic-fv-cfg: %d(count) ~ %d(coutn), %duV\n",
			chip->dynamic_fv_config->fv_cfg[i].low_threshold,
			chip->dynamic_fv_config->fv_cfg[i].high_threshold,
			chip->dynamic_fv_config->fv_cfg[i].value);

	return;

reschedule:
	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

}

static int get_val(struct range_data *range, int hysteresis, int current_index,
		int threshold,
		int *new_index, int *val)
{
	int i;

	*new_index = -EINVAL;

	/*
	 * As battery temperature may be below 0, range.xxx is a unsigned int, but battery
	 * temperature is a signed int, so cannot compare them when battery temp is below 0,
	 * we treat it as 0 degree when the parameter threshold(battery temp) is below 0.
	 */
	if (threshold < 0)
		threshold = 0;

	/*
	 * If the threshold is lesser than the minimum allowed range,
	 * return -ENODATA.
	 */
	if (threshold < range[0].low_threshold)
		return -ENODATA;

	/* First try to find the matching index without hysteresis */
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++) {
		if (!range[i].high_threshold && !range[i].low_threshold) {
			/* First invalid table entry; exit loop */
			break;
		}

		if (is_between(range[i].low_threshold,
			range[i].high_threshold, threshold)) {
			*new_index = i;
			*val = range[i].value;
			break;
		}
	}

	/*
	 * If nothing was found, the threshold exceeds the max range for sure
	 * as the other case where it is lesser than the min range is handled
	 * at the very beginning of this function. Therefore, clip it to the
	 * max allowed range value, which is the one corresponding to the last
	 * valid entry in the battery profile data array.
	 */
	if (*new_index == -EINVAL) {
		if (i == 0) {
			/* Battery profile data array is completely invalid */
			return -ENODATA;
		}

		*new_index = (i - 1);
		*val = range[*new_index].value;
	}

	if (threshold < range[0].low_threshold) {
		*new_index = 0;
		*val = range[*new_index].value;
	} else if (threshold > range[MAX_STEP_CHG_ENTRIES - 1].low_threshold) {
		*new_index = MAX_STEP_CHG_ENTRIES - 1;
		*val = range[*new_index].value;
	}

	/*
	 * If we don't have a current_index return this
	 * newfound value. There is no hysterisis from out of range
	 * to in range transition
	 */
	if (current_index == -EINVAL)
		return 0;

	/*
	 * Check for hysteresis if it in the neighbourhood
	 * of our current index.
	 */
	if (*new_index == current_index + 1) {
		if (threshold < range[*new_index].low_threshold + hysteresis) {
			/*
			 * Stay in the current index, threshold is not higher
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	} else if (*new_index == current_index - 1) {
		if (threshold > range[*new_index].high_threshold - hysteresis) {
			/*
			 * stay in the current index, threshold is not lower
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	}
	return 0;
}

#define TAPERED_STEP_CHG_FCC_REDUCTION_STEP_MA		50000 /* 50 mA */
static void taper_fcc_step_chg(struct step_chg_info *chip, int index,
					int current_voltage)
{
	u32 current_fcc, target_fcc;

	if (index < 0) {
		pr_err("Invalid STEP CHG index\n");
		return;
	}

	current_fcc = get_effective_result(chip->fcc_votable);
	target_fcc = chip->step_chg_config->fcc_cfg[index].value;

	if (index == 0) {
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, target_fcc);
	} else if (current_voltage >
		(chip->step_chg_config->fcc_cfg[index - 1].high_threshold +
		chip->step_chg_config->param.hysteresis)) {
		/*
		 * Ramp down FCC in pre-configured steps till the current index
		 * FCC configuration is reached, whenever the step charging
		 * control parameter exceeds the high threshold of previous
		 * step charging index configuration.
		 */
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, max(target_fcc,
			current_fcc - TAPERED_STEP_CHG_FCC_REDUCTION_STEP_MA));
	} else if ((current_fcc >
		chip->step_chg_config->fcc_cfg[index - 1].value) &&
		(current_voltage >
		chip->step_chg_config->fcc_cfg[index - 1].low_threshold +
		chip->step_chg_config->param.hysteresis)) {
		/*
		 * In case the step charging index switch to the next higher
		 * index without FCCs saturation for the previous index, ramp
		 * down FCC till previous index FCC configuration is reached.
		 */
		vote(chip->fcc_votable, STEP_CHG_VOTER, true,
			max(chip->step_chg_config->fcc_cfg[index - 1].value,
			current_fcc - TAPERED_STEP_CHG_FCC_REDUCTION_STEP_MA));
	}
}

static int handle_step_chg_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, current_index;
	u64 elapsed_us;

	elapsed_us = ktime_us_delta(ktime_get(), chip->step_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		return 0;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED, &pval);
	if (rc < 0)
		chip->step_chg_enable = 0;
	else
		chip->step_chg_enable = pval.intval;

	if (!chip->step_chg_enable || !chip->step_chg_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	if (chip->step_chg_config->param.use_bms)
		rc = power_supply_get_property(chip->bms_psy,
				chip->step_chg_config->param.psy_prop, &pval);
	else
		rc = power_supply_get_property(chip->batt_psy,
				chip->step_chg_config->param.psy_prop, &pval);

	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
			chip->step_chg_config->param.prop_name, rc);
		return rc;
	}

	current_index = chip->step_index;
	rc = get_val(chip->step_chg_config->fcc_cfg,
			chip->step_chg_config->param.hysteresis,
			chip->step_index,
			pval.intval,
			&chip->step_index,
			&fcc_ua);
	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	/* Do not drop step-chg index, if input supply is present */
	if (is_input_present(chip)) {
		if (chip->step_index < current_index)
			chip->step_index = current_index;
	} else {
		chip->step_index = 0;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		return -EINVAL;

	if (chip->taper_fcc) {
		taper_fcc_step_chg(chip, chip->step_index, pval.intval);
	} else {
		fcc_ua = chip->step_chg_config->fcc_cfg[chip->step_index].value;
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, fcc_ua);
	}

	pr_debug("%s = %d Step-FCC = %duA taper-fcc: %d\n",
		chip->step_chg_config->param.prop_name, pval.intval,
		get_client_vote(chip->fcc_votable, STEP_CHG_VOTER),
		chip->taper_fcc);

update_time:
	chip->step_last_update_time = ktime_get();
	return 0;
}

static int handle_dynamic_fv(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fv_uv, cycle_count;
	u64 elapsed_us;
	int batt_vol = 0;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_DYNAMIC_FV_ENABLED, &pval);
	if (rc < 0)
		chip->dynamic_fv_enable = 0;
	else
		chip->dynamic_fv_enable = pval.intval;

	if (!chip->dynamic_fv_enable || !chip->dynamic_fv_cfg_valid) {
		/*need recovery some setting*/
		if (chip->fv_votable)
			vote(chip->fv_votable, DYNAMIC_FV_VOTER, false, 0);
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->dynamic_fv_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		return 0;

	rc = power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				chip->dynamic_fv_config->prop_name, rc);
		return rc;
	}
	cycle_count = pval.intval;

	rc = get_val(chip->dynamic_fv_config->fv_cfg,
			0,
			chip->dynamic_fv_index,
			cycle_count,
			&chip->dynamic_fv_index,
			&fv_uv);
	if (rc < 0) {
		/* remove the vote if no step-based fv is found */
		if (chip->fv_votable)
			vote(chip->fv_votable, DYNAMIC_FV_VOTER, false, 0);
		goto update_time;
	}

	power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	batt_vol = pval.intval;
	if (batt_vol >= fv_uv) {
		goto update_time;
	}

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto update_time;

	vote(chip->fv_votable, DYNAMIC_FV_VOTER, true, fv_uv);
	vote(chip->fv_votable, BATT_PROFILE_VOTER, true, fv_uv);

	/*set battery full voltage to FLOAT VOLTAGE*/
	pval.intval = fv_uv;
	rc = power_supply_set_property(chip->bms_psy,
		POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc < 0) {
		pr_err("Couldn't set CONSTANT VOLTAGE property rc=%d\n", rc);
		return rc;
	}

	pr_debug("%s:cycle_count:%d,Batt_full:%d,fv:%d,\n", __func__, cycle_count, pval.intval, fv_uv);

update_time:
	chip->dynamic_fv_last_update_time = ktime_get();
	return 0;
}

#define JEITA_SUSPEND_HYST_UV		130000
#define JEITA_SIX_PIN_BATT_HYST_UV		100000
#define WARM_VFLOAT_UV			4100000
static int handle_jeita(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0;
	u64 elapsed_us;
	int curr_vfloat_uv, curr_vbat_uv;
	int temp, pd_authen_result = 0;
	enum power_supply_type real_type = POWER_SUPPLY_TYPE_UNKNOWN;
	static bool fast_mode_dis;

	pr_err("handle_jeita enter\n");
	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &pval);
	if (rc < 0)
		chip->sw_jeita_enable = 0;
	else
		chip->sw_jeita_enable = pval.intval;

	if (!chip->sw_jeita_enable || !chip->sw_jeita_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		if (chip->usb_icl_votable)
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->jeita_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		return 0;

	if (chip->jeita_fcc_config->param.use_bms)
		rc = power_supply_get_property(chip->bms_psy,
				chip->jeita_fcc_config->param.psy_prop, &pval);
	else
		rc = power_supply_get_property(chip->batt_psy,
				chip->jeita_fcc_config->param.psy_prop, &pval);

	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				chip->jeita_fcc_config->param.prop_name, rc);
		return rc;
	}

	temp = pval.intval;
	/* should disable/enable cp(smb1395) when soft jeita trigger and clear */
	if (chip->six_pin_battery || (lct_check_hwversion() == VDF_HWVERSION)
		 || (lct_check_hwversion() == JAPAN_HWVERSION)) {
		if (!chip->cp_disable_votable)
			chip->cp_disable_votable = find_votable("CP_DISABLE");

		if (!chip->cp_disable_votable)
			goto update_time;

		if (temp <= BATT_CP_COOL_THRESHOLD || temp >= BATT_CP_WARM_THRESHOLD)
			vote(chip->cp_disable_votable, JEITA_VOTER, true, 0);
		else if ((temp > BATT_CP_COOL_THRESHOLD + chip->jeita_fv_config->param.hysteresis)
				|| (temp < BATT_CP_WARM_THRESHOLD - chip->jeita_fv_config->param.hysteresis))
			vote(chip->cp_disable_votable, JEITA_VOTER, false, 0);
	}

	rc = get_val(chip->jeita_fcc_config->fcc_cfg,
			chip->jeita_fcc_config->param.hysteresis,
			chip->jeita_fcc_index,
			pval.intval,
			&chip->jeita_fcc_index,
			&fcc_ua);
	pr_err("temp:%d, new JEITA index:%d, New JEITA FCC:%d \n",
				pval.intval, chip->jeita_fcc_index, fcc_ua);
	if (rc < 0)
		fcc_ua = 0;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);

	rc = get_val(chip->jeita_fv_config->fv_cfg,
			chip->jeita_fv_config->param.hysteresis,
			chip->jeita_fv_index,
			pval.intval,
			&chip->jeita_fv_index,
			&fv_uv);
	if (rc < 0)
		fv_uv = 0;

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto update_time;

	if (!chip->usb_icl_votable)
		chip->usb_icl_votable = find_votable("USB_ICL");

	if (!chip->usb_icl_votable)
		goto set_jeita_fv;

	pr_err("%s = %d FCC = %duA FV = %duV\n",
			chip->jeita_fcc_config->param.prop_name, pval.intval, fcc_ua, fv_uv);

	/* set and clear fast charge mode when soft jeita trigger and clear */
	if (chip->six_pin_battery || chip->qc3p5_ffc_batt) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PD_AUTHENTICATION, &pval);
		if (rc < 0)
			pr_err("Get fastcharge mode status failed, rc=%d\n", rc);
		pd_authen_result = pval.intval;

		rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &pval);
		real_type = pval.intval;

		rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_HVDCP3_TYPE, &pval);
		if (rc < 0)
			pr_err("get hvdcp3_type failed, rc=%d\n", rc);

		if ((real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5)
					|| (pval.intval == HVDCP3_CLASS_B_27W)
					|| (pd_authen_result == 1)) {
			if ((temp >= BATT_WARM_THRESHOLD || temp <= BATT_COOL_THRESHOLD)
						&& !fast_mode_dis) {
				pr_err("temp:%d disable fastcharge mode\n", temp);
				pval.intval = false;
				rc = power_supply_set_property(chip->usb_psy,
						POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
				if (rc < 0) {
					pr_err("Set fastcharge mode failed, rc=%d\n", rc);
					return rc;
				}
				fast_mode_dis = true;
			} else if ((temp < BATT_WARM_THRESHOLD - chip->jeita_fv_config->param.hysteresis)
						&& (temp > BATT_COOL_THRESHOLD + chip->jeita_fv_config->param.hysteresis)
							&& fast_mode_dis) {
				pr_err("temp:%d enable fastcharge mode\n", temp);
				pval.intval = true;
				rc = power_supply_set_property(chip->usb_psy,
						POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
				if (rc < 0) {
					pr_err("Set fastcharge mode failed, rc=%d\n", rc);
					return rc;
				}
				fast_mode_dis = false;
			}
		} else {
			fast_mode_dis = false;
		}
	}

	/*
	 * If JEITA float voltage is same as max-vfloat of battery then
	 * skip any further VBAT specific checks.
	 */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc || (pval.intval == fv_uv)) {
		vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		goto set_jeita_fv;
	}

	/*
	 * Suspend USB input path if battery voltage is above
	 * JEITA VFLOAT threshold.
	 */
	/* if (chip->jeita_arb_en && fv_uv > 0) { */
	if (fv_uv > 0) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("Get battery voltage failed, rc = %d\n", rc);
			goto set_jeita_fv;
		}
		curr_vbat_uv = pval.intval;

		curr_vfloat_uv = get_effective_result(chip->fv_votable);

		rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			pr_err("Get charge type failed, rc = %d\n", rc);
			goto set_jeita_fv;
		}

		if (curr_vbat_uv > fv_uv + ( chip->qc3p5_ffc_batt ? JEITA_SIX_PIN_BATT_HYST_UV: 20000)) {
			if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER && fv_uv == WARM_VFLOAT_UV) {
				pr_err("curr_vbat_uv = %d, fv_uv = %d, set usb_icl=0\n", curr_vbat_uv, fv_uv);
				vote(chip->usb_icl_votable, JEITA_VOTER, true, 0);
			}
		} else if (curr_vbat_uv < (fv_uv - JEITA_SUSPEND_HYST_UV)) {
			pr_err("curr_vbat_uv = %d, fv_uv = %d, remove usb_icl=0\n", curr_vbat_uv, fv_uv);
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		}
	}

set_jeita_fv:
	vote(chip->fv_votable, JEITA_VOTER, fv_uv ? true : false, fv_uv);

update_time:
	chip->jeita_last_update_time = ktime_get();

	return 0;
}

static int handle_battery_insertion(struct step_chg_info *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return rc;
	}

	if (chip->batt_missing != (!pval.intval)) {
		chip->batt_missing = !pval.intval;
		pr_debug("battery %s detected\n",
				chip->batt_missing ? "removal" : "insertion");
		if (chip->batt_missing) {
			chip->step_chg_cfg_valid = false;
			chip->sw_jeita_cfg_valid = false;
			chip->dynamic_fv_cfg_valid = false;
			chip->get_config_retry_count = 0;
		} else {
			/*
			 * Get config for the new inserted battery, delay
			 * to make sure BMS has read out the batt_id.
			 */
			schedule_delayed_work(&chip->get_config_work,
				msecs_to_jiffies(WAIT_BATT_ID_READY_MS));
		}
	}

	return rc;
}

static void status_change_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, status_change_work.work);
	int rc = 0;
	union power_supply_propval prop = {0, };

	if (!is_batt_available(chip) || !is_bms_available(chip))
		goto exit_work;

	handle_battery_insertion(chip);

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc < 0)
		pr_err("Couldn't handle sw jeita rc = %d\n", rc);

	rc = handle_dynamic_fv(chip);
	if (rc < 0)
		pr_err("Couldn't handle sw dynamic fv rc = %d\n", rc);

	rc = handle_step_chg_config(chip);
	if (rc < 0)
		pr_err("Couldn't handle step rc = %d\n", rc);

	/* Remove stale votes on USB removal */
	if (is_usb_available(chip)) {
		prop.intval = 0;
		power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
		if (!prop.intval) {
			if (chip->usb_icl_votable)
				vote(chip->usb_icl_votable, JEITA_VOTER,
						false, 0);
		}
	}

exit_work:
	__pm_relax(chip->step_chg_ws);
}

static int step_chg_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct step_chg_info *chip = container_of(nb, struct step_chg_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->step_chg_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	if ((strcmp(psy->desc->name, "bms") == 0)) {
		if (chip->bms_psy == NULL)
			chip->bms_psy = psy;
		if (!chip->config_is_read)
			schedule_delayed_work(&chip->get_config_work, 0);
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

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable, bool jeita_arb_en)
{
	int rc;
	struct step_chg_info *chip;

	if (the_chip) {
		pr_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->step_chg_ws = wakeup_source_register("qcom-step-chg");
	if (!chip->step_chg_ws)
		return -EINVAL;

	chip->dev = dev;
	chip->step_chg_enable = step_chg_enable;
	chip->sw_jeita_enable = sw_jeita_enable;
	chip->jeita_arb_en = jeita_arb_en;
	chip->step_index = -EINVAL;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;
	chip->dynamic_fv_index = -EINVAL;

	chip->step_chg_config = devm_kzalloc(dev,
			sizeof(struct step_chg_cfg), GFP_KERNEL);
	if (!chip->step_chg_config)
		return -ENOMEM;

	chip->step_chg_config->param.psy_prop = POWER_SUPPLY_PROP_VOLTAGE_NOW;
	chip->step_chg_config->param.prop_name = "VBATT";
	chip->step_chg_config->param.hysteresis = 100000;

	chip->jeita_fcc_config = devm_kzalloc(dev,
			sizeof(struct jeita_fcc_cfg), GFP_KERNEL);
	chip->jeita_fv_config = devm_kzalloc(dev,
			sizeof(struct jeita_fv_cfg), GFP_KERNEL);
	chip->dynamic_fv_config = devm_kzalloc(dev,
			sizeof(struct dynamic_fv_cfg), GFP_KERNEL);
	if (!chip->jeita_fcc_config || !chip->jeita_fv_config || !chip->dynamic_fv_config)
		return -ENOMEM;

	chip->jeita_fcc_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fcc_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fcc_config->param.hysteresis = 5;
	chip->jeita_fv_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fv_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fv_config->param.hysteresis = 5;

	chip->dynamic_fv_config->prop_name = "BATT_CYCLE_COUNT";

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->get_config_work, get_config_work);

	rc = step_chg_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

	the_chip = chip;

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->step_chg_ws);
	return rc;
}

void qcom_step_chg_deinit(void)
{
	struct step_chg_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	cancel_delayed_work_sync(&chip->get_config_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->step_chg_ws);
	the_chip = NULL;
}
