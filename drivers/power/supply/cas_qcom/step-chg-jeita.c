// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define SOC_FCC_VOTER		"SOC_FCC_VOTER"
#define STEP_BMS_CHG_VOTER	"STEP_BMS_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"
#define CC_MODE_VOTER		"CC_MODE_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct step_chg_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct soc_fcc_cfg {
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

struct cold_step_chg_cfg {
	struct step_chg_jeita_param     param;
	struct range_data               fcc_cfg[MAX_COLD_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	struct device		*dev;
	ktime_t			step_last_update_time;
	ktime_t			jeita_last_update_time;
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	bool			jeita_arb_en;
	bool			config_is_read;
	bool			taper_fcc_rate_valid;
	bool			soc_fcc_cfg_valid;
	bool			step_chg_cfg_valid;
	bool			sw_jeita_cfg_valid;
	bool                    cold_step_chg_cfg_valid;
	bool			soc_based_step_chg;
	bool			ocv_based_step_chg;
	bool			vbat_avg_based_step_chg;
	bool			batt_missing;
	bool			use_bq_pump;
	bool			use_bq_gauge;
	bool			taper_fcc;
	int			taper_fcc_limit;
	bool			six_pin_battery;
	bool			fcc_taper_start;
	bool			sw_jeita_start;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			jeita_cold_fcc_index;
	int			step_index;
	int			soc_index;
	int			rate_index;
	int			get_config_retry_count;
	int			jeita_hot_th;
	int			jeita_cold_th;
	int			jeita_cool_th;
	int			jeita_warm_th;
	int			jeita_target_fcc;
	int			jeita_current_fcc;
	bool			jeita_taper_start;

	struct step_chg_cfg	*step_chg_config;
	struct soc_fcc_cfg	*soc_fcc_config;
	struct step_chg_cfg	*taper_fcc_rate;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;
	struct cold_step_chg_cfg *cold_step_chg_config;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*input_suspend_votable;
	struct votable		*chg_disable_votable;
	struct votable		*cp_disable_votable;
	struct votable		*pass_disable_votable;
	struct votable		*ffc_mode_dis_votable;
	struct votable		*fcc_main_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*wls_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
	struct delayed_work	fcc_taper_work;
	struct delayed_work	jeita_work;
	struct delayed_work	jeita_taper_work;
	struct delayed_work	step_fcc_work;
	struct notifier_block	nb;
};

static struct step_chg_info *the_chip;

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

#define BATT_HOT_DECIDEGREE_MAX			600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200
#define FCC_TAPER_DELAY_MS		1000
#define JEITA_WORK_DELAY_MS		1000
#define BATT_CAP_SOC_MAX		100
#define JEITA_TAPER_DELAY_MS		10000
#define JEITA_TAPER_STEP_MA		200000
#define JEITA_TAPER_TEMP_LIMIT		250
#define JEITA_TAPER_DEFUALT_MA		2000000
#define JEITA_TAPER_LIMIT_MA		3000000

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
		pr_err("Read %s failed, rc=%d\n", prop_str, rc);
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

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
					&max_fv_uv);
	if (rc < 0) {
		pr_err("max-voltage_uv reading failed, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		return rc;
	}

	chip->taper_fcc = of_property_read_bool(profile_node, "qcom,taper-fcc");

	if (chip->taper_fcc) {
		rc = of_property_read_u32(profile_node, "qcom,taper-fcc-limit",
				&chip->taper_fcc_limit);
		if (rc < 0) {
			pr_err("get the taper fcc limit failed, rc=%d\n", rc);
			return rc;
		}
	}

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
				POWER_SUPPLY_PROP_VOLTAGE_NOW;
		chip->step_chg_config->param.prop_name = "OCV";
		chip->step_chg_config->param.hysteresis = 30000;
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

	chip->soc_fcc_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,soc-fcc-ranges",
			chip->soc_fcc_config->fcc_cfg,
			BATT_CAP_SOC_MAX, max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,soc-fcc-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->soc_fcc_cfg_valid = false;
	}

	chip->cold_step_chg_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,cold-step-chg-ranges",
			chip->cold_step_chg_config->fcc_cfg,
			max_fv_uv, max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,-fv-ranges failed from battery profile, rc=%d\n",
				rc);
		chip->cold_step_chg_cfg_valid = false;
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

	chip->taper_fcc_rate_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,taper-fcc-rate",
			chip->taper_fcc_rate->fcc_cfg,
			max_fcc_ma * 1000, 100);
	if (rc < 0) {
		pr_debug("Read qcom,taper-fcc-rate failed from battery profile, rc=%d\n",
					rc);
		chip->taper_fcc_rate_valid = false;
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

	rc = of_property_read_u32(profile_node, "qcom,jeita-too-hot",
					&chip->jeita_hot_th);
	if (rc < 0) {
		pr_err("do not use external fg and set jeita to hot to invaled\n");
		chip->jeita_hot_th = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,jeita-too-cold",
					&chip->jeita_cold_th);
	if (rc < 0) {
		pr_err("do not use external fg and set jeita too cold to invaled\n");
		chip->jeita_cold_th = -EINVAL;
	}

	chip->jeita_warm_th = BATT_WARM_THRESHOLD;
	rc = of_property_read_u32(profile_node, "qcom,jeita-warm-th",
					&chip->jeita_warm_th);
	if (rc < 0) {
		pr_err("do not use dtsi config and set jeita warm to invaled\n");
	}

	chip->jeita_cool_th = BATT_COOL_THRESHOLD;
	rc = of_property_read_u32(profile_node, "qcom,jeita-cool-th",
					&chip->jeita_cool_th);
	if (rc < 0) {
		pr_err("do not use dtsi config and set jeita cool to invaled\n");
	}
	chip->use_bq_pump =
			of_property_read_bool(profile_node, "qcom,use-bq-pump");

	chip->use_bq_gauge =
			of_property_read_bool(profile_node, "qcom,use-ext-gauge");

	chip->six_pin_battery =
		of_property_read_bool(profile_node, "mi,six-pin-battery");

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
		pr_info("step-chg-cfg: %duV(SoC) ~ %duV(SoC), %duA\n",
			chip->step_chg_config->fcc_cfg[i].low_threshold,
			chip->step_chg_config->fcc_cfg[i].high_threshold,
			chip->step_chg_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_info("soc-fcc-cfg: %d(Soc)~ %d(Soc), %duA\n",
			chip->soc_fcc_config->fcc_cfg[i].low_threshold,
			chip->soc_fcc_config->fcc_cfg[i].high_threshold,
			chip->soc_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_info("jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %duA\n",
			chip->jeita_fcc_config->fcc_cfg[i].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_info("jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %duV\n",
			chip->jeita_fv_config->fv_cfg[i].low_threshold,
			chip->jeita_fv_config->fv_cfg[i].high_threshold,
			chip->jeita_fv_config->fv_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_info("taper-fcc-tate: %duV ~ %duV, %d\n",
			chip->taper_fcc_rate->fcc_cfg[i].low_threshold,
			chip->taper_fcc_rate->fcc_cfg[i].high_threshold,
			chip->taper_fcc_rate->fcc_cfg[i].value);

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
		if (threshold < range[*new_index].low_threshold) {
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

#define TAPERED_STEP_CHG_FCC_REDUCTION_STEP_UA		50000 /* 50 mA */
#define TAPER_FCC_DEFUALT_RATE                         50 /* 50% */
static void taper_fcc_step_chg(struct step_chg_info *chip, int index,
					int current_voltage)
{
	u32 current_fcc, target_fcc;
	int step_ua = 0, rate, rc;

	if (index < 0) {
		pr_err("Invalid STEP CHG index\n");
		return;
	}

	target_fcc = chip->step_chg_config->fcc_cfg[index].value;
	if (target_fcc <= chip->taper_fcc_limit) {
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, target_fcc);
		return;
	} else
		current_fcc = get_effective_result(chip->fcc_votable);

	rc = get_val(chip->taper_fcc_rate->fcc_cfg, 0,
			chip->rate_index, current_fcc,
			&chip->rate_index,
			&rate);
	if (rc < 0) {
		rate = TAPER_FCC_DEFUALT_RATE;
	}

	if (index == 0) {
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, target_fcc);
	} else if (current_voltage >
		chip->step_chg_config->fcc_cfg[index - 1].high_threshold) {
		/*
		 * Ramp down FCC in pre-configured steps till the current index
		 * FCC configuration is reached, whenever the step charging
		 * control parameter exceeds the high threshold of previous
		 * step charging index configuration.
		 */
		if (current_fcc > target_fcc) {
			step_ua = (current_fcc - target_fcc) * rate / 100;
			if (step_ua < TAPERED_STEP_CHG_FCC_REDUCTION_STEP_UA)
				step_ua = TAPERED_STEP_CHG_FCC_REDUCTION_STEP_UA;
		}
		vote(chip->fcc_votable, STEP_CHG_VOTER, true, max(target_fcc,
			current_fcc - step_ua));
	} else if ((current_fcc >
		chip->step_chg_config->fcc_cfg[index - 1].value) &&
		(current_voltage >
		chip->step_chg_config->fcc_cfg[index - 1].low_threshold)) {
		/*
		 * In case the step charging index switch to the next higher
		 * index without FCCs saturation for the previous index, ramp
		 * down FCC till previous index FCC configuration is reached.
		 */
		if (current_fcc > chip->step_chg_config->fcc_cfg[index - 1].value) {
			step_ua = (current_fcc - chip->step_chg_config->fcc_cfg[index - 1].value) * rate / 100;
			if (step_ua < TAPERED_STEP_CHG_FCC_REDUCTION_STEP_UA)
				step_ua = TAPERED_STEP_CHG_FCC_REDUCTION_STEP_UA;
		}
		vote(chip->fcc_votable, STEP_CHG_VOTER, true,
			max(chip->step_chg_config->fcc_cfg[index - 1].value,
			current_fcc - step_ua));
	}
	pr_info("cur:%d, target:%d, rate:%d, vol:%d cfg[index-1].value:%d, low:%d, high:%d, hysteresis:%d",
		current_fcc, target_fcc, rate,
		current_voltage, chip->step_chg_config->fcc_cfg[index - 1].value,
		chip->step_chg_config->fcc_cfg[index - 1].low_threshold,
		chip->step_chg_config->fcc_cfg[index - 1].high_threshold, chip->step_chg_config->param.hysteresis);
}

static int handle_soc_fcc_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, current_index;
	static int usb_present;

	if (!chip->soc_fcc_cfg_valid)
		return 0;

	if (!is_batt_available(chip))
		return 0;

	if (!is_usb_available(chip))
		return 0;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get usb present status failed, rc=%d\n", rc);
		return rc;
	}
	if (!pval.intval) {
		usb_present = 0;
		return 0;
	}

	if (pval.intval == usb_present) {
		return 0;
	} else
		usb_present = pval.intval;


	rc = power_supply_get_property(chip->batt_psy,
			chip->soc_fcc_config->param.psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
			chip->soc_fcc_config->param.prop_name, rc);
		return rc;
	}

	current_index = chip->soc_index;
	rc = get_val(chip->soc_fcc_config->fcc_cfg,
			chip->soc_fcc_config->param.hysteresis,
			chip->soc_index,
			pval.intval,
			&chip->soc_index,
			&fcc_ua);
	if (rc < 0) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, SOC_FCC_VOTER, false, 0);
	}

	if (current_index == chip->soc_index || chip->soc_index < 0)
		return 0;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		return -EINVAL;

	fcc_ua = chip->soc_fcc_config->fcc_cfg[chip->soc_index].value;
	if (fcc_ua)
		vote(chip->fcc_votable, SOC_FCC_VOTER, true, fcc_ua);

	pr_info("%s = %d SOC-FCC = %duA\n",
		chip->soc_fcc_config->param.prop_name, pval.intval,
		get_client_vote(chip->fcc_votable, SOC_FCC_VOTER));

	return 0;
}

