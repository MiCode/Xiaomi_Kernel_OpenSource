/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>

#include "msm.h"
#include "msm_vfe40.h"

struct vfe40_isr_queue_cmd {
	struct list_head list;
	uint32_t                           vfeInterruptStatus0;
	uint32_t                           vfeInterruptStatus1;
};

static const char * const vfe40_general_cmd[] = {
	"DUMMY_0",  /* 0 */
	"SET_CLK",
	"RESET",
	"START",
	"TEST_GEN_START",
	"OPERATION_CFG",  /* 5 */
	"AXI_OUT_CFG",
	"CAMIF_CFG",
	"AXI_INPUT_CFG",
	"BLACK_LEVEL_CFG",
	"ROLL_OFF_CFG",  /* 10 */
	"DEMUX_CFG",
	"FOV_CFG",
	"MAIN_SCALER_CFG",
	"WB_CFG",
	"COLOR_COR_CFG", /* 15 */
	"RGB_G_CFG",
	"LA_CFG",
	"CHROMA_EN_CFG",
	"CHROMA_SUP_CFG",
	"MCE_CFG", /* 20 */
	"SK_ENHAN_CFG",
	"ASF_CFG",
	"S2Y_CFG",
	"S2CbCr_CFG",
	"CHROMA_SUBS_CFG",  /* 25 */
	"OUT_CLAMP_CFG",
	"FRAME_SKIP_CFG",
	"DUMMY_1",
	"DUMMY_2",
	"DUMMY_3",  /* 30 */
	"UPDATE",
	"BL_LVL_UPDATE",
	"DEMUX_UPDATE",
	"FOV_UPDATE",
	"MAIN_SCALER_UPDATE",  /* 35 */
	"WB_UPDATE",
	"COLOR_COR_UPDATE",
	"RGB_G_UPDATE",
	"LA_UPDATE",
	"CHROMA_EN_UPDATE",  /* 40 */
	"CHROMA_SUP_UPDATE",
	"MCE_UPDATE",
	"SK_ENHAN_UPDATE",
	"S2CbCr_UPDATE",
	"S2Y_UPDATE",  /* 45 */
	"ASF_UPDATE",
	"FRAME_SKIP_UPDATE",
	"CAMIF_FRAME_UPDATE",
	"STATS_AF_UPDATE",
	"STATS_AE_UPDATE",  /* 50 */
	"STATS_AWB_UPDATE",
	"STATS_RS_UPDATE",
	"STATS_CS_UPDATE",
	"STATS_SKIN_UPDATE",
	"STATS_IHIST_UPDATE",  /* 55 */
	"DUMMY_4",
	"EPOCH1_ACK",
	"EPOCH2_ACK",
	"START_RECORDING",
	"STOP_RECORDING",  /* 60 */
	"DUMMY_5",
	"DUMMY_6",
	"CAPTURE",
	"DUMMY_7",
	"STOP",  /* 65 */
	"GET_HW_VERSION",
	"GET_FRAME_SKIP_COUNTS",
	"OUTPUT1_BUFFER_ENQ",
	"OUTPUT2_BUFFER_ENQ",
	"OUTPUT3_BUFFER_ENQ",  /* 70 */
	"JPEG_OUT_BUF_ENQ",
	"RAW_OUT_BUF_ENQ",
	"RAW_IN_BUF_ENQ",
	"STATS_AF_ENQ",
	"STATS_AE_ENQ",  /* 75 */
	"STATS_AWB_ENQ",
	"STATS_RS_ENQ",
	"STATS_CS_ENQ",
	"STATS_SKIN_ENQ",
	"STATS_IHIST_ENQ",  /* 80 */
	"DUMMY_8",
	"JPEG_ENC_CFG",
	"DUMMY_9",
	"STATS_AF_START",
	"STATS_AF_STOP",  /* 85 */
	"STATS_AE_START",
	"STATS_AE_STOP",
	"STATS_AWB_START",
	"STATS_AWB_STOP",
	"STATS_RS_START",  /* 90 */
	"STATS_RS_STOP",
	"STATS_CS_START",
	"STATS_CS_STOP",
	"STATS_SKIN_START",
	"STATS_SKIN_STOP",  /* 95 */
	"STATS_IHIST_START",
	"STATS_IHIST_STOP",
	"DUMMY_10",
	"SYNC_TIMER_SETTING",
	"ASYNC_TIMER_SETTING",  /* 100 */
	"LIVESHOT",
	"LA_SETUP",
	"LINEARIZATION_CFG",
	"DEMOSAICV3",
	"DEMOSAICV3_ABCC_CFG", /* 105 */
	"DEMOSAICV3_DBCC_CFG",
	"DEMOSAICV3_DBPC_CFG",
	"DEMOSAICV3_ABF_CFG",
	"DEMOSAICV3_ABCC_UPDATE",
	"DEMOSAICV3_DBCC_UPDATE", /* 110 */
	"DEMOSAICV3_DBPC_UPDATE",
	"XBAR_CFG",
	"EZTUNE_CFG",
	"V40_ZSL",
	"LINEARIZATION_UPDATE", /*115*/
	"DEMOSAICV3_ABF_UPDATE",
	"CLF_CFG",
	"CLF_LUMA_UPDATE",
	"CLF_CHROMA_UPDATE",
	"PCA_ROLL_OFF_CFG", /*120*/
	"PCA_ROLL_OFF_UPDATE",
	"GET_REG_DUMP",
	"GET_LINEARIZATON_TABLE",
	"GET_MESH_ROLLOFF_TABLE",
	"GET_PCA_ROLLOFF_TABLE", /*125*/
	"GET_RGB_G_TABLE",
	"GET_LA_TABLE",
	"DEMOSAICV3_UPDATE",
	"ACTIVE_REGION_CONFIG",
	"COLOR_PROCESSING_CONFIG", /*130*/
	"STATS_WB_AEC_CONFIG",
	"STATS_WB_AEC_UPDATE",
	"Y_GAMMA_CONFIG",
	"SCALE_OUTPUT1_CONFIG",
	"SCALE_OUTPUT2_CONFIG", /*135*/
	"CAPTURE_RAW",
	"STOP_LIVESHOT",
	"RECONFIG_VFE",
	"STATS_REQBUF_CFG",
	"STATS_ENQUEUEBUF_CFG",/*140*/
	"STATS_FLUSH_BUFQ_CFG",
	"FOV_ENC_CFG",
	"FOV_VIEW_CFG",
	"FOV_ENC_UPDATE",
	"FOV_VIEW_UPDATE",/*145*/
	"SCALER_ENC_CFG",
	"SCALER_VIEW_CFG",
	"SCALER_ENC_UPDATE",
	"SCALER_VIEW_UPDATE",
	"COLORXFORM_ENC_CFG",/*150*/
	"COLORXFORM_VIEW_CFG",
	"COLORXFORM_ENC_UPDATE",
	"COLORXFORM_VIEW_UPDATE",
};

static void vfe40_stop(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;

	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 0);

	/* for reset hw modules, and send msg when reset_irq comes.*/
	spin_lock_irqsave(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
	vfe40_ctrl->share_ctrl->stop_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);

	/* disable all interrupts.  */
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
			vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_camera_io_w(VFE_CLEAR_ALL_IRQ0,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(VFE_CLEAR_ALL_IRQ1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_1);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CMD);

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time. stop camif immediately. */
	msm_camera_io_w(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CAMIF_COMMAND);
}

void vfe40_subdev_notify(int id, int path, int image_mode,
	struct v4l2_subdev *sd, struct vfe_share_ctrl_t *share_ctrl)
{
	struct msm_vfe_resp rp;
	struct msm_frame_info frame_info;
	unsigned long flags = 0;
	spin_lock_irqsave(&share_ctrl->sd_notify_lock, flags);
	CDBG("%s: msgId = %d\n", __func__, id);
	memset(&rp, 0, sizeof(struct msm_vfe_resp));
	rp.evt_msg.type   = MSM_CAMERA_MSG;
	frame_info.image_mode = image_mode;
	frame_info.path = path;
	rp.evt_msg.data = &frame_info;
	rp.type	   = id;
	v4l2_subdev_notify(sd, NOTIFY_VFE_BUF_EVT, &rp);
	spin_unlock_irqrestore(&share_ctrl->sd_notify_lock, flags);
}

static void vfe40_reset_internal_variables(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	vfe40_ctrl->vfeImaskCompositePacked = 0;
	/* state control variables */
	vfe40_ctrl->start_ack_pending = FALSE;
	atomic_set(&vfe40_ctrl->share_ctrl->irq_cnt, 0);

	spin_lock_irqsave(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
	vfe40_ctrl->share_ctrl->stop_ack_pending  = FALSE;
	spin_unlock_irqrestore(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);

	vfe40_ctrl->reset_ack_pending  = FALSE;

	spin_lock_irqsave(&vfe40_ctrl->update_ack_lock, flags);
	vfe40_ctrl->update_ack_pending = FALSE;
	spin_unlock_irqrestore(&vfe40_ctrl->update_ack_lock, flags);

	vfe40_ctrl->recording_state = VFE_STATE_IDLE;
	vfe40_ctrl->share_ctrl->liveshot_state = VFE_STATE_IDLE;

	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 0);

	/* 0 for continuous mode, 1 for snapshot mode */
	vfe40_ctrl->share_ctrl->operation_mode = 0;
	vfe40_ctrl->share_ctrl->outpath.output_mode = 0;
	vfe40_ctrl->share_ctrl->vfe_capture_count = 0;

	/* this is unsigned 32 bit integer. */
	vfe40_ctrl->share_ctrl->vfeFrameId = 0;
	/* Stats control variables. */
	memset(&(vfe40_ctrl->afStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->awbStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->aecStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->ihistStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->rsStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->csStatsControl), 0,
		sizeof(struct vfe_stats_control));

	vfe40_ctrl->frame_skip_cnt = 31;
	vfe40_ctrl->frame_skip_pattern = 0xffffffff;
	vfe40_ctrl->snapshot_frame_cnt = 0;
}

static void vfe40_reset(struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_reset_internal_variables(vfe40_ctrl);
	/* disable all interrupts.  vfeImaskLocal is also reset to 0
	* to begin with. */
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_CMD);

	/* enable reset_ack interrupt.  */
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
	vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Write to VFE_GLOBAL_RESET_CMD to reset the vfe hardware. Once reset
	 * is done, hardware interrupt will be generated.  VFE ist processes
	 * the interrupt to complete the function call.  Note that the reset
	 * function is synchronous. */

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(VFE_RESET_UPON_RESET_CMD,
		vfe40_ctrl->share_ctrl->vfebase + VFE_GLOBAL_RESET);

	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_0);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_1);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_2);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_3);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_4);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_5);
	msm_camera_io_w(0xAAAAAAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_6);
	msm_camera_io_w(0x0002AAAA,
	vfe40_ctrl->share_ctrl->vfebase + VFE_0_BUS_BDG_QOS_CFG_7);
}

