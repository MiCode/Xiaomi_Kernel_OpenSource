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
#include <media/msm_isp.h>

#include "msm.h"
#include "msm_vfe32.h"
#include "msm_vpe1.h"
#include "msm_ispif.h"

atomic_t irq_cnt;

#define CHECKED_COPY_FROM_USER(in) {					\
	if (copy_from_user((in), (void __user *)cmd->value,		\
			cmd->length)) {					\
		rc = -EFAULT;						\
		break;							\
	}								\
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

static struct vfe32_ctrl_type *vfe32_ctrl;
static struct msm_camera_io_clk camio_clk;
static void  *vfe_syncdata;

struct vfe32_isr_queue_cmd {
	struct list_head list;
	uint32_t                           vfeInterruptStatus0;
	uint32_t                           vfeInterruptStatus1;
};

static struct vfe32_cmd_type vfe32_cmd[] = {
/* 0*/	{VFE_CMD_DUMMY_0},
		{VFE_CMD_SET_CLK},
		{VFE_CMD_RESET},
		{VFE_CMD_START},
		{VFE_CMD_TEST_GEN_START},
/* 5*/	{VFE_CMD_OPERATION_CFG, V32_OPERATION_CFG_LEN},
		{VFE_CMD_AXI_OUT_CFG, V32_AXI_OUT_LEN, V32_AXI_OUT_OFF, 0xFF},
		{VFE_CMD_CAMIF_CFG, V32_CAMIF_LEN, V32_CAMIF_OFF, 0xFF},
		{VFE_CMD_AXI_INPUT_CFG},
		{VFE_CMD_BLACK_LEVEL_CFG, V32_BLACK_LEVEL_LEN,
		V32_BLACK_LEVEL_OFF,
		0xFF},
/*10*/  {VFE_CMD_ROLL_OFF_CFG, V32_ROLL_OFF_CFG_LEN, V32_ROLL_OFF_CFG_OFF,
		0xFF},
		{VFE_CMD_DEMUX_CFG, V32_DEMUX_LEN, V32_DEMUX_OFF, 0xFF},
		{VFE_CMD_FOV_CFG, V32_FOV_LEN, V32_FOV_OFF, 0xFF},
		{VFE_CMD_MAIN_SCALER_CFG, V32_MAIN_SCALER_LEN,
		V32_MAIN_SCALER_OFF, 0xFF},
		{VFE_CMD_WB_CFG, V32_WB_LEN, V32_WB_OFF, 0xFF},
/*15*/	{VFE_CMD_COLOR_COR_CFG, V32_COLOR_COR_LEN, V32_COLOR_COR_OFF, 0xFF},
		{VFE_CMD_RGB_G_CFG, V32_RGB_G_LEN, V32_RGB_G_OFF, 0xFF},
		{VFE_CMD_LA_CFG, V32_LA_LEN, V32_LA_OFF, 0xFF },
		{VFE_CMD_CHROMA_EN_CFG, V32_CHROMA_EN_LEN, V32_CHROMA_EN_OFF,
		0xFF},
		{VFE_CMD_CHROMA_SUP_CFG, V32_CHROMA_SUP_LEN, V32_CHROMA_SUP_OFF,
		0xFF},
/*20*/	{VFE_CMD_MCE_CFG, V32_MCE_LEN, V32_MCE_OFF, 0xFF},
		{VFE_CMD_SK_ENHAN_CFG, V32_SCE_LEN, V32_SCE_OFF, 0xFF},
		{VFE_CMD_ASF_CFG, V32_ASF_LEN, V32_ASF_OFF, 0xFF},
		{VFE_CMD_S2Y_CFG, V32_S2Y_LEN, V32_S2Y_OFF, 0xFF},
		{VFE_CMD_S2CbCr_CFG, V32_S2CbCr_LEN, V32_S2CbCr_OFF, 0xFF},
/*25*/	{VFE_CMD_CHROMA_SUBS_CFG, V32_CHROMA_SUBS_LEN, V32_CHROMA_SUBS_OFF,
		0xFF},
		{VFE_CMD_OUT_CLAMP_CFG, V32_OUT_CLAMP_LEN, V32_OUT_CLAMP_OFF,
		0xFF},
		{VFE_CMD_FRAME_SKIP_CFG, V32_FRAME_SKIP_LEN, V32_FRAME_SKIP_OFF,
		0xFF},
		{VFE_CMD_DUMMY_1},
		{VFE_CMD_DUMMY_2},
/*30*/	{VFE_CMD_DUMMY_3},
		{VFE_CMD_UPDATE},
		{VFE_CMD_BL_LVL_UPDATE, V32_BLACK_LEVEL_LEN,
		V32_BLACK_LEVEL_OFF, 0xFF},
		{VFE_CMD_DEMUX_UPDATE, V32_DEMUX_LEN, V32_DEMUX_OFF, 0xFF},
		{VFE_CMD_FOV_UPDATE, V32_FOV_LEN, V32_FOV_OFF, 0xFF},
/*35*/	{VFE_CMD_MAIN_SCALER_UPDATE, V32_MAIN_SCALER_LEN, V32_MAIN_SCALER_OFF,
		0xFF},
		{VFE_CMD_WB_UPDATE, V32_WB_LEN, V32_WB_OFF, 0xFF},
		{VFE_CMD_COLOR_COR_UPDATE, V32_COLOR_COR_LEN, V32_COLOR_COR_OFF,
		0xFF},
		{VFE_CMD_RGB_G_UPDATE, V32_RGB_G_LEN, V32_CHROMA_EN_OFF, 0xFF},
		{VFE_CMD_LA_UPDATE, V32_LA_LEN, V32_LA_OFF, 0xFF },
/*40*/	{VFE_CMD_CHROMA_EN_UPDATE, V32_CHROMA_EN_LEN, V32_CHROMA_EN_OFF,
		0xFF},
		{VFE_CMD_CHROMA_SUP_UPDATE, V32_CHROMA_SUP_LEN,
		V32_CHROMA_SUP_OFF, 0xFF},
		{VFE_CMD_MCE_UPDATE, V32_MCE_LEN, V32_MCE_OFF, 0xFF},
		{VFE_CMD_SK_ENHAN_UPDATE, V32_SCE_LEN, V32_SCE_OFF, 0xFF},
		{VFE_CMD_S2CbCr_UPDATE, V32_S2CbCr_LEN, V32_S2CbCr_OFF, 0xFF},
/*45*/	{VFE_CMD_S2Y_UPDATE, V32_S2Y_LEN, V32_S2Y_OFF, 0xFF},
		{VFE_CMD_ASF_UPDATE, V32_ASF_UPDATE_LEN, V32_ASF_OFF, 0xFF},
		{VFE_CMD_FRAME_SKIP_UPDATE},
		{VFE_CMD_CAMIF_FRAME_UPDATE},
		{VFE_CMD_STATS_AF_UPDATE, V32_STATS_AF_LEN, V32_STATS_AF_OFF},
/*50*/	{VFE_CMD_STATS_AE_UPDATE, V32_STATS_AE_LEN, V32_STATS_AE_OFF},
		{VFE_CMD_STATS_AWB_UPDATE, V32_STATS_AWB_LEN,
		V32_STATS_AWB_OFF},
		{VFE_CMD_STATS_RS_UPDATE, V32_STATS_RS_LEN, V32_STATS_RS_OFF},
		{VFE_CMD_STATS_CS_UPDATE, V32_STATS_CS_LEN, V32_STATS_CS_OFF},
		{VFE_CMD_STATS_SKIN_UPDATE},
/*55*/	{VFE_CMD_STATS_IHIST_UPDATE, V32_STATS_IHIST_LEN, V32_STATS_IHIST_OFF},
		{VFE_CMD_DUMMY_4},
		{VFE_CMD_EPOCH1_ACK},
		{VFE_CMD_EPOCH2_ACK},
		{VFE_CMD_START_RECORDING},
/*60*/	{VFE_CMD_STOP_RECORDING},
		{VFE_CMD_DUMMY_5},
		{VFE_CMD_DUMMY_6},
		{VFE_CMD_CAPTURE, V32_CAPTURE_LEN, 0xFF},
		{VFE_CMD_DUMMY_7},
/*65*/	{VFE_CMD_STOP},
		{VFE_CMD_GET_HW_VERSION},
		{VFE_CMD_GET_FRAME_SKIP_COUNTS},
		{VFE_CMD_OUTPUT1_BUFFER_ENQ},
		{VFE_CMD_OUTPUT2_BUFFER_ENQ},
/*70*/	{VFE_CMD_OUTPUT3_BUFFER_ENQ},
		{VFE_CMD_JPEG_OUT_BUF_ENQ},
		{VFE_CMD_RAW_OUT_BUF_ENQ},
		{VFE_CMD_RAW_IN_BUF_ENQ},
		{VFE_CMD_STATS_AF_ENQ},
/*75*/	{VFE_CMD_STATS_AE_ENQ},
		{VFE_CMD_STATS_AWB_ENQ},
		{VFE_CMD_STATS_RS_ENQ},
		{VFE_CMD_STATS_CS_ENQ},
		{VFE_CMD_STATS_SKIN_ENQ},
/*80*/	{VFE_CMD_STATS_IHIST_ENQ},
		{VFE_CMD_DUMMY_8},
		{VFE_CMD_JPEG_ENC_CFG},
		{VFE_CMD_DUMMY_9},
		{VFE_CMD_STATS_AF_START, V32_STATS_AF_LEN, V32_STATS_AF_OFF},
/*85*/	{VFE_CMD_STATS_AF_STOP},
		{VFE_CMD_STATS_AE_START, V32_STATS_AE_LEN, V32_STATS_AE_OFF},
		{VFE_CMD_STATS_AE_STOP},
		{VFE_CMD_STATS_AWB_START, V32_STATS_AWB_LEN, V32_STATS_AWB_OFF},
		{VFE_CMD_STATS_AWB_STOP},
/*90*/	{VFE_CMD_STATS_RS_START, V32_STATS_RS_LEN, V32_STATS_RS_OFF},
		{VFE_CMD_STATS_RS_STOP},
		{VFE_CMD_STATS_CS_START, V32_STATS_CS_LEN, V32_STATS_CS_OFF},
		{VFE_CMD_STATS_CS_STOP},
		{VFE_CMD_STATS_SKIN_START},
/*95*/	{VFE_CMD_STATS_SKIN_STOP},
		{VFE_CMD_STATS_IHIST_START,
		V32_STATS_IHIST_LEN, V32_STATS_IHIST_OFF},
		{VFE_CMD_STATS_IHIST_STOP},
		{VFE_CMD_DUMMY_10},
		{VFE_CMD_SYNC_TIMER_SETTING, V32_SYNC_TIMER_LEN,
			V32_SYNC_TIMER_OFF},
/*100*/	{VFE_CMD_ASYNC_TIMER_SETTING, V32_ASYNC_TIMER_LEN, V32_ASYNC_TIMER_OFF},
		{VFE_CMD_LIVESHOT},
		{VFE_CMD_LA_SETUP},
		{VFE_CMD_LINEARIZATION_CFG, V32_LINEARIZATION_LEN1,
			V32_LINEARIZATION_OFF1},
		{VFE_CMD_DEMOSAICV3},
/*105*/	{VFE_CMD_DEMOSAICV3_ABCC_CFG},
		{VFE_CMD_DEMOSAICV3_DBCC_CFG, V32_DEMOSAICV3_DBCC_LEN,
			V32_DEMOSAICV3_DBCC_OFF},
		{VFE_CMD_DEMOSAICV3_DBPC_CFG},
		{VFE_CMD_DEMOSAICV3_ABF_CFG, V32_DEMOSAICV3_ABF_LEN,
			V32_DEMOSAICV3_ABF_OFF},
		{VFE_CMD_DEMOSAICV3_ABCC_UPDATE},
/*110*/	{VFE_CMD_DEMOSAICV3_DBCC_UPDATE, V32_DEMOSAICV3_DBCC_LEN,
			V32_DEMOSAICV3_DBCC_OFF},
		{VFE_CMD_DEMOSAICV3_DBPC_UPDATE},
		{VFE_CMD_XBAR_CFG},
		{VFE_CMD_EZTUNE_CFG},
		{VFE_CMD_ZSL},
/*115*/	{VFE_CMD_LINEARIZATION_UPDATE, V32_LINEARIZATION_LEN1,
			V32_LINEARIZATION_OFF1},
		{VFE_CMD_DEMOSAICV3_ABF_UPDATE, V32_DEMOSAICV3_ABF_LEN,
			V32_DEMOSAICV3_ABF_OFF},
		{VFE_CMD_CLF_CFG, V32_CLF_CFG_LEN, V32_CLF_CFG_OFF},
		{VFE_CMD_CLF_LUMA_UPDATE, V32_CLF_LUMA_UPDATE_LEN,
			V32_CLF_LUMA_UPDATE_OFF},
		{VFE_CMD_CLF_CHROMA_UPDATE, V32_CLF_CHROMA_UPDATE_LEN,
			V32_CLF_CHROMA_UPDATE_OFF},
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
	"V32_ZSL",
	"LINEARIZATION_UPDATE", /*115*/
	"DEMOSAICV3_ABF_UPDATE",
	"CLF_CFG",
	"CLF_LUMA_UPDATE",
	"CLF_CHROMA_UPDATE",
};

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
	 * at any time. stop ispif & camif immediately. */
	v4l2_subdev_notify(vfe32_ctrl->subdev, NOTIFY_ISPIF_STREAM,
			(void *)ISPIF_STREAM(PIX0, ISPIF_OFF_IMMEDIATELY));
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

