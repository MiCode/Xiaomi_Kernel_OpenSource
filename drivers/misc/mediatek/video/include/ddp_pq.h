/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DDP_PQ_H__
#define __DDP_PQ_H__

/* PQ */
#define C_TUN_IDX 19 /* COLOR_TUNING_INDEX */
#define COLOR_TUNING_INDEX 19
#define THSHP_TUNING_INDEX 12
#define THSHP_PARAM_MAX 146 /* TDSHP_3_0 */
#define PARTIAL_Y_INDEX 10
#define GLOBAL_SAT_SIZE 10
#define CONTRAST_SIZE 10
#define BRIGHTNESS_SIZE 10
#define PARTIAL_Y_SIZE 16
#define PQ_HUE_ADJ_PHASE_CNT 4
#define PQ_SAT_ADJ_PHASE_CNT 4
#define PQ_PARTIALS_CONTROL 5
#define PURP_TONE_SIZE 3
#define SKIN_TONE_SIZE 8  /* (-6) */
#define GRASS_TONE_SIZE 6 /* (-2) */
#define SKY_TONE_SIZE 3
#define CCORR_COEF_CNT 4 /* ccorr feature */
#define S_GAIN_BY_Y_CONTROL_CNT 5
#define S_GAIN_BY_Y_HUE_PHASE_CNT 20
#define LSP_CONTROL_CNT 8
#define COLOR_3D_CNT 4 /* color 3D feature */
#define COLOR_3D_WINDOW_CNT 3
#define COLOR_3D_WINDOW_SIZE 45
#define C_3D_CNT 4 /* COLOR_3D_CNT */
#define C_3D_WINDOW_CNT 3
#define C_3D_WINDOW_SIZE 45
#define DISP_COLOR_TM_MAX 4

enum TONE_ENUM { PURP_TONE = 0, SKIN_TONE = 1, GRASS_TONE = 2, SKY_TONE = 3 };

struct DISP_PQ_PARAM {
	unsigned int u4SHPGain;  /* 0 : min , 9 : max. */
	unsigned int u4SatGain;  /* 0 : min , 9 : max. */
	unsigned int u4PartialY; /* 0 : min , 9 : max. */
	unsigned int u4HueAdj[PQ_HUE_ADJ_PHASE_CNT];
	unsigned int u4SatAdj[PQ_SAT_ADJ_PHASE_CNT];
	unsigned int u4Contrast;   /* 0 : min , 9 : max. */
	unsigned int u4Brightness; /* 0 : min , 9 : max. */
	unsigned int u4Ccorr;      /* 0 : min , 3 : max. ccorr feature */
	unsigned int u4ColorLUT; /* 0 : min , 3 : max.  ccorr feature */
};
#define DISP_PQ_PARAM_T struct DISP_PQ_PARAM

struct DISP_PQ_WIN_PARAM {
	int split_en;
	int start_x;
	int start_y;
	int end_x;
	int end_y;
};
#define DISP_PQ_WIN_PARAM_T struct DISP_PQ_WIN_PARAM

struct DISP_PQ_MAPPING_PARAM {
	int image;
	int video;
	int camera;
};

#define DISP_PQ_MAPPING_PARAM_T struct DISP_PQ_MAPPING_PARAM

struct MDP_COLOR_CAP {
	unsigned int en;
	unsigned int pos_x;
	unsigned int pos_y;
};

struct MDP_TDSHP_REG {
	unsigned int TDS_GAIN_MID;
	unsigned int TDS_GAIN_HIGH;
	unsigned int TDS_COR_GAIN;
	unsigned int TDS_COR_THR;
	unsigned int TDS_COR_ZERO;
	unsigned int TDS_GAIN;
	unsigned int TDS_COR_VALUE;
};
struct DISPLAY_PQ_T {

