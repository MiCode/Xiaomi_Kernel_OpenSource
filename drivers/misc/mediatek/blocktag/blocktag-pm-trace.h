/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _BLOCKTAG_BLK_PM_TRACE_H
#define _BLOCKTAG_BLK_PM_TRACE_H

#include <linux/types.h>
#include "mtk_blocktag.h"

#define BLK_PM_MAX_LOG (4096)

struct blk_pm_log_s {
	uint64_t ns_time;
	int event_type;		/* trace event type */
	int rpm_status;
	int pm_only;
	int mq_freeze_depth;
	int dying;
	int ret;
	short pid;			/* process id */
};

struct blk_pm_logs_s {
	long max;				/* max number of logs */
	long nr;				/* current number of logs */
	struct blk_pm_log_s trace[BLK_PM_MAX_LOG];
	int head;
	int tail;
	spinlock_t lock;
};

enum btag_blk_pm_event {
	PRE_RT_SUSPEND_START = 0,
	PRE_RT_SUSPEND_END,
	POST_RT_SUSPEND_START,
	POST_RT_SUSPEND_END,
	PRE_RT_RESUME_START,
	PRE_RT_RESUME_END,
	POST_RT_RESUME_START,
	POST_RT_RESUME_END,
	SET_RT_ACTIVE_START,
	SET_RT_ACTIVE_END,
	BLK_QUEUE_ENTER,
	BLK_QUEUE_ENTER_SLEEP,
	BLK_QUEUE_ENTER_WAKEUP,
	NR_BTAG_BLK_PM_EVENT,
};

void btag_blk_pre_runtime_suspend_start(void *data,
		struct request_queue *q);

void btag_blk_pre_runtime_suspend_end(void *data,
		struct request_queue *q, int ret);

void btag_blk_post_runtime_suspend_start(void *data,
		struct request_queue *q, int err);

void btag_blk_post_runtime_suspend_end(void *data,
		struct request_queue *q, int err);

void btag_blk_pre_runtime_resume_start(void *data,
		struct request_queue *q);

void btag_blk_pre_runtime_resume_end(void *data,
		struct request_queue *q);

void btag_blk_post_runtime_resume_start(void *data,
		struct request_queue *q, int err);

void btag_blk_post_runtime_resume_end(void *data,
		struct request_queue *q, int err);

void btag_blk_set_runtime_active_start(void *data,
		struct request_queue *q);

void btag_blk_set_runtime_active_end(void *data,
		struct request_queue *q);

void btag_blk_queue_enter(void *data,
		struct request_queue *q);

void btag_blk_queue_enter_sleep(void *data,
		struct request_queue *q);

void btag_blk_queue_enter_wakeup(void *data,
		struct request_queue *q);

void mtk_btag_blk_pm_show(char **buff, unsigned long *size,
		struct seq_file *seq);

void mtk_btag_blk_pm_init(void);

#endif /* _BLOCKTAG_BLK_PM_TRACE_H */
