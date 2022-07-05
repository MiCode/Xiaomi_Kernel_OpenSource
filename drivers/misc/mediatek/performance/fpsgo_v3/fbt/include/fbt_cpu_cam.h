/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FBT_CPU_CAM_H_
#define _FBT_CPU_CAM_H_

#define QUOTA_RAW_SIZE 300

struct fbt_cam_group {
	struct rb_node rb_node;
	int group_id;
	int blc_wt;
};

struct fbt_cam_thread {
	struct rb_root group_tree;
	struct rb_node rb_node;
	pid_t tid;
};

struct fbt_cam_frame {
	struct hlist_node hlist;
	int group_id;
	int *dep_list;
	int dep_list_num;
	int blc_wt;
	long *area;
	unsigned long long ts;
	unsigned long long last_runtime;
	unsigned long long cpu_time;
	unsigned long long q2q_time;
	unsigned long long target_time;
	struct xgf_thread_loading ploading;
	struct fbt_thread_blc *p_blc;

	int frame_count;

	// rescue
	int rescue_pct_1;
	int rescue_f_1;
	struct fbt_proc proc_1;
	int rescue_pct_2;
	int rescue_f_2;
	struct fbt_proc proc_2;

	// gcc
	int quota_raw[QUOTA_RAW_SIZE];
	int quota_raw_idx;
	int prev_total_quota;
	int cur_total_quota;
	int gcc_std_filter;
	int gcc_history_window;
	int gcc_up_check;
	int gcc_up_thrs;
	int gcc_up_step;
	int gcc_down_check;
	int gcc_down_thrs;
	int gcc_down_step;
	int gcc_correction;
	unsigned long long gcc_target_time;
};

void fpsgo_fbt2cam_notify_uclamp_boost_enable(int enable);
int __init fbt_cpu_cam_init(void);
void __exit fbt_cpu_cam_exit(void);

#endif
