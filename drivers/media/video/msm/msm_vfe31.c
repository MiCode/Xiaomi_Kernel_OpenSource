/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <asm/atomic.h>

#include "msm_vfe31.h"
#include "msm_vpe1.h"

atomic_t irq_cnt;

#define CHECKED_COPY_FROM_USER(in) {					\
	if (copy_from_user((in), (void __user *)cmd->value,		\
			cmd->length)) {					\
		rc = -EFAULT;						\
		break;							\
	}								\
}

static struct vfe31_ctrl_type *vfe31_ctrl;
static struct msm_camera_io_clk camio_clk;
static void *vfe_syncdata;
static void vfe31_send_msg_no_payload(enum VFE31_MESSAGE_ID id);
static void vfe31_reset_hist_cfg(void);

struct vfe31_isr_queue_cmd {
	struct list_head list;
	uint32_t                           vfeInterruptStatus0;
	uint32_t                           vfeInterruptStatus1;
	uint32_t                           vfePingPongStatus;
	struct vfe_frame_asf_info          vfeAsfFrameInfo;
	struct vfe_frame_bpc_info          vfeBpcFrameInfo;
	struct vfe_msg_camif_status        vfeCamifStatusLocal;
};

static struct vfe31_cmd_type vfe31_cmd[] = {
/* 0*/	{V31_DUMMY_0},
		{V31_SET_CLK},
		{V31_RESET},
		{V31_START},
		{V31_TEST_GEN_START},
/* 5*/	{V31_OPERATION_CFG, V31_OPERATION_CFG_LEN},
		{V31_AXI_OUT_CFG, V31_AXI_OUT_LEN, V31_AXI_OUT_OFF, 0xFF},
		{V31_CAMIF_CFG, V31_CAMIF_LEN, V31_CAMIF_OFF, 0xFF},
		{V31_AXI_INPUT_CFG},
		{V31_BLACK_LEVEL_CFG, V31_BLACK_LEVEL_LEN, V31_BLACK_LEVEL_OFF,
		0xFF},
/*10*/  {V31_ROLL_OFF_CFG, V31_ROLL_OFF_CFG_LEN, V31_ROLL_OFF_CFG_OFF,
		0xFF},
		{V31_DEMUX_CFG, V31_DEMUX_LEN, V31_DEMUX_OFF, 0xFF},
		{V31_DEMOSAIC_0_CFG, V31_DEMOSAIC_0_LEN, V31_DEMOSAIC_0_OFF,
		0xFF},
		{V31_DEMOSAIC_1_CFG, V31_DEMOSAIC_1_LEN, V31_DEMOSAIC_1_OFF,
		0xFF},
		{V31_DEMOSAIC_2_CFG, V31_DEMOSAIC_2_LEN, V31_DEMOSAIC_2_OFF,
		0xFF},
/*15*/	{V31_FOV_CFG, V31_FOV_LEN, V31_FOV_OFF, 0xFF},
		{V31_MAIN_SCALER_CFG, V31_MAIN_SCALER_LEN, V31_MAIN_SCALER_OFF,
		0xFF},
		{V31_WB_CFG, V31_WB_LEN, V31_WB_OFF, 0xFF},
		{V31_COLOR_COR_CFG, V31_COLOR_COR_LEN, V31_COLOR_COR_OFF, 0xFF},
		{V31_RGB_G_CFG, V31_RGB_G_LEN, V31_RGB_G_OFF, 0xFF},
/*20*/	{V31_LA_CFG, V31_LA_LEN, V31_LA_OFF, 0xFF },
		{V31_CHROMA_EN_CFG, V31_CHROMA_EN_LEN, V31_CHROMA_EN_OFF, 0xFF},
		{V31_CHROMA_SUP_CFG, V31_CHROMA_SUP_LEN, V31_CHROMA_SUP_OFF,
		0xFF},
		{V31_MCE_CFG, V31_MCE_LEN, V31_MCE_OFF, 0xFF},
		{V31_SK_ENHAN_CFG, V31_SCE_LEN, V31_SCE_OFF, 0xFF},
/*25*/	{V31_ASF_CFG, V31_ASF_LEN, V31_ASF_OFF, 0xFF},
		{V31_S2Y_CFG, V31_S2Y_LEN, V31_S2Y_OFF, 0xFF},
		{V31_S2CbCr_CFG, V31_S2CbCr_LEN, V31_S2CbCr_OFF, 0xFF},
		{V31_CHROMA_SUBS_CFG, V31_CHROMA_SUBS_LEN, V31_CHROMA_SUBS_OFF,
		0xFF},
		{V31_OUT_CLAMP_CFG, V31_OUT_CLAMP_LEN, V31_OUT_CLAMP_OFF,
		0xFF},
/*30*/	{V31_FRAME_SKIP_CFG, V31_FRAME_SKIP_LEN, V31_FRAME_SKIP_OFF,
		0xFF},
		{V31_DUMMY_1},
		{V31_DUMMY_2},
		{V31_DUMMY_3},
		{V31_UPDATE},
/*35*/	{V31_BL_LVL_UPDATE, V31_BLACK_LEVEL_LEN, V31_BLACK_LEVEL_OFF,
		0xFF},
		{V31_DEMUX_UPDATE, V31_DEMUX_LEN, V31_DEMUX_OFF, 0xFF},
		{V31_DEMOSAIC_1_UPDATE, V31_DEMOSAIC_1_LEN, V31_DEMOSAIC_1_OFF,
		0xFF},
		{V31_DEMOSAIC_2_UPDATE, V31_DEMOSAIC_2_LEN, V31_DEMOSAIC_2_OFF,
		0xFF},
		{V31_FOV_UPDATE, V31_FOV_LEN, V31_FOV_OFF, 0xFF},
/*40*/	{V31_MAIN_SCALER_UPDATE, V31_MAIN_SCALER_LEN, V31_MAIN_SCALER_OFF,
		0xFF},
		{V31_WB_UPDATE, V31_WB_LEN, V31_WB_OFF, 0xFF},
		{V31_COLOR_COR_UPDATE, V31_COLOR_COR_LEN, V31_COLOR_COR_OFF,
		0xFF},
		{V31_RGB_G_UPDATE, V31_RGB_G_LEN, V31_CHROMA_EN_OFF, 0xFF},
		{V31_LA_UPDATE, V31_LA_LEN, V31_LA_OFF, 0xFF },
/*45*/	{V31_CHROMA_EN_UPDATE, V31_CHROMA_EN_LEN, V31_CHROMA_EN_OFF,
		0xFF},
		{V31_CHROMA_SUP_UPDATE, V31_CHROMA_SUP_LEN, V31_CHROMA_SUP_OFF,
		0xFF},
		{V31_MCE_UPDATE, V31_MCE_LEN, V31_MCE_OFF, 0xFF},
		{V31_SK_ENHAN_UPDATE, V31_SCE_LEN, V31_SCE_OFF, 0xFF},
		{V31_S2CbCr_UPDATE, V31_S2CbCr_LEN, V31_S2CbCr_OFF, 0xFF},
/*50*/	{V31_S2Y_UPDATE, V31_S2Y_LEN, V31_S2Y_OFF, 0xFF},
		{V31_ASF_UPDATE, V31_ASF_UPDATE_LEN, V31_ASF_OFF, 0xFF},
		{V31_FRAME_SKIP_UPDATE},
		{V31_CAMIF_FRAME_UPDATE},
		{V31_STATS_AF_UPDATE, V31_STATS_AF_LEN, V31_STATS_AF_OFF},
/*55*/	{V31_STATS_AE_UPDATE, V31_STATS_AE_LEN, V31_STATS_AE_OFF},
		{V31_STATS_AWB_UPDATE, V31_STATS_AWB_LEN, V31_STATS_AWB_OFF},
		{V31_STATS_RS_UPDATE, V31_STATS_RS_LEN, V31_STATS_RS_OFF},
		{V31_STATS_CS_UPDATE, V31_STATS_CS_LEN, V31_STATS_CS_OFF},
		{V31_STATS_SKIN_UPDATE},
/*60*/	{V31_STATS_IHIST_UPDATE, V31_STATS_IHIST_LEN, V31_STATS_IHIST_OFF},
		{V31_DUMMY_4},
		{V31_EPOCH1_ACK},
		{V31_EPOCH2_ACK},
		{V31_START_RECORDING},
/*65*/	{V31_STOP_RECORDING},
		{V31_DUMMY_5},
		{V31_DUMMY_6},
		{V31_CAPTURE, V31_CAPTURE_LEN, 0xFF},
		{V31_DUMMY_7},
/*70*/	{V31_STOP},
		{V31_GET_HW_VERSION},
		{V31_GET_FRAME_SKIP_COUNTS},
		{V31_OUTPUT1_BUFFER_ENQ},
		{V31_OUTPUT2_BUFFER_ENQ},
/*75*/	{V31_OUTPUT3_BUFFER_ENQ},
		{V31_JPEG_OUT_BUF_ENQ},
		{V31_RAW_OUT_BUF_ENQ},
		{V31_RAW_IN_BUF_ENQ},
		{V31_STATS_AF_ENQ},
/*80*/	{V31_STATS_AE_ENQ},
		{V31_STATS_AWB_ENQ},
		{V31_STATS_RS_ENQ},
		{V31_STATS_CS_ENQ},
		{V31_STATS_SKIN_ENQ},
/*85*/	{V31_STATS_IHIST_ENQ},
		{V31_DUMMY_8},
		{V31_JPEG_ENC_CFG},
		{V31_DUMMY_9},
		{V31_STATS_AF_START, V31_STATS_AF_LEN, V31_STATS_AF_OFF},
/*90*/	{V31_STATS_AF_STOP},
		{V31_STATS_AE_START, V31_STATS_AE_LEN, V31_STATS_AE_OFF},
		{V31_STATS_AE_STOP},
		{V31_STATS_AWB_START, V31_STATS_AWB_LEN, V31_STATS_AWB_OFF},
		{V31_STATS_AWB_STOP},
/*95*/	{V31_STATS_RS_START, V31_STATS_RS_LEN, V31_STATS_RS_OFF},
		{V31_STATS_RS_STOP},
		{V31_STATS_CS_START, V31_STATS_CS_LEN, V31_STATS_CS_OFF},
		{V31_STATS_CS_STOP},
		{V31_STATS_SKIN_START},
/*100*/	{V31_STATS_SKIN_STOP},
		{V31_STATS_IHIST_START,
		V31_STATS_IHIST_LEN, V31_STATS_IHIST_OFF},
		{V31_STATS_IHIST_STOP},
		{V31_DUMMY_10},
		{V31_SYNC_TIMER_SETTING, V31_SYNC_TIMER_LEN,
			V31_SYNC_TIMER_OFF},
/*105*/	{V31_ASYNC_TIMER_SETTING, V31_ASYNC_TIMER_LEN, V31_ASYNC_TIMER_OFF},
		{V31_LIVESHOT},
		{V31_ZSL, V31_CAPTURE_LEN, 0xFF},
		{V31_STEREOCAM},
		{V31_LA_SETUP},
/*110*/	{V31_XBAR_CFG, V31_XBAR_CFG_LEN, V31_XBAR_CFG_OFF},
/*111*/	{V31_EZTUNE_CFG, V31_EZTUNE_CFG_LEN, V31_EZTUNE_CFG_OFF},
};

uint32_t vfe31_AXI_WM_CFG[] = {
	0x0000004C,
	0x00000064,
	0x0000007C,
	0x00000094,
	0x000000AC,
	0x000000C4,
	0x000000DC,
};

