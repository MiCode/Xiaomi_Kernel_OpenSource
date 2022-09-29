// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <dt-bindings/interconnect/mtk,mmqos.h>
//#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-plat.h"
#include "mtk_imgsys-cmdq-plat_def.h"
#include "mtk_imgsys-cmdq-ext.h"
#include "mtk_imgsys-trace.h"
#include "mtk-interconnect.h"

#if DVFS_QOS_READY
#include "mtk-smi-dbg.h"
#endif

#if IMGSYS_SECURE_ENABLE
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#endif

#define WPE_BWLOG_HW_COMB (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP)
#define WPE_BWLOG_HW_COMB_ninA (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A)
#define WPE_BWLOG_HW_COMB_ninB (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_B)
#define WPE_BWLOG_HW_COMB_ninC (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW)
#define WPE_BWLOG_HW_COMB_ninD (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_LTR)

#define IMGSYS_QOS_SYNC_OWNER	(0x412d454d5f53)
#define IMGSYS_QOS_MAX_PERF	(MTK_MMQOS_MAX_SMI_FREQ_BW >> 1)

#if CMDQ_CB_KTHREAD
static struct kthread_worker imgsys_cmdq_worker;
static struct task_struct *imgsys_cmdq_kworker_task;
#else
static struct workqueue_struct *imgsys_cmdq_wq;
#endif
static u32 is_stream_off;
#if IMGSYS_SECURE_ENABLE
static u32 is_sec_task_create;
#endif
static struct imgsys_event_history event_hist[IMGSYS_CMDQ_SYNC_POOL_NUM];

enum SMI_MONITOR_STATE {
	SMI_MONITOR_IDLE_STATE = 0,
	SMI_MONITOR_START_STATE,
	SMI_MONITOR_ACQUIRE_STATE,
	SMI_MONITOR_STOP_STATE,
};

enum {
	SMI_COMMON_ID_31 = 0,
	SMI_COMMON_ID_32
};

enum {
	SMI_READ = 0,
	SMI_WRITE
};


void imgsys_cmdq_init_plat7s(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	u32 idx = 0;

	pr_info("%s: +, dev(0x%x), num(%d)\n", __func__, dev, nr_imgsys_dev);

	/* Only first user has to do init work queue */
	if (nr_imgsys_dev == 1) {
#if CMDQ_CB_KTHREAD
		kthread_init_worker(&imgsys_cmdq_worker);
		imgsys_cmdq_kworker_task = kthread_run(kthread_worker_fn,
			&imgsys_cmdq_worker, "imgsys-cmdqcb");
		if (IS_ERR(imgsys_cmdq_kworker_task)) {
			dev_info(dev, "%s: failed to start imgsys_cmdqcb kthread worker\n",
				__func__);
			imgsys_cmdq_kworker_task = NULL;
		} else
			sched_set_normal(imgsys_cmdq_kworker_task, -20);
#else
		imgsys_cmdq_wq = alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE | WQ_HIGHPRI,
			"imgsys_cmdq_cb_wq");
		if (!imgsys_cmdq_wq)
			pr_info("%s: Create workquque IMGSYS-CMDQ fail!\n",
				__func__);
#endif
	}

	switch (nr_imgsys_dev) {
	case 1: /* DIP */
		/* request thread by index (in dts) 0 */
		for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
			imgsys_clt[idx] = cmdq_mbox_create(dev, idx);
			pr_info("%s: cmdq_mbox_create(%d, 0x%x)\n", __func__, idx, imgsys_clt[idx]);
		}
		#if IMGSYS_SECURE_ENABLE
		/* request for imgsys secure gce thread */
		for (idx = IMGSYS_NOR_THD; idx < (IMGSYS_NOR_THD + IMGSYS_SEC_THD); idx++) {
			imgsys_sec_clt[idx-IMGSYS_NOR_THD] = cmdq_mbox_create(dev, idx);
			pr_info(
				"%s: cmdq_mbox_create sec_thd(%d, 0x%x)\n",
				__func__, idx, imgsys_sec_clt[idx-IMGSYS_NOR_THD]);
		}
		#endif
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
	mutex_init(&imgsys_dev->power_ctrl_lock);
}

void imgsys_cmdq_release_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	pr_info("%s: +\n", __func__);

	/* Destroy cmdq client */
	for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
		cmdq_mbox_destroy(imgsys_clt[idx]);
		imgsys_clt[idx] = NULL;
	}
	#if IMGSYS_SECURE_ENABLE
	for (idx = 0; idx < IMGSYS_SEC_THD; idx++) {
		cmdq_mbox_destroy(imgsys_sec_clt[idx]);
		imgsys_sec_clt[idx] = NULL;
	}
	#endif

	/* Release work_quque */
#if CMDQ_CB_KTHREAD
	if (imgsys_cmdq_kworker_task)
		kthread_stop(imgsys_cmdq_kworker_task);
#else
	flush_workqueue(imgsys_cmdq_wq);
	destroy_workqueue(imgsys_cmdq_wq);
	imgsys_cmdq_wq = NULL;
#endif
	mutex_destroy(&imgsys_dev->dvfs_qos_lock);
	mutex_destroy(&imgsys_dev->power_ctrl_lock);
}

void imgsys_cmdq_streamon_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	dev_info(imgsys_dev->dev,
		"%s: cmdq stream on (%d) quick_pwr(%d)\n",
		__func__, is_stream_off, imgsys_quick_onoff_enable_plat7s());
	is_stream_off = 0;

	cmdq_mbox_enable(imgsys_clt[0]->chan);

	for (idx = IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_START;
		idx <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END; idx++)
		cmdq_clear_event(imgsys_clt[0]->chan, imgsys_event[idx].event);

	memset((void *)event_hist, 0x0,
		sizeof(struct imgsys_event_history)*IMGSYS_CMDQ_SYNC_POOL_NUM);
#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_reset_plat7s(imgsys_dev);
	mtk_imgsys_mmqos_reset_plat7s(imgsys_dev);
	mtk_imgsys_mmqos_monitor_plat7s(imgsys_dev, SMI_MONITOR_START_STATE);
#endif
}

void imgsys_cmdq_streamoff_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	dev_info(imgsys_dev->dev,
		"%s: cmdq stream off (%d) idx(%d) quick_pwr(%d)\n",
		__func__, is_stream_off, idx, imgsys_quick_onoff_enable_plat7s());
	is_stream_off = 1;

	#if CMDQ_STOP_FUNC
	for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
		cmdq_mbox_stop(imgsys_clt[idx]);
		dev_dbg(imgsys_dev->dev,
			"%s: calling cmdq_mbox_stop(%d, 0x%x)\n",
			__func__, idx, imgsys_clt[idx]);
	}
	#endif
	#if IMGSYS_SECURE_ENABLE
	if (is_sec_task_create) {
		cmdq_sec_mbox_stop(imgsys_sec_clt[0]);
		/* cmdq_pkt_destroy(pkt_sec); */
		/* pkt_sec = NULL; */
		is_sec_task_create = 0;
	}
	#endif

	cmdq_mbox_disable(imgsys_clt[0]->chan);

	#if DVFS_QOS_READY
	mtk_imgsys_mmqos_reset_plat7s(imgsys_dev);
	mtk_imgsys_mmqos_monitor_plat7s(imgsys_dev, SMI_MONITOR_STOP_STATE);
	#endif
}

