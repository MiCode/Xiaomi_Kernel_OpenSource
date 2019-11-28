/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef BUILD_POLICY_TEST
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#endif

#include "apu_power_api.h"
#include "apusys_power_debug.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power.h"
#include "apu_power_table.h"
#include "hal_config_power.h"
#include "apu_log.h"

struct apusys_dvfs_opps apusys_opps;



bool dvfs_user_support(enum DVFS_USER user)
{
	return apusys_dvfs_user_support[user];
}

bool dvfs_power_domain_support(enum DVFS_VOLTAGE_DOMAIN domain)
{
	return apusys_dvfs_buck_domain_support[domain];
}


int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER user,
					enum APU_OPP_INDEX opp)
{
	// need to check constraint voltage, fixed me

	apusys_opps.thermal_opp[user] = opp;
	PWR_LOG_INF("%s, user=%d, vpu_opp=%d\n", __func__, user, opp);

	return 0;
}

int32_t apusys_thermal_dis_throttle_cb(enum DVFS_USER user)
{
	apusys_opps.thermal_opp[user] = 0;
	PWR_LOG_INF("%s, user=%d\n", __func__, user);

	return 0;
}

uint8_t apusys_boost_value_to_opp(enum DVFS_USER user, uint8_t boost_value)
{
	uint8_t i = 0;
	uint8_t opp = APUSYS_MAX_NUM_OPPS-1;
	uint32_t max_freq = 0, freq = 0;
	enum DVFS_VOLTAGE_DOMAIN buck_domain = apusys_user_to_buck_domain[user];

		if (boost_value >= 100) {
			opp	= 0;
		} else {
			max_freq =
					apusys_opps.opps[0][buck_domain].freq;
		    freq = boost_value * max_freq / 100;

		for (i = 1; i < APUSYS_MAX_NUM_OPPS; i++) {
			if (freq > apusys_opps.opps[i][buck_domain].freq) {
				opp = i-1;
				break;
			}
		}
	}
	  PWR_LOG_INF(
		"%s, user=%d, boost_value=%d,max_freq=%d, freq=%d, opp=%d\n",
		__func__, user, boost_value, max_freq, freq, opp);
	return opp;

}
EXPORT_SYMBOL(apusys_boost_value_to_opp);


enum DVFS_FREQ apusys_opp_to_freq(enum DVFS_USER user, uint8_t opp)
{
	enum DVFS_FREQ freq = DVFS_FREQ_NOT_SUPPORT;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return freq;

	buck_domain = apusys_user_to_buck_domain[user];

	if (opp >= 0 && opp < APUSYS_MAX_NUM_OPPS)
		freq = apusys_opps.opps[opp][buck_domain].freq;

	return freq;
}
EXPORT_SYMBOL(apusys_opp_to_freq);

uint8_t apusys_freq_to_opp(enum DVFS_VOLTAGE_DOMAIN buck_domain, uint32_t freq)
{
	uint8_t opp = 0;
	uint32_t next_freq = 0;

	for (opp = 0 ; opp < APUSYS_MAX_NUM_OPPS - 1 ; opp++) {
		next_freq = apusys_opps.opps[opp+1][buck_domain].freq + 1;
		if (freq >= next_freq &&
			freq <= (apusys_opps.opps[opp][buck_domain].freq + 1))
			break;
	}

	return opp;
}
EXPORT_SYMBOL(apusys_freq_to_opp);

int8_t apusys_get_opp(enum DVFS_USER user)
{
	enum DVFS_VOLTAGE_DOMAIN buck_domain;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return -1;

	buck_domain = apusys_user_to_buck_domain[user];

	return apusys_opps.cur_opp_index[buck_domain];
}
EXPORT_SYMBOL(apusys_get_opp);


int8_t apusys_get_ceiling_opp(enum DVFS_USER user)
{
	uint8_t used_opp;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return -1;

	buck_domain = apusys_user_to_buck_domain[user];
	used_opp = apusys_opps.cur_opp_index[buck_domain];

	// upper bound for power hal
	used_opp = MAX(used_opp, apusys_opps.power_lock_min_opp[user]);
	// lower bound for power hal
	used_opp = MIN(used_opp, apusys_opps.power_lock_max_opp[user]);

	// upper bound for thermal
	used_opp = MAX(used_opp, apusys_opps.thermal_opp[user]);

	return used_opp;
}
EXPORT_SYMBOL(apusys_get_ceiling_opp);


void apusys_set_pwr_lock(enum DVFS_USER user, uint8_t min_opp, uint8_t max_opp)
{
	apusys_opps.power_lock_min_opp[user] = min_opp;
	apusys_opps.power_lock_max_opp[user] = max_opp;
	PWR_LOG_INF("%s, user=%d, min_opp=%d, max_opp=%d,\n",
		__func__, user, min_opp, max_opp);
}


