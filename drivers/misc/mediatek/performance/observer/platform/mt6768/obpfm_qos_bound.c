/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#define pr_fmt(fmt) "pob_qos: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/trace_events.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kthread.h>

#include <trace/events/pob.h>

#include "helio-dvfsrc.h"

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#else
__weak int dram_steps_freq(unsigned int step) { return 0; }
#endif

#include "pob_int.h"
#include "obpfm_qos_bound.h"

static BLOCKING_NOTIFIER_HEAD(qos_bound_chain_head);

static DECLARE_WAIT_QUEUE_HEAD(qos_waitqueue);

unsigned short bound_seqnum;
unsigned short bound_idx;

//#define _MTK_OBPFMQOS_DO_CHECK_ 1
//#define _MTK_OBPFMQOS_DEBUG_ 1

static u64 checked_timestamp;
static u64 ms_interval = 2 * NSEC_PER_MSEC;
static DEFINE_SPINLOCK(check_lock);

static struct qos_bound bound;

static int sspm_lastidx = -1;

static int inited;

static HLIST_HEAD(bwq_list);

struct bwq {
	struct hlist_node hlist;
	struct work_struct sWork;
};

static struct workqueue_struct *_gpBWQNtfWQ;

#ifdef _MTK_OBPFMQOS_DEBUG_
static inline void _trace_pob_log(char *log)
{
	preempt_disable();
	trace_pob_log(log);
	preempt_enable();
}

#define TRACELOG_SIZE 512

void pob_wall_tracelog(u64 wallclock, s64 diff)
{
	char log[TRACELOG_SIZE];
	int tmplen;

	tmplen = snprintf(log, TRACELOG_SIZE, "%s %llu %llu",
				"wallclock",
				(unsigned long long) wallclock,
				(unsigned long long) diff);
	if (tmplen < 0 || tmplen >= sizeof(log))
		return;

	_trace_pob_log(log);
}

void pob_src_tracelog(char *file, int line, void *ptr)
{
	char log[TRACELOG_SIZE];
	int tmplen;

	tmplen = snprintf(log, TRACELOG_SIZE, "%s %d %p",
				file, line, ptr);
	if (tmplen < 0 || tmplen >= sizeof(log))
		return;

	_trace_pob_log(log);
}
#endif

static inline s64 do_check(u64 wallclock)
{
	s64 do_check = 0;
#ifdef _MTK_OBPFMQOS_DO_CHECK_
	unsigned long flags;
#endif

	/* check interval */
#ifdef _MTK_OBPFMQOS_DO_CHECK_
	spin_lock_irqsave(&check_lock, flags);
#endif

	do_check = (s64)(wallclock - checked_timestamp);
	if (do_check >= (s64)ms_interval)
		checked_timestamp = wallclock;
	else
		do_check = 0;

#ifdef _MTK_OBPFMQOS_DO_CHECK_
	spin_unlock_irqrestore(&check_lock, flags);
#endif

	return do_check;
}

static int _get_qos_bound_bw_threshold(int state, int val)
{
	if (state == QOS_BOUND_BW_FULL)
		return val * QOS_BOUND_BW_FULL_PCT / 100;
	else if (state == QOS_BOUND_BW_CONGESTIVE)
		return val * QOS_BOUND_BW_CONGESTIVE_PCT / 100;

	return 0;
}

static int _pob_qos_tracker_set_event(struct qos_bound_stat *stat)
{
	int isfull = 0;

	int val = dram_steps_freq(0) * 4;

	if (stat->emibw_mon[QOS_EMIBM_TOTAL] >=
			_get_qos_bound_bw_threshold(QOS_BOUND_BW_FULL, val)) {
		stat->event = QOS_BOUND_BW_FULL;
		isfull = 1;
	} else if (stat->emibw_mon[QOS_EMIBM_TOTAL] >=
		_get_qos_bound_bw_threshold(QOS_BOUND_BW_CONGESTIVE, val))
		stat->event = QOS_BOUND_BW_CONGESTIVE;
	else
		stat->event = QOS_BOUND_BW_FREE;

	return isfull;
}

static int _pob_qos_tracker_set_stat(struct qos_bound_stat *stat,
					int sramidx,
					u64 wallclock)
{
	int isfull = 0;
	int bw_total;

	bw_total = qos_sram_read(QOS_TOTAL_BW_BUF(sramidx));

	stat->emibw_mon[QOS_EMIBM_TOTAL] = 0x0000FFFF & bw_total;
	stat->num = (0xFF000000 & bw_total) >> 24;

	stat->wallclock = wallclock;

	if (_pob_qos_tracker_set_event(stat))
		isfull = 1;

	return isfull;
}

static void mtk_qb_wq_cb(struct work_struct *psWork)
{
	unsigned long flags;
	struct bwq *vbwq;

	vbwq = POB_CONTAINER_OF(psWork, struct bwq, sWork);

	spin_lock_irqsave(&check_lock, flags);
	hlist_add_head(&(vbwq->hlist), &bwq_list);
	spin_unlock_irqrestore(&check_lock, flags);

	qos_notifier_call_chain(bound.state, &bound);
}