static void imgsys_cmdq_cmd_dump_plat7s(struct swfrm_info_t *frm_info, u32 frm_idx)
{
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 cmd_ofst = 0;

	cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;
	cmd_num = cmd_buf->curr_length / sizeof(struct Command);
	cmd_ofst = sizeof(struct GCERecoder);

	pr_info(
	"%s: +, req fd/no(%d/%d) frame no(%d) frm(%d/%d), cmd_oft(0x%x/0x%x), cmd_len(%d), num(%d), sz_per_cmd(%d), frm_blk(%d), hw_comb(0x%x)\n",
		__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
		frm_idx, frm_info->total_frmnum, cmd_buf->cmd_offset, cmd_ofst,
		cmd_buf->curr_length, cmd_num, sizeof(struct Command), cmd_buf->frame_block,
		frm_info->user_info[frm_idx].hw_comb);

	if (cmd_ofst != cmd_buf->cmd_offset) {
		pr_info("%s: [ERROR]cmd offset is not match (0x%x/0x%x)!\n",
			__func__, cmd_buf->cmd_offset, cmd_ofst);
		return;
	}

	cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
		(unsigned long)(cmd_buf->cmd_offset));

	for (cmd_idx = 0; cmd_idx < cmd_num; cmd_idx++) {
		switch (cmd[cmd_idx].opcode) {
		case IMGSYS_CMD_READ:
			pr_info(
			"%s: READ with source(0x%08lx) target(0x%08lx) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.source, cmd[cmd_idx].u.target, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WRITE:
			pr_debug(
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
		case IMGSYS_CMD_TIME:
			pr_info("%s: Get cmdq TIME stamp\n", __func__);
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

static void imgsys_cmdq_fence_done_plat7s(struct dma_fence *fence, struct dma_fence_cb *cb)
{

	u32 event_id = 0L, event_val = 0L, thd_idx = 0L;
	struct mtk_imgsys_fence *imgsys_fence = NULL;

	imgsys_fence = container_of(cb, struct mtk_imgsys_fence, cb);
	thd_idx = imgsys_fence->thd_idx;
	event_id = imgsys_fence->event_id;

	if (is_stream_off == 1) {
		pr_info("%s: [ERROR] pipe had been turned off(%d)! with fence:0x%x,fencefd:%d,gthrd:%d,event:%d\n",
			__func__, is_stream_off, fence, imgsys_fence->fence_fd,
			thd_idx, event_id);
		return;
	}

	event_val = cmdq_get_event(imgsys_clt[thd_idx]->chan, imgsys_event[event_id].event);

	if (event_val == 0) {
		if (imgsys_fence_dbg_enable_plat7s()) {
			pr_info("%s: +SetEvent with fence:0x%x,fencefd:%d,gthrd:%d,event:%d,event_val:%d\n",
				__func__, fence, imgsys_fence->fence_fd, thd_idx,
				event_id, event_val);
		}
		cmdq_set_event(imgsys_clt[thd_idx]->chan, imgsys_event[event_id].event);
		if (imgsys_fence_dbg_enable_plat7s()) {
			pr_info("%s: -SetEvent success with fence:0x%x,fencefd:%d,gthrd:%d,event:%d,event_val:%d\n",
				__func__, fence, imgsys_fence->fence_fd, thd_idx,
				event_id, event_val);
		}
	} else {
		pr_info("%s: [ERROR]SetEvent fail with fence:0x%x,fencefd:%d,gthrd:%d,event:%d,event_val:%d\n",
			__func__, fence, imgsys_fence->fence_fd, thd_idx, event_id, event_val);
	}

}

static void imgsys_cmdq_fence_set_plat7s(struct mtk_imgsys_dev *imgsys_dev,
					struct swfrm_info_t *frm_info,
					struct mtk_imgsys_cb_param *cb_param,
					u32 frm_idx, u32 thd_idx)
{
	u32 f_lop_idx = 0;
	u32 waitf_num = 0, notif_num = 0;
	struct fence_event *fence_evt = NULL;
	struct mtk_imgsys_fence *imgsys_fence = NULL;

	pr_debug("%s: +\n", __func__);

	notif_num = frm_info->user_info[frm_idx].notify_fence_num;
	if (notif_num <= 0)
		goto CHECK_WAIT_FENCE;

	for (f_lop_idx = 0; f_lop_idx < notif_num; f_lop_idx++) {
		fence_evt = &(frm_info->user_info[frm_idx].notify_fence_list[f_lop_idx]);
		imgsys_fence = &(cb_param->notifence[f_lop_idx]);
		imgsys_fence->kfence = (struct dma_fence *)fence_evt->dma_fence;
		if (imgsys_fence->kfence == NULL) {
			dev_info(imgsys_dev->dev,
				"%s: [ERROR] Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[notify-#%d/%d] null fence, fd:%d\n",
				__func__, (char *)(&(frm_info->frm_owner)),
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_idx, f_lop_idx,
				fence_evt->fence_fd);
			return;
		}

		imgsys_fence->thd_idx = thd_idx;
		imgsys_fence->event_id = fence_evt->gce_event;
		imgsys_fence->fence_fd = fence_evt->fence_fd;

		if (imgsys_fence_dbg_enable_plat7s()) {
			dev_info(imgsys_dev->dev,
				"%s: Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[notify-#%d/%d]0x%x,fencefd:%d,gthrd:%d,ref_ct:%d\n",
				__func__, (char *)(&(frm_info->frm_owner)),
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->kfence, imgsys_fence->fence_fd,
				imgsys_fence->thd_idx,
				atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
		}
	}

CHECK_WAIT_FENCE:
	waitf_num = frm_info->user_info[frm_idx].wait_fence_num;
	if (waitf_num <= 0)
		return;

	for (f_lop_idx = 0; f_lop_idx < waitf_num; f_lop_idx++) {
		int ret_f = 0;

		fence_evt = &(frm_info->user_info[frm_idx].wait_fence_list[f_lop_idx]);
		imgsys_fence = &(cb_param->waitfence[f_lop_idx]);
		imgsys_fence->kfence = (struct dma_fence *)fence_evt->dma_fence;
		if (imgsys_fence->kfence == NULL) {
			dev_info(imgsys_dev->dev,
				"%s: [ERROR] Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d:[wait-#%d/%d]null fence, fd:%d\n",
				__func__, (char *)(&(frm_info->frm_owner)),
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->fence_fd);
			return;
		}

		imgsys_fence->thd_idx = thd_idx;
		imgsys_fence->event_id = fence_evt->gce_event;
		imgsys_fence->fence_fd = fence_evt->fence_fd;

		dma_fence_get(imgsys_fence->kfence);
		ret_f = dma_fence_add_callback(
							imgsys_fence->kfence,
							&(imgsys_fence->cb),
							imgsys_cmdq_fence_done_plat7s);
		if (ret_f == -ENOENT) {
			if (imgsys_fence_dbg_enable_plat7s()) {
				dev_info(imgsys_dev->dev,
					"%s: Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[wait-#%d/%d]already signaled! 0x%x,fencefd:%d,gthrd:%d,event:%d,ref_ct:%d\n",
					__func__, (char *)(&(frm_info->frm_owner)),
					frm_info->frame_no, frm_info->request_no,
					frm_info->request_fd, frm_idx, f_lop_idx,
					imgsys_fence->kfence, imgsys_fence->fence_fd,
					imgsys_fence->thd_idx, imgsys_fence->event_id,
					atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
			}
			imgsys_cmdq_fence_done_plat7s(imgsys_fence->kfence, &(imgsys_fence->cb));

		} else if (ret_f != 0) {
			dev_info(imgsys_dev->dev,
				"%s: [ERROR] Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[wait-#%d/%d]dma_fence_add_callback errno:%d\n",
				__func__, (char *)(&(frm_info->frm_owner)),
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_idx, f_lop_idx, ret_f);
				return;
		} else {
			if (imgsys_fence_dbg_enable_plat7s()) {
				dev_info(imgsys_dev->dev,
					"%s: Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[wait-#%d/%d]0x%x,fencefd:%d,gthrd:%d,event:%d,ref_ct:%d\n",
					__func__, (char *)(&(frm_info->frm_owner)),
					frm_info->frame_no, frm_info->request_no,
					frm_info->request_fd, frm_idx, f_lop_idx,
					imgsys_fence->kfence, imgsys_fence->fence_fd,
					imgsys_fence->thd_idx, imgsys_fence->event_id,
					atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
			}
		}

	}
}

static void imgsys_cmdq_fence_notify_plat7s(struct mtk_imgsys_dev *imgsys_dev,
			struct mtk_imgsys_cb_param *cb_param)
{
	u32 frm_idx = 0;
	u32 f_lop_idx = 0, notif_num = 0;
	struct mtk_imgsys_fence *imgsys_fence = NULL;

	if (cb_param->err != 0)
		return;

	frm_idx = cb_param->frm_idx;
	notif_num = cb_param->frm_info->user_info[frm_idx].notify_fence_num;
	if (notif_num <= 0)
		return;

	for (f_lop_idx = 0; f_lop_idx < notif_num; f_lop_idx++) {
		imgsys_fence = &(cb_param->notifence[f_lop_idx]);
		if (imgsys_fence->kfence == NULL) {
			dev_info(imgsys_dev->dev,
				"%s: [ERROR] Own:%s, MWFrame:#%d MWReq:#%d ReqFd:%d,[notify-#%d/%d]null fence, fd:%d\n",
				__func__, (char *)(&(cb_param->frm_info->frm_owner)),
				cb_param->frm_info->frame_no, cb_param->frm_info->request_no,
				cb_param->frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->fence_fd);
			continue;
		}

		if (imgsys_fence_dbg_enable_plat7s()) {
			dev_info(imgsys_dev->dev,
				"%s: Own:%s, MWFrame:#%d MWReq:#%d ReqFd:%d,[notify-#%d/%d]+,0x%x,fencefd:%d,gthrd:%d,ref_ct:%d\n",
				__func__, (char *)(&(cb_param->frm_info->frm_owner)),
				cb_param->frm_info->frame_no, cb_param->frm_info->request_no,
				cb_param->frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->kfence, imgsys_fence->fence_fd, imgsys_fence->thd_idx,
				atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
		}

		dma_fence_signal(imgsys_fence->kfence);
		if (imgsys_fence_dbg_enable_plat7s()) {
			dev_info(imgsys_dev->dev,
				"%s: Own:%s, MWFrame:#%d MWReq:#%d ReqFd:%d,[notify-#%d/%d]-,0x%x,fencefd:%d,gthrd:%d,ref_ct:%d\n",
				__func__, (char *)(&(cb_param->frm_info->frm_owner)),
				cb_param->frm_info->frame_no, cb_param->frm_info->request_no,
				cb_param->frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->kfence, imgsys_fence->fence_fd, imgsys_fence->thd_idx,
				atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
		}

	}
}

static void imgsys_cmdq_fence_rmcb_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct mtk_imgsys_cb_param *cb_param)
{
	u32 frm_idx = 0;
	u32 f_lop_idx = 0, waitf_num = 0;
	struct mtk_imgsys_fence *imgsys_fence = NULL;

	frm_idx = cb_param->frm_idx;
	waitf_num = cb_param->frm_info->user_info[frm_idx].wait_fence_num;
	if (waitf_num <= 0)
		return;

	for (f_lop_idx = 0; f_lop_idx < waitf_num; f_lop_idx++) {
		imgsys_fence = &(cb_param->waitfence[f_lop_idx]);
		if (imgsys_fence->kfence == NULL) {
			dev_info(imgsys_dev->dev,
				"%s: [ERROR] Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d:[wait-#%d/%d]null fence, fd:%d\n",
				__func__, (char *)(&(cb_param->frm_info->frm_owner)),
				cb_param->frm_info->frame_no, cb_param->frm_info->request_no,
				cb_param->frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->fence_fd);
			return;
		}

		dma_fence_remove_callback(
							imgsys_fence->kfence,
							&(imgsys_fence->cb));
		dma_fence_put(imgsys_fence->kfence);

		if (imgsys_fence_dbg_enable_plat7s()) {
			dev_info(imgsys_dev->dev,
				"%s: Own:%s MWFrame:#%d MWReq:#%d ReqFd:%d,[wait-#%d/%d]0x%x,fencefd:%d,gthrd:%d,event:%d,ref_ct:%d\n",
				__func__, (char *)(&(cb_param->frm_info->frm_owner)),
				cb_param->frm_info->frame_no, cb_param->frm_info->request_no,
				cb_param->frm_info->request_fd, frm_idx, f_lop_idx,
				imgsys_fence->kfence, imgsys_fence->fence_fd,
				imgsys_fence->thd_idx, imgsys_fence->event_id,
				atomic_read(&imgsys_fence->kfence->refcount.refcount.refs));
		}
	}

}

#if CMDQ_CB_KTHREAD
static void imgsys_cmdq_cb_work_plat7s(struct kthread_work *work)
#else
static void imgsys_cmdq_cb_work_plat7s(struct work_struct *work)
#endif
{
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dev *imgsys_dev = NULL;
	u32 hw_comb = 0;
	u32 cb_frm_cnt = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;
	int req_fd = 0, req_no = 0, frm_no = 0, ret_sn = 0;
	u32 tsSwEvent = 0, tsHwEvent = 0, tsHw = 0, tsTaskPending = 0;
	u32 tsHwStr = 0, tsHwEnd = 0;
	bool isLastTaskInReq = 0;
	char *wpestr = NULL;
	u32 wpebw_en = imgsys_wpe_bwlog_enable_plat7s();
	char logBuf_temp[MTK_IMGSYS_LOG_LENGTH];
	u32 idx = 0;
	u32 real_frm_idx = 0;

	pr_debug("%s: +\n", __func__);

	cb_param = container_of(work, struct mtk_imgsys_cb_param, cmdq_cb_work);
	cb_param->cmdqTs.tsCmdqCbWorkStart = ktime_get_boottime_ns()/1000;
	imgsys_dev = cb_param->imgsys_dev;

	dev_dbg(imgsys_dev->dev,
		"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)\n",
		__func__, cb_param, cb_param->group_id,
		cb_param->blk_idx,  cb_param->blk_num,
		cb_param->frm_idx, cb_param->frm_num,
		cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
		cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
		cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
		cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);

	imgsys_cmdq_fence_notify_plat7s(imgsys_dev, cb_param);
	imgsys_cmdq_fence_rmcb_plat7s(imgsys_dev, cb_param);

#ifndef CONFIG_FPGA_EARLY_PORTING
	mtk_imgsys_power_ctrl_plat7s(imgsys_dev, false);
#endif

	if (imgsys_cmdq_ts_enable_plat7s()) {
		for (idx = 0; idx < cb_param->task_cnt; idx++) {
			/* Calculating task timestamp */
			tsSwEvent = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1]
					- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0];
			tsHwEvent = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2]
					- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1];
			tsHwStr = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2];
			tsHwEnd = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3];
			tsHw = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3]
				- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2];
			tsTaskPending =
			cb_param->taskTs.dma_va[cb_param->taskTs.ofst+cb_param->taskTs.num-1]
			- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0];
			CMDQ_TICK_TO_US(tsSwEvent);
			CMDQ_TICK_TO_US(tsHwEvent);
			CMDQ_TICK_TO_US(tsHw);
			CMDQ_TICK_TO_US(tsHwStr);
			CMDQ_TICK_TO_US(tsHwEnd);
			CMDQ_TICK_TO_US(tsTaskPending);
			tsTaskPending =
				(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart)
				- tsTaskPending;
			dev_dbg(imgsys_dev->dev,
			"%s: TSus cb(%p) err(%d) frm(%d/%d/%d) hw_comb(0x%x) ts_num(%d) sw_event(%d) hw_event(%d) hw_real(%d) (%d/%d/%d/%d)\n",
				__func__, cb_param, cb_param->err, cb_param->frm_idx,
				cb_param->frm_num, cb_frm_cnt, hw_comb,
				cb_param->taskTs.num, tsSwEvent, tsHwEvent, tsHw,
				cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0],
				cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1],
				cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2],
				cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3]
			);
			/* if (imgsys_cmdq_ts_dbg_enable_plat7s()) { */
				real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + idx;
				hw_comb = cb_param->frm_info->user_info[real_frm_idx].hw_comb;
				memset((char *)logBuf_temp, 0x0, MTK_IMGSYS_LOG_LENGTH);
				logBuf_temp[strlen(logBuf_temp)] = '\0';
				ret_sn = snprintf(logBuf_temp, MTK_IMGSYS_LOG_LENGTH,
					"/[%d/%d/%d/%d]hw_comb(0x%x)ts(%d-%d-%d-%d)hw(%d-%d)",
					real_frm_idx, cb_param->frm_num,
					cb_param->blk_idx, cb_param->blk_num,
					hw_comb, tsTaskPending, tsSwEvent, tsHwEvent,
					tsHw, tsHwStr, tsHwEnd);
				if (ret_sn < 0) {
					pr_info("%s: [ERROR] snprintf fail: %d\n",
						__func__, ret_sn);
				}
				strncat(cb_param->frm_info->hw_ts_log, logBuf_temp,
						strlen(logBuf_temp));
			/* } */
		}
	}

	if (cb_param->err != 0)
		pr_info(
			"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%x) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d) earlycb(%d) ofst(0x%x) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)",
			__func__, cb_param, cb_param->req_fd,
			cb_param->req_no, cb_param->frm_no,
			cb_param->err, cb_param->group_id,
			cb_param->clt, cb_param->hw_comb,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->isBlkLast, cb_param->isFrmLast,
			cb_param->is_earlycb, cb_param->pkt->err_data.offset,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
	if (is_stream_off == 1)
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)!\n",
			__func__, cb_param, is_stream_off);

	dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) isBlkLast(%d) isFrmLast(%d) isECB(%d) isGPLast(%d) isGPECB(%d) for frm(%d/%d)\n",
		__func__, cb_param->frm_info->request_fd,
		cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
		cb_param, cb_param->frm_info, cb_param->isBlkLast,
		cb_param->isFrmLast, cb_param->is_earlycb,
		cb_param->frm_info->user_info[cb_param->frm_idx].is_lastingroup,
		cb_param->frm_info->user_info[cb_param->frm_idx].is_earlycb,
		cb_param->frm_idx, cb_param->frm_num);

	req_fd = cb_param->frm_info->request_fd;
	req_no = cb_param->frm_info->request_no;
	frm_no = cb_param->frm_info->frame_no;

	cb_param->frm_info->cb_frmcnt++;
	cb_frm_cnt = cb_param->frm_info->cb_frmcnt;

	if (wpebw_en > 0) {
		for (idx = 0; idx < cb_param->task_cnt; idx++) {
			wpestr = NULL;
			real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + idx;
			hw_comb = cb_param->frm_info->user_info[real_frm_idx].hw_comb;
			switch (wpebw_en) {
			case 1:
				if ((hw_comb & WPE_BWLOG_HW_COMB) == WPE_BWLOG_HW_COMB)
					wpestr = "tnr";
				break;
			case 2:
				if (((hw_comb & WPE_BWLOG_HW_COMB_ninA) == WPE_BWLOG_HW_COMB_ninA)
				 || ((hw_comb & WPE_BWLOG_HW_COMB_ninB) == WPE_BWLOG_HW_COMB_ninB))
					wpestr = "eis";
				break;
			case 3:
				if (((hw_comb & WPE_BWLOG_HW_COMB_ninC) == WPE_BWLOG_HW_COMB_ninC)
				 || ((hw_comb & WPE_BWLOG_HW_COMB_ninD) == WPE_BWLOG_HW_COMB_ninD))
					wpestr = "lite";
				break;
			}
			if (wpestr) {
				dev_info(imgsys_dev->dev,
					"%s: wpe_bwlog req fd/no(%d/%d)frameNo(%d)cb(%p)err(%d)frm(%d/%d/%d)hw_comb(0x%x)read_num(%d)-%s(%d/%d/%d/%d)\n",
					__func__, cb_param->frm_info->request_fd,
					cb_param->frm_info->request_no,
					cb_param->frm_info->frame_no,
					cb_param, cb_param->err, real_frm_idx,
					cb_param->frm_num, cb_frm_cnt, hw_comb,
					cb_param->taskTs.num,
					wpestr,
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+0],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+1],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+2],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+3]
					);
			}
		}
	}

	hw_comb = cb_param->frm_info->user_info[cb_param->frm_idx].hw_comb;

	dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) isBlkLast(%d) cb_param->frm_num(%d) cb_frm_cnt(%d)\n",
		__func__, cb_param->frm_info->request_fd,
		cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
		cb_param, cb_param->frm_info, cb_param->isBlkLast, cb_param->frm_num,
		cb_frm_cnt);

	if (cb_param->isBlkLast && cb_param->user_cmdq_cb &&
		((cb_param->frm_info->total_taskcnt == cb_frm_cnt) || cb_param->is_earlycb)) {
		struct cmdq_cb_data user_cb_data;

		/* PMQOS API */
		tsDvfsQosStart = ktime_get_boottime_ns()/1000;
		IMGSYS_CMDQ_SYSTRACE_BEGIN(
			"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
			__func__, "dvfs_qos", cb_param->frm_info->frame_no,
			cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
			cb_param->frm_info->frm_owner);
		/* Calling PMQOS API if last frame */
		if (cb_param->frm_info->total_taskcnt == cb_frm_cnt) {
			mutex_lock(&(imgsys_dev->dvfs_qos_lock));
			#if DVFS_QOS_READY
			mtk_imgsys_mmdvfs_mmqos_cal_plat7s(imgsys_dev, cb_param->frm_info, 0);
			mtk_imgsys_mmdvfs_set_plat7s(imgsys_dev, cb_param->frm_info, 0);
			#endif
			mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
			if (imgsys_cmdq_ts_enable_plat7s() || imgsys_wpe_bwlog_enable_plat7s()) {
				IMGSYS_CMDQ_SYSTRACE_BEGIN(
					"%s_%s|%s",
					__func__, "hw_ts_trace", cb_param->frm_info->hw_ts_log);
				cmdq_mbox_buf_free(cb_param->clt,
					cb_param->taskTs.dma_va, cb_param->taskTs.dma_pa);
				if (imgsys_cmdq_ts_dbg_enable_plat7s())
					dev_info(imgsys_dev->dev, "%s: %s",
						__func__, cb_param->frm_info->hw_ts_log);
				vfree(cb_param->frm_info->hw_ts_log);
				IMGSYS_CMDQ_SYSTRACE_END();
			}
			isLastTaskInReq = 1;
		} else
			isLastTaskInReq = 0;
		IMGSYS_CMDQ_SYSTRACE_END();
		tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

		user_cb_data.err = cb_param->err;
		user_cb_data.data = (void *)cb_param->frm_info;
		cb_param->cmdqTs.tsUserCbStart = ktime_get_boottime_ns()/1000;
		IMGSYS_CMDQ_SYSTRACE_BEGIN(
			"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
			__func__, "user_cb", cb_param->frm_info->frame_no,
			cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
			cb_param->frm_info->frm_owner);
		cb_param->user_cmdq_cb(user_cb_data, cb_param->frm_idx, isLastTaskInReq);
		IMGSYS_CMDQ_SYSTRACE_END();
		cb_param->cmdqTs.tsUserCbEnd = ktime_get_boottime_ns()/1000;
	}

	IMGSYS_CMDQ_SYSTRACE_BEGIN(
		"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb:%p thd:%d frm(%d/%d/%d) DvfsSt(%lld) SetCmd(%lld) HW(%lld/%d-%d-%d-%d) Cmdqcb(%lld) WK(%lld) UserCb(%lld) DvfsEnd(%lld)",
		__func__, "wait_pkt", cb_param->frm_info->frame_no,
		cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
		cb_param->frm_info->user_info[cb_param->frm_idx].subfrm_idx, hw_comb,
		cb_param->frm_info->frm_owner, cb_param, cb_param->thd_idx,
		cb_param->frm_idx, cb_param->frm_num, cb_frm_cnt,
		(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
		(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
		(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
		tsTaskPending, tsSwEvent, tsHwEvent, tsHw,
		(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
		(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
		(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
		(tsDvfsQosEnd-tsDvfsQosStart));

	cmdq_pkt_wait_complete(cb_param->pkt);
	cmdq_pkt_destroy(cb_param->pkt);
	cb_param->cmdqTs.tsReqEnd = ktime_get_boottime_ns()/1000;
	IMGSYS_CMDQ_SYSTRACE_END();

	if (imgsys_cmdq_ts_dbg_enable_plat7s())
		dev_dbg(imgsys_dev->dev,
			"%s: TSus req fd/no(%d/%d) frame no(%d) thd(%d) cb(%p) err(%d) frm(%d/%d/%d) hw_comb(0x%x) DvfsSt(%lld) Req(%lld) SetCmd(%lld) HW(%lld/%d-%d-%d-%d) Cmdqcb(%lld) WK(%lld) CmdqCbWk(%lld) UserCb(%lld) DvfsEnd(%lld)\n",
			__func__, req_fd, req_no, frm_no, cb_param->thd_idx,
			cb_param, cb_param->err, cb_param->frm_idx,
			cb_param->frm_num, cb_frm_cnt, hw_comb,
			(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
			(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsReqStart),
			(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
			(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
			tsTaskPending, tsSwEvent, tsHwEvent, tsHw,
			(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
			(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
			(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsCmdqCbWorkStart),
			(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
			(tsDvfsQosEnd-tsDvfsQosStart)
			);
	vfree(cb_param);
}

void imgsys_cmdq_task_cb_plat7s(struct cmdq_cb_data data)
{
	struct mtk_imgsys_cb_param *cb_param;
	struct mtk_imgsys_pipe *pipe;
	size_t err_ofst;
	u32 idx = 0, err_idx = 0, real_frm_idx = 0;
	u16 event = 0, event_sft = 0;
	u64 event_diff = 0;
	bool isHWhang = 0;

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
		err_ofst = cb_param->pkt->err_data.offset;
		err_idx = 0;
		for (idx = 0; idx < cb_param->task_cnt; idx++)
			if (err_ofst > cb_param->pkt_ofst[idx])
				err_idx++;
			else
				break;
		if (err_idx >= cb_param->task_cnt) {
			pr_info(
				"%s: [ERROR] can't find task in task list! cb(%p) error(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%x) erridx(%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)",
				__func__, cb_param, cb_param->err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				err_idx, real_frm_idx,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
				cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
				cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
			err_idx = cb_param->task_cnt - 1;
		}
		real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + err_idx;
		pr_info(
			"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%x) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d/%d) earlycb(%d) ofst(0x%x) erridx(%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)",
			__func__, cb_param, cb_param->req_fd,
			cb_param->req_no, cb_param->frm_no,
			cb_param->err, cb_param->group_id,
			cb_param->clt, cb_param->hw_comb,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
			cb_param->is_earlycb, cb_param->pkt->err_data.offset,
			err_idx, real_frm_idx,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
		if (is_stream_off == 1)
			pr_info("%s: [ERROR] cb(%p) pipe had been turned off(%d)!\n",
				__func__, cb_param, is_stream_off);
		pipe = (struct mtk_imgsys_pipe *)cb_param->frm_info->pipe;
		if (!pipe->streaming) {
			/* is_stream_off = 1; */
			pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
				__func__, cb_param, is_stream_off);
		}

		event = cb_param->pkt->err_data.event;
		if ((event >= IMGSYS_CMDQ_HW_TOKEN_BEGIN) &&
			(event <= IMGSYS_CMDQ_HW_TOKEN_END)) {
			pr_info(
				"%s: [ERROR] HW token timeout! wfe(%d) event(%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT1_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT1_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT1_BEGIN;
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event1 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT2_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT2_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT2_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event2 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT3_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT3_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT3_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event3 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT4_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT4_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT4_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event4 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT5_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT5_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT5_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event5 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_GPR_EVENT_BEGIN) &&
			(event <= IMGSYS_CMDQ_GPR_EVENT_END)) {
			isHWhang = 1;
			pr_info(
				"%s: [ERROR] GPR event timeout! wfe(%d) event(%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else if ((is_stream_off == 1) && (event == 0)) {
			pr_info(
				"%s: [ERROR] pipe had been turned off(%d)! wfe(%d) event(%d) isHW(%d)",
				__func__, is_stream_off,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else {
			isHWhang = 1;
			pr_info(
				"%s: [ERROR] HW event timeout! wfe(%d) event(%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		}

		imgsys_cmdq_cmd_dump_plat7s(cb_param->frm_info, real_frm_idx);

		if (cb_param->user_cmdq_err_cb) {
			struct cmdq_cb_data user_cb_data;

			user_cb_data.err = cb_param->err;
			user_cb_data.data = (void *)cb_param->frm_info;
			cb_param->user_cmdq_err_cb(
				user_cb_data, real_frm_idx, isHWhang,
				event_sft + IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START);
		}
	}

	cb_param->cmdqTs.tsCmdqCbEnd = ktime_get_boottime_ns()/1000;

#if CMDQ_CB_KTHREAD
	kthread_init_work(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work_plat7s);
	kthread_queue_work(&imgsys_cmdq_worker, &cb_param->cmdq_cb_work);
#else
	INIT_WORK(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work_plat7s);
	queue_work(imgsys_cmdq_wq, &cb_param->cmdq_cb_work);
#endif
}

int imgsys_cmdq_task_aee_cb_plat7s(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_pipe *pipe = NULL;
	size_t err_ofst;
	u32 idx = 0, err_idx = 0, real_frm_idx = 0;
	u16 event = 0, event_sft = 0;
	u64 event_diff = 0;
	bool isHWhang = 0;
	enum cmdq_aee_type ret = CMDQ_NO_AEE;

	pkt = (struct cmdq_pkt *)data.data;
	cb_param = pkt->user_priv;

	err_ofst = cb_param->pkt->err_data.offset;
	err_idx = 0;
	for (idx = 0; idx < cb_param->task_cnt; idx++)
		if (err_ofst > cb_param->pkt_ofst[idx])
			err_idx++;
		else
			break;
	if (err_idx >= cb_param->task_cnt) {
		pr_info(
			"%s: [ERROR] can't find task in task list! cb(%p) error(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%x) erridx(%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)",
			__func__, cb_param, cb_param->err, cb_param->group_id,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->pkt->err_data.offset,
			err_idx, real_frm_idx,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
		err_idx = cb_param->task_cnt - 1;
	}
	real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + err_idx;
	pr_info(
		"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%x) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d/%d) earlycb(%d) ofst(0x%x) erridx(%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)",
		__func__, cb_param, cb_param->req_fd,
		cb_param->req_no, cb_param->frm_no,
		cb_param->err, cb_param->group_id,
		cb_param->clt, cb_param->hw_comb,
		cb_param->frm_idx, cb_param->frm_num,
		cb_param->blk_idx, cb_param->blk_num,
		cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
		cb_param->is_earlycb, cb_param->pkt->err_data.offset,
		err_idx, real_frm_idx,
		cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
		cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
		cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
	if (is_stream_off == 1)
		pr_info("%s: [ERROR] cb(%p) pipe had been turned off(%d)!\n",
			__func__, cb_param, is_stream_off);
	pipe = (struct mtk_imgsys_pipe *)cb_param->frm_info->pipe;
	if (!pipe->streaming) {
		/* is_stream_off = 1; */
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
			__func__, cb_param, is_stream_off);
	}

	event = cb_param->pkt->err_data.event;
	if ((event >= IMGSYS_CMDQ_HW_TOKEN_BEGIN) &&
		(event <= IMGSYS_CMDQ_HW_TOKEN_END)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] HW token timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT1_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT1_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT1_BEGIN;
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event1 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT2_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT2_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT2_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event2 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT3_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT3_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT3_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event3 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT4_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT4_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT4_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event4 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT5_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT5_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT5_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event5 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_GPR_EVENT_BEGIN) &&
		(event <= IMGSYS_CMDQ_GPR_EVENT_END)) {
		isHWhang = 1;
		ret = CMDQ_AEE_EXCEPTION;
		pr_info(
			"%s: [ERROR] GPR event timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else if ((is_stream_off == 1) && (event == 0)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] pipe had been turned off(%d)! wfe(%d) event(%d) isHW(%d)",
			__func__, is_stream_off,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else {
		isHWhang = 1;
		ret = CMDQ_AEE_EXCEPTION;
		pr_info(
			"%s: [ERROR] HW event timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	}

	return ret;
}

int imgsys_cmdq_sendtask_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data,
					uint32_t subfidx, bool isLastTaskInReq),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
					uint32_t fail_subfidx, bool isHWhang, uint32_t hangEvent))
{
	struct cmdq_client *clt = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dvfs *dvfs_info = NULL;
	dma_addr_t pkt_ts_pa = 0;
	u32 *pkt_ts_va = NULL;
	u32 pkt_ts_num = 0;
	u32 pkt_ts_ofst = 0;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 blk_idx = 0; /* For Vss block cnt */
	u32 blk_num = 0;
	u32 thd_idx = 0;
	u32 hw_comb = 0;
	int ret = 0, ret_flush = 0, ret_sn = 0;
	u64 tsReqStart = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;
	u32 frm_num = 0, frm_idx = 0;
	u32 cmd_ofst = 0;
	bool isPack = 0;
	u32 task_idx = 0;
	u32 task_id = 0;
	u32 task_num = 0;
	u32 task_cnt = 0;
	size_t pkt_ofst[MAX_FRAME_IN_TASK] = {0};
	char logBuf_temp[MTK_IMGSYS_LOG_LENGTH];

	/* PMQOS API */
	tsDvfsQosStart = ktime_get_boottime_ns()/1000;
	IMGSYS_CMDQ_SYSTRACE_BEGIN("%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
		__func__, "dvfs_qos", frm_info->frame_no, frm_info->request_no,
		frm_info->request_fd, frm_info->frm_owner);
	mutex_lock(&(imgsys_dev->dvfs_qos_lock));
	#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_mmqos_cal_plat7s(imgsys_dev, frm_info, 1);
	mtk_imgsys_mmdvfs_set_plat7s(imgsys_dev, frm_info, 1);
	mtk_imgsys_mmqos_set_by_scen_plat7s(imgsys_dev, frm_info, 1);
	#endif
	mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
	IMGSYS_CMDQ_SYSTRACE_END();
	tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

	/* is_stream_off = 0; */
	frm_num = frm_info->total_frmnum;
	frm_info->cb_frmcnt = 0;
	frm_info->total_taskcnt = 0;
	cmd_ofst = sizeof(struct GCERecoder);
	dvfs_info = &imgsys_dev->dvfs_info;

	#if IMGSYS_SECURE_ENABLE
	if (frm_info->is_secReq && (is_sec_task_create == 0)) {
		imgsys_cmdq_sec_sendtask_plat7s(imgsys_dev);
		is_sec_task_create = 1;
		pr_info(
			"%s: create imgsys secure task is_secReq(%d)\n",
			__func__, frm_info->is_secReq);
	}
	#endif

	/* Allocate cmdq buffer for task timestamp */
	if (imgsys_cmdq_ts_enable_plat7s() || imgsys_wpe_bwlog_enable_plat7s()) {
		pkt_ts_va = cmdq_mbox_buf_alloc(imgsys_clt[0], &pkt_ts_pa);
		/* if (imgsys_cmdq_ts_dbg_enable_plat7s()) { */
			frm_info->hw_ts_log = vzalloc(sizeof(char)*MTK_IMGSYS_LOG_LENGTH*4);
			memset((char *)frm_info->hw_ts_log, 0x0, MTK_IMGSYS_LOG_LENGTH*4);
			frm_info->hw_ts_log[strlen(frm_info->hw_ts_log)] = '\0';
			memset((char *)logBuf_temp, 0x0, MTK_IMGSYS_LOG_LENGTH);
			logBuf_temp[strlen(logBuf_temp)] = '\0';
			ret_sn = snprintf(logBuf_temp, MTK_IMGSYS_LOG_LENGTH,
				"Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%s gid(%d) fps(%d) dvfs_v/f(%d/%ld) freq(%ld/%ld/%ld/%ld)",
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, (char *)(&(frm_info->frm_owner)),
				frm_info->group_id, frm_info->fps,
				dvfs_info->cur_volt, dvfs_info->cur_freq,
				dvfs_info->freq, dvfs_info->pixel_size[0],
				dvfs_info->pixel_size[1], dvfs_info->pixel_size[2]);
			if (ret_sn < 0)
				pr_info("%s: [ERROR] snprintf fail: %d\n", __func__, ret_sn);
			strncat(frm_info->hw_ts_log, logBuf_temp, strlen(logBuf_temp));
		/* } */
	}

	for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
		cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;

		if ((cmd_buf->header_code != 0x5A5A5A5A) ||
				(cmd_buf->check_pre != 0x55AA55AA) ||
				(cmd_buf->check_post != 0xAA55AA55) ||
				(cmd_buf->footer_code != 0xA5A5A5A5)) {
			pr_info("%s: Incorrect guard word: %08x/%08x/%08x/%08x", __func__,
				cmd_buf->header_code, cmd_buf->check_pre, cmd_buf->check_post,
				cmd_buf->footer_code);
			return -1;
		}

		cmd_num = cmd_buf->curr_length / sizeof(struct Command);
		cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
			(unsigned long)(cmd_buf->cmd_offset));
		blk_num = cmd_buf->frame_block;
		hw_comb = frm_info->user_info[frm_idx].hw_comb;

		if (isPack == 0) {
			if (frm_info->group_id == -1) {
				/* Choose cmdq_client base on hw scenario */
				for (thd_idx = 0; thd_idx < IMGSYS_NOR_THD; thd_idx++) {
					if (hw_comb & 0x1) {
						clt = imgsys_clt[thd_idx];
						pr_debug(
						"%s: chosen mbox thread (%d, 0x%x) for frm(%d/%d)\n",
						__func__, thd_idx, clt, frm_idx, frm_num);
						break;
					}
					hw_comb = hw_comb>>1;
				}
				/* This segment can be removed since user had set dependency */
				if (frm_info->user_info[frm_idx].hw_comb & IMGSYS_ENG_DIP) {
					thd_idx = 4;
					clt = imgsys_clt[thd_idx];
				}
			} else {
				if (frm_info->group_id < IMGSYS_NOR_THD) {
					thd_idx = frm_info->group_id;
					clt = imgsys_clt[thd_idx];
				} else {
					pr_info(
						"%s: [ERROR] group_id(%d) is over max hw num(%d) for frm(%d/%d)!\n",
						__func__, frm_info->group_id, IMGSYS_NOR_THD,
						frm_info->user_info[frm_idx].hw_comb,
						frm_idx, frm_num);
					return -1;
				}
			}

			/* This is work around for low latency flow.		*/
			/* If we change to request base,			*/
			/* we don't have to take this condition into account.	*/
			if (frm_info->sync_id != -1) {
				thd_idx = 0;
				clt = imgsys_clt[thd_idx];
			}

			if (clt == NULL) {
				pr_info("%s: [ERROR] No HW Found (0x%x) for frm(%d/%d)!\n",
					__func__, frm_info->user_info[frm_idx].hw_comb,
					frm_idx, frm_num);
				return -1;
			}
		}

		dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) frm(%d/%d) cmd_oft(0x%x/0x%x), cmd_len(%d), num(%d), sz_per_cmd(%d), frm_blk(%d), hw_comb(0x%x), sync_id(%d), gce_thd(%d), gce_clt(0x%x)\n",
			__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
			frm_idx, frm_num, cmd_buf->cmd_offset, cmd_ofst, cmd_buf->curr_length,
			cmd_num, sizeof(struct Command), cmd_buf->frame_block,
			frm_info->user_info[frm_idx].hw_comb, frm_info->sync_id, thd_idx, clt);

		cmd_idx = 0;
		for (blk_idx = 0; blk_idx < blk_num; blk_idx++) {
			tsReqStart = ktime_get_boottime_ns()/1000;
			if (isPack == 0) {
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
				/* Reset pkt timestamp num */
				pkt_ts_num = 0;

				/* Assign task priority according to is_time_shared */
				if (frm_info->user_info[frm_idx].is_time_shared)
					pkt->priority = IMGSYS_PRI_LOW;
				else
					pkt->priority = IMGSYS_PRI_HIGH;
			}

			IMGSYS_CMDQ_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx frm(%d/%d) blk(%d)",
				__func__, "cmd_parser", frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_info->user_info[frm_idx].subfrm_idx,
				frm_info->user_info[frm_idx].hw_comb, frm_info->frm_owner,
				frm_idx, frm_num, blk_idx);
			// Add secure token begin
			#if IMGSYS_SECURE_ENABLE
			if (frm_info->user_info[frm_idx].is_secFrm)
				imgsys_cmdq_sec_cmd_plat7s(pkt);
			#endif

			ret = imgsys_cmdq_parser_plat7s(frm_info, pkt, &cmd[cmd_idx], hw_comb,
				(pkt_ts_pa + 4 * pkt_ts_ofst), &pkt_ts_num, thd_idx);
			if (ret < 0) {
				pr_info(
					"%s: [ERROR] parsing idx(%d) with cmd(%d) in block(%d) for frm(%d/%d) fail\n",
					__func__, cmd_idx, cmd[cmd_idx].opcode,
					blk_idx, frm_idx, frm_num);
				cmdq_pkt_destroy(pkt);
				goto sendtask_done;
			}
			cmd_idx += ret;

			// Add secure token end
			#if IMGSYS_SECURE_ENABLE
			if (frm_info->user_info[frm_idx].is_secFrm)
				imgsys_cmdq_sec_cmd_plat7s(pkt);
			#endif

			IMGSYS_CMDQ_SYSTRACE_END();

			/* Check for packing gce task */
			pkt_ofst[task_cnt] = pkt->cmd_buf_size - CMDQ_INST_SIZE;
			task_cnt++;
			if ((frm_info->user_info[frm_idx].is_time_shared)
				|| (frm_info->user_info[frm_idx].is_secFrm)
				|| (frm_info->user_info[frm_idx].is_earlycb)
				|| ((frm_idx + 1) == frm_num)
				|| (frm_info->user_info[frm_idx].wait_fence_num > 0)
				|| (frm_info->user_info[frm_idx].notify_fence_num > 0)) {
#ifndef CONFIG_FPGA_EARLY_PORTING
				mtk_imgsys_power_ctrl_plat7s(imgsys_dev, true);
#endif
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

				task_num++;
				cb_param->pkt = pkt;
				cb_param->frm_info = frm_info;
				cb_param->req_fd = frm_info->request_fd;
				cb_param->req_no = frm_info->request_no;
				cb_param->frm_no = frm_info->frame_no;
				cb_param->hw_comb = hw_comb;
				cb_param->frm_idx = frm_idx;
				cb_param->frm_num = frm_num;
				cb_param->user_cmdq_cb = cmdq_cb;
				cb_param->user_cmdq_err_cb = cmdq_err_cb;
				if ((blk_idx + 1) == blk_num)
					cb_param->isBlkLast = 1;
				else
					cb_param->isBlkLast = 0;
				if ((frm_idx + 1) == frm_num)
					cb_param->isFrmLast = 1;
				else
					cb_param->isFrmLast = 0;
				cb_param->blk_idx = blk_idx;
				cb_param->blk_num = blk_num;
				cb_param->is_earlycb = frm_info->user_info[frm_idx].is_earlycb;
				cb_param->group_id = frm_info->group_id;
				cb_param->cmdqTs.tsReqStart = tsReqStart;
				cb_param->cmdqTs.tsDvfsQosStart = tsDvfsQosStart;
				cb_param->cmdqTs.tsDvfsQosEnd = tsDvfsQosEnd;
				cb_param->imgsys_dev = imgsys_dev;
				cb_param->thd_idx = thd_idx;
				cb_param->clt = clt;
				cb_param->task_cnt = task_cnt;
				for (task_idx = 0; task_idx < task_cnt; task_idx++)
					cb_param->pkt_ofst[task_idx] = pkt_ofst[task_idx];
				task_cnt = 0;
				cb_param->task_id = task_id;
				task_id++;
				if ((cb_param->isBlkLast) && (cb_param->isFrmLast)) {
					cb_param->isTaskLast = 1;
					cb_param->task_num = task_num;
					frm_info->total_taskcnt = task_num;
				} else {
					cb_param->isTaskLast = 0;
					cb_param->task_num = 0;
				}

				imgsys_cmdq_fence_set_plat7s(imgsys_dev, frm_info, cb_param,
					frm_idx, thd_idx);

				dev_dbg(imgsys_dev->dev,
					"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%x/%x/%x/%x/%x)\n",
					__func__, cb_param, cb_param->group_id,
					cb_param->blk_idx,  cb_param->blk_num,
					cb_param->frm_idx, cb_param->frm_num,
					cb_param->isBlkLast, cb_param->isFrmLast,
					cb_param->isTaskLast, cb_param->task_id,
					cb_param->task_num, cb_param->task_cnt,
					cb_param->pkt_ofst[0], cb_param->pkt_ofst[1],
					cb_param->pkt_ofst[2], cb_param->pkt_ofst[3],
					cb_param->pkt_ofst[4]);

				if (imgsys_cmdq_ts_enable_plat7s()
					|| imgsys_wpe_bwlog_enable_plat7s()) {
					cb_param->taskTs.dma_pa = pkt_ts_pa;
					cb_param->taskTs.dma_va = pkt_ts_va;
					cb_param->taskTs.num = pkt_ts_num;
					cb_param->taskTs.ofst = pkt_ts_ofst;
					pkt_ts_ofst += pkt_ts_num;
				}

				/* flush synchronized, block API */
				cb_param->cmdqTs.tsFlushStart = ktime_get_boottime_ns()/1000;

				pkt->aee_cb = imgsys_cmdq_task_aee_cb_plat7s;
				pkt->user_priv = (void *)cb_param;

				IMGSYS_CMDQ_SYSTRACE_BEGIN(
					"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb(%p) frm(%d/%d) blk(%d/%d)",
					__func__, "pkt_flush", frm_info->frame_no,
					frm_info->request_no, frm_info->request_fd,
					frm_info->user_info[frm_idx].subfrm_idx,
					frm_info->user_info[frm_idx].hw_comb,
					frm_info->frm_owner, cb_param, frm_idx, frm_num,
					blk_idx, blk_num);

				ret_flush = cmdq_pkt_flush_async(pkt, imgsys_cmdq_task_cb_plat7s,
								(void *)cb_param);
				IMGSYS_CMDQ_SYSTRACE_END();
				if (ret_flush < 0)
					pr_info(
					"%s: [ERROR] cmdq_pkt_flush_async fail(%d) for frm(%d/%d)!\n",
						__func__, ret_flush, frm_idx, frm_num);
				else
					pr_debug(
					"%s: cmdq_pkt_flush_async success(%d), blk(%d), frm(%d/%d)!\n",
						__func__, ret_flush, blk_idx, frm_idx, frm_num);
				isPack = 0;
			} else {
				isPack = 1;
			}
		}
	}

sendtask_done:
	return ret;
}

int imgsys_cmdq_parser_plat7s(struct swfrm_info_t *frm_info, struct cmdq_pkt *pkt,
						struct Command *cmd, u32 hw_comb,
						dma_addr_t dma_pa, uint32_t *num, u32 thd_idx)
{
	bool stop = 0;
	int count = 0;
	int req_fd = 0, req_no = 0, frm_no = 0;
	u32 event = 0;

	req_fd = frm_info->request_fd;
	req_no = frm_info->request_no;
	frm_no = frm_info->frame_no;

	pr_debug("%s: +, cmd(%d)\n", __func__, cmd->opcode);

	do {
		switch (cmd->opcode) {
		case IMGSYS_CMD_READ:
			pr_debug(
				"%s: READ with source(0x%08lx) target(0x%08lx) mask(0x%08x)\n",
				__func__, cmd->u.source, cmd->u.target, cmd->u.mask);
			if (imgsys_wpe_bwlog_enable_plat7s()) {
				cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.source,
					dma_pa + (4*(*num)), CMDQ_THR_SPR_IDX3);
				(*num)++;
			} else
				pr_info(
					"%s: [ERROR]Not enable imgsys read cmd!!\n",
					__func__);
			break;
		case IMGSYS_CMD_WRITE:
			pr_debug(
				"%s: WRITE with addr(0x%08lx) value(0x%08x) mask(0x%08x)\n",
				__func__, cmd->u.address, cmd->u.value, cmd->u.mask);
			cmdq_pkt_write(pkt, NULL, (dma_addr_t)cmd->u.address,
					cmd->u.value, cmd->u.mask);
			break;
		case IMGSYS_CMD_POLL:
			pr_debug(
				"%s: POLL with addr(0x%08lx) value(0x%08x) mask(0x%08x) thd(%d)\n",
				__func__, cmd->u.address, cmd->u.value, cmd->u.mask, thd_idx);
			/* cmdq_pkt_poll(pkt, NULL, cmd->u.value, cmd->u.address, */
			/* cmd->u.mask, CMDQ_GPR_R15); */
			cmdq_pkt_poll_timeout(pkt, cmd->u.value, SUBSYS_NO_SUPPORT,
				cmd->u.address, cmd->u.mask, 0xFFFF, CMDQ_GPR_R03+thd_idx);
			break;
		case IMGSYS_CMD_WAIT:
			pr_debug(
				"%s: WAIT event(%d/%d) action(%d)\n",
				__func__, cmd->u.event, imgsys_event[cmd->u.event].event,
				cmd->u.action);
			if (cmd->u.action == 1) {
				cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event);
				if ((cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
					(cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st++;
					event_hist[event].wait.req_fd = req_fd;
					event_hist[event].wait.req_no = req_no;
					event_hist[event].wait.frm_no = frm_no;
					event_hist[event].wait.ts = ktime_get_boottime_ns()/1000;
					event_hist[event].wait.frm_info = frm_info;
					event_hist[event].wait.pkt = pkt;
				}
			} else if (cmd->u.action == 0)
				cmdq_pkt_wait_no_clear(pkt, imgsys_event[cmd->u.event].event);
			else
				pr_info("%s: [ERROR]Not Support wait action(%d)!\n",
					__func__, cmd->u.action);
			break;
		case IMGSYS_CMD_UPDATE:
			pr_debug(
				"%s: UPDATE event(%d/%d) action(%d)\n",
				__func__, cmd->u.event, imgsys_event[cmd->u.event].event,
				cmd->u.action);
			if (cmd->u.action == 1) {
				cmdq_pkt_set_event(pkt, imgsys_event[cmd->u.event].event);
				if ((cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
					(cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st--;
					event_hist[event].set.req_fd = req_fd;
					event_hist[event].set.req_no = req_no;
					event_hist[event].set.frm_no = frm_no;
					event_hist[event].set.ts = ktime_get_boottime_ns()/1000;
					event_hist[event].set.frm_info = frm_info;
					event_hist[event].set.pkt = pkt;
				}
			} else if (cmd->u.action == 0)
				cmdq_pkt_clear_event(pkt, imgsys_event[cmd->u.event].event);
			else
				pr_info("%s: [ERROR]Not Support update action(%d)!\n",
					__func__, cmd->u.action);
			break;
		case IMGSYS_CMD_ACQUIRE:
			pr_debug(
				"%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
				cmd->u.event, imgsys_event[cmd->u.event].event, cmd->u.action);
				cmdq_pkt_acquire_event(pkt, imgsys_event[cmd->u.event].event);
			break;
		case IMGSYS_CMD_TIME:
			pr_debug(
				"%s: TIME with addr(0x%08lx) num(0x%08x)\n",
				__func__, dma_pa, *num);
			if (imgsys_cmdq_ts_enable_plat7s()) {
				cmdq_pkt_write_indriect(pkt, NULL, dma_pa + (4*(*num)),
					CMDQ_TPR_ID, ~0);
				(*num)++;
			} else
				pr_info(
					"%s: [ERROR]Not enable imgsys cmdq ts function!!\n",
					__func__);
			break;
		case IMGSYS_CMD_STOP:
			pr_debug("%s: End Of Cmd!\n", __func__);
			stop = 1;
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd->opcode);
			return -1;
		}
		cmd++;
		count++;
	} while (stop == 0);

	return count;
}

void imgsys_cmdq_sec_task_cb_plat7s(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt_sec = (struct cmdq_pkt *)data.data;

	cmdq_pkt_destroy(pkt_sec);
}

int imgsys_cmdq_sec_sendtask_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct cmdq_client *clt_sec = NULL;
	#if IMGSYS_SECURE_ENABLE
	struct cmdq_pkt *pkt_sec = NULL;
	#endif
	int ret = 0;

	clt_sec = imgsys_sec_clt[0];
	#if IMGSYS_SECURE_ENABLE
	pkt_sec = cmdq_pkt_create(clt_sec);
	cmdq_sec_pkt_set_data(pkt_sec, 0, 0, CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(pkt_sec, true);
	cmdq_pkt_finalize_loop(pkt_sec);
	cmdq_pkt_flush_threaded(pkt_sec, imgsys_cmdq_sec_task_cb_plat7s, (void *)pkt_sec);
	#endif
	return ret;
}

void imgsys_cmdq_sec_cmd_plat7s(struct cmdq_pkt *pkt)
{
	cmdq_pkt_set_event(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT].event);
	cmdq_pkt_wfe(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISP_SET].event);
}

void imgsys_cmdq_setevent_plat7s(u64 u_id)
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

void imgsys_cmdq_clearevent_plat7s(int event_id)
{
	u32 event = 0;

	if ((event_id >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
		(event_id <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
		cmdq_mbox_enable(imgsys_clt[0]->chan);
		cmdq_clear_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);
		pr_debug("%s: cmdq_clear_event with (%d/%d)!\n",
			__func__, event_id, imgsys_event[event_id].event);
		event = event_id - IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
		memset((void *)&event_hist[event], 0x0,
			sizeof(struct imgsys_event_history));
		cmdq_mbox_disable(imgsys_clt[0]->chan);
	} else {
		pr_info("%s: [ERROR]unexpected event_id=(%d)!\n",
			__func__, event_id);
	}
}

#if DVFS_QOS_READY
void mtk_imgsys_mmdvfs_init_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	u64 freq = 0;
	int ret = 0, opp_num = 0, opp_idx = 0, idx = 0, volt;
	struct device_node *np, *child_np = NULL;
	struct of_phandle_iterator it;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_imgsys_dvfs));
	dvfs_info->dev = imgsys_dev->dev;
	dvfs_info->reg = NULL;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_info(dvfs_info->dev,
			"%s: [ERROR] fail to init opp table: %d\n", __func__, ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get_optional(dvfs_info->dev, "dvfsrc-vmm");
	if (IS_ERR_OR_NULL(dvfs_info->reg)) {
		dev_info(dvfs_info->dev,
			"%s: [ERROR] Failed to get dvfsrc-vmm\n", __func__);
		dvfs_info->reg = NULL;
		dvfs_info->mmdvfs_clk = devm_clk_get(dvfs_info->dev, "mmdvfs_clk");
		if (IS_ERR_OR_NULL(dvfs_info->mmdvfs_clk)) {
			dev_info(dvfs_info->dev,
				"%s: [ERROR] Failed to get mmdvfs_clk\n", __func__);
			dvfs_info->mmdvfs_clk = NULL;
			return;
		}
	}

	if (dvfs_info->reg)
		opp_num = regulator_count_voltages(dvfs_info->reg);
	of_for_each_phandle(
		&it, ret, dvfs_info->dev->of_node, "operating-points-v2", NULL, 0) {
		np = of_node_get(it.node);
		if (!np) {
			dev_info(dvfs_info->dev, "%s: [ERROR] of_node_get fail\n", __func__);
			return;
		}

		do {
			child_np = of_get_next_available_child(np, child_np);
			if (child_np) {
				of_property_read_u64(child_np, "opp-hz", &freq);
				of_property_read_u32(child_np, "opp-microvolt", &volt);
				if ((freq == 0) || (volt == 0)) {
					dev_info(
						dvfs_info->dev,
						"%s: [ERROR] parsing zero freq/volt(%d/%d) at idx(%d)\n",
						__func__, freq, volt, idx);
					continue;
				} else if (volt > IMGSYS_DVFS_MAX_VOLT) {
					dev_info(
						dvfs_info->dev,
						"%s: [ERROR] volt(%d) is over maximum(%d) at idx(%d)\n",
						__func__, volt, IMGSYS_DVFS_MAX_VOLT, idx);
					break;
				}
				dvfs_info->clklv_dts[opp_idx][idx] = freq;
				dvfs_info->voltlv_dts[opp_idx][idx] = volt;
				idx++;
			}
		} while (child_np);
		dvfs_info->clklv_num_dts[opp_idx] = idx;
		dvfs_info->clklv_target[opp_idx] = dvfs_info->clklv_dts[opp_idx][0];
		dvfs_info->clklv_idx[opp_idx] = 0;
		idx = 0;
		opp_idx++;
		of_node_put(np);
	}

	opp_num = opp_idx;
	for (opp_idx = 0; opp_idx < opp_num; opp_idx++) {
		for (idx = 0; idx < dvfs_info->clklv_num_dts[opp_idx]; idx++) {
			dev_info(dvfs_info->dev, "[%s] opp=%d, idx_dts=%d, clk_dts=%d volt_dts=%d\n",
				__func__, opp_idx, idx, dvfs_info->clklv_dts[opp_idx][idx],
				dvfs_info->voltlv_dts[opp_idx][idx]);
		}
	}

	/* For fine grain dvfs */
	for (opp_idx = 0; opp_idx < opp_num; opp_idx++) {
		idx = 0;
		dvfs_info->clklv_num_fg[opp_idx] =
			dvfs_info->clklv_num_dts[opp_idx] * 2 - 1;
		dvfs_info->clklv_fg[opp_idx][idx] = dvfs_info->clklv_dts[opp_idx][idx];
		dvfs_info->voltlv_fg[opp_idx][idx] = dvfs_info->voltlv_dts[opp_idx][idx];
		dev_info(dvfs_info->dev, "[%s] opp=%d, idx_fg=%d, clk_fg=%d volt_fg=%d\n",
			__func__, opp_idx, idx,
			dvfs_info->clklv_fg[opp_idx][idx],
			dvfs_info->voltlv_fg[opp_idx][idx]);
		for (idx = 1; idx < dvfs_info->clklv_num_dts[opp_idx]; idx++) {
			dvfs_info->clklv_fg[opp_idx][idx*2-1] =
				(((dvfs_info->clklv_dts[opp_idx][idx-1] / IMGSYS_DVFS_MHz) *
				IMGSYS_DVFS_RATIO_L +
				(dvfs_info->clklv_dts[opp_idx][idx] / IMGSYS_DVFS_MHz) *
				IMGSYS_DVFS_RATIO_H) /
				(IMGSYS_DVFS_RATIO_L + IMGSYS_DVFS_RATIO_H)) * IMGSYS_DVFS_MHz;
			dvfs_info->voltlv_fg[opp_idx][idx*2-1] =
				(dvfs_info->voltlv_dts[opp_idx][idx-1] * IMGSYS_DVFS_RATIO_L +
				dvfs_info->voltlv_dts[opp_idx][idx] * IMGSYS_DVFS_RATIO_H) /
				(IMGSYS_DVFS_RATIO_L + IMGSYS_DVFS_RATIO_H);
			dvfs_info->clklv_fg[opp_idx][idx*2] =
				dvfs_info->clklv_dts[opp_idx][idx];
			dvfs_info->voltlv_fg[opp_idx][idx*2] =
				dvfs_info->voltlv_dts[opp_idx][idx];
			dev_info(dvfs_info->dev, "[%s] opp=%d, idx_fg=%d, clk_fg=%d volt_fg=%d\n",
				__func__, opp_idx, idx*2-1,
				dvfs_info->clklv_fg[opp_idx][idx*2-1],
				dvfs_info->voltlv_fg[opp_idx][idx*2-1]);
			dev_info(dvfs_info->dev, "[%s] opp=%d, idx_fg=%d, clk_fg=%d volt_fg=%d\n",
				__func__, opp_idx, idx*2,
				dvfs_info->clklv_fg[opp_idx][idx*2],
				dvfs_info->voltlv_fg[opp_idx][idx*2]);
		}
	}

	dvfs_info->cur_volt = 0;
	dvfs_info->cur_freq = 0;
	dvfs_info->vss_task_cnt = 0;
	dvfs_info->smvr_task_cnt = 0;
	dvfs_info->opp_num = opp_num;

}

void mtk_imgsys_mmdvfs_uninit_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	int volt = 0, ret = 0;
	unsigned long freq = 0;

	dvfs_info->cur_volt = volt;
	dvfs_info->cur_freq = freq;

	if (dvfs_info->reg) {
		ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);
		if (ret)
			dev_info(dvfs_info->dev,
				"[%s] Failed to set regulator voltage(%d) with ret(%d)\n",
				__func__, volt, ret);
	} else if (dvfs_info->mmdvfs_clk) {
		ret = clk_set_rate(dvfs_info->mmdvfs_clk, freq);
		if (ret)
			dev_info(dvfs_info->dev,
				"[%s] Failed to set mmdvfs rate(%ld) with ret(%d)\n",
				__func__, freq, ret);
	} else
		dev_info(dvfs_info->dev,
			"%s: [ERROR] reg and clk is err or null\n", __func__);

}

void mtk_imgsys_mmdvfs_set_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	int volt = 0, ret = 0, idx = 0, opp_idx = 0;
	unsigned long freq = 0;
	/* u32 hw_comb = frm_info->user_info[0].hw_comb; */

	freq = dvfs_info->freq;

	if (IS_ERR_OR_NULL(dvfs_info->reg) && IS_ERR_OR_NULL(dvfs_info->mmdvfs_clk))
		dev_dbg(dvfs_info->dev, "%s: [ERROR] reg and clk is err or null\n", __func__);
	else {
		/* Choose for IPESYS */
		/* if (hw_comb & IMGSYS_ENG_ME) */
			/* opp_idx = 1; */

		for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
			if (freq <= dvfs_info->clklv[opp_idx][idx])
				break;
		}
		if (idx == dvfs_info->clklv_num[opp_idx])
			idx--;
		volt = dvfs_info->voltlv[opp_idx][idx];

		if (freq > 0) {
			freq = dvfs_info->clklv[opp_idx][idx]; // signed-off
		} else {
			volt = 0;
			freq = 0;
		}

		if (dvfs_info->cur_volt != volt) {
			if (imgsys_dvfs_dbg_enable_plat7s())
				dev_info(dvfs_info->dev, "[%s] volt change opp=%d, idx=%d, clk=%d volt=%d\n",
					__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
					dvfs_info->voltlv[opp_idx][idx]);
			if (dvfs_info->reg) {
				ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);
				if (ret)
					dev_info(dvfs_info->dev,
						"[%s] Failed to set regulator voltage(%d) with ret(%d)\n",
						__func__, volt, ret);
			} else if (dvfs_info->mmdvfs_clk) {
				ret = clk_set_rate(dvfs_info->mmdvfs_clk, freq);
				if (ret)
					dev_info(dvfs_info->dev,
						"[%s] Failed to set mmdvfs rate(%ld) with ret(%d)\n",
						__func__, freq, ret);
			}
			dvfs_info->cur_volt = volt;
			dvfs_info->cur_freq = freq;
		}
	}
}

void mtk_imgsys_mmdvfs_reset_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	unsigned int *clklv = NULL, *voltlv = NULL;
	int idx = 0, opp_idx = 0;

	for (opp_idx = 0; opp_idx < dvfs_info->opp_num; opp_idx++) {
		if (imgsys_fine_grain_dvfs_enable_plat7s()) {
			dvfs_info->clklv_num[opp_idx] = dvfs_info->clklv_num_fg[opp_idx];
			clklv = &dvfs_info->clklv_fg[opp_idx][0];
			voltlv = &dvfs_info->voltlv_fg[opp_idx][0];
		} else {
			dvfs_info->clklv_num[opp_idx] = dvfs_info->clklv_num_dts[opp_idx];
			clklv = &dvfs_info->clklv_dts[opp_idx][0];
			voltlv = &dvfs_info->voltlv_dts[opp_idx][0];
		}
		dev_info(dvfs_info->dev, "[%s] fine grain(%d), opp=%d, clk_num=%d\n",
			__func__, imgsys_fine_grain_dvfs_enable_plat7s(),
			opp_idx, dvfs_info->clklv_num[opp_idx]);
		for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
			dvfs_info->clklv[opp_idx][idx] = clklv[idx];
			dvfs_info->voltlv[opp_idx][idx] = voltlv[idx];
			dev_dbg(dvfs_info->dev, "[%s] opp=%d, idx=%d, clk=%d volt=%d\n",
				__func__, opp_idx, idx,
				dvfs_info->clklv[opp_idx][idx],
				dvfs_info->voltlv[opp_idx][idx]);
		}
	}

	for (idx = 0; idx < MTK_IMGSYS_DVFS_GROUP; idx++)
		dvfs_info->pixel_size[idx] = 0;

	dvfs_info->cur_volt = 0;
	dvfs_info->cur_freq = 0;
	dvfs_info->vss_task_cnt = 0;
	dvfs_info->smvr_task_cnt = 0;
}

void mtk_imgsys_mmqos_init_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	//struct icc_path *path;
	int idx = 0;
	u32 ret = 0;

	memset((void *)qos_info, 0x0, sizeof(struct mtk_imgsys_qos));
	qos_info->dev = imgsys_dev->dev;
	qos_info->qos_path = imgsys_qos_path;

#ifndef CONFIG_FPGA_EARLY_PORTING
	if (of_property_read_u32(qos_info->dev->of_node,
		"mediatek,imgsys-qos-sc-motr", &ret) != 0) {
		dev_info(qos_info->dev, "mmqos monitor is not exist\n");
	} else {
		qos_info->sc_monitor = ret;

		if (of_property_read_u32(qos_info->dev->of_node,
			"mediatek,imgsys-qos-sc-nums", &ret) != 0) {
			dev_info(qos_info->dev, "mmqos sc num is not exist\n");
		} else {
			qos_info->sc_nums = ret;
			dev_info(qos_info->dev, "mmqos monitor: %d, sc_nums: %d\n",
				 qos_info->sc_monitor, qos_info->sc_nums);

			if (qos_info->sc_nums > 0 &&
				qos_info->sc_nums < MTK_IMGSYS_QOS_SC_MAX_ID) {
				for (idx = 0; idx < qos_info->sc_nums; idx++) {
					if (of_property_read_u32_index(qos_info->dev->of_node,
					"mediatek,imgsys-qos-sc-id", idx, &ret) == 0)
						qos_info->sc_id[idx] = ret;
					dev_info(qos_info->dev, "mmqos sc_id[%d]: %d\n",
						idx, qos_info->sc_id[idx]);
				}
			}
		}
	}

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
#endif
	mtk_imgsys_mmqos_reset_plat7s(imgsys_dev);
}

void mtk_imgsys_mmqos_uninit_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	int idx = 0;

	for (idx = 0; idx < IMGSYS_M4U_PORT_MAX; idx++) {
		if (IS_ERR_OR_NULL(qos_info->qos_path[idx].path)) {
			dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n", __func__, idx);
			continue;
		}
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].bw);
		qos_info->qos_path[idx].bw = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
		mtk_icc_set_bw(qos_info->qos_path[idx].path, 0, 0);