static void vfe32_subdev_notify(int id, int path)
{
	struct msm_vfe_resp *rp;
	unsigned long flags = 0;
	spin_lock_irqsave(&vfe32_ctrl->sd_notify_lock, flags);
	rp = msm_isp_sync_alloc(sizeof(struct msm_vfe_resp), GFP_ATOMIC);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}
	CDBG("vfe32_subdev_notify : msgId = %d\n", id);
	rp->evt_msg.type   = MSM_CAMERA_MSG;
	rp->evt_msg.msg_id = path;
	rp->type	   = id;
	v4l2_subdev_notify(vfe32_ctrl->subdev, NOTIFY_VFE_BUF_EVT, rp);
	spin_unlock_irqrestore(&vfe32_ctrl->sd_notify_lock, flags);
}

static int vfe32_config_axi(int mode, uint32_t *ao)
{
	int32_t *ch_info;

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

	switch (mode) {

	case OUTPUT_2:
		vfe32_ctrl->outpath.output_mode |= VFE32_OUTPUT_MODE_PT;
		break;

	case OUTPUT_1_AND_2:
		/* use wm0& 4 for thumbnail, wm1&5 for main image.*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_S;  /* main image.*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_PT;  /* thumbnail. */
		break;

	case OUTPUT_1_2_AND_3:
		CDBG("%s: OUTPUT_1_2_AND_3", __func__);
		/* use wm0& 4 for postview, wm1&5 for preview.*/
		/* use wm2& 6 for main img */
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_S;  /* main image.*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_P;  /* preview. */
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_T;  /* thumbnail. */
		break;

	case OUTPUT_1_AND_3:
		/* use wm0& 4 for preview, wm1&5 for video.*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_V;  /* video*/
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_PT;  /* preview */
		break;
	case CAMIF_TO_AXI_VIA_OUTPUT_2:
		/* use wm0 only */
		CDBG("config axi for raw snapshot.\n");
		vfe32_ctrl->outpath.out1.ch0 = 0; /* raw */
		vfe32_ctrl->outpath.output_mode |= VFE32_OUTPUT_MODE_S;
		break;
	default:
		break;
	}
	msm_io_memcpy(vfe32_ctrl->vfebase +
		vfe32_cmd[VFE_CMD_AXI_OUT_CFG].offset, ao,
		vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length - V32_AXI_CH_INF_LEN);
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

	vfe32_ctrl->recording_state = VFE_REC_STATE_IDLE;

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
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);

	v4l2_subdev_notify(vfe32_ctrl->subdev, NOTIFY_ISPIF_STREAM,
			(void *)ISPIF_STREAM(PIX0, ISPIF_ON_FRAME_BOUNDARY));

	atomic_set(&vfe32_ctrl->vstate, 1);
}

