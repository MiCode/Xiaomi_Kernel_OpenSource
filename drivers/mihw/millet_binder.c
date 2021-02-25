/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2020. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: oem_binder.c
 * Description: millet-binder-driver
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2020/9/9
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include "../android/binder_oem.h"
#include "millet.h"

static void
__binder_sendto(struct millet_data *data, struct task_struct *dst,
		struct task_struct *src, int caller_tid, bool oneway, int code);

extern void query_binder_app_stat(int uid);
extern void oem_register_binder_hook(struct oem_binder_hook *set);
extern struct oem_binder_hook oem_binder_hook_set;

static void
mi_binder_reply_hook(struct task_struct *dst, struct task_struct *src,
		int caller_tid, bool oneway, int code)
{
	struct millet_data data;

	if (unlikely(!dst))
		return;

	get_task_struct(dst);
	if (task_uid(dst).val > frozen_uid_min) {
		put_task_struct(dst);
		return;
	}

	memset(&data, 0, sizeof(struct millet_data));
	data.pri[0] =  BINDER_REPLY;

	get_task_struct(src);
	__binder_sendto(&data, dst, src, caller_tid, oneway, code);
	put_task_struct(src);
	put_task_struct(dst);
}

static void
mi_binder_trans_hook(struct task_struct *dst, struct task_struct *src,
		int caller_tid, bool oneway, int code)
{
	struct millet_data data;

	if (unlikely(!dst))
		return;

	get_task_struct(dst);
	if ((task_uid(dst).val <= frozen_uid_min)
			|| (dst->pid == src->pid)) {
		put_task_struct(dst);
		return;
	}

	memset(&data, 0, sizeof(struct millet_data));
	data.pri[0] =  BINDER_TRANS;

	get_task_struct(src);
	__binder_sendto(&data, dst, src, caller_tid, oneway, code);
	put_task_struct(src);
	put_task_struct(dst);
}

static void
mi_binder_wait4_hook(struct task_struct *dst, struct task_struct *src,
			int caller_tid, bool oneway, int code)
{
	struct millet_data data;

	if (unlikely(!dst))
		return;

	get_task_struct(dst);
	if ((task_uid(dst).val > frozen_uid_min)
			|| (dst->pid == src->pid)) {
		put_task_struct(dst);
		return;
	}

	memset(&data, 0, sizeof(struct millet_data));
	data.pri[0] =  BINDER_THREAD_HAS_WORK;

	get_task_struct(src);
	__binder_sendto(&data, dst, src, caller_tid, oneway, code);
	put_task_struct(src);
	put_task_struct(dst);
}

static void
mi_binder_overflow_hook(struct task_struct *dst, struct task_struct *src,
		int caller_tid, bool oneway, int code)
{
	struct millet_data data;

	if (unlikely(!dst))
		return;

	get_task_struct(dst);
	memset(&data, 0, sizeof(struct millet_data));
	data.pri[0] =  BINDER_BUFF_WARN;
	get_task_struct(src);
	__binder_sendto(&data, dst, src, caller_tid, oneway, code);
	put_task_struct(src);
	put_task_struct(dst);
}


static void
__binder_sendto(struct millet_data *data, struct task_struct *dst,
		struct task_struct *src, int caller_tid, bool oneway, int code)
{
	data->mod.k_priv.binder.trans.src_task = src;
	data->mod.k_priv.binder.trans.caller_tid = caller_tid;
	data->mod.k_priv.binder.trans.dst_task = dst;
	data->mod.k_priv.binder.trans.tf_oneway = oneway;
	data->mod.k_priv.binder.trans.code = code;
	millet_sendmsg(BINDER_TYPE, dst, data);
}

static void
mi_query_binder_st_hook(int uid, struct task_struct *tsk, int tid, int pid,
		enum BINDER_STAT reason)
{
	struct millet_data data;
	memset(&data, 0, sizeof(struct millet_data));

	data.uid = uid;
	data.mod.k_priv.binder.stat.task = tsk;
	data.mod.k_priv.binder.stat.tid = tid;
	data.mod.k_priv.binder.stat.pid = pid;
	data.mod.k_priv.binder.stat.reason = reason;
	millet_sendmsg(BINDER_ST_TYPE, tsk, &data);
}

static int binder_sendmsg(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)
{
	int ret = 0;
	struct task_struct *src_task;
	struct task_struct *dst_task;

	if (!sk || !data || !tsk) {
		pr_err("%s input invalid\n", __FUNCTION__);
		return RET_ERR;
	}

	data->msg_type = MSG_TO_USER;
	data->owner = BINDER_TYPE;
	src_task = (struct task_struct *)data->mod.k_priv.binder.trans.src_task;
	data->mod.k_priv.binder.trans.caller_pid = task_pid_nr(src_task);
	data->mod.k_priv.binder.trans.caller_uid = task_uid(src_task).val;
	dst_task = (struct task_struct *)data->mod.k_priv.binder.trans.dst_task;
	data->mod.k_priv.binder.trans.dst_pid = task_pid_nr(dst_task);
	data->uid = task_uid(dst_task).val;

	if (frozen_task_group(tsk))
		ret = millet_sendto_user(tsk, data, sk);

	return ret;
}

static void binder_init_millet(struct millet_sock *sk)
{
	if (sk)
		sk->mod[BINDER_TYPE].monitor = BINDER_TYPE;
}

static int binder_st_sendmsg(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)

{
	int ret = RET_OK;
	struct task_struct *task_node;

	if (!sk || !data || !tsk) {
		pr_err("%s input invalid\n", __FUNCTION__);
		return RET_ERR;
	}

	task_node = (struct task_struct *)(data->mod.k_priv.binder.stat.task);
	data->msg_type = MSG_TO_USER;
	data->owner = BINDER_ST_TYPE;
	//data->uid = task_uid(task_node).val;
	ret = millet_sendto_user(tsk, data, sk);

	return ret;
}

static void binder_st_init_millet(struct millet_sock *sk)
{
	if (sk)
		sk->mod[BINDER_ST_TYPE].monitor = BINDER_TYPE;
}

static void binder_recv_hook(void *data, unsigned int len)
{
	struct millet_userconf *payload = (struct millet_userconf *)data;
	int uid = payload->mod.u_priv.binder_st.uid;

	query_binder_app_stat(uid);
}

static void get_oem_binder_hook(struct oem_binder_hook *set)
{
	set->oem_wahead_thresh = WARN_AHEAD_MSGS;
	set->oem_wahead_space = WARN_AHEAD_SPACE;
	set->oem_reply_hook = mi_binder_reply_hook;
	set->oem_trans_hook = mi_binder_trans_hook;
	set->oem_wait4_hook = mi_binder_wait4_hook;
	set->oem_query_st_hook = mi_query_binder_st_hook;
	set->oem_buf_overflow_hook = mi_binder_overflow_hook;
}

static int __init init_millet_binder_drv(void)
{
	struct oem_binder_hook oem_set;

	get_oem_binder_hook(&oem_set);
	oem_register_binder_hook(&oem_set);
	register_millet_hook(BINDER_TYPE, NULL,
			binder_sendmsg, binder_init_millet);
	register_millet_hook(BINDER_ST_TYPE, binder_recv_hook,
			binder_st_sendmsg, binder_st_init_millet);

	return 0;
}

module_init(init_millet_binder_drv);

MODULE_LICENSE("GPL");

