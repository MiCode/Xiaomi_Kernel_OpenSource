/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include "msm.h"
#include "msm_cam_server.h"
#include "msm_vfe40.h"

atomic_t irq_cnt;

#define VFE_WM_CFG_BASE 0x0070
#define VFE_WM_CFG_LEN 0x0024

#define vfe40_get_ch_ping_addr(base, chn) \
	(msm_camera_io_r((base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn)))
#define vfe40_get_ch_pong_addr(base, chn) \
	(msm_camera_io_r((base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn) + 4))
#define vfe40_get_ch_addr(ping_pong, base, chn) \
	((((ping_pong) & (1 << (chn))) == 0) ? \
	(vfe40_get_ch_pong_addr((base), chn)) : \
	(vfe40_get_ch_ping_addr((base), chn)))

#define vfe40_put_ch_ping_addr(base, chn, addr) \
	(msm_camera_io_w((addr), \
	(base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn)))
#define vfe40_put_ch_pong_addr(base, chn, addr) \
	(msm_camera_io_w((addr), \
	(base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn) + 4))
#define vfe40_put_ch_addr(ping_pong, base, chn, addr) \
	(((ping_pong) & (1 << (chn))) == 0 ?   \
	vfe40_put_ch_pong_addr((base), (chn), (addr)) : \
	vfe40_put_ch_ping_addr((base), (chn), (addr)))

static uint32_t vfe_clk_rate;
static void vfe40_send_isp_msg(struct v4l2_subdev *sd,
	uint32_t vfeFrameId, uint32_t isp_msg_id);


struct vfe40_isr_queue_cmd {
	struct list_head list;
	uint32_t                           vfeInterruptStatus0;
	uint32_t                           vfeInterruptStatus1;
};

static struct vfe40_cmd_type vfe40_cmd[] = {
	[1] = {VFE_CMD_SET_CLK},
	[2] = {VFE_CMD_RESET},
	[3] = {VFE_CMD_START},
	[4] = {VFE_CMD_TEST_GEN_START},
	[5] = {VFE_CMD_OPERATION_CFG, V40_OPERATION_CFG_LEN},
	[6] = {VFE_CMD_AXI_OUT_CFG, V40_AXI_OUT_LEN, V40_AXI_BUS_CMD_OFF, 0xFF},
	[7] = {VFE_CMD_CAMIF_CFG, V40_CAMIF_LEN, V40_CAMIF_OFF, 0xFF},
	[8] = {VFE_CMD_AXI_INPUT_CFG},
	[9] = {VFE_CMD_BLACK_LEVEL_CFG},
	[10] = {VFE_CMD_MESH_ROLL_OFF_CFG, V40_MESH_ROLL_OFF_CFG_LEN,
		V40_MESH_ROLL_OFF_CFG_OFF, 0xFF},
	[11] = {VFE_CMD_DEMUX_CFG, V40_DEMUX_LEN, V40_DEMUX_OFF, 0xFF},
	[12] = {VFE_CMD_FOV_CFG},
	[13] = {VFE_CMD_MAIN_SCALER_CFG},
	[14] = {VFE_CMD_WB_CFG, V40_WB_LEN, V40_WB_OFF, 0xFF},
	[15] = {VFE_CMD_COLOR_COR_CFG, V40_COLOR_COR_LEN,
		V40_COLOR_COR_OFF, 0xFF},
	[16] = {VFE_CMD_RGB_G_CFG, V40_RGB_G_LEN, V40_RGB_G_OFF, 0xFF},
	[17] = {VFE_CMD_LA_CFG, V40_LA_LEN, V40_LA_OFF, 0xFF },
	[18] = {VFE_CMD_CHROMA_EN_CFG, V40_CHROMA_EN_LEN, V40_CHROMA_EN_OFF,
		0xFF},
	[19] = {VFE_CMD_CHROMA_SUP_CFG, V40_CHROMA_SUP_LEN,
		V40_CHROMA_SUP_OFF, 0xFF},
	[20] = {VFE_CMD_MCE_CFG, V40_MCE_LEN, V40_MCE_OFF, 0xFF},
	[21] = {VFE_CMD_SK_ENHAN_CFG, V40_SCE_LEN, V40_SCE_OFF, 0xFF},
	[22] = {VFE_CMD_ASF_CFG, V40_ASF_LEN, V40_ASF_OFF, 0xFF},
	[23] = {VFE_CMD_S2Y_CFG},
	[24] = {VFE_CMD_S2CbCr_CFG},
	[25] = {VFE_CMD_CHROMA_SUBS_CFG},
	[26] = {VFE_CMD_OUT_CLAMP_CFG, V40_OUT_CLAMP_LEN, V40_OUT_CLAMP_OFF,
		0xFF},
	[27] = {VFE_CMD_FRAME_SKIP_CFG},
	[31] = {VFE_CMD_UPDATE},
	[32] = {VFE_CMD_BL_LVL_UPDATE},
	[33] = {VFE_CMD_DEMUX_UPDATE, V40_DEMUX_LEN, V40_DEMUX_OFF, 0xFF},
	[34] = {VFE_CMD_FOV_UPDATE},
	[35] = {VFE_CMD_MAIN_SCALER_UPDATE},
	[36] = {VFE_CMD_WB_UPDATE, V40_WB_LEN, V40_WB_OFF, 0xFF},
	[37] = {VFE_CMD_COLOR_COR_UPDATE, V40_COLOR_COR_LEN,
		V40_COLOR_COR_OFF, 0xFF},
	[38] = {VFE_CMD_RGB_G_UPDATE, V40_RGB_G_LEN, V40_CHROMA_EN_OFF, 0xFF},
	[39] = {VFE_CMD_LA_UPDATE, V40_LA_LEN, V40_LA_OFF, 0xFF },
	[40] = {VFE_CMD_CHROMA_EN_UPDATE, V40_CHROMA_EN_LEN,
		V40_CHROMA_EN_OFF, 0xFF},
	[41] = {VFE_CMD_CHROMA_SUP_UPDATE, V40_CHROMA_SUP_LEN,
		V40_CHROMA_SUP_OFF, 0xFF},
	[42] = {VFE_CMD_MCE_UPDATE, V40_MCE_LEN, V40_MCE_OFF, 0xFF},
	[43] = {VFE_CMD_SK_ENHAN_UPDATE, V40_SCE_LEN, V40_SCE_OFF, 0xFF},
	[44] = {VFE_CMD_S2CbCr_UPDATE},
	[45] = {VFE_CMD_S2Y_UPDATE},
	[46] = {VFE_CMD_ASF_UPDATE, V40_ASF_UPDATE_LEN, V40_ASF_OFF, 0xFF},
	[47] = {VFE_CMD_FRAME_SKIP_UPDATE},
	[48] = {VFE_CMD_CAMIF_FRAME_UPDATE},
	[49] = {VFE_CMD_STATS_AF_UPDATE},
	[50] = {VFE_CMD_STATS_AE_UPDATE},
	[51] = {VFE_CMD_STATS_AWB_UPDATE, V40_STATS_AWB_LEN,
		V40_STATS_AWB_OFF},
	[52] = {VFE_CMD_STATS_RS_UPDATE, V40_STATS_RS_LEN, V40_STATS_RS_OFF},
	[53] = {VFE_CMD_STATS_CS_UPDATE, V40_STATS_CS_LEN, V40_STATS_CS_OFF},
	[54] = {VFE_CMD_STATS_SKIN_UPDATE},
	[55] = {VFE_CMD_STATS_IHIST_UPDATE, V40_STATS_IHIST_LEN,
		V40_STATS_IHIST_OFF},
	[57] = {VFE_CMD_EPOCH1_ACK},
	[58] = {VFE_CMD_EPOCH2_ACK},
	[59] = {VFE_CMD_START_RECORDING},
	[60] = {VFE_CMD_STOP_RECORDING},
	[63] = {VFE_CMD_CAPTURE, V40_CAPTURE_LEN, 0xFF},
	[65] = {VFE_CMD_STOP},
	[66] = {VFE_CMD_GET_HW_VERSION, V40_GET_HW_VERSION_LEN,
		V40_GET_HW_VERSION_OFF},
	[67] = {VFE_CMD_GET_FRAME_SKIP_COUNTS},
	[68] = {VFE_CMD_OUTPUT1_BUFFER_ENQ},
	[69] = {VFE_CMD_OUTPUT2_BUFFER_ENQ},
	[70] = {VFE_CMD_OUTPUT3_BUFFER_ENQ},
	[71] = {VFE_CMD_JPEG_OUT_BUF_ENQ},
	[72] = {VFE_CMD_RAW_OUT_BUF_ENQ},
	[73] = {VFE_CMD_RAW_IN_BUF_ENQ},
	[74] = {VFE_CMD_STATS_AF_ENQ},
	[75] = {VFE_CMD_STATS_AE_ENQ},
	[76] = {VFE_CMD_STATS_AWB_ENQ},
	[77] = {VFE_CMD_STATS_RS_ENQ},
	[78] = {VFE_CMD_STATS_CS_ENQ},
	[79] = {VFE_CMD_STATS_SKIN_ENQ},
	[80] = {VFE_CMD_STATS_IHIST_ENQ},
	[82] = {VFE_CMD_JPEG_ENC_CFG},
	[84] = {VFE_CMD_STATS_AF_START},
	[85] = {VFE_CMD_STATS_AF_STOP},
	[86] = {VFE_CMD_STATS_AE_START},
	[87] = {VFE_CMD_STATS_AE_STOP},
	[88] = {VFE_CMD_STATS_AWB_START, V40_STATS_AWB_LEN, V40_STATS_AWB_OFF},
	[89] = {VFE_CMD_STATS_AWB_STOP},
	[90] = {VFE_CMD_STATS_RS_START, V40_STATS_RS_LEN, V40_STATS_RS_OFF},
	[91] = {VFE_CMD_STATS_RS_STOP},
	[92] = {VFE_CMD_STATS_CS_START, V40_STATS_CS_LEN, V40_STATS_CS_OFF},
	[93] = {VFE_CMD_STATS_CS_STOP},
	[94] = {VFE_CMD_STATS_SKIN_START},
	[95] = {VFE_CMD_STATS_SKIN_STOP},
	[96] = {VFE_CMD_STATS_IHIST_START,
		V40_STATS_IHIST_LEN, V40_STATS_IHIST_OFF},
	[97] = {VFE_CMD_STATS_IHIST_STOP},
	[99] = {VFE_CMD_SYNC_TIMER_SETTING, V40_SYNC_TIMER_LEN,
			V40_SYNC_TIMER_OFF},
	[100] = {VFE_CMD_ASYNC_TIMER_SETTING, V40_ASYNC_TIMER_LEN,
		V40_ASYNC_TIMER_OFF},
	[101] = {VFE_CMD_LIVESHOT},
	[102] = {VFE_CMD_LA_SETUP},
	[103] = {VFE_CMD_LINEARIZATION_CFG, V40_LINEARIZATION_LEN1,
			V40_LINEARIZATION_OFF1},
	[104] = {VFE_CMD_DEMOSAICV3},
	[105] = {VFE_CMD_DEMOSAICV3_ABCC_CFG},
	[106] = {VFE_CMD_DEMOSAICV3_DBCC_CFG, V40_DEMOSAICV3_DBCC_LEN,
			V40_DEMOSAICV3_DBCC_OFF},
	[107] = {VFE_CMD_DEMOSAICV3_DBPC_CFG},
	[108] = {VFE_CMD_DEMOSAICV3_ABF_CFG, V40_DEMOSAICV3_ABF_LEN,
			V40_DEMOSAICV3_ABF_OFF},
	[109] = {VFE_CMD_DEMOSAICV3_ABCC_UPDATE},
	[110] = {VFE_CMD_DEMOSAICV3_DBCC_UPDATE, V40_DEMOSAICV3_DBCC_LEN,
			V40_DEMOSAICV3_DBCC_OFF},
	[111] = {VFE_CMD_DEMOSAICV3_DBPC_UPDATE},
	[112] = {VFE_CMD_XBAR_CFG},
	[113] = {VFE_CMD_MODULE_CFG, V40_MODULE_CFG_LEN, V40_MODULE_CFG_OFF},
	[114] = {VFE_CMD_ZSL},
	[115] = {VFE_CMD_LINEARIZATION_UPDATE, V40_LINEARIZATION_LEN1,
			V40_LINEARIZATION_OFF1},
	[116] = {VFE_CMD_DEMOSAICV3_ABF_UPDATE, V40_DEMOSAICV3_ABF_LEN,
			V40_DEMOSAICV3_ABF_OFF},
	[117] = {VFE_CMD_CLF_CFG, V40_CLF_CFG_LEN, V40_CLF_CFG_OFF},
	[118] = {VFE_CMD_CLF_LUMA_UPDATE, V40_CLF_LUMA_UPDATE_LEN,
			V40_CLF_LUMA_UPDATE_OFF},
	[119] = {VFE_CMD_CLF_CHROMA_UPDATE, V40_CLF_CHROMA_UPDATE_LEN,
			V40_CLF_CHROMA_UPDATE_OFF},
	[120] = {VFE_CMD_PCA_ROLL_OFF_CFG},
	[121] = {VFE_CMD_PCA_ROLL_OFF_UPDATE},
	[122] = {VFE_CMD_GET_REG_DUMP},
	[123] = {VFE_CMD_GET_LINEARIZATON_TABLE},
	[124] = {VFE_CMD_GET_MESH_ROLLOFF_TABLE},
	[125] = {VFE_CMD_GET_PCA_ROLLOFF_TABLE},
	[126] = {VFE_CMD_GET_RGB_G_TABLE},
	[127] = {VFE_CMD_GET_LA_TABLE},
	[128] = {VFE_CMD_DEMOSAICV3_UPDATE},
	[129] = {VFE_CMD_ACTIVE_REGION_CFG},
	[130] = {VFE_CMD_COLOR_PROCESSING_CONFIG},
	[131] = {VFE_CMD_STATS_WB_AEC_CONFIG},
	[132] = {VFE_CMD_STATS_WB_AEC_UPDATE},
	[133] = {VFE_CMD_Y_GAMMA_CONFIG},
	[134] = {VFE_CMD_SCALE_OUTPUT1_CONFIG},
	[135] = {VFE_CMD_SCALE_OUTPUT2_CONFIG},
	[136] = {VFE_CMD_CAPTURE_RAW},
	[137] = {VFE_CMD_STOP_LIVESHOT},
	[138] = {VFE_CMD_RECONFIG_VFE},
	[139] = {VFE_CMD_STATS_REQBUF},
	[140] = {VFE_CMD_STATS_ENQUEUEBUF},
	[141] = {VFE_CMD_STATS_FLUSH_BUFQ},
	[142] = {VFE_CMD_STATS_UNREGBUF},
	[143] = {VFE_CMD_STATS_BG_START, V40_STATS_BG_LEN, V40_STATS_BG_OFF},
	[144] = {VFE_CMD_STATS_BG_STOP},
	[145] = {VFE_CMD_STATS_BF_START, V40_STATS_BF_LEN, V40_STATS_BF_OFF},
	[146] = {VFE_CMD_STATS_BF_STOP},
	[147] = {VFE_CMD_STATS_BHIST_START, V40_STATS_BHIST_LEN,
			V40_STATS_BHIST_OFF},
	[148] = {VFE_CMD_STATS_BHIST_STOP},
	[149] = {VFE_CMD_RESET_2},
	[150] = {VFE_CMD_FOV_ENC_CFG, V40_FOV_ENC_LEN,
		V40_FOV_ENC_OFF, 0xFF},
	[151] = {VFE_CMD_FOV_VIEW_CFG, V40_FOV_VIEW_LEN,
		V40_FOV_VIEW_OFF, 0xFF},
	[152] = {VFE_CMD_FOV_ENC_UPDATE, V40_FOV_ENC_LEN,
		V40_FOV_ENC_OFF, 0xFF},
	[153] = {VFE_CMD_FOV_VIEW_UPDATE, V40_FOV_VIEW_LEN,
		V40_FOV_VIEW_OFF, 0xFF},
	[154] = {VFE_CMD_SCALER_ENC_CFG, V40_SCALER_ENC_LEN,
		V40_SCALER_ENC_OFF, 0xFF},
	[155] = {VFE_CMD_SCALER_VIEW_CFG, V40_SCALER_VIEW_LEN,
		V40_SCALER_VIEW_OFF, 0xFF},
	[156] = {VFE_CMD_SCALER_ENC_UPDATE, V40_SCALER_ENC_LEN,
		V40_SCALER_ENC_OFF, 0xFF},
	[157] = {VFE_CMD_SCALER_VIEW_UPDATE, V40_SCALER_VIEW_LEN,
		V40_SCALER_VIEW_OFF, 0xFF},
	[158] = {VFE_CMD_COLORXFORM_ENC_CFG, V40_COLORXFORM_ENC_CFG_LEN,
		V40_COLORXFORM_ENC_CFG_OFF, 0xFF},
	[159] = {VFE_CMD_COLORXFORM_VIEW_CFG, V40_COLORXFORM_VIEW_CFG_LEN,
		V40_COLORXFORM_VIEW_CFG_OFF},
	[160] = {VFE_CMD_COLORXFORM_ENC_UPDATE, V40_COLORXFORM_ENC_CFG_LEN,
		V40_COLORXFORM_ENC_CFG_OFF, 0xFF},
	[161] = {VFE_CMD_COLORXFORM_VIEW_UPDATE, V40_COLORXFORM_VIEW_CFG_LEN,
		V40_COLORXFORM_VIEW_CFG_OFF, 0xFF},
	[163] = {VFE_CMD_STATS_BE_START, V40_STATS_BE_LEN, V40_STATS_BE_OFF},
	[164] = {VFE_CMD_STATS_BE_STOP},
};

