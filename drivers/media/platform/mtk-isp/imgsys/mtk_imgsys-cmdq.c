// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-ext.h"
#include "mtk_imgsys-cmdq-plat.h"
#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-trace.h"
#include "mtk-interconnect.h"

struct workqueue_struct *imgsys_cmdq_wq;
static u32 is_stream_off;

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	u32 idx = 0;

	pr_info("%s: +, dev(0x%x), num(%d)\n", __func__, dev, nr_imgsys_dev);

	/* Only first user has to do init work queue */
	if (nr_imgsys_dev == 1) {
		imgsys_cmdq_wq = alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE,
				"imgsys_cmdq_cb_wq");
		if (!imgsys_cmdq_wq)
			pr_info("%s: Create workquque IMGSYS-CMDQ fail!\n",
								__func__);
	}

	switch (nr_imgsys_dev) {
	case 1:	/* DIP */
		/* request thread by index (in dts) 0 */
		for (idx = 0; idx < IMGSYS_ENG_MAX; idx++) {
			imgsys_clt[idx] = cmdq_mbox_create(dev, idx);
			pr_info("%s: cmdq_mbox_create(%d, 0x%x)\n", __func__, idx, imgsys_clt[idx]);
		}
		/* parse hardware event */
		for (idx = 0; idx < IMGSYS_CMDQ_EVENT_MAX; idx++) {
			of_property_read_u16(dev->of_node,
				imgsys_event[idx].dts_name,
				&imgsys_event[idx].event);
			pr_info("%s: event idx %d is (%s, %d)\n", __func__,
				idx, imgsys_event[idx].dts_name,
				imgsys_event[idx].event);
		}
		break;
	default:
		break;
	}

	mutex_init(&imgsys_dev->dvfs_qos_lock);

}

void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;
	pr_info("%s: +\n", __func__);

	/* Destroy cmdq client */
	for (idx = 0; idx < IMGSYS_ENG_MAX; idx++) {
		cmdq_mbox_destroy(imgsys_clt[idx]);
		imgsys_clt[idx] = NULL;
	}

	/* Release work_quque */
	flush_workqueue(imgsys_cmdq_wq);
	destroy_workqueue(imgsys_cmdq_wq);
	imgsys_cmdq_wq = NULL;
	mutex_destroy(&imgsys_dev->dvfs_qos_lock);
}

