// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/battmngr/xm_charger_core.h>

static int log_level = 2;
#define stepchg_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[stepchg_jeita] " fmt, ##__VA_ARGS__);	\
} while (0)

#define stepchg_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[stepchg_jeita] " fmt, ##__VA_ARGS__);	\
} while (0)

#define stepchg_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[stepchg_jeita] " fmt, ##__VA_ARGS__);	\
} while (0)

static bool is_batt_available(struct step_chg_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
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
			stepchg_err("%s: Couldn't read USB Present status, rc=%d\n", __func__, rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

static int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		int max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	if (!node || !prop_str || !ranges) {
		stepchg_err("%s: Invalid parameters passed\n", __func__);
		return -EINVAL;
	}

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		stepchg_err("%s: Count %s failed, rc=%d\n", __func__, prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		stepchg_err("%s: %s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;

	if (tuples > MAX_STEP_CHG_ENTRIES) {
		stepchg_err("%s: too many entries(%d), only %d allowed\n", __func__,
				tuples, MAX_STEP_CHG_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		stepchg_err("%s: Read %s failed, rc=%d\n", __func__, prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			stepchg_err("%s: %s thresholds should be in ascendant ranges\n", __func__,
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				stepchg_err("%s: %s thresholds should be in ascendant ranges\n", __func__,
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

static int get_step_chg_jeita_setting_from_profile(struct step_chg_info *chip)
{
	struct device_node *node = chip->dev->of_node;
	u32 max_fv_uv = 0, max_fcc_ma = 0;
	int hysteresis[2] = {0}, rc = 0;

	if (!node) {
		stepchg_err("%s: device tree node missing\n", __func__);
		return -EINVAL;
	}

	chip->taper_fcc = of_property_read_bool(node, "xm,taper-fcc");

	rc = of_property_read_u32(node, "xm,fv-max-uv",
					&max_fv_uv);
	if (rc < 0) {
		pr_err("max-voltage_uv reading failed, rc=%d\n", rc);
		max_fv_uv = -EINVAL;
	}

	rc = of_property_read_u32(node, "xm,fcc-max-ua",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		max_fcc_ma = -EINVAL;
	}

	chip->step_chg_cfg_valid = true;
	if (chip->cycle_count_status == CYCLE_COUNT_NORMAL) {
		rc = read_range_data_from_node(node,
				"xm,step-chg-normal-ranges",
				chip->step_chg_config->fcc_cfg,
				max_fv_uv, max_fcc_ma * 1000);
		if (rc < 0) {
			stepchg_err("%s: Read xm,step-chg-normal-ranges failed from battery profile, rc=%d\n",
					__func__, rc);
			chip->step_chg_cfg_valid = false;
		}
	} else if (chip->cycle_count_status == CYCLE_COUNT_HIGH) {
		rc = read_range_data_from_node(node,
				"xm,step-chg-high-ranges",
				chip->step_chg_config->fcc_cfg,
				max_fv_uv, max_fcc_ma * 1000);
		if (rc < 0) {
			stepchg_err("%s: Read xm,step-chg-high-ranges failed from battery profile, rc=%d\n",
					__func__, rc);
			chip->step_chg_cfg_valid = false;
		}
	}

	chip->sw_jeita_cfg_valid = true;
	rc = read_range_data_from_node(node,
			"xm,jeita-fcc-ranges",
			chip->jeita_fcc_config->fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);
	if (rc < 0) {
		stepchg_err("%s: Read xm,jeita-fcc-ranges failed from battery profile, rc=%d\n",
				__func__, rc);
		chip->sw_jeita_cfg_valid = false;
	}

	chip->cold_step_chg_cfg_valid = true;
	rc = read_range_data_from_node(node,
			"xm,cold-step-chg-ranges",
			chip->cold_step_chg_config->fcc_cfg,
			max_fv_uv, max_fcc_ma * 1000);
	if (rc < 0) {
		stepchg_err("%s: Read xm,cold-step-chg-ranges failed from battery profile, rc=%d\n",
				__func__, rc);
		chip->cold_step_chg_cfg_valid = false;
	}

	rc = read_range_data_from_node(node,
			"xm,jeita-fv-ranges",
			chip->jeita_fv_config->fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fv_uv);
	if (rc < 0) {
		stepchg_err("%s: Read xm,jeita-fv-ranges failed from battery profile, rc=%d\n",
				__func__, rc);
		chip->sw_jeita_cfg_valid = false;
	}

	rc = of_property_read_u32_array(node,
			"xm,step-jeita-hysteresis", hysteresis, 2);
	if (!rc) {
		chip->jeita_fcc_config->param.rise_hys = hysteresis[0];
		chip->jeita_fcc_config->param.fall_hys = hysteresis[1];
		chip->jeita_fv_config->param.rise_hys = hysteresis[0];
		chip->jeita_fv_config->param.fall_hys = hysteresis[1];
		stepchg_err("%s: jeita-hys: rise_hys=%u, fall_hys=%u\n", __func__,
			hysteresis[0], hysteresis[1]);
	}

	rc = of_property_read_u32(node, "xm,jeita-too-hot",
					&chip->jeita_hot_th);
	if (rc < 0) {
		pr_err("jeita-too-hot reading failed, rc=%d\n", rc);
		chip->jeita_hot_th = -EINVAL;
	}

	rc = of_property_read_u32(node, "xm,jeita-too-cold",
					&chip->jeita_cold_th);
	if (rc < 0) {
		pr_err("jeita-too-cold reading failed, rc=%d\n", rc);
		chip->jeita_cold_th = -EINVAL;
	}

	return rc;
}

static void check_cycle_count_status(struct step_chg_info *chip)
{
	bool update = false;
	int rc = 0, cycle_count = 0;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CYCLE_COUNT, &cycle_count);
	if (rc) {
		stepchg_err("%s: failed to get cycle_count\n", __func__);
		return;
	}

	chip->cycle_count = cycle_count;
	stepchg_err("%s: cycle_count = %d\n", __func__, cycle_count);

	if (chip->cycle_count <= 100) {
		if (chip->cycle_count_status != CYCLE_COUNT_NORMAL) {
			chip->cycle_count_status = CYCLE_COUNT_NORMAL;
			update = true;
		}
	} else {
		if (chip->cycle_count_status != CYCLE_COUNT_HIGH) {
			chip->cycle_count_status = CYCLE_COUNT_HIGH;
			update = true;
		}
	}

	if (update)
		schedule_delayed_work(&chip->get_config_work, 0);

	return;
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
				stepchg_info("%s: bms is not ready, retry: %d\n", __func__,
						chip->get_config_retry_count);
				goto reschedule;
			}
		}
	}

	chip->config_is_read = true;

	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		stepchg_info("%s: step-chg-cfg: %duV(SoC) ~ %duV(SoC), %duA\n", __func__,
			chip->step_chg_config->fcc_cfg[i].low_threshold,
			chip->step_chg_config->fcc_cfg[i].high_threshold,
			chip->step_chg_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		stepchg_info("%s: cold-step-chg-cfg: %duV(SoC) ~ %duV(SoC), %duA\n", __func__,
			chip->cold_step_chg_config->fcc_cfg[i].low_threshold,
			chip->cold_step_chg_config->fcc_cfg[i].high_threshold,
			chip->cold_step_chg_config->fcc_cfg[i].value);

	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		stepchg_info("%s: jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %duA\n", __func__,
			chip->jeita_fcc_config->fcc_cfg[i].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		stepchg_info("%s: jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %duV\n", __func__,
			chip->jeita_fv_config->fv_cfg[i].low_threshold,
			chip->jeita_fv_config->fv_cfg[i].high_threshold,
			chip->jeita_fv_config->fv_cfg[i].value);

	return;

reschedule:
	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

}

static int get_val(struct range_data *range, int rise_hys, int fall_hys,
		int current_index, int threshold, int *new_index, int *val)
{
	int i;

	*new_index = -EINVAL;

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
		if (threshold <
			(range[*new_index].low_threshold + rise_hys)) {
			/*
			 * Stay in the current index, threshold is not higher
			 * by hysteresis amount
			 */
			 if (*new_index < 5) {
				*new_index = current_index;
				*val = range[current_index].value;
			 }
		}
	} else if (*new_index == current_index - 1) {
		if (threshold >
			range[*new_index].high_threshold - fall_hys) {
			/*
			 * stay in the current index, threshold is not lower
			 * by hysteresis amount
			 */
			 if (*new_index >= 4) {
				*new_index = current_index;
				*val = range[current_index].value;
			 }
		}
	}
	return 0;
}

#define TAPERED_STEP_CHG_FCC_REDUCTION_STEP_MA		100000 /* 100 mA */
static void taper_fcc_step_chg(struct step_chg_info *chip, int index,
					int current_voltage)
{
	u32 current_fcc, target_fcc;

	stepchg_err("%s: enter\n", __func__);

	if (index < 0) {
		stepchg_err("%s: Invalid STEP CHG index\n", __func__);
		return;
	}

	current_fcc = get_effective_result(chip->fcc_votable);
	target_fcc = chip->step_chg_config->fcc_cfg[index].value;

	if (current_fcc <= 0) {
		stepchg_err("current_fcc is low(%d), return.\n", current_fcc);
		return;
	}

	if (index == 0) {
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, target_fcc);
	} else if (current_voltage >
		(chip->step_chg_config->fcc_cfg[index - 1].high_threshold +
		chip->step_chg_config->param.rise_hys)) {
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
		chip->step_chg_config->param.fall_hys)) {
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

	stepchg_err("%s: enter\n", __func__);

	if (!chip->step_chg_enable || !chip->step_chg_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto out;
	}

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
		chip->step_chg_config->param.iio_prop, &pval.intval);
	if (rc < 0) {
		stepchg_err("%s: Failed to read IIO prop %d rc=%d\n", __func__,
			chip->step_chg_config->param.iio_prop, rc);
		return rc;
	}

	current_index = chip->step_index;
	rc = get_val(chip->step_chg_config->fcc_cfg,
			chip->step_chg_config->param.rise_hys,
			chip->step_chg_config->param.fall_hys,
			chip->step_index,
			pval.intval,
			&chip->step_index,
			&fcc_ua);
	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto out;
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

	stepchg_err("%s: VBAT_NOW = %d Step-FCC = %duA step_index = %d taper-fcc: %d\n",
		__func__, pval.intval, get_client_vote(chip->fcc_votable, STEP_CHG_VOTER),
		chip->step_index, chip->taper_fcc);

out:
	return 0;
}

#define JEITA_SUSPEND_HYST_UV		50000
#define JEITA_SUSPEND_HYST_UV_WARM 	120000
static int handle_jeita(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0, status = 0;
	int fastcharge_mode = 0, batt_temp = 0;
	int batt_volt = 0, cold_fcc_ua = 0, volt_now = 0, current_jeita_cold_fcc_index;

	stepchg_err("%s: enter\n", __func__);

	if (!chip->sw_jeita_enable || !chip->sw_jeita_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		if (chip->usb_icl_votable)
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		return 0;
	}

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
		chip->jeita_fcc_config->param.iio_prop, &pval.intval);
	if (rc < 0) {
		stepchg_err("%s: Failed to read IIO prop %d rc=%d\n", __func__,
			chip->jeita_fcc_config->param.iio_prop, rc);
		return rc;
	}
	batt_temp = pval.intval;

	if(chip->jeita_hot_th >= 0 && chip->jeita_cold_th <= 0) {
		if ((batt_temp >= chip->jeita_hot_th ||
				batt_temp <= chip->jeita_cold_th) && !chip->hot_cold_dis_chg) {
			stepchg_err("sw-jeita: temp is :%d, stop charing\n", batt_temp);
			chip->hot_cold_dis_chg = true;
			xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
				MAIN_CHARGER_ENABLED, false);
		} else if((batt_temp < chip->jeita_hot_th && batt_temp > chip->jeita_cold_th)
				&& g_xm_charger->input_suspend == 0 && chip->hot_cold_dis_chg) {
			stepchg_err("sw-jeita: temp is :%d, start charing\n", batt_temp);
			chip->hot_cold_dis_chg = false;
			xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
				MAIN_CHARGER_ENABLED, true);
		}
	}

	rc = get_val(chip->jeita_fcc_config->fcc_cfg,
			chip->jeita_fcc_config->param.rise_hys,
			chip->jeita_fcc_config->param.fall_hys,
			chip->jeita_fcc_index,
			pval.intval,
			&chip->jeita_fcc_index,
			&fcc_ua);
	if (rc < 0)
		fcc_ua = 0;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			chip->cold_step_chg_config->param.iio_prop, &volt_now);
	if (rc < 0) {
		stepchg_err("%s: Failed to read IIO prop %d rc=%d\n", __func__,
			chip->cold_step_chg_config->param.iio_prop, rc);
		return rc;
	}

	current_jeita_cold_fcc_index = chip->jeita_cold_fcc_index;
	if (chip->cold_step_chg_cfg_valid) {
		rc = get_val(chip->cold_step_chg_config->fcc_cfg,
				chip->cold_step_chg_config->param.rise_hys,
				chip->cold_step_chg_config->param.fall_hys,
				chip->jeita_cold_fcc_index,
				volt_now,
				&chip->jeita_cold_fcc_index,
				&cold_fcc_ua);
		if (rc < 0)
			cold_fcc_ua = 0;
	}

	if (is_input_present(chip)) {
		if (chip->jeita_cold_fcc_index < current_jeita_cold_fcc_index)
			chip->jeita_cold_fcc_index = current_jeita_cold_fcc_index;
	} else {
		chip->jeita_cold_fcc_index = 0;
	}
	cold_fcc_ua = chip->cold_step_chg_config->fcc_cfg[chip->jeita_cold_fcc_index].value;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	if (chip->cold_step_chg_cfg_valid) {
		vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);
		if (chip->jeita_fcc_index == 0 && chip->jeita_cold_fcc_index != 0)
			vote(chip->fcc_votable, JEITA_VOTER, cold_fcc_ua ? true : false, cold_fcc_ua);
	} else {
		vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);
	}

	rc = get_val(chip->jeita_fv_config->fv_cfg,
			chip->jeita_fv_config->param.rise_hys,
			chip->jeita_fv_config->param.fall_hys,
			chip->jeita_fv_index,
			pval.intval,
			&chip->jeita_fv_index,
			&fv_uv);
	if (rc < 0)
		fv_uv = 0;

	stepchg_err("temp:%d, new jeita index:%d, new jeita FCC:%d, new jeita cold index:%d, new jeita cold FCC:%d, new jeita FV:%d\n",
					pval.intval, chip->jeita_fcc_index, fcc_ua, chip->jeita_cold_fcc_index, cold_fcc_ua, fv_uv);

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto out;

	if (!chip->usb_icl_votable)
		chip->usb_icl_votable = find_votable("ICL");

	if (!chip->usb_icl_votable)
		goto set_jeita_fv;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0)
		stepchg_err("%s: Couldn't read battery status, rc=%d\n", __func__, rc);
	else
		status = pval.intval;
	if (status == POWER_SUPPLY_STATUS_FULL && !chip->fastmode_flag) {
		chip->fastmode_flag = true;
		xm_charger_set_fastcharge_mode(g_xm_charger, 0);
	} else if (status != POWER_SUPPLY_STATUS_FULL && chip->fastmode_flag) {
		chip->fastmode_flag = false;
	}

	if (g_xm_charger->pd_verified) {
		xm_charger_get_fastcharge_mode(g_xm_charger, &fastcharge_mode);
		if ((batt_temp >= BATT_WARM_THRESHOLD || batt_temp <= BATT_COOL_THRESHOLD)
				&& fastcharge_mode) {
			fastcharge_mode = 0;
			xm_charger_set_fastcharge_mode(g_xm_charger, fastcharge_mode);

		} else if ((batt_temp <= BATT_WARM_THRESHOLD - BATT_HYS_THRESHOLD)
				&& (batt_temp > BATT_COOL_THRESHOLD + BATT_HYS_THRESHOLD)
				&& !fastcharge_mode && status != POWER_SUPPLY_STATUS_FULL) {
			fastcharge_mode = 1;
			xm_charger_set_fastcharge_mode(g_xm_charger, fastcharge_mode);
		}
	}

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			chip->step_chg_config->param.iio_prop, &batt_volt);
	if (rc < 0) {
		stepchg_err("%s: Failed to read IIO prop %d rc=%d\n", __func__,
			chip->step_chg_config->param.iio_prop, rc);
		return rc;
	}

	if ((batt_temp >= BATT_WARM_THRESHOLD) && (batt_volt >= BATT_WARM_VBAT_THRESHOLD)
			&& !chip->chg_enable_flag) {
		stepchg_err("sw-jeita: temp is :%d, batt_volt is :%d. stop charing\n", batt_temp, batt_volt);
		chip->chg_enable_flag = true;
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
				MAIN_CHARGER_ENABLED, false);
		cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), 0);
	} else if (((batt_temp <= BATT_WARM_THRESHOLD - BATT_HYS_THRESHOLD) || (batt_volt < BATT_WARM_VBAT_THRESHOLD - JEITA_SUSPEND_HYST_UV_WARM))
			&& chip->chg_enable_flag && (g_xm_charger->input_suspend == 0)) {
		stepchg_err("sw-jeita: temp is :%d, batt_volt is :%d. start charing\n", batt_temp, batt_volt);
		chip->chg_enable_flag = false;
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
				MAIN_CHARGER_ENABLED, true);
		cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), 0);
	}

	if ((batt_temp >= LIGHTING_ICON_CHANGE)
			&& !chip->chg_change_flag) {
		stepchg_err("sw-jeita: temp is :%d, change lightnig icon\n", batt_temp);
		chip->chg_change_flag = true;
		cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), 0);
	} else if ((batt_temp < LIGHTING_ICON_CHANGE)
			&& chip->chg_change_flag && (g_xm_charger->input_suspend == 0)) {
		stepchg_err("sw-jeita: temp is :%d, change lightnig icon\n", batt_temp);
		chip->chg_change_flag = false;
		cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), 0);
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
	if (chip->jeita_arb_en && fv_uv > 0) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc && (pval.intval > fv_uv))
			vote(chip->usb_icl_votable, JEITA_VOTER, true, 0);
		else if (pval.intval < (fv_uv - JEITA_SUSPEND_HYST_UV))
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
	}

