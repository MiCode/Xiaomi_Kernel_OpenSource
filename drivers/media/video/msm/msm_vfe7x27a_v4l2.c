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

#include <linux/msm_adsp.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/android_pmem.h>
#include <linux/slab.h>
#include <linux/pm_qos_params.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>
#include <mach/msm_adsp.h>
#include <mach/clk.h>
#include <mach/camera.h>
#include "msm_vfe7x27a_v4l2.h"
#include "msm.h"

/* ADSP Messages */
#define MSG_RESET_ACK  0
#define MSG_STOP_ACK  1
#define MSG_SNAPSHOT  2
#define MSG_ILLEGAL_COMMAND  3
#define MSG_START_ACK  4
#define MSG_UPDATE_ACK  5
#define MSG_OUTPUT1  6
#define MSG_OUTPUT2  7
#define MSG_STATS_AF  8
#define MSG_STATS_WE  9
#define MSG_STATS_HISTOGRAM  10
#define MSG_EPOCH1  11
#define MSG_EPOCH2  12
#define MSG_VFE_ERROR 13
#define MSG_SYNC_TIMER1_DONE  14
#define MSG_SYNC_TIMER2_DONE  15
#define MSG_ASYNC_TIMER1_DONE  16
#define MSG_ASYNC_TIMER2_DONE  17
#define MSG_CAPTURE_COMPLETE  18
#define MSG_TABLE_CMD_ACK  19
#define MSG_EXP_TIMEOUT_ACK  20
#define MSG_SOF  21
#define MSG_OUTPUT_T  22
#define MSG_OUTPUT_S  23

#define VFE_ADSP_EVENT 0xFFFF
#define SNAPSHOT_MASK_MODE 0x00000001
#define MSM_AXI_QOS_PREVIEW	122000
#define MSM_AXI_QOS_SNAPSHOT	192000


#define QDSP_CMDQUEUE 25
#define QDSP_SCALEQUEUE 26
#define QDSP_TABLEQUEUE 27

/* ADSP Scler queue Cmd IDs */
#define VFE_SCALE_OUTPUT1_CONFIG  0
#define VFE_SCALE_OUTPUT2_CONFIG  1
#define VFE_SCALE_MAX  0xFFFFFFFF

/* ADSP table queue Cmd IDs */
#define VFE_AXI_INPUT_CONFIG  0
#define VFE_AXI_OUTPUT_CONFIG  1
#define VFE_RGB_GAMMA_CONFIG  2
#define VFE_Y_GAMMA_CONFIG  3
#define VFE_ROLL_OFF_CONFIG  4
#define VFE_DEMOSAICv3_BPC_CFG  6
#define VFE_DEMOSAICv3_ABF_CFG  7
#define VFE_DEMOSAICv3_CFG  8
#define VFE_MAX  0xFFFFFFFF

/* ADSP cfg queue cmd IDs */
#define VFE_RESET  0
#define VFE_START  1
#define VFE_STOP  2
#define VFE_UPDATE  3
#define VFE_CAMIF_CONFIG  4
#define VFE_ACTIVE_REGION_CONFIG  5
#define VFE_DEMOSAIC_CONFIG  6
#define VFE_INPUT_FORMAT_CONFIG  7
#define VFE_OUTPUT_CLAMP_CONFIG  8
#define VFE_CHROMA_SUBSAMPLE_CONFIG  9
#define VFE_BLACK_LEVEL_CONFIG  10
#define VFE_WHITE_BALANCE_CONFIG  11
#define VFE_COLOR_PROCESSING_CONFIG  12
#define VFE_ADAPTIVE_FILTER_CONFIG  13
#define VFE_FRAME_SKIP_CONFIG  14
#define VFE_FOV_CROP  15
#define VFE_STATS_AUTOFOCUS_CONFIG  16
#define VFE_STATS_WB_EXP_CONFIG  17
#define VFE_STATS_HISTOGRAM_CONFIG  18
#define VFE_OUTPUT1_ACK  19
#define VFE_OUTPUT2_ACK  20
#define VFE_STATS_AUTOFOCUS_ACK  21
#define VFE_STATS_WB_EXP_ACK  22
#define VFE_EPOCH1_ACK  23
#define VFE_EPOCH2_ACK  24
#define VFE_UPDATE_CAMIF_FRAME_CONFIG  25
#define VFE_SYNC_TIMER1_CONFIG  26
#define VFE_SYNC_TIMER2_CONFIG  27
#define VFE_ASYNC_TIMER1_START  28
#define VFE_ASYNC_TIMER2_START  29
#define VFE_STATS_AUTOFOCUS_UPDATE  30
#define VFE_STATS_WB_EXP_UPDATE  31
#define VFE_ROLL_OFF_UPDATE  33
#define VFE_DEMOSAICv3_BPC_UPDATE  34
#define VFE_TESTGEN_START  35
#define VFE_STATS_MA  0xFFFFFFFF

struct msg_id_map msgs_map[] = {
	{MSG_RESET_ACK, MSG_ID_RESET_ACK},
	{MSG_STOP_ACK, MSG_ID_STOP_ACK},
	{MSG_SNAPSHOT, MSG_ID_SNAPSHOT_DONE},
	{MSG_ILLEGAL_COMMAND, VFE_MAX},
	{MSG_START_ACK, MSG_ID_START_ACK},
	{MSG_UPDATE_ACK, MSG_ID_UPDATE_ACK},
	{MSG_OUTPUT1, VFE_MAX},
	{MSG_OUTPUT2, VFE_MAX},
	{MSG_STATS_AF, MSG_ID_STATS_AF},
	{MSG_STATS_WE, MSG_ID_STATS_AWB_AEC},
	{MSG_STATS_HISTOGRAM, MSG_ID_STATS_IHIST},
	{MSG_EPOCH1, MSG_ID_EPOCH1},
	{MSG_EPOCH2, MSG_ID_EPOCH2},
	{MSG_VFE_ERROR, MSG_ID_CAMIF_ERROR},
	{MSG_SYNC_TIMER1_DONE, MSG_ID_SYNC_TIMER1_DONE},
	{MSG_SYNC_TIMER2_DONE, MSG_ID_SYNC_TIMER2_DONE},
	{MSG_ASYNC_TIMER1_DONE, MSG_ID_ASYNC_TIMER1_DONE},
	{MSG_ASYNC_TIMER2_DONE, MSG_ID_ASYNC_TIMER2_DONE},
	{MSG_CAPTURE_COMPLETE, MSG_CAPTURE_COMPLETE},
	{MSG_TABLE_CMD_ACK, MSG_TABLE_CMD_ACK},
	{MSG_EXP_TIMEOUT_ACK, MSG_EXP_TIMEOUT_ACK},
	{MSG_SOF, MSG_ID_SOF_ACK},
	{MSG_OUTPUT_T, MSG_ID_OUTPUT_T},
	{MSG_OUTPUT_S, MSG_ID_OUTPUT_S},
};

