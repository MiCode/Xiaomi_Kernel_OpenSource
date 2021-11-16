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


#ifndef __DRTX_TYPE_H__
#define __DRTX_TYPE_H__
#include "mtk_drm_ddp_comp.h"
#include <drm/drm_device.h>
#include <drm/drm_dp_helper.h>
#include "drm/mediatek_drm.h"
#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include "mtk_dp_hdcp.h"
#include "mtk_dp_debug.h"


#ifndef BYTE
#define BYTE    unsigned char
#endif
#ifndef WORD
#define WORD    unsigned short
#endif
#ifndef DWORD
#define DWORD   unsigned long
#endif

#ifndef UINT32
#define UINT32  unsigned int
#endif

#ifndef UINT8
#define UINT8   unsigned char
#endif


#define EDID_SIZE 0x200
#define ENABLE_DPTX_SSC_FORCEON		0
#define ENABLE_DPTX_FIX_LRLC		0
#define ENABLE_DPTX_SSC_OUTPUT		1
#define ENABLE_DPTX_FIX_TPS2		0
#define AUX_WRITE_READ_WAIT_TIME        20 //us
#define DPTX_SUPPORT_DSC                1
#define DPTX_PHY_LEVEL_COUNT            10
#define DPTX_PHY_REG_COUNT              6

#define DPTX_AutoTest_ENABLE		0x1
#if DPTX_AutoTest_ENABLE
#define DPTX_TEST_LINK_TRAINING_EN	0x1
#define DPTX_TEST_PATTERN_EN		0x0
#define DPTX_TEST_EDID_READ_EN		0x0
#define DPTX_PHY_TEST_PATTERN_EN	0x1

#if DPTX_PHY_TEST_PATTERN_EN
#define DPTX_TEST_D10_2_EN		0x1
#define DPTX_TEST_SYMBERR_EN		0x0
#define DPTX_TEST_PRBS7_EN		0x1
#define DPTX_TEST_PHY80B_EN		0x1
#define DPTX_TEST_HBR2EYE_EN		0x1
#define DPTX_TEST_CP2520_P3_EN          0x1
#else
#define DPTX_TEST_D10_2_EN		0x0
#define DPTX_TEST_SYMBERR_EN		0x0
#define DPTX_TEST_PRBS7_EN		0x0
#define DPTX_TEST_PHY80B_EN		0x0
#define DPTX_TEST_HBR2EYE_EN		0x0
#define DPTX_TEST_CP2520_P3_EN          0x0
#endif

#define PATTERN_NONE			0x0
#define PATTERN_D10_2			0x1
#define PATTERN_SYMBOL_ERR		0x2
#define PATTERN_PRBS7			0x3
#define PATTERN_80B			0x4
#define PATTERN_HBR2_COM_EYE		0x5
#define CP2520_PATTERN2                 0x6
#define CP2520_PATTERN3                 0x7
#endif

enum DP_ATF_CMD {
	DP_ATF_DUMP = 0x20,
	DP_ATF_VIDEO_UNMUTE,
	DP_ATF_REG_WRITE,
	DP_ATF_REG_READ,
	DP_ATF_CMD_COUNT
};

union PPS_T {
	struct{
		BYTE major : 4;
		BYTE minor : 4;              //pps0
		BYTE pps_id : 8;              //pps1
		BYTE reserved1 : 8;          //pps2
		BYTE color_depth : 4;
		BYTE buffer_depth : 4;       //pps3
		BYTE reserved2 : 2;
		bool bp_enable : 1;
		bool convert_rgb : 1;
		bool simple_422 : 1;
		bool vbr_enable : 1;
		WORD bit_per_pixel : 10;     //pps4-5
		WORD pic_height : 16;        //pps6-7
		WORD pic_width : 16;         //pps8-9
		WORD slice_height : 16;      //pps10-11
		WORD slice_width : 16;       //pps12-13
		WORD chunk_size : 16;        //pps14-15
		BYTE reserved3 : 6;
		WORD init_xmit_delay : 10;   //pps16-17
		WORD init_dec_delay : 16;    //pps18-19
		WORD reserved4 : 10;
		BYTE init_scale_val : 6;     //pps20-21
		WORD scale_inc_interval : 16;//pps22-23
		BYTE reserved5 : 4;
		WORD scale_dec_interval : 12;//pps24-25
		WORD reserved6 : 11;
		BYTE first_line_offset : 5;  //pps26-27
		WORD nfl_bpg_offset : 16;    //pps28-29
		WORD slice_bpg_offset : 16;  //pps30-31
		WORD init_offset : 16;       //pps32-33
		WORD final_offset : 16;      //pps34-35
		BYTE reserved7 : 3;
		BYTE min_qp : 5;             //pps36
		BYTE reserved8 : 3;
		BYTE max_qp : 5;             //pps37
		BYTE rc_param_set[50];      //pps38-87

