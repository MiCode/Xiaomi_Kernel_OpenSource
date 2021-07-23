/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#include <linux/kernel.h>
#include "mtk_log.h"
#include "mtk_rect.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_ddp_addon.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct mtk_plane_state;
struct drm_crtc_state;
struct mm_qos_request;

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_DITHER,
	MTK_DISP_CCORR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DPI,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_BLS,
	MTK_DISP_RSZ,
	MTK_DISP_POSTMASK,
	MTK_DMDP_RDMA,
	MTK_DMDP_HDR,
	MTK_DMDP_AAL,
	MTK_DMDP_RSZ,
	MTK_DMDP_TDSHP,
	MTK_DISP_DSC,
	MTK_DP_INTF,
	MTK_DISP_MERGE,
	MTK_DISP_DPTX,
	MTK_DISP_VIRTUAL,
	MTK_DDP_COMP_TYPE_MAX,
};

#define DECLARE_DDP_COMP(EXPR)                                                 \
	EXPR(DDP_COMPONENT_AAL0)                                            \
	EXPR(DDP_COMPONENT_AAL1)                                            \
	EXPR(DDP_COMPONENT_BLS)                                             \
	EXPR(DDP_COMPONENT_CCORR0)                                          \
	EXPR(DDP_COMPONENT_CCORR1)                                          \
	EXPR(DDP_COMPONENT_COLOR0)                                          \
	EXPR(DDP_COMPONENT_COLOR1)                                          \
	EXPR(DDP_COMPONENT_COLOR2)                                          \
	EXPR(DDP_COMPONENT_DITHER0)                                         \
	EXPR(DDP_COMPONENT_DITHER1)                                         \
	EXPR(DDP_COMPONENT_DPI0)                                            \
	EXPR(DDP_COMPONENT_DPI1)                                            \
	EXPR(DDP_COMPONENT_DSI0)                                            \
	EXPR(DDP_COMPONENT_DSI1)                                            \
	EXPR(DDP_COMPONENT_GAMMA0)                                          \
	EXPR(DDP_COMPONENT_GAMMA1)                                          \
	EXPR(DDP_COMPONENT_OD)                                              \
	EXPR(DDP_COMPONENT_OD1)                                             \
	EXPR(DDP_COMPONENT_OVL0)                                            \
	EXPR(DDP_COMPONENT_OVL1)                                            \
	EXPR(DDP_COMPONENT_OVL2)                                            \
	EXPR(DDP_COMPONENT_OVL0_2L)                                         \
	EXPR(DDP_COMPONENT_OVL1_2L)                                         \
	EXPR(DDP_COMPONENT_OVL2_2L)                                         \
	EXPR(DDP_COMPONENT_OVL3_2L)                                         \
	EXPR(DDP_COMPONENT_OVL0_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL1_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL0_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_OVL1_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_OVL0_OVL0_2L_VIRTUAL0)                           \
	EXPR(DDP_COMPONENT_PWM0)                                            \
	EXPR(DDP_COMPONENT_PWM1)                                            \
	EXPR(DDP_COMPONENT_PWM2)                                            \
	EXPR(DDP_COMPONENT_RDMA0)                                           \
	EXPR(DDP_COMPONENT_RDMA1)                                           \
	EXPR(DDP_COMPONENT_RDMA2)                                           \
	EXPR(DDP_COMPONENT_RDMA3)                                           \
	EXPR(DDP_COMPONENT_RDMA4)                                           \
	EXPR(DDP_COMPONENT_RDMA5)                                           \
	EXPR(DDP_COMPONENT_RDMA0_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA1_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA2_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RSZ0)                                            \
	EXPR(DDP_COMPONENT_RSZ1)                                            \
	EXPR(DDP_COMPONENT_UFOE)                                            \
	EXPR(DDP_COMPONENT_WDMA0)                                           \
	EXPR(DDP_COMPONENT_WDMA1)                                           \
	EXPR(DDP_COMPONENT_UFBC_WDMA0)                                      \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL1)                                   \
	EXPR(DDP_COMPONENT_POSTMASK0)                                       \
	EXPR(DDP_COMPONENT_POSTMASK1)                                       \
	EXPR(DDP_COMPONENT_DMDP_RDMA0)                                      \
	EXPR(DDP_COMPONENT_DMDP_HDR0)                                       \
	EXPR(DDP_COMPONENT_DMDP_AAL0)                                       \
	EXPR(DDP_COMPONENT_DMDP_RSZ0)                                       \
	EXPR(DDP_COMPONENT_DMDP_TDSHP0)                                     \
	EXPR(DDP_COMPONENT_DSC0)                                            \
	EXPR(DDP_COMPONENT_MERGE0)                                          \
	EXPR(DDP_COMPONENT_DPTX)                                            \
	EXPR(DDP_COMPONENT_DP_INTF0)                                        \
	EXPR(DDP_COMPONENT_RDMA4_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA5_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_MERGE1)                                          \
	EXPR(DDP_COMPONENT_SPR0_VIRTUAL)                                    \
	EXPR(DDP_COMPONENT_CM0)                                          \
	EXPR(DDP_COMPONENT_SPR0)                                          \
	EXPR(DDP_COMPONENT_ID_MAX)

