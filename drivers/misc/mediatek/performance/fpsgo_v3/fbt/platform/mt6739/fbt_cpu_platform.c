/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "eas_ctrl.h"
#include "fbt_cpu_platform.h"
#include <fpsgo_common.h>
#include <linux/pm_qos.h>
#include <mtk_vcorefs_governor.h>
#include <mtk_vcorefs_manager.h>
#include <mach/mtk_ppm_api.h>

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
		fpsgo_systrace_c_fbt_gm(-100, ret, "fbt_dram_arbitration_ret");

	fpsgo_systrace_c_fbt_gm(-100, cm_req, "cm_req");
	fpsgo_systrace_c_fbt_gm(-100, ultra_req, "ultra_req");
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
	fpsgo_systrace_c_fbt_gm(-100, base_blc, "TA_cap");

	/* single cluster for mt6739 */
	cpi = ppm_get_cluster_cpi(0);
	fpsgo_systrace_c_fbt_gm(-100, cpi, "cpi");
	cm_req = base_blc > cpi_uclamp_thres && cpi > cpi_thres;

	fbt_dram_arbitration();
}

void fbt_clear_boost_value(void)
{
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, 0);
	fpsgo_systrace_c_fbt_gm(-100, 0, "TA_cap");

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

	if (!pid)
		return;

#ifdef CONFIG_UCLAMP_TASK
	ret = set_task_util_min_pct(pid, base_blc);
#endif
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, base_blc, "min_cap");
}

int fbt_get_L_cluster_num(void)
{
	return 0;
}

int fbt_get_L_min_ceiling(void)
{
	int freq = 0;

	return freq;
}

int fbt_get_default_boost_ta(void)
{
	return 1;
}
