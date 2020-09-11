/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/average.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <asm/div64.h>

#include <linux/mutex.h>
#include <linux/rbtree.h>

#include <mt-plat/mtk_perfobserver.h>

#include "rs_base.h"
#include "rs_log.h"
#include "rs_trace.h"

//#define _DEBUG_BQD_ 1

#define RSP_DEBUGFS_ENTRY(name) \
static int rsp_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, rsp_##name##_show, i->i_private); \
} \
\
static const struct file_operations rsp_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = rsp_##name##_open, \
	.read = seq_read, \
	.write = rsp_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#ifndef _DEBUG_BQD_

#define RSP_SYSTRACE_LIST(macro) \
	macro(MANDATORY, 0), \
	macro(FPSGO, 1), \
	macro(MAX, 2), \

#else

#define RSP_SYSTRACE_LIST(macro) \
	macro(MANDATORY, 0), \
	macro(FPSGO, 1), \
	macro(BQD, 1), \
	macro(MAX, 2), \

#endif

#define RSP_GENERATE_ENUM(name, shft) RSP_DEBUG_##name = 1U << shft
enum {
	RSP_SYSTRACE_LIST(RSP_GENERATE_ENUM)
};

#define RSP_GENERATE_STRING(name, unused) #name
static const char * const mask_string[] = {
	RSP_SYSTRACE_LIST(RSP_GENERATE_STRING)
};

#define rsp_systrace_c(mask, pid, val, fmt...) \
	do { \
		if (rsp_systrace_mask & mask) \
			__rs_systrace_c(pid, val, fmt); \
	} while (0)

#define rsp_systrace_c_uint64(mask, pid, val, fmt...) \
	do { \
		if (rsp_systrace_mask & mask) \
			__rs_systrace_c_uint64(pid, val, fmt); \
	} while (0)

#define rsp_systrace_b(mask, tgid, fmt, ...) \
	do { \
		if (rsp_systrace_mask & mask) \
			__rs_systrace_b(tgid, fmt); \
	} while (0)

#define rsp_systrace_e(mask) \
	do { \
		if (rsp_systrace_mask & mask) \
			__rs_systrace_e(); \
	} while (0)

#define rsp_systrace_c_log(pid, val, fmt...) \
	rsp_systrace_c(RSP_DEBUG_MANDATORY, pid, val, fmt)

#define rsp_systrace_c_uint64_log(pid, val, fmt...) \
	rsp_systrace_c_uint64(RSP_DEBUG_MANDATORY, pid, val, fmt)

static uint32_t rsp_systrace_mask;

static int rsp_systrace_mask_show(struct seq_file *m, void *unused)
{
	int i;

	seq_puts(m, " Current enabled systrace:\n");
	for (i = 0; (1U << i) < RSP_DEBUG_MAX; i++)
		seq_printf(m, "  %-*s ... %s\n", 12, mask_string[i],
			   rsp_systrace_mask & (1U << i) ?
			     "On" : "Off");

	return 0;
}

static ssize_t rsp_systrace_mask_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	uint32_t val;
	int ret;

	ret = kstrtou32_from_user(ubuf, cnt, 16, &val);
	if (ret)
		return ret;

	val = val & (RSP_DEBUG_MAX - 1U);

	rsp_systrace_mask = val;

	return cnt;
}

RSP_DEBUGFS_ENTRY(systrace_mask);

#ifdef _DEBUG_BQD_
struct FSTB_FRAME_INFO {
	struct hlist_node hlist;

	int connected_api;
	unsigned long long bufid;

	int ged_acquirecount;
	int ged_queuecount;

	int ged_connectapi;

	int queuecount;
};

static HLIST_HEAD(rs_frame_infos);

static DEFINE_MUTEX(fstb_lock);

void rs_pob_bqd_queue_update(unsigned long long bufferid, int connectapi,
					unsigned long long cameraid)
{
	struct FSTB_FRAME_INFO *iter;

	char buff[32];

	snprintf(buff, 32, "QB GR%d-%llu", connectapi, bufferid);
	rsp_systrace_c(RSP_DEBUG_BQD, 0 - connectapi, 1, buff);

	snprintf(buff, 32, "QB CI%d-%llu", connectapi, bufferid);
	rsp_systrace_c(RSP_DEBUG_BQD, 0 - connectapi, cameraid, buff);

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &rs_frame_infos, hlist) {
		if (iter->bufid == bufferid)
			break;
	}

	if (iter == NULL) {
		struct FSTB_FRAME_INFO *new_frame_info;

		new_frame_info = vmalloc(sizeof(*new_frame_info));
		if (new_frame_info == NULL) {
			mutex_unlock(&fstb_lock);
			return;
		}
		new_frame_info->bufid = bufferid;
		new_frame_info->connected_api = 0;
		new_frame_info->queuecount = 0;

		new_frame_info->ged_acquirecount = 0;
		new_frame_info->ged_queuecount = 1;

		iter = new_frame_info;
		hlist_add_head(&iter->hlist, &rs_frame_infos);
	} else
		iter->ged_queuecount++;

	iter->ged_connectapi = connectapi;

	mutex_unlock(&fstb_lock);
}