uint8_t apusys_pwr_max_min_check(enum DVFS_USER user, uint8_t opp)
{
	uint8_t used_opp = opp;

	// upper bound for power hal
	used_opp = MAX(used_opp, apusys_opps.power_lock_min_opp[user]);
	// lower bound for power hal
	used_opp = MIN(used_opp, apusys_opps.power_lock_max_opp[user]);

	// upper bound for thermal
	used_opp = MAX(used_opp, apusys_opps.thermal_opp[user]);

	if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
		PWR_LOG_INF(
	"%s, %s, used_opp=%d,thermal_opp=%d,pwr_lock_min=%d,pwr_lock_max=%d\n",
		__func__,
		user_str[user],
		used_opp,
		apusys_opps.thermal_opp[user],
		apusys_opps.power_lock_min_opp[user],
		apusys_opps.power_lock_max_opp[user]);

	return used_opp;
}


void apusys_clk_path_update_pwr(enum DVFS_USER user, enum DVFS_VOLTAGE voltage)
{
	uint8_t  path_volt_index = 0;

	if (apusys_opps.is_power_on[user] == false)
		return;

	for (path_volt_index = 0;
	path_volt_index < APUSYS_PATH_USER_NUM; path_volt_index++){
		apusys_opps.user_path_volt[user][path_volt_index] =
			(voltage >
			dvfs_clk_path_max_vol[user][path_volt_index] ?
			dvfs_clk_path_max_vol[user][path_volt_index] :
			voltage);

		if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
			PWR_LOG_INF("%s, user_path_volt[%s][%d]=%d\n",
			__func__,
			user_str[user],
			path_volt_index,
			apusys_opps.user_path_volt[user][path_volt_index]);
	}
}


void apusys_final_volt_check(void)
{
	uint8_t  user_index = 0, path_index = 0, buck_index = 0;

// for every buck domain, check clk path matrix for buck shared relation
for (buck_index = 0;
buck_index < APUSYS_BUCK_NUM; buck_index++) {
	#if !VCORE_DVFS_SUPPORT
	if (buck_index == VCORE_BUCK)
		continue;
	#endif
	for (user_index = 0; user_index < APUSYS_DVFS_USER_NUM; user_index++) {
		if (dvfs_user_support(user_index) == false)
			continue;
		if (apusys_opps.is_power_on[user_index] == false)
			continue;
		for (path_index = 0;
		path_index < APUSYS_PATH_USER_NUM; path_index++) {
			if (buck_shared[buck_index][user_index][path_index]
		== true) {
				apusys_opps.next_buck_volt[buck_index] =
		MAX(apusys_opps.next_buck_volt[buck_index],
		apusys_opps.user_path_volt[user_index][path_index]);

	if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
		PWR_LOG_INF("%s, %s = %d,(%s,%d)=%d\n",
		__func__,
		buck_str[buck_index],
		apusys_opps.next_buck_volt[buck_index],
		user_str[user_index],
		path_index,
		apusys_opps.user_path_volt[user_index][path_index]);
			}
			}
		}
	}
}


void apusys_pwr_constraint_check(void)
{
	uint8_t i = 0;
	int8_t opp_index = APUSYS_MAX_NUM_OPPS-1;
	enum DVFS_BUCK buck0;
	enum DVFS_BUCK buck1;
	enum DVFS_VOLTAGE voltage0;
	enum DVFS_VOLTAGE voltage1;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;
	bool vcore_constraint = false;

	for (i = 0; i < APUSYS_DVFS_CONSTRAINT_NUM; i++) {
		buck0 = dvfs_constraint_table[i].buck0;
	    buck1 = dvfs_constraint_table[i].buck1;
	    voltage0 = dvfs_constraint_table[i].voltage0;
	    voltage1 = dvfs_constraint_table[i].voltage1;
if (apusys_opps.next_buck_volt[buck0] == voltage0 &&
	apusys_opps.next_buck_volt[buck1] == voltage1) {
	if (buck0 == VCORE_BUCK)
		vcore_constraint = true;
	for (opp_index = APUSYS_MAX_NUM_OPPS-1;
		opp_index >= 0; opp_index--) {
		if (voltage0 < voltage1) {
			buck_domain = apusys_buck_to_buck_domain[buck0];
			if (apusys_opps.opps[opp_index][buck_domain].voltage
	> voltage0) {
				apusys_opps.next_buck_volt[buck0] =
			apusys_opps.opps[opp_index][buck_domain].voltage;

				PWR_LOG_INF("%s, %s from %d --> %d\n",
				__func__,
				buck_str[buck0],
				voltage0,
			apusys_opps.next_buck_volt[buck0]);
				break;
				}
		} else if (voltage0 > voltage1) {
			buck_domain = apusys_buck_to_buck_domain[buck1];
			if (apusys_opps.opps[opp_index][buck_domain].voltage
			> voltage1) {
				apusys_opps.next_buck_volt[buck1] =
			apusys_opps.opps[opp_index][buck_domain].voltage;

				PWR_LOG_INF("%s, %s from %d --> %d\n",
				__func__,
				buck_str[buck1],
				voltage1,
			apusys_opps.next_buck_volt[buck1]);
				break;
				}
			}
		}
	}
}

	if (vcore_constraint == false)
		apusys_opps.next_buck_volt[VCORE_BUCK] = VCORE_DEFAULT_VOLT;

}


