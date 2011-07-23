/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "msm.h"
#include "msm_vfe32.h"
#include "msm_vpe1.h"

atomic_t irq_cnt;

#define CHECKED_COPY_FROM_USER(in) {					\
	if (copy_from_user((in), (void __user *)cmd->value,		\
			cmd->length)) {					\
		rc = -EFAULT;						\
		break;							\
	}								\
}

static struct vfe32_ctrl_type *vfe32_ctrl;
static struct msm_camera_io_clk camio_clk;
static void  *vfe_syncdata;

struct vfe32_isr_queue_cmd {
	struct list_head list;
	uint32_t                           vfeInterruptStatus0;
	uint32_t                           vfeInterruptStatus1;
	struct vfe_frame_asf_info          vfeAsfFrameInfo;
	struct vfe_frame_bpc_info          vfeBpcFrameInfo;
	struct vfe_msg_camif_status        vfeCamifStatusLocal;
};

static struct vfe32_cmd_type vfe32_cmd[] = {
/* 0*/	{V32_DUMMY_0},
		{V32_SET_CLK},
		{V32_RESET},
		{V32_START},
		{V32_TEST_GEN_START},
/* 5*/	{V32_OPERATION_CFG, V32_OPERATION_CFG_LEN},
		{V32_AXI_OUT_CFG, V32_AXI_OUT_LEN, V32_AXI_OUT_OFF, 0xFF},
		{V32_CAMIF_CFG, V32_CAMIF_LEN, V32_CAMIF_OFF, 0xFF},
		{V32_AXI_INPUT_CFG},
		{V32_BLACK_LEVEL_CFG, V32_BLACK_LEVEL_LEN, V32_BLACK_LEVEL_OFF,
		0xFF},
/*10*/  {V32_ROLL_OFF_CFG, V32_ROLL_OFF_CFG_LEN, V32_ROLL_OFF_CFG_OFF,
		0xFF},
		{V32_DEMUX_CFG, V32_DEMUX_LEN, V32_DEMUX_OFF, 0xFF},
		{V32_FOV_CFG, V32_FOV_LEN, V32_FOV_OFF, 0xFF},
		{V32_MAIN_SCALER_CFG, V32_MAIN_SCALER_LEN, V32_MAIN_SCALER_OFF,
		0xFF},
		{V32_WB_CFG, V32_WB_LEN, V32_WB_OFF, 0xFF},
/*15*/	{V32_COLOR_COR_CFG, V32_COLOR_COR_LEN, V32_COLOR_COR_OFF, 0xFF},
		{V32_RGB_G_CFG, V32_RGB_G_LEN, V32_RGB_G_OFF, 0xFF},
		{V32_LA_CFG, V32_LA_LEN, V32_LA_OFF, 0xFF },
		{V32_CHROMA_EN_CFG, V32_CHROMA_EN_LEN, V32_CHROMA_EN_OFF, 0xFF},
		{V32_CHROMA_SUP_CFG, V32_CHROMA_SUP_LEN, V32_CHROMA_SUP_OFF,
		0xFF},
/*20*/	{V32_MCE_CFG, V32_MCE_LEN, V32_MCE_OFF, 0xFF},
		{V32_SK_ENHAN_CFG, V32_SCE_LEN, V32_SCE_OFF, 0xFF},
		{V32_ASF_CFG, V32_ASF_LEN, V32_ASF_OFF, 0xFF},
		{V32_S2Y_CFG, V32_S2Y_LEN, V32_S2Y_OFF, 0xFF},
		{V32_S2CbCr_CFG, V32_S2CbCr_LEN, V32_S2CbCr_OFF, 0xFF},
/*25*/	{V32_CHROMA_SUBS_CFG, V32_CHROMA_SUBS_LEN, V32_CHROMA_SUBS_OFF,
		0xFF},
		{V32_OUT_CLAMP_CFG, V32_OUT_CLAMP_LEN, V32_OUT_CLAMP_OFF,
		0xFF},
		{V32_FRAME_SKIP_CFG, V32_FRAME_SKIP_LEN, V32_FRAME_SKIP_OFF,
		0xFF},
		{V32_DUMMY_1},
		{V32_DUMMY_2},
/*30*/	{V32_DUMMY_3},
		{V32_UPDATE},
		{V32_BL_LVL_UPDATE, V32_BLACK_LEVEL_LEN, V32_BLACK_LEVEL_OFF,
		0xFF},
		{V32_DEMUX_UPDATE, V32_DEMUX_LEN, V32_DEMUX_OFF, 0xFF},
		{V32_FOV_UPDATE, V32_FOV_LEN, V32_FOV_OFF, 0xFF},
/*35*/	{V32_MAIN_SCALER_UPDATE, V32_MAIN_SCALER_LEN, V32_MAIN_SCALER_OFF,
		0xFF},
		{V32_WB_UPDATE, V32_WB_LEN, V32_WB_OFF, 0xFF},
		{V32_COLOR_COR_UPDATE, V32_COLOR_COR_LEN, V32_COLOR_COR_OFF,
		0xFF},
		{V32_RGB_G_UPDATE, V32_RGB_G_LEN, V32_CHROMA_EN_OFF, 0xFF},
		{V32_LA_UPDATE, V32_LA_LEN, V32_LA_OFF, 0xFF },
/*40*/	{V32_CHROMA_EN_UPDATE, V32_CHROMA_EN_LEN, V32_CHROMA_EN_OFF,
		0xFF},
		{V32_CHROMA_SUP_UPDATE, V32_CHROMA_SUP_LEN, V32_CHROMA_SUP_OFF,
		0xFF},
		{V32_MCE_UPDATE, V32_MCE_LEN, V32_MCE_OFF, 0xFF},
		{V32_SK_ENHAN_UPDATE, V32_SCE_LEN, V32_SCE_OFF, 0xFF},
		{V32_S2CbCr_UPDATE, V32_S2CbCr_LEN, V32_S2CbCr_OFF, 0xFF},
/*45*/	{V32_S2Y_UPDATE, V32_S2Y_LEN, V32_S2Y_OFF, 0xFF},
		{V32_ASF_UPDATE, V32_ASF_UPDATE_LEN, V32_ASF_OFF, 0xFF},
		{V32_FRAME_SKIP_UPDATE},
		{V32_CAMIF_FRAME_UPDATE},
		{V32_STATS_AF_UPDATE, V32_STATS_AF_LEN, V32_STATS_AF_OFF},
/*50*/	{V32_STATS_AE_UPDATE, V32_STATS_AE_LEN, V32_STATS_AE_OFF},
		{V32_STATS_AWB_UPDATE, V32_STATS_AWB_LEN, V32_STATS_AWB_OFF},
		{V32_STATS_RS_UPDATE, V32_STATS_RS_LEN, V32_STATS_RS_OFF},
		{V32_STATS_CS_UPDATE, V32_STATS_CS_LEN, V32_STATS_CS_OFF},
		{V32_STATS_SKIN_UPDATE},
/*55*/	{V32_STATS_IHIST_UPDATE, V32_STATS_IHIST_LEN, V32_STATS_IHIST_OFF},
		{V32_DUMMY_4},
		{V32_EPOCH1_ACK},
		{V32_EPOCH2_ACK},
		{V32_START_RECORDING},
/*60*/	{V32_STOP_RECORDING},
		{V32_DUMMY_5},
		{V32_DUMMY_6},
		{V32_CAPTURE, V32_CAPTURE_LEN, 0xFF},
		{V32_DUMMY_7},
/*65*/	{V32_STOP},
		{V32_GET_HW_VERSION},
		{V32_GET_FRAME_SKIP_COUNTS},
		{V32_OUTPUT1_BUFFER_ENQ},
		{V32_OUTPUT2_BUFFER_ENQ},
/*70*/	{V32_OUTPUT3_BUFFER_ENQ},
		{V32_JPEG_OUT_BUF_ENQ},
		{V32_RAW_OUT_BUF_ENQ},
		{V32_RAW_IN_BUF_ENQ},
		{V32_STATS_AF_ENQ},
/*75*/	{V32_STATS_AE_ENQ},
		{V32_STATS_AWB_ENQ},
		{V32_STATS_RS_ENQ},
		{V32_STATS_CS_ENQ},
		{V32_STATS_SKIN_ENQ},
/*80*/	{V32_STATS_IHIST_ENQ},
		{V32_DUMMY_8},
		{V32_JPEG_ENC_CFG},
		{V32_DUMMY_9},
		{V32_STATS_AF_START, V32_STATS_AF_LEN, V32_STATS_AF_OFF},
/*85*/	{V32_STATS_AF_STOP},
		{V32_STATS_AE_START, V32_STATS_AE_LEN, V32_STATS_AE_OFF},
		{V32_STATS_AE_STOP},
		{V32_STATS_AWB_START, V32_STATS_AWB_LEN, V32_STATS_AWB_OFF},
		{V32_STATS_AWB_STOP},
/*90*/	{V32_STATS_RS_START, V32_STATS_RS_LEN, V32_STATS_RS_OFF},
		{V32_STATS_RS_STOP},
		{V32_STATS_CS_START, V32_STATS_CS_LEN, V32_STATS_CS_OFF},
		{V32_STATS_CS_STOP},
		{V32_STATS_SKIN_START},
/*95*/	{V32_STATS_SKIN_STOP},
		{V32_STATS_IHIST_START,
		V32_STATS_IHIST_LEN, V32_STATS_IHIST_OFF},
		{V32_STATS_IHIST_STOP},
		{V32_DUMMY_10},
		{V32_SYNC_TIMER_SETTING, V32_SYNC_TIMER_LEN,
			V32_SYNC_TIMER_OFF},
/*100*/	{V32_ASYNC_TIMER_SETTING, V32_ASYNC_TIMER_LEN, V32_ASYNC_TIMER_OFF},
		{V32_LIVESHOT},
		{V32_LA_SETUP},
		{V32_LINEARIZATION, V32_LINEARIZATION_LEN1,
					V32_LINEARIZATION_OFF1},
		{V32_DEMOSAICV3},
/*105*/	{V32_DEMOSAICV3_ABCC_CFG},
		{V32_DEMOSAICV3_DBCC_CFG, V32_DEMOSAICV3_DBCC_LEN,
			V32_DEMOSAICV3_DBCC_OFF},
		{V32_DEMOSAICV3_DBPC_CFG},
		{V32_DEMOSAICV3_ABF_CFG},
		{V32_DEMOSAICV3_ABCC_UPDATE},
		{V32_DEMOSAICV3_DBCC_UPDATE, V32_DEMOSAICV3_DBCC_LEN,
			V32_DEMOSAICV3_DBCC_OFF},
		{V32_DEMOSAICV3_DBPC_UPDATE},
};

uint32_t vfe32_AXI_WM_CFG[] = {
	0x0000004C,
	0x00000064,
	0x0000007C,
	0x00000094,
	0x000000AC,
	0x000000C4,
	0x000000DC,
};