static int vfe40_operation_config(uint32_t *cmd,
			struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t *p = cmd;

	vfe40_ctrl->share_ctrl->operation_mode = *p;
	vfe40_ctrl->share_ctrl->stats_comp = *(++p);
	vfe40_ctrl->hfr_mode = *(++p);

	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_CFG);
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_RDI0_CFG);
	if (msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
		V40_GET_HW_VERSION_OFF) ==
		VFE40_HW_NUMBER) {
		msm_camera_io_w(*(++p),
			vfe40_ctrl->share_ctrl->vfebase + VFE_RDI1_CFG);
		msm_camera_io_w(*(++p),
			vfe40_ctrl->share_ctrl->vfebase + VFE_RDI2_CFG);
	}  else {
		++p;
		++p;
	}
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_REALIGN_BUF);
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_CHROMA_UP);
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_STATS_CFG);
	return 0;
}

static unsigned long vfe40_stats_dqbuf(struct vfe40_ctrl_type *vfe40_ctrl,
	enum msm_stats_enum_type stats_type)
{
	struct msm_stats_meta_buf *buf = NULL;
	int rc = 0;
	rc = vfe40_ctrl->stats_ops.dqbuf(
			vfe40_ctrl->stats_ops.stats_ctrl, stats_type, &buf);
	if (rc < 0) {
		pr_err("%s: dq stats buf (type = %d) err = %d",
			__func__, stats_type, rc);
		return 0L;
	}
	return buf->paddr;
}

static unsigned long vfe40_stats_flush_enqueue(
	struct vfe40_ctrl_type *vfe40_ctrl,
	enum msm_stats_enum_type stats_type)
{
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;
	int rc = 0;
	int i;

	/*
	 * Passing NULL for ion client as the buffers are already
	 * mapped at this stage, client is not required, flush all
	 * the buffers, and buffers move to PREPARE state
	 */

	rc = vfe40_ctrl->stats_ops.bufq_flush(
			vfe40_ctrl->stats_ops.stats_ctrl, stats_type, NULL);
	if (rc < 0) {
		pr_err("%s: dq stats buf (type = %d) err = %d",
			__func__, stats_type, rc);
		return 0L;
	}
	/* Queue all the buffers back to QUEUED state */
	bufq = vfe40_ctrl->stats_ctrl.bufq[stats_type];
	for (i = 0; i < bufq->num_bufs; i++) {
		stats_buf = &bufq->bufs[i];
		rc = vfe40_ctrl->stats_ops.enqueue_buf(
				vfe40_ctrl->stats_ops.stats_ctrl,
				&(stats_buf->info), NULL);
		if (rc < 0) {
			pr_err("%s: dq stats buf (type = %d) err = %d",
				 __func__, stats_type, rc);
			return rc;
		}
	}
	return 0L;
}

static int vfe_stats_awb_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl,
	struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AWB);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq awb ping buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AWB_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AWB);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq awb ping buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	return 0;
}

static int vfe_stats_aec_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl, struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AEC);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq aec ping buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AEC_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AEC);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq aec pong buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AEC_WR_PONG_ADDR);
	return 0;
}

static int vfe_stats_af_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl, struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	rc = vfe40_stats_flush_enqueue(vfe40_ctrl, MSM_STATS_TYPE_AF);
	if (rc < 0) {
		pr_err("%s: dq stats buf err = %d",
			   __func__, rc);
		spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
		return -EINVAL;
	}
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AF);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq af ping buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AF_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AF);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq af pong buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AF_WR_PONG_ADDR);

	return 0;
}

static int vfe_stats_ihist_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl, struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_IHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq ihist ping buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_HIST_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_IHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq ihist pong buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_HIST_WR_PONG_ADDR);

	return 0;
}

static int vfe_stats_rs_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl, struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_RS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq rs ping buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_RS_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_RS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq rs pong buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_RS_WR_PONG_ADDR);
	return 0;
}

static int vfe_stats_cs_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl, struct vfe_cmd_stats_buf *in)
{
	uint32_t addr;
	unsigned long flags;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_CS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq cs ping buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_CS_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_CS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq cs pong buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_CS_WR_PONG_ADDR);
	return 0;
}

static void vfe40_start_common(struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t irq_mask = 0x1E000011;
	vfe40_ctrl->start_ack_pending = TRUE;
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe40_ctrl->share_ctrl->operation_mode,
		vfe40_ctrl->share_ctrl->outpath.output_mode);

	msm_camera_io_w(irq_mask,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CAMIF_COMMAND);

	msm_camera_io_dump(vfe40_ctrl->share_ctrl->vfebase,
		vfe40_ctrl->share_ctrl->register_total*4);

	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 1);
}

static int vfe40_start_recording(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_VIDEO);
	vfe40_ctrl->recording_state = VFE_STATE_START_REQUESTED;
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe40_stop_recording(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_ctrl->recording_state = VFE_STATE_STOP_REQUESTED;
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	return 0;
}

static void vfe40_start_liveshot(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* Hardcode 1 live snapshot for now. */
	vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt = 1;
	vfe40_ctrl->share_ctrl->vfe_capture_count =
		vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt;

	vfe40_ctrl->share_ctrl->liveshot_state = VFE_STATE_START_REQUESTED;
	msm_camera_io_w_mb(1, vfe40_ctrl->
		share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static int vfe40_zsl(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	irq_comp_mask	=
		msm_camera_io_r(vfe40_ctrl->
		share_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	CDBG("%s:op mode %d O/P Mode %d\n", __func__,
		vfe40_ctrl->share_ctrl->operation_mode,
		vfe40_ctrl->share_ctrl->outpath.output_mode);

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
		VFE40_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch0)) |
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch1)));
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= (
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch0)) |
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch1)) |
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch2)));
	}

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
		VFE40_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= ((0x1 << (vfe40_ctrl->
				share_ctrl->outpath.out1.ch0 + 8)) |
			(0x1 << (vfe40_ctrl->
				share_ctrl->outpath.out1.ch1 + 8)));
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			   VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= (
			(0x1 << (vfe40_ctrl->
				share_ctrl->outpath.out1.ch0 + 8)) |
			(0x1 << (vfe40_ctrl->
				share_ctrl->outpath.out1.ch1 + 8)) |
			(0x1 << (vfe40_ctrl->
				share_ctrl->outpath.out1.ch2 + 8)));
	}

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch0]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch1]);
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch0]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch1]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch2]);
	}

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out1.ch0]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out1.ch1]);
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out1.ch0]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out1.ch1]);
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out1.ch2]);
	}

	msm_camera_io_w(irq_comp_mask,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe40_start_common(vfe40_ctrl);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_ZSL);

	msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase + 0x18C);
	msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase + 0x188);
	return 0;
}
static int vfe40_capture_raw(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;

	vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt = num_frames_capture;
	vfe40_ctrl->share_ctrl->vfe_capture_count = num_frames_capture;

	irq_comp_mask	=
		msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
		VFE40_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |=
			(0x1 << (vfe40_ctrl->share_ctrl->outpath.out0.ch0));
		msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
			share_ctrl->outpath.out0.ch0]);
	}

	msm_camera_io_w(irq_comp_mask,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_CAPTURE);
	vfe40_start_common(vfe40_ctrl);
	return 0;
}

static int vfe40_capture(
	struct msm_cam_media_controller *pmctl,
	uint32_t num_frames_capture,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t irq_comp_mask = 0;

	/* capture command is valid for both idle and active state. */
	vfe40_ctrl->share_ctrl->outpath.out1.capture_cnt = num_frames_capture;
	if (vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG) {
		vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt =
			num_frames_capture;
	}

	vfe40_ctrl->share_ctrl->vfe_capture_count = num_frames_capture;
	irq_comp_mask = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN) {
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			irq_comp_mask |= (0x1 << vfe40_ctrl->
				share_ctrl->outpath.out0.ch0 |
				0x1 << vfe40_ctrl->
				share_ctrl->outpath.out0.ch1);
		}
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
			irq_comp_mask |=
				(0x1 << (vfe40_ctrl->
					share_ctrl->outpath.out1.ch0 + 8) |
				0x1 << (vfe40_ctrl->
					share_ctrl->outpath.out1.ch1 + 8));
		}
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch1]);
		}
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch1]);
		}
	}

	vfe40_ctrl->share_ctrl->vfe_capture_count = num_frames_capture;

	msm_camera_io_w(irq_comp_mask,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_CAPTURE);

	vfe40_start_common(vfe40_ctrl);
	/* for debug */
	msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase + 0x18C);
	msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase + 0x188);
	return 0;
}

static int vfe40_start(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t irq_comp_mask = 0;
	irq_comp_mask	=
		msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			VFE_IRQ_COMP_MASK);

	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (
			0x1 << vfe40_ctrl->share_ctrl->outpath.out0.ch0 |
			0x1 << vfe40_ctrl->share_ctrl->outpath.out0.ch1);
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			   VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= (
			0x1 << vfe40_ctrl->share_ctrl->outpath.out0.ch0 |
			0x1 << vfe40_ctrl->share_ctrl->outpath.out0.ch1 |
			0x1 << vfe40_ctrl->share_ctrl->outpath.out0.ch2);
	}
	if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= (
			0x1 << (vfe40_ctrl->share_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe40_ctrl->share_ctrl->outpath.out1.ch1 + 8));
	} else if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= (
			0x1 << (vfe40_ctrl->share_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe40_ctrl->share_ctrl->outpath.out1.ch1 + 8) |
			0x1 << (vfe40_ctrl->share_ctrl->outpath.out1.ch2 + 8));
	}
	msm_camera_io_w(irq_comp_mask,
		vfe40_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	/*
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);*/
	vfe40_start_common(vfe40_ctrl);
	return 0;
}

static void vfe40_update(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t value = 0;
	if (vfe40_ctrl->update_linear) {
		if (!msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_LINEARIZATION_OFF1))
			msm_camera_io_w(1,
				vfe40_ctrl->share_ctrl->vfebase +
				V40_LINEARIZATION_OFF1);
		else
			msm_camera_io_w(0,
				vfe40_ctrl->share_ctrl->vfebase +
				V40_LINEARIZATION_OFF1);
		vfe40_ctrl->update_linear = false;
	}

	if (vfe40_ctrl->update_la) {
		if (!msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_LA_OFF))
			msm_camera_io_w(1,
				vfe40_ctrl->share_ctrl->vfebase + V40_LA_OFF);
		else
			msm_camera_io_w(0,
				vfe40_ctrl->share_ctrl->vfebase + V40_LA_OFF);
		vfe40_ctrl->update_la = false;
	}

	if (vfe40_ctrl->update_gamma) {
		value = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_RGB_G_OFF);
		value ^= V40_GAMMA_LUT_BANK_SEL_MASK;
		msm_camera_io_w(value,
			vfe40_ctrl->share_ctrl->vfebase + V40_RGB_G_OFF);
		vfe40_ctrl->update_gamma = false;
	}

	spin_lock_irqsave(&vfe40_ctrl->update_ack_lock, flags);
	vfe40_ctrl->update_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe40_ctrl->update_ack_lock, flags);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return;
}