static const uint32_t vfe40_AXI_WM_CFG[] = {
	0x0000006C,
	0x00000090,
	0x000000B4,
	0x000000D8,
	0x000000FC,
	0x00000120,
	0x00000144,
};

static const char * const vfe40_general_cmd[] = {
	[1] = "SET_CLK",
	[2] = "RESET",
	[3] = "START",
	[4] = "TEST_GEN_START",
	[5] = "OPERATION_CFG",  /* 5 */
	[6] = "AXI_OUT_CFG",
	[7] = "CAMIF_CFG",
	[8] = "AXI_INPUT_CFG",
	[9] = "BLACK_LEVEL_CFG",
	[10] = "ROLL_OFF_CFG",  /* 10 */
	[11] = "DEMUX_CFG",
	[12] = "FOV_CFG",
	[13] = "MAIN_SCALER_CFG",
	[14] = "WB_CFG",
	[15] = "COLOR_COR_CFG", /* 15 */
	[16] = "RGB_G_CFG",
	[17] = "LA_CFG",
	[18] = "CHROMA_EN_CFG",
	[19] = "CHROMA_SUP_CFG",
	[20] = "MCE_CFG", /* 20 */
	[21] = "SK_ENHAN_CFG",
	[22] = "ASF_CFG",
	[23] = "S2Y_CFG",
	[24] = "S2CbCr_CFG",
	[25] = "CHROMA_SUBS_CFG",  /* 25 */
	[26] = "OUT_CLAMP_CFG",
	[27] = "FRAME_SKIP_CFG",
	[31] = "UPDATE",
	[32] = "BL_LVL_UPDATE",
	[33] = "DEMUX_UPDATE",
	[34] = "FOV_UPDATE",
	[35] = "MAIN_SCALER_UPDATE",  /* 35 */
	[36] = "WB_UPDATE",
	[37] = "COLOR_COR_UPDATE",
	[38] = "RGB_G_UPDATE",
	[39] = "LA_UPDATE",
	[40] = "CHROMA_EN_UPDATE",  /* 40 */
	[41] = "CHROMA_SUP_UPDATE",
	[42] = "MCE_UPDATE",
	[43] = "SK_ENHAN_UPDATE",
	[44] = "S2CbCr_UPDATE",
	[45] = "S2Y_UPDATE",  /* 45 */
	[46] = "ASF_UPDATE",
	[47] = "FRAME_SKIP_UPDATE",
	[48] = "CAMIF_FRAME_UPDATE",
	[49] = "STATS_AF_UPDATE",
	[50] = "STATS_AE_UPDATE",  /* 50 */
	[51] = "STATS_AWB_UPDATE",
	[52] = "STATS_RS_UPDATE",
	[53] = "STATS_CS_UPDATE",
	[54] = "STATS_SKIN_UPDATE",
	[55] = "STATS_IHIST_UPDATE",  /* 55 */
	[57] = "EPOCH1_ACK",
	[58] = "EPOCH2_ACK",
	[59] = "START_RECORDING",
	[60] = "STOP_RECORDING",  /* 60 */
	[63] = "CAPTURE",
	[65] = "STOP",  /* 65 */
	[66] = "GET_HW_VERSION",
	[67] = "GET_FRAME_SKIP_COUNTS",
	[68] = "OUTPUT1_BUFFER_ENQ",
	[69] = "OUTPUT2_BUFFER_ENQ",
	[70] = "OUTPUT3_BUFFER_ENQ",  /* 70 */
	[71] = "JPEG_OUT_BUF_ENQ",
	[72] = "RAW_OUT_BUF_ENQ",
	[73] = "RAW_IN_BUF_ENQ",
	[74] = "STATS_AF_ENQ",
	[75] = "STATS_AE_ENQ",  /* 75 */
	[76] = "STATS_AWB_ENQ",
	[77] = "STATS_RS_ENQ",
	[78] = "STATS_CS_ENQ",
	[79] = "STATS_SKIN_ENQ",
	[80] = "STATS_IHIST_ENQ",  /* 80 */
	[82] = "JPEG_ENC_CFG",
	[84] = "STATS_AF_START",
	[85] = "STATS_AF_STOP",  /* 85 */
	[86] = "STATS_AE_START",
	[87] = "STATS_AE_STOP",
	[88] = "STATS_AWB_START",
	[89] = "STATS_AWB_STOP",
	[90] = "STATS_RS_START",  /* 90 */
	[91] = "STATS_RS_STOP",
	[92] = "STATS_CS_START",
	[93] = "STATS_CS_STOP",
	[94] = "STATS_SKIN_START",
	[95] = "STATS_SKIN_STOP",  /* 95 */
	[96] = "STATS_IHIST_START",
	[97] = "STATS_IHIST_STOP",
	[99] = "SYNC_TIMER_SETTING",
	[100] = "ASYNC_TIMER_SETTING",  /* 100 */
	[101] = "LIVESHOT",
	[102] = "LA_SETUP",
	[103] = "LINEARIZATION_CFG",
	[104] = "DEMOSAICV3",
	[105] = "DEMOSAICV3_ABCC_CFG", /* 105 */
	[106] = "DEMOSAICV3_DBCC_CFG",
	[107] = "DEMOSAICV3_DBPC_CFG",
	[108] = "DEMOSAICV3_ABF_CFG",
	[109] = "DEMOSAICV3_ABCC_UPDATE",
	[110] = "DEMOSAICV3_DBCC_UPDATE", /* 110 */
	[111] = "DEMOSAICV3_DBPC_UPDATE",
	[112] = "XBAR_CFG",
	[113] = "EZTUNE_CFG",
	[114] = "V40_ZSL",
	[115] = "LINEARIZATION_UPDATE", /*115*/
	[116] = "DEMOSAICV3_ABF_UPDATE",
	[117] = "CLF_CFG",
	[118] = "CLF_LUMA_UPDATE",
	[119] = "CLF_CHROMA_UPDATE",
	[120] = "PCA_ROLL_OFF_CFG", /*120*/
	[121] = "PCA_ROLL_OFF_UPDATE",
	[122] = "GET_REG_DUMP",
	[123] = "GET_LINEARIZATON_TABLE",
	[124] = "GET_MESH_ROLLOFF_TABLE",
	[125] = "GET_PCA_ROLLOFF_TABLE", /*125*/
	[126] = "GET_RGB_G_TABLE",
	[127] = "GET_LA_TABLE",
	[128] = "DEMOSAICV3_UPDATE",
	[139] = "STATS_REQBUF",
	[140] = "STATS_ENQUEUEBUF", /*140*/
	[141] = "STATS_FLUSH_BUFQ",
	[142] = "STATS_UNREGBUF",
	[143] = "STATS_BG_START",
	[144] = "STATS_BG_STOP",
	[145] = "STATS_BF_START", /*145*/
	[146] = "STATS_BF_STOP",
	[147] = "STATS_BHIST_START",
	[148] = "STATS_BHIST_STOP",
	[149] = "RESET_2",
};

/*Temporary use fixed bus vectors in VFE */
static struct msm_bus_vectors vfe_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors vfe_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 110592000,
	},
};

static struct msm_bus_vectors vfe_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274406400,
		.ib  = 617103360,
	},
};

static struct msm_bus_vectors vfe_liveshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 348192000,
		.ib  = 617103360,
	},
};

static struct msm_bus_vectors vfe_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274423680,
		.ib  = 1097694720,
	},
};

static struct msm_bus_vectors vfe_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 302071680,
		.ib  = 1208286720,
	},
};

static struct msm_bus_paths vfe_bus_client_config[] = {
	{
		ARRAY_SIZE(vfe_init_vectors),
		vfe_init_vectors,
	},
	{
		ARRAY_SIZE(vfe_preview_vectors),
		vfe_preview_vectors,
	},
	{
		ARRAY_SIZE(vfe_video_vectors),
		vfe_video_vectors,
	},
	{
		ARRAY_SIZE(vfe_snapshot_vectors),
		vfe_snapshot_vectors,
	},
	{
		ARRAY_SIZE(vfe_zsl_vectors),
		vfe_zsl_vectors,
	},
	{
		ARRAY_SIZE(vfe_liveshot_vectors),
		vfe_liveshot_vectors,
	},
};

static struct msm_bus_scale_pdata vfe_bus_client_pdata = {
		vfe_bus_client_config,
		ARRAY_SIZE(vfe_bus_client_config),
		.name = "msm_camera_vfe",
};

uint8_t vfe40_use_bayer_stats(struct vfe40_ctrl_type *vfe40_ctrl)
{
	if (vfe40_ctrl->ver_num.main >= 4) {
		/* VFE 4 or above uses bayer stats */
		return TRUE;
	} else {
		return FALSE;
	}
}

static void axi_enable_irq(struct vfe_share_ctrl_t *share_ctrl)
{
	uint32_t irq_mask;
	uint16_t vfe_operation_mode =
		share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
			VFE_OUTPUTS_RDI1);
	irq_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	irq_mask |= VFE_IMASK_WHILE_STOPPING_0;

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI0)
		irq_mask |= VFE_IRQ_STATUS0_RDI0_REG_UPDATE_MASK;

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI1)
		irq_mask |= VFE_IRQ_STATUS0_RDI1_REG_UPDATE_MASK;

	msm_camera_io_w(irq_mask, share_ctrl->vfebase +
		VFE_IRQ_MASK_0);

	if (vfe_operation_mode) {
		irq_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		irq_mask |= 0x00000021;
		if (share_ctrl->stats_comp)
			irq_mask |= VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0;
		else
			irq_mask |= 0x00FF0000;
		msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		atomic_set(&share_ctrl->vstate, 1);
	}
	atomic_set(&share_ctrl->handle_common_irq, 1);
}

static void axi_disable_irq(struct vfe_share_ctrl_t *share_ctrl)
{

	/* disable all interrupts.  */

	uint32_t irq_mask = 0;
	uint16_t vfe_operation_mode =
		share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
			VFE_OUTPUTS_RDI1);

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI0) {
		irq_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		irq_mask &= ~(VFE_IRQ_STATUS0_RDI0_REG_UPDATE_MASK);
		msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		msm_camera_io_w(VFE_IRQ_STATUS0_RDI0_REG_UPDATE_MASK,
			share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	}
	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI1) {
		irq_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		irq_mask &= ~(VFE_IRQ_STATUS0_RDI1_REG_UPDATE_MASK);
		msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		msm_camera_io_w(VFE_IRQ_STATUS0_RDI1_REG_UPDATE_MASK,
			share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	}
	if (vfe_operation_mode) {
		atomic_set(&share_ctrl->vstate, 0);
		irq_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
		irq_mask &= ~(0x00000011);
		if (share_ctrl->stats_comp)
			irq_mask &= ~(VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0);
		else
			irq_mask &= ~0x00FF0000;
		msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
	}

}

static void vfe40_stop(struct vfe40_ctrl_type *vfe40_ctrl)
{

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time. stop camif immediately. */
	msm_camera_io_w(CAMIF_COMMAND_STOP_IMMEDIATELY,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CAMIF_COMMAND);
	vfe40_ctrl->share_ctrl->operation_mode &=
		~(vfe40_ctrl->share_ctrl->current_mode);
	vfe40_ctrl->share_ctrl->current_mode = 0;
}

static void vfe40_subdev_notify(int id, int path, uint32_t inst_handle,
	struct v4l2_subdev *sd, struct vfe_share_ctrl_t *share_ctrl)
{
	struct msm_vfe_resp rp;
	struct msm_frame_info frame_info;
	unsigned long flags = 0;
	spin_lock_irqsave(&share_ctrl->sd_notify_lock, flags);
	CDBG("vfe40_subdev_notify : msgId = %d\n", id);
	memset(&rp, 0, sizeof(struct msm_vfe_resp));
	rp.evt_msg.type   = MSM_CAMERA_MSG;
	frame_info.inst_handle = inst_handle;
	frame_info.path = path;
	rp.evt_msg.data = &frame_info;
	rp.type	   = id;
	v4l2_subdev_notify(sd, NOTIFY_VFE_BUF_EVT, &rp);
	spin_unlock_irqrestore(&share_ctrl->sd_notify_lock, flags);
}

static int vfe40_config_axi(
	struct axi_ctrl_t *axi_ctrl, int mode, uint32_t *ao)
{
	uint32_t *ch_info;
	uint32_t *axi_cfg = ao;
	int vfe_mode = (mode & ~(OUTPUT_TERT1|OUTPUT_TERT2));

	/* Update the corresponding write masters for each output*/
	ch_info = axi_cfg + V40_AXI_CFG_LEN;
	axi_ctrl->share_ctrl->outpath.out0.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out0.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out0.ch2 = 0x0000FFFF & *ch_info++;
	axi_ctrl->share_ctrl->outpath.out0.inst_handle = *ch_info++;

	axi_ctrl->share_ctrl->outpath.out1.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out1.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out1.ch2 = 0x0000FFFF & *ch_info++;
	axi_ctrl->share_ctrl->outpath.out1.inst_handle = *ch_info++;

	axi_ctrl->share_ctrl->outpath.out2.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out2.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out2.ch2 = 0x0000FFFF & *ch_info++;
	axi_ctrl->share_ctrl->outpath.out2.inst_handle = *ch_info++;

	axi_ctrl->share_ctrl->outpath.out3.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out3.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out3.ch2 = 0x0000FFFF & *ch_info++;
	axi_ctrl->share_ctrl->outpath.out3.inst_handle = *ch_info++;

	axi_ctrl->share_ctrl->outpath.output_mode = 0;

	if (mode & OUTPUT_TERT1)
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_TERTIARY1;
	if (mode & OUTPUT_TERT2)
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_TERTIARY2;
	if (mode == OUTPUT_TERT1 || mode == OUTPUT_TERT1
		|| mode == (OUTPUT_TERT1|OUTPUT_TERT2))
			goto bus_cfg;

	switch (vfe_mode) {
	case OUTPUT_PRIM:
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_PRIMARY;
		break;
	case OUTPUT_PRIM_ALL_CHNLS:
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC:
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_PRIMARY;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS:
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_PRIMARY;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC:
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY;
		break;
	default:
		pr_err("%s Invalid AXI mode %d ", __func__, mode);
		return -EINVAL;
	}

bus_cfg:
	msm_camera_io_memcpy(axi_ctrl->share_ctrl->vfebase +
		vfe40_cmd[VFE_CMD_AXI_OUT_CFG].offset, axi_cfg,
		V40_AXI_BUS_CFG_LEN);
	msm_camera_io_w(*ch_info++,
		axi_ctrl->share_ctrl->vfebase + VFE_RDI0_CFG);
	msm_camera_io_w(*ch_info++,
		axi_ctrl->share_ctrl->vfebase + VFE_RDI1_CFG);
	msm_camera_io_w(*ch_info++,
		axi_ctrl->share_ctrl->vfebase + VFE_RDI2_CFG);
	return 0;
}

static void axi_reset_internal_variables(
	struct axi_ctrl_t *axi_ctrl)
{
	unsigned long flags;
	/* state control variables */
	axi_ctrl->share_ctrl->start_ack_pending = FALSE;
	atomic_set(&irq_cnt, 0);

	spin_lock_irqsave(&axi_ctrl->share_ctrl->stop_flag_lock, flags);
	axi_ctrl->share_ctrl->stop_ack_pending  = FALSE;
	spin_unlock_irqrestore(&axi_ctrl->share_ctrl->stop_flag_lock, flags);

	spin_lock_irqsave(&axi_ctrl->share_ctrl->update_ack_lock, flags);
	axi_ctrl->share_ctrl->update_ack_pending = FALSE;
	spin_unlock_irqrestore(&axi_ctrl->share_ctrl->update_ack_lock, flags);

	axi_ctrl->share_ctrl->recording_state = VFE_STATE_IDLE;
	axi_ctrl->share_ctrl->liveshot_state = VFE_STATE_IDLE;

	atomic_set(&axi_ctrl->share_ctrl->vstate, 0);
	atomic_set(&axi_ctrl->share_ctrl->handle_common_irq, 0);
	atomic_set(&axi_ctrl->share_ctrl->pix0_update_ack_pending, 0);
	atomic_set(&axi_ctrl->share_ctrl->rdi0_update_ack_pending, 0);
	atomic_set(&axi_ctrl->share_ctrl->rdi1_update_ack_pending, 0);
	atomic_set(&axi_ctrl->share_ctrl->rdi2_update_ack_pending, 0);

	/* 0 for continuous mode, 1 for snapshot mode */
	axi_ctrl->share_ctrl->operation_mode = 0;
	axi_ctrl->share_ctrl->current_mode = 0;
	axi_ctrl->share_ctrl->outpath.output_mode = 0;
	axi_ctrl->share_ctrl->comp_output_mode = 0;
	axi_ctrl->share_ctrl->vfe_capture_count = 0;

	/* this is unsigned 32 bit integer. */
	axi_ctrl->share_ctrl->vfeFrameId = 0;
	axi_ctrl->share_ctrl->rdi0FrameId = 0;
	axi_ctrl->share_ctrl->rdi1FrameId = 0;
	axi_ctrl->share_ctrl->rdi2FrameId = 0;
}