static int vfe32_start_recording(void)
{
	vfe32_ctrl->recording_state = VFE_REC_STATE_START_REQUESTED;
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe32_stop_recording(void)
{
	vfe32_ctrl->recording_state = VFE_REC_STATE_STOP_REQUESTED;
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static void vfe32_liveshot(void){
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync)
		p_sync->liveshot_enabled = true;
}

static int vfe32_zsl(void)
{
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	CDBG("%s:op mode %d O/P Mode %d\n", __func__,
		vfe32_ctrl->operation_mode, vfe32_ctrl->outpath.output_mode);
	if ((vfe32_ctrl->operation_mode == VFE_MODE_OF_OPERATION_ZSL)) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_P) {
			irq_comp_mask |=
				((0x1 << (vfe32_ctrl->outpath.out0.ch0)) |
				(0x1 << (vfe32_ctrl->outpath.out0.ch1)));
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_T) {
			irq_comp_mask |=
				((0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8)) |
				(0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8)));
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_S) {
			irq_comp_mask |=
			((0x1 << (vfe32_ctrl->outpath.out2.ch0 + 8)) |
			(0x1 << (vfe32_ctrl->outpath.out2.ch1 + 8)));
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_P) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_T) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		}
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_S) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch1]);
		}
	}
	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe32_start_common();
	msm_camio_set_perf_lvl(S_ZSL);

	msm_io_w(1, vfe32_ctrl->vfebase + 0x18C);
	msm_io_w(1, vfe32_ctrl->vfebase + 0x188);
	return 0;
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
	if (vfe32_ctrl->update_linear) {
		if (!msm_io_r(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1))
			msm_io_w(1,
				vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1);
		else
			msm_io_w(0,
				vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1);
		vfe32_ctrl->update_linear = false;
	}
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
static struct vfe32_output_ch *vfe32_get_ch(int path)
{
	struct vfe32_output_ch *ch = NULL;

