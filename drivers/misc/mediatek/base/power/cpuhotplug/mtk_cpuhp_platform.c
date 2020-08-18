/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