static void vfe40_program_dmi_cfg(
	enum VFE40_DMI_RAM_SEL bankSel,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* set bit 8 for auto increment. */
	uint32_t value = VFE_DMI_CFG_DEFAULT;
	value += (uint32_t)bankSel;
	CDBG("%s: banksel = %d\n", __func__, bankSel);

	msm_camera_io_w(value, vfe40_ctrl->share_ctrl->vfebase +
		VFE_DMI_CFG);
	/* by default, always starts with offset 0.*/
	msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
		VFE_DMI_ADDR);
}

static void vfe40_reset_dmi_tables(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	int i = 0;

	/* Reset Histogram LUTs */
	CDBG("Reset Bayer histogram LUT : 0\n");
	vfe40_program_dmi_cfg(STATS_BHIST_RAM0, vfe40_ctrl);
	/* Loop for configuring LUT */
	for (i = 0; i < 256; i++) {
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_HI);
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_LO);
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);

	CDBG("Reset Bayer Histogram LUT: 1\n");
	vfe40_program_dmi_cfg(STATS_BHIST_RAM1, vfe40_ctrl);
	/* Loop for configuring LUT */
	for (i = 0; i < 256; i++) {
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_HI);
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_LO);
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);

	CDBG("Reset IHistogram LUT\n");
	vfe40_program_dmi_cfg(STATS_IHIST_RAM, vfe40_ctrl);
	/* Loop for configuring LUT */
	for (i = 0; i < 256; i++) {
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_HI);
		msm_camera_io_w(0, vfe40_ctrl->share_ctrl->vfebase +
			VFE_DMI_DATA_LO);
	}
	vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
}

static void vfe40_set_default_reg_values(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	msm_camera_io_w(0x800080,
		vfe40_ctrl->share_ctrl->vfebase + VFE_DEMUX_GAIN_0);
	msm_camera_io_w(0x800080,
		vfe40_ctrl->share_ctrl->vfebase + VFE_DEMUX_GAIN_1);
	msm_camera_io_w(0x198FFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CGC_OVERRIDE);

	msm_camera_io_w(0,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_ENC_MIN);
	msm_camera_io_w(0xFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_ENC_MAX);
	msm_camera_io_w(0,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_VIEW_MIN);
	msm_camera_io_w(0xFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase + VFE_CLAMP_VIEW_MAX);

	/* stats UB config */
	CDBG("%s: Use bayer stats = %d\n", __func__,
		 vfe40_use_bayer_stats(vfe40_ctrl));

	msm_camera_io_w(0x82F80007,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_RS_WR_UB_CFG);
	msm_camera_io_w(0x8300000F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_CS_WR_UB_CFG);

	msm_camera_io_w(0x8310003F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BG_WR_UB_CFG);
	msm_camera_io_w(0x8350003F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BE_WR_UB_CFG);
	msm_camera_io_w(0x8390003F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BF_WR_UB_CFG);

	msm_camera_io_w(0x83D0000F,
	vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_HIST_WR_UB_CFG);
	msm_camera_io_w(0x83E0000F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_SKIN_WR_UB_CFG);

	msm_camera_io_w(0x83F0000F,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_AWB_WR_UB_CFG);

	/* stats frame subsample config*/
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_HIST_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BG_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BE_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BF_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_RS_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_CS_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_SKIN_WR_FRAMEDROP_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_AWB_WR_FRAMEDROP_PATTERN);

	/* stats irq subsample config*/
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_HIST_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BG_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BE_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_BF_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_RS_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_CS_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_SKIN_WR_IRQ_SUBSAMPLE_PATTERN);
	msm_camera_io_w(0xFFFFFFFF,
		vfe40_ctrl->share_ctrl->vfebase +
			VFE_BUS_STATS_AWB_WR_IRQ_SUBSAMPLE_PATTERN);

	vfe40_reset_dmi_tables(vfe40_ctrl);
}

static void vfe40_reset_internal_variables(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* Stats control variables. */
	memset(&(vfe40_ctrl->bfStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->awbStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->bgStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->beStatsControl), 0,
		sizeof(struct vfe_stats_control));

	memset(&(vfe40_ctrl->bhistStatsControl), 0,
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
	vfe40_set_default_reg_values(vfe40_ctrl);
}

static int vfe40_reset(struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t irq_mask;
	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 0);
	msm_camera_io_w(VFE_MODULE_RESET_CMD,
		vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_RESET);
	msm_camera_io_w(0,
		vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_RESET);

	irq_mask =
		msm_camera_io_r(vfe40_ctrl->share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
	irq_mask &= ~(0x00FF0011|VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0);

	/* enable reset_ack interrupt.  */
	irq_mask |= VFE_IMASK_WHILE_STOPPING_0;
	msm_camera_io_w(irq_mask, vfe40_ctrl->share_ctrl->vfebase +
		VFE_IRQ_MASK_0);

	msm_camera_io_w_mb(VFE_ONLY_RESET_CMD,
		vfe40_ctrl->share_ctrl->vfebase + VFE_GLOBAL_RESET);

	return wait_for_completion_interruptible(
			&vfe40_ctrl->share_ctrl->reset_complete);
}

static int axi_reset(struct axi_ctrl_t *axi_ctrl)
{
	axi_reset_internal_variables(axi_ctrl);
	/* disable all interrupts.  vfeImaskLocal is also reset to 0
	* to begin with. */
	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);

	msm_camera_io_w(VFE_DISABLE_ALL_IRQS,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* clear all pending interrupts*/
	msm_camera_io_w(VFE_CLEAR_ALL_IRQ0,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_0);
	msm_camera_io_w(VFE_CLEAR_ALL_IRQ1,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CLEAR_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(1, axi_ctrl->share_ctrl->vfebase + VFE_IRQ_CMD);

	/* enable reset_ack interrupt.  */
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_0,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);

	/* Write to VFE_GLOBAL_RESET_CMD to reset the vfe hardware. Once reset
	 * is done, hardware interrupt will be generated.  VFE ist processes
	 * the interrupt to complete the function call.  Note that the reset
	 * function is synchronous. */

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(VFE_RESET_UPON_RESET_CMD,
		axi_ctrl->share_ctrl->vfebase + VFE_GLOBAL_RESET);

	return wait_for_completion_interruptible(
			&axi_ctrl->share_ctrl->reset_complete);
}

static int vfe40_operation_config(uint32_t *cmd,
			struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t *p = cmd;

	vfe40_ctrl->share_ctrl->stats_comp = *(++p);
	vfe40_ctrl->hfr_mode = *(++p);

	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_CFG);
	msm_camera_io_w(*(++p),
		vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
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
		pr_err("%s: dq stats buf (type = %d) err = %d\n",
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
				&(stats_buf->info), NULL, -1);
		if (rc < 0) {
			pr_err("%s: dq stats buf (type = %d) err = %d",
				 __func__, stats_type, rc);
			return rc;
		}
	}
	return 0L;
}


static unsigned long vfe40_stats_unregbuf(
	struct vfe40_ctrl_type *vfe40_ctrl,
	struct msm_stats_reqbuf *req_buf, int domain_num)
{
	int i = 0, rc = 0;

	for (i = 0; i < req_buf->num_buf; i++) {
		rc = vfe40_ctrl->stats_ops.buf_unprepare(
			vfe40_ctrl->stats_ops.stats_ctrl,
			req_buf->stats_type, i,
			vfe40_ctrl->stats_ops.client, domain_num);
		if (rc < 0) {
			pr_err("%s: unreg stats buf (type = %d) err = %d",
				__func__, req_buf->stats_type, rc);
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
		pr_err("%s: dq awb ping buf from free buf queue\n", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AWB_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_AWB);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq awb ping buf from free buf queue\n",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	return 0;
}

static uint32_t vfe_stats_be_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t addr;
	unsigned long flags;
	uint32_t stats_type;

	stats_type = MSM_STATS_TYPE_BE;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq BE ping buf from free buf queue\n",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BE_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq BE pong buf from free buf queue\n",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BE_WR_PONG_ADDR);
	return 0;
}

static uint32_t vfe_stats_bg_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t addr;
	unsigned long flags;
	uint32_t stats_type;

	stats_type = MSM_STATS_TYPE_BG;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq aec/Bg ping buf from free buf queue\n",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BG_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq aec/Bg pong buf from free buf queue\n",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BG_WR_PONG_ADDR);
	return 0;
}

static int vfe_stats_bf_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t addr;
	unsigned long flags;
	int rc = 0;

	uint32_t stats_type;
	stats_type = MSM_STATS_TYPE_BF;

	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	rc = vfe40_stats_flush_enqueue(vfe40_ctrl, stats_type);
	if (rc < 0) {
		pr_err("%s: dq stats buf err = %d",
			   __func__, rc);
		spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
		return -EINVAL;
	}
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq af ping buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BF_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq af pong buf from free buf queue", __func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_BF_WR_PONG_ADDR);
	return 0;
}

static uint32_t vfe_stats_bhist_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint32_t addr;
	unsigned long flags;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_BHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq ihist ping buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_SKIN_WR_PING_ADDR);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_BHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (!addr) {
		pr_err("%s: dq ihist pong buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_SKIN_WR_PONG_ADDR);

	return 0;
}

static int vfe_stats_ihist_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
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
		pr_err("%s: dq Ihist pong buf from free buf queue",
			__func__);
		return -ENOMEM;
	}
	msm_camera_io_w(addr,
		vfe40_ctrl->share_ctrl->vfebase +
		VFE_BUS_STATS_HIST_WR_PONG_ADDR);

	return 0;
}

static int vfe_stats_rs_buf_init(
	struct vfe40_ctrl_type *vfe40_ctrl)
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
	struct vfe40_ctrl_type *vfe40_ctrl)
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

static void vfe40_cfg_qos_parms(struct vfe40_ctrl_type *vfe40_ctrl)
{
	void __iomem *vfebase = vfe40_ctrl->share_ctrl->vfebase;
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_0);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_1);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_2);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_3);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_4);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_5);
	msm_camera_io_w(0xAAAAAAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_6);
	msm_camera_io_w(0x0002AAAA, vfebase + VFE_0_BUS_BDG_QOS_CFG_7);
}

static void vfe40_start_common(struct vfe40_ctrl_type *vfe40_ctrl)
{
	uint16_t vfe_operation_mode =
		vfe40_ctrl->share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
			VFE_OUTPUTS_RDI1);
	CDBG("VFE opertaion mode = 0x%x, output mode = 0x%x\n",
		vfe40_ctrl->share_ctrl->current_mode,
		vfe40_ctrl->share_ctrl->outpath.output_mode);

	vfe40_cfg_qos_parms(vfe40_ctrl);

	msm_camera_io_w_mb(0x1,
			vfe40_ctrl->share_ctrl->vfebase +
			VFE_REG_UPDATE_CMD);

	msm_camera_io_w_mb(0x00003fff,
			vfe40_ctrl->share_ctrl->vfebase +
			V40_AXI_BUS_CMD_OFF);
	msm_camera_io_w_mb(1,
			vfe40_ctrl->share_ctrl->vfebase +
			V40_BUS_PM_CMD);
	if (vfe_operation_mode) {
		msm_camera_io_w_mb(1, vfe40_ctrl->share_ctrl->vfebase +
			VFE_CAMIF_COMMAND);
	}

}

static int vfe40_start_recording(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_ctrl->share_ctrl->recording_state = VFE_STATE_START_REQUESTED;
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	return 0;
}

