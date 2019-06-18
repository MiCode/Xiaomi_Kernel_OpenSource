// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "../sched.h"

bool is_intra_domain(int prev, int target)
{
	return arch_cpu_cluster_id(prev) ==
			arch_cpu_cluster_id(target);
}
