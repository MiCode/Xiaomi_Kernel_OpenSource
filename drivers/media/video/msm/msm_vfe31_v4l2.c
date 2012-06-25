/* Copyright (c) 2012 Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <mach/clk.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>

#include "msm.h"
#include "msm_vfe31_v4l2.h"

atomic_t irq_cnt;

#define BUFF_SIZE_128 128

#define VFE31_AXI_OFFSET 0x0050
#define vfe31_get_ch_ping_addr(chn) \
	(msm_camera_io_r(vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe31_get_ch_pong_addr(chn) \
	(msm_camera_io_r(vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe31_get_ch_addr(ping_pong, chn) \
	(((ping_pong) & (1 << (chn))) == 0 ? \
	vfe31_get_ch_pong_addr(chn) : vfe31_get_ch_ping_addr(chn))

#define vfe31_put_ch_ping_addr(chn, addr) \
	(msm_camera_io_w((addr), vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn)))
#define vfe31_put_ch_pong_addr(chn, addr) \
	(msm_camera_io_w((addr), \
	vfe31_ctrl->vfebase + 0x0050 + 0x18 * (chn) + 4))
#define vfe31_put_ch_addr(ping_pong, chn, addr) \
	(((ping_pong) & (1 << (chn))) == 0 ?   \
	vfe31_put_ch_pong_addr((chn), (addr)) : \
	vfe31_put_ch_ping_addr((chn), (addr)))

#define VFE_CLK_RATE	153600000
#define CAMIF_CFG_RMSK             0x1fffff

static struct vfe31_ctrl_type *vfe31_ctrl;
static uint32_t vfe_clk_rate;

struct vfe31_isr_queue_cmd {
	struct list_head	list;
	uint32_t		vfeInterruptStatus0;
	uint32_t		vfeInterruptStatus1;
};

/*TODO: Why is V32 reference in arch/arm/mach-msm/include/mach/camera.h?*/
#define VFE_MSG_V31_START VFE_MSG_V32_START
#define VFE_MSG_V31_CAPTURE VFE_MSG_V32_CAPTURE
#define VFE_MSG_V31_JPEG_CAPTURE VFE_MSG_V32_JPEG_CAPTURE
#define VFE_MSG_V31_START_RECORDING VFE_MSG_V32_START_RECORDING