static void vfe40_sync_timer_stop(struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t value = 0;
	vfe40_ctrl->sync_timer_state = 0;
	if (vfe40_ctrl->sync_timer_number == 0)
		value = 0x10000;
	else if (vfe40_ctrl->sync_timer_number == 1)
		value = 0x20000;
	else if (vfe40_ctrl->sync_timer_number == 2)
		value = 0x40000;

	/* Timer Stop */
	msm_camera_io_w(value,
		vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_OFF);
}

static void vfe40_sync_timer_start(
	const uint32_t *tbl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* set bit 8 for auto increment. */
	uint32_t value = 1;
	uint32_t val;

	vfe40_ctrl->sync_timer_state = *tbl++;
	vfe40_ctrl->sync_timer_repeat_count = *tbl++;
	vfe40_ctrl->sync_timer_number = *tbl++;
	CDBG("%s timer_state %d, repeat_cnt %d timer number %d\n",
		 __func__, vfe40_ctrl->sync_timer_state,
		 vfe40_ctrl->sync_timer_repeat_count,
		 vfe40_ctrl->sync_timer_number);

	if (vfe40_ctrl->sync_timer_state) { /* Start Timer */
		value = value << vfe40_ctrl->sync_timer_number;
	} else { /* Stop Timer */
		CDBG("Failed to Start timer\n");
		return;
	}

	/* Timer Start */
	msm_camera_io_w(value,
		vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_OFF);
	/* Sync Timer Line Start */
	value = *tbl++;
	msm_camera_io_w(value,
		vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_OFF +
		4 + ((vfe40_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Start */
	value = *tbl++;
	msm_camera_io_w(value,
			vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_OFF +
			 8 + ((vfe40_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Duration */
	value = *tbl++;
	val = vfe40_ctrl->share_ctrl->vfe_clk_rate / 10000;
	val = 10000000 / val;
	val = value * 10000 / val;
	CDBG("%s: Pixel Clk Cycles!!! %d\n", __func__, val);
	msm_camera_io_w(val,
		vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_OFF +
		12 + ((vfe40_ctrl->sync_timer_number) * 12));
	/* Timer0 Active High/LOW */
	value = *tbl++;
	msm_camera_io_w(value,
		vfe40_ctrl->share_ctrl->vfebase + V40_SYNC_TIMER_POLARITY_OFF);
	/* Selects sync timer 0 output to drive onto timer1 port */
	value = 0;
	msm_camera_io_w(value,
		vfe40_ctrl->share_ctrl->vfebase + V40_TIMER_SELECT_OFF);
}

static void vfe40_program_dmi_cfg(
	enum VFE40_DMI_RAM_SEL bankSel,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* set bit 8 for auto increment. */
	uint32_t value = VFE_DMI_CFG_DEFAULT;
	value += (uint32_t)bankSel;
	CDBG("%s: banksel = %d\n", __func__, bankSel);

	msm_camera_io_w(value, vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_CFG);
	/* by default, always starts with offset 0.*/
	msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_ADDR);
}
static void vfe40_write_gamma_cfg(
	enum VFE40_DMI_RAM_SEL channel_sel,
	const uint32_t *tbl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	int i;
	uint32_t value, value1, value2;
	vfe40_program_dmi_cfg(channel_sel, vfe40_ctrl);
	for (i = 0 ; i < (VFE40_GAMMA_NUM_ENTRIES/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_camera_io_w((value1),
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_camera_io_w((value2),
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
}

static void vfe40_read_gamma_cfg(
	enum VFE40_DMI_RAM_SEL channel_sel,
	uint32_t *tbl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	int i;
	vfe40_program_dmi_cfg(channel_sel, vfe40_ctrl);
	CDBG("%s: Gamma table channel: %d\n", __func__, channel_sel);
	for (i = 0 ; i < VFE40_GAMMA_NUM_ENTRIES ; i++) {
		*tbl = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
		CDBG("%s: %08x\n", __func__, *tbl);
		tbl++;
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
}

static void vfe40_write_la_cfg(
	enum VFE40_DMI_RAM_SEL channel_sel,
	const uint32_t *tbl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t i;
	uint32_t value, value1, value2;

	vfe40_program_dmi_cfg(channel_sel, vfe40_ctrl);
	for (i = 0 ; i < (VFE40_LA_TABLE_LENGTH/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_camera_io_w((value1),
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_camera_io_w((value2),
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
}

struct vfe40_output_ch *vfe40_get_ch(
	int path, struct vfe_share_ctrl_t *share_ctrl)
{
	struct vfe40_output_ch *ch = NULL;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		ch = &share_ctrl->outpath.out0;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		ch = &share_ctrl->outpath.out1;
	else
		pr_err("%s: Invalid path %d\n", __func__,
			path);

	BUG_ON(ch == NULL);
	return ch;
}

static int vfe40_configure_pingpong_buffers(
	int id, int path, struct vfe40_ctrl_type *vfe40_ctrl)
{
	struct vfe40_output_ch *outch = NULL;
	int rc = 0;
	uint32_t image_mode = 0;
	if (path == VFE_MSG_OUTPUT_PRIMARY)
		image_mode = vfe40_ctrl->share_ctrl->outpath.out0.image_mode;
	else
		image_mode = vfe40_ctrl->share_ctrl->outpath.out1.image_mode;

	vfe40_subdev_notify(id, path, image_mode,
		&vfe40_ctrl->subdev, vfe40_ctrl->share_ctrl);
	outch = vfe40_get_ch(path, vfe40_ctrl->share_ctrl);
	if (outch->ping.ch_paddr[0] && outch->pong.ch_paddr[0]) {
		/* Configure Preview Ping Pong */
		CDBG("%s Configure ping/pong address for %d",
						__func__, path);
		vfe40_put_ch_ping_addr(
			vfe40_ctrl->share_ctrl->vfebase, outch->ch0,
			outch->ping.ch_paddr[0]);
		vfe40_put_ch_pong_addr(
			vfe40_ctrl->share_ctrl->vfebase, outch->ch0,
			outch->pong.ch_paddr[0]);

		if (vfe40_ctrl->share_ctrl->operation_mode !=
			VFE_OUTPUTS_RAW) {
			vfe40_put_ch_ping_addr(
				vfe40_ctrl->share_ctrl->vfebase, outch->ch1,
				outch->ping.ch_paddr[1]);
			vfe40_put_ch_pong_addr(
				vfe40_ctrl->share_ctrl->vfebase, outch->ch1,
				outch->pong.ch_paddr[1]);
		}

		if (outch->ping.num_planes > 2)
			vfe40_put_ch_ping_addr(
				vfe40_ctrl->share_ctrl->vfebase, outch->ch2,
				outch->ping.ch_paddr[2]);
		if (outch->pong.num_planes > 2)
			vfe40_put_ch_pong_addr(
				vfe40_ctrl->share_ctrl->vfebase, outch->ch2,
				outch->pong.ch_paddr[2]);

		/* avoid stale info */
		memset(&outch->ping, 0, sizeof(struct msm_free_buf));
		memset(&outch->pong, 0, sizeof(struct msm_free_buf));
	} else {
		pr_err("%s ping/pong addr is null!!", __func__);
		rc = -EINVAL;
	}
	return rc;
}

static void vfe40_write_linear_cfg(
	enum VFE40_DMI_RAM_SEL channel_sel,
	const uint32_t *tbl, struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t i;

	vfe40_program_dmi_cfg(channel_sel, vfe40_ctrl);
	/* for loop for configuring LUT. */
	for (i = 0 ; i < VFE40_LINEARIZATON_TABLE_LENGTH ; i++) {
		msm_camera_io_w(*tbl,
			vfe40_ctrl->share_ctrl->vfebase + VFE_DMI_DATA_LO);
		tbl++;
	}
	CDBG("done writing to linearization table\n");
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
}

void vfe40_send_isp_msg(
	struct v4l2_subdev *sd,
	uint32_t vfeFrameId,
	uint32_t isp_msg_id)
{
	struct isp_msg_event isp_msg_evt;

	isp_msg_evt.msg_id = isp_msg_id;
	isp_msg_evt.sof_count = vfeFrameId;
	v4l2_subdev_notify(sd,
			NOTIFY_ISP_MSG_EVT,
			(void *)&isp_msg_evt);
}

static int vfe40_proc_general(
	struct msm_cam_media_controller *pmctl,
	struct msm_isp_cmd *cmd,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	int i , rc = 0;
	uint32_t old_val = 0 , new_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;
	uint32_t temp1 = 0, temp2 = 0;

	CDBG("vfe40_proc_general: cmdID = %s, length = %d\n",
		vfe40_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case VFE_CMD_RESET:
		CDBG("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		vfe40_reset(vfe40_ctrl);
		break;
	case VFE_CMD_START:
		CDBG("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		if ((vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) ||
				(vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW))
			/* Configure primary channel */
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_START, VFE_MSG_OUTPUT_PRIMARY,
				vfe40_ctrl);
		else
			/* Configure secondary channel */
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_START, VFE_MSG_OUTPUT_SECONDARY,
				vfe40_ctrl);
		if (rc < 0) {
			pr_err(
				"%s error configuring pingpong buffers for preview",
				__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}

		rc = vfe40_start(pmctl, vfe40_ctrl);
		break;
	case VFE_CMD_UPDATE:
		vfe40_update(vfe40_ctrl);
		break;
	case VFE_CMD_CAPTURE_RAW:
		CDBG("%s: cmdID = VFE_CMD_CAPTURE_RAW\n", __func__);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe40_configure_pingpong_buffers(
			VFE_MSG_CAPTURE, VFE_MSG_OUTPUT_PRIMARY,
			vfe40_ctrl);
		if (rc < 0) {
			pr_err(
				"%s error configuring pingpong buffers for snapshot",
				__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe40_capture_raw(pmctl, vfe40_ctrl, snapshot_cnt);
		break;
	case VFE_CMD_CAPTURE:
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}

		if (vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG) {
			if (snapshot_cnt != 1) {
				pr_err("only support 1 inline snapshot\n");
				rc = -EINVAL;
				goto proc_general_done;
			}
			/* Configure primary channel for JPEG */
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_JPEG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY,
				vfe40_ctrl);
		} else {
			/* Configure primary channel */
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY,
				vfe40_ctrl);
		}
		if (rc < 0) {
			pr_err(
			"%s error configuring pingpong buffers for primary output",
			__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		/* Configure secondary channel */
		rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_CAPTURE, VFE_MSG_OUTPUT_SECONDARY,
				vfe40_ctrl);
		if (rc < 0) {
			pr_err(
			"%s error configuring pingpong buffers for secondary output",
			__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe40_capture(pmctl, snapshot_cnt, vfe40_ctrl);
		break;
	case VFE_CMD_START_RECORDING:
		CDBG("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		if (vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_PREVIEW_AND_VIDEO)
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_START_RECORDING,
				VFE_MSG_OUTPUT_SECONDARY,
				vfe40_ctrl);
		else if (vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_VIDEO_AND_PREVIEW)
			rc = vfe40_configure_pingpong_buffers(
				VFE_MSG_START_RECORDING,
				VFE_MSG_OUTPUT_PRIMARY,
				vfe40_ctrl);
		if (rc < 0) {
			pr_err(
				"%s error configuring pingpong buffers for video\n",
				__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe40_start_recording(pmctl, vfe40_ctrl);
		break;
	case VFE_CMD_STOP_RECORDING:
		CDBG("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		rc = vfe40_stop_recording(pmctl, vfe40_ctrl);
		break;
	case VFE_CMD_OPERATION_CFG: {
		if (cmd->length != V40_OPERATION_CFG_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V40_OPERATION_CFG_LEN, GFP_ATOMIC);
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			V40_OPERATION_CFG_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe40_operation_config(cmdp, vfe40_ctrl);
		}
		break;

	case VFE_CMD_STATS_AE_START: {
		rc = vfe_stats_aec_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of AEC",
				 __func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= BG_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;
	case VFE_CMD_STATS_AF_START: {
		rc = vfe_stats_af_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of AF",
				__func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		old_val = msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			VFE_MODULE_CFG);
		old_val |= BF_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_STATS_AWB_START: {
		rc = vfe_stats_awb_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of AWB",
				 __func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AWB_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_STATS_IHIST_START: {
		rc = vfe_stats_ihist_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of IHIST",
				 __func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= IHIST_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;


	case VFE_CMD_STATS_RS_START: {
		rc = vfe_stats_rs_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of RS",
				__func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_STATS_CS_START: {
		rc = vfe_stats_cs_buf_init(vfe40_ctrl, NULL);
		if (rc < 0) {
			pr_err("%s: cannot config ping/pong address of CS",
				__func__);
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_MCE_UPDATE:
	case VFE_CMD_MCE_CFG:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		/* Incrementing with 4 so as to point to the 2nd Register as
		the 2nd register has the mce_enable bit */
		old_val = msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 4);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;
		old_val &= MCE_EN_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 4, &new_val, 4);
		cmdp_local += 1;

		old_val = msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 8, &new_val, 4);
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp_local, (vfe40_cmd[cmd->id].length));
		}
		break;
	case VFE_CMD_CHROMA_SUP_UPDATE:
	case VFE_CMD_CHROMA_SUP_CFG:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF, cmdp_local, 4);

		cmdp_local += 1;
		new_val = *cmdp_local;
		/* Incrementing with 4 so as to point to the 2nd Register as
		 * the 2nd register has the mce_enable bit
		 */
		old_val = msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 4);
		old_val &= ~MCE_EN_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 4, &new_val, 4);
		cmdp_local += 1;

		old_val = msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= ~MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_CHROMA_SUP_OFF + 8, &new_val, 4);
		}
		break;
	case VFE_CMD_BLACK_LEVEL_CFG:
		rc = -EFAULT;
		goto proc_general_done;

	case VFE_CMD_LA_CFG:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {

			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp_local, (vfe40_cmd[cmd->id].length));

		cmdp_local += 1;
		vfe40_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0,
						   cmdp_local, vfe40_ctrl);
		break;

	case VFE_CMD_LA_UPDATE: {
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {

			rc = -EFAULT;
			goto proc_general_done;
		}

		cmdp_local = cmdp + 1;
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_LA_OFF);
		if (old_val != 0x0)
			vfe40_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0,
				cmdp_local, vfe40_ctrl);
		else
			vfe40_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK1,
				cmdp_local, vfe40_ctrl);
		}
		vfe40_ctrl->update_la = true;
		break;

	case VFE_CMD_GET_LA_TABLE:
		temp1 = sizeof(uint32_t) * VFE40_LA_TABLE_LENGTH / 2;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kzalloc(temp1, GFP_KERNEL);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		if (msm_camera_io_r(vfe40_ctrl->
				share_ctrl->vfebase + V40_LA_OFF))
			vfe40_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK1,
						vfe40_ctrl);
		else
			vfe40_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK0,
						vfe40_ctrl);
		for (i = 0 ; i < (VFE40_LA_TABLE_LENGTH / 2) ; i++) {
			*cmdp_local =
				msm_camera_io_r(
					vfe40_ctrl->share_ctrl->vfebase +
					VFE_DMI_DATA_LO);
			*cmdp_local |= (msm_camera_io_r(
				vfe40_ctrl->share_ctrl->vfebase +
				VFE_DMI_DATA_LO)) << 16;
			cmdp_local++;
		}
		vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_SK_ENHAN_CFG:
	case VFE_CMD_SK_ENHAN_UPDATE:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_SCE_OFF,
			cmdp, V40_SCE_LEN);
		}
		break;

	case VFE_CMD_LIVESHOT:
		/* Configure primary channel */
		rc = vfe40_configure_pingpong_buffers(VFE_MSG_CAPTURE,
					VFE_MSG_OUTPUT_PRIMARY, vfe40_ctrl);
		if (rc < 0) {
			pr_err(
			"%s error configuring pingpong buffers for primary output\n",
			__func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		vfe40_start_liveshot(pmctl, vfe40_ctrl);
		break;

	case VFE_CMD_LINEARIZATION_CFG:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp, (void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_LINEARIZATION_OFF1,
			cmdp_local, V40_LINEARIZATION_LEN1);

		cmdp_local = cmdp + 17;
		vfe40_write_linear_cfg(BLACK_LUT_RAM_BANK0,
					cmdp_local, vfe40_ctrl);
		break;

	case VFE_CMD_LINEARIZATION_UPDATE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp, (void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		cmdp_local++;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_LINEARIZATION_OFF1 + 4,
			cmdp_local, (V40_LINEARIZATION_LEN1 - 4));
		cmdp_local = cmdp + 17;
		/*extracting the bank select*/
		old_val = msm_camera_io_r(
				vfe40_ctrl->share_ctrl->vfebase +
				V40_LINEARIZATION_OFF1);

		if (old_val != 0x0)
			vfe40_write_linear_cfg(BLACK_LUT_RAM_BANK0,
						cmdp_local, vfe40_ctrl);
		else
			vfe40_write_linear_cfg(BLACK_LUT_RAM_BANK1,
						cmdp_local, vfe40_ctrl);
		vfe40_ctrl->update_linear = true;
		break;

	case VFE_CMD_GET_LINEARIZATON_TABLE:
		temp1 = sizeof(uint32_t) * VFE40_LINEARIZATON_TABLE_LENGTH;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kzalloc(temp1, GFP_KERNEL);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		if (msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_LINEARIZATION_OFF1))
			vfe40_program_dmi_cfg(BLACK_LUT_RAM_BANK1, vfe40_ctrl);
		else
			vfe40_program_dmi_cfg(BLACK_LUT_RAM_BANK0, vfe40_ctrl);
		CDBG("%s: Linearization Table\n", __func__);
		for (i = 0 ; i < VFE40_LINEARIZATON_TABLE_LENGTH ; i++) {
			*cmdp_local = msm_camera_io_r(
				vfe40_ctrl->share_ctrl->vfebase +
				VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_DEMOSAICV3:
		if (cmd->length !=
			V40_DEMOSAICV3_0_LEN+V40_DEMOSAICV3_1_LEN) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF,
			cmdp_local, V40_DEMOSAICV3_0_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_1_OFF,
			cmdp_local, V40_DEMOSAICV3_1_LEN);
		break;

	case VFE_CMD_DEMOSAICV3_UPDATE:
		if (cmd->length !=
			V40_DEMOSAICV3_0_LEN * V40_DEMOSAICV3_UP_REG_CNT) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF,
			cmdp_local, V40_DEMOSAICV3_0_LEN);
		/* As the address space is not contiguous increment by 2
		 * before copying to next address space */
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_1_OFF,
			cmdp_local, 2 * V40_DEMOSAICV3_0_LEN);
		/* As the address space is not contiguous increment by 2
		 * before copying to next address space */
		cmdp_local += 2;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_2_OFF,
			cmdp_local, 2 * V40_DEMOSAICV3_0_LEN);
		break;

	case VFE_CMD_DEMOSAICV3_ABCC_CFG:
		rc = -EFAULT;
		break;

	case VFE_CMD_DEMOSAICV3_ABF_UPDATE:/* 116 ABF update  */
	case VFE_CMD_DEMOSAICV3_ABF_CFG: { /* 108 ABF config  */
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF);
		old_val &= ABF_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF,
			cmdp_local, 4);

		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp_local, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_DEMOSAICV3_DBCC_CFG:
	case VFE_CMD_DEMOSAICV3_DBCC_UPDATE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF);
		old_val &= DBCC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF,
			cmdp_local, 4);
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp_local, (vfe40_cmd[cmd->id].length));
		break;

	case VFE_CMD_DEMOSAICV3_DBPC_CFG:
	case VFE_CMD_DEMOSAICV3_DBPC_UPDATE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		new_val = *cmdp_local;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_DEMOSAICV3_0_OFF);
		old_val &= DBPC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_DEMOSAICV3_0_OFF,
			cmdp_local, V40_DEMOSAICV3_0_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_DEMOSAICV3_DBPC_CFG_OFF,
			cmdp_local, V40_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_DEMOSAICV3_DBPC_CFG_OFF0,
			cmdp_local, V40_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_DEMOSAICV3_DBPC_CFG_OFF1,
			cmdp_local, V40_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(vfe40_ctrl->share_ctrl->vfebase +
			V40_DEMOSAICV3_DBPC_CFG_OFF2,
			cmdp_local, V40_DEMOSAICV3_DBPC_LEN);
		break;

	case VFE_CMD_RGB_G_CFG: {
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase + V40_RGB_G_OFF,
			cmdp, 4);
		cmdp += 1;

		vfe40_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0, cmdp, vfe40_ctrl);
		vfe40_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0, cmdp, vfe40_ctrl);
		vfe40_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0, cmdp, vfe40_ctrl);
		}
	    cmdp -= 1;
		break;

	case VFE_CMD_RGB_G_UPDATE: {
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp, (void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_RGB_G_OFF);
		cmdp += 1;
		if (old_val != 0x0) {
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH0_BANK0, cmdp, vfe40_ctrl);
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH1_BANK0, cmdp, vfe40_ctrl);
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH2_BANK0, cmdp, vfe40_ctrl);
		} else {
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH0_BANK1, cmdp, vfe40_ctrl);
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH1_BANK1, cmdp, vfe40_ctrl);
			vfe40_write_gamma_cfg(
				RGBLUT_RAM_CH2_BANK1, cmdp, vfe40_ctrl);
		}
		}
		vfe40_ctrl->update_gamma = TRUE;
		cmdp -= 1;
		break;

	case VFE_CMD_GET_RGB_G_TABLE:
		temp1 = sizeof(uint32_t) * VFE40_GAMMA_NUM_ENTRIES * 3;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kzalloc(temp1, GFP_KERNEL);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		cmdp_local = cmdp;

		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + V40_RGB_G_OFF);
		temp2 = old_val ? RGBLUT_RAM_CH0_BANK1 :
			RGBLUT_RAM_CH0_BANK0;
		for (i = 0; i < 3; i++) {
			vfe40_read_gamma_cfg(temp2,
				cmdp_local + (VFE40_GAMMA_NUM_ENTRIES * i),
				vfe40_ctrl);
			temp2 += 2;
		}
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;

	case VFE_CMD_STATS_AWB_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AWB_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_AE_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~BG_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STATS_AF_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~BF_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		rc = vfe40_stats_flush_enqueue(vfe40_ctrl, MSM_STATS_TYPE_AF);
		if (rc < 0) {
			pr_err("%s: dq stats buf err = %d",
				   __func__, rc);
			return -EINVAL;
		}
		}
		break;

	case VFE_CMD_STATS_IHIST_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~IHIST_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_RS_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~RS_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_CS_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~CS_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STOP:
		CDBG("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		vfe40_stop(vfe40_ctrl);
		break;

	case VFE_CMD_SYNC_TIMER_SETTING:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp, (void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		vfe40_sync_timer_start(cmdp, vfe40_ctrl);
		break;

	case VFE_CMD_MODULE_CFG: {
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		*cmdp &= ~STATS_ENABLE_MASK;
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= STATS_ENABLE_MASK;
		*cmdp |= old_val;

		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_ZSL:
		rc = vfe40_configure_pingpong_buffers(VFE_MSG_START,
			VFE_MSG_OUTPUT_PRIMARY, vfe40_ctrl);
		if (rc < 0)
			goto proc_general_done;
		rc = vfe40_configure_pingpong_buffers(VFE_MSG_START,
			VFE_MSG_OUTPUT_SECONDARY, vfe40_ctrl);
		if (rc < 0)
			goto proc_general_done;

		rc = vfe40_zsl(pmctl, vfe40_ctrl);
		break;

	case VFE_CMD_ASF_CFG:
	case VFE_CMD_ASF_UPDATE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp, (void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		cmdp_local = cmdp + V40_ASF_LEN/4;
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			V40_ASF_SPECIAL_EFX_CFG_OFF,
			cmdp_local, V40_ASF_SPECIAL_EFX_CFG_LEN);
		break;

	case VFE_CMD_GET_HW_VERSION:
		if (cmd->length != V40_GET_HW_VERSION_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V40_GET_HW_VERSION_LEN, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		*cmdp = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase+V40_GET_HW_VERSION_OFF);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			V40_GET_HW_VERSION_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_GET_REG_DUMP:
		temp1 = sizeof(uint32_t) *
			vfe40_ctrl->share_ctrl->register_total;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(temp1, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		msm_camera_io_dump(vfe40_ctrl->share_ctrl->vfebase,
			vfe40_ctrl->share_ctrl->register_total*4);
		CDBG("%s: %p %p %d\n", __func__, (void *)cmdp,
			vfe40_ctrl->share_ctrl->vfebase, temp1);
		memcpy_fromio((void *)cmdp,
			vfe40_ctrl->share_ctrl->vfebase, temp1);
		if (copy_to_user((void __user *)(cmd->value), cmdp, temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_FRAME_SKIP_CFG:
		if (cmd->length != vfe40_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe40_cmd[cmd->id].length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}

		if (copy_from_user((cmdp), (void __user *)cmd->value,
				cmd->length)) {
			rc = -EFAULT;
			pr_err("%s copy from user failed for cmd %d",
				__func__, cmd->id);
			break;
		}

		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		vfe40_ctrl->frame_skip_cnt = ((uint32_t)
			*cmdp & VFE_FRAME_SKIP_PERIOD_MASK) + 1;
		vfe40_ctrl->frame_skip_pattern = (uint32_t)(*(cmdp + 2));
		break;
	default:
		if (cmd->length != vfe40_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe40_cmd[cmd->id].length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}

		if (copy_from_user((cmdp), (void __user *)cmd->value,
				cmd->length)) {
			rc = -EFAULT;
			pr_err("%s copy from user failed for cmd %d",
				__func__, cmd->id);
			goto proc_general_done;
		}
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp, (vfe40_cmd[cmd->id].length));
		break;

	}

proc_general_done:
	kfree(cmdp);

	return rc;
}

static inline void vfe40_read_irq_status(
	struct axi_ctrl_t *axi_ctrl, struct vfe40_irq_status *out)
{
	uint32_t *temp;
	memset(out, 0, sizeof(struct vfe40_irq_status));
	temp = (uint32_t *)(axi_ctrl->share_ctrl->vfebase + VFE_IRQ_STATUS_0);
	out->vfeIrqStatus0 = msm_camera_io_r(temp);

	temp = (uint32_t *)(axi_ctrl->share_ctrl->vfebase + VFE_IRQ_STATUS_1);
	out->vfeIrqStatus1 = msm_camera_io_r(temp);

	temp = (uint32_t *)(axi_ctrl->share_ctrl->vfebase + VFE_CAMIF_STATUS);
	out->camifStatus = msm_camera_io_r(temp);
	CDBG("camifStatus  = 0x%x\n", out->camifStatus);

	/* clear the pending interrupt of the same kind.*/
	msm_camera_io_w(out->vfeIrqStatus0,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(out->vfeIrqStatus1,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CMD);

}

static void vfe40_process_reg_update_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;

	if (vfe40_ctrl->recording_state == VFE_STATE_START_REQUESTED) {
		if (vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch1]);
		} else if (vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch1]);
		}
		vfe40_ctrl->recording_state = VFE_STATE_STARTED;
		msm_camera_io_w_mb(1,
			vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
	} else if (vfe40_ctrl->recording_state ==
			VFE_STATE_STOP_REQUESTED) {
		if (vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch1]);
		} else if (vfe40_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out1.ch1]);
		}
		CDBG("stop video triggered .\n");
	}

	if (vfe40_ctrl->start_ack_pending == TRUE) {
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId, MSG_ID_START_ACK);
		vfe40_ctrl->start_ack_pending = FALSE;
	} else {
		if (vfe40_ctrl->recording_state ==
				VFE_STATE_STOP_REQUESTED) {
			vfe40_ctrl->recording_state = VFE_STATE_STOPPED;
			/* request a reg update and send STOP_REC_ACK
			 * when we process the next reg update irq.
			 */
			msm_camera_io_w_mb(1,
			vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		} else if (vfe40_ctrl->recording_state ==
					VFE_STATE_STOPPED) {
			vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				vfe40_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_STOP_REC_ACK);
			vfe40_ctrl->recording_state = VFE_STATE_IDLE;
		}
		spin_lock_irqsave(&vfe40_ctrl->update_ack_lock, flags);
		if (vfe40_ctrl->update_ack_pending == TRUE) {
			vfe40_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe40_ctrl->update_ack_lock, flags);
			vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				vfe40_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
				&vfe40_ctrl->update_ack_lock, flags);
		}
	}

	if (vfe40_ctrl->share_ctrl->liveshot_state ==
		VFE_STATE_START_REQUESTED) {
		CDBG("%s enabling liveshot output\n", __func__);
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe40_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch1]);
			vfe40_ctrl->share_ctrl->liveshot_state =
				VFE_STATE_STARTED;
		}
	}

	if (vfe40_ctrl->share_ctrl->liveshot_state == VFE_STATE_STARTED) {
		vfe40_ctrl->share_ctrl->vfe_capture_count--;
		if (!vfe40_ctrl->share_ctrl->vfe_capture_count)
			vfe40_ctrl->share_ctrl->liveshot_state =
				VFE_STATE_STOP_REQUESTED;
		msm_camera_io_w_mb(1, vfe40_ctrl->
			share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} else if (vfe40_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOP_REQUESTED) {
		CDBG("%s: disabling liveshot output\n", __func__);
		if (vfe40_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[vfe40_ctrl->
				share_ctrl->outpath.out0.ch1]);
			vfe40_ctrl->share_ctrl->liveshot_state =
				VFE_STATE_STOPPED;
			msm_camera_io_w_mb(1, vfe40_ctrl->share_ctrl->vfebase +
				VFE_REG_UPDATE_CMD);
		}
	} else if (vfe40_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOPPED) {
		vfe40_ctrl->share_ctrl->liveshot_state = VFE_STATE_IDLE;
	}

	if ((vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN) ||
		(vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB) ||
		(vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG) ||
		(vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB)) {
		/* in snapshot mode */
		/* later we need to add check for live snapshot mode. */
		if (vfe40_ctrl->frame_skip_pattern & (0x1 <<
			(vfe40_ctrl->snapshot_frame_cnt %
				vfe40_ctrl->frame_skip_cnt))) {
			vfe40_ctrl->share_ctrl->vfe_capture_count--;
			/* if last frame to be captured: */
			if (vfe40_ctrl->share_ctrl->vfe_capture_count == 0) {
				/* stop the bus output:write master enable = 0*/
				if (vfe40_ctrl->share_ctrl->outpath.output_mode
					& VFE40_OUTPUT_MODE_PRIMARY) {
					msm_camera_io_w(0,
						vfe40_ctrl->share_ctrl->vfebase+
						vfe40_AXI_WM_CFG[vfe40_ctrl->
						share_ctrl->outpath.out0.ch0]);
					msm_camera_io_w(0,
						vfe40_ctrl->share_ctrl->vfebase+
						vfe40_AXI_WM_CFG[vfe40_ctrl->
						share_ctrl->outpath.out0.ch1]);
				}
				if (vfe40_ctrl->share_ctrl->outpath.output_mode&
						VFE40_OUTPUT_MODE_SECONDARY) {
					msm_camera_io_w(0,
						vfe40_ctrl->share_ctrl->vfebase+
						vfe40_AXI_WM_CFG[vfe40_ctrl->
						share_ctrl->outpath.out1.ch0]);
					msm_camera_io_w(0,
						vfe40_ctrl->share_ctrl->vfebase+
						vfe40_AXI_WM_CFG[vfe40_ctrl->
						share_ctrl->outpath.out1.ch1]);
				}
				msm_camera_io_w_mb
				(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe40_ctrl->share_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
				vfe40_ctrl->snapshot_frame_cnt = -1;
				vfe40_ctrl->frame_skip_cnt = 31;
				vfe40_ctrl->frame_skip_pattern = 0xffffffff;
			} /*if snapshot count is 0*/
		} /*if frame is not being dropped*/
		vfe40_ctrl->snapshot_frame_cnt++;
		/* then do reg_update. */
		msm_camera_io_w(1,
			vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} /* if snapshot mode. */
}

static void vfe40_set_default_reg_values(
			struct vfe40_ctrl_type *vfe40_ctrl)
{
	msm_camera_io_w(0x800080,
		vfe40_ctrl->share_ctrl->vfebase + VFE_DEMUX_GAIN_0);
	msm_camera_io_w(0x800080,
		vfe40_ctrl->share_ctrl->vfebase + VFE_DEMUX_GAIN_1);
	/* What value should we program CGC_OVERRIDE to? */
	msm_camera_io_w(0xFFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CGC_OVERRIDE);

	/* default frame drop period and pattern */
	msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_ENC_MIN);
	msm_camera_io_w(0xFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_ENC_MAX);
	msm_camera_io_w(0,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_VIEW_MIN);
	msm_camera_io_w(0xFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_VIEW_MAX);

	/* stats UB config */
	msm_camera_io_w(0x3980007,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_AEC_UB_CFG);
	msm_camera_io_w(0x3A00007,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_AF_UB_CFG);
	msm_camera_io_w(0x3A8000F,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_AWB_UB_CFG);
	msm_camera_io_w(0x3B80007,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_RS_UB_CFG);
	msm_camera_io_w(0x3C0001F,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_CS_UB_CFG);
	msm_camera_io_w(0x3E0001F,
		vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_STATS_HIST_UB_CFG);
}