void apusys_pwr_efficiency_check(void)
{
	uint8_t buck_domain_index = 0;
	uint8_t buck_index = 0;
	uint8_t opp_index = 0;
	enum DVFS_USER user;

	for (buck_domain_index = 0;
	buck_domain_index < APUSYS_BUCK_DOMAIN_NUM; buck_domain_index++) {
		if (dvfs_power_domain_support(buck_domain_index) == false)
			continue;
		buck_index = apusys_buck_domain_to_buck[buck_domain_index];
	for (opp_index = 0;	opp_index < APUSYS_MAX_NUM_OPPS; opp_index++) {
		if (apusys_opps.opps[opp_index][buck_domain_index].voltage <=
			apusys_opps.next_buck_volt[buck_index]){
			user = apusys_buck_domain_to_user[buck_domain_index];
			if (user < APUSYS_DVFS_USER_NUM) {
				if (apusys_opps.is_power_on[user] == false)
					continue;
				apusys_opps.next_opp_index[buck_domain_index] =
				apusys_pwr_max_min_check(user, opp_index);
			} else {
				apusys_opps.next_opp_index[buck_domain_index] =
				opp_index;
			}
			if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
				PWR_LOG_INF("%s, %s, opp=%d\n",
			__func__,
			buck_domain_str[buck_domain_index],
			apusys_opps.next_opp_index[buck_domain_index]);
			break;
			}
		}
	}
}


void apusys_buck_up_check(void)
{
	uint8_t user = 0;  // don't care
	uint8_t buck_small_index = 0, buck_large_index = 0;
	struct hal_param_volt volt_data;

	if (apusys_opps.cur_buck_volt[VCORE_BUCK] <
			apusys_opps.next_buck_volt[VCORE_BUCK]) {
		volt_data.target_buck = VCORE_BUCK;
		volt_data.target_volt = apusys_opps.next_buck_volt[VCORE_BUCK];

		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.cur_buck_volt[VPU_BUCK] <
		apusys_opps.cur_buck_volt[MDLA_BUCK]) {
		buck_small_index = VPU_BUCK;
		buck_large_index = MDLA_BUCK;
	} else if (apusys_opps.cur_buck_volt[VPU_BUCK] >
		apusys_opps.cur_buck_volt[MDLA_BUCK]){
		buck_small_index = MDLA_BUCK;
		buck_large_index = VPU_BUCK;
	} else {
		if (apusys_opps.next_buck_volt[VPU_BUCK] <
		apusys_opps.next_buck_volt[MDLA_BUCK]){
			buck_small_index = VPU_BUCK;
			buck_large_index = MDLA_BUCK;
		} else {
			buck_small_index = MDLA_BUCK;
			buck_large_index = VPU_BUCK;
		}
	}

if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
	PWR_LOG_INF("%s,cur vpu=%d, cur mdla=%d, vpu=%d, mdla=%d, vsrm=%d\n",
		__func__,
		apusys_opps.cur_buck_volt[VPU_BUCK],
		apusys_opps.cur_buck_volt[MDLA_BUCK],
		apusys_opps.next_buck_volt[VPU_BUCK],
		apusys_opps.next_buck_volt[MDLA_BUCK],
		apusys_opps.vsram_volatge);

	if (apusys_opps.vsram_volatge == VSRAM_LOW_VOLT &&
		(apusys_opps.next_buck_volt[VPU_BUCK] > VSRAM_TRANS_VOLT ||
		apusys_opps.next_buck_volt[MDLA_BUCK] > VSRAM_TRANS_VOLT)) {
		if ((apusys_opps.cur_buck_volt[buck_small_index] <
			apusys_opps.next_buck_volt[buck_small_index])) {
			if (apusys_opps.next_buck_volt[buck_small_index] >
				VSRAM_TRANS_VOLT)
				volt_data.target_volt = VSRAM_TRANS_VOLT;
			else
				volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_small_index];
			volt_data.target_buck = buck_small_index;
			if (apusys_opps.cur_buck_volt[buck_small_index] !=
				volt_data.target_volt)
				hal_config_power(PWR_CMD_SET_VOLT, user,
				(void *)&volt_data);
		}

		if ((apusys_opps.cur_buck_volt[buck_large_index] <
			apusys_opps.next_buck_volt[buck_large_index])) {
			if (apusys_opps.next_buck_volt[buck_large_index]
				> VSRAM_TRANS_VOLT)
				volt_data.target_volt = VSRAM_TRANS_VOLT;
			else
				volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_large_index];
			volt_data.target_buck = buck_large_index;
			if (apusys_opps.cur_buck_volt[buck_large_index] !=
				volt_data.target_volt)
				hal_config_power(PWR_CMD_SET_VOLT, user,
				(void *)&volt_data);
		}

		volt_data.target_buck = SRAM_BUCK;
		volt_data.target_volt = VSRAM_HIGH_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		apusys_opps.vsram_volatge = VSRAM_HIGH_VOLT;

		if ((apusys_opps.next_buck_volt[buck_small_index] >
			VSRAM_TRANS_VOLT) &&
		(apusys_opps.next_buck_volt[buck_small_index] >
		apusys_opps.cur_buck_volt[buck_small_index])) {
			volt_data.target_buck = buck_small_index;
			volt_data.target_volt =
			apusys_opps.next_buck_volt[buck_small_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}

		if ((apusys_opps.next_buck_volt[buck_large_index] >
			VSRAM_TRANS_VOLT) &&
			(apusys_opps.next_buck_volt[buck_large_index] >
		apusys_opps.cur_buck_volt[buck_large_index])) {
			volt_data.target_buck = buck_large_index;
			volt_data.target_volt =
			apusys_opps.next_buck_volt[buck_large_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}
	} else {
		if (apusys_opps.next_buck_volt[buck_small_index] >
			apusys_opps.cur_buck_volt[buck_small_index]) {
			volt_data.target_buck = buck_small_index;
			volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_small_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}

		if (apusys_opps.next_buck_volt[buck_large_index] >
			apusys_opps.cur_buck_volt[buck_large_index]) {
			volt_data.target_buck = buck_large_index;
			volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_large_index];
			hal_config_power(PWR_CMD_SET_VOLT, user,
				(void *)&volt_data);
		}
	}
}