	switch (vfe32_ctrl->operation_mode) {
	case VFE_MODE_OF_OPERATION_CONTINUOUS:
		if (path == VFE_MSG_OUTPUT_P)
			ch = &vfe32_ctrl->outpath.out0;
		break;
	case VFE_MODE_OF_OPERATION_SNAPSHOT:
		if (path == VFE_MSG_OUTPUT_T)
			ch = &vfe32_ctrl->outpath.out0;
		else if (path == VFE_MSG_OUTPUT_S)
			ch = &vfe32_ctrl->outpath.out1;
		break;
	case VFE_MODE_OF_OPERATION_VIDEO:
		if (path == VFE_MSG_OUTPUT_P)
			ch = &vfe32_ctrl->outpath.out0;
		else if (path == VFE_MSG_OUTPUT_V)
			ch = &vfe32_ctrl->outpath.out2;
		break;
	case VFE_MODE_OF_OPERATION_RAW_SNAPSHOT:
		if (path == VFE_MSG_OUTPUT_S)
			ch = &vfe32_ctrl->outpath.out0;
		break;
	case VFE_MODE_OF_OPERATION_ZSL:
		if (path == VFE_MSG_OUTPUT_P)
			ch = &vfe32_ctrl->outpath.out0;
		else if (path == VFE_MSG_OUTPUT_T)
			ch = &vfe32_ctrl->outpath.out1;
		else if (path == VFE_MSG_OUTPUT_S)
			ch = &vfe32_ctrl->outpath.out2;
		break;
	default:
		pr_err("%s: Unsupported operation mode %d\n", __func__,
					vfe32_ctrl->operation_mode);
	}