static int vfe40_stop_recording(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_ctrl->share_ctrl->recording_state = VFE_STATE_STOP_REQUESTED;
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
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

static void vfe40_stop_liveshot(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_ctrl->share_ctrl->liveshot_state = VFE_STATE_STOP_REQUESTED;
	msm_camera_io_w_mb(1,
		vfe40_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static int vfe40_zsl(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	vfe40_ctrl->share_ctrl->start_ack_pending = TRUE;
	vfe40_start_common(vfe40_ctrl);

	return 0;
}
static int vfe40_capture_raw(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t num_frames_capture)
{
	vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt = num_frames_capture;
	vfe40_ctrl->share_ctrl->vfe_capture_count = num_frames_capture;
	vfe40_start_common(vfe40_ctrl);
	return 0;
}

static int vfe40_capture(
	struct msm_cam_media_controller *pmctl,
	uint32_t num_frames_capture,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	/* capture command is valid for both idle and active state. */
	vfe40_ctrl->share_ctrl->outpath.out1.capture_cnt = num_frames_capture;
	if (vfe40_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		vfe40_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		vfe40_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		vfe40_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG) {
		vfe40_ctrl->share_ctrl->outpath.out0.capture_cnt =
			num_frames_capture;
	}

	vfe40_ctrl->share_ctrl->vfe_capture_count = num_frames_capture;


	vfe40_start_common(vfe40_ctrl);
	/* for debug */
	return 0;
}

static int vfe40_start(
	struct msm_cam_media_controller *pmctl,
	struct vfe40_ctrl_type *vfe40_ctrl)
{
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

	spin_lock_irqsave(&vfe40_ctrl->share_ctrl->update_ack_lock, flags);
	vfe40_ctrl->share_ctrl->update_ack_pending = TRUE;
	spin_unlock_irqrestore(&vfe40_ctrl->share_ctrl->update_ack_lock, flags);
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
	val = vfe_clk_rate / 10000;
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

static struct vfe40_output_ch *vfe40_get_ch(
	int path, struct vfe_share_ctrl_t *share_ctrl)
{
	struct vfe40_output_ch *ch = NULL;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		ch = &share_ctrl->outpath.out0;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		ch = &share_ctrl->outpath.out1;
	else if (path == VFE_MSG_OUTPUT_TERTIARY1)
		ch = &share_ctrl->outpath.out2;
	else if (path == VFE_MSG_OUTPUT_TERTIARY2)
		ch = &share_ctrl->outpath.out3;
	else
		pr_err("%s: Invalid path %d\n", __func__,
			path);

	BUG_ON(ch == NULL);
	return ch;
}
static struct msm_free_buf *vfe40_check_free_buffer(
	int id, int path, struct axi_ctrl_t *axi_ctrl)
{
	struct vfe40_output_ch *outch = NULL;
	struct msm_free_buf *b = NULL;
	uint32_t inst_handle = 0;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		inst_handle = axi_ctrl->share_ctrl->outpath.out0.inst_handle;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		inst_handle = axi_ctrl->share_ctrl->outpath.out1.inst_handle;
	else if (path == VFE_MSG_OUTPUT_TERTIARY1)
		inst_handle = axi_ctrl->share_ctrl->outpath.out2.inst_handle;
	else if (path == VFE_MSG_OUTPUT_TERTIARY2)
		inst_handle = axi_ctrl->share_ctrl->outpath.out3.inst_handle;

	vfe40_subdev_notify(id, path, inst_handle,
		&axi_ctrl->subdev, axi_ctrl->share_ctrl);
	outch = vfe40_get_ch(path, axi_ctrl->share_ctrl);
	if (outch->free_buf.ch_paddr[0])
		b = &outch->free_buf;
	return b;
}
static int configure_pingpong_buffers(
	int id, int path, struct axi_ctrl_t *axi_ctrl)
{
	struct vfe40_output_ch *outch = NULL;
	int rc = 0;
	uint32_t inst_handle = 0;
	if (path == VFE_MSG_OUTPUT_PRIMARY)
		inst_handle = axi_ctrl->share_ctrl->outpath.out0.inst_handle;
	else if (path == VFE_MSG_OUTPUT_SECONDARY)
		inst_handle = axi_ctrl->share_ctrl->outpath.out1.inst_handle;
	else if (path == VFE_MSG_OUTPUT_TERTIARY1)
		inst_handle = axi_ctrl->share_ctrl->outpath.out2.inst_handle;
	else if (path == VFE_MSG_OUTPUT_TERTIARY2)
		inst_handle = axi_ctrl->share_ctrl->outpath.out3.inst_handle;

	vfe40_subdev_notify(id, path, inst_handle,
		&axi_ctrl->subdev, axi_ctrl->share_ctrl);
	outch = vfe40_get_ch(path, axi_ctrl->share_ctrl);
	if (outch->ping.ch_paddr[0] && outch->pong.ch_paddr[0]) {
		/* Configure Preview Ping Pong */
		pr_info("%s Configure ping/pong address for %d\n",
						__func__, path);
		vfe40_put_ch_ping_addr(
			axi_ctrl->share_ctrl->vfebase, outch->ch0,
			outch->ping.ch_paddr[0]);
		vfe40_put_ch_pong_addr(
			axi_ctrl->share_ctrl->vfebase, outch->ch0,
			outch->pong.ch_paddr[0]);

		if ((axi_ctrl->share_ctrl->current_mode !=
			VFE_OUTPUTS_RAW) && (path != VFE_MSG_OUTPUT_TERTIARY1)
			&& (path != VFE_MSG_OUTPUT_TERTIARY2)) {
			vfe40_put_ch_ping_addr(
				axi_ctrl->share_ctrl->vfebase, outch->ch1,
				outch->ping.ch_paddr[1]);
			vfe40_put_ch_pong_addr(
				axi_ctrl->share_ctrl->vfebase, outch->ch1,
				outch->pong.ch_paddr[1]);
		}

		if (outch->ping.num_planes > 2)
			vfe40_put_ch_ping_addr(
				axi_ctrl->share_ctrl->vfebase, outch->ch2,
				outch->ping.ch_paddr[2]);
		if (outch->pong.num_planes > 2)
			vfe40_put_ch_pong_addr(
				axi_ctrl->share_ctrl->vfebase, outch->ch2,
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

static void vfe40_send_isp_msg(
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
	uint32_t old_val = 0 , new_val = 0, module_val = 0;
	uint32_t *cmdp = NULL;
	uint32_t *cmdp_local = NULL;
	uint32_t snapshot_cnt = 0;
	uint32_t temp1 = 0, temp2 = 0;
	struct msm_camera_vfe_params_t vfe_params;

	CDBG("vfe40_proc_general: cmdID = %s, length = %d\n",
		vfe40_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case VFE_CMD_RESET:
		pr_info("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		vfe40_ctrl->share_ctrl->vfe_reset_flag = true;
		vfe40_reset(vfe40_ctrl);
		break;
	case VFE_CMD_START:
		pr_info("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		if (copy_from_user(&vfe_params,
				(void __user *)(cmd->value),
				sizeof(struct msm_camera_vfe_params_t))) {
				rc = -EFAULT;
				goto proc_general_done;
		}

		vfe40_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;

		rc = vfe40_start(pmctl, vfe40_ctrl);
		break;
	case VFE_CMD_UPDATE:
		vfe40_update(vfe40_ctrl);
		break;
	case VFE_CMD_CAPTURE_RAW:
		pr_info("%s: cmdID = VFE_CMD_CAPTURE_RAW\n", __func__);
		if (copy_from_user(&vfe_params,
				(void __user *)(cmd->value),
				sizeof(struct msm_camera_vfe_params_t))) {
				rc = -EFAULT;
				goto proc_general_done;
		}

		snapshot_cnt = vfe_params.capture_count;
		vfe40_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;
		rc = vfe40_capture_raw(pmctl, vfe40_ctrl, snapshot_cnt);
		break;
	case VFE_CMD_CAPTURE:
		pr_info("%s: cmdID = VFE_CMD_CAPTURE\n", __func__);
		if (copy_from_user(&vfe_params,
				(void __user *)(cmd->value),
				sizeof(struct msm_camera_vfe_params_t))) {
				rc = -EFAULT;
				goto proc_general_done;
		}

		snapshot_cnt = vfe_params.capture_count;
		vfe40_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;

		rc = vfe40_capture(pmctl, snapshot_cnt, vfe40_ctrl);
		break;
	case VFE_CMD_START_RECORDING:
		pr_info("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		rc = vfe40_start_recording(pmctl, vfe40_ctrl);
		break;
	case VFE_CMD_STOP_RECORDING:
		pr_info("vfe40_proc_general: cmdID = %s\n",
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
		if (vfe40_use_bayer_stats(vfe40_ctrl)) {
			/* Error */
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe_stats_bg_buf_init(vfe40_ctrl);
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
		if (vfe40_use_bayer_stats(vfe40_ctrl)) {
			/* Error */
			rc = -EFAULT;
			goto proc_general_done;
		}
		rc = vfe_stats_bf_buf_init(vfe40_ctrl);
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
		rc = vfe_stats_ihist_buf_init(vfe40_ctrl);
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
		rc = vfe_stats_rs_buf_init(vfe40_ctrl);
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
		rc = vfe_stats_cs_buf_init(vfe40_ctrl);
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

	case VFE_CMD_STATS_BG_START:
	case VFE_CMD_STATS_BE_START:
	case VFE_CMD_STATS_BF_START:
	case VFE_CMD_STATS_BHIST_START: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_STATS_CFG);
		module_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);

		if (VFE_CMD_STATS_BE_START == cmd->id) {
			module_val |= BE_ENABLE_MASK;
			rc = vfe_stats_be_buf_init(vfe40_ctrl);
			if (rc < 0) {
				pr_err("%s: cannot config ping/pong address of BG",
					__func__);
				goto proc_general_done;
			}
		} else if (VFE_CMD_STATS_BG_START == cmd->id) {
			module_val |= BG_ENABLE_MASK;
			rc = vfe_stats_bg_buf_init(vfe40_ctrl);
			if (rc < 0) {
				pr_err("%s: cannot config ping/pong address of BG",
					__func__);
				goto proc_general_done;
			}
		} else if (VFE_CMD_STATS_BF_START == cmd->id) {
			module_val |= BF_ENABLE_MASK;
			rc = vfe_stats_bf_buf_init(vfe40_ctrl);
			if (rc < 0) {
				pr_err("%s: cannot config ping/pong address of BF",
					__func__);
				goto proc_general_done;
			}
		} else {
			module_val |= BHIST_ENABLE_MASK;
			old_val |= STATS_BHIST_ENABLE_MASK;
			rc = vfe_stats_bhist_buf_init(vfe40_ctrl);
			if (rc < 0) {
				pr_err("%s: cannot config ping/pong address of BHist",
					__func__);
				goto proc_general_done;
			}
		}
		msm_camera_io_w(old_val, vfe40_ctrl->share_ctrl->vfebase +
			VFE_STATS_CFG);
		msm_camera_io_w(module_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
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
		msm_camera_io_memcpy(
			vfe40_ctrl->share_ctrl->vfebase +
			vfe40_cmd[cmd->id].offset,
			cmdp_local, V40_MESH_ROLL_OFF_CFG_LEN);
		cmdp_local += 9;
		vfe40_program_dmi_cfg(ROLLOFF_RAM0_BANK0, vfe40_ctrl);
		/* for loop for extrcting table. */
		for (i = 0; i < (V40_MESH_ROLL_OFF_TABLE_SIZE * 2); i++) {
			msm_camera_io_w(*cmdp_local,
				vfe40_ctrl->share_ctrl->vfebase +
				VFE_DMI_DATA_LO);
			cmdp_local++;
		}
		CDBG("done writing mesh table\n");
		vfe40_program_dmi_cfg(NO_MEM_SELECTED, vfe40_ctrl);
	}
	break;
	case VFE_CMD_GET_MESH_ROLLOFF_TABLE:
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
	case VFE_CMD_STATS_BG_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~BG_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		}
		break;
	case VFE_CMD_STATS_BF_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~BF_ENABLE_MASK;
		msm_camera_io_w(old_val,
		vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);

		rc = vfe40_stats_flush_enqueue(vfe40_ctrl,
				MSM_STATS_TYPE_BF);
		if (rc < 0) {
			pr_err("%s: dq stats buf err = %d",
				   __func__, rc);
			return -EINVAL;
			}
		}
		break;

	case VFE_CMD_STATS_BE_STOP: {
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
		old_val &= ~BE_ENABLE_MASK;
		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_MODULE_CFG);
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

	case VFE_CMD_STATS_BHIST_STOP: {
		if (!vfe40_use_bayer_stats(vfe40_ctrl)) {
			/* Error */
			rc = -EFAULT;
			goto proc_general_done;
		}
		old_val = msm_camera_io_r(
			vfe40_ctrl->share_ctrl->vfebase + VFE_STATS_CFG);

		if (VFE_CMD_STATS_BHIST_STOP == cmd->id)
			old_val &= ~STATS_BHIST_ENABLE_MASK;

		msm_camera_io_w(old_val,
			vfe40_ctrl->share_ctrl->vfebase + VFE_STATS_CFG);
		}
		break;

	case VFE_CMD_STOP:
		pr_info("vfe40_proc_general: cmdID = %s\n",
			vfe40_general_cmd[cmd->id]);
		if (copy_from_user(&vfe_params,
				(void __user *)(cmd->value),
				sizeof(struct msm_camera_vfe_params_t))) {
				rc = -EFAULT;
				goto proc_general_done;
		}

		vfe40_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;
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
		if (copy_from_user(&vfe_params,
				(void __user *)(cmd->value),
				sizeof(struct msm_camera_vfe_params_t))) {
				rc = -EFAULT;
				goto proc_general_done;
		}

		vfe40_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;

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
	case VFE_CMD_STOP_LIVESHOT:
		CDBG("%s Stopping liveshot ", __func__);
		vfe40_stop_liveshot(pmctl, vfe40_ctrl);
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

void axi_stop_pix(struct vfe_share_ctrl_t *share_ctrl)
{
	uint32_t operation_mode =
	share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
		VFE_OUTPUTS_RDI1);
	uint32_t irq_comp_mask, irq_mask;
	uint32_t reg_update = 0x1;

	irq_comp_mask =
		msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_COMP_MASK);
	irq_mask = msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	switch (share_ctrl->cmd_type) {
	case AXI_CMD_PREVIEW: {
		switch (operation_mode) {
		case VFE_OUTPUTS_PREVIEW:
		case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
			if (share_ctrl->comp_output_mode &
				VFE40_OUTPUT_MODE_PRIMARY) {
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out0.ch0]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out0.ch1]);
				irq_comp_mask &= ~(
					0x1 << share_ctrl->outpath.out0.ch0 |
					0x1 << share_ctrl->outpath.out0.ch1);
				share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_PRIMARY;
			} else if (share_ctrl->comp_output_mode &
					VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out0.ch0]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out0.ch1]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out0.ch2]);
				irq_comp_mask &= ~(
					0x1 << share_ctrl->outpath.out0.ch0 |
					0x1 << share_ctrl->outpath.out0.ch1 |
					0x1 << share_ctrl->outpath.out0.ch2);
				share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
			}
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK;
			break;
		default:
			if (share_ctrl->comp_output_mode &
				VFE40_OUTPUT_MODE_SECONDARY) {
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out1.ch0]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out1.ch1]);
				irq_comp_mask &= ~(
					0x1 << share_ctrl->outpath.out1.ch0 |
					0x1 << share_ctrl->outpath.out1.ch1);
				share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_SECONDARY;
			} else if (share_ctrl->comp_output_mode &
				VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out1.ch0]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out1.ch1]);
				msm_camera_io_w(0, share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[share_ctrl->
					outpath.out1.ch2]);
				irq_comp_mask &= ~(
					0x1 << share_ctrl->outpath.out1.ch0 |
					0x1 << share_ctrl->outpath.out1.ch1 |
					0x1 << share_ctrl->outpath.out1.ch2);
				share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
			}
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK;
			break;
			}
		}
		break;
	default:
		if (share_ctrl->comp_output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out0.ch0]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out0.ch1]);
			irq_comp_mask &= ~(
				0x1 << share_ctrl->outpath.out0.ch0 |
				0x1 << share_ctrl->outpath.out0.ch1);
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK;
			share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_PRIMARY;
		} else if (share_ctrl->comp_output_mode &
				VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out0.ch0]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out0.ch1]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out0.ch2]);
			irq_comp_mask &= ~(
				0x1 << share_ctrl->outpath.out0.ch0 |
				0x1 << share_ctrl->outpath.out0.ch1 |
				0x1 << share_ctrl->outpath.out0.ch2);
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK;
			share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		}

		if (share_ctrl->comp_output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->
				outpath.out1.ch0]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->outpath.out1.ch1]);
			irq_comp_mask &= ~(
				0x1 << share_ctrl->outpath.out1.ch0 |
				0x1 << share_ctrl->outpath.out1.ch1);
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK;
			share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_SECONDARY;
		} else if (share_ctrl->comp_output_mode &
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->outpath.out1.ch1]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[share_ctrl->outpath.out1.ch2]);
			irq_comp_mask &= ~(
				0x1 << share_ctrl->outpath.out1.ch0 |
				0x1 << share_ctrl->outpath.out1.ch1 |
				0x1 << share_ctrl->outpath.out1.ch2);
			irq_mask &= ~VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK;
			share_ctrl->outpath.output_mode |=
					VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
		}
		break;
	}

	msm_camera_io_w_mb(reg_update,
		share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camera_io_w(irq_comp_mask,
		share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
}

void axi_stop_rdi0(struct vfe_share_ctrl_t *share_ctrl)
{
	uint32_t reg_update = 0x2;
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI0) {
		msm_camera_io_w(0, share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[share_ctrl->outpath.out2.ch0]);
		irq_mask &= ~(0x1 << (share_ctrl->outpath.out2.ch0 +
				VFE_WM_OFFSET));
	}
	msm_camera_io_w_mb(reg_update,
		share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
}

void axi_stop_rdi1(struct vfe_share_ctrl_t *share_ctrl)
{
	uint32_t reg_update = 0x4;
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI1) {
		msm_camera_io_w(1, share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[share_ctrl->outpath.out3.ch0]);
		irq_mask &= ~(0x1 << (share_ctrl->outpath.out3.ch0 +
			VFE_WM_OFFSET));
	}
	msm_camera_io_w_mb(reg_update,
		share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	msm_camera_io_w(irq_mask, share_ctrl->vfebase +
			VFE_IRQ_MASK_0);
}

void axi_stop_process(struct vfe_share_ctrl_t *share_ctrl)
{
	uint32_t operation_mode =
	share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
		VFE_OUTPUTS_RDI1);

	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI0) {
		axi_stop_rdi0(share_ctrl);
		share_ctrl->comp_output_mode &= ~VFE40_OUTPUT_MODE_TERTIARY1;
	}
	if (share_ctrl->current_mode & VFE_OUTPUTS_RDI1) {
		axi_stop_rdi1(share_ctrl);
		share_ctrl->comp_output_mode &= ~VFE40_OUTPUT_MODE_TERTIARY2;
	}
	if (operation_mode) {
		axi_stop_pix(share_ctrl);
		share_ctrl->comp_output_mode &=
				~(share_ctrl->outpath.output_mode);
	}
}

static void vfe40_process_reg_update_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	struct vfe_share_ctrl_t *share_ctrl = vfe40_ctrl->share_ctrl;

	if (atomic_cmpxchg(
		&share_ctrl->pix0_update_ack_pending, 2, 0) == 2) {
		axi_stop_pix(share_ctrl);
		msm_camera_io_w_mb(
				CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
				share_ctrl->vfebase + VFE_CAMIF_COMMAND);
		axi_disable_irq(share_ctrl);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			share_ctrl->vfeFrameId,
			MSG_ID_PIX0_UPDATE_ACK);
		share_ctrl->comp_output_mode &=
				~(share_ctrl->outpath.output_mode);
		share_ctrl->current_mode &=
			(VFE_OUTPUTS_RDI0|VFE_OUTPUTS_RDI0);
	}  else {
		if (share_ctrl->recording_state == VFE_STATE_START_REQUESTED) {
			if (share_ctrl->operation_mode &
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
				msm_camera_io_w(1,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(1,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch1]);
			} else if (share_ctrl->operation_mode &
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
				msm_camera_io_w(1,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out1.ch0]);
				msm_camera_io_w(1,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out1.ch1]);
		}
			share_ctrl->recording_state = VFE_STATE_STARTED;
		msm_camera_io_w_mb(1,
				share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
		CDBG("start video triggered .\n");
		} else if (share_ctrl->recording_state ==
			VFE_STATE_STOP_REQUESTED) {
			if (share_ctrl->operation_mode &
				VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
				msm_camera_io_w(0,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(0,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch1]);
			} else if (share_ctrl->operation_mode &
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
				msm_camera_io_w(0,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out1.ch0]);
				msm_camera_io_w(0,
					share_ctrl->vfebase + vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out1.ch1]);
		}
		CDBG("stop video triggered .\n");
	}

		if (atomic_cmpxchg(
			&share_ctrl->pix0_update_ack_pending, 1, 0) == 1) {
			share_ctrl->comp_output_mode |=
				(share_ctrl->outpath.output_mode
				& ~(VFE40_OUTPUT_MODE_TERTIARY1|
					VFE40_OUTPUT_MODE_TERTIARY2));
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				share_ctrl->vfeFrameId, MSG_ID_PIX0_UPDATE_ACK);
			share_ctrl->current_mode &=
				(VFE_OUTPUTS_RDI0|VFE_OUTPUTS_RDI0);
	} else {
		if (share_ctrl->recording_state ==
			VFE_STATE_STOP_REQUESTED) {
				share_ctrl->recording_state = VFE_STATE_STOPPED;
			/* request a reg update and send STOP_REC_ACK
			 * when we process the next reg update irq.
			 */
			msm_camera_io_w_mb(1, share_ctrl->vfebase +
						VFE_REG_UPDATE_CMD);
		} else if (share_ctrl->recording_state ==
					VFE_STATE_STOPPED) {
			vfe40_send_isp_msg(&vfe40_ctrl->subdev,
					share_ctrl->vfeFrameId,
				MSG_ID_STOP_REC_ACK);
				share_ctrl->recording_state = VFE_STATE_IDLE;
		}
		spin_lock_irqsave(&share_ctrl->update_ack_lock, flags);
		if (share_ctrl->update_ack_pending == TRUE) {
			share_ctrl->update_ack_pending = FALSE;
			spin_unlock_irqrestore(
				&share_ctrl->update_ack_lock, flags);
			vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				share_ctrl->vfeFrameId, MSG_ID_UPDATE_ACK);
		} else {
			spin_unlock_irqrestore(
					&share_ctrl->update_ack_lock, flags);
		}
	}

	switch (share_ctrl->liveshot_state) {
	case VFE_STATE_START_REQUESTED:
		CDBG("%s enabling liveshot output\n", __func__);
			if (share_ctrl->comp_output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
				msm_camera_io_w(1, share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(1, share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[
				share_ctrl->outpath.out0.ch1]);

				share_ctrl->liveshot_state =
				VFE_STATE_STARTED;
				msm_camera_io_w_mb(1, share_ctrl->vfebase +
				VFE_REG_UPDATE_CMD);
		}
		break;
	case VFE_STATE_STARTED:
		CDBG("%s disabling liveshot output\n", __func__);
		if (share_ctrl->vfe_capture_count >= 1) {
			if (share_ctrl->vfe_capture_count == 1 &&
				(share_ctrl->comp_output_mode &
				VFE40_OUTPUT_MODE_PRIMARY)) {
				msm_camera_io_w(0, share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[
					share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(0, share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[
					share_ctrl->outpath.out0.ch1]);
				msm_camera_io_w_mb(1, share_ctrl->vfebase +
					VFE_REG_UPDATE_CMD);
			}
			share_ctrl->vfe_capture_count--;
		}
		break;
	case VFE_STATE_STOP_REQUESTED:
		CDBG("%s disabling liveshot output from stream off\n",
			__func__);
		if (share_ctrl->comp_output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			/* Stop requested, stop write masters, and
			 * trigger REG_UPDATE. Send STOP_LS_ACK in
			 * next reg update. */
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[
			share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(0, share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[
			share_ctrl->outpath.out0.ch1]);
			share_ctrl->liveshot_state = VFE_STATE_STOPPED;
			msm_camera_io_w_mb(1, share_ctrl->vfebase +
				VFE_REG_UPDATE_CMD);
		}
		break;
	case VFE_STATE_STOPPED:
		CDBG("%s Sending STOP_LS ACK\n", __func__);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			share_ctrl->vfeFrameId, MSG_ID_STOP_LS_ACK);
			share_ctrl->liveshot_state = VFE_STATE_IDLE;
		break;
	default:
		break;
	}

	if ((share_ctrl->operation_mode & VFE_OUTPUTS_THUMB_AND_MAIN) ||
		(share_ctrl->operation_mode &
		VFE_OUTPUTS_MAIN_AND_THUMB) ||
		(share_ctrl->operation_mode &
		VFE_OUTPUTS_THUMB_AND_JPEG) ||
		(share_ctrl->operation_mode &
		VFE_OUTPUTS_JPEG_AND_THUMB)) {
		/* in snapshot mode */
		/* later we need to add check for live snapshot mode. */
		if (vfe40_ctrl->frame_skip_pattern & (0x1 <<
			(vfe40_ctrl->snapshot_frame_cnt %
				vfe40_ctrl->frame_skip_cnt))) {
				share_ctrl->vfe_capture_count--;
			/* if last frame to be captured: */
				if (share_ctrl->vfe_capture_count == 0) {
					/* stop the bus output: */
					if (share_ctrl->comp_output_mode
					& VFE40_OUTPUT_MODE_PRIMARY) {
						msm_camera_io_w(0,
							share_ctrl->vfebase+
							vfe40_AXI_WM_CFG[
							share_ctrl->
							outpath.out0.ch0]);
						msm_camera_io_w(0,
							share_ctrl->vfebase+
							vfe40_AXI_WM_CFG[
							share_ctrl->
							outpath.out0.ch1]);
					}
					if (share_ctrl->comp_output_mode &
						VFE40_OUTPUT_MODE_SECONDARY) {
						msm_camera_io_w(0,
							share_ctrl->vfebase+
							vfe40_AXI_WM_CFG[
							share_ctrl->
							outpath.out1.ch0]);
						msm_camera_io_w(0,
							share_ctrl->vfebase+
							vfe40_AXI_WM_CFG[
							share_ctrl->
							outpath.out1.ch1]);
				}
				msm_camera_io_w_mb
				(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
					share_ctrl->vfebase +
					VFE_CAMIF_COMMAND);
				vfe40_ctrl->snapshot_frame_cnt = -1;
				vfe40_ctrl->frame_skip_cnt = 31;
				vfe40_ctrl->frame_skip_pattern = 0xffffffff;
			} /*if snapshot count is 0*/
		} /*if frame is not being dropped*/
		vfe40_ctrl->snapshot_frame_cnt++;
		/* then do reg_update. */
		msm_camera_io_w(1,
				share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
	} /* if snapshot mode. */
}
}

static void vfe40_process_rdi0_reg_update_irq(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	if (atomic_cmpxchg(
		&vfe40_ctrl->share_ctrl->rdi0_update_ack_pending, 2, 0) == 2) {
		axi_stop_rdi0(vfe40_ctrl->share_ctrl);
		axi_disable_irq(vfe40_ctrl->share_ctrl);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_RDI0_UPDATE_ACK);
		vfe40_ctrl->share_ctrl->comp_output_mode &=
			~VFE40_OUTPUT_MODE_TERTIARY1;
		vfe40_ctrl->share_ctrl->current_mode &=
			~(VFE_OUTPUTS_RDI0);
	}

	if (atomic_cmpxchg(
		&vfe40_ctrl->share_ctrl->rdi0_update_ack_pending, 1, 0) == 1) {
		vfe40_ctrl->share_ctrl->comp_output_mode |=
			VFE40_OUTPUT_MODE_TERTIARY1;
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_RDI0_UPDATE_ACK);
		vfe40_ctrl->share_ctrl->current_mode &=
			~(VFE_OUTPUTS_RDI0);
	}
}

