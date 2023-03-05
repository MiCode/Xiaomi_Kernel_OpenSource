/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef PANEL_BOE_NT37705_DPHY_CMD_FHD_H
#define PANEL_BOE_NT37705_DPHY_CMD_FHD_H

#include <linux/trace_events.h>
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_2k.h"
#endif

#define REGFLAG_CMD				0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD

#define MODE_NUM	2

#define mtk_drm_trace_begin(fmt, args...) do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), \
			"B|%d|"fmt"\n", current->tgid, ##args); \
		preempt_enable();\
	} \
} while (0)

#define mtk_drm_trace_end() do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), "E\n"); \
		preempt_enable(); \
	} \
} while (0)

#define mtk_drm_trace_c(fmt, args...) do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), \
			"C|"fmt"\n", ##args); \
		preempt_enable();\
	} \
} while (0)

//#ifdef OPLUS_ADFR
enum MODE_ID {
	FHD_SDC60 = 0,
	FHD_SDC120 = 1,
};
//#endif
#define FRAME_WIDTH				(1080)
#define FRAME_HEIGHT			(2520)
#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SCR_VER                 44
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            40
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               549
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      950
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         13
#define DSC_NFL_BPG_OFFSET          683
#define DSC_SLICE_BPG_OFFSET        651
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

extern bool g_trace_log;
extern unsigned long mtk_drm_get_tracing_mark(void);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned int count;
	unsigned char para_list[256];
};

/* ------------------------- timming parameters ------------------------- */

/* --------------- DSC Setting --------------- */
/* FHD DSC1.2 10bit */
struct LCM_setting_table fhd_dsc_cmd[] = {

};

/* WQHD DSC1.2 10bit */
struct LCM_setting_table wqhd_dsc_cmd[] = {

};

/* --------------- timing@fhd_sdc_60 --------------- */
/* timing-switch cmd */
struct LCM_setting_table fhd_timing_switch_cmd_sdc60[] = {
	{REGFLAG_CMD, 2, {0x2F, 0x30}},
	{REGFLAG_CMD, 2, {0X6D, 0x02}},
	{REGFLAG_CMD, 2, {0X5A, 0x01}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x01}},
};
struct LCM_setting_table fhd_timing_switch_cmd_sdc90[] = {
	{REGFLAG_CMD, 2, {0x2F, 0x01}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x00}},
};
struct LCM_setting_table fhd_timing_switch_cmd_sdc120[] = {
	{REGFLAG_CMD, 2, {0x2F, 0x00}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x01}},
};

/* dsi-on cmd */
struct LCM_setting_table fhd_dsi_on_cmd_sdc60[] = {
	/*IP Setting*/
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x80}},
	{REGFLAG_CMD, 2, {0x6F, 0x19}},
	{REGFLAG_CMD, 2, {0xF2, 0x00}},
	{REGFLAG_CMD, 2, {0x6F, 0x1A}},
	{REGFLAG_CMD, 2, {0XF4, 0x55}},

	{REGFLAG_CMD, 2, {0x6F, 0x11}},
	{REGFLAG_CMD, 3, {0xF8, 0x01, 0x4E}},
	{REGFLAG_CMD, 2, {0x6F, 0x2D}},
	{REGFLAG_CMD, 3, {0XF8, 0x00, 0xFD}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x81}},
	{REGFLAG_CMD, 2, {0x6F, 0x05}},
	{REGFLAG_CMD, 2, {0xFE, 0x3C}},
	{REGFLAG_CMD, 2, {0x6F, 0x02}},
	{REGFLAG_CMD, 2, {0XF9, 0x04}},
	{REGFLAG_CMD, 2, {0x6F, 0x1E}},
	{REGFLAG_CMD, 2, {0XFB, 0x0F}},
	{REGFLAG_CMD, 2, {0x6F, 0x0F}},
	{REGFLAG_CMD, 2, {0XF5, 0x20}},
	{REGFLAG_CMD, 2, {0x6F, 0x0D}},
	{REGFLAG_CMD, 2, {0XFB, 0x80}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x82}},
	{REGFLAG_CMD, 2, {0x6F, 0x09}},
	{REGFLAG_CMD, 2, {0XF2, 0x55}},
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x83}},
	{REGFLAG_CMD, 2, {0x6F, 0x12}},
	{REGFLAG_CMD, 2, {0XFE, 0x41}},
	{REGFLAG_CMD, 2, {0x6F, 0x13}},
	{REGFLAG_CMD, 2, {0XFD, 0x21}},
	{REGFLAG_CMD, 2, {0X26, 0x00}},
	{REGFLAG_CMD, 3, {0X81, 0x01, 0x00}},
	/*TE(Vsync) ON/OFF*/
	{REGFLAG_CMD, 2, {0X35, 0x00}},
	/*CASET/PASET Setting*/
	{REGFLAG_CMD, 2, {0x53, 0x20}},
	{REGFLAG_CMD, 5, {0x2A, 0x00, 0x00, 0x04, 0xD7}},
	{REGFLAG_CMD, 5, {0x2B, 0x00, 0x00, 0x09, 0xD3}},

	/*DSC setting 8bit-3x*/
	{REGFLAG_CMD, 2, {0x03, 0x01}},
	{REGFLAG_CMD, 3, {0x90, 0x03, 0x03}},
	{REGFLAG_CMD, 19, {0x91, 0x89, 0x28, 0x00, 0x28, 0xD2, 0x00,
			0x02, 0x25, 0x03, 0xB6, 0x00, 0x07, 0x02, 0xAB, 0x02,
			0x8B, 0x10, 0xF0}},
	/*120Hz>60Hz*/
	{REGFLAG_CMD, 2, {0x2F, 0x30}},
	{REGFLAG_CMD, 2, {0X6D, 0x02}},
	{REGFLAG_CMD, 2, {0X5A, 0x01}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x01}},
	/*sleep out*/
	{REGFLAG_CMD, 2, {0x11, 0x00}},
	{REGFLAG_DELAY, 123, {}},
	/*display on*/
	{REGFLAG_CMD, 2, {0x29, 0x00}},
};