static void vfe40_process_reset_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;

	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 0);

	spin_lock_irqsave(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
	if (vfe40_ctrl->share_ctrl->stop_ack_pending) {
		vfe40_ctrl->share_ctrl->stop_ack_pending = FALSE;
		spin_unlock_irqrestore(
			&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId, MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(
			&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe40_set_default_reg_values(vfe40_ctrl);

		/* reload all write masters. (frame & line)*/
		msm_camera_io_w(0x7FFF,
			vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_CMD);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId, MSG_ID_RESET_ACK);
	}
}

static void vfe40_process_camif_sof_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	if (vfe40_ctrl->share_ctrl->operation_mode ==
		VFE_OUTPUTS_RAW) {
		if (vfe40_ctrl->start_ack_pending) {
			vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				vfe40_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_START_ACK);
			vfe40_ctrl->start_ack_pending = FALSE;
		}
		vfe40_ctrl->share_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe40_ctrl->share_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			 to the command register using the barrier */
			msm_camera_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
			vfe40_ctrl->share_ctrl->vfebase + VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */
	if ((vfe40_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe40_ctrl->share_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_VIDEO) &&
		(vfe40_ctrl->share_ctrl->vfeFrameId %
			vfe40_ctrl->hfr_mode != 0)) {
		vfe40_ctrl->share_ctrl->vfeFrameId++;
		CDBG("Skip the SOF notification when HFR enabled\n");
		return;
	}
	vfe40_ctrl->share_ctrl->vfeFrameId++;
	vfe40_send_isp_msg(&vfe40_ctrl->subdev,
		vfe40_ctrl->share_ctrl->vfeFrameId, MSG_ID_SOF_ACK);
	CDBG("camif_sof_irq, frameId = %d\n",
		vfe40_ctrl->share_ctrl->vfeFrameId);

	if (vfe40_ctrl->sync_timer_state) {
		if (vfe40_ctrl->sync_timer_repeat_count == 0)
			vfe40_sync_timer_stop(vfe40_ctrl);
		else
			vfe40_ctrl->sync_timer_repeat_count--;
	}
}