static int handle_step_chg_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, current_index, update_now = 0;
	u64 elapsed_us;
	static int usb_present;

	if (!is_usb_available(chip))
		return 0;
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return rc;
	}
	if (pval.intval && pval.intval != usb_present)
		update_now = true;
	usb_present = pval.intval;

	elapsed_us = ktime_us_delta(ktime_get(), chip->step_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US && !update_now && !chip->fcc_taper_start)
		return 0;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED, &pval);
	if (rc < 0)
		chip->step_chg_enable = false;
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

	pr_debug("%s = %d Step-FCC = %duA\n",
		chip->step_chg_config->param.prop_name, pval.intval,
		get_client_vote(chip->fcc_votable, STEP_CHG_VOTER));
update_time:
	chip->step_last_update_time = ktime_get();
	return 0;
}

static int handle_fast_charge_mode(struct step_chg_info *chip, int temp)
{
	static bool ffc_mode_dis;
	union power_supply_propval pval = {0, };
	int rc;

	if (!is_usb_available(chip))
		return 0;

	if (!chip->ffc_mode_dis_votable)
		chip->ffc_mode_dis_votable = find_votable("FFC_MODE_DIS");

	if (!chip->ffc_mode_dis_votable)
		return -EINVAL;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return rc;
	}
	if (!pval.intval)
		vote(chip->ffc_mode_dis_votable, JEITA_VOTER, false, 0);

	ffc_mode_dis = get_effective_result(chip->ffc_mode_dis_votable);

	if ((temp > chip->jeita_warm_th || temp < chip->jeita_cool_th) && !ffc_mode_dis) {
		vote(chip->ffc_mode_dis_votable, JEITA_VOTER, true, 0);
	} else if ((temp < chip->jeita_warm_th - chip->jeita_fv_config->param.hysteresis) &&
			(temp > chip->jeita_cool_th + chip->jeita_fv_config->param.hysteresis) && ffc_mode_dis) {
		vote(chip->ffc_mode_dis_votable, JEITA_VOTER, false, 0);
	}

	return rc;
}