void apusys_frequency_check(void)
{
	uint8_t buck_domain_index = 0;
	uint8_t next_opp_index = 0, cur_opp_index = 0;
	struct hal_param_freq  freq_data;
	enum DVFS_USER user;
	bool apusys_power_on = false;

	for (buck_domain_index = 0;
	buck_domain_index < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain_index++) {
		if (dvfs_power_domain_support(buck_domain_index) == false)
			continue;

		user = apusys_buck_domain_to_user[buck_domain_index];
		if (user < APUSYS_DVFS_USER_NUM) {
			if (apusys_opps.is_power_on[user] == false)
				continue;
			else
				apusys_power_on = true;
		} else {
			if (apusys_power_on == false &&
			apusys_opps.is_power_on[EDMA] == false &&
			apusys_opps.is_power_on[EDMA2] == false &&
			apusys_opps.is_power_on[REVISER] == false)
				continue;
		}

		if (apusys_opps.cur_opp_index[buck_domain_index] ==
			apusys_opps.next_opp_index[buck_domain_index])
			continue;

		next_opp_index = apusys_opps.next_opp_index[buck_domain_index];
		cur_opp_index =	apusys_opps.cur_opp_index[buck_domain_index];

		if (apusys_opps.opps[next_opp_index][buck_domain_index].freq !=
		apusys_opps.opps[cur_opp_index][buck_domain_index].freq){
			freq_data.target_volt_domain = buck_domain_index;
		freq_data.target_freq = apusys_opps.opps[next_opp_index]
						[buck_domain_index].freq;
		hal_config_power(PWR_CMD_SET_FREQ, VPU0, (void *)&freq_data);

		if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
			PWR_LOG_INF("%s, %s, freq from %d --> %d\n", __func__,
		buck_domain_str[buck_domain_index],
		apusys_opps.opps[cur_opp_index][buck_domain_index].freq,
		apusys_opps.opps[next_opp_index][buck_domain_index].freq);
		}
	}
}