static void vfe40_process_error_irq(
	struct axi_ctrl_t *axi_ctrl, uint32_t errStatus)
{
	uint32_t reg_value;

	if (errStatus & VFE40_IMASK_CAMIF_ERROR) {
		pr_err("vfe40_irq: camif errors\n");
		reg_value = msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_CAMIF_STATUS);
		pr_err("camifStatus  = 0x%x\n", reg_value);
		vfe40_send_isp_msg(&axi_ctrl->subdev,
			axi_ctrl->share_ctrl->vfeFrameId, MSG_ID_CAMIF_ERROR);
	}

	if (errStatus & VFE40_IMASK_BHIST_OVWR)
		pr_err("vfe40_irq: stats bhist overwrite\n");

	if (errStatus & VFE40_IMASK_STATS_CS_OVWR)
		pr_err("vfe40_irq: stats cs overwrite\n");

	if (errStatus & VFE40_IMASK_STATS_IHIST_OVWR)
		pr_err("vfe40_irq: stats ihist overwrite\n");

	if (errStatus & VFE40_IMASK_REALIGN_BUF_Y_OVFL)
		pr_err("vfe40_irq: realign bug Y overflow\n");

	if (errStatus & VFE40_IMASK_REALIGN_BUF_CB_OVFL)
		pr_err("vfe40_irq: realign bug CB overflow\n");

	if (errStatus & VFE40_IMASK_REALIGN_BUF_CR_OVFL)
		pr_err("vfe40_irq: realign bug CR overflow\n");

	if (errStatus & VFE40_IMASK_VIOLATION) {
		pr_err("vfe40_irq: violation interrupt\n");
		reg_value = msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_VIOLATION_STATUS);
		pr_err("%s: violationStatus  = 0x%x\n", __func__, reg_value);
	}

	if (errStatus & VFE40_IMASK_IMG_MAST_0_BUS_OVFL)
		pr_err("vfe40_irq: image master 0 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_1_BUS_OVFL)
		pr_err("vfe40_irq: image master 1 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_2_BUS_OVFL)
		pr_err("vfe40_irq: image master 2 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_3_BUS_OVFL)
		pr_err("vfe40_irq: image master 3 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_4_BUS_OVFL)
		pr_err("vfe40_irq: image master 4 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_5_BUS_OVFL)
		pr_err("vfe40_irq: image master 5 bus overflow\n");

	if (errStatus & VFE40_IMASK_IMG_MAST_6_BUS_OVFL)
		pr_err("vfe40_irq: image master 6 bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_AE_BG_BUS_OVFL)
		pr_err("vfe40_irq: ae/bg stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_AF_BF_BUS_OVFL)
		pr_err("vfe40_irq: af/bf stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_AWB_BUS_OVFL)
		pr_err("vfe40_irq: awb stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_RS_BUS_OVFL)
		pr_err("vfe40_irq: rs stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_CS_BUS_OVFL)
		pr_err("vfe40_irq: cs stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_IHIST_BUS_OVFL)
		pr_err("vfe40_irq: ihist stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_SKIN_BHIST_BUS_OVFL)
		pr_err("vfe40_irq: skin/bhist stats bus overflow\n");

	if (errStatus & VFE40_IMASK_AXI_ERROR)
		pr_err("vfe40_irq: axi error\n");
}

static uint32_t  vfe40_process_stats_irq_common(
	struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t statsNum, uint32_t newAddr)
{
	uint32_t pingpongStatus;
	uint32_t returnAddr;
	uint32_t pingpongAddr;

	/* must be 0=ping, 1=pong */
	pingpongStatus =
		((msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_PING_PONG_STATUS))
	& ((uint32_t)(1<<(statsNum + 7)))) >> (statsNum + 7);
	/* stats bits starts at 7 */
	CDBG("statsNum %d, pingpongStatus %d\n", statsNum, pingpongStatus);
	pingpongAddr =
		((uint32_t)(vfe40_ctrl->share_ctrl->vfebase +
				VFE_BUS_STATS_PING_PONG_BASE)) +
				(3*statsNum)*4 + (1-pingpongStatus)*4;
	returnAddr = msm_camera_io_r((uint32_t *)pingpongAddr);
	msm_camera_io_w(newAddr, (uint32_t *)pingpongAddr);
	return returnAddr;
}

static void
vfe_send_stats_msg(struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t bufAddress, uint32_t statsNum)
{
	int rc = 0;
	void *vaddr = NULL;
	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	struct isp_msg_stats msgStats;
	msgStats.frameCounter = vfe40_ctrl->share_ctrl->vfeFrameId;
	msgStats.buffer = bufAddress;

	switch (statsNum) {
	case statsAeNum:{
		msgStats.id = MSG_ID_STATS_AEC;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_AEC, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsAfNum:{
		msgStats.id = MSG_ID_STATS_AF;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_AF, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsAwbNum: {
		msgStats.id = MSG_ID_STATS_AWB;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_AWB, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;

	case statsIhistNum: {
		msgStats.id = MSG_ID_STATS_IHIST;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_IHIST, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsRsNum: {
		msgStats.id = MSG_ID_STATS_RS;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_RS, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsCsNum: {
		msgStats.id = MSG_ID_STATS_CS;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_CS, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;

	default:
		goto stats_done;
	}
	if (rc == 0) {
		msgStats.buffer = (uint32_t)vaddr;
		v4l2_subdev_notify(&vfe40_ctrl->subdev,
			NOTIFY_VFE_MSG_STATS,
			&msgStats);
	} else {
		pr_err("%s: paddr to idx mapping error, stats_id = %d, paddr = 0x%d",
			 __func__, msgStats.id, msgStats.buffer);
	}
stats_done:
	spin_unlock_irqrestore(&ctrl->state_lock, flags);
	return;
}

static void vfe_send_comp_stats_msg(
	struct vfe40_ctrl_type *vfe40_ctrl, uint32_t status_bits)
{
	struct msm_stats_buf msgStats;
	uint32_t temp;

	msgStats.frame_id = vfe40_ctrl->share_ctrl->vfeFrameId;
	msgStats.status_bits = status_bits;

	msgStats.aec.buff = vfe40_ctrl->aecStatsControl.bufToRender;
	msgStats.awb.buff = vfe40_ctrl->awbStatsControl.bufToRender;
	msgStats.af.buff = vfe40_ctrl->afStatsControl.bufToRender;

	msgStats.ihist.buff = vfe40_ctrl->ihistStatsControl.bufToRender;
	msgStats.rs.buff = vfe40_ctrl->rsStatsControl.bufToRender;
	msgStats.cs.buff = vfe40_ctrl->csStatsControl.bufToRender;

	temp = msm_camera_io_r(
		vfe40_ctrl->share_ctrl->vfebase + VFE_STATS_AWB_SGW_CFG);
	msgStats.awb_ymin = (0xFF00 & temp) >> 8;

	v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_MSG_COMP_STATS,
				&msgStats);
}

static void vfe40_process_stats_awb_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AWB);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->awbStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsAwbNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->awbStatsControl.bufToRender, statsAwbNum);
	} else{
		vfe40_ctrl->awbStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->awbStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats_ihist_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_IHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->ihistStatsControl.bufToRender =
			vfe40_process_stats_irq_common(
			vfe40_ctrl, statsIhistNum, addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->ihistStatsControl.bufToRender,
			statsIhistNum);
	} else {
		vfe40_ctrl->ihistStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->ihistStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats_rs_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_RS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->rsStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsRsNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->rsStatsControl.bufToRender, statsRsNum);
	} else {
		vfe40_ctrl->rsStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->rsStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats_cs_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_CS);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->csStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsCsNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->csStatsControl.bufToRender, statsCsNum);
	} else {
		vfe40_ctrl->csStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->csStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats(struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t status_bits)
{
	unsigned long flags;
	int32_t process_stats = false;
	uint32_t addr;

	CDBG("%s, stats = 0x%x\n", __func__, status_bits);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	if (status_bits & VFE_IRQ_STATUS0_STATS_AWB) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
			MSM_STATS_TYPE_AWB);
		if (addr) {
			vfe40_ctrl->awbStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsAwbNum,
				addr);
			process_stats = true;
		} else{
			vfe40_ctrl->awbStatsControl.droppedStatsFrameCount++;
			vfe40_ctrl->awbStatsControl.bufToRender = 0;
		}
	} else {
		vfe40_ctrl->awbStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_IHIST) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
					MSM_STATS_TYPE_IHIST);
		if (addr) {
			vfe40_ctrl->ihistStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsIhistNum,
				addr);
			process_stats = true;
		} else {
			vfe40_ctrl->ihistStatsControl.droppedStatsFrameCount++;
			vfe40_ctrl->ihistStatsControl.bufToRender = 0;
		}
	} else {
		vfe40_ctrl->ihistStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_RS) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
					MSM_STATS_TYPE_RS);
		if (addr) {
			vfe40_ctrl->rsStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsRsNum,
				addr);
			process_stats = true;
		} else {
			vfe40_ctrl->rsStatsControl.droppedStatsFrameCount++;
			vfe40_ctrl->rsStatsControl.bufToRender = 0;
		}
	} else {
		vfe40_ctrl->rsStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_CS) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
					MSM_STATS_TYPE_CS);
		if (addr) {
			vfe40_ctrl->csStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsCsNum,
				addr);
			process_stats = true;
		} else {
			vfe40_ctrl->csStatsControl.droppedStatsFrameCount++;
			vfe40_ctrl->csStatsControl.bufToRender = 0;
		}
	} else {
		vfe40_ctrl->csStatsControl.bufToRender = 0;
	}
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (process_stats)
		vfe_send_comp_stats_msg(vfe40_ctrl, status_bits);

	return;
}

