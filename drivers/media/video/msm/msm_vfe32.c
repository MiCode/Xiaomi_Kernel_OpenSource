/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>

#include "msm.h"
#include "msm_vfe32.h"

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
static void  *vfe_syncdata;
static uint32_t vfe_clk_rate;

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
/*10*/  {VFE_CMD_MESH_ROLL_OFF_CFG, V32_MESH_ROLL_OFF_CFG_LEN,
		V32_MESH_ROLL_OFF_CFG_OFF, 0xFF},
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
		{VFE_CMD_GET_HW_VERSION, V32_GET_HW_VERSION_LEN,
		V32_GET_HW_VERSION_OFF},
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
		{VFE_CMD_MODULE_CFG, V32_MODULE_CFG_LEN, V32_MODULE_CFG_OFF},
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
/*120*/ {VFE_CMD_PCA_ROLL_OFF_CFG},
		{VFE_CMD_PCA_ROLL_OFF_UPDATE},
		{VFE_CMD_GET_REG_DUMP},
		{VFE_CMD_GET_LINEARIZATON_TABLE},
		{VFE_CMD_GET_MESH_ROLLOFF_TABLE},
/*125*/ {VFE_CMD_GET_PCA_ROLLOFF_TABLE},
		{VFE_CMD_GET_RGB_G_TABLE},
		{VFE_CMD_GET_LA_TABLE},
		{VFE_CMD_DEMOSAICV3_UPDATE},
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
	"PCA_ROLL_OFF_CFG", /*120*/
	"PCA_ROLL_OFF_UPDATE",
	"GET_REG_DUMP",
	"GET_LINEARIZATON_TABLE",
	"GET_MESH_ROLLOFF_TABLE",
	"GET_PCA_ROLLOFF_TABLE", /*125*/
	"GET_RGB_G_TABLE",
	"GET_LA_TABLE",
	"DEMOSAICV3_UPDATE",
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
	 * at any time. stop camif immediately. */
	msm_io_w(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);

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
	v4l2_subdev_notify(&vfe32_ctrl->subdev, NOTIFY_VFE_BUF_EVT, rp);
	spin_unlock_irqrestore(&vfe32_ctrl->sd_notify_lock, flags);
}

static int vfe32_config_axi(int mode, uint32_t *ao)
{
	uint32_t *ch_info;
	uint32_t *axi_cfg = ao+V32_AXI_BUS_FMT_OFF;

	/* Update the corresponding write masters for each output*/
	ch_info = axi_cfg + V32_AXI_CFG_LEN;
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
	case OUTPUT_PRIM:
		vfe32_ctrl->outpath.output_mode =
			VFE32_OUTPUT_MODE_PRIMARY;
		break;
	case OUTPUT_PRIM_ALL_CHNLS:
		vfe32_ctrl->outpath.output_mode =
			VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC:
		vfe32_ctrl->outpath.output_mode =
			VFE32_OUTPUT_MODE_PRIMARY;
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_SECONDARY;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS:
		vfe32_ctrl->outpath.output_mode =
			VFE32_OUTPUT_MODE_PRIMARY;
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC:
		vfe32_ctrl->outpath.output_mode =
			VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		vfe32_ctrl->outpath.output_mode |=
			VFE32_OUTPUT_MODE_SECONDARY;
		break;
	default:
		pr_err("%s Invalid AXI mode %d ", __func__, mode);
		return -EINVAL;
	}
	msm_io_w(*ao, vfe32_ctrl->vfebase +
		VFE_BUS_IO_FORMAT_CFG);
	msm_io_memcpy(vfe32_ctrl->vfebase +
		vfe32_cmd[VFE_CMD_AXI_OUT_CFG].offset, axi_cfg,
		vfe32_cmd[VFE_CMD_AXI_OUT_CFG].length - V32_AXI_CH_INF_LEN
		- V32_AXI_BUS_FMT_LEN);
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

	vfe32_ctrl->recording_state = VFE_STATE_IDLE;
	vfe32_ctrl->liveshot_state = VFE_STATE_IDLE;

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

	vfe32_ctrl->frame_skip_cnt = 31;
	vfe32_ctrl->frame_skip_pattern = 0xffffffff;
	vfe32_ctrl->snapshot_frame_cnt = 0;
}

static void vfe32_reset(void)
{
	vfe32_reset_internal_variables();
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
	vfe32_ctrl->hfr_mode = *(++p);

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
	uint32_t irq_mask = 0x00E00021;
	vfe32_ctrl->start_ack_pending = TRUE;
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe32_ctrl->operation_mode, vfe32_ctrl->outpath.output_mode);
	if (vfe32_ctrl->stats_comp)
		irq_mask |= VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK;
	else
		irq_mask |= 0x000FE000;

	msm_io_w(irq_mask, vfe32_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe32_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);


	atomic_set(&vfe32_ctrl->vstate, 1);
}

static int vfe32_start_recording(void)
{
	struct msm_sync *sync = vfe_syncdata;
	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_VIDEO);
	vfe32_ctrl->recording_state = VFE_STATE_START_REQUESTED;
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe32_stop_recording(void)
{
	struct msm_sync *sync = vfe_syncdata;
	vfe32_ctrl->recording_state = VFE_STATE_STOP_REQUESTED;
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	return 0;
}

static void vfe32_start_liveshot(void){
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;
	if (p_sync)
		p_sync->liveshot_enabled = true;

	/* Hardcode 1 live snapshot for now. */
	vfe32_ctrl->outpath.out0.capture_cnt = 1;
	vfe32_ctrl->vfe_capture_count = vfe32_ctrl->outpath.out0.capture_cnt;

	vfe32_ctrl->liveshot_state = VFE_STATE_START_REQUESTED;
	msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static int vfe32_zsl(void)
{
	struct msm_sync *sync = vfe_syncdata;
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	CDBG("%s:op mode %d O/P Mode %d\n", __func__,
		vfe32_ctrl->operation_mode, vfe32_ctrl->outpath.output_mode);

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= ((0x1 << (vfe32_ctrl->outpath.out0.ch0)) |
				(0x1 << (vfe32_ctrl->outpath.out0.ch1)));
	} else if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= ((0x1 << (vfe32_ctrl->outpath.out0.ch0)) |
				(0x1 << (vfe32_ctrl->outpath.out0.ch1)) |
				(0x1 << (vfe32_ctrl->outpath.out0.ch2)));
	}

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= ((0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8)) |
				(0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8)));
	} else if (vfe32_ctrl->outpath.output_mode &
			   VFE32_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= ((0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8)) |
				(0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8)) |
				(0x1 << (vfe32_ctrl->outpath.out1.ch2 + 8)));
	}

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PRIMARY) {
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
	} else if (vfe32_ctrl->outpath.output_mode &
				VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch2]);
	}

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_SECONDARY) {
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
	} else if (vfe32_ctrl->outpath.output_mode &
				VFE32_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch2]);
	}

	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe32_start_common();
	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_ZSL);

	msm_io_w(1, vfe32_ctrl->vfebase + 0x18C);
	msm_io_w(1, vfe32_ctrl->vfebase + 0x188);
	return 0;
}
static int vfe32_capture_raw(uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;
	struct msm_sync* p_sync = (struct msm_sync *)vfe_syncdata;

	vfe32_ctrl->outpath.out0.capture_cnt = num_frames_capture;
	vfe32_ctrl->vfe_capture_count = num_frames_capture;

	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (0x1 << (vfe32_ctrl->outpath.out0.ch0));
		msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
	}

	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		p_sync->sdata->pdata->cam_bus_scale_table, S_CAPTURE);
	vfe32_start_common();
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
	if (vfe32_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) {
		vfe32_ctrl->outpath.out0.capture_cnt =
			num_frames_capture;
	}

	vfe32_ctrl->vfe_capture_count = num_frames_capture;
	irq_comp_mask	= msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN) {
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_PRIMARY) {
			irq_comp_mask |= (0x1 << vfe32_ctrl->outpath.out0.ch0 |
					0x1 << vfe32_ctrl->outpath.out0.ch1);
		}
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_SECONDARY) {
			irq_comp_mask |=
				(0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8) |
				0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8));
		}
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_PRIMARY) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		}
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_SECONDARY) {
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		}
	}

	vfe32_ctrl->vfe_capture_count = num_frames_capture;

	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		p_sync->sdata->pdata->cam_bus_scale_table, S_CAPTURE);

	vfe32_start_common();
	/* for debug */
	msm_io_w(1, vfe32_ctrl->vfebase + 0x18C);
	msm_io_w(1, vfe32_ctrl->vfebase + 0x188);
	return 0;
}