#endif
	}
}


void mtk_imgsys_mmqos_monitor_plat7s(struct mtk_imgsys_dev *imgsys_dev, u32 state)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;

	u32 common_port[MAX_MON_REQ] = { 0 };
	u32 flag[MAX_MON_REQ] = { 0 };
	u32 bw0[MAX_MON_REQ] = { 0 };
	u32 bw1[MAX_MON_REQ] = { 0 };

	if (unlikely(!qos_info->sc_monitor))
		return;

	common_port[0] = qos_info->sc_id[0];
	common_port[1] = qos_info->sc_id[1];
	common_port[2] = qos_info->sc_id[2];
	common_port[3] = qos_info->sc_id[3];

	flag[0] = 1;
	flag[1] = 2;
	flag[2] = 1;
	flag[3] = 2;

	if (state == SMI_MONITOR_STOP_STATE ||
	    state == SMI_MONITOR_ACQUIRE_STATE) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		smi_monitor_stop(NULL, 0, bw0, SMI_BW_IMGSYS);
		if (qos_info->sc_monitor > 1)
			smi_monitor_stop(NULL, 1, bw1, SMI_BW_IMGSYS);
#endif
	}

	if (state == SMI_MONITOR_START_STATE || state == SMI_MONITOR_STOP_STATE) {
		qos_info->bw_total[SMI_COMMON_ID_31][SMI_READ] = 0;
		qos_info->bw_total[SMI_COMMON_ID_31][SMI_WRITE] = 0;
		qos_info->bw_total[SMI_COMMON_ID_32][SMI_READ] = 0;
		qos_info->bw_total[SMI_COMMON_ID_32][SMI_WRITE] = 0;
	} else if (state == SMI_MONITOR_ACQUIRE_STATE) {
		if (qos_info->req_cnt == 0) { //Initial setting
			qos_info->bw_total[SMI_COMMON_ID_31][SMI_READ] = IMGSYS_QOS_MAX_PERF >> 6;
			qos_info->bw_total[SMI_COMMON_ID_31][SMI_WRITE] = IMGSYS_QOS_MAX_PERF >> 6;
			qos_info->bw_total[SMI_COMMON_ID_32][SMI_READ] = IMGSYS_QOS_MAX_PERF >> 6;
			qos_info->bw_total[SMI_COMMON_ID_32][SMI_WRITE] = IMGSYS_QOS_MAX_PERF >> 6;
		} else {
			qos_info->bw_total[SMI_COMMON_ID_31][SMI_READ] = bw0[0] >> 20;
			qos_info->bw_total[SMI_COMMON_ID_31][SMI_WRITE] = bw0[1] >> 20;

			if (qos_info->sc_monitor > 1) {
				qos_info->bw_total[SMI_COMMON_ID_32][SMI_READ] = bw1[0] >> 20;
				qos_info->bw_total[SMI_COMMON_ID_32][SMI_WRITE] = bw1[1] >> 20;
			} else {
				qos_info->bw_total[SMI_COMMON_ID_32][SMI_READ] = bw0[2] >> 20;
				qos_info->bw_total[SMI_COMMON_ID_32][SMI_WRITE] = bw0[3] >> 20;
			}
		}
	}

	if (state == SMI_MONITOR_START_STATE ||
	    state == SMI_MONITOR_ACQUIRE_STATE) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		smi_monitor_start(qos_info->dev, 0, common_port, flag, SMI_BW_IMGSYS);
		if (qos_info->sc_monitor > 1)
			smi_monitor_start(qos_info->dev, 1, common_port, flag, SMI_BW_IMGSYS);