	BUG_ON(ch == NULL);
	return ch;
}
static struct msm_free_buf *vfe32_check_free_buffer(int id, int path)
{
	struct vfe32_output_ch *outch = NULL;
	struct msm_free_buf *b = NULL;
	vfe32_subdev_notify(id, path);
	outch = vfe32_get_ch(path);
	if (outch->free_buf.ch_paddr[0])
		b = &outch->free_buf;
	return b;
}
static int vfe32_configure_pingpong_buffers(int id, int path)
{
	struct vfe32_output_ch *outch = NULL;
	int rc = 0;
	vfe32_subdev_notify(id, path);
	outch = vfe32_get_ch(path);
	if (outch->ping.ch_paddr[0] && outch->pong.ch_paddr[0]) {
		/* Configure Preview Ping Pong */
		pr_info("%s Configure ping/pong address for %d",
						__func__, path);
		vfe32_put_ch_ping_addr(outch->ch0,
			outch->ping.ch_paddr[0]);
		vfe32_put_ch_ping_addr(outch->ch1,
			outch->ping.ch_paddr[1]);
		if (outch->ping.num_planes > 2)
			vfe32_put_ch_ping_addr(outch->ch2,
				outch->ping.ch_paddr[2]);

		vfe32_put_ch_pong_addr(outch->ch0,
			outch->pong.ch_paddr[0]);
		vfe32_put_ch_pong_addr(outch->ch1,
			outch->pong.ch_paddr[1]);
		if (outch->pong.num_planes > 2)
			vfe32_put_ch_pong_addr(outch->ch2,
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

static void vfe32_write_linear_cfg(enum VFE32_DMI_RAM_SEL channel_sel,
	const uint32_t *tbl)
{
	uint32_t i;

	vfe32_program_dmi_cfg(channel_sel);
	/* for loop for configuring LUT. */
	for (i = 0 ; i < VFE32_LINEARIZATON_TABLE_LENGTH ; i++) {
		msm_io_w(*tbl, vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
		tbl++;
	}
	CDBG("done writing to linearization table\n");
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
	case VFE_CMD_RESET:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		vfe32_reset();
		break;
	case VFE_CMD_START:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
							VFE_MSG_OUTPUT_P);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe32_start();
		break;
	case VFE_CMD_UPDATE:
		vfe32_update();
		break;
	case VFE_CMD_CAPTURE:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_CAPTURE,
							VFE_MSG_OUTPUT_S);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				   " for snapshot", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		if (vfe32_ctrl->operation_mode !=
				VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_CAPTURE, VFE_MSG_OUTPUT_T);
			if (rc < 0) {
				pr_err("%s error configuring pingpong buffers"
					   " for thumbnail", __func__);
				rc = -EINVAL;
				goto proc_general_done;
			}
		}
		rc = vfe32_capture(snapshot_cnt);
		break;
	case VFE_CMD_START_RECORDING:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_configure_pingpong_buffers(
			VFE_MSG_V32_START_RECORDING, VFE_MSG_OUTPUT_V);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for video", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe32_start_recording();
		break;
	case VFE_CMD_STOP_RECORDING:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		rc = vfe32_stop_recording();
		break;
	case VFE_CMD_OPERATION_CFG: {
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

	case VFE_CMD_STATS_AE_START: {
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
	case VFE_CMD_STATS_AF_START: {
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
	case VFE_CMD_STATS_AWB_START: {
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

	case VFE_CMD_STATS_IHIST_START: {
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


	case VFE_CMD_STATS_RS_START: {
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

	case VFE_CMD_STATS_CS_START: {
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

	case VFE_CMD_MCE_UPDATE:
	case VFE_CMD_MCE_CFG:{
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
	case VFE_CMD_BLACK_LEVEL_CFG:
		rc = -EFAULT;
		goto proc_general_done;
	case VFE_CMD_ROLL_OFF_CFG: {
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

	case VFE_CMD_LA_CFG:
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_SCE_OFF,
				cmdp, V32_SCE_LEN);
		}
		break;

	case VFE_CMD_LIVESHOT:
		vfe32_liveshot();
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1,
			cmdp_local, V32_LINEARIZATION_LEN1);
		cmdp_local += 4;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF2,
			cmdp_local, V32_LINEARIZATION_LEN2);

		cmdp_local = cmdp + 17;
		vfe32_write_linear_cfg(BLACK_LUT_RAM_BANK0, cmdp_local);
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1,
			cmdp_local, (V32_LINEARIZATION_LEN1 - 4));
		cmdp_local += 3;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF2,
			cmdp_local, V32_LINEARIZATION_LEN2);
		cmdp_local = cmdp + 17;
		/*extracting the bank select*/
		old_val =
			msm_io_r(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1);

		if (old_val != 0x0)
			vfe32_write_linear_cfg(BLACK_LUT_RAM_BANK0, cmdp_local);
		else
			vfe32_write_linear_cfg(BLACK_LUT_RAM_BANK1, cmdp_local);
		vfe32_ctrl->update_linear = true;
	break;

	case VFE_CMD_DEMOSAICV3:
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF);
		old_val &= ABF_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF,
		    cmdp_local, 4);

		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp_local, (vfe32_cmd[cmd->id].length));
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_RGB_G_OFF,
				cmdp, 4);
		cmdp += 1;
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0 , cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0 , cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0 , cmdp);
		cmdp -= 1;
		}
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

	case VFE_CMD_STATS_AWB_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AWB_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STATS_AE_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AE_BG_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STATS_AF_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AF_BF_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_IHIST_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~IHIST_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_RS_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~RS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;

	case VFE_CMD_STATS_CS_STOP: {
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~CS_ENABLE_MASK;
		msm_io_w(old_val,
			vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STOP:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		vfe32_stop();
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
		vfe32_sync_timer_start(cmdp);
		break;

	case VFE_CMD_EZTUNE_CFG: {
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

	case VFE_CMD_ZSL:
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
							VFE_MSG_OUTPUT_P);
		if (rc < 0)
			goto proc_general_done;
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
							VFE_MSG_OUTPUT_T);
		if (rc < 0)
			goto proc_general_done;
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
							VFE_MSG_OUTPUT_S);
		if (rc < 0)
			goto proc_general_done;

		rc = vfe32_zsl();
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
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp, (vfe32_cmd[cmd->id].length));
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