static int vfe32_start(void)
{
	uint32_t irq_comp_mask = 0;
	struct msm_sync *sync = vfe_syncdata;

	irq_comp_mask	=
		msm_io_r(vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (0x1 << vfe32_ctrl->outpath.out0.ch0 |
			0x1 << vfe32_ctrl->outpath.out0.ch1);
	} else if (vfe32_ctrl->outpath.output_mode &
			   VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= (0x1 << vfe32_ctrl->outpath.out0.ch0 |
			0x1 << vfe32_ctrl->outpath.out0.ch1 |
			0x1 << vfe32_ctrl->outpath.out0.ch2);
	}
	if (vfe32_ctrl->outpath.output_mode & VFE32_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= (0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8));
	} else if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= (0x1 << (vfe32_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe32_ctrl->outpath.out1.ch1 + 8) |
			0x1 << (vfe32_ctrl->outpath.out1.ch2 + 8));
	}
	msm_io_w(irq_comp_mask, vfe32_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe32_ctrl->operation_mode == VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_PRIMARY) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		} else if (vfe32_ctrl->outpath.output_mode &
				VFE32_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch2]);
		}
	} else {
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_SECONDARY) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		} else if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch2]);
		}
	}
	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	vfe32_start_common();
	return 0;
}

static void vfe32_update(void)
{
	unsigned long flags;
	uint32_t value = 0;
	if (vfe32_ctrl->update_linear) {
		if (!msm_io_r(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1))
			msm_io_w(1,
				vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1);
		else
			msm_io_w(0,
				vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1);
		vfe32_ctrl->update_linear = false;
	}

	if (vfe32_ctrl->update_rolloff) {
		value = msm_io_r(vfe32_ctrl->vfebase +
			V33_PCA_ROLL_OFF_CFG_OFF1);
		value ^= V33_PCA_ROLL_OFF_LUT_BANK_SEL_MASK;
		msm_io_w(value, vfe32_ctrl->vfebase +
			V33_PCA_ROLL_OFF_CFG_OFF1);
		vfe32_ctrl->update_rolloff = false;
	}

	if (vfe32_ctrl->update_la) {
		if (!msm_io_r(vfe32_ctrl->vfebase + V32_LA_OFF))
			msm_io_w(1,
				vfe32_ctrl->vfebase + V32_LA_OFF);
		else
			msm_io_w(0,
				vfe32_ctrl->vfebase + V32_LA_OFF);
		vfe32_ctrl->update_la = false;
	}

	if (vfe32_ctrl->update_gamma) {
		value = msm_io_r(vfe32_ctrl->vfebase + V32_RGB_G_OFF);
		value ^= V32_GAMMA_LUT_BANK_SEL_MASK;
		msm_io_w(value, vfe32_ctrl->vfebase + V32_RGB_G_OFF);
		vfe32_ctrl->update_gamma = false;
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
	val = vfe_clk_rate / 10000;
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
	CDBG("%s: banksel = %d\n", __func__, bankSel);

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
	for (i = 0 ; i < (VFE32_GAMMA_NUM_ENTRIES/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_io_w((value1), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_io_w((value2), vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe32_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe32_read_gamma_cfg(enum VFE32_DMI_RAM_SEL channel_sel,
	uint32_t *tbl)
{
	int i;
	vfe32_program_dmi_cfg(channel_sel);
	CDBG("%s: Gamma table channel: %d\n", __func__, channel_sel);
	for (i = 0 ; i < VFE32_GAMMA_NUM_ENTRIES ; i++) {
		*tbl = msm_io_r(vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
		CDBG("%s: %08x\n", __func__, *tbl);
		tbl++;
	}
	vfe32_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe32_write_la_cfg(enum VFE32_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	uint32_t i;
	uint32_t value, value1, value2;

	vfe32_program_dmi_cfg(channel_sel);
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

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		ch = &vfe32_ctrl->outpath.out0;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		ch = &vfe32_ctrl->outpath.out1;
	else
		pr_err("%s: Invalid path %d\n", __func__,
			path);

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
		vfe32_put_ch_pong_addr(outch->ch0,
			outch->pong.ch_paddr[0]);

		if (vfe32_ctrl->operation_mode !=
			VFE_OUTPUTS_RAW) {
			vfe32_put_ch_ping_addr(outch->ch1,
				outch->ping.ch_paddr[1]);
			vfe32_put_ch_pong_addr(outch->ch1,
				outch->pong.ch_paddr[1]);
		}

		if (outch->ping.num_planes > 2)
			vfe32_put_ch_ping_addr(outch->ch2,
				outch->ping.ch_paddr[2]);
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

static void vfe32_send_isp_msg(
	struct vfe32_ctrl_type *vctrl,
	uint32_t isp_msg_id)
{
	struct isp_msg_event isp_msg_evt;

	isp_msg_evt.msg_id = isp_msg_id;
	isp_msg_evt.sof_count = vfe32_ctrl->vfeFrameId;
	v4l2_subdev_notify(&vctrl->subdev,
			NOTIFY_ISP_MSG_EVT,
			(void *)&isp_msg_evt);
}

static int vfe32_proc_general(struct msm_isp_cmd *cmd)
{
	int i , rc = 0;
	uint32_t old_val = 0 , new_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;
	uint32_t temp1 = 0, temp2 = 0;

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
		if (vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO)
			/* Configure primary channel */
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_START, VFE_MSG_OUTPUT_PRIMARY);
		else
			/* Configure secondary channel */
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_START, VFE_MSG_OUTPUT_SECONDARY);
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
	case VFE_CMD_CAPTURE_RAW:
		pr_info("%s: cmdID = VFE_CMD_CAPTURE_RAW\n", __func__);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_CAPTURE,
							VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				   " for snapshot", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe32_capture_raw(snapshot_cnt);
		break;
	case VFE_CMD_CAPTURE:
		CDBG("vfe32_proc_general: cmdID = %s op mode = %d\n",
			vfe32_general_cmd[cmd->id], vfe32_ctrl->operation_mode);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
				sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}

		if (vfe32_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) {
			if (snapshot_cnt != 1) {
				pr_err("only support 1 inline snapshot\n");
				rc = -EINVAL;
				goto proc_general_done;
			}
			/* Configure primary channel for JPEG */
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_JPEG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY);
		} else {
			/* Configure primary channel */
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY);
		}
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				   " for primary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		/* Configure secondary channel */
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_CAPTURE,
						  VFE_MSG_OUTPUT_SECONDARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				   " for secondary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe32_capture(snapshot_cnt);
		break;
	case VFE_CMD_START_RECORDING:
		pr_info("vfe32_proc_general: cmdID = %s\n",
			vfe32_general_cmd[cmd->id]);
		if (vfe32_ctrl->operation_mode ==
			VFE_OUTPUTS_PREVIEW_AND_VIDEO)
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_START_RECORDING,
				VFE_MSG_OUTPUT_SECONDARY);
		else if (vfe32_ctrl->operation_mode ==
			VFE_OUTPUTS_VIDEO_AND_PREVIEW)
			rc = vfe32_configure_pingpong_buffers(
				VFE_MSG_V32_START_RECORDING,
				VFE_MSG_OUTPUT_PRIMARY);
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
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
				cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_MCE_UPDATE:
	case VFE_CMD_MCE_CFG:{
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		/* Incrementing with 4 so as to point to the 2nd Register as
		the 2nd register has the mce_enable bit */
		old_val = msm_io_r(vfe32_ctrl->vfebase +
			V32_CHROMA_SUP_OFF + 4);
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_SUP_OFF + 4,
			&new_val, 4);
		cmdp_local += 1;

		old_val = msm_io_r(vfe32_ctrl->vfebase +
			V32_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_SUP_OFF + 8,
		&new_val, 4);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
		cmdp_local, (vfe32_cmd[cmd->id].length));
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_SUP_OFF,
			cmdp_local, 4);

		cmdp_local += 1;
		new_val = *cmdp_local;
		/* Incrementing with 4 so as to point to the 2nd Register as
		 * the 2nd register has the mce_enable bit
		 */
		old_val = msm_io_r(vfe32_ctrl->vfebase +
			V32_CHROMA_SUP_OFF + 4);
		old_val &= ~MCE_EN_MASK;
		new_val = new_val | old_val;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_SUP_OFF + 4,
			&new_val, 4);
		cmdp_local += 1;

		old_val = msm_io_r(vfe32_ctrl->vfebase +
			V32_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= ~MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_CHROMA_SUP_OFF + 8,
			&new_val, 4);
		}
		break;
	case VFE_CMD_BLACK_LEVEL_CFG:
		rc = -EFAULT;
		goto proc_general_done;

	case VFE_CMD_MESH_ROLL_OFF_CFG: {
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
		vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK0);
		/* for loop for extrcting init table. */
		for (i = 0; i < (V32_MESH_ROLL_OFF_INIT_TABLE_SIZE * 2); i++) {
			msm_io_w(*cmdp_local ,
			vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		CDBG("done writing init table\n");
		/* by default, always starts with offset 0. */
		msm_io_w(V32_MESH_ROLL_OFF_DELTA_TABLE_OFFSET,
		vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		/* for loop for extracting delta table. */
		for (i = 0; i < (V32_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2); i++) {
			msm_io_w(*cmdp_local,
			vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		}
		break;

	case VFE_CMD_GET_MESH_ROLLOFF_TABLE:
		temp1 = sizeof(uint32_t) * ((V32_MESH_ROLL_OFF_INIT_TABLE_SIZE *
			2) + (V32_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2));
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
		vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK0);
		CDBG("%s: Mesh Rolloff init Table\n", __func__);
		for (i = 0; i < (V32_MESH_ROLL_OFF_INIT_TABLE_SIZE * 2); i++) {
			*cmdp_local =
				msm_io_r(vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		msm_io_w(V32_MESH_ROLL_OFF_DELTA_TABLE_OFFSET,
			vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		CDBG("%s: Mesh Rolloff Delta Table\n", __func__);
		for (i = 0; i < (V32_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2); i++) {
			*cmdp_local =
				msm_io_r(vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		CDBG("done reading delta table\n");
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
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
		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp_local, (vfe32_cmd[cmd->id].length));

		cmdp_local += 1;
		vfe32_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0, cmdp_local);
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_LA_OFF);
		if (old_val != 0x0)
			vfe32_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0,
				cmdp_local);
		else
			vfe32_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK1,
				cmdp_local);
		}
		vfe32_ctrl->update_la = true;
		break;

	case VFE_CMD_GET_LA_TABLE:
		temp1 = sizeof(uint32_t) * VFE32_LA_TABLE_LENGTH / 2;
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
		if (msm_io_r(vfe32_ctrl->vfebase + V32_LA_OFF))
			vfe32_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK1);
		else
			vfe32_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK0);
		for (i = 0 ; i < (VFE32_LA_TABLE_LENGTH / 2) ; i++) {
			*cmdp_local =
				msm_io_r(vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			*cmdp_local |= (msm_io_r(vfe32_ctrl->vfebase +
				VFE_DMI_DATA_LO)) << 16;
			cmdp_local++;
		}
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_SCE_OFF,
				cmdp, V32_SCE_LEN);
		}
		break;

	case VFE_CMD_LIVESHOT:
		/* Configure primary channel */
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_CAPTURE,
						VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				   " for primary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		vfe32_start_liveshot();
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
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1 + 4,
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

	case VFE_CMD_GET_LINEARIZATON_TABLE:
		temp1 = sizeof(uint32_t) * VFE32_LINEARIZATON_TABLE_LENGTH;
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
		if (msm_io_r(vfe32_ctrl->vfebase + V32_LINEARIZATION_OFF1))
			vfe32_program_dmi_cfg(BLACK_LUT_RAM_BANK1);
		else
			vfe32_program_dmi_cfg(BLACK_LUT_RAM_BANK0);
		CDBG("%s: Linearization Table\n", __func__);
		for (i = 0 ; i < VFE32_LINEARIZATON_TABLE_LENGTH ; i++) {
			*cmdp_local =
				msm_io_r(vfe32_ctrl->vfebase + VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
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
		new_val = *cmdp_local;

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF,
			cmdp_local, V32_DEMOSAICV3_0_LEN);
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_1_OFF,
			cmdp_local, V32_DEMOSAICV3_1_LEN);
		break;

	case VFE_CMD_DEMOSAICV3_UPDATE:
		if (cmd->length !=
			V32_DEMOSAICV3_0_LEN * V32_DEMOSAICV3_UP_REG_CNT) {
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_0_OFF,
			cmdp_local, V32_DEMOSAICV3_0_LEN);
		/* As the address space is not contiguous increment by 2
		 * before copying to next address space */
		cmdp_local += 1;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_1_OFF,
			cmdp_local, 2 * V32_DEMOSAICV3_0_LEN);
		/* As the address space is not contiguous increment by 2
		 * before copying to next address space */
		cmdp_local += 2;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_DEMOSAICV3_2_OFF,
			cmdp_local, 2 * V32_DEMOSAICV3_0_LEN);
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

		vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0, cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0, cmdp);
		vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0, cmdp);
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_RGB_G_OFF);
		cmdp += 1;
		if (old_val != 0x0) {
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0, cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0, cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0, cmdp);
		} else {
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH0_BANK1, cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH1_BANK1, cmdp);
			vfe32_write_gamma_cfg(RGBLUT_RAM_CH2_BANK1, cmdp);
		}
		}
		vfe32_ctrl->update_gamma = TRUE;
		cmdp -= 1;
		break;

	case VFE_CMD_GET_RGB_G_TABLE:
		temp1 = sizeof(uint32_t) * VFE32_GAMMA_NUM_ENTRIES * 3;
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

		old_val = msm_io_r(vfe32_ctrl->vfebase + V32_RGB_G_OFF);
		temp2 = old_val ? RGBLUT_RAM_CH0_BANK1 :
			RGBLUT_RAM_CH0_BANK0;
		for (i = 0; i < 3; i++) {
			vfe32_read_gamma_cfg(temp2,
				cmdp_local + (VFE32_GAMMA_NUM_ENTRIES * i));
			temp2 += 2;
		}
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
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
		old_val = msm_io_r(vfe32_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= STATS_ENABLE_MASK;
		*cmdp |= old_val;

		msm_io_memcpy(vfe32_ctrl->vfebase + vfe32_cmd[cmd->id].offset,
			cmdp, (vfe32_cmd[cmd->id].length));
		}
		break;

	case VFE_CMD_ZSL:
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
			VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0)
			goto proc_general_done;
		rc = vfe32_configure_pingpong_buffers(VFE_MSG_V32_START,
			VFE_MSG_OUTPUT_SECONDARY);
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
		cmdp_local = cmdp + V32_ASF_LEN/4;
		msm_io_memcpy(vfe32_ctrl->vfebase + V32_ASF_SPECIAL_EFX_CFG_OFF,
			cmdp_local, V32_ASF_SPECIAL_EFX_CFG_LEN);
		break;

	case VFE_CMD_PCA_ROLL_OFF_CFG:
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

		temp1 = *cmdp_local;
		cmdp_local++;

		msm_io_memcpy(vfe32_ctrl->vfebase + V33_PCA_ROLL_OFF_CFG_OFF1,
			cmdp_local, V33_PCA_ROLL_OFF_CFG_LEN1);
		cmdp_local += 4;
		msm_io_memcpy(vfe32_ctrl->vfebase + V33_PCA_ROLL_OFF_CFG_OFF2,
			cmdp_local, V33_PCA_ROLL_OFF_CFG_LEN2);

		cmdp_local += 3;
		CDBG("%s: start writing RollOff Ram0 table\n", __func__);
		vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK0);
		msm_io_w(temp1, vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		for (i = 0 ; i < V33_PCA_ROLL_OFF_TABLE_SIZE ; i++) {
			msm_io_w(*(cmdp_local + 1),
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_HI);
			msm_io_w(*cmdp_local,
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_LO);
			cmdp_local += 2;
		}
		CDBG("%s: end writing RollOff Ram0 table\n", __func__);

		CDBG("%s: start writing RollOff Ram1 table\n", __func__);
		vfe32_program_dmi_cfg(ROLLOFF_RAM1_BANK0);
		msm_io_w(temp1, vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		for (i = 0 ; i < V33_PCA_ROLL_OFF_TABLE_SIZE ; i++) {
			msm_io_w(*cmdp_local,
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_LO);
			cmdp_local += 2;
		}
		CDBG("%s: end writing RollOff Ram1 table\n", __func__);

		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		break;

	case VFE_CMD_PCA_ROLL_OFF_UPDATE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value), cmd->length)) {
			rc = -EFAULT;
			goto proc_general_done;
		}

		cmdp_local = cmdp;

		temp1 = *cmdp_local;
		cmdp_local += 8;

		temp2 = msm_io_r(vfe32_ctrl->vfebase +
			V33_PCA_ROLL_OFF_CFG_OFF1)
			& V33_PCA_ROLL_OFF_LUT_BANK_SEL_MASK;

		CDBG("%s: start writing RollOff Ram0 table\n", __func__);
		if (temp2)
			vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK0);
		else
			vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK1);

		msm_io_w(temp1, vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		for (i = 0 ; i < V33_PCA_ROLL_OFF_TABLE_SIZE ; i++) {
			msm_io_w(*(cmdp_local + 1),
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_HI);
			msm_io_w(*cmdp_local,
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_LO);
			cmdp_local += 2;
		}
		CDBG("%s: end writing RollOff Ram0 table\n", __func__);

		CDBG("%s: start writing RollOff Ram1 table\n", __func__);
		if (temp2)
			vfe32_program_dmi_cfg(ROLLOFF_RAM1_BANK0);
		else
			vfe32_program_dmi_cfg(ROLLOFF_RAM1_BANK1);

		msm_io_w(temp1, vfe32_ctrl->vfebase + VFE_DMI_ADDR);
		for (i = 0 ; i < V33_PCA_ROLL_OFF_TABLE_SIZE ; i++) {
			msm_io_w(*cmdp_local,
				vfe32_ctrl->vfebase + VFE33_DMI_DATA_LO);
			cmdp_local += 2;
		}
		CDBG("%s: end writing RollOff Ram1 table\n", __func__);

		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		vfe32_ctrl->update_rolloff = true;
		break;
	case VFE_CMD_GET_PCA_ROLLOFF_TABLE:
		temp1 = sizeof(uint64_t) * V33_PCA_ROLL_OFF_TABLE_SIZE * 2;
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
		old_val = msm_io_r(vfe32_ctrl->vfebase +
			V33_PCA_ROLL_OFF_CFG_OFF1) &
			V33_PCA_ROLL_OFF_LUT_BANK_SEL_MASK;

		if (old_val)
			vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK1);
		else
			vfe32_program_dmi_cfg(ROLLOFF_RAM0_BANK0);

		CDBG("%s: PCA Rolloff Ram0\n", __func__);
		for (i = 0 ; i < V33_PCA_ROLL_OFF_TABLE_SIZE * 2; i++) {
			temp2 = (i == (V33_PCA_ROLL_OFF_TABLE_SIZE));
			if (old_val && temp2)
				vfe32_program_dmi_cfg(ROLLOFF_RAM1_BANK1);
			else if (!old_val && temp2)
				vfe32_program_dmi_cfg(ROLLOFF_RAM1_BANK0);

			*cmdp_local = msm_io_r(vfe32_ctrl->vfebase +
				VFE33_DMI_DATA_LO);
			*(cmdp_local + 1) =
				msm_io_r(vfe32_ctrl->vfebase +
				VFE33_DMI_DATA_HI);
			CDBG("%s: %08x%08x\n", __func__,
				*(cmdp_local + 1), *cmdp_local);
			cmdp_local += 2;
		}
		vfe32_program_dmi_cfg(NO_MEM_SELECTED);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_GET_HW_VERSION:
		if (cmd->length != V32_GET_HW_VERSION_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V32_GET_HW_VERSION_LEN, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		*cmdp = msm_io_r(vfe32_ctrl->vfebase+V32_GET_HW_VERSION_OFF);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			V32_GET_HW_VERSION_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_GET_REG_DUMP:
		temp1 = sizeof(uint32_t) * vfe32_ctrl->register_total;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(temp1, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		msm_io_dump(vfe32_ctrl->vfebase, vfe32_ctrl->register_total*4);
		CDBG("%s: %p %p %d\n", __func__, (void *)cmdp,
			vfe32_ctrl->vfebase, temp1);
		memcpy_fromio((void *)cmdp, vfe32_ctrl->vfebase, temp1);
		if (copy_to_user((void __user *)(cmd->value), cmdp, temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_FRAME_SKIP_CFG:
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
		vfe32_ctrl->frame_skip_cnt = ((uint32_t)
			*cmdp & VFE_FRAME_SKIP_PERIOD_MASK) + 1;
		vfe32_ctrl->frame_skip_pattern = (uint32_t)(*(cmdp + 2));
		break;
	default:
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
		break;

	}

proc_general_done:
	kfree(cmdp);

	return rc;
}

static void vfe32_stats_af_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->af_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->afStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->afStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe32_stats_awb_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->awb_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->awbStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->awbStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe32_stats_aec_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->aec_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->aecStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->aecStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe32_stats_ihist_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->ihist_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->ihistStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->ihistStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}
static void vfe32_stats_rs_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->rs_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->rsStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->rsStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}
static void vfe32_stats_cs_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe32_ctrl->stats_comp ?
		&vfe32_ctrl->comp_stats_ack_lock :
		&vfe32_ctrl->cs_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe32_ctrl->csStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe32_ctrl->csStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
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
	unsigned long flags;

	if (vfe32_ctrl->recording_state == VFE_STATE_START_REQUESTED) {
		if (vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		} else if (vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		}
		vfe32_ctrl->recording_state = VFE_STATE_STARTED;
		msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
	} else if (vfe32_ctrl->recording_state ==
			VFE_STATE_STOP_REQUESTED) {
		if (vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_io_w(0, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(0, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
		} else if (vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_io_w(0, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch0]);
			msm_io_w(0, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out1.ch1]);
		}
		CDBG("stop video triggered .\n");
	}

	if (vfe32_ctrl->start_ack_pending == TRUE) {
		vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_START_ACK);
		vfe32_ctrl->start_ack_pending = FALSE;
	} else {
		if (vfe32_ctrl->recording_state ==
				VFE_STATE_STOP_REQUESTED) {
			vfe32_ctrl->recording_state = VFE_STATE_STOPPED;
			/* request a reg update and send STOP_REC_ACK
			 * when we process the next reg update irq.
			 */
			msm_io_w_mb(1,
			vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		} else if (vfe32_ctrl->recording_state ==
					VFE_STATE_STOPPED) {
			vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_STOP_REC_ACK);
			vfe32_ctrl->recording_state = VFE_STATE_IDLE;
		}
		spin_lock_irqsave(&vfe32_ctrl->update_ack_lock, flags);
		if (vfe32_ctrl->update_ack_pending == TRUE) {
			vfe32_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe32_ctrl->update_ack_lock, flags);
			vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
				&vfe32_ctrl->update_ack_lock, flags);
		}
	}

	if (vfe32_ctrl->liveshot_state == VFE_STATE_START_REQUESTED) {
		pr_info("%s enabling liveshot output\n", __func__);
		if (vfe32_ctrl->outpath.output_mode &
				VFE32_OUTPUT_MODE_PRIMARY) {
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(1, vfe32_ctrl->vfebase +
			vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
			vfe32_ctrl->liveshot_state = VFE_STATE_STARTED;
		}
	}

	if (vfe32_ctrl->liveshot_state == VFE_STATE_STARTED) {
		vfe32_ctrl->vfe_capture_count--;
		if (!vfe32_ctrl->vfe_capture_count)
			vfe32_ctrl->liveshot_state = VFE_STATE_STOP_REQUESTED;
		msm_io_w_mb(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} else if (vfe32_ctrl->liveshot_state == VFE_STATE_STOP_REQUESTED) {
		CDBG("%s: disabling liveshot output\n", __func__);
		if (vfe32_ctrl->outpath.output_mode &
			VFE32_OUTPUT_MODE_PRIMARY) {
			msm_io_w(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch0]);
			msm_io_w(0, vfe32_ctrl->vfebase +
				vfe32_AXI_WM_CFG[vfe32_ctrl->outpath.out0.ch1]);
			vfe32_ctrl->liveshot_state = VFE_STATE_STOPPED;
			msm_io_w_mb(1, vfe32_ctrl->vfebase +
				VFE_REG_UPDATE_CMD);
		}
	} else if (vfe32_ctrl->liveshot_state == VFE_STATE_STOPPED) {
		vfe32_ctrl->liveshot_state = VFE_STATE_IDLE;
	}

	if ((vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN) ||
		(vfe32_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB) ||
		(vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) ||
		(vfe32_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB)) {
		/* in snapshot mode */
		/* later we need to add check for live snapshot mode. */
		if (vfe32_ctrl->frame_skip_pattern & (0x1 <<
			(vfe32_ctrl->snapshot_frame_cnt %
				vfe32_ctrl->frame_skip_cnt))) {
			vfe32_ctrl->vfe_capture_count--;
			/* if last frame to be captured: */
			if (vfe32_ctrl->vfe_capture_count == 0) {
				/* stop the bus output:write master enable = 0*/
				if (vfe32_ctrl->outpath.output_mode &
						VFE32_OUTPUT_MODE_PRIMARY) {
					msm_io_w(0, vfe32_ctrl->vfebase +
						vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out0.ch0]);
					msm_io_w(0, vfe32_ctrl->vfebase +
						vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out0.ch1]);
				}
				if (vfe32_ctrl->outpath.output_mode &
						VFE32_OUTPUT_MODE_SECONDARY) {
					msm_io_w(0, vfe32_ctrl->vfebase +
						vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out1.ch0]);
					msm_io_w(0, vfe32_ctrl->vfebase +
						vfe32_AXI_WM_CFG[vfe32_ctrl->
							outpath.out1.ch1]);
				}
				msm_io_w_mb
				(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
				vfe32_ctrl->snapshot_frame_cnt = -1;
				vfe32_ctrl->frame_skip_cnt = 31;
				vfe32_ctrl->frame_skip_pattern = 0xffffffff;
			} /*if snapshot count is 0*/
		} /*if frame is not being dropped*/
		vfe32_ctrl->snapshot_frame_cnt++;
		/* then do reg_update. */
		msm_io_w(1, vfe32_ctrl->vfebase + VFE_REG_UPDATE_CMD);
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
		vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(&vfe32_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe32_set_default_reg_values();

		/* reload all write masters. (frame & line)*/
		msm_io_w(0x7FFF, vfe32_ctrl->vfebase + VFE_BUS_CMD);
		vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_RESET_ACK);
	}
}