#endif
	}

}

void mtk_imgsys_mmqos_set_by_scen_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_qos *qos_info = &imgsys_dev->qos_info;
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	u32 hw_comb = 0;
	u64 pixel_sz = 0;
	u64 cur_interval = 0;
	u32 fps = 0;
	u64 frame_duration = 0;
	u32 frm_num = 0;
	u64 bw_final[4] = {0};
	u32 sidx = 0;
	const u32 step = imgsys_qos_update_freq;

	frm_num = frm_info->total_frmnum;
	hw_comb = frm_info->user_info[frm_num-1].hw_comb;
	pixel_sz = frm_info->user_info[frm_num-1].pixel_bw;
	fps = frm_info->fps;
	sidx = frm_info->user_info[0].subfrm_idx;

	if (is_stream_off == 0 && isSet == 1) {
		if (imgsys_qos_dbg_enable_plat7s())
			dev_info(qos_info->dev,
				 "imgsys_qos: frame_no:%d req_cnt:%d fps:%d vss:%d\n",
				 frm_info->frame_no, qos_info->req_cnt,
				 fps, dvfs_info->vss_task_cnt);

		if (dvfs_info->vss_task_cnt > 0 &&
		    qos_info->qos_path[IMGSYS_COMMON_0_R].bw < IMGSYS_QOS_MAX_PERF) {
			qos_info->qos_path[IMGSYS_COMMON_0_R].bw = IMGSYS_QOS_MAX_PERF;
			qos_info->qos_path[IMGSYS_COMMON_0_W].bw = IMGSYS_QOS_MAX_PERF;
			qos_info->qos_path[IMGSYS_COMMON_1_R].bw = IMGSYS_QOS_MAX_PERF;
			qos_info->qos_path[IMGSYS_COMMON_1_W].bw = IMGSYS_QOS_MAX_PERF;

			bw_final[0] = qos_info->qos_path[IMGSYS_COMMON_0_R].bw;
			bw_final[1] = qos_info->qos_path[IMGSYS_COMMON_0_W].bw;
			bw_final[2] = qos_info->qos_path[IMGSYS_COMMON_1_R].bw;
			bw_final[3] = qos_info->qos_path[IMGSYS_COMMON_1_W].bw;

			bw_final[0] = (bw_final[0] * imgsys_qos_factor) >> 2;
			bw_final[1] = (bw_final[1] * imgsys_qos_factor) >> 2;
			bw_final[2] = (bw_final[2] * imgsys_qos_factor) >> 2;
			bw_final[3] = (bw_final[3] * imgsys_qos_factor) >> 2;
			if (imgsys_qos_dbg_enable_plat7s())
				dev_info(qos_info->dev,
					"imgsys_qos: frame_no:%d-sc0_r-%d sc0_w-%d, sc1_r-%d sc0_w-%d\n",
					frm_info->frame_no,
					bw_final[0], bw_final[1], bw_final[2], bw_final[3]);

			IMGSYS_CMDQ_SYSTRACE_BEGIN("SetQos");
#ifndef CONFIG_FPGA_EARLY_PORTING
			mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_R].path,
					MBps_to_icc(bw_final[0]),
					0);
			mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_W].path,
					MBps_to_icc(bw_final[1]),
					0);
			mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_R].path,
					MBps_to_icc(bw_final[2]),
					0);
			mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_W].path,
					MBps_to_icc(bw_final[3]),
					0);