static struct vfe31_cmd_type vfe31_cmd[] = {
/* 0*/	{VFE_CMD_DUMMY_0},
		{VFE_CMD_SET_CLK},
		{VFE_CMD_RESET},
		{VFE_CMD_START},
		{VFE_CMD_TEST_GEN_START},
/* 5*/	{VFE_CMD_OPERATION_CFG, V31_OPERATION_CFG_LEN},
		{VFE_CMD_AXI_OUT_CFG, V31_AXI_OUT_LEN, V31_AXI_OUT_OFF, 0xFF},
		{VFE_CMD_CAMIF_CFG, V31_CAMIF_LEN, V31_CAMIF_OFF, 0xFF},
		{VFE_CMD_AXI_INPUT_CFG},
		{VFE_CMD_BLACK_LEVEL_CFG, V31_BLACK_LEVEL_LEN,
		V31_BLACK_LEVEL_OFF,
		0xFF},
/*10*/  {VFE_CMD_MESH_ROLL_OFF_CFG, V31_MESH_ROLL_OFF_CFG_LEN,
		V31_MESH_ROLL_OFF_CFG_OFF, 0xFF},
		{VFE_CMD_DEMUX_CFG, V31_DEMUX_LEN, V31_DEMUX_OFF, 0xFF},
		{VFE_CMD_FOV_CFG, V31_FOV_LEN, V31_FOV_OFF, 0xFF},
		{VFE_CMD_MAIN_SCALER_CFG, V31_MAIN_SCALER_LEN,
		V31_MAIN_SCALER_OFF, 0xFF},
		{VFE_CMD_WB_CFG, V31_WB_LEN, V31_WB_OFF, 0xFF},
/*15*/	{VFE_CMD_COLOR_COR_CFG, V31_COLOR_COR_LEN, V31_COLOR_COR_OFF, 0xFF},
		{VFE_CMD_RGB_G_CFG, V31_RGB_G_LEN, V31_RGB_G_OFF, 0xFF},
		{VFE_CMD_LA_CFG, V31_LA_LEN, V31_LA_OFF, 0xFF },
		{VFE_CMD_CHROMA_EN_CFG, V31_CHROMA_EN_LEN, V31_CHROMA_EN_OFF,
		0xFF},
		{VFE_CMD_CHROMA_SUP_CFG, V31_CHROMA_SUP_LEN, V31_CHROMA_SUP_OFF,
		0xFF},
/*20*/	{VFE_CMD_MCE_CFG, V31_MCE_LEN, V31_MCE_OFF, 0xFF},
		{VFE_CMD_SK_ENHAN_CFG, V31_SCE_LEN, V31_SCE_OFF, 0xFF},
		{VFE_CMD_ASF_CFG, V31_ASF_LEN, V31_ASF_OFF, 0xFF},
		{VFE_CMD_S2Y_CFG, V31_S2Y_LEN, V31_S2Y_OFF, 0xFF},
		{VFE_CMD_S2CbCr_CFG, V31_S2CbCr_LEN, V31_S2CbCr_OFF, 0xFF},
/*25*/	{VFE_CMD_CHROMA_SUBS_CFG, V31_CHROMA_SUBS_LEN, V31_CHROMA_SUBS_OFF,
		0xFF},
		{VFE_CMD_OUT_CLAMP_CFG, V31_OUT_CLAMP_LEN, V31_OUT_CLAMP_OFF,
		0xFF},
		{VFE_CMD_FRAME_SKIP_CFG, V31_FRAME_SKIP_LEN, V31_FRAME_SKIP_OFF,
		0xFF},
		{VFE_CMD_DUMMY_1},
		{VFE_CMD_DUMMY_2},
/*30*/	{VFE_CMD_DUMMY_3},
		{VFE_CMD_UPDATE},
		{VFE_CMD_BL_LVL_UPDATE, V31_BLACK_LEVEL_LEN,
		V31_BLACK_LEVEL_OFF, 0xFF},
		{VFE_CMD_DEMUX_UPDATE, V31_DEMUX_LEN, V31_DEMUX_OFF, 0xFF},
		{VFE_CMD_FOV_UPDATE, V31_FOV_LEN, V31_FOV_OFF, 0xFF},
/*35*/	{VFE_CMD_MAIN_SCALER_UPDATE, V31_MAIN_SCALER_LEN, V31_MAIN_SCALER_OFF,
		0xFF},
		{VFE_CMD_WB_UPDATE, V31_WB_LEN, V31_WB_OFF, 0xFF},
		{VFE_CMD_COLOR_COR_UPDATE, V31_COLOR_COR_LEN, V31_COLOR_COR_OFF,
		0xFF},
		{VFE_CMD_RGB_G_UPDATE, V31_RGB_G_LEN, V31_CHROMA_EN_OFF, 0xFF},
		{VFE_CMD_LA_UPDATE, V31_LA_LEN, V31_LA_OFF, 0xFF },
/*40*/	{VFE_CMD_CHROMA_EN_UPDATE, V31_CHROMA_EN_LEN, V31_CHROMA_EN_OFF,
		0xFF},
		{VFE_CMD_CHROMA_SUP_UPDATE, V31_CHROMA_SUP_LEN,
		V31_CHROMA_SUP_OFF, 0xFF},
		{VFE_CMD_MCE_UPDATE, V31_MCE_LEN, V31_MCE_OFF, 0xFF},
		{VFE_CMD_SK_ENHAN_UPDATE, V31_SCE_LEN, V31_SCE_OFF, 0xFF},
		{VFE_CMD_S2CbCr_UPDATE, V31_S2CbCr_LEN, V31_S2CbCr_OFF, 0xFF},
/*45*/	{VFE_CMD_S2Y_UPDATE, V31_S2Y_LEN, V31_S2Y_OFF, 0xFF},
		{VFE_CMD_ASF_UPDATE, V31_ASF_UPDATE_LEN, V31_ASF_OFF, 0xFF},
		{VFE_CMD_FRAME_SKIP_UPDATE},
		{VFE_CMD_CAMIF_FRAME_UPDATE},
		{VFE_CMD_STATS_AF_UPDATE, V31_STATS_AF_LEN, V31_STATS_AF_OFF},
/*50*/	{VFE_CMD_STATS_AE_UPDATE, V31_STATS_AE_LEN, V31_STATS_AE_OFF},
		{VFE_CMD_STATS_AWB_UPDATE, V31_STATS_AWB_LEN,
		V31_STATS_AWB_OFF},
		{VFE_CMD_STATS_RS_UPDATE, V31_STATS_RS_LEN, V31_STATS_RS_OFF},
		{VFE_CMD_STATS_CS_UPDATE, V31_STATS_CS_LEN, V31_STATS_CS_OFF},
		{VFE_CMD_STATS_SKIN_UPDATE},
/*55*/	{VFE_CMD_STATS_IHIST_UPDATE, V31_STATS_IHIST_LEN, V31_STATS_IHIST_OFF},
		{VFE_CMD_DUMMY_4},
		{VFE_CMD_EPOCH1_ACK},
		{VFE_CMD_EPOCH2_ACK},
		{VFE_CMD_START_RECORDING},
/*60*/	{VFE_CMD_STOP_RECORDING},
		{VFE_CMD_DUMMY_5},
		{VFE_CMD_DUMMY_6},
		{VFE_CMD_CAPTURE, V31_CAPTURE_LEN, 0xFF},
		{VFE_CMD_DUMMY_7},
/*65*/	{VFE_CMD_STOP},
		{VFE_CMD_GET_HW_VERSION, V31_GET_HW_VERSION_LEN,
		V31_GET_HW_VERSION_OFF},
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
		{VFE_CMD_STATS_AF_START, V31_STATS_AF_LEN, V31_STATS_AF_OFF},
/*85*/	{VFE_CMD_STATS_AF_STOP},
		{VFE_CMD_STATS_AE_START, V31_STATS_AE_LEN, V31_STATS_AE_OFF},
		{VFE_CMD_STATS_AE_STOP},
		{VFE_CMD_STATS_AWB_START, V31_STATS_AWB_LEN, V31_STATS_AWB_OFF},
		{VFE_CMD_STATS_AWB_STOP},
/*90*/	{VFE_CMD_STATS_RS_START, V31_STATS_RS_LEN, V31_STATS_RS_OFF},
		{VFE_CMD_STATS_RS_STOP},
		{VFE_CMD_STATS_CS_START, V31_STATS_CS_LEN, V31_STATS_CS_OFF},
		{VFE_CMD_STATS_CS_STOP},
		{VFE_CMD_STATS_SKIN_START},
/*95*/	{VFE_CMD_STATS_SKIN_STOP},
		{VFE_CMD_STATS_IHIST_START,
		V31_STATS_IHIST_LEN, V31_STATS_IHIST_OFF},
		{VFE_CMD_STATS_IHIST_STOP},
		{VFE_CMD_DUMMY_10},
		{VFE_CMD_SYNC_TIMER_SETTING, V31_SYNC_TIMER_LEN,
			V31_SYNC_TIMER_OFF},
/*100*/	{VFE_CMD_ASYNC_TIMER_SETTING, V31_ASYNC_TIMER_LEN, V31_ASYNC_TIMER_OFF},
		{VFE_CMD_LIVESHOT},
		{VFE_CMD_LA_SETUP},
		{VFE_CMD_LINEARIZATION_CFG},
		{VFE_CMD_DEMOSAICV3},
/*105*/	{VFE_CMD_DEMOSAICV3_ABCC_CFG},
	{VFE_CMD_DEMOSAICV3_DBCC_CFG},
		{VFE_CMD_DEMOSAICV3_DBPC_CFG, V31_DEMOSAICV3_DBPC_LEN,
			V31_DEMOSAICV3_DBPC_CFG_OFF},
		{VFE_CMD_DEMOSAICV3_ABF_CFG, V31_DEMOSAICV3_ABF_LEN,
			V31_DEMOSAICV3_ABF_OFF},
		{VFE_CMD_DEMOSAICV3_ABCC_UPDATE},
/*110*/	{VFE_CMD_DEMOSAICV3_DBCC_UPDATE},
		{VFE_CMD_DEMOSAICV3_DBPC_UPDATE, V31_DEMOSAICV3_DBPC_LEN,
			V31_DEMOSAICV3_DBPC_CFG_OFF},
		{VFE_CMD_XBAR_CFG},
		{VFE_CMD_MODULE_CFG, V31_MODULE_CFG_LEN, V31_MODULE_CFG_OFF},
		{VFE_CMD_ZSL},
/*115*/	{VFE_CMD_LINEARIZATION_UPDATE},
		{VFE_CMD_DEMOSAICV3_ABF_UPDATE, V31_DEMOSAICV3_ABF_LEN,
			V31_DEMOSAICV3_ABF_OFF},
		{VFE_CMD_CLF_CFG},
		{VFE_CMD_CLF_LUMA_UPDATE},
		{VFE_CMD_CLF_CHROMA_UPDATE},
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

uint32_t vfe31_AXI_WM_CFG[] = {
	0x0000004C,
	0x00000064,
	0x0000007C,
	0x00000094,
	0x000000AC,
	0x000000C4,
	0x000000DC,
};

static const char * const vfe31_general_cmd[] = {
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
	"V31_ZSL",
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

static void vfe31_stop(void)
{
	uint8_t  axiBusyFlag = true;
	unsigned long flags;

	atomic_set(&vfe31_ctrl->vstate, 0);

	/* for reset hw modules, and send msg when reset_irq comes.*/
	spin_lock_irqsave(&vfe31_ctrl->stop_flag_lock, flags);
	vfe31_ctrl->stop_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe31_ctrl->stop_flag_lock, flags);

	/* disable all interrupts.  */
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1,
		vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time. stop camif immediately. */
	msm_camera_io_w_mb(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);
	/* axi halt command. */
	msm_camera_io_w(AXI_HALT,
		vfe31_ctrl->vfebase + VFE_AXI_CMD);
	wmb();
	while (axiBusyFlag) {
		if (msm_camera_io_r(vfe31_ctrl->vfebase + VFE_AXI_STATUS) & 0x1)
			axiBusyFlag = false;
	}
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(AXI_HALT_CLEAR,
		vfe31_ctrl->vfebase + VFE_AXI_CMD);

	/* now enable only halt_irq & reset_irq */
	msm_camera_io_w(0xf0000000,          /* this is for async timer. */
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	msm_camera_io_w_mb(VFE_RESET_UPON_STOP_CMD,
		vfe31_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static void vfe31_subdev_notify(int id, int path, int image_mode)
{
	struct msm_vfe_resp rp;
	struct msm_frame_info frame_info;
	unsigned long flags;
	spin_lock_irqsave(&vfe31_ctrl->sd_notify_lock, flags);
	memset(&rp, 0, sizeof(struct msm_vfe_resp));
	CDBG("vfe31_subdev_notify : msgId = %d\n", id);
	rp.evt_msg.type   = MSM_CAMERA_MSG;
	frame_info.image_mode = image_mode;
	frame_info.path = path;
	rp.evt_msg.data = &frame_info;
	rp.type	   = id;
	v4l2_subdev_notify(&vfe31_ctrl->subdev, NOTIFY_VFE_BUF_EVT, &rp);
	spin_unlock_irqrestore(&vfe31_ctrl->sd_notify_lock, flags);
}

static int vfe31_config_axi(int mode, uint32_t *ao)
{
	uint32_t *ch_info;
	uint32_t *axi_cfg = ao+V31_AXI_RESERVED;
	/* Update the corresponding write masters for each output*/
	ch_info = axi_cfg + V31_AXI_CFG_LEN;
	vfe31_ctrl->outpath.out0.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out0.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out0.ch2 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out0.image_mode = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out1.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out1.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out1.ch2 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out1.image_mode = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out2.ch0 = 0x0000FFFF & *ch_info;
	vfe31_ctrl->outpath.out2.ch1 = 0x0000FFFF & (*ch_info++ >> 16);
	vfe31_ctrl->outpath.out2.ch2 = 0x0000FFFF & *ch_info++;

	switch (mode) {
	case OUTPUT_PRIM:
		vfe31_ctrl->outpath.output_mode =
			VFE31_OUTPUT_MODE_PRIMARY;
		break;
	case OUTPUT_PRIM_ALL_CHNLS:
		vfe31_ctrl->outpath.output_mode =
			VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC:
		vfe31_ctrl->outpath.output_mode =
			VFE31_OUTPUT_MODE_PRIMARY;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_SECONDARY;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS:
		vfe31_ctrl->outpath.output_mode =
			VFE31_OUTPUT_MODE_PRIMARY;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC:
		vfe31_ctrl->outpath.output_mode =
			VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		vfe31_ctrl->outpath.output_mode |=
			VFE31_OUTPUT_MODE_SECONDARY;
		break;
	default:
		pr_err("%s Invalid AXI mode %d ", __func__, mode);
		return -EINVAL;
	}

	msm_camera_io_memcpy(vfe31_ctrl->vfebase +
		vfe31_cmd[VFE_CMD_AXI_OUT_CFG].offset, axi_cfg,
		vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length - V31_AXI_CH_INF_LEN -
			V31_AXI_RESERVED);
	return 0;
}

static void vfe31_reset_internal_variables(void)
{
	unsigned long flags;
	vfe31_ctrl->vfeImaskCompositePacked = 0;
	/* state control variables */
	vfe31_ctrl->start_ack_pending = FALSE;
	atomic_set(&irq_cnt, 0);

	spin_lock_irqsave(&vfe31_ctrl->stop_flag_lock, flags);
	vfe31_ctrl->stop_ack_pending  = FALSE;
	spin_unlock_irqrestore(&vfe31_ctrl->stop_flag_lock, flags);

	vfe31_ctrl->reset_ack_pending  = FALSE;

	spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
	vfe31_ctrl->update_ack_pending = FALSE;
	spin_unlock_irqrestore(&vfe31_ctrl->update_ack_lock, flags);

	vfe31_ctrl->recording_state = VFE_STATE_IDLE;
	vfe31_ctrl->liveshot_state = VFE_STATE_IDLE;

	atomic_set(&vfe31_ctrl->vstate, 0);

	/* 0 for continuous mode, 1 for snapshot mode */
	vfe31_ctrl->operation_mode = 0;
	vfe31_ctrl->outpath.output_mode = 0;
	vfe31_ctrl->vfe_capture_count = 0;

	/* this is unsigned 32 bit integer. */
	vfe31_ctrl->vfeFrameId = 0;
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

	vfe31_ctrl->frame_skip_cnt = 31;
	vfe31_ctrl->frame_skip_pattern = 0xffffffff;
	vfe31_ctrl->snapshot_frame_cnt = 0;
}

static void vfe31_reset(void)
{
	vfe31_reset_internal_variables();
	/* disable all interrupts.  vfeImaskLocal is also reset to 0
	* to begin with. */
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(VFE_CLEAR_ALL_IRQS,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_IRQ_CMD);

	/* enable reset_ack interrupt.  */
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
	vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Write to VFE_GLOBAL_RESET_CMD to reset the vfe hardware. Once reset
	 * is done, hardware interrupt will be generated.  VFE ist processes
	 * the interrupt to complete the function call.  Note that the reset
	 * function is synchronous. */

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(VFE_RESET_UPON_RESET_CMD,
		vfe31_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static int vfe31_operation_config(uint32_t *cmd)
{
	uint32_t *p = cmd;

	vfe31_ctrl->operation_mode = *p;
	vfe31_ctrl->stats_comp = *(++p);
	vfe31_ctrl->hfr_mode = *(++p);

	msm_camera_io_w(*(++p), vfe31_ctrl->vfebase + VFE_CFG);
	msm_camera_io_w(*(++p), vfe31_ctrl->vfebase + VFE_MODULE_CFG);
	msm_camera_io_w(*(++p), vfe31_ctrl->vfebase + VFE_REALIGN_BUF);
	msm_camera_io_w(*(++p), vfe31_ctrl->vfebase + VFE_CHROMA_UP);
	msm_camera_io_w(*(++p), vfe31_ctrl->vfebase + VFE_STATS_CFG);
	return 0;
}

static uint32_t vfe_stats_awb_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	vfe31_ctrl->awbStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_aec_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_WR_PONG_ADDR);

	vfe31_ctrl->aecStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_af_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);

	vfe31_ctrl->afStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_ihist_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PONG_ADDR);

	vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_rs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_WR_PONG_ADDR);

	vfe31_ctrl->rsStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static uint32_t vfe_stats_cs_buf_init(struct vfe_cmd_stats_buf *in)
{
	uint32_t *ptr = in->statsBuf;
	uint32_t addr;

	addr = ptr[0];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PING_ADDR);
	addr = ptr[1];
	msm_camera_io_w(addr,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_WR_PONG_ADDR);

	vfe31_ctrl->csStatsControl.nextFrameAddrBuf = in->statsBuf[2];
	return 0;
}

static void msm_camera_io_dump2(void __iomem *addr, int size)
{
	char line_str[BUFF_SIZE_128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			CDBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CDBG("%s\n", line_str);
}

static void vfe31_start_common(void)
{
	uint32_t irq_mask = 0x00E00021;
	vfe31_ctrl->start_ack_pending = TRUE;
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe31_ctrl->operation_mode, vfe31_ctrl->outpath.output_mode);
	if (vfe31_ctrl->stats_comp)
		irq_mask |= VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK;
	else
		irq_mask |= 0x000FE000;

	msm_camera_io_w(irq_mask, vfe31_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
		vfe31_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);

	msm_camera_io_dump2(vfe31_ctrl->vfebase, vfe31_ctrl->register_total*4);
	atomic_set(&vfe31_ctrl->vstate, 1);
}

static int vfe31_start_recording(struct msm_cam_media_controller *pmctl)
{
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_VIDEO);
	vfe31_ctrl->recording_state = VFE_STATE_START_REQUESTED;
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe31_stop_recording(struct msm_cam_media_controller *pmctl)
{
	vfe31_ctrl->recording_state = VFE_STATE_STOP_REQUESTED;
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	return 0;
}

static void vfe31_start_liveshot(struct msm_cam_media_controller *pmctl)
{
	/* Hardcode 1 live snapshot for now. */
	vfe31_ctrl->outpath.out0.capture_cnt = 1;
	vfe31_ctrl->vfe_capture_count = vfe31_ctrl->outpath.out0.capture_cnt;

	vfe31_ctrl->liveshot_state = VFE_STATE_START_REQUESTED;
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static int vfe31_zsl(struct msm_cam_media_controller *pmctl)
{
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	irq_comp_mask	=
		msm_camera_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	CDBG("%s:op mode %d O/P Mode %d\n", __func__,
		vfe31_ctrl->operation_mode, vfe31_ctrl->outpath.output_mode);

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= ((0x1 << (vfe31_ctrl->outpath.out0.ch0)) |
			(0x1 << (vfe31_ctrl->outpath.out0.ch1)));
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= ((0x1 << (vfe31_ctrl->outpath.out0.ch0)) |
			(0x1 << (vfe31_ctrl->outpath.out0.ch1)) |
			(0x1 << (vfe31_ctrl->outpath.out0.ch2)));
	}

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= ((0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8)) |
			(0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8)));
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= ((0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8)) |
			(0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8)) |
			(0x1 << (vfe31_ctrl->outpath.out1.ch2 + 8)));
	}

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PRIMARY) {
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch2]);
	}

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_SECONDARY) {
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch2]);
	}

	msm_camera_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	vfe31_start_common();
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_ZSL);

	msm_camera_io_w(1, vfe31_ctrl->vfebase + 0x18C);
	msm_camera_io_w(1, vfe31_ctrl->vfebase + 0x188);
	return 0;
}
static int vfe31_capture_raw(
	struct msm_cam_media_controller *pmctl,
	uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;

	vfe31_ctrl->outpath.out0.capture_cnt = num_frames_capture;
	vfe31_ctrl->vfe_capture_count = num_frames_capture;

	irq_comp_mask =
		msm_camera_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (0x1 << (vfe31_ctrl->outpath.out0.ch0));
		msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
	}

	msm_camera_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_CAPTURE);
	vfe31_start_common();
	return 0;
}