void rs_pob_bqd_acquire_update(unsigned long long bufferid, int connectapi)
{
	struct FSTB_FRAME_INFO *iter;

	char buff[32];

	snprintf(buff, 32, "AB GR%d-%llu", connectapi, bufferid);
	rsp_systrace_c(RSP_DEBUG_BQD, 0 - connectapi, 1, buff);

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &rs_frame_infos, hlist) {
		if (iter->bufid == bufferid)
			break;
	}

	if (iter)
		iter->ged_acquirecount++;

	mutex_unlock(&fstb_lock);
}

static int rs_pob_bqd_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct pob_bqd_info *pbi = data;

	switch (val) {
	case POB_BQD_QUEUE:
		rs_pob_bqd_queue_update(pbi->bqid, pbi->connectapi,
					pbi->cameraid);
		break;
	case POB_BQD_ACQUIRE:
		rs_pob_bqd_acquire_update(pbi->bqid, pbi->connectapi);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rs_pob_bqd_notifier = {
	.notifier_call = rs_pob_bqd_cb,
};
#endif

struct rs_qtsk_ctxt {
	struct rb_node tsk_rbnode;

	int tskid;

	int cpu_rescuing;
};

static struct rb_root qtsk_rbtree;
static DEFINE_MUTEX(qtsk_ctxts_mlock);
static int cpu_rescuing_cnt;

int _gMaxQWCPUTime;
int _gMaxQWGPUTime;

inline int rs_qtsk_ctxts_lock(void)
{
	mutex_lock(&qtsk_ctxts_mlock);
	return 0;
}

inline int rs_qtsk_ctxts_unlock(void)
{
	mutex_unlock(&qtsk_ctxts_mlock);
	return 0;
}

int rs_qtsk_search_and_add(int tskid, int forceadd, struct rs_qtsk_ctxt **qtsk)
{
	struct rb_node **p = &qtsk_rbtree.rb_node;
	struct rb_node *parent = NULL;
	struct rs_qtsk_ctxt *tmp = NULL;

	if (!qtsk)
		return -EINVAL;

	*qtsk = NULL;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct rs_qtsk_ctxt, tsk_rbnode);

		if (tskid < tmp->tskid)
			p = &(*p)->rb_left;
		else if (tskid > tmp->tskid)
			p = &(*p)->rb_right;
		else
			break;
		tmp = NULL;
	}

	if (tmp) {
		*qtsk = tmp;
		return 0;
	}

	if (!forceadd)
		return 0;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	tmp->tskid = tskid;

	/* Add new node and rebalance tree. */
	rb_link_node(&tmp->tsk_rbnode, parent, p);
	rb_insert_color(&tmp->tsk_rbnode, &qtsk_rbtree);

	*qtsk = tmp;

	return 0;
}

int rs_qtsk_del(int tskid)
{
	struct rs_qtsk_ctxt *data = NULL;

	rs_qtsk_search_and_add(tskid, 0, &data);

	if (!data)
		return 0;

	rb_erase(&data->tsk_rbnode, &qtsk_rbtree);

	kfree(data);

	return 0;
}

int rs_qtsk_del_byctxt(struct rs_qtsk_ctxt *data)
{
	if (!data)
		return 0;

	rb_erase(&data->tsk_rbnode, &qtsk_rbtree);

	kfree(data);

	return 0;
}

int rs_qtsk_delall(void)
{
	struct rb_node *n;
	struct rs_qtsk_ctxt *iter;

	n = rb_first(&qtsk_rbtree);
	while (n) {
		iter = rb_entry(n, struct rs_qtsk_ctxt, tsk_rbnode);
		rb_erase(n, &qtsk_rbtree);
		kfree(iter);
		n = rb_first(&qtsk_rbtree);
	}

	return 0;
}

int rs_qtsk_add_cb(struct pob_fpsgo_qtsk_info *pfqi)
{
	struct rs_qtsk_ctxt *qtsk = NULL;

	if (!pfqi)
		return -EINVAL;

	rs_qtsk_ctxts_lock();
	rs_qtsk_search_and_add(pfqi->tskid, 1, &qtsk);
	rs_qtsk_ctxts_unlock();

	return 0;
}

int rs_qtsk_del_cb(struct pob_fpsgo_qtsk_info *pfqi)
{
	struct rs_qtsk_ctxt *iter = NULL;

	if (!pfqi)
		return -1;

	rs_qtsk_ctxts_lock();

	rs_qtsk_search_and_add(pfqi->tskid, 0, &iter);

	if (iter) {
		if (iter->cpu_rescuing && cpu_rescuing_cnt > 0)
			cpu_rescuing_cnt--;

		rs_qtsk_del_byctxt(iter);

		if (cpu_rescuing_cnt == 0) {
			/* TO DO: infor others */
			pob_rs_fps_update(POB_RS_CPURESCUE_END);
		}
	}

	rs_qtsk_ctxts_unlock();

	return 0;
}

