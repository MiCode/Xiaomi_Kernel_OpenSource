// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Chiayu Ku <chiayu.ku@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/sched/stat.h>
#include <linux/sched/clock.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <asm/div64.h>
#include <linux/sched/task.h>
#include <trace/events/mtk_events.h>
#include <trace/events/block.h>
#include <linux/blk_types.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <mt-plat/mtk_blocktag.h>

#include "rs_index.h"
#include "rs_trace.h"

#define TIME_5S 5000000000
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

enum  {
	RS_RET_SUCCESS  = 0,
	RS_RET_ERROR  = -1,
	RS_RET_INVALID  = -2,
};

enum  {
	RS_STATE_INACTIVE  = 0,
	RS_STATE_ACTIVE  = 1,
	RS_STATE_READY  = 2,
};

struct rs_io_stat {
	unsigned long long dur_ns;

	int io_wl;
	int io_top;

	int io_reqc_r;
	int io_reqc_w;

	int io_q_dept;

	struct list_head entry;
};

static DEFINE_MUTEX(rs_loading_mutex);
static DEFINE_SPINLOCK(rs_loading_slock);

static int mask;
static int is_active;

static void wq_func(struct work_struct *data);
static DECLARE_WORK(rs_work, (void *) wq_func);
static unsigned long long last_access_ts;

static struct list_head io_stat_list;

static unsigned long long prev_ts;

#define RSL_TAG		"RSL:"
#define rsi_systrace(mask, pid, val, fmt...) \
	do { \
		if (mask) \
			__rs_systrace_c_uint64(pid, val, fmt); \
	} while (0)

#define rsi_systrace_log(pid, val, fmt...) \
	rsi_systrace(mask, pid, val, RSL_TAG fmt)
#define rsi_systrace_def(pid, val, fmt...) \
	rsi_systrace(1, pid, val, RSL_TAG fmt)

static void rs_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&rs_loading_mutex));
}

static u64 rs_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

static void wq_func(struct work_struct *data)
{
	unsigned long long cur_ts = rs_get_time();
	unsigned long long diff, last;
	unsigned long flags;

	spin_lock_irqsave(&rs_loading_slock, flags);
	last = last_access_ts;
	spin_unlock_irqrestore(&rs_loading_slock, flags);

	if (cur_ts < last)
		return;

	diff = cur_ts - last;

	if (diff <= TIME_5S)
		return;

	rsi_switch_collect(0);

}

static void rs_foolproof_slocked(void)
{
	unsigned long long cur_ts = rs_get_time();
	unsigned long long diff;

	if (cur_ts < last_access_ts)
		return;

	diff = cur_ts - last_access_ts;

	if (diff <= TIME_5S)
		return;

	schedule_work(&rs_work);
}

static void rs_update_io_stat(void *data, long free_mem, long avail_mem,
		int io_wl, int io_req_r, int io_all_r, int io_reqsz_r, int io_reqc_r,
		int io_req_w, int io_all_w, int io_reqsz_w, int io_reqc_w,
		int io_dur, int io_q_dept, int io_top, int *stall)
{
	struct rs_io_stat *obj;
	unsigned long flags;

	spin_lock_irqsave(&rs_loading_slock, flags);
	if (is_active == RS_STATE_INACTIVE) {
		spin_unlock_irqrestore(&rs_loading_slock, flags);
		return;
	}
	spin_unlock_irqrestore(&rs_loading_slock, flags);

	obj = kzalloc(sizeof(struct rs_io_stat), GFP_ATOMIC);
	if (!obj)
		return;

	INIT_LIST_HEAD(&obj->entry);
	obj->dur_ns = io_dur;
	obj->io_wl = io_wl;
	obj->io_reqc_r = io_reqc_r;
	obj->io_reqc_w = io_reqc_w;
	obj->io_q_dept = io_q_dept;
	obj->io_top = io_top;

	spin_lock_irqsave(&rs_loading_slock, flags);
	list_add(&obj->entry, &io_stat_list);
	rs_foolproof_slocked();
	spin_unlock_irqrestore(&rs_loading_slock, flags);
}

static void rs_reset_io_list_locked(void)
{
	struct rs_io_stat *pos, *next;

	list_for_each_entry_safe(pos, next, &(io_stat_list), entry) {
		list_del(&pos->entry);
		kfree(pos);
	}
}

static void rs_get_io_stat_oneshot(struct rs_sys_data *stat)
{
#ifdef CONFIG_MTK_BLOCK_TAG
	struct mtk_btag_mictx_iostat_struct iostat = {0};

	if (mtk_btag_mictx_get_data(&iostat))
		mtk_btag_mictx_enable(1);

	stat->io_wl = iostat.wl;
	stat->io_top = iostat.top;
	stat->io_reqc_r = iostat.reqcnt_r;
	stat->io_reqc_w = iostat.reqcnt_w;
	stat->io_q_dept = iostat.q_depth;
#endif
}