static int vfe31_capture(
	struct msm_cam_media_controller *pmctl,
	uint32_t num_frames_capture)
{
	uint32_t irq_comp_mask = 0;
	/* capture command is valid for both idle and active state. */
	vfe31_ctrl->outpath.out1.capture_cnt = num_frames_capture;
	if (vfe31_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) {
		vfe31_ctrl->outpath.out0.capture_cnt =
			num_frames_capture;
	}

	vfe31_ctrl->vfe_capture_count = num_frames_capture;
	irq_comp_mask = msm_camera_io_r(
				vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe31_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN) {
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY) {
			irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
				0x1 << vfe31_ctrl->outpath.out0.ch1);
		}
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_SECONDARY) {
			irq_comp_mask |=
				(0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8) |
				0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8));
		}
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		}
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		}
	}

	vfe31_ctrl->vfe_capture_count = num_frames_capture;

	msm_camera_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camera_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_CAPTURE);

	vfe31_start_common();
	/* for debug */
	msm_camera_io_w(1, vfe31_ctrl->vfebase + 0x18C);
	msm_camera_io_w(1, vfe31_ctrl->vfebase + 0x188);
	return 0;
}

static int vfe31_start(struct msm_cam_media_controller *pmctl)
{
	uint32_t irq_comp_mask = 0;

	irq_comp_mask	=
		msm_camera_io_r(vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_PRIMARY) {
		irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
			0x1 << vfe31_ctrl->outpath.out0.ch1);
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= (0x1 << vfe31_ctrl->outpath.out0.ch0 |
			0x1 << vfe31_ctrl->outpath.out0.ch1 |
			0x1 << vfe31_ctrl->outpath.out0.ch2);
	}
	if (vfe31_ctrl->outpath.output_mode & VFE31_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= (0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8));
	} else if (vfe31_ctrl->outpath.output_mode &
		VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= (0x1 << (vfe31_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (vfe31_ctrl->outpath.out1.ch1 + 8) |
			0x1 << (vfe31_ctrl->outpath.out1.ch2 + 8));
	}
	msm_camera_io_w(irq_comp_mask, vfe31_ctrl->vfebase + VFE_IRQ_COMP_MASK);

	switch (vfe31_ctrl->operation_mode) {
	case VFE_OUTPUTS_PREVIEW:
	case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		} else if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch2]);
		}
		break;
	default:
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		} else if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch2]);
		}
		break;
	}
	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	vfe31_start_common();
	return 0;
}

static void vfe31_update(void)
{
	unsigned long flags;

	if (vfe31_ctrl->update_la) {
		if (!msm_camera_io_r(vfe31_ctrl->vfebase + V31_LA_OFF))
			msm_camera_io_w(1, vfe31_ctrl->vfebase + V31_LA_OFF);
		else
			msm_camera_io_w(0, vfe31_ctrl->vfebase + V31_LA_OFF);
		vfe31_ctrl->update_la = false;
	}

	if (vfe31_ctrl->update_gamma) {
		if (!msm_camera_io_r(vfe31_ctrl->vfebase + V31_RGB_G_OFF))
			msm_camera_io_w(7, vfe31_ctrl->vfebase+V31_RGB_G_OFF);
		else
			msm_camera_io_w(0, vfe31_ctrl->vfebase+V31_RGB_G_OFF);
		vfe31_ctrl->update_gamma = false;
	}

	spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
	vfe31_ctrl->update_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe31_ctrl->update_ack_lock, flags);
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
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
	msm_camera_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF);
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
	msm_camera_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF);
	/* Sync Timer Line Start */
	value = *tbl++;
	msm_camera_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
		4 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Start */
	value = *tbl++;
	msm_camera_io_w(value, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
		 8 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Sync Timer Pixel Duration */
	value = *tbl++;
	val = vfe_clk_rate / 10000;
	val = 10000000 / val;
	val = value * 10000 / val;
	CDBG("%s: Pixel Clk Cycles!!! %d\n", __func__, val);
	msm_camera_io_w(val, vfe31_ctrl->vfebase + V31_SYNC_TIMER_OFF +
		12 + ((vfe31_ctrl->sync_timer_number) * 12));
	/* Timer0 Active High/LOW */
	value = *tbl++;
	msm_camera_io_w(value,
		vfe31_ctrl->vfebase + V31_SYNC_TIMER_POLARITY_OFF);
	/* Selects sync timer 0 output to drive onto timer1 port */
	value = 0;
	msm_camera_io_w(value, vfe31_ctrl->vfebase + V31_TIMER_SELECT_OFF);
}