#define DECLARE_NUM(ENUM) ENUM,
#define DECLARE_STR(STR) #STR,

enum mtk_ddp_comp_id { DECLARE_DDP_COMP(DECLARE_NUM) };

#if 0 /* Origin enum define */
enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_COLOR2,
	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_OVL2,
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_ID_MAX,
};
#endif

struct mtk_ddp_comp;
struct cmdq_pkt;
enum mtk_ddp_comp_trigger_flag {
	MTK_TRIG_FLAG_TRIGGER,
	MTK_TRIG_FLAG_EOF,
	MTK_TRIG_FLAG_LAYER_REC,
};

enum mtk_ddp_io_cmd {
	REQ_PANEL_EXT = 0,
	MTK_IO_CMD_RDMA_GOLDEN_SETTING = 1,
	MTK_IO_CMD_OVL_GOLDEN_SETTING = 2,
	DSI_START_VDO_MODE = 3,
	DSI_STOP_VDO_MODE = 4,
	ESD_CHECK_READ = 5,
	ESD_CHECK_CMP = 6,
	REQ_ESD_EINT_COMPAT = 7,
	COMP_REG_START = 8,
	CONNECTOR_ENABLE = 9,
	CONNECTOR_DISABLE = 10,
	CONNECTOR_RESET = 11,
	CONNECTOR_READ_EPILOG = 12,
	CONNECTOR_IS_ENABLE = 13,
	CONNECTOR_PANEL_ENABLE = 14,
	CONNECTOR_PANEL_DISABLE = 15,
	OVL_ALL_LAYER_OFF = 16,
	IRQ_LEVEL_ALL = 17,
	IRQ_LEVEL_IDLE = 18,
	DSI_VFP_IDLE_MODE = 19,
	DSI_VFP_DEFAULT_MODE = 20,
	DSI_GET_TIMING = 21,
	DSI_GET_MODE_BY_MAX_VREFRESH = 22,
	PMQOS_SET_BW = 23,
	PMQOS_SET_HRT_BW = 24,
	PMQOS_UPDATE_BW = 25,
	OVL_REPLACE_BOOTUP_MVA = 26,
	BACKUP_INFO_CMP = 27,
	LCM_RESET = 28,
	DSI_SET_BL = 29,
	DSI_SET_BL_GRP = 30,
	DSI_HBM_SET = 31,
	DSI_HBM_GET_STATE = 32,
	DSI_HBM_GET_WAIT_STATE = 33,
	DSI_HBM_SET_WAIT_STATE = 34,
	DSI_HBM_WAIT = 35,
	LCM_ATA_CHECK = 36,
	DSI_SET_CRTC_AVAIL_MODES = 37,
	DSI_TIMING_CHANGE = 38,
	GET_PANEL_NAME = 39,
	DSI_CHANGE_MODE = 40,
	BACKUP_OVL_STATUS = 41,
	MIPI_HOPPING = 42,
	PANEL_OSC_HOPPING = 43,
	DYN_FPS_INDEX = 44,
	SET_MMCLK_BY_DATARATE = 45,
	GET_FRAME_HRT_BW_BY_DATARATE = 46,
	DSI_SEND_DDIC_CMD = 47,
	DSI_READ_DDIC_CMD = 48,
	DSI_GET_VIRTUAL_HEIGH = 49,
	DSI_GET_VIRTUAL_WIDTH = 50,
	FRAME_DIRTY = 51,
	DSI_SET_IDLE_FPS = 52,
	DSI_SET_NON_IDLE_FPS = 53,
	DSI_SET_BL_AOD = 54,
	DSI_HBM_NEED_DELAY = 55,
	DSI_LFR_SET = 56,
	DSI_LFR_UPDATE = 57,
	DSI_LFR_STATUS_CHECK = 58,
	DSI_HBM_SOLUTION = 59,
};