static const char * const vfe32_general_cmd[] = {
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
	"LINEARIZATION",
	"DEMOSAICV3",
	"DEMOSAICV3_ABCC_CFG", /* 105 */
	"DEMOSAICV3_DBCC_CFG",
	"DEMOSAICV3_DBPC_CFG",
	"DEMOSAICV3_ABF_CFG", /* 108 */
	"DEMOSAICV3_ABCC_UPDATE",
	"DEMOSAICV3_DBCC_UPDATE",
	"DEMOSAICV3_DBPC_UPDATE",
	"EZTUNE_CFG",
};

static void vfe_addr_convert(struct msm_vfe_phy_info *pinfo,
	enum vfe_resp_msg type, void *data, void **ext, int32_t *elen)
{
	uint8_t outid;
	switch (type) {
	case VFE_MSG_OUTPUT_T:
	case VFE_MSG_OUTPUT_P:
	case VFE_MSG_OUTPUT_S:
	case VFE_MSG_OUTPUT_V:
		pinfo->output_id =
			((struct vfe_message *)data)->_u.msgOut.output_id;

		switch (type) {
		case VFE_MSG_OUTPUT_P:
			outid = OUTPUT_TYPE_P;
			break;
		case VFE_MSG_OUTPUT_V:
			outid = OUTPUT_TYPE_V;
			break;
		case VFE_MSG_OUTPUT_T:
			outid = OUTPUT_TYPE_T;
			break;
		case VFE_MSG_OUTPUT_S:
			outid = OUTPUT_TYPE_S;
			break;
		default:
			outid = 0xff;
			break;
		}
		pinfo->output_id = outid;
		pinfo->y_phy =
			((struct vfe_message *)data)->_u.msgOut.yBuffer;
		pinfo->cbcr_phy =
			((struct vfe_message *)data)->_u.msgOut.cbcrBuffer;

		pinfo->frame_id =
		((struct vfe_message *)data)->_u.msgOut.frameCounter;

		((struct vfe_msg_output *)(vfe32_ctrl->extdata))->bpcInfo =
		((struct vfe_message *)data)->_u.msgOut.bpcInfo;
		((struct vfe_msg_output *)(vfe32_ctrl->extdata))->asfInfo =
		((struct vfe_message *)data)->_u.msgOut.asfInfo;
		((struct vfe_msg_output *)(vfe32_ctrl->extdata))->frameCounter =
		((struct vfe_message *)data)->_u.msgOut.frameCounter;
		*ext  = vfe32_ctrl->extdata;
		*elen = vfe32_ctrl->extlen;
		break;
	case VFE_MSG_STATS_AF:
	case VFE_MSG_STATS_AEC:
	case VFE_MSG_STATS_AWB:
	case VFE_MSG_STATS_IHIST:
	case VFE_MSG_STATS_RS:
	case VFE_MSG_STATS_CS:
		pinfo->sbuf_phy =
		((struct vfe_message *)data)->_u.msgStats.buffer;

		pinfo->frame_id =
		((struct vfe_message *)data)->_u.msgStats.frameCounter;

		break;
	default:
		break;
	} /* switch */
}

static void vfe32_proc_ops(enum VFE32_MESSAGE_ID id, void *msg, size_t len)
{
	struct msm_vfe_resp *rp;

	rp = msm_isp_sync_alloc(sizeof(struct msm_vfe_resp),
		vfe32_ctrl->syncdata, GFP_ATOMIC);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}
	CDBG("vfe32_proc_ops, msgId = %d\n", id);
	rp->evt_msg.type   = MSM_CAMERA_MSG;
	rp->evt_msg.msg_id = id;
	rp->evt_msg.len    = len;
	rp->evt_msg.data   = msg;

	switch (rp->evt_msg.msg_id) {
	case MSG_ID_SNAPSHOT_DONE:
		rp->type = VFE_MSG_SNAPSHOT;
		break;

	case MSG_ID_OUTPUT_P:
		rp->type = VFE_MSG_OUTPUT_P;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT_P,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_OUTPUT_T:
		rp->type = VFE_MSG_OUTPUT_T;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT_T,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_OUTPUT_S:
		rp->type = VFE_MSG_OUTPUT_S;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT_S,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_OUTPUT_V:
		rp->type = VFE_MSG_OUTPUT_V;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT_V,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_STATS_AF:
		rp->type = VFE_MSG_STATS_AF;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_AF,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_AWB:
		rp->type = VFE_MSG_STATS_AWB;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_AWB,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_AEC:
		rp->type = VFE_MSG_STATS_AEC;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_AEC,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_SKIN:
		rp->type = VFE_MSG_STATS_SKIN;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_SKIN,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_IHIST:
		rp->type = VFE_MSG_STATS_IHIST;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_IHIST,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_RS:
		rp->type = VFE_MSG_STATS_RS;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_RS,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_STATS_CS:
		rp->type = VFE_MSG_STATS_CS;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_CS,
				rp->evt_msg.data, NULL, NULL);
		break;

	case MSG_ID_SYNC_TIMER0_DONE:
		rp->type = VFE_MSG_SYNC_TIMER0;
		break;

	case MSG_ID_SYNC_TIMER1_DONE:
		rp->type = VFE_MSG_SYNC_TIMER1;
		break;

	case MSG_ID_SYNC_TIMER2_DONE:
		rp->type = VFE_MSG_SYNC_TIMER2;
		break;

	default:
		rp->type = VFE_MSG_GENERAL;
		break;
	}

	/* save the frame id.*/
	rp->evt_msg.frame_id = rp->phy.frame_id;

	v4l2_subdev_notify(vfe32_ctrl->subdev, NOTIFY_VFE_MSG_EVT, rp);
}

static void vfe_send_outmsg(uint8_t msgid, uint32_t pyaddr,
	uint32_t pcbcraddr)
{
	struct vfe_message msg;
	uint8_t outid;

	msg._d = msgid;   /* now the output mode is redundnat. */

	switch (msgid) {
	case MSG_ID_OUTPUT_P:
		outid = OUTPUT_TYPE_P;
		break;
	case MSG_ID_OUTPUT_V:
		outid = OUTPUT_TYPE_V;
		break;
	case MSG_ID_OUTPUT_T:
		outid = OUTPUT_TYPE_T;
		break;
	case MSG_ID_OUTPUT_S:
		outid = OUTPUT_TYPE_S;
		break;
	default:
		outid = 0xff;  /* -1 for error condition.*/
		break;
	}
	msg._u.msgOut.output_id   = msgid;
	msg._u.msgOut.yBuffer     = pyaddr;
	msg._u.msgOut.cbcrBuffer  = pcbcraddr;
	vfe32_proc_ops(msgid, &msg, sizeof(struct vfe_message));
	return;
}