static void vfe40_process_stats_irq(
	struct vfe40_ctrl_type *vfe40_ctrl, uint32_t irqstatus)
{
	uint32_t status_bits = VFE_COM_STATUS & irqstatus;

	if ((vfe40_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe40_ctrl->share_ctrl->vfeFrameId %
		 vfe40_ctrl->hfr_mode != 0)) {
		CDBG("Skip the stats when HFR enabled\n");
		return;
	}

	vfe40_process_stats(vfe40_ctrl, status_bits);
	return;
}

static void vfe40_process_irq(
	struct vfe40_ctrl_type *vfe40_ctrl, uint32_t irqstatus)
{
	if (irqstatus &
		VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0) {
		vfe40_process_stats_irq(vfe40_ctrl, irqstatus);
		return;
	}

	switch (irqstatus) {
	case VFE_IRQ_STATUS0_CAMIF_SOF_MASK:
		CDBG("irq	camifSofIrq\n");
		vfe40_process_camif_sof_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_REG_UPDATE_MASK:
		CDBG("irq	regUpdateIrq\n");
		vfe40_process_reg_update_irq(vfe40_ctrl);
		break;
	case VFE_IMASK_WHILE_STOPPING_0:
		CDBG("irq	resetAckIrq\n");
		vfe40_process_reset_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_AWB:
		CDBG("Stats AWB irq occured.\n");
		vfe40_process_stats_awb_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_IHIST:
		CDBG("Stats IHIST irq occured.\n");
		vfe40_process_stats_ihist_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_RS:
		CDBG("Stats RS irq occured.\n");
		vfe40_process_stats_rs_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_CS:
		CDBG("Stats CS irq occured.\n");
		vfe40_process_stats_cs_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS1_SYNC_TIMER0:
		CDBG("SYNC_TIMER 0 irq occured.\n");
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_SYNC_TIMER0_DONE);
		break;
	case VFE_IRQ_STATUS1_SYNC_TIMER1:
		CDBG("SYNC_TIMER 1 irq occured.\n");
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_SYNC_TIMER1_DONE);
		break;
	case VFE_IRQ_STATUS1_SYNC_TIMER2:
		CDBG("SYNC_TIMER 2 irq occured.\n");
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_SYNC_TIMER2_DONE);
		break;
	default:
		pr_err("Invalid IRQ status\n");
	}
}

