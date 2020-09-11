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

#include <time.h>
//#include <sync_write.h>

#include "test.h"

#include "apu_log.h"
#include "apusys_power_debug.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "hal_config_power.h"


#define ALL_COMBINATION 0

#define VOLT_CONSTRAINTS_1	(225000)
#define CONSTRAINTS_2_SIZE 37

int g_pwr_log_level = APUSYS_PWR_LOG_INFO;
bool is_power_debug_lock;

enum DVFS_VOLTAGE sram_constraint[CONSTRAINTS_2_SIZE][3] = {
	{DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_825000_V, DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_825000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_825000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_825000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_825000_V},

	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_800000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_825000_V},

	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_750000_V, DVFS_VOLT_00_575000_V, DVFS_VOLT_00_750000_V},

	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_700000_V, DVFS_VOLT_00_575000_V, DVFS_VOLT_00_750000_V},

	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_825000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_800000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_825000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_650000_V, DVFS_VOLT_00_575000_V, DVFS_VOLT_00_750000_V},

	{DVFS_VOLT_00_575000_V, DVFS_VOLT_00_750000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_575000_V, DVFS_VOLT_00_700000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_575000_V, DVFS_VOLT_00_650000_V, DVFS_VOLT_00_750000_V},
	{DVFS_VOLT_00_575000_V, DVFS_VOLT_00_575000_V, DVFS_VOLT_00_750000_V},
};



struct hal_param_init_power init_power_data;


void fix_dvfs_debug(void)
{

}

u32 get_devinfo_with_index(unsigned int index)
{
	return 0x1;	// 5G-L , vpu2, mdla1 not support
	//return 0x10; // 5G_H
	//return 2;
}

static int segment_user_support_check(void *param)
{
	uint32_t val = 0;
	struct hal_param_seg_support *seg_info =
		(struct hal_param_seg_support *)param;

	seg_info->support = true;
	seg_info->seg = SEGMENT_1;

	val = get_devinfo_with_index(30);
	if (val == 0x1) {
		seg_info->seg = SEGMENT_0;
		if (seg_info->user == VPU2 || seg_info->user == MDLA1)
			seg_info->support = false;
	} else if (val == 0x10)
		seg_info->seg = SEGMENT_2;

	if (seg_info->support == false)
		LOG_INF("%s user=%d, support=%d\n", __func__,
		seg_info->user, seg_info->support);

	return 0;
}


void apusys_power_hal_test(void)
{
	int i = 0;
	int args[3] = {3, 40, 60};

	if ((args[0] == VPU0 || args[0] == VPU1 || args[0] == VPU2)
		&& APUSYS_VPU_NUM != 0) {
		for (i = VPU0; i < VPU0 + APUSYS_VPU_NUM; i++) {
			apusys_opps.power_lock_max_opp[i] =
				apusys_boost_value_to_opp(i, args[1]);
			apusys_opps.power_lock_min_opp[i] =
				apusys_boost_value_to_opp(i, args[2]);
		}
	}

	if ((args[0] == MDLA0 || args[0] == MDLA1)
		&& APUSYS_MDLA_NUM != 0) {
		for (i = MDLA0; i < MDLA0 + APUSYS_MDLA_NUM; i++) {
			apusys_opps.power_lock_max_opp[i] =
				apusys_boost_value_to_opp(i, args[1]);
			apusys_opps.power_lock_min_opp[i] =
				apusys_boost_value_to_opp(i, args[2]);
		}
	}


}

void apusys_set_dfvs_debug_test(void)
{
	int i = 0, j = 0;
	int args[1] = {6};
	int opp = 0;

	LOG_INF("lock opp = %d\n", (int)args[0]);

	for (i = VPU0; i < VPU0 + APUSYS_VPU_NUM; i++)
		apusys_opps.next_opp_index[i] = args[0];

	for (i = MDLA0; i < MDLA0 + APUSYS_MDLA_NUM; i++)
		apusys_opps.next_opp_index[i] = args[0];

	apusys_opps.next_buck_volt[VPU_BUCK] =
			apusys_opps.opps[args[0]][V_VPU0].voltage;
		apusys_opps.next_buck_volt[MDLA_BUCK] =
			apusys_opps.opps[args[0]][V_MDLA0].voltage;
		apusys_opps.next_buck_volt[VCORE_BUCK] =
			apusys_opps.opps[args[0]][V_VCORE].voltage;

		// determine buck domain opp
		for (i = 0; i < APUSYS_BUCK_DOMAIN_NUM; i++) {
			if (dvfs_power_domain_support(i) == false)
				continue;
			for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
				if ((i == V_APU_CONN ||	i == V_TOP_IOMMU) &&
					(apusys_opps.opps[opp][i].voltage ==
					apusys_opps.next_buck_volt[VPU_BUCK])) {
					apusys_opps.next_opp_index[i] = opp;
					break;
				} else if (i == V_VCORE &&
				apusys_opps.opps[opp][i].voltage ==
				apusys_opps.next_buck_volt[VCORE_BUCK]) {
					apusys_opps.next_opp_index[i] = opp;
					break;
				}
			}
		}

		is_power_debug_lock = true;
	//	apusys_dvfs_policy(0);
}