static void _pob_qos_tracker(u64 wallclock, s64 diff)
{
	int i;
	int isfull = 0;
	int vIdx;
	int vLastIdx;
	u64 vDiff;
	int bi;

	vLastIdx = sspm_lastidx;

	if (vLastIdx == -1) {
		vLastIdx = qos_sram_read(QOS_TOTAL_BW_BUF_LAST);
		if (vLastIdx >= QOS_TOTAL_BW_BUF_SIZE || vLastIdx < 0)
			goto final;
		sspm_lastidx = vLastIdx;
	}

	vDiff = div_u64(((u64) diff), NSEC_PER_MSEC);

	if (vDiff > QOS_TOTAL_BW_BUF_SIZE - 2)
		vDiff = QOS_TOTAL_BW_BUF_SIZE - 2;

	vIdx = qos_sram_read(QOS_TOTAL_BW_BUF_LAST);
	if (vIdx >= QOS_TOTAL_BW_BUF_SIZE || vIdx < 0)
		goto final;

	if (vDiff == QOS_TOTAL_BW_BUF_SIZE - 2)
		vLastIdx = (vIdx + 3) % QOS_TOTAL_BW_BUF_SIZE;
	else
		vLastIdx = (vLastIdx + 1) % QOS_TOTAL_BW_BUF_SIZE;

	if (vLastIdx > vIdx) {
		for (i = 0; vLastIdx + i < QOS_TOTAL_BW_BUF_SIZE; i++) {
			bi = bound_idx % QOS_BOUND_BUF_SIZE;

			if (_pob_qos_tracker_set_stat(&(bound.stats[bi]),
					vLastIdx + i, wallclock))
				isfull = 1;

			bound.idx = bi;
			bound_idx++;
		}

		vLastIdx = 0;
	}

	for (i = 0; vLastIdx + i <= vIdx; i++) {
		bi = bound_idx % QOS_BOUND_BUF_SIZE;

		if (_pob_qos_tracker_set_stat(&(bound.stats[bi]),
						vLastIdx + i, wallclock))
			isfull = 1;

		bound.idx = bi;
		bound_idx++;
	}

	sspm_lastidx = vIdx;

	if (isfull) {
		unsigned long flags;
		struct bwq *vbwq = NULL;
		struct bwq *iter;
		struct hlist_node *tmp;

		bound.state = QOS_BOUND_BW_FULL;

		spin_lock_irqsave(&check_lock, flags);
		hlist_for_each_entry_safe(iter, tmp, &bwq_list, hlist) {
			hlist_del(&iter->hlist);
			vbwq = iter;
		}
		spin_unlock_irqrestore(&check_lock, flags);

		if (vbwq) {
			INIT_WORK(&vbwq->sWork, mtk_qb_wq_cb);
			queue_work(_gpBWQNtfWQ, &vbwq->sWork);
		}
	}

final:
	return;
}

void pob_qos_tracker(u64 wallclock)
{
	s64 diff;

	diff = do_check(wallclock);

#ifdef _MTK_OBPFMQOS_DEBUG_
	pob_wall_tracelog(wallclock, diff);
#endif

	if (!inited || !diff)
		return;

	_pob_qos_tracker(wallclock, diff);
}

struct qos_bound *get_qos_bound(void)
{
	return &bound;
}

unsigned short get_qos_bound_idx(void)
{
	return bound.idx;
}

int get_qos_bound_bw_threshold(int state)
{
	int val = dram_steps_freq(0) * 4;

	if (state == QOS_BOUND_BW_FULL)
		return val * QOS_BOUND_BW_FULL_PCT / 100;
	else if (state == QOS_BOUND_BW_CONGESTIVE)
		return val * QOS_BOUND_BW_CONGESTIVE_PCT / 100;

	return 0;
}

int register_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&qos_bound_chain_head, nb);
}

int unregister_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&qos_bound_chain_head, nb);
}

int qos_notifier_call_chain(unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;

	ret = blocking_notifier_call_chain(&qos_bound_chain_head, val, v);

	return notifier_to_errno(ret);
}

void qos_bound_init(void)
{
	struct bwq *vbwq[3];
	int i;
	unsigned long flags;

	_gpBWQNtfWQ = create_singlethread_workqueue("bwq_ntf_wq");
	if (_gpBWQNtfWQ == NULL)
		return;

	for (i = 0; i < 3; i++)
		vbwq[i] = pob_alloc_atomic(sizeof(struct bwq));

	spin_lock_irqsave(&check_lock, flags);
	for (i = 0; i < 3; i++)
		hlist_add_head(&(vbwq[i]->hlist), &bwq_list);
	spin_unlock_irqrestore(&check_lock, flags);

	inited = 1;
}

