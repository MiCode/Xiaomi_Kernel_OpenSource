/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_MCDI_GOVERNOR_H__
#define __MTK_MCDI_GOVERNOR_H__

#include <linux/hrtimer.h>

enum {
	PAUSE_CNT	= 0,
	MULTI_CORE_CNT,
	LATENCY_CNT,
	RESIDENCY_CNT,
	LAST_CORE_CNT,
	NF_ANY_CORE_CPU_COND_INFO
};

struct mcdi_feature_status {
	bool enable;
	bool pause;
	bool cluster_off;
	bool any_core;
	int s_state;
	unsigned int pauseby;
};

struct mcdi_cluster_dev {
	int owner;

	bool chk_res_each_core;
	bool use_max_remain;
	unsigned int chk_res_cnt;
	unsigned int chk_res_fail;

	bool tmr_en;
	bool tmr_running;
	struct hrtimer timer;
};

int mcdi_governor_select(int cpu, int cluster_idx);
void mcdi_governor_reflect(int cpu, int state);
void mcdi_avail_cpu_cluster_update(void);
void mcdi_governor_init(void);
void set_mcdi_enable_status(bool enabled);
void set_mcdi_s_state(int state);
void set_mcdi_idle_state(int cpu, int state);
const struct mcdi_feature_status *get_mcdi_feature_stat(void);
struct mcdi_cluster_dev *get_mcdi_cluster_dev(void);
void get_mcdi_feature_status(struct mcdi_feature_status *stat);
void get_mcdi_avail_mask(unsigned int *cpu_mask, unsigned int *cluster_mask);
int get_residency_latency_result(int cpu);
int get_cluster_off_token(int cpu);
void mcdi_state_pause(unsigned int id, bool pause);
void any_core_cpu_cond_get(unsigned long buf[NF_ANY_CORE_CPU_COND_INFO]);
void any_core_cpu_cond_inc(int idx);
bool is_mcdi_working(void);
bool is_last_core_in_mcusys(int cpu);
bool is_last_core_in_cluster(int cpu);
unsigned int mcdi_get_gov_data_num_mcusys(void);
void mcdi_cluster_chk_res_each_core_set(bool en);
void mcdi_cluster_tmr_en_set(bool en);

void idle_refcnt_inc(void);
void idle_refcnt_dec(void);
int all_cpu_idle_ratio_get(void);
bool is_all_cpu_idle_criteria(void);
unsigned int mcdi_get_boot_time_check(void);

unsigned int mcdi_get_mtk_idle_mode(void);

#endif /* __MTK_MCDI_GOVERNOR_H__ */
