/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_DEF_H__
#define __MTK_CAM_SENINF_DEF_H__


#define SENINF_VC_MAXCNT 8
#define SENINF_DEF_PIXEL_MODE 3
#define SENINF_CLK_MARGIN_IN_PERCENT 0

#define SENINF_TIMESTAMP_CLK 1000
#define HW_BUF_EFFECT 10
#define ISP_CLK_LOW 273000000


#define CSI_CLK_242MHZ

/* data lane hs settle, base on 130 MHz csi ck */
#ifdef CSI_CLK_130MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x06
#define SENINF_DPHY_SETTLE_DELAY_DT 0x0A
#define SENINF_SETTLE_DELAY_CK 0x9
#define SENINF_HS_TRAIL_PARAMETER 0x34
#endif

/* data lane hs settle, base on 242 MHz csi ck */
#ifdef CSI_CLK_242MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x11
#define SENINF_DPHY_SETTLE_DELAY_DT 0x15
#define SENINF_SETTLE_DELAY_CK 0x9
#define SENINF_HS_TRAIL_PARAMETER 0x34
#endif

/* data lane hs settle, base on 208 MHz csi ck */
#ifdef CSI_CLK_208MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x15
#define SENINF_DPHY_SETTLE_DELAY_DT 0x15
#define SENINF_SETTLE_DELAY_CK 0x9
#define SENINF_HS_TRAIL_PARAMETER 0x8
#endif
/* data lane hs settle, base on 208 MHz csi ck */
#ifdef CSI_CLK_273MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x10
#define SENINF_DPHY_SETTLE_DELAY_DT 0x10
#define SENINF_SETTLE_DELAY_CK 0x11
#define SENINF_HS_TRAIL_PARAMETER 0x34
#endif

#ifdef CSI_CLK_499MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x20
#define SENINF_DPHY_SETTLE_DELAY_DT 0x2f
#define SENINF_SETTLE_DELAY_CK 0x2f
#define SENINF_HS_TRAIL_PARAMETER 0x34
#endif

/* data lane hs settle, base on 312 MHz csi ck */
#ifdef CSI_CLK_312MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x20
#define SENINF_DPHY_SETTLE_DELAY_DT 0x20
#define SENINF_SETTLE_DELAY_CK 0xD
#define SENINF_HS_TRAIL_PARAMETER 0xa
#endif

/* data lane hs settle, base on 356 MHz csi ck */
#ifdef CSI_CLK_356MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x1c
#define SENINF_DPHY_SETTLE_DELAY_DT 0x27
#define SENINF_SETTLE_DELAY_CK 0x13
#define SENINF_HS_TRAIL_PARAMETER 0x25
#endif

/* clock lane hs settle, base on 393 MHz csi ck */
#ifdef CSI_CLK_393MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x28
#define SENINF_DPHY_SETTLE_DELAY_DT 0x28
#define SENINF_SETTLE_DELAY_CK 0x10
#define SENINF_HS_TRAIL_PARAMETER 0x30
#endif

/* clock lane hs settle, base on 416 MHz csi ck */
#ifdef CSI_CLK_416MHZ
#define SENINF_CPHY_SETTLE_DELAY_DT 0x27
#define SENINF_DPHY_SETTLE_DELAY_DT 0x27
#define SENINF_SETTLE_DELAY_CK 0x13
#define SENINF_HS_TRAIL_PARAMETER 0x30
#endif

/* under 14.5 Gbps, trail should be enable */
#define SENINF_HS_TRAIL_EN_CONDITION 1450000000

#define SENSOR_CLOCK_POLARITY_HIGH	0
#define SENSOR_CLOCK_POLARITY_LOW	1
#define NUM_PORTS			2

#define SENINF_DEBUG

#ifdef SENINF_DEBUG
#define SENINF_DEBUG_FL_IGNORE_CAM_LINK 1
#define SENINF_DEBUG_FL_ALLOC_CAM_MUX 2
#endif

//	FIX ME: #define it to get aov parameters
#define AOV_GET_PARAM 1
//	FIX ME: #define it to switch i2c bus aux function on/off when gpio pin ready
#define SENSING_MODE_READY

/* FIX ME: #define it to enable pm/clk use on aov suspend/resume
 * #undef it to all on clk
 */
#define AOV_SUSPEND_RESUME_USE_PM_CLK

