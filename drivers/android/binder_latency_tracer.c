// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <uapi/linux/android/binder.h>
#include "binder_internal.h"
#include "binder_trace.h"

/**
 * The reason setting the binder_txn_latency_threshold to 2 sec
 * is that most of timeout abort is greater or equal to 2 sec.
 * Making it configurable to let all users determine which
 * threshold is more suitable.
 */
static uint32_t binder_txn_latency_threshold = 2;
module_param_named(threshold, binder_txn_latency_threshold,
			uint, 0644);

/*
 * probe_binder_txn_latency_free - Output info of a delay transaction
 * @t:          pointer to the over-time transaction
 */
void probe_binder_txn_latency_free(void *ignore, struct binder_transaction *t,
					int from_proc, int from_thread,
					int to_proc, int to_thread)
{
	struct rtc_time tm;
	struct timespec64 *startime;
	struct timespec64 cur, sub_t;

	ktime_get_ts64(&cur);
	startime = &t->timestamp;
	sub_t = timespec64_sub(cur, *startime);

	/* if transaction time is over than binder_txn_latency_threshold (sec),
	 * show timeout warning log.
	 */
	if (sub_t.tv_sec < binder_txn_latency_threshold)
		return;

	rtc_time_to_tm(t->tv.tv_sec, &tm);

	pr_info_ratelimited("%d: from %d:%d to %d:%d",
			t->debug_id, from_proc, from_thread,
			to_proc, to_thread);

	pr_info_ratelimited(" total %u.%03ld s code %u start %lu.%03ld android %d-%02d-%02d %02d:%02d:%02d.%03lu\n",
			(unsigned int)sub_t.tv_sec,
			(sub_t.tv_nsec / NSEC_PER_MSEC),
			t->code,
			(unsigned long)startime->tv_sec,
			(startime->tv_nsec / NSEC_PER_MSEC),
			(tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)(t->tv.tv_usec / USEC_PER_MSEC));
}

static void probe_binder_txn_latency_alloc(void *ignore,
					struct binder_transaction *t)
{
	struct timespec64 now;

	ktime_get_ts64(&t->timestamp);
	ktime_get_real_ts64(&now);
	t->tv.tv_sec = now.tv_sec;
	t->tv.tv_sec -= (sys_tz.tz_minuteswest * 60);
	t->tv.tv_usec = now.tv_nsec/1000;
}

static void probe_binder_txn_latency_info(void *ignore, struct seq_file *m,
					struct binder_transaction *t)
{
	struct rtc_time tm;

	rtc_time_to_tm(t->tv.tv_sec, &tm);
	seq_printf(m,
		   " start %lu.%06lu android %d-%02d-%02d %02d:%02d:%02d.%03lu",
		   (unsigned long)t->timestamp.tv_sec,
		   (t->timestamp.tv_nsec / NSEC_PER_USEC),
		   (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
		   tm.tm_hour, tm.tm_min, tm.tm_sec,
		   (unsigned long)(t->tv.tv_usec / USEC_PER_MSEC));
}

static int __init init_binder_latency_tracer(void)
{
	register_trace_binder_txn_latency_free(
			probe_binder_txn_latency_free, NULL);
	register_trace_binder_txn_latency_alloc(
			probe_binder_txn_latency_alloc, NULL);
	register_trace_binder_txn_latency_info(
			probe_binder_txn_latency_info, NULL);

	return 0;
}

static void exit_binder_latency_tracer(void)
{
	unregister_trace_binder_txn_latency_free(
			probe_binder_txn_latency_free, NULL);
	unregister_trace_binder_txn_latency_alloc(
			probe_binder_txn_latency_alloc, NULL);
	unregister_trace_binder_txn_latency_info(
			probe_binder_txn_latency_info, NULL);
}

module_init(init_binder_latency_tracer);
module_exit(exit_binder_latency_tracer);

MODULE_LICENSE("GPL v2");
