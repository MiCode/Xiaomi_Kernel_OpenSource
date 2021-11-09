/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef USER_H
#define USER_H

#define TEEPERF_DEVNODE	"teeperf"

#define TEEPERF_IOC_MAGIC	'T'
#define TEEPERF_IO_HIGH_FREQ	_IO(TEEPERF_IOC_MAGIC, 0)

#define FREQ_LEVEL_INDEX	23

#define PFX	"[TEEPERF]: "

extern u32 cpu_map;

enum cpu_map_type {
	CPU_4_3_1_MAP = 1,
	CPU_6_2_MAP = 2
};

int teeperf_user_init(struct cdev *cdev);
static inline void teeperf_user_exit(void)
{
}

#endif /* USER_H */
