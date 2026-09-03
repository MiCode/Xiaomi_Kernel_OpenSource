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

#define pr_fmt(fmt) "sprd-imsbr-transit: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>

#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_sipc.h"
#include "imsbr_transit.h"

static struct imsbr_sipc imsbr_data_ap __read_mostly = {
	.dst		= SIPC_ID_AP,
	.channel	= SMSG_CH_IMSBR_DATA,
	.hdrlen		= sizeof(struct imsbr_packet),
	.blksize	= IMSBR_DATA_BLKSZ,
	.blknum		= IMSBR_DATA_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.process	= imsbr_transit_process,
};

static struct imsbr_sipc imsbr_ctrl_ap __read_mostly = {
	.dst		= SIPC_ID_AP,
	.channel	= SMSG_CH_IMSBR_CTRL,
	.hdrlen		= sizeof(struct imsbr_msghdr),
	.blksize	= IMSBR_CTRL_BLKSZ,
	.blknum		= IMSBR_CTRL_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.process	= imsbr_transit_process,
};

static struct imsbr_sipc imsbr_data_cp __read_mostly = {
	.dst		= SIPC_ID_LTE,
	.channel	= SMSG_CH_IMSBR_DATA,
	.hdrlen		= sizeof(struct imsbr_packet),
	.blksize	= IMSBR_DATA_BLKSZ,
	.blknum		= IMSBR_DATA_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.process	= imsbr_transit_process,
};

static struct imsbr_sipc imsbr_ctrl_cp __read_mostly = {
	.dst		= SIPC_ID_LTE,
	.channel	= SMSG_CH_IMSBR_CTRL,
	.hdrlen		= sizeof(struct imsbr_msghdr),
	.blksize	= IMSBR_CTRL_BLKSZ,
	.blknum		= IMSBR_CTRL_BLKNUM,
	.peer_ready	= ATOMIC_INIT(0),
	.process	= imsbr_transit_process,
};

static char *imsbr_transit_get_sipc_name(struct imsbr_sipc *sipc)
{
	if (sipc == &imsbr_ctrl_ap)
		return "ctrl_ap";
	else if (sipc == &imsbr_data_ap)
		return "data_ap";
	else if (sipc == &imsbr_ctrl_cp)
		return "ctrl_cp";
	else if (sipc == &imsbr_data_cp)
		return "data_cp";

	return "unknown";
}

static void imsbr_transit_sblock_release(struct imsbr_sipc *sipc,
					 struct sblock *blk)
{
	sblock_release(sipc->dst, sipc->channel, blk);
}

static int imsbr_transit_sblock_receive(struct imsbr_sipc *sipc,
					struct sblock *blk)
{
	int err;

	err = sblock_receive(sipc->dst, sipc->channel, blk, -1);
	if (unlikely(err < 0)) {
		pr_err("sblock_receive %s fail, error=%d\n",
		       sipc->desc, err);
		return err;
	}

	return 0;
}

static int imsbr_transit_sblock_send(struct imsbr_sipc *sipc,
				     struct sblock *blk, int size)
{
	int err;

	err = sblock_send(sipc->dst, sipc->channel, blk);
	if (unlikely(err < 0)) {
		pr_err("sblock_send %s fail, error=%d\n",
		       sipc->desc, err);
		sblock_put(sipc->dst, sipc->channel, blk);
		return err;
	}

	return 0;
}

static void imsbr_transit_sblock_put(struct imsbr_sipc *sipc,
				     struct sblock *blk)
{
	sblock_put(sipc->dst, sipc->channel, blk);
}

static int imsbr_transit_sblock_get(struct imsbr_sipc *sipc, struct sblock *blk,
				    int size)
{
	int err;

	err = sblock_get(sipc->dst, sipc->channel, blk, 0);
	if (unlikely(err < 0)) {
		pr_err("sblock_get %s fail, error=%d\n",
		       sipc->desc, err);
		return err;
	}

	if (unlikely(blk->length < size)) {
		pr_err("sblock len %d is too short, alloc size is %d\n",
		       blk->length, size);
		imsbr_transit_sblock_put(sipc, blk);
		return -EINVAL;
	}

	return 0;
}