		BYTE reserved9 : 6;
		bool native_420 : 1;
		bool native_422 : 1;         //pps88
		BYTE reserved10 : 3;
		BYTE sec_line_bpg_offset : 5;//pps89
		WORD nsl_bpg_offset : 16;    //pps90-91
		WORD sec_line_offset : 16;   //pps92-93
		BYTE reserved11[34];        //pps94-127

	} dp_pps;

	BYTE ucPPS[128];
};

union MISC_T {
	struct {
		BYTE is_sync_clock : 1;
		BYTE color_format : 2;
		BYTE spec_def1 : 2;
		BYTE color_depth : 3;

		BYTE interlaced : 1;
		BYTE stereo_attr : 2;
		BYTE reserved : 3;
		BYTE is_vsc_sdp : 1;
		BYTE spec_def2 : 1;

	} dp_misc;
	BYTE ucMISC[2];
};

struct DPTX_TIMING_PARAMETER {
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
	int Video_ip_mode;
};

struct DPTX_TRAINING_INFO {
	bool bSinkEXTCAP_En : 1;
	bool bTPS3 : 1;
	bool bTPS4 : 1;
	bool bSinkSSC_En : 1;
	bool bDPTxAutoTest_EN : 1;
	bool bCablePlugIn : 1;
	bool bCableStateChange : 1;
	bool bDPMstCAP : 1;
	bool bDPMstBranch : 1;
	bool bDWN_STRM_PORT_PRESENT : 1;
	bool cr_done : 1;
	bool eq_done : 1;
	bool set_max_linkrate;

	BYTE ubDPSysVersion;
	BYTE ubSysMaxLinkRate;
	BYTE ubLinkRate;
	BYTE ubLinkLaneCount;
	WORD usPHY_STS;
	BYTE ubDPCD_REV;
	BYTE ubSinkCountNum;
	BYTE ucCheckCapTimes;
};

struct DPTX_INFO {
	uint8_t input_src;
	uint8_t depth;
	uint8_t format;
	uint8_t resolution;
	unsigned int audio_caps;
	unsigned int audio_config;
	struct DPTX_TIMING_PARAMETER DPTX_OUTBL;

	bool bPatternGen : 1;
	bool bSinkSSC_En : 1;
	bool bSetAudioMute : 1;
	bool bSetVideoMute : 1;
	bool bAudioMute : 1;
	bool bVideoMute : 1;
	bool bForceHDCP1x : 1;

#ifdef DPTX_HDCP_ENABLE
	BYTE bAuthStatus;
	struct HDCP1X_INFO hdcp1x_info;
	struct HDCP2_INFO hdcp2_info;
#endif

};

struct DPTX_PHY_PARAMETER {
	unsigned char C0;
	unsigned char CP1;
};

struct mtk_dp {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector conn;
	struct drm_encoder enc;
	int id;
	struct edid *edid;
	struct drm_dp_aux aux;
	u8 rx_cap[16];
	struct drm_display_mode mode;
	struct DPTX_INFO info;
	int state;
	int state_pre;
	struct DPTX_TRAINING_INFO training_info;
	int training_state;
	int training_state_pre;
	wait_queue_head_t control_wq;
	struct task_struct *control_task;

	struct workqueue_struct *dptx_wq;
	struct work_struct hdcp_work;
	struct work_struct dptx_work;

	u32 min_clock;
	u32 max_clock;
	u32 max_hdisplay;
	u32 max_vdisplay;

	void __iomem *regs;
	struct clk *dp_tx_clk;

	bool bUeventToHwc;
	int disp_status;  //for DDP
	bool bPowerOn;
	bool audio_enable;
	bool video_enable;
	bool dp_ready;
	bool has_dsc;
	bool has_fec;
	bool dsc_enable;
	struct mtk_drm_private *priv;
	//phy_params[10] = {L0P0,L0P1,L0P2,L0P3,L1P0,L1P1,L1P2,L2P0,L2P1,L3P0};
	struct DPTX_PHY_PARAMETER phy_params[DPTX_PHY_LEVEL_COUNT];
};

#endif /*__DRTX_TYPE_H__*/