static const char *vfe31_general_cmd[] = {
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
	"DEMOSAIC_0_CFG",  /* general */
	"DEMOSAIC_1_CFG",  /* ABF     */
	"DEMOSAIC_2_CFG",  /* BPC     */
	"FOV_CFG",  /* 15  */
	"MAIN_SCALER_CFG",
	"WB_CFG",
	"COLOR_COR_CFG",
	"RGB_G_CFG",
	"LA_CFG",  /* 20 */
	"CHROMA_EN_CFG",
	"CHROMA_SUP_CFG",
	"MCE_CFG",
	"SK_ENHAN_CFG",
	"ASF_CFG",  /* 25 */
	"S2Y_CFG",
	"S2CbCr_CFG",
	"CHROMA_SUBS_CFG",
	"OUT_CLAMP_CFG",
	"FRAME_SKIP_CFG",  /* 30 */
	"DUMMY_1",
	"DUMMY_2",
	"DUMMY_3",
	"UPDATE",
	"BL_LVL_UPDATE",  /* 35 */
	"DEMUX_UPDATE",
	"DEMOSAIC_1_UPDATE",  /* BPC */
	"DEMOSAIC_2_UPDATE",  /* ABF */
	"FOV_UPDATE",
	"MAIN_SCALER_UPDATE",  /* 40 */
	"WB_UPDATE",
	"COLOR_COR_UPDATE",
	"RGB_G_UPDATE",
	"LA_UPDATE",
	"CHROMA_EN_UPDATE",  /* 45 */
	"CHROMA_SUP_UPDATE",
	"MCE_UPDATE",
	"SK_ENHAN_UPDATE",
	"S2CbCr_UPDATE",
	"S2Y_UPDATE",  /* 50 */
	"ASF_UPDATE",
	"FRAME_SKIP_UPDATE",
	"CAMIF_FRAME_UPDATE",
	"STATS_AF_UPDATE",
	"STATS_AE_UPDATE",  /* 55 */
	"STATS_AWB_UPDATE",
	"STATS_RS_UPDATE",
	"STATS_CS_UPDATE",
	"STATS_SKIN_UPDATE",
	"STATS_IHIST_UPDATE",  /* 60 */
	"DUMMY_4",
	"EPOCH1_ACK",
	"EPOCH2_ACK",
	"START_RECORDING",
	"STOP_RECORDING",  /* 65 */
	"DUMMY_5",
	"DUMMY_6",
	"CAPTURE",
	"DUMMY_7",
	"STOP",  /* 70 */
	"GET_HW_VERSION",
	"GET_FRAME_SKIP_COUNTS",
	"OUTPUT1_BUFFER_ENQ",
	"OUTPUT2_BUFFER_ENQ",
	"OUTPUT3_BUFFER_ENQ",  /* 75 */
	"JPEG_OUT_BUF_ENQ",
	"RAW_OUT_BUF_ENQ",
	"RAW_IN_BUF_ENQ",
	"STATS_AF_ENQ",
	"STATS_AE_ENQ",  /* 80 */
	"STATS_AWB_ENQ",
	"STATS_RS_ENQ",
	"STATS_CS_ENQ",
	"STATS_SKIN_ENQ",
	"STATS_IHIST_ENQ",  /* 85 */
	"DUMMY_8",
	"JPEG_ENC_CFG",
	"DUMMY_9",
	"STATS_AF_START",
	"STATS_AF_STOP",  /* 90 */
	"STATS_AE_START",
	"STATS_AE_STOP",
	"STATS_AWB_START",
	"STATS_AWB_STOP",
	"STATS_RS_START",  /* 95 */
	"STATS_RS_STOP",
	"STATS_CS_START",
	"STATS_CS_STOP",
	"STATS_SKIN_START",
	"STATS_SKIN_STOP",  /* 100 */
	"STATS_IHIST_START",
	"STATS_IHIST_STOP",
	"DUMMY_10",
	"SYNC_TIMER_SETTING",
	"ASYNC_TIMER_SETTING",  /* 105 */
	"V31_LIVESHOT",
	"V31_ZSL",
	"V31_STEREOCAM",
	"V31_LA_SETUP",
	"V31_XBAR_CFG",
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
	{
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
		pinfo->p0_phy =
			((struct vfe_message *)data)->_u.msgOut.p0_addr;
		pinfo->p1_phy =
			((struct vfe_message *)data)->_u.msgOut.p1_addr;
		pinfo->p2_phy =
			((struct vfe_message *)data)->_u.msgOut.p2_addr;

		pinfo->frame_id =
		((struct vfe_message *)data)->_u.msgOut.frameCounter;

		((struct vfe_msg_output *)(vfe31_ctrl->extdata))->bpcInfo =
		((struct vfe_message *)data)->_u.msgOut.bpcInfo;
		((struct vfe_msg_output *)(vfe31_ctrl->extdata))->asfInfo =
		((struct vfe_message *)data)->_u.msgOut.asfInfo;
		((struct vfe_msg_output *)(vfe31_ctrl->extdata))->frameCounter =
		((struct vfe_message *)data)->_u.msgOut.frameCounter;
		*ext  = vfe31_ctrl->extdata;
		*elen = vfe31_ctrl->extlen;
	}
		break;

	default:
		break;
	} /* switch */
}


static void vfe31_proc_ops(enum VFE31_MESSAGE_ID id, void *msg, size_t len)
{
	struct msm_vfe_resp *rp;

	rp = vfe31_ctrl->resp->vfe_alloc(sizeof(struct msm_vfe_resp),
		vfe31_ctrl->syncdata, GFP_ATOMIC);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}
	CDBG("vfe31_proc_ops, msgId = %d\n", id);
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

	case MSG_ID_COMMON:
		rp->type = VFE_MSG_COMMON;
		rp->stats_msg.status_bits = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.status_bits;
		rp->stats_msg.frame_id = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.frameCounter;

		rp->stats_msg.aec_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.aec;
		rp->stats_msg.awb_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.awb;
		rp->stats_msg.af_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.af;
		rp->stats_msg.ihist_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.ihist;
		rp->stats_msg.rs_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.rs;
		rp->stats_msg.cs_buff = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.cs;
		rp->stats_msg.awb_ymin = ((struct vfe_message *)
			rp->evt_msg.data)->_u.msgStats.buff.awb_ymin;
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

	vfe31_ctrl->resp->vfe_resp(rp, MSM_CAM_Q_VFE_MSG, vfe31_ctrl->syncdata,
		GFP_ATOMIC);
}

static void vfe_send_outmsg(uint8_t msgid, uint32_t p0_addr,
	uint32_t p1_addr, uint32_t p2_addr)
{
	struct vfe_message msg;
	uint8_t outid;

	msg._d = msgid;   /* now the output mode is redundnat. */
	msg._u.msgOut.frameCounter = vfe31_ctrl->vfeFrameId;

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
	msg._u.msgOut.p0_addr     = p0_addr;
	msg._u.msgOut.p1_addr     = p1_addr;
	msg._u.msgOut.p2_addr     = p2_addr;
	CDBG("%s p2_addr = 0x%x\n", __func__, p2_addr);
	vfe31_proc_ops(msgid, &msg, sizeof(struct vfe_message));
	return;
}
static int vfe31_enable(struct camera_enable_cmd *enable)
{
	return 0;
}

static void vfe31_stop(void)
{
	atomic_set(&vfe31_ctrl->vstate, 0);
	atomic_set(&vfe31_ctrl->stop_ack_pending, 1);

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time. stop camif immediately. */
	msm_io_w_mb(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);

	/* disable all interrupts.  */
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1,
		vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	/* now enable only halt_irq & reset_irq */
	msm_io_w(0xf0000000,          /* this is for async timer. */
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_IMASK_AXI_HALT,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* then apply axi halt command. */
	msm_io_w_mb(AXI_HALT,
		vfe31_ctrl->vfebase + VFE_AXI_CMD);
}

static int vfe31_disable(struct camera_enable_cmd *enable,
	struct platform_device *dev)
{
	msm_camio_set_perf_lvl(S_EXIT);
	msm_camio_disable(dev);
	return 0;
}

static int vfe31_add_free_buf2(struct vfe31_output_ch *outch,
	uint32_t paddr, uint32_t p0_off, uint32_t p1_off, uint32_t p2_off)
{
	struct vfe31_free_buf *free_buf = NULL;
	unsigned long flags = 0;
	free_buf = kmalloc(sizeof(struct vfe31_free_buf), GFP_KERNEL);
	if (!free_buf)
		return -ENOMEM;

	spin_lock_irqsave(&outch->free_buf_lock, flags);
	free_buf->paddr = paddr;
	free_buf->planar0_off = p0_off;
	free_buf->planar1_off = p1_off;
	free_buf->planar2_off = p2_off;
	list_add_tail(&free_buf->node, &outch->free_buf_head);

	CDBG("%s: free_buf paddr = 0x%x, y_off = %d, cbcr_off = %d\n",
		__func__, free_buf->paddr, free_buf->planar0_off,
		free_buf->planar1_off);
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
	return 0;
}

#define vfe31_add_free_buf(outch, regptr) \
	vfe31_add_free_buf2(outch, regptr->paddr, \
	regptr->info.planar0_off,	\
	regptr->info.planar1_off,	\
	regptr->info.planar2_off)

#define vfe31_free_buf_available(outch) \
	(!list_empty(&outch.free_buf_head))

static inline struct vfe31_free_buf *vfe31_get_free_buf(
	struct vfe31_output_ch *outch)
{
	unsigned long flags = 0;
	struct vfe31_free_buf *free_buf = NULL;
	spin_lock_irqsave(&outch->free_buf_lock, flags);
	if (!list_empty(&outch->free_buf_head)) {
		free_buf = list_first_entry(&outch->free_buf_head,
			struct vfe31_free_buf, node);
		if (free_buf)
			list_del_init(&free_buf->node);
	}
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
	return free_buf;
}

static inline void vfe31_reset_free_buf_queue(
	struct vfe31_output_ch *outch)
{
	unsigned long flags = 0;
	struct vfe31_free_buf *free_buf = NULL;
	spin_lock_irqsave(&outch->free_buf_lock, flags);
	while (!list_empty(&outch->free_buf_head)) {
		free_buf = list_first_entry(&outch->free_buf_head,
			struct vfe31_free_buf, node);
		if (free_buf) {
			list_del_init(&free_buf->node);
			kfree(free_buf);
		}
	}
	spin_unlock_irqrestore(&outch->free_buf_lock, flags);
}

#define vfe31_init_free_buf_queue() do {	\
	INIT_LIST_HEAD(&vfe31_ctrl->outpath.out0.free_buf_head);	\
	INIT_LIST_HEAD(&vfe31_ctrl->outpath.out1.free_buf_head);	\
	INIT_LIST_HEAD(&vfe31_ctrl->outpath.out2.free_buf_head);	\
	spin_lock_init(&vfe31_ctrl->outpath.out0.free_buf_lock);	\
	spin_lock_init(&vfe31_ctrl->outpath.out1.free_buf_lock);	\
	spin_lock_init(&vfe31_ctrl->outpath.out2.free_buf_lock);	\
} while (0)

#define vfe31_reset_free_buf_queue_all() do {	\
	vfe31_reset_free_buf_queue(&vfe31_ctrl->outpath.out0);	\
	vfe31_reset_free_buf_queue(&vfe31_ctrl->outpath.out1);	\
	vfe31_reset_free_buf_queue(&vfe31_ctrl->outpath.out2);	\
} while (0)