static void vfe31_program_dmi_cfg(enum VFE31_DMI_RAM_SEL bankSel)
{
	/* set bit 8 for auto increment. */
	uint32_t value = VFE_DMI_CFG_DEFAULT;
	value += (uint32_t)bankSel;
	CDBG("%s: banksel = %d\n", __func__, bankSel);

	msm_camera_io_w(value, vfe31_ctrl->vfebase + VFE_DMI_CFG);
	/* by default, always starts with offset 0.*/
	msm_camera_io_w(0, vfe31_ctrl->vfebase + VFE_DMI_ADDR);
}
static void vfe31_write_gamma_cfg(enum VFE31_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	int i;
	uint32_t value, value1, value2;
	vfe31_program_dmi_cfg(channel_sel);
	for (i = 0 ; i < (VFE31_GAMMA_NUM_ENTRIES/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_camera_io_w((value1),
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_camera_io_w((value2),
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe31_read_gamma_cfg(enum VFE31_DMI_RAM_SEL channel_sel,
	uint32_t *tbl)
{
	int i;
	vfe31_program_dmi_cfg(channel_sel);
	CDBG("%s: Gamma table channel: %d\n", __func__, channel_sel);
	for (i = 0 ; i < VFE31_GAMMA_NUM_ENTRIES ; i++) {
		*tbl = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
		CDBG("%s: %08x\n", __func__, *tbl);
		tbl++;
	}
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static void vfe31_write_la_cfg(enum VFE31_DMI_RAM_SEL channel_sel,
						const uint32_t *tbl)
{
	uint32_t i;
	uint32_t value, value1, value2;

	vfe31_program_dmi_cfg(channel_sel);
	for (i = 0 ; i < (VFE31_LA_TABLE_LENGTH/2) ; i++) {
		value = *tbl++;
		value1 = value & 0x0000FFFF;
		value2 = (value & 0xFFFF0000)>>16;
		msm_camera_io_w((value1),
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
		msm_camera_io_w((value2),
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
	}
	vfe31_program_dmi_cfg(NO_MEM_SELECTED);
}

static struct vfe31_output_ch *vfe31_get_ch(int path)
{
	struct vfe31_output_ch *ch = NULL;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		ch = &vfe31_ctrl->outpath.out0;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		ch = &vfe31_ctrl->outpath.out1;
	else
		pr_err("%s: Invalid path %d\n", __func__, path);

	BUG_ON(ch == NULL);
	return ch;
}
static struct msm_free_buf *vfe31_check_free_buffer(int id, int path)
{
	struct vfe31_output_ch *outch = NULL;
	struct msm_free_buf *b = NULL;
	uint32_t image_mode = 0;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		image_mode = vfe31_ctrl->outpath.out0.image_mode;
	else
		image_mode = vfe31_ctrl->outpath.out1.image_mode;

	vfe31_subdev_notify(id, path, image_mode);
	outch = vfe31_get_ch(path);
	if (outch->free_buf.ch_paddr[0])
		b = &outch->free_buf;
	return b;
}
static int vfe31_configure_pingpong_buffers(int id, int path)
{
	struct vfe31_output_ch *outch = NULL;
	int rc = 0;
	uint32_t image_mode = 0;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		image_mode = vfe31_ctrl->outpath.out0.image_mode;
	else
		image_mode = vfe31_ctrl->outpath.out1.image_mode;

	vfe31_subdev_notify(id, path, image_mode);
	outch = vfe31_get_ch(path);
	if (outch->ping.ch_paddr[0] && outch->pong.ch_paddr[0]) {
		/* Configure Preview Ping Pong */
		CDBG("%s Configure ping/pong address for %d",
			__func__, path);
		vfe31_put_ch_ping_addr(outch->ch0,
			outch->ping.ch_paddr[0]);
		vfe31_put_ch_pong_addr(outch->ch0,
			outch->pong.ch_paddr[0]);

		if (vfe31_ctrl->operation_mode !=
			VFE_OUTPUTS_RAW) {
			vfe31_put_ch_ping_addr(outch->ch1,
				outch->ping.ch_paddr[1]);
			vfe31_put_ch_pong_addr(outch->ch1,
				outch->pong.ch_paddr[1]);
		}

		if (outch->ping.num_planes > 2)
			vfe31_put_ch_ping_addr(outch->ch2,
				outch->ping.ch_paddr[2]);
		if (outch->pong.num_planes > 2)
			vfe31_put_ch_pong_addr(outch->ch2,
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

static void vfe31_send_isp_msg(struct vfe31_ctrl_type *vctrl,
	uint32_t isp_msg_id)
{
	struct isp_msg_event isp_msg_evt;

	isp_msg_evt.msg_id = isp_msg_id;
	isp_msg_evt.sof_count = vfe31_ctrl->vfeFrameId;
	v4l2_subdev_notify(&vctrl->subdev,
		NOTIFY_ISP_MSG_EVT, (void *)&isp_msg_evt);
}

static int vfe31_proc_general(
	struct msm_cam_media_controller *pmctl,
	struct msm_isp_cmd *cmd)
{
	int i , rc = 0;
	uint32_t old_val = 0 , new_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;
	uint32_t temp1 = 0, temp2 = 0;

	CDBG("vfe31_proc_general: cmdID = %s, length = %d\n",
		vfe31_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case VFE_CMD_RESET:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		vfe31_reset();
		break;
	case VFE_CMD_START:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		if ((vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) ||
			(vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_PREVIEW))
			/* Configure primary channel */
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_START, VFE_MSG_OUTPUT_PRIMARY);
		else
			/* Configure secondary channel */
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_START, VFE_MSG_OUTPUT_SECONDARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe31_start(pmctl);
		break;
	case VFE_CMD_UPDATE:
		vfe31_update();
		break;
	case VFE_CMD_CAPTURE_RAW:
		pr_info("%s: cmdID = VFE_CMD_CAPTURE_RAW\n", __func__);
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
			sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe31_configure_pingpong_buffers(VFE_MSG_V31_CAPTURE,
			VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for snapshot", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe31_capture_raw(pmctl, snapshot_cnt);
		break;
	case VFE_CMD_CAPTURE:
		if (copy_from_user(&snapshot_cnt, (void __user *)(cmd->value),
			sizeof(uint32_t))) {
			rc = -EFAULT;
			goto proc_general_done;
		}

		if (vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) {
			if (snapshot_cnt != 1) {
				pr_err("only support 1 inline snapshot\n");
				rc = -EINVAL;
				goto proc_general_done;
			}
			/* Configure primary channel for JPEG */
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_JPEG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY);
		} else {
			/* Configure primary channel */
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY);
		}
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for primary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		/* Configure secondary channel */
		rc = vfe31_configure_pingpong_buffers(VFE_MSG_V31_CAPTURE,
			VFE_MSG_OUTPUT_SECONDARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for secondary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe31_capture(pmctl, snapshot_cnt);
		break;
	case VFE_CMD_START_RECORDING:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_PREVIEW_AND_VIDEO)
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_START_RECORDING,
				VFE_MSG_OUTPUT_SECONDARY);
		else if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_VIDEO_AND_PREVIEW)
			rc = vfe31_configure_pingpong_buffers(
				VFE_MSG_V31_START_RECORDING,
				VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for video", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		rc = vfe31_start_recording(pmctl);
		break;
	case VFE_CMD_STOP_RECORDING:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		rc = vfe31_stop_recording(pmctl);
		break;
	case VFE_CMD_OPERATION_CFG:
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
		break;

	case VFE_CMD_STATS_AE_START:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AE_BG_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp, (vfe31_cmd[cmd->id].length));
		break;
	case VFE_CMD_STATS_AF_START:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AF_BF_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
		cmdp, (vfe31_cmd[cmd->id].length));
		break;
	case VFE_CMD_STATS_AWB_START:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= AWB_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_STATS_IHIST_START:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val |= IHIST_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_STATS_RS_START:
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
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_STATS_CS_START:
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
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_MCE_UPDATE:
	case VFE_CMD_MCE_CFG:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		/* Incrementing with 4 so as to point to the 2nd Register as
		the 2nd register has the mce_enable bit */
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase +
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
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 4,
			&new_val, 4);
		cmdp_local += 1;

		old_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			V31_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 8,
			&new_val, 4);
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp_local, (vfe31_cmd[cmd->id].length));
		break;
	case VFE_CMD_CHROMA_SUP_UPDATE:
	case VFE_CMD_CHROMA_SUP_CFG:
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
		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF,
			cmdp_local, 4);

		cmdp_local += 1;
		new_val = *cmdp_local;
		/* Incrementing with 4 so as to point to the 2nd Register as
		 * the 2nd register has the mce_enable bit
		 */
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			V31_CHROMA_SUP_OFF + 4);
		old_val &= ~MCE_EN_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 4,
			&new_val, 4);
		cmdp_local += 1;

		old_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			V31_CHROMA_SUP_OFF + 8);
		new_val = *cmdp_local;
		old_val &= ~MCE_Q_K_MASK;
		new_val = new_val | old_val;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + V31_CHROMA_SUP_OFF + 8,
			&new_val, 4);
		break;

	case VFE_CMD_MESH_ROLL_OFF_CFG:
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
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp_local, 16);
		cmdp_local += 4;
		vfe31_program_dmi_cfg(ROLLOFF_RAM);
		/* for loop for extrcting init table. */
		for (i = 0; i < (V31_MESH_ROLL_OFF_INIT_TABLE_SIZE * 2); i++) {
			msm_camera_io_w(*cmdp_local ,
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		CDBG("done writing init table\n");
		/* by default, always starts with offset 0. */
		msm_camera_io_w(V31_MESH_ROLL_OFF_DELTA_TABLE_OFFSET,
		vfe31_ctrl->vfebase + VFE_DMI_ADDR);
		/* for loop for extracting delta table. */
		for (i = 0; i < (V31_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2); i++) {
			msm_camera_io_w(*cmdp_local,
			vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		vfe31_program_dmi_cfg(NO_MEM_SELECTED);
		break;

	case VFE_CMD_GET_MESH_ROLLOFF_TABLE:
		temp1 = sizeof(uint32_t) * ((V31_MESH_ROLL_OFF_INIT_TABLE_SIZE *
			2) + (V31_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2));
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
		vfe31_program_dmi_cfg(ROLLOFF_RAM);
		CDBG("%s: Mesh Rolloff init Table\n", __func__);
		for (i = 0; i < (V31_MESH_ROLL_OFF_INIT_TABLE_SIZE * 2); i++) {
			*cmdp_local = msm_camera_io_r(
					vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		msm_camera_io_w(V31_MESH_ROLL_OFF_DELTA_TABLE_OFFSET,
			vfe31_ctrl->vfebase + VFE_DMI_ADDR);
		CDBG("%s: Mesh Rolloff Delta Table\n", __func__);
		for (i = 0; i < (V31_MESH_ROLL_OFF_DELTA_TABLE_SIZE * 2); i++) {
			*cmdp_local = msm_camera_io_r(
					vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			CDBG("%s: %08x\n", __func__, *cmdp_local);
			cmdp_local++;
		}
		CDBG("done reading delta table\n");
		vfe31_program_dmi_cfg(NO_MEM_SELECTED);
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
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp_local, (vfe31_cmd[cmd->id].length));

		cmdp_local += 1;
		vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0, cmdp_local);
		break;

	case VFE_CMD_LA_UPDATE:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + V31_LA_OFF);
		if (old_val != 0x0)
			vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK0,
				cmdp_local);
		else
			vfe31_write_la_cfg(LUMA_ADAPT_LUT_RAM_BANK1,
				cmdp_local);
		vfe31_ctrl->update_la = true;
		break;

	case VFE_CMD_GET_LA_TABLE:
		temp1 = sizeof(uint32_t) * VFE31_LA_TABLE_LENGTH / 2;
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
		if (msm_camera_io_r(vfe31_ctrl->vfebase + V31_LA_OFF))
			vfe31_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK1);
		else
			vfe31_program_dmi_cfg(LUMA_ADAPT_LUT_RAM_BANK0);
		for (i = 0 ; i < (VFE31_LA_TABLE_LENGTH / 2) ; i++) {
			*cmdp_local = msm_camera_io_r(
					vfe31_ctrl->vfebase + VFE_DMI_DATA_LO);
			*cmdp_local |= (msm_camera_io_r(vfe31_ctrl->vfebase +
				VFE_DMI_DATA_LO)) << 16;
			cmdp_local++;
		}
		vfe31_program_dmi_cfg(NO_MEM_SELECTED);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_SK_ENHAN_CFG:
	case VFE_CMD_SK_ENHAN_UPDATE:
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
		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_SCE_OFF,
			cmdp, V31_SCE_LEN);
		break;

	case VFE_CMD_LIVESHOT:
		/* Configure primary channel */
		rc = vfe31_configure_pingpong_buffers(VFE_MSG_V31_CAPTURE,
			VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for primary output", __func__);
			rc = -EINVAL;
			goto proc_general_done;
		}
		vfe31_start_liveshot(pmctl);
		break;

	case VFE_CMD_DEMOSAICV3:
		if (cmd->length != V31_DEMOSAICV3_LEN) {
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
				vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF,
			cmdp_local, V31_DEMOSAICV3_LEN);
		break;

	case VFE_CMD_DEMOSAICV3_UPDATE:
		if (cmd->length !=
			V31_DEMOSAICV3_LEN * V31_DEMOSAICV3_UP_REG_CNT) {
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
				vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF);
		old_val &= DEMOSAIC_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF,
			cmdp_local, V31_DEMOSAICV3_LEN);

		break;

	case VFE_CMD_DEMOSAICV3_ABCC_CFG:
		rc = -EFAULT;
		break;

	case VFE_CMD_DEMOSAICV3_ABF_UPDATE:/* 116 ABF update  */
	case VFE_CMD_DEMOSAICV3_ABF_CFG: /* 108 ABF config  */
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
				vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF);
		old_val &= ABF_MASK;
		new_val = new_val | old_val;
		*cmdp_local = new_val;

		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF,
		    cmdp_local, 4);

		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp_local, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_DEMOSAICV3_DBCC_CFG:
	case VFE_CMD_DEMOSAICV3_DBCC_UPDATE:
		return -EINVAL;

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
				vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF);
		old_val &= BPC_MASK;

		new_val = new_val | old_val;
		*cmdp_local = new_val;
		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_DEMOSAICV3_OFF,
					cmdp_local, V31_DEMOSAICV3_LEN);
		cmdp_local += 1;
		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + V31_DEMOSAICV3_DBPC_CFG_OFF,
			cmdp_local, V31_DEMOSAICV3_DBPC_LEN);
		break;

	case VFE_CMD_RGB_G_CFG:
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
		msm_camera_io_memcpy(vfe31_ctrl->vfebase + V31_RGB_G_OFF,
			cmdp, 4);
		cmdp += 1;

		vfe31_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0, cmdp);
		vfe31_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0, cmdp);
		vfe31_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0, cmdp);
		cmdp -= 1;
		break;

	case VFE_CMD_RGB_G_UPDATE:
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

		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + V31_RGB_G_OFF);
		cmdp += 1;
		if (old_val != 0x0) {
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH0_BANK0, cmdp);
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH1_BANK0, cmdp);
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH2_BANK0, cmdp);
		} else {
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH0_BANK1, cmdp);
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH1_BANK1, cmdp);
			vfe31_write_gamma_cfg(RGBLUT_RAM_CH2_BANK1, cmdp);
		}
		vfe31_ctrl->update_gamma = TRUE;
		cmdp -= 1;
		break;

	case VFE_CMD_GET_RGB_G_TABLE:
		temp1 = sizeof(uint32_t) * VFE31_GAMMA_NUM_ENTRIES * 3;
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

		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + V31_RGB_G_OFF);
		temp2 = old_val ? RGBLUT_RAM_CH0_BANK1 :
			RGBLUT_RAM_CH0_BANK0;
		for (i = 0; i < 3; i++) {
			vfe31_read_gamma_cfg(temp2,
				cmdp_local + (VFE31_GAMMA_NUM_ENTRIES * i));
			temp2 += 2;
		}
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;

	case VFE_CMD_STATS_AWB_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AWB_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;
	case VFE_CMD_STATS_AE_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AE_BG_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;
	case VFE_CMD_STATS_AF_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~AF_BF_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;

	case VFE_CMD_STATS_IHIST_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~IHIST_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;

	case VFE_CMD_STATS_RS_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~RS_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;

	case VFE_CMD_STATS_CS_STOP:
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~CS_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		break;

	case VFE_CMD_STOP:
		pr_info("vfe31_proc_general: cmdID = %s\n",
			vfe31_general_cmd[cmd->id]);
		vfe31_stop();
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
		vfe31_sync_timer_start(cmdp);
		break;

	case VFE_CMD_MODULE_CFG:
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
		old_val = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= STATS_ENABLE_MASK;
		*cmdp |= old_val;

		msm_camera_io_memcpy(
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	case VFE_CMD_ZSL:
		rc = vfe31_configure_pingpong_buffers(VFE_MSG_V31_START,
			VFE_MSG_OUTPUT_PRIMARY);
		if (rc < 0)
			goto proc_general_done;
		rc = vfe31_configure_pingpong_buffers(VFE_MSG_V31_START,
			VFE_MSG_OUTPUT_SECONDARY);
		if (rc < 0)
			goto proc_general_done;

		rc = vfe31_zsl(pmctl);
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
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		cmdp_local = cmdp + V31_ASF_LEN/4;
		break;

	case VFE_CMD_GET_HW_VERSION:
		if (cmd->length != V31_GET_HW_VERSION_LEN) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(V31_GET_HW_VERSION_LEN, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		*cmdp = msm_camera_io_r(
				vfe31_ctrl->vfebase+V31_GET_HW_VERSION_OFF);
		if (copy_to_user((void __user *)(cmd->value), cmdp,
			V31_GET_HW_VERSION_LEN)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_GET_REG_DUMP:
		temp1 = sizeof(uint32_t) * vfe31_ctrl->register_total;
		if (cmd->length != temp1) {
			rc = -EINVAL;
			goto proc_general_done;
		}
		cmdp = kmalloc(temp1, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto proc_general_done;
		}
		msm_camera_io_dump(
			vfe31_ctrl->vfebase, vfe31_ctrl->register_total*4);
		CDBG("%s: %p %p %d\n", __func__, (void *)cmdp,
			vfe31_ctrl->vfebase, temp1);
		memcpy_fromio((void *)cmdp, vfe31_ctrl->vfebase, temp1);
		if (copy_to_user((void __user *)(cmd->value), cmdp, temp1)) {
			rc = -EFAULT;
			goto proc_general_done;
		}
		break;
	case VFE_CMD_FRAME_SKIP_CFG:
		if (cmd->length != vfe31_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe31_cmd[cmd->id].length, GFP_ATOMIC);
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
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		vfe31_ctrl->frame_skip_cnt = ((uint32_t)
			*cmdp & VFE_FRAME_SKIP_PERIOD_MASK) + 1;
		vfe31_ctrl->frame_skip_pattern = (uint32_t)(*(cmdp + 2));
		break;
	default:
		if (cmd->length != vfe31_cmd[cmd->id].length)
			return -EINVAL;

		cmdp = kmalloc(vfe31_cmd[cmd->id].length, GFP_ATOMIC);
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
			vfe31_ctrl->vfebase + vfe31_cmd[cmd->id].offset,
			cmdp, (vfe31_cmd[cmd->id].length));
		break;

	}

proc_general_done:
	kfree(cmdp);

	return rc;
}

