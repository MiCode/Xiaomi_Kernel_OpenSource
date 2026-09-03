// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2016 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sprd-imsbr: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>

#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_sipc.h"

/**
 * Temporary solution, will notify CP imsbr whether the volte video's socket
 * is located at AP (0 - disabled 1 - enabled).
 *
 * -- I hope that this ugly codes will be abandoned one day :(
 */
static unsigned int volte_video_apsk __read_mostly = 1;

int imsbr_notify_ltevideo_apsk(void)
{
	int err;
	u32 enable = volte_video_apsk;
	struct sblock blk;

	err = imsbr_build_cmd("ltevideo-apsk", &blk, &enable, sizeof(enable));
	if (unlikely(err < 0))
		return err;

	err = imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(enable));
	if (unlikely(err < 0))
		return err;

	pr_info("notify ltevideo-apsk=%d success!\n", enable);
	return 0;
}

static int imsbr_pre_notify(struct imsbr_sipc *sipc)
{
	int err, i, max = 10;

	for (i = 0; i < max; i++) {
		err = imsbr_notify_ltevideo_apsk();
		if (!err)
			break;

		/* Waiting for cp to be truely alive! */
		schedule_timeout_interruptible(HZ / 100);
	}

	if (i == max) {
		pr_err("fail to notify cp with ltevideo-apsk msg\n");
		return err;
	}

	return 0;
}

struct imsbr_sipc imsbr_data __read_mostly = {
	.dst		= SIPC_ID_LTE,
	.channel	= SMSG_CH_IMSBR_DATA,
	.hdrlen		= sizeof(struct imsbr_packet),
	.blksize	= IMSBR_DATA_BLKSZ,
	.blknum		= IMSBR_DATA_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.process	= imsbr_process_packet,
};

struct imsbr_sipc imsbr_ctrl __read_mostly = {
	.dst		= SIPC_ID_LTE,
	.channel	= SMSG_CH_IMSBR_CTRL,
	.hdrlen		= sizeof(struct imsbr_msghdr),
	.blksize	= IMSBR_CTRL_BLKSZ,
	.blknum		= IMSBR_CTRL_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.pre_hook	= imsbr_pre_notify,
	.process	= imsbr_process_msg,
};