static int vfe31_config_axi(int mode, struct axidata *ad, uint32_t *ao)
{
	int i;
	uint32_t *p, *p1, *p2, *p3;
	int32_t *ch_info;
	struct vfe31_output_ch *outp1, *outp2, *outp3;
	struct msm_pmem_region *regp1 = NULL;
	struct msm_pmem_region *regp2 = NULL;
	struct msm_pmem_region *regp3 = NULL;
	int ret;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;

	outp1 = NULL;
	outp2 = NULL;
	outp3 = NULL;

	p = ao + 2;

	/* Update the corresponding write masters for each output*/
	ch_info = ao + V31_AXI_CFG_LEN;
	vfe31_ctrl->outpath.out0.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out0.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out0.ch2 = 0x0000FFFF & *ch_info++;
	vfe31_ctrl->outpath.out1.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out1.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out1.ch2 = 0x0000FFFF & *ch_info++;
	vfe31_ctrl->outpath.out2.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out2.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out2.ch2 = 0x0000FFFF & *ch_info++;

	CDBG("vfe31_config_axi: mode = %d, bufnum1 = %d, bufnum2 = %d"
		"bufnum3 = %d", mode, ad->bufnum1, ad->bufnum2, ad->bufnum3);

	switch (mode) {

	case OUTPUT_2: {
		if (ad->bufnum2 != 3)
			return -EINVAL;
		regp1 = &(ad->region[ad->bufnum1]);
		outp1 = &(vfe31_ctrl->outpath.out0);
		vfe31_ctrl->outpath.output_mode |= VFE31_OUTPUT_MODE_PT;

		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.planar0_off);

			p1 = ao + 12 + i;  /* wm1 for cbcr */
			*p1 = (regp1->paddr + regp1->info.planar1_off);
			regp1++;
		}
		ret = vfe31_add_free_buf(outp1, regp1);
		if (ret < 0)
			return ret;
	}
		break;

	case OUTPUT_1_AND_2:
		/* use wm0& 4 for thumbnail, wm1&5 for main image.*/
		if ((ad->bufnum1 < 1) || (ad->bufnum2 < 1))
			return -EINVAL;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_S;  /* main image.*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_PT;  /* thumbnail. */

		/* this is thumbnail buffer. */
		regp1 = &(ad->region[ad->bufnum1-1]);
		/* this is main image buffer. */
		regp2 = &(ad->region[ad->bufnum1+ad->bufnum2-1]);

		outp1 = &(vfe31_ctrl->outpath.out0);
		outp2 = &(vfe31_ctrl->outpath.out1); /* snapshot */

		/*  Parse the buffers!!! */
		if (ad->bufnum2 == 1) {	/* assuming bufnum1 = bufnum2 */
			p1 = ao + 6;   /* wm0 ping */
			*p1++ = (regp1->paddr + regp1->info.planar0_off);

			/* this is to duplicate ping address to pong.*/
			*p1 = (regp1->paddr + regp1->info.planar0_off);

			p1 = ao + 30;  /* wm4 ping */
			*p1++ = (regp1->paddr + regp1->info.planar1_off);
			CDBG("%s: regp1->info.cbcr_off = 0x%x\n", __func__,
						 regp1->info.planar1_off);

			/* this is to duplicate ping address to pong.*/
			*p1 = (regp1->paddr + regp1->info.planar1_off);

			p1 = ao + 12;   /* wm1 ping */
			*p1++ = (regp2->paddr + regp2->info.planar0_off);

			/* pong = ping,*/
			*p1 = (regp2->paddr + regp2->info.planar0_off);

			p1 = ao + 36;  /* wm5 */
			*p1++ = (regp2->paddr + regp2->info.planar1_off);
			CDBG("%s: regp2->info.cbcr_off = 0x%x\n", __func__,
						 regp2->info.planar1_off);

			/* pong = ping,*/
			*p1 = (regp2->paddr + regp2->info.planar1_off);
		} else { /* more than one snapshot */
			/* first fill ping & pong */
			for (i = 0; i < 2; i++) {
				p1 = ao + 6 + i;    /* wm0 for y  */
				*p1 = (regp1->paddr + regp1->info.planar0_off);
				p1 = ao + 30 + i;  /* wm4 for cbcr */
				*p1 = (regp1->paddr + regp1->info.planar1_off);
				regp1--;
			}

			for (i = 0; i < 2; i++) {
				p2 = ao + 12 + i;    /* wm1 for y  */
				*p2 = (regp2->paddr + regp2->info.planar0_off);
				p2 = ao + 36 + i;  /* wm5 for cbcr */
				*p2 = (regp2->paddr + regp2->info.planar1_off);
				regp2--;
			}

			for (i = 2; i < ad->bufnum1; i++) {
				ret = vfe31_add_free_buf(outp1, regp1);
				if (ret < 0)
					return ret;
				regp1--;
			}

			for (i = 2; i < ad->bufnum2; i++) {
				ret = vfe31_add_free_buf(outp2, regp2);
				if (ret < 0)
					return ret;
				regp2--;
			}
		}
		break;

	case OUTPUT_1_2_AND_3:
		CDBG("%s: OUTPUT_1_2_AND_3", __func__);
		CDBG("%s: %d %d %d", __func__, ad->bufnum1, ad->bufnum2,
			ad->bufnum3);
		/* use wm0& 4 for postview, wm1&5 for preview.*/
		/* use wm2& 6 for main img */
		if ((ad->bufnum1 < 1) || (ad->bufnum2 < 1) || (ad->bufnum3 < 1))
			return -EINVAL;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_S;  /* main image.*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_P;  /* preview. */
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_T;  /* thumbnail. */

		/* this is preview buffer. */
		regp1 = &(ad->region[0]);
		/* this is thumbnail buffer. */
		regp2 = &(ad->region[ad->bufnum1]);
		/* this is main image buffer. */
		regp3 = &(ad->region[ad->bufnum1+ad->bufnum2]);
		outp1 = &(vfe31_ctrl->outpath.out0);
		outp2 = &(vfe31_ctrl->outpath.out1);
		outp3 = &(vfe31_ctrl->outpath.out2);

		/*  Parse the buffers!!! */
		/* first fill ping & pong */
		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.planar0_off);
			p1 = ao + 30 + i;  /* wm4 for cbcr */
			*p1 = (regp1->paddr + regp1->info.planar1_off);
			regp1++;
		}

		for (i = 0; i < 2; i++) {
			p2 = ao + 12 + i;    /* wm1 for y  */
			*p2 = (regp2->paddr + regp2->info.planar0_off);
			p2 = ao + 36 + i;  /* wm5 for cbcr */
			*p2 = (regp2->paddr + regp2->info.planar1_off);
			regp2++;
		}

		for (i = 0; i < 2; i++) {
			p3 = ao + 18 + i;    /* wm2 for y  */
			*p3 = (regp3->paddr + regp3->info.planar0_off);
			p3 = ao + 42 + i;  /* wm6 for cbcr */
			*p3 = (regp3->paddr + regp3->info.planar1_off);
			regp3++;
		}

		for (i = 2; i < ad->bufnum1; i++) {
			ret = vfe31_add_free_buf(outp1, regp1);
			if (ret < 0)
				return ret;
			regp1++;
		}

		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe31_add_free_buf(outp2, regp2);
			if (ret < 0)
				return ret;
			regp2++;
		}

		for (i = 2; i < ad->bufnum3; i++) {
			ret = vfe31_add_free_buf(outp3, regp3);
			if (ret < 0)
				return ret;
			regp3++;
		}
		break;

	case OUTPUT_ZSL_ALL_CHNLS:
		CDBG("%s: OUTPUT_ZSL_ALL_CHNLS", __func__);
		CDBG("%s: %d %d %d", __func__, ad->bufnum1, ad->bufnum2,
			ad->bufnum3);
		/* use wm0& 4 for postview, wm1&5 for preview.*/
		/* use wm2& 6 for main img */
		if ((ad->bufnum1 < 1) || (ad->bufnum2 < 1) || (ad->bufnum3 < 1))
			return -EINVAL;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_S;  /* main image.*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_P_ALL_CHNLS;  /* preview. */
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_T;  /* thumbnail. */

		/* this is preview buffer. */
		regp1 = &(ad->region[0]);
		/* this is thumbnail buffer. */
		regp2 = &(ad->region[ad->bufnum1]);
		/* this is main image buffer. */
		regp3 = &(ad->region[ad->bufnum1+ad->bufnum2]);
		outp1 = &(vfe31_ctrl->outpath.out0);
		outp2 = &(vfe31_ctrl->outpath.out1);
		outp3 = &(vfe31_ctrl->outpath.out2);

		/*  Parse the buffers!!! */
		/* first fill ping & pong */
		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp2->paddr + regp2->info.planar0_off);
			p1 = ao + 12 + i;  /* wm1 for cbcr */
			*p1 = (regp2->paddr + regp2->info.planar1_off);
			regp2++;
		}

		for (i = 0; i < 2; i++) {
			p2 = ao + 30 + i;    /* wm4 for y  */
			*p2 = (regp1->paddr + regp1->info.planar0_off);
			p2 = ao + 36 + i;  /* wm5 for cbcr */
			*p2 = (regp1->paddr + regp1->info.planar1_off);
			p2 = ao + 42 + i;  /* wm5 for cbcr */
			*p2 = (regp1->paddr + regp1->info.planar2_off);
			regp1++;
		}

		for (i = 0; i < 2; i++) {
			p3 = ao + 18 + i;    /* wm2 for y  */
			*p3 = (regp3->paddr + regp3->info.planar0_off);
			p3 = ao + 24 + i;  /* wm3 for cbcr */
			*p3 = (regp3->paddr + regp3->info.planar1_off);
			regp3++;
		}
		for (i = 2; i < ad->bufnum1; i++) {
			ret = vfe31_add_free_buf(outp1, regp1);
			if (ret < 0)
				return ret;
			regp1++;
		}

		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe31_add_free_buf(outp2, regp2);
			if (ret < 0)
				return ret;
			regp2++;
		}

		for (i = 2; i < ad->bufnum3; i++) {
			ret = vfe31_add_free_buf(outp3, regp3);
			if (ret < 0)
				return ret;
			regp3++;
		}
		break;

	case OUTPUT_1_AND_3: {
		/* use wm0&4 for preview, wm1&5 for video.*/
		if ((ad->bufnum1 < 2) || (ad->bufnum2 < 2))
			return -EINVAL;

#ifdef CONFIG_MSM_CAMERA_V4L2
		*p++ = 0x1;    /* xbar cfg0 */
		*p = 0x1a03;    /* xbar cfg1 */
#endif
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_V;  /* video*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_PT;  /* preview */

		regp1 = &(ad->region[0]); /* this is preview buffer. */
		regp2 = &(ad->region[ad->bufnum1]);/* this is video buffer. */
		outp1 = &(vfe31_ctrl->outpath.out0); /* preview */
		outp2 = &(vfe31_ctrl->outpath.out2); /* video */


		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.planar0_off);

			p1 = ao + 30 + i;  /* wm4 for cbcr */
			*p1 = (regp1->paddr + regp1->info.planar1_off);
			regp1++;
		}

		for (i = 0; i < 2; i++) {
			p2 = ao + 12 + i;    /* wm1 for y  */
			*p2 = (regp2->paddr + regp2->info.planar0_off);

			p2 = ao + 36 + i;  /* wm5 for cbcr */
			*p2 = (regp2->paddr + regp2->info.planar1_off);
			regp2++;
		}
		for (i = 2; i < ad->bufnum1; i++) {
			ret = vfe31_add_free_buf(outp1, regp1);
			if (ret < 0)
				return ret;
			regp1++;
		}

		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe31_add_free_buf(outp2, regp2);
			if (ret < 0)
				return ret;
			regp2++;
		}
	}
		break;

	case OUTPUT_VIDEO_ALL_CHNLS: {
		/* use wm0&4 for preview, wm1&5 for video.*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_V;  /* video*/
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_P_ALL_CHNLS;
		regp1 = &(ad->region[0]);
		regp2 = &(ad->region[ad->bufnum1]);
		outp1 = &(vfe31_ctrl->outpath.out0);
		outp2 = &(vfe31_ctrl->outpath.out2);

		for (i = 0; i < 2; i++) {
			p1 = ao + 6 + i;    /* wm0 for y  */
			*p1 = (regp1->paddr + regp1->info.planar0_off);

			p1 = ao + 12 + i;  /* wm1 for cbcr */
			*p1 = (regp1->paddr + regp1->info.planar1_off);

			p1 = ao + 18 + i;  /* wm2 for cbcr */
			*p1 = (regp1->paddr + regp1->info.planar2_off);
			regp1++;
		}
		for (i = 0; i < 2; i++) {
			p2 = ao + 30 + i;    /* wm4 for y  */
			*p2 = (regp2->paddr + regp2->info.planar0_off);

			p2 = ao + 36 + i;  /* wm5 for cbcr */
			*p2 = (regp2->paddr + regp2->info.planar1_off);
			regp2++;
		}
		for (i = 2; i < ad->bufnum1; i++) {
			ret = vfe31_add_free_buf(outp1, regp1);
			if (ret < 0)
				return ret;
			regp1++;
		}
		for (i = 2; i < ad->bufnum2; i++) {
			ret = vfe31_add_free_buf(outp2, regp2);
			if (ret < 0)
				return ret;
			regp2++;
		}
	}
		break;

	case CAMIF_TO_AXI_VIA_OUTPUT_2: {  /* use wm0 only */
		if (ad->bufnum2 < 1)
			return -EINVAL;
		CDBG("config axi for raw snapshot.\n");
		vfe31_ctrl->outpath.out1.ch0 = 0; /* raw */
		regp1 = &(ad->region[ad->bufnum1]);
		vfe31_ctrl->outpath.output_mode |= VFE31_OUTPUT_MODE_S;
		p1 = ao + 6;    /* wm0 for y  */
		*p1 = (regp1->paddr + regp1->info.planar0_off);
		if (p_sync->stereocam_enabled)
			p_sync->stereo_state = STEREO_RAW_SNAP_IDLE;
	}
		break;
	default:
		break;
	}
	msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[V31_AXI_OUT_CFG].offset,
		ao, vfe31_cmd[V31_AXI_OUT_CFG].length - V31_AXI_CH_INF_LEN);

	return 0;
}

static void vfe31_reset_internal_variables(void)
{
	unsigned long flags;
	vfe31_ctrl->vfeImaskCompositePacked = 0;
	/* state control variables */
	vfe31_ctrl->start_ack_pending = FALSE;
	atomic_set(&irq_cnt, 0);

	spin_lock_irqsave(&vfe31_ctrl->xbar_lock, flags);
	vfe31_ctrl->xbar_update_pending = 0;
	spin_unlock_irqrestore(&vfe31_ctrl->xbar_lock, flags);

	atomic_set(&vfe31_ctrl->stop_ack_pending, 0);
	atomic_set(&vfe31_ctrl->vstate, 0);

	vfe31_ctrl->aec_ack_pending = FALSE;
	vfe31_ctrl->af_ack_pending = FALSE;
	vfe31_ctrl->awb_ack_pending = FALSE;
	vfe31_ctrl->ihist_ack_pending = FALSE;
	vfe31_ctrl->rs_ack_pending = FALSE;
	vfe31_ctrl->cs_ack_pending = FALSE;

	vfe31_ctrl->reset_ack_pending  = FALSE;

	spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
	vfe31_ctrl->update_ack_pending = FALSE;
	spin_unlock_irqrestore(&vfe31_ctrl->update_ack_lock, flags);

	vfe31_ctrl->recording_state = VFE_REC_STATE_IDLE;

	/* 0 for continuous mode, 1 for snapshot mode */
	vfe31_ctrl->operation_mode = VFE_MODE_OF_OPERATION_CONTINUOUS;
	vfe31_ctrl->outpath.output_mode = 0;
	vfe31_ctrl->vfe_capture_count = 0;

	/* this is unsigned 32 bit integer. */
	vfe31_ctrl->vfeFrameId = 0;

	vfe31_ctrl->output1Pattern = 0xffffffff;
	vfe31_ctrl->output1Period  = 31;
	vfe31_ctrl->output2Pattern = 0xffffffff;
	vfe31_ctrl->output2Period  = 31;
	vfe31_ctrl->vfeFrameSkipCount   = 0;
	vfe31_ctrl->vfeFrameSkipPeriod  = 31;

	/* Stats control variables. */
	memset(&(vfe31_ctrl->afStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe31_ctrl->awbStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe31_ctrl->aecStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe31_ctrl->ihistStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe31_ctrl->rsStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe31_ctrl->csStatsControl), 0,
		sizeof(struct vfe_stats_control));
}