static void vfe32_process_camif_sof_irq(void)
{
	if (vfe32_ctrl->operation_mode ==
		VFE_OUTPUTS_RAW) {
		if (vfe32_ctrl->start_ack_pending) {
			vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_START_ACK);
			vfe32_ctrl->start_ack_pending = FALSE;
		}
		vfe32_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe32_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			 to the command register using the barrier */
			msm_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe32_ctrl->vfebase + VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */
	if ((vfe32_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe32_ctrl->operation_mode == VFE_MODE_OF_OPERATION_VIDEO) &&
		(vfe32_ctrl->vfeFrameId % vfe32_ctrl->hfr_mode != 0)) {
		vfe32_ctrl->vfeFrameId++;
		CDBG("Skip the SOF notification when HFR enabled\n");
		return;
	}
	vfe32_ctrl->vfeFrameId++;
	vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_SOF_ACK);
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
	uint32_t reg_value;

	if (errStatus & VFE32_IMASK_CAMIF_ERROR) {
		pr_err("vfe32_irq: camif errors\n");
		reg_value = msm_io_r(vfe32_ctrl->vfebase + VFE_CAMIF_STATUS);
		pr_err("camifStatus  = 0x%x\n", reg_value);
		vfe32_send_isp_msg(vfe32_ctrl, MSG_ID_CAMIF_ERROR);
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

	if (errStatus & VFE32_IMASK_VIOLATION) {
		pr_err("vfe32_irq: violation interrupt\n");
		reg_value =
			msm_io_r(vfe32_ctrl->vfebase + VFE_VIOLATION_STATUS);
		pr_err("%s: violationStatus  = 0x%x\n", __func__, reg_value);
	}

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

	v4l2_subdev_notify(&vfe32_ctrl->subdev,
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

	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_PRIMARY);

	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool = ((vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe32_ctrl->operation_mode == VFE_OUTPUTS_RAW ||
		vfe32_ctrl->liveshot_state == VFE_STATE_STARTED ||
		vfe32_ctrl->liveshot_state == VFE_STATE_STOP_REQUESTED ||
		vfe32_ctrl->liveshot_state == VFE_STATE_STOPPED) &&
		(vfe32_ctrl->vfe_capture_count <= 1)) || free_buf;

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
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_JPEG ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			vfe32_ctrl->liveshot_state == VFE_STATE_STOPPED)
			vfe32_ctrl->outpath.out0.capture_cnt--;

		vfe_send_outmsg(&vfe32_ctrl->subdev,
			MSG_ID_OUTPUT_PRIMARY, ch0_paddr,
			ch1_paddr, ch2_paddr);

		if (vfe32_ctrl->liveshot_state == VFE_STATE_STOPPED)
			vfe32_ctrl->liveshot_state = VFE_STATE_IDLE;

	} else {
		vfe32_ctrl->outpath.out0.frame_drop_cnt++;
		CDBG("path_irq_0 - no free buffer!\n");
	}
}

