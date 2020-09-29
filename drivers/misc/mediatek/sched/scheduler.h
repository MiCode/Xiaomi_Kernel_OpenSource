/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#define MAX_PD_COUNT 3
#define MAX_CAP_ENTRYIES 168

#define DVFS_TBL_BASE_PHYS 0x0011BC00
#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

struct pd_capacity_info {
	int nr_caps;
	unsigned long *caps;
	struct cpumask cpus;
};

#if defined(CONFIG_MTK_OPP_CAP_INFO)
extern int pd_freq_to_opp(int cpu, unsigned long freq);
extern unsigned long pd_get_opp_capacity(int cpu, int opp);
#endif

#endif
