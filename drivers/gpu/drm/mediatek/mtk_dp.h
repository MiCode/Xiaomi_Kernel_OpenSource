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


#define DPTX_CheckSinkCap_TimeOutCnt		0x30

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

enum DPTx_InPutSrcType {
	DPTXInputSrc_PG	= 0x00,
	DPTXInputSrc_DPINTF = 0x01,
};

enum DPTx_State {
	DPTXSTATE_INITIAL		= 0,
	DPTXSTATE_IDLE			= 1,
	DPTXSTATE_HDCP_AUTH		= 2,
	DPTXSTATE_PREPARE		= 3,
	DPTXSTATE_NORMAL		= 4,
};

enum DPTx_Return_Status {
	DPTX_NOERR			= 0,
	DPTX_PLUG_OUT			= 1,
	DPTX_TIMEOUT			= 2,
	DPTX_AUTH_FAIL			= 3,
	DPTX_EDID_FAIL			= 4,
	DPTX_TRANING_FAIL		= 5,
	DPTX_TRANING_STATE_CHANGE	= 6,
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

struct DPTX_DRV_TIMING_PARAMETER {
	int Video_ip_mode;
	WORD Htt;
	WORD Hde;
	WORD Hbk;
	WORD Hfp;
	WORD Hsw;
	bool bHsp;
	WORD Hbp;
	WORD Vtt;
	WORD Vde;
	WORD Vbk;
	WORD Vfp;
	WORD Vsw;
	bool bVsp;
	WORD Vbp;
	BYTE FrameRate;
	DWORD PixRateKhz;
	BYTE MISC[2];
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

enum DP_VIDEO_TIMING_TYPE {
	SINK_480P           = 0,
	SINK_720P60         = 1,
	SINK_1080P60        = 2,
	SINK_480P_1440      = 3,
	SINK_480P_2880      = 4,
	SINK_1080P30        = 5,
	SINK_576P           = 6,
	SINK_720P50         = 7,
	SINK_1080P50        = 8,
	SINK_576P_1440      = 9,
	SINK_576P_2880      = 10,
	SINK_1080P25        = 11,
	SINK_1080P24        = 12,
	SINK_2K2K60         = 13,
	SINK_4K2K30         = 14,
	SINK_4K2K60R        = 15,
	SINK_8K4K60R        = 16,
	SINK_1080P60_2460   = 17,
	SINK_1200P60_1920   = 18,
	SINK_UNKNOWN        = 19,
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

#define DPTx_PATTERN_RGB_640_480_EN		0x1
#define DPTx_PATTERN_RGB_720_480_EN		0x1
#define DPTx_PATTERN_RGB_800_600_EN		0x1
#define DPTx_PATTERN_RGB_1280_720_EN		0x1
#define DPTx_PATTERN_RGB_1280_1024_EN		0x1
#define DPTx_PATTERN_RGB_1920_1080_EN		0x1
#define DPTx_PATTERN_RGB_3840_2160_EN		0x1
#define DPTx_PATTERN_RGB_4096_2160_EN		0x1
#define DPTx_PATTERN_RGB_7680_4320_EN		0x1
#define DPTx_PATTERN_RGB_848_480_EN		0x1
#define DPTx_PATTERN_RGB_1280_960_EN		0x1
#define DPTx_PATTERN_RGB_1920_1440_EN		0x1
#define DPTx_PATTERN_RGB_1280_800_EN		0x1
#define DPTx_PATTERN_RGB_DEFINERES_EN		0x1

enum DPTx_PATTERN_NUM {
#if DPTx_PATTERN_RGB_640_480_EN
	DPTx_PATTERN_RGB_640_480 = 0x00,
#endif
#if DPTx_PATTERN_RGB_720_480_EN
	DPTx_PATTERN_RGB_720_480 = 0x01,
#endif
#if DPTx_PATTERN_RGB_800_600_EN
	DPTx_PATTERN_RGB_800_600 = 0x02,
#endif
#if DPTx_PATTERN_RGB_1280_720_EN
	DPTx_PATTERN_RGB_1280_720 = 0x03,
#endif
#if DPTx_PATTERN_RGB_1280_1024_EN
	DPTx_PATTERN_RGB_1280_1024 = 0x04,
#endif
#if DPTx_PATTERN_RGB_1920_1080_EN
	DPTx_PATTERN_RGB_1920_1080 = 0x05,
#endif
#if DPTx_PATTERN_RGB_3840_2160_EN
	DPTx_PATTERN_RGB_3840_2160 = 0x06,
#endif
#if DPTx_PATTERN_RGB_4096_2160_EN
	DPTx_PATTERN_RGB_4096_2160 = 0x07,
#endif
#if DPTx_PATTERN_RGB_7680_4320_EN
	DPTx_PATTERN_RGB_7680_4320 = 0x08,
#endif
#if DPTx_PATTERN_RGB_848_480_EN
	DPTx_PATTERN_RGB_848_480 = 0x09,
#endif
#if DPTx_PATTERN_RGB_1280_960_EN
	DPTx_PATTERN_RGB_1280_960 = 0x0A,
#endif
#if DPTx_PATTERN_RGB_1920_1440_EN
	DPTx_PATTERN_RGB_1920_1440 = 0x0B,
#endif
#if DPTx_PATTERN_RGB_1280_800_EN
	DPTx_PATTERN_RGB_1280_800 = 0x0C,
#endif
#if DPTx_PATTERN_RGB_DEFINERES_EN
	DPTx_PATTERN_RGB_DEFINERES = 0x0D,
#endif

