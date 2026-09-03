// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_dele: %s " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sipa.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include "sipa_dele_priv.h"

static int conn_thread(void *data)
{
	struct smsg mrecv;
	struct sipa_delegator *delegator = data;
	int ret;

	/* since the channel open may hang, we call it in the thread context */
	ret = smsg_ch_open(delegator->dst, delegator->chan, -1);
	if (ret != 0) {
		pr_err("sipa_delegator failed to open dst %d channel %d\n",
		       delegator->dst,
		       delegator->chan);
		/* assign NULL to thread poniter as failed to open channel */
		delegator->thread = NULL;
		return ret;
	}

	/* set connect status */
	delegator->connected = true;
	delegator->on_open(delegator, 0, 0);

	/* start listen the smsg events */
	while (!kthread_should_stop()) {
		/* monitor seblock recv smsg */
		smsg_set(&mrecv, delegator->chan, 0, 0, 0);
		ret = smsg_recv(delegator->dst, &mrecv, -1);
		if (ret == -EIO || ret == -ENODEV) {
			/* channel state is FREE */
			usleep_range(5000, 10000);
			continue;
		}

		pr_info("smsg_recv, smsg_cnt=%d, dst=%d, chan=%d, type=%d, flag=0x%x, value=0x%08x\n",
			delegator->smsg_cnt++,
			delegator->dst, delegator->chan,
			mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* just ack open */
			smsg_open_ack(delegator->dst, delegator->chan);
			/* set connect status */
			delegator->connected = true;
			delegator->on_open(delegator, mrecv.flag, mrecv.value);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel close */
			smsg_close_ack(delegator->dst, delegator->chan);
			/* set disconnect status */
			delegator->connected = false;
			delegator->on_close(delegator, mrecv.flag, mrecv.value);
			break;
		case SMSG_TYPE_CMD:
			/* handle commads */
			delegator->on_cmd(delegator, mrecv.flag, mrecv.value);
			break;
		case SMSG_TYPE_DONE:
			/* handle cmd done */
			delegator->on_done(delegator, mrecv.flag, mrecv.value);
			break;
		case SMSG_TYPE_EVENT:
			/* handle events */
			delegator->on_evt(delegator, mrecv.flag, mrecv.value);
			break;
		default:
			ret = 1;
			break;
		};

		if (ret) {
			pr_info("unknown msg in conn_thrd: %d-%d, %d, %d, %d\n",
				delegator->dst, delegator->chan,
				mrecv.type, mrecv.flag, mrecv.value);
			ret = 0;
		}
	}

	pr_err("sipa dele thread %d-%d stop",
	       delegator->dst, delegator->chan);

	return ret;
}

static void sipa_dele_wq_handler(struct work_struct *work)
{
	int ret;
	struct sipa_dele_smsg_work_type *smsg_work =
		container_of(work,
			     struct sipa_dele_smsg_work_type,
			     work);

	pr_info("smsg_send, dst=%d chan=%d type=%d flag=%d\n",
		smsg_work->delegator->dst,
		smsg_work->msg.channel,
		smsg_work->msg.type,
		smsg_work->msg.flag);

	ret = smsg_send(smsg_work->delegator->dst,
			&smsg_work->msg, -1);
	if (ret) {
		enum sipa_rm_res_id prod_id = smsg_work->delegator->prod_id;

		if (smsg_work->msg.flag == SMSG_FLG_DELE_REQUEST)
			sipa_rm_notify_completion(SIPA_RM_EVT_FAIL, prod_id);
		pr_err("smsg send fail %d prod_id = %d\n", ret, prod_id);
	}
}

static void sipa_dele_notify_handler(struct work_struct *work)
{
	struct sipa_delegator *delegator =
		container_of(work, struct sipa_delegator, notify_work);

	pr_debug("sipa dele notify fail\n");
	sipa_rm_notify_completion(SIPA_RM_EVT_FAIL, delegator->prod_id);
}

static void sipa_dele_start_req_work(struct sipa_delegator *delegator)
{
	struct sipa_dele_smsg_work_type *work;

	work = &delegator->req_work;
	INIT_WORK((struct work_struct *)work, sipa_dele_wq_handler);
	work->delegator = delegator;
	work->msg.channel = delegator->chan;
	work->msg.type = SMSG_TYPE_CMD;
	work->msg.flag = SMSG_FLG_DELE_REQUEST;
	work->msg.value = 0;

	if (delegator->stat != SIPA_DELE_POWER_OFF)
		queue_work(delegator->smsg_wq, (struct work_struct *)work);
}

static void sipa_dele_start_rls_work(struct sipa_delegator *delegator)
{
	struct sipa_dele_smsg_work_type *work;

	work = &delegator->rls_work;
	INIT_WORK((struct work_struct *)work, sipa_dele_wq_handler);
	work->delegator = delegator;
	work->msg.channel = delegator->chan;
	work->msg.type = SMSG_TYPE_CMD;
	work->msg.flag = SMSG_FLG_DELE_RELEASE;
	work->msg.value = 0;

	if (delegator->stat != SIPA_DELE_POWER_OFF)
		queue_work(delegator->smsg_wq, (struct work_struct *)work);
}

