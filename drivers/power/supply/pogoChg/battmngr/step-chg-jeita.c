// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 */
#include <linux/battmngr/qti_use_pogo.h>
#include <linux/battmngr/xm_charger_core.h>
#include <../extSOC/inc/virtual_fg.h>
#include <linux/battmngr/battmngr_notifier.h>
//static int log_level = 2;
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

#define GET_CONFIG_DELAY_MS		2000
#define STEP_CHG_DELAY_3S			3000
static bool is_batt_available(struct chg_jeita_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_usb_available(struct chg_jeita_info *chip)
{
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy)
		return false;

	return true;
}

static int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct jeita_range_data *ranges,
		int max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	if (!node || !prop_str || !ranges) {
		charger_err("%s: Invalid parameters passed\n", __func__);
		return -EINVAL;
	}

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		charger_err("%s: Count %s failed, rc=%d\n",
			__func__, prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct jeita_range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		charger_err("%s: %s length (%d) should be multiple of %d\n",
			__func__, prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;

	if (tuples > MAX_JEITA_ENTRIES) {
		charger_err("%s: too many entries(%d), only %d allowed\n",
			__func__, tuples, MAX_JEITA_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		charger_err("%s: Read %s failed, rc=%d\n", __func__, prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			charger_err("%s: %s thresholds should be in ascendant ranges\n",
					__func__, prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				charger_err("%s: %s thresholds should be in ascendant ranges\n",
						__func__, prop_str);
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
	memset(ranges, 0, tuples * sizeof(struct jeita_range_data));
	return rc;
}

static int get_jeita_config_from_dt(struct chg_jeita_info *info)
{
	struct device_node *node = info->dev->of_node;
	int hysteresis[2] = {0};
	int rc, i;
	struct {
		const char *name;
		int *val;
	} chg_data[] = {
		{"xm,charger-jeita-fv-max-mv", &info->max_fv_uv},
		{"xm,charger-jeita-fcc-max-ma", &info->max_fcc_ma},
		{"xm,charger-jeita-too-hot", &info->jeita_hot_th},
		{"xm,charger-jeita-too-cold", &info->jeita_cold_th},
	};

	for (i = 0;i < ARRAY_SIZE(chg_data);i++) {
		rc = of_property_read_u32(node, chg_data[i].name, chg_data[i].val);
		if (rc < 0) {
			charger_err("%s: not find property %s\n", chg_data[i].name, __func__);
			return rc;
		} else {
			charger_err("%s: %d\n", chg_data[i].name, *chg_data[i].val);
		}
	}

	rc = read_range_data_from_node(node,
			"xm,charger-jeita-fcc-ranges",
			info->jeita_fcc_cfg->ranges,
			BATT_HOT_DECIDEGREE_MAX, info->max_fcc_ma);
	if (rc < 0) {
		charger_err("%s: xm,charger-jeita-fcc-ranges reading failed, rc=%d\n",
				__func__, rc);
		return rc;
	}
	for (i = 0; i < MAX_JEITA_ENTRIES; i++)
		charger_err("%s: jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %dmA\n",
			__func__, info->jeita_fcc_cfg->ranges[i].low_threshold,
			info->jeita_fcc_cfg->ranges[i].high_threshold,
			info->jeita_fcc_cfg->ranges[i].value);

	rc = read_range_data_from_node(node,
			"xm,charger-jeita-fv-ranges",
			info->jeita_fv_cfg->ranges,
			BATT_HOT_DECIDEGREE_MAX, info->max_fv_uv);
	if (rc < 0) {
		charger_err("%s: xm,charger-jeita-fv-ranges reading failed, rc=%d\n",
				__func__, rc);
		return rc;
	}
	for (i = 0; i < MAX_JEITA_ENTRIES; i++)
		charger_err("%s: jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %dmV\n",
			__func__, info->jeita_fv_cfg->ranges[i].low_threshold,
			info->jeita_fv_cfg->ranges[i].high_threshold,
			info->jeita_fv_cfg->ranges[i].value);

	rc = of_property_read_u32_array(node,
			"xm,charger-jeita-hysteresis", hysteresis, 2);
	if (rc < 0) {
		charger_err("xm,charger-jeita-hysteresis reading failed, rc=%d\n", rc);
		return rc;
	}
	info->jeita_fcc_cfg->param.rise_hys = hysteresis[0];
	info->jeita_fcc_cfg->param.fall_hys = hysteresis[1];
	info->jeita_fv_cfg->param.rise_hys = hysteresis[0];
	info->jeita_fv_cfg->param.fall_hys = hysteresis[1];
	charger_err("%s: jeita-hys: rise_hys=%u, fall_hys=%u\n",
		__func__, hysteresis[0], hysteresis[1]);

	return rc;
}

static int jeita_get_val(struct jeita_range_data *range, int rise_hys, int fall_hys,
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
	for (i = 0; i < MAX_JEITA_ENTRIES; i++) {
		if (!range[i].high_threshold && !range[i].low_threshold) {
			/* First invalid table entry; exit loop */
			break;
		}

		if (jeita_is_between(range[i].low_threshold,
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

#define JEITA_SUSPEND_HYST_UV		50000
#define JEITA_SUSPEND_HYST_UV_WARM 	120000
static int handle_jeita(struct chg_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0, batt_temp = 0, volt_now = 0;

	stepchg_err("%s: enter\n", __func__);

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		stepchg_err("%s: Get battery temp failed, rc=%d\n",
				__func__, rc);
		return rc;
	}
	batt_temp = pval.intval;

	//jeita too hot or cold, disable chg
	if(chip->jeita_hot_th >= 0 && chip->jeita_cold_th <= 0) {
		if ((batt_temp >= chip->jeita_hot_th || batt_temp <= chip->jeita_cold_th) && !chip->hot_cold_dis_chg) {
			stepchg_err("sw-jeita: temp is :%d, stop charing\n", batt_temp);
			chip->hot_cold_dis_chg = true;
			battmngr_qtiops_set_fg_suspend(g_xm_charger->battmg_dev, true);
		} else if((batt_temp < chip->jeita_hot_th && batt_temp > chip->jeita_cold_th) && chip->hot_cold_dis_chg) {
			stepchg_err("sw-jeita: temp is :%d, start charing\n", batt_temp);
			chip->hot_cold_dis_chg = false;
			battmngr_qtiops_set_fg_suspend(g_xm_charger->battmg_dev, false);
		}
	}

	rc = jeita_get_val(chip->jeita_fcc_cfg->ranges,
			chip->jeita_fcc_cfg->param.rise_hys,
			chip->jeita_fcc_cfg->param.fall_hys,
			chip->jeita_fcc_index,
			batt_temp,
			&chip->jeita_fcc_index,
			&fcc_ua);
	if (rc < 0)
		fcc_ua = 0;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);

	rc = jeita_get_val(chip->jeita_fv_cfg->ranges,
			chip->jeita_fv_cfg->param.rise_hys,
			chip->jeita_fv_cfg->param.fall_hys,
			chip->jeita_fv_index,
			batt_temp,
			&chip->jeita_fv_index,
			&fv_uv);
	if (rc < 0)
		fv_uv = 0;

	stepchg_err("temp:%d, new jeita index:%d, new jeita FCC:%d, new jeita FV:%d\n",
					batt_temp, chip->jeita_fcc_index, fcc_ua, fv_uv);

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto out;

	if (!chip->usb_icl_votable)
		chip->usb_icl_votable = find_votable("ICL");

	if (!chip->usb_icl_votable)
		goto set_jeita_fv;

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&pval);
	if (rc < 0) {
		stepchg_err("%s: Get battery voltage failed, rc=%d\n",
				__func__, rc);
		return rc;
	}
	volt_now = pval.intval;

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

static void status_change_work(struct work_struct *work)
{
	struct chg_jeita_info *chip = container_of(work,
			struct chg_jeita_info, status_change_work.work);
	int rc = 0;
	union power_supply_propval prop = {0, };

	stepchg_err("%s: work\n", __func__);

	if (!is_batt_available(chip))
		return;

	if (!is_usb_available(chip))
		return;

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc < 0)
		stepchg_err("%s: Couldn't handle sw jeita rc = %d\n", __func__, rc);

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

int charger_jeita_start_stop(struct xm_charger *charger,
			struct chg_jeita_info *chip)
{
	stepchg_err("%s\n", __func__);

	if (g_xm_charger->bc12_active) {
		cancel_delayed_work_sync(&chip->status_change_work);
		schedule_delayed_work(&chip->status_change_work, 0);
	} else {
		chip->chg_change_flag = false;
		chip->chg_enable_flag = false;
		chip->hot_cold_dis_chg = false;
		cancel_delayed_work_sync(&chip->status_change_work);
	}

	return 0;
}

int charger_jeita_init(struct xm_charger *charger)
{
	struct chg_jeita_info *chip;
	int rc;

	stepchg_err("%s: Start\n", __func__);

	if (charger->chg_jeita) {
		stepchg_err("%s: Already initialized\n", __func__);
		return -EINVAL;
	}

	chip = devm_kzalloc(charger->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = charger->dev;
	chip->jeita_arb_en = false;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;

	chip->chg_change_flag = false;
	chip->chg_enable_flag = false;

	chip->jeita_fcc_cfg = devm_kzalloc(chip->dev,
			sizeof(struct jeita_fcc_config), GFP_KERNEL);
	chip->jeita_fv_cfg = devm_kzalloc(chip->dev,
			sizeof(struct jeita_fv_config), GFP_KERNEL);
	if (!chip->jeita_fcc_cfg || !chip->jeita_fv_cfg)
		return -ENOMEM;

	rc = get_jeita_config_from_dt(chip);
	if (rc < 0) {
		charger_err("%s: Couldn't parse jeita config rc=%d\n", __func__, rc);
		return rc;
	}

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	charger->chg_jeita = chip;
	stepchg_err("%s: End\n", __func__);

	return 0;
}

void charger_jeita_deinit(struct xm_charger *charger)
{
	struct chg_jeita_info *chip = charger->chg_jeita;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	chip = NULL;

	return;
}

