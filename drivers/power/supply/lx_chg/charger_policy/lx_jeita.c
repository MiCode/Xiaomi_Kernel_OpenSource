// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "lx_jeita: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "lx_jeita.h"
#include "../lx_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][jeita]"
#endif


static struct lx_jeita_info *the_chip;
static bool warm_stop_charge;

struct step_jeita_cfg_other jeita_other_cfg_0_5 = {3000,4200,4530,1294,920};
struct step_jeita_cfg_other jeita_other_cfg_5_10 = {3000,4200,4530,2041,1294};
struct step_jeita_cfg_other jeita_other_cfg_10_15 = {3000,4200,4530,5029,3535};

int cc_cv_cycle_count[CC_CV_CYCLE_COUNT_MAX] = {0,100,300,800};

/*ffc cycle*/
struct step_jeita_cycle_cfg jeita_FFC_cfg_20_45[CC_CV_CYCLE_COUNT_MAX] = {{3000,4100,4200,4560,7800,5800,5200},{3000,4050,4150,4540,7800,5800,5200},{3000,4020,4120,4520,7800,5800,5200},{3000,4000,4100,4510,6200,4600,4120}};

/*nomal cycle  20°C-35°C*/
struct step_jeita_cycle_cfg jeita_nomal1_fcc_cfg[CC_CV_CYCLE_COUNT_MAX] = {{3000,4100,4490,0,7800,5200,0},{3000,4050,4480,0,7800,5200,0},{3000,4020,4470,0,7800,5200,0},{3000,4000,4450,0,7800,5200,0}};
/*35°C-40°C*/
struct step_jeita_cycle_cfg jeita_nomal2_fcc_cfg[CC_CV_CYCLE_COUNT_MAX] = {{3000,4100,4480,0,7800,5200,0},{3000,4050,4470,0,7800,5200,0},{3000,4020,4460,0,7800,5200,0},{3000,4000,4440,0,7800,5200,0}};
/*40°C-45°C*/
struct step_jeita_cycle_cfg jeita_nomal3_fcc_cfg[CC_CV_CYCLE_COUNT_MAX] = {{3000,4100,4470,0,7800,5200,0},{3000,4050,4460,0,7800,5200,0},{3000,4020,4450,0,6000,5400,0},{3000,4000,4430,0,7800,5200,0}};

/*other nomal cycle*/
struct step_jeita_normal_cfg jeita_nomal1_cfg[CC_CV_CYCLE_COUNT_MAX] = {{4490,5400},{4480,5400},{4470,5400},{4450,4320}};
struct step_jeita_normal_cfg jeita_nomal2_cfg[CC_CV_CYCLE_COUNT_MAX] = {{4480,5400},{4470,5400},{4460,5400},{4440,4320}};
struct step_jeita_normal_cfg jeita_nomal3_cfg[CC_CV_CYCLE_COUNT_MAX] = {{4470,5400},{4460,5400},{4450,5400},{4430,4320}};

struct step_jeita_ffc_iterm_cfg jeita_ffc_iterm_cfg [CC_CV_TEMP_STEP_MAX] = {{1618,1460,1546},{2474,2157,2136},{3358,2878,2878}};	//{1494,1270,1344.6},{2312,2016,1942.2},{3138,2690,2689.2}all*1.1 0%~20, >2A*1.07 0%~10%
/*cycle count end*/
bool get_warm_stop_charge_state(void)
{
	return warm_stop_charge;
}
EXPORT_SYMBOL(get_warm_stop_charge_state);