static void imgsys_cmdq_cmd_dump(struct swfrm_info_t *frm_info, u32 frm_idx)
{
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;

	cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;
	cmd_num = cmd_buf->curr_length / sizeof(struct Command);

	pr_info(
	"%s: +, req fd/no(%d/%d) frame no(%d) frm(%d/%d), cmd_oft(0x%x), cmd_len(%d), num(%d), sz_per_cmd(%d), frm_blk(%d), hw_comb(0x%x)\n",
		__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
		frm_idx, frm_info->total_frmnum, cmd_buf->cmd_offset,
		cmd_buf->curr_length, cmd_num, sizeof(struct Command), cmd_buf->frame_block,
		frm_info->user_info[frm_idx].hw_comb);
	cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
		(unsigned long)(cmd_buf->cmd_offset));

	for (cmd_idx = 0; cmd_idx < cmd_num; cmd_idx++) {
		switch (cmd[cmd_idx].opcode) {
		case IMGSYS_CMD_READ:
			pr_info(
			"%s: READ with source(0x%08lx) target(0x%08x) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.source, cmd[cmd_idx].u.target, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WRITE:
			pr_info(
			"%s: WRITE with addr(0x%08lx) value(0x%08x) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_POLL:
			pr_info(
			"%s: POLL with addr(0x%08lx) value(0x%08x) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WAIT:
			pr_info(
			"%s: WAIT event(%d/%d) action(%d)\n", __func__,
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_UPDATE:
			pr_info(
			"%s: UPDATE event(%d/%d) action(%d)\n", __func__,
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_ACQUIRE:
			pr_info(
			"%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_STOP:
			pr_info("%s: End Of Cmd!\n", __func__);
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd[cmd_idx].opcode);
			break;
		}
	}
}

static void imgsys_cmdq_cb_work(struct work_struct *work)
{
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dev *imgsys_dev = NULL;
	u32 hw_comb = 0;
	u32 cb_frm_cnt = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;

	pr_debug("%s: +\n", __func__);

	cb_param = container_of(work, struct mtk_imgsys_cb_param, cmdq_cb_work);
	cb_param->cmdqTs.tsCmdqCbWorkStart = ktime_get_boottime_ns()/1000;
	imgsys_dev = cb_param->imgsys_dev;

	if (is_stream_off == 0) {
		dev_dbg(imgsys_dev->dev,
			"%s: req fd/no(%d/%d) frame no(%d) cb(%p) isBlkLast(%d) isFrmLast(%d) for frm(%d/%d)\n",
			__func__, cb_param->frm_info->request_fd,
			cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
			cb_param, cb_param->isBlkLast, cb_param->isFrmLast,
			cb_param->frm_idx, cb_param->frm_num);

		hw_comb = cb_param->frm_info->user_info[cb_param->frm_idx].hw_comb;

		if (cb_param->isBlkLast) {
			cb_param->frm_info->cb_frmcnt++;
			cb_frm_cnt = cb_param->frm_info->cb_frmcnt;
		}

		if (cb_param->isBlkLast &&
			(cb_param->frm_num == cb_frm_cnt) &&
			cb_param->user_cmdq_cb) {
			struct cmdq_cb_data user_cb_data;

			/* PMQOS API */
			tsDvfsQosStart = ktime_get_boottime_ns()/1000;
			IMGSYS_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx\n",
				__func__, "dvfs_qos", cb_param->frm_info->frame_no,
				cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
				cb_param->frm_info->frm_owner);
			mutex_lock(&(imgsys_dev->dvfs_qos_lock));
			#if DVFS_QOS_READY
			mtk_imgsys_mmdvfs_mmqos_cal(imgsys_dev, cb_param->frm_info, 0);
			mtk_imgsys_mmdvfs_set(imgsys_dev, cb_param->frm_info, 0);
			mtk_imgsys_mmqos_set(imgsys_dev, cb_param->frm_info, 0);
			#endif
			mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
			IMGSYS_SYSTRACE_END();
			tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

			user_cb_data.err = cb_param->err;
			user_cb_data.data = (void *)cb_param->frm_info;
			cb_param->cmdqTs.tsUserCbStart = ktime_get_boottime_ns()/1000;
			IMGSYS_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx\n",
				__func__, "user_cb", cb_param->frm_info->frame_no,
				cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
				cb_param->frm_info->frm_owner);
			cb_param->user_cmdq_cb(user_cb_data);
			IMGSYS_SYSTRACE_END();
			cb_param->cmdqTs.tsUserCbEnd = ktime_get_boottime_ns()/1000;
		}
	} else
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)!\n",
			__func__, cb_param, is_stream_off);

	IMGSYS_SYSTRACE_BEGIN(
		"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb:%p frm(%d/%d/%d) DvfsSt(%lld) SetCmd(%lld) HW(%lld) Cmdqcb(%lld) WK(%lld) UserCb(%lld) DvfsEnd(%lld)\n",
		__func__, "wait_pkt", cb_param->frm_info->frame_no,
		cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
		cb_param->frm_info->user_info[cb_param->frm_idx].subfrm_idx, hw_comb,
		cb_param->frm_info->frm_owner, cb_param,
		cb_param->frm_idx, cb_param->frm_num, cb_frm_cnt,
		(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
		(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
		(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
		(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
		(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
		(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
		(tsDvfsQosEnd-tsDvfsQosStart));
	cmdq_pkt_wait_complete(cb_param->pkt);
	cmdq_pkt_destroy(cb_param->pkt);
	cb_param->cmdqTs.tsReqEnd = ktime_get_boottime_ns()/1000;
	IMGSYS_SYSTRACE_END();

	dev_dbg(imgsys_dev->dev,
	"%s: TSus cb(%p) err(%d) frm(%d/%d/%d) hw_comb(0x%x) DvfsSt(%lld) Req(%lld) SetCmd(%lld) HW(%lld) Cmdqcb(%lld) WK(%lld) CmdqCbWk(%lld) UserCb(%lld) DvfsEnd(%lld)\n",
		__func__, cb_param, cb_param->err, cb_param->frm_idx,
		cb_param->frm_num, cb_frm_cnt, hw_comb,
		(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
		(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsReqStart),
		(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
		(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
		(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
		(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
		(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsCmdqCbWorkStart),
		(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
		(tsDvfsQosEnd-tsDvfsQosStart)
		);
	vfree(cb_param);
}

void imgsys_cmdq_task_cb(struct cmdq_cb_data data)
{
	struct mtk_imgsys_cb_param *cb_param;
	struct mtk_imgsys_pipe *pipe;

	pr_debug("%s: +\n", __func__);

	if (!data.data) {
		pr_info("%s: [ERROR]no callback data\n", __func__);
		return;
	}

	cb_param = (struct mtk_imgsys_cb_param *)data.data;
	cb_param->err = data.err;
	cb_param->cmdqTs.tsCmdqCbStart = ktime_get_boottime_ns()/1000;

	pr_debug(
		"%s: Receive cb(%p) with err(%d) for frm(%d/%d)\n",
		__func__, cb_param, data.err, cb_param->frm_idx, cb_param->frm_num);

	if (cb_param->err != 0) {
		pr_info("%s: [ERROR] cb(%p) error(%d) for frm(%d/%d)",
			__func__, cb_param, cb_param->err, cb_param->frm_idx, cb_param->frm_num);
		if (is_stream_off == 0) {
			pipe = (struct mtk_imgsys_pipe *)cb_param->frm_info->pipe;
			if (pipe->streaming) {
				imgsys_cmdq_cmd_dump(cb_param->frm_info, cb_param->frm_idx);
				if (cb_param->user_cmdq_err_cb) {
					struct cmdq_cb_data user_cb_data;

					user_cb_data.err = cb_param->err;
					user_cb_data.data = (void *)cb_param->frm_info;
					cb_param->user_cmdq_err_cb(
						user_cb_data, cb_param->frm_idx);
				}
			} else {
				is_stream_off = 1;
				pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
					__func__, cb_param, is_stream_off);
			}
		} else
			pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)!\n",
				__func__, cb_param, is_stream_off);
	}

	cb_param->cmdqTs.tsCmdqCbEnd = ktime_get_boottime_ns()/1000;

	INIT_WORK(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work);
	queue_work(imgsys_cmdq_wq, &cb_param->cmdq_cb_work);
}

int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
				uint32_t fail_subfidx))
{
	struct cmdq_client *clt = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 blk_idx = 0; /* For Vss block cnt */
	u32 thd_idx = 0;
	u32 hw_comb = 0;
	int ret = 0, ret_flush = 0;
	u64 tsReqStart = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;
	u32 frm_num = 0, frm_idx = 0;

	/* PMQOS API */
	tsDvfsQosStart = ktime_get_boottime_ns()/1000;
	IMGSYS_SYSTRACE_BEGIN("%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx\n",
		__func__, "dvfs_qos", frm_info->frame_no, frm_info->request_no,
		frm_info->request_fd, frm_info->frm_owner);
	mutex_lock(&(imgsys_dev->dvfs_qos_lock));
	#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_mmqos_cal(imgsys_dev, frm_info, 1);
	mtk_imgsys_mmdvfs_set(imgsys_dev, frm_info, 1);
	mtk_imgsys_mmqos_set(imgsys_dev, frm_info, 1);
	#endif
	mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
	IMGSYS_SYSTRACE_END();
	tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

	is_stream_off = 0;
	frm_num = frm_info->total_frmnum;
	for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
		cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;
		cmd_num = cmd_buf->curr_length / sizeof(struct Command);
		cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
			(unsigned long)(cmd_buf->cmd_offset));
		hw_comb = frm_info->user_info[frm_idx].hw_comb;
		/* Choose cmdq_client base on hw scenario */
		for (thd_idx = 0; thd_idx < IMGSYS_ENG_MAX; thd_idx++) {
			if (hw_comb & 0x1) {
				clt = imgsys_clt[thd_idx];
				pr_debug(
				"%s: chosen mbox thread (%d, 0x%x) for frm(%d/%d)\n",
					__func__, thd_idx, clt, frm_idx, frm_num);
				break;
			}
			hw_comb = hw_comb>>1;
		}
		/*	This segment can be removed since user had set dependency	*/
		if (frm_info->user_info[frm_idx].hw_comb & IMGSYS_ENG_DIP) {
			thd_idx = 4;
			clt = imgsys_clt[thd_idx];
		}

		/* This is work around for low latency flow.		*/
		/* If we change to request base,			*/
		/* we don't have to take this condition into account.	*/
		if (frm_info->sync_id != -1)
			clt = imgsys_clt[0];

		if (clt == NULL) {
			pr_info("%s: [ERROR] No HW Found (0x%x) for frm(%d/%d)!\n",
				__func__, frm_info->user_info[frm_idx].hw_comb, frm_idx, frm_num);
			return -1;
		}

		dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) frm(%d/%d) cmd_oft(0x%x), cmd_len(%d), num(%d), sz_per_cmd(%d), frm_blk(%d), hw_comb(0x%x), sync_id(%d), gce_thd(%d), gce_clt(0x%x)\n",
			__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
			frm_idx, frm_num, cmd_buf->cmd_offset, cmd_buf->curr_length,
			cmd_num, sizeof(struct Command), cmd_buf->frame_block,
			frm_info->user_info[frm_idx].hw_comb, frm_info->sync_id, thd_idx, clt);

		cmd_idx = 0;
		for (blk_idx = 0; blk_idx < cmd_buf->frame_block; blk_idx++) {
			tsReqStart = ktime_get_boottime_ns()/1000;
			/* create pkt and hook clt as pkt's private data */
			pkt = cmdq_pkt_create(clt);
			if (pkt == NULL) {
				pr_info(
				"%s: [ERROR] cmdq_pkt_create fail in block(%d)!\n",
					__func__, blk_idx);
				return -1;
			}
			pr_debug(
			"%s: cmdq_pkt_create success(0x%x) in block(%d) for frm(%d/%d)\n",
				__func__, pkt, blk_idx, frm_idx, frm_num);

			/* Assign task priority according to is_time_shared */
			if (frm_info->user_info[frm_idx].is_time_shared)
				pkt->priority = IMGSYS_PRI_LOW;
			else
				pkt->priority = IMGSYS_PRI_HIGH;

			IMGSYS_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx frm(%d/%d) blk(%d)\n",
				__func__, "cmd_parser", frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_info->user_info[frm_idx].subfrm_idx,
				frm_info->user_info[frm_idx].hw_comb, frm_info->frm_owner,
				frm_idx, frm_num, blk_idx);
			do {
				pr_debug(
				"%s: parsing idx(%d) with cmd(%d)\n", __func__, cmd_idx,
					cmd[cmd_idx].opcode);
				ret = imgsys_cmdq_parser(pkt, &cmd[cmd_idx]);
				cmd_idx++;
				if (ret == IMGSYS_CMD_STOP)
					break;
			} while (cmd_idx < cmd_num);
			IMGSYS_SYSTRACE_END();

			/* Prepare cb param */
			cb_param =
				vzalloc(sizeof(struct mtk_imgsys_cb_param));
			if (cb_param == NULL) {
				cmdq_pkt_destroy(pkt);
				return -1;
			}
			dev_dbg(imgsys_dev->dev,
			"%s: cb_param kzalloc success cb(%p) in block(%d) for frm(%d/%d)!\n",
				__func__, cb_param, blk_idx, frm_idx, frm_num);

			cb_param->pkt = pkt;
			cb_param->frm_info = frm_info;
			cb_param->frm_idx = frm_idx;
			cb_param->frm_num = frm_num;
			cb_param->user_cmdq_cb = cmdq_cb;
			cb_param->user_cmdq_err_cb = cmdq_err_cb;
			if ((blk_idx + 1) == cmd_buf->frame_block)
				cb_param->isBlkLast = 1;
			else
				cb_param->isBlkLast = 0;
			if ((frm_idx + 1) == frm_num)
				cb_param->isFrmLast = 1;
			else
				cb_param->isFrmLast = 0;
			cb_param->cmdqTs.tsReqStart = tsReqStart;
			cb_param->cmdqTs.tsDvfsQosStart = tsDvfsQosStart;
			cb_param->cmdqTs.tsDvfsQosEnd = tsDvfsQosEnd;
			cb_param->imgsys_dev = imgsys_dev;

			/* flush synchronized, block API */
			cb_param->cmdqTs.tsFlushStart = ktime_get_boottime_ns()/1000;
			IMGSYS_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb(%p) frm(%d/%d) blk(%d)\n",
				__func__, "pkt_flush", frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_info->user_info[frm_idx].subfrm_idx,
				frm_info->user_info[frm_idx].hw_comb, frm_info->frm_owner,
				cb_param, frm_idx, frm_num, blk_idx);

			ret_flush = cmdq_pkt_flush_async(pkt, imgsys_cmdq_task_cb,
								(void *)cb_param);
			IMGSYS_SYSTRACE_END();
			if (ret_flush < 0)
				pr_info(
				"%s: [ERROR] cmdq_pkt_flush_async fail(%d) for frm(%d/%d)!\n",
					__func__, ret_flush, frm_idx, frm_num);
			else
				pr_debug(
				"%s: cmdq_pkt_flush_async success(%d), blk(%d), frm(%d/%d)!\n",
					__func__, ret_flush, blk_idx, frm_idx, frm_num);
		}
	}

	return ret;
}

int imgsys_cmdq_parser(struct cmdq_pkt *pkt, struct Command *cmd)
{
	pr_debug("%s: +, cmd(%d)\n", __func__, cmd->opcode);

	switch (cmd->opcode) {
	case IMGSYS_CMD_READ:
		pr_debug(
		"%s: READ with source(0x%08lx) target(0x%08x) mask(0x%08x)\n", __func__,
						cmd->u.source, cmd->u.target, cmd->u.mask);
		cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.source,
			(dma_addr_t)cmd->u.target, CMDQ_THR_SPR_IDX3);
		break;
	case IMGSYS_CMD_WRITE:
		pr_debug(
		"%s: WRITE with addr(0x%08lx) value(0x%08x) mask(0x%08x)\n", __func__,
						cmd->u.address, cmd->u.value, cmd->u.mask);
		cmdq_pkt_write(pkt, NULL, (dma_addr_t)cmd->u.address,
						cmd->u.value, cmd->u.mask);
		break;
	case IMGSYS_CMD_POLL:
		pr_info(
		"%s: POLL with addr(0x%08lx) value(0x%08x) mask(0x%08x)\n", __func__,
						cmd->u.address, cmd->u.value, cmd->u.mask);
		cmdq_pkt_poll(pkt, NULL, cmd->u.value, cmd->u.address, cmd->u.mask, 1);
		break;
	case IMGSYS_CMD_WAIT:
		pr_debug(
		"%s: WAIT event(%d/%d) action(%d)\n", __func__,
			cmd->u.event, imgsys_event[cmd->u.event].event,	cmd->u.action);
		if (cmd->u.action == 1)
			cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event);
		else if (cmd->u.action == 0)
			cmdq_pkt_wait_no_clear(pkt, imgsys_event[cmd->u.event].event);
		else
			pr_info("%s: [ERROR]Not Support wait action(%d)!\n", __func__, cmd->u.action);
		break;
	case IMGSYS_CMD_UPDATE:
		pr_debug(
		"%s: UPDATE event(%d/%d) action(%d)\n", __func__,
			cmd->u.event, imgsys_event[cmd->u.event].event, cmd->u.action);
		if (cmd->u.action == 1)
			cmdq_pkt_set_event(pkt, imgsys_event[cmd->u.event].event);
		else if (cmd->u.action == 0)
			cmdq_pkt_clear_event(pkt, imgsys_event[cmd->u.event].event);
		else
			pr_info("%s: [ERROR]Not Support update action(%d)!\n", __func__, cmd->u.action);
		break;
	case IMGSYS_CMD_ACQUIRE:
		pr_debug(
		"%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
			cmd->u.event, imgsys_event[cmd->u.event].event, cmd->u.action);
		cmdq_pkt_acquire_event(pkt, imgsys_event[cmd->u.event].event);
		break;
	case IMGSYS_CMD_STOP:
		pr_debug("%s: End Of Cmd!\n", __func__);
		return IMGSYS_CMD_STOP;
	default:
		pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd->opcode);
		return -1;
	}

	return 0;
}