static void vfe31_stats_af_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->af_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->afStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->afStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe31_stats_awb_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->awb_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->awbStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->awbStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe31_stats_aec_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->aec_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->aecStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->aecStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static void vfe31_stats_ihist_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->ihist_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->ihistStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}
static void vfe31_stats_rs_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->rs_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->rsStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->rsStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}
static void vfe31_stats_cs_ack(struct vfe_cmd_stats_ack *pAck)
{
	unsigned long flags;
	spinlock_t *lock = (vfe31_ctrl->stats_comp ?
		&vfe31_ctrl->comp_stats_ack_lock :
		&vfe31_ctrl->cs_ack_lock);
	spin_lock_irqsave(lock, flags);
	vfe31_ctrl->csStatsControl.nextFrameAddrBuf = pAck->nextStatsBuf;
	vfe31_ctrl->csStatsControl.ackPending = FALSE;
	spin_unlock_irqrestore(lock, flags);
}

static inline void vfe31_read_irq_status(struct vfe31_irq_status *out)
{
	uint32_t *temp;
	memset(out, 0, sizeof(struct vfe31_irq_status));
	temp = (uint32_t *)(vfe31_ctrl->vfebase + VFE_IRQ_STATUS_0);
	out->vfeIrqStatus0 = msm_camera_io_r(temp);

	temp = (uint32_t *)(vfe31_ctrl->vfebase + VFE_IRQ_STATUS_1);
	out->vfeIrqStatus1 = msm_camera_io_r(temp);

	temp = (uint32_t *)(vfe31_ctrl->vfebase + VFE_CAMIF_STATUS);
	out->camifStatus = msm_camera_io_r(temp);
	CDBG("camifStatus  = 0x%x\n", out->camifStatus);

	/* clear the pending interrupt of the same kind.*/
	msm_camera_io_w(out->vfeIrqStatus0,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(out->vfeIrqStatus1,
		vfe31_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_IRQ_CMD);

}

static void vfe31_process_reg_update_irq(void)
{
	unsigned long flags;

	if (vfe31_ctrl->recording_state == VFE_STATE_START_REQUESTED) {
		if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		} else if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		}
		vfe31_ctrl->recording_state = VFE_STATE_STARTED;
		msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
	} else if (vfe31_ctrl->recording_state ==
		VFE_STATE_STOP_REQUESTED) {
		if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
		} else if (vfe31_ctrl->operation_mode ==
			VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out1.ch1]);
		}
		CDBG("stop video triggered .\n");
	}

	if (vfe31_ctrl->start_ack_pending == TRUE) {
		vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_START_ACK);
		vfe31_ctrl->start_ack_pending = FALSE;
	} else {
		if (vfe31_ctrl->recording_state ==
			VFE_STATE_STOP_REQUESTED) {
			vfe31_ctrl->recording_state = VFE_STATE_STOPPED;
			/* request a reg update and send STOP_REC_ACK
			 * when we process the next reg update irq.
			 */
			msm_camera_io_w_mb(1,
			vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		} else if (vfe31_ctrl->recording_state ==
			VFE_STATE_STOPPED) {
			vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_STOP_REC_ACK);
			vfe31_ctrl->recording_state = VFE_STATE_IDLE;
		}
		spin_lock_irqsave(&vfe31_ctrl->update_ack_lock, flags);
		if (vfe31_ctrl->update_ack_pending == TRUE) {
			vfe31_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&vfe31_ctrl->update_ack_lock, flags);
			vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
				&vfe31_ctrl->update_ack_lock, flags);
		}
	}

	if (vfe31_ctrl->liveshot_state == VFE_STATE_START_REQUESTED) {
		pr_info("%s enabling liveshot output\n", __func__);
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, vfe31_ctrl->vfebase +
			vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
			vfe31_ctrl->liveshot_state = VFE_STATE_STARTED;
		}
	}

	if (vfe31_ctrl->liveshot_state == VFE_STATE_STARTED) {
		vfe31_ctrl->vfe_capture_count--;
		if (!vfe31_ctrl->vfe_capture_count)
			vfe31_ctrl->liveshot_state = VFE_STATE_STOP_REQUESTED;
		msm_camera_io_w_mb(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} else if (vfe31_ctrl->liveshot_state == VFE_STATE_STOP_REQUESTED) {
		CDBG("%s: disabling liveshot output\n", __func__);
		if (vfe31_ctrl->outpath.output_mode &
			VFE31_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(0, vfe31_ctrl->vfebase +
				vfe31_AXI_WM_CFG[vfe31_ctrl->outpath.out0.ch1]);
			vfe31_ctrl->liveshot_state = VFE_STATE_STOPPED;
			msm_camera_io_w_mb(1, vfe31_ctrl->vfebase +
				VFE_REG_UPDATE_CMD);
		}
	} else if (vfe31_ctrl->liveshot_state == VFE_STATE_STOPPED) {
		vfe31_ctrl->liveshot_state = VFE_STATE_IDLE;
	}

	if ((vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB)) {
		/* in snapshot mode */
		/* later we need to add check for live snapshot mode. */
		if (vfe31_ctrl->frame_skip_pattern & (0x1 <<
			(vfe31_ctrl->snapshot_frame_cnt %
				vfe31_ctrl->frame_skip_cnt))) {
			/* if last frame to be captured: */
			if (vfe31_ctrl->vfe_capture_count == 0) {
				/* stop the bus output:write master enable = 0*/
				if (vfe31_ctrl->outpath.output_mode &
					VFE31_OUTPUT_MODE_PRIMARY) {
					msm_camera_io_w(0, vfe31_ctrl->vfebase +
						vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out0.ch0]);
					msm_camera_io_w(0, vfe31_ctrl->vfebase +
						vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out0.ch1]);
				}
				if (vfe31_ctrl->outpath.output_mode &
					VFE31_OUTPUT_MODE_SECONDARY) {
					msm_camera_io_w(0, vfe31_ctrl->vfebase +
						vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out1.ch0]);
					msm_camera_io_w(0, vfe31_ctrl->vfebase +
						vfe31_AXI_WM_CFG[vfe31_ctrl->
						outpath.out1.ch1]);
				}
				msm_camera_io_w_mb
				(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);
				vfe31_ctrl->snapshot_frame_cnt = -1;
				vfe31_ctrl->frame_skip_cnt = 31;
				vfe31_ctrl->frame_skip_pattern = 0xffffffff;
			} /*if snapshot count is 0*/
		} /*if frame is not being dropped*/
		/* then do reg_update. */
		msm_camera_io_w(1, vfe31_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} /* if snapshot mode. */
}

static void vfe31_set_default_reg_values(void)
{
	msm_camera_io_w(0x800080, vfe31_ctrl->vfebase + VFE_DEMUX_GAIN_0);
	msm_camera_io_w(0x800080, vfe31_ctrl->vfebase + VFE_DEMUX_GAIN_1);
	/* What value should we program CGC_OVERRIDE to? */
	msm_camera_io_w(0xFFFFF, vfe31_ctrl->vfebase + VFE_CGC_OVERRIDE);

	/* default frame drop period and pattern */
	msm_camera_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_CFG);
	msm_camera_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_CFG);
	msm_camera_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_PATTERN);
	msm_camera_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y);
	msm_camera_io_w(0x1f, vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR);
	msm_camera_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe31_ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR_PATTERN);
	msm_camera_io_w(0, vfe31_ctrl->vfebase + VFE_CLAMP_MIN);
	msm_camera_io_w(0xFFFFFF, vfe31_ctrl->vfebase + VFE_CLAMP_MAX);

	/* stats UB config */
	msm_camera_io_w(0x3980007,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AEC_UB_CFG);
	msm_camera_io_w(0x3A00007,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AF_UB_CFG);
	msm_camera_io_w(0x3A8000F,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_AWB_UB_CFG);
	msm_camera_io_w(0x3B80007,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_RS_UB_CFG);
	msm_camera_io_w(0x3C0001F,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_CS_UB_CFG);
	msm_camera_io_w(0x3E0001F,
		vfe31_ctrl->vfebase + VFE_BUS_STATS_HIST_UB_CFG);
}

static void vfe31_process_reset_irq(void)
{
	unsigned long flags;

	atomic_set(&vfe31_ctrl->vstate, 0);

	spin_lock_irqsave(&vfe31_ctrl->stop_flag_lock, flags);
	if (vfe31_ctrl->stop_ack_pending) {
		vfe31_ctrl->stop_ack_pending = FALSE;
		spin_unlock_irqrestore(&vfe31_ctrl->stop_flag_lock, flags);
		vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(&vfe31_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe31_set_default_reg_values();

		/* reload all write masters. (frame & line)*/
		msm_camera_io_w(0x7FFF, vfe31_ctrl->vfebase + VFE_BUS_CMD);
		vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_RESET_ACK);
	}
}

static void vfe31_process_camif_sof_irq(void)
{
	if (vfe31_ctrl->operation_mode ==
		VFE_OUTPUTS_RAW) {
		if (vfe31_ctrl->start_ack_pending) {
			vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_START_ACK);
			vfe31_ctrl->start_ack_pending = FALSE;
		}
		vfe31_ctrl->vfe_capture_count--;
		/* if last frame to be captured: */
		if (vfe31_ctrl->vfe_capture_count == 0) {
			/* Ensure the write order while writing
			 to the command register using the barrier */
			msm_camera_io_w_mb(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				vfe31_ctrl->vfebase + VFE_CAMIF_COMMAND);
		}
	} /* if raw snapshot mode. */
	if ((vfe31_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe31_ctrl->operation_mode == VFE_MODE_OF_OPERATION_VIDEO) &&
		(vfe31_ctrl->vfeFrameId % vfe31_ctrl->hfr_mode != 0)) {
		vfe31_ctrl->vfeFrameId++;
		CDBG("Skip the SOF notification when HFR enabled\n");
		return;
	}
	vfe31_ctrl->vfeFrameId++;
	vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_SOF_ACK);
	CDBG("camif_sof_irq, frameId = %d\n", vfe31_ctrl->vfeFrameId);

	if (vfe31_ctrl->sync_timer_state) {
		if (vfe31_ctrl->sync_timer_repeat_count == 0)
			vfe31_sync_timer_stop();
		else
			vfe31_ctrl->sync_timer_repeat_count--;
	}
	if ((vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG) ||
		(vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB)) {
		if (vfe31_ctrl->frame_skip_pattern & (0x1 <<
			(vfe31_ctrl->snapshot_frame_cnt %
				vfe31_ctrl->frame_skip_cnt))) {
			vfe31_ctrl->vfe_capture_count--;
		}
		vfe31_ctrl->snapshot_frame_cnt++;
	}
}