static bool is_batt_available(struct lx_jeita_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct lx_jeita_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_input_present(struct lx_jeita_info *chip)
{
	int rc = 0, input_present = 0;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0)
			lx_err("Couldn't read USB Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

int lx_step_jeita_get_index(struct step_jeita_cfg *cfg, int value)
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

static void judge_warm_stop_charge(struct lx_jeita_info *chip, int temp, int vbat)
{
	if (temp >= TEMP_LEVEL_45/* && vbat >= TEMP_45_TO_56_VOL*/)
		warm_stop_charge = true;

	if (warm_stop_charge) {
		if (temp <= (TEMP_LEVEL_45 - WARM_RECHG_TEMP_OFFSET) || vbat < (TEMP_45_TO_56_VOL - WARM_RECHG_VOLT_OFFSET))
			warm_stop_charge = false;
	}
}

static int fast_charge_status(int temp)
{
	int fastcharge_mode = 0;
	static int pre_fastcharge_mode = -1;
	static bool flag = false;
	if ((temp > TEMP_LEVEL_20 + WARM_RECHG_TEMP_OFFSET) && (temp < TEMP_LEVEL_45 - WARM_RECHG_TEMP_OFFSET)) {
		fastcharge_mode = 1;
		flag = false;
	} else if ((temp > TEMP_LEVEL_20) && (temp < TEMP_LEVEL_45)) {
		if (flag) {
			fastcharge_mode = 0;
		} else {
			fastcharge_mode = 1;
			flag = false;
		}
	} else {
		fastcharge_mode = 0;
	}

	if (pre_fastcharge_mode == -1) {
		pre_fastcharge_mode = fastcharge_mode;
	} else {
		if (pre_fastcharge_mode == 1 && pre_fastcharge_mode != fastcharge_mode) {
			flag = true;
		}
	}
	pre_fastcharge_mode = fastcharge_mode;
	lx_err("fastcharge_mode = %d flag = %d pre_fastcharge_mode = %d\n", fastcharge_mode,flag ,pre_fastcharge_mode);
	return fastcharge_mode;
}
#include "../fuelgauge/bq28z610.h"
static int handle_jeita(struct lx_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int ret = 0;
	int i = 0;
	int j = 0;
	int temp_now, vol_now;
	int eu_mode = -EINVAL;
	int curr_offset = 0;
	static bool cold_curr_lmt = false;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int cyclecount;
	int battery_id;
	int fastcharge_mode = 0;
	static int turnnormal_flag = 0;
	int fv_temp = 0;
	struct bq_fg_chip *bq = NULL;
	bool usb_online = 0;
	struct charger_manager *manager;
	int capacity;
	int normal_fv = 0;
	int ffc_fv =0;
	static int temp_stop_flag = 0;

	if (!is_batt_available(chip)) {
		pr_err("failed to get batt psy\n");
		return 0;
	}

	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	chip->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chip->pd_adapter)
		lx_err("failed to pd_adapter\n");
	else
		chip->pd_verifed = chip->pd_adapter->verifed;
	#endif
	if (chip->fuel_gauge == NULL)
		chip->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (IS_ERR_OR_NULL(chip->fuel_gauge)) {
		lx_err("failed to get fuel_gauge\n");
	}
	bq = fuel_gauge_get_private(chip->fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		lx_err("failed to get bp\n");
		return 0;
	}
	if (chip->charger == NULL)
		chip->charger = charger_find_dev_by_name("primary_chg");
	if (IS_ERR_OR_NULL(chip->charger)) {
		lx_err("failed to get main charger\n");
		return 0;
	}
	manager = (struct charger_manager *)power_supply_get_drvdata(chip->batt_psy);
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return 0;
	}
	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt temp, ret=%d\n", ret);
	}
	if (pval.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
		cold_curr_lmt = false;
		return 0;
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt temp, ret=%d\n", ret);
	}
	temp_now = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt voltage_now, ret=%d\n", ret);
	}
	vol_now = pval.intval / 1000;

	if (temp_now < 0) {
		charger_set_rechg_volt(chip->charger, LOW_TEMP_RECHG_OFFSET);
	} else {
		charger_set_rechg_volt(chip->charger, NOR_TEMP_RECHG_OFFSET);
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt capacity, ret=%d\n", ret);
	}
	capacity = pval.intval;

	battery_id = fuel_gauge_get_battery_id(chip->fuel_gauge);

	charger_get_online(chip->charger, &usb_online);
	if (capacity <= 90 || !usb_online)
		turnnormal_flag = false;

	if (chip->pd_verifed) {
		fastcharge_mode = fast_charge_status(temp_now);
		if (capacity >= 95 && manager->first_plug == 1 && fastcharge_mode == 1) {
			fastcharge_mode = 0;
			lx_info("capacity > 95, set fastcharge_mode 0\n");
		}
		if (turnnormal_flag && fastcharge_mode == 1) {
			fastcharge_mode = 0;
			lx_info("capacity > 90 and total_fcc low than ffc_iterm, set fastcharge_mode 0\n");
		}
		fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, fastcharge_mode);
	} else {
		fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, false);
	}
	judge_warm_stop_charge(chip, temp_now, vol_now);
	charger_is_charge_done(chip->charger, &(chip->is_charge_done));

	if (temp_now < TEMP_LEVEL_NEGATIVE_10 || temp_now >= TEMP_LEVEL_56) {
		temp_stop_flag = 1;
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		lx_info("temp_now < TEMP_LEVEL_NEGATIVE_10 || temp_now > TEMP_LEVEL_56, stop charge");
		return 0;
	} else if (temp_stop_flag == 1) {
		if (temp_now >= (TEMP_LEVEL_NEGATIVE_10 + WARM_RECHG_TEMP_OFFSET) && temp_now <= (TEMP_LEVEL_56 - WARM_RECHG_TEMP_OFFSET_30)) {
			temp_stop_flag = 0;
		}
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		lx_info("temp_now is WARM_RECHG_TEMP_OFFSET ,so stop charge");
		return 0;
	}

	if (warm_stop_charge) {
		if(vol_now >= TEMP_45_TO_56_VOL)
			vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		if (battery_id == 1) {
			vote(chip->iterm_votable, ITER_VOTER, true, ATL_ITERM);
		} else {
			vote(chip->iterm_votable, ITER_VOTER, true, 600);
		}
		vote(chip->fv_votable, JEITA_VOTER, true, 4100);
		charger_set_term(manager->charger, true);
		lx_info("warm_stop_charge = true, stop charge1");
		return 0;
	}

	chip->jeita_index = lx_step_jeita_get_index(chip->jeita_fcc_cfg, temp_now);
	//vbat < 4.2V and -10 < temp <0，ichg = 1A
	if (chip->jeita_index == 0 && (vol_now < NAGETIVE_10_TO_0_VOL_4200)) {
		if (cold_curr_lmt) { //if limited,need vbat below 4.1V,than set ichg = 1A
			if ((vol_now < (NAGETIVE_10_TO_0_VOL_4200 - COLD_RECHG_VOLT_OFFSET))) {
				curr_offset = chip->under_4200_curr_offset;
				cold_curr_lmt = false;
			}
		} else
			curr_offset = chip->under_4200_curr_offset;
	} else { //vbat >= 4.2V, ichg = 0.7A
		curr_offset = 0;
		cold_curr_lmt = true;
	}
	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt voltage_now, ret=%d\n", ret);
	}
	cyclecount = pval.intval;
	ret = charger_get_vbus_type(chip->charger, &vbus_type);
	if (ret < 0){
		lx_err("Couldn't get charger type ret=%d\n", ret);
		return -EINVAL;
	}
	chip->fv = chip->jeita_fv_cfg[chip->jeita_index].value;

	while (cyclecount >= cc_cv_cycle_count[j]) {
		++j;
		if (j >= CC_CV_CYCLE_COUNT_MAX)
			break;
	}

	i = j-1;
	eu_mode = get_eu_mode();
	lx_info("cc_cv_cycle_count_index = %d  cyclecount = %d eu_mode = %d batt_id = %d\n", i, cyclecount , eu_mode, battery_id);
	if ((chip->jeita_index == INDEX_20_to_35) || (chip->jeita_index == INDEX_35_to_40) || (chip->jeita_index == INDEX_40_to_45)) {
		ffc_fv =  jeita_FFC_cfg_20_45[i].vbat4;
	}
	if (eu_mode == true) {
		if (chip->jeita_index == INDEX_20_to_35) {
			normal_fv = jeita_nomal1_fcc_cfg[i].vbat3;
		} else if (chip->jeita_index == INDEX_35_to_40) {
			normal_fv = jeita_nomal2_fcc_cfg[i].vbat3;
		} else if (chip->jeita_index == INDEX_40_to_45) {
			normal_fv = jeita_nomal3_fcc_cfg[i].vbat3;
		}
	} else {
		if (chip->jeita_index == INDEX_20_to_35) {
			normal_fv = jeita_nomal1_cfg[i].cv_val;
		} else if (chip->jeita_index == INDEX_35_to_40) {
			normal_fv = jeita_nomal2_cfg[i].cv_val;
		} else if (chip->jeita_index == INDEX_40_to_45) {
			normal_fv = jeita_nomal3_cfg[i].cv_val;
		}
	}
	/*FFC: fcc*/
	if (chip->pd_verifed && fastcharge_mode) {
		switch (chip->jeita_index) {
			case INDEX_0_to_5:
				if (jeita_other_cfg_0_5.vbat1 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat2) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val1);
				} else if (jeita_other_cfg_0_5.vbat2 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat3) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val2);
				}
				break;
			case INDEX_5_to_10:
				if (jeita_other_cfg_5_10.vbat1 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat2) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val1);
				} else if (jeita_other_cfg_5_10.vbat2 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat3) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val2);
				}
				break;
			case INDEX_10_to_15:
				if (jeita_other_cfg_10_15.vbat1 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat2) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val1);
				} else if (jeita_other_cfg_10_15.vbat2 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat3) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val2);
				}
				break;
			case INDEX_20_to_35:
			case INDEX_35_to_40:
			case INDEX_40_to_45:
				if (jeita_FFC_cfg_20_45[i].vbat1 <= vol_now && vol_now <= jeita_FFC_cfg_20_45[i].vbat2) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_FFC_cfg_20_45[i].cur_val1);
				} else if(jeita_FFC_cfg_20_45[i].vbat2 <= vol_now && vol_now <= jeita_FFC_cfg_20_45[i].vbat3) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_FFC_cfg_20_45[i].cur_val2);
				} else if(jeita_FFC_cfg_20_45[i].vbat3 <= vol_now && vol_now <= jeita_FFC_cfg_20_45[i].vbat4) {
					vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_FFC_cfg_20_45[i].cur_val3);
				}
				chip->fv = ffc_fv;
				break;
			default:
				vote(chip->total_fcc_votable, JEITA_VOTER, true, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset));
				break;
		}
	} else {
		if (eu_mode == true) {
			switch (chip->jeita_index) {
				case INDEX_0_to_5:
					if (jeita_other_cfg_0_5.vbat1 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val1);
					} else if (jeita_other_cfg_0_5.vbat2 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val2);
					}
					break;
				case INDEX_5_to_10:
					if (jeita_other_cfg_5_10.vbat1 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val1);
					} else if (jeita_other_cfg_5_10.vbat2 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val2);
					}
					break;
				case INDEX_10_to_15:
					if (jeita_other_cfg_10_15.vbat1 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val1);
					} else if (jeita_other_cfg_10_15.vbat2 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val2);
					}
					break;
				case INDEX_20_to_35:
					if (jeita_nomal1_fcc_cfg[i].vbat1 <= vol_now && vol_now <= jeita_nomal1_fcc_cfg[i].vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal1_fcc_cfg[i].cur_val1);
					} else if(jeita_nomal1_fcc_cfg[i].vbat2 <= vol_now && vol_now <= jeita_nomal1_fcc_cfg[i].vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal1_fcc_cfg[i].cur_val2);
					}
					chip->fv = normal_fv;
					break;
				case INDEX_35_to_40:
					if (jeita_nomal2_fcc_cfg[i].vbat1 <= vol_now && vol_now <= jeita_nomal2_fcc_cfg[i].vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal2_fcc_cfg[i].cur_val1);
					} else if(jeita_nomal2_fcc_cfg[i].vbat2 <= vol_now && vol_now <= jeita_nomal2_fcc_cfg[i].vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal2_fcc_cfg[i].cur_val2);
					}
					chip->fv = normal_fv;
					break;
				case INDEX_40_to_45:
					if (jeita_nomal3_fcc_cfg[i].vbat1 <= vol_now && vol_now <= jeita_nomal3_fcc_cfg[i].vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal3_fcc_cfg[i].cur_val1);
					} else if(jeita_nomal3_fcc_cfg[i].vbat2 <= vol_now && vol_now <= jeita_nomal3_fcc_cfg[i].vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal3_fcc_cfg[i].cur_val2);
					}
					chip->fv = normal_fv;
					break;
				default:
					vote(chip->total_fcc_votable, JEITA_VOTER, true, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset));
					break;
			}
		} else {
			switch (chip->jeita_index) {
				case INDEX_0_to_5:
					if (jeita_other_cfg_0_5.vbat1 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val1);
					} else if (jeita_other_cfg_0_5.vbat2 <= vol_now && vol_now <= jeita_other_cfg_0_5.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_0_5.cur_val2);
					}
					break;
				case INDEX_5_to_10:
					if (jeita_other_cfg_5_10.vbat1 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val1);
					} else if (jeita_other_cfg_5_10.vbat2 <= vol_now && vol_now <= jeita_other_cfg_5_10.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_5_10.cur_val2);
					}
					break;
				case INDEX_10_to_15:
					if (jeita_other_cfg_10_15.vbat1 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat2) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val1);
					} else if (jeita_other_cfg_10_15.vbat2 <= vol_now && vol_now <= jeita_other_cfg_10_15.vbat3) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_other_cfg_10_15.cur_val2);
					}
					break;
				case INDEX_20_to_35:
					if (vol_now >= CC_CV_CYCLE_COUNT_VOLT && vol_now <= jeita_nomal1_cfg[i].cv_val) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal1_cfg[i].cur_val);
					}
					chip->fv = normal_fv;
					break;
				case INDEX_35_to_40:
					if (vol_now >= CC_CV_CYCLE_COUNT_VOLT && vol_now <= jeita_nomal2_cfg[i].cv_val) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal2_cfg[i].cur_val);
					}
					chip->fv = normal_fv;
					break;
				case INDEX_40_to_45:
					if (vol_now >= CC_CV_CYCLE_COUNT_VOLT && vol_now <= jeita_nomal3_cfg[i].cv_val) {
						vote(chip->total_fcc_votable, JEITA_VOTER, true, jeita_nomal3_cfg[i].cur_val);
					}
					chip->fv = normal_fv;
					break;
				default:
					vote(chip->total_fcc_votable, JEITA_VOTER, true, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset));
					break;
			}
		}
	}

	if (battery_id == 1) {
		chip->iterm_curr = ATL_ITERM;
	} else
		chip->iterm_curr = chip->iterm;
	/*FFC: iterm + fv*/
	if (fastcharge_mode) {
		if (chip->jeita_index == INDEX_20_to_35) {
			i = 0 ;
		} else if (chip->jeita_index == INDEX_35_to_40) {
			i = 1;
		} else if (chip->jeita_index == INDEX_40_to_45) {
			i = 2;
		}
		if (battery_id == 1) {
			chip->iterm_ffc = jeita_ffc_iterm_cfg[i].bat1_iterm;
		} else if (battery_id == 2) {
			chip->iterm_ffc = jeita_ffc_iterm_cfg[i].bat2_iterm;
		}else if (battery_id == 3) {
			chip->iterm_ffc = jeita_ffc_iterm_cfg[i].bat3_iterm;
		}
		if (chip->jeita_index == INDEX_20_to_35) {
			chip->iterm_curr = chip->iterm_ffc;
		} else if (chip->jeita_index == INDEX_35_to_40) {
			chip->iterm_curr = chip->iterm_ffc;
		} else if (chip->jeita_index == INDEX_40_to_45) {
			chip->iterm_curr = chip->iterm_ffc;
		}
		if (chip->is_charge_done)
			chip->fv = normal_fv;

		ret = get_effective_result(chip->total_fcc_votable);
		if (ret < 0)
			lx_err("failed to get total_fcc_votable\n");
		if ((capacity > 90) && (ret < chip->iterm_curr))
			turnnormal_flag = true;
	}

	ret = get_effective_result(chip->fv_votable);
	if (ret < 0) {
		lx_err("failed to get fv_votable\n");
	} else {
		fv_temp = ret;
		if (fv_temp == TEMP_45_TO_56_VOL && chip->is_charge_done && chip->fv > fv_temp) {
			charger_set_chg(chip->charger, false);
			/* recharge delay */
			msleep(50);
			charger_set_chg(chip->charger, true);
			manager->prp_is_enable = false;
			lx_info("recharge due to fv change \n");
		}
	}
	charger_is_charge_done(chip->charger, &(chip->is_charge_done));
	/*BUGP19A-2410 eea low temp increase iterm 50ma*/
	if (temp_now < 0) {
		chip->iterm_curr = chip->iterm_curr + 50;
		lx_info("low temp iterm increase 50, tmp = %d, iterm = %d\n", temp_now, chip->iterm_curr);
	}
	vote(chip->iterm_votable, ITER_VOTER, true, chip->iterm_curr);
	vote(chip->fv_votable, JEITA_VOTER, true, chip->fv);

	lx_debug("pd_verifed = %d %d %d cp_charge_done = %d, jeita_index = %d, curr_offset = %d, battery_id = %d ,is_charge_done = %d ibat = %d\n",
				chip->pd_verifed, usb_online,vbus_type, g_policy->cp_charge_done, chip->jeita_index, curr_offset, battery_id, chip->is_charge_done, manager->ibat / 1000);
	lx_debug("step_chg_fcc = %d, jeita_fcc = %d, jeita_fv = %d, jeita_iterm = %d fv_temp = %d rsoc = %d\n",
				chip->step_chg_cfg[i].value, ((chip->jeita_fcc_cfg[chip->jeita_index].value) + curr_offset), chip->fv, chip->iterm_curr, fv_temp, bq->raw_soc);
	return 0;
}

