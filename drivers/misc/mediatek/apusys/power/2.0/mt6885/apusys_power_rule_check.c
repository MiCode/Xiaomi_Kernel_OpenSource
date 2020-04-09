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
		apusys_get_power_on_status(VPU2) == true ||
		apusys_get_power_on_status(MDLA0) == true ||
		apusys_get_power_on_status(MDLA1) == true)
		return true;

	return false;
}

void apu_power_assert_check(struct apu_power_info *info)
{
	int dsp_freq;
	int dsp1_freq;
	int dsp2_freq;
	int dsp3_freq;
	int dsp5_freq;
	int dsp6_freq;
	int dsp7_freq;
	//int ipuif_freq = apusys_get_dvfs_freq(V_VCORE)/1000;

	int vvpu = info->vvpu * info->dump_div;
	int vmdla = info->vmdla * info->dump_div;
	int vsram = info->vsram * info->dump_div;
	int vcore = info->vcore * info->dump_div;

	if (apusys_get_power_on_status(VPU0) == true && info->dsp1_freq != 0) {
		dsp1_freq = apusys_get_dvfs_freq(V_VPU0)/info->dump_div;
		if ((abs(dsp1_freq - info->dsp1_freq) * 100) >
			dsp1_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp1_freq=%d, info->dsp1_freq=%d\n",
						dsp1_freq, info->dsp1_freq);
		}
	}

	if (apusys_get_power_on_status(VPU1) == true && info->dsp2_freq != 0) {
		dsp2_freq = apusys_get_dvfs_freq(V_VPU1)/info->dump_div;
		if ((abs(dsp2_freq - info->dsp2_freq) * 100) >
			dsp2_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp2_freq=%d, info->dsp2_freq=%d\n",
						dsp2_freq, info->dsp2_freq);
		}
	}

	if (dvfs_user_support(VPU2) &&
		apusys_get_power_on_status(VPU2) == true &&
			info->dsp3_freq != 0) {
		dsp3_freq = apusys_get_dvfs_freq(V_VPU2)/info->dump_div;
		if ((abs(dsp3_freq - info->dsp3_freq) * 100) >
			dsp3_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp3_freq=%d, info->dsp3_freq=%d\n",
						dsp3_freq, info->dsp3_freq);
		}
	}

	if ((apusys_get_power_on_status(MDLA0) == true ||
				(dvfs_user_support(MDLA1) &&
				 apusys_get_power_on_status(MDLA1) == true)) &&
			info->dsp6_freq != 0) {
		dsp5_freq = apusys_get_dvfs_freq(V_MDLA0)/info->dump_div;
		dsp6_freq = apusys_get_dvfs_freq(V_MDLA1)/info->dump_div;
		if (((abs(dsp5_freq - info->dsp6_freq) * 100) >
			dsp5_freq * ASSERTION_PERCENTAGE) &&
			((abs(dsp6_freq - info->dsp6_freq) * 100) >
			dsp6_freq * ASSERTION_PERCENTAGE)) {
			LOG_WRN("ASSERT dsp5=%d, dsp6=%d, info->dsp6_freq=%d\n",
					dsp5_freq, dsp6_freq, info->dsp6_freq);
		}
	}


	if (apusys_get_conn_power_on_status() == true) {
		if (info->dsp_freq != 0) {
			dsp_freq =
			apusys_get_dvfs_freq(V_APU_CONN)/info->dump_div;
			if ((abs(dsp_freq - info->dsp_freq) * 100) >
				dsp_freq * ASSERTION_PERCENTAGE) {
				LOG_WRN(
				"ASSERT dsp_freq=%d, info->dsp_freq=%d\n",
						dsp_freq, info->dsp_freq);
			}
		}

		if (info->dsp7_freq != 0) {
			dsp7_freq =
			apusys_get_dvfs_freq(V_TOP_IOMMU)/info->dump_div;
			if ((abs(dsp7_freq - info->dsp7_freq) * 100) >
					dsp7_freq * ASSERTION_PERCENTAGE) {
				LOG_WRN(
				"ASSERT dsp7_freq=%d, info->dsp7_freq=%d\n",
						dsp7_freq, info->dsp7_freq);
			}
		}
	}


#if 0   // dvfs don't use vcore
	if (abs(ipuif_freq - info->ipuif_freq) >
			ipuif_freq * ASSERTION_PERCENTAGE) {
		apu_aee_warn("VCORE",
				"ipuif_freq=%d, info->ipuif_freq=%d\n",
				ipuif_freq, info->ipuif_freq)
	}
#endif

	if (vvpu == DVFS_VOLT_00_575000_V && vmdla >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
				vvpu, vmdla);
	}

	if (vmdla == DVFS_VOLT_00_575000_V && vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
				vvpu, vmdla);
	}

	if (vcore == DVFS_VOLT_00_575000_V && vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vcore=%d\n",
				vvpu, vcore);
	}

	if ((vvpu > VSRAM_TRANS_VOLT || vmdla > VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_LOW_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				vvpu, vmdla, vsram);
	}

	if ((vvpu < VSRAM_TRANS_VOLT && vmdla < VSRAM_TRANS_VOLT)
			&& vsram == VSRAM_HIGH_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				vvpu, vmdla, vsram);
	}
}

void constraints_check_stress(int opp)
{
	int i = 0, j = 0;
	int m = 0, n = 0, o = 0;
	int count = 0;
	int loop = opp;
	struct apu_power_info info = {0};

for (loop = 0; loop < count; loop++) {
	for (i = 0 ; i < APUSYS_MAX_NUM_OPPS ; i++) {
		apusys_set_opp(VPU0, i);
		for (j = 0 ; j < APUSYS_MAX_NUM_OPPS; j++) {
			apusys_set_opp(VPU1, j);
			for (m = 0 ; m < APUSYS_MAX_NUM_OPPS; m++) {
				apusys_set_opp(VPU2, m);
				for (n = 0 ; n < APUSYS_MAX_NUM_OPPS;
					n++) {
					apusys_set_opp(MDLA0, n);
					for (o = 0 ; o < APUSYS_MAX_NUM_OPPS;
					o++) {
						count++;
						LOG_WRN(
						"## dvfs conbinational test, count = %d ##\n",
								count);
						apusys_set_opp(
							MDLA1, o);
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
}

}

void voltage_constraint_check(void)
{
	struct apu_power_info info = {0};

	dump_voltage(&info);

	if (info.vvpu == DVFS_VOLT_00_575000_V &&
			info.vmdla >= DVFS_VOLT_00_800000_V) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vmdla=%d\n",
				info.vvpu, info.vmdla);
	}

	if (info.vmdla == DVFS_VOLT_00_575000_V &&
			info.vvpu >= DVFS_VOLT_00_800000_V) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vmdla=%d\n",
				info.vvpu, info.vmdla);
	}

	if (info.vcore == DVFS_VOLT_00_575000_V &&
			info.vvpu >= DVFS_VOLT_00_800000_V) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vcore=%d\n",
				info.vvpu, info.vcore);
	}

	if ((info.vvpu > VSRAM_TRANS_VOLT || info.vmdla > VSRAM_TRANS_VOLT)
			&& info.vsram == VSRAM_LOW_VOLT) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				info.vvpu, info.vmdla, info.vsram);
	}

	if ((info.vvpu < VSRAM_TRANS_VOLT && info.vmdla < VSRAM_TRANS_VOLT)
			&& info.vsram == VSRAM_HIGH_VOLT) {
		apu_aee_warn("APU PWR Constraint",
				"ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
				info.vvpu, info.vmdla, info.vsram);
	}
}