#endif
			IMGSYS_CMDQ_SYSTRACE_END();
		}

		if (fps) {
			frame_duration = 1000 / (fps << 1);
			cur_interval = (ktime_get_boottime_ns()/1000000) - qos_info->time_prev_req;

			if (frm_info->frm_owner == IMGSYS_QOS_SYNC_OWNER && sidx == 0 &&
			    frm_info->frame_no == 0) {
				qos_info->req_cnt = 0;
				qos_info->avg_cnt = 0;
			}

			if (cur_interval >= frame_duration &&
			    frm_info->frame_no >= qos_info->req_cnt) {

				mtk_imgsys_mmqos_monitor_plat7s(imgsys_dev,
								SMI_MONITOR_ACQUIRE_STATE);
				qos_info->req_cnt = frm_info->frame_no + 1;
				qos_info->time_prev_req = ktime_get_boottime_ns()/1000000;

				bw_final[0] = (qos_info->bw_total[0][0] * fps);
				bw_final[1] = (qos_info->bw_total[0][1] * fps);
				bw_final[2] = (qos_info->bw_total[1][0] * fps);
				bw_final[3] = (qos_info->bw_total[1][1] * fps);

				if (imgsys_qos_dbg_enable_plat7s())
					dev_info(qos_info->dev,
						 "imgsys_qos: ori frame_no:%d-sc0_r-%d sc0_w-%d, sc1_r-%d sc0_w-%d\n",
						 frm_info->frame_no,
						 bw_final[0], bw_final[1],
						 bw_final[2], bw_final[3]);

				qos_info->bw_avg[0][0] += bw_final[0];
				qos_info->bw_avg[0][1] += bw_final[1];
				qos_info->bw_avg[1][0] += bw_final[2];
				qos_info->bw_avg[1][1] += bw_final[3];
				qos_info->avg_cnt++;

				if (dvfs_info->vss_task_cnt == 0 &&
					((qos_info->avg_cnt >= step) ||
					 (qos_info->req_cnt <= 1))) {
					/* unit is MB/s */
					bw_final[0] = qos_info->bw_avg[0][0] / qos_info->avg_cnt;
					bw_final[1] = qos_info->bw_avg[0][1] / qos_info->avg_cnt;
					bw_final[2] = qos_info->bw_avg[1][0] / qos_info->avg_cnt;
					bw_final[3] = qos_info->bw_avg[1][1] / qos_info->avg_cnt;

					if (imgsys_qos_dbg_enable_plat7s())
						dev_info(qos_info->dev,
							 "imgsys_qos: avg frame_no:%d-sc0_r-%d sc0_w-%d, sc1_r-%d sc0_w-%d\n",
							 frm_info->frame_no,
							 bw_final[0], bw_final[1],
							 bw_final[2], bw_final[3]);

					qos_info->qos_path[IMGSYS_COMMON_0_R].bw = bw_final[0];
					qos_info->qos_path[IMGSYS_COMMON_0_W].bw = bw_final[1];
					qos_info->qos_path[IMGSYS_COMMON_1_R].bw = bw_final[2];
					qos_info->qos_path[IMGSYS_COMMON_1_W].bw = bw_final[3];

					bw_final[0] = (bw_final[0] * imgsys_qos_factor) >> 2;
					bw_final[1] = (bw_final[1] * imgsys_qos_factor) >> 2;
					bw_final[2] = (bw_final[2] * imgsys_qos_factor) >> 2;
					bw_final[3] = (bw_final[3] * imgsys_qos_factor) >> 2;

					if (imgsys_qos_dbg_enable_plat7s())
						dev_info(qos_info->dev,
							 "imgsys_qos: frame_no:%d-sc0_r-%d sc0_w-%d, sc1_r-%d sc0_w-%d\n",
							 frm_info->frame_no,
							 bw_final[0], bw_final[1],
							 bw_final[2], bw_final[3]);

					IMGSYS_CMDQ_SYSTRACE_BEGIN("SetQos");
#ifndef CONFIG_FPGA_EARLY_PORTING
					mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_R].path,
							MBps_to_icc(bw_final[0]),
							0);
					mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_W].path,
							MBps_to_icc(bw_final[1]),
							0);
					mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_R].path,
							MBps_to_icc(bw_final[2]),
							0);
					mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_W].path,
							MBps_to_icc(bw_final[3]),
							0);