static void axi40_do_tasklet(unsigned long data)
{
	unsigned long flags;
	struct axi_ctrl_t *axi_ctrl = (struct axi_ctrl_t *)data;
	struct vfe40_isr_queue_cmd *qcmd = NULL;

	CDBG("=== axi40_do_tasklet start ===\n");

	while (atomic_read(&axi_ctrl->share_ctrl->irq_cnt)) {
		spin_lock_irqsave(&axi_ctrl->tasklet_lock, flags);
		qcmd = list_first_entry(&axi_ctrl->tasklet_q,
			struct vfe40_isr_queue_cmd, list);
		atomic_sub(1, &axi_ctrl->share_ctrl->irq_cnt);

		if (!qcmd) {
			spin_unlock_irqrestore(&axi_ctrl->tasklet_lock,
				flags);
			return;
		}

		list_del(&qcmd->list);
		spin_unlock_irqrestore(&axi_ctrl->tasklet_lock,
			flags);

		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_CAMIF_SOF_MASK)
			v4l2_subdev_notify(&axi_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_CAMIF_SOF_MASK);

		/* interrupt to be processed,  *qcmd has the payload.  */
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_REG_UPDATE_MASK) {
			v4l2_subdev_notify(&axi_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_REG_UPDATE_MASK);
		}

		if (qcmd->vfeInterruptStatus0 &
				VFE_IMASK_WHILE_STOPPING_0)
			v4l2_subdev_notify(&axi_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IMASK_WHILE_STOPPING_0);

		if (atomic_read(&axi_ctrl->share_ctrl->vstate)) {
			if (qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe40_process_error_irq(
					axi_ctrl,
					qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_ERROR_ONLY_1);
			}
			v4l2_subdev_notify(&axi_ctrl->subdev,
				NOTIFY_AXI_IRQ,
				(void *)qcmd->vfeInterruptStatus0);

			/* then process stats irq. */
			if (axi_ctrl->share_ctrl->stats_comp) {
				/* process stats comb interrupt. */
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0) {
					CDBG("Stats composite irq occured.\n");
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)qcmd->vfeInterruptStatus0);
				}
			} else {
				/* process individual stats interrupt. */
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_AWB)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_AWB);
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_IHIST)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_IHIST);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_RS)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_RS);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_CS)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_CS);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS1_SYNC_TIMER0)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER0);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS1_SYNC_TIMER1)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER1);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS1_SYNC_TIMER2)
					v4l2_subdev_notify(&axi_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER2);
			}
		}
		kfree(qcmd);
	}
	CDBG("=== axi40_do_tasklet end ===\n");
}

static irqreturn_t vfe40_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	struct vfe40_irq_status irq;
	struct vfe40_isr_queue_cmd *qcmd;
	struct axi_ctrl_t *axi_ctrl = data;

	CDBG("vfe_parse_irq\n");

	vfe40_read_irq_status(axi_ctrl, &irq);

	if ((irq.vfeIrqStatus0 == 0) && (irq.vfeIrqStatus1 == 0)) {
		CDBG("vfe_parse_irq: vfeIrqStatus0 & 1 are both 0!\n");
		return IRQ_HANDLED;
	}

	qcmd = kzalloc(sizeof(struct vfe40_isr_queue_cmd),
		GFP_ATOMIC);
	if (!qcmd) {
		pr_err("vfe_parse_irq: qcmd malloc failed!\n");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&axi_ctrl->share_ctrl->stop_flag_lock, flags);
	if (axi_ctrl->share_ctrl->stop_ack_pending) {
		irq.vfeIrqStatus0 &= VFE_IMASK_WHILE_STOPPING_0;
		irq.vfeIrqStatus1 &= VFE_IMASK_WHILE_STOPPING_1;
	}
	spin_unlock_irqrestore(&axi_ctrl->share_ctrl->stop_flag_lock, flags);

	CDBG("vfe_parse_irq: Irq_status0 = 0x%x, Irq_status1 = 0x%x.\n",
		irq.vfeIrqStatus0, irq.vfeIrqStatus1);

	qcmd->vfeInterruptStatus0 = irq.vfeIrqStatus0;
	qcmd->vfeInterruptStatus1 = irq.vfeIrqStatus1;

	spin_lock_irqsave(&axi_ctrl->tasklet_lock, flags);
	list_add_tail(&qcmd->list, &axi_ctrl->tasklet_q);

	atomic_add(1, &axi_ctrl->share_ctrl->irq_cnt);
	spin_unlock_irqrestore(&axi_ctrl->tasklet_lock, flags);
	tasklet_schedule(&axi_ctrl->vfe40_tasklet);
	return IRQ_HANDLED;
}


static long vfe_stats_bufq_sub_ioctl(
	struct vfe40_ctrl_type *vfe_ctrl,
	struct msm_vfe_cfg_cmd *cmd, void *ion_client)
{
	long rc = 0;
	switch (cmd->cmd_type) {
	case VFE_CMD_STATS_REQBUF:
	if (!vfe_ctrl->stats_ops.stats_ctrl) {
		/* stats_ctrl has not been init yet */
		rc = msm_stats_buf_ops_init(&vfe_ctrl->stats_ctrl,
				(struct ion_client *)ion_client,
				&vfe_ctrl->stats_ops);
		if (rc < 0) {
			pr_err("%s: cannot init stats ops", __func__);
			goto end;
		}
		rc = vfe_ctrl->stats_ops.stats_ctrl_init(&vfe_ctrl->stats_ctrl);
		if (rc < 0) {
			pr_err("%s: cannot init stats_ctrl ops", __func__);
			memset(&vfe_ctrl->stats_ops, 0,
				sizeof(vfe_ctrl->stats_ops));
			goto end;
		}
		if (sizeof(struct msm_stats_reqbuf) != cmd->length) {
			/* error. the length not match */
			pr_err("%s: stats reqbuf input size = %d,\n"
				"struct size = %d, mitch match\n",
				 __func__, cmd->length,
				sizeof(struct msm_stats_reqbuf));
			rc = -EINVAL ;
			goto end;
		}
	}
	rc = vfe_ctrl->stats_ops.reqbuf(
			&vfe_ctrl->stats_ctrl,
			(struct msm_stats_reqbuf *)cmd->value,
			vfe_ctrl->stats_ops.client);
	break;
	case VFE_CMD_STATS_ENQUEUEBUF:
	if (sizeof(struct msm_stats_buf_info) != cmd->length) {
		/* error. the length not match */
		pr_err("%s: stats enqueuebuf input size = %d,\n"
			"struct size = %d, mitch match\n",
			 __func__, cmd->length,
			sizeof(struct msm_stats_buf_info));
			rc = -EINVAL;
			goto end;
	}
	rc = vfe_ctrl->stats_ops.enqueue_buf(
			&vfe_ctrl->stats_ctrl,
			(struct msm_stats_buf_info *)cmd->value,
			vfe_ctrl->stats_ops.client);
	break;
	case VFE_CMD_STATS_FLUSH_BUFQ:
	{
		struct msm_stats_flush_bufq *flush_req = NULL;
		flush_req = (struct msm_stats_flush_bufq *)cmd->value;
		if (sizeof(struct msm_stats_flush_bufq) != cmd->length) {
			/* error. the length not match */
			pr_err("%s: stats flush queue input size = %d,\n"
				"struct size = %d, mitch match\n",
				__func__, cmd->length,
				sizeof(struct msm_stats_flush_bufq));
			rc = -EINVAL;
			goto end;
	}
	rc = vfe_ctrl->stats_ops.bufq_flush(
			&vfe_ctrl->stats_ctrl,
			(enum msm_stats_enum_type)flush_req->stats_type,
			vfe_ctrl->stats_ops.client);
	}
	break;
	default:
		rc = -1;
		pr_err("%s: cmd_type %d not supported", __func__,
			cmd->cmd_type);
	break;
	}
end:
	return rc;
}