	unsigned int GLOBAL_SAT[GLOBAL_SAT_SIZE];
	unsigned int CONTRAST[CONTRAST_SIZE];
	unsigned int BRIGHTNESS[BRIGHTNESS_SIZE];
	unsigned int PARTIAL_Y[PARTIAL_Y_INDEX][PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[COLOR_TUNING_INDEX]
			       [PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[COLOR_TUNING_INDEX][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[COLOR_TUNING_INDEX][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[COLOR_TUNING_INDEX][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[COLOR_TUNING_INDEX][SKY_TONE_SIZE];
	unsigned int CCORR_COEF[CCORR_COEF_CNT][3][3];
	unsigned int S_GAIN_BY_Y[5][S_GAIN_BY_Y_HUE_PHASE_CNT];
	unsigned int S_GAIN_BY_Y_EN;
	unsigned int LSP_EN;
	unsigned int LSP[LSP_CONTROL_CNT];
	unsigned int COLOR_3D[4][COLOR_3D_WINDOW_CNT][COLOR_3D_WINDOW_SIZE];
};
#define DISPLAY_PQ struct DISPLAY_PQ_T

struct DISPLAY_COLOR_REG {
	unsigned int GLOBAL_SAT;
	unsigned int CONTRAST;
	unsigned int BRIGHTNESS;
	unsigned int PARTIAL_Y[PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[SKY_TONE_SIZE];
	unsigned int S_GAIN_BY_Y[S_GAIN_BY_Y_CONTROL_CNT]
				[S_GAIN_BY_Y_HUE_PHASE_CNT];
	unsigned int S_GAIN_BY_Y_EN;
	unsigned int LSP_EN;
	unsigned int COLOR_3D[COLOR_3D_WINDOW_CNT][COLOR_3D_WINDOW_SIZE];
};
#define DISPLAY_COLOR_REG_T struct DISPLAY_COLOR_REG

struct DISPLAY_TDSHP_T {

	unsigned int entry[THSHP_TUNING_INDEX][THSHP_PARAM_MAX];

};
#define DISPLAY_TDSHP struct DISPLAY_TDSHP_T

enum PQ_DS_index_t {
DS_en = 0,
	iUpSlope,
	iUpThreshold,
	iDownSlope,
	iDownThreshold,
	iISO_en,
	iISO_thr1,
	iISO_thr0,
	iISO_thr3,
	iISO_thr2,
	iISO_IIR_alpha,
	iCorZero_clip2,
	iCorZero_clip1,
	iCorZero_clip0,
	iCorThr_clip2,
	iCorThr_clip1,
	iCorThr_clip0,
	iCorGain_clip2,
	iCorGain_clip1,
	iCorGain_clip0,
	iGain_clip2,
	iGain_clip1,
	iGain_clip0,
	PQ_DS_INDEX_MAX
};

struct DISP_PQ_DS_PARAM {
	int param[PQ_DS_INDEX_MAX];
};
#define DISP_PQ_DS_PARAM_T struct DISP_PQ_DS_PARAM

enum PQ_DC_index_t {
	BlackEffectEnable = 0,
	WhiteEffectEnable,
	StrongBlackEffect,
	StrongWhiteEffect,
	AdaptiveBlackEffect,
	AdaptiveWhiteEffect,
	ScenceChangeOnceEn,
	ScenceChangeControlEn,
	ScenceChangeControl,
	ScenceChangeTh1,
	ScenceChangeTh2,
	ScenceChangeTh3,
	ContentSmooth1,
	ContentSmooth2,
	ContentSmooth3,
	MiddleRegionGain1,
	MiddleRegionGain2,
	BlackRegionGain1,
	BlackRegionGain2,
	BlackRegionRange,
	BlackEffectLevel,
	BlackEffectParam1,
	BlackEffectParam2,
	BlackEffectParam3,
	BlackEffectParam4,
	WhiteRegionGain1,
	WhiteRegionGain2,
	WhiteRegionRange,
	WhiteEffectLevel,
	WhiteEffectParam1,
	WhiteEffectParam2,
	WhiteEffectParam3,
	WhiteEffectParam4,
	ContrastAdjust1,
	ContrastAdjust2,
	DCChangeSpeedLevel,
	ProtectRegionEffect,
	DCChangeSpeedLevel2,
	ProtectRegionWeight,
	DCEnable,
	DarkSceneTh,
	DarkSceneSlope,
	DarkDCGain,
	DarkACGain,
	BinomialTh,
	BinomialSlope,
	BinomialDCGain,
	BinomialACGain,
	BinomialTarRange,
	bIIRCurveDiffSumTh,
	bIIRCurveDiffMaxTh,
	bGlobalPQEn,
	bHistAvoidFlatBgEn,
	PQDC_INDEX_MAX
};
#define PQ_DC_index enum PQ_DC_index_t

struct DISP_PQ_DC_PARAM {
	int param[PQDC_INDEX_MAX];
};

struct DISP_COLOR_TRANSFORM {
	int matrix[DISP_COLOR_TM_MAX][DISP_COLOR_TM_MAX];
};

/* OD */
struct DISP_OD_CMD {
	unsigned int size;
	unsigned int type;
	unsigned int ret;
	unsigned long param0;
	unsigned long param1;
	unsigned long param2;
	unsigned long param3;
};
#endif