	DPTx_PATTERN_RGB_MAX,
	DPTx_PATTERN_YCbCr422_MAX,
	DPTx_PATTERN_YCbCr444_MAX,
	DPTx_PATTERN_MAX,
};

#if DPTx_PATTERN_RGB_DEFINERES_EN
#define DPTX480P		0
#define DPTX720P		1
#define DPTX1080P		2
#define DPTX2K2K		3
#define DPTX2K2KR		4
#define DPTX4K2KR		5
#define DPTX8K4KR		6
#define DPTX1080i		7
#define DPTXPanelDefine	8
#define DPTX_Timing		DPTX1080P
#if (DPTX_Timing == DPTX8K4KR)
#define DPTXMSA_Htotal		8040
#define DPTXMSA_HStart		336		// HStart  == HSW+HBACKPORCH
#define DPTXMSA_HSP		1		// HSP   [15]=Polarity
#define DPTXMSA_HSW		96		//19 //38//22//24
#define DPTXMSA_Vtotal		4381	//2185 //2180 //2200 //2250
#define DPTXMSA_VStart		14		// Vstart  8 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		8		// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		7680
#define DPTXMSA_Vheight		4320
#elif (DPTX_Timing == DPTX4K2KR)
#define DPTXMSA_Htotal		4400	//(2048*2)//(2080*2) //(2200*2) //4400
#define DPTXMSA_HStart		(28+40)	// HStart  == HSW+HBACKPORCH
#define DPTXMSA_HSP		1		// HSP   [15]=Polarity
#define DPTXMSA_HSW		28		//19 //38//22//24
#define DPTXMSA_Vtotal		2250	//2185 //2180 //2200 //2250
#define DPTXMSA_VStart		(3+12)	// Vstart  8 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0003	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		(1920*2)	//3840
#define DPTXMSA_Vheight		2160
#elif (DPTX_Timing == DPTX2K2K)
#define DPTXMSA_Htotal		0x0820	// Htotal 2080
#define DPTXMSA_HStart		0x0058	// HStart 88 == HSW+HBACKPORCH
#define DPTXMSA_HSP		1		// HSP   [15]=Polarity
#define DPTXMSA_HSW		0x002C	// Hsw 44  [15]=Polarity
#define DPTXMSA_Vtotal		0x08CA	// Vtotal  2250
#define DPTXMSA_VStart		0x0008	// Vstart  8 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0005	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		0x0780	// Hactive 1920
#define DPTXMSA_Vheight		0x0870	// Vactive 2160
#elif (DPTX_Timing == DPTX1080i) // i-mode => V 4 parameters divide by 2
#define DPTXMSA_Htotal		0x0898	// Htotal 2200
#define DPTXMSA_HStart		0x0058	// HStart 88 == HSW+HBACKPORCH
#define DPTXMSA_HSP		1	// HSP   [15]=Polarity
#define DPTXMSA_HSW		0x002C	// Hsw 44  [15]=Polarity
#define DPTXMSA_Vtotal		0x0232	// Vtotal  1125
#define DPTXMSA_VStart		0x0008	// Vstart  16 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0002	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		0x0780	// Hactive 1920
#define DPTXMSA_Vheight		0x021C	// Vactive 1080
#elif (DPTX_Timing == DPTX1080P)
#define DPTXMSA_Htotal		0x0898	// Htotal 2200
#define DPTXMSA_HStart		0x0058	// HStart 88 == HSW+HBACKPORCH
#define DPTXMSA_HSP		1	// HSP   [15]=Polarity
#define DPTXMSA_HSW		0x002C	// Hsw 44  [15]=Polarity
#define DPTXMSA_Vtotal		0x0465	// Vtotal  1125
#define DPTXMSA_VStart		0x0010	// Vstart  16 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0005	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		0x0780	// Hactive 1920
#define DPTXMSA_Vheight		0x0438	// Vactive 1080
#elif (DPTX_Timing == DPTX720P)
#define DPTXMSA_Htotal		0x0672	// Htotal 1650
#define DPTXMSA_HStart		0x006E	// HStart 110 == HSW+HBACKPORCH
#define DPTXMSA_HSP		1	// HSP   [15]=Polarity
#define DPTXMSA_HSW		0x0028	// Hsw 40  [15]=Polarity
#define DPTXMSA_Vtotal		0x02EE	// Vtotal  750
#define DPTXMSA_VStart		0x0008	// Vstart  8 == VSW+VBACKPORCH
#define DPTXMSA_VSP		0	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0005	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		0x0500	// Hactive 1280
#define DPTXMSA_Vheight		0x02D0	// Vactive  720
#elif (DPTX_Timing == DPTX480P)
#define DPTXMSA_Htotal		0x035A	// Htotal 858
#define DPTXMSA_HStart		0x007A	// HStart 122 == HSW+HBACKPORCH
#define DPTXMSA_HSP		1	// HSP   [15]=Polarity
#define DPTXMSA_HSW		0x003E	// Hsw 62  [15]=Polarity
#define DPTXMSA_Vtotal		0x020D	// Vtotal  525
#define DPTXMSA_VStart		0x0024	// Vstart  36 == VSW+VBACKPORCH
#define DPTXMSA_VSP		1	// VSP   [15]=Polarity
#define DPTXMSA_VSW		0x0006	// Vsw   [15]=Polarity
#define DPTXMSA_Hwidth		0x02D0	// Hactive 720
#define DPTXMSA_Vheight		0x01E0	// Vactive  480
#endif
#define DPTXMSA_HB	((DPTXMSA_Htotal) - (DPTXMSA_Hwidth))
#define DPTXMSA_VB	((DPTXMSA_Vtotal) - (DPTXMSA_Vheight))
#define DPTXMSA_HFP	((DPTXMSA_HB) - (DPTXMSA_HStart))
#define DPTXMSA_VFP	((DPTXMSA_VB) - (DPTXMSA_VStart))
#define DPTXMSA_HBP	((DPTXMSA_HStart) - (DPTXMSA_HSW))
#define DPTXMSA_VBP	((DPTXMSA_VStart) - (DPTXMSA_VSW))
#endif

void mtk_dp_poweroff(void);
void mtk_dp_poweron(void);
void mtk_dp_video_trigger(int res);
struct edid *mtk_dp_handle_edid(struct mtk_dp *mtk_dp);
int mdrv_DPTx_SetTrainingStart(struct mtk_dp *mtk_dp);
void mdrv_DPTx_CheckMaxLinkRate(struct mtk_dp *mtk_dp);
void mdrv_DPTx_SetPatternGenMode(struct mtk_dp *mtk_dp, u8 ucDPTx_PATTERN_NUM);
void mtk_dp_video_config(struct mtk_dp *mtk_dp, unsigned int config);
void mtk_dp_force_res(unsigned int res, unsigned int bpc);
void mtk_dp_hotplug_uevent(unsigned int status);
void mtk_dp_enable_4k60(int enable);
void mdrv_DPTx_FEC_Ready(struct mtk_dp *mtk_dp, u8 err_cnt_sel);
void mdrv_DPTx_DSC_Support(struct mtk_dp *mtk_dp);
void mtk_dp_dsc_pps_send(u8 *PPS_128);
bool mdrv_DPTx_PHY_AutoTest(struct mtk_dp *mtk_dp, u8 ubDPCD_201);
void mdrv_DPTx_VideoMute(struct mtk_dp *mtk_dp, bool bENABLE);
void mdrv_DPTx_OutPutMute(struct mtk_dp *mtk_dp, bool bEnable);
void mdrv_DPTx_SPKG_SDP(struct mtk_dp *mtk_dp, bool bEnable, u8 ucSDPType,
	u8 *pHB, u8 *pDB);
void mdrv_DPTx_I2S_Audio_Enable(struct mtk_dp *mtk_dp, bool bEnable);
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
void mtk_dp_set_adjust_phy(uint8_t c0, uint8_t cp1);
int mtk_dp_hdcp_getInfo(char *buffer, int size);
int mdrv_DPTx_set_reTraining(struct mtk_dp *mtk_dp);

#endif //__MTK_DP__H__