static inline const char *get_cmd_name(enum mtk_ddp_io_cmd cmd)
{
	switch (cmd) {
	case REQ_PANEL_EXT:
		return "REQ_PANEL_EXT";
	case MTK_IO_CMD_RDMA_GOLDEN_SETTING:
		return "MTK_IO_CMD_RDMA_GOLDEN_SETTING";
	case MTK_IO_CMD_OVL_GOLDEN_SETTING:
		return "MTK_IO_CMD_OVL_GOLDEN_SETTING";
	case DSI_START_VDO_MODE:
		return "DSI_START_VDO_MODE";
	case DSI_STOP_VDO_MODE:
		return "DSI_STOP_VDO_MODE";
	case ESD_CHECK_READ:
		return "ESD_CHECK_READ";
	case ESD_CHECK_CMP:
		return "REQ_ESD_EINT_COMPAT";
	case REQ_ESD_EINT_COMPAT:
		return "REQ_ESD_EINT_COMPAT";
	case COMP_REG_START:
		return "COMP_REG_START";
	case CONNECTOR_ENABLE:
		return "CONNECTOR_ENABLE";
	case CONNECTOR_DISABLE:
		return "CONNECTOR_DISABLE";
	case CONNECTOR_RESET:
		return "CONNECTOR_RESET";
	case CONNECTOR_READ_EPILOG:
		return "CONNECTOR_READ_EPILOG";
	case CONNECTOR_IS_ENABLE:
		return "CONNECTOR_IS_ENABLE";
	case CONNECTOR_PANEL_ENABLE:
		return "CONNECTOR_PANEL_ENABLE";
	case CONNECTOR_PANEL_DISABLE:
		return "CONNECTOR_PANEL_DISABLE";
	case OVL_ALL_LAYER_OFF:
		return "OVL_ALL_LAYER_OFF";
	case IRQ_LEVEL_ALL:
		return "IRQ_LEVEL_ALL";
	case IRQ_LEVEL_IDLE:
		return "IRQ_LEVEL_IDLE";
	case DSI_VFP_IDLE_MODE:
		return "DSI_VFP_IDLE_MODE";
	case DSI_VFP_DEFAULT_MODE:
		return "DSI_VFP_DEFAULT_MODE";
	case DSI_GET_TIMING:
		return "DSI_GET_TIMING";
	case DSI_GET_MODE_BY_MAX_VREFRESH:
		return "DSI_GET_MODE_BY_MAX_VREFRESH";
	case PMQOS_SET_BW:
		return "PMQOS_SET_BW";
	case PMQOS_SET_HRT_BW:
		return "PMQOS_SET_HRT_BW";
	case PMQOS_UPDATE_BW:
		return "PMQOS_UPDATE_BW";
	case OVL_REPLACE_BOOTUP_MVA:
		return "OVL_REPLACE_BOOTUP_MVA";
	case BACKUP_INFO_CMP:
		return "BACKUP_INFO_CMP";
	case LCM_RESET:
		return "LCM_RESET";
	case DSI_SET_BL:
		return "DSI_SET_BL";
	case DSI_SET_BL_GRP:
		return "DSI_SET_BL_GRP";
	case DSI_HBM_SET:
		return "DSI_HBM_SET";
	case DSI_HBM_GET_STATE:
		return "DSI_HBM_GET_STATE";
	case DSI_HBM_GET_WAIT_STATE:
		return "DSI_HBM_GET_WAIT_STATE";
	case DSI_HBM_SET_WAIT_STATE:
		return "DSI_HBM_SET_WAIT_STATE";
	case DSI_HBM_WAIT:
		return "DSI_HBM_WAIT";
	case LCM_ATA_CHECK:
		return "LCM_ATA_CHECK";
	case DSI_SET_CRTC_AVAIL_MODES:
		return "DSI_SET_CRTC_AVAIL_MODES";
	case DSI_TIMING_CHANGE:
		return "DSI_TIMING_CHANGE";
	case GET_PANEL_NAME:
		return "GET_PANEL_NAME";
	case DSI_CHANGE_MODE:
		return "DSI_CHANGE_MODE";
	case BACKUP_OVL_STATUS:
		return "BACKUP_OVL_STATUS";
	case MIPI_HOPPING:
		return "MIPI_HOPPING";
	case PANEL_OSC_HOPPING:
		return "PANEL_OSC_HOPPING";
	case DYN_FPS_INDEX:
		return "DYN_FPS_INDEX";
	case SET_MMCLK_BY_DATARATE:
		return "SET_MMCLK_BY_DATARATE";
	case GET_FRAME_HRT_BW_BY_DATARATE:
		return "GET_FRAME_HRT_BW_BY_DATARATE";
	case DSI_SEND_DDIC_CMD:
		return "DSI_SEND_DDIC_CMD";
	case DSI_READ_DDIC_CMD:
		return "DSI_READ_DDIC_CMD";
	case DSI_GET_VIRTUAL_HEIGH:
		return "DSI_GET_VIRTUAL_HEIGH";
	case DSI_GET_VIRTUAL_WIDTH:
		return "DSI_GET_VIRTUAL_WIDTH";
	case FRAME_DIRTY:
		return "FRAME_DIRTY";
	case DSI_SET_IDLE_FPS:
		return "DSI_SET_IDLE_FPS";
	case DSI_SET_NON_IDLE_FPS:
		return "DSI_SET_NON_IDLE_FPS";
	case DSI_SET_BL_AOD:
		return "DSI_SET_BL_AOD";
	case DSI_HBM_NEED_DELAY:
		return "DSI_HBM_NEED_DELAY";
	case DSI_LFR_SET:
		return "DSI_LFR_SET";
	case DSI_LFR_UPDATE:
		return "DSI_LFR_UPDATE";
	case DSI_LFR_STATUS_CHECK:
		return "DSI_LFR_STATUS_CHECK";
	case DSI_HBM_SOLUTION:
		return "DSI_HBM_SOLUTION";
	}
}