void imsbr_transit_process(struct imsbr_sipc *sipc, struct sblock *blk,
			   bool freeit __maybe_unused)
{
	struct imsbr_sipc *target = NULL;
	struct sblock sblk;
	struct imsbr_msghdr *msghdr;
	char msgbuff[IMSBR_CTRL_BLKSZ];
	int err, len;
	char *cmd;

	if (!blk) {
		pr_err("blk is NULL");
		return;
	}

	len = blk->length;

	if (sipc == &imsbr_data_ap) {
		target = &imsbr_data_cp;
	} else if (sipc == &imsbr_ctrl_ap) {
		target = &imsbr_ctrl_cp;
	} else if (sipc == &imsbr_data_cp) {
		target = &imsbr_data_ap;
	} else if (sipc == &imsbr_ctrl_cp) {
		target = &imsbr_ctrl_ap;
	} else {
		pr_err("unknown sipc type");
		imsbr_transit_sblock_release(sipc, blk);
		return;
	}

	err = imsbr_transit_sblock_get(target, &sblk, len);
	if (err) {
		pr_err("fail to get sblock from %s, err %d",
		       target->desc, err);
		imsbr_transit_sblock_release(sipc, blk);
		return;
	}

	unalign_memcpy(sblk.addr, blk->addr, len);
	sblk.length = len;

	imsbr_transit_sblock_release(sipc, blk);

	if (target == &imsbr_ctrl_ap || target == &imsbr_ctrl_cp) {
		unalign_memcpy(msgbuff, sblk.addr, sblk.length);
		msghdr = (struct imsbr_msghdr *)msgbuff;

		cmd = msghdr->imsbr_cmd;
		pr_debug("ctrl[%s] len=%d to %s\n", msghdr->imsbr_cmd,
			 len, target->desc);

	} else if (target == &imsbr_data_ap || target == &imsbr_data_cp) {
		pr_debug("data len=%d to %s\n", len, target->desc);
	}

	err = imsbr_transit_sblock_send(target, &sblk, len);
	if (err)
		pr_err("fail to transit sblock to %s, err %d",
		       target->desc, err);
}

static void imsbr_sipc_handler(int event, void *data)
{
	struct imsbr_sipc *sipc = (struct imsbr_sipc *)data;

	WARN_ON(!sipc);

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
	struct imsbr_sipc *sipc = arg;

	set_user_nice(current, -20);
	wait_for_completion_interruptible(&sipc->peer_comp);
	pr_debug("%s is ok, kthread run!\n", sipc->desc);

	while (!kthread_should_stop()) {
		struct sblock blk;

		if (imsbr_transit_sblock_receive(sipc, &blk)) {
			schedule_timeout_interruptible(HZ / 50);
			continue;
		}

		pr_debug("receive blk addr %p, blk length %d from %s",
			 blk.addr, blk.length, sipc->desc);

		if (blk.length < sipc->hdrlen) {
			pr_err("%s recv len=%d, min=%d, too short!\n",
			       sipc->desc, blk.length, sipc->hdrlen);
			imsbr_transit_sblock_release(sipc, &blk);
		} else if (blk.length > sipc->blksize) {
			pr_err("%s recv len=%d, max=%d, too long!\n",
			       sipc->desc, blk.length, sipc->blksize);
			imsbr_transit_sblock_release(sipc, &blk);
		} else {
			sipc->process(sipc, &blk, true);
		}
	}

	return 0;
}

static int imsbr_sipc_create(struct imsbr_sipc *sipc)
{
	struct task_struct *tsk;
	int err;

	snprintf(sipc->desc, sizeof(sipc->desc), "%s[%d-%d]",
		 imsbr_transit_get_sipc_name(sipc), sipc->dst, sipc->channel);

	init_completion(&sipc->peer_comp);

	err = sblock_create(sipc->dst, sipc->channel, sipc->blknum,
			    sipc->blksize, sipc->blknum, sipc->blksize);
	if (err) {
		pr_err("sblock_create %s fail, error=%d\n",
		       sipc->desc, err);
		return err;
	}

	err = sblock_register_notifier(sipc->dst, sipc->channel,
				       imsbr_sipc_handler, sipc);
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

static int __init imsbr_transit_init(void)
{
	int err;

	err = imsbr_sipc_create(&imsbr_data_ap);
	if (err) {
		pr_err("fail to create imsbr_data_ap");
		return err;
	}

	err = imsbr_sipc_create(&imsbr_ctrl_ap);
	if (err) {
		pr_err("fail to create imsbr_ctrl_ap");
		goto err_ctrl_ap;
	}

	err = imsbr_sipc_create(&imsbr_data_cp);
	if (err) {
		pr_err("fail to create imsbr_data_cp");
		goto err_data_cp;
	}

	err = imsbr_sipc_create(&imsbr_ctrl_cp);
	if (err) {
		pr_err("fail to create err_ctrl_cp");
		goto err_ctrl_cp;
	}

	pr_debug("ok to create all sblock channel");
	return 0;

err_ctrl_cp:
	imsbr_sipc_destroy(&imsbr_data_cp);
	imsbr_sipc_destroy(&imsbr_ctrl_ap);
	imsbr_sipc_destroy(&imsbr_data_ap);
err_data_cp:
	imsbr_sipc_destroy(&imsbr_ctrl_ap);
	imsbr_sipc_destroy(&imsbr_data_ap);
err_ctrl_ap:
	imsbr_sipc_destroy(&imsbr_data_ap);
	return err;
}

static void imsbr_transit_exit(void)
{
	imsbr_sipc_destroy(&imsbr_ctrl_cp);
	imsbr_sipc_destroy(&imsbr_data_cp);
	imsbr_sipc_destroy(&imsbr_ctrl_ap);
	imsbr_sipc_destroy(&imsbr_data_ap);
}

module_init(imsbr_transit_init);
module_exit(imsbr_transit_exit);

MODULE_AUTHOR("Wade Shu <wade.shu@unisoc.com>");
MODULE_DESCRIPTION("IMS bridge transit module for two IMSBR modules in both AP and CP");
MODULE_LICENSE("GPL v2");