set_jeita_fv:
	vote(chip->fv_votable, JEITA_VOTER, fv_uv ? true : false, fv_uv);

out:
	return 0;
}

static int handle_battery_insertion(struct step_chg_info *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		stepchg_err("%s: Get battery present status failed, rc=%d\n", __func__, rc);
		return rc;
	}

	stepchg_err("%s: battery present : %d\n", __func__, pval.intval);

	if (chip->batt_missing != (!pval.intval)) {
		chip->batt_missing = !pval.intval;
		stepchg_err("%s: battery %s detected\n", __func__,
				chip->batt_missing ? "removal" : "insertion");
		if (chip->batt_missing) {
			chip->step_chg_cfg_valid = false;
			chip->sw_jeita_cfg_valid = false;
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

	stepchg_err("%s: work\n", __func__);

	if (!is_batt_available(chip))
		return;

	handle_battery_insertion(chip);

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc < 0)
		stepchg_err("%s: Couldn't handle sw jeita rc = %d\n", __func__, rc);

	check_cycle_count_status(chip);
	rc = handle_step_chg_config(chip);
	if (rc < 0)
		stepchg_err("%s: Couldn't handle step rc = %d\n", __func__, rc);

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
	rc = xm_charger_thermal(g_xm_charger);
	if (rc < 0)
		stepchg_err("%s: Couldn't handle thermal rc = %d\n", __func__, rc);

	schedule_delayed_work(&chip->status_change_work,
			msecs_to_jiffies(STEP_CHG_DELAY_3S));
}