enum DISPPARAM_MODE {
	DISPPARAM_WARM = 0x1,
	DISPPARAM_DEFAULT = 0x2,
	DISPPARAM_COLD = 0x3,
	DISPPARAM_PAPERMODE8 = 0x5,
	DISPPARAM_PAPERMODE1 = 0x6,
	DISPPARAM_PAPERMODE2 = 0x7,
	DISPPARAM_PAPERMODE3 = 0x8,
	DISPPARAM_PAPERMODE4 = 0x9,
	DISPPARAM_PAPERMODE5 = 0xA,
	DISPPARAM_PAPERMODE6 = 0xB,
	DISPPARAM_PAPERMODE7 = 0xC,
	DISPPARAM_WHITEPOINT_XY = 0xE,
	DISPPARAM_CE_ON = 0x10,
	DISPPARAM_CE_OFF = 0xF0,
	DISPPARAM_CABCUI_ON = 0x100,
	DISPPARAM_CABCSTILL_ON = 0x200,
	DISPPARAM_CABCMOVIE_ON = 0x300,
	DISPPARAM_CABC_OFF = 0x400,
	DISPPARAM_SKIN_CE_CABCUI_ON = 0x500,
	DISPPARAM_SKIN_CE_CABCSTILL_ON = 0x600,
	DISPPARAM_SKIN_CE_CABCMOVIE_ON = 0x700,
	DISPPARAM_SKIN_CE_CABC_OFF = 0x800,
	DISPPARAM_DIMMING_OFF = 0xE00,
	DISPPARAM_DIMMING = 0xF00,
	DISPPARAM_ACL_L1 = 0x1000,
	DISPPARAM_ACL_L2 = 0x2000,
	DISPPARAM_ACL_L3 = 0x3000,
	DISPPARAM_ACL_OFF = 0xF000,
	DISPPARAM_HBM_ON = 0x10000,
	DISPPARAM_HBM_FOD_ON = 0x20000,
	DISPPARAM_HBM_FOD2NORM = 0x30000,
	DISPPARAM_DC_ON = 0x40000,
	DISPPARAM_DC_OFF = 0x50000,
	DISPPARAM_HBM_FOD_OFF = 0xE0000,
	DISPPARAM_HBM_OFF = 0xF0000,
	DISPPARAM_LCD_HBM_L1_ON = 0xB0000,
	DISPPARAM_LCD_HBM_L2_ON = 0xC0000,
	DISPPARAM_LCD_HBM_L3_ON = 0xD0000,
	DISPPARAM_LCD_HBM_OFF      = 0xA0000,
	DISPPARAM_NORMALMODE1 = 0x100000,
	DISPPARAM_P3 = 0x200000,
	DISPPARAM_SRGB = 0x300000,
	DISPPARAM_SKIN_CE = 0x400000,
	DISPPARAM_SKIN_CE_OFF = 0x500000,
	DISPPARAM_DOZE_BRIGHTNESS_HBM = 0x600000,
	DISPPARAM_DOZE_BRIGHTNESS_LBM = 0x700000,
	DISPPARAM_DOZE_OFF = 0x800000,
	DISPPARAM_HBM_BACKLIGHT_RESEND = 0xA00000,
	DISPPARAM_FOD_BACKLIGHT = 0xD00000,
	DISPPARAM_CRC_P3_D65 = 0xE00000,
	DISPPARAM_CRC_OFF = 0xF00000,
	DISPPARAM_FOD_BACKLIGHT_ON = 0x1000000,
	DISPPARAM_FOD_BACKLIGHT_OFF = 0x2000000,
	DISPPARAM_FLAT_CRC_P3 = 0x4000000,
	DISPPARAM_ONE_PLUSE = 0x5000000,
	DISPPARAM_FOUR_PLUSE = 0x6000000,
	DISPPARAM_DEMURA_LEVEL02 = 0x7000000,
	DISPPARAM_DEMURA_LEVEL08 = 0x8000000,
	DISPPARAM_DEMURA_LEVEL0D = 0x9000000,
	DISPPARAM_IDLE_ON = 0xA000000,
	DISPPARAM_IDLE_OFF = 0xB000000,
	DISPPARAM_BACKLIGHT_SET = 0xC000000,
	DISPPARAM_PANEL_ID_GET = 0xD000000,
	DISPPARAM_SET_THERMAL_HBM_DISABLE     = 0xE000000,
	DISPPARAM_CLEAR_THERMAL_HBM_DISABLE   = 0xF000000,
	DISPPARAM_DOZE_STATE = 0x10000000,
	DISPPARAM_RESTORE_BACKLIGHT = 0x20000000,
	DISPPARAM_DFPS_STATE = 0x30000000,
};

