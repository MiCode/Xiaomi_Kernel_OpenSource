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
 *
 */
#ifndef __MSM_ISP_H__
#define __MSM_ISP_H__

#define BIT(nr)			(1UL << (nr))

/* ISP message IDs */
#define MSG_ID_RESET_ACK                0
#define MSG_ID_START_ACK                1
#define MSG_ID_STOP_ACK                 2
#define MSG_ID_UPDATE_ACK               3
#define MSG_ID_OUTPUT_P                 4
#define MSG_ID_OUTPUT_T                 5
#define MSG_ID_OUTPUT_S                 6
#define MSG_ID_OUTPUT_V                 7
#define MSG_ID_SNAPSHOT_DONE            8
#define MSG_ID_STATS_AEC                9
#define MSG_ID_STATS_AF                 10
#define MSG_ID_STATS_AWB                11
#define MSG_ID_STATS_RS                 12
#define MSG_ID_STATS_CS                 13
#define MSG_ID_STATS_IHIST              14
#define MSG_ID_STATS_SKIN               15
#define MSG_ID_EPOCH1                   16
#define MSG_ID_EPOCH2                   17
#define MSG_ID_SYNC_TIMER0_DONE         18
#define MSG_ID_SYNC_TIMER1_DONE         19
#define MSG_ID_SYNC_TIMER2_DONE         20
#define MSG_ID_ASYNC_TIMER0_DONE        21
#define MSG_ID_ASYNC_TIMER1_DONE        22
#define MSG_ID_ASYNC_TIMER2_DONE        23
#define MSG_ID_ASYNC_TIMER3_DONE        24
#define MSG_ID_AE_OVERFLOW              25
#define MSG_ID_AF_OVERFLOW              26
#define MSG_ID_AWB_OVERFLOW             27
#define MSG_ID_RS_OVERFLOW              28
#define MSG_ID_CS_OVERFLOW              29
#define MSG_ID_IHIST_OVERFLOW           30
#define MSG_ID_SKIN_OVERFLOW            31
#define MSG_ID_AXI_ERROR                32
#define MSG_ID_CAMIF_OVERFLOW           33
#define MSG_ID_VIOLATION                34
#define MSG_ID_CAMIF_ERROR              35
#define MSG_ID_BUS_OVERFLOW             36
#define MSG_ID_SOF_ACK                  37
#define MSG_ID_STOP_REC_ACK             38
#define MSG_ID_STATS_AWB_AEC            39
#define MSG_ID_OUTPUT_PRIMARY           40
#define MSG_ID_OUTPUT_SECONDARY         41
#define MSG_ID_STATS_COMPOSITE          42
#define MSG_ID_OUTPUT_TERTIARY1         43
#define MSG_ID_STOP_LS_ACK              44
#define MSG_ID_OUTPUT_TERTIARY2         45