static void vfe32_process_output_path_irq_1(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe32_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_SECONDARY);

	out_bool = ((vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW) &&
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

		pr_debug("%s ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			__func__, ch0_paddr, ch1_paddr, ch2_paddr);
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
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe32_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW)
			vfe32_ctrl->outpath.out1.capture_cnt--;

		vfe_send_outmsg(&vfe32_ctrl->subdev,
			MSG_ID_OUTPUT_SECONDARY, ch0_paddr,
			ch1_paddr, ch2_paddr);
	} else {
		vfe32_ctrl->outpath.out1.frame_drop_cnt++;
		CDBG("path_irq_1 - no free buffer!\n");
	}
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
		spin_lock_irqsave(&vfe32_ctrl->ihist_ack_lock, flags);
		vfe32_ctrl->ihistStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->ihist_ack_lock, flags);
		}
		break;
	case statsRsNum: {
		msgStats.id = MSG_ID_STATS_RS;
		spin_lock_irqsave(&vfe32_ctrl->rs_ack_lock, flags);
		vfe32_ctrl->rsStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->rs_ack_lock, flags);
		}
		break;
	case statsCsNum: {
		msgStats.id = MSG_ID_STATS_CS;
		spin_lock_irqsave(&vfe32_ctrl->cs_ack_lock, flags);
		vfe32_ctrl->csStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe32_ctrl->cs_ack_lock, flags);
		}
		break;

	default:
		goto stats_done;
	}

	v4l2_subdev_notify(&vfe32_ctrl->subdev,
				NOTIFY_VFE_MSG_STATS,
				&msgStats);