static inline const char *get_dispparam_name(enum DISPPARAM_MODE mode)
{
	switch (mode) {
	case DISPPARAM_WARM:
		return "DISPPARAM_WARM";
	case DISPPARAM_DEFAULT:
		return "DISPPARAM_DEFAULT";
	case DISPPARAM_COLD:
		return "DISPPARAM_COLD";
	case DISPPARAM_PAPERMODE8:
		return "DISPPARAM_PAPERMODE8";
	case DISPPARAM_PAPERMODE1:
		return "DISPPARAM_PAPERMODE1";
	case DISPPARAM_PAPERMODE2:
		return "DISPPARAM_PAPERMODE2";
	case DISPPARAM_PAPERMODE3:
		return "DISPPARAM_PAPERMODE3";
	case DISPPARAM_PAPERMODE4:
		return "DISPPARAM_PAPERMODE4";
	case DISPPARAM_PAPERMODE5:
		return "DISPPARAM_PAPERMODE5";
	case DISPPARAM_PAPERMODE6:
		return "DISPPARAM_PAPERMODE6";
	case DISPPARAM_PAPERMODE7:
		return "DISPPARAM_PAPERMODE7";
	case DISPPARAM_WHITEPOINT_XY:
		return "DISPPARAM_WHITEPOINT_XY";
	case DISPPARAM_CE_ON:
		return "DISPPARAM_CE_ON";
	case DISPPARAM_CE_OFF:
		return "DISPPARAM_CE_OFF";
	case DISPPARAM_CABCUI_ON:
		return "DISPPARAM_CABCUI_ON";
	case DISPPARAM_CABCSTILL_ON:
		return "DISPPARAM_CABCSTILL_ON";
	case DISPPARAM_CABCMOVIE_ON:
		return "DISPPARAM_CABCMOVIE_ON";
	case DISPPARAM_CABC_OFF:
		return "DISPPARAM_CABC_OFF";
	case DISPPARAM_SKIN_CE_CABCUI_ON:
		return "DISPPARAM_SKIN_CE_CABCUI_ON";
	case DISPPARAM_SKIN_CE_CABCSTILL_ON:
		return "DISPPARAM_SKIN_CE_CABCSTILL_ON";
	case DISPPARAM_SKIN_CE_CABCMOVIE_ON:
		return "DISPPARAM_SKIN_CE_CABCMOVIE_ON";
	case DISPPARAM_SKIN_CE_CABC_OFF:
		return "DISPPARAM_SKIN_CE_CABC_OFF";
	case DISPPARAM_DIMMING_OFF:
		return "DISPPARAM_DIMMING_OFF";
	case DISPPARAM_DIMMING:
		return "DISPPARAM_DIMMING";
	case DISPPARAM_ACL_L1:
		return "DISPPARAM_ACL_L1";
	case DISPPARAM_ACL_L2:
		return "DISPPARAM_ACL_L2";
	case DISPPARAM_ACL_L3:
		return "DISPPARAM_ACL_L3";
	case DISPPARAM_ACL_OFF:
		return "DISPPARAM_ACL_OFF";
	case DISPPARAM_HBM_ON:
		return "DISPPARAM_HBM_ON";
	case DISPPARAM_HBM_FOD_ON:
		return "DISPPARAM_HBM_FOD_ON";
	case DISPPARAM_HBM_FOD2NORM:
		return "DISPPARAM_HBM_FOD2NORM";
	case DISPPARAM_DC_ON:
		return "DISPPARAM_DC_ON";
	case DISPPARAM_DC_OFF:
		return "DISPPARAM_DC_OFF";
	case DISPPARAM_HBM_FOD_OFF:
		return "DISPPARAM_HBM_FOD_OFF";
	case DISPPARAM_HBM_OFF:
		return "DISPPARAM_HBM_OFF";
	case DISPPARAM_LCD_HBM_L1_ON:
		return "DISPPARAM_LCD_HBM_L1_ON";
	case DISPPARAM_LCD_HBM_L2_ON:
		return "DISPPARAM_LCD_HBM_L2_ON";
	case DISPPARAM_LCD_HBM_L3_ON:
		return "DISPPARAM_LCD_HBM_L3_ON";
	case DISPPARAM_LCD_HBM_OFF:
		return "DISPPARAM_LCD_HBM_OFF";
	case DISPPARAM_NORMALMODE1:
		return "DISPPARAM_NORMALMODE1";
	case DISPPARAM_P3:
		return "DISPPARAM_P3";
	case DISPPARAM_SRGB:
		return "DISPPARAM_SRGB";
	case DISPPARAM_SKIN_CE:
		return "DISPPARAM_SKIN_CE";
	case DISPPARAM_SKIN_CE_OFF:
		return "DISPPARAM_SKIN_CE_OFF";
	case DISPPARAM_DOZE_BRIGHTNESS_HBM:
		return "DISPPARAM_DOZE_BRIGHTNESS_HBM";
	case DISPPARAM_DOZE_BRIGHTNESS_LBM:
		return "DISPPARAM_DOZE_BRIGHTNESS_LBM";
	case DISPPARAM_DOZE_OFF:
		return "DISPPARAM_DOZE_OFF";
	case DISPPARAM_HBM_BACKLIGHT_RESEND:
		return "DISPPARAM_HBM_BACKLIGHT_RESEND";
	case DISPPARAM_FOD_BACKLIGHT:
		return "DISPPARAM_FOD_BACKLIGHT";
	case DISPPARAM_CRC_P3_D65:
		return "DISPPARAM_CRC_P3_D65";
	case DISPPARAM_CRC_OFF:
		return "DISPPARAM_CRC_OFF";
	case DISPPARAM_FOD_BACKLIGHT_ON:
		return "DISPPARAM_FOD_BACKLIGHT_ON";
	case DISPPARAM_FOD_BACKLIGHT_OFF:
		return "DISPPARAM_FOD_BACKLIGHT_OFF";
	case DISPPARAM_FLAT_CRC_P3:
		return "DISPPARAM_FLAT_CRC_P3";
	case DISPPARAM_ONE_PLUSE:
		return "DISPPARAM_ONE_PLUSE";
	case DISPPARAM_FOUR_PLUSE:
		return "DISPPARAM_FOUR_PLUSE";
	case DISPPARAM_DEMURA_LEVEL02:
		return "DISPPARAM_DEMURA_LEVEL02";
	case DISPPARAM_DEMURA_LEVEL08:
		return "DISPPARAM_DEMURA_LEVEL08";
	case DISPPARAM_DEMURA_LEVEL0D:
		return "DISPPARAM_DEMURA_LEVEL0D";
	case DISPPARAM_IDLE_ON:
		return "DISPPARAM_IDLE_ON";
	case DISPPARAM_IDLE_OFF:
		return "DISPPARAM_IDLE_OFF";
	case DISPPARAM_BACKLIGHT_SET:
		return "DISPPARAM_BACKLIGHT_SET";
	case DISPPARAM_PANEL_ID_GET:
		return "DISPPARAM_PANEL_ID_GET";
	case DISPPARAM_SET_THERMAL_HBM_DISABLE:
		return "DISPPARAM_SET_THERMAL_HBM_DISABLE";
	case DISPPARAM_CLEAR_THERMAL_HBM_DISABLE:
		return "DISPPARAM_CLEAR_THERMAL_HBM_DISABLE";
	case DISPPARAM_DOZE_STATE:
		return "DISPPARAM_DOZE_STATE";
	case DISPPARAM_RESTORE_BACKLIGHT:
		return "DISPPARAM_RESTORE_BACKLIGHT";
	case DISPPARAM_DFPS_STATE:
		return "DISPPARAM_DFPS_STATE";
	}
}

