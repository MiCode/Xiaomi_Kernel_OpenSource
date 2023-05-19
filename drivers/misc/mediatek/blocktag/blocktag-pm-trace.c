// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "blocktag-pm-trace.h"

#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) { \
		seq_printf(evt, fmt, ##args); \
	} \
} while (0)


struct blk_pm_logs_s pm_traces;

static const char *event_name(int event)
{
	const char *ret;

	switch (event) {
	case PRE_RT_SUSPEND_START:
		ret = "pre_runtime_suspend_start";
		break;
	case PRE_RT_SUSPEND_END:
		ret = "pre_runtime_suspend_end";
		break;
	case POST_RT_SUSPEND_START:
		ret = "post_runtime_suspend_start";
		break;
	case POST_RT_SUSPEND_END:
		ret = "post_runtime_suspend_end";
		break;
	case PRE_RT_RESUME_START:
		ret = "pre_runtime_resume_start";
		break;
	case PRE_RT_RESUME_END:
		ret = "pre_runtime_resume_end";
		break;
	case POST_RT_RESUME_START:
		ret = "post_runtime_resume_start";
		break;
	case POST_RT_RESUME_END:
		ret = "post_runtime_resume_end";
		break;
	case SET_RT_ACTIVE_START:
		ret = "set_runtime_active_start";
		break;
	case SET_RT_ACTIVE_END:
		ret = "set_runtime_active_end";
		break;
	case BLK_QUEUE_ENTER:
		ret = "blk_queue_enter";
		break;
	case BLK_QUEUE_ENTER_SLEEP:
		ret = "blk_queue_enter_sleep";
		break;
	case BLK_QUEUE_ENTER_WAKEUP:
		ret = "blk_queue_enter_wakeup";
		break;
	default:
		ret = "";
	}

	return ret;
}

static const char *rpm_status_str(int rpm_status)
{
	const char *ret;

	switch (rpm_status) {
	case RPM_ACTIVE:
		ret = "RPM_ACTIVE";
		break;
	case RPM_RESUMING:
		ret = "RPM_RESUMING";
		break;
	case RPM_SUSPENDED:
		ret = "RPM_SUSPENED";
		break;
	case RPM_SUSPENDING:
		ret = "RPM_SUSPENDING";
		break;
	default:
		ret = "";
	}

	return ret;
}

static void btag_blk_pm_add_log(int rpm, int pm_only,
		int dying, int mq_freeze_depth, int ret, int event)
{
	uint64_t ns_time = sched_clock();

	spin_lock(&pm_traces.lock);
	pm_traces.tail = (pm_traces.tail + 1) % BLK_PM_MAX_LOG;
	if (pm_traces.tail == pm_traces.head)
		pm_traces.head = (pm_traces.head + 1) % BLK_PM_MAX_LOG;
	if (unlikely(pm_traces.head == -1))
		pm_traces.head++;

	pm_traces.trace[pm_traces.tail].ns_time = ns_time;
	pm_traces.trace[pm_traces.tail].event_type = event;
	pm_traces.trace[pm_traces.tail].rpm_status = rpm;
	pm_traces.trace[pm_traces.tail].pm_only = pm_only;
	pm_traces.trace[pm_traces.tail].mq_freeze_depth = mq_freeze_depth;
	pm_traces.trace[pm_traces.tail].ret = ret;
	pm_traces.trace[pm_traces.tail].pid = current->pid;
	pm_traces.trace[pm_traces.tail].dying = dying;
	spin_unlock(&pm_traces.lock);
}

void btag_blk_pre_runtime_suspend_start(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, PRE_RT_SUSPEND_START);
}

void btag_blk_pre_runtime_suspend_end(void *data,
		struct request_queue *q, int ret)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, ret, PRE_RT_SUSPEND_END);
}

void btag_blk_post_runtime_suspend_start(void *data,
		struct request_queue *q, int err)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, err, POST_RT_SUSPEND_START);
}

void btag_blk_post_runtime_suspend_end(void *data,
		struct request_queue *q, int err)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, err, POST_RT_SUSPEND_END);
}


void btag_blk_pre_runtime_resume_start(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, PRE_RT_RESUME_START);
}

void btag_blk_pre_runtime_resume_end(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, PRE_RT_RESUME_END);
}

void btag_blk_post_runtime_resume_start(void *data,
		struct request_queue *q, int err)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, err, POST_RT_RESUME_START);
}

void btag_blk_post_runtime_resume_end(void *data,
		struct request_queue *q, int err)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, err, POST_RT_RESUME_END);
}

void btag_blk_set_runtime_active_start(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, SET_RT_ACTIVE_START);
}

void btag_blk_set_runtime_active_end(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, SET_RT_ACTIVE_END);
}

void btag_blk_queue_enter_sleep(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, BLK_QUEUE_ENTER_SLEEP);
}

void btag_blk_queue_enter_wakeup(void *data,
		struct request_queue *q)
{
	int rpm_status = q->rpm_status;
	int pm_only = blk_queue_pm_only(q);
	int mq_freeze_depth = q->mq_freeze_depth;
	int dying = blk_queue_dying(q);

	btag_blk_pm_add_log(rpm_status, pm_only, dying,
			mq_freeze_depth, 0, BLK_QUEUE_ENTER_WAKEUP);
}

void mtk_btag_blk_pm_show(char **buff, unsigned long *size,
		struct seq_file *seq)
{
	struct blk_pm_log_s *tr;
	int idx;

	spin_lock(&pm_traces.lock);
	idx = pm_traces.tail;

	SPREAD_PRINTF(buff, size, seq,
		"time,pid,func,rpm_status,pm_only,freeze_depth,dying,ret/err\n");
	while (idx >= 0) {
		tr = &pm_traces.trace[idx];
		if (tr->event_type == PRE_RT_SUSPEND_END) {
			SPREAD_PRINTF(buff, size, seq,
				"%lld,%d,%s,%s,%d,%d,%d,%d\n",
				tr->ns_time,
				tr->pid,
				event_name(tr->event_type),
				rpm_status_str(tr->rpm_status),
				tr->pm_only,
				tr->mq_freeze_depth,
				tr->dying,
				tr->ret);
		} else if ((tr->event_type == POST_RT_SUSPEND_START) ||
			   (tr->event_type == POST_RT_SUSPEND_END) ||
			   (tr->event_type == POST_RT_RESUME_START) ||
			   (tr->event_type == POST_RT_RESUME_END)) {
			SPREAD_PRINTF(buff, size, seq,
				"%lld,%d,%s,%s,%d,%d,%d,%d\n",
				tr->ns_time,
				tr->pid,
				event_name(tr->event_type),
				rpm_status_str(tr->rpm_status),
				tr->pm_only,
				tr->mq_freeze_depth,
				tr->dying,
				tr->ret);
		} else {
			SPREAD_PRINTF(buff, size, seq,
				"%lld,%d,%s,%s,%d,%d,%d\n",
				tr->ns_time,
				tr->pid,
				event_name(tr->event_type),
				rpm_status_str(tr->rpm_status),
				tr->pm_only,
				tr->mq_freeze_depth,
				tr->dying);
		}
		if (idx == pm_traces.head)
			break;
		idx = idx ? idx - 1 : BLK_PM_MAX_LOG - 1;
	}
	spin_unlock(&pm_traces.lock);
}

void mtk_btag_blk_pm_init(void)
{
	pm_traces.head = -1;
	pm_traces.tail = -1;
	spin_lock_init(&pm_traces.lock);
}
