/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_DP__H__
#define __MTK_DP__H__

#include "mtk_dp_common.h"


#define DPTX_CheckSinkCap_TimeOutCnt		0x3

#define HPD_INT_EVNET		BIT(3)
#define HPD_CONNECT		BIT(2)
#define HPD_DISCONNECT		BIT(1)
#define HPD_INITIAL_STATE	0

#define DPTX_TBC_SELBUF_CASE		2
#define DPTX_TBC_BUF_SIZE		DPTX_TBC_SELBUF_CASE
#if (DPTX_TBC_SELBUF_CASE == 2)
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x08
#elif (DPTX_TBC_SELBUF_CASE == 1)
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x10
#else
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x1F
#endif

#define ENABLE_DPTX_EF_MODE		0x1
#if (ENABLE_DPTX_EF_MODE == 0x01)
#define DPTX_AUX_SET_ENAHNCED_FRAME	0x80
#else
#define DPTX_AUX_SET_ENAHNCED_FRAME	0x00
#endif

#define DPTX_TRAIN_RETRY_LIMIT		0x8
#define DPTX_TRAIN_MAX_ITERATION	0x5

enum DPTx_SOURCE_TYPE {
	DPTX_SRC_DPINTF = 0,
	DPTX_SRC_PG	= 1,
};

enum DPTx_State {
	DPTXSTATE_INITIAL		= 0,
	DPTXSTATE_IDLE			= 1,
	DPTXSTATE_PREPARE		= 2,
	DPTXSTATE_NORMAL		= 3,
};

enum DPTx_DISP_State {
	DPTX_DISP_NONE		= 0,
	DPTX_DISP_RESUME	= 1,
	DPTX_DISP_SUSPEND	= 2,
};

enum DPTx_Return_Status {
	DPTX_NOERR			= 0,
	DPTX_PLUG_OUT			= 1,
	DPTX_TIMEOUT			= 2,
	DPTX_AUTH_FAIL			= 3,
	DPTX_EDID_FAIL			= 4,
	DPTX_TRANING_FAIL		= 5,
	DPTX_RETRANING			= 6,
};

enum DPTX_TRAINING_STATE {
	DPTX_NTSTATE_STARTUP		= 0x0,
	DPTX_NTSTATE_CHECKCAP		= 0x1,
	DPTX_NTSTATE_CHECKEDID		= 0x2,
	DPTX_NTSTATE_TRAINING_PRE	= 0x3,
	DPTX_NTSTATE_TRAINING		= 0x4,
	DPTX_NTSTATE_CHECKTIMING	= 0x5,
	DPTX_NTSTATE_NORMAL		= 0x6,
	DPTX_NTSTATE_POWERSAVE		= 0x7,
	DPTX_NTSTATE_DPIDLE		= 0x8,
	DPTX_NTSTATE_MAX,
};

enum DPTx_FEC_ERROR_COUNT_TYPE {
	FEC_ERROR_COUNT_DISABLE                = 0x0,
	FEC_UNCORRECTED_BLOCK_ERROR_COUNT      = 0x1,
	FEC_CORRECTED_BLOCK_ERROR_COUNT        = 0x2,
	FEC_BIT_ERROR_COUNT                    = 0x3,
	FEC_PARITY_BLOCK_ERROR_COUNT           = 0x4,
	FEC_PARITY_BIT_ERROR_COUNT             = 0x5,
};

struct DP_CTS_AUTO_REQ {
	unsigned int test_link_training;
	unsigned int test_pattern_req;
	unsigned int test_edid_read;
	unsigned int test_link_rate;	//06h:1.62Gbps, 0Ah:2.7Gbps
	unsigned int test_lane_count;
	//01h:color ramps,02h:black&white vertical,03h:color square
	unsigned int test_pattern;
	unsigned int test_h_total;
	unsigned int test_v_total;
	unsigned int test_h_start;
	unsigned int test_v_start;
	unsigned int test_hsync_width;
	unsigned int test_hsync_polarity;
	unsigned int test_vsync_width;
	unsigned int test_vsync_polarity;
	unsigned int test_h_width;
	unsigned int test_v_height;
	unsigned int test_sync_clk;
	unsigned int test_color_fmt;
	unsigned int test_dynamic_range;
	unsigned int test_YCbCr_coefficient;
	unsigned int test_bit_depth;
	unsigned int test_refresh_denominator;
	unsigned int test_interlaced;
	unsigned int test_refresh_rate_numerator;
	unsigned int test_aduio_channel_count;
	unsigned int test_aduio_samling_rate;
};

