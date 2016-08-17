/*
 * drivers/misc/tegra-profiler/hrt.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_HRT_H
#define __QUADD_HRT_H

#define QUADD_MAX_STACK_DEPTH		64

#ifdef __KERNEL__

#include <linux/hrtimer.h>
#include <linux/limits.h>
#include <linux/kprobes.h>

#include "backtrace.h"

#define QUADD_USE_CORRECT_SAMPLE_TS	1

struct quadd_thread_data {
	pid_t pid;
	pid_t tgid;
};

struct quadd_cpu_context {
	struct hrtimer hrtimer;

	struct quadd_callchain callchain_data;
	char mmap_filename[PATH_MAX];

	struct quadd_thread_data active_thread;
	atomic_t nr_active;

#ifdef QUADD_USE_CORRECT_SAMPLE_TS
	u64 prev_time;
	u64 current_time;
#endif
};

struct quadd_hrt_ctx {
	struct quadd_cpu_context * __percpu cpu_ctx;
	u64 sample_period;

	struct kprobe kp_in;
	/* struct kinstr ki_out; */

	struct quadd_ctx *quadd_ctx;

	int active;
	atomic64_t counter_samples;
	atomic_t nr_active_all_core;

	struct timer_list ma_timer;
	unsigned int ma_period;

	unsigned long vm_size_prev;
	unsigned long rss_size_prev;
};

#define QUADD_HRT_MIN_FREQ	110

#define QUADD_U32_MAX (~(__u32)0)

struct quadd_hrt_ctx;
struct quadd_record_data;
struct quadd_module_state;

struct quadd_hrt_ctx *quadd_hrt_init(struct quadd_ctx *ctx);
void quadd_hrt_deinit(void);

int quadd_hrt_start(void);
void quadd_hrt_stop(void);

void quadd_put_sample(struct quadd_record_data *data,
		      char *extra_data, unsigned int extra_length);

void quadd_hrt_get_state(struct quadd_module_state *state);
u64 quadd_get_time(void);

#endif	/* __KERNEL__ */

#endif	/* __QUADD_HRT_H */