struct golden_setting_context {
	unsigned int is_vdo_mode;
	unsigned int is_dc;
	unsigned int dst_width;
	unsigned int dst_height;
	// add for rdma default goden setting
	unsigned int vrefresh;
};

struct mtk_ddp_config {
	void *pa;
	unsigned int w;
	unsigned int h;
	unsigned int x;
	unsigned int y;
	unsigned int vrefresh;
	unsigned int bpc;
	struct golden_setting_context *p_golden_setting_context;
};

struct mtk_ddp_fb_info {
	unsigned int fb_pa;
	unsigned int fb_mva;
	unsigned int fb_size;
};

struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg,
		       struct cmdq_pkt *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*stop)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp, struct drm_crtc *crtc,
			      struct cmdq_pkt *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
	void (*layer_on)(struct mtk_ddp_comp *comp, unsigned int idx,
			 unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp, unsigned int idx,
			  unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp, unsigned int idx,
			     struct mtk_plane_state *state,
			     struct cmdq_pkt *handle);
	void (*gamma_set)(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state,
			  struct cmdq_pkt *handle);
	void (*first_cfg)(struct mtk_ddp_comp *comp,
		       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle);
	void (*bypass)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*config_trigger)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle,
			       enum mtk_ddp_comp_trigger_flag trig_flag);
	void (*addon_config)(struct mtk_ddp_comp *comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *handle);
	int (*io_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      enum mtk_ddp_io_cmd cmd, void *params);
	int (*io_cmd_dispparam)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      enum DISPPARAM_MODE cmd, void *params);
	int (*user_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      unsigned int cmd, void *params);
	void (*connect)(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			enum mtk_ddp_comp_id next);
	int (*is_busy)(struct mtk_ddp_comp *comp);
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;
	int irq;
	struct device *larb_dev;
	struct device *dev;
	struct mtk_drm_crtc *mtk_crtc;
	u32 larb_id;
	enum mtk_ddp_comp_id id;
	struct drm_framebuffer *fb;
	const struct mtk_ddp_comp_funcs *funcs;
	void *comp_mode;
	struct cmdq_base *cmdq_base;