static void vfe31_process_error_irq(uint32_t errStatus)
{
	uint32_t reg_value, read_val;

	if (errStatus & VFE31_IMASK_CAMIF_ERROR) {
		pr_err("vfe31_irq: camif errors\n");
		reg_value = msm_camera_io_r(
				vfe31_ctrl->vfebase + VFE_CAMIF_STATUS);
		pr_err("camifStatus  = 0x%x\n", reg_value);
		vfe31_send_isp_msg(vfe31_ctrl, MSG_ID_CAMIF_ERROR);
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

	if (errStatus & VFE31_IMASK_STATS_AE_BG_BUS_OVFL)
		pr_err("vfe31_irq: ae/bg stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_AF_BF_BUS_OVFL)
		pr_err("vfe31_irq: af/bf stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_AWB_BUS_OVFL)
		pr_err("vfe31_irq: awb stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_RS_BUS_OVFL)
		pr_err("vfe31_irq: rs stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_CS_BUS_OVFL)
		pr_err("vfe31_irq: cs stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_IHIST_BUS_OVFL)
		pr_err("vfe31_irq: ihist stats bus overflow\n");

	if (errStatus & VFE31_IMASK_STATS_SKIN_BHIST_BUS_OVFL)
		pr_err("vfe31_irq: skin/bhist stats bus overflow\n");

	if (errStatus & VFE31_IMASK_AXI_ERROR) {
		pr_err("vfe31_irq: axi error\n");
		/* read status too when overflow happens.*/
		read_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);
		pr_debug("VFE_BUS_PING_PONG_STATUS = 0x%x\n", read_val);
		read_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_OPERATION_STATUS);
		pr_debug("VFE_BUS_OPERATION_STATUS = 0x%x\n", read_val);
		read_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_0);
		pr_debug("VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_0 = 0x%x\n",
			read_val);
		read_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_1);
		pr_debug("VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_1 = 0x%x\n",
			read_val);
		read_val = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_AXI_STATUS);
		pr_debug("VFE_AXI_STATUS = 0x%x\n", read_val);
	}
}
static void vfe_send_outmsg(struct v4l2_subdev *sd, uint8_t msgid,
	uint32_t ch0_paddr, uint32_t ch1_paddr,
	uint32_t ch2_paddr, uint32_t image_mode)
{
	struct isp_msg_output msg;

	msg.output_id		= msgid;
	msg.buf.image_mode	= image_mode;
	msg.buf.ch_paddr[0]	= ch0_paddr;
	msg.buf.ch_paddr[1]	= ch1_paddr;
	msg.buf.ch_paddr[2]	= ch2_paddr;
	msg.frameCounter	= vfe31_ctrl->vfeFrameId;

	v4l2_subdev_notify(&vfe31_ctrl->subdev,
		NOTIFY_VFE_MSG_OUT, &msg);
	return;
}

static void vfe31_process_output_path_irq_0(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe31_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_PRIMARY);

	/* we render frames in the following conditions:
	 * 1. Continuous mode and the free buffer is avaialable.
	 * 2. In snapshot shot mode, free buffer is not always available.
	 * when pending snapshot count is <=1,  then no need to use
	 * free buffer.
	 */
	out_bool = ((vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_THUMB_AND_JPEG ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe31_ctrl->operation_mode == VFE_OUTPUTS_RAW ||
		vfe31_ctrl->liveshot_state == VFE_STATE_STARTED ||
		vfe31_ctrl->liveshot_state == VFE_STATE_STOP_REQUESTED ||
		vfe31_ctrl->liveshot_state == VFE_STATE_STOPPED) &&
		(vfe31_ctrl->vfe_capture_count <= 1)) || free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Channel 0*/
		ch0_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch0);
		/* Channel 1*/
		ch1_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch1);
		/* Channel 2*/
		ch2_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch2);

		CDBG("output path 0, ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out0.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe31_put_ch_addr(ping_pong,
					vfe31_ctrl->outpath.out0.ch2,
					free_buf->ch_paddr[2]);
		}
		if (vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_JPEG ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			vfe31_ctrl->liveshot_state == VFE_STATE_STOPPED)
			vfe31_ctrl->outpath.out0.capture_cnt--;

		vfe_send_outmsg(&vfe31_ctrl->subdev,
			MSG_ID_OUTPUT_PRIMARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			vfe31_ctrl->outpath.out0.image_mode);

		if (vfe31_ctrl->liveshot_state == VFE_STATE_STOPPED)
			vfe31_ctrl->liveshot_state = VFE_STATE_IDLE;

	} else {
		vfe31_ctrl->outpath.out0.frame_drop_cnt++;
		CDBG("path_irq_0 - no free buffer!\n");
	}
}

static void vfe31_process_output_path_irq_1(void)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe31_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_SECONDARY);
	out_bool = ((vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB) &&
			(vfe31_ctrl->vfe_capture_count <= 1)) || free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(vfe31_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		ch0_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		ch1_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch1);
		ch2_paddr = vfe31_get_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch2);

		pr_debug("%s ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			__func__, ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe31_put_ch_addr(ping_pong,
			vfe31_ctrl->outpath.out1.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe31_put_ch_addr(ping_pong,
					vfe31_ctrl->outpath.out1.ch2,
					free_buf->ch_paddr[2]);
		}
		if (vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			vfe31_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB)
			vfe31_ctrl->outpath.out1.capture_cnt--;

		vfe_send_outmsg(&vfe31_ctrl->subdev,
			MSG_ID_OUTPUT_SECONDARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			vfe31_ctrl->outpath.out1.image_mode);
	} else {
		vfe31_ctrl->outpath.out1.frame_drop_cnt++;
		CDBG("path_irq_1 - no free buffer!\n");
	}
}

static uint32_t  vfe31_process_stats_irq_common(uint32_t statsNum,
	uint32_t newAddr)
{

	uint32_t pingpongStatus;
	uint32_t returnAddr;
	uint32_t pingpongAddr;

	/* must be 0=ping, 1=pong */
	pingpongStatus =
		((msm_camera_io_r(vfe31_ctrl->vfebase +
		VFE_BUS_PING_PONG_STATUS))
		& ((uint32_t)(1<<(statsNum + 7)))) >> (statsNum + 7);
	/* stats bits starts at 7 */
	CDBG("statsNum %d, pingpongStatus %d\n", statsNum, pingpongStatus);
	pingpongAddr =
		((uint32_t)(vfe31_ctrl->vfebase +
		VFE_BUS_STATS_PING_PONG_BASE)) +
		(3*statsNum)*4 + (1-pingpongStatus)*4;
	returnAddr = msm_camera_io_r((uint32_t *)pingpongAddr);
	msm_camera_io_w(newAddr, (uint32_t *)pingpongAddr);
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
	msgStats.frameCounter = vfe31_ctrl->vfeFrameId;
	msgStats.buffer = bufAddress;

	switch (statsNum) {
	case STATS_AE_NUM:{
		msgStats.id = MSG_ID_STATS_AEC;
		spin_lock_irqsave(&vfe31_ctrl->aec_ack_lock, flags);
		vfe31_ctrl->aecStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->aec_ack_lock, flags);
		}
		break;
	case STATS_AF_NUM:{
		msgStats.id = MSG_ID_STATS_AF;
		spin_lock_irqsave(&vfe31_ctrl->af_ack_lock, flags);
		vfe31_ctrl->afStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->af_ack_lock, flags);
		}
		break;
	case STATS_AWB_NUM: {
		msgStats.id = MSG_ID_STATS_AWB;
		spin_lock_irqsave(&vfe31_ctrl->awb_ack_lock, flags);
		vfe31_ctrl->awbStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->awb_ack_lock, flags);
		}
		break;

	case STATS_IHIST_NUM: {
		msgStats.id = MSG_ID_STATS_IHIST;
		spin_lock_irqsave(&vfe31_ctrl->ihist_ack_lock, flags);
		vfe31_ctrl->ihistStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->ihist_ack_lock, flags);
		}
		break;
	case STATS_RS_NUM: {
		msgStats.id = MSG_ID_STATS_RS;
		spin_lock_irqsave(&vfe31_ctrl->rs_ack_lock, flags);
		vfe31_ctrl->rsStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->rs_ack_lock, flags);
		}
		break;
	case STATS_CS_NUM: {
		msgStats.id = MSG_ID_STATS_CS;
		spin_lock_irqsave(&vfe31_ctrl->cs_ack_lock, flags);
		vfe31_ctrl->csStatsControl.ackPending = TRUE;
		spin_unlock_irqrestore(&vfe31_ctrl->cs_ack_lock, flags);
		}
		break;

	default:
		goto stats_done;
	}

	v4l2_subdev_notify(&vfe31_ctrl->subdev,
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

	msgStats.frame_id = vfe31_ctrl->vfeFrameId;
	msgStats.status_bits = status_bits;

	msgStats.aec.buff = vfe31_ctrl->aecStatsControl.bufToRender;
	msgStats.awb.buff = vfe31_ctrl->awbStatsControl.bufToRender;
	msgStats.af.buff = vfe31_ctrl->afStatsControl.bufToRender;

	msgStats.ihist.buff = vfe31_ctrl->ihistStatsControl.bufToRender;
	msgStats.rs.buff = vfe31_ctrl->rsStatsControl.bufToRender;
	msgStats.cs.buff = vfe31_ctrl->csStatsControl.bufToRender;

	temp = msm_camera_io_r(vfe31_ctrl->vfebase + VFE_STATS_AWB_SGW_CFG);
	msgStats.awb_ymin = (0xFF00 & temp) >> 8;

	v4l2_subdev_notify(&vfe31_ctrl->subdev,
		NOTIFY_VFE_MSG_COMP_STATS, &msgStats);
}