int imsbr_build_cmd(const char *cmd, struct sblock *blk,
		    void *payload, int paylen)
{
	int err;
	struct imsbr_msghdr _msg;

	if (paylen > IMSBR_MSG_MAXLEN) {
		pr_err("paylen %d is too large, max is %d\n",
		       paylen, (int)IMSBR_MSG_MAXLEN);
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	err = imsbr_sblock_get(&imsbr_ctrl, blk, IMSBR_MSG_MAXLEN);
	if (err)
		return err;

	_msg.imsbr_version = IMSBR_MSG_VERSION;
	_msg.imsbr_paylen = paylen;
	strlcpy(_msg.imsbr_cmd, cmd, IMSBR_CMD_MAXSZ);

	unalign_memcpy(blk->addr, &_msg, sizeof(_msg));

	if (payload) {
		struct imsbr_msghdr *m = blk->addr;

		unalign_memcpy(m->imsbr_payload, payload, paylen);
	}

	return 0;
}

int imsbr_sblock_receive(struct imsbr_sipc *sipc, struct sblock *blk)
{
	int err;

	err = sblock_receive(sipc->dst, sipc->channel, blk, -1);
	if (unlikely(err < 0)) {
		IMSBR_STAT_INC(imsbr_stats->sipc_receive_fail);
		pr_err("sblock_receive %s fail, error=%d\n",
		       sipc->desc, err);
		return err;
	}

	return 0;
}

int imsbr_sblock_get(struct imsbr_sipc *sipc, struct sblock *blk, int size)
{
	int err;

	size += sipc->hdrlen;

	err = sblock_get(sipc->dst, sipc->channel, blk, 0);
	if (unlikely(err < 0)) {
		IMSBR_STAT_INC(imsbr_stats->sipc_get_fail);
		pr_err("sblock_get %s fail, error=%d\n",
		       sipc->desc, err);
		return err;
	}

	if (unlikely(blk->length < size)) {
		pr_err("sblock len %d is too short, alloc size is %d\n",
		       blk->length, size);
		imsbr_sblock_put(sipc, blk);
		return err;
	}

	return 0;
}

int imsbr_sblock_send(struct imsbr_sipc *sipc, struct sblock *blk, int size)
{
	int err;

	blk->length = sipc->hdrlen + size;

	err = sblock_send(sipc->dst, sipc->channel, blk);
	if (unlikely(err < 0)) {
		IMSBR_STAT_INC(imsbr_stats->sipc_send_fail);
		pr_err("sblock_send %s fail, error=%d\n",
		       sipc->desc, err);
		sblock_put(sipc->dst, sipc->channel, blk);
		return err;
	}

	return 0;
}

void imsbr_sblock_release(struct imsbr_sipc *sipc, struct sblock *blk)
{
	sblock_release(sipc->dst, sipc->channel, blk);
}

void imsbr_sblock_put(struct imsbr_sipc *sipc, struct sblock *blk)
{
	sblock_put(sipc->dst, sipc->channel, blk);
}

static void imsbr_handler(int event, void *data)
{
	struct imsbr_sipc *sipc = (struct imsbr_sipc *)data;

	WARN_ON(!sipc);

	if (!sipc) {
		pr_err("sipc is null\n");
		return;
	}

	/* Receieve event means peer in CP side is ok! */
	if (!atomic_cmpxchg(&sipc->peer_ready, 0, 1)) {
		pr_info("recv %d event, %s's peer is alive!\n",
			event, sipc->desc);
		complete_all(&sipc->peer_comp);
	}

	switch (event) {
	case SBLOCK_NOTIFY_GET:
	case SBLOCK_NOTIFY_RECV:
	case SBLOCK_NOTIFY_STATUS:
	case SBLOCK_NOTIFY_OPEN:
	case SBLOCK_NOTIFY_CLOSE:
		pr_debug("%s recv event %d\n", sipc->desc, event);
		break;
	default:
		pr_err("%s recv event %d\n", sipc->desc, event);
		break;
	}
}

static int imsbr_kthread(void *arg)
{
	int err;
	struct imsbr_sipc *sipc = arg;

	set_user_nice(current, -20);
	wait_for_completion_interruptible(&sipc->peer_comp);
	pr_info("%s is ok, kthread run!\n", sipc->desc);

	if (sipc->pre_hook) {
		err = sipc->pre_hook(sipc);
		if (unlikely(err < 0))
			return err;
	}

	while (!kthread_should_stop()) {
		struct sblock blk;

		if (imsbr_sblock_receive(sipc, &blk)) {
			schedule_timeout_interruptible(HZ / 50);
			continue;
		}

		if (blk.length < sipc->hdrlen) {
			pr_err("%s recv len=%d, min=%d, too short!\n",
			       sipc->desc, blk.length, sipc->hdrlen);
			imsbr_sblock_release(sipc, &blk);
		} else if (blk.length > sipc->blksize) {
			pr_err("%s recv len=%d, max=%d, too long!\n",
			       sipc->desc, blk.length, sipc->blksize);
			imsbr_sblock_release(sipc, &blk);
		} else {
			sipc->process(sipc, &blk, true);
		}
	}

	return 0;
}

static int imsbr_sipc_query_to_register(struct imsbr_sipc *sipc)
{
	struct task_struct *tsk;
	int err;
	int try = 0;

	snprintf(sipc->desc, sizeof(sipc->desc), "%s[%d-%d]",
		 sipc == &imsbr_ctrl ? "ctrl" : "data",
		 sipc->dst, sipc->channel);

	init_completion(&sipc->peer_comp);
again:
	err = sblock_query(sipc->dst, sipc->channel);
	if (err) {
		if (++try > 300) {
			pr_err("sblock [%d-%u] not ready, try %d, err %d\n",
			       sipc->dst, sipc->channel, try, err);
			return err;
		}
		msleep(1000);
		goto again;
	}

	err = sblock_register_notifier(sipc->dst, sipc->channel,
				       imsbr_handler, sipc);
	if (err) {
		pr_err("sblock_register_notifier %s fail, error=%d\n",
		       sipc->desc, err);
		goto fail;
	}

	tsk = kthread_run(imsbr_kthread, sipc, "imsbr-%s", sipc->desc);
	if (IS_ERR(tsk)) {
		pr_err("%s kthread_create fail: %ld\n",
		       sipc->desc, PTR_ERR(tsk));
		goto fail;
	}

	sipc->task = tsk;
	return 0;

fail:
	sblock_destroy(sipc->dst, sipc->channel);
	return err;
}

static void imsbr_sipc_destroy(struct imsbr_sipc *sipc)
{
	if (sipc->task)
		kthread_stop(sipc->task);

	sblock_destroy(sipc->dst, sipc->channel);
}

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

void call_imsbr_sipc_function(struct call_internal_function *cif)
{
	cif->sipc_handler = imsbr_handler;
	cif->sipc_kthread = imsbr_kthread;
	cif->sipc_create = imsbr_sipc_query_to_register;
	cif->sipc_destroy = imsbr_sipc_destroy;
}

#endif

static void imsbr_sipc_init_work(struct work_struct *wk)
{
	struct imsbr_sipc *sipc = container_of(wk, struct imsbr_sipc,
					       initwork);

	pr_debug("sipc %s [%d-%d] workqueue start\n",
		sipc->desc, sipc->dst, sipc->channel);
	imsbr_sipc_query_to_register(sipc);
}

int __init imsbr_sipc_init(void)
{
	INIT_WORK(&imsbr_data.initwork, imsbr_sipc_init_work);
	INIT_WORK(&imsbr_ctrl.initwork, imsbr_sipc_init_work);
	schedule_work(&imsbr_data.initwork);
	schedule_work(&imsbr_ctrl.initwork);

	return 0;
}

void imsbr_sipc_exit(void)
{
	imsbr_sipc_destroy(&imsbr_ctrl);
	imsbr_sipc_destroy(&imsbr_data);
}