stats_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_send_comp_stats_msg(uint32_t status_bits)
{
	struct msm_stats_buf msgStats;
	uint32_t temp;

	msgStats.frame_id = vfe32_ctrl->vfeFrameId;
	msgStats.status_bits = status_bits;

	msgStats.aec.buff = vfe32_ctrl->aecStatsControl.bufToRender;
	msgStats.awb.buff = vfe32_ctrl->awbStatsControl.bufToRender;
	msgStats.af.buff = vfe32_ctrl->afStatsControl.bufToRender;

	msgStats.ihist.buff = vfe32_ctrl->ihistStatsControl.bufToRender;
	msgStats.rs.buff = vfe32_ctrl->rsStatsControl.bufToRender;
	msgStats.cs.buff = vfe32_ctrl->csStatsControl.bufToRender;

	temp = msm_io_r(vfe32_ctrl->vfebase + VFE_STATS_AWB_SGW_CFG);
	msgStats.awb_ymin = (0xFF00 & temp) >> 8;

	v4l2_subdev_notify(&vfe32_ctrl->subdev,
				NOTIFY_VFE_MSG_COMP_STATS,
				&msgStats);
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
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->aecStatsControl.droppedStatsFrameCount);
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
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->awbStatsControl.droppedStatsFrameCount);
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
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->afStatsControl.droppedStatsFrameCount);
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
	} else {
		vfe32_ctrl->ihistStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->ihistStatsControl.droppedStatsFrameCount);
	}
}

