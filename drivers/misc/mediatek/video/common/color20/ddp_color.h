/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DDP_COLOR_H__
#define __DDP_COLOR_H__

#include "ddp_reg.h"
#include "ddp_aal.h"
#include "ddp_drv.h"
enum {
	ENUM_Y_SLOPE = 0 * 28,

	ENUM_S_GAIN1 = 1 * 28,
	ENUM_S_GAIN2 = 2 * 28,
	ENUM_S_GAIN3 = 3 * 28,
	ENUM_S_P1 = 4 * 28,
	ENUM_S_P2 = 5 * 28,

	ENUM_H_FTN = 6 * 28,

};

enum {
	COLOR_ID_0 = 0,
	COLOR_ID_1 = 1,
};

#define C0_OFFSET (0)
#define C1_OFFSET (DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE)

/* --------------------------------------------------------------------------- */
#define GAMMA_SIZE 1024
#define NR_SIZE 50
#define MAX_SCE_TABLE_SIZE (7*28)
#define SCE_PHASE 28
#define SCE_SIZE (SCE_PHASE*7)

#define PURP_TONE    0
#define SKIN_TONE    1
#define GRASS_TONE   2
#define SKY_TONE     3

#define PURP_TONE_START    0
#define PURP_TONE_END      2
#define SKIN_TONE_START    3
#define SKIN_TONE_END     10
#define GRASS_TONE_START  11
#define GRASS_TONE_END    16
#define SKY_TONE_START    17
#define SKY_TONE_END      19

#define SG1 0
#define SG2 1
#define SG3 2
#define SP1 3
#define SP2 4

#define MIRAVISION_HW_VERSION_MASK  (0xFF000000)
#define MIRAVISION_SW_VERSION_MASK  (0x00FF0000)
#define MIRAVISION_SW_FEATURE_MASK  (0x0000FFFF)
#define MIRAVISION_HW_VERSION_SHIFT (24)
#define MIRAVISION_SW_VERSION_SHIFT (16)
#define MIRAVISION_SW_FEATURE_SHIFT (0)

#if defined(CONFIG_ARCH_MT6595)
#define MIRAVISION_HW_VERSION       (1)
#elif defined(CONFIG_ARCH_MT6752)
#define MIRAVISION_HW_VERSION       (2)
#elif defined(CONFIG_ARCH_MT6795)
#define MIRAVISION_HW_VERSION       (3)
#elif defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT8167)
#define MIRAVISION_HW_VERSION       (4)
#elif defined(CONFIG_ARCH_MT6735M)
#define MIRAVISION_HW_VERSION       (5)
#elif defined(CONFIG_ARCH_MT6753)
#define MIRAVISION_HW_VERSION       (6)
#elif defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
#define MIRAVISION_HW_VERSION       (7)
#elif defined(CONFIG_ARCH_MT6755)
#define MIRAVISION_HW_VERSION       (8)
#elif defined(CONFIG_ARCH_MT6797)
#define MIRAVISION_HW_VERSION       (9)
#else
#define MIRAVISION_HW_VERSION       (0)
#endif

#define MIRAVISION_SW_VERSION       (3)	/* 3:Android N */
#define MIRAVISION_SW_FEATURE_VIDEO_DC  (0x1)
#define MIRAVISION_SW_FEATURE_AAL       (0x2)
#define MIRAVISION_SW_FEATURE_PQDS       (0x4)

#define MIRAVISION_VERSION          ((MIRAVISION_HW_VERSION << MIRAVISION_HW_VERSION_SHIFT) |   \
				     (MIRAVISION_SW_VERSION << MIRAVISION_SW_VERSION_SHIFT) |   \
				     MIRAVISION_SW_FEATURE_VIDEO_DC |   \
				     MIRAVISION_SW_FEATURE_AAL |   \
				     MIRAVISION_SW_FEATURE_PQDS)

#define SW_VERSION_VIDEO_DC         (1)
#define SW_VERSION_AAL              (1)
#if defined(CONFIG_ARCH_MT6755)
#define SW_VERSION_PQDS             (2)
#else
#define SW_VERSION_PQDS             (1)
#endif

#if defined(DISP_COLOR_ON)
#define COLOR_MODE			(1)
#elif defined(MDP_COLOR_ON)
#define COLOR_MODE			(2)
#elif defined(DISP_MDP_COLOR_ON)
#define COLOR_MODE			(3)
#else
#define COLOR_MODE			(0)	/*color feature off */
#endif