static void vfe32_stop(void)
{
	uint8_t  axiBusyFlag = true;
	unsigned long flags;

	atomic_set(&vfe32_ctrl->vstate, 0);

	/* for reset hw modules, and send msg when reset_irq comes.*/
	spin_lock_irqsave(&vfe32_ctrl->stop_flag_lock, flags);
	vfe32_ctrl->stop_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);

	/* disable all interrupts.  */
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_DISABLE_ALL_IRQS,
			vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_1);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1,
		vfe32_ctrl->vfebase + VFE_IRQ_CMD);

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time. stop camif immediately. */
	msm_io_w(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
	wmb();
	/* axi halt command. */
	msm_io_w(AXI_HALT,
		vfe32_ctrl->vfebase + VFE_AXI_CMD);
	wmb();
	while (axiBusyFlag) {
		if (msm_io_r(vfe32_ctrl->vfebase + VFE_AXI_STATUS) & 0x1)
			axiBusyFlag = false;
	}
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(AXI_HALT_CLEAR,
		vfe32_ctrl->vfebase + VFE_AXI_CMD);

	/* after axi halt, then ok to apply global reset. */
	/* enable reset_ack and async timer interrupt only while
	stopping the pipeline.*/
	msm_io_w(0xf0000000,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(VFE_RESET_UPON_STOP_CMD,
		vfe32_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static int vfe32_enqueue_free_buf(struct vfe32_output_ch *outch,
	uint32_t paddr, uint32_t y_off, uint32_t cbcr_off)
{
	struct vfe32_free_buf *free_buf = NULL;
	unsigned long flags = 0;
	free_buf = kmalloc(sizeof(struct vfe32_free_buf), GFP_KERNEL);
	if (!free_buf)
		return -ENOMEM;

	spin_lock_irqsave(&outch->free_buf_lock, flags);
	free_buf->paddr = paddr;
	free_buf->y_off = y_off;
	free_buf->cbcr_off = cbcr_off;
	list_add_tail(&free_buf->node, &outch->free_buf_queue);
	CDBG("%s: free_buf paddr = 0x%x, y_off = %d, cbcr_off = %d\n",
		__func__, free_buf->paddr, free_buf->y_off,
		free_buf->cbcr_off);
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
	return 0;
}

static struct vfe32_free_buf *vfe32_dequeue_free_buf(
	struct vfe32_output_ch *outch)
{
	unsigned long flags = 0;
	struct vfe32_free_buf *free_buf = NULL;
	spin_lock_irqsave(&outch->free_buf_lock, flags);
	if (!list_empty(&outch->free_buf_queue)) {
		free_buf = list_first_entry(&outch->free_buf_queue,
			struct vfe32_free_buf, node);
		if (free_buf)
			list_del_init(&free_buf->node);
	}
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
	return free_buf;
}

static void vfe32_reset_free_buf_queue(
	struct vfe32_output_ch *outch)
{
	unsigned long flags = 0;
	struct vfe32_free_buf *free_buf = NULL;
	spin_lock_irqsave(&outch->free_buf_lock, flags);
	while (!list_empty(&outch->free_buf_queue)) {
		free_buf = list_first_entry(&outch->free_buf_queue,
			struct vfe32_free_buf, node);
		if (free_buf) {
			list_del_init(&free_buf->node);
			kfree(free_buf);
		}
	}
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
}

static void vfe32_init_free_buf_queues(void)
{
	INIT_LIST_HEAD(&vfe32_ctrl->outpath.out0.free_buf_queue);
	INIT_LIST_HEAD(&vfe32_ctrl->outpath.out1.free_buf_queue);
	INIT_LIST_HEAD(&vfe32_ctrl->outpath.out2.free_buf_queue);
	spin_lock_init(&vfe32_ctrl->outpath.out0.free_buf_lock);
	spin_lock_init(&vfe32_ctrl->outpath.out1.free_buf_lock);
	spin_lock_init(&vfe32_ctrl->outpath.out2.free_buf_lock);
}

static void vfe32_reset_free_buf_queues(void)
{
	vfe32_reset_free_buf_queue(&vfe32_ctrl->outpath.out0);
	vfe32_reset_free_buf_queue(&vfe32_ctrl->outpath.out1);
	vfe32_reset_free_buf_queue(&vfe32_ctrl->outpath.out2);
}

static int vfe32_config_axi(int mode, struct axidata *ad, uint32_t *ao)
{
	int ret;
	int i;
	uint32_t *p, *p1, *p2;
	int32_t *ch_info;
	struct vfe32_output_ch *outp1, *outp2;
	struct msm_pmem_region *regp1 = NULL;
	struct msm_pmem_region *regp2 = NULL;

	outp1 = NULL;
	outp2 = NULL;

	p = ao + 2;

	/* Update the corresponding write masters for each output*/
	ch_info = ao + V32_AXI_CFG_LEN;
	vfe32_ctrl->outpath.out0.ch0 = 0x0000FFFF & *ch_info;
	vfe32_ctrl->outpath.out0.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe32_ctrl->outpath.out0.ch2 = 0x0000FFFF & *ch_info++;
	vfe32_ctrl->outpath.out1.ch0 = 0x0000FFFF & *ch_info;
	vfe32_ctrl->outpath.out1.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe32_ctrl->outpath.out1.ch2 = 0x0000FFFF & *ch_info++;
	vfe32_ctrl->outpath.out2.ch0 = 0x0000FFFF & *ch_info;
	vfe32_ctrl->outpath.out2.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe32_ctrl->outpath.out2.ch2 = 0x0000FFFF & *ch_info++;

	CDBG("vfe32_config_axi: mode = %d, bufnum1 = %d, bufnum2 = %d\n",
		mode, ad->bufnum1, ad->bufnum2);

	switch (mode) {

	case OUTPUT_2: {
		if (ad->bufnum2 < 3)
			return -EINVAL;
		regp1 = &(ad->region[ad->bufnum1]);
		outp1 = &(vfe32_ctrl->outpath.out0);
		vfe32_ctrl->outpath.output_mode |= VFE32_OUTPUT_MODE_PT;

		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.y_off);

			p1 = ao + 12 + i;  /* wm1 for cbcr */
			*p1 = (regp1->paddr + regp1->info.cbcr_off);
			regp1++;
		}
		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe32_enqueue_free_buf(outp1, regp1->paddr,
				regp1->info.y_off, regp1->info.cbcr_off);
			if (ret < 0)
				return ret;
			regp1++;
		}
	}
		break;

	case OUTPUT_1_AND_2:
		/* use wm0& 4 for thumbnail, wm1&5 for main image.*/
		if ((ad->bufnum1 < 1) || (ad->bufnum2 < 1))
			return -EINVAL;
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_S;  /* main image.*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_PT;  /* thumbnail. */

		regp1 = &(ad->region[0]); /* this is thumbnail buffer. */
		/* this is main image buffer. */
		regp2 = &(ad->region[ad->bufnum1]);
		outp1 = &(vfe32_ctrl->outpath.out0);
		outp2 = &(vfe32_ctrl->outpath.out1); /* snapshot */

		/*  Parse the buffers!!! */
		if (ad->bufnum2 == 1) {	/* assuming bufnum1 = bufnum2 */
			p1 = ao + 6;   /* wm0 ping */
			*p1++ = (regp1->paddr + regp1->info.y_off);
			/* this is to duplicate ping address to pong.*/
			*p1 = (regp1->paddr + regp1->info.y_off);
			p1 = ao + 30;  /* wm4 ping */
			*p1++ = (regp1->paddr + regp1->info.cbcr_off);
			/* this is to duplicate ping address to pong.*/
			*p1 = (regp1->paddr + regp1->info.cbcr_off);
			p1 = ao + 12;   /* wm1 ping */
			*p1++ = (regp2->paddr + regp2->info.y_off);
			/* pong = ping,*/
			*p1 = (regp2->paddr + regp2->info.y_off);
			p1 = ao + 36;  /* wm5 */
			*p1++ = (regp2->paddr + regp2->info.cbcr_off);
			*p1 = (regp2->paddr + regp2->info.cbcr_off);

		} else { /* more than one snapshot */
			/* first fill ping & pong */
			for (i = 0; i < 2; i++) {
				p1 = ao + 6 + i;    /* wm0 for y  */
				*p1 = (regp1->paddr + regp1->info.y_off);
				p1 = ao + 30 + i;  /* wm4 for cbcr */
				*p1 = (regp1->paddr + regp1->info.cbcr_off);
				regp1++;
			}

			for (i = 0; i < 2; i++) {
				p2 = ao + 12 + i;    /* wm1 for y  */
				*p2 = (regp2->paddr + regp2->info.y_off);
				p2 = ao + 36 + i;  /* wm5 for cbcr */
				*p2 = (regp2->paddr + regp2->info.cbcr_off);
				regp2++;
			}

			for (i = 2; i < ad->bufnum1; i++) {
				ret = vfe32_enqueue_free_buf(outp1,
							regp1->paddr,
							regp1->info.y_off,
							regp1->info.cbcr_off);
				if (ret < 0)
					return ret;
				regp1++;
			}
			for (i = 2; i < ad->bufnum2; i++) {
				ret = vfe32_enqueue_free_buf(outp2,
							regp2->paddr,
							regp2->info.y_off,
							regp2->info.cbcr_off);
				if (ret < 0)
					return ret;
				regp2++;
			}
		}
		break;

	case OUTPUT_1_AND_3:
		/* use wm0& 4 for preview, wm1&5 for video.*/
		if ((ad->bufnum1 < 2) || (ad->bufnum2 < 2))
			return -EINVAL;

		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_V;  /* video*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_PT;  /* preview */

		regp1 = &(ad->region[0]); /* this is preview buffer. */
		regp2 = &(ad->region[ad->bufnum1]);/* this is video buffer. */
		outp1 = &(vfe32_ctrl->outpath.out0); /* preview */
		outp2 = &(vfe32_ctrl->outpath.out2); /* video */


		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.y_off);

			p1 = ao + 30 + i;  /* wm1 for cbcr */
			*p1 = (regp1->paddr + regp1->info.cbcr_off);
			regp1++;
		}

		for (i = 0; i < 2; i++) {
			p2 = ao + 12 + i;    /* wm0 for y  */
			*p2 = (regp2->paddr + regp2->info.y_off);

			p2 = ao + 36 + i;  /* wm1 for cbcr */
			*p2 = (regp2->paddr + regp2->info.cbcr_off);
			regp2++;
		}
		for (i = 2; i < ad->bufnum1; i++) {
			ret = vfe32_enqueue_free_buf(outp1, regp1->paddr,
						regp1->info.y_off,
						regp1->info.cbcr_off);
			if (ret < 0)
				return ret;
			regp1++;
		}
		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe32_enqueue_free_buf(outp2, regp2->paddr,
						regp2->info.y_off,
						regp2->info.cbcr_off);
			if (ret < 0)
				return ret;
			regp2++;
		}
		break;
	case CAMIF_TO_AXI_VIA_OUTPUT_2: {  /* use wm0 only */
		if (ad->bufnum2 < 1)
			return -EINVAL;
		CDBG("config axi for raw snapshot.\n");
		vfe32_ctrl->outpath.out1.ch0 = 0; /* raw */
		regp1 = &(ad->region[ad->bufnum1]);
		vfe32_ctrl->outpath.output_mode |= VFE32_OUTPUT_MODE_S;
		p1 = ao + 6;    /* wm0 for y  */
		*p1 = (regp1->paddr + regp1->info.y_off);
		}
		break;
	default:
		break;
	}
	msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[V32_AXI_OUT_CFG].offset,
		ao, vfe32_cmd[V32_AXI_OUT_CFG].length - V32_AXI_CH_INF_LEN);
	return 0;
}

static void vfe32_reset_internal_variables(void)
{
	unsigned long flags;
	vfe32_ctrl->vfeImaskCompositePacked = 0;
	/* state control variables */
	vfe32_ctrl->start_ack_pending = FALSE;
	atomic_set(&irq_cnt, 0);

	spin_lock_irqsave(&vfe32_ctrl->stop_flag_lock, flags);
	vfe32_ctrl->stop_ack_pending  = FALSE;
	spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);

	vfe32_ctrl->reset_ack_pending  = FALSE;

	spin_lock_irqsave(&vfe32_ctrl->update_ack_lock, flags);
	vfe32_ctrl->update_ack_pending = FALSE;
	spin_unlock_irqrestore(&vfe32_ctrl->update_ack_lock, flags);

	vfe32_ctrl->req_stop_video_rec = FALSE;
	vfe32_ctrl->req_start_video_rec = FALSE;

	atomic_set(&vfe32_ctrl->vstate, 0);

	/* 0 for continuous mode, 1 for snapshot mode */
	vfe32_ctrl->operation_mode = 0;
	vfe32_ctrl->outpath.output_mode = 0;
	vfe32_ctrl->vfe_capture_count = 0;

	/* this is unsigned 32 bit integer. */
	vfe32_ctrl->vfeFrameId = 0;
	/* Stats control variables. */
	memset(&(vfe32_ctrl->afStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe32_ctrl->awbStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe32_ctrl->aecStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe32_ctrl->ihistStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe32_ctrl->rsStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe32_ctrl->csStatsControl), 0,
		sizeof(struct vfe_stats_control));
}

static void vfe32_reset(void)
{
	uint32_t vfe_version;
	vfe32_reset_free_buf_queues();
	vfe32_reset_internal_variables();
	vfe_version = msm_io_r(vfe32_ctrl->vfebase);
	CDBG("vfe_version = 0x%x\n", vfe_version);
	/* disable all interrupts.  vfeImaskLocal is also reset to 0
	* to begin with. */
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_io_w(VFE_CLEAR_ALL_IRQS, vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(VFE_CLEAR_ALL_IRQS, vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_IRQ_CMD);

	/* enable reset_ack interrupt.  */
	msm_io_w(VFE_IMASK_WHILE_STOPPING_1,
	vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Write to VFE_GLOBAL_RESET_CMD to reset the vfe hardware. Once reset
	 * is done, hardware interrupt will be generated.  VFE ist processes
	 * the interrupt to complete the function call.  Note that the reset
	 * function is synchronous. */

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(VFE_RESET_UPON_RESET_CMD,
		vfe32_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static int vfe32_operation_config(uint32_t *cmd)
{
	uint32_t *p = cmd;

	vfe32_ctrl->operation_mode = *p;
	vfe32_ctrl->stats_comp = *(++p);

	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_CFG);
	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_MODULE_CFG);
	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_PIXEL_IF_CFG);
	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_REALIGN_BUF);
	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_CHROMA_UP);
	msm_io_w(*(++p), vfe32_ctrl->vfebase + VFE_STATS_CFG);
	return 0;
}