/* set JEITA_SUSPEND_HYST_UV to 70mV to avoid recharge frequently when jeita warm */
#define JEITA_SUSPEND_HYST_UV		240000
#define JEITA_HYSTERESIS_TEMP_THRED	150
#define JEITA_SIX_PIN_BATT_HYST_UV	100000
#define WARM_VFLOAT_UV                  8180000
static int handle_jeita(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0, temp = 0;
	int volt_now = 0, cold_fcc_ua = 0, cold_index, last_index;
	u64 elapsed_us;
	int curr_vbat_uv, cell_vbat_uv;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &pval);
	if (rc < 0)
		chip->sw_jeita_enable = false;
	else
		chip->sw_jeita_enable = pval.intval;

	if (!chip->sw_jeita_enable || !chip->sw_jeita_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		if (chip->input_suspend_votable)
			vote(chip->input_suspend_votable, JEITA_VOTER, false, 0);
		return 0;
	}

	if (!is_usb_available(chip))
		return 0;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return rc;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->jeita_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US  && !pval.intval)
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

	if (chip->cold_step_chg_cfg_valid) {
		if (chip->cold_step_chg_config->param.use_bms)
			rc = power_supply_get_property(chip->bms_psy,
					chip->cold_step_chg_config->param.psy_prop, &pval);
		else
			rc = power_supply_get_property(chip->batt_psy,
					chip->cold_step_chg_config->param.psy_prop, &pval);
		if (rc < 0) {
			pr_err("Couldn't read %s property rc=%d\n",
					chip->cold_step_chg_config->param.prop_name, rc);
			return rc;
		}

		volt_now = pval.intval;
	}

	if (!chip->chg_disable_votable)
		chip->chg_disable_votable = find_votable("CHG_DISABLE");

	if (!chip->cp_disable_votable)
		chip->cp_disable_votable = find_votable("CP_DISABLE");

	if (!chip->pass_disable_votable)
		chip->pass_disable_votable = find_votable("PASSTHROUGH");

	if (!chip->fcc_main_votable)
		chip->fcc_main_votable = find_votable("FCC_MAIN");

	/* qcom charge pump use cp_disable_voter, others do not need */
	if (!chip->use_bq_pump) {
		if (!chip->chg_disable_votable || !chip->cp_disable_votable ||
				!chip->pass_disable_votable)
			goto update_time;
	} else {
		if (!chip->chg_disable_votable)
			goto update_time;
	}

	if (chip->jeita_hot_th != -EINVAL && chip->jeita_cold_th != -EINVAL) {
		if (temp >= chip->jeita_hot_th ||
				temp <= chip->jeita_cold_th) {
			pr_info("sw-jeita: temp is :%d, stop charing\n", temp);
			vote(chip->chg_disable_votable, JEITA_VOTER, true, 0);
			vote_override(chip->fcc_main_votable, CC_MODE_VOTER, false, 0);
			vote_override(chip->input_suspend_votable, CC_MODE_VOTER, false, 0);

		} else {
			vote(chip->chg_disable_votable, JEITA_VOTER, false, 0);
		}
	}

	if (temp <= JEITA_HYSTERESIS_TEMP_THRED) {
		chip->jeita_fv_config->param.hysteresis = 5;
		chip->jeita_fcc_config->param.hysteresis = 5;
	} else  {
		chip->jeita_fv_config->param.hysteresis = 20;
		chip->jeita_fcc_config->param.hysteresis = 20;
	}

	if (!chip->use_bq_pump) {
		if (temp < chip->jeita_cool_th || temp > chip->jeita_warm_th) {
			vote(chip->pass_disable_votable, JEITA_VOTER, true, 0);
			vote(chip->cp_disable_votable, JEITA_VOTER, true, 0);
			vote_override(chip->fcc_main_votable, CC_MODE_VOTER, false, 0);
			vote_override(chip->input_suspend_votable, CC_MODE_VOTER, false, 0);
		} else if (temp >= chip->jeita_cool_th + chip->jeita_fcc_config->param.hysteresis &&
				temp <= chip->jeita_warm_th - chip->jeita_fcc_config->param.hysteresis) {
			vote(chip->pass_disable_votable, JEITA_VOTER, false, 0);
			vote(chip->cp_disable_votable, JEITA_VOTER, false, 0);
		}
	}

	if (chip->jeita_fcc_index == -EINVAL)
		last_index = BATT_COOL_INDEX + 1;
	else
		last_index = chip->jeita_fcc_index;
	rc = get_val(chip->jeita_fcc_config->fcc_cfg,
			chip->jeita_fcc_config->param.hysteresis,
			chip->jeita_fcc_index,
			temp,
			&chip->jeita_fcc_index,
			&fcc_ua);
	if (rc < 0)
		fcc_ua = 0;

	cold_index = chip->jeita_cold_fcc_index;
	if (chip->cold_step_chg_cfg_valid == true) {
		rc = get_val(chip->cold_step_chg_config->fcc_cfg,
				chip->cold_step_chg_config->param.hysteresis,
				chip->jeita_cold_fcc_index,
				volt_now,
				&chip->jeita_cold_fcc_index,
				&cold_fcc_ua);
		if (rc < 0)
			cold_fcc_ua = 0;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	if (chip->cold_step_chg_cfg_valid) {
		if (chip->jeita_fcc_index == 0 && temp < 0) {
			cancel_delayed_work(&chip->jeita_taper_work);
			chip->jeita_taper_start = false;
			/* Do not drop cold-step index, if input supply is present */
			if (is_input_present(chip)) {
				if (chip->jeita_cold_fcc_index < cold_index)
					chip->jeita_cold_fcc_index = cold_index;
			} else
				chip->jeita_cold_fcc_index = 0;
			cold_fcc_ua = chip->cold_step_chg_config->fcc_cfg[chip->jeita_cold_fcc_index].value;
			vote(chip->fcc_votable, JEITA_VOTER, cold_fcc_ua ? true : false, cold_fcc_ua);
		} else {
			if (!chip->jeita_taper_start) {
				if (last_index <= BATT_COOL_INDEX && chip->jeita_fcc_index > BATT_COOL_INDEX) {
					chip->jeita_current_fcc = JEITA_TAPER_DEFUALT_MA;
					chip->jeita_target_fcc = fcc_ua;
					schedule_delayed_work(&chip->jeita_taper_work, 0);
				 } else
					vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);
			}
			chip->jeita_cold_fcc_index = 0;
		}
	} else {
		vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);
	}
	pr_info("handle_jeita: temp:%d, fcc_ua:%d\n", temp, fcc_ua);

	rc = get_val(chip->jeita_fv_config->fv_cfg,
			chip->jeita_fv_config->param.hysteresis,
			chip->jeita_fv_index,
			temp,
			&chip->jeita_fv_index,
			&fv_uv);
	if (rc < 0)
		fv_uv = 0;

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto update_time;

	if (!chip->input_suspend_votable)
		chip->input_suspend_votable = find_votable("INPUT_SUSPEND");

	if (!chip->input_suspend_votable)
		goto set_jeita_fv;

	handle_fast_charge_mode(chip, temp);

	/*
	 * If JEITA float voltage is same as max-vfloat of battery then
	 * skip any further VBAT specific checks.
	 */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc || (pval.intval == fv_uv)) {
		vote(chip->input_suspend_votable, JEITA_VOTER, false, 0);
		vote(chip->fv_votable, JEITA_VOTER, fv_uv ? true : false, fv_uv);
		goto update_time;
	}

	/*
	 * Suspend USB input path if battery voltage is above
	 * JEITA VFLOAT threshold.
	 */
	/* if (chip->jeita_arb_en && fv_uv > 0) { */
	if (fv_uv > 0) {
		rc = power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("Get battery voltage failed, rc = %d\n", rc);
			goto set_jeita_fv;
		}
		cell_vbat_uv = pval.intval;

		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("Get battery voltage failed, rc = %d\n", rc);
			goto set_jeita_fv;
		}
		curr_vbat_uv = pval.intval;

		pr_info("handle_jeita: temp:%d, curr_vbat_uv:%d, cell_vbat_uv:%d fv_uv:%d\n",
				temp, curr_vbat_uv, cell_vbat_uv, fv_uv);
		if ((curr_vbat_uv > fv_uv) && ((cell_vbat_uv > fv_uv)) && (temp >= chip->jeita_warm_th))
			vote(chip->input_suspend_votable, JEITA_VOTER, true, 0);
		else if (curr_vbat_uv < (fv_uv - JEITA_SUSPEND_HYST_UV))
			vote(chip->input_suspend_votable, JEITA_VOTER, false, 0);
	}

