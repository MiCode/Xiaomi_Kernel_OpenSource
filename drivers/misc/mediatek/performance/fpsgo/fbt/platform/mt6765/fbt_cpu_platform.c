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

void fbt_notify_CM_limit(int reach_limit)
{
	cm_mgr_perf_set_status(reach_limit);
	fpsgo_systrace_c_fbt_gm(-100, reach_limit, "notify_cm");
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = (base_blc << 10) / 100U;
	base_blc = clamp(base_blc, 1U, 1024U);
	capacity_min_write_for_perf_idx(CGROUP_TA, (int)base_blc);
}

void fbt_clear_boost_value(void)
{
	capacity_min_write_for_perf_idx(CGROUP_TA, 0);
	fbt_notify_CM_limit(0);
}

int fbt_is_mips_different(void)
{
	return 1;
}

int fbt_get_L_min_ceiling(void)
{
	return 1100000;
}

int fbt_get_L_cluster_num(void)
{
	return 1;
}