static void vfe40_process_rdi1_reg_update_irq(
	struct vfe40_ctrl_type *vfe40_ctrl)
{
	if (atomic_cmpxchg(
		&vfe40_ctrl->share_ctrl->rdi1_update_ack_pending, 2, 0) == 2) {
		axi_stop_rdi1(vfe40_ctrl->share_ctrl);
		axi_disable_irq(vfe40_ctrl->share_ctrl);
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_RDI1_UPDATE_ACK);
			vfe40_ctrl->share_ctrl->comp_output_mode &=
				~VFE40_OUTPUT_MODE_TERTIARY2;
		vfe40_ctrl->share_ctrl->current_mode &=
			~(VFE_OUTPUTS_RDI1);
	}

	if (atomic_cmpxchg(
		&vfe40_ctrl->share_ctrl->rdi1_update_ack_pending, 1, 0) == 1) {
		vfe40_ctrl->share_ctrl->comp_output_mode |=
			VFE40_OUTPUT_MODE_TERTIARY2;
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
			vfe40_ctrl->share_ctrl->vfeFrameId,
			MSG_ID_RDI1_UPDATE_ACK);
		vfe40_ctrl->share_ctrl->current_mode &=
			~(VFE_OUTPUTS_RDI1);
	}
}

static void vfe40_process_reset_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;

	atomic_set(&vfe40_ctrl->share_ctrl->vstate, 0);
	atomic_set(&vfe40_ctrl->share_ctrl->handle_common_irq, 0);

	spin_lock_irqsave(&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
	if (vfe40_ctrl->share_ctrl->stop_ack_pending) {
		vfe40_ctrl->share_ctrl->stop_ack_pending = FALSE;
		spin_unlock_irqrestore(
			&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
		if (vfe40_ctrl->share_ctrl->sync_abort)
			complete(&vfe40_ctrl->share_ctrl->reset_complete);
		else
		vfe40_send_isp_msg(&vfe40_ctrl->subdev,
				vfe40_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_STOP_ACK);
	} else {
		spin_unlock_irqrestore(
			&vfe40_ctrl->share_ctrl->stop_flag_lock, flags);
		/* this is from reset command. */
		vfe40_reset_internal_variables(vfe40_ctrl);
		if (vfe40_ctrl->share_ctrl->vfe_reset_flag) {
			vfe40_ctrl->share_ctrl->vfe_reset_flag = false;
			msm_camera_io_w(0xFF00,
				vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_CMD);
		} else {
		/* reload all write masters. (frame & line)*/
		msm_camera_io_w(0xFF7F,
			vfe40_ctrl->share_ctrl->vfebase + VFE_BUS_CMD);
		}
		complete(&vfe40_ctrl->share_ctrl->reset_complete);
	}
}

static void vfe40_process_camif_sof_irq(
		struct vfe40_ctrl_type *vfe40_ctrl)
{
	if (vfe40_ctrl->share_ctrl->operation_mode &
		VFE_OUTPUTS_RAW) {
		if (vfe40_ctrl->share_ctrl->start_ack_pending)
			vfe40_ctrl->share_ctrl->start_ack_pending = FALSE;

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
		if (vfe40_ctrl->vfe_sof_count_enable)
			vfe40_ctrl->share_ctrl->vfeFrameId++;
		CDBG("Skip the SOF notification when HFR enabled\n");
		return;
	}

	if (vfe40_ctrl->vfe_sof_count_enable)
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
	if (errStatus & VFE40_IMASK_VIOLATION) {
		pr_err("vfe40_irq: violation interrupt\n");
		reg_value = msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_VIOLATION_STATUS);
		pr_err("%s: violationStatus  = 0x%x\n", __func__, reg_value);
	}

	if (errStatus & VFE40_IMASK_CAMIF_ERROR) {
		pr_err("vfe40_irq: camif errors\n");
		reg_value = msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_CAMIF_STATUS);
		v4l2_subdev_notify(&axi_ctrl->subdev,
			NOTIFY_VFE_CAMIF_ERROR, (void *)NULL);
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

	if (errStatus & VFE40_IMASK_STATS_BE_BUS_OVFL)
		pr_err("vfe40_irq: be stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_BG_BUS_OVFL)
		pr_err("vfe40_irq: bg stats bus overflow\n");

	if (errStatus & VFE40_IMASK_STATS_BF_BUS_OVFL)
		pr_err("vfe40_irq: bf stats bus overflow\n");

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
}

static void vfe40_process_common_error_irq(
	struct axi_ctrl_t *axi_ctrl, uint32_t errStatus)
{
	if (errStatus & VFE40_IMASK_BUS_BDG_HALT_ACK)
		pr_err("vfe40_irq: BUS BDG HALT ACK\n");

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

}

static void vfe_send_outmsg(
	struct axi_ctrl_t *axi_ctrl, uint8_t msgid,
	uint32_t ch0_paddr, uint32_t ch1_paddr,
	uint32_t ch2_paddr, uint32_t inst_handle)
{
	struct isp_msg_output msg;

	msg.output_id = msgid;
	msg.buf.inst_handle = inst_handle;
	msg.buf.ch_paddr[0]	= ch0_paddr;
	msg.buf.ch_paddr[1]	= ch1_paddr;
	msg.buf.ch_paddr[2]	= ch2_paddr;
	switch (msgid) {
	case MSG_ID_OUTPUT_TERTIARY1:
		msg.frameCounter = axi_ctrl->share_ctrl->rdi0FrameId;
		break;
	case MSG_ID_OUTPUT_TERTIARY2:
		msg.frameCounter = axi_ctrl->share_ctrl->rdi1FrameId;
		break;
	default:
		msg.frameCounter = axi_ctrl->share_ctrl->vfeFrameId;
		break;
	}

	v4l2_subdev_notify(&axi_ctrl->subdev,
			NOTIFY_VFE_MSG_OUT,
			&msg);
	return;
}

static void vfe40_process_output_path_irq_0(
	struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_PRIMARY, axi_ctrl);

	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool = (
		(axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_THUMB_AND_JPEG ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_RAW ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STARTED ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOP_REQUESTED ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOPPED) &&
		(axi_ctrl->share_ctrl->vfe_capture_count <= 1)) ||
			free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Channel 0*/
		ch0_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch0);
		/* Channel 1*/
		ch1_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch1);
		/* Channel 2*/
		ch2_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch2);

		CDBG("output path 0, ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe40_put_ch_addr(ping_pong,
					axi_ctrl->share_ctrl->vfebase,
					axi_ctrl->share_ctrl->outpath.out0.ch2,
					free_buf->ch_paddr[2]);
		}
		if (axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_THUMB_AND_JPEG ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_JPEG_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->liveshot_state ==
				VFE_STATE_STOPPED)
			axi_ctrl->share_ctrl->outpath.out0.capture_cnt--;

		vfe_send_outmsg(axi_ctrl,
			MSG_ID_OUTPUT_PRIMARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			axi_ctrl->share_ctrl->outpath.out0.inst_handle);

	} else {
		axi_ctrl->share_ctrl->outpath.out0.frame_drop_cnt++;
		CDBG("path_irq_0 - no free buffer!\n");
	}
}

static void vfe40_process_output_path_irq_1(
	struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_SECONDARY, axi_ctrl);
	out_bool = ((axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_JPEG_AND_THUMB) &&
			(axi_ctrl->share_ctrl->vfe_capture_count <= 1)) ||
				free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		ch0_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		ch1_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch1);
		ch2_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch2);

		CDBG("%s ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			__func__, ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe40_put_ch_addr(ping_pong,
					axi_ctrl->share_ctrl->vfebase,
					axi_ctrl->share_ctrl->outpath.out1.ch2,
					free_buf->ch_paddr[2]);
		}
		if (axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->operation_mode &
				VFE_OUTPUTS_JPEG_AND_THUMB)
			axi_ctrl->share_ctrl->outpath.out1.capture_cnt--;

		vfe_send_outmsg(axi_ctrl,
			MSG_ID_OUTPUT_SECONDARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			axi_ctrl->share_ctrl->outpath.out1.inst_handle);

	} else {
		axi_ctrl->share_ctrl->outpath.out1.frame_drop_cnt++;
		CDBG("path_irq_1 - no free buffer!\n");
	}
}

static void vfe40_process_output_path_irq_rdi0(
			struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr = 0;
	/* this must be rdi image output. */
	struct msm_free_buf *free_buf = NULL;
	/*RDI0*/
	if (axi_ctrl->share_ctrl->operation_mode & VFE_OUTPUTS_RDI0) {
		free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
			VFE_MSG_OUTPUT_TERTIARY1, axi_ctrl);
		if (free_buf) {
			ping_pong = msm_camera_io_r(axi_ctrl->
				share_ctrl->vfebase +
				VFE_BUS_PING_PONG_STATUS);

			/* Y only channel */
			ch0_paddr = vfe40_get_ch_addr(ping_pong,
				axi_ctrl->share_ctrl->vfebase,
				axi_ctrl->share_ctrl->outpath.out2.ch0);

			pr_debug("%s ch0 = 0x%x\n",
				__func__, ch0_paddr);

			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
				axi_ctrl->share_ctrl->vfebase,
				axi_ctrl->share_ctrl->outpath.out2.ch0,
				free_buf->ch_paddr[0]);

			vfe_send_outmsg(axi_ctrl,
				MSG_ID_OUTPUT_TERTIARY1, ch0_paddr,
				0, 0,
				axi_ctrl->share_ctrl->outpath.out2.inst_handle);

		} else {
			axi_ctrl->share_ctrl->outpath.out2.frame_drop_cnt++;
			pr_err("path_irq_2 irq - no free buffer for rdi0!\n");
		}
	}
}

static void vfe40_process_output_path_irq_rdi1(
	struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr = 0;
	/* this must be rdi image output. */
	struct msm_free_buf *free_buf = NULL;
	/*RDI1*/
	if (axi_ctrl->share_ctrl->operation_mode & VFE_OUTPUTS_RDI1) {
		free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
			VFE_MSG_OUTPUT_TERTIARY2, axi_ctrl);
		if (free_buf) {
			ping_pong = msm_camera_io_r(axi_ctrl->
				share_ctrl->vfebase +
				VFE_BUS_PING_PONG_STATUS);

			/* Y channel */
			ch0_paddr = vfe40_get_ch_addr(ping_pong,
				axi_ctrl->share_ctrl->vfebase,
				axi_ctrl->share_ctrl->outpath.out3.ch0);
			pr_debug("%s ch0 = 0x%x\n",
				__func__, ch0_paddr);

			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
				axi_ctrl->share_ctrl->vfebase,
				axi_ctrl->share_ctrl->outpath.out3.ch0,
				free_buf->ch_paddr[0]);

			vfe_send_outmsg(axi_ctrl,
				MSG_ID_OUTPUT_TERTIARY2, ch0_paddr,
				0, 0,
				axi_ctrl->share_ctrl->outpath.out3.inst_handle);
		} else {
			axi_ctrl->share_ctrl->outpath.out3.frame_drop_cnt++;
			pr_err("path_irq irq - no free buffer for rdi1!\n");
		}
	}
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
	CDBG("%s:statsNum %d, pingpongStatus %d\n", __func__,
		 statsNum, pingpongStatus);
	pingpongAddr =
		((uint32_t)(vfe40_ctrl->share_ctrl->vfebase +
				VFE_BUS_STATS_PING_PONG_BASE)) +
				(VFE_STATS_BUS_REG_NUM*statsNum)*4 +
				(1-pingpongStatus)*4;
	returnAddr = msm_camera_io_r((uint32_t *)pingpongAddr);
	msm_camera_io_w(newAddr, (uint32_t *)pingpongAddr);
	return returnAddr;
}