int voltage_constraint_check(int vvpu, int vmdla, int vsram, int vcore)
{
	if (vvpu == DVFS_VOLT_00_575000_V &&
			vmdla >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
				vvpu, vmdla);
		return 1;
	}

	if (vmdla == DVFS_VOLT_00_575000_V &&
			vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
				vvpu, vmdla);
		return 1;
	}

	if (vcore == DVFS_VOLT_00_575000_V &&
			vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vcore=%d\n",
				vvpu, vcore);
		return 1;
	}

	if ((vvpu > VSRAM_TRANS_VOLT || vmdla > VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_LOW_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				vvpu, vmdla, vsram);
		return 1;
	}

	if ((vvpu < VSRAM_TRANS_VOLT && vmdla < VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_HIGH_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				vvpu, vmdla, vsram);
		return 1;
	}

	return 0;
}



int hal_config_power(enum HAL_POWER_CMD cmd, enum DVFS_USER user, void *param)
{
	int target_volt = 0;
	enum DVFS_BUCK buck = 0;
	int i = 0;
	static int vpu_curr_volt = VVPU_DEFAULT_VOLT;
	static int mdla_curr_volt = VMDLA_DEFAULT_VOLT;
	static int vsram_curr_volt = VSRAM_DEFAULT_VOLT;
	static int vcore_curr_volt = VCORE_DEFAULT_VOLT;

	if (cmd == PWR_CMD_SET_VOLT) {
		buck = ((struct hal_param_volt *)param)->target_buck;
		target_volt = ((struct hal_param_volt *)param)->target_volt;
		if (buck == VPU_BUCK)
			vpu_curr_volt = target_volt;
		if (buck == MDLA_BUCK)
			mdla_curr_volt = target_volt;
		if (buck == VCORE_BUCK)
			vcore_curr_volt = target_volt;
		if (buck == SRAM_BUCK)
			vsram_curr_volt = target_volt;
		//int vpu_curr_volt = apusys_opps.cur_buck_volt[VPU_BUCK];
		//int mdla_curr_volt = apusys_opps.cur_buck_volt[MDLA_BUCK];
		//vsram_curr_volt = apusys_opps.vsram_volatge;

		LOG_INF("%s cmd = %d, buck = %d, voltage=%d\n",
			__func__, cmd, buck, target_volt);


		//if (apusys_policy_checker()) {
		//		while(1);
		//	}
		#if 1
		if ((vpu_curr_volt - mdla_curr_volt >= VOLT_CONSTRAINTS_1) ||
		(mdla_curr_volt - vpu_curr_volt >= VOLT_CONSTRAINTS_1) ||
		vpu_curr_volt - vcore_curr_volt >= VOLT_CONSTRAINTS_1) {
			LOG_WRN(">>>>>>>> vpu_curr_volt = %d\n",
				vpu_curr_volt);
			LOG_WRN(">>>>>>>> mdla_curr_volt = %d\n",
				mdla_curr_volt);
			LOG_WRN(">>>>>>>> vcore_core_volt = %d\n",
				vcore_curr_volt);
			LOG_WRN("%s fail : mdla, vpu volt diff > %d !\n",
						__func__, VOLT_CONSTRAINTS_1);
			exit(1);
		}
		#endif

		if (voltage_constraint_check(vpu_curr_volt,
			mdla_curr_volt, vsram_curr_volt, vcore_curr_volt))
			exit(1);

		for (i = 0; i < CONSTRAINTS_2_SIZE; i++) {
			if (vpu_curr_volt == sram_constraint[i][0] &&
			mdla_curr_volt == sram_constraint[i][1] &&
			vsram_curr_volt == sram_constraint[i][2]) {
				break;
			}
		}

		if (i == CONSTRAINTS_2_SIZE) {
			LOG_WRN("vpu_curr_volt = %d\n", vpu_curr_volt);
			LOG_WRN("mdla_curr_volt = %d\n", mdla_curr_volt);
			LOG_WRN("vsram_cur_volt = %d\n", vsram_curr_volt);
			LOG_WRN("%s fail : constraint violate!\n",
				__func__);
			exit(1);
		}

	} else if (cmd == PWR_CMD_SEGMENT_CHECK) {
		segment_user_support_check(param);
	} else {
		LOG_INF("%s cmd = %d, user = %d\n", __func__, cmd, user);
	}
}