static int rs_get_io_stat(struct rs_sys_data *stat)
{
	int ret = 0;
	struct rs_io_stat *pos, *next;
	unsigned long flags;
	unsigned long long io_dur = 0;
	long long io_wl = 0;
	long long io_top = 0;
	long long io_reqc_r = 0;
	long long io_reqc_w = 0;
	long long io_q_dept = 0;
	int cnt = 0;

	spin_lock_irqsave(&rs_loading_slock, flags);

	if (list_empty(&(io_stat_list))) {
		spin_unlock_irqrestore(&rs_loading_slock, flags);
		rs_get_io_stat_oneshot(stat);
		ret = -1;
		goto EXIT;
	}

	list_for_each_entry_safe(pos, next, &(io_stat_list), entry) {
		io_dur += pos->dur_ns;
		io_wl += pos->io_wl;
		io_top += pos->io_top;
		io_reqc_r += pos->io_reqc_r;
		io_reqc_w += pos->io_reqc_w;
		io_q_dept += pos->io_q_dept;
		cnt++;
	}

	rs_reset_io_list_locked();

	spin_unlock_irqrestore(&rs_loading_slock, flags);

	stat->io_wl = div64_u64(io_wl, cnt);
	stat->io_top = div64_u64(io_top, cnt);
	stat->io_reqc_r = div64_u64(io_reqc_r, cnt);
	stat->io_reqc_w = div64_u64(io_reqc_w, cnt);
	stat->io_q_dept = div64_u64(io_q_dept, cnt);

EXIT:
	return ret;
}

int rsi_get_data(struct rs_sys_data *sysdata)
{
	int ret = RS_RET_SUCCESS;
	int ret_io;
	u64 cur_ts;
	long long dur = 0;
	unsigned long flags;

	rs_lockprove(__func__);

	cur_ts = rs_get_time();
	if (prev_ts)
		dur = cur_ts - prev_ts;
	prev_ts = cur_ts;

	spin_lock_irqsave(&rs_loading_slock, flags);
	last_access_ts = cur_ts;
	if (is_active == RS_STATE_ACTIVE) {
		is_active = RS_STATE_READY;
		ret = RS_RET_INVALID;
	}
	spin_unlock_irqrestore(&rs_loading_slock, flags);

	ret_io = rs_get_io_stat(sysdata);

	if (ret_io)
		ret = RS_RET_ERROR;

	return ret;
}

static void rs_start_collect(void)
{
	struct rs_sys_data sysdata = {0};
	unsigned long flags;

	rs_lockprove(__func__);

	spin_lock_irqsave(&rs_loading_slock, flags);

	if (is_active != RS_STATE_INACTIVE) {
		spin_unlock_irqrestore(&rs_loading_slock, flags);
		return;
	}

	is_active = RS_STATE_ACTIVE;

	spin_unlock_irqrestore(&rs_loading_slock, flags);

	register_trace_perf_index_l(rs_update_io_stat, NULL);

	rsi_get_data(&sysdata);
}

static void rs_stop_collect(void)
{
	unsigned long flags;

	rs_lockprove(__func__);

	spin_lock_irqsave(&rs_loading_slock, flags);

	if (is_active == RS_STATE_INACTIVE) {
		spin_unlock_irqrestore(&rs_loading_slock, flags);
		return;
	}
	is_active = RS_STATE_INACTIVE;

	rs_reset_io_list_locked();
	spin_unlock_irqrestore(&rs_loading_slock, flags);

	unregister_trace_perf_index_l(rs_update_io_stat, NULL);

	prev_ts = 0;
}

void rsi_switch_collect(int cmd)
{
	mutex_lock(&rs_loading_mutex);

	if (cmd)
		rs_start_collect();
	else
		rs_stop_collect();

	mutex_unlock(&rs_loading_mutex);
}

void rsi_trans_index(__s32 *data, __s32 input_size)
{
	struct rs_sys_data sysdata = {0};
	int limit_size;
	int ret;

	mutex_lock(&rs_loading_mutex);
	ret = rsi_get_data(&sysdata);
	mutex_unlock(&rs_loading_mutex);

	if (ret == RS_RET_INVALID)
		return;

	limit_size = MIN(input_size, sizeof(struct rs_sys_data));

	memcpy(data, &sysdata, limit_size);
}

#define RS_DEBUGFS_ENTRY(name) \
static int rs_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, rs_##name##_show, i->i_private); \
} \
\
static const struct file_operations rs_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = rs_##name##_open, \
	.read = seq_read, \
	.write = rs_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static int rs_mask_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "%d\n", mask);

	return 0;
}

static ssize_t rs_mask_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	uint32_t val;
	int ret;

	ret = kstrtou32_from_user(ubuf, cnt, 16, &val);
	if (ret)
		return ret;

	mask = val;

	return cnt;
}

RS_DEBUGFS_ENTRY(mask);

int __init rs_index_init(void)
{
	INIT_LIST_HEAD(&(io_stat_list));

	rsi_getindex_fp = rsi_trans_index;
	rsi_switch_collect_fp = rsi_switch_collect;

	return 0;
}

void rs_index_exit(void)
{
}

MODULE_AUTHOR("Chiayu Gu <chiayu.gu@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek Performance Index Collector");
MODULE_LICENSE("GPL v2");

