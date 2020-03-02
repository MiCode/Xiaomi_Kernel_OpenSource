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

#include "eas_controller.h"
#include "fbt_cpu_platform.h"

#define CLUSTER_FREQ 0

void fbt_set_boost_value(int cluster, unsigned int base_blc)
{
	if (cluster == 1)
		update_eas_boost_value(EAS_KIR_FBC, CGROUP_TA, (base_blc - 1) + 3200);
	else if (cluster == 0)
		update_eas_boost_value(EAS_KIR_FBC, CGROUP_TA, (base_blc - 1) + 3100);
}

void fbt_init_cpuset_freq_bound_table(void)
{
	switch (cluster_num) {
	case 2:
		cluster_freq_bound[0] = CLUSTER_FREQ;
		cluster_rescue_bound[0] = CLUSTER_FREQ;
		break;
	default:
		break;
	}
}