static void vfe31_process_stats_ae_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe31_ctrl->aec_ack_lock, flags);
	if (!(vfe31_ctrl->aecStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe31_ctrl->aec_ack_lock, flags);
		vfe31_ctrl->aecStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_AE_NUM,
			vfe31_ctrl->aecStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->aecStatsControl.bufToRender,
			STATS_AE_NUM);
	} else{
		spin_unlock_irqrestore(&vfe31_ctrl->aec_ack_lock, flags);
		vfe31_ctrl->aecStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->aecStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats_awb_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe31_ctrl->awb_ack_lock, flags);
	if (!(vfe31_ctrl->awbStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe31_ctrl->awb_ack_lock, flags);
		vfe31_ctrl->awbStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_AWB_NUM,
			vfe31_ctrl->awbStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->awbStatsControl.bufToRender,
			STATS_AWB_NUM);
	} else{
		spin_unlock_irqrestore(&vfe31_ctrl->awb_ack_lock, flags);
		vfe31_ctrl->awbStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->awbStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats_af_irq(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vfe31_ctrl->af_ack_lock, flags);
	if (!(vfe31_ctrl->afStatsControl.ackPending)) {
		spin_unlock_irqrestore(&vfe31_ctrl->af_ack_lock, flags);
		vfe31_ctrl->afStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_AF_NUM,
			vfe31_ctrl->afStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->afStatsControl.bufToRender,
			STATS_AF_NUM);
	} else{
		spin_unlock_irqrestore(&vfe31_ctrl->af_ack_lock, flags);
		vfe31_ctrl->afStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->afStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats_ihist_irq(void)
{
	if (!(vfe31_ctrl->ihistStatsControl.ackPending)) {
		vfe31_ctrl->ihistStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_IHIST_NUM,
			vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->ihistStatsControl.bufToRender,
			STATS_IHIST_NUM);
	} else {
		vfe31_ctrl->ihistStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->ihistStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats_rs_irq(void)
{
	if (!(vfe31_ctrl->rsStatsControl.ackPending)) {
		vfe31_ctrl->rsStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_RS_NUM,
			vfe31_ctrl->rsStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->rsStatsControl.bufToRender,
			STATS_RS_NUM);
	} else {
		vfe31_ctrl->rsStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->rsStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats_cs_irq(void)
{
	if (!(vfe31_ctrl->csStatsControl.ackPending)) {
		vfe31_ctrl->csStatsControl.bufToRender =
			vfe31_process_stats_irq_common(STATS_CS_NUM,
			vfe31_ctrl->csStatsControl.nextFrameAddrBuf);

		vfe_send_stats_msg(vfe31_ctrl->csStatsControl.bufToRender,
			STATS_CS_NUM);
	} else {
		vfe31_ctrl->csStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe31_ctrl->csStatsControl.droppedStatsFrameCount);
	}
}

static void vfe31_process_stats(uint32_t status_bits)
{
	unsigned long flags;
	int32_t process_stats = false;
	CDBG("%s, stats = 0x%x\n", __func__, status_bits);

	spin_lock_irqsave(&vfe31_ctrl->comp_stats_ack_lock, flags);
	if (status_bits & VFE_IRQ_STATUS0_STATS_AEC) {
		if (!vfe31_ctrl->aecStatsControl.ackPending) {
			vfe31_ctrl->aecStatsControl.ackPending = TRUE;
			vfe31_ctrl->aecStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_AE_NUM,
				vfe31_ctrl->aecStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe31_ctrl->aecStatsControl.bufToRender = 0;
			vfe31_ctrl->aecStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe31_ctrl->aecStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_AWB) {
		if (!vfe31_ctrl->awbStatsControl.ackPending) {
			vfe31_ctrl->awbStatsControl.ackPending = TRUE;
			vfe31_ctrl->awbStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_AWB_NUM,
				vfe31_ctrl->awbStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else{
			vfe31_ctrl->awbStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->awbStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->awbStatsControl.bufToRender = 0;
	}


	if (status_bits & VFE_IRQ_STATUS0_STATS_AF) {
		if (!vfe31_ctrl->afStatsControl.ackPending) {
			vfe31_ctrl->afStatsControl.ackPending = TRUE;
			vfe31_ctrl->afStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_AF_NUM,
				vfe31_ctrl->afStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->afStatsControl.bufToRender = 0;
			vfe31_ctrl->afStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe31_ctrl->afStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_IHIST) {
		if (!vfe31_ctrl->ihistStatsControl.ackPending) {
			vfe31_ctrl->ihistStatsControl.ackPending = TRUE;
			vfe31_ctrl->ihistStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_IHIST_NUM,
				vfe31_ctrl->ihistStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->ihistStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->ihistStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->ihistStatsControl.bufToRender = 0;
	}

	if (status_bits & VFE_IRQ_STATUS0_STATS_RS) {
		if (!vfe31_ctrl->rsStatsControl.ackPending) {
			vfe31_ctrl->rsStatsControl.ackPending = TRUE;
			vfe31_ctrl->rsStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_RS_NUM,
				vfe31_ctrl->rsStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->rsStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->rsStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->rsStatsControl.bufToRender = 0;
	}


	if (status_bits & VFE_IRQ_STATUS0_STATS_CS) {
		if (!vfe31_ctrl->csStatsControl.ackPending) {
			vfe31_ctrl->csStatsControl.ackPending = TRUE;
			vfe31_ctrl->csStatsControl.bufToRender =
				vfe31_process_stats_irq_common(STATS_CS_NUM,
				vfe31_ctrl->csStatsControl.nextFrameAddrBuf);
			process_stats = true;
		} else {
			vfe31_ctrl->csStatsControl.droppedStatsFrameCount++;
			vfe31_ctrl->csStatsControl.bufToRender = 0;
		}
	} else {
		vfe31_ctrl->csStatsControl.bufToRender = 0;
	}

	spin_unlock_irqrestore(&vfe31_ctrl->comp_stats_ack_lock, flags);
	if (process_stats)
		vfe_send_comp_stats_msg(status_bits);

	return;
}

static void vfe31_process_stats_irq(uint32_t *irqstatus)
{
	uint32_t status_bits = VFE_COM_STATUS & *irqstatus;

	if ((vfe31_ctrl->hfr_mode != HFR_MODE_OFF) &&
		(vfe31_ctrl->vfeFrameId % vfe31_ctrl->hfr_mode != 0)) {
		CDBG("Skip the stats when HFR enabled\n");
		return;
	}

	vfe31_process_stats(status_bits);
	return;
}

static void vfe31_do_tasklet(unsigned long data)
{
	unsigned long flags;

	struct vfe31_isr_queue_cmd *qcmd = NULL;

	CDBG("=== vfe31_do_tasklet start ===\n");

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

		if (qcmd->vfeInterruptStatus0 &
			VFE_IRQ_STATUS0_CAMIF_SOF_MASK) {
			CDBG("irq	camifSofIrq\n");
			vfe31_process_camif_sof_irq();
		}
		/* interrupt to be processed,  *qcmd has the payload.  */
		if (qcmd->vfeInterruptStatus0 &
			VFE_IRQ_STATUS0_REG_UPDATE_MASK) {
			CDBG("irq	regUpdateIrq\n");
			vfe31_process_reg_update_irq();
		}

		if (qcmd->vfeInterruptStatus1 &
			VFE_IMASK_WHILE_STOPPING_1) {
			CDBG("irq	resetAckIrq\n");
			vfe31_process_reset_irq();
		}

		if (atomic_read(&vfe31_ctrl->vstate)) {
			if (qcmd->vfeInterruptStatus1 &
				VFE31_IMASK_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe31_process_error_irq(
					qcmd->vfeInterruptStatus1 &
					VFE31_IMASK_ERROR_ONLY_1);
			}
			/* next, check output path related interrupts. */
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK) {
				CDBG("Image composite done 0 irq occured.\n");
				vfe31_process_output_path_irq_0();
			}
			if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK) {
				CDBG("Image composite done 1 irq occured.\n");
				vfe31_process_output_path_irq_1();
			}
			/* in snapshot mode if done then send
			snapshot done message */
			if (vfe31_ctrl->operation_mode ==
					VFE_OUTPUTS_THUMB_AND_MAIN ||
				vfe31_ctrl->operation_mode ==
					VFE_OUTPUTS_MAIN_AND_THUMB ||
				vfe31_ctrl->operation_mode ==
					VFE_OUTPUTS_THUMB_AND_JPEG ||
				vfe31_ctrl->operation_mode ==
					VFE_OUTPUTS_JPEG_AND_THUMB ||
				vfe31_ctrl->operation_mode ==
					VFE_OUTPUTS_RAW) {
				if ((vfe31_ctrl->outpath.out0.capture_cnt == 0)
					&& (vfe31_ctrl->outpath.out1.
					capture_cnt == 0)) {
					msm_camera_io_w_mb(
						CAMIF_COMMAND_STOP_IMMEDIATELY,
						vfe31_ctrl->vfebase +
						VFE_CAMIF_COMMAND);
					vfe31_send_isp_msg(vfe31_ctrl,
						MSG_ID_SNAPSHOT_DONE);
				}
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
					VFE_IRQ_STATUS0_STATS_AEC) {
					CDBG("Stats AEC irq occured.\n");
					vfe31_process_stats_ae_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_AWB) {
					CDBG("Stats AWB irq occured.\n");
					vfe31_process_stats_awb_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_AF) {
					CDBG("Stats AF irq occured.\n");
					vfe31_process_stats_af_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_IHIST) {
					CDBG("Stats IHIST irq occured.\n");
					vfe31_process_stats_ihist_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_RS) {
					CDBG("Stats RS irq occured.\n");
					vfe31_process_stats_rs_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_CS) {
					CDBG("Stats CS irq occured.\n");
					vfe31_process_stats_cs_irq();
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_SYNC_TIMER0) {
					CDBG("SYNC_TIMER 0 irq occured.\n");
					vfe31_send_isp_msg(vfe31_ctrl,
						MSG_ID_SYNC_TIMER0_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_SYNC_TIMER1) {
					CDBG("SYNC_TIMER 1 irq occured.\n");
					vfe31_send_isp_msg(vfe31_ctrl,
						MSG_ID_SYNC_TIMER1_DONE);
				}
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_SYNC_TIMER2) {
					CDBG("SYNC_TIMER 2 irq occured.\n");
					vfe31_send_isp_msg(vfe31_ctrl,
						MSG_ID_SYNC_TIMER2_DONE);
				}
			}
		}
		kfree(qcmd);
	}
	CDBG("=== vfe31_do_tasklet end ===\n");
}

DECLARE_TASKLET(vfe31_tasklet, vfe31_do_tasklet, 0);

static irqreturn_t vfe31_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	struct vfe31_irq_status irq;
	struct vfe31_isr_queue_cmd *qcmd;

	CDBG("vfe_parse_irq\n");

	vfe31_read_irq_status(&irq);

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

	spin_lock_irqsave(&vfe31_ctrl->stop_flag_lock, flags);
	if (vfe31_ctrl->stop_ack_pending) {
		irq.vfeIrqStatus0 &= VFE_IMASK_WHILE_STOPPING_0;
		irq.vfeIrqStatus1 &= VFE_IMASK_WHILE_STOPPING_1;
	}
	spin_unlock_irqrestore(&vfe31_ctrl->stop_flag_lock, flags);

	CDBG("vfe_parse_irq: Irq_status0 = 0x%x, Irq_status1 = 0x%x.\n",
		irq.vfeIrqStatus0, irq.vfeIrqStatus1);

	qcmd->vfeInterruptStatus0 = irq.vfeIrqStatus0;
	qcmd->vfeInterruptStatus1 = irq.vfeIrqStatus1;

	spin_lock_irqsave(&vfe31_ctrl->tasklet_lock, flags);
	list_add_tail(&qcmd->list, &vfe31_ctrl->tasklet_q);

	atomic_add(1, &irq_cnt);
	spin_unlock_irqrestore(&vfe31_ctrl->tasklet_lock, flags);
	tasklet_schedule(&vfe31_tasklet);
	return IRQ_HANDLED;
}

static long msm_vfe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int subdev_cmd, void *arg)
{
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
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
		if (NULL != cmd->value) {
			if (copy_from_user(&vfecmd,
				(void __user *)(cmd->value),
				sizeof(vfecmd))) {
				pr_err("%s %d: copy_from_user failed\n",
					__func__, __LINE__);
				return -EFAULT;
			}
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
		default:
			pr_err("%s Unsupported cmd type %d",
				__func__, cmd->cmd_type);
			break;
		}
		goto vfe31_config_done;
	}
	switch (cmd->cmd_type) {
	case CMD_GENERAL: {
		rc = vfe31_proc_general(pmctl, &vfecmd);
		}
		break;
	case CMD_CONFIG_PING_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe31_output_ch *outch = vfe31_get_ch(path);
		outch->ping = *((struct msm_free_buf *)data);
		}
		break;

	case CMD_CONFIG_PONG_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe31_output_ch *outch = vfe31_get_ch(path);
		outch->pong = *((struct msm_free_buf *)data);
		}
		break;

	case CMD_CONFIG_FREE_BUF_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe31_output_ch *outch = vfe31_get_ch(path);
		outch->free_buf = *((struct msm_free_buf *)data);
		}
		break;

	case CMD_SNAP_BUF_RELEASE:
		break;

	case CMD_STATS_AEC_BUF_RELEASE: {
		vfe31_stats_aec_ack(sack);
		}
		break;

	case CMD_STATS_AF_BUF_RELEASE: {
		vfe31_stats_af_ack(sack);
		}
		break;

	case CMD_STATS_AWB_BUF_RELEASE: {
		vfe31_stats_awb_ack(sack);
		}
		break;

	case CMD_STATS_IHIST_BUF_RELEASE: {
		vfe31_stats_ihist_ack(sack);
		}
		break;

	case CMD_STATS_RS_BUF_RELEASE: {
		vfe31_stats_rs_ack(sack);
		}
		break;

	case CMD_STATS_CS_BUF_RELEASE: {
		vfe31_stats_cs_ack(sack);
		}
		break;

	case CMD_AXI_CFG_PRIM: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_PRIM, axio);
		kfree(axio);
		}
		break;

	case CMD_AXI_CFG_PRIM_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_PRIM_ALL_CHNLS, axio);
		kfree(axio);
		}
		break;

	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_PRIM|OUTPUT_SEC, axio);
		kfree(axio);
		}
		break;

	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS, axio);
		kfree(axio);
		}
		break;

	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe31_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe31_config_axi(OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC, axio);
		kfree(axio);
		}
		break;

	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC_ALL_CHNLS: {
		pr_err("%s Invalid/Unsupported AXI configuration %x",
			__func__, cmd->cmd_type);
		}
		break;

	case CMD_AXI_START:
		/* No need to decouple AXI/VFE for VFE3.1*/
		break;

	case CMD_AXI_STOP:
		/* No need to decouple AXI/VFE for VFE3.1*/
		break;

	default:
		pr_err("%s Unsupported AXI configuration %x ", __func__,
			cmd->cmd_type);
		break;
	}