static void vfe31_reset(void)
{
	uint32_t vfe_version;
	vfe31_reset_free_buf_queue_all();
	vfe31_reset_internal_variables();

	vfe31_reset_hist_cfg();
	vfe_version = msm_io_r(vfe31_ctrl->vfebase);
	CDBG("vfe_version = 0x%x\n", vfe_version);
	/* disable all interrupts.  vfeImaskLocal is also reset to 0
	* to begin with. */
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_io_w(VFE_CLEAR_ALL_IRQS, vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(VFE_CLEAR_ALL_IRQS, vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	/* enable reset_ack interrupt.  */
	msm_io_w(VFE_IMASK_RESET,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Write to VFE_GLOBAL_RESET_CMD to reset the vfe hardware. Once reset
	 * is done, hardware interrupt will be generated.  VFE ist processes
	 * the interrupt to complete the function call.  Note that the reset
	 * function is synchronous. */

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(VFE_RESET_UPON_RESET_CMD,
		vfe31_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static int vfe31_operation_config(uint32_t *cmd)
{
	uint32_t *p = cmd;

	vfe31_ctrl->operation_mode = *p;
	vpe_ctrl->pad_2k_bool = (vfe31_ctrl->operation_mode & 1) ?
		FALSE : TRUE;

	vfe31_ctrl->stats_comp = *(++p);
	vfe31_ctrl->hfr_mode = *(++p);

	msm_io_w(*(++p), vfe31_ctrl->vfebase + VFE_CFG_OFF);
	msm_io_w(*(++p), vfe31_ctrl->vfebase + VFE_MODULE_CFG);
	msm_io_w(*(++p), vfe31_ctrl->vfebase + VFE_REALIGN_BUF);
	msm_io_w(*(++p), vfe31_ctrl->vfebase + VFE_CHROMA_UP);
	msm_io_w(*(++p), vfe31_ctrl->vfebase + VFE_STATS_CFG);
	wmb();
	return 0;
}
static uint32_t vfe_stats_awb_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	vfe31_ctrl->awbStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}


static uint32_t vfe_stats_aec_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PONG_ADDR);

	vfe31_ctrl->aecStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_af_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);

	vfe31_ctrl->afStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_ihist_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PONG_ADDR);

	vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_rs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PONG_ADDR);

	vfe31_ctrl->rsStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}
static uint32_t vfe_stats_cs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PING_ADDR);
	addr = ptr[1];
	msm_io_w(addr, vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PONG_ADDR);

	vfe31_ctrl->csStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static void vfe31_start_common(void)
{
	uint32_t irq_mask = 0x00E00021;
	vfe31_ctrl->start_ack_pending = TRUE;
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe31_ctrl->operation_mode, vfe31_ctrl->outpath.output_mode);
	/* Enable IRQ for comp stats, Image master, SOF & Reg Update*/
	if (vfe31_ctrl->stats_comp)
		irq_mask |= 0x01000000;
	else /* Enable IRQ for Image masters, AF stats, SOF & Reg Update */
		irq_mask |= 0x00004000;

	/* Enable EOF for video mode */
	if (VFE_MODE_OF_OPERATION_VIDEO == vfe31_ctrl->operation_mode)
		irq_mask |= 0x4;

	msm_io_w(irq_mask, vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_io_w(VFE_IMASK_RESET,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);
	/* enable out of order option */
	msm_io_w(0x80000000, vfe31_ctrl->vfebase + VFE_AXI_CFG);
	/* enable performance monitor */
	msm_io_w(1, vfe31_ctrl->vfebase + VFE_BUS_PM_CFG);
	msm_io_w(1, vfe31_ctrl->vfebase + VFE_BUS_PM_CMD);


	msm_io_dump(vfe31_ctrl->vfebase, 0x600);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_io_w(1, vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);
	wmb();

	atomic_set(&vfe31_ctrl->vstate, 1);
}

static int vfe31_start_recording(void)
{
	msm_camio_set_perf_lvl(S_VIDEO);
	usleep(1000);
	vfe31_ctrl->recording_state = VFE_REC_STATE_START_REQUESTED;
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe31_stop_recording(void)
{
	vfe31_ctrl->recording_state = VFE_REC_STATE_STOP_REQUESTED;
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camio_set_perf_lvl(S_PREVIEW);
	return 0;
}

static void vfe31_liveshot(void)
{
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync)
		p_sync->liveshot_enabled = true;
}

static void vfe31_stereocam(uint32_t enable)
{
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync) {
		CDBG("%s: Enable StereoCam %d!!!\n", __func__, enable);
		p_sync->stereocam_enabled = enable;
	}
}

static int vfe31_zsl(void)
{
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	irq_comp_mask	=
		msm_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	CDBG("%s:op mode %d O/P Mode %d\n", __func__,
		vfe31_ctrl->operation_mode, vfe31_ctrl->outpath.output_mode);
	if ((vfe31_ctrl->operation_mode == VFE_MODE_OF_OPERATION_ZSL)) {
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_P) {
			irq_comp_mask |=
				((0x1 << (vfe31_ctrl->outpath.out0.ch0)) |
				(0x1 << (vfe31_ctrl->outpath.out0.ch1)));
		} else if (vfe31_ctrl->outpath.output_mode &
				VFE31_OUTPUT_MODE_P_ALL_CHNLS) {
			irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
				0x1 << vfe31_ctrl->outpath.out0.ch1 |
				0x1 << vfe31_ctrl->outpath.out0.ch2);
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_T) {
			irq_comp_mask |=
				((0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8)) |
				(0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8)));
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_S) {
			irq_comp_mask |=
			((0x1 << (vfe31_ctrl->outpath.out2.ch0 + 8)) |
			(0x1 << (vfe31_ctrl->outpath.out2.ch1 + 8)));
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_P) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		} else if (vfe31_ctrl->outpath.output_mode &
				VFE31_OUTPUT_MODE_P_ALL_CHNLS) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch2]);
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_T) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_S) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch1]);
		}
	}
	msm_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe31_start_common();
	msm_camio_set_perf_lvl(S_ZSL);
	usleep(1000);
	/* for debug */
	msm_io_w(1, vfe31_ctrl->vfebase + 0x18C);
	msm_io_w(1, vfe31_ctrl->vfebase + 0x188);
	return 0;
}

static int vfe31_capture(uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;

	/* capture command is valid for both idle and active state. */
	vfe31_ctrl->vfe_capture_count = num_frames_capture;
	if (p_sync) {
		p_sync->snap_count = num_frames_capture;
		p_sync->thumb_count = num_frames_capture;
	}

	irq_comp_mask	=
		msm_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if ((vfe31_ctrl->operation_mode ==
		 VFE_MODE_OF_OPERATION_SNAPSHOT) ||
		(vfe31_ctrl->operation_mode ==
		 VFE_MODE_OF_OPERATION_ZSL)){
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PT) {
			irq_comp_mask |=
				((0x1 << (vfe31_ctrl->outpath.out0.ch0 + 8)) |
				(0x1 << (vfe31_ctrl->outpath.out0.ch1 + 8)));
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_S) {
			irq_comp_mask |=
			((0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8)) |
			(0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8)));
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PT) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		}
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_S) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		}
	} else {  /* this is raw snapshot mode. */
		CDBG("config the comp imask for raw snapshot mode.\n");
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_S) {
			irq_comp_mask |=
			(0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8));
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
		}
	}
	msm_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	if (p_sync->stereocam_enabled)
		msm_camio_set_perf_lvl(S_STEREO_CAPTURE);
	else
		msm_camio_set_perf_lvl(S_CAPTURE);

	usleep(1000);
	vfe31_start_common();
	return 0;
}

static int vfe31_start(void)
{
	uint32_t irq_comp_mask = 0;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	/* start command now is only good for continuous mode. */
	if ((vfe31_ctrl->operation_mode != VFE_MODE_OF_OPERATION_CONTINUOUS) &&
		(vfe31_ctrl->operation_mode != VFE_MODE_OF_OPERATION_VIDEO))
		return 0;
	irq_comp_mask =
		msm_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PT) {
		irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
			0x1 << vfe31_ctrl->outpath.out0.ch1);
			if (vfe31_ctrl->outpath.out0.ch2 >= 0)
				irq_comp_mask |=
					(0x1 << vfe31_ctrl->outpath.out0.ch0 |
					0x1 << vfe31_ctrl->outpath.out0.ch1 |
					0x1 << vfe31_ctrl->outpath.out0.ch2);
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_P_ALL_CHNLS) {
			irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
				0x1 << vfe31_ctrl->outpath.out0.ch1 |
				0x1 << vfe31_ctrl->outpath.out0.ch2);
	}

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_V) {
		irq_comp_mask |= (0x1 << (vfe31_ctrl->outpath.out2.ch0 + 16)|
			0x1 << (vfe31_ctrl->outpath.out2.ch1 + 16));
	}

	msm_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);


	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PT) {
		msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
		msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		if (vfe31_ctrl->outpath.out0.ch2 >= 0)
			msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch2]);
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_P_ALL_CHNLS) {
		msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
		msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		msm_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch2]);
	}

	if (p_sync->stereocam_enabled)
		msm_camio_set_perf_lvl(S_STEREO_VIDEO);
	else
		msm_camio_set_perf_lvl(S_PREVIEW);

	usleep(1000);
	vfe31_start_common();
	return 0;
}

static void vfe31_update(void)
{
	unsigned long flags;
	CDBG("vfe31_update\n");

	if (vfe31_ctrl->update_gamma) {
		if (!msm_io_r(vfe31_ctrl->vfebase + V31_GAMMA_CFG_OFF))
			msm_io_w(7, vfe31_ctrl->vfebase+V31_GAMMA_CFG_OFF);
		else
			msm_io_w(0, vfe31_ctrl->vfebase+V31_GAMMA_CFG_OFF);
		vfe31_ctrl->update_gamma = false;
	}
	if (vfe31_ctrl->update_luma) {
		if (!msm_io_r(vfe31_ctrl->vfebase + V31_LUMA_CFG_OFF))
			msm_io_w(1, vfe31_ctrl->vfebase + V31_LUMA_CFG_OFF);
		else
			msm_io_w(0, vfe31_ctrl->vfebase + V31_LUMA_CFG_OFF);
		vfe31_ctrl->update_luma = false;
	}
	spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
	vfe31_ctrl->update_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe31_ctrl->update_ack_lock, flags);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return;
}

static void vfe31_sync_timer_stop(void)
{
	uint32_t value = 0;
	vfe31_ctrl->sync_timer_state = 0;
	if (vfe31_ctrl->sync_timer_number == 0)
		value = 0x10000;
	else if (vfe31_ctrl->sync_timer_number == 1)
		value = 0x20000;
	else if (vfe31_ctrl->sync_timer_number == 2)
		value = 0x40000;

	/* Timer Stop */
	msm_io_w_mb(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF);
}

static void vfe31_sync_timer_start(const uint32_t *tbl)
{
	/* set bit 8 for auto increment. */
	uint32_t value = 1;
	uint32_t val;

	vfe31_ctrl->sync_timer_state = *tbl++;
	vfe31_ctrl->sync_timer_repeat_count = *tbl++;
	vfe31_ctrl->sync_timer_number = *tbl++;
	CDBG("%s timer_state %d, repeat_cnt %d timer number %d\n",
		 __func__, vfe31_ctrl->sync_timer_state,
		 vfe31_ctrl->sync_timer_repeat_count,
		 vfe31_ctrl->sync_timer_number);

	if (vfe31_ctrl->sync_timer_state) { /* Start Timer */
		value = value << vfe31_ctrl->sync_timer_number;
	} else { /* Stop Timer */
		CDBG("Failed to Start timer\n");
		 return;
	}

	/* Timer Start */
	msm_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF);
	/* Sync Timer Line Start */
	value = *tbl++;
	msm_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
		4 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Start */
	value = *tbl++;
	msm_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
			 8 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Duration */
	value = *tbl++;
	val = camio_clk.vfe_clk_rate / 10000;
	val = 10000000 / val;
	val = value * 10000 / val;
	CDBG("%s: Pixel Clk Cycles!!! %d \n", __func__, val);
	msm_io_w(val, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
		12 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Timer0 Active High/LOW */
	value = *tbl++;
	msm_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_POLARITY_OFF);
	/* Selects sync timer 0 output to drive onto timer1 port */
	value = 0;
	msm_io_w(value, vfe31_ctrl->vfebase + V31_TIMER_SELECT_OFF);
	wmb();
}

