/*
 * drivers/misc/tegra-profiler/hrt.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/ratelimit.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/err.h>

#include <asm/cputype.h>
#include <asm/irq_regs.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "hrt.h"
#include "comm.h"
#include "mmap.h"
#include "ma.h"
#include "power_clk.h"
#include "tegra.h"
#include "debug.h"

static struct quadd_hrt_ctx hrt;

static void
read_all_sources(struct pt_regs *regs, struct task_struct *task);

struct hrt_event_value {
	int event_id;
	u32 value;
};

static enum hrtimer_restart hrtimer_handler(struct hrtimer *hrtimer)
{
	struct pt_regs *regs;

	regs = get_irq_regs();

	if (!hrt.active)
		return HRTIMER_NORESTART;

	qm_debug_handler_sample(regs);

	if (regs)
		read_all_sources(regs, NULL);

	hrtimer_forward_now(hrtimer, ns_to_ktime(hrt.sample_period));
	qm_debug_timer_forward(regs, hrt.sample_period);

	return HRTIMER_RESTART;
}

static void start_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	u64 period = hrt.sample_period;

	hrtimer_start(&cpu_ctx->hrtimer, ns_to_ktime(period),
		      HRTIMER_MODE_REL_PINNED);
	qm_debug_timer_start(NULL, period);
}

static void cancel_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	hrtimer_cancel(&cpu_ctx->hrtimer);
	qm_debug_timer_cancel();
}

static void init_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	hrtimer_init(&cpu_ctx->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cpu_ctx->hrtimer.function = hrtimer_handler;
}

u64 quadd_get_time(void)
{
	struct timespec ts;

	do_posix_clock_monotonic_gettime(&ts);
	return timespec_to_ns(&ts);
}

static void put_header(void)
{
	int nr_events = 0, max_events = QUADD_MAX_COUNTERS;
	unsigned int events[QUADD_MAX_COUNTERS];
	struct quadd_record_data record;
	struct quadd_header_data *hdr = &record.hdr;
	struct quadd_parameters *param = &hrt.quadd_ctx->param;
	unsigned int extra = param->reserved[QUADD_PARAM_IDX_EXTRA];
	struct quadd_iovec vec;
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_event_source_interface *pmu = ctx->pmu;
	struct quadd_event_source_interface *pl310 = ctx->pl310;

	record.record_type = QUADD_RECORD_TYPE_HEADER;

	hdr->magic = QUADD_HEADER_MAGIC;
	hdr->version = QUADD_SAMPLES_VERSION;

	hdr->backtrace = param->backtrace;
	hdr->use_freq = param->use_freq;
	hdr->system_wide = param->system_wide;

	/* TODO: dynamically */
#ifdef QM_DEBUG_SAMPLES_ENABLE
	hdr->debug_samples = 1;
#else
	hdr->debug_samples = 0;
#endif

	hdr->freq = param->freq;
	hdr->ma_freq = param->ma_freq;
	hdr->power_rate_freq = param->power_rate_freq;

	hdr->power_rate = hdr->power_rate_freq > 0 ? 1 : 0;
	hdr->get_mmap = (extra & QUADD_PARAM_EXTRA_GET_MMAP) ? 1 : 0;

	hdr->reserved = 0;
	hdr->extra_length = 0;

	if (pmu)
		nr_events += pmu->get_current_events(events, max_events);

	if (pl310)
		nr_events += pl310->get_current_events(events + nr_events,
						       max_events - nr_events);

	hdr->nr_events = nr_events;

	vec.base = events;
	vec.len = nr_events * sizeof(events[0]);

	quadd_put_sample(&record, &vec, 1);
}

void quadd_put_sample(struct quadd_record_data *data,
		      struct quadd_iovec *vec, int vec_count)
{
	struct quadd_comm_data_interface *comm = hrt.quadd_ctx->comm;

	comm->put_sample(data, vec, vec_count);
	atomic64_inc(&hrt.counter_samples);
}

static int get_sample_data(struct quadd_sample_data *sample,
			   struct pt_regs *regs,
			   struct task_struct *task)
{
	unsigned int cpu, flags;
	struct quadd_ctx *quadd_ctx = hrt.quadd_ctx;

	cpu = quadd_get_processor_id(regs, &flags);
	sample->cpu = cpu;