enum CSI_PORT {
	CSI_PORT_0 = 0,
	CSI_PORT_1,
	CSI_PORT_2,
	CSI_PORT_3,
	CSI_PORT_4,
	CSI_PORT_5,
	CSI_PORT_0A,
	CSI_PORT_0B,
	CSI_PORT_1A,
	CSI_PORT_1B,
	CSI_PORT_2A,
	CSI_PORT_2B,
	CSI_PORT_3A,
	CSI_PORT_3B,
	CSI_PORT_4A,
	CSI_PORT_4B,
	CSI_PORT_5A,
	CSI_PORT_5B,
	CSI_PORT_MAX_NUM,
};

#define SENINF_CSI_PORT_NAMES \
	"0", \
	"1", \
	"2", \
	"3", \
	"4", \
	"5", \
	"0A", \
	"0B", \
	"1A", \
	"1B", \
	"2A", \
	"2B", \
	"3A", \
	"3B", \
	"4A", \
	"4B", \
	"5A", \
	"5B", \

enum SENINF_PHY_VER_ENUM {
	SENINF_PHY_3_0,
	SENINF_PHY_2_0,
	SENINF_PHY_VER_NUM,
};

#define MTK_CSI_PHY_VERSIONS \
"mtk_csi_phy_3_0", \
"mtk_csi_phy_2_0", \

enum SENINF_ENUM {
	SENINF_1,
	SENINF_2,
	SENINF_3,
	SENINF_4,
	SENINF_5,
	SENINF_6,
	SENINF_7,
	SENINF_8,
	SENINF_9,
	SENINF_10,
	SENINF_11,
	SENINF_12,
	SENINF_NUM,
};

enum SENINF_MUX_ENUM {
	SENINF_MUX1,
	SENINF_MUX2,
	SENINF_MUX3,
	SENINF_MUX4,
	SENINF_MUX5,
	SENINF_MUX6,
	SENINF_MUX7,
	SENINF_MUX8,
	SENINF_MUX9,
	SENINF_MUX10,
	SENINF_MUX11,
	SENINF_MUX12,
	SENINF_MUX13,
	SENINF_MUX14,
	SENINF_MUX15,
	SENINF_MUX16,
	SENINF_MUX17,
	SENINF_MUX18,
	SENINF_MUX19,
	SENINF_MUX20,
	SENINF_MUX21,
	SENINF_MUX22,
	SENINF_MUX_NUM,

	SENINF_MUX_ERROR = -1,
};

enum SENINF_CAM_MUX_ENUM {
	SENINF_CAM_MUX0,
	SENINF_CAM_MUX1,
	SENINF_CAM_MUX2,
	SENINF_CAM_MUX3,
	SENINF_CAM_MUX4,
	SENINF_CAM_MUX5,
	SENINF_CAM_MUX6,
	SENINF_CAM_MUX7,
	SENINF_CAM_MUX8,
	SENINF_CAM_MUX9,
	SENINF_CAM_MUX10,
	SENINF_CAM_MUX11,
	SENINF_CAM_MUX12,
	SENINF_CAM_MUX13,
	SENINF_CAM_MUX14,
	SENINF_CAM_MUX15,
	SENINF_CAM_MUX16,
	SENINF_CAM_MUX17,
	SENINF_CAM_MUX18,
	SENINF_CAM_MUX19,
	SENINF_CAM_MUX20,
	SENINF_CAM_MUX21,
	SENINF_CAM_MUX22,
	SENINF_CAM_MUX23,
	SENINF_CAM_MUX24,
	SENINF_CAM_MUX25,
	SENINF_CAM_MUX26,
	SENINF_CAM_MUX27,
	SENINF_CAM_MUX28,
	SENINF_CAM_MUX29,
	SENINF_CAM_MUX30,
	SENINF_CAM_MUX31,
	SENINF_CAM_MUX32,
	SENINF_CAM_MUX33,
	SENINF_CAM_MUX34,
	SENINF_CAM_MUX35,
	SENINF_CAM_MUX36,
	SENINF_CAM_MUX37,
	SENINF_CAM_MUX38,
	SENINF_CAM_MUX39,
	SENINF_CAM_MUX40,
	SENINF_CAM_MUX_NUM,

	SENINF_CAM_MUX_ERR = 0xff
};

enum CAM_TYPE_ENUM {
	TYPE_CAMSV_SAT,
	TYPE_CAMSV_NORMAL,
	TYPE_RAW,
	TYPE_UISP,
	TYPE_PDP,
	TYPE_MAX_NUM,
};

