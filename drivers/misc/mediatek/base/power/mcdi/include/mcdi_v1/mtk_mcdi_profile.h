/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_PROFILE_H__
#define __MTK_MCDI_PROFILE_H__

#include <linux/proc_fs.h>
#include <linux/cpuidle.h>

#include <mtk_mcdi_plat.h>

/* #define MCDI_PWR_SEQ_PROF_BREAKDOWN */

#define MCDI_LAT_NAME_SIZE 36

enum {
	MCDI_PROF_FLAG_STOP,
	MCDI_PROF_FLAG_START,
	MCDI_PROF_FLAG_POLLING,
	NF_MCDI_PROF_FLAG
};

enum {
	MCDI_PROFILE_ENTER = 0,
	MCDI_PROFILE_CPU_DORMANT_ENTER,
	MCDI_PROFILE_CPU_DORMANT_LEAVE,
	MCDI_PROFILE_LEAVE,

	MCDI_PROFILE_RSV0,
	MCDI_PROFILE_RSV1,

	NF_MCDI_PROFILE,
};

struct mcdi_prof_lat_raw {
	unsigned int cnt;
	unsigned int max;
	union {
		unsigned long long avg;
		unsigned long long sum;
	};
};

struct mcdi_prof_lat_data {
	const char *name;
	bool valid;
	int start;
	int end;
	unsigned long long *start_ts;
	unsigned long long *end_ts;
	struct mcdi_prof_lat_raw curr[NF_CPU_TYPE];
	struct mcdi_prof_lat_raw result[NF_CPU_TYPE];
	unsigned long long ts[NF_CPU];
};

struct mcdi_prof_lat {
	bool enable;
	bool pause;
	struct mcdi_prof_lat_data section[NF_MCDI_PROFILE];
};

struct mcdi_prof_dev {
	int cpu;
	int last_residency;
	int last_state_idx;
	int actual_state;
	unsigned long long enter;
	unsigned long long leave;
	struct {
		unsigned int cnt;
		unsigned long long dur;
	} state[CPUIDLE_STATE_MAX];
};

struct mcdi_prof_usage {
	bool enable;
	bool pause;
	unsigned int cpu_valid;
	unsigned long long start;
	unsigned long long prof_dur;
	int last_id[NF_CLUSTER];
	struct mcdi_prof_dev dev[NF_CPU];
};

/* this define should sync with mcdi in sspm */
#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
enum prof_bk_onoff {
	MCDI_PROF_BK_ON = 0,
	MCDI_PROF_BK_OFF,
	MCDI_PROF_BK_ONOFF_NUM,
};

enum prof_bk_ts {
	CLUSTER,
	CPU,
	ARMPLL,
	BUCK,

	MCDI_PROF_BK_NUM,
};

struct mcdi_prof_breakdown_item {
	unsigned int item[MCDI_PROF_BK_NUM][NF_CLUSTER];
	unsigned int count[MCDI_PROF_BK_NUM][NF_CLUSTER];
};

struct mcdi_prof_breakdown {
	struct mcdi_prof_breakdown_item onoff[MCDI_PROF_BK_ONOFF_NUM];
};
#endif

unsigned int mcdi_usage_get_cnt(int cpu, int state_idx);
void mcdi_usage_time_start(int cpu);
void mcdi_usage_time_stop(int cpu);
void mcdi_usage_calc(int cpu);
bool mcdi_usage_cpu_valid(int cpu);

void mcdi_profile_ts(int cpu_idx, unsigned int prof_idx);
void mcdi_profile_calc(int cpu);

void mcdi_prof_core_cluster_off_token(int cpu);
void mcdi_prof_set_idle_state(int cpu, int state);
void mcdi_prof_init(void);
void mcdi_procfs_profile_init(struct proc_dir_entry *mcdi_dir);

#endif /* __MTK_MCDI_PROFILE_H__ */