vfe31_config_done:
	kfree(scfg);
	kfree(sack);
	CDBG("%s done: rc = %d\n", __func__, (int) rc);
	return rc;
}

static int msm_vfe_subdev_s_crystal_freq(struct v4l2_subdev *sd,
	u32 freq, u32 flags)
{
	int rc = 0;
	int round_rate;

	round_rate = clk_round_rate(vfe31_ctrl->vfe_clk[0], freq);
	if (rc < 0) {
		pr_err("%s: clk_round_rate failed %d\n",
			__func__, rc);
		return rc;
	}

	vfe_clk_rate = round_rate;
	rc = clk_set_rate(vfe31_ctrl->vfe_clk[0], round_rate);
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

static struct msm_cam_clk_info vfe_clk_info[] = {
	{"vfe_clk", VFE_CLK_RATE},
	{"vfe_pclk", -1},
};

static struct msm_cam_clk_info vfe_camif_clk_info[] = {
	{"camif_pad_pclk", -1},
	{"vfe_camif_clk", -1},
};

static void msm_vfe_camio_clk_sel(enum msm_camio_clk_src_type srctype)
{
	struct clk *clk = NULL;

	clk = vfe31_ctrl->vfe_clk[0];

	if (clk != NULL) {
		switch (srctype) {
		case MSM_CAMIO_CLK_SRC_INTERNAL:
			clk_set_flags(clk, 0x00000100 << 1);
			break;

		case MSM_CAMIO_CLK_SRC_EXTERNAL:
			clk_set_flags(clk, 0x00000100);
			break;

		default:
			break;
		}
	}
}

static void msm_vfe_camif_pad_reg_reset(void)
{
	uint32_t reg;

	msm_vfe_camio_clk_sel(MSM_CAMIO_CLK_SRC_INTERNAL);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe31_ctrl->camifbase)) & CAMIF_CFG_RMSK;
	reg |= 0x3;
	msm_camera_io_w(reg, vfe31_ctrl->camifbase);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe31_ctrl->camifbase)) & CAMIF_CFG_RMSK;
	reg |= 0x10;
	msm_camera_io_w(reg, vfe31_ctrl->camifbase);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe31_ctrl->camifbase)) & CAMIF_CFG_RMSK;
	/* Need to be uninverted*/
	reg &= 0x03;
	msm_camera_io_w(reg, vfe31_ctrl->camifbase);
	usleep_range(10000, 15000);
}

int msm_vfe_subdev_init(struct v4l2_subdev *sd,
		struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	v4l2_set_subdev_hostdata(sd, mctl);

	spin_lock_init(&vfe31_ctrl->stop_flag_lock);
	spin_lock_init(&vfe31_ctrl->state_lock);
	spin_lock_init(&vfe31_ctrl->io_lock);
	spin_lock_init(&vfe31_ctrl->update_ack_lock);
	spin_lock_init(&vfe31_ctrl->tasklet_lock);

	spin_lock_init(&vfe31_ctrl->aec_ack_lock);
	spin_lock_init(&vfe31_ctrl->awb_ack_lock);
	spin_lock_init(&vfe31_ctrl->af_ack_lock);
	spin_lock_init(&vfe31_ctrl->ihist_ack_lock);
	spin_lock_init(&vfe31_ctrl->rs_ack_lock);
	spin_lock_init(&vfe31_ctrl->cs_ack_lock);
	spin_lock_init(&vfe31_ctrl->comp_stats_ack_lock);
	spin_lock_init(&vfe31_ctrl->sd_notify_lock);
	INIT_LIST_HEAD(&vfe31_ctrl->tasklet_q);

	vfe31_ctrl->update_linear = false;
	vfe31_ctrl->update_rolloff = false;
	vfe31_ctrl->update_la = false;
	vfe31_ctrl->update_gamma = false;
	vfe31_ctrl->hfr_mode = HFR_MODE_OFF;

	vfe31_ctrl->vfebase = ioremap(vfe31_ctrl->vfemem->start,
		resource_size(vfe31_ctrl->vfemem));
	if (!vfe31_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto vfe_remap_failed;
	}
	if (!mctl->sdata->csi_if) {
		vfe31_ctrl->camifbase = ioremap(vfe31_ctrl->camifmem->start,
			resource_size(vfe31_ctrl->camifmem));
		if (!vfe31_ctrl->camifbase) {
			rc = -ENOMEM;
			pr_err("%s: camif ioremap failed\n", __func__);
			goto camif_remap_failed;
		}
	}

	if (vfe31_ctrl->fs_vfe) {
		rc = regulator_enable(vfe31_ctrl->fs_vfe);
		if (rc) {
			pr_err("%s: Regulator FS_VFE enable failed\n",
							__func__);
			goto vfe_fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&vfe31_ctrl->pdev->dev, vfe_clk_info,
		vfe31_ctrl->vfe_clk, ARRAY_SIZE(vfe_clk_info), 1);
	if (rc < 0)
		goto vfe_clk_enable_failed;

	if (!mctl->sdata->csi_if) {
		rc = msm_cam_clk_enable(&vfe31_ctrl->pdev->dev,
			vfe_camif_clk_info,
			vfe31_ctrl->vfe_camif_clk,
			ARRAY_SIZE(vfe_camif_clk_info), 1);
		if (rc < 0)
			goto vfe_clk_enable_failed;
		msm_vfe_camif_pad_reg_reset();
	}

	msm_camio_bus_scale_cfg(
		mctl->sdata->pdata->cam_bus_scale_table, S_INIT);
	msm_camio_bus_scale_cfg(
		mctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);
	vfe31_ctrl->register_total = VFE31_REGISTER_TOTAL;

	enable_irq(vfe31_ctrl->vfeirq->start);

	return rc;

vfe_clk_enable_failed:
	regulator_disable(vfe31_ctrl->fs_vfe);
vfe_fs_failed:
	if (!mctl->sdata->csi_if)
		iounmap(vfe31_ctrl->camifbase);
camif_remap_failed:
	iounmap(vfe31_ctrl->vfebase);
vfe_remap_failed:
	disable_irq(vfe31_ctrl->vfeirq->start);
	return rc;
}

void msm_vfe_subdev_release(struct v4l2_subdev *sd)
{
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	disable_irq(vfe31_ctrl->vfeirq->start);
	tasklet_kill(&vfe31_tasklet);

	if (!pmctl->sdata->csi_if)
		msm_cam_clk_enable(&vfe31_ctrl->pdev->dev,
			vfe_camif_clk_info,
			vfe31_ctrl->vfe_camif_clk,
			ARRAY_SIZE(vfe_camif_clk_info), 0);

	msm_cam_clk_enable(&vfe31_ctrl->pdev->dev, vfe_clk_info,
		vfe31_ctrl->vfe_clk, ARRAY_SIZE(vfe_clk_info), 0);

	if (vfe31_ctrl->fs_vfe)
		regulator_disable(vfe31_ctrl->fs_vfe);

	CDBG("%s Releasing resources\n", __func__);
	if (!pmctl->sdata->csi_if)
		iounmap(vfe31_ctrl->camifbase);
	iounmap(vfe31_ctrl->vfebase);

	if (atomic_read(&irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_EXIT);
}

static const struct v4l2_subdev_internal_ops msm_vfe_internal_ops;

static int __devinit vfe31_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cam_subdev_info sd_info;

	CDBG("%s: device id = %d\n", __func__, pdev->id);

	vfe31_ctrl = kzalloc(sizeof(struct vfe31_ctrl_type), GFP_KERNEL);
	if (!vfe31_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&vfe31_ctrl->subdev, &msm_vfe_subdev_ops);
	vfe31_ctrl->subdev.internal_ops = &msm_vfe_internal_ops;
	vfe31_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(vfe31_ctrl->subdev.name,
			 sizeof(vfe31_ctrl->subdev.name), "vfe3.1");
	v4l2_set_subdevdata(&vfe31_ctrl->subdev, vfe31_ctrl);
	platform_set_drvdata(pdev, &vfe31_ctrl->subdev);

	vfe31_ctrl->vfemem = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "msm_vfe");
	if (!vfe31_ctrl->vfemem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vfe31_no_resource;
	}
	vfe31_ctrl->vfeirq = platform_get_resource_byname(pdev,
		IORESOURCE_IRQ, "msm_vfe");
	if (!vfe31_ctrl->vfeirq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vfe31_no_resource;
	}
	vfe31_ctrl->camifmem = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "msm_camif");
	if (!vfe31_ctrl->camifmem)
		pr_err("%s: camif not supported\n", __func__);

	vfe31_ctrl->vfeio = request_mem_region(vfe31_ctrl->vfemem->start,
		resource_size(vfe31_ctrl->vfemem), pdev->name);
	if (!vfe31_ctrl->vfeio) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto vfe31_no_resource;
	}

	if (vfe31_ctrl->camifmem) {
		vfe31_ctrl->camifio = request_mem_region(
			vfe31_ctrl->camifmem->start,
			resource_size(vfe31_ctrl->camifmem), pdev->name);
		if (!vfe31_ctrl->camifio) {
			release_mem_region(vfe31_ctrl->vfemem->start,
				resource_size(vfe31_ctrl->vfemem));
			pr_err("%s: no valid mem region\n", __func__);
			rc = -EBUSY;
			goto vfe31_no_resource;
		}
	}

	rc = request_irq(vfe31_ctrl->vfeirq->start, vfe31_parse_irq,
		IRQF_TRIGGER_RISING, "vfe", 0);
	if (rc < 0) {
		if (vfe31_ctrl->camifmem) {
			release_mem_region(vfe31_ctrl->camifmem->start,
				resource_size(vfe31_ctrl->camifmem));
		}
		release_mem_region(vfe31_ctrl->vfemem->start,
			resource_size(vfe31_ctrl->vfemem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto vfe31_no_resource;
	}

	disable_irq(vfe31_ctrl->vfeirq->start);

	vfe31_ctrl->pdev = pdev;
	vfe31_ctrl->fs_vfe = regulator_get(&vfe31_ctrl->pdev->dev, "vdd");
	if (IS_ERR(vfe31_ctrl->fs_vfe)) {
		pr_err("%s: Regulator get failed %ld\n", __func__,
			PTR_ERR(vfe31_ctrl->fs_vfe));
		vfe31_ctrl->fs_vfe = NULL;
	}

	sd_info.sdev_type = VFE_DEV;
	sd_info.sd_index = 0;
	sd_info.irq_num = vfe31_ctrl->vfeirq->start;
	msm_cam_register_subdev_node(&vfe31_ctrl->subdev, &sd_info);
	return 0;

vfe31_no_resource:
	kfree(vfe31_ctrl);
	return 0;
}

static struct platform_driver vfe31_driver = {
	.probe = vfe31_probe,
	.driver = {
	.name = MSM_VFE_DRV_NAME,
	.owner = THIS_MODULE,
	},
};

static int __init msm_vfe31_init_module(void)
{
	return platform_driver_register(&vfe31_driver);
}

static void __exit msm_vfe31_exit_module(void)
{
	platform_driver_unregister(&vfe31_driver);
}

module_init(msm_vfe31_init_module);
module_exit(msm_vfe31_exit_module);
MODULE_DESCRIPTION("VFE 3.1 driver");
MODULE_LICENSE("GPL v2");