/* dsi-on cmd */
struct LCM_setting_table fhd_dsi_on_cmd_sdc90[] = {
	/*IP Setting*/
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x80}},
	{REGFLAG_CMD, 2, {0x6F, 0x19}},
	{REGFLAG_CMD, 2, {0xF2, 0x00}},
	{REGFLAG_CMD, 2, {0x6F, 0x1A}},
	{REGFLAG_CMD, 2, {0XF4, 0x55}},

	{REGFLAG_CMD, 2, {0x6F, 0x11}},
	{REGFLAG_CMD, 3, {0xF8, 0x01, 0x4E}},
	{REGFLAG_CMD, 2, {0x6F, 0x2D}},
	{REGFLAG_CMD, 3, {0XF8, 0x00, 0xFD}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x81}},
	{REGFLAG_CMD, 2, {0x6F, 0x05}},
	{REGFLAG_CMD, 2, {0xFE, 0x3C}},
	{REGFLAG_CMD, 2, {0x6F, 0x02}},
	{REGFLAG_CMD, 2, {0XF9, 0x04}},
	{REGFLAG_CMD, 2, {0x6F, 0x1E}},
	{REGFLAG_CMD, 2, {0XFB, 0x0F}},
	{REGFLAG_CMD, 2, {0x6F, 0x0F}},
	{REGFLAG_CMD, 2, {0XF5, 0x20}},
	{REGFLAG_CMD, 2, {0x6F, 0x0D}},
	{REGFLAG_CMD, 2, {0XFB, 0x80}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x82}},
	{REGFLAG_CMD, 2, {0x6F, 0x09}},
	{REGFLAG_CMD, 2, {0XF2, 0x55}},
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x83}},
	{REGFLAG_CMD, 2, {0x6F, 0x12}},
	{REGFLAG_CMD, 2, {0XFE, 0x41}},
	{REGFLAG_CMD, 2, {0x6F, 0x13}},
	{REGFLAG_CMD, 2, {0XFD, 0x21}},
	{REGFLAG_CMD, 2, {0X26, 0x00}},
	{REGFLAG_CMD, 3, {0X81, 0x01, 0x00}},
	/*TE(Vsync) ON/OFF*/
	{REGFLAG_CMD, 2, {0X35, 0x00}},
	{REGFLAG_CMD, 2, {0x53, 0x20}},
	{REGFLAG_CMD, 5, {0x2A, 0x00, 0x00, 0x04, 0XD7}},
	{REGFLAG_CMD, 5, {0x2B, 0x00, 0x00, 0x09, 0XD3}},
	/*DSC setting 8bit-3x*/
	{REGFLAG_CMD, 2, {0x03, 0x01}},
	{REGFLAG_CMD, 3, {0x90, 0x03, 0x03}},
	{REGFLAG_CMD, 19, {0x91, 0x89, 0x28, 0x00, 0x28, 0xD2, 0x00, 0x02,
		0x25, 0x03, 0xB6, 0x00, 0x07, 0x02, 0xAB, 0x02, 0x8B, 0x10, 0xF0}},
	/*120Hz>90Hz*/
	{REGFLAG_CMD, 2, {0x2F, 0x01}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x00}},
	/*sleep out*/
	{REGFLAG_CMD, 2, {0x11, 0x00}},
	{REGFLAG_DELAY, 123, {}},
	/*display on*/
	{REGFLAG_CMD, 2, {0x29, 0x00}},
};