#endif
					IMGSYS_CMDQ_SYSTRACE_END();
					qos_info->bw_avg[0][0] = 0;
					qos_info->bw_avg[0][1] = 0;
					qos_info->bw_avg[1][0] = 0;
					qos_info->bw_avg[1][1] = 0;
					qos_info->avg_cnt = 0;
				}

			}
		}
	}
}

void mtk_imgsys_mmqos_reset_plat7s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 dvfs_idx = 0, qos_idx = 0;
	struct mtk_imgsys_qos *qos_info = NULL;

	qos_info = &imgsys_dev->qos_info;

	qos_info->qos_path[IMGSYS_COMMON_0_R].bw = 0;
	qos_info->qos_path[IMGSYS_COMMON_0_W].bw = 0;

	qos_info->qos_path[IMGSYS_COMMON_1_R].bw = 0;
	qos_info->qos_path[IMGSYS_COMMON_1_W].bw = 0;

#ifndef CONFIG_FPGA_EARLY_PORTING
	mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_R].path, 0, 0);
	mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_0_W].path, 0, 0);
	mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_R].path, 0, 0);
	mtk_icc_set_bw(qos_info->qos_path[IMGSYS_COMMON_1_W].path, 0, 0);
#endif

	for (dvfs_idx = 0; dvfs_idx < MTK_IMGSYS_DVFS_GROUP; dvfs_idx++) {
		for (qos_idx = 0; qos_idx < MTK_IMGSYS_QOS_GROUP; qos_idx++) {
			qos_info->bw_total[dvfs_idx][qos_idx] = 0;
			qos_info->bw_avg[dvfs_idx][qos_idx] = 0;
		}
		qos_info->ts_total[dvfs_idx] = 0;
	}
	qos_info->req_cnt = 0;
	qos_info->avg_cnt = 0;
	qos_info->time_prev_req = 0;
	qos_info->isIdle = 0;

	if (imgsys_qos_update_freq == 0)
		imgsys_qos_update_freq = IMGSYS_QOS_UPDATE_FREQ;
	if (imgsys_qos_blank_int == 0)
		imgsys_qos_blank_int = IMGSYS_QOS_BLANK_INT;
	if (imgsys_qos_factor == 0)
		imgsys_qos_factor = IMGSYS_QOS_FACTOR;

}