static uint32_t vfe_stats_awb_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	vfe32_ctrl->awbStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_aec_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PONG_ADDR);

	vfe32_ctrl->aecStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_af_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);

	vfe32_ctrl->afStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_ihist_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PONG_ADDR);

	vfe32_ctrl->ihistStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_rs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PONG_ADDR);

	vfe32_ctrl->rsStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_cs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe32_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PONG_ADDR);

	vfe32_ctrl->csStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static void vfe32_start_common(void)
{

	vfe32_ctrl->start_ack_pending = TRUE;
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe32_ctrl->operation_mode, vfe32_ctrl->outpath.output_mode);
	msm_io_w(0x00EFE021, vfe32_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	msm_io_dump(vfe32_ctrl->vfebase, 0x740);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_io_w(1, vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
	wmb();

	atomic_set(&vfe32_ctrl->vstate, 1);
}

#define ENQUEUED_BUFFERS 3
static int vfe32_start_recording(void)
{
	/* Clear out duplicate entries in free_buf qeueue,
	 * because the same number of the buffers were programmed
	 * during AXI config and then enqueued before recording.
	 * TODO: Do AXI config separately for recording at the
	 * time of enqueue */
	int i;
	for (i = 0; i < ENQUEUED_BUFFERS; ++i) {
		struct vfe32_free_buf *free_buf = NULL;
		free_buf = vfe32_dequeue_free_buf(&vfe32_ctrl->outpath.out2);
		kfree(free_buf);
	}

	vfe32_ctrl->req_start_video_rec = TRUE;
	/* Mask with 0x7 to extract the pixel pattern*/
	switch (msm_io_r(vfe32_ctrl->vfebase + VFE_CFG) & 0x7) {
	case VFE_YUV_YCbYCr:
	case VFE_YUV_YCrYCb:
	case VFE_YUV_CbYCrY:
	case VFE_YUV_CrYCbY:
		msm_io_w_mb(1,
		vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		break;
	default:
		break;
	}
	return 0;
}

static int vfe32_stop_recording(void)
{
	vfe32_ctrl->req_stop_video_rec = TRUE;
	/* Mask with 0x7 to extract the pixel pattern*/
	switch (msm_io_r(vfe32_ctrl->vfebase + VFE_CFG) & 0x7) {
	case VFE_YUV_YCbYCr:
	case VFE_YUV_YCrYCb:
	case VFE_YUV_CbYCrY:
	case VFE_YUV_CrYCbY:
		msm_io_w_mb(1,
		vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		break;
	default:
		break;
	}

	return 0;
}

static void vfe32_liveshot(void){
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync)
		p_sync->liveshot_enabled = true;
}

static int vfe32_capture(uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync) {
		p_sync->snap_count = num_frames_capture;
		p_sync->thumb_count = num_frames_capture;
	}
	/* capture command is valid for both idle and active state. */
	vfe32_ctrl->outpath.out1.capture_cnt = num_frames_capture;
	if (vfe32_ctrl->operation_mode == VFE_MODE_OF_OPERATION_SNAPSHOT) {
		vfe32_ctrl->outpath.out0.capture_cnt =
		num_frames_capture;
	}

	vfe32_ctrl->vfe_capture_count = num_frames_capture;
	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->operation_mode == VFE_MODE_OF_OPERATION_SNAPSHOT) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PT) {
			irq_comp_mask |= (0x1 << vfe32_ctrl->outpath.out0.ch0 |
					0x1 << vfe32_ctrl->outpath.out0.ch1);
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_S) {
			irq_comp_mask |=
			(0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8));
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PT) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_S) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		}
	} else {  /* this is raw snapshot mode. */
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_S) {
			irq_comp_mask |=
			(0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8));
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(0x1000, vfe32_ctrl->vfebase +
					VFE_BUS_IO_FORMAT_CFG);
		}
	}
	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe32_start_common();
	msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	/* for debug */
	msm_io_w(1, vfe32_ctrl->vfebase + 0x18C);
	msm_io_w(1, vfe32_ctrl->vfebase + 0x188);
	return 0;
}

static int vfe32_start(void)
{
	uint32_t irq_comp_mask = 0;
	/* start command now is only good for continuous mode. */
	if ((vfe32_ctrl->operation_mode != VFE_MODE_OF_OPERATION_CONTINUOUS) &&
		(vfe32_ctrl->operation_mode != VFE_MODE_OF_OPERATION_VIDEO))
		return 0;
	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PT) {
		irq_comp_mask |= (0x1 << vfe32_ctrl->outpath.out0.ch0 |
			0x1 << vfe32_ctrl->outpath.out0.ch1);
	}

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_V) {
		irq_comp_mask |= (0x1 << (vfe32_ctrl->outpath.out2.ch0 + 16)|
			0x1 << (vfe32_ctrl->outpath.out2.ch1 + 16));
	}

	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PT) {
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
	}
	vfe32_start_common();
	return 0;
}

static void vfe32_update(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->update_ack_lock, flags);
	vfe32_ctrl->update_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe32_ctrl->update_ack_lock, flags);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return;
}

static void vfe32_sync_timer_stop(void)
{
	uint32_t value = 0;
	vfe32_ctrl->sync_timer_state = 0;
	if (vfe32_ctrl->sync_timer_number == 0)
		value = 0x10000;
	else if (vfe32_ctrl->sync_timer_number == 1)
		value = 0x20000;
	else if (vfe32_ctrl->sync_timer_number == 2)
		value = 0x40000;

	/* Timer Stop */
	msm_io_w(value, vfe32_ctrl->vfebase + V32_SYNC_TIMER_OFF);
}

static void vfe32_sync_timer_start(const uint32_t *tbl)
{
	/* set bit 8 for auto increment. */
	uint32_t value = 1;
	uint32_t val;

	vfe32_ctrl->sync_timer_state = *tbl++;
	vfe32_ctrl->sync_timer_repeat_count = *tbl++;
	vfe32_ctrl->sync_timer_number = *tbl++;
	CDBG("%s timer_state %d, repeat_cnt %d timer number %d\n",
		 __func__, vfe32_ctrl->sync_timer_state,
		 vfe32_ctrl->sync_timer_repeat_count,
		 vfe32_ctrl->sync_timer_number);

	if (vfe32_ctrl->sync_timer_state) { /* Start Timer */
		value = value << vfe32_ctrl->sync_timer_number;
	} else { /* Stop Timer */
		CDBG("Failed to Start timer\n");
		return;
	}

	/* Timer Start */
	msm_io_w(value, vfe32_ctrl->vfebase + V32_SYNC_TIMER_OFF);
	/* Sync Timer Line Start */
	value = *tbl++;
	msm_io_w(value, vfe32_ctrl->vfebase + V32_SYNC_TIMER_OFF +
		4 + ((vfe32_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Start */
	value = *tbl++;
	msm_io_w(value, vfe32_ctrl->vfebase + V32_SYNC_TIMER_OFF +
			 8 + ((vfe32_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Duration */
	value = *tbl++;
	val = camio_clk.vfe_clk_rate / 10000;
	val = 10000000 / val;
	val = value * 10000 / val;
	CDBG("%s: Pixel Clk Cycles!!! %d\n", __func__, val);
	msm_io_w(val, vfe32_ctrl->vfebase + V32_SYNC_TIMER_OFF +
		12 + ((vfe32_ctrl->sync_timer_number) * 12));
	/* Timer0 Active High/LOW */
	value = *tbl++;
	msm_io_w(value, vfe32_ctrl->vfebase + V32_SYNC_TIMER_POLARITY_OFF);
	/* Selects sync timer 0 output to drive onto timer1 port */
	value = 0;
	msm_io_w(value, vfe32_ctrl->vfebase + V32_TIMER_SELECT_OFF);
}

static void vfe32_program_dmi_cfg(enum VFE32_DMI_RAM_SEL bankSel)
{
	/* set bit 8 for auto increment. */
	uint32_t value = VFE_DMI_CFG_DEFAULT;
	value += (uint32_t)bankSel;

	msm_io_w(value, vfe32_ctrl->vfebase + VFE_DMI_CFG);
	/* by default, always starts with offset 0.*/
	msm_io_w(0, vfe32_ctrl->vfebase + VFE_DMI_ADDR);
}
static void vfe32_write_gamma_cfg(enum VFE32_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	int i;
	uint32_t value, value1, value2;
	vfe32_program_dmi_cfg(channel_sel);
	/* for loop for extracting init table. */
	for (i = 0 ; i < (VFE32_GAMMA_NUM_ENTRIES/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_io_w((value1), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_io_w((value2), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe32_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe32_write_la_cfg(enum VFE32_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	uint32_t i;
	uint32_t value, value1, value2;

	vfe32_program_dmi_cfg(channel_sel);
	/* for loop for extracting init table. */
	for (i = 0 ; i < (VFE32_LA_TABLE_LENGTH/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_io_w((value1), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_io_w((value2), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe32_program_dmi_cfg(NO_MEM_SELECTED);
}


static int vfe32_proc_general(struct msm_vfe32_cmd *cmd)
{
	int i , rc = 0;
	uint32_t old_val = 0 , new_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;

	CDBG("vfe32_proc_general: cmdID = %s, length = %d\n",
		vfe32_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case V32_RESET:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		vfe32_reset();
		break;
	case V32_START:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_start();
		break;
	case V32_UPDATE:
		vfe32_update();
		break;
	case V32_CAPTURE:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe32_capture(snapshot_cnt);
		break;
	case V32_START_RECORDING:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_start_recording();
		break;
	case V32_STOP_RECORDING:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_stop_recording();
		break;
	case V32_OPERATION_CFG: {
		if (cmd->length != V32_OPERATION_CFG_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V32_OPERATION_CFG_LEN, GFP_ATOMIC);
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			V32_OPERATION_CFG_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe32_operation_config(cmdp);
		}
		break;

	case V32_STATS_AE_START: {
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AE_BG_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
		cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;
	case V32_STATS_AF_START: {
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AF_BF_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
		cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;
	case V32_STATS_AWB_START: {
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AWB_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	case V32_STATS_IHIST_START: {
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= IHIST_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;


	case V32_STATS_RS_START: {
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
		/*
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= RS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		*/
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	case V32_STATS_CS_START: {
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
		/*
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= CS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		*/
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	case V32_MCE_UPDATE:
	case V32_MCE_CFG:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		/* Incrementing with 4 so as to point to the 2nd Register as
		the 2nd register has the mce_enable bit */
		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_CHROMA_EN_OFF + 4);
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_EN_OFF + 4,
					&new_val, 4);
		cmdp_local += 1;

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_CHROMA_EN_OFF + 8);
		new_val = *cmdp_local;
		old_val &= MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_EN_OFF + 8,
		&new_val, 4);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
		cmdp_local, (vfe32_cmd[cmd->id].length));
		}
		break;
	case V32_BLACK_LEVEL_CFG:
		rc = -EFAULT;
		goto proc_general_done;
	case V32_ROLL_OFF_CFG: {
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value) , cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		cmdp_local = cmdp;
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
		cmdp_local, 16);
		cmdp_local += 4;
		vfe32_program_dmi_cfg(ROLLOFF_RAM);
		/* for loop for extrcting init table. */
		for (i = 0 ; i < (VFE32_ROLL_OFF_INIT_TABLE_SIZE * 2) ; i++) {
			msm_io_w(*cmdp_local ,
			vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		CDBG("done writing init table\n");
		/* by default, always starts with offset 0. */
		msm_io_w(LENS_ROLL_OFF_DELTA_TABLE_OFFSET,
		vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		/* for loop for extracting delta table. */
		for (i = 0 ; i < (VFE32_ROLL_OFF_DELTA_TABLE_SIZE * 2) ; i++) {
			msm_io_w(*cmdp_local,
			vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		}
		break;

	case V32_LA_CFG:
	case V32_LA_UPDATE: {
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
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));

		old_val = *cmdp;
		cmdp += 1;
		if (old_val == 0x0)
			vfe32_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0 , cmdp);
		else
			vfe32_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK1 , cmdp);
		cmdp -= 1;
		}
		break;

	case V32_SK_ENHAN_CFG:
	case V32_SK_ENHAN_UPDATE:{
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_SCE_OFF,
				cmdp, V32_SCE_LEN);
		}
		break;

	case V32_LIVESHOT:
		vfe32_liveshot();
		break;

	case V32_LINEARIZATION:
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1,
				cmdp_local, V32_LINEARIZATION_LEN1);
		cmdp_local += 4;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF2,
						cmdp_local,
						V32_LINEARIZATION_LEN2);
		break;

	case V32_DEMOSAICV3:
		if (cmd->length !=
			V32_DEMOSAICV3_0_LEN+V32_DEMOSAICV3_1_LEN) {
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

		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF,
			cmdp_local, V32_DEMOSAICV3_0_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_1_OFF,
			cmdp_local, V32_DEMOSAICV3_1_LEN);
		break;

	case V32_DEMOSAICV3_ABCC_CFG:
		rc = -EFAULT;
		break;

	case V32_DEMOSAICV3_DBCC_CFG:
	case V32_DEMOSAICV3_DBCC_UPDATE:
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF);
		old_val &= DBCC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF,
					cmdp_local, 4);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp_local, (vfe32_cmd[cmd->id].length));
		break;

	case V32_DEMOSAICV3_DBPC_CFG:
	case V32_DEMOSAICV3_DBPC_UPDATE:
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF);
		old_val &= DBPC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_io_memcpy(vfe32_ctrl->vfebase +
			V32_DEMOSAICV3_0_OFF,
			cmdp_local, V32_DEMOSAICV3_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase +
			V32_DEMOSAICV3_DBPC_CFG_OFF,
			cmdp_local, V32_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase +
			V32_DEMOSAICV3_DBPC_CFG_OFF0,
			cmdp_local, V32_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase +
			V32_DEMOSAICV3_DBPC_CFG_OFF1,
			cmdp_local, V32_DEMOSAICV3_DBPC_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase +
			V32_DEMOSAICV3_DBPC_CFG_OFF2,
			cmdp_local, V32_DEMOSAICV3_DBPC_LEN);
		break;

	case V32_RGB_G_CFG: {
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_RGB_G_OFF,
				cmdp, 4);
		cmdp += 1;
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0 , cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0 , cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0 , cmdp);
		cmdp -= 1;
		}
		break;

	case V32_RGB_G_UPDATE: {
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

		msm_io_memcpy(vfe32_ctrl->vfebase + V32_RGB_G_OFF, cmdp, 4);
		old_val = *cmdp;
		cmdp += 1;

		if (old_val) {
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK1 , cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK1 , cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK1 , cmdp);
		} else {
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0 , cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0 , cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0 , cmdp);
		}
		cmdp -= 1;
		}
		break;

	case V32_STATS_AWB_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AWB_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V32_STATS_AE_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AE_BG_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V32_STATS_AF_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AF_BF_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V32_STATS_IHIST_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~IHIST_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V32_STATS_RS_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~RS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V32_STATS_CS_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~CS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V32_STOP:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		vfe32_stop();
		break;

	case V32_SYNC_TIMER_SETTING:
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
		vfe32_sync_timer_start(cmdp);
		break;

	case V32_EZTUNE_CFG: {
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= STATS_ENABLE_MASK;
		*cmdp |= old_val;

		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	default: {
		if (cmd->length != vfe32_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe32_cmd[cmd->id].length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}

		CHECKED_COPY_FROM_USER(cmdp);
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp, (vfe32_cmd[cmd->id].length));
	}
	break;

	}

proc_general_done:
	kfree(cmdp);

	return rc;
}

static void vfe32_stats_af_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->af_ack_lock, flags);
	vfe32_ctrl->afStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->afStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(&vfe32_ctrl->af_ack_lock, flags);
}

static void vfe32_stats_awb_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->awb_ack_lock, flags);
	vfe32_ctrl->awbStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->awbStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(&vfe32_ctrl->awb_ack_lock, flags);
}