/* dsi-on cmd */
struct LCM_setting_table fhd_dsi_on_cmd_sdc120[] = {
	/*IP Setting*/
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x80}},
	{REGFLAG_CMD, 2, {0x6F, 0x19}},
	{REGFLAG_CMD, 2, {0xF2, 0x00}},
	{REGFLAG_CMD, 2, {0x6F, 0x1A}},
	{REGFLAG_CMD, 2, {0XF4, 0x55}},

	{REGFLAG_CMD, 2, {0x6F, 0x11}},
	{REGFLAG_CMD, 3, {0xF8, 0x01, 0x4E}},
	{REGFLAG_CMD, 2, {0x6F, 0x2D}},
	{REGFLAG_CMD, 3, {0XF8, 0x00, 0xFD}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x81}},
	{REGFLAG_CMD, 2, {0x6F, 0x05}},
	{REGFLAG_CMD, 2, {0xFE, 0x3C}},
	{REGFLAG_CMD, 2, {0x6F, 0x02}},
	{REGFLAG_CMD, 2, {0XF9, 0x04}},
	{REGFLAG_CMD, 2, {0x6F, 0x1E}},
	{REGFLAG_CMD, 2, {0XFB, 0x0F}},
	{REGFLAG_CMD, 2, {0x6F, 0x0F}},
	{REGFLAG_CMD, 2, {0XF5, 0x20}},
	{REGFLAG_CMD, 2, {0x6F, 0x0D}},
	{REGFLAG_CMD, 2, {0XFB, 0x80}},

	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x82}},
	{REGFLAG_CMD, 2, {0x6F, 0x09}},
	{REGFLAG_CMD, 2, {0XF2, 0x55}},
	{REGFLAG_CMD, 5, {0xFF, 0xAA, 0x55, 0xA5, 0x83}},
	{REGFLAG_CMD, 2, {0x6F, 0x12}},
	{REGFLAG_CMD, 2, {0XFE, 0x41}},
	{REGFLAG_CMD, 2, {0x6F, 0x13}},
	{REGFLAG_CMD, 2, {0XFD, 0x21}},
	{REGFLAG_CMD, 2, {0X26, 0x00}},
	{REGFLAG_CMD, 3, {0X81, 0x01, 0x00}},
	/*TE(Vsync) ON/OFF*/
	{REGFLAG_CMD, 2, {0X35, 0x00}},
	{REGFLAG_CMD, 2, {0x53, 0x20}},
	{REGFLAG_CMD, 5, {0x2A, 0x00, 0x00, 0x04, 0XD7}},
	{REGFLAG_CMD, 5, {0x2B, 0x00, 0x00, 0x09, 0XD3}},

	/*DSC setting 8bit-3x*/
	{REGFLAG_CMD, 2, {0x03, 0x01}},
	{REGFLAG_CMD, 3, {0x90, 0x03, 0x03}},
	{REGFLAG_CMD, 19, {0x91, 0x89, 0x28, 0x00, 0x28, 0xD2, 0x00, 0x02, 0x25,
		0x03, 0xB6, 0x00, 0x07, 0x02, 0xAB, 0x02, 0x8B, 0x10, 0xF0}},
	/*120Hz*/
	{REGFLAG_CMD, 2, {0x2F, 0x00}},
	{REGFLAG_CMD, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
	{REGFLAG_CMD, 2, {0x6F, 0x6B}},
	{REGFLAG_CMD, 2, {0xB0, 0x01}},
	/*sleep out*/
	{REGFLAG_CMD, 2, {0x11, 0x00}},
	{REGFLAG_DELAY, 123, {}},
	/*display on*/
	{REGFLAG_CMD, 2, {0x29, 0x00}},
};


/* ------------------------- common parameters ------------------------- */

/* --------------- panel common --------------- */
/* aod/fod command */
struct LCM_setting_table aod_on_cmd[] = {

};

struct LCM_setting_table aod_off_cmd[] = {

};

struct LCM_setting_table aod_high_mode[] = {

};

struct LCM_setting_table aod_low_mode[] = {

};

struct LCM_setting_table hbm_on_cmd[] = {

};

struct LCM_setting_table hbm_off_cmd[] = {

};

/* loading effect */
struct LCM_setting_table lcm_seed_mode0[] = {
	/* Loading/Effect Setting mode0 off */
};

struct LCM_setting_table lcm_seed_mode1[] = {
	/* Loading/Effect Setting mode1 100%*/
};

struct LCM_setting_table lcm_seed_mode2[] = {
	/* Loading/Effect Setting mode2 110% */
};

/* --------------- adfr common --------------- */
/* pre-switch cmd */
struct LCM_setting_table pre_switch_cmd[] = {

};

/* fake frame cmd, used for sdc 90/120 */
struct LCM_setting_table fakeframe_cmd[] = {

};

/* SDC Auto On */
struct LCM_setting_table auto_on_cmd[] = {

};

/* SDC Auto Off */
struct LCM_setting_table auto_off_cmd[] = {

};

/* SDC Auto Off Min Fps */
struct LCM_setting_table auto_off_minfps_cmd[] = {

};

/* SDC Auto On Min Fps */
struct LCM_setting_table auto_on_minfps_cmd[] = {

};

#endif