int stepchg_jeita_start_stop(struct xm_charger *charger,
			struct step_chg_info *chip)
{
	stepchg_err("%s\n", __func__);

	if (g_xm_charger->bc12_active || g_xm_charger->pd_active) {
		cancel_delayed_work_sync(&chip->status_change_work);
		schedule_delayed_work(&chip->status_change_work, 0);
	} else {
		chip->step_index = 0;
		chip->jeita_cold_fcc_index = 0;
		chip->hot_cold_dis_chg = false;
		chip->fastmode_flag = false;
		chip->chg_change_flag = false;
		chip->chg_enable_flag = false;
		cancel_delayed_work_sync(&chip->status_change_work);
	}

	return 0;
}

int xm_stepchg_jeita_init(struct xm_charger *charger, bool step_chg_enable, bool sw_jeita_enable)
{
	struct step_chg_info *chip;

	stepchg_err("%s: Start\n", __func__);

	if (charger->step_chg) {
		stepchg_err("%s: Already initialized\n", __func__);
		return -EINVAL;
	}

	chip = devm_kzalloc(charger->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = charger->dev;
	chip->step_chg_enable = step_chg_enable;
	chip->sw_jeita_enable = sw_jeita_enable;
	chip->jeita_arb_en = false;
	chip->step_index = -EINVAL;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;
	chip->cycle_count_status = CYCLE_COUNT_NORMAL;

	chip->hot_cold_dis_chg = false;
	chip->fastmode_flag = false;
	chip->chg_change_flag = false;
	chip->chg_enable_flag = false;

	chip->step_chg_config = devm_kzalloc(chip->dev,
			sizeof(struct step_chg_cfg), GFP_KERNEL);
	if (!chip->step_chg_config)
		return -ENOMEM;

	chip->step_chg_config->param.iio_prop = BATT_FG_VOLTAGE_NOW;
	chip->step_chg_config->param.rise_hys = 5000;
	chip->step_chg_config->param.fall_hys = 5000;

	chip->cold_step_chg_config = devm_kzalloc(chip->dev,
			sizeof(struct cold_step_chg_cfg), GFP_KERNEL);
	if (!chip->cold_step_chg_config)
		return -ENOMEM;

	chip->cold_step_chg_config->param.iio_prop = BATT_FG_VOLTAGE_NOW;
	chip->cold_step_chg_config->param.rise_hys = 5000;
	chip->cold_step_chg_config->param.fall_hys = 5000;

	chip->jeita_fcc_config = devm_kzalloc(chip->dev,
			sizeof(struct jeita_fcc_cfg), GFP_KERNEL);
	chip->jeita_fv_config = devm_kzalloc(chip->dev,
			sizeof(struct jeita_fv_cfg), GFP_KERNEL);
	if (!chip->jeita_fcc_config || !chip->jeita_fv_config)
		return -ENOMEM;

	chip->jeita_fcc_config->param.iio_prop = BATT_FG_TEMP;
	chip->jeita_fcc_config->param.rise_hys = 10;
	chip->jeita_fcc_config->param.fall_hys = 10;
	chip->jeita_fv_config->param.iio_prop = BATT_FG_TEMP;
	chip->jeita_fv_config->param.rise_hys = 10;
	chip->jeita_fv_config->param.fall_hys = 10;

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->get_config_work, get_config_work);

	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

	charger->step_chg = chip;
	stepchg_err("%s: End\n", __func__);

	return 0;
}

void xm_step_chg_deinit(void)
{
	struct step_chg_info *chip = g_xm_charger->step_chg;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	cancel_delayed_work_sync(&chip->get_config_work);
	chip = NULL;

	return;
}