#define DISP_COLOR_SWREG_START              (0xFFFF0000)
#define DISP_COLOR_SWREG_COLOR_BASE         (DISP_COLOR_SWREG_START)	/* 0xFFFF0000 */
#define DISP_COLOR_SWREG_TDSHP_BASE         (DISP_COLOR_SWREG_COLOR_BASE + 0x1000)	/* 0xFFFF1000 */
#define DISP_COLOR_SWREG_PQDC_BASE          (DISP_COLOR_SWREG_TDSHP_BASE + 0x1000)	/* 0xFFFF2000 */
#define DISP_COLOR_SWREG_PQDS_BASE          (DISP_COLOR_SWREG_PQDC_BASE + 0x1000)	/* 0xFFFF3000 */
#define DISP_COLOR_SWREG_MDP_COLOR_BASE     (DISP_COLOR_SWREG_PQDS_BASE + 0x1000)	/* 0xFFFF4000 */
#define DISP_COLOR_SWREG_END                (DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x1000)	/* 0xFFFF5000 */

#define SWREG_COLOR_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0000)
#define SWREG_GAMMA_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0001)
#define SWREG_TDSHP_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0002)
#define SWREG_AAL_BASE_ADDRESS              (DISP_COLOR_SWREG_COLOR_BASE + 0x0003)
#define SWREG_MIRAVISION_VERSION            (DISP_COLOR_SWREG_COLOR_BASE + 0x0004)
#define SWREG_SW_VERSION_VIDEO_DC           (DISP_COLOR_SWREG_COLOR_BASE + 0x0005)
#define SWREG_SW_VERSION_AAL                (DISP_COLOR_SWREG_COLOR_BASE + 0x0006)
#define SWREG_CCORR_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0007)
#define SWREG_MDP_COLOR_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0008)
#define SWREG_COLOR_MODE                    (DISP_COLOR_SWREG_COLOR_BASE + 0x0009)
#define SWREG_RSZ_BASE_ADDRESS              (DISP_COLOR_SWREG_COLOR_BASE + 0x000A)


#define SWREG_TDSHP_TUNING_MODE             (DISP_COLOR_SWREG_TDSHP_BASE + 0x0000)
#define SWREG_TDSHP_GAIN_MID	            (DISP_COLOR_SWREG_TDSHP_BASE + 0x0001)
#define SWREG_TDSHP_GAIN_HIGH	            (DISP_COLOR_SWREG_TDSHP_BASE + 0x0002)
#define SWREG_TDSHP_COR_GAIN	            (DISP_COLOR_SWREG_TDSHP_BASE + 0x0003)
#define SWREG_TDSHP_COR_THR                 (DISP_COLOR_SWREG_TDSHP_BASE + 0x0004)
#define SWREG_TDSHP_COR_ZERO	            (DISP_COLOR_SWREG_TDSHP_BASE + 0x0005)
#define SWREG_TDSHP_GAIN                    (DISP_COLOR_SWREG_TDSHP_BASE + 0x0006)
#define SWREG_TDSHP_COR_VALUE	            (DISP_COLOR_SWREG_TDSHP_BASE + 0x0007)

#define SWREG_PQDC_BLACK_EFFECT_ENABLE      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectEnable)
#define SWREG_PQDC_WHITE_EFFECT_ENABLE      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectEnable)
#define SWREG_PQDC_STRONG_BLACK_EFFECT      (DISP_COLOR_SWREG_PQDC_BASE + StrongBlackEffect)
#define SWREG_PQDC_STRONG_WHITE_EFFECT      (DISP_COLOR_SWREG_PQDC_BASE + StrongWhiteEffect)
#define SWREG_PQDC_ADAPTIVE_BLACK_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + AdaptiveBlackEffect)
#define SWREG_PQDC_ADAPTIVE_WHITE_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + AdaptiveWhiteEffect)
#define SWREG_PQDC_SCENCE_CHANGE_ONCE_EN    (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeOnceEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL_EN (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControlEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL    (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControl)
#define SWREG_PQDC_SCENCE_CHANGE_TH1        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh1)
#define SWREG_PQDC_SCENCE_CHANGE_TH2        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh2)
#define SWREG_PQDC_SCENCE_CHANGE_TH3        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh3)
#define SWREG_PQDC_CONTENT_SMOOTH1          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth1)
#define SWREG_PQDC_CONTENT_SMOOTH2          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth2)
#define SWREG_PQDC_CONTENT_SMOOTH3          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth3)
#define SWREG_PQDC_MIDDLE_REGION_GAIN1      (DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain1)
#define SWREG_PQDC_MIDDLE_REGION_GAIN2      (DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain2)
#define SWREG_PQDC_BLACK_REGION_GAIN1       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain1)
#define SWREG_PQDC_BLACK_REGION_GAIN2       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain2)
#define SWREG_PQDC_BLACK_REGION_RANGE       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionRange)
#define SWREG_PQDC_BLACK_EFFECT_LEVEL       (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectLevel)
#define SWREG_PQDC_BLACK_EFFECT_PARAM1      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam1)
#define SWREG_PQDC_BLACK_EFFECT_PARAM2      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam2)
#define SWREG_PQDC_BLACK_EFFECT_PARAM3      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam3)
#define SWREG_PQDC_BLACK_EFFECT_PARAM4      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam4)
#define SWREG_PQDC_WHITE_REGION_GAIN1       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain1)
#define SWREG_PQDC_WHITE_REGION_GAIN2       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain2)
#define SWREG_PQDC_WHITE_REGION_RANGE       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionRange)
#define SWREG_PQDC_WHITE_EFFECT_LEVEL       (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectLevel)
#define SWREG_PQDC_WHITE_EFFECT_PARAM1      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam1)
#define SWREG_PQDC_WHITE_EFFECT_PARAM2      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam2)
#define SWREG_PQDC_WHITE_EFFECT_PARAM3      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam3)
#define SWREG_PQDC_WHITE_EFFECT_PARAM4      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam4)
#define SWREG_PQDC_CONTRAST_ADJUST1         (DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust1)
#define SWREG_PQDC_CONTRAST_ADJUST2         (DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust2)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL    (DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel)
#define SWREG_PQDC_PROTECT_REGION_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionEffect)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL2   (DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel2)
#define SWREG_PQDC_PROTECT_REGION_WEIGHT    (DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionWeight)
#define SWREG_PQDC_DC_ENABLE                (DISP_COLOR_SWREG_PQDC_BASE + DCEnable)