#define MUX_RANGE_NAMES \
	"mux-camsv-sat_range", \
	"mux-camsv-normal_range", \
	"mux-raw_range", \
	"mux-uisp_range", \
	"mux-pdp_range", \

#define CAMMUX_RANGE_NAMES \
	"cammux-camsv-sat_range", \
	"cammux-camsv-normal_range", \
	"cammux-raw_range", \
	"cammux-uisp_range", \
	"cammux-pdp_range", \

#define VC_STREAM_MAX_NUM 8

enum VC_CH_GROUP {
	VC_CH_GROUP_0,
	VC_CH_GROUP_1,
	VC_CH_GROUP_2,
	VC_CH_GROUP_3,

	VC_CH_GROUP_ALL = VC_CH_GROUP_0,
	VC_CH_GROUP_RAW1 = VC_CH_GROUP_1,
	VC_CH_GROUP_RAW2 = VC_CH_GROUP_2,
	VC_CH_GROUP_RAW3 = VC_CH_GROUP_3,

	VC_CH_GROUP_MAX_NUM,
};

enum SENINF_SOURCE_ENUM { //0:CSI2(2.5G), 3: parallel, 8:NCSI2(1.5G)
	CSI2 = 0x0, /* 2.5G support */
	TEST_MODEL = 0x1,
	CCIR656	= 0x2,
	PARALLEL_SENSOR = 0x3,
	SERIAL_SENSOR = 0x4,
	HD_TV = 0x5,
	EXT_CSI2_OUT1 = 0x6,
	EXT_CSI2_OUT2 = 0x7,
	MIPI_SENSOR = 0x8,/* 1.5G support */
	VIRTUAL_CHANNEL_1 = 0x9,
	VIRTUAL_CHANNEL_2 = 0xA,
	VIRTUAL_CHANNEL_3 = 0xB,
	VIRTUAL_CHANNEL_4 = 0xC,
	VIRTUAL_CHANNEL_5 = 0xD,
	VIRTUAL_CHANNEL_6 = 0xE,
};

enum SENINF_CSI2_ENUM {
	CSI2_1_5G = 0x0, /* 1.5G support */
	CSI2_2_5G = 0x1, /* 2.5G support */
	CSI2_2_5G_CPHY = 0x2, /* 2.5G support */
};

enum TG_FORMAT_ENUM {
	RAW_8BIT_FMT = 0x0,
	RAW_10BIT_FMT = 0x1,
	RAW_12BIT_FMT = 0x2,
	YUV422_FMT = 0x3,
	RAW_14BIT_FMT = 0x4,
	RGB565_MIPI_FMT	= 0x5,
	RGB888_MIPI_FMT	= 0x6,
	JPEG_FMT = 0x7
};

enum {
	/* cam cg */
	CLK_CAM_SENINF = 0,
	/* csi clk */
	CLK_TOP_SENINF,
	CLK_TOP_SENINF1,
	CLK_TOP_SENINF2,
	CLK_TOP_SENINF3,
	CLK_TOP_SENINF4,
	CLK_TOP_SENINF5,
	CLK_TOP_SENINF_END,
	/* test model clk */
	CLK_TOP_CAMTM = CLK_TOP_SENINF_END,
	CLK_TOP_OSC_D4,					// aov 130M seninf csi clk on scp
	CLK_TOP_MAINPLL2_D9,			// default 242M seninf csi clk on apmcu
	CLK_TOP_CAMTM_END,
	/* analog phyD clk */
	CLK_TOP_CAMTG = CLK_TOP_CAMTM_END,
	CLK_TOP_OSC_D20,				// aov low power 26M reference clk on scp
	CLK_TOP_TCK_26M_MX9,			// default 26M analog reference clk on apmcu
	CLK_MAXCNT,
};

#define SENINF_CLK_NAMES \
	"clk_cam_seninf", \
	"clk_top_seninf", \
	"clk_top_seninf1", \
	"clk_top_seninf2", \
	"clk_top_seninf3", \
	"clk_top_seninf4", \
	"clk_top_seninf5", \
	"clk_top_camtm", \
	"clk_top_osc_d4", \
	"clk_top_mainpll2_d9", \
	"clk_top_camtg", \
	"clk_top_osc_d20", \
	"clk_top_tck_26m_mx9", \

#endif