void imgsys_cmdq_setevent(u64 u_id)
{
	u32 event_id = 0L, event_val = 0L;

	event_id = IMGSYS_CMDQ_SYNC_TOKEN_CAMSYS_POOL_1 + (u_id % 10);
	event_val = cmdq_get_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);

	if (event_val == 0) {
		cmdq_set_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);
		pr_debug("%s: SetEvent success with (u_id/event_id/event_val)=(%d/%d/%d)!\n",
			__func__, u_id, event_id, event_val);
	} else {
		pr_info("%s: [ERROR]SetEvent fail with (u_id/event_id/event_val)=(%d/%d/%d)!\n",
			__func__, u_id, event_id, event_val);
	}
}

void mtk_imgsys_mmdvfs_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	u64 freq = 0;
	int ret = 0, opp_num = 0, opp_idx = 0, idx = 0, volt;
	struct device_node *np, *child_np = NULL;
	struct of_phandle_iterator it;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_imgsys_dvfs));
	dvfs_info->dev = imgsys_dev->dev;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_dbg(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get(dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(dvfs_info->reg)) {
		dev_dbg(dvfs_info->dev, "can't get dvfsrc-vcore\n");
		return;
	}

	opp_num = regulator_count_voltages(dvfs_info->reg);
	of_for_each_phandle(
		&it, ret, dvfs_info->dev->of_node, "operating-points-v2", NULL, 0) {
		np = of_node_get(it.node);
		if (!np) {
			dev_dbg(dvfs_info->dev, "of_node_get fail\n");
			return;
		}

		do {
			child_np = of_get_next_available_child(np, child_np);
			if (child_np) {
				of_property_read_u64(child_np, "opp-hz", &freq);
				dvfs_info->clklv[opp_idx][idx] = freq;
				of_property_read_u32(child_np, "opp-microvolt", &volt);
				dvfs_info->voltlv[opp_idx][idx] = volt;
				idx++;
			}
		} while (child_np);
		dvfs_info->clklv_num[opp_idx] = idx;
		dvfs_info->clklv_target[opp_idx] = dvfs_info->clklv[opp_idx][0];
		dvfs_info->clklv_idx[opp_idx] = 0;
		idx = 0;
		opp_idx++;
		of_node_put(np);
	}

	opp_num = opp_idx;
	for (opp_idx = 0; opp_idx < opp_num; opp_idx++) {
		for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
			dev_info(dvfs_info->dev, "[%s] opp=%d, idx=%d, clk=%d volt=%d\n",
				__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
				dvfs_info->voltlv[opp_idx][idx]);
		}
	}
	dvfs_info->cur_volt = 0;
	dvfs_info->ts_end = 0;

}