set_jeita_fv:
	if (cell_vbat_uv < fv_uv)
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

static void jeita_workfunc(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, jeita_work.work);
	int rc = 0;
	union power_supply_propval pval = {0, };

	if (!is_usb_available(chip))
		return;

	rc = handle_soc_fcc_config(chip);
	if (rc < 0)
		pr_err("Couldn't handle soc fcc rc = %d\n", rc);

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc < 0)
		pr_err("Couldn't handle sw jeita rc = %d\n", rc);
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return;
	}
	if (pval.intval) {
		chip->sw_jeita_start = true;
		schedule_delayed_work(&chip->jeita_work,
			msecs_to_jiffies(JEITA_WORK_DELAY_MS));
	} else
		chip->sw_jeita_start = false;
}

static void fcc_taper_workfunc(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, fcc_taper_work.work);

	handle_step_chg_config(chip);

	schedule_delayed_work(&chip->fcc_taper_work,
			msecs_to_jiffies(FCC_TAPER_DELAY_MS));
}

static void jeita_taper_workfunc(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, jeita_taper_work.work);
	int target_fcc, rc;
	union power_supply_propval pval = {0, };

	chip->jeita_taper_start = true;
	rc = power_supply_get_property(chip->bms_psy,
			chip->jeita_fcc_config->param.psy_prop, &pval);

	if (pval.intval < JEITA_TAPER_TEMP_LIMIT)
		target_fcc = JEITA_TAPER_LIMIT_MA;
	else
		target_fcc = chip->jeita_target_fcc;

	pr_info("handle_jeita curr:%d, target:%d\n",
			chip->jeita_current_fcc, chip->jeita_target_fcc);
	chip->jeita_current_fcc = min((chip->jeita_current_fcc + JEITA_TAPER_STEP_MA),
			target_fcc);

	vote(chip->fcc_votable, JEITA_VOTER, true, chip->jeita_current_fcc);

	if (chip->jeita_current_fcc == chip->jeita_target_fcc) {
		chip->jeita_taper_start = false;
		return;
	}

	schedule_delayed_work(&chip->jeita_taper_work,
			msecs_to_jiffies(JEITA_TAPER_DELAY_MS));
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

	if (!is_usb_available(chip))
		return;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return;
	}
	if (prop.intval) {
		if (!chip->sw_jeita_start) {
			schedule_delayed_work(&chip->jeita_work, 0);
		}
	} else {
		/* skip elapsed_us debounce for handling battery temperature */
		rc = handle_jeita(chip);
		if (rc < 0)
			pr_err("Couldn't handle sw jeita rc = %d\n", rc);
	}

	power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &prop);

	if (prop.intval == POWER_SUPPLY_PD_PPS_ACTIVE) {
		if (!chip->fcc_taper_start) {
			chip->fcc_taper_start = true;
			schedule_delayed_work(&chip->fcc_taper_work, 0);
		}
	} else {
		chip->fcc_taper_start = false;
		cancel_delayed_work(&chip->fcc_taper_work);
		rc = handle_step_chg_config(chip);
		if (rc < 0)
			pr_err("Couldn't handle step rc = %d\n", rc);
	}

	/* Remove stale votes on USB removal */
	if (is_usb_available(chip)) {
		prop.intval = 0;
		power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
		if (!prop.intval) {
			if (chip->input_suspend_votable)
				vote(chip->input_suspend_votable, JEITA_VOTER,
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

	chip->step_chg_ws = wakeup_source_register(dev,"qcom-step-chg");
	if (!chip->step_chg_ws)
		return -EINVAL;

	chip->dev = dev;
	chip->step_chg_enable = step_chg_enable;
	chip->sw_jeita_enable = sw_jeita_enable;
	chip->jeita_arb_en = jeita_arb_en;
	chip->step_index = -EINVAL;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;

	chip->step_chg_config = devm_kzalloc(dev,
			sizeof(struct step_chg_cfg), GFP_KERNEL);
	if (!chip->step_chg_config)
		return -ENOMEM;

	chip->step_chg_config->param.psy_prop = POWER_SUPPLY_PROP_VOLTAGE_NOW;
	chip->step_chg_config->param.prop_name = "VBATT";
	chip->step_chg_config->param.hysteresis = 30000;

	chip->taper_fcc_rate = devm_kzalloc(dev,
			sizeof(struct step_chg_cfg), GFP_KERNEL);
	if (!chip->taper_fcc_rate)
		return -ENOMEM;

	chip->cold_step_chg_config = devm_kzalloc(dev,
			sizeof(struct cold_step_chg_cfg), GFP_KERNEL);
	chip->cold_step_chg_config->param.psy_prop = POWER_SUPPLY_PROP_VOLTAGE_NOW;
	chip->cold_step_chg_config->param.prop_name = "VBATT";
	chip->cold_step_chg_config->param.hysteresis = 100000;
	chip->cold_step_chg_config->param.use_bms = true;

	chip->soc_fcc_config = devm_kzalloc(dev,
			sizeof(struct soc_fcc_cfg), GFP_KERNEL);
	if (!chip->soc_fcc_config)
		return -ENOMEM;
	chip->soc_fcc_config->param.psy_prop = POWER_SUPPLY_PROP_CAPACITY;
	chip->soc_fcc_config->param.prop_name = "SOC";
	chip->soc_fcc_config->param.hysteresis = 0;

	chip->jeita_fcc_config = devm_kzalloc(dev,
			sizeof(struct jeita_fcc_cfg), GFP_KERNEL);
	chip->jeita_fv_config = devm_kzalloc(dev,
			sizeof(struct jeita_fv_cfg), GFP_KERNEL);
	if (!chip->jeita_fcc_config || !chip->jeita_fv_config)
		return -ENOMEM;

	chip->jeita_fcc_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fcc_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fcc_config->param.hysteresis = 20;
	chip->jeita_fv_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fv_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fv_config->param.hysteresis = 20;

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->get_config_work, get_config_work);
	INIT_DELAYED_WORK(&chip->fcc_taper_work, fcc_taper_workfunc);
	INIT_DELAYED_WORK(&chip->jeita_work, jeita_workfunc);
	INIT_DELAYED_WORK(&chip->jeita_taper_work, jeita_taper_workfunc);

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