void sipa_dele_start_done_work(struct sipa_delegator *delegator,
			       u16 flag, u32 val)
{
	struct sipa_dele_smsg_work_type *work;

	work = &delegator->done_work;
	INIT_WORK((struct work_struct *)work, sipa_dele_wq_handler);
	work->delegator = delegator;
	work->msg.channel = delegator->chan;
	work->msg.type = SMSG_TYPE_DONE;
	work->msg.flag = flag;
	work->msg.value = val;

	if (delegator->stat != SIPA_DELE_POWER_OFF)
		queue_work(delegator->smsg_wq, (struct work_struct *)work);
}

static void sipa_dele_r_user_req_cons(struct sipa_delegator *delegator)
{
	int ret;

	atomic_set(&delegator->requesting_cons, 1);
	ret = sipa_rm_request_resource(delegator->cons_user);
	switch (ret) {
	case 0:
		delegator->cons_ref_cnt++;
		if (atomic_cmpxchg(&delegator->requesting_cons, 1, 0))
			sipa_dele_start_done_work(delegator,
						  SMSG_FLG_DELE_REQUEST,
						  SMSG_VAL_DELE_REQ_SUCCESS);
		break;
	case -EINPROGRESS:
		delegator->cons_ref_cnt++;
		break;
	default:
		atomic_set(&delegator->requesting_cons, 0);
		sipa_dele_start_done_work(delegator,
					  SMSG_FLG_DELE_REQUEST,
					  SMSG_VAL_DELE_REQ_FAIL);
		break;
	}
}

static void sipa_dele_r_user_rls_cons(struct sipa_delegator *delegator)
{
	if (!delegator->cons_ref_cnt)
		return;

	delegator->cons_ref_cnt--;
	sipa_rm_release_resource(delegator->cons_user);
}

static void sipa_dele_on_open(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;
	unsigned long flags;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	spin_lock_irqsave(&delegator->lock, flags);
	if (delegator->stat == SIPA_DELE_REQUESTING)
		sipa_dele_start_req_work(delegator);
	spin_unlock_irqrestore(&delegator->lock, flags);
}

static void sipa_dele_on_close(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;
	unsigned long flags;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	spin_lock_irqsave(&delegator->lock, flags);
	delegator->stat = SIPA_DELE_RELEASED;
	spin_unlock_irqrestore(&delegator->lock, flags);
}

void sipa_dele_on_commad(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	switch (flag) {
	case SMSG_FLG_DELE_REQUEST:
		sipa_dele_r_user_req_cons(delegator);
		break;
	case SMSG_FLG_DELE_RELEASE:
		sipa_dele_r_user_rls_cons(delegator);
		break;
	default:
		break;
	}
}

static void sipa_dele_on_done(void *priv, u16 flag, u32 val)
{
	struct sipa_delegator *delegator = priv;
	enum sipa_dele_state last_stat;
	unsigned long flags;

	pr_debug("prod_id:%d, flag = %d\n", delegator->prod_id, flag);

	if (flag != SMSG_FLG_DELE_REQUEST)
		return;
	spin_lock_irqsave(&delegator->lock, flags);
	last_stat = delegator->stat;
	switch (delegator->stat) {
	case SIPA_DELE_ACTIVE:
		break;
	case SIPA_DELE_REQUESTING:
		if (val == SMSG_VAL_DELE_REQ_FAIL) {
			sipa_dele_start_req_work(delegator);
		} else {
			delegator->stat = SIPA_DELE_ACTIVE;
			/* do request completed notify */
			sipa_rm_notify_completion(SIPA_RM_EVT_GRANTED,
						  delegator->prod_id);
		}
		break;
	case SIPA_DELE_RELEASING:
		sipa_dele_start_rls_work(delegator);
		delegator->stat = SIPA_DELE_RELEASED;
		break;
	case SIPA_DELE_RELEASED:
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&delegator->lock, flags);

	pr_debug("smsg req done stat:%d -> %d\n",
		 last_stat, delegator->stat);
}

static void sipa_dele_on_event(void *priv, u16 flag, u32 data)
{
	pr_debug("flag:%d data:%d\n", flag, data);
}

static int sipa_dele_local_rls_r_prod(void *user_data)
{
	unsigned long flags;
	struct sipa_delegator *delegator = user_data;
	int ret;

	pr_info("prod_id:%d, delegator->stat = %d\n",
		delegator->prod_id, delegator->stat);
	spin_lock_irqsave(&delegator->lock, flags);
	switch (delegator->stat) {
	case SIPA_DELE_ACTIVE:
		delegator->stat = SIPA_DELE_RELEASED;
		sipa_dele_start_rls_work(delegator);
		ret = 0;
		break;
	case SIPA_DELE_REQUESTING:
		delegator->stat = SIPA_DELE_RELEASED;
		sipa_dele_start_rls_work(delegator);
		ret = -EINPROGRESS;
		break;
	case SIPA_DELE_RELEASING:
		ret = -EINPROGRESS;
		break;
	case SIPA_DELE_POWER_OFF:
		ret = 0;
		break;
	case SIPA_DELE_RELEASED:
		ret = 0;
		break;
	default:
		ret = -EPERM;
		break;
	}
	spin_unlock_irqrestore(&delegator->lock, flags);

	return ret;
}

