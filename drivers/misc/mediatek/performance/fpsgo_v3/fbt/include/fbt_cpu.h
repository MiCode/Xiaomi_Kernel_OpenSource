/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_H__
#define __FBT_CPU_H__

#include "fpsgo_base.h"
#include "fbt_cpu_ctrl.h"

extern int fpsgo_fbt2xgf_get_dep_list_num(int pid, unsigned long long bufID);
extern int fpsgo_fbt2xgf_get_dep_list(int pid, int count,
		struct fpsgo_loading *arr, unsigned long long bufID);

#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
void fpsgo_ctrl2fbt_dfrc_fps(int fps_limit);
void fpsgo_ctrl2fbt_cpufreq_cb_exp(int cid, unsigned long freq);
void fpsgo_ctrl2fbt_vsync(unsigned long long ts);
int fpsgo_ctrl2fbt_switch_uclamp(int enable);
void fpsgo_comp2fbt_frame_start(struct render_info *thr,
		unsigned long long ts);
void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts);

void fpsgo_base2fbt_node_init(struct render_info *obj);
void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr);
int fpsgo_base2fbt_get_max_blc_pid(int *pid, unsigned long long *buffer_id);
void fpsgo_base2fbt_check_max_blc(void);
void fpsgo_base2fbt_no_one_render(void);
void fpsgo_base2fbt_only_bypass(void);
void fpsgo_base2fbt_set_min_cap(struct render_info *thr, int min_cap,
					int min_cap_b, int min_cap_m);
void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr);
void fpsgo_base2fbt_cancel_jerk(struct render_info *thr);
int fpsgo_base2fbt_is_finished(struct render_info *thr);
void fpsgo_base2fbt_stop_boost(struct render_info *thr);
void fpsgo_sbe2fbt_rescue(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id);
void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
		unsigned long long t_duration);


int __init fbt_cpu_init(void);
void __exit fbt_cpu_exit(void);

int fpsgo_ctrl2fbt_switch_fbt(int enable);
int fbt_switch_ceiling(int value);

void fbt_set_limit(int cur_pid, unsigned int blc_wt,
	int pid, unsigned long long buffer_id,
	int dep_num, struct fpsgo_loading dep[],
	struct render_info *thread_info, long long runtime);
int fbt_get_max_cap(int floor, int bhr_opp_local,
	int bhr_local, int pid, unsigned long long buffer_id);
unsigned int fbt_get_new_base_blc(struct cpu_ctrl_data *pld,
	int floor, int enhance, int eenhance_opp, int headroom);
int fbt_limit_capacity(int blc_wt, int is_rescue);
void fbt_set_ceiling(struct cpu_ctrl_data *pld,
	int pid, unsigned long long buffer_id);
struct fbt_thread_blc *fbt_xgff_list_blc_add(int pid,
	unsigned long long buffer_id);
void fbt_xgff_list_blc_del(struct fbt_thread_blc *p_blc);
void fbt_xgff_blc_set(struct fbt_thread_blc *p_blc, int blc_wt,
	int dep_num, int *dep_arr);
int fbt_xgff_dep_thread_notify(int pid, int op);

void fbt_set_render_boost_attr(struct render_info *thr);
void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts);

#else
static inline void fpsgo_ctrl2fbt_dfrc_fps(int fps_limit) { }
static inline void fpsgo_ctrl2fbt_cpufreq_cb_exp(int cid,
		unsigned long freq) { }
static inline void fpsgo_ctrl2fbt_vsync(unsigned long long ts) { }
int fpsgo_ctrl2fbt_switch_uclamp(int enable) { return 0; }
static inline void fpsgo_comp2fbt_frame_start(struct render_info *thr,
	unsigned long long ts) { }
static inline void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts) { }

static inline int fbt_cpu_init(void) { return 0; }
static inline void fbt_cpu_exit(void) { }

static inline int fpsgo_ctrl2fbt_switch_fbt(int enable) { return 0; }

static inline void fpsgo_base2fbt_node_init(struct render_info *obj) { }
static inline void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr) { }
static inline int fpsgo_base2fbt_get_max_blc_pid(int *pid,
		unsigned long long *buffer_id) { return 0; }
static inline void fpsgo_base2fbt_check_max_blc(void) { }
static inline void fpsgo_base2fbt_no_one_render(void) { }
static inline int fbt_switch_ceiling(int en) { return 0; }
static inline void fpsgo_base2fbt_set_min_cap(struct render_info *thr,
				int min_cap) { }
static inline void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr) { }
static inline void fpsgo_base2fbt_cancel_jerk(struct render_info *thr) { }
static inline int fpsgo_base2fbt_is_finished(struct render_info *thr) { return 0; }
static inline void fpsgo_base2fbt_stop_boost(struct render_info *thr) { }
static inline void fpsgo_sbe2fbt_rescue(struct render_info *thr, int start, int enhance,
			unsigned long long frame_id) { }
static inline void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
			unsigned long long t_duration) { }
static inline void fbt_set_render_boost_attr(struct render_info *thr) { }
static inline void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts) { }
#endif

#endif