void apusys_buck_down_check(void)
{
	uint8_t user = 0; //don't care
	uint8_t buck_small_index = 0, buck_large_index = 0;
	struct hal_param_volt volt_data;

	if (apusys_opps.cur_buck_volt[VPU_BUCK] <
		apusys_opps.cur_buck_volt[MDLA_BUCK]) {
		buck_small_index = VPU_BUCK;
		buck_large_index = MDLA_BUCK;
	} else if (apusys_opps.cur_buck_volt[VPU_BUCK] >
		apusys_opps.cur_buck_volt[MDLA_BUCK]){
		buck_small_index = MDLA_BUCK;
		buck_large_index = VPU_BUCK;
	} else {
		if (apusys_opps.next_buck_volt[VPU_BUCK] <
		apusys_opps.next_buck_volt[MDLA_BUCK]){
			buck_small_index = VPU_BUCK;
			buck_large_index = MDLA_BUCK;
		} else {
			buck_small_index = MDLA_BUCK;
			buck_large_index = VPU_BUCK;
		}
	}

if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
	PWR_LOG_INF("%s,cur vpu=%d, cur mdla=%d, vpu=%d, mdla=%d, vsrm=%d\n",
		__func__,
		apusys_opps.cur_buck_volt[VPU_BUCK],
		apusys_opps.cur_buck_volt[MDLA_BUCK],
		apusys_opps.next_buck_volt[VPU_BUCK],
		apusys_opps.next_buck_volt[MDLA_BUCK],
		apusys_opps.vsram_volatge);

	if (apusys_opps.vsram_volatge == VSRAM_HIGH_VOLT &&
		(apusys_opps.next_buck_volt[VPU_BUCK] <= VSRAM_TRANS_VOLT &&
		apusys_opps.next_buck_volt[MDLA_BUCK] <= VSRAM_TRANS_VOLT)) {
		if (apusys_opps.cur_buck_volt[buck_large_index] >
			apusys_opps.next_buck_volt[buck_large_index]){
			if (apusys_opps.next_buck_volt[buck_large_index] <=
				VSRAM_TRANS_VOLT)
				volt_data.target_volt = VSRAM_TRANS_VOLT;
			else
				volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_large_index];
			volt_data.target_buck = buck_large_index;

		if (apusys_opps.cur_buck_volt[buck_large_index] !=
				volt_data.target_volt)
			hal_config_power(PWR_CMD_SET_VOLT, user,
			(void *)&volt_data);
		}
		if (apusys_opps.cur_buck_volt[buck_small_index] >
			apusys_opps.next_buck_volt[buck_small_index]){
			if (apusys_opps.next_buck_volt[buck_small_index] <=
				VSRAM_TRANS_VOLT)
				volt_data.target_volt = VSRAM_TRANS_VOLT;
			else
				volt_data.target_volt =
				apusys_opps.next_buck_volt[buck_small_index];
			volt_data.target_buck = buck_small_index;

		if (apusys_opps.cur_buck_volt[buck_small_index] !=
			volt_data.target_volt)
			hal_config_power(PWR_CMD_SET_VOLT, user,
			(void *)&volt_data);
		}

		volt_data.target_buck = SRAM_BUCK;
		volt_data.target_volt = VSRAM_LOW_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		apusys_opps.vsram_volatge = VSRAM_LOW_VOLT;

	}

	if (apusys_opps.cur_buck_volt[buck_large_index] >
			apusys_opps.next_buck_volt[buck_large_index]) {
		volt_data.target_buck = buck_large_index;
		volt_data.target_volt =
			apusys_opps.next_buck_volt[buck_large_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.cur_buck_volt[buck_small_index] >
			apusys_opps.next_buck_volt[buck_small_index]) {
		volt_data.target_buck = buck_small_index;
		volt_data.target_volt =
			apusys_opps.next_buck_volt[buck_small_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.cur_buck_volt[VCORE_BUCK] >
			apusys_opps.next_buck_volt[VCORE_BUCK]) {
		volt_data.target_buck = VCORE_BUCK;
		volt_data.target_volt = apusys_opps.next_buck_volt[VCORE_BUCK];

		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}
}


void apusys_dvfs_info(void)
{
	char log_str[128];
	uint8_t buck_domain;
	uint8_t cur_opp;
	uint8_t next_opp;
	enum DVFS_USER user;
	int div = 1000;
	uint8_t cur_opp_index[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t next_opp_index[APUSYS_BUCK_DOMAIN_NUM];

	for (buck_domain = 0;
	buck_domain < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain++) {
		cur_opp_index[buck_domain] =
			apusys_opps.cur_opp_index[buck_domain];
		next_opp_index[buck_domain] =
			apusys_opps.next_opp_index[buck_domain];

if (dvfs_power_domain_support(buck_domain) == false) {
	apusys_opps.opps[cur_opp_index[buck_domain]][buck_domain].voltage = 0;
	apusys_opps.opps[next_opp_index[buck_domain]][buck_domain].voltage = 0;
	apusys_opps.opps[cur_opp_index[buck_domain]][buck_domain].freq = 0;
	apusys_opps.opps[next_opp_index[buck_domain]][buck_domain].freq = 0;
	continue;
}

		cur_opp = apusys_opps.cur_opp_index[buck_domain];
		next_opp = apusys_opps.next_opp_index[buck_domain];

		if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG) {
			PWR_LOG_WRN(
				"%s, %s, opp(%d, %d),freq(%d, %d), volt(%d, %d)  %llu\n",
				__func__,
				buck_domain_str[buck_domain],
				cur_opp,
				next_opp,
				apusys_opps.opps[cur_opp][buck_domain].freq,
				apusys_opps.opps[next_opp][buck_domain].freq,
				apusys_opps.opps[cur_opp][buck_domain].voltage,
				apusys_opps.opps[next_opp][buck_domain].voltage,
				apusys_opps.id);

			if (buck_domain == V_VPU0 || buck_domain == V_VPU1
				|| buck_domain == V_VPU2
				|| buck_domain == V_MDLA0
				|| buck_domain == V_MDLA1){
				user = apusys_buck_domain_to_user[buck_domain];
				PWR_LOG_WRN(
				"%s, %s, user_opp=%d,(T=%d, Pmin=%d, Pmax=%d) %llu\n",
					__func__,
					buck_domain_str[buck_domain],
					apusys_opps.driver_opp_index[user],
					apusys_opps.thermal_opp[user],
					apusys_opps.power_lock_min_opp[user],
					apusys_opps.power_lock_max_opp[user],
					apusys_opps.id);
			}
		}
	}
	if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG) {
		PWR_LOG_WRN(
			"%s, next VSRAM=%d, VVPU=%d, VMDLA=%d, VCORE=%d %llu\n",
			__func__,
			apusys_opps.vsram_volatge,
			apusys_opps.next_buck_volt[VPU_BUCK],
			apusys_opps.next_buck_volt[MDLA_BUCK],
			apusys_opps.next_buck_volt[VCORE_BUCK],
			apusys_opps.id);
	} else {
		snprintf(log_str, sizeof(log_str),
			"[(u_op,T,min,max), (%d,%d,%d,%d),(%d,%d,%d,%d),(%d,%d,%d,%d),(%d,%d,%d,%d),(%d,%d,%d,%d)] %llu",
			apusys_opps.driver_opp_index[V_VPU0],
			apusys_opps.thermal_opp[V_VPU0],
			apusys_opps.power_lock_min_opp[V_VPU0],
			apusys_opps.power_lock_max_opp[V_VPU0],
			apusys_opps.driver_opp_index[V_VPU1],
			apusys_opps.thermal_opp[V_VPU1],
			apusys_opps.power_lock_min_opp[V_VPU1],
			apusys_opps.power_lock_max_opp[V_VPU1],
			apusys_opps.driver_opp_index[V_VPU2],
			apusys_opps.thermal_opp[V_VPU2],
			apusys_opps.power_lock_min_opp[V_VPU2],
			apusys_opps.power_lock_max_opp[V_VPU2],
			apusys_opps.driver_opp_index[V_MDLA0],
			apusys_opps.thermal_opp[V_MDLA0],
			apusys_opps.power_lock_min_opp[V_MDLA0],
			apusys_opps.power_lock_max_opp[V_MDLA0],
			apusys_opps.driver_opp_index[V_MDLA1],
			apusys_opps.thermal_opp[V_MDLA1],
			apusys_opps.power_lock_min_opp[V_MDLA1],
			apusys_opps.power_lock_max_opp[V_MDLA1],
			apusys_opps.id);
	PWR_LOG_WRN("APUPWR DVFS %s\n", log_str);

	snprintf(log_str, sizeof(log_str),
	"v[(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d)] %llu",
	apusys_opps.opps[cur_opp_index[V_VPU0]][V_VPU0].voltage/div,
	apusys_opps.opps[next_opp_index[V_VPU0]][V_VPU0].voltage/div,
	apusys_opps.opps[cur_opp_index[V_VPU1]][V_VPU1].voltage/div,
	apusys_opps.opps[next_opp_index[V_VPU1]][V_VPU1].voltage/div,
	apusys_opps.opps[cur_opp_index[V_VPU2]][V_VPU2].voltage/div,
	apusys_opps.opps[next_opp_index[V_VPU2]][V_VPU2].voltage/div,
	apusys_opps.opps[cur_opp_index[V_MDLA0]][V_MDLA0].voltage/div,
	apusys_opps.opps[next_opp_index[V_MDLA0]][V_MDLA0].voltage/div,
	apusys_opps.opps[cur_opp_index[V_MDLA1]][V_MDLA1].voltage/div,
	apusys_opps.opps[next_opp_index[V_MDLA1]][V_MDLA1].voltage/div,
	apusys_opps.opps[cur_opp_index[V_APU_CONN]][V_APU_CONN].voltage/div,
	apusys_opps.opps[next_opp_index[V_APU_CONN]][V_APU_CONN].voltage/div,
	apusys_opps.opps[cur_opp_index[V_TOP_IOMMU]][V_TOP_IOMMU].voltage/div,
	apusys_opps.opps[next_opp_index[V_TOP_IOMMU]][V_TOP_IOMMU].voltage/div,
	apusys_opps.opps[cur_opp_index[V_VCORE]][V_VCORE].voltage/div,
	apusys_opps.opps[next_opp_index[V_VCORE]][V_VCORE].voltage/div,
	apusys_opps.id);

	PWR_LOG_WRN("APUPWR DVFS %s\n", log_str);

	snprintf(log_str, sizeof(log_str),
	"f[(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d),(%d,%d)] %llu",
	apusys_opps.opps[cur_opp_index[V_VPU0]][V_VPU0].freq/div,
	apusys_opps.opps[next_opp_index[V_VPU0]][V_VPU0].freq/div,
	apusys_opps.opps[cur_opp_index[V_VPU1]][V_VPU1].freq/div,
	apusys_opps.opps[next_opp_index[V_VPU1]][V_VPU1].freq/div,
	apusys_opps.opps[cur_opp_index[V_VPU2]][V_VPU2].freq/div,
	apusys_opps.opps[next_opp_index[V_VPU2]][V_VPU2].freq/div,
	apusys_opps.opps[cur_opp_index[V_MDLA0]][V_MDLA0].freq/div,
	apusys_opps.opps[next_opp_index[V_MDLA0]][V_MDLA0].freq/div,
	apusys_opps.opps[cur_opp_index[V_MDLA1]][V_MDLA1].freq/div,
	apusys_opps.opps[next_opp_index[V_MDLA1]][V_MDLA1].freq/div,
	apusys_opps.opps[cur_opp_index[V_APU_CONN]][V_APU_CONN].freq/div,
	apusys_opps.opps[next_opp_index[V_APU_CONN]][V_APU_CONN].freq/div,
	apusys_opps.opps[cur_opp_index[V_TOP_IOMMU]][V_TOP_IOMMU].freq/div,
	apusys_opps.opps[next_opp_index[V_TOP_IOMMU]][V_TOP_IOMMU].freq/div,
	apusys_opps.opps[cur_opp_index[V_VCORE]][V_VCORE].freq/div,
	apusys_opps.opps[next_opp_index[V_VCORE]][V_VCORE].freq/div,
	apusys_opps.id);

	PWR_LOG_WRN("APUPWR DVFS %s\n", log_str);
	}

}