void mtk_imgsys_mmdvfs_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	int volt = 0, ret = 0;

	dev_info(dvfs_info->dev, "[%s]\n", __func__);

	dvfs_info->cur_volt = volt;

	ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);

}

void mtk_imgsys_mmdvfs_set(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	int volt = 0, ret = 0, idx = 0, opp_idx = 0;
	unsigned long freq = 0;
	u32 hw_comb = frm_info->user_info[0].hw_comb;

	freq = dvfs_info->freq;

	/* Choose for IPESYS */
	if (hw_comb & IMGSYS_ENG_ME)
		opp_idx = 1;

	for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
		if (freq <= dvfs_info->clklv[opp_idx][idx])
			break;
	}
	if (idx == dvfs_info->clklv_num[opp_idx])
		idx--;
	volt = dvfs_info->voltlv[opp_idx][idx];

	if (dvfs_info->cur_volt != volt) {
		dev_dbg(dvfs_info->dev, "[%s] volt change opp=%d, idx=%d, clk=%d volt=%d\n",
			__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
			dvfs_info->voltlv[opp_idx][idx]);
		ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);
		dvfs_info->cur_volt = volt;
	}
}

void mtk_imgsys_mmqos_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	//struct icc_path *path;
	int idx = 0;

	memset((void *)qos_info, 0x0, sizeof(struct mtk_imgsys_qos));
	qos_info->dev = imgsys_dev->dev;
	qos_info->qos_path = imgsys_qos_path;

	for (idx = 0; idx < IMGSYS_M4U_PORT_MAX; idx++) {
		qos_info->qos_path[idx].path =
			of_mtk_icc_get(qos_info->dev, qos_info->qos_path[idx].dts_name);
		qos_info->qos_path[idx].bw = 0;
		dev_info(qos_info->dev, "[%s] idx=%d, path=%p, name=%s, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].dts_name,
			qos_info->qos_path[idx].bw);
	}
}