static void vfe32_process_reg_update_irq(void)
{
	uint32_t  old_val;
	unsigned long flags;
	if (vfe32_ctrl->recording_state == VFE_REC_STATE_START_REQUESTED) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_V) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch1]);
		}
		vfe32_ctrl->recording_state = VFE_REC_STATE_STARTED;
		if (vpe_ctrl && vpe_ctrl->dis_en) {
			old_val = msm_io_r(
				vfe32_ctrl->vfebase + VFE_MODULE_CFG);
			old_val |= RS_CS_ENABLE_MASK;
			msm_io_w(old_val,
				vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		}
		msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
	} else if (vfe32_ctrl->recording_state ==
			VFE_REC_STATE_STOP_REQUESTED) {
		if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_V) {
			msm_io_w_mb(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch0]);
			msm_io_w_mb(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out2.ch1]);
		}

		/*disable rs& cs when stop recording. */
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= (~RS_CS_ENABLE_MASK);
		msm_io_w(old_val, vfe32_ctrl->vfebase + VFE_MODULE_CFG);

		CDBG("stop video triggered .\n");
	}
	if (vfe32_ctrl->start_ack_pending == TRUE) {
		v4l2_subdev_notify(vfe32_ctrl->subdev, NOTIFY_ISP_MSG_EVT,
			(void *)MSG_ID_START_ACK);
		vfe32_ctrl->start_ack_pending = FALSE;
	} else {
		if (vfe32_ctrl->recording_state ==
			VFE_REC_STATE_STOP_REQUESTED) {
			vfe32_ctrl->recording_state = VFE_REC_STATE_STOPPED;
			/* request a reg update and send STOP_REC_ACK
			 * when we process the next reg update irq.
			 */
			msm_io_w_mb(1,
			vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		} else if (vfe32_ctrl->recording_state ==
			VFE_REC_STATE_STOPPED) {
			v4l2_subdev_notify(vfe32_ctrl->subdev,
					NOTIFY_ISP_MSG_EVT,
					(void *)MSG_ID_STOP_REC_ACK);
			vfe32_ctrl->recording_state = VFE_REC_STATE_IDLE;
		}
		spin_lock_irqsave(&vfe32_ctrl->update_ack_lock, flags);
		if (vfe32_ctrl->update_ack_pending == TRUE) {
			vfe32_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe32_ctrl->update_ack_lock, flags);
			v4l2_subdev_notify(vfe32_ctrl->subdev,
					NOTIFY_ISP_MSG_EVT,
					(void *)MSG_ID_UPDATE_ACK);
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

			v4l2_subdev_notify(vfe32_ctrl->subdev,
				NOTIFY_ISPIF_STREAM, (void *)
				ISPIF_STREAM(PIX0, ISPIF_OFF_FRAME_BOUNDARY));
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);

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
		v4l2_subdev_notify(vfe32_ctrl->subdev,
					NOTIFY_ISP_MSG_EVT,
					(void *)MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe32_set_default_reg_values();

		/* reload all write masters. (frame & line)*/
		msm_io_w(0x7FFF, vfe32_ctrl->vfebase + VFE_BUS_CMD);
		v4l2_subdev_notify(vfe32_ctrl->subdev,
					NOTIFY_ISP_MSG_EVT,
					(void *)MSG_ID_RESET_ACK);
	}
}

static void vfe32_process_camif_sof_irq(void)
{
	/* in raw snapshot mode */
	if (vfe32_ctrl->operation_mode ==
		VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
		if (vfe32_ctrl->start_ack_pending) {
			v4l2_subdev_notify(vfe32_ctrl->subdev,
					NOTIFY_ISP_MSG_EVT,
					(void *)MSG_ID_START_ACK);
			vfe32_ctrl->start_ack_pending = FALSE;
		}
		vfe32_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe32_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			 to the command register using the barrier */
			v4l2_subdev_notify(vfe32_ctrl->subdev,
				NOTIFY_ISPIF_STREAM, (void *)
				ISPIF_STREAM(PIX0, ISPIF_OFF_FRAME_BOUNDARY));
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */

	v4l2_subdev_notify(vfe32_ctrl->subdev,
				NOTIFY_ISP_MSG_EVT,
				(void *)MSG_ID_SOF_ACK);
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
		v4l2_subdev_notify(vfe32_ctrl->subdev,
				NOTIFY_ISP_MSG_EVT,
				(void *)MSG_ID_CAMIF_ERROR);
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

static void vfe_send_outmsg(struct v4l2_subdev *sd, uint8_t msgid,
	uint32_t ch0_paddr, uint32_t ch1_paddr, uint32_t ch2_paddr)
{
	struct isp_msg_output msg;

	msg.output_id = msgid;
	msg.buf.ch_paddr[0]	= ch0_paddr;
	msg.buf.ch_paddr[1]	= ch1_paddr;
	msg.buf.ch_paddr[2]	= ch2_paddr;
	msg.frameCounter = vfe32_ctrl->vfeFrameId;

	v4l2_subdev_notify(vfe32_ctrl->subdev,
			NOTIFY_VFE_MSG_OUT,
			&msg);
	return;
}

static void vfe32_process_output_path_irq_0(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;
	if (vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT)
		free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
							VFE_MSG_OUTPUT_T);
	else
		free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
							VFE_MSG_OUTPUT_P);
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

		/* Channel 0*/
		ch0_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch0);
		/* Channel 1*/
		ch1_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch1);
		/* Channel 2*/
		ch2_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch2);

		CDBG("output path 0, ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out0.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe32_put_ch_addr(ping_pong,
					vfe32_ctrl->outpath.out0.ch2,
					free_buf->ch_paddr[2]);
		}
		if (vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT) {
			/* will add message for multi-shot. */
			vfe32_ctrl->outpath.out0.capture_cnt--;
			vfe_send_outmsg(vfe32_ctrl->subdev,
				MSG_ID_OUTPUT_T, ch0_paddr,
				ch1_paddr, ch2_paddr);
		} else {
			/* always send message for continous mode. */
			/* if continuous mode, for display. (preview) */
			vfe_send_outmsg(vfe32_ctrl->subdev,
				MSG_ID_OUTPUT_P, ch0_paddr,
				ch1_paddr, ch2_paddr);
		}
	} else {
		vfe32_ctrl->outpath.out0.frame_drop_cnt++;
		CDBG("path_irq_0 - no free buffer!\n");
	}
}