static void vfe32_stats_aec_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->aec_ack_lock, flags);
	vfe32_ctrl->aecStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->aecStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(&vfe32_ctrl->aec_ack_lock, flags);
}

static void vfe32_stats_ihist_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe32_ctrl->ihistStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->ihistStatsControl.ackPending = FALSE;
}
static void vfe32_stats_rs_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe32_ctrl->rsStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->rsStatsControl.ackPending = FALSE;
}
static void vfe32_stats_cs_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe32_ctrl->csStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->csStatsControl.ackPending = FALSE;
}


static inline void vfe32_read_irq_status(struct vfe32_irq_status *out)
{
	uint32_t *temp;
	memset(out, 0, sizeof(struct vfe32_irq_status));
	temp = (uint32_t *)(vfe32_ctrl->vfebase + VFE_IRQ_STATUS_0);
	out->vfeIrqStatus0 = msm_io_r(temp);

	temp = (uint32_t *)(vfe32_ctrl->vfebase + VFE_IRQ_STATUS_1);
	out->vfeIrqStatus1 = msm_io_r(temp);

	temp = (uint32_t *)(vfe32_ctrl->vfebase + VFE_CAMIF_STATUS);
	out->camifStatus = msm_io_r(temp);
	CDBG("camifStatus  = 0x%x\n", out->camifStatus);

	/* clear the pending interrupt of the same kind.*/
	msm_io_w(out->vfeIrqStatus0, vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(out->vfeIrqStatus1, vfe32_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_IRQ_CMD);

}

static void vfe32_send_msg_no_payload(enum VFE32_MESSAGE_ID id)
{
	struct vfe_message msg;

	CDBG("vfe32_send_msg_no_payload\n");
	msg._d = id;
	vfe32_proc_ops(id, &msg, 0);
}

static void vfe32_process_reg_update_irq(void)
{
	uint32_t  temp, old_val;
	unsigned long flags;
	if (vfe32_ctrl->req_start_video_rec) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_V) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch1]);
			/* Mask with 0x7 to extract the pixel pattern*/
			switch (msm_io_r(vfe32_ctrl->vfebase + VFE_CFG)
				& 0x7) {
			case VFE_YUV_YCbYCr:
			case VFE_YUV_YCrYCb:
			case VFE_YUV_CbYCrY:
			case VFE_YUV_CrYCbY:
				msm_io_w_mb(1,
				vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
				break;
			default:
				break;
			}
		}
		vfe32_ctrl->req_start_video_rec =  FALSE;
		if (vpe_ctrl && vpe_ctrl->dis_en) {
			old_val = msm_io_r(
				vfe32_ctrl->vfebase + VFE_MODULE_CFG);
			old_val |= RS_CS_ENABLE_MASK;
			msm_io_w(old_val,
				vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		CDBG("start video triggered .\n");
	} else if (vfe32_ctrl->req_stop_video_rec) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_V) {
			msm_io_w(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch0]);
			msm_io_w(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch1]);
			/* Mask with 0x7 to extract the pixel pattern*/
			switch (msm_io_r(vfe32_ctrl->vfebase + VFE_CFG)
				& 0x7) {
			case VFE_YUV_YCbYCr:
			case VFE_YUV_YCrYCb:
			case VFE_YUV_CbYCrY:
			case VFE_YUV_CrYCbY:
				msm_io_w_mb(1,
				vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
				break;
			default:
				break;
			}
		}
		vfe32_ctrl->req_stop_video_rec =  FALSE;
		vfe32_send_msg_no_payload(MSG_ID_STOP_REC_ACK);

		/*disable rs& cs when stop recording. */
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= (~RS_CS_ENABLE_MASK);
		msm_io_w(old_val, vfe32_ctrl->vfebase + VFE_MODULE_CFG);

		CDBG("stop video triggered .\n");
	}
	if (vfe32_ctrl->start_ack_pending == TRUE) {
		vfe32_send_msg_no_payload(MSG_ID_START_ACK);
		vfe32_ctrl->start_ack_pending = FALSE;
	} else {
		spin_lock_irqsave(&vfe32_ctrl->update_ack_lock, flags);
		if (vfe32_ctrl->update_ack_pending == TRUE) {
			vfe32_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe32_ctrl->update_ack_lock, flags);
			vfe32_send_msg_no_payload(MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
				&vfe32_ctrl->update_ack_lock, flags);
		}
	}
	if (vfe32_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_SNAPSHOT) {  /* in snapshot mode */
		/* later we need to add check for live snapshot mode. */
		vfe32_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe32_ctrl->vfe_capture_count == 0) {
			/* stop the bus output:  write master enable = 0*/
			if (vfe32_ctrl->outpath.output_mode &
					VFE32_OUTPUT_MODE_PT) {
				msm_io_w(0, vfe32_ctrl->vfebase +
					vfe32_AXI_WM_CFG[vfe32_ctrl->
						outpath.out0.ch0]);
				msm_io_w(0, vfe32_ctrl->vfebase +
					vfe32_AXI_WM_CFG[vfe32_ctrl->
						outpath.out0.ch1]);
			}
			if (vfe32_ctrl->outpath.output_mode &
					VFE32_OUTPUT_MODE_S) {
				msm_io_w(0, vfe32_ctrl->vfebase +
					vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out1.ch0]);
				msm_io_w(0, vfe32_ctrl->vfebase +
					vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out1.ch1]);
			}

			/* Ensure the write order while writing
			to the command register using the barrier */
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);

			/* Ensure the read order while reading
			to the command register using the barrier */
			temp = msm_io_r_mb(vfe32_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
			/* then do reg_update. */
			msm_io_w(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		}
	} /* if snapshot mode. */
}

static void vfe32_set_default_reg_values(void)
{
	msm_io_w(0x800080, vfe32_ctrl->vfebase + VFE_DEMUX_GAIN_0);
	msm_io_w(0x800080, vfe32_ctrl->vfebase + VFE_DEMUX_GAIN_1);
	/* What value should we program CGC_OVERRIDE to? */
	msm_io_w(0xFFFFF, vfe32_ctrl->vfebase + VFE_CGC_OVERRIDE);

	/* default frame drop period and pattern */
	msm_io_w(0x1f, vfe32_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_CFG);
	msm_io_w(0x1f, vfe32_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_CFG);
	msm_io_w(0xFFFFFFFF, vfe32_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_PATTERN);
	msm_io_w(0xFFFFFFFF,
		vfe32_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_PATTERN);
	msm_io_w(0x1f, vfe32_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y);
	msm_io_w(0x1f, vfe32_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR);
	msm_io_w(0xFFFFFFFF,
		vfe32_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_PATTERN);
	msm_io_w(0xFFFFFFFF,
		vfe32_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR_PATTERN);
	msm_io_w(0, vfe32_ctrl->vfebase + VFE_CLAMP_MIN);
	msm_io_w(0xFFFFFF, vfe32_ctrl->vfebase + VFE_CLAMP_MAX);

	/* stats UB config */
	msm_io_w(0x3980007, vfe32_ctrl->vfebase + VFE_BUS_STATS_AEC_UB_CFG);
	msm_io_w(0x3A00007, vfe32_ctrl->vfebase + VFE_BUS_STATS_AF_UB_CFG);
	msm_io_w(0x3A8000F, vfe32_ctrl->vfebase + VFE_BUS_STATS_AWB_UB_CFG);
	msm_io_w(0x3B80007, vfe32_ctrl->vfebase + VFE_BUS_STATS_RS_UB_CFG);
	msm_io_w(0x3C0001F, vfe32_ctrl->vfebase + VFE_BUS_STATS_CS_UB_CFG);
	msm_io_w(0x3E0001F, vfe32_ctrl->vfebase + VFE_BUS_STATS_HIST_UB_CFG);
}