void mtk_imgsys_mmdvfs_mmqos_cal_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet)
{
	struct mtk_imgsys_dvfs *dvfs_info = NULL;
	struct mtk_imgsys_qos *qos_info = NULL;
	unsigned long pixel_size[MTK_IMGSYS_DVFS_GROUP] = {0};
	int frm_num = 0, frm_idx = 0, g_idx;
	u32 hw_comb = 0;
	u32 batch_num = 0;
	u32 fps = 0;
	u32 bw_exe = 0;
	unsigned long freq = 0;
	#if IMGSYS_DVFS_ENABLE
	unsigned long pixel_max = 0, pixel_total_max = 0;
	/* struct timeval curr_time; */
	u64 ts_fps = 0;
	#endif

	dvfs_info = &imgsys_dev->dvfs_info;
	qos_info = &imgsys_dev->qos_info;
	frm_num = frm_info->total_frmnum;
	batch_num = frm_info->batchnum;
	fps = frm_info->fps;
	/* fps = (batch_num)?(frm_info->fps*batch_num):frm_info->fps; */
	bw_exe = fps;
	hw_comb = frm_info->user_info[0].hw_comb;

	/* Calculate DVFS*/
	if (fps != 0) {
		for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
			for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++) {
				if (frm_info->user_info[frm_idx].hw_comb & dvfs_group[g_idx].g_hw)
					pixel_size[g_idx] += frm_info->user_info[frm_idx].pixel_bw;
			}
		}
	}
	#if IMGSYS_DVFS_ENABLE
	/* Check current time */
	/* do_gettimeofday(&curr_time); */
	/* ts_curr = curr_time.tv_sec * 1000000 + curr_time.tv_usec; */
	/* ts_eq = frm_info->eqtime.tv_sec * 1000000 + frm_info->eqtime.tv_usec; */
	/* ts_sw = ts_curr - ts_eq; */
	if (fps != 0)
		ts_fps = 1000000 / fps;
	else
		ts_fps = 0;

	if (isSet == 1) {
		if (fps != 0) {
			for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++) {
				dvfs_info->pixel_size[g_idx] += (pixel_size[g_idx]*fps);
				if (pixel_size[g_idx] > pixel_max)
					pixel_max = pixel_size[g_idx];
				if (dvfs_info->pixel_size[g_idx] > pixel_total_max)
					pixel_total_max = dvfs_info->pixel_size[g_idx];
			}

			if (batch_num > 1) {
				dvfs_info->smvr_task_cnt++;
				if (pixel_total_max < IMGSYS_SMVR_FREQ_FLOOR)
					freq = IMGSYS_SMVR_FREQ_FLOOR;
				else
					freq = pixel_total_max;
			} else if (dvfs_info->smvr_task_cnt == 0)
				freq = pixel_total_max;

			if (dvfs_info->vss_task_cnt == 0)
				dvfs_info->freq = freq;
		} else {
			dvfs_info->vss_task_cnt++;
			freq = IMGSYS_VSS_FREQ_FLOOR; /* Forcing highest frequency if fps is 0 */
			if (freq > dvfs_info->freq)
				dvfs_info->freq = freq;
		}
	} else if (isSet == 0) {
		if (fps != 0) {
			for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++) {
				dvfs_info->pixel_size[g_idx] -= (pixel_size[g_idx]*fps);
				if (pixel_size[g_idx] > pixel_max)
					pixel_max = pixel_size[g_idx];
				if (dvfs_info->pixel_size[g_idx] > pixel_total_max)
					pixel_total_max = dvfs_info->pixel_size[g_idx];
			}

			if (batch_num > 1) {
				dvfs_info->smvr_task_cnt--;
				if (pixel_total_max < IMGSYS_SMVR_FREQ_FLOOR)
					freq = IMGSYS_SMVR_FREQ_FLOOR;
				else
					freq = pixel_total_max;
			} else if (dvfs_info->smvr_task_cnt == 0)
				freq = pixel_total_max;

			if (dvfs_info->vss_task_cnt == 0)
				dvfs_info->freq = freq;

			//if (pixel_total_max == 0) {
			//	freq = 0;
			//	dvfs_info->freq = freq;
			//}
		} else {
			dvfs_info->vss_task_cnt--;
			if (dvfs_info->vss_task_cnt == 0) {
				for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++)
					if (dvfs_info->pixel_size[g_idx] > pixel_total_max)
						pixel_total_max = dvfs_info->pixel_size[g_idx];
				freq = pixel_total_max;
				dvfs_info->freq = freq;
			}
		}
	}

	if (imgsys_dvfs_dbg_enable_plat7s())
		dev_info(qos_info->dev,
		"[%s] isSet(%d) fps(%d/%d) batchNum(%d) bw_exe(%d) vss(%d) smvr(%d) freq(%lld/%lld) local_pix_sz(%lld/%lld/%lld/%lld) global_pix_sz(%lld/%lld/%lld/%lld)\n",
		__func__, isSet, fps, frm_info->fps, batch_num, bw_exe,
		dvfs_info->vss_task_cnt, dvfs_info->smvr_task_cnt, freq, dvfs_info->freq,
		pixel_size[0], pixel_size[1], pixel_size[2], pixel_max,
		dvfs_info->pixel_size[0], dvfs_info->pixel_size[1],
		dvfs_info->pixel_size[2], pixel_total_max
		);
	#else
	if (isSet == 1) {
		for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++)
			dvfs_info->pixel_size[0] += pixel_size[g_idx];
		freq = 650000000;
		dvfs_info->freq = freq;
	} else if (isSet == 0) {
		for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++)
			dvfs_info->pixel_size[0] -= pixel_size[g_idx];
		if (dvfs_info->pixel_size[0] == 0) {
			freq = 0;
			dvfs_info->freq = freq;
		}
	}
	#endif

}