	sample->lp_mode =
		(flags & QUADD_CPUMODE_TEGRA_POWER_CLUSTER_LP) ? 1 : 0;
	sample->thumb_mode = (flags & QUADD_CPUMODE_THUMB) ? 1 : 0;
	sample->user_mode = user_mode(regs) ? 1 : 0;

	sample->ip = instruction_pointer(regs);

	/* For security reasons, hide IPs from the kernel space. */
	if (!sample->user_mode && !quadd_ctx->collect_kernel_ips)
		sample->ip = 0;
	else
		sample->ip = instruction_pointer(regs);

	sample->time = quadd_get_time();
	sample->reserved = 0;
	sample->pid = task->pid;
	sample->in_interrupt = in_interrupt() ? 1 : 0;

	return 0;
}

static int read_source(struct quadd_event_source_interface *source,
		       struct pt_regs *regs,
		       struct hrt_event_value *events_vals,
		       int max_events)
{
	int nr_events, i;
	u32 prev_val, val, res_val;
	struct event_data events[QUADD_MAX_COUNTERS];

	if (!source)
		return 0;

	max_events = min_t(int, max_events, QUADD_MAX_COUNTERS);
	nr_events = source->read(events, max_events);

	for (i = 0; i < nr_events; i++) {
		struct event_data *s = &events[i];

		prev_val = s->prev_val;
		val = s->val;

		if (prev_val <= val)
			res_val = val - prev_val;
		else
			res_val = QUADD_U32_MAX - prev_val + val;

		if (s->event_source == QUADD_EVENT_SOURCE_PL310) {
			int nr_active = atomic_read(&hrt.nr_active_all_core);
			if (nr_active > 1)
				res_val /= nr_active;
		}

		events_vals[i].event_id = s->event_id;
		events_vals[i].value = res_val;
	}

	return nr_events;
}

static void
read_all_sources(struct pt_regs *regs, struct task_struct *task)
{
	u32 state;
	int i, vec_idx = 0, bt_size = 0;
	int nr_events = 0, nr_positive_events = 0;
	struct pt_regs *user_regs;
	struct quadd_iovec vec[3];
	struct hrt_event_value events[QUADD_MAX_COUNTERS];
	u32 events_extra[QUADD_MAX_COUNTERS];

	struct quadd_record_data record_data;
	struct quadd_sample_data *s = &record_data.sample;

	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_callchain *cc = &cpu_ctx->cc;

	if (!regs)
		return;

	if (atomic_read(&cpu_ctx->nr_active) == 0)
		return;

	if (!task) {
		pid_t pid;
		struct pid *pid_s;
		struct quadd_thread_data *t_data;

		t_data = &cpu_ctx->active_thread;
		pid = t_data->pid;

		rcu_read_lock();
		pid_s = find_vpid(pid);
		if (pid_s)
			task = pid_task(pid_s, PIDTYPE_PID);
		rcu_read_unlock();
		if (!task)
			return;
	}

	if (ctx->pmu && ctx->pmu_info.active)
		nr_events += read_source(ctx->pmu, regs,
					 events, QUADD_MAX_COUNTERS);

	if (ctx->pl310 && ctx->pl310_info.active)
		nr_events += read_source(ctx->pl310, regs,
					 events + nr_events,
					 QUADD_MAX_COUNTERS - nr_events);

	if (!nr_events)
		return;

	if (user_mode(regs))
		user_regs = regs;
	else
		user_regs = current_pt_regs();

	if (get_sample_data(s, regs, task))
		return;

	s->reserved = 0;

	if (ctx->param.backtrace) {
		bt_size = quadd_get_user_callchain(user_regs, cc, ctx);
		if (bt_size > 0) {
			vec[vec_idx].base = cc->ip;
			vec[vec_idx].len =
				bt_size * sizeof(cc->ip[0]);
			vec_idx++;
		}

		s->reserved |= cc->unw_method << QUADD_SAMPLE_UNW_METHOD_SHIFT;

		if (cc->unw_method == QUADD_UNW_METHOD_EHT)
			s->reserved |= cc->unw_rc << QUADD_SAMPLE_URC_SHIFT;
	}
	s->callchain_nr = bt_size;

	record_data.record_type = QUADD_RECORD_TYPE_SAMPLE;

	s->events_flags = 0;
	for (i = 0; i < nr_events; i++) {
		u32 value = events[i].value;
		if (value > 0) {
			s->events_flags |= 1 << i;
			events_extra[nr_positive_events++] = value;
		}
	}

	if (nr_positive_events == 0)
		return;

	vec[vec_idx].base = events_extra;
	vec[vec_idx].len = nr_positive_events * sizeof(events_extra[0]);
	vec_idx++;

	state = task->state;
	if (state) {
		s->state = 1;
		vec[vec_idx].base = &state;
		vec[vec_idx].len = sizeof(state);
		vec_idx++;
	} else {
		s->state = 0;
	}

	quadd_put_sample(&record_data, vec, vec_idx);
}