void apusys_dvfs_state_machine(void)
{

	apusys_buck_up_check();

	apusys_frequency_check();

	apusys_buck_down_check();
}


void apusys_dvfs_policy(uint64_t round_id)
{
	uint8_t user;
	uint8_t opp;
	uint8_t use_opp;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;
	enum DVFS_VOLTAGE voltage;
	uint8_t buck_domain_num = 0;
	uint8_t buck_index;
	uint8_t i = 0, j = 0;

	apusys_opps.id = round_id;


	for (user = 0; user < APUSYS_DVFS_USER_NUM; user++) {
		apusys_opps.driver_opp_index[user] =
			apusys_opps.user_opp_index[user];
		if (is_power_debug_lock == false) {
			if (dvfs_user_support(user) == false)
				continue;
			opp = apusys_opps.driver_opp_index[user];
			buck_domain = apusys_user_to_buck_domain[user];
			use_opp = apusys_pwr_max_min_check(user, opp);

			voltage =
				apusys_opps.opps[use_opp][buck_domain].voltage;
			apusys_clk_path_update_pwr(user, voltage);
		}
	}

	if (is_power_debug_lock == false) {
		apusys_final_volt_check();

		apusys_pwr_constraint_check();

		apusys_pwr_efficiency_check();
	}


	apusys_dvfs_state_machine();

	apusys_dvfs_info();


	for (buck_domain_num = 0;
	buck_domain_num < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain_num++) {
		if (dvfs_power_domain_support(buck_domain_num) == false)
			continue;
		apusys_opps.cur_opp_index[buck_domain_num] =
			apusys_opps.next_opp_index[buck_domain_num];
	}


	for (buck_index = 0; buck_index < APUSYS_BUCK_NUM;
	buck_index++) {
		apusys_opps.cur_buck_volt[buck_index] =
			apusys_opps.next_buck_volt[buck_index];
		apusys_opps.next_buck_volt[buck_index] =
			DVFS_VOLT_00_575000_V;
	}

	for (i = 0; i < APUSYS_DVFS_USER_NUM; i++) {
		for (j = 0; j < APUSYS_PATH_USER_NUM; j++)
			apusys_opps.user_path_volt[i][j] =
			DVFS_VOLT_00_575000_V;
	}

}