static void vfe32_process_zsl_frame(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	struct msm_free_buf *free_buf = NULL;

	ping_pong = msm_io_r(vfe32_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

	/* Thumbnail */
	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
						VFE_MSG_OUTPUT_T);
	if (free_buf) {
		ch0_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0);
		ch1_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1);
		ch2_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch2);

		vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0,
			free_buf->ch_paddr[0]);
		vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1,
			free_buf->ch_paddr[1]);
		if (free_buf->num_planes > 2)
			vfe32_put_ch_addr(ping_pong,
				vfe32_ctrl->outpath.out1.ch2,
				free_buf->ch_paddr[2]);
		vfe_send_outmsg(vfe32_ctrl->subdev,
			MSG_ID_OUTPUT_T, ch0_paddr,
			ch1_paddr, ch2_paddr);
	}

	/* Mainimg */
	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
						VFE_MSG_OUTPUT_S);
	if (free_buf) {
		ch0_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch0);
		ch1_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch1);
		ch2_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch2);
		if (free_buf->num_planes > 2)
			vfe32_put_ch_addr(ping_pong,
				vfe32_ctrl->outpath.out2.ch2,
				free_buf->ch_paddr[2]);

		vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch0,
			free_buf->ch_paddr[0]);
		vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch1,
			free_buf->ch_paddr[1]);
		vfe_send_outmsg(vfe32_ctrl->subdev,
			MSG_ID_OUTPUT_S, ch0_paddr,
			ch1_paddr, ch2_paddr);
	}
}

static void vfe32_process_output_path_irq_1(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	if (vfe32_ctrl->operation_mode == VFE_MODE_OF_OPERATION_ZSL) {
		vfe32_process_zsl_frame();
		return;
	}

	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
						VFE_MSG_OUTPUT_S);

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
		ch0_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		ch1_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1);
		ch2_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch2);

		CDBG("snapshot main, ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe32_put_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out1.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe32_put_ch_addr(ping_pong,
					vfe32_ctrl->outpath.out1.ch2,
					free_buf->ch_paddr[2]);
		}

		if (vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_SNAPSHOT ||
			vfe32_ctrl->operation_mode ==
			VFE_MODE_OF_OPERATION_RAW_SNAPSHOT) {
			vfe32_ctrl->outpath.out1.capture_cnt--;
			vfe_send_outmsg(vfe32_ctrl->subdev,
				MSG_ID_OUTPUT_S, ch0_paddr,
				ch1_paddr, ch2_paddr);
		}
	} else {
		vfe32_ctrl->outpath.out1.frame_drop_cnt++;
		CDBG("path_irq_1 - no free buffer!\n");
	}
}