static void vfe_send_stats_msg(
	struct vfe40_ctrl_type *vfe40_ctrl,
	uint32_t bufAddress, uint32_t statsNum)
{
	int rc = 0;
	void *vaddr = NULL;
	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	struct isp_msg_stats msgStats;
	uint32_t stats_type;
	msgStats.frameCounter = vfe40_ctrl->share_ctrl->vfeFrameId;
	if (vfe40_ctrl->simultaneous_sof_stat)
		msgStats.frameCounter--;
	msgStats.buffer = bufAddress;
	switch (statsNum) {
	case statsBgNum:{
		msgStats.id = MSG_ID_STATS_BG;
		stats_type = MSM_STATS_TYPE_BG;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				stats_type, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsBeNum:{
		msgStats.id = MSG_ID_STATS_BE;
		stats_type = MSM_STATS_TYPE_BE;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				stats_type, bufAddress,
				&msgStats.buf_idx, &vaddr, &msgStats.fd,
				vfe40_ctrl->stats_ops.client);
		}
		break;
	case statsBfNum:{
		msgStats.id = MSG_ID_STATS_BF;
		stats_type =  MSM_STATS_TYPE_BF;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				stats_type, bufAddress,
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
	case statsSkinNum: {
		msgStats.id = MSG_ID_STATS_BHIST;
		rc = vfe40_ctrl->stats_ops.dispatch(
				vfe40_ctrl->stats_ops.stats_ctrl,
				MSM_STATS_TYPE_BHIST, bufAddress,
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
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_send_comp_stats_msg(
	struct vfe40_ctrl_type *vfe40_ctrl, uint32_t status_bits)
{
	struct msm_stats_buf msgStats;
	uint32_t temp;

	msgStats.frame_id = vfe40_ctrl->share_ctrl->vfeFrameId;
	if (vfe40_ctrl->simultaneous_sof_stat)
		msgStats.frame_id--;

	msgStats.status_bits = status_bits;

	msgStats.aec.buff = vfe40_ctrl->bgStatsControl.bufToRender;
	msgStats.awb.buff = vfe40_ctrl->awbStatsControl.bufToRender;
	msgStats.af.buff = vfe40_ctrl->bfStatsControl.bufToRender;

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

static void vfe40_process_stats_be_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	uint32_t stats_type;
	stats_type = MSM_STATS_TYPE_BE;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->beStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsBeNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->beStatsControl.bufToRender, statsBeNum);
	} else{
		vfe40_ctrl->beStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->beStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats_bg_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	uint32_t stats_type;
	stats_type = MSM_STATS_TYPE_BG;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->bgStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsBgNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->bgStatsControl.bufToRender, statsBgNum);
	} else{
		vfe40_ctrl->bgStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->bgStatsControl.droppedStatsFrameCount);
	}
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

static void vfe40_process_stats_bf_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	uint32_t stats_type;
	stats_type = MSM_STATS_TYPE_BF;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, stats_type);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->bfStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl, statsBfNum,
			addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->bfStatsControl.bufToRender, statsBfNum);
	} else{
		vfe40_ctrl->bfStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->bfStatsControl.droppedStatsFrameCount);
	}
}

static void vfe40_process_stats_bhist_irq(struct vfe40_ctrl_type *vfe40_ctrl)
{
	unsigned long flags;
	uint32_t addr;
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);
	addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl, MSM_STATS_TYPE_BHIST);
	spin_unlock_irqrestore(&vfe40_ctrl->stats_bufq_lock, flags);
	if (addr) {
		vfe40_ctrl->bhistStatsControl.bufToRender =
			vfe40_process_stats_irq_common(vfe40_ctrl,
				statsSkinNum, addr);

		vfe_send_stats_msg(vfe40_ctrl,
			vfe40_ctrl->bhistStatsControl.bufToRender,
			statsSkinNum);
	} else{
		vfe40_ctrl->bhistStatsControl.droppedStatsFrameCount++;
		CDBG("%s: droppedStatsFrameCount = %d", __func__,
			vfe40_ctrl->bhistStatsControl.droppedStatsFrameCount);
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
				vfe40_ctrl->csStatsControl.bufToRender,
				statsCsNum);
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
	uint32_t stats_type;

	CDBG("%s, stats = 0x%x\n", __func__, status_bits);
	spin_lock_irqsave(&vfe40_ctrl->stats_bufq_lock, flags);

	stats_type = MSM_STATS_TYPE_BE;
	if (status_bits & VFE_IRQ_STATUS0_STATS_BE) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
				stats_type);
		if (addr) {
			vfe40_ctrl->beStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsBeNum, addr);
			process_stats = true;
		} else{
			vfe40_ctrl->beStatsControl.bufToRender = 0;
			vfe40_ctrl->beStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe40_ctrl->beStatsControl.bufToRender = 0;
	}

	stats_type = MSM_STATS_TYPE_BG;
	if (status_bits & VFE_IRQ_STATUS0_STATS_BG) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
				stats_type);
		if (addr) {
			vfe40_ctrl->bgStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsBgNum, addr);
			process_stats = true;
		} else{
			vfe40_ctrl->bgStatsControl.bufToRender = 0;
			vfe40_ctrl->bgStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe40_ctrl->bgStatsControl.bufToRender = 0;
	}

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

	stats_type = MSM_STATS_TYPE_BF;
	if (status_bits & VFE_IRQ_STATUS0_STATS_BF) {
		addr = (uint32_t)vfe40_stats_dqbuf(vfe40_ctrl,
					stats_type);
		if (addr) {
			vfe40_ctrl->bfStatsControl.bufToRender =
				vfe40_process_stats_irq_common(
				vfe40_ctrl, statsBfNum,
				addr);
			process_stats = true;
		} else {
			vfe40_ctrl->bfStatsControl.bufToRender = 0;
			vfe40_ctrl->bfStatsControl.droppedStatsFrameCount++;
		}
	} else {
		vfe40_ctrl->bfStatsControl.bufToRender = 0;
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
	case VFE_IRQ_STATUS0_RDI0_REG_UPDATE:
		CDBG("irq	rdi0 regUpdateIrq\n");
		vfe40_process_rdi0_reg_update_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_RDI1_REG_UPDATE:
		CDBG("irq	rdi1 regUpdateIrq\n");
		vfe40_process_rdi1_reg_update_irq(vfe40_ctrl);
		break;
	case VFE_IMASK_WHILE_STOPPING_0:
		CDBG("irq	resetAckIrq\n");
		vfe40_process_reset_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_BG:
		CDBG("Stats BG irq occured.\n");
		vfe40_process_stats_bg_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_BE:
		CDBG("Stats BE irq occured.\n");
		vfe40_process_stats_be_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_BF:
		CDBG("Stats BF irq occured.\n");
		vfe40_process_stats_bf_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_AWB:
		CDBG("Stats AWB irq occured.\n");
		vfe40_process_stats_awb_irq(vfe40_ctrl);
		break;
	case VFE_IRQ_STATUS0_STATS_SKIN_BHIST:
		CDBG("Stats BHIST irq occured.\n");
		vfe40_process_stats_bhist_irq(vfe40_ctrl);
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
	struct vfe40_ctrl_type *vfe40_ctrl = axi_ctrl->share_ctrl->vfe40_ctrl;
	struct vfe40_isr_queue_cmd *qcmd = NULL;
	int stat_interrupt;

	CDBG("=== axi40_do_tasklet start ===\n");

	while (atomic_read(&irq_cnt)) {
		spin_lock_irqsave(&axi_ctrl->tasklet_lock, flags);
		qcmd = list_first_entry(&axi_ctrl->tasklet_q,
			struct vfe40_isr_queue_cmd, list);
		atomic_sub(1, &irq_cnt);

		if (!qcmd) {
			spin_unlock_irqrestore(&axi_ctrl->tasklet_lock,
				flags);
			return;
		}

		list_del(&qcmd->list);
		spin_unlock_irqrestore(&axi_ctrl->tasklet_lock,
			flags);

		if (axi_ctrl->share_ctrl->stats_comp) {
			stat_interrupt = (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0);
		} else {
			stat_interrupt =
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_BG) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_BE) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_AWB) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_BF) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_IHIST) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_RS) |
				(qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_CS);
		}
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_CAMIF_SOF_MASK) {
			if (stat_interrupt)
				vfe40_ctrl->simultaneous_sof_stat = 1;
			v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_CAMIF_SOF_MASK);
		}

		/* interrupt to be processed,  *qcmd has the payload.  */
		if (qcmd->vfeInterruptStatus0 &
				VFE_IRQ_STATUS0_REG_UPDATE_MASK)
			v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_REG_UPDATE_MASK);

		if (qcmd->vfeInterruptStatus1 &
				VFE_IRQ_STATUS0_RDI0_REG_UPDATE_MASK)
			v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_RDI0_REG_UPDATE);

		if (qcmd->vfeInterruptStatus1 &
				VFE_IRQ_STATUS0_RDI1_REG_UPDATE_MASK)
			v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IRQ_STATUS0_RDI1_REG_UPDATE);

		if (qcmd->vfeInterruptStatus0 &
				VFE_IMASK_WHILE_STOPPING_0)
			v4l2_subdev_notify(&vfe40_ctrl->subdev,
				NOTIFY_VFE_IRQ,
				(void *)VFE_IMASK_WHILE_STOPPING_0);

		if (atomic_read(&axi_ctrl->share_ctrl->handle_common_irq)) {
			if (qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_COMMON_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe40_process_common_error_irq(
					axi_ctrl,
					qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_COMMON_ERROR_ONLY_1);
			}

			v4l2_subdev_notify(&axi_ctrl->subdev,
				NOTIFY_AXI_IRQ,
				(void *)qcmd->vfeInterruptStatus0);
		}

		if (atomic_read(&axi_ctrl->share_ctrl->vstate)) {
			if (qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_VFE_ERROR_ONLY_1) {
				pr_err("irq	errorIrq\n");
				vfe40_process_error_irq(
					axi_ctrl,
					qcmd->vfeInterruptStatus1 &
					VFE40_IMASK_VFE_ERROR_ONLY_1);
			}

			/* then process stats irq. */
			if (axi_ctrl->share_ctrl->stats_comp) {
				/* process stats comb interrupt. */
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0) {
					CDBG("Stats composite irq occured.\n");
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)qcmd->vfeInterruptStatus0);
				}
			} else {
				/* process individual stats interrupt. */
				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_BG)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_BG);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_BE)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_BE);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_AWB)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_AWB);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_BF)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_BF);
				if (qcmd->vfeInterruptStatus0 &
					VFE_IRQ_STATUS0_STATS_SKIN_BHIST)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)
					VFE_IRQ_STATUS0_STATS_SKIN_BHIST);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_IHIST)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_IHIST);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_RS)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_RS);

				if (qcmd->vfeInterruptStatus0 &
						VFE_IRQ_STATUS0_STATS_CS)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS0_STATS_CS);

				if (qcmd->vfeInterruptStatus1 &
						VFE_IRQ_STATUS1_SYNC_TIMER0)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER0);

				if (qcmd->vfeInterruptStatus1 &
						VFE_IRQ_STATUS1_SYNC_TIMER1)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER1);

				if (qcmd->vfeInterruptStatus1 &
						VFE_IRQ_STATUS1_SYNC_TIMER2)
					v4l2_subdev_notify(&vfe40_ctrl->subdev,
					NOTIFY_VFE_IRQ,
					(void *)VFE_IRQ_STATUS1_SYNC_TIMER2);
			}
		}
		vfe40_ctrl->simultaneous_sof_stat = 0;
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

	atomic_add(1, &irq_cnt);
	spin_unlock_irqrestore(&axi_ctrl->tasklet_lock, flags);
	tasklet_schedule(&axi_ctrl->vfe40_tasklet);
	return IRQ_HANDLED;
}

int msm_axi_subdev_isr_routine(struct v4l2_subdev *sd,
	u32 status, bool *handled)
{
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	CDBG("%s E ", __func__);
	ret = vfe40_parse_irq(axi_ctrl->vfeirq->start, axi_ctrl);
	*handled = TRUE;
	return 0;
}

static long vfe_stats_bufq_sub_ioctl(
	struct vfe40_ctrl_type *vfe_ctrl,
	struct msm_vfe_cfg_cmd *cmd, void *ion_client, int domain_num)
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
			vfe_ctrl->stats_ops.client, domain_num);
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
	case VFE_CMD_STATS_UNREGBUF:
	{
		struct msm_stats_reqbuf *req_buf = NULL;
		req_buf = (struct msm_stats_reqbuf *)cmd->value;
		if (sizeof(struct msm_stats_reqbuf) != cmd->length) {
			/* error. the length not match */
			pr_err("%s: stats reqbuf input size = %d,\n"
				"struct size = %d, mitch match\n",
				 __func__, cmd->length,
				sizeof(struct msm_stats_reqbuf));
			rc = -EINVAL ;
			goto end;
		}
		rc = vfe40_stats_unregbuf(vfe_ctrl, req_buf, domain_num);
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
	struct msm_camvfe_params *vfe_params;
	struct msm_vfe_cfg_cmd *cmd;
	void *data;

	long rc = 0;
	struct vfe_cmd_stats_buf *scfg = NULL;
	struct vfe_cmd_stats_ack *sack = NULL;

	if (!vfe40_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}

	CDBG("%s\n", __func__);
	if (subdev_cmd == VIDIOC_MSM_VFE_INIT) {
		CDBG("%s init\n", __func__);
		return msm_vfe_subdev_init(sd);
	} else if (subdev_cmd == VIDIOC_MSM_VFE_RELEASE) {
		msm_vfe_subdev_release(sd);
		return 0;
	}
	vfe_params = (struct msm_camvfe_params *)arg;
	cmd = vfe_params->vfe_cfg;
	data = vfe_params->data;
	switch (cmd->cmd_type) {
	case CMD_VFE_PROCESS_IRQ:
		vfe40_process_irq(vfe40_ctrl, (uint32_t) data);
		return rc;
	case VFE_CMD_STATS_REQBUF:
	case VFE_CMD_STATS_ENQUEUEBUF:
	case VFE_CMD_STATS_FLUSH_BUFQ:
	case VFE_CMD_STATS_UNREGBUF:
		/* for easy porting put in one envelope */
		rc = vfe_stats_bufq_sub_ioctl(vfe40_ctrl,
				cmd, vfe_params->data, pmctl->domain_num);
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
		cmd->cmd_type != CMD_STATS_AF_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_BG_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_BE_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_BF_BUF_RELEASE &&
		cmd->cmd_type != CMD_STATS_BHIST_BUF_RELEASE &&
		cmd->cmd_type != CMD_VFE_PIX_SOF_COUNT_UPDATE &&
		cmd->cmd_type != CMD_VFE_COUNT_PIX_SOF_ENABLE) {
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
	}

	CDBG("%s: cmdType = %d\n", __func__, cmd->cmd_type);

	if ((cmd->cmd_type == CMD_STATS_AF_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_AWB_ENABLE)   ||
		(cmd->cmd_type == CMD_STATS_IHIST_ENABLE) ||
		(cmd->cmd_type == CMD_STATS_RS_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_CS_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_AEC_ENABLE)   ||
		(cmd->cmd_type == CMD_STATS_BG_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_BE_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_BF_ENABLE)    ||
		(cmd->cmd_type == CMD_STATS_BHIST_ENABLE)) {
		struct axidata *axid;
		axid = data;
		if (!axid) {
			rc = -EFAULT;
			goto vfe40_config_done;
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
		case CMD_STATS_AEC_ENABLE:
		case CMD_STATS_BG_ENABLE:
		case CMD_STATS_BE_ENABLE:
		case CMD_STATS_BF_ENABLE:
		case CMD_STATS_BHIST_ENABLE:
		case CMD_STATS_AWB_ENABLE:
		case CMD_STATS_IHIST_ENABLE:
		case CMD_STATS_RS_ENABLE:
		case CMD_STATS_CS_ENABLE:
		default:
			pr_err("%s Unsupported cmd type %d",
				__func__, cmd->cmd_type);
			break;
		}
		goto vfe40_config_done;
	}
	switch (cmd->cmd_type) {
	case CMD_GENERAL:
		rc = vfe40_proc_general(pmctl, &vfecmd, vfe40_ctrl);
	break;
	case CMD_VFE_COUNT_PIX_SOF_ENABLE: {
		int enable = *((int *)cmd->value);
		if (enable)
			vfe40_ctrl->vfe_sof_count_enable = TRUE;
		else
			vfe40_ctrl->vfe_sof_count_enable = false;
	}
	break;
	case CMD_VFE_PIX_SOF_COUNT_UPDATE:
		if (!vfe40_ctrl->vfe_sof_count_enable)
			vfe40_ctrl->share_ctrl->vfeFrameId =
			*((uint32_t *)vfe_params->data);
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
vfe40_config_done:
	kfree(scfg);
	kfree(sack);
	CDBG("%s done: rc = %d\n", __func__, (int) rc);
	return rc;
}

static struct msm_cam_clk_info vfe40_clk_info[] = {
	{"camss_top_ahb_clk", -1},
	{"vfe_clk_src", 266670000},
	{"camss_vfe_vfe_clk", -1},
	{"camss_csi_vfe_clk", -1},
	{"iface_clk", -1},
	{"bus_clk", -1},
	{"alt_bus_clk", -1},
};

static int msm_axi_subdev_s_crystal_freq(struct v4l2_subdev *sd,
						u32 freq, u32 flags)
{
	int rc = 0;
	int round_rate;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);

	round_rate = clk_round_rate(axi_ctrl->vfe_clk[1], freq);
	if (rc < 0) {
		pr_err("%s: clk_round_rate failed %d\n",
					__func__, rc);
		return rc;
	}

	vfe_clk_rate = round_rate;
	rc = clk_set_rate(axi_ctrl->vfe_clk[1], round_rate);
	if (rc < 0)
		pr_err("%s: clk_set_rate failed %d\n",
					__func__, rc);

	return rc;
}