static void status_change_work(struct work_struct *work)
{
	struct lx_jeita_info*chip = container_of(work,
			struct lx_jeita_info, status_change_work.work);
	int rc = 0;

	if (!is_batt_available(chip)|| !is_bms_available(chip))
		goto exit_work;

	rc = handle_jeita(chip);
	if (rc < 0)
		lx_err("Couldn't handle sw jeita rc = %d\n", rc);

	if (! is_input_present(chip)) {
		vote(chip->main_icl_votable, JEITA_VOTER, false, 0);
	}

exit_work:
	__pm_relax(chip->lx_jeita_ws);
}

static int jeita_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct lx_jeita_info*chip = container_of(nb, struct lx_jeita_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->lx_jeita_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int jeita_register_notifier(struct lx_jeita_info *chip)
{
	int rc;

	chip->nb.notifier_call = jeita_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		lx_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static bool lx_parse_jeita_dt(struct device_node *node, struct lx_jeita_info *chip)
{
	int total_length = 0;
	bool ret = 0;
	int i = 0;

	total_length = of_property_count_elems_of_size(node, "jeita_fcc_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of jeita_fcc_cfg\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_fcc_cfg", (u32 *)chip->jeita_fcc_cfg, total_length);
	if (ret) {
		lx_err("failed to parse jeita_fcc_cfg\n");
		return 0;
	}

	total_length = of_property_count_elems_of_size(node, "jeita_fv_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of jeita_fv_cfg\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_fv_cfg", (u32 *)chip->jeita_fv_cfg, total_length);
	if (ret) {
		lx_err("failed to parse jeita_fv_cfg\n");
		return 0;
	}

	total_length = of_property_count_elems_of_size(node, "jeita_bypass_high_lmt_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of jeita_bypass_high_lmt_cfg\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_bypass_high_lmt_cfg", (u32 *)chip->jeita_bypass_high_lmt_cfg, total_length);
	if (ret) {
		lx_err("failed to parse jeita_bypass_high_lmt_cfg\n");
		return 0;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
		lx_info("[jeita_fcc_cfg]%d %d %d [jeita_fv_cfg]%d %d %d [jeita_bypass_high_lmt_cfg]%d %d %d\n",
					chip->jeita_fcc_cfg[i].low_threshold, chip->jeita_fcc_cfg[i].high_threshold, chip->jeita_fcc_cfg[i].value,
					chip->jeita_fv_cfg[i].low_threshold, chip->jeita_fv_cfg[i].high_threshold, chip->jeita_fv_cfg[i].value,
					chip->jeita_bypass_high_lmt_cfg[i].low_threshold, chip->jeita_bypass_high_lmt_cfg[i].high_threshold, chip->jeita_bypass_high_lmt_cfg[i].value);

	ret |= of_property_read_u32(node, "iterm", &chip->iterm);

	ret |= of_property_read_u32(node, "under_4200_curr_offset", &chip->under_4200_curr_offset);
	ret |= of_property_read_u32(node, "fv_offset_15_to_35", &chip->fv_offset_15_to_35);
	ret |= of_property_read_u32(node, "fv_offset_35_to_48", &chip->fv_offset_35_to_48);

	return !ret;
}

static bool lx_parse_ffc_dt(struct device_node *node, struct lx_jeita_info *chip)
{
	int total_length = 0;
	int i = 0;
	bool ret = 0;

	total_length = of_property_count_elems_of_size(node, "step_chg_cfg_cycle", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of step_chg_cfg_cycle\n");
		return 0;
	}

	ret |= of_property_read_u32_array(node, "step_chg_cfg_cycle", (u32 *)chip->step_chg_cfg, total_length);
	if (ret)
	{
		lx_err("failed to parse step_chg_cfg_cycle\n");
		return false;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
		lx_info("[STEP_CHG] [step_chg_cfg_cycle]%d %d %d\n",
					chip->step_chg_cfg[i].low_threshold, chip->step_chg_cfg[i].high_threshold, chip->step_chg_cfg[i].value);

	return !ret;
}
struct lx_jeita_info *get_jeita_info(void){
	return the_chip;
}
int lx_jeita_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct device_node *step_jeita_node = NULL;
	struct lx_jeita_info *chip = NULL;

	int rc = 0;

	if (node) {
		step_jeita_node = of_find_node_by_name(node, "step_jeita");
	}

	if (the_chip) {
		lx_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->lx_jeita_ws = wakeup_source_register(dev, "lx_jeita");
	if (!chip->lx_jeita_ws)
		return -EINVAL;

	chip->dev = dev;

	rc = lx_parse_jeita_dt(step_jeita_node, chip);
	if(!rc)
		lx_err("lx_parse_jeita_dt failed\n");

	rc = lx_parse_ffc_dt(step_jeita_node, chip);
	if(!rc)
		lx_err("lx_parse_ffc_dt failed\n");

	chip->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!chip->total_fcc_votable)
		lx_err("find TOTAL_FCC voltable failed\n");

	chip->fv_votable = find_votable("MAIN_FV");
	if (!chip->fv_votable)
		lx_err("find MAIN_FV voltable failed\n");

	chip->iterm_votable = find_votable("MAIN_ITERM");
	if (!chip->iterm_votable)
		lx_err("find MAIN_FV voltable failed\n");

	chip->charger = charger_find_dev_by_name("primary_chg");
	if (chip->charger == NULL) {
		lx_err("failed get charger\n");
	}

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	rc = jeita_register_notifier(chip);
	if (rc < 0) {
		lx_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	the_chip = chip;

	lx_info("lx_jeita_init success\n");

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->lx_jeita_ws);
	return rc;
}

void lx_jeita_deinit(void)
{
	struct lx_jeita_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->lx_jeita_ws);
	the_chip = NULL;
}