void mtk_imgsys_mmqos_bw_cal_plat7s(struct mtk_imgsys_dev *imgsys_dev,
					void *smi_port, uint32_t hw_comb,
					uint32_t port_st, uint32_t port_num, uint32_t port_id)
{
	struct mtk_imgsys_qos *qos_info = NULL;
	struct smi_port_t *smi = NULL;
	uint32_t port_idx = 0, g_idx = 0;

	qos_info = &imgsys_dev->qos_info;
	smi = (struct smi_port_t *)smi_port;
	for (port_idx = port_st; port_idx < (port_num + port_st); port_idx++)
		for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++)
			if (hw_comb & dvfs_group[g_idx].g_hw)
				qos_info->bw_total[g_idx][port_id] += smi[port_idx-port_st].portbw;
}

void mtk_imgsys_mmqos_ts_cal_plat7s(struct mtk_imgsys_dev *imgsys_dev,
				struct mtk_imgsys_cb_param *cb_param, uint32_t hw_comb)
{
	struct mtk_imgsys_qos *qos_info = NULL;
	uint32_t g_idx = 0;
	u64 ts_hw = 0;

	if (is_stream_off == 0) {
		qos_info = &imgsys_dev->qos_info;
		ts_hw = cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart;
		for (g_idx = 0; g_idx < MTK_IMGSYS_DVFS_GROUP; g_idx++)
			if (hw_comb & dvfs_group[g_idx].g_hw)
				qos_info->ts_total[g_idx] += ts_hw;
	}
}

void mtk_imgsys_power_ctrl_plat7s(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	u32 user_cnt = 0;
	int i;

	if (isPowerOn) {
		user_cnt = atomic_inc_return(&imgsys_dev->imgsys_user_cnt);
		if (user_cnt == 1) {
			if (!imgsys_quick_onoff_enable_plat7s())
				dev_info(dvfs_info->dev,
					"[%s] isPowerOn(%d) user(%d)\n",
					__func__, isPowerOn, user_cnt);

			mutex_lock(&(imgsys_dev->power_ctrl_lock));

			pm_runtime_get_sync(imgsys_dev->dev);

			/*set default value for hw module*/
			mtk_imgsys_mod_get(imgsys_dev);
			for (i = 0; i < (imgsys_dev->num_mods); i++)
				if (imgsys_dev->modules[i].set)
					imgsys_dev->modules[i].set(imgsys_dev);
			mutex_unlock(&(imgsys_dev->power_ctrl_lock));
		}
	} else {
		user_cnt = atomic_dec_return(&imgsys_dev->imgsys_user_cnt);
		if (user_cnt == 0) {
			if (!imgsys_quick_onoff_enable_plat7s())
				dev_info(dvfs_info->dev,
					"[%s] isPowerOn(%d) user(%d)\n",
					__func__, isPowerOn, user_cnt);

			mutex_lock(&(imgsys_dev->power_ctrl_lock));

			mtk_imgsys_mod_put(imgsys_dev);
			pm_runtime_put_sync(imgsys_dev->dev);
			//pm_runtime_mark_last_busy(imgsys_dev->dev);
			//pm_runtime_put_autosuspend(imgsys_dev->dev);

			mutex_unlock(&(imgsys_dev->power_ctrl_lock));
		}
	}
}

#endif

bool imgsys_cmdq_ts_enable_plat7s(void)
{
	return imgsys_cmdq_ts_en;
}

u32 imgsys_wpe_bwlog_enable_plat7s(void)
{
	return imgsys_wpe_bwlog_en;
}

bool imgsys_cmdq_ts_dbg_enable_plat7s(void)
{
	return imgsys_cmdq_ts_dbg_en;
}

bool imgsys_dvfs_dbg_enable_plat7s(void)
{
	return imgsys_dvfs_dbg_en;
}

bool imgsys_qos_dbg_enable_plat7s(void)
{
	return imgsys_qos_dbg_en;
}

bool imgsys_quick_onoff_enable_plat7s(void)
{
	return imgsys_quick_onoff_en;
}

bool imgsys_fence_dbg_enable_plat7s(void)
{
	return imgsys_fence_dbg_en;
}

bool imgsys_fine_grain_dvfs_enable_plat7s(void)
{
	return imgsys_fine_grain_dvfs_en;
}

struct imgsys_cmdq_cust_data imgsys_cmdq_data_7s = {
	.cmdq_init = imgsys_cmdq_init_plat7s,
	.cmdq_release = imgsys_cmdq_release_plat7s,
	.cmdq_streamon = imgsys_cmdq_streamon_plat7s,
	.cmdq_streamoff = imgsys_cmdq_streamoff_plat7s,
	.cmdq_sendtask = imgsys_cmdq_sendtask_plat7s,
	.cmdq_parser = imgsys_cmdq_parser_plat7s,
	.cmdq_sec_sendtask = imgsys_cmdq_sec_sendtask_plat7s,
	.cmdq_sec_cmd = imgsys_cmdq_sec_cmd_plat7s,
	.cmdq_clearevent = imgsys_cmdq_clearevent_plat7s,
#if DVFS_QOS_READY
	.mmdvfs_init = mtk_imgsys_mmdvfs_init_plat7s,
	.mmdvfs_uninit = mtk_imgsys_mmdvfs_uninit_plat7s,
	.mmdvfs_set = mtk_imgsys_mmdvfs_set_plat7s,
	.mmqos_init = mtk_imgsys_mmqos_init_plat7s,
	.mmqos_uninit = mtk_imgsys_mmqos_init_plat7s,
	.mmqos_set_by_scen = mtk_imgsys_mmqos_set_by_scen_plat7s,
	.mmqos_reset = mtk_imgsys_mmqos_reset_plat7s,
	.mmdvfs_mmqos_cal = mtk_imgsys_mmdvfs_mmqos_cal_plat7s,
	.mmqos_bw_cal = mtk_imgsys_mmqos_bw_cal_plat7s,
	.mmqos_ts_cal = mtk_imgsys_mmqos_ts_cal_plat7s,
	.power_ctrl = mtk_imgsys_power_ctrl_plat7s,
	.mmqos_monitor = mtk_imgsys_mmqos_monitor_plat7s,
#endif
	.cmdq_ts_en = imgsys_cmdq_ts_enable_plat7s,
	.wpe_bwlog_en = imgsys_wpe_bwlog_enable_plat7s,
	.cmdq_ts_dbg_en = imgsys_cmdq_ts_dbg_enable_plat7s,
	.dvfs_dbg_en = imgsys_dvfs_dbg_enable_plat7s,
	.quick_onoff_en = imgsys_quick_onoff_enable_plat7s,
};