/* ISP command IDs */
#define VFE_CMD_DUMMY_0                                 0
#define VFE_CMD_SET_CLK                                 1
#define VFE_CMD_RESET                                   2
#define VFE_CMD_START                                   3
#define VFE_CMD_TEST_GEN_START                          4
#define VFE_CMD_OPERATION_CFG                           5
#define VFE_CMD_AXI_OUT_CFG                             6
#define VFE_CMD_CAMIF_CFG                               7
#define VFE_CMD_AXI_INPUT_CFG                           8
#define VFE_CMD_BLACK_LEVEL_CFG                         9
#define VFE_CMD_MESH_ROLL_OFF_CFG                       10
#define VFE_CMD_DEMUX_CFG                               11
#define VFE_CMD_FOV_CFG                                 12
#define VFE_CMD_MAIN_SCALER_CFG                         13
#define VFE_CMD_WB_CFG                                  14
#define VFE_CMD_COLOR_COR_CFG                           15
#define VFE_CMD_RGB_G_CFG                               16
#define VFE_CMD_LA_CFG                                  17
#define VFE_CMD_CHROMA_EN_CFG                           18
#define VFE_CMD_CHROMA_SUP_CFG                          19
#define VFE_CMD_MCE_CFG                                 20
#define VFE_CMD_SK_ENHAN_CFG                            21
#define VFE_CMD_ASF_CFG                                 22
#define VFE_CMD_S2Y_CFG                                 23
#define VFE_CMD_S2CbCr_CFG                              24
#define VFE_CMD_CHROMA_SUBS_CFG                         25
#define VFE_CMD_OUT_CLAMP_CFG                           26
#define VFE_CMD_FRAME_SKIP_CFG                          27
#define VFE_CMD_DUMMY_1                                 28
#define VFE_CMD_DUMMY_2                                 29
#define VFE_CMD_DUMMY_3                                 30
#define VFE_CMD_UPDATE                                  31
#define VFE_CMD_BL_LVL_UPDATE                           32
#define VFE_CMD_DEMUX_UPDATE                            33
#define VFE_CMD_FOV_UPDATE                              34
#define VFE_CMD_MAIN_SCALER_UPDATE                      35
#define VFE_CMD_WB_UPDATE                               36
#define VFE_CMD_COLOR_COR_UPDATE                        37
#define VFE_CMD_RGB_G_UPDATE                            38
#define VFE_CMD_LA_UPDATE                               39
#define VFE_CMD_CHROMA_EN_UPDATE                        40
#define VFE_CMD_CHROMA_SUP_UPDATE                       41
#define VFE_CMD_MCE_UPDATE                              42
#define VFE_CMD_SK_ENHAN_UPDATE                         43
#define VFE_CMD_S2CbCr_UPDATE                           44
#define VFE_CMD_S2Y_UPDATE                              45
#define VFE_CMD_ASF_UPDATE                              46
#define VFE_CMD_FRAME_SKIP_UPDATE                       47
#define VFE_CMD_CAMIF_FRAME_UPDATE                      48
#define VFE_CMD_STATS_AF_UPDATE                         49
#define VFE_CMD_STATS_AE_UPDATE                         50
#define VFE_CMD_STATS_AWB_UPDATE                        51
#define VFE_CMD_STATS_RS_UPDATE                         52
#define VFE_CMD_STATS_CS_UPDATE                         53
#define VFE_CMD_STATS_SKIN_UPDATE                       54
#define VFE_CMD_STATS_IHIST_UPDATE                      55
#define VFE_CMD_DUMMY_4                                 56
#define VFE_CMD_EPOCH1_ACK                              57
#define VFE_CMD_EPOCH2_ACK                              58
#define VFE_CMD_START_RECORDING                         59
#define VFE_CMD_STOP_RECORDING                          60
#define VFE_CMD_DUMMY_5                                 61
#define VFE_CMD_DUMMY_6                                 62
#define VFE_CMD_CAPTURE                                 63
#define VFE_CMD_DUMMY_7                                 64
#define VFE_CMD_STOP                                    65
#define VFE_CMD_GET_HW_VERSION                          66
#define VFE_CMD_GET_FRAME_SKIP_COUNTS                   67
#define VFE_CMD_OUTPUT1_BUFFER_ENQ                      68
#define VFE_CMD_OUTPUT2_BUFFER_ENQ                      69
#define VFE_CMD_OUTPUT3_BUFFER_ENQ                      70
#define VFE_CMD_JPEG_OUT_BUF_ENQ                        71
#define VFE_CMD_RAW_OUT_BUF_ENQ                         72
#define VFE_CMD_RAW_IN_BUF_ENQ                          73
#define VFE_CMD_STATS_AF_ENQ                            74
#define VFE_CMD_STATS_AE_ENQ                            75
#define VFE_CMD_STATS_AWB_ENQ                           76
#define VFE_CMD_STATS_RS_ENQ                            77
#define VFE_CMD_STATS_CS_ENQ                            78
#define VFE_CMD_STATS_SKIN_ENQ                          79
#define VFE_CMD_STATS_IHIST_ENQ                         80
#define VFE_CMD_DUMMY_8                                 81
#define VFE_CMD_JPEG_ENC_CFG                            82
#define VFE_CMD_DUMMY_9                                 83
#define VFE_CMD_STATS_AF_START                          84
#define VFE_CMD_STATS_AF_STOP                           85
#define VFE_CMD_STATS_AE_START                          86
#define VFE_CMD_STATS_AE_STOP                           87
#define VFE_CMD_STATS_AWB_START                         88
#define VFE_CMD_STATS_AWB_STOP                          89
#define VFE_CMD_STATS_RS_START                          90
#define VFE_CMD_STATS_RS_STOP                           91
#define VFE_CMD_STATS_CS_START                          92
#define VFE_CMD_STATS_CS_STOP                           93
#define VFE_CMD_STATS_SKIN_START                        94
#define VFE_CMD_STATS_SKIN_STOP                         95
#define VFE_CMD_STATS_IHIST_START                       96
#define VFE_CMD_STATS_IHIST_STOP                        97
#define VFE_CMD_DUMMY_10                                98
#define VFE_CMD_SYNC_TIMER_SETTING                      99
#define VFE_CMD_ASYNC_TIMER_SETTING                     100
#define VFE_CMD_LIVESHOT                                101
#define VFE_CMD_LA_SETUP                                102
#define VFE_CMD_LINEARIZATION_CFG                       103
#define VFE_CMD_DEMOSAICV3                              104
#define VFE_CMD_DEMOSAICV3_ABCC_CFG                     105
#define VFE_CMD_DEMOSAICV3_DBCC_CFG                     106
#define VFE_CMD_DEMOSAICV3_DBPC_CFG                     107
#define VFE_CMD_DEMOSAICV3_ABF_CFG                      108
#define VFE_CMD_DEMOSAICV3_ABCC_UPDATE                  109
#define VFE_CMD_DEMOSAICV3_DBCC_UPDATE                  110
#define VFE_CMD_DEMOSAICV3_DBPC_UPDATE                  111
#define VFE_CMD_XBAR_CFG                                112
#define VFE_CMD_MODULE_CFG                              113
#define VFE_CMD_ZSL                                     114
#define VFE_CMD_LINEARIZATION_UPDATE                    115
#define VFE_CMD_DEMOSAICV3_ABF_UPDATE                   116
#define VFE_CMD_CLF_CFG                                 117
#define VFE_CMD_CLF_LUMA_UPDATE                         118
#define VFE_CMD_CLF_CHROMA_UPDATE                       119
#define VFE_CMD_PCA_ROLL_OFF_CFG                        120
#define VFE_CMD_PCA_ROLL_OFF_UPDATE                     121
#define VFE_CMD_GET_REG_DUMP                            122
#define VFE_CMD_GET_LINEARIZATON_TABLE                  123
#define VFE_CMD_GET_MESH_ROLLOFF_TABLE                  124
#define VFE_CMD_GET_PCA_ROLLOFF_TABLE                   125
#define VFE_CMD_GET_RGB_G_TABLE                         126
#define VFE_CMD_GET_LA_TABLE                            127
#define VFE_CMD_DEMOSAICV3_UPDATE                       128
#define VFE_CMD_ACTIVE_REGION_CFG                       129
#define VFE_CMD_COLOR_PROCESSING_CONFIG                 130
#define VFE_CMD_STATS_WB_AEC_CONFIG                     131
#define VFE_CMD_STATS_WB_AEC_UPDATE                     132
#define VFE_CMD_Y_GAMMA_CONFIG                          133
#define VFE_CMD_SCALE_OUTPUT1_CONFIG                    134
#define VFE_CMD_SCALE_OUTPUT2_CONFIG                    135
#define VFE_CMD_CAPTURE_RAW                             136
#define VFE_CMD_STOP_LIVESHOT                           137
#define VFE_CMD_RECONFIG_VFE                            138