#if 0
	u8 cmdq_subsys;
#endif
	unsigned int qos_attr;
	struct mm_qos_request qos_req;
	struct mm_qos_request fbdc_qos_req;
	struct mm_qos_request hrt_qos_req;
	bool blank_mode;
	u32 qos_bw;
	u32 fbdc_bw;
	u32 hrt_bw;
};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->config && !comp->blank_mode)
		comp->funcs->config(comp, cfg, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->prepare && !comp->blank_mode)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->unprepare && !comp->blank_mode)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->start && !comp->blank_mode)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->stop && !comp->blank_mode)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
					      struct drm_crtc *crtc,
					      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->enable_vblank &&
			!comp->blank_mode)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp,
					       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->disable_vblank &&
			!comp->blank_mode)
		comp->funcs->disable_vblank(comp, handle);
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
					 unsigned int idx, unsigned int ext_idx,
					 struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_on && !comp->blank_mode)
		comp->funcs->layer_on(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
					  unsigned int idx,
					  unsigned int ext_idx,
					  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_off && !comp->blank_mode)
		comp->funcs->layer_off(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
					     unsigned int idx,
					     struct mtk_plane_state *state,
					     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_config &&
			!comp->blank_mode) {
		DDPDBG("[DRM]func:%s, line:%d ==>\n",
			__func__, __LINE__);
		DDPDBG("comp_funcs:0x%p, layer_config:0x%p\n",
			comp->funcs, comp->funcs->layer_config);

		comp->funcs->layer_config(comp, idx, state, handle);
	}
}

