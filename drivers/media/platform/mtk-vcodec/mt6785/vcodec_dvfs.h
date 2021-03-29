/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Cheng-Jung Ho <cheng-jung.ho@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VCODEC_DVFS_H__
#define __VCODEC_DVFS_H__

#define MAX_HISTORY 10
#define MIN_SUBMIT_GAP 2000		/* 2ms */
#define MAX_SUBMIT_GAP (1000*1000)	/* 1 second */
#define FREE_HIST_DELAY (5000*1000)	/* Free history delay */
#define DEFAULT_MHZ 99999

struct codec_history {
	void *handle;
	int kcy[MAX_HISTORY];
	long long submit[MAX_HISTORY];
	long long start[MAX_HISTORY];
	long long end[MAX_HISTORY];
	long long sw_time[MAX_HISTORY];
	long long submit_interval;
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
	int hw_kcy;
	int mhz;
	struct codec_job *next;
};

long long get_time_us(void);

/* Add a new job to job queue */
struct codec_job *add_job(void *handle, struct codec_job **head);

/* Move target job to queue head for processing */
struct codec_job *move_job_to_head(void *handle, struct codec_job **head);

/* Update history with completed job */
int update_hist(struct codec_job *job, struct codec_history **head,
		long long submit_interval);

/* Estimate required freq from job queue and previous history */
int est_freq(void *handle, struct codec_job **job, struct codec_history *head);
u64 match_freq(int target_mhz, u64 *freq_list, u32 freq_cnt);

/* Free unused/all history */
int free_hist(struct codec_history **head, int only_unused);
int free_hist_by_handle(void *handle, struct codec_history **head);
#endif