void mtk_imgsys_mmqos_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	int idx = 0;

	for (idx = 0; idx < IMGSYS_M4U_PORT_MAX; idx++) {
		if (qos_info->qos_path[idx].path == NULL) {
			dev_info(qos_info->dev, "[%s] path of idx(%d) is NULL\n", __func__, idx);
			continue;
		}
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].bw);
		qos_info->qos_path[idx].bw = 0;
		mtk_icc_set_bw(qos_info->qos_path[idx].path, 0, 0);
	}
}

void mtk_imgsys_mmqos_set(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	u32 hw_comb = 0;
	u32 port_st = 0, port_num = 0, port_idx = 0;
	u32 frm_num = 0, frm_idx = 0;
	u32 bw;

	bw = 0;
	frm_num = frm_info->total_frmnum;
	for (frm_idx = 0; frm_idx < frm_num; frm_idx++)
		hw_comb |= frm_info->user_info[frm_idx].hw_comb;

	#if IMGSYS_QOS_ENABLE
	if (hw_comb & IMGSYS_ENG_WPE_EIS) {
		port_st = IMGSYS_M4U_PORT_WPE_EIS_START;
		port_num = WPE_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_WPE_TNR) {
		port_st = IMGSYS_M4U_PORT_WPE_TNR_START;
		port_num = WPE_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_TRAW) {
		port_st = IMGSYS_M4U_PORT_TRAW_START;
		port_num = TRAW_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_LTR) {
		port_st = IMGSYS_M4U_PORT_LTRAW_START;
		port_num = LTRAW_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_DIP) {
		port_st = IMGSYS_M4U_PORT_DIP_START;
		port_num = DIP_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_PQDIP_A) {
		port_st = IMGSYS_M4U_PORT_PQDIP_A_START;
		port_num = PQ_DIP_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_PQDIP_B) {
		port_st = IMGSYS_M4U_PORT_PQDIP_B_START;
		port_num = PQ_DIP_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	if (hw_comb & IMGSYS_ENG_ME) {
		port_st = IMGSYS_M4U_PORT_ME_START;
		port_num = ME_SMI_PORT_NUM;
		for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
			if (qos_info->qos_path[port_idx].path == NULL) {
				dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
					__func__, port_idx);
				continue;
			}
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				Bps_to_icc(qos_info->qos_path[port_idx].bw),
				0);
		}
	}
	#else
	bw = 10240;
	port_st = 0;
	port_num = IMGSYS_M4U_PORT_MAX;
	for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
		if (qos_info->qos_path[port_idx].path == NULL) {
			dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
				__func__, port_idx);
			continue;
		}
		if (qos_info->qos_path[port_idx].bw != bw) {
			dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, port_idx,
				qos_info->qos_path[port_idx].path,
				qos_info->qos_path[port_idx].bw, bw);
			qos_info->qos_path[port_idx].bw = bw;
			mtk_icc_set_bw(
				qos_info->qos_path[port_idx].path,
				MBps_to_icc(qos_info->qos_path[port_idx].bw),
				MBps_to_icc(qos_info->qos_path[port_idx].bw));
		}
	}
	#endif
}