static void vfe31_program_dmi_cfg(enum VFE31_DMI_RAM_SEL bankSel)
{
	/* set bit 8 for auto increment. */
	uint32_t value = VFE_DMI_CFG_DEFAULT;
	value += (uint32_t)bankSel;

	msm_io_w_mb(value, vfe31_ctrl->vfebase + VFE_DMI_CFG);
	/* by default, always starts with offset 0.*/
	msm_io_w(0, vfe31_ctrl->vfebase + VFE_DMI_ADDR);
	wmb();
}
static void vfe31_write_gamma_cfg(enum VFE31_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	int i;
	uint32_t value, value1, value2;
	vfe31_program_dmi_cfg(channel_sel);
	/* for loop for extracting init table. */
	for (i = 0 ; i < (VFE31_GAMMA_NUM_ENTRIES/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_io_w((value1), vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_io_w((value2), vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe31_reset_hist_cfg()
{
	uint32_t i;
	uint32_t value = 0;

	vfe31_program_dmi_cfg(STATS_HIST_RAM);
	for (i = 0 ; i < VFE31_HIST_TABLE_LENGTH ; i++)
		msm_io_w(value, vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe31_write_la_cfg(enum VFE31_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	uint32_t i;
	uint32_t value, value1, value2;

	vfe31_program_dmi_cfg(channel_sel);
	/* for loop for extracting init table. */
	for (i = 0 ; i < (VFE31_LA_TABLE_LENGTH/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_io_w((value1), vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_io_w((value2), vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static int vfe31_proc_general(struct msm_vfe31_cmd *cmd)
{
	int i , rc = 0;
	uint32_t old_val = 0 , new_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;
	uint32_t stereo_cam_enable = 0;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;

	CDBG("vfe31_proc_general: cmdID = %s, length = %d\n",
		vfe31_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case V31_RESET:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		vfe31_reset();
		break;
	case V31_START:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		rc = vfe31_start();
		break;
	case V31_UPDATE:
		vfe31_update();
		break;
	case V31_ZSL:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		vfe31_zsl();
		break;
	case V31_CAPTURE:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe31_capture(snapshot_cnt);
		break;
	case V31_START_RECORDING:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		rc = vfe31_start_recording();
		if (p_sync->stereocam_enabled)
			p_sync->stereo_state = STEREO_VIDEO_ACTIVE;
		break;
	case V31_STOP_RECORDING:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		rc = vfe31_stop_recording();
		if (p_sync->stereocam_enabled)
			p_sync->stereo_state = STEREO_VIDEO_IDLE;
		break;
	case V31_OPERATION_CFG: {
		if (cmd->length != V31_OPERATION_CFG_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V31_OPERATION_CFG_LEN, GFP_ATOMIC);
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			V31_OPERATION_CFG_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe31_operation_config(cmdp);
		}
		break;

	case V31_STATS_AE_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AE_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;
	case V31_STATS_AF_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AF_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;
	case V31_STATS_AWB_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AWB_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
				cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;

	case V31_STATS_IHIST_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= IHIST_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
				cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;

	case V31_XBAR_CFG: {
		unsigned long flags = 0;
		spin_lock_irqsave(&vfe31_ctrl->xbar_lock, flags);
		if ((cmd->length != V31_XBAR_CFG_LEN)
			|| vfe31_ctrl->xbar_update_pending) {
			rc = -EINVAL;
			spin_unlock_irqrestore(&vfe31_ctrl->xbar_lock, flags);
			goto proc_general_done;
		}
		spin_unlock_irqrestore(&vfe31_ctrl->xbar_lock, flags);
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
		spin_lock_irqsave(&vfe31_ctrl->xbar_lock, flags);
		vfe31_ctrl->xbar_cfg[0] = *cmdp;
		vfe31_ctrl->xbar_cfg[1] = *(cmdp+1);
		vfe31_ctrl->xbar_update_pending = 1;
		spin_unlock_irqrestore(&vfe31_ctrl->xbar_lock, flags);
		CDBG("%s: xbar0 0x%x xbar1 0x%x", __func__,
			vfe31_ctrl->xbar_cfg[0],
			vfe31_ctrl->xbar_cfg[1]);
		}
		break;

	case V31_STATS_RS_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= RS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		*/
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
				cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;

	case V31_STATS_CS_START: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= CS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		*/
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
				cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;

	case V31_MCE_UPDATE:
	case V31_MCE_CFG:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		/* Incrementing with 4 so as to point to the 2nd Register as
		the 2nd register has the mce_enable bit */
		old_val = msm_io_r(vfe31_ctrl->vfebase +
						V31_CHROMA_SUP_OFF + 4);
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
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 4,
					&new_val, 4);
		cmdp_local += 1;

		old_val = msm_io_r(vfe31_ctrl->vfebase +
						V31_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 8,
		&new_val, 4);
		cmdp_local += 1;
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp_local, (vfe31_cmd[cmd->id].length));
		}
		break;
	case V31_DEMOSAIC_2_UPDATE: /* 38 BPC update   */
	case V31_DEMOSAIC_2_CFG: {  /* 14 BPC config   */
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

		old_val = msm_io_r(vfe31_ctrl->vfebase + V31_DEMOSAIC_0_OFF);
		old_val &= BPC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAIC_0_OFF,
					cmdp_local, 4);
		cmdp_local += 1;
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp_local, (vfe31_cmd[cmd->id].length));
		}
		break;
	case V31_DEMOSAIC_1_UPDATE:/* 37 ABF update  */
	case V31_DEMOSAIC_1_CFG: { /* 13 ABF config  */
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

		old_val = msm_io_r(vfe31_ctrl->vfebase + V31_DEMOSAIC_0_OFF);
		old_val &= ABF_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAIC_0_OFF,
		    cmdp_local, 4);

		cmdp_local += 1;
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp_local, (vfe31_cmd[cmd->id].length));
		}
		break;
	case V31_ROLL_OFF_CFG: {
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
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp_local, 16);
		cmdp_local += 4;
		vfe31_program_dmi_cfg(ROLLOFF_RAM);
		/* for loop for extrcting init table. */
		for (i = 0 ; i < (VFE31_ROLL_OFF_INIT_TABLE_SIZE * 2) ; i++) {
			msm_io_w(*cmdp_local ,
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		CDBG("done writing init table \n");
		/* by default, always starts with offset 0. */
		msm_io_w(LENS_ROLL_OFF_DELTA_TABLE_OFFSET,
		vfe31_ctrl->vfebase + VFE_DMI_ADDR);
		/* for loop for extracting delta table. */
		for (i = 0 ; i < (VFE31_ROLL_OFF_DELTA_TABLE_SIZE * 2) ; i++) {
			msm_io_w(*cmdp_local,
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		vfe31_program_dmi_cfg(NO_MEM_SELECTED);
		}
		break;

	case V31_LA_CFG:{
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
		/* Select Bank 0*/
		*cmdp = 0;
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
				cmdp, (vfe31_cmd[cmd->id].length));
		cmdp += 1;
		vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0 , cmdp);
		cmdp -= 1;
		}
		break;

	case V31_LA_UPDATE: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + V31_LUMA_CFG_OFF);
		cmdp += 1;
		if (old_val != 0x0)
			vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0 , cmdp);
		else
			vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK1 , cmdp);
		vfe31_ctrl->update_luma = true;
		cmdp -= 1;
		}
		break;

	case V31_SK_ENHAN_CFG:
	case V31_SK_ENHAN_UPDATE:{
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
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_SCE_OFF,
				cmdp, V31_SCE_LEN);
		}
		break;

	case V31_LIVESHOT:
		vfe31_liveshot();
		break;

	case V31_STEREOCAM:
		if (copy_from_user(&stereo_cam_enable,
			(void __user *)(cmd->value), sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		vfe31_stereocam(stereo_cam_enable);
		break;

	case V31_RGB_G_CFG: {
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
		/* Select Bank 0*/
		*cmdp = 0;
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_RGB_G_OFF,
				cmdp, 4);
		cmdp += 1;
		vfe31_write_gamma_cfg(RGBLUT_CHX_BANK0, cmdp);
		cmdp -= 1;
		}
		break;

	case V31_RGB_G_UPDATE: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + V31_GAMMA_CFG_OFF);
		cmdp += 1;

		if (!old_val) {
			vfe31_write_gamma_cfg(RGBLUT_CHX_BANK1, cmdp);
		} else {
			vfe31_write_gamma_cfg(RGBLUT_CHX_BANK0, cmdp);
			}
		vfe31_ctrl->update_gamma = true;
		cmdp -= 1;
		}
		break;

	case V31_STATS_AWB_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AWB_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V31_STATS_AE_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AE_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V31_STATS_AF_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AF_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V31_STATS_IHIST_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~IHIST_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V31_STATS_RS_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~RS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case V31_STATS_CS_STOP: {
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~CS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case V31_STOP:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		vfe31_stop();
		break;

	case V31_SYNC_TIMER_SETTING:
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
		vfe31_sync_timer_start(cmdp);
		break;

	case V31_EZTUNE_CFG: {
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
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= STATS_ENABLE_MASK;
		*cmdp |= old_val;

		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		}
		break;

	default: {
		if (cmd->length != vfe31_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe31_cmd[cmd->id].length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}

		CHECKED_COPY_FROM_USER(cmdp);
		msm_io_memcpy(vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
	}
	break;

	}

proc_general_done:
	kfree(cmdp);

	return rc;
}

static void vfe31_stats_af_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->afStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->af_ack_pending = FALSE;
}

static void vfe31_stats_awb_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->awbStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->awb_ack_pending = FALSE;
}

static void vfe31_stats_aec_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->aecStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->aec_ack_pending = FALSE;
}

static void vfe31_stats_ihist_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->ihist_ack_pending = FALSE;
}

static void vfe31_stats_rs_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->rsStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->rs_ack_pending = FALSE;
}

static void vfe31_stats_cs_ack(struct vfe_cmd_stats_ack *pAck)
{
	vfe31_ctrl->csStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->cs_ack_pending = FALSE;
}

static int vfe31_config(struct msm_vfe_cfg_cmd *cmd, void *data)
{
	struct msm_vfe31_cmd vfecmd;

	long rc = 0;
	uint32_t i = 0;
	struct vfe_cmd_stats_buf *scfg = NULL;
	struct msm_pmem_region   *regptr = NULL;
	struct vfe_cmd_stats_ack *sack = NULL;

	if (cmd->cmd_type != CMD_FRAME_BUF_RELEASE &&
		cmd->cmd_type != CMD_SNAP_BUF_RELEASE &&
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
		if (cmd->cmd_type != CMD_FRAME_BUF_RELEASE &&
			cmd->cmd_type != CMD_SNAP_BUF_RELEASE) {
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

	if ((cmd->cmd_type == CMD_STATS_AF_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_AWB_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_IHIST_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_RS_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_CS_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_AEC_ENABLE)) {
		struct axidata *axid;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			goto vfe31_config_done;
		}

		scfg =
			kmalloc(sizeof(struct vfe_cmd_stats_buf),
				GFP_ATOMIC);
		if (!scfg) {
			rc = -ENOMEM;
			goto vfe31_config_done;
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
		rc = vfe31_proc_general(&vfecmd);
		break;

	case CMD_FRAME_BUF_RELEASE: {
		struct msm_frame *b;
		unsigned long p;
		int ret;
		struct vfe31_output_ch *outch = NULL;
		if (!data) {
			rc = -EFAULT;
			break;
		}

		b = (struct msm_frame *)(cmd->value);
		p = *(unsigned long *)data;

		CDBG("CMD_FRAME_BUF_RELEASE b->path = %d\n", b->path);

		if (b->path & OUTPUT_TYPE_P) {
			CDBG("CMD_FRAME_BUF_RELEASE got free buffer\n");
			outch = &vfe31_ctrl->outpath.out0;
		} else if (b->path & OUTPUT_TYPE_S) {
			outch = &vfe31_ctrl->outpath.out1;
		} else if (b->path & OUTPUT_TYPE_V) {
			outch = &vfe31_ctrl->outpath.out2;
		} else {
			rc = -EFAULT;
			break;
		}

		ret = vfe31_add_free_buf2(outch, p, b->planar0_off,
			b->planar1_off, b->planar2_off);
		if (ret < 0)
			return ret;
		break;
	}

	case CMD_SNAP_BUF_RELEASE: {
		struct msm_frame *b;
		unsigned long p;
		int ret;
		struct vfe31_output_ch *outch = NULL;
		if (!data)
			return -EFAULT;

		b = (struct msm_frame *)(cmd->value);
		p = *(unsigned long *)data;

		CDBG("CMD_PIC_BUF_RELEASE b->path = %d\n", b->path);

		if (b->path & OUTPUT_TYPE_T) {
			CDBG("CMD_FRAME_BUF_RELEASE got free buffer\n");
			outch = &vfe31_ctrl->outpath.out1;
		} else if (b->path & OUTPUT_TYPE_S) {
			outch = &vfe31_ctrl->outpath.out2;
		} else
			return -EFAULT;

		ret = vfe31_add_free_buf2(outch, p, b->planar0_off,
			b->planar1_off,	b->planar2_off);
		if (ret < 0)
			return ret;
		break;
	}

	case CMD_STATS_AEC_BUF_RELEASE:
		vfe31_stats_aec_ack(sack);
		break;

	case CMD_STATS_AF_BUF_RELEASE:
		vfe31_stats_af_ack(sack);
		break;

	case CMD_STATS_AWB_BUF_RELEASE:
		vfe31_stats_awb_ack(sack);
		break;

	case CMD_STATS_IHIST_BUF_RELEASE:
		vfe31_stats_ihist_ack(sack);
		break;

	case CMD_STATS_RS_BUF_RELEASE:
		vfe31_stats_rs_ack(sack);
		break;

	case CMD_STATS_CS_BUF_RELEASE:
		vfe31_stats_cs_ack(sack);
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
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_2, axid, axio);
		kfree(axio);
		break;
	}

	case CMD_RAW_PICT_AXI_CFG: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			break;
		}
		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(CAMIF_TO_AXI_VIA_OUTPUT_2, axid, axio);
		kfree(axio);
		break;
	}

	case CMD_AXI_CFG_SNAP: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		CDBG("%s, CMD_AXI_CFG_SNAP\n", __func__);
		axid = data;
		if (!axid)
			return -EFAULT;
		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_1_AND_2, axid, axio);
		kfree(axio);
		break;
	}

	case CMD_AXI_CFG_ZSL: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		CDBG("%s, CMD_AXI_CFG_ZSL\n", __func__);
		axid = data;
		if (!axid)
			return -EFAULT;
		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_1_2_AND_3, axid, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_ZSL_ALL_CHNLS: {
		struct axidata *axid;
		uint32_t *axio;
		CDBG("%s, CMD_AXI_CFG_ZSL\n", __func__);
		axid = data;
		if (!axid)
			return -EFAULT;
		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_ZSL_ALL_CHNLS, axid, axio);
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

		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_1_AND_3, axid, axio);
		kfree(axio);
		break;
	}

	case CMD_AXI_CFG_VIDEO_ALL_CHNLS: {
		struct axidata *axid;
		uint32_t *axio = NULL;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			break;
		}

		axio =
			kmalloc(vfe31_cmd[V31_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[V31_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_VIDEO_ALL_CHNLS, axid, axio);
		kfree(axio);
		break;
	}

	default:
		break;
	}
vfe31_config_done:
	kfree(scfg);
	kfree(sack);
	CDBG("%s done: rc = %d\n", __func__, (int) rc);
	return rc;
}

static void vfe31_send_msg_no_payload(enum VFE31_MESSAGE_ID id)
{
	struct vfe_message msg;

	CDBG("vfe31_send_msg_no_payload\n");
	msg._d = id;
	vfe31_proc_ops(id, &msg, 0);
}