static const struct v4l2_subdev_core_ops msm_vfe_subdev_core_ops = {
	.ioctl = msm_vfe_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_vfe_subdev_ops = {
	.core = &msm_vfe_subdev_core_ops,
};

static void msm_vfe40_init_vbif_parms(void __iomem *vfe_vbif_base)
{
	msm_camera_io_w(0x1,
		vfe_vbif_base + VFE40_VBIF_CLKON);
	msm_camera_io_w(0x01010101,
		vfe_vbif_base + VFE40_VBIF_IN_RD_LIM_CONF0);
	msm_camera_io_w(0x01010101,
		vfe_vbif_base + VFE40_VBIF_IN_RD_LIM_CONF1);
	msm_camera_io_w(0x10010110,
		vfe_vbif_base + VFE40_VBIF_IN_RD_LIM_CONF2);
	msm_camera_io_w(0x10101010,
		vfe_vbif_base + VFE40_VBIF_IN_WR_LIM_CONF0);
	msm_camera_io_w(0x10101010,
		vfe_vbif_base + VFE40_VBIF_IN_WR_LIM_CONF1);
	msm_camera_io_w(0x10101010,
		vfe_vbif_base + VFE40_VBIF_IN_WR_LIM_CONF2);
	msm_camera_io_w(0x00001010,
		vfe_vbif_base + VFE40_VBIF_OUT_RD_LIM_CONF0);
	msm_camera_io_w(0x00001010,
		vfe_vbif_base + VFE40_VBIF_OUT_WR_LIM_CONF0);
	msm_camera_io_w(0x00000707,
		vfe_vbif_base + VFE40_VBIF_DDR_OUT_MAX_BURST);
	msm_camera_io_w(0x00000707,
		vfe_vbif_base + VFE40_VBIF_OCMEM_OUT_MAX_BURST);
	msm_camera_io_w(0x00000030,
		vfe_vbif_base + VFE40_VBIF_ARB_CTL);
	msm_camera_io_w(0x04210842,
		vfe_vbif_base + VFE40_VBIF_DDR_ARB_CONF0);
	msm_camera_io_w(0x04210842,
		vfe_vbif_base + VFE40_VBIF_DDR_ARB_CONF1);
	msm_camera_io_w(0x00000001,
		vfe_vbif_base + VFE40_VBIF_ROUND_ROBIN_QOS_ARB);
	msm_camera_io_w(0x22222222,
		vfe_vbif_base + VFE40_VBIF_OUT_AXI_AMEMTYPE_CONF0);
	msm_camera_io_w(0x00002222,
		vfe_vbif_base + VFE40_VBIF_OUT_AXI_AMEMTYPE_CONF1);
	msm_camera_io_w(0x00000FFF,
		vfe_vbif_base + VFE40_VBIF_OUT_AXI_AOOO_EN);
	msm_camera_io_w(0x0FFF0FFF,
		vfe_vbif_base + VFE40_VBIF_OUT_AXI_AOOO);
}

int msm_axi_subdev_init(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	struct msm_cam_media_controller *mctl;
	mctl = v4l2_get_subdev_hostdata(sd);
	if (mctl == NULL) {
		pr_err("%s: mctl is NULL\n", __func__);
		rc = -EINVAL;
		goto mctl_failed;
	}
	axi_ctrl->share_ctrl->axi_ref_cnt++;
	if (axi_ctrl->share_ctrl->axi_ref_cnt > 1)
		return rc;

	spin_lock_init(&axi_ctrl->tasklet_lock);
	INIT_LIST_HEAD(&axi_ctrl->tasklet_q);
	spin_lock_init(&axi_ctrl->share_ctrl->sd_notify_lock);

	axi_ctrl->share_ctrl->vfebase = ioremap(axi_ctrl->vfemem->start,
		resource_size(axi_ctrl->vfemem));
	if (!axi_ctrl->share_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto remap_failed;
	}

	axi_ctrl->share_ctrl->vfe_vbif_base =
		ioremap(axi_ctrl->vfe_vbif_mem->start,
			resource_size(axi_ctrl->vfe_vbif_mem));
	if (!axi_ctrl->share_ctrl->vfe_vbif_base) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto remap_failed;
	}

	if (axi_ctrl->fs_vfe) {
		rc = regulator_enable(axi_ctrl->fs_vfe);
		if (rc) {
			pr_err("%s: Regulator enable failed\n",	__func__);
			goto fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&axi_ctrl->pdev->dev, vfe40_clk_info,
			axi_ctrl->vfe_clk, ARRAY_SIZE(vfe40_clk_info), 1);
	if (rc < 0)
		goto clk_enable_failed;

	axi_ctrl->bus_perf_client =
		msm_bus_scale_register_client(&vfe_bus_client_pdata);
	if (!axi_ctrl->bus_perf_client) {
		pr_err("%s: Registration Failed!\n", __func__);
		axi_ctrl->bus_perf_client = 0;
		goto bus_scale_register_failed;
	}

	msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_PREVIEW);

	rc = iommu_attach_device(mctl->domain, axi_ctrl->iommu_ctx);
	if (rc < 0) {
		pr_err("%s: imgwr attach failed rc = %d\n", __func__, rc);
		rc = -ENODEV;
		goto device_imgwr_attach_failed;
	}

	msm_vfe40_init_vbif_parms(axi_ctrl->share_ctrl->vfe_vbif_base);

	axi_ctrl->share_ctrl->register_total = VFE40_REGISTER_TOTAL;

	spin_lock_init(&axi_ctrl->share_ctrl->stop_flag_lock);
	spin_lock_init(&axi_ctrl->share_ctrl->update_ack_lock);
	spin_lock_init(&axi_ctrl->share_ctrl->start_ack_lock);
	init_completion(&axi_ctrl->share_ctrl->reset_complete);

	if (!axi_ctrl->use_irq_router)
		enable_irq(axi_ctrl->vfeirq->start);

	return rc;

bus_scale_register_failed:
	msm_cam_clk_enable(&axi_ctrl->pdev->dev, vfe40_clk_info,
		axi_ctrl->vfe_clk, ARRAY_SIZE(vfe40_clk_info), 0);
clk_enable_failed:
	if (axi_ctrl->fs_vfe)
		regulator_disable(axi_ctrl->fs_vfe);
fs_failed:
	iounmap(axi_ctrl->share_ctrl->vfebase);
	axi_ctrl->share_ctrl->vfebase = NULL;
remap_failed:
	iommu_detach_device(mctl->domain, axi_ctrl->iommu_ctx);
device_imgwr_attach_failed:
	if (!axi_ctrl->use_irq_router)
		disable_irq(axi_ctrl->vfeirq->start);
mctl_failed:
	return rc;
}

int msm_vfe_subdev_init(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct vfe40_ctrl_type *vfe40_ctrl =
		(struct vfe40_ctrl_type *)v4l2_get_subdevdata(sd);

	spin_lock_init(&vfe40_ctrl->state_lock);
	spin_lock_init(&vfe40_ctrl->stats_bufq_lock);

	vfe40_ctrl->update_linear = false;
	vfe40_ctrl->update_rolloff = false;
	vfe40_ctrl->update_la = false;
	vfe40_ctrl->update_gamma = false;
	vfe40_ctrl->vfe_sof_count_enable = true;
	vfe40_ctrl->hfr_mode = HFR_MODE_OFF;

	memset(&vfe40_ctrl->stats_ctrl, 0,
		   sizeof(struct msm_stats_bufq_ctrl));
	memset(&vfe40_ctrl->stats_ops, 0, sizeof(struct msm_stats_ops));

	return rc;
}

void msm_axi_subdev_release(struct v4l2_subdev *sd)
{
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return;
	}

	CDBG("%s, free_irq\n", __func__);
	axi_ctrl->share_ctrl->axi_ref_cnt--;
	if (axi_ctrl->share_ctrl->axi_ref_cnt > 0)
		return;
	if (!axi_ctrl->use_irq_router)
		disable_irq(axi_ctrl->vfeirq->start);
	tasklet_kill(&axi_ctrl->vfe40_tasklet);

	iommu_detach_device(pmctl->domain, axi_ctrl->iommu_ctx);

	msm_cam_clk_enable(&axi_ctrl->pdev->dev, vfe40_clk_info,
			axi_ctrl->vfe_clk, ARRAY_SIZE(vfe40_clk_info), 0);
	if (axi_ctrl->fs_vfe)
		regulator_disable(axi_ctrl->fs_vfe);

	iounmap(axi_ctrl->share_ctrl->vfebase);
	iounmap(axi_ctrl->share_ctrl->vfe_vbif_base);
	axi_ctrl->share_ctrl->vfebase = NULL;

	if (atomic_read(&irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_EXIT);
	axi_ctrl->bus_perf_client = 0;

	msm_vfe_subdev_release(&axi_ctrl->share_ctrl->vfe40_ctrl->subdev);
}

void msm_vfe_subdev_release(struct v4l2_subdev *sd)
{
	struct vfe40_ctrl_type *vfe40_ctrl =
		(struct vfe40_ctrl_type *)v4l2_get_subdevdata(sd);
	CDBG("vfe subdev release %p\n",
		vfe40_ctrl->share_ctrl->vfebase);
}

void axi_abort(struct axi_ctrl_t *axi_ctrl)
{
	uint8_t  axi_busy_flag = true;
	unsigned long flags;
	/* axi halt command. */

	spin_lock_irqsave(&axi_ctrl->share_ctrl->stop_flag_lock, flags);
	axi_ctrl->share_ctrl->stop_ack_pending  = TRUE;
	spin_unlock_irqrestore(&axi_ctrl->share_ctrl->stop_flag_lock, flags);
	msm_camera_io_w(AXI_HALT,
		axi_ctrl->share_ctrl->vfebase + VFE_AXI_CMD);
	wmb();
	while (axi_busy_flag) {
		if (msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_AXI_STATUS) & 0x1)
			axi_busy_flag = false;
	}
	/* Ensure the write order while writing
	* to the command register using the barrier */
	msm_camera_io_w_mb(AXI_HALT_CLEAR,
		axi_ctrl->share_ctrl->vfebase + VFE_AXI_CMD);

	/* after axi halt, then ok to apply global reset.
	* enable reset_ack and async timer interrupt only while
	* stopping the pipeline.*/
	msm_camera_io_w(0x80000000,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(0xF0000000,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	* to the command register using the barrier */
	msm_camera_io_w_mb(VFE_RESET_UPON_STOP_CMD,
		axi_ctrl->share_ctrl->vfebase + VFE_GLOBAL_RESET);
	if (axi_ctrl->share_ctrl->sync_abort)
		wait_for_completion_interruptible(
			&axi_ctrl->share_ctrl->reset_complete);
}

int axi_config_buffers(struct axi_ctrl_t *axi_ctrl,
	struct msm_camera_vfe_params_t vfe_params)
{
	uint16_t vfe_mode = axi_ctrl->share_ctrl->current_mode
			& ~(VFE_OUTPUTS_RDI0|VFE_OUTPUTS_RDI1);
	int rc = 0;
	switch (vfe_params.cmd_type) {
	case AXI_CMD_PREVIEW:
		if (vfe_mode) {
			if ((axi_ctrl->share_ctrl->current_mode &
				VFE_OUTPUTS_PREVIEW_AND_VIDEO) ||
				(axi_ctrl->share_ctrl->current_mode &
				VFE_OUTPUTS_PREVIEW))
				/* Configure primary channel */
				rc = configure_pingpong_buffers(
					VFE_MSG_START,
					VFE_MSG_OUTPUT_PRIMARY,
					axi_ctrl);
			else
			/* Configure secondary channel */
				rc = configure_pingpong_buffers(
					VFE_MSG_START,
					VFE_MSG_OUTPUT_SECONDARY,
					axi_ctrl);
		}
		if (axi_ctrl->share_ctrl->current_mode &
				VFE_OUTPUTS_RDI0)
			rc = configure_pingpong_buffers(
				VFE_MSG_START, VFE_MSG_OUTPUT_TERTIARY1,
				axi_ctrl);
		if (axi_ctrl->share_ctrl->current_mode &
				VFE_OUTPUTS_RDI1)
			rc = configure_pingpong_buffers(
				VFE_MSG_START, VFE_MSG_OUTPUT_TERTIARY2,
				axi_ctrl);

		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for preview",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		break;
	case AXI_CMD_RAW_CAPTURE:
		rc = configure_pingpong_buffers(
			VFE_MSG_CAPTURE, VFE_MSG_OUTPUT_PRIMARY,
			axi_ctrl);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for snapshot",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		break;
	case AXI_CMD_ZSL:
		rc = configure_pingpong_buffers(VFE_MSG_START,
			VFE_MSG_OUTPUT_PRIMARY, axi_ctrl);
		if (rc < 0)
			goto config_done;
		rc = configure_pingpong_buffers(VFE_MSG_START,
			VFE_MSG_OUTPUT_SECONDARY, axi_ctrl);
		if (rc < 0)
			goto config_done;
		break;
	case AXI_CMD_RECORD:
		if (axi_ctrl->share_ctrl->current_mode &
			VFE_OUTPUTS_PREVIEW_AND_VIDEO) {
			axi_ctrl->share_ctrl->outpath.out1.inst_handle =
				vfe_params.inst_handle;
			rc = configure_pingpong_buffers(
				VFE_MSG_START_RECORDING,
				VFE_MSG_OUTPUT_SECONDARY,
				axi_ctrl);
		} else if (axi_ctrl->share_ctrl->current_mode &
			VFE_OUTPUTS_VIDEO_AND_PREVIEW) {
			axi_ctrl->share_ctrl->outpath.out0.inst_handle =
				vfe_params.inst_handle;
			rc = configure_pingpong_buffers(
				VFE_MSG_START_RECORDING,
				VFE_MSG_OUTPUT_PRIMARY,
				axi_ctrl);
		}
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for video",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		break;
	case AXI_CMD_LIVESHOT:
		axi_ctrl->share_ctrl->outpath.out0.inst_handle =
			vfe_params.inst_handle;
		rc = configure_pingpong_buffers(VFE_MSG_CAPTURE,
					VFE_MSG_OUTPUT_PRIMARY, axi_ctrl);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for primary output",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		break;
	case AXI_CMD_CAPTURE:
		if (axi_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		axi_ctrl->share_ctrl->current_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG) {

			/* Configure primary channel for JPEG */
			rc = configure_pingpong_buffers(
				VFE_MSG_JPEG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY,
				axi_ctrl);
		} else {
			/* Configure primary channel */
			rc = configure_pingpong_buffers(
				VFE_MSG_CAPTURE,
				VFE_MSG_OUTPUT_PRIMARY,
				axi_ctrl);
		}
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for primary output",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		/* Configure secondary channel */
		rc = configure_pingpong_buffers(
				VFE_MSG_CAPTURE, VFE_MSG_OUTPUT_SECONDARY,
				axi_ctrl);
		if (rc < 0) {
			pr_err("%s error configuring pingpong buffers for secondary output",
				__func__);
			rc = -EINVAL;
			goto config_done;
		}
		break;
	default:
		rc = -EINVAL;
		break;

	}
config_done:
	return rc;
}

void axi_start(struct msm_cam_media_controller *pmctl,
	struct axi_ctrl_t *axi_ctrl, struct msm_camera_vfe_params_t vfe_params)
{
	uint32_t irq_comp_mask = 0, irq_mask = 0;
	int rc = 0;
	uint32_t reg_update = 0;
	uint16_t operation_mode =
		(axi_ctrl->share_ctrl->current_mode &
		~(VFE_OUTPUTS_RDI0|VFE_OUTPUTS_RDI1));
	rc = axi_config_buffers(axi_ctrl, vfe_params);
	if (rc < 0)
		return;

	switch (vfe_params.cmd_type) {
	case AXI_CMD_PREVIEW:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_PREVIEW);
		break;
	case AXI_CMD_CAPTURE:
	case AXI_CMD_RAW_CAPTURE:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_CAPTURE);
		break;
	case AXI_CMD_RECORD:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_VIDEO);
		return;
	case AXI_CMD_ZSL:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_ZSL);
		break;
	case AXI_CMD_LIVESHOT:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_LIVESHOT);
		return;
	default:
		return;
	}

	irq_comp_mask =
		msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_IRQ_COMP_MASK);
	irq_mask = msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
		if (vfe_params.cmd_type == AXI_CMD_RAW_CAPTURE) {
			irq_comp_mask |=
				0x1 << axi_ctrl->share_ctrl->outpath.out0.ch0;
		} else {
			irq_comp_mask |= (
				0x1 << axi_ctrl->share_ctrl->outpath.out0.ch0 |
				0x1 << axi_ctrl->share_ctrl->outpath.out0.ch1);
		}
		irq_mask |= VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK;
	} else if (axi_ctrl->share_ctrl->outpath.output_mode &
			   VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
		irq_comp_mask |= (
			0x1 << axi_ctrl->share_ctrl->outpath.out0.ch0 |
			0x1 << axi_ctrl->share_ctrl->outpath.out0.ch1 |
			0x1 << axi_ctrl->share_ctrl->outpath.out0.ch2);
		irq_mask |= VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK;
	}
	if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
		irq_comp_mask |= (
			0x1 << (axi_ctrl->share_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (axi_ctrl->share_ctrl->outpath.out1.ch1 + 8));
		irq_mask |= VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK;
	} else if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
		irq_comp_mask |= (
			0x1 << (axi_ctrl->share_ctrl->outpath.out1.ch0 + 8) |
			0x1 << (axi_ctrl->share_ctrl->outpath.out1.ch1 + 8) |
			0x1 << (axi_ctrl->share_ctrl->outpath.out1.ch2 + 8));
		irq_mask |= VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK;
	}
	if (axi_ctrl->share_ctrl->outpath.output_mode &
		VFE40_OUTPUT_MODE_TERTIARY1) {
		irq_mask |= (0x1 << (axi_ctrl->share_ctrl->outpath.out2.ch0 +
			VFE_WM_OFFSET));
	}
	if (axi_ctrl->share_ctrl->outpath.output_mode &
		VFE40_OUTPUT_MODE_TERTIARY2) {
		irq_mask |= (0x1 << (axi_ctrl->share_ctrl->outpath.out3.ch0 +
			VFE_WM_OFFSET));
	}

	msm_camera_io_w(irq_comp_mask,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_COMP_MASK);
	msm_camera_io_w(irq_mask, axi_ctrl->share_ctrl->vfebase +
			VFE_IRQ_MASK_0);

	switch (vfe_params.cmd_type) {
	case AXI_CMD_PREVIEW: {
		switch (operation_mode) {
		case VFE_OUTPUTS_PREVIEW:
		case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
			if (axi_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_PRIMARY) {
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch1]);
			} else if (axi_ctrl->share_ctrl->outpath.output_mode &
					VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch1]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch2]);
			}
			break;
		default:
			if (axi_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_SECONDARY) {
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out1.ch0]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out1.ch1]);
			} else if (axi_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out1.ch0]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out1.ch1]);
				msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase
					+ vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out1.ch2]);
			}
			break;
			}
		}
		break;
	default:
		if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			if (vfe_params.cmd_type == AXI_CMD_RAW_CAPTURE) {
				msm_camera_io_w(1,
					axi_ctrl->share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch0]);
			} else {
				msm_camera_io_w(1,
					axi_ctrl->share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[axi_ctrl
					->share_ctrl->outpath.out0.ch0]);
				msm_camera_io_w(1,
					axi_ctrl->share_ctrl->vfebase +
					vfe40_AXI_WM_CFG[axi_ctrl->
					share_ctrl->outpath.out0.ch1]);
			}
		} else if (axi_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch1]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch2]);
		}

		if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch1]);
		} else if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch1]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch2]);
		}
		break;
	}

	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI0)
		msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[axi_ctrl->share_ctrl->
			outpath.out2.ch0]);
	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI1)
		msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
			vfe40_AXI_WM_CFG[axi_ctrl->share_ctrl->
			outpath.out3.ch0]);

	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI0) {
		irq_mask |= VFE_IRQ_STATUS0_RDI0_REG_UPDATE_MASK;
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->rdi0_update_ack_pending,
				0, 1))
			reg_update |= 0x2;
	}
	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI1) {
		irq_mask |= VFE_IRQ_STATUS0_RDI1_REG_UPDATE_MASK;
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->rdi1_update_ack_pending,
				0, 1))
			reg_update |= 0x4;
	}
	msm_camera_io_w(irq_mask, axi_ctrl->share_ctrl->vfebase +
		VFE_IRQ_MASK_0);
	if (operation_mode) {
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->pix0_update_ack_pending,
				0, 1))
			reg_update |= 0x1;
	}

	msm_camera_io_w_mb(reg_update,
			axi_ctrl->share_ctrl->vfebase +
			VFE_REG_UPDATE_CMD);
	axi_ctrl->share_ctrl->operation_mode |=
		axi_ctrl->share_ctrl->current_mode;
	axi_enable_irq(axi_ctrl->share_ctrl);
}

