// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Chiayu Ku <chiayu.ku@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */
#include <asm/div64.h>
#include <linux/blk_types.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/sched/stat.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#include <trace/events/block.h>
#include "mtk_blocktag.h"
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define TIME_5S 5000000000
#define TAG "BLOCKTAG"
#define EARASYS_MAX_SIZE 27
struct _EARA_SYS_PACKAGE {
	union {
		__s32 cmd;
		__s32 data[EARASYS_MAX_SIZE];
	};
};
#define EARA_GETINDEX                _IOW('g', 1, struct _EARA_SYS_PACKAGE)
#define EARA_COLLECT                 _IOW('g', 2, struct _EARA_SYS_PACKAGE)
struct rs_sys_data {
	int io_wl;
	int io_top;
	int io_reqc_r;
	int io_reqc_w;
	int io_q_dept;
};
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
static DEFINE_MUTEX(rs_index_mutex);
static DEFINE_SPINLOCK(rs_index_lock);
static int is_active;
static unsigned long long last_access_ts;
static unsigned long long prev_ts;
static struct list_head io_stat_list;
void (*rsi_getindex_fp)(__s32 *data, __s32 input_size);
void (*rsi_switch_collect_fp)(__s32 cmd);
static void wq_func(struct work_struct *data);
static DECLARE_WORK(rs_work, (void *) wq_func);
static void rsi_switch_collect(int cmd);
static void rs_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&rs_index_mutex));
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

	spin_lock_irqsave(&rs_index_lock, flags);
	last = last_access_ts;
	spin_unlock_irqrestore(&rs_index_lock, flags);
	if (cur_ts < last)
		return;
	diff = cur_ts - last;
	if (diff <= TIME_5S)
		return;
	rsi_switch_collect(0);
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
	struct mtk_btag_mictx_iostat_struct iostat = {0};

	if (mtk_btag_mictx_get_data(&iostat))
		mtk_btag_mictx_enable(1);
	stat->io_wl = iostat.wl;
	stat->io_top = iostat.top;
	stat->io_reqc_r = iostat.reqcnt_r;
	stat->io_reqc_w = iostat.reqcnt_w;
	stat->io_q_dept = iostat.q_depth;
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

	spin_lock_irqsave(&rs_index_lock, flags);
	if (list_empty(&(io_stat_list))) {
		spin_unlock_irqrestore(&rs_index_lock, flags);
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
	spin_unlock_irqrestore(&rs_index_lock, flags);
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
	spin_lock_irqsave(&rs_index_lock, flags);
	last_access_ts = cur_ts;
	if (is_active == RS_STATE_ACTIVE) {
		is_active = RS_STATE_READY;
		ret = RS_RET_INVALID;
	}
	spin_unlock_irqrestore(&rs_index_lock, flags);
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
	spin_lock_irqsave(&rs_index_lock, flags);
	if (is_active != RS_STATE_INACTIVE) {
		spin_unlock_irqrestore(&rs_index_lock, flags);
		return;
	}
	is_active = RS_STATE_ACTIVE;
	spin_unlock_irqrestore(&rs_index_lock, flags);
	/* register_trace_perf_index_l(rs_update_io_stat, NULL); */
	rsi_get_data(&sysdata);
}
static void rs_stop_collect(void)
{
	unsigned long flags;

	rs_lockprove(__func__);
	spin_lock_irqsave(&rs_index_lock, flags);
	if (is_active == RS_STATE_INACTIVE) {
		spin_unlock_irqrestore(&rs_index_lock, flags);
		return;
	}
	is_active = RS_STATE_INACTIVE;
	rs_reset_io_list_locked();
	spin_unlock_irqrestore(&rs_index_lock, flags);
	/* unregister_trace_perf_index_l(rs_update_io_stat, NULL); */
	prev_ts = 0;
}
static void rsi_switch_collect(int cmd)
{
	mutex_lock(&rs_index_mutex);
	if (cmd)
		rs_start_collect();
	else
		rs_stop_collect();
	mutex_unlock(&rs_index_mutex);
}
void rsi_trans_index(__s32 *data, __s32 input_size)
{
	struct rs_sys_data sysdata = {0};
	int limit_size;
	int ret;

	mutex_lock(&rs_index_mutex);
	ret = rsi_get_data(&sysdata);
	mutex_unlock(&rs_index_mutex);
	if (ret == RS_RET_INVALID)
		return;
	limit_size = MIN(input_size, sizeof(struct rs_sys_data));
	memcpy(data, &sysdata, limit_size);
}
static int earasys_show(struct seq_file *m, void *v)
{
	return 0;
}
static int earasys_open(struct inode *inode, struct file *file)
{
	return single_open(file, earasys_show, inode->i_private);
}
static unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(VERIFY_READ, pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}
static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(VERIFY_WRITE, pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}
static long earasys_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _EARA_SYS_PACKAGE *msgKM = NULL;
	struct _EARA_SYS_PACKAGE *msgUM = (struct _EARA_SYS_PACKAGE *)arg;
	struct _EARA_SYS_PACKAGE smsgKM = {0};

	msgKM = &smsgKM;
	switch (cmd) {
	case EARA_GETINDEX:
		if (rsi_getindex_fp)
			rsi_getindex_fp(smsgKM.data, sizeof(struct _EARA_SYS_PACKAGE));
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _EARA_SYS_PACKAGE));
		break;
	case EARA_COLLECT:
		if (perfctl_copy_from_user(msgKM, msgUM,
					sizeof(struct _EARA_SYS_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		if (rsi_switch_collect_fp)
			rsi_switch_collect_fp(msgKM->cmd);
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}
ret_ioctl:
	return ret;
}
static const struct file_operations earasys_fops = {
	.unlocked_ioctl = earasys_ioctl,
	.compat_ioctl = earasys_ioctl,
	.open = earasys_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
void rs_index_init(struct mtk_blocktag *btag,
	struct proc_dir_entry *parent)
{
	int ret = 0;

	INIT_LIST_HEAD(&(io_stat_list));
	rsi_getindex_fp = rsi_trans_index;
	rsi_switch_collect_fp = rsi_switch_collect;
	btag->dentry.dindex = proc_create("eara_io",
		0664, parent, &earasys_fops);
	if (IS_ERR(btag->dentry.dindex)) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret);
		ret = -ENOMEM;
	}
}