static void vfe31_process_reg_update_irq(void)
{
	uint32_t  temp, old_val;
	unsigned long flags;
	if (vfe31_ctrl->recording_state == VFE_REC_STATE_START_REQUESTED) {
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_V) {
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch0]);
			msm_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch1]);
		}
		vfe31_ctrl->recording_state = VFE_REC_STATE_STARTED;
		if (vpe_ctrl->dis_en) {
			old_val = msm_io_r(
				vfe31_ctrl->vfebase + VFE_MODULE_CFG);
			old_val |= RS_CS_ENABLE_MASK;
			msm_io_w(old_val,
				vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		}
		msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
	} else if (vfe31_ctrl->recording_state
			== VFE_REC_STATE_STOP_REQUESTED) {
		if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_V) {
			msm_io_w(0, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch0]);
			msm_io_w(0, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out2.ch1]);
		}

		/*disable rs& cs when stop recording. */
		old_val = msm_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= (~RS_CS_ENABLE_MASK);
		msm_io_w(old_val,
				vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		CDBG("stop video triggered\n");
	}
	if (vfe31_ctrl->start_ack_pending == TRUE) {
		vfe31_send_msg_no_payload(MSG_ID_START_ACK);
		vfe31_ctrl->start_ack_pending = FALSE;
	} else {
		if (vfe31_ctrl->recording_state ==
			VFE_REC_STATE_STOP_REQUESTED) {
			vfe31_ctrl->recording_state = VFE_REC_STATE_STOPPED;
			msm_io_w_mb(1, vfe31_ctrl->vfebase +
						VFE_REG_UPDATE_CMD);
		} else if (vfe31_ctrl->recording_state ==
			VFE_REC_STATE_STOPPED) {
			CDBG("sent stop video rec ACK");
			vfe31_send_msg_no_payload(MSG_ID_STOP_REC_ACK);
			vfe31_ctrl->recording_state = VFE_REC_STATE_IDLE;
		}
		spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
		if (vfe31_ctrl->update_ack_pending == TRUE) {
			vfe31_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe31_ctrl->update_ack_lock, flags);
			vfe31_send_msg_no_payload(MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
				&vfe31_ctrl->update_ack_lock, flags);
		}
	}
	/* in snapshot mode */
	if (vfe31_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_SNAPSHOT) {
		/* later we need to add check for live snapshot mode. */

		if (vfe31_ctrl->vfe_capture_count)
			vfe31_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe31_ctrl->vfe_capture_count == 0) {
			/* stop the bus output:  write master enable = 0*/
			if (vfe31_ctrl->outpath.output_mode &
					VFE31_OUTPUT_MODE_PT) {
				msm_io_w(0, vfe31_ctrl->vfebase +
					vfe31_AXI_WM_CFG[
						vfe31_ctrl->outpath.out0.ch0]);
				msm_io_w(0, vfe31_ctrl->vfebase +
					vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out0.ch1]);
			}
			if (vfe31_ctrl->outpath.output_mode &
					VFE31_OUTPUT_MODE_S) {
				msm_io_w(0, vfe31_ctrl->vfebase +
					vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out1.ch0]);
				msm_io_w(0, vfe31_ctrl->vfebase +
					vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out1.ch1]);
			}

			/* Ensure the write order while writing
			to the command register using the barrier */
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);

			/* Ensure the read order while reading
			to the command register using the barrier */
			temp = msm_io_r_mb(vfe31_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
		}
		/* then do reg_update. */
		msm_io_w_mb(1, vfe31_ctrl->vfebase +
			VFE_REG_UPDATE_CMD);
	} /* if snapshot mode. */
}

static void vfe31_set_default_reg_values(void)
{
	msm_io_w(0x800080, vfe31_ctrl->vfebase + VFE_DEMUX_GAIN_0);
	msm_io_w(0x800080, vfe31_ctrl->vfebase + VFE_DEMUX_GAIN_1);
	msm_io_w(0xFFFFF, vfe31_ctrl->vfebase + VFE_CGC_OVERRIDE);

	/* default frame drop period and pattern */
	msm_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_CFG);
	msm_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_CFG);
	msm_io_w(0xFFFFFFFF, vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_PATTERN);
	msm_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_PATTERN);
	msm_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y);
	msm_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR);
	msm_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_PATTERN);
	msm_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR_PATTERN);
	msm_io_w(0, vfe31_ctrl->vfebase + VFE_CLAMP_MIN);
	msm_io_w(0xFFFFFF, vfe31_ctrl->vfebase + VFE_CLAMP_MAX);

	/* stats UB config */
	msm_io_w(0x3980007, vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_UB_CFG);
	msm_io_w(0x3A00007, vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_UB_CFG);
	msm_io_w(0x3A8000F, vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_UB_CFG);
	msm_io_w(0x3B80007, vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_UB_CFG);
	msm_io_w(0x3C0001F, vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_UB_CFG);
	msm_io_w(0x3E0001F, vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_UB_CFG);
}

static void vfe31_process_reset_irq(void)
{
	atomic_set(&vfe31_ctrl->vstate, 0);
	vfe31_ctrl->while_stopping_mask = VFE_IMASK_WHILE_STOPPING_1;
	if (atomic_read(&vfe31_ctrl->stop_ack_pending)) {
		/* this is from the stop command. */
		atomic_set(&vfe31_ctrl->stop_ack_pending, 0);
		vfe31_send_msg_no_payload(MSG_ID_STOP_ACK);
	} else {
		/* this is from reset command. */
		vfe31_set_default_reg_values();

		/* reload all write masters. (frame & line)*/
		msm_io_w_mb(0x7FFF, vfe31_ctrl->vfebase + VFE_BUS_CMD);
		vfe31_send_msg_no_payload(MSG_ID_RESET_ACK);
	}
}


static void vfe31_process_axi_halt_irq(void)
{
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(AXI_HALT_CLEAR,
		vfe31_ctrl->vfebase + VFE_AXI_CMD);
	vfe31_ctrl->while_stopping_mask = VFE_IMASK_RESET;

	/* disable all interrupts.  */
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1,
		vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	/* now enable only halt_irq & reset_irq */
	msm_io_w(0xf0000000,          /* this is for async timer. */
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_IMASK_RESET,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	CDBG("%s: about to reset vfe...\n", __func__);
	msm_io_w_mb(VFE_RESET_UPON_STOP_CMD,
		vfe31_ctrl->vfebase + VFE_GLOBAL_RESET);

}

static void vfe31_process_camif_sof_irq(void)
{
	uint32_t  temp;

	/* in raw snapshot mode */
	if (vfe31_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
		if (vfe31_ctrl->start_ack_pending) {
			vfe31_send_msg_no_payload(MSG_ID_START_ACK);
			vfe31_ctrl->start_ack_pending = FALSE;
		}
		if (vfe31_ctrl->vfe_capture_count)
			vfe31_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe31_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			to the command register using the barrier */
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);
			temp = msm_io_r_mb(vfe31_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */

	if ((vfe31_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe31_ctrl->operation_mode == VFE_MODE_OF_OPERATION_VIDEO) &&
		(vfe31_ctrl->vfeFrameId % vfe31_ctrl->hfr_mode != 0)) {
		vfe31_ctrl->vfeFrameId++;
		CDBG("Skip the SOF notification when HFR enabled\n");
		return;
	}
	vfe31_send_msg_no_payload(MSG_ID_SOF_ACK);
	vfe31_ctrl->vfeFrameId++;
	CDBG("camif_sof_irq, frameId = %d\n", vfe31_ctrl->vfeFrameId);

	if (vfe31_ctrl->sync_timer_state) {
		if (vfe31_ctrl->sync_timer_repeat_count == 0)
			vfe31_sync_timer_stop();
		else
		vfe31_ctrl->sync_timer_repeat_count--;
	}
}

static void vfe31_process_error_irq(uint32_t errStatus)
{
	uint32_t camifStatus, read_val;
	uint32_t *temp;

	if (errStatus & VFE31_IMASK_CAMIF_ERROR) {
		pr_err("vfe31_irq: camif errors\n");
		temp = (uint32_t *)(vfe31_ctrl->vfebase + VFE_CAMIF_STATUS);
		camifStatus = msm_io_r(temp);
		pr_err("camifStatus  = 0x%x\n", camifStatus);
		vfe31_send_msg_no_payload(MSG_ID_CAMIF_ERROR);
	}

	if (errStatus & VFE31_IMASK_STATS_CS_OVWR)
		pr_err("vfe31_irq: stats cs overwrite\n");

	if (errStatus & VFE31_IMASK_STATS_IHIST_OVWR)
		pr_err("vfe31_irq: stats ihist overwrite\n");

	if (errStatus & VFE31_IMASK_REALIGN_BUF_Y_OVFL)
		pr_err("vfe31_irq: realign bug Y overflow\n");

	if (errStatus & VFE31_IMASK_REALIGN_BUF_CB_OVFL)
		pr_err("vfe31_irq: realign bug CB overflow\n");

	if (errStatus & VFE31_IMASK_REALIGN_BUF_CR_OVFL)
		pr_err("vfe31_irq: realign bug CR overflow\n");

	if (errStatus & VFE31_IMASK_VIOLATION)
		pr_err("vfe31_irq: violation interrupt\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_0_BUS_OVFL)
		pr_err("vfe31_irq: image master 0 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_1_BUS_OVFL)
		pr_err("vfe31_irq: image master 1 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_2_BUS_OVFL)
		pr_err("vfe31_irq: image master 2 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_3_BUS_OVFL)
		pr_err("vfe31_irq: image master 3 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_4_BUS_OVFL)
		pr_err("vfe31_irq: image master 4 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_5_BUS_OVFL)
		pr_err("vfe31_irq: image master 5 bus overflow\n");

	if (errStatus & VFE31_IMASK_IMG_MAST_6_BUS_OVFL)
		pr_err("vfe31_irq: image master 6 bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_AE_BUS_OVFL)
		pr_err("vfe31_irq: ae stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_AF_BUS_OVFL)
		pr_err("vfe31_irq: af stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_AWB_BUS_OVFL)
		pr_err("vfe31_irq: awb stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_RS_BUS_OVFL)
		pr_err("vfe31_irq: rs stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_CS_BUS_OVFL)
		pr_err("vfe31_irq: cs stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_IHIST_BUS_OVFL)
		pr_err("vfe31_irq: ihist stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_SKIN_BUS_OVFL)
		pr_err("vfe31_irq: skin stats bus overflow\n");

	if (errStatus & VFE31_IMASK_AXI_ERROR) {
		pr_err("vfe31_irq: axi error\n");
		/* read status too when overflow happens.*/
		read_val = msm_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);
		pr_debug("VFE_BUS_PING_PONG_STATUS = 0x%x\n", read_val);
		read_val = msm_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_OPERATION_STATUS);
		pr_debug("VFE_BUS_OPERATION_STATUS = 0x%x\n", read_val);
		read_val = msm_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_0);
		pr_debug("VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_0 = 0x%x\n",
			read_val);
		read_val = msm_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_1);
		pr_debug("VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_1 = 0x%x\n",
			read_val);
		read_val = msm_io_r(vfe31_ctrl->vfebase +
			VFE_AXI_STATUS);
		pr_debug("VFE_AXI_STATUS = 0x%x\n", read_val);
	}
}

#define VFE31_AXI_OFFSET 0x0050
#define vfe31_get_ch_ping_addr(chn) \
	(msm_io_r(vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe31_get_ch_pong_addr(chn) \
	(msm_io_r(vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe31_get_ch_addr(ping_pong, chn) \
	(((ping_pong) & (1 << (chn))) == 0 ? \
	vfe31_get_ch_pong_addr(chn) : vfe31_get_ch_ping_addr(chn))

#define vfe31_put_ch_ping_addr(chn, addr) \
	(msm_io_w((addr), vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe31_put_ch_pong_addr(chn, addr) \
	(msm_io_w((addr), vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe31_put_ch_addr(ping_pong, chn, addr) \
	(((ping_pong) & (1 << (chn))) == 0 ?   \
	vfe31_put_ch_pong_addr((chn), (addr)) : \
	vfe31_put_ch_ping_addr((chn), (addr)))

static void vfe31_process_output_path_irq_0(uint32_t ping_pong)
{
	uint32_t p0_addr, p1_addr, p2_addr;
#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	struct vfe31_free_buf *free_buf = NULL;
	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	*/
	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out0);

	if (free_buf) {
		/* Y channel */
		p0_addr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		p1_addr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch1);
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_P_ALL_CHNLS) {
			p2_addr = vfe31_get_ch_addr(ping_pong,
				vfe31_ctrl->outpath.out0.ch2);
		} else {
			p2_addr = p0_addr;
		}
		CDBG("Output path 0, p0_addr = 0x%x, p1_addr = 0x%x,"
			 "p2_addr = 0x%x\n", p0_addr, p1_addr, p2_addr);
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch1,
			free_buf->paddr + free_buf->planar1_off);
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_P_ALL_CHNLS)
			vfe31_put_ch_addr(ping_pong,
				vfe31_ctrl->outpath.out0.ch2,
			free_buf->paddr + free_buf->planar2_off);
			kfree(free_buf);
			/* if continuous mode, for display. (preview) */
			vfe_send_outmsg(MSG_ID_OUTPUT_P,  p0_addr, p1_addr,
				p2_addr);
	} else {
		vfe31_ctrl->outpath.out0.frame_drop_cnt++;
		pr_warning("path_irq_0 - no free buffer!\n");
#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out0.ch1);
		/* Y channel */
		pyaddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out0.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out0.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
			(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out0.ch0,
			pyaddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out0.ch0,
			pyaddr_ping);
		/* SWAP chroma channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out0.ch1,
			pcbcraddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out0.ch1,
			pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}
}

static void vfe31_process_snapshot_frame(uint32_t ping_pong)
{
	uint32_t p0_addr, p1_addr;
	struct vfe31_free_buf *free_buf = NULL;
	/* Y channel- Main Image */
	p0_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch0);
	/* Chroma channel - TN Image */
	p1_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch1);

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out1);
	CDBG("%s: snapshot main, p0_addr = 0x%x, p1_addr = 0x%x\n",
		__func__, p0_addr, p1_addr);
	if (free_buf) {
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch1,
			free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
	}
	vfe_send_outmsg(MSG_ID_OUTPUT_S, p0_addr, p1_addr, p0_addr);

	/* Y channel- TN Image */
	p0_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out0.ch0);
	/* Chroma channel - TN Image */
	p1_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out0.ch1);

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out0);
	CDBG("%s: snapshot TN, p0_addr = 0x%x, p1_addr = 0x%x\n",
		__func__, p0_addr, p1_addr);
	if (free_buf) {
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch1,
			free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
	}

	vfe_send_outmsg(MSG_ID_OUTPUT_T, p0_addr, p1_addr, p0_addr);

	/* in snapshot mode if done then send
		snapshot done message */
	if (vfe31_ctrl->vfe_capture_count == 0) {
		vfe31_send_msg_no_payload(MSG_ID_SNAPSHOT_DONE);
		/* Ensure the write order while writing
			to the cmd register using barrier */
		msm_io_w_mb(CAMIF_COMMAND_STOP_IMMEDIATELY,
			vfe31_ctrl->vfebase +
			VFE_CAMIF_COMMAND);
	}
}

static void vfe31_process_raw_snapshot_frame(uint32_t ping_pong)
{
	uint32_t pyaddr, pcbcraddr;
	struct vfe31_free_buf *free_buf = NULL;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;

	if (p_sync->stereocam_enabled)
		p_sync->stereo_state = STEREO_RAW_SNAP_STARTED;

	/* Y channel- Main Image */
	pyaddr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch0);
	/* Chroma channel - Main Image */
	pcbcraddr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch1);

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out1);
	CDBG("%s: snapshot raw, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
		__func__, pyaddr, pcbcraddr);
	if (free_buf) {
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch1,
			free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
	}
	 vfe_send_outmsg(MSG_ID_OUTPUT_S, pyaddr, pcbcraddr, 0);

	/* in snapshot mode if done then send
		snapshot done message */
	if (vfe31_ctrl->vfe_capture_count == 0) {
		vfe31_send_msg_no_payload(MSG_ID_SNAPSHOT_DONE);
		/* Ensure the write order while writing
		to the cmd register using barrier */
		msm_io_w_mb(CAMIF_COMMAND_STOP_IMMEDIATELY,
			vfe31_ctrl->vfebase +
			VFE_CAMIF_COMMAND);
	}
}
static void vfe31_process_zsl_frame(uint32_t ping_pong)
{
	uint32_t p0_addr, p1_addr;
	struct vfe31_free_buf *free_buf = NULL;
	/* Y channel- Main Image */
	p0_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out2.ch0);
	/* Chroma channel - Main Image */
	p1_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out2.ch1);

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out2);
	CDBG("%s: snapshot main, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
		__func__, p0_addr, p1_addr);
	if (free_buf) {
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out2.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out2.ch1,
			free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
	}
	 vfe_send_outmsg(MSG_ID_OUTPUT_S, p0_addr, p1_addr, p0_addr);

	/* Y channel- TN Image */
	p0_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch0);
	/* Chroma channel - TN Image */
	p1_addr = vfe31_get_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out1.ch1);

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out1);
	CDBG("%s: snapshot TN, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
		__func__, p0_addr, p1_addr);
	if (free_buf) {
		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch0,
			free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch1,
			free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
	}

	vfe_send_outmsg(MSG_ID_OUTPUT_T, p0_addr, p1_addr, p0_addr);
}