void axi_stop(struct msm_cam_media_controller *pmctl,
	struct axi_ctrl_t *axi_ctrl, struct msm_camera_vfe_params_t vfe_params)
{
	uint32_t reg_update = 0;
	uint32_t operation_mode =
	axi_ctrl->share_ctrl->current_mode & ~(VFE_OUTPUTS_RDI0|
		VFE_OUTPUTS_RDI1);

	switch (vfe_params.cmd_type) {
	case AXI_CMD_PREVIEW:
	case AXI_CMD_CAPTURE:
	case AXI_CMD_RAW_CAPTURE:
	case AXI_CMD_ZSL:
		break;
	case AXI_CMD_RECORD:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_PREVIEW);
		return;
	case AXI_CMD_LIVESHOT:
		msm_camera_bus_scale_cfg(axi_ctrl->bus_perf_client, S_VIDEO);
		return;
	default:
		return;
	}

	if (axi_ctrl->share_ctrl->stop_immediately) {
		axi_disable_irq(axi_ctrl->share_ctrl);
		axi_stop_process(axi_ctrl->share_ctrl);
		return;
	}

	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI0) {
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->rdi0_update_ack_pending, 0, 2))
			reg_update |= 0x2;
	}
	if (axi_ctrl->share_ctrl->current_mode & VFE_OUTPUTS_RDI1) {
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->rdi1_update_ack_pending, 0, 2))
			reg_update |= 0x4;
	}
	if (operation_mode) {
		if (!atomic_cmpxchg(
			&axi_ctrl->share_ctrl->pix0_update_ack_pending, 0, 2))
			reg_update |= 0x1;
	}
	msm_camera_io_w_mb(reg_update,
		axi_ctrl->share_ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static int msm_axi_config(struct v4l2_subdev *sd, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_isp_cmd vfecmd;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	int rc = 0, vfe_cmd_type = 0, rdi_mode = 0;

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}
	memset(&cfgcmd, 0, sizeof(struct msm_vfe_cfg_cmd));
	if (NULL != arg) {
		if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	}
	memset(&vfecmd, 0, sizeof(struct msm_isp_cmd));
	if (NULL != cfgcmd.value) {
		if (copy_from_user(&vfecmd,
				(void __user *)(cfgcmd.value),
				sizeof(vfecmd))) {
			pr_err("%s %d: copy_from_user failed\n", __func__,
				__LINE__);
			return -EFAULT;
		}
	}

	vfe_cmd_type = (cfgcmd.cmd_type & ~(CMD_AXI_CFG_TERT1|
		CMD_AXI_CFG_TERT2));
	switch (cfgcmd.cmd_type) {
	case CMD_AXI_CFG_TERT1:{
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio)
			return -ENOMEM;

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			return -EFAULT;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_TERT1, axio);
		kfree(axio);
		return rc;
		}
	case CMD_AXI_CFG_TERT2:{
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio)
			return -ENOMEM;

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			return -EFAULT;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_TERT2, axio);
		kfree(axio);
		return rc;
		}
	case CMD_AXI_CFG_TERT1|CMD_AXI_CFG_TERT2:{
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio)
			return -ENOMEM;

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			return -EFAULT;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_TERT1|OUTPUT_TERT2, axio);
		kfree(axio);
		return rc;
		}
	default:
		if (cfgcmd.cmd_type & CMD_AXI_CFG_TERT1)
			rdi_mode |= OUTPUT_TERT1;
		if (cfgcmd.cmd_type & CMD_AXI_CFG_TERT2)
			rdi_mode |= OUTPUT_TERT2;
	}
	switch (vfe_cmd_type) {
	case CMD_AXI_CFG_PRIM: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl, rdi_mode|OUTPUT_PRIM, axio);
		kfree(axio);
		break;
		}
	case CMD_AXI_CFG_PRIM_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl, rdi_mode|OUTPUT_PRIM_ALL_CHNLS,
			axio);
		kfree(axio);
		break;
		}
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl,
			rdi_mode|OUTPUT_PRIM|OUTPUT_SEC, axio);
		kfree(axio);
		break;
		}
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl,
			rdi_mode|OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS, axio);
		kfree(axio);
		break;
		}
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl,
			rdi_mode|OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC, axio);
		kfree(axio);
		break;
		}

	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC_ALL_CHNLS:
		pr_err("%s Invalid/Unsupported AXI configuration %x",
			__func__, cfgcmd.cmd_type);
		break;
	case CMD_AXI_START: {
		struct msm_camera_vfe_params_t vfe_params;
		if (copy_from_user(&vfe_params,
				(void __user *)(vfecmd.value),
				sizeof(struct msm_camera_vfe_params_t))) {
				return -EFAULT;
		}
		axi_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;
		axi_start(pmctl, axi_ctrl, vfe_params);
		}
		break;
	case CMD_AXI_STOP: {
		struct msm_camera_vfe_params_t vfe_params;
		if (copy_from_user(&vfe_params,
				(void __user *)(vfecmd.value),
				sizeof(struct msm_camera_vfe_params_t))) {
				return -EFAULT;
		}
		axi_ctrl->share_ctrl->current_mode =
			vfe_params.operation_mode;
		axi_ctrl->share_ctrl->stop_immediately =
			vfe_params.stop_immediately;
		axi_stop(pmctl, axi_ctrl, vfe_params);
		}
		break;
	case CMD_AXI_RESET:
		axi_reset(axi_ctrl);
		break;
	case CMD_AXI_ABORT:
		if (copy_from_user(&axi_ctrl->share_ctrl->sync_abort,
				(void __user *)(vfecmd.value),
				sizeof(uint8_t))) {
				return -EFAULT;
		}
		axi_abort(axi_ctrl);
		break;
	default:
		pr_err("%s Unsupported AXI configuration %x ", __func__,
			cfgcmd.cmd_type);
		break;
	}
	return rc;
}

static void msm_axi_process_irq(struct v4l2_subdev *sd, void *arg)
{
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	uint32_t irqstatus = (uint32_t) arg;

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return;
	}

	/* next, check output path related interrupts. */
	if (irqstatus &
		VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK) {
		CDBG("Image composite done 0 irq occured.\n");
		vfe40_process_output_path_irq_0(axi_ctrl);
	}
	if (irqstatus &
		VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK) {
		CDBG("Image composite done 1 irq occured.\n");
		vfe40_process_output_path_irq_1(axi_ctrl);
	}

	if (axi_ctrl->share_ctrl->comp_output_mode &
		VFE40_OUTPUT_MODE_TERTIARY1)
		if (irqstatus & (0x1 << (axi_ctrl->share_ctrl->outpath.out2.ch0
			+ VFE_WM_OFFSET)))
			vfe40_process_output_path_irq_rdi0(axi_ctrl);
	if (axi_ctrl->share_ctrl->comp_output_mode &
		VFE40_OUTPUT_MODE_TERTIARY2)
		if (irqstatus & (0x1 << (axi_ctrl->share_ctrl->outpath.out3.ch0
			+ VFE_WM_OFFSET)))
			vfe40_process_output_path_irq_rdi1(axi_ctrl);

	/* in snapshot mode if done then send
	snapshot done message */
	if (
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_THUMB_AND_JPEG ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode &
			VFE_OUTPUTS_RAW) {
		if ((axi_ctrl->share_ctrl->outpath.out0.capture_cnt == 0)
				&& (axi_ctrl->share_ctrl->outpath.out1.
				capture_cnt == 0)) {
			msm_camera_io_w_mb(
				CAMIF_COMMAND_STOP_IMMEDIATELY,
				axi_ctrl->share_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
			axi_disable_irq(axi_ctrl->share_ctrl);
			vfe40_send_isp_msg(&axi_ctrl->subdev,
				axi_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_PIX0_UPDATE_ACK);
			vfe40_send_isp_msg(&axi_ctrl->subdev,
				axi_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_SNAPSHOT_DONE);
		}
	}
}

static int msm_axi_buf_cfg(struct v4l2_subdev *sd, void __user *arg)
{
	struct msm_camvfe_params *vfe_params =
		(struct msm_camvfe_params *)arg;
	struct msm_vfe_cfg_cmd *cmd = vfe_params->vfe_cfg;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	void *data = vfe_params->data;
	int rc = 0;

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}

	switch (cmd->cmd_type) {
	case CMD_CONFIG_PING_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->ping = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_PONG_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->pong = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_FREE_BUF_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->free_buf = *((struct msm_free_buf *)data);
	}
		break;
	default:
		pr_err("%s Unsupported AXI Buf config %x ", __func__,
			cmd->cmd_type);
	}
	return rc;
};

static const struct v4l2_subdev_internal_ops msm_vfe_internal_ops;

static long msm_axi_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	switch (cmd) {
	case VIDIOC_MSM_AXI_INIT:
		rc = msm_axi_subdev_init(sd);
		break;
	case VIDIOC_MSM_AXI_CFG:
		rc = msm_axi_config(sd, arg);
		break;
	case VIDIOC_MSM_AXI_IRQ:
		msm_axi_process_irq(sd, arg);
		rc = 0;
		break;
	case VIDIOC_MSM_AXI_BUF_CFG:
		msm_axi_buf_cfg(sd, arg);
		rc = 0;
		break;
	case VIDIOC_MSM_AXI_RELEASE:
		msm_axi_subdev_release(sd);
		rc = 0;
		break;
	case VIDIOC_MSM_AXI_RDI_COUNT_UPDATE: {
		struct rdi_count_msg *msg = (struct rdi_count_msg *)arg;
		struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
		switch (msg->rdi_interface) {
		case RDI_0:
			axi_ctrl->share_ctrl->rdi0FrameId = msg->count;
			rc = 0;
			break;
		case RDI_1:
			axi_ctrl->share_ctrl->rdi1FrameId = msg->count;
			rc = 0;
			break;
		case RDI_2:
			axi_ctrl->share_ctrl->rdi2FrameId = msg->count;
			rc = 0;
			break;
		default:
			pr_err("%s: Incorrect interface sent\n", __func__);
			rc = -EINVAL;
			break;
		}
		break;
	}
	default:
		pr_err("%s: command %d not found\n", __func__,
						_IOC_NR(cmd));
		break;
	}
	return rc;
}

static const struct v4l2_subdev_core_ops msm_axi_subdev_core_ops = {
	.ioctl = msm_axi_subdev_ioctl,
	.interrupt_service_routine = msm_axi_subdev_isr_routine,
};

static const struct v4l2_subdev_video_ops msm_axi_subdev_video_ops = {
	.s_crystal_freq = msm_axi_subdev_s_crystal_freq,
};

static const struct v4l2_subdev_ops msm_axi_subdev_ops = {
	.core = &msm_axi_subdev_core_ops,
	.video = &msm_axi_subdev_video_ops,
};

static const struct v4l2_subdev_internal_ops msm_axi_internal_ops;

static int __devinit vfe40_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct axi_ctrl_t *axi_ctrl;
	struct vfe40_ctrl_type *vfe40_ctrl;
	struct vfe_share_ctrl_t *share_ctrl;
	struct intr_table_entry irq_req;
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
	axi_ctrl->share_ctrl->axi_ref_cnt = 0;
	v4l2_subdev_init(&axi_ctrl->subdev, &msm_axi_subdev_ops);
	axi_ctrl->subdev.internal_ops = &msm_axi_internal_ops;
	axi_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(axi_ctrl->subdev.name,
			 sizeof(axi_ctrl->subdev.name), "axi");
	v4l2_set_subdevdata(&axi_ctrl->subdev, axi_ctrl);
	axi_ctrl->pdev = pdev;

	sd_info.sdev_type = AXI_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = 0;
	msm_cam_register_subdev_node(&axi_ctrl->subdev, &sd_info);

	media_entity_init(&axi_ctrl->subdev.entity, 0, NULL, 0);
	axi_ctrl->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	axi_ctrl->subdev.entity.group_id = AXI_DEV;
	axi_ctrl->subdev.entity.name = pdev->name;
	axi_ctrl->subdev.entity.revision = axi_ctrl->subdev.devnode->num;

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

	axi_ctrl->vfe_vbif_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "vfe_vbif");
	if (!axi_ctrl->vfe_vbif_mem) {
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

	axi_ctrl->fs_vfe = regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(axi_ctrl->fs_vfe)) {
		pr_err("%s: Regulator get failed %ld\n", __func__,
			PTR_ERR(axi_ctrl->fs_vfe));
		axi_ctrl->fs_vfe = NULL;
	}

	/* Register subdev node before requesting irq since
	 * irq_num is needed by msm_cam_server */
	sd_info.sdev_type = VFE_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = axi_ctrl->vfeirq->start;
	msm_cam_register_subdev_node(&vfe40_ctrl->subdev, &sd_info);

	media_entity_init(&vfe40_ctrl->subdev.entity, 0, NULL, 0);
	vfe40_ctrl->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vfe40_ctrl->subdev.entity.group_id = VFE_DEV;
	vfe40_ctrl->subdev.entity.name = pdev->name;
	vfe40_ctrl->subdev.entity.revision = vfe40_ctrl->subdev.devnode->num;

	/* Request for this device irq from the camera server. If the
	 * IRQ Router is present on this target, the interrupt will be
	 * handled by the camera server and the interrupt service
	 * routine called. If the request_irq call returns ENXIO, then
	 * the IRQ Router hardware is not present on this target. We
	 * have to request for the irq ourselves and register the
	 * appropriate interrupt handler. */
	axi_ctrl->use_irq_router = true;
	irq_req.cam_hw_idx       = MSM_CAM_HW_VFE0 + pdev->id;
	irq_req.dev_name         = "vfe";
	irq_req.irq_idx          = CAMERA_SS_IRQ_8;
	irq_req.irq_num          = axi_ctrl->vfeirq->start;
	irq_req.is_composite     = 0;
	irq_req.irq_trigger_type = IRQF_TRIGGER_RISING;
	irq_req.num_hwcore       = 1;
	irq_req.subdev_list[0]   = &axi_ctrl->subdev;
	irq_req.data             = (void *)axi_ctrl;
	rc = msm_cam_server_request_irq(&irq_req);
	if (rc == -ENXIO) {
		/* IRQ Router hardware is not present on this hardware.
		 * Request for the IRQ and register the interrupt handler. */
		axi_ctrl->use_irq_router = false;
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
	} else if (rc < 0) {
		pr_err("%s Error registering irq ", __func__);
		goto vfe40_no_resource;
	}

	/*get device context for IOMMU*/
	if (pdev->id == 0)
		axi_ctrl->iommu_ctx = msm_iommu_get_ctx("vfe0");
	else if (pdev->id == 1)
		axi_ctrl->iommu_ctx = msm_iommu_get_ctx("vfe1");
	if (!axi_ctrl->iommu_ctx) {
		release_mem_region(axi_ctrl->vfemem->start,
			resource_size(axi_ctrl->vfemem));
		pr_err("%s: No iommu fw context found\n", __func__);
		rc = -ENODEV;
		goto vfe40_no_resource;
	}

	tasklet_init(&axi_ctrl->vfe40_tasklet,
		axi40_do_tasklet, (unsigned long)axi_ctrl);

	vfe40_ctrl->pdev = pdev;
	/*enable bayer stats by default*/
	vfe40_ctrl->ver_num.main = 4;

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