int rs_qtsk_delall_cb(void)
{
	rs_qtsk_ctxts_lock();
	rs_qtsk_delall();

	if (cpu_rescuing_cnt) {
		cpu_rescuing_cnt = 0;
		/* TO DO: infor others */
		pob_rs_fps_update(POB_RS_CPURESCUE_END);
	}

	rs_qtsk_ctxts_unlock();

	return 0;
}

int rs_qtsk_cpucap_update_cb(struct pob_fpsgo_qtsk_info *pfqi)
{
	struct rs_qtsk_ctxt *iter = NULL;

	if (!pfqi)
		return -EINVAL;

	rs_qtsk_ctxts_lock();
	rs_qtsk_search_and_add(pfqi->tskid, 0, &iter);

	if (!iter)
		goto out;

	if (iter->cpu_rescuing && !pfqi->rescue_cpu) {
		iter->cpu_rescuing = 0;
		cpu_rescuing_cnt--;
		/* TO DO: infor others */

		rsp_systrace_c(RSP_DEBUG_FPSGO, 0, cpu_rescuing_cnt,
				"CPU Rescuing Cnt");

		if (!cpu_rescuing_cnt)
			pob_rs_fps_update(POB_RS_CPURESCUE_END);

	} else if (!iter->cpu_rescuing && pfqi->rescue_cpu) {
		iter->cpu_rescuing = 1;
		cpu_rescuing_cnt++;

		rsp_systrace_c(RSP_DEBUG_FPSGO, 0, cpu_rescuing_cnt,
				"CPU Rescuing Cnt");

		/* TO DO: infor others */
		if (cpu_rescuing_cnt == 1)
			pob_rs_fps_update(POB_RS_CPURESCUE_START);
	}

out:
	rs_qtsk_ctxts_unlock();

	return 0;
}

static int rs_pob_fpsgo_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	switch (val) {
	case POB_FPSGO_TURNON:
	case POB_FPSGO_TURNOFF:

		break;
	case POB_FPSGO_FSTB_STATS_START:
		_gMaxQWCPUTime = 0;
		_gMaxQWGPUTime = 0;
		break;
	case POB_FPSGO_FSTB_STATS_END:
		{
			struct pob_rs_quaweitime_info info;

			info.MaxQWCPUTime = _gMaxQWCPUTime;
			info.MaxQWGPUTime = _gMaxQWGPUTime;
			info.MaxQWAPUTime = 0;

			pob_rs_qw_update(&info);

			rsp_systrace_c(RSP_DEBUG_FPSGO, 0,
					_gMaxQWCPUTime, "MAXQWCPUTime");
			rsp_systrace_c(RSP_DEBUG_FPSGO, 0,
					_gMaxQWGPUTime, "MAXQWGPUTime");
		}
		break;
	case POB_FPSGO_FSTB_STATS_UPDATE:
		{
			struct pob_fpsgo_fpsstats_info *pffi;

			pffi = (struct pob_fpsgo_fpsstats_info *) data;

			if (_gMaxQWCPUTime < pffi->quantile_weighted_cpu_time)
				_gMaxQWCPUTime =
					pffi->quantile_weighted_cpu_time;
			if (_gMaxQWGPUTime < pffi->quantile_weighted_gpu_time)
				_gMaxQWGPUTime =
					pffi->quantile_weighted_gpu_time;
		}
		break;

	case POB_FPSGO_QTSK_ADD:
		rs_qtsk_add_cb(data);
		break;
	case POB_FPSGO_QTSK_DEL:
		rs_qtsk_del_cb(data);
		break;
	case POB_FPSGO_QTSK_DELALL:
		rs_qtsk_delall_cb();
		break;
	case POB_FPSGO_QTSK_CPUCAP_UPDATE:
		rs_qtsk_cpucap_update_cb(data);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rs_pob_fpsgo_notifier = {
	.notifier_call = rs_pob_fpsgo_cb,
};

static int __init rsp_debugfs_init(struct dentry *pob_debugfs_dir)
{
	if (!pob_debugfs_dir)
		return -ENODEV;

	debugfs_create_file("systrace_mask",
			    0644,
			    pob_debugfs_dir,
			    NULL,
			    &rsp_systrace_mask_fops);

	return 0;
}

static int __init mtk_rs_pob_init(void)
{
	struct dentry *rs_debugfs_dir = NULL;
	struct dentry *rs_pob_debugfs_dir = NULL;

	rs_debugfs_dir = rsm_debugfs_dir;

	if (!rs_debugfs_dir)
		return -ENOENT;

	rs_pob_debugfs_dir = debugfs_create_dir("pob", rs_debugfs_dir);

	if (!rs_pob_debugfs_dir)
		return -ENODEV;

	rsp_debugfs_init(rs_pob_debugfs_dir);

	rsp_systrace_mask = RSP_DEBUG_MANDATORY;

#ifdef _DEBUG_BQD_
	pob_bqd_register_client(&rs_pob_bqd_notifier);
#endif
	pob_fpsgo_register_client(&rs_pob_fpsgo_notifier);

	return 0;
}

//device_initcall(mtk_rs_pob_init);
late_initcall(mtk_rs_pob_init);
