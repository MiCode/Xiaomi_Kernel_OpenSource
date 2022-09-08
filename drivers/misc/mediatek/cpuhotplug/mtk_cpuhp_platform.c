// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#define pr_fmt(fmt) "cpuhp: " fmt

#include "mtk_cpuhp_private.h"

int __weak cpuhp_platform_cpuon(int cluster, int cpu, int isalone, int action)
{
	return 0;
}

int __weak cpuhp_platform_cpuoff(int cluster, int cpu, int isalone, int action)
{
	poweroff_cpu(cpu);

	if (isalone)
		poweroff_cluster(cluster);

	return 0;
}


int __weak cpuhp_platform_init(void)
{
	return 0;
}