static inline void mtk_ddp_gamma_set(struct mtk_ddp_comp *comp,
				     struct drm_crtc_state *state,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->gamma_set && !comp->blank_mode)
		comp->funcs->gamma_set(comp, state, handle);
}

static inline void mtk_ddp_comp_bypass(struct mtk_ddp_comp *comp,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->bypass && !comp->blank_mode)
		comp->funcs->bypass(comp, handle);
}

static inline void mtk_ddp_comp_first_cfg(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->first_cfg && !comp->blank_mode)
		comp->funcs->first_cfg(comp, cfg, handle);
}

static inline void
mtk_ddp_comp_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_trigger_flag flag)
{
	if (comp && comp->funcs && comp->funcs->config_trigger &&
			!comp->blank_mode)
		comp->funcs->config_trigger(comp, handle, flag);
}

static inline void
mtk_ddp_comp_addon_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			  enum mtk_ddp_comp_id next,
			  union mtk_addon_config *addon_config,
			  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->addon_config &&
			!comp->blank_mode)
		comp->funcs->addon_config(comp, prev, next, addon_config,
				handle);
}

static inline int mtk_ddp_comp_io_cmd(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle,
				      enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = -EINVAL;

	if (comp && comp->funcs && comp->funcs->io_cmd && !comp->blank_mode)
		ret = comp->funcs->io_cmd(comp, handle, io_cmd, params);

	return ret;
}

static inline int mtk_ddp_comp_io_cmd_dispparam(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle,
				      enum DISPPARAM_MODE io_cmd, void *params)
{
	int ret = -EINVAL;

	if (comp && comp->funcs && comp->funcs->io_cmd)
		ret = comp->funcs->io_cmd_dispparam(comp, handle, io_cmd, params);

	return ret;
}

static inline int
mtk_ddp_comp_is_busy(struct mtk_ddp_comp *comp)
{
	int ret = 0;

	if (comp && comp->funcs && comp->funcs->is_busy && !comp->blank_mode)
		ret = comp->funcs->is_busy(comp);

	return ret;
}

static inline void mtk_ddp_cpu_mask_write(struct mtk_ddp_comp *comp,
					  unsigned int off, unsigned int val,
					  unsigned int mask)
{
	unsigned int v = (readl(comp->regs + off) & (~mask));

	v += (val & mask);
	writel_relaxed(v, comp->regs + off);
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_id(struct device_node *node,
					 enum mtk_ddp_comp_type comp_type);
struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id);
unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);
int mtk_ddp_comp_get_type(enum mtk_ddp_comp_id comp_id);
bool mtk_dsi_is_cmd_mode(struct mtk_ddp_comp *comp);
void mtk_dsi_esd_recovery_flag(struct mtk_ddp_comp *comp, struct drm_crtc *crtc);
int mtk_show_brightness_clone(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_output(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_get_name(struct mtk_ddp_comp *comp, char *buf, int buf_len);
int mtk_ovl_layer_num(struct mtk_ddp_comp *comp);
void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle);
void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle);
void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle);
void mtk_ddp_write_mask_cpu(struct mtk_ddp_comp *comp,
			unsigned int value, unsigned int offset,
			unsigned int mask);
void mtk_ddp_comp_clk_prepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_clk_unprepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_iommu_enable(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
void mt6779_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6885_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6873_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6853_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6833_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);


int mtk_ddp_comp_helper_get_opt(struct mtk_ddp_comp *comp,
				enum MTK_DRM_HELPER_OPT option);
#endif /* MTK_DRM_DDP_COMP_H */
