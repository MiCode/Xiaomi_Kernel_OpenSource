/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _DVFS_V2_
#define _DVFS_V2_

#define MAX_HISTORY 10
#define MIN_SUBMIT_GAP 2000		/* 2ms */
#define MAX_SUBMIT_GAP (1000*1000)	/* 1 second */
#define FREE_HIST_DELAY (5000*1000)	/* Free history delay */

struct codec_history {
	void *handle;
	int kcy[MAX_HISTORY];
	long long submit[MAX_HISTORY];
	long long start[MAX_HISTORY];
	long long end[MAX_HISTORY];
	int cur_idx;
	int cur_cnt;
	int tot_kcy;
	long long tot_time;
	struct codec_history *next;
};

struct codec_job {
	void *handle;
	long long submit;
	long long start;
	long long end;
	int mhz;
	struct codec_job *next;
};

long long get_time_us(void);

/* Add a new job to job queue */
struct codec_job *add_job(void *handle, struct codec_job **head);

/* Move target job to queue head for processing */
struct codec_job *move_job_to_head(void *handle, struct codec_job **head);

/* Update history with completed job */
int update_hist(struct codec_job *job, struct codec_history **head);

/* Estimate required freq from job queue and previous history */
int est_freq(void *handle, struct codec_job **job, struct codec_history *head);
u64 match_freq(int target_mhz, u64 *freq_list, u32 freq_cnt);

/* Free unused/all history */
int free_hist(struct codec_history **head, int only_unused);
#endif