int sipa_dele_local_req_r_prod(void *user_data)
{
	unsigned long flags;
	struct sipa_delegator *delegator = user_data;
	int ret;

	pr_info("prod_id:%d, delegator->stat = %d\n",
		delegator->prod_id, delegator->stat);
	spin_lock_irqsave(&delegator->lock, flags);
	switch (delegator->stat) {
	case SIPA_DELE_ACTIVE:
		ret = 0;
		break;
	case SIPA_DELE_REQUESTING:
		ret = -EINPROGRESS;
		break;
	case SIPA_DELE_RELEASING:
		delegator->stat = SIPA_DELE_REQUESTING;
		ret = -EINPROGRESS;
		break;
	case SIPA_DELE_RELEASED:
		delegator->stat = SIPA_DELE_REQUESTING;
		if (delegator->connected)
			sipa_dele_start_req_work(delegator);
		ret = -EINPROGRESS;
		break;
	case SIPA_DELE_POWER_OFF:
		queue_work(delegator->smsg_wq, &delegator->notify_work);
		ret = -EINPROGRESS;
		break;
	default:
		ret = -EPERM;
		break;
	}
	spin_unlock_irqrestore(&delegator->lock, flags);

	return ret;
}

static void sipa_dele_cons_notify_cb(void *user_data,
				     enum sipa_rm_event event,
				     unsigned long data)
{
	struct sipa_delegator *delegator = user_data;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	if (event != SIPA_RM_EVT_GRANTED)
		return;
	if (atomic_cmpxchg(&delegator->requesting_cons, 1, 0))
		sipa_dele_start_done_work(delegator,
					  SMSG_FLG_DELE_REQUEST,
					  SMSG_VAL_DELE_REQ_SUCCESS);
}

int sipa_delegator_init(struct sipa_delegator *delegator,
			struct sipa_delegator_create_params *params)
{
	/* value init */
	delegator->pdev = params->pdev;
	delegator->cfg = params->cfg;
	delegator->prod_id = params->prod_id;
	delegator->cons_prod = params->cons_prod;
	delegator->cons_user = params->cons_user;
	delegator->stat = SIPA_DELE_RELEASED;
	delegator->cons_ref_cnt = 0;
	delegator->dst = params->dst;
	delegator->chan = params->chan;
	delegator->connected = false;
	delegator->is_powered = true;
	atomic_set(&delegator->requesting_cons, 0);
	delegator->on_open = sipa_dele_on_open;
	delegator->on_close = sipa_dele_on_close;
	delegator->on_cmd = sipa_dele_on_commad;
	delegator->on_done = sipa_dele_on_done;
	delegator->on_evt = sipa_dele_on_event;
	delegator->local_request_prod = sipa_dele_local_req_r_prod;
	delegator->local_release_prod = sipa_dele_local_rls_r_prod;
	INIT_WORK(&delegator->notify_work, sipa_dele_notify_handler);
	spin_lock_init(&delegator->lock);

	delegator->smsg_wq = create_singlethread_workqueue("dele_smsg_wq");
	if (!delegator->smsg_wq) {
		pr_err("create workqueue failed\n");
		return -ENOMEM;
	}

	/* create channel thread for this seblock channel */
	delegator->thread = kthread_create(conn_thread, delegator,
					   "dele-%d-%d", delegator->prod_id,
					   delegator->dst);
	if (IS_ERR(delegator->thread)) {
		pr_err("Failed to create monitor kthread: prod_id:%d\n",
		       delegator->prod_id);
		destroy_workqueue(delegator->smsg_wq);
		return PTR_ERR(delegator->thread);
	}

	return 0;
}

void sipa_delegator_exit(struct sipa_delegator *delegator)
{
	if (delegator) {
		destroy_workqueue(delegator->smsg_wq);
		kthread_stop(delegator->thread);
	}
}

int sipa_delegator_start(struct sipa_delegator *delegator)
{
	struct sipa_rm_create_params rm_params;
	struct sipa_rm_register_params reg_params;
	int ret;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	/* start monitor thread */
	wake_up_process(delegator->thread);

	/* sipa resource manage operations */
	rm_params.name = delegator->prod_id;
	rm_params.floor_voltage = 0;
	rm_params.reg_params.notify_cb = NULL;
	rm_params.reg_params.user_data = delegator;
	rm_params.request_resource = delegator->local_request_prod;
	rm_params.release_resource = delegator->local_release_prod;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret)
		return ret;

	ret = sipa_rm_add_dependency(delegator->cons_prod,
				     delegator->prod_id);
	if (ret)
		goto del_res;

	reg_params.notify_cb = sipa_dele_cons_notify_cb;
	reg_params.user_data = delegator;
	ret = sipa_rm_register(delegator->cons_user, &reg_params);
	if (ret)
		goto del_res;

	return 0;
del_res:
	sipa_rm_delete_resource(delegator->prod_id);
	return ret;
}