static inline int
is_profile_process(struct task_struct *task)
{
	int i;
	pid_t pid, profile_pid;
	struct quadd_ctx *ctx = hrt.quadd_ctx;

	if (!task)
		return 0;

	pid = task->tgid;

	for (i = 0; i < ctx->param.nr_pids; i++) {
		profile_pid = ctx->param.pids[i];
		if (profile_pid == pid)
			return 1;
	}
	return 0;
}

static int
add_active_thread(struct quadd_cpu_context *cpu_ctx, pid_t pid, pid_t tgid)
{
	struct quadd_thread_data *t_data = &cpu_ctx->active_thread;

	if (t_data->pid > 0 ||
		atomic_read(&cpu_ctx->nr_active) > 0) {
		pr_warn_once("Warning for thread: %d\n", (int)pid);
		return 0;
	}

	t_data->pid = pid;
	t_data->tgid = tgid;
	return 1;
}

static int remove_active_thread(struct quadd_cpu_context *cpu_ctx, pid_t pid)
{
	struct quadd_thread_data *t_data = &cpu_ctx->active_thread;

	if (t_data->pid < 0)
		return 0;

	if (t_data->pid == pid) {
		t_data->pid = -1;
		t_data->tgid = -1;
		return 1;
	}

	pr_warn_once("Warning for thread: %d\n", (int)pid);
	return 0;
}

void __quadd_task_sched_in(struct task_struct *prev,
			   struct task_struct *task)
{
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct event_data events[QUADD_MAX_COUNTERS];
	/* static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 2); */

	if (likely(!hrt.active))
		return;
/*
	if (__ratelimit(&ratelimit_state))
		pr_info("sch_in, cpu: %d, prev: %u (%u) \t--> curr: %u (%u)\n",
			smp_processor_id(), (unsigned int)prev->pid,
			(unsigned int)prev->tgid, (unsigned int)task->pid,
			(unsigned int)task->tgid);
*/

	if (is_profile_process(task)) {
		add_active_thread(cpu_ctx, task->pid, task->tgid);
		atomic_inc(&cpu_ctx->nr_active);

		if (atomic_read(&cpu_ctx->nr_active) == 1) {
			if (ctx->pmu)
				ctx->pmu->start();

			if (ctx->pl310)
				ctx->pl310->read(events, 1);

			start_hrtimer(cpu_ctx);
			atomic_inc(&hrt.nr_active_all_core);
		}
	}
}

void __quadd_task_sched_out(struct task_struct *prev,
			    struct task_struct *next)
{
	int n;
	struct pt_regs *user_regs;
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	/* static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 2); */

	if (likely(!hrt.active))
		return;
/*
	if (__ratelimit(&ratelimit_state))
		pr_info("sch_out: cpu: %d, prev: %u (%u) \t--> next: %u (%u)\n",
			smp_processor_id(), (unsigned int)prev->pid,
			(unsigned int)prev->tgid, (unsigned int)next->pid,
			(unsigned int)next->tgid);
*/

	if (is_profile_process(prev)) {
		user_regs = task_pt_regs(prev);
		if (user_regs)
			read_all_sources(user_regs, prev);

		n = remove_active_thread(cpu_ctx, prev->pid);
		atomic_sub(n, &cpu_ctx->nr_active);

		if (atomic_read(&cpu_ctx->nr_active) == 0) {
			cancel_hrtimer(cpu_ctx);
			atomic_dec(&hrt.nr_active_all_core);

			if (ctx->pmu)
				ctx->pmu->stop();
		}
	}
}