void apusys_set_opp(enum DVFS_USER user, uint8_t opp)
{
	if (is_power_debug_lock == false) {
		if (apusys_opps.is_power_on[user] == true) {
			apusys_opps.user_opp_index[user] = opp;

		if (g_pwr_log_level == APUSYS_PWR_LOG_DEBUG)
			PWR_LOG_INF("%s, %s, user_opp=%d\n",
			__func__, user_str[user], opp);
		}
	}
}


bool apusys_check_opp_change(void)
{
	uint8_t user;

	for (user = 0; user < APUSYS_DVFS_USER_NUM; user++) {
		if (dvfs_user_support(user) == false)
			continue;
		if (apusys_opps.user_opp_index[user] !=
			apusys_opps.driver_opp_index[user])
			return true;
	}

	return false;
}


int apusys_power_on(enum DVFS_USER user)
{
	int ret = 0;
	struct hal_param_pwr_mask pwr_mask;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;

	ret = hal_config_power(PWR_CMD_SET_BOOT_UP, user, (void *)&pwr_mask);

	if (ret == 0) {
		if (apusys_opps.power_bit_mask == 0) {	// first power on
			PWR_LOG_INF("%s first power on\n", __func__);
			apusys_opps.cur_buck_volt[VPU_BUCK] =
				VVPU_DEFAULT_VOLT;
			apusys_opps.cur_buck_volt[MDLA_BUCK] =
				VMDLA_DEFAULT_VOLT;
			apusys_opps.cur_buck_volt[VCORE_BUCK] =
				VCORE_DEFAULT_VOLT;
			apusys_opps.vsram_volatge = VSRAM_DEFAULT_VOLT;

			apusys_opps.cur_opp_index[V_APU_CONN] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[V_APU_CONN] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.cur_opp_index[V_TOP_IOMMU] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[V_TOP_IOMMU] =
				APUSYS_DEFAULT_OPP;
		}

		if (user < APUSYS_DVFS_USER_NUM) {
			buck_domain = apusys_user_to_buck_domain[user];
			apusys_opps.cur_opp_index[buck_domain] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[buck_domain] =
				APUSYS_DEFAULT_OPP;

			if (is_power_debug_lock == true)
				fix_dvfs_debug();
		}

		apusys_opps.is_power_on[user] = true;
		apusys_opps.power_bit_mask |= (1<<user);
	}

	return ret;
}