void mtk_imgsys_mmdvfs_mmqos_cal(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_dvfs *dvfs_info = NULL;
	struct mtk_imgsys_qos *qos_info = NULL;
	unsigned long pixel_size = 0;
	int frm_num = 0, frm_idx = 0;
	u32 hw_comb = 0;
	u32 fps = 0;
	u32 bw_exe = 0;
	unsigned long freq = 0;
	#if IMGSYS_DVFS_ENABLE
	struct timeval curr_time;
	u64 ts_curr = 0, ts_eq = 0, ts_sw = 0, ts_end = 0, ts_exe = 0, ts_hw = 0, ts_fps = 0;
	#endif
	#if IMGSYS_QOS_ENABLE
	struct frame_bw_t *bw_buf = NULL;
	struct smi_port_t *smi_port = NULL;
	u32 port_st = 0, port_num = 0, port_idx = 0;
	#endif

	dvfs_info = &imgsys_dev->dvfs_info;
	qos_info = &imgsys_dev->qos_info;
	frm_num = frm_info->total_frmnum;
	fps = frm_info->fps;
	bw_exe = fps;
	hw_comb = frm_info->user_info[0].hw_comb;

	/* Calculate DVFS*/
	for (frm_idx = 0; frm_idx < frm_num; frm_idx++)
		pixel_size += frm_info->user_info[frm_idx].pixel_bw;
	#if IMGSYS_DVFS_ENABLE
	if (isSet == 1) {
		dvfs_info->pixel_size += pixel_size;

		/* Check current time */
		/* do_gettimeofday(&curr_time); */
		ts_curr = curr_time.tv_sec * 1000000 + curr_time.tv_usec;
		ts_eq = frm_info->eqtime.tv_sec * 1000000 + frm_info->eqtime.tv_usec;
		ts_sw = ts_curr - ts_eq;
		ts_fps = 1000000 / fps;
		if (ts_fps > ts_sw)
			ts_hw = ts_fps - ts_sw;
		else
			ts_hw = 0;

		if ((fps == 0) || (ts_hw == 0)) {
			freq = 650000000; /* Forcing highest frequency if fps is 0 */
			ts_exe = (pixel_size * 1000000) / freq;
			ts_end = ts_curr + ts_exe;
			bw_exe = 1000000 / ts_exe;
		} else {
			freq = (dvfs_info->pixel_size * 1000000) / ts_hw;
			/* freq = (dvfs_info->pixel_size * fps); */
			ts_exe = ts_hw;
			/* ts_exe = (dvfs_info->pixel_size * 1000000) / freq; */
			ts_end = ts_curr + ts_exe;
			bw_exe = fps;
		}

		if (freq > dvfs_info->freq)
			dvfs_info->freq = freq;
		if (ts_end > dvfs_info->ts_end)
			dvfs_info->ts_end = ts_end;
	} else if (isSet == 0) {
		dvfs_info->pixel_size -= pixel_size;

		if (fps == 0) {
			freq = 650000000; /* Forcing highest frequency if fps is 0 */
			ts_exe = (pixel_size * 1000000) / freq;
			bw_exe = 1000000 / ts_exe;
		} else
			bw_exe = fps;

		if (dvfs_info->pixel_size == 0) {
			freq = 0;
			dvfs_info->freq = freq;
		}
	}
	dev_dbg(qos_info->dev,
	"[%s] isSet(%d) fps(%d) bw_exe(%d) freq(%d/%d) pix_sz(%d/%d) eq(%lld) curr(%lld) sw(%lld) end(%lld) exe(%lld)\n",
		__func__, isSet, fps, bw_exe, freq, dvfs_info->freq,
		pixel_size, dvfs_info->pixel_size,
		ts_eq, ts_curr, ts_sw, ts_end, ts_exe);
	#else
	if (isSet == 1) {
		dvfs_info->pixel_size += pixel_size;
		freq = 650000000;
		dvfs_info->freq = freq;
	} else if (isSet == 0) {
		dvfs_info->pixel_size -= pixel_size;
		if (dvfs_info->pixel_size == 0) {
			freq = 0;
			dvfs_info->freq = freq;
		}
	}
	#endif

	/* Calculate QOS*/
	#if IMGSYS_QOS_ENABLE
	for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
		hw_comb = frm_info->user_info[frm_idx].hw_comb;
		bw_buf = (struct frame_bw_t *)frm_info->user_info[frm_idx].bw_swbuf;
		//pixel_size += frm_info->user_info[frm_idx].pixel_bw;
		if (hw_comb & IMGSYS_ENG_WPE_EIS) {
			port_st = IMGSYS_M4U_PORT_WPE_EIS_START;
			port_num = WPE_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->wpe_eis.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] WPE_EIS idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_WPE_TNR) {
			port_st = IMGSYS_M4U_PORT_WPE_TNR_START;
			port_num = WPE_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->wpe_tnr.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] WPE_TNR idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_TRAW) {
			port_st = IMGSYS_M4U_PORT_TRAW_START;
			port_num = TRAW_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->traw.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] TRAW idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_LTR) {
			port_st = IMGSYS_M4U_PORT_LTRAW_START;
			port_num = LTRAW_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->ltraw.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] LTRAW idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_DIP) {
			port_st = IMGSYS_M4U_PORT_DIP_START;
			port_num = DIP_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->dip.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] DIP idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_PQDIP_A) {
			port_st = IMGSYS_M4U_PORT_PQDIP_A_START;
			port_num = PQ_DIP_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->pqdip_a.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] PQDIP_A idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						(smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						(smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_PQDIP_B) {
			port_st = IMGSYS_M4U_PORT_PQDIP_B_START;
			port_num = PQ_DIP_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->pqdip_b.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] PQDIP_B idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						 (smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						 (smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
		if (hw_comb & IMGSYS_ENG_ME) {
			port_st = IMGSYS_M4U_PORT_ME_START;
			port_num = ME_SMI_PORT_NUM;
			smi_port = (struct smi_port_t *)bw_buf->me.smiport;
			for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++) {
				if (isSet == 1) {
					dev_dbg(qos_info->dev,
						"[%s] ME idx(%d) fps(%d) bw_exe(%d) bw(%d/%d)\n",
						__func__, port_idx, fps, bw_exe,
						smi_port[port_idx-port_st].portbw,
						qos_info->qos_path[port_idx].bw);
					qos_info->qos_path[port_idx].bw +=
						 (smi_port[port_idx-port_st].portbw * bw_exe);
				} else if (isSet == 0)
					qos_info->qos_path[port_idx].bw -=
						 (smi_port[port_idx-port_st].portbw * bw_exe);
			}
		}
	}
	#endif

}