struct msm_isp_cmd {
	int32_t  id;
	uint16_t length;
	void     *value;
};

#define VPE_CMD_DUMMY_0                                 0
#define VPE_CMD_INIT                                    1
#define VPE_CMD_DEINIT                                  2
#define VPE_CMD_ENABLE                                  3
#define VPE_CMD_DISABLE                                 4
#define VPE_CMD_RESET                                   5
#define VPE_CMD_FLUSH                                   6
#define VPE_CMD_OPERATION_MODE_CFG                      7
#define VPE_CMD_INPUT_PLANE_CFG                         8
#define VPE_CMD_OUTPUT_PLANE_CFG                        9
#define VPE_CMD_INPUT_PLANE_UPDATE                      10
#define VPE_CMD_SCALE_CFG_TYPE                          11
#define VPE_CMD_ZOOM                                    13
#define VPE_CMD_MAX                                     14

#define MSM_PP_CMD_TYPE_NOT_USED        0  /* not used */
#define MSM_PP_CMD_TYPE_VPE             1  /* VPE cmd */
#define MSM_PP_CMD_TYPE_MCTL            2  /* MCTL cmd */

#define MCTL_CMD_DUMMY_0                0  /* not used */
#define MCTL_CMD_GET_FRAME_BUFFER       1  /* reserve a free frame buffer */
#define MCTL_CMD_PUT_FRAME_BUFFER       2  /* return the free frame buffer */
#define MCTL_CMD_DIVERT_FRAME_PP_PATH   3  /* divert frame for pp */