static long msm_vfe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int subdev_cmd, void *arg)
{
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	struct vfe40_ctrl_type *vfe40_ctrl =
		(struct vfe40_ctrl_type *)v4l2_get_subdevdata(sd);
	struct msm_isp_cmd vfecmd;
	struct msm_camvfe_params *vfe_params =
		(struct msm_camvfe_params *)arg;
	struct msm_vfe_cfg_cmd *cmd = vfe_params->vfe_cfg;
	void *data = vfe_params->data;

	long rc = 0;
	struct vfe_cmd_stats_buf *scfg = NULL;
	struct vfe_cmd_stats_ack *sack = NULL;

	if (!vfe40_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}

	switch (cmd->cmd_type) {
	case CMD_VFE_PROCESS_IRQ:
		vfe40_process_irq(vfe40_ctrl, (uint32_t) data);
		return rc;
	case VFE_CMD_STATS_REQBUF:
	case VFE_CMD_STATS_ENQUEUEBUF:
	case VFE_CMD_STATS_FLUSH_BUFQ:
		/* for easy porting put in one envelope */
		rc = vfe_stats_bufq_sub_ioctl(vfe40_ctrl,
				cmd, vfe_params->data);
		return rc;
	default:
		if (cmd->cmd_type != CMD_CONFIG_PING_ADDR &&
			cmd->cmd_type != CMD_CONFIG_PONG_ADDR &&
			cmd->cmd_type != CMD_CONFIG_FREE_BUF_ADDR &&
			cmd->cmd_type != CMD_STATS_AEC_BUF_RELEASE &&
			cmd->cmd_type != CMD_STATS_AWB_BUF_RELEASE &&
			cmd->cmd_type != CMD_STATS_IHIST_BUF_RELEASE &&
			cmd->cmd_type != CMD_STATS_RS_BUF_RELEASE &&
			cmd->cmd_type != CMD_STATS_CS_BUF_RELEASE &&
			cmd->cmd_type != CMD_STATS_AF_BUF_RELEASE) {
				if (copy_from_user(&vfecmd,
					(void __user *)(cmd->value),
					sizeof(vfecmd))) {
						pr_err("%s %d: copy_from_user failed\n",
							__func__, __LINE__);
					return -EFAULT;
				}
		} else {
			/* here eith stats release or frame release. */
			if (cmd->cmd_type != CMD_CONFIG_PING_ADDR &&
				cmd->cmd_type != CMD_CONFIG_PONG_ADDR &&
				cmd->cmd_type != CMD_CONFIG_FREE_BUF_ADDR) {
				/* then must be stats release. */
				if (!data) {
					pr_err("%s: data = NULL, cmd->cmd_type = %d",
						__func__, cmd->cmd_type);
					return -EFAULT;
				}
				sack = kmalloc(sizeof(struct vfe_cmd_stats_ack),
							GFP_ATOMIC);
				if (!sack) {
					pr_err("%s: no mem for cmd->cmd_type = %d",
					 __func__, cmd->cmd_type);
					return -ENOMEM;
				}
				sack->nextStatsBuf = *(uint32_t *)data;
			}
		}
		CDBG("%s: cmdType = %d\n", __func__, cmd->cmd_type);

		if ((cmd->cmd_type == CMD_STATS_AF_ENABLE)    ||
			(cmd->cmd_type == CMD_STATS_AWB_ENABLE)   ||
			(cmd->cmd_type == CMD_STATS_IHIST_ENABLE) ||
			(cmd->cmd_type == CMD_STATS_RS_ENABLE)    ||
			(cmd->cmd_type == CMD_STATS_CS_ENABLE)    ||
			(cmd->cmd_type == CMD_STATS_AEC_ENABLE)) {
				scfg = NULL;
				/* individual */
				goto vfe40_config_done;
		}
		switch (cmd->cmd_type) {
		case CMD_GENERAL:
			rc = vfe40_proc_general(pmctl, &vfecmd, vfe40_ctrl);
		break;
		case CMD_CONFIG_PING_ADDR: {
			int path = *((int *)cmd->value);
			struct vfe40_output_ch *outch =
				vfe40_get_ch(path, vfe40_ctrl->share_ctrl);
			outch->ping = *((struct msm_free_buf *)data);
		}
		break;

		case CMD_CONFIG_PONG_ADDR: {
			int path = *((int *)cmd->value);
			struct vfe40_output_ch *outch =
				vfe40_get_ch(path, vfe40_ctrl->share_ctrl);
			outch->pong = *((struct msm_free_buf *)data);
		}
		break;

		case CMD_CONFIG_FREE_BUF_ADDR: {
			int path = *((int *)cmd->value);
			struct vfe40_output_ch *outch =
				vfe40_get_ch(path, vfe40_ctrl->share_ctrl);
			outch->free_buf = *((struct msm_free_buf *)data);
		}
		break;
		case CMD_SNAP_BUF_RELEASE:
			break;
		default:
			pr_err("%s Unsupported AXI configuration %x ", __func__,
				cmd->cmd_type);
		break;
		}
	}
vfe40_config_done:
	kfree(scfg);
	kfree(sack);
	CDBG("%s done: rc = %d\n", __func__, (int) rc);
	return rc;
}

static const struct v4l2_subdev_core_ops msm_vfe_subdev_core_ops = {
	.ioctl = msm_vfe_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_vfe_subdev_ops = {
	.core = &msm_vfe_subdev_core_ops,
};

int msm_vfe_subdev_init(struct v4l2_subdev *sd,
			struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	struct vfe40_ctrl_type *vfe40_ctrl =
		(struct vfe40_ctrl_type *)v4l2_get_subdevdata(sd);
	v4l2_set_subdev_hostdata(sd, mctl);

	spin_lock_init(&vfe40_ctrl->share_ctrl->stop_flag_lock);
	spin_lock_init(&vfe40_ctrl->state_lock);
	spin_lock_init(&vfe40_ctrl->io_lock);
	spin_lock_init(&vfe40_ctrl->update_ack_lock);
	spin_lock_init(&vfe40_ctrl->stats_bufq_lock);


	vfe40_ctrl->update_linear = false;
	vfe40_ctrl->update_rolloff = false;
	vfe40_ctrl->update_la = false;
	vfe40_ctrl->update_gamma = false;
	vfe40_ctrl->hfr_mode = HFR_MODE_OFF;

	return rc;
}

void msm_vfe_subdev_release(struct v4l2_subdev *sd)
{
	struct vfe40_ctrl_type *vfe40_ctrl =
		(struct vfe40_ctrl_type *)v4l2_get_subdevdata(sd);
	if (!vfe40_ctrl->share_ctrl->vfebase)
		vfe40_ctrl->share_ctrl->vfebase = NULL;
}

static const struct v4l2_subdev_internal_ops msm_vfe_internal_ops;

static int __devinit vfe40_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct axi_ctrl_t *axi_ctrl;
	struct vfe40_ctrl_type *vfe40_ctrl;
	struct vfe_share_ctrl_t *share_ctrl;
	struct msm_cam_subdev_info sd_info;
	CDBG("%s: device id = %d\n", __func__, pdev->id);

	share_ctrl = kzalloc(sizeof(struct vfe_share_ctrl_t), GFP_KERNEL);
	if (!share_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	axi_ctrl = kzalloc(sizeof(struct axi_ctrl_t), GFP_KERNEL);
	if (!axi_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		kfree(share_ctrl);
		return -ENOMEM;
	}

	vfe40_ctrl = kzalloc(sizeof(struct vfe40_ctrl_type), GFP_KERNEL);
	if (!vfe40_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		kfree(share_ctrl);
		kfree(axi_ctrl);
		return -ENOMEM;
	}

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	share_ctrl->axi_ctrl = axi_ctrl;
	share_ctrl->vfe40_ctrl = vfe40_ctrl;
	axi_ctrl->share_ctrl = share_ctrl;
	vfe40_ctrl->share_ctrl = share_ctrl;
	axi_ctrl->pdev = pdev;
	vfe40_axi_probe(axi_ctrl);

	v4l2_subdev_init(&vfe40_ctrl->subdev, &msm_vfe_subdev_ops);
	vfe40_ctrl->subdev.internal_ops = &msm_vfe_internal_ops;
	vfe40_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(vfe40_ctrl->subdev.name,
			 sizeof(vfe40_ctrl->subdev.name), "vfe4.0");
	v4l2_set_subdevdata(&vfe40_ctrl->subdev, vfe40_ctrl);
	platform_set_drvdata(pdev, &vfe40_ctrl->subdev);

	axi_ctrl->vfemem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "vfe");
	if (!axi_ctrl->vfemem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vfe40_no_resource;
	}
	axi_ctrl->vfeirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vfe");
	if (!axi_ctrl->vfeirq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vfe40_no_resource;
	}

	axi_ctrl->vfeio = request_mem_region(axi_ctrl->vfemem->start,
		resource_size(axi_ctrl->vfemem), pdev->name);
	if (!axi_ctrl->vfeio) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto vfe40_no_resource;
	}

	rc = request_irq(axi_ctrl->vfeirq->start, vfe40_parse_irq,
		IRQF_TRIGGER_RISING, "vfe", axi_ctrl);
	if (rc < 0) {
		release_mem_region(axi_ctrl->vfemem->start,
			resource_size(axi_ctrl->vfemem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto vfe40_no_resource;
	}

	disable_irq(axi_ctrl->vfeirq->start);

	tasklet_init(&axi_ctrl->vfe40_tasklet,
		axi40_do_tasklet, (unsigned long)axi_ctrl);

	vfe40_ctrl->pdev = pdev;
	sd_info.sdev_type = VFE_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = axi_ctrl->vfeirq->start;
	msm_cam_register_subdev_node(&vfe40_ctrl->subdev, &sd_info);
	return 0;

vfe40_no_resource:
	kfree(vfe40_ctrl);
	kfree(axi_ctrl);
	return 0;
}

static const struct of_device_id msm_vfe_dt_match[] = {
	{.compatible = "qcom,vfe40"},
};

MODULE_DEVICE_TABLE(of, msm_vfe_dt_match);

static struct platform_driver vfe40_driver = {
	.probe = vfe40_probe,
	.driver = {
		.name = MSM_VFE_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_vfe_dt_match,
	},
};

static int __init msm_vfe40_init_module(void)
{
	return platform_driver_register(&vfe40_driver);
}

static void __exit msm_vfe40_exit_module(void)
{
	platform_driver_unregister(&vfe40_driver);
}

module_init(msm_vfe40_init_module);
module_exit(msm_vfe40_exit_module);
MODULE_DESCRIPTION("VFE 4.0 driver");
MODULE_LICENSE("GPL v2");