static void vfe31_process_output_path_irq_1(uint32_t ping_pong)
{

#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	CDBG("%s, operation_mode = %d, cap_cnt = %d\n", __func__,
		vfe31_ctrl->operation_mode, vfe31_ctrl->vfe_capture_count);

	/* In Snapshot mode */
	if ((VFE_MODE_OF_OPERATION_SNAPSHOT == vfe31_ctrl->operation_mode)
		&& ((vfe31_ctrl->vfe_capture_count <= 1)
		|| (vfe31_free_buf_available(vfe31_ctrl->outpath.out0) &&
		vfe31_free_buf_available(vfe31_ctrl->outpath.out1)))) {
		vfe31_process_snapshot_frame(ping_pong);
	} else if ((VFE_MODE_OF_OPERATION_RAW_SNAPSHOT ==
		vfe31_ctrl->operation_mode) &&
		((vfe31_ctrl->vfe_capture_count <= 1) ||
		vfe31_free_buf_available(vfe31_ctrl->outpath.out1))) {
		vfe31_process_raw_snapshot_frame(ping_pong);
	} else if ((VFE_MODE_OF_OPERATION_ZSL == vfe31_ctrl->operation_mode)
		&& (vfe31_free_buf_available(vfe31_ctrl->outpath.out1)
		&& vfe31_free_buf_available(vfe31_ctrl->outpath.out2))) {
		vfe31_process_zsl_frame(ping_pong);
	} else {
		vfe31_ctrl->outpath.out1.frame_drop_cnt++;
		pr_info("path_irq_1 - no free buffer!\n");
#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out1.ch1);
		/* Y channel */
		pyaddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out1.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
			(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out1.ch0,
			pyaddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out1.ch0,
			pyaddr_ping);
		/* SWAP chroma channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out1.ch1,
			pcbcraddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out1.ch1,
			pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}

}

static void vfe31_process_output_path_irq_2(uint32_t ping_pong)
{
	uint32_t p0_addr, p1_addr, p2_addr;
	struct vfe31_free_buf *free_buf = NULL;

#ifdef CONFIG_MSM_CAMERA_V4L2
	uint32_t pyaddr_ping, pcbcraddr_ping, pyaddr_pong, pcbcraddr_pong;
#endif
	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	*/
	CDBG("%s, operation_mode = %d, state %d\n", __func__,
		vfe31_ctrl->operation_mode,
		vfe31_ctrl->recording_state);
	/* Ensure that both wm1 and wm5 ping and pong buffers are active*/
	if (!(((ping_pong & 0x22) == 0x22) ||
		((ping_pong & 0x22) == 0x0))) {
		pr_err(" Irq_2 - skip the frame pp_status is not proper"
			"PP_status = 0x%x\n", ping_pong);
		return;
	}
	if ((vfe31_ctrl->recording_state == VFE_REC_STATE_STOP_REQUESTED)
		|| (vfe31_ctrl->recording_state == VFE_REC_STATE_STOPPED)) {
		vfe31_ctrl->outpath.out2.frame_drop_cnt++;
		pr_warning("path_irq_2 - recording stopped\n");
		return;
	}

	free_buf = vfe31_get_free_buf(&vfe31_ctrl->outpath.out2);

	if (free_buf) {
		/* Y channel */
		p0_addr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		p1_addr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out2.ch1);
		p2_addr = p0_addr;
		CDBG("video output, pyaddr = 0x%x, pcbcraddr = 0x%x\n",
			p0_addr, p1_addr);

		/* Y channel */
		vfe31_put_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out2.ch0,
		free_buf->paddr + free_buf->planar0_off);
		/* Chroma channel */
		vfe31_put_ch_addr(ping_pong,
		vfe31_ctrl->outpath.out2.ch1,
		free_buf->paddr + free_buf->planar1_off);
		kfree(free_buf);
		vfe_send_outmsg(MSG_ID_OUTPUT_V, p0_addr, p1_addr, p2_addr);
	} else {
		vfe31_ctrl->outpath.out2.frame_drop_cnt++;
		pr_warning("path_irq_2 - no free buffer!\n");

#ifdef CONFIG_MSM_CAMERA_V4L2
		pr_info("Swapping ping and pong\n");

		/*get addresses*/
		/* Y channel */
		pyaddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		pcbcraddr_ping = vfe31_get_ch_ping_addr(
			vfe31_ctrl->outpath.out2.ch1);
		/* Y channel */
		pyaddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		pcbcraddr_pong = vfe31_get_ch_pong_addr(
			vfe31_ctrl->outpath.out2.ch1);

		CDBG("ping = 0x%p, pong = 0x%p\n", (void *)pyaddr_ping,
			(void *)pyaddr_pong);
		CDBG("ping_cbcr = 0x%p, pong_cbcr = 0x%p\n",
			(void *)pcbcraddr_ping, (void *)pcbcraddr_pong);

		/*put addresses*/
		/* SWAP y channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out2.ch0,
			pyaddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out2.ch0,
			pyaddr_ping);
		/* SWAP chroma channel*/
		vfe31_put_ch_ping_addr(vfe31_ctrl->outpath.out2.ch1,
			pcbcraddr_pong);
		vfe31_put_ch_pong_addr(vfe31_ctrl->outpath.out2.ch1,
			pcbcraddr_ping);
		CDBG("after swap: ping = 0x%p, pong = 0x%p\n",
			(void *)pyaddr_pong, (void *)pyaddr_ping);
#endif
	}
}


static uint32_t  vfe31_process_stats_irq_common(uint32_t statsNum,
						uint32_t newAddr) {

	uint32_t pingpongStatus;
	uint32_t returnAddr;
	uint32_t pingpongAddr;

	/* must be 0=ping, 1=pong */
	pingpongStatus =
		((msm_io_r(vfe31_ctrl->vfebase +
		VFE_BUS_PING_PONG_STATUS))
	& ((uint32_t)(1<<(statsNum + 7)))) >> (statsNum + 7);
	/* stats bits starts at 7 */
	CDBG("statsNum %d, pingpongStatus %d\n", statsNum, pingpongStatus);
	pingpongAddr =
		((uint32_t)(vfe31_ctrl->vfebase +
				VFE_BUS_STATS_PING_PONG_BASE)) +
				(3*statsNum)*4 + (1-pingpongStatus)*4;
	returnAddr = msm_io_r((uint32_t *)pingpongAddr);
	msm_io_w(newAddr, (uint32_t *)pingpongAddr);
	return returnAddr;
}

static void vfe_send_stats_msg(void)
{
	struct  vfe_message msg;
	uint32_t temp;

	/* fill message with right content. */
	msg._u.msgStats.frameCounter = vfe31_ctrl->vfeFrameId;
	msg._u.msgStats.status_bits = vfe31_ctrl->status_bits;
	msg._d = MSG_ID_COMMON;

	msg._u.msgStats.buff.aec = vfe31_ctrl->aecStatsControl.bufToRender;
	msg._u.msgStats.buff.awb = vfe31_ctrl->awbStatsControl.bufToRender;
	msg._u.msgStats.buff.af = vfe31_ctrl->afStatsControl.bufToRender;

	msg._u.msgStats.buff.ihist = vfe31_ctrl->ihistStatsControl.bufToRender;
	msg._u.msgStats.buff.rs = vfe31_ctrl->rsStatsControl.bufToRender;
	msg._u.msgStats.buff.cs = vfe31_ctrl->csStatsControl.bufToRender;

	temp = msm_io_r(vfe31_ctrl->vfebase + VFE_STATS_AWB_SGW_CFG);
	msg._u.msgStats.buff.awb_ymin = (0xFF00 & temp) >> 8;

	vfe31_proc_ops(msg._d,
		&msg, sizeof(struct vfe_message));
	return;
}