/* event typese sending to MCTL PP module */
#define MCTL_PP_EVENT_NOTUSED           0
#define MCTL_PP_EVENT_CMD_ACK           1

#define VPE_OPERATION_MODE_CFG_LEN      4
#define VPE_INPUT_PLANE_CFG_LEN         24
#define VPE_OUTPUT_PLANE_CFG_LEN        20
#define VPE_INPUT_PLANE_UPDATE_LEN      12
#define VPE_SCALER_CONFIG_LEN           260
#define VPE_DIS_OFFSET_CFG_LEN          12


#define CAPTURE_WIDTH          1280
#define IMEM_Y_SIZE            (CAPTURE_WIDTH*16)
#define IMEM_CBCR_SIZE         (CAPTURE_WIDTH*8)

#define IMEM_Y_PING_OFFSET     0x2E000000
#define IMEM_CBCR_PING_OFFSET  (IMEM_Y_PING_OFFSET + IMEM_Y_SIZE)

#define IMEM_Y_PONG_OFFSET     (IMEM_CBCR_PING_OFFSET + IMEM_CBCR_SIZE)
#define IMEM_CBCR_PONG_OFFSET  (IMEM_Y_PONG_OFFSET + IMEM_Y_SIZE)


struct msm_vpe_op_mode_cfg {
	uint8_t op_mode_cfg[VPE_OPERATION_MODE_CFG_LEN];
};

struct msm_vpe_input_plane_cfg {
	uint8_t input_plane_cfg[VPE_INPUT_PLANE_CFG_LEN];
};

struct msm_vpe_output_plane_cfg {
	uint8_t output_plane_cfg[VPE_OUTPUT_PLANE_CFG_LEN];
};

struct msm_vpe_input_plane_update_cfg {
	uint8_t input_plane_update_cfg[VPE_INPUT_PLANE_UPDATE_LEN];
};

struct msm_vpe_scaler_cfg {
	uint8_t scaler_cfg[VPE_SCALER_CONFIG_LEN];
};

struct msm_vpe_flush_frame_buffer {
	uint32_t src_buf_handle;
	uint32_t dest_buf_handle;
	int path;
};

struct msm_mctl_pp_frame_buffer {
	uint32_t buf_handle;
	int path;
};
struct msm_mctl_pp_divert_pp {
	int path;
	int enable;
};
struct msm_vpe_clock_rate {
	uint32_t rate;
};
struct msm_pp_crop {
	uint32_t  src_x;
	uint32_t  src_y;
	uint32_t  src_w;
	uint32_t  src_h;
	uint32_t  dst_x;
	uint32_t  dst_y;
	uint32_t  dst_w;
	uint32_t  dst_h;
	uint8_t update_flag;
};
#define MSM_MCTL_PP_VPE_FRAME_ACK    (1<<0)
#define MSM_MCTL_PP_VPE_FRAME_TO_APP (1<<1)

struct msm_mctl_pp_frame_cmd {
	uint32_t cookie;
	uint8_t  vpe_output_action;
	uint32_t src_buf_handle;
	uint32_t dest_buf_handle;
	struct msm_pp_crop crop;
	int path;
	/* TBD: 3D related */
};

#define VFE_OUTPUTS_MAIN_AND_PREVIEW    BIT(0)
#define VFE_OUTPUTS_MAIN_AND_VIDEO      BIT(1)
#define VFE_OUTPUTS_MAIN_AND_THUMB      BIT(2)
#define VFE_OUTPUTS_THUMB_AND_MAIN      BIT(3)
#define VFE_OUTPUTS_PREVIEW_AND_VIDEO   BIT(4)
#define VFE_OUTPUTS_VIDEO_AND_PREVIEW   BIT(5)
#define VFE_OUTPUTS_PREVIEW             BIT(6)
#define VFE_OUTPUTS_VIDEO               BIT(7)
#define VFE_OUTPUTS_RAW                 BIT(8)
#define VFE_OUTPUTS_JPEG_AND_THUMB      BIT(9)
#define VFE_OUTPUTS_THUMB_AND_JPEG      BIT(10)
#define VFE_OUTPUTS_RDI0                BIT(11)
#define VFE_OUTPUTS_RDI1                BIT(12)

struct msm_frame_info {
	uint32_t image_mode;
	uint32_t path;
};

#endif /*__MSM_ISP_H__*/