enum DPTx_PG_Sel {
	DPTx_PG_20bit	= 0,
	DPTx_PG_80bit	= 1,
	DPTx_PG_11bit	= 2,
	DPTx_PG_8bit	= 3,
	DPTx_PG_PRBS7	= 4,
};

enum DP_POWER_STATUS_TYPE {
	DP_POWER_STATUS_NONE = 0,
	DP_POWER_STATUS_AC_ON,
	DP_POWER_STATUS_DC_ON,
	DP_POWER_STATUS_PS_ON,
	DP_POWER_STATUS_DC_OFF,
	DP_POWER_STATUS_POWER_SAVING,
};

enum DP_LANECOUNT {
	DP_LANECOUNT_1 = 0x1,
	DP_LANECOUNT_2 = 0x2,
	DP_LANECOUNT_4 = 0x4,
};

enum DP_VERSION {
	DP_VERSION_11 = 0x11,
	DP_VERSION_12 = 0x12,
	DP_VERSION_14 = 0x14,
	DP_VERSION_12_14 = 0x16,
	DP_VERSION_14_14 = 0x17,
	DP_VERSION_MAX,
};

enum DP_LINKRATE {
	DP_LINKRATE_RBR = 0x6,
	DP_LINKRATE_HBR = 0xA,
	DP_LINKRATE_HBR2 = 0x14,
	DP_LINKRATE_HBR25 = 0x19,
	DP_LINKRATE_HBR3 = 0x1E,
};

enum DPTx_SWING_NUM {
	DPTx_SWING0	= 0x00,
	DPTx_SWING1	= 0x01,
	DPTx_SWING2	= 0x02,
	DPTx_SWING3	= 0x03,
};

enum DPTx_PREEMPHASIS_NUM {
	DPTx_PREEMPHASIS0	= 0x00,
	DPTx_PREEMPHASIS1	= 0x01,
	DPTx_PREEMPHASIS2	= 0x02,
	DPTx_PREEMPHASIS3	= 0x03,
};

//SINK_WIDTH_HIGHT_FPS
enum DP_VIDEO_TIMING_TYPE {
	SINK_640_480        = 0,
	SINK_800_600        = 1,
	SINK_1280_720       = 2,
	SINK_1280_960       = 3,
	SINK_1280_1024      = 4,
	SINK_1920_1080      = 5,
	SINK_1080_2460      = 6,
	SINK_1920_1200      = 7,
	SINK_1920_1440      = 8,
	SINK_2560_1600      = 9,
	SINK_3840_2160_30   = 10,//4K30
	SINK_3840_2160      = 11,//4k60
	SINK_7680_4320      = 12,
	SINK_MAX,
};

enum DP_VIDEO_MUTE {
	video_unmute	= 1,
	video_mute	= 2,
};

enum DPTX_VIDEO_MODE {
	DPTX_VIDEO_INTERLACE    = 0,
	DPTX_VIDEO_PROGRESSIVE  = 1,
};


#define MAX_LANECOUNT	DP_LANECOUNT_2

#define FAKE_DEFAULT_RES 0xFF

#define DP_VIDEO_TIMING_MASK 0x000000ff
#define DP_VIDEO_TIMING_SFT 0
#define DP_COLOR_DEPTH_MASK 0x0000ff00
#define DP_COLOR_DEPTH_SFT 8
#define DP_COLOR_FORMAT_MASK 0x00ff0000
#define DP_COLOR_FORMAT_SFT 16

#define DP_CHANNEL_2      BIT(0)
#define DP_CHANNEL_3      BIT(1)
#define DP_CHANNEL_4      BIT(2)
#define DP_CHANNEL_5      BIT(3)
#define DP_CHANNEL_6      BIT(4)
#define DP_CHANNEL_7      BIT(5)
#define DP_CHANNEL_8      BIT(6)