struct cmd_id_map cmds_map[] = {
	{VFE_CMD_DUMMY_0, VFE_MAX, VFE_MAX},
	{VFE_CMD_SET_CLK, VFE_MAX, VFE_MAX},
	{VFE_CMD_RESET, VFE_RESET, QDSP_CMDQUEUE,
			"VFE_CMD_RESET", "VFE_RESET"},
	{VFE_CMD_START, VFE_START, QDSP_CMDQUEUE,
			"VFE_CMD_START", "VFE_START"},
	{VFE_CMD_TEST_GEN_START, VFE_TESTGEN_START, QDSP_CMDQUEUE,
		"VFE_CMD_TEST_GEN_START", "VFE_TESTGEN_START"},
	{VFE_CMD_OPERATION_CFG, VFE_MAX , VFE_MAX},
	{VFE_CMD_AXI_OUT_CFG, VFE_AXI_OUTPUT_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_AXI_OUT_CFG", "VFE_AXI_OUTPUT_CONFIG"},
	{VFE_CMD_CAMIF_CFG, VFE_CAMIF_CONFIG, QDSP_CMDQUEUE,
			"VFE_CMD_CAMIF_CFG", "VFE_CAMIF_CONFIG"},
	{VFE_CMD_AXI_INPUT_CFG, VFE_AXI_INPUT_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_AXI_INPUT_CFG", "VFE_AXI_INPUT_CONFIG"},
	{VFE_CMD_BLACK_LEVEL_CFG, VFE_BLACK_LEVEL_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_BLACK_LEVEL_CFG", "VFE_BLACK_LEVEL_CONFIG"},
	{VFE_CMD_MESH_ROLL_OFF_CFG, VFE_ROLL_OFF_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_MESH_ROLL_OFF_CFG", "VFE_ROLL_OFF_CONFIG"},
	{VFE_CMD_DEMUX_CFG, VFE_INPUT_FORMAT_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_DEMUX_CFG", "VFE_INPUT_FORMAT_CONFIG"},
	{VFE_CMD_FOV_CFG, VFE_FOV_CROP, QDSP_CMDQUEUE,
		"VFE_CMD_FOV_CFG", "VFE_FOV_CROP"},
	{VFE_CMD_MAIN_SCALER_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_WB_CFG, VFE_WHITE_BALANCE_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_WB_CFG", "VFE_WHITE_BALANCE_CONFIG"},
	{VFE_CMD_COLOR_COR_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_RGB_G_CFG, VFE_RGB_GAMMA_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_RGB_G_CFG", "VFE_RGB_GAMMA_CONFIG"},
	{VFE_CMD_LA_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_CHROMA_EN_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_CHROMA_SUP_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_MCE_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_SK_ENHAN_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_ASF_CFG, VFE_ADAPTIVE_FILTER_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_ASF_CFG", "VFE_ADAPTIVE_FILTER_CONFIG"},
	{VFE_CMD_S2Y_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_S2CbCr_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_CHROMA_SUBS_CFG, VFE_CHROMA_SUBSAMPLE_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_CHROMA_SUBS_CFG", "VFE_CHROMA_SUBSAMPLE_CONFIG"},
	{VFE_CMD_OUT_CLAMP_CFG, VFE_OUTPUT_CLAMP_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_OUT_CLAMP_CFG", "VFE_OUTPUT_CLAMP_CONFIG"},
	{VFE_CMD_FRAME_SKIP_CFG, VFE_FRAME_SKIP_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_FRAME_SKIP_CFG", "VFE_FRAME_SKIP_CONFIG"},
	{VFE_CMD_DUMMY_1, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_2, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_3, VFE_MAX, VFE_MAX},
	{VFE_CMD_UPDATE, VFE_UPDATE, QDSP_CMDQUEUE,
		"VFE_CMD_UPDATE", "VFE_UPDATE"},
	{VFE_CMD_BL_LVL_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMUX_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_FOV_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_MAIN_SCALER_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_WB_UPDATE, VFE_WHITE_BALANCE_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_WB_UPDATE", "VFE_WHITE_BALANCE_CONFIG"},
	{VFE_CMD_COLOR_COR_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_RGB_G_UPDATE, VFE_RGB_GAMMA_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_RGB_G_UPDATE", "VFE_RGB_GAMMA_CONFIG"},
	{VFE_CMD_LA_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_CHROMA_EN_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_CHROMA_SUP_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_MCE_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_SK_ENHAN_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_S2CbCr_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_S2Y_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_ASF_UPDATE, VFE_ADAPTIVE_FILTER_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_ASF_UPDATE", "VFE_ADAPTIVE_FILTER_CONFIG"},
	{VFE_CMD_FRAME_SKIP_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_CAMIF_FRAME_UPDATE, VFE_UPDATE_CAMIF_FRAME_CONFIG,
		QDSP_CMDQUEUE, "VFE_CMD_CAMIF_FRAME_UPDATE",
		"VFE_UPDATE_CAMIF_FRAME_CONFIG"},
	{VFE_CMD_STATS_AF_UPDATE, VFE_STATS_AUTOFOCUS_UPDATE, QDSP_CMDQUEUE,
		"VFE_CMD_STATS_AF_UPDATE", "VFE_STATS_AUTOFOCUS_UPDATE"},
	{VFE_CMD_STATS_AE_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AWB_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_RS_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_CS_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_SKIN_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_IHIST_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_4, VFE_MAX, VFE_MAX},
	{VFE_CMD_EPOCH1_ACK, VFE_EPOCH1_ACK, QDSP_CMDQUEUE,
			"VFE_CMD_EPOCH1_ACK", "VFE_EPOCH1_ACK"},
	{VFE_CMD_EPOCH2_ACK, VFE_EPOCH2_ACK, QDSP_CMDQUEUE,
			"VFE_CMD_EPOCH2_ACK", "VFE_EPOCH2_ACK"},
	{VFE_CMD_START_RECORDING, VFE_MAX, VFE_MAX},
	{VFE_CMD_STOP_RECORDING, VFE_MAX , VFE_MAX},
	{VFE_CMD_DUMMY_5, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_6, VFE_MAX, VFE_MAX},
	{VFE_CMD_CAPTURE, VFE_START, QDSP_CMDQUEUE,
			"VFE_CMD_CAPTURE", "VFE_START"},
	{VFE_CMD_DUMMY_7, VFE_MAX, VFE_MAX},
	{VFE_CMD_STOP, VFE_STOP, QDSP_CMDQUEUE, "VFE_CMD_STOP", "VFE_STOP"},
	{VFE_CMD_GET_HW_VERSION, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_FRAME_SKIP_COUNTS, VFE_MAX, VFE_MAX},
	{VFE_CMD_OUTPUT1_BUFFER_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_OUTPUT2_BUFFER_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_OUTPUT3_BUFFER_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_JPEG_OUT_BUF_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_RAW_OUT_BUF_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_RAW_IN_BUF_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AF_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AE_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AWB_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_RS_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_CS_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_SKIN_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_IHIST_ENQ, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_8, VFE_MAX, VFE_MAX},
	{VFE_CMD_JPEG_ENC_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_9, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AF_START, VFE_STATS_AUTOFOCUS_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_STATS_AF_START", "VFE_STATS_AUTOFOCUS_CONFIG"},
	{VFE_CMD_STATS_AF_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AE_START, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AE_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AWB_START, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_AWB_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_RS_START, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_RS_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_CS_START, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_CS_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_SKIN_START, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_SKIN_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_STATS_IHIST_START, VFE_STATS_HISTOGRAM_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_STATS_IHIST_START", "VFE_STATS_HISTOGRAM_CONFIG"},
	{VFE_CMD_STATS_IHIST_STOP, VFE_MAX, VFE_MAX},
	{VFE_CMD_DUMMY_10, VFE_MAX, VFE_MAX},
	{VFE_CMD_SYNC_TIMER_SETTING, VFE_MAX, VFE_MAX},
	{VFE_CMD_ASYNC_TIMER_SETTING, VFE_MAX, VFE_MAX},
	{VFE_CMD_LIVESHOT, VFE_MAX, VFE_MAX},
	{VFE_CMD_LA_SETUP, VFE_MAX, VFE_MAX},
	{VFE_CMD_LINEARIZATION_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3, VFE_DEMOSAICv3_CFG, QDSP_TABLEQUEUE,
		"VFE_CMD_DEMOSAICV3", "VFE_DEMOSAICv3_CFG"},
	{VFE_CMD_DEMOSAICV3_ABCC_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_DBCC_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_DBPC_CFG, VFE_DEMOSAICv3_BPC_CFG, QDSP_TABLEQUEUE,
		"VFE_CMD_DEMOSAICV3_DBPC_CFG", "VFE_DEMOSAICv3_BPC_CFG"},
	{VFE_CMD_DEMOSAICV3_ABF_CFG, VFE_DEMOSAICv3_ABF_CFG, QDSP_TABLEQUEUE,
		"VFE_CMD_DEMOSAICV3_ABF_CFG", "VFE_DEMOSAICv3_ABF_CFG"},
	{VFE_CMD_DEMOSAICV3_ABCC_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_DBCC_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_DBPC_UPDATE, VFE_DEMOSAICv3_BPC_UPDATE,
		QDSP_CMDQUEUE, "VFE_CMD_DEMOSAICV3_DBPC_UPDATE",
		"VFE_DEMOSAICv3_BPC_UPDATE"},
	{VFE_CMD_XBAR_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_MODULE_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_ZSL, VFE_MAX, VFE_MAX},
	{VFE_CMD_LINEARIZATION_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_ABF_UPDATE, VFE_DEMOSAICv3_ABF_CFG,
		QDSP_TABLEQUEUE, "VFE_CMD_DEMOSAICV3_ABF_UPDATE",
		"VFE_DEMOSAICv3_ABF_CFG"},
	{VFE_CMD_CLF_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_CLF_LUMA_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_CLF_CHROMA_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_PCA_ROLL_OFF_CFG, VFE_MAX, VFE_MAX},
	{VFE_CMD_PCA_ROLL_OFF_UPDATE, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_REG_DUMP, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_LINEARIZATON_TABLE, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_MESH_ROLLOFF_TABLE, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_PCA_ROLLOFF_TABLE, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_RGB_G_TABLE, VFE_MAX, VFE_MAX},
	{VFE_CMD_GET_LA_TABLE, VFE_MAX, VFE_MAX},
	{VFE_CMD_DEMOSAICV3_UPDATE, VFE_DEMOSAICv3_CFG, QDSP_TABLEQUEUE,
		"VFE_CMD_DEMOSAICV3_UPDATE", "VFE_DEMOSAICv3_CFG"},
	{VFE_CMD_ACTIVE_REGION_CFG, VFE_ACTIVE_REGION_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_ACTIVE_REGION_CFG", "VFE_ACTIVE_REGION_CONFIG"},
	{VFE_CMD_COLOR_PROCESSING_CONFIG, VFE_COLOR_PROCESSING_CONFIG,
		QDSP_CMDQUEUE, "VFE_CMD_COLOR_PROCESSING_CONFIG",
		"VFE_COLOR_PROCESSING_CONFIG"},
	{VFE_CMD_STATS_WB_AEC_CONFIG, VFE_STATS_WB_EXP_CONFIG, QDSP_CMDQUEUE,
		"VFE_CMD_STATS_WB_AEC_CONFIG", "VFE_STATS_WB_EXP_CONFIG"},
	{VFE_CMD_STATS_WB_AEC_UPDATE, VFE_STATS_WB_EXP_UPDATE, QDSP_CMDQUEUE,
		"VFE_CMD_STATS_WB_AEC_UPDATE", "VFE_STATS_WB_EXP_UPDATE"},
	{VFE_CMD_Y_GAMMA_CONFIG, VFE_Y_GAMMA_CONFIG, QDSP_TABLEQUEUE,
		"VFE_CMD_Y_GAMMA_CONFIG", "VFE_Y_GAMMA_CONFIG"},
	{VFE_CMD_SCALE_OUTPUT1_CONFIG, VFE_SCALE_OUTPUT1_CONFIG,
		QDSP_SCALEQUEUE, "VFE_CMD_SCALE_OUTPUT1_CONFIG",
		"VFE_SCALE_OUTPUT1_CONFIG"},
	{VFE_CMD_SCALE_OUTPUT2_CONFIG, VFE_SCALE_OUTPUT2_CONFIG,
		QDSP_SCALEQUEUE, "VFE_CMD_SCALE_OUTPUT2_CONFIG",
		"VFE_SCALE_OUTPUT2_CONFIG"},
};


static struct msm_adsp_module *qcam_mod;
static struct msm_adsp_module *vfe_mod;
static void *extdata;
static uint32_t extlen;

struct mutex vfe_lock;
static void     *vfe_syncdata;
static uint8_t vfestopped;

static struct stop_event stopevent;

static uint32_t op_mode;
static uint32_t raw_mode;
static struct vfe2x_ctrl_type *vfe2x_ctrl;

static void vfe2x_send_isp_msg(
	struct vfe2x_ctrl_type *vctrl,
	uint32_t isp_msg_id)
{
	struct isp_msg_event isp_msg_evt;

	isp_msg_evt.msg_id = isp_msg_id;
	isp_msg_evt.sof_count = vfe2x_ctrl->vfeFrameId;
	v4l2_subdev_notify(&vctrl->subdev,
			NOTIFY_ISP_MSG_EVT,
			(void *)&isp_msg_evt);
}

static void vfe_send_outmsg(struct v4l2_subdev *sd, uint8_t msgid,
		uint32_t ch0_paddr, uint32_t ch1_paddr)
{
	struct isp_msg_output msg;

	msg.output_id = msgid;
	msg.buf.ch_paddr[0]     = ch0_paddr;
	msg.buf.ch_paddr[1]     = ch1_paddr;
	msg.frameCounter = vfe2x_ctrl->vfeFrameId;

	v4l2_subdev_notify(&vfe2x_ctrl->subdev,
			NOTIFY_VFE_MSG_OUT,
			&msg);
	return;
}

static void vfe_send_stats_msg(uint32_t buf_addr, uint32_t msg_id)
{
	struct isp_msg_stats msg_stats;

	msg_stats.frameCounter = vfe2x_ctrl->vfeFrameId;
	msg_stats.buffer       = buf_addr;
	msg_stats.id           = msg_id;

	v4l2_subdev_notify(&vfe2x_ctrl->subdev,
				NOTIFY_VFE_MSG_STATS,
				&msg_stats);
}

static void vfe_7x_ops(void *driver_data, unsigned id, size_t len,
		void (*getevent)(void *ptr, size_t len))
{
	uint32_t evt_buf[3];
	void *data;
	struct buf_info *outch = NULL;
	uint32_t y_phy, cbcr_phy;
	struct table_cmd *table_pending = NULL;
	unsigned long flags;
	void   *cmd_data = NULL;
	unsigned char buf[256];
	struct msm_free_buf *free_buf = NULL;
	struct vfe_outputack fack;

	CDBG("%s:id=%d\n", __func__, id);

	if (id != VFE_ADSP_EVENT) {
		data = kzalloc(len, GFP_KERNEL);
		if (!data) {
			pr_err("%s: rp: cannot allocate buffer\n", __func__);
			return;
		}
	}
	if (id == VFE_ADSP_EVENT) {
		/* event */
		getevent(evt_buf, sizeof(evt_buf));
		CDBG("%s:event:msg_id=%d\n", __func__, id);
	} else {
		/* messages */
		getevent(data, len);
		CDBG("%s:messages:msg_id=%d\n", __func__, id);

		switch (id) {
		case MSG_SNAPSHOT:
			msm_camio_set_perf_lvl(S_PREVIEW);
			vfe_7x_ops(driver_data, MSG_OUTPUT_S, len, getevent);
			if (!raw_mode)
				vfe_7x_ops(driver_data, MSG_OUTPUT_T,
						len, getevent);
			vfe2x_send_isp_msg(vfe2x_ctrl, MSG_ID_SNAPSHOT_DONE);
			return;
		case MSG_OUTPUT_S:
			outch = &vfe2x_ctrl->snap;
			y_phy = outch->ping.ch_paddr[0];
			cbcr_phy = outch->ping.ch_paddr[1];
			CDBG("MSG_OUTPUT_S: %x %x\n",
				(unsigned int)y_phy, (unsigned int)cbcr_phy);
			vfe_send_outmsg(&vfe2x_ctrl->subdev, MSG_ID_OUTPUT_S,
							y_phy, cbcr_phy);
			break;
		case MSG_OUTPUT_T:
			outch = &vfe2x_ctrl->thumb;
			y_phy = outch->ping.ch_paddr[0];
			cbcr_phy = outch->ping.ch_paddr[1];
			CDBG("MSG_OUTPUT_T: %x %x\n",
				(unsigned int)y_phy, (unsigned int)cbcr_phy);
			vfe_send_outmsg(&vfe2x_ctrl->subdev, MSG_ID_OUTPUT_T,
							y_phy, cbcr_phy);
			break;
		case MSG_OUTPUT1:
		case MSG_OUTPUT2:
			if (op_mode & SNAPSHOT_MASK_MODE) {
				kfree(data);
				return;
			} else {
				free_buf = vfe2x_check_free_buffer(
					VFE_MSG_OUTPUT_IRQ,
					VFE_MSG_OUTPUT_P);
			      CDBG("free_buf = %x\n", (unsigned int) free_buf);
			      if (free_buf) {
					fack.header = VFE_OUTPUT2_ACK;

					fack.output2newybufferaddress =
						(void *)(free_buf->ch_paddr[0]);

					fack.output2newcbcrbufferaddress =
						(void *)(free_buf->ch_paddr[1]);

					cmd_data = &fack;
					len = sizeof(fack);
					msm_adsp_write(vfe_mod, QDSP_CMDQUEUE,
							cmd_data, len);
			      } else {
					fack.header = VFE_OUTPUT2_ACK;
					fack.output2newybufferaddress =
					(void *)
				((struct vfe_endframe *)data)->y_address;
					fack.output2newcbcrbufferaddress =
					(void *)
				((struct vfe_endframe *)data)->cbcr_address;
					cmd_data = &fack;
					len = sizeof(fack);
					msm_adsp_write(vfe_mod, QDSP_CMDQUEUE,
						cmd_data, len);
				}
			}
			y_phy = ((struct vfe_endframe *)data)->y_address;
			cbcr_phy = ((struct vfe_endframe *)data)->cbcr_address;


			CDBG("vfe_7x_convert, y_phy = 0x%x, cbcr_phy = 0x%x\n",
				 y_phy, cbcr_phy);

			memcpy(((struct vfe_frame_extra *)extdata),
				&((struct vfe_endframe *)data)->extra,
				sizeof(struct vfe_frame_extra));

			vfe2x_ctrl->vfeFrameId =
				((struct vfe_frame_extra *)extdata)->frame_id;
			vfe_send_outmsg(&vfe2x_ctrl->subdev,
							MSG_ID_OUTPUT_P,
							y_phy, cbcr_phy);
			break;
		case MSG_RESET_ACK:
		case MSG_START_ACK:
		case MSG_UPDATE_ACK:
		case MSG_VFE_ERROR:
		case MSG_SYNC_TIMER1_DONE:
		case MSG_SYNC_TIMER2_DONE:
			vfe2x_send_isp_msg(vfe2x_ctrl, msgs_map[id].isp_id);
			if (id == MSG_START_ACK)
				vfe2x_ctrl->vfe_started = 1;
			break;
		case MSG_SOF:
			vfe2x_ctrl->vfeFrameId++;
			if (vfe2x_ctrl->vfeFrameId == 0)
				vfe2x_ctrl->vfeFrameId = 1; /* wrapped back */
			vfe2x_send_isp_msg(vfe2x_ctrl, MSG_ID_SOF_ACK);
			if (raw_mode)
				vfe2x_send_isp_msg(vfe2x_ctrl,
						MSG_ID_START_ACK);
			break;
		case MSG_STOP_ACK:
			stopevent.state = 1;
			vfe2x_ctrl->vfe_started = 0;
			wake_up(&stopevent.wait);
			vfe2x_send_isp_msg(vfe2x_ctrl, MSG_ID_STOP_ACK);
			break;
		case MSG_STATS_AF:
		case MSG_STATS_WE:
			vfe_send_stats_msg(*(uint32_t *)data,
						msgs_map[id].isp_id);
			break;
		default:
			vfe2x_send_isp_msg(vfe2x_ctrl, msgs_map[id].isp_id);
			break;
		}
	}
	if (MSG_TABLE_CMD_ACK == id) {
		spin_lock_irqsave(&vfe2x_ctrl->table_lock, flags);
		if (list_empty(&vfe2x_ctrl->table_q)) {
			if (vfe2x_ctrl->start_pending) {
				CDBG("Send START\n");
				cmd_data = buf;
				*(uint32_t *)cmd_data = VFE_START;
				memcpy(((char *)cmd_data) + 4,
					&vfe2x_ctrl->start_cmd,
					sizeof(vfe2x_ctrl->start_cmd));
				/* Send Start cmd here */
				len  = sizeof(vfe2x_ctrl->start_cmd) + 4;
				msm_adsp_write(vfe_mod, QDSP_CMDQUEUE,
						cmd_data, len);
				vfe2x_ctrl->start_pending = 0;
			}
			vfe2x_ctrl->tableack_pending = 0;
			spin_unlock_irqrestore(&vfe2x_ctrl->table_lock, flags);
			return;
		}
		table_pending = list_first_entry(&vfe2x_ctrl->table_q,
					struct table_cmd, list);
		if (!table_pending) {
			vfe2x_ctrl->tableack_pending = 0;
			spin_unlock_irqrestore(&vfe2x_ctrl->table_lock, flags);
			return;
		}
		msm_adsp_write(vfe_mod, table_pending->queue,
				table_pending->cmd, table_pending->size);
		list_del(&table_pending->list);
		kfree(table_pending->cmd);
		vfe2x_ctrl->tableack_pending = 1;
		spin_unlock_irqrestore(&vfe2x_ctrl->table_lock, flags);
	} else if (!vfe2x_ctrl->tableack_pending) {
		if (!list_empty(&vfe2x_ctrl->table_q))
			return;
	}
}

static struct msm_adsp_ops vfe_7x_sync = {
	.event = vfe_7x_ops,
};

static int vfe_7x_config_axi(int mode,
	struct buf_info *ad, struct axiout *ao)
{
	unsigned long *bptr;
	int    cnt;
	int rc = 0;


	if (mode == OUTPUT_1) {
		ao->output1buffer1_y_phy = ad->ping.ch_paddr[0];
		ao->output1buffer1_cbcr_phy = ad->ping.ch_paddr[1];
		ao->output1buffer2_y_phy = ad->pong.ch_paddr[0];
		ao->output1buffer2_cbcr_phy = ad->pong.ch_paddr[1];
		bptr = &ao->output1buffer3_y_phy;
		for (cnt = 0; cnt < 6; cnt++) {
			*bptr = ad->pong.ch_paddr[0];
			bptr++;
			*bptr = ad->pong.ch_paddr[1];
			bptr++;
		}
	}

	if (mode == OUTPUT_2) {
		ao->output2buffer1_y_phy = ad->ping.ch_paddr[0];
		ao->output2buffer1_cbcr_phy = ad->ping.ch_paddr[1];
		ao->output2buffer2_y_phy = ad->pong.ch_paddr[0];
		ao->output2buffer2_cbcr_phy = ad->pong.ch_paddr[1];
		ao->output2buffer3_y_phy = ad->free_buf.ch_paddr[0];
		ao->output2buffer3_cbcr_phy = ad->free_buf.ch_paddr[1];
		bptr = &ao->output2buffer4_y_phy;
		for (cnt = 0; cnt < 5; cnt++) {
			*bptr = ad->pong.ch_paddr[0];
			bptr++;
			*bptr = ad->pong.ch_paddr[1];
			bptr++;
		}
	}

	if (mode == OUTPUT_1_AND_2) {
		if (raw_mode) {
			ao->output2buffer1_y_phy = ad->ping.ch_paddr[0];
			ao->output2buffer1_cbcr_phy = ad->ping.ch_paddr[0];
			ao->output2buffer2_y_phy = ad->pong.ch_paddr[0];
			ao->output2buffer2_cbcr_phy = ad->pong.ch_paddr[0];
		} else {
			ao->output2buffer1_y_phy = ad->ping.ch_paddr[0];
			ao->output2buffer1_cbcr_phy = ad->ping.ch_paddr[1];
			ao->output2buffer2_y_phy = ad->pong.ch_paddr[0];
			ao->output2buffer2_cbcr_phy = ad->pong.ch_paddr[1];
	}
		bptr = &ao->output2buffer3_y_phy;
		for (cnt = 0; cnt < 6; cnt++) {
			*bptr = ad->pong.ch_paddr[0];
			bptr++;
			*bptr = ad->pong.ch_paddr[0];
			bptr++;
		}
	}

	return rc;
}

static void vfe2x_subdev_notify(int id, int path)
{
	struct msm_vfe_resp *rp;
	unsigned long flags = 0;
	spin_lock_irqsave(&vfe2x_ctrl->sd_notify_lock, flags);
	rp = msm_isp_sync_alloc(sizeof(struct msm_vfe_resp), GFP_ATOMIC);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}
	CDBG("vfe2x_subdev_notify : msgId = %d\n", id);
	rp->evt_msg.type   = MSM_CAMERA_MSG;
	rp->evt_msg.msg_id = path;
	rp->type	   = id;
	v4l2_subdev_notify(&vfe2x_ctrl->subdev, NOTIFY_VFE_BUF_EVT, rp);
	spin_unlock_irqrestore(&vfe2x_ctrl->sd_notify_lock, flags);
}

static struct msm_free_buf *vfe2x_check_free_buffer(int id, int path)
{
	struct buf_info *outch = NULL;

	vfe2x_subdev_notify(id, path);
	if (op_mode & SNAPSHOT_MASK_MODE) {
		if (path == VFE_MSG_OUTPUT_S)
			outch = &vfe2x_ctrl->snap;
		else if (path == VFE_MSG_OUTPUT_T)
			outch = &vfe2x_ctrl->thumb;
	} else {
		if (path == VFE_MSG_OUTPUT_P)
			outch = &vfe2x_ctrl->prev;
	}
	if (outch->free_buf.ch_paddr[0])
		return &outch->free_buf;

	return NULL;
}

static int vfe2x_configure_pingpong_buffers(int id, int path)
{
	struct buf_info *outch = NULL;
	int rc = 0;

	vfe2x_subdev_notify(id, path);

	CDBG("Opmode = %d\n", op_mode);
	if (op_mode & SNAPSHOT_MASK_MODE) {
		if (path == VFE_MSG_OUTPUT_S)
			outch = &vfe2x_ctrl->snap;
		else if (path == VFE_MSG_OUTPUT_T)
			outch = &vfe2x_ctrl->thumb;
	} else {
		if (path == VFE_MSG_OUTPUT_P)
			outch = &vfe2x_ctrl->prev;
	}
	if (outch->ping.ch_paddr[0] && outch->pong.ch_paddr[0]) {
		/* Configure Preview Ping Pong */
		CDBG("%s Configure ping/pong address for %d",
						__func__, path);
	} else {
		pr_err("%s ping/pong addr is null!!", __func__);
		rc = -EINVAL;
	}
	return rc;
}

static struct buf_info *vfe2x_get_ch(int path)
{
	struct buf_info *ch = NULL;

	CDBG("path = %d op_mode = %d\n", path, op_mode);
	if (op_mode & SNAPSHOT_MASK_MODE) {
		if (path == VFE_MSG_OUTPUT_T)
			ch = &vfe2x_ctrl->thumb;
		else if (path == VFE_MSG_OUTPUT_S)
			ch = &vfe2x_ctrl->snap;
	} else {
		if (path == VFE_MSG_OUTPUT_P)
			ch = &vfe2x_ctrl->prev;
	}

	BUG_ON(ch == NULL);
	return ch;
}

static long msm_vfe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int subdev_cmd, void *arg)
{
	struct msm_isp_cmd vfecmd;
	struct msm_camvfe_params *vfe_params =
		(struct msm_camvfe_params *)arg;
	struct msm_vfe_cfg_cmd *cmd = vfe_params->vfe_cfg;
	struct table_cmd *table_pending;
	long rc = 0;
	void *data = vfe_params->data;

	struct msm_pmem_region *regptr;
	unsigned char buf[256];

	struct vfe_stats_ack sack;
	struct axidata *axid;
	uint32_t i;
	uint32_t header = 0;
	uint32_t queue = 0;
	struct vfe_stats_we_cfg *scfg = NULL;
	struct vfe_stats_af_cfg *sfcfg = NULL;

	struct axiout *axio = NULL;
	void   *cmd_data = NULL;
	void   *cmd_data_alloc = NULL;
	unsigned long flags;
	struct msm_free_buf *free_buf = NULL;
	struct vfe_outputack fack;

	CDBG("msm_vfe_subdev_ioctl is called\n");
	if (cmd->cmd_type != CMD_FRAME_BUF_RELEASE &&
	    cmd->cmd_type != CMD_STATS_BUF_RELEASE &&
	    cmd->cmd_type != CMD_STATS_AF_BUF_RELEASE &&
		cmd->cmd_type != CMD_CONFIG_PING_ADDR &&
		cmd->cmd_type != CMD_CONFIG_PONG_ADDR &&
		cmd->cmd_type != CMD_CONFIG_FREE_BUF_ADDR &&
		cmd->cmd_type != CMD_VFE_BUFFER_RELEASE) {
		if (copy_from_user(&vfecmd,
				(void __user *)(cmd->value),
				sizeof(vfecmd))) {
			pr_err("copy_from_user in msm_vfe_subdev_ioctl fail\n");
			return -EFAULT;
		}
	}

	switch (cmd->cmd_type) {
	case CMD_VFE_BUFFER_RELEASE: {
		if (!(vfe2x_ctrl->vfe_started) || op_mode == 1)
			return 0;
		if (op_mode & SNAPSHOT_MASK_MODE) {
			free_buf = vfe2x_check_free_buffer(
					VFE_MSG_OUTPUT_IRQ,
					VFE_MSG_OUTPUT_T);
		} else {
			free_buf = vfe2x_check_free_buffer(
					VFE_MSG_OUTPUT_IRQ,
					VFE_MSG_OUTPUT_P);
			if (free_buf) {
				fack.header = VFE_OUTPUT2_ACK;

				fack.output2newybufferaddress =
						(void *)(free_buf->ch_paddr[0]);

				fack.output2newcbcrbufferaddress =
						(void *)(free_buf->ch_paddr[1]);

				cmd_data = &fack;
				vfecmd.length = sizeof(fack) - 4;
				queue = QDSP_CMDQUEUE;
			}
		}
	}
	break;
	case CMD_CONFIG_PING_ADDR: {
		int path = *((int *)cmd->value);
		struct buf_info *outch = vfe2x_get_ch(path);
		outch->ping = *((struct msm_free_buf *)data);
	}
		return 0;
	case CMD_CONFIG_PONG_ADDR: {
		int path = *((int *)cmd->value);
		struct buf_info *outch = vfe2x_get_ch(path);
		outch->pong = *((struct msm_free_buf *)data);
	}
		return 0;

	case CMD_CONFIG_FREE_BUF_ADDR: {
		int path = *((int *)cmd->value);
		struct buf_info *outch = vfe2x_get_ch(path);
		outch->free_buf = *((struct msm_free_buf *)data);
	}
		return 0;

	case CMD_STATS_AEC_AWB_ENABLE:
	case CMD_STATS_AXI_CFG: {
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			goto config_failure;
		}

		scfg =
			kmalloc(sizeof(struct vfe_stats_we_cfg),
				GFP_ATOMIC);
		if (!scfg) {
			rc = -ENOMEM;
			goto config_failure;
		}

		if (copy_from_user((char *)scfg + 4,
					(void __user *)(vfecmd.value),
					vfecmd.length)) {

			rc = -EFAULT;
			goto config_done;
		}

		CDBG("STATS_ENABLE: bufnum = %d, enabling = %d\n",
			axid->bufnum1, scfg->wb_expstatsenable);

		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_failure;
		}
		*(uint32_t *)scfg = header;
		if (axid->bufnum1 > 0) {
			regptr = axid->region;

			for (i = 0; i < axid->bufnum1; i++) {

				CDBG("STATS_ENABLE, phy = 0x%lx\n",
					regptr->paddr);

				scfg->wb_expstatoutputbuffer[i] =
					(void *)regptr->paddr;
				regptr++;
			}

			cmd_data = scfg;

		} else {
			rc = -EINVAL;
			goto config_done;
		}
	}
		break;
	case CMD_STATS_AF_ENABLE:
	case CMD_STATS_AF_AXI_CFG: {
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			goto config_failure;
		}

		sfcfg =
			kmalloc(sizeof(struct vfe_stats_af_cfg),
				GFP_ATOMIC);

		if (!sfcfg) {
			rc = -ENOMEM;
			goto config_failure;
		}

		if (copy_from_user((char *)sfcfg + 4,
					(void __user *)(vfecmd.value),
					vfecmd.length)) {

			rc = -EFAULT;
			goto config_done;
		}

		CDBG("AF_ENABLE: bufnum = %d, enabling = %d\n",
			axid->bufnum1, sfcfg->af_enable);

		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_failure;
		}
		*(uint32_t *)sfcfg = header;
		if (axid->bufnum1 > 0) {
			regptr = &axid->region[0];

			for (i = 0; i < axid->bufnum1; i++) {

				CDBG("STATS_ENABLE, phy = 0x%lx\n",
					regptr->paddr);

				sfcfg->af_outbuf[i] =
					(void *)regptr->paddr;

				regptr++;
			}

			cmd_data = sfcfg;

		} else {
			rc = -EINVAL;
			goto config_done;
		}
	}
		break;
	case CMD_SNAP_BUF_RELEASE:
		break;
	case CMD_STATS_BUF_RELEASE: {
		CDBG("vfe_7x_config: CMD_STATS_BUF_RELEASE\n");
		if (!data) {
			rc = -EFAULT;
			goto config_failure;
		}

		sack.header = VFE_STATS_WB_EXP_ACK;
		sack.bufaddr = (void *)*(uint32_t *)data;

		queue  = QDSP_CMDQUEUE;
		vfecmd.length = sizeof(struct vfe_stats_ack) - 4;
		cmd_data = &sack;
	}
		break;
	case CMD_STATS_AF_BUF_RELEASE: {
		CDBG("vfe_7x_config: CMD_STATS_AF_BUF_RELEASE\n");
		if (!data) {
			rc = -EFAULT;
			goto config_failure;
		}

		sack.header = VFE_STATS_AUTOFOCUS_ACK;
		sack.bufaddr = (void *)*(uint32_t *)data;

		queue  = QDSP_CMDQUEUE;
		vfecmd.length = sizeof(struct vfe_stats_ack) - 4;
		cmd_data = &sack;
	}
		break;
	case CMD_GENERAL:
	case CMD_STATS_DISABLE: {
		CDBG("CMD_GENERAL:%d %d\n", vfecmd.id, vfecmd.length);
		if (vfecmd.id == VFE_CMD_OPERATION_CFG) {
			if (copy_from_user(&vfe2x_ctrl->start_cmd,
						(void __user *)(vfecmd.value),
							vfecmd.length))
				rc = -EFAULT;
			op_mode = vfe2x_ctrl->start_cmd.mode_of_operation;
			return rc;
		}
		if (vfecmd.length > 256 - 4) {
			cmd_data_alloc =
			cmd_data = kmalloc(vfecmd.length + 4, GFP_ATOMIC);
			if (!cmd_data) {
				rc = -ENOMEM;
				goto config_failure;
			}
		} else
			cmd_data = buf;

		if (copy_from_user(((char *)cmd_data) + 4,
					(void __user *)(vfecmd.value),
					vfecmd.length)) {

			rc = -EFAULT;
			goto config_done;
		}
		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_done;
		}
		CDBG("%s %s\n", cmds_map[vfecmd.id].isp_id_name,
			cmds_map[vfecmd.id].vfe_id_name);
		*(uint32_t *)cmd_data = header;
		if (queue == QDSP_CMDQUEUE) {
			switch (vfecmd.id) {
			case VFE_CMD_RESET:
				msm_adsp_enable(qcam_mod);
				msm_adsp_enable(vfe_mod);
				msm_camio_vfe_blk_reset();
				vfestopped = 0;
				break;
			case VFE_CMD_START:
			case VFE_CMD_CAPTURE:
				spin_lock_irqsave(&vfe2x_ctrl->table_lock,
									flags);
				if (!list_empty(&vfe2x_ctrl->table_q)) {
					vfe2x_ctrl->start_pending = 1;
					spin_unlock_irqrestore(
						&vfe2x_ctrl->table_lock,
								flags);
					return 0;
				}
				spin_unlock_irqrestore(&vfe2x_ctrl->table_lock,
									flags);
				vfecmd.length = sizeof(vfe2x_ctrl->start_cmd);
				memcpy(((char *)cmd_data) + 4,
					&vfe2x_ctrl->start_cmd,
					sizeof(vfe2x_ctrl->start_cmd));
				if (op_mode & SNAPSHOT_MASK_MODE)
					msm_camio_set_perf_lvl(S_CAPTURE);
				else
					msm_camio_set_perf_lvl(S_PREVIEW);
				vfestopped = 0;
				break;
			case VFE_CMD_STOP:
				vfestopped = 1;
				vfe2x_ctrl->vfe_started = 0;
				goto config_send;

			default:
				break;
			}
		} /* QDSP_CMDQUEUE */
	}
		break;
	case CMD_AXI_CFG_PREVIEW: {
		CDBG("CMD_AXI_CFG_PREVIEW\n");
		raw_mode = 0;
		axio = kmalloc(sizeof(struct axiout), GFP_ATOMIC);
		if (!axio) {
			pr_err("NULL axio\n");
			rc = -ENOMEM;
			goto config_failure;
		}

		if (copy_from_user((char *)axio + 4,
					(void __user *)(vfecmd.value),
					sizeof(struct axiout))) {
			pr_err("copy_from_user failed\n");
			rc = -EFAULT;
			goto config_done;
		}
		rc = vfe2x_configure_pingpong_buffers(VFE_MSG_V2X_PREVIEW,
							VFE_MSG_OUTPUT_P);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto config_done;
		}

		free_buf = vfe2x_check_free_buffer(
					VFE_MSG_OUTPUT_IRQ,
					VFE_MSG_OUTPUT_P);
		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_done;
		}
		*(uint32_t *)axio = header;
		vfe_7x_config_axi(OUTPUT_2, &vfe2x_ctrl->prev, axio);
		cmd_data = axio;
	}
		break;
	case CMD_RAW_PICT_AXI_CFG: {
		CDBG("CMD_RAW_PICT_AXI_CFG:%d\n", op_mode);
		raw_mode = 1;
		axio = kmalloc(sizeof(struct axiout), GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			goto config_failure;
		}

		if (copy_from_user((char *)axio + 4,
					(void __user *)(vfecmd.value),
					sizeof(struct axiout))) {
			rc = -EFAULT;
			goto config_done;
		}
		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		rc = vfe2x_configure_pingpong_buffers(VFE_MSG_V2X_CAPTURE,
							VFE_MSG_OUTPUT_S);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto config_done;
		}
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_done;
		}
		*(uint32_t *)axio = header;
		vfe_7x_config_axi(OUTPUT_1_AND_2, &vfe2x_ctrl->snap, axio);
		cmd_data = axio;
	}
		break;
	case CMD_AXI_CFG_SNAP: {
		raw_mode = 0;
		CDBG("CMD_AXI_CFG_SNAP :%d\n", op_mode);
		axio = kmalloc(sizeof(struct axiout), GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			goto config_failure;
		}

		if (copy_from_user((char *)axio + 4,
					(void __user *)(vfecmd.value),
					sizeof(struct axiout))) {
			rc = -EFAULT;
			goto config_done;
		}
		header = cmds_map[vfecmd.id].vfe_id;
		queue = cmds_map[vfecmd.id].queue;
		rc = vfe2x_configure_pingpong_buffers(VFE_MSG_V2X_CAPTURE,
							VFE_MSG_OUTPUT_S);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto config_done;
		}
		if (header == -1 && queue == -1) {
			rc = -EFAULT;
			goto config_done;
		}
		*(uint32_t *)axio = header;
		vfe_7x_config_axi(OUTPUT_1_AND_2, &vfe2x_ctrl->snap, axio);
		rc = vfe2x_configure_pingpong_buffers(VFE_MSG_V2X_CAPTURE,
							VFE_MSG_OUTPUT_T);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers"
				" for preview", __func__);
			rc = -EINVAL;
			goto config_done;
		}
		vfe_7x_config_axi(OUTPUT_1, &vfe2x_ctrl->thumb, axio);
		cmd_data = axio;
	}
		break;
	default:
		break;
	}

	if (vfestopped)
		goto config_done;

config_send:
	CDBG("send adsp command = %d\n", *(uint32_t *)cmd_data);
	if (queue == QDSP_TABLEQUEUE &&
			vfe2x_ctrl->tableack_pending) {
		table_pending = kzalloc(sizeof(struct table_cmd), GFP_ATOMIC);
		if (!table_pending) {
			rc = -ENOMEM;
			goto config_done;
		}
		table_pending->cmd = kzalloc(vfecmd.length + 4, GFP_ATOMIC);
		if (!table_pending->cmd) {
			kfree(table_pending);
			rc = -ENOMEM;
			goto config_done;
		}
		memcpy(table_pending->cmd, cmd_data, vfecmd.length + 4);
		table_pending->queue = queue;
		table_pending->size = vfecmd.length + 4;
		spin_lock_irqsave(&vfe2x_ctrl->table_lock, flags);
		list_add_tail(&table_pending->list, &vfe2x_ctrl->table_q);
		spin_unlock_irqrestore(&vfe2x_ctrl->table_lock, flags);
	} else {
		if (queue == QDSP_TABLEQUEUE) {
			spin_lock_irqsave(&vfe2x_ctrl->table_lock, flags);
			rc = msm_adsp_write(vfe_mod, queue,
				cmd_data, vfecmd.length + 4);
			vfe2x_ctrl->tableack_pending = 1;
			spin_unlock_irqrestore(&vfe2x_ctrl->table_lock, flags);
		} else {
			if (*(uint32_t *)cmd_data == VFE_OUTPUT2_ACK) {
				uint32_t *ptr = cmd_data;
				CDBG("%x %x %x\n", ptr[0], ptr[1], ptr[2]);
			}
			rc = msm_adsp_write(vfe_mod, queue,
				cmd_data, vfecmd.length + 4);
			CDBG("%x\n", vfecmd.length + 4);
		}
	}

config_done:
	kfree(cmd_data_alloc);

config_failure:
	kfree(scfg);
	kfree(axio);
	return rc;
}

static struct msm_cam_clk_info vfe2x_clk_info[] = {
	{"vfe_clk", 192000000},
};

int msm_vfe_subdev_init(struct v4l2_subdev *sd, void *data,
	struct platform_device *pdev)
{
	int rc = 0;
	v4l2_set_subdev_hostdata(sd, data);
	vfe_syncdata = data;

	spin_lock_init(&vfe2x_ctrl->sd_notify_lock);
	spin_lock_init(&vfe2x_ctrl->table_lock);
	spin_lock_init(&vfe2x_ctrl->vfe_msg_lock);
	init_waitqueue_head(&stopevent.wait);
	INIT_LIST_HEAD(&vfe2x_ctrl->table_q);
	INIT_LIST_HEAD(&vfe2x_ctrl->vfe_msg_q);
	stopevent.timeout = 200;
	stopevent.state = 0;
	vfe2x_ctrl->vfe_started = 0;


	CDBG("msm_cam_clk_enable: enable vfe_clk\n");
	rc = msm_cam_clk_enable(&vfe2x_ctrl->pdev->dev, vfe2x_clk_info,
			vfe2x_ctrl->vfe_clk, ARRAY_SIZE(vfe2x_clk_info), 1);
	if (rc < 0)
		return rc;

	msm_camio_set_perf_lvl(S_INIT);

	/* TODO : check is it required */
	extlen = sizeof(struct vfe_frame_extra);

	extdata = kmalloc(extlen, GFP_ATOMIC);
	if (!extdata) {
		rc = -ENOMEM;
		goto init_fail;
	}

	rc = msm_adsp_get("QCAMTASK", &qcam_mod, &vfe_7x_sync, NULL);
	if (rc) {
		rc = -EBUSY;
		goto get_qcam_fail;
	}

	rc = msm_adsp_get("VFETASK", &vfe_mod, &vfe_7x_sync, NULL);
	if (rc) {
		rc = -EBUSY;
		goto get_vfe_fail;
	}
	return 0;

get_vfe_fail:
	msm_adsp_put(qcam_mod);
get_qcam_fail:
	kfree(extdata);
init_fail:
	extlen = 0;
	return rc;
}

int msm_vpe_subdev_init(struct v4l2_subdev *sd, void *data,
	struct platform_device *pdev)
{
	return 0;
}

void msm_vpe_subdev_release(struct platform_device *pdev)
{
	return;
}

void msm_vfe_subdev_release(struct platform_device *pdev)
{
	msm_cam_clk_enable(&vfe2x_ctrl->pdev->dev, vfe2x_clk_info,
			vfe2x_ctrl->vfe_clk, ARRAY_SIZE(vfe2x_clk_info), 0);
	vfe_syncdata = NULL;

	msm_adsp_disable(qcam_mod);
	msm_adsp_disable(vfe_mod);

	msm_adsp_put(qcam_mod);
	msm_adsp_put(vfe_mod);

	kfree(extdata);
	msm_camio_set_perf_lvl(S_EXIT);
	return;
}

static const struct v4l2_subdev_core_ops msm_vfe_subdev_core_ops = {
	.ioctl = msm_vfe_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_vfe_subdev_ops = {
	.core = &msm_vfe_subdev_core_ops,
};

static int __devinit vfe2x_probe(struct platform_device *pdev)
{
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	vfe2x_ctrl = kzalloc(sizeof(struct vfe2x_ctrl_type), GFP_KERNEL);
	if (!vfe2x_ctrl) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&vfe2x_ctrl->subdev, &msm_vfe_subdev_ops);
	snprintf(vfe2x_ctrl->subdev.name,
			 sizeof(vfe2x_ctrl->subdev.name), "vfe2.x");
	v4l2_set_subdevdata(&vfe2x_ctrl->subdev, vfe2x_ctrl);
	platform_set_drvdata(pdev, &vfe2x_ctrl->subdev);

	vfe2x_ctrl->pdev = pdev;
	return 0;
}

static struct platform_driver vfe2x_driver = {
	.probe = vfe2x_probe,
	.driver = {
		.name = MSM_VFE_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_vfe2x_init_module(void)
{
	return platform_driver_register(&vfe2x_driver);
}

static void __exit msm_vfe2x_exit_module(void)
{
	platform_driver_unregister(&vfe2x_driver);
}

module_init(msm_vfe2x_init_module);
module_exit(msm_vfe2x_exit_module);
MODULE_DESCRIPTION("VFE 2.x driver");
MODULE_LICENSE("GPL v2");
