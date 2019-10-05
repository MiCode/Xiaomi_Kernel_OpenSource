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

#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power.h"
#include "hal_config_power.h"
#include "apu_log.h"

#define POWER_ON_DELAY	(100)

struct apusys_dvfs_opps apusys_opps;
bool is_power_debug_lock;

int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER user, uint8_t opp)
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

	for (path_volt_index = 0;
	path_volt_index < APUSYS_PATH_USER_NUM; path_volt_index++){
		apusys_opps.user_path_volt[user][path_volt_index] = voltage;
			(voltage >
			dvfs_clk_path_max_vol[user][path_volt_index] ?
			dvfs_clk_path_max_vol[user][path_volt_index] :
			voltage);
		PWR_LOG_INF("%s, user_path_volt[%s][%d]=%d\n",
			__func__,
			user_str[user],
			path_volt_index,
			voltage);
	}
}


void apusys_final_volt_check(void)
{
	uint8_t  user_index = 0, path_index = 0, buck_index = 0;

// for every buck domain, check clk path matrix for buck shared relation
for (buck_index = 0;
buck_index < APUSYS_BUCK_NUM; buck_index++) {
	for (user_index = 0; user_index < APUSYS_DVFS_USER_NUM; user_index++) {
		for (path_index = 0;
		path_index < APUSYS_PATH_USER_NUM; path_index++) {
			if (buck_shared[buck_index][user_index][path_index]
		== true) {
				apusys_opps.cur_buck_volt[buck_index] =
		MAX(apusys_opps.cur_buck_volt[buck_index],
		apusys_opps.user_path_volt[user_index][path_index]);
	PWR_LOG_INF("%s, %s = %d,(%s,%d)=%d\n",
		__func__,
		buck_str[buck_index],
		apusys_opps.cur_buck_volt[buck_index],
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

	for (i = 0; i < APUSYS_DVFS_CONSTRAINT_NUM; i++) {
		buck0 = dvfs_constraint_table[i].buck0;
	    buck1 = dvfs_constraint_table[i].buck1;
	    voltage0 = dvfs_constraint_table[i].voltage0;
	    voltage1 = dvfs_constraint_table[i].voltage1;
if (apusys_opps.cur_buck_volt[buck0] == voltage0 &&
	apusys_opps.cur_buck_volt[buck1] == voltage1) {
	for (opp_index = APUSYS_MAX_NUM_OPPS-1;
		opp_index >= 0; opp_index--) {
		if (voltage0 < voltage1) {
			if (apusys_opps.opps[opp_index][buck0].voltage
	> voltage0) {
				apusys_opps.cur_buck_volt[buck0] =
			apusys_opps.opps[opp_index][buck0].voltage;
				PWR_LOG_INF("%s, %s from %d --> %d\n",
				__func__,
				buck_str[buck0],
				voltage0,
			apusys_opps.cur_buck_volt[buck0]);
				break;
				}
		} else if (voltage0 > voltage1) {
			if (apusys_opps.opps[opp_index][buck1].voltage
			> voltage1) {
				apusys_opps.cur_buck_volt[buck1] =
			apusys_opps.opps[opp_index][buck1].voltage;
				PWR_LOG_INF("%s, %s from %d --> %d\n",
				__func__,
				buck_str[buck1],
				voltage1,
			apusys_opps.cur_buck_volt[buck1]);
				break;
				}
			}
		}
	}
}
}


void apusys_pwr_efficiency_check(void)
{
	uint8_t buck_domain_index = 0;
	uint8_t buck_index = 0;
	uint8_t opp_index = 0;
	enum DVFS_USER user;

	for (buck_domain_index = 0;
	buck_domain_index < APUSYS_BUCK_DOMAIN_NUM; buck_domain_index++) {
		if (buck_domain_index == V_VCORE)
			continue;
		buck_index = apusys_buck_domain_to_buck[buck_domain_index];
	for (opp_index = 0;	opp_index < APUSYS_MAX_NUM_OPPS; opp_index++) {
		if (apusys_opps.opps[opp_index][buck_domain_index].voltage <=
			apusys_opps.cur_buck_volt[buck_index]){
			user = apusys_buck_domain_to_user[buck_domain_index];
			if (user < APUSYS_DVFS_USER_NUM) {
				apusys_opps.cur_opp_index[buck_domain_index] =
				apusys_pwr_max_min_check(user, opp_index);
			} else {
				apusys_opps.cur_opp_index[buck_domain_index] =
				opp_index;
			}
			PWR_LOG_INF("%s, %s, opp=%d\n",
			__func__,
			buck_domain_str[buck_domain_index],
			apusys_opps.cur_opp_index[buck_domain_index]);
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

	if (apusys_opps.prev_buck_volt[VCORE_BUCK] <
			apusys_opps.cur_buck_volt[VCORE_BUCK]) {
		volt_data.target_buck = VCORE_BUCK;
		volt_data.target_volt = apusys_opps.cur_buck_volt[VCORE_BUCK];

		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.prev_buck_volt[VPU_BUCK] <
		apusys_opps.prev_buck_volt[MDLA_BUCK]) {
		buck_small_index = VPU_BUCK;
		buck_large_index = MDLA_BUCK;
	} else if (apusys_opps.prev_buck_volt[VPU_BUCK] >
		apusys_opps.prev_buck_volt[MDLA_BUCK]){
		buck_small_index = MDLA_BUCK;
		buck_large_index = VPU_BUCK;
	} else {
		if (apusys_opps.cur_buck_volt[VPU_BUCK] <
		apusys_opps.cur_buck_volt[MDLA_BUCK]){
			buck_small_index = VPU_BUCK;
			buck_large_index = MDLA_BUCK;
		} else {
			buck_small_index = MDLA_BUCK;
			buck_large_index = VPU_BUCK;
		}
	}

	if (apusys_opps.vsram_volatge == DVFS_VOLT_00_750000_V &&
		(apusys_opps.cur_buck_volt[VPU_BUCK] > VSRAM_TRANS_VOLT ||
		apusys_opps.cur_buck_volt[MDLA_BUCK] > VSRAM_TRANS_VOLT)) {
		if ((apusys_opps.prev_buck_volt[buck_small_index] <
			apusys_opps.cur_buck_volt[buck_small_index]) &&
			(apusys_opps.cur_buck_volt[buck_small_index] >
				VSRAM_TRANS_VOLT)) {
			volt_data.target_buck = buck_small_index;
			volt_data.target_volt = VSRAM_TRANS_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}
		if ((apusys_opps.prev_buck_volt[buck_large_index] <
			apusys_opps.cur_buck_volt[buck_large_index]) &&
			(apusys_opps.cur_buck_volt[buck_large_index] >
				VSRAM_TRANS_VOLT)) {
			volt_data.target_buck = buck_large_index;
			volt_data.target_volt = VSRAM_TRANS_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}

			volt_data.target_buck = SRAM_BUCK;
			volt_data.target_volt = DVFS_VOLT_00_825000_V;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		apusys_opps.vsram_volatge = DVFS_VOLT_00_825000_V;

	}

	if (apusys_opps.prev_buck_volt[buck_small_index] <
			apusys_opps.cur_buck_volt[buck_small_index]) {
		volt_data.target_buck = buck_small_index;
		volt_data.target_volt =
			apusys_opps.cur_buck_volt[buck_small_index];
	hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.prev_buck_volt[buck_large_index] <
			apusys_opps.cur_buck_volt[buck_large_index]) {
		volt_data.target_buck = buck_large_index;
		volt_data.target_volt =
			apusys_opps.cur_buck_volt[buck_large_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}
}


void apusys_frequency_check(void)
{
	uint8_t buck_domain_index = 0;
	uint8_t cur_opp_index = 0, prev_opp_index = 0;
	enum DVFS_USER user;
	struct hal_param_freq  freq_data;

	for (buck_domain_index = 0;
	buck_domain_index < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain_index++) {
		user = apusys_buck_domain_to_user[buck_domain_index];
		cur_opp_index = apusys_opps.cur_opp_index[buck_domain_index];
		prev_opp_index = apusys_opps.prev_opp_index[buck_domain_index];
		if (apusys_opps.opps[cur_opp_index][buck_domain_index].freq !=
		apusys_opps.opps[prev_opp_index][buck_domain_index].freq){
			freq_data.target_volt_domain = buck_domain_index;
			freq_data.target_freq = apusys_opps.opps[cur_opp_index]
						[buck_domain_index].freq;
		hal_config_power(PWR_CMD_SET_FREQ, user, (void *)&freq_data);

		PWR_LOG_INF("%s, %s, freq from %d --> %d\n",	__func__,
		buck_domain_str[buck_domain_index],
		apusys_opps.opps[prev_opp_index][buck_domain_index].freq,
		apusys_opps.opps[cur_opp_index][buck_domain_index].freq);
		}
	}
}


void apusys_buck_down_check(void)
{
	uint8_t user = 0; //don't care
	uint8_t buck_small_index = 0, buck_large_index = 0;
	struct hal_param_volt volt_data;

	if (apusys_opps.prev_buck_volt[VPU_BUCK] <
		apusys_opps.prev_buck_volt[MDLA_BUCK]) {
		buck_small_index = VPU_BUCK;
		buck_large_index = MDLA_BUCK;
	} else if (apusys_opps.prev_buck_volt[VPU_BUCK] >
		apusys_opps.prev_buck_volt[MDLA_BUCK]){
		buck_small_index = MDLA_BUCK;
		buck_large_index = VPU_BUCK;
	} else {
		if (apusys_opps.cur_buck_volt[VPU_BUCK] <
		apusys_opps.cur_buck_volt[MDLA_BUCK]){
			buck_small_index = VPU_BUCK;
			buck_large_index = MDLA_BUCK;
		} else {
			buck_small_index = MDLA_BUCK;
			buck_large_index = VPU_BUCK;
		}
	}

	if (apusys_opps.vsram_volatge == DVFS_VOLT_00_825000_V &&
		(apusys_opps.cur_buck_volt[VPU_BUCK] <= VSRAM_TRANS_VOLT &&
		apusys_opps.cur_buck_volt[MDLA_BUCK] <= VSRAM_TRANS_VOLT)) {
		if ((apusys_opps.prev_buck_volt[buck_large_index] >
			apusys_opps.cur_buck_volt[buck_large_index]) &&
			(apusys_opps.cur_buck_volt[buck_large_index] <=
				VSRAM_TRANS_VOLT)) {
			volt_data.target_buck = buck_large_index;
			volt_data.target_volt = VSRAM_TRANS_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}
		if ((apusys_opps.prev_buck_volt[buck_small_index] >
			apusys_opps.cur_buck_volt[buck_small_index]) &&
			(apusys_opps.cur_buck_volt[buck_small_index] <=
				VSRAM_TRANS_VOLT)) {
			volt_data.target_buck = buck_small_index;
			volt_data.target_volt = VSRAM_TRANS_VOLT;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		}

		volt_data.target_buck = SRAM_BUCK;
		volt_data.target_volt = DVFS_VOLT_00_750000_V;
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
		apusys_opps.vsram_volatge = DVFS_VOLT_00_750000_V;

	}

	if (apusys_opps.prev_buck_volt[buck_large_index] >
			apusys_opps.cur_buck_volt[buck_large_index]) {
		volt_data.target_buck = buck_large_index;
		volt_data.target_volt =
			apusys_opps.cur_buck_volt[buck_large_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.prev_buck_volt[buck_small_index] >
			apusys_opps.cur_buck_volt[buck_small_index]) {
		volt_data.target_buck = buck_small_index;
		volt_data.target_volt =
			apusys_opps.cur_buck_volt[buck_small_index];
		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}

	if (apusys_opps.prev_buck_volt[VCORE_BUCK] >
			apusys_opps.cur_buck_volt[VCORE_BUCK]) {
		volt_data.target_buck = VCORE_BUCK;
		volt_data.target_volt = apusys_opps.cur_buck_volt[VCORE_BUCK];

		hal_config_power(PWR_CMD_SET_VOLT, user, (void *)&volt_data);
	}
}


void apusys_dvfs_info(void)
{
	uint8_t buck_domain_index;
	uint8_t prev_opp;
	uint8_t cur_opp;
	enum DVFS_USER user;

	for (buck_domain_index = 0;
	buck_domain_index < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain_index++) {
		prev_opp = apusys_opps.prev_opp_index[buck_domain_index];
		cur_opp = apusys_opps.cur_opp_index[buck_domain_index];

		PWR_LOG_INF(
			"%s, %s, opp(%d, %d),freq(%d, %d), volt(%d, %d) %llu\n",
			__func__,
			buck_domain_str[buck_domain_index],
			prev_opp,
			cur_opp,
			apusys_opps.opps[prev_opp][buck_domain_index].freq,
			apusys_opps.opps[cur_opp][buck_domain_index].freq,
			apusys_opps.opps[prev_opp][buck_domain_index].voltage,
			apusys_opps.opps[cur_opp][buck_domain_index].voltage,
			apusys_opps.id);

		if (buck_domain_index == V_VPU0 || buck_domain_index == V_VPU1
			|| buck_domain_index == V_VPU2
			|| buck_domain_index == V_MDLA0
			|| buck_domain_index == V_MDLA1){
			user = apusys_buck_domain_to_user[buck_domain_index];
			PWR_LOG_INF(
			"%s, %s, user_opp=%d,(T=%d, Pmin=%d, Pmax=%d) %llu\n",
				__func__,
				buck_domain_str[buck_domain_index],
				apusys_opps.driver_opp_index[user],
				apusys_opps.thermal_opp[user],
				apusys_opps.power_lock_min_opp[user],
				apusys_opps.power_lock_max_opp[user],
				apusys_opps.id);
		}
	}
}


void apusys_dvfs_state_machine(void)
{
	uint8_t buck_domain_num = 0;
	uint8_t buck_index;

	apusys_buck_up_check();

	apusys_frequency_check();

	apusys_buck_down_check();

	apusys_dvfs_info();

	for (buck_domain_num = 0;
	buck_domain_num < APUSYS_BUCK_DOMAIN_NUM;
	buck_domain_num++) {
		buck_index = apusys_buck_domain_to_buck[buck_domain_num];
		apusys_opps.prev_opp_index[buck_domain_num] =
			apusys_opps.cur_opp_index[buck_domain_num];
		apusys_opps.prev_buck_volt[buck_index] =
			apusys_opps.cur_buck_volt[buck_index];
	}

	for (buck_index = 0; buck_index < APUSYS_BUCK_NUM; buck_index++)
		apusys_opps.cur_buck_volt[buck_index] = 0;
}


void apusys_dvfs_policy(uint64_t round_id)
{
	uint8_t user;
	uint8_t opp;
	uint8_t use_opp;
	enum DVFS_VOLTAGE_DOMAIN buck_domain;
	enum DVFS_VOLTAGE voltage;

	apusys_opps.id = round_id;

	if (is_power_debug_lock == false) {
		for (user = 0; user < APUSYS_DVFS_USER_NUM; user++) {
			apusys_opps.driver_opp_index[user] =
				apusys_opps.user_opp_index[user];
			opp = apusys_opps.driver_opp_index[user];
			buck_domain = apusys_user_to_buck_domain[user];
			use_opp = apusys_pwr_max_min_check(user, opp);

		voltage = apusys_opps.opps[use_opp][buck_domain].voltage;
		apusys_clk_path_update_pwr(user, voltage);
		}

		apusys_final_volt_check();

		apusys_pwr_constraint_check();

		apusys_pwr_efficiency_check();
	}

	apusys_dvfs_state_machine();

}

void apusys_set_opp(enum DVFS_USER user, uint8_t opp)
{
	apusys_opps.user_opp_index[user] = opp;
	PWR_LOG_INF("%s, %s, user_opp=%d\n", __func__, user_str[user], opp);
}


bool apusys_check_opp_change(void)
{
	uint8_t user;

	for (user = 0; user < APUSYS_DVFS_USER_NUM; user++) {
		if (apusys_opps.user_opp_index[user] !=
			apusys_opps.driver_opp_index[user])
			return true;
	}

	return false;
}


void apusys_power_on(enum DVFS_USER user)
{
	struct hal_param_pwr_mask pwr_mask;

	if (apusys_opps.is_power_on[user] == false) {
		pwr_mask.power_bit_mask = apusys_opps.power_bit_mask;
		hal_config_power(PWR_CMD_SET_BOOT_UP, user, (void *)&pwr_mask);
		apusys_opps.power_bit_mask |= (1<<user);
		apusys_opps.is_power_on[user] = true;
	}
}

void apusys_power_off(enum DVFS_USER user)
{
	struct hal_param_pwr_mask pwr_mask;

	if (apusys_opps.is_power_on[user] == true) {
		apusys_opps.power_bit_mask &= (~(1<<user));
		pwr_mask.power_bit_mask = apusys_opps.power_bit_mask;
		hal_config_power(PWR_CMD_SET_SHUT_DOWN,
					user, (void *)&pwr_mask);
		apusys_opps.is_power_on[user] = false;
	}
}

void apusys_power_init(enum DVFS_USER user, void *init_power_data)
{
	int i = 0, j = 0;

	apusys_opps.opps = dvfs_table;
	for (i = 0; i < APUSYS_DVFS_USER_NUM; i++)	{
		apusys_opps.thermal_opp[i] = 0;
		apusys_opps.user_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.driver_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.power_lock_max_opp[i] = APUSYS_MAX_NUM_OPPS-1;
		apusys_opps.power_lock_min_opp[i] = 0;
		apusys_opps.is_power_on[i] = false;
		for (j = 0; j < APUSYS_PATH_USER_NUM; j++)
			apusys_opps.user_path_volt[i][j] =
			DVFS_VOLT_00_725000_V;
	}

	for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++) {
		apusys_opps.cur_opp_index[i] = APUSYS_DEFAULT_OPP;
		apusys_opps.prev_opp_index[i] = APUSYS_DEFAULT_OPP;
	}

	for (i = 0; i < APUSYS_BUCK_NUM; i++) {
		apusys_opps.cur_buck_volt[i] = DVFS_VOLT_00_575000_V;
		apusys_opps.prev_buck_volt[i] = DVFS_VOLT_00_575000_V;
	}

	apusys_opps.power_bit_mask = 0;
	apusys_opps.vsram_volatge = DVFS_VOLT_00_750000_V;

	hal_config_power(PWR_CMD_INIT_POWER, user, init_power_data);
	PWR_LOG_INF("%s done,\n", __func__);
}


void apusys_power_uninit(enum DVFS_USER user)
{
	hal_config_power(PWR_CMD_UNINIT_POWER, user, NULL);
	PWR_LOG_INF("%s done,\n", __func__);
}