#define SWREG_PQDS_DS_EN                    (DISP_COLOR_SWREG_PQDS_BASE + DS_en)
#define SWREG_PQDS_UP_SLOPE                 (DISP_COLOR_SWREG_PQDS_BASE + iUpSlope)
#define SWREG_PQDS_UP_THR                   (DISP_COLOR_SWREG_PQDS_BASE + iUpThreshold)
#define SWREG_PQDS_DOWN_SLOPE               (DISP_COLOR_SWREG_PQDS_BASE + iDownSlope)
#define SWREG_PQDS_DOWN_THR                 (DISP_COLOR_SWREG_PQDS_BASE + iDownThreshold)
#define SWREG_PQDS_ISO_EN                   (DISP_COLOR_SWREG_PQDS_BASE + iISO_en)
#define SWREG_PQDS_ISO_THR1                 (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr1)
#define SWREG_PQDS_ISO_THR0                 (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr0)
#define SWREG_PQDS_ISO_THR3                 (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr3)
#define SWREG_PQDS_ISO_THR2                 (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr2)
#define SWREG_PQDS_ISO_IIR                  (DISP_COLOR_SWREG_PQDS_BASE + iISO_IIR_alpha)
#define SWREG_PQDS_COR_ZERO_2               (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip2)
#define SWREG_PQDS_COR_ZERO_1               (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip1)
#define SWREG_PQDS_COR_ZERO_0               (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip0)
#define SWREG_PQDS_COR_THR_2                (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip2)
#define SWREG_PQDS_COR_THR_1                (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip1)
#define SWREG_PQDS_COR_THR_0                (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip0)
#define SWREG_PQDS_COR_GAIN_2               (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip2)
#define SWREG_PQDS_COR_GAIN_1               (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip1)
#define SWREG_PQDS_COR_GAIN_0               (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip0)
#define SWREG_PQDS_GAIN_2                   (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip2)
#define SWREG_PQDS_GAIN_1                   (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip1)
#define SWREG_PQDS_GAIN_0                   (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip0)
#define SWREG_PQDS_END                      (DISP_COLOR_SWREG_PQDS_BASE + PQ_DS_INDEX_MAX)

#define SWREG_MDP_COLOR_CAPTURE_EN          (DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0000)
#define SWREG_MDP_COLOR_CAPTURE_POS_X       (DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0001)
#define SWREG_MDP_COLOR_CAPTURE_POS_Y       (DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0002)


/* --------------------------------------------------------------------------- */


DISP_PQ_PARAM *get_Color_config(int id);
DISP_PQ_PARAM *get_Color_Cam_config(void);
DISP_PQ_PARAM *get_Color_Gal_config(void);
DISPLAY_PQ_T *get_Color_index(void);
extern DISPLAY_TDSHP_T *get_TDSHP_index(void);

void disp_color_set_window(unsigned int sat_upper, unsigned int sat_lower,
			   unsigned int hue_upper, unsigned int hue_lower);
#endif