static void vfe31_process_stats(void)
{
	int32_t process_stats = false;

	CDBG("%s, stats = 0x%x\n", __func__, vfe31_ctrl->status_bits);

	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_AEC) {
		if (!vfe31_ctrl->aec_ack_pending) {
			vfe31_ctrl->aec_ack_pending = TRUE;
			vfe31_ctrl->aecStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsAeNum,
				vfe31_ctrl->aecStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe31_ctrl->aecStatsControl.bufToRender = 0;
			vfe31_ctrl->aecStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe31_ctrl->aecStatsControl.bufToRender = 0;
	}

	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_AWB) {
		if (!vfe31_ctrl->awb_ack_pending) {
			vfe31_ctrl->awb_ack_pending = TRUE;
			vfe31_ctrl->awbStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsAwbNum,
				vfe31_ctrl->awbStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe31_ctrl->awbStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->awbStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->awbStatsControl.bufToRender = 0;
	}


	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_AF) {
		if (!vfe31_ctrl->af_ack_pending) {
			vfe31_ctrl->af_ack_pending = TRUE;
			vfe31_ctrl->afStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsAfNum,
				vfe31_ctrl->afStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->afStatsControl.bufToRender = 0;
			vfe31_ctrl->afStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe31_ctrl->afStatsControl.bufToRender = 0;
	}

	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_IHIST) {
		if (!vfe31_ctrl->ihist_ack_pending) {
			vfe31_ctrl->ihist_ack_pending = TRUE;
			vfe31_ctrl->ihistStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsIhistNum,
				vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->ihistStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->ihistStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->ihistStatsControl.bufToRender = 0;
	}

	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_RS) {
		if (!vfe31_ctrl->rs_ack_pending) {
			vfe31_ctrl->rs_ack_pending = TRUE;
			vfe31_ctrl->rsStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsRsNum,
				vfe31_ctrl->rsStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->rsStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->rsStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->rsStatsControl.bufToRender = 0;
	}


	if (vfe31_ctrl->status_bits & VFE_IRQ_STATUS0_STATS_CS) {
		if (!vfe31_ctrl->cs_ack_pending) {
			vfe31_ctrl->cs_ack_pending = TRUE;
			vfe31_ctrl->csStatsControl.bufToRender =
				vfe31_process_stats_irq_common(statsCsNum,
				vfe31_ctrl->csStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->csStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->csStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->csStatsControl.bufToRender = 0;
	}

	if (process_stats)
		vfe_send_stats_msg();

	return;
}

static void vfe31_process_stats_irq(uint32_t *irqstatus)
{
	/* Subsample the stats according to the hfr speed*/
	if ((vfe31_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe31_ctrl->vfeFrameId % vfe31_ctrl->hfr_mode != 0)) {
		CDBG("Skip the stats when HFR enabled\n");
		return;
	}

	vfe31_ctrl->status_bits = VFE_COM_STATUS & *irqstatus;
	vfe31_process_stats();
	return;
}

static void vfe31_do_tasklet(unsigned long data)
{
	unsigned long flags;

	struct vfe31_isr_queue_cmd *qcmd = NULL;

	CDBG("=== vfe31_do_tasklet start === \n");

	while (atomic_read(&irq_cnt)) {
		spin_lock_irqsave(&vfe31_ctrl->tasklet_lock, flags);
		qcmd = list_first_entry(&vfe31_ctrl->tasklet_q,
			struct vfe31_isr_queue_cmd, list);
		atomic_sub(1, &irq_cnt);

		if (!qcmd) {
			spin_unlock_irqrestore(&vfe31_ctrl->tasklet_lock,
				flags);
			return;
		}

		list_del(&qcmd->list);
		spin_unlock_irqrestore(&vfe31_ctrl->tasklet_lock,
			flags);

		/* interrupt to be processed,  *qcmd has the payload.  */
		if (qcmd->vfeInterruptStatus0 &
			VFE_IRQ_STATUS0_REG_UPDATE_MASK) {
			CDBG("irq regUpdateIrq\n");
			vfe31_process_reg_update_irq();
		}

		if (qcmd->vfeInterruptStatus1 &
			VFE_IMASK_RESET) {
			CDBG("irq resetAckIrq\n");
			vfe31_process_reset_irq();
		}


		if (qcmd->vfeInterruptStatus1 &
			VFE_IMASK_AXI_HALT) {
			CDBG("irq axi halt irq\n");
			vfe31_process_axi_halt_irq();
		}

		if (atomic_read(&vfe31_ctrl->vstate)) {
			if (qcmd->vfeInterruptStatus1 &
					VFE31_IMASK_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe31_process_error_irq(
					qcmd->vfeInterruptStatus1 &
					VFE31_IMASK_ERROR_ONLY_1);
			}

			/* irqs below are only valid when in active state. */
			/* next, check output path related interrupts. */
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK) {
				CDBG("Image composite done 0 irq occured.\n");
				vfe31_process_output_path_irq_0(
					qcmd->vfePingPongStatus);
			}

			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK) {
				CDBG("Image composite done 1 irq occured.\n");
				vfe31_process_output_path_irq_1(
					qcmd->vfePingPongStatus);
			}

			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE2_MASK) {
				CDBG("Image composite done 2 irq occured.\n");
				vfe31_process_output_path_irq_2(
					qcmd->vfePingPongStatus);
			}

			/* then process stats irq. */
			if (vfe31_ctrl->stats_comp) {
				/* process stats comb interrupt. */
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK) {
					CDBG("Stats composite irq occured.\n");
					vfe31_process_stats_irq(
						&qcmd->vfeInterruptStatus0);
				}
			} else {
				/* process individual stats interrupt. */
				if (qcmd->vfeInterruptStatus0 &
						VFE_COM_STATUS) {
					CDBG("VFE stats occured.\n");
					vfe31_process_stats_irq(
						&qcmd->vfeInterruptStatus0);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER0) {
					CDBG("SYNC_TIMER 0 irq occured.\n");
					vfe31_send_msg_no_payload(
						MSG_ID_SYNC_TIMER0_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER1) {
					CDBG("SYNC_TIMER 1 irq occured.\n");
					vfe31_send_msg_no_payload(
						MSG_ID_SYNC_TIMER1_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER2) {
					CDBG("SYNC_TIMER 2 irq occured.\n");
					vfe31_send_msg_no_payload(
						MSG_ID_SYNC_TIMER2_DONE);
				}
			}
		}
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_CAMIF_SOF_MASK) {
			CDBG("irq	camifSofIrq\n");
			vfe31_process_camif_sof_irq();
		}
		kfree(qcmd);
	}
	CDBG("=== vfe31_do_tasklet end === \n");
}

DECLARE_TASKLET(vfe31_tasklet, vfe31_do_tasklet, 0);

static irqreturn_t vfe31_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	struct vfe31_irq_status irq;
	struct vfe31_isr_queue_cmd *qcmd;
	uint32_t *val;
	CDBG("vfe_parse_irq\n");
	memset(&irq, 0, sizeof(struct vfe31_irq_status));

	val = (uint32_t *)(vfe31_ctrl->vfebase + VFE_IRQ_STATUS_0);
	irq.vfeIrqStatus0 = msm_io_r(val);

	val = (uint32_t *)(vfe31_ctrl->vfebase + VFE_IRQ_STATUS_1);
	irq.vfeIrqStatus1 = msm_io_r(val);

	if (irq.vfeIrqStatus1 & VFE_IMASK_AXI_HALT) {
		msm_io_w(VFE_IMASK_RESET, vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

		msm_io_w_mb(AXI_HALT_CLEAR,
			vfe31_ctrl->vfebase + VFE_AXI_CMD);
	}

	val = (uint32_t *)(vfe31_ctrl->vfebase + VFE_CAMIF_STATUS);
	irq.camifStatus = msm_io_r(val);
	CDBG("camifStatus  = 0x%x\n", irq.camifStatus);

	val = (uint32_t *)(vfe31_ctrl->vfebase + VFE_BUS_PING_PONG_STATUS);
	irq.vfePingPongStatus = msm_io_r(val);

	/* clear the pending interrupt of the same kind.*/
	msm_io_w(irq.vfeIrqStatus0, vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_io_w(irq.vfeIrqStatus1, vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	if ((irq.vfeIrqStatus0 == 0) && (irq.vfeIrqStatus1 == 0)) {
		CDBG("vfe_parse_irq: vfeIrqStatus0 & 1 are both 0!\n");
		return IRQ_HANDLED;
	}

	qcmd = kzalloc(sizeof(struct vfe31_isr_queue_cmd),
		GFP_ATOMIC);
	if (!qcmd) {
		pr_err("vfe_parse_irq: qcmd malloc failed!\n");
		return IRQ_HANDLED;
	}

	if (atomic_read(&vfe31_ctrl->stop_ack_pending)) {
		irq.vfeIrqStatus0 &= VFE_IMASK_WHILE_STOPPING_0;
		irq.vfeIrqStatus1 &= vfe31_ctrl->while_stopping_mask;
	}

	spin_lock_irqsave(&vfe31_ctrl->xbar_lock, flags);
	if ((irq.vfeIrqStatus0 &
		VFE_IRQ_STATUS0_CAMIF_EOF_MASK) &&
		vfe31_ctrl->xbar_update_pending) {
		CDBG("irq camifEofIrq\n");
		msm_io_memcpy(vfe31_ctrl->vfebase + V31_XBAR_CFG_OFF,
			(void *)vfe31_ctrl->xbar_cfg, V31_XBAR_CFG_LEN);
		vfe31_ctrl->xbar_update_pending = 0;
	}
	spin_unlock_irqrestore(&vfe31_ctrl->xbar_lock, flags);
	CDBG("vfe_parse_irq: Irq_status0 = 0x%x, Irq_status1 = 0x%x.\n",
		irq.vfeIrqStatus0, irq.vfeIrqStatus1);

	qcmd->vfeInterruptStatus0 = irq.vfeIrqStatus0;
	qcmd->vfeInterruptStatus1 = irq.vfeIrqStatus1;
	qcmd->vfePingPongStatus = irq.vfePingPongStatus;

	spin_lock_irqsave(&vfe31_ctrl->tasklet_lock, flags);
	list_add_tail(&qcmd->list, &vfe31_ctrl->tasklet_q);

	atomic_add(1, &irq_cnt);
	spin_unlock_irqrestore(&vfe31_ctrl->tasklet_lock, flags);
	tasklet_schedule(&vfe31_tasklet);
	return IRQ_HANDLED;
}

static void vfe31_release(struct platform_device *pdev)
{
	struct resource	*vfemem, *vfeio;

	vfe31_reset_free_buf_queue_all();
	CDBG("%s, free_irq\n", __func__);
	free_irq(vfe31_ctrl->vfeirq, 0);
	tasklet_kill(&vfe31_tasklet);

	if (atomic_read(&irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	vfemem = vfe31_ctrl->vfemem;
	vfeio  = vfe31_ctrl->vfeio;

	msm_vpe_release();

	kfree(vfe31_ctrl->extdata);
	iounmap(vfe31_ctrl->vfebase);
	kfree(vfe31_ctrl);
	vfe31_ctrl = NULL;
	release_mem_region(vfemem->start, (vfemem->end - vfemem->start) + 1);
	CDBG("%s, msm_camio_disable\n", __func__);
	msm_camio_disable(pdev);
	msm_camio_set_perf_lvl(S_EXIT);

	vfe_syncdata = NULL;
}

static int vfe31_resource_init(struct msm_vfe_callback *presp,
	struct platform_device *pdev, void *sdata)
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

	vfe31_ctrl = kzalloc(sizeof(struct vfe31_ctrl_type), GFP_KERNEL);
	if (!vfe31_ctrl) {
		rc = -ENOMEM;
		goto cmd_init_failed1;
	}

	vfe31_ctrl->vfeirq = vfeirq->start;

	vfe31_ctrl->vfebase =
		ioremap(vfemem->start, (vfemem->end - vfemem->start) + 1);
	if (!vfe31_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto cmd_init_failed2;
	}

	if (presp && presp->vfe_resp)
		vfe31_ctrl->resp = presp;
	else {
		rc = -EINVAL;
		goto cmd_init_failed3;
	}

	vfe31_ctrl->extdata =
		kmalloc(sizeof(struct vfe31_frame_extra), GFP_KERNEL);
	if (!vfe31_ctrl->extdata) {
		rc = -ENOMEM;
		goto cmd_init_failed3;
	}

	vfe31_ctrl->extlen = sizeof(struct vfe31_frame_extra);

	spin_lock_init(&vfe31_ctrl->io_lock);
	spin_lock_init(&vfe31_ctrl->update_ack_lock);
	spin_lock_init(&vfe31_ctrl->tasklet_lock);
	spin_lock_init(&vfe31_ctrl->xbar_lock);

	INIT_LIST_HEAD(&vfe31_ctrl->tasklet_q);
	vfe31_init_free_buf_queue();

	vfe31_ctrl->syncdata = sdata;
	vfe31_ctrl->vfemem = vfemem;
	vfe31_ctrl->vfeio  = vfeio;
	vfe31_ctrl->update_gamma = false;
	vfe31_ctrl->update_luma = false;
	vfe31_ctrl->s_info = s_info;
	vfe31_ctrl->stats_comp = 0;
	vfe31_ctrl->hfr_mode = HFR_MODE_OFF;
	return 0;

cmd_init_failed3:
	free_irq(vfe31_ctrl->vfeirq, 0);
	iounmap(vfe31_ctrl->vfebase);
cmd_init_failed2:
	kfree(vfe31_ctrl);
cmd_init_failed1:
	release_mem_region(vfemem->start, (vfemem->end - vfemem->start) + 1);
	return rc;
}

static int vfe31_init(struct msm_vfe_callback *presp,
	struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	camio_clk = camdev->ioclk;

	rc = vfe31_resource_init(presp, pdev, vfe_syncdata);
	if (rc < 0)
		return rc;
	/* Bring up all the required GPIOs and Clocks */
	rc = msm_camio_enable(pdev);
	msm_camio_set_perf_lvl(S_INIT);
	if (msm_vpe_open() < 0)
		CDBG("%s: vpe_open failed\n", __func__);

	/* TO DO: Need to release the VFE resources */
	rc = request_irq(vfe31_ctrl->vfeirq, vfe31_parse_irq,
			IRQF_TRIGGER_RISING, "vfe", 0);

	return rc;
}

void msm_camvfe_fn_init(struct msm_camvfe_fn *fptr, void *data)
{
	fptr->vfe_init    = vfe31_init;
	fptr->vfe_enable  = vfe31_enable;
	fptr->vfe_config  = vfe31_config;
	fptr->vfe_disable = vfe31_disable;
	fptr->vfe_release = vfe31_release;
	fptr->vfe_stop = vfe31_stop;
	vfe_syncdata = data;
}

void msm_camvpe_fn_init(struct msm_camvpe_fn *fptr, void *data)
{
	fptr->vpe_reg		= msm_vpe_reg;
	fptr->send_frame_to_vpe	= msm_send_frame_to_vpe;
	fptr->vpe_config	= msm_vpe_config;
	fptr->vpe_cfg_update	= msm_vpe_cfg_update;
	fptr->dis		= &(vpe_ctrl->dis_en);
	fptr->vpe_cfg_offset = msm_vpe_offset_update;
	vpe_ctrl->syncdata = data;
}