static void vfe32_process_output_path_irq_2(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	struct msm_free_buf *free_buf = NULL;

	if (vfe32_ctrl->recording_state == VFE_REC_STATE_STOP_REQUESTED) {
		vfe32_ctrl->outpath.out2.frame_drop_cnt++;
		CDBG("%s: path_irq_2 - recording stop requested ", __func__);
		return;
	}

	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
						VFE_MSG_OUTPUT_V);
	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	-- when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/

	CDBG("%s: op mode = %d, capture_cnt = %d\n", __func__,
		 vfe32_ctrl->operation_mode, vfe32_ctrl->vfe_capture_count);

	if (free_buf) {
		ping_pong = msm_io_r(vfe32_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		ch0_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch0);
		/* Chroma channel */
		ch1_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch1);
		ch2_paddr = vfe32_get_ch_addr(ping_pong,
			vfe32_ctrl->outpath.out2.ch2);

		CDBG("video output, ch0 = 0x%x,	ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);

		/* Y channel */
		vfe32_put_ch_addr(ping_pong,
		vfe32_ctrl->outpath.out2.ch0,
		free_buf->ch_paddr[0]);
		/* Chroma channel */
		vfe32_put_ch_addr(ping_pong,
		vfe32_ctrl->outpath.out2.ch1,
		free_buf->ch_paddr[1]);
		if (free_buf->num_planes > 2)
			vfe32_put_ch_addr(ping_pong,
				vfe32_ctrl->outpath.out2.ch2,
				free_buf->ch_paddr[2]);

		vfe_send_outmsg(vfe32_ctrl->subdev,
			MSG_ID_OUTPUT_V, ch0_paddr,
			ch1_paddr, ch2_paddr);

	} else {
		vfe32_ctrl->outpath.out2.frame_drop_cnt++;
		CDBG("path_irq_2 - no free buffer!\n");
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
	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	struct isp_msg_stats msgStats;
	msgStats.frameCounter = vfe32_ctrl->vfeFrameId;
	msgStats.buffer = bufAddress;

	switch (statsNum) {
	case statsAeNum:{
		msgStats.id = MSG_ID_STATS_AEC;
		spin_lock_irqsave(&vfe32_ctrl->aec_ack_lock, flags);
		vfe32_ctrl->aecStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->aec_ack_lock, flags);
		}
		break;
	case statsAfNum:{
		msgStats.id = MSG_ID_STATS_AF;
		spin_lock_irqsave(&vfe32_ctrl->af_ack_lock, flags);
		vfe32_ctrl->afStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->af_ack_lock, flags);
		}
		break;
	case statsAwbNum: {
		msgStats.id = MSG_ID_STATS_AWB;
		spin_lock_irqsave(&vfe32_ctrl->awb_ack_lock, flags);
		vfe32_ctrl->awbStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->awb_ack_lock, flags);
		}
		break;

	case statsIhistNum: {
		msgStats.id = MSG_ID_STATS_IHIST;
		vfe32_ctrl->ihistStatsControl.ackPending = TRUE;
		}
		break;
	case statsRsNum: {
		msgStats.id = MSG_ID_STATS_RS;
		vfe32_ctrl->rsStatsControl.ackPending = TRUE;
		}
		break;
	case statsCsNum: {
		msgStats.id = MSG_ID_STATS_CS;
		vfe32_ctrl->csStatsControl.ackPending = TRUE;
		}
		break;

	default:
		goto stats_done;
	}

	v4l2_subdev_notify(vfe32_ctrl->subdev,
				NOTIFY_VFE_MSG_STATS,
				&msgStats);
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
					v4l2_subdev_notify(vfe32_ctrl->subdev,
						NOTIFY_ISPIF_STREAM, (void *)
						ISPIF_STREAM(PIX0,
						ISPIF_OFF_IMMEDIATELY));
					msm_io_w_mb(
						CAMIF_COMMAND_STOP_IMMEDIATELY,
						vfe32_ctrl->vfebase +
						VFE_CAMIF_COMMAND);
					v4l2_subdev_notify(vfe32_ctrl->subdev,
							NOTIFY_ISP_MSG_EVT,
						(void *)MSG_ID_SNAPSHOT_DONE);
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
					v4l2_subdev_notify(vfe32_ctrl->subdev,
						NOTIFY_ISP_MSG_EVT, (void *)
						MSG_ID_SYNC_TIMER0_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER1) {
					CDBG("SYNC_TIMER 1 irq occured.\n");
					v4l2_subdev_notify(vfe32_ctrl->subdev,
						NOTIFY_ISP_MSG_EVT, (void *)
						MSG_ID_SYNC_TIMER1_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER2) {
					CDBG("SYNC_TIMER 2 irq occured.\n");
					v4l2_subdev_notify(vfe32_ctrl->subdev,
						NOTIFY_ISP_MSG_EVT, (void *)
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
	spin_lock_init(&vfe32_ctrl->sd_notify_lock);
	INIT_LIST_HEAD(&vfe32_ctrl->tasklet_q);

	vfe32_ctrl->syncdata = sdata;
	vfe32_ctrl->vfemem = vfemem;
	vfe32_ctrl->vfeio  = vfeio;
	vfe32_ctrl->update_linear = false;
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
			pr_err("%s %d: copy_from_user failed\n", __func__,
				__LINE__);
			return -EFAULT;
		}
	} else {
	/* here eith stats release or frame release. */
		if (cmd->cmd_type != CMD_CONFIG_PING_ADDR &&
			cmd->cmd_type != CMD_CONFIG_PONG_ADDR &&
			cmd->cmd_type != CMD_CONFIG_FREE_BUF_ADDR) {
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

	case CMD_CONFIG_PING_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe32_output_ch *outch = vfe32_get_ch(path);
		outch->ping = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_PONG_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe32_output_ch *outch = vfe32_get_ch(path);
		outch->pong = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_FREE_BUF_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe32_output_ch *outch = vfe32_get_ch(path);
		outch->free_buf = *((struct msm_free_buf *)data);
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
		uint32_t *axio = NULL;
		axio = kmalloc(vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_2, axio);
		kfree(axio);
	}
		break;

	case CMD_RAW_PICT_AXI_CFG: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(CAMIF_TO_AXI_VIA_OUTPUT_2, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_SNAP: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_1_AND_2, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_ZSL: {
		uint32_t *axio = NULL;
		CDBG("%s, CMD_AXI_CFG_ZSL\n", __func__);
		axio = kmalloc(vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_1_2_AND_3, axio);
		kfree(axio);
	}
		break;

	case CMD_AXI_CFG_VIDEO: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe32_config_axi(OUTPUT_1_AND_3, axio);
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
