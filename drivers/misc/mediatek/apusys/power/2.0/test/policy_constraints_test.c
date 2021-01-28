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

#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "hal_config_power.h"
#include "apu_log.h"

#define VOLT_CONSTRAINTS_1	(50000)

struct hal_param_init_power init_power_data;

int hal_config_power(enum HAL_POWER_CMD cmd, enum DVFS_USER user, void *param)
{
	LOG_INF("%s cmd = %d, user = %d\n", __func__, cmd, user);
}

int check_constraint_1(void)
{
	int vpu_curr_volt = apusys_opps.prev_buck_volt[VPU_BUCK];
	int mdla_curr_volt = apusys_opps.prev_buck_volt[MDLA_BUCK];

	if ((vpu_curr_volt - mdla_curr_volt > VOLT_CONSTRAINTS_1) ||
		(mdla_curr_volt - vpu_curr_volt > VOLT_CONSTRAINTS_1)) {
		LOG_INF(">>>>>>>> vpu_curr_volt = %d\n", vpu_curr_volt);
		LOG_INF(">>>>>>>> mdla_curr_volt = %d\n", mdla_curr_volt);
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

void test_case(int power_on_round, int opp_change_round, int fail_stop)
{
	int i, j, k, opp;

	for (i = 1 ; i <= power_on_round ; i++) {
		LOG_INF("### power on round #%d start ###\n", i);
		apusys_power_on(VPU0);
		apusys_power_on(MDLA0);

		for (j = 1 ; j <= opp_change_round ; j++) {
			LOG_INF("## opp change round #%d start ##\n", j);

			for (k = 0 ; k < APUSYS_DVFS_USER_NUM ; k++) {
				opp = rand() % APUSYS_MAX_NUM_OPPS;
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

			LOG_INF("## opp change round #%d end ##\n", j);
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