#define DP_SAMPLERATE_32  BIT(0)
#define DP_SAMPLERATE_44  BIT(1)
#define DP_SAMPLERATE_48  BIT(2)
#define DP_SAMPLERATE_96  BIT(3)
#define DP_SAMPLERATE_192 BIT(4)

#define DP_BITWIDTH_16    BIT(0)
#define DP_BITWIDTH_20    BIT(1)
#define DP_BITWIDTH_24    BIT(2)

#define DP_CAPABILITY_CHANNEL_MASK              0x7F
#define DP_CAPABILITY_CHANNEL_SFT               0
#define DP_CAPABILITY_SAMPLERATE_MASK           0x1F
#define DP_CAPABILITY_SAMPLERATE_SFT            8
#define DP_CAPABILITY_BITWIDTH_MASK             0x07
#define DP_CAPABILITY_BITWIDTH_SFT              16


void mtk_dp_poweroff(void);
void mtk_dp_poweron(void);
void mtk_dp_video_trigger(int res);
struct edid *mtk_dp_handle_edid(struct mtk_dp *mtk_dp);
int mdrv_DPTx_SetTrainingStart(struct mtk_dp *mtk_dp);
void mdrv_DPTx_CheckMaxLinkRate(struct mtk_dp *mtk_dp);
void mtk_dp_video_config(struct mtk_dp *mtk_dp);
void mtk_dp_force_res(unsigned int res, unsigned int bpc);
void mtk_dp_hotplug_uevent(unsigned int status);
void mtk_dp_enable_4k60(int enable);
void mdrv_DPTx_FEC_Ready(struct mtk_dp *mtk_dp, u8 err_cnt_sel);
void mdrv_DPTx_DSC_Support(struct mtk_dp *mtk_dp);
void mtk_dp_dsc_pps_send(u8 *PPS_128);
bool mdrv_DPTx_PHY_AutoTest(struct mtk_dp *mtk_dp, u8 ubDPCD_201);
void mdrv_DPTx_VideoMute(struct mtk_dp *mtk_dp, bool bENABLE);
void mdrv_DPTx_AudioMute(struct mtk_dp *mtk_dp, bool bENABLE);
void mdrv_DPTx_SPKG_SDP(struct mtk_dp *mtk_dp, bool bEnable, u8 ucSDPType,
	u8 *pHB, u8 *pDB);
void mdrv_DPTx_I2S_Audio_Config(struct mtk_dp *mtk_dp);
void mdrv_DPTx_I2S_Audio_Enable(struct mtk_dp *mtk_dp, bool bEnable);
void mdrv_DPTx_I2S_Audio_Ch_Status_Set(struct mtk_dp *mtk_dp, u8 ucChannel,
	u8 ucFs, u8 ucWordlength);
void mdrv_DPTx_I2S_Audio_SDP_Channel_Setting(struct mtk_dp *mtk_dp,
	u8 ucChannel, u8 ucFs, u8 ucWordlength);
int mdrv_DPTx_HPD_HandleInThread(struct mtk_dp *mtk_dp);
void mdrv_DPTx_Video_Enable(struct mtk_dp *mtk_dp, bool bEnable);
DWORD getTimeDiff(DWORD dwPreTime);
DWORD getSystemTime(void);
void mtk_dp_fake_plugin(unsigned int status, unsigned int bpc);
void mtk_dp_force_audio(unsigned int ch, unsigned int fs, unsigned int len);
void mtk_dp_test(unsigned int status);
void mtk_dp_fec_enable(unsigned int status);
void mtk_dp_power_save(unsigned int status);
void mtk_dp_hdcp_enable(bool enable);
void mtk_dp_force_hdcp1x(bool enable);
void mtk_dp_set_adjust_phy(uint8_t index, uint8_t c0, uint8_t cp1);
int mtk_dp_hdcp_getInfo(char *buffer, int size);
int mtk_dp_phy_getInfo(char *buffer, int size);
void mdrv_DPTx_reAuthentication(struct mtk_dp *mtk_dp);
void mdrv_DPTx_PatternSet(bool enable, int resolution);
void mdrv_DPTx_set_maxlinkrate(bool enable, int maxlinkrate);
extern void mhal_DPTx_VideoClock(bool enable, int resolution);

#endif //__MTK_DP__H__