int apusys_power_off(enum DVFS_USER user)
{
	int ret = 0;
	struct hal_param_pwr_mask pwr_mask;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;

	apusys_opps.is_power_on[user] = false;
	apusys_opps.power_bit_mask &= (~(1<<user));

	ret = hal_config_power(PWR_CMD_SET_SHUT_DOWN, user, (void *)&pwr_mask);

	if (ret == 0) {
		if (user < APUSYS_DVFS_USER_NUM) {
			buck_domain = apusys_user_to_buck_domain[user];
			apusys_opps.cur_opp_index[buck_domain] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[buck_domain] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.user_opp_index[user] =
				 APUSYS_DEFAULT_OPP;
			apusys_opps.driver_opp_index[user] =
				 APUSYS_DEFAULT_OPP;
		}
		if (apusys_opps.power_bit_mask == 0) {
			PWR_LOG_INF("%s all power off\n", __func__);
			apusys_opps.cur_opp_index[V_APU_CONN] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[V_APU_CONN] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.cur_opp_index[V_TOP_IOMMU] =
				APUSYS_DEFAULT_OPP;
			apusys_opps.next_opp_index[V_TOP_IOMMU] =
				APUSYS_DEFAULT_OPP;
		}
	}

	return ret;
}

void apusys_power_init(enum DVFS_USER user, void *init_power_data)
{
	int i = 0, j = 0;
	struct hal_param_seg_support  seg_data;
	enum DVFS_VOLTAGE_DOMAIN domain;

	hal_config_power(PWR_CMD_SEGMENT_CHECK, VPU0, (void *)&seg_data);

	if (seg_data.seg == SEGMENT_0)
		apusys_opps.opps = dvfs_table_0;
	else if (seg_data.seg == SEGMENT_2)
		apusys_opps.opps = dvfs_table_2;
	else
		apusys_opps.opps = dvfs_table_1;

	for (i = 0; i < APUSYS_DVFS_USER_NUM; i++)	{
		seg_data.user = i;
		hal_config_power(PWR_CMD_SEGMENT_CHECK,
			VPU0, (void *)&seg_data);
		domain = apusys_user_to_buck_domain[i];
		apusys_dvfs_user_support[i] = seg_data.support;
		apusys_dvfs_buck_domain_support[domain] = seg_data.support;
		if (dvfs_user_support(user) == false)
			continue;
		apusys_opps.thermal_opp[i] = 0;
		apusys_opps.user_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.driver_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.power_lock_max_opp[i] = APUSYS_MAX_NUM_OPPS-1;
		apusys_opps.power_lock_min_opp[i] = 0;
		apusys_opps.is_power_on[i] = false;
		for (j = 0; j < APUSYS_PATH_USER_NUM; j++)
			apusys_opps.user_path_volt[i][j] =
			DVFS_VOLT_00_575000_V;
	}

	for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++) {
		apusys_opps.next_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.cur_opp_index[i] = APUSYS_DEFAULT_OPP;
	}

	apusys_opps.cur_buck_volt[VPU_BUCK] = VVPU_DEFAULT_VOLT;
	apusys_opps.cur_buck_volt[MDLA_BUCK] = VMDLA_DEFAULT_VOLT;
	apusys_opps.cur_buck_volt[VCORE_BUCK] = VCORE_DEFAULT_VOLT;
	apusys_opps.next_buck_volt[VPU_BUCK] = DVFS_VOLT_00_575000_V;
	apusys_opps.next_buck_volt[MDLA_BUCK] = DVFS_VOLT_00_575000_V;
	apusys_opps.next_buck_volt[VCORE_BUCK] = VCORE_DEFAULT_VOLT;
	apusys_opps.vsram_volatge = VSRAM_DEFAULT_VOLT;

	apusys_opps.power_bit_mask = 0;


	hal_config_power(PWR_CMD_INIT_POWER, user, init_power_data);
	PWR_LOG_INF("%s done,\n", __func__);
}


void apusys_power_uninit(enum DVFS_USER user)
{
	hal_config_power(PWR_CMD_UNINIT_POWER, user, NULL);
	PWR_LOG_INF("%s done,\n", __func__);
}

enum DVFS_FREQ apusys_get_dvfs_freq(enum DVFS_VOLTAGE_DOMAIN domain)
{
	return apusys_opps.opps[apusys_opps.cur_opp_index[domain]][domain].freq;
}