static void vfe32_process_stats_rs_irq(void)
{
	if (!(vfe32_ctrl->rsStatsControl.ackPending)) {
		vfe32_ctrl->rsStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsRsNum,
			vfe32_ctrl->rsStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->rsStatsControl.bufToRender,
						statsRsNum);
	} else {
		vfe32_ctrl->rsStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->rsStatsControl.droppedStatsFrameCount);
	}
}

static void vfe32_process_stats_cs_irq(void)
{
	if (!(vfe32_ctrl->csStatsControl.ackPending)) {
		vfe32_ctrl->csStatsControl.bufToRender =
			vfe32_process_stats_irq_common(statsCsNum,
			vfe32_ctrl->csStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe32_ctrl->csStatsControl.bufToRender,
						statsCsNum);
	} else {
		vfe32_ctrl->csStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe32_ctrl->csStatsControl.droppedStatsFrameCount);
	}
}

static void vfe32_process_stats(uint32_t status_bits)
{
	unsigned long flags;
	int32_t process_stats = false;
	CDBG("%s, stats = 0x%x\n", __func__, status_bits);

	spin_lock_irqsave(&vfe32_ctrl->comp_stats_ack_lock, flags);
	if (status_bits & VFE_IRQ_STATUS0_STATS_AEC) {
		if (!vfe32_ctrl->aecStatsControl.ackPending) {
			vfe32_ctrl->aecStatsControl.ackPending = TRUE;
			vfe32_ctrl->aecStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsAeNum,
				vfe32_ctrl->aecStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe32_ctrl->aecStatsControl.bufToRender = 0;
			vfe32_ctrl->aecStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe32_ctrl->aecStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_AWB) {
		if (!vfe32_ctrl->awbStatsControl.ackPending) {
			vfe32_ctrl->awbStatsControl.ackPending = TRUE;
			vfe32_ctrl->awbStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsAwbNum,
				vfe32_ctrl->awbStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe32_ctrl->awbStatsControl.droppedStatsFrameCount++;
			vfe32_ctrl->awbStatsControl.bufToRender = 0;
		}
	} else {
		vfe32_ctrl->awbStatsControl.bufToRender = 0;
	}


	if (status_bits & VFE_IRQ_STATUS0_STATS_AF) {
		if (!vfe32_ctrl->afStatsControl.ackPending) {
			vfe32_ctrl->afStatsControl.ackPending = TRUE;
			vfe32_ctrl->afStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsAfNum,
				vfe32_ctrl->afStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe32_ctrl->afStatsControl.bufToRender = 0;
			vfe32_ctrl->afStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe32_ctrl->afStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_IHIST) {
		if (!vfe32_ctrl->ihistStatsControl.ackPending) {
			vfe32_ctrl->ihistStatsControl.ackPending = TRUE;
			vfe32_ctrl->ihistStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsIhistNum,
				vfe32_ctrl->ihistStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe32_ctrl->ihistStatsControl.droppedStatsFrameCount++;
			vfe32_ctrl->ihistStatsControl.bufToRender = 0;
		}
	} else {
		vfe32_ctrl->ihistStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_RS) {
		if (!vfe32_ctrl->rsStatsControl.ackPending) {
			vfe32_ctrl->rsStatsControl.ackPending = TRUE;
			vfe32_ctrl->rsStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsRsNum,
				vfe32_ctrl->rsStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe32_ctrl->rsStatsControl.droppedStatsFrameCount++;
			vfe32_ctrl->rsStatsControl.bufToRender = 0;
		}
	} else {
		vfe32_ctrl->rsStatsControl.bufToRender = 0;
	}


	if (status_bits & VFE_IRQ_STATUS0_STATS_CS) {
		if (!vfe32_ctrl->csStatsControl.ackPending) {
			vfe32_ctrl->csStatsControl.ackPending = TRUE;
			vfe32_ctrl->csStatsControl.bufToRender =
				vfe32_process_stats_irq_common(statsCsNum,
				vfe32_ctrl->csStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe32_ctrl->csStatsControl.droppedStatsFrameCount++;
			vfe32_ctrl->csStatsControl.bufToRender = 0;
		}
	} else {
		vfe32_ctrl->csStatsControl.bufToRender = 0;
	}

	spin_unlock_irqrestore(&vfe32_ctrl->comp_stats_ack_lock, flags);
	if (process_stats)
		vfe_send_comp_stats_msg(status_bits);

	return;
}

static void vfe32_process_stats_irq(uint32_t *irqstatus)
{
	uint32_t status_bits = VFE_COM_STATUS & *irqstatus;

	if ((vfe32_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe32_ctrl->vfeFrameId % vfe32_ctrl->hfr_mode != 0)) {
		CDBG("Skip the stats when HFR enabled\n");
		return;
	}

	vfe32_process_stats(status_bits);
	return;
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

		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_CAMIF_SOF_MASK) {
			CDBG("irq	camifSofIrq\n");
			vfe32_process_camif_sof_irq();
		}
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
			/* in snapshot mode if done then send
			snapshot done message */
			if (vfe32_ctrl->operation_mode ==
					VFE_OUTPUTS_THUMB_AND_MAIN ||
				vfe32_ctrl->operation_mode ==
					VFE_OUTPUTS_MAIN_AND_THUMB ||
				vfe32_ctrl->operation_mode ==
					VFE_OUTPUTS_THUMB_AND_JPEG ||
				vfe32_ctrl->operation_mode ==
					VFE_OUTPUTS_JPEG_AND_THUMB ||
				vfe32_ctrl->operation_mode ==
					VFE_OUTPUTS_RAW) {
				if ((vfe32_ctrl->outpath.out0.capture_cnt == 0)
						&& (vfe32_ctrl->outpath.out1.
						capture_cnt == 0)) {
					msm_io_w_mb(
						CAMIF_COMMAND_STOP_IMMEDIATELY,
						vfe32_ctrl->vfebase +
						VFE_CAMIF_COMMAND);
					vfe32_send_isp_msg(vfe32_ctrl,
						MSG_ID_SNAPSHOT_DONE);
				}
			}
			/* then process stats irq. */
			if (vfe32_ctrl->stats_comp) {
				/* process stats comb interrupt. */
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK) {
					CDBG("Stats composite irq occured.\n");
					vfe32_process_stats_irq(
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
					vfe32_send_isp_msg(vfe32_ctrl,
						MSG_ID_SYNC_TIMER0_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER1) {
					CDBG("SYNC_TIMER 1 irq occured.\n");
					vfe32_send_isp_msg(vfe32_ctrl,
						MSG_ID_SYNC_TIMER1_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_SYNC_TIMER2) {
					CDBG("SYNC_TIMER 2 irq occured.\n");
					vfe32_send_isp_msg(vfe32_ctrl,
						MSG_ID_SYNC_TIMER2_DONE);
				}
			}
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

static long msm_vfe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int subdev_cmd, void *arg)
{
	struct msm_isp_cmd vfecmd;
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
		default:
			pr_err("%s Unsupported cmd type %d",
				__func__, cmd->cmd_type);
			break;
		}
		goto vfe32_config_done;
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

	case CMD_AXI_CFG_PRIM: {
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
		vfe32_config_axi(OUTPUT_PRIM, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS: {
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
		vfe32_config_axi(OUTPUT_PRIM_ALL_CHNLS, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC: {
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
		vfe32_config_axi(OUTPUT_PRIM|OUTPUT_SEC, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC_ALL_CHNLS: {
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
		vfe32_config_axi(OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC: {
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
		vfe32_config_axi(OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC_ALL_CHNLS:
		pr_err("%s Invalid/Unsupported AXI configuration %x",
			__func__, cmd->cmd_type);
		break;
	default:
		pr_err("%s Unsupported AXI configuration %x ", __func__,
			cmd->cmd_type);
		break;
	}
vfe32_config_done:
	kfree(scfg);
	kfree(sack);
	CDBG("%s done: rc = %d\n", __func__, (int) rc);
	return rc;
}

static struct msm_cam_clk_info vfe32_clk_info[] = {
	{"vfe_clk", 228570000},
	{"vfe_pclk", -1},
	{"csi_vfe_clk", -1},
};

static int msm_vfe_subdev_s_crystal_freq(struct v4l2_subdev *sd,
						u32 freq, u32 flags)
{
	int rc = 0;
	int round_rate;

	round_rate = clk_round_rate(vfe32_ctrl->vfe_clk[0], freq);
	if (rc < 0) {
		pr_err("%s: clk_round_rate failed %d\n",
					__func__, rc);
		return rc;
	}

	vfe_clk_rate = round_rate;
	rc = clk_set_rate(vfe32_ctrl->vfe_clk[0], round_rate);
	if (rc < 0)
		pr_err("%s: clk_set_rate failed %d\n",
					__func__, rc);

	return rc;
}

static const struct v4l2_subdev_video_ops msm_vfe_subdev_video_ops = {
	.s_crystal_freq = msm_vfe_subdev_s_crystal_freq,
};

static const struct v4l2_subdev_core_ops msm_vfe_subdev_core_ops = {
	.ioctl = msm_vfe_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_vfe_subdev_ops = {
	.core = &msm_vfe_subdev_core_ops,
	.video = &msm_vfe_subdev_video_ops,
};

int msm_vfe_subdev_init(struct v4l2_subdev *sd, void *data,
	struct platform_device *pdev)
{
	int rc = 0;
	struct msm_sync *sync = data;
	v4l2_set_subdev_hostdata(sd, data);
	vfe_syncdata = data;

	spin_lock_init(&vfe32_ctrl->stop_flag_lock);
	spin_lock_init(&vfe32_ctrl->state_lock);
	spin_lock_init(&vfe32_ctrl->io_lock);
	spin_lock_init(&vfe32_ctrl->update_ack_lock);
	spin_lock_init(&vfe32_ctrl->tasklet_lock);

	spin_lock_init(&vfe32_ctrl->aec_ack_lock);
	spin_lock_init(&vfe32_ctrl->awb_ack_lock);
	spin_lock_init(&vfe32_ctrl->af_ack_lock);
	spin_lock_init(&vfe32_ctrl->ihist_ack_lock);
	spin_lock_init(&vfe32_ctrl->rs_ack_lock);
	spin_lock_init(&vfe32_ctrl->cs_ack_lock);
	spin_lock_init(&vfe32_ctrl->comp_stats_ack_lock);
	spin_lock_init(&vfe32_ctrl->sd_notify_lock);
	INIT_LIST_HEAD(&vfe32_ctrl->tasklet_q);

	vfe32_ctrl->update_linear = false;
	vfe32_ctrl->update_rolloff = false;
	vfe32_ctrl->update_la = false;
	vfe32_ctrl->update_gamma = false;
	vfe32_ctrl->hfr_mode = HFR_MODE_OFF;

	enable_irq(vfe32_ctrl->vfeirq->start);

	vfe32_ctrl->vfebase = ioremap(vfe32_ctrl->vfemem->start,
		resource_size(vfe32_ctrl->vfemem));
	if (!vfe32_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto vfe_remap_failed;
	}

	if (vfe32_ctrl->fs_vfe == NULL) {
		vfe32_ctrl->fs_vfe =
			regulator_get(&vfe32_ctrl->pdev->dev, "fs_vfe");
		if (IS_ERR(vfe32_ctrl->fs_vfe)) {
			pr_err("%s: Regulator FS_VFE get failed %ld\n",
				__func__, PTR_ERR(vfe32_ctrl->fs_vfe));
			vfe32_ctrl->fs_vfe = NULL;
			goto vfe_fs_failed;
		} else if (regulator_enable(vfe32_ctrl->fs_vfe)) {
			pr_err("%s: Regulator FS_VFE enable failed\n",
							__func__);
			regulator_put(vfe32_ctrl->fs_vfe);
			vfe32_ctrl->fs_vfe = NULL;
			goto vfe_fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&vfe32_ctrl->pdev->dev, vfe32_clk_info,
			vfe32_ctrl->vfe_clk, ARRAY_SIZE(vfe32_clk_info), 1);
	if (rc < 0)
		goto vfe_clk_enable_failed;

	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_INIT);
	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_PREVIEW);

	if (msm_io_r(vfe32_ctrl->vfebase + V32_GET_HW_VERSION_OFF) ==
		VFE32_HW_NUMBER)
		vfe32_ctrl->register_total = VFE32_REGISTER_TOTAL;
	else
		vfe32_ctrl->register_total = VFE33_REGISTER_TOTAL;

	return rc;

vfe_clk_enable_failed:
	regulator_disable(vfe32_ctrl->fs_vfe);
	regulator_put(vfe32_ctrl->fs_vfe);
	vfe32_ctrl->fs_vfe = NULL;
vfe_fs_failed:
	iounmap(vfe32_ctrl->vfebase);
vfe_remap_failed:
	disable_irq(vfe32_ctrl->vfeirq->start);
	return rc;
}

void msm_vfe_subdev_release(struct platform_device *pdev)
{
	struct msm_sync *sync = vfe_syncdata;
	msm_cam_clk_enable(&vfe32_ctrl->pdev->dev, vfe32_clk_info,
			vfe32_ctrl->vfe_clk, ARRAY_SIZE(vfe32_clk_info), 0);
	if (vfe32_ctrl->fs_vfe) {
		regulator_disable(vfe32_ctrl->fs_vfe);
		regulator_put(vfe32_ctrl->fs_vfe);
		vfe32_ctrl->fs_vfe = NULL;
	}
	CDBG("%s, free_irq\n", __func__);
	disable_irq(vfe32_ctrl->vfeirq->start);
	tasklet_kill(&vfe32_tasklet);
	iounmap(vfe32_ctrl->vfebase);

	if (atomic_read(&irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	msm_camio_bus_scale_cfg(
		sync->sdata->pdata->cam_bus_scale_table, S_EXIT);
	vfe_syncdata = NULL;
}

static int __devinit vfe32_probe(struct platform_device *pdev)
{
	int rc = 0;
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	vfe32_ctrl = kzalloc(sizeof(struct vfe32_ctrl_type), GFP_KERNEL);
	if (!vfe32_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&vfe32_ctrl->subdev, &msm_vfe_subdev_ops);
	snprintf(vfe32_ctrl->subdev.name,
			 sizeof(vfe32_ctrl->subdev.name), "vfe3.2");
	v4l2_set_subdevdata(&vfe32_ctrl->subdev, vfe32_ctrl);
	platform_set_drvdata(pdev, &vfe32_ctrl->subdev);

	vfe32_ctrl->vfemem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "vfe32");
	if (!vfe32_ctrl->vfemem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vfe32_no_resource;
	}
	vfe32_ctrl->vfeirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vfe32");
	if (!vfe32_ctrl->vfeirq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vfe32_no_resource;
	}

	vfe32_ctrl->vfeio = request_mem_region(vfe32_ctrl->vfemem->start,
		resource_size(vfe32_ctrl->vfemem), pdev->name);
	if (!vfe32_ctrl->vfeio) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto vfe32_no_resource;
	}

	rc = request_irq(vfe32_ctrl->vfeirq->start, vfe32_parse_irq,
		IRQF_TRIGGER_RISING, "vfe", 0);
	if (rc < 0) {
		release_mem_region(vfe32_ctrl->vfemem->start,
			resource_size(vfe32_ctrl->vfemem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto vfe32_no_resource;
	}

	disable_irq(vfe32_ctrl->vfeirq->start);

	vfe32_ctrl->pdev = pdev;
	return 0;

vfe32_no_resource:
	kfree(vfe32_ctrl);
	return 0;
}

static struct platform_driver vfe32_driver = {
	.probe = vfe32_probe,
	.driver = {
		.name = MSM_VFE_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_vfe32_init_module(void)
{
	return platform_driver_register(&vfe32_driver);
}

static void __exit msm_vfe32_exit_module(void)
{
	platform_driver_unregister(&vfe32_driver);
}

module_init(msm_vfe32_init_module);
module_exit(msm_vfe32_exit_module);
MODULE_DESCRIPTION("VFE 3.2 driver");
MODULE_LICENSE("GPL v2");
