/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef USER_H
#define USER_H

#define TEEPERF_DEVNODE	"teeperf"

#define TEEPERF_IOC_MAGIC	'T'
#define TEEPERF_IO_HIGH_FREQ	_IO(TEEPERF_IOC_MAGIC, 0)

#define SUPER_CPU_FREQ_LEVEL_INDEX	0
//#define BIG_CPU_FREQ_LEVEL_INDEX	19
//#define LITTLE_CPU_FREQ_LEVEL_INDEX_4_3_1	7
#define LITTLE_CPU_FREQ_LEVEL_INDEX_6_2		1
#define BIG_CPU_FREQ_LEVEL_INDEX	7	//2.4/2.1 GHZ for MT6985/MT6886
#define LITTLE_CPU_FREQ_LEVEL_INDEX_4_3_1	0

/* The CPU enter TEE */
#define TEE_CPU	0x6

#define PFX	"[TEEPERF]: "

extern u32 cpu_type;
extern u32 cpu_map;

enum teeperf_cpu_type {
	CPU_V9_TYPE = 1,
	CPU_V8_TYPE = 2
};

enum teeperf_cpu_map {
	CPU_4_3_1_MAP = 1,
	CPU_6_2_MAP = 2
};

enum teeperf_cpu_group {
	CPU_SUPER_GROUP = 1,
	CPU_BIG_GROUP = 2,
	CPU_LITTLE_GROUP = 3
};

void teeperf_set_cpu_group_to_high_freq(enum teeperf_cpu_group group,
					u32 high_freq);
int teeperf_user_init(struct cdev *cdev);
static inline void teeperf_user_exit(void)
{
}

#endif /* USER_H */
