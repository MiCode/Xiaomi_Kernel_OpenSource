/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __THERMAL_JATM_H__
#define __THERMAL_JATM_H__

/* jatm parameters (in ms) */
#define THER_INVALID_TEMP -274000

#define DEFAULT_JATM_STOP_DEADLINE 133  // 167 - 16.6 * 2
#define DEFAULT_JATM_INTERVAL 1000
#define DEFAULT_JATM_INFINITE_BUDGET 5000

struct jatm_policy {
	unsigned int               cpu;
	s32                        cpu_limit;
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct list_head           jatm_list;
};

enum jatm_stop_reason {
	FRAME_COMPLETE = 0,
	BUDGET_RUNNING_OUT = 1
};

char *jatm_stop_reason_string[] = {
	"0: Frame_complete",
	"1: JATM_budget_running_out"
};

struct jatm_record {
	struct timespec64 end_tv;
	int usage;
	struct list_head list;
};

enum jatm_mode {
	BUDGET,
	STOP_DEADLINE
};

enum jatm_not_start_reason {
	ENABLE = 0,
	ALREADY_ENABLED,
	MIN_TTJ,
	SUSPENDED,
	NO_BUDGET
};

extern void (*jatm_notify_fp)(int enable);

#endif
