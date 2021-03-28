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

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power.h"
#include "apu_power_api.h"
#include "apu_log.h"
#include "hal_config_power.h"


static bool apusys_get_conn_power_on_status(void)
{
	if (apusys_get_power_on_status(VPU0) == true ||
		apusys_get_power_on_status(VPU1) == true ||
		apusys_get_power_on_status(MDLA0) == true)
		return true;

	return false;
}

void apu_power_assert_check(struct apu_power_info *info)
{
	int conn_freq;
	int vpu0_freq;
	int vpu1_freq;
	int mdla0_freq;
	int default_freq;


	/* Vvpu, Vsram anad Vcore's constrain */
	int vvpu = info->vvpu * info->dump_div;
	int vsram = info->vsram * info->dump_div;
	int vcore = info->vcore * info->dump_div;

	/* Get VPU0/1 default freq */
	default_freq = BUCK_VVPU_DOMAIN_DEFAULT_FREQ / info->dump_div;

	/* check whether VPU0 opp's value match info */
	if (apusys_get_power_on_status(VPU0) == true &&
	    info->vpu0_freq != 0 &&
	    info->vpu0_freq  != default_freq) {
		vpu0_freq  = apusys_get_dvfs_freq(V_VPU0) / info->dump_div;
		if (((abs(vpu0_freq  - info->vpu0_freq) * 100) >
			vpu0_freq  * ASSERTION_PERCENTAGE)  &&
		    (vpu0_freq  != 0)) {
			LOG_WRN("ASSERT vpu0_freq =%d, info->vpu0_freq =%d\n",
						vpu0_freq, info->vpu0_freq);
		}
	}

	/* check whether VPU1 opp's value match info */
	if ((apusys_get_power_on_status(VPU1) == true &&
	     info->vpu1_freq != 0 &&
	     info->vpu1_freq != default_freq)) {
		vpu1_freq = apusys_get_dvfs_freq(V_VPU1) / info->dump_div;
		if (((abs(vpu1_freq - info->vpu1_freq) * 100) >
			vpu1_freq * ASSERTION_PERCENTAGE) &&
			(vpu1_freq != 0)) {
			LOG_WRN("ASSERT vpu1_freq=%d, info->vpu1_freq=%d\n",
						vpu1_freq, info->vpu1_freq);
		}
	}

	if (apusys_get_power_on_status(MDLA0) == true &&
		info->mdla0_freq != 0) {
		mdla0_freq = apusys_get_dvfs_freq(V_MDLA0) / info->dump_div;
		if ((abs(mdla0_freq - info->mdla0_freq) * 100) >
			mdla0_freq * ASSERTION_PERCENTAGE &&
			(mdla0_freq != 0)) {
			LOG_WRN("ASSERT mdla0_freq=%d, info->mdla0_freq=%d\n",
					mdla0_freq, info->mdla0_freq);
		}
	}


	if (apusys_get_conn_power_on_status() == true &&
		info->conn_freq != 0) {
		conn_freq = apusys_get_dvfs_freq(V_APU_CONN)/info->dump_div;
		if ((abs(conn_freq - info->conn_freq) * 100) >
			conn_freq * ASSERTION_PERCENTAGE &&
			(conn_freq != 0)) {
			LOG_WRN("ASSERT conn_freq=%d, info->conn_freq=%d\n",
					conn_freq, info->conn_freq);
		}
	}


	/*
	 * Vvpu, Vcore, Vsram anad Vcore's constrain.
	 * If (Vvpu or Vcore) > 0.75v
	 *	Vsram has to be 0.8v
	 *
	 * If (Vvpu And Vcore) <= 0.75v
	 *	Vsram has to be 0.75v
	 */
	if ((vvpu > VSRAM_TRANS_VOLT || vcore > VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_LOW_VOLT)
		LOG_WRN("ASSERT vvpu=%d, vcore=%d, vsram=%d\n",
				vvpu, vcore, vsram);

	if ((vvpu < VSRAM_TRANS_VOLT && vcore < VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_HIGH_VOLT)
		LOG_WRN("ASSERT vvpu=%d, vcore=%d, vsram=%d\n",
				vvpu, vcore, vsram);
}

void constraints_check_stress(int opp)
{
	int i = 0, j = 0;
	int m = 0;
	int count = 0;
	int loop = opp;
	struct apu_power_info info = {0};

for (loop = 0; loop < count; loop++) {
	for (i = 0 ; i < APUSYS_MAX_NUM_OPPS ; i++) {
		apusys_set_opp(VPU0, i);
		for (j = 0 ; j < APUSYS_MAX_NUM_OPPS; j++) {
			apusys_set_opp(VPU1, j);
			for (m = 0 ; m < APUSYS_MAX_NUM_OPPS; m++) {
				count++;
				LOG_WRN(
				"## dvfs conbinational test, count = %d ##\n",
						count);
				apusys_set_opp(
					MDLA0, m);
				info.id = 0;
				info.type = 0;

				apusys_dvfs_policy(0);

				hal_config_power(
				PWR_CMD_GET_POWER_INFO,
					VPU0, &info);
				apu_power_assert_check(
						&info);
				udelay(100);
			}
		}
	}
}

}

void voltage_constraint_check(void)
{
	struct apu_power_info info = {0};

	dump_voltage(&info);
	/*
	 * Vvpu, Vcore, Vsram anad Vcore's constrain.
	 * If (Vvpu or Vcore) > 0.75v
	 *	Vsram has to be 0.8v
	 *
	 * If (Vvpu And Vcore) <= 0.75v
	 *	Vsram has to be 0.75v
	 */
	if ((info.vvpu > VSRAM_TRANS_VOLT || info.vcore > VSRAM_TRANS_VOLT)
			&& info.vsram == VSRAM_LOW_VOLT) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vcore=%d, vsram=%d\n",
				info.vvpu, info.vcore, info.vsram);
	}

	if ((info.vvpu < VSRAM_TRANS_VOLT && info.vcore < VSRAM_TRANS_VOLT)
			&& info.vsram == VSRAM_HIGH_VOLT) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vcore=%d, vsram=%d\n",
				info.vvpu, info.vcore, info.vsram);
	}
}