int check_constraint_1(void)
{
	int vpu_curr_volt = apusys_opps.cur_buck_volt[VPU_BUCK];
	int mdla_curr_volt = apusys_opps.cur_buck_volt[MDLA_BUCK];
	int vcore_curr_volt = apusys_opps.cur_buck_volt[VCORE_BUCK];

	if ((vpu_curr_volt - mdla_curr_volt >= VOLT_CONSTRAINTS_1) ||
		(mdla_curr_volt - vpu_curr_volt >= VOLT_CONSTRAINTS_1) ||
		vpu_curr_volt - vcore_curr_volt >= VOLT_CONSTRAINTS_1) {
		LOG_INF(">>>>>>>> vpu_curr_volt = %d\n", vpu_curr_volt);
		LOG_INF(">>>>>>>> mdla_curr_volt = %d\n", mdla_curr_volt);
		LOG_INF(">>>>>>>> vcore_core_volt = %d\n", vcore_curr_volt);
		LOG_WRN("%s fail : mdla, vpu volt diff larger than %d !\n",
						__func__, VOLT_CONSTRAINTS_1);
		return -1; // return non zero for check fail !
	}

	return 0;
}

int check_constraint_2(void)
{
	// TODO : add more constraints check
	return 0;
}

int apusys_policy_checker(void)
{
	int ret = 0;

	ret |= check_constraint_1();
	ret |= check_constraint_2();

	return ret;
}

void reset_all_opp_to_default(void)
{
	apusys_set_opp(VPU0, APUSYS_MAX_NUM_OPPS-1);
	apusys_set_opp(VPU1, APUSYS_MAX_NUM_OPPS-1);
	apusys_set_opp(VPU2, APUSYS_MAX_NUM_OPPS-1);
	apusys_set_opp(MDLA0, APUSYS_MAX_NUM_OPPS-1);
	apusys_set_opp(MDLA1, APUSYS_MAX_NUM_OPPS-1);
	apusys_dvfs_policy(100);
}

void test_case(int power_on_round, int opp_change_round, int fail_stop)
{
	int i, j, k, opp;
	int m, n, x, y;

//	apusys_power_hal_test();
//	apusys_set_dfvs_debug_test();

	for (i = 1 ; i <= power_on_round ; i++) {
		LOG_INF("### power on round #%d start ###\n", i);
		apusys_power_on(VPU0);
		apusys_power_on(MDLA0);

		for (j = 1 ; j <= opp_change_round ; j++) {
			LOG_INF("## opp change round #%d start ##\n", j);

		#if ALL_COMBINATION
for (k = 0 ; k < APUSYS_MAX_NUM_OPPS ; k++) {
	apusys_set_opp(VPU0, k);
	for (m = 0 ; m < APUSYS_MAX_NUM_OPPS ; m++) {
		apusys_set_opp(VPU1, m);
		for (n = 0 ; n < APUSYS_MAX_NUM_OPPS ; n++) {
			apusys_set_opp(VPU2, n);
			for (x = 0 ; x < APUSYS_MAX_NUM_OPPS ; x++) {
				apusys_set_opp(MDLA0, x);
				for (y = 0 ; y < APUSYS_MAX_NUM_OPPS ; y++) {
					apusys_set_opp(MDLA1, y);
					apusys_dvfs_policy(100);
					reset_all_opp_to_default();
				}
			}
		}
	}
}

		#else
			for (k = 0 ; k < APUSYS_DVFS_USER_NUM ; k++) {
				if (dvfs_user_support(k) == false)
					continue;
				opp = rand() % APUSYS_MAX_NUM_OPPS;
				//opp = 0;
				apusys_opps.is_power_on[k] = true;
				apusys_set_opp(k, opp);
			}

			apusys_dvfs_policy(100);

			if (apusys_policy_checker()) {
				LOG_WRN("!!! policy check fail !!!\n");
				if (fail_stop) {
					LOG_WRN("!!! stop now !!!\n");
					return;
				}
			}

			is_power_debug_lock = false;
			LOG_INF("## opp change round #%d end ##\n", j);
		#endif
		}


		apusys_power_off(VPU0);
		apusys_power_off(MDLA0);
		LOG_INF("### power on round #%d end ###\n", i);
	}
}

int main(int argc, char *argv[])
{
	int power_on_round = 0;
	int opp_change_round = 0;
	int fail_stop = 0;

	power_on_round = atoi(argv[1]);
	opp_change_round = atoi(argv[2]);
	fail_stop = atoi(argv[3]);

	apusys_power_init(VPU0, (void *)&init_power_data);
	srand(time(NULL));
	test_case(power_on_round, opp_change_round, fail_stop);
	apusys_power_uninit(VPU0);

	LOG_INF("test finish (%d,%d)\n", power_on_round, opp_change_round);
}