static void vfe32_process_reset_irq(void)
{
	unsigned long flags;

	atomic_set(&vfe32_ctrl->vstate, 0);

	spin_lock_irqsave(&vfe32_ctrl->stop_flag_lock, flags);
	if (vfe32_ctrl->stop_ack_pending) {
		vfe32_ctrl->stop_ack_pending = FALSE;
		spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);
		vfe32_send_msg_no_payload(MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe32_set_default_reg_values();

		/* reload all write masters. (frame & line)*/
		msm_io_w(0x7FFF, vfe32_ctrl->vfebase + VFE_BUS_CMD);
		vfe32_send_msg_no_payload(MSG_ID_RESET_ACK);
	}
}

static void vfe32_process_camif_sof_irq(void)
{
	uint32_t  temp;

	/* in raw snapshot mode */
	if (vfe32_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
		if (vfe32_ctrl->start_ack_pending) {
			vfe32_send_msg_no_payload(MSG_ID_START_ACK);
			vfe32_ctrl->start_ack_pending = FALSE;
		}
		vfe32_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe32_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			to the command register using the barrier */
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
			temp = msm_io_r_mb(vfe32_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */
	vfe32_send_msg_no_payload(MSG_ID_SOF_ACK);
	vfe32_ctrl->vfeFrameId++;
	CDBG("camif_sof_irq, frameId = %d\n", vfe32_ctrl->vfeFrameId);

	if (vfe32_ctrl->sync_timer_state) {
		if (vfe32_ctrl->sync_timer_repeat_count == 0)
			vfe32_sync_timer_stop();
		else
			vfe32_ctrl->sync_timer_repeat_count--;
	}
}

static void vfe32_process_error_irq(uint32_t errStatus)
{
	uint32_t camifStatus;
	uint32_t *temp;

	if (errStatus & VFE32_IMASK_CAMIF_ERROR) {
		pr_err("vfe32_irq: camif errors\n");
		temp = (uint32_t *)(vfe32_ctrl->vfebase + VFE_CAMIF_STATUS);
		camifStatus = msm_io_r(temp);
		pr_err("camifStatus  = 0x%x\n", camifStatus);
		vfe32_send_msg_no_payload(MSG_ID_CAMIF_ERROR);
	}

	if (errStatus & VFE32_IMASK_BHIST_OVWR)
		pr_err("vfe32_irq: stats bhist overwrite\n");

	if (errStatus & VFE32_IMASK_STATS_CS_OVWR)
		pr_err("vfe32_irq: stats cs overwrite\n");

	if (errStatus & VFE32_IMASK_STATS_IHIST_OVWR)
		pr_err("vfe32_irq: stats ihist overwrite\n");

	if (errStatus & VFE32_IMASK_REALIGN_BUF_Y_OVFL)
		pr_err("vfe32_irq: realign bug Y overflow\n");

	if (errStatus & VFE32_IMASK_REALIGN_BUF_CB_OVFL)
		pr_err("vfe32_irq: realign bug CB overflow\n");

	if (errStatus & VFE32_IMASK_REALIGN_BUF_CR_OVFL)
		pr_err("vfe32_irq: realign bug CR overflow\n");

	if (errStatus & VFE32_IMASK_VIOLATION)
		pr_err("vfe32_irq: violation interrupt\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_0_BUS_OVFL)
		pr_err("vfe32_irq: image master 0 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_1_BUS_OVFL)
		pr_err("vfe32_irq: image master 1 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_2_BUS_OVFL)
		pr_err("vfe32_irq: image master 2 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_3_BUS_OVFL)
		pr_err("vfe32_irq: image master 3 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_4_BUS_OVFL)
		pr_err("vfe32_irq: image master 4 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_5_BUS_OVFL)
		pr_err("vfe32_irq: image master 5 bus overflow\n");

	if (errStatus & VFE32_IMASK_IMG_MAST_6_BUS_OVFL)
		pr_err("vfe32_irq: image master 6 bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_AE_BG_BUS_OVFL)
		pr_err("vfe32_irq: ae/bg stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_AF_BF_BUS_OVFL)
		pr_err("vfe32_irq: af/bf stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_AWB_BUS_OVFL)
		pr_err("vfe32_irq: awb stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_RS_BUS_OVFL)
		pr_err("vfe32_irq: rs stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_CS_BUS_OVFL)
		pr_err("vfe32_irq: cs stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_IHIST_BUS_OVFL)
		pr_err("vfe32_irq: ihist stats bus overflow\n");

	if (errStatus & VFE32_IMASK_STATS_SKIN_BHIST_BUS_OVFL)
		pr_err("vfe32_irq: skin/bhist stats bus overflow\n");

	if (errStatus & VFE32_IMASK_AXI_ERROR)
		pr_err("vfe32_irq: axi error\n");
}

#define VFE32_AXI_OFFSET 0x0050
#define vfe32_get_ch_ping_addr(chn) \
	(msm_io_r(vfe32_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe32_get_ch_pong_addr(chn) \
	(msm_io_r(vfe32_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe32_get_ch_addr(ping_pong, chn) \
	(((ping_pong) & (1 << (chn))) == 0 ? \
	vfe32_get_ch_pong_addr(chn) : vfe32_get_ch_ping_addr(chn))

#define vfe32_put_ch_ping_addr(chn, addr) \
	(msm_io_w((addr), vfe32_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe32_put_ch_pong_addr(chn, addr) \
	(msm_io_w((addr), vfe32_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe32_put_ch_addr(ping_pong, chn, addr) \
	(((ping_pong) & (1 << (chn))) == 0 ?   \
	vfe32_put_ch_pong_addr((chn), (addr)) : \
	vfe32_put_ch_ping_addr((chn), (addr)))

static void vfe32_process_output_path_irq_0(void)
{
	uint32_t ping_pong;
	uint32_t pyaddr, pcbcraddr;
#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	uint8_t out_bool = 0;
	struct vfe32_free_buf *free_buf = NULL;
	free_buf = vfe32_dequeue_free_buf(&vfe32_ctrl->outpath.out0);
	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool =
		((vfe32_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_SNAPSHOT ||
		vfe32_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) &&
		(vfe32_ctrl->vfe_capture_count <= 1)) ||
		free_buf;
	if (out_bool) {
		ping_pong = msm_io_r(vfe32_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		pyaddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		pcbcraddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch1);

		CDBG("output path 0, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
			pyaddr, pcbcraddr);
		if (free_buf) {
			/* Y channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch0,
			free_buf->paddr + free_buf->y_off);
			/* Chroma channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch1,
			free_buf->paddr + free_buf->cbcr_off);
			kfree(free_buf);
		}
		if (vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT) {
			/* will add message for multi-shot. */
			vfe32_ctrl->outpath.out0.capture_cnt--;
			vfe_send_outmsg(MSG_ID_OUTPUT_T, pyaddr,
				pcbcraddr);
		} else {
			/* always send message for continous mode. */
			/* if continuous mode, for display. (preview) */
			vfe_send_outmsg(MSG_ID_OUTPUT_P, pyaddr,
				pcbcraddr);
		}
	} else {
		vfe32_ctrl->outpath.out0.frame_drop_cnt++;
		pr_warning("path_irq_0 - no free buffer!\n");
#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out0.ch1);
		/* Y channel */
		pyaddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out0.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
						(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out0.ch0,
							pyaddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out0.ch0,
							pyaddr_ping);
		/* SWAP chroma channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out0.ch1,
						pcbcraddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out0.ch1,
						pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}
}

static void vfe32_process_output_path_irq_1(void)
{
	uint32_t ping_pong;
	uint32_t pyaddr, pcbcraddr;
#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct vfe32_free_buf *free_buf = NULL;
	free_buf = vfe32_dequeue_free_buf(&vfe32_ctrl->outpath.out1);

	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	-- when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool =
		((vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT ||
			vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) &&
		 (vfe32_ctrl->vfe_capture_count <= 1)) || free_buf;
	if (out_bool) {
		ping_pong = msm_io_r(vfe32_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		pyaddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		pcbcraddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1);

		CDBG("snapshot main, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
			pyaddr, pcbcraddr);
		if (free_buf) {
			/* Y channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0,
			free_buf->paddr + free_buf->y_off);
			/* Chroma channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1,
			free_buf->paddr + free_buf->cbcr_off);
			kfree(free_buf);
		}
		if (vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT ||
			vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
			vfe32_ctrl->outpath.out1.capture_cnt--;
			vfe_send_outmsg(MSG_ID_OUTPUT_S, pyaddr,
				pcbcraddr);
		}
	} else {
		vfe32_ctrl->outpath.out1.frame_drop_cnt++;
		pr_warning("path_irq_1 - no free buffer!\n");
#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out1.ch1);
		/* Y channel */
		pyaddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out1.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
						(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out1.ch0,
							pyaddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out1.ch0,
							pyaddr_ping);
		/* SWAP chroma channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out1.ch1,
						pcbcraddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out1.ch1,
						pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}
}

static void vfe32_process_output_path_irq_2(void)
{
	uint32_t ping_pong;
	uint32_t pyaddr, pcbcraddr;
#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	uint8_t out_bool = 0;
	struct vfe32_free_buf *free_buf = NULL;
	free_buf = vfe32_dequeue_free_buf(&vfe32_ctrl->outpath.out2);

	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	-- when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool =
		((vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT) &&
		(vfe32_ctrl->vfe_capture_count <= 1)) || free_buf;

	CDBG("%s: op mode = %d, capture_cnt = %d\n", __func__,
		 vfe32_ctrl->operation_mode, vfe32_ctrl->vfe_capture_count);

	if (out_bool) {
		ping_pong = msm_io_r(vfe32_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		pyaddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		pcbcraddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch1);

		CDBG("video output, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
			pyaddr, pcbcraddr);

		if (free_buf) {
			/* Y channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch0,
			free_buf->paddr + free_buf->y_off);
			/* Chroma channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch1,
			free_buf->paddr + free_buf->cbcr_off);
			kfree(free_buf);
		}
		vfe_send_outmsg(MSG_ID_OUTPUT_V, pyaddr, pcbcraddr);
	} else {
		vfe32_ctrl->outpath.out2.frame_drop_cnt++;
		pr_warning("path_irq_2 - no free buffer!\n");
#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe32_get_ch_ping_addr(
			vfe32_ctrl->outpath.out2.ch1);
		/* Y channel */
		pyaddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe32_get_ch_pong_addr(
			vfe32_ctrl->outpath.out2.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
						(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out2.ch0,
							pyaddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out2.ch0,
							pyaddr_ping);
		/* SWAP chroma channel*/
		vfe32_put_ch_ping_addr(vfe32_ctrl->outpath.out2.ch1,
						pcbcraddr_pong);
		vfe32_put_ch_pong_addr(vfe32_ctrl->outpath.out2.ch1,
						pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}
}

static void vfe32_process_stats_comb_irq(uint32_t *irqstatus)
{
	return;
}

static uint32_t  vfe32_process_stats_irq_common(uint32_t statsNum,
						uint32_t newAddr) {

	uint32_t pingpongStatus;
	uint32_t returnAddr;
	uint32_t pingpongAddr;

	/* must be 0=ping, 1=pong */
	pingpongStatus =
		((msm_io_r(vfe32_ctrl->vfebase +
		VFE_BUS_PING_PONG_STATUS))
	& ((uint32_t)(1<<(statsNum + 7)))) >> (statsNum + 7);
	/* stats bits starts at 7 */
	CDBG("statsNum %d, pingpongStatus %d\n", statsNum, pingpongStatus);
	pingpongAddr =
		((uint32_t)(vfe32_ctrl->vfebase +
				VFE_BUS_STATS_PING_PONG_BASE)) +
				(3*statsNum)*4 + (1-pingpongStatus)*4;
	returnAddr = msm_io_r((uint32_t *)pingpongAddr);
	msm_io_w(newAddr, (uint32_t *)pingpongAddr);
	return returnAddr;
}

static void
vfe_send_stats_msg(uint32_t bufAddress, uint32_t statsNum)
{
	unsigned long flags;
	struct  vfe_message msg;
	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	msg._u.msgStats.frameCounter = vfe32_ctrl->vfeFrameId;
	msg._u.msgStats.buffer = bufAddress;

	switch (statsNum) {
	case statsAeNum:{
		msg._d = MSG_ID_STATS_AEC;
		spin_lock_irqsave(&vfe32_ctrl->aec_ack_lock, flags);
		vfe32_ctrl->aecStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->aec_ack_lock, flags);
		}
		break;
	case statsAfNum:{
		msg._d = MSG_ID_STATS_AF;
		spin_lock_irqsave(&vfe32_ctrl->af_ack_lock, flags);
		vfe32_ctrl->afStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->af_ack_lock, flags);
		}
		break;
	case statsAwbNum: {
		msg._d = MSG_ID_STATS_AWB;
		spin_lock_irqsave(&vfe32_ctrl->awb_ack_lock, flags);
		vfe32_ctrl->awbStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->awb_ack_lock, flags);
		}
		break;

	case statsIhistNum: {
		msg._d = MSG_ID_STATS_IHIST;
		vfe32_ctrl->ihistStatsControl.ackPending = TRUE;
		}
		break;
	case statsRsNum: {
		msg._d = MSG_ID_STATS_RS;
		vfe32_ctrl->rsStatsControl.ackPending = TRUE;
		}
		break;
	case statsCsNum: {
		msg._d = MSG_ID_STATS_CS;
		vfe32_ctrl->csStatsControl.ackPending = TRUE;
		}
		break;

	default:
		goto stats_done;
	}

	vfe32_proc_ops(msg._d,
		&msg, sizeof(struct vfe_message));
stats_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe32_process_stats_ae_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->aec_ack_lock, flags);
	if (!(vfe32_ctrl->aecStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe32_ctrl->aec_ack_lock, flags);
		vfe32_ctrl->aecStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsAeNum,
			vfe32_ctrl->aecStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->aecStatsControl.bufToRender,
						statsAeNum);
	} else{
		spin_unlock_irqrestore(&vfe32_ctrl->aec_ack_lock, flags);
		vfe32_ctrl->aecStatsControl.droppedStatsFrameCount++;
	}
}

static void vfe32_process_stats_awb_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->awb_ack_lock, flags);
	if (!(vfe32_ctrl->awbStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe32_ctrl->awb_ack_lock, flags);
		vfe32_ctrl->awbStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsAwbNum,
			vfe32_ctrl->awbStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->awbStatsControl.bufToRender,
						statsAwbNum);
	} else{
		spin_unlock_irqrestore(&vfe32_ctrl->awb_ack_lock, flags);
		vfe32_ctrl->awbStatsControl.droppedStatsFrameCount++;
	}
}

static void vfe32_process_stats_af_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe32_ctrl->af_ack_lock, flags);
	if (!(vfe32_ctrl->afStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe32_ctrl->af_ack_lock, flags);
		vfe32_ctrl->afStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsAfNum,
			vfe32_ctrl->afStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->afStatsControl.bufToRender,
						statsAfNum);
	} else{
		spin_unlock_irqrestore(&vfe32_ctrl->af_ack_lock, flags);
		vfe32_ctrl->afStatsControl.droppedStatsFrameCount++;
	}
}

static void vfe32_process_stats_ihist_irq(void)
{
	if (!(vfe32_ctrl->ihistStatsControl.ackPending)) {
		vfe32_ctrl->ihistStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsIhistNum,
			vfe32_ctrl->ihistStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->ihistStatsControl.bufToRender,
						statsIhistNum);
	} else
		vfe32_ctrl->ihistStatsControl.droppedStatsFrameCount++;
}

static void vfe32_process_stats_rs_irq(void)
{
	if (!(vfe32_ctrl->rsStatsControl.ackPending)) {
		vfe32_ctrl->rsStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsRsNum,
			vfe32_ctrl->rsStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->rsStatsControl.bufToRender,
						statsRsNum);
	} else
		vfe32_ctrl->rsStatsControl.droppedStatsFrameCount++;
}

static void vfe32_process_stats_cs_irq(void)
{
	if (!(vfe32_ctrl->csStatsControl.ackPending)) {
		vfe32_ctrl->csStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsCsNum,
			vfe32_ctrl->csStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->csStatsControl.bufToRender,
						statsCsNum);
	} else
		vfe32_ctrl->csStatsControl.droppedStatsFrameCount++;
}

static void vfe32_do_tasklet(unsigned long data)
{
	unsigned long flags;

	struct vfe32_isr_queue_cmd *qcmd = NULL;

	CDBG("=== vfe32_do_tasklet start ===\n");

	while (atomic_read(&irq_cnt)) {
		spin_lock_irqsave(&vfe32_ctrl->tasklet_lock, flags);
		qcmd = list_first_entry(&vfe32_ctrl->tasklet_q,
			struct vfe32_isr_queue_cmd, list);
		atomic_sub(1, &irq_cnt);

		if (!qcmd) {
			spin_unlock_irqrestore(&vfe32_ctrl->tasklet_lock,
				flags);
			return;
		}

		list_del(&qcmd->list);
		spin_unlock_irqrestore(&vfe32_ctrl->tasklet_lock,
			flags);

		/* interrupt to be processed,  *qcmd has the payload.  */
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_REG_UPDATE_MASK) {
			CDBG("irq	regUpdateIrq\n");
			vfe32_process_reg_update_irq();
		}

		if (qcmd->vfeInterruptStatus1 &
				VFE_IMASK_WHILE_STOPPING_1) {
			CDBG("irq	resetAckIrq\n");
			vfe32_process_reset_irq();
		}

		if (atomic_read(&vfe32_ctrl->vstate)) {
			if (qcmd->vfeInterruptStatus1 &
					VFE32_IMASK_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe32_process_error_irq(
					qcmd->vfeInterruptStatus1 &
					VFE32_IMASK_ERROR_ONLY_1);
			}
			/* next, check output path related interrupts. */
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK) {
				CDBG("Image composite done 0 irq occured.\n");
				vfe32_process_output_path_irq_0();
			}
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK) {
				CDBG("Image composite done 1 irq occured.\n");
				vfe32_process_output_path_irq_1();
			}
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE2_MASK) {
				CDBG("Image composite done 2 irq occured.\n");
				vfe32_process_output_path_irq_2();
			}
			/* in snapshot mode if done then send
			snapshot done message */
			if (vfe32_ctrl->operation_mode ==
				VFE_MODE_OF_OPERATION_SNAPSHOT ||
				vfe32_ctrl->operation_mode ==
				VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
				if ((vfe32_ctrl->outpath.out0.capture_cnt == 0)
						&& (vfe32_ctrl->outpath.out1.
						capture_cnt == 0)) {
					vfe32_send_msg_no_payload(
						MSG_ID_SNAPSHOT_DONE);

					/* Ensure the write order while writing
					to the cmd register using barrier */
					msm_io_w_mb(
						CAMIF_COMMAND_STOP_IMMEDIATELY,
						vfe32_ctrl->vfebase +
						VFE_CAMIF_COMMAND);
				}
			}
			/* then process stats irq. */
			if (vfe32_ctrl->stats_comp) {
				/* process stats comb interrupt. */
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK) {
					CDBG("Stats composite irq occured.\n");
					vfe32_process_stats_comb_irq(
						&qcmd->vfeInterruptStatus0);
				}
			} else {
				/* process individual stats interrupt. */
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_AEC) {
					CDBG("Stats AEC irq occured.\n");
					vfe32_process_stats_ae_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_AWB) {
					CDBG("Stats AWB irq occured.\n");
					vfe32_process_stats_awb_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_AF) {
					CDBG("Stats AF irq occured.\n");
					vfe32_process_stats_af_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_IHIST) {
					CDBG("Stats IHIST irq occured.\n");
					vfe32_process_stats_ihist_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_RS) {
					CDBG("Stats RS irq occured.\n");
					vfe32_process_stats_rs_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_CS) {
					CDBG("Stats CS irq occured.\n");
					vfe32_process_stats_cs_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER0) {
					CDBG("SYNC_TIMER 0 irq occured.\n");
					vfe32_send_msg_no_payload(
						MSG_ID_SYNC_TIMER0_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER1) {
					CDBG("SYNC_TIMER 1 irq occured.\n");
					vfe32_send_msg_no_payload(
						MSG_ID_SYNC_TIMER1_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER2) {
					CDBG("SYNC_TIMER 2 irq occured.\n");
					vfe32_send_msg_no_payload(
						MSG_ID_SYNC_TIMER2_DONE);
				}
			}
		}
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_CAMIF_SOF_MASK) {
			CDBG("irq	camifSofIrq\n");
			vfe32_process_camif_sof_irq();
		}
		kfree(qcmd);
	}
	CDBG("=== vfe32_do_tasklet end ===\n");
}

DECLARE_TASKLET(vfe32_tasklet, vfe32_do_tasklet, 0);

static irqreturn_t vfe32_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	struct vfe32_irq_status irq;
	struct vfe32_isr_queue_cmd *qcmd;

	CDBG("vfe_parse_irq\n");

	vfe32_read_irq_status(&irq);

	if ((irq.vfeIrqStatus0 == 0) && (irq.vfeIrqStatus1 == 0)) {
		CDBG("vfe_parse_irq: vfeIrqStatus0 & 1 are both 0!\n");
		return IRQ_HANDLED;
	}

	qcmd = kzalloc(sizeof(struct vfe32_isr_queue_cmd),
		GFP_ATOMIC);
	if (!qcmd) {
		pr_err("vfe_parse_irq: qcmd malloc failed!\n");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&vfe32_ctrl->stop_flag_lock, flags);
	if (vfe32_ctrl->stop_ack_pending) {
		irq.vfeIrqStatus0 &= VFE_IMASK_WHILE_STOPPING_0;
		irq.vfeIrqStatus1 &= VFE_IMASK_WHILE_STOPPING_1;
	}
	spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);

	CDBG("vfe_parse_irq: Irq_status0 = 0x%x, Irq_status1 = 0x%x.\n",
		irq.vfeIrqStatus0, irq.vfeIrqStatus1);

	qcmd->vfeInterruptStatus0 = irq.vfeIrqStatus0;
	qcmd->vfeInterruptStatus1 = irq.vfeIrqStatus1;

	spin_lock_irqsave(&vfe32_ctrl->tasklet_lock, flags);
	list_add_tail(&qcmd->list, &vfe32_ctrl->tasklet_q);

	atomic_add(1, &irq_cnt);
	spin_unlock_irqrestore(&vfe32_ctrl->tasklet_lock, flags);
	tasklet_schedule(&vfe32_tasklet);
	return IRQ_HANDLED;
}

static int vfe32_resource_init(struct platform_device *pdev, void *sdata)
{
	struct resource	*vfemem, *vfeirq, *vfeio;
	int rc;
	struct msm_camera_sensor_info *s_info;
	s_info = pdev->dev.platform_data;

	pdev->resource = s_info->resource;
	pdev->num_resources = s_info->num_resources;

	vfemem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!vfemem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}

	vfeirq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!vfeirq) {
		pr_err("%s: no irq resource?\n", __func__);
		return -ENODEV;
	}

	vfeio = request_mem_region(vfemem->start,
		resource_size(vfemem), pdev->name);
	if (!vfeio) {
		pr_err("%s: VFE region already claimed\n", __func__);
		return -EBUSY;
	}

	vfe32_ctrl = kzalloc(sizeof(struct vfe32_ctrl_type), GFP_KERNEL);
	if (!vfe32_ctrl) {
		rc = -ENOMEM;
		goto cmd_init_failed1;
	}

	vfe32_ctrl->vfeirq = vfeirq->start;

	vfe32_ctrl->vfebase =
		ioremap(vfemem->start, (vfemem->end - vfemem->start) + 1);
	if (!vfe32_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto cmd_init_failed2;
	}

	vfe32_ctrl->extdata =
		kmalloc(sizeof(struct vfe32_frame_extra), GFP_KERNEL);
	if (!vfe32_ctrl->extdata) {
		rc = -ENOMEM;
		goto cmd_init_failed3;
	}

	vfe32_ctrl->extlen = sizeof(struct vfe32_frame_extra);

	spin_lock_init(&vfe32_ctrl->stop_flag_lock);
	spin_lock_init(&vfe32_ctrl->state_lock);
	spin_lock_init(&vfe32_ctrl->io_lock);
	spin_lock_init(&vfe32_ctrl->update_ack_lock);
	spin_lock_init(&vfe32_ctrl->tasklet_lock);

	spin_lock_init(&vfe32_ctrl->aec_ack_lock);
	spin_lock_init(&vfe32_ctrl->awb_ack_lock);
	spin_lock_init(&vfe32_ctrl->af_ack_lock);
	INIT_LIST_HEAD(&vfe32_ctrl->tasklet_q);
	vfe32_init_free_buf_queues();

	vfe32_ctrl->syncdata = sdata;
	vfe32_ctrl->vfemem = vfemem;
	vfe32_ctrl->vfeio  = vfeio;
	return 0;

cmd_init_failed3:
	free_irq(vfe32_ctrl->vfeirq, 0);
	iounmap(vfe32_ctrl->vfebase);
cmd_init_failed2:
	kfree(vfe32_ctrl);
cmd_init_failed1:
	release_mem_region(vfemem->start, (vfemem->end - vfemem->start) + 1);
	return rc;
}

static long msm_vfe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int subdev_cmd, void *arg)
{
	struct msm_vfe32_cmd vfecmd;
	struct msm_camvfe_params *vfe_params =
		(struct msm_camvfe_params *)arg;
	struct msm_vfe_cfg_cmd *cmd = vfe_params->vfe_cfg;
	void *data = vfe_params->data;

	long rc = 0;
	uint32_t i = 0;
	struct vfe_cmd_stats_buf *scfg = NULL;
	struct msm_pmem_region   *regptr = NULL;
	struct vfe_cmd_stats_ack *sack = NULL;
	if (cmd->cmd_type != CMD_FRAME_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_AEC_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_AWB_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_IHIST_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_RS_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_CS_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_AF_BUF_RELEASE) {
		if (copy_from_user(&vfecmd,
				(void __user *)(cmd->value),
				sizeof(vfecmd))) {
			pr_err("%s %d: copy_from_user failed\n", __func__,
				__LINE__);
			return -EFAULT;
		}
	} else {
	/* here eith stats release or frame release. */
		if (cmd->cmd_type != CMD_FRAME_BUF_RELEASE) {
			/* then must be stats release. */
			if (!data)
				return -EFAULT;
			sack = kmalloc(sizeof(struct vfe_cmd_stats_ack),
							GFP_ATOMIC);
			if (!sack)
				return -ENOMEM;

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
		struct axidata *axid;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			goto vfe32_config_done;
		}

		scfg =
			kmalloc(sizeof(struct vfe_cmd_stats_buf),
				GFP_ATOMIC);
		if (!scfg) {
			rc = -ENOMEM;
			goto vfe32_config_done;
		}
		regptr = axid->region;
		if (axid->bufnum1 > 0) {
			for (i = 0; i < axid->bufnum1; i++) {
				scfg->statsBuf[i] =
					(uint32_t)(regptr->paddr);
				regptr++;
			}
		}
		/* individual */
		switch (cmd->cmd_type) {
		case CMD_STATS_AEC_ENABLE:
			rc = vfe_stats_aec_buf_init(scfg);
			break;
		case CMD_STATS_AF_ENABLE:
			rc = vfe_stats_af_buf_init(scfg);
			break;
		case CMD_STATS_AWB_ENABLE:
			rc = vfe_stats_awb_buf_init(scfg);
			break;
		case CMD_STATS_IHIST_ENABLE:
			rc = vfe_stats_ihist_buf_init(scfg);
			break;
		case CMD_STATS_RS_ENABLE:
			rc = vfe_stats_rs_buf_init(scfg);
			break;
		case CMD_STATS_CS_ENABLE:
			rc = vfe_stats_cs_buf_init(scfg);
			break;
		}
	}
	switch (cmd->cmd_type) {
	case CMD_GENERAL:
		rc = vfe32_proc_general(&vfecmd);
		break;
	case CMD_FRAME_BUF_RELEASE: {
		struct msm_frame *b;
		unsigned long p;
		struct vfe32_output_ch *outch = NULL;
		if (!data) {
			rc = -EFAULT;
			break;
		}

		b = (struct msm_frame *)(cmd->value);
		p = *(unsigned long *)data;

		CDBG("CMD_FRAME_BUF_RELEASE b->path = %d\n", b->path);

		if ((b->path & OUTPUT_TYPE_P) || (b->path & OUTPUT_TYPE_T)) {
			CDBG("CMD_FRAME_BUF_RELEASE got free buffer\n");
			outch = &vfe32_ctrl->outpath.out0;
		} else if (b->path & OUTPUT_TYPE_S) {
			outch = &vfe32_ctrl->outpath.out1;
		} else if (b->path & OUTPUT_TYPE_V) {
			outch = &vfe32_ctrl->outpath.out2;
		} else {
			rc = -EFAULT;
			break;
		}

		rc = vfe32_enqueue_free_buf(outch, p, b->y_off, b->cbcr_off);
	}
		break;

	case CMD_SNAP_BUF_RELEASE:
		break;
	case CMD_STATS_AEC_BUF_RELEASE:
		vfe32_stats_aec_ack(sack);
		break;
	case CMD_STATS_AF_BUF_RELEASE:
		vfe32_stats_af_ack(sack);
		break;
	case CMD_STATS_AWB_BUF_RELEASE:
		vfe32_stats_awb_ack(sack);
		break;

	case CMD_STATS_IHIST_BUF_RELEASE:
		vfe32_stats_ihist_ack(sack);
		break;
	case CMD_STATS_RS_BUF_RELEASE:
		vfe32_stats_rs_ack(sack);
		break;
	case CMD_STATS_CS_BUF_RELEASE:
		vfe32_stats_cs_ack(sack);
		break;

	case CMD_AXI_CFG_PREVIEW: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			break;
		}
		axio =
			kmalloc(vfe32_cmd[V32_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[V32_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_2, axid, axio);
		kfree(axio);
	}
		break;

	case CMD_RAW_PICT_AXI_CFG: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			break;
		}
		axio = kmalloc(vfe32_cmd[V32_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[V32_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(CAMIF_TO_AXI_VIA_OUTPUT_2, axid, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_SNAP: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid)
			return -EFAULT;
		axio =
			kmalloc(vfe32_cmd[V32_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[V32_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_1_AND_2, axid, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_VIDEO: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			break;
		}

		axio = kmalloc(vfe32_cmd[V32_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[V32_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_1_AND_3, axid, axio);
		kfree(axio);
	}
		break;
	default:
		break;
	}
vfe32_config_done:
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

int msm_vfe_subdev_init(struct v4l2_subdev *sd, void *data,
	struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	v4l2_subdev_init(sd, &msm_vfe_subdev_ops);
	v4l2_set_subdev_hostdata(sd, data);
	snprintf(sd->name, sizeof(sd->name), "vfe3.2");

	vfe_syncdata = data;

	camio_clk = camdev->ioclk;

	rc = vfe32_resource_init(pdev, vfe_syncdata);
	if (rc < 0)
		return rc;

	vfe32_ctrl->subdev = sd;
	/* Bring up all the required GPIOs and Clocks */
	rc = msm_camio_enable(pdev);
	msm_camio_set_perf_lvl(S_INIT);
	msm_camio_set_perf_lvl(S_PREVIEW);

	/* TO DO: Need to release the VFE resources */
	rc = request_irq(vfe32_ctrl->vfeirq, vfe32_parse_irq,
			IRQF_TRIGGER_RISING, "vfe", 0);

	return rc;
}

void msm_vfe_subdev_release(struct platform_device *pdev)
{
	struct resource	*vfemem, *vfeio;

	vfe32_reset_free_buf_queues();
	CDBG("%s, free_irq\n", __func__);
	free_irq(vfe32_ctrl->vfeirq, 0);
	tasklet_kill(&vfe32_tasklet);

	if (atomic_read(&irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	vfemem = vfe32_ctrl->vfemem;
	vfeio  = vfe32_ctrl->vfeio;

	kfree(vfe32_ctrl->extdata);
	iounmap(vfe32_ctrl->vfebase);
	kfree(vfe32_ctrl);
	vfe32_ctrl = NULL;
	release_mem_region(vfemem->start, (vfemem->end - vfemem->start) + 1);
	CDBG("%s, msm_camio_disable\n", __func__);
	msm_camio_disable(pdev);
	msm_camio_set_perf_lvl(S_EXIT);

	vfe_syncdata = NULL;
}

void msm_camvpe_fn_init(struct msm_camvpe_fn *fptr, void *data)
{
	fptr->vpe_reg		= msm_vpe_reg;
	fptr->send_frame_to_vpe	= msm_send_frame_to_vpe;
	fptr->vpe_config	= msm_vpe_config;
	fptr->vpe_cfg_update	= msm_vpe_cfg_update;
	fptr->dis		= &(vpe_ctrl->dis_en);
	vpe_ctrl->syncdata = data;
}