void __quadd_event_mmap(struct vm_area_struct *vma)
{
	struct quadd_parameters *param;

	if (likely(!hrt.active))
		return;

	if (!is_profile_process(current))
		return;

	param = &hrt.quadd_ctx->param;
	quadd_process_mmap(vma, param->pids[0]);
}

static void reset_cpu_ctx(void)
{
	int cpu_id;
	struct quadd_cpu_context *cpu_ctx;
	struct quadd_thread_data *t_data;

	for (cpu_id = 0; cpu_id < nr_cpu_ids; cpu_id++) {
		cpu_ctx = per_cpu_ptr(hrt.cpu_ctx, cpu_id);
		t_data = &cpu_ctx->active_thread;

		atomic_set(&cpu_ctx->nr_active, 0);

		t_data->pid = -1;
		t_data->tgid = -1;
	}
}

int quadd_hrt_start(void)
{
	int err;
	u64 period;
	long freq;
	unsigned int extra;
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_parameters *param = &ctx->param;

	freq = ctx->param.freq;
	freq = max_t(long, QUADD_HRT_MIN_FREQ, freq);
	period = NSEC_PER_SEC / freq;
	hrt.sample_period = period;

	if (ctx->param.ma_freq > 0)
		hrt.ma_period = MSEC_PER_SEC / ctx->param.ma_freq;
	else
		hrt.ma_period = 0;

	atomic64_set(&hrt.counter_samples, 0);

	reset_cpu_ctx();

	put_header();

	extra = param->reserved[QUADD_PARAM_IDX_EXTRA];

	if (extra & QUADD_PARAM_EXTRA_GET_MMAP) {
		err = quadd_get_current_mmap(param->pids[0]);
		if (err) {
			pr_err("error: quadd_get_current_mmap\n");
			return err;
		}
	}

	if (ctx->pl310)
		ctx->pl310->start();

	quadd_ma_start(&hrt);

	hrt.active = 1;

	pr_info("Start hrt: freq/period: %ld/%llu\n", freq, period);
	return 0;
}

void quadd_hrt_stop(void)
{
	struct quadd_ctx *ctx = hrt.quadd_ctx;

	pr_info("Stop hrt, number of samples: %llu\n",
		atomic64_read(&hrt.counter_samples));

	if (ctx->pl310)
		ctx->pl310->stop();

	quadd_ma_stop(&hrt);

	hrt.active = 0;

	atomic64_set(&hrt.counter_samples, 0);

	/* reset_cpu_ctx(); */
}

void quadd_hrt_deinit(void)
{
	if (hrt.active)
		quadd_hrt_stop();

	free_percpu(hrt.cpu_ctx);
}

void quadd_hrt_get_state(struct quadd_module_state *state)
{
	state->nr_all_samples = atomic64_read(&hrt.counter_samples);
	state->nr_skipped_samples = 0;
}

struct quadd_hrt_ctx *quadd_hrt_init(struct quadd_ctx *ctx)
{
	int cpu_id;
	u64 period;
	long freq;
	struct quadd_cpu_context *cpu_ctx;

	hrt.quadd_ctx = ctx;
	hrt.active = 0;

	freq = ctx->param.freq;
	freq = max_t(long, QUADD_HRT_MIN_FREQ, freq);
	period = NSEC_PER_SEC / freq;
	hrt.sample_period = period;

	if (ctx->param.ma_freq > 0)
		hrt.ma_period = MSEC_PER_SEC / ctx->param.ma_freq;
	else
		hrt.ma_period = 0;

	atomic64_set(&hrt.counter_samples, 0);

	hrt.cpu_ctx = alloc_percpu(struct quadd_cpu_context);
	if (!hrt.cpu_ctx)
		return ERR_PTR(-ENOMEM);

	for (cpu_id = 0; cpu_id < nr_cpu_ids; cpu_id++) {
		cpu_ctx = per_cpu_ptr(hrt.cpu_ctx, cpu_id);

		atomic_set(&cpu_ctx->nr_active, 0);

		cpu_ctx->active_thread.pid = -1;
		cpu_ctx->active_thread.tgid = -1;

		init_hrtimer(cpu_ctx);
	}

	return &hrt;
}
