/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "eas_ctrl.h"
#include "fbt_cpu_platform.h"
#include <fpsgo_common.h>
#include <linux/pm_qos.h>
#include <mtk_vcorefs_governor.h>
#include <mtk_vcorefs_manager.h>
#include <mach/mtk_ppm_api.h>
#include <linux/cpumask.h>

int ultra_req;
int cm_req;
/*
 * cpi_uclamp_thres = 123500000 / 149500000 * 100
 */
static int cpi_thres = 250;
static unsigned int cpi_uclamp_thres = 82;

void fbt_notify_CM_limit(int reach_limit)
{
}

void fbt_reg_dram_request(int reg)
{
}

void fbt_dram_arbitration(void)
{
	int ret = -1;

	if (cm_req || ultra_req)
		ret = vcorefs_request_dvfs_opp(KIR_FBT, 0);
	else
		ret = vcorefs_request_dvfs_opp(KIR_FBT, -1);

	if (ret < 0)
		fpsgo_systrace_c_fbt_gm(-100, 0, ret,
			"fbt_dram_arbitration_ret");

	fpsgo_systrace_c_fbt_gm(-100, 0, cm_req, "cm_req");
	fpsgo_systrace_c_fbt_gm(-100, 0, ultra_req, "ultra_req");
}

void fbt_boost_dram(int boost)
{

	if (boost == ultra_req)
		return;

	ultra_req = boost;
	fbt_dram_arbitration();

}

void fbt_set_boost_value(unsigned int base_blc)
{
	int cpi = 0;

	base_blc = clamp(base_blc, 1U, 100U);
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, (int)base_blc);
	fpsgo_systrace_c_fbt_gm(-100, 0, base_blc, "TA_cap");

	/* single cluster for mt6739 */
	cpi = ppm_get_cluster_cpi(0);
	fpsgo_systrace_c_fbt_gm(-100, 0, cpi, "cpi");
	cm_req = base_blc > cpi_uclamp_thres && cpi > cpi_thres;

	fbt_dram_arbitration();
}

void fbt_clear_boost_value(void)
{
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, 0);
	fpsgo_systrace_c_fbt_gm(-100, 0, 0, "TA_cap");

	cm_req = 0;
	ultra_req = 0;
	fbt_notify_CM_limit(0);
	fbt_dram_arbitration();
}

/*
 * mt6739 use boost_ta to support CM optimize.
 * per-task uclamp doesn't support CM optimize.
 */
void fbt_set_per_task_min_cap(int pid, unsigned int base_blc)
{
	int ret = -1;
	unsigned int base_blc_1024;

	if (!pid)
		return;

	base_blc_1024 = (base_blc << 10) / 100U;
	base_blc_1024 = clamp(base_blc_1024, 1U, 1024U);

#ifdef CONFIG_UCLAMP_TASK
	ret = set_task_util_min(pid, base_blc_1024);
#endif
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, 0, base_blc, "min_cap");
}

void fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{

}

void fbt_set_cpu_prefer(int pid, unsigned int prefer_type)
{

}

int fbt_get_L_min_ceiling(void)
{
	return 0;
}

int fbt_get_default_boost_ta(void)
{
	return 1;
}

int fbt_get_default_adj_loading(void)
{
	return 0;
}

int fbt_get_default_adj_count(void)
{
	return 30;
}

int fbt_get_default_adj_tdiff(void)
{
	return 2000000;
}

int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq)
{
	return 0;
}

int fbt_get_default_uboost(void)
{
	return 0;
}

int fbt_get_default_qr_enable(void)
{
	return 0;
}

int fbt_get_default_gcc_enable(void)
{
	return 0;
}

int fbt_get_l_min_bhropp(void)
{
	return 0;
}

int fbt_get_L_cluster_num(void)
{
	return 0;
}

