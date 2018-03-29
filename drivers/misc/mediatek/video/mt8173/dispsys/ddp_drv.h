/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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


#ifndef __DDP_DRV_H__
#define __DDP_DRV_H__
#include <linux/ioctl.h>
#include "ddp_hal.h"
#include "ddp_aal.h"
#include "ddp_gamma.h"
#include "disp_event.h"
/*#include "../videox/DpDataType.h"*/


typedef struct {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
} DISP_WRITE_REG;

typedef struct {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
} DISP_READ_REG;

#if 0
typedef struct {
	DISP_MODULE_ENUM module;
	unsigned int timeout_ms;	/* timeout, unit is ms */
} disp_wait_irq_struct;
#endif

typedef struct DISP_EXEC_COMMAND {
	int taskID;
	uint32_t scenario;
	uint32_t priority;
	uint32_t engineFlag;
	uint32_t *pFrameBaseSW;
	uint32_t *pTileBaseSW;
	uint32_t blockSize;
} DISP_EXEC_COMMAND;

typedef struct {
	int layer;

	unsigned long addr;
	unsigned int fmt;

	int x;
	int y;
	int w;
	int h;			/* clip region */
	int pitch;
} DISP_OVL_INFO;

/* PQ */
#define COLOR_TUNING_INDEX 19
#define THSHP_TUNING_INDEX 12
#define THSHP_PARAM_MAX 83
#define PARTIAL_Y_INDEX 10


#define GLOBAL_SAT_SIZE 10
#define CONTRAST_SIZE 10
#define BRIGHTNESS_SIZE 10
#define PARTIAL_Y_SIZE 16
#define PQ_HUE_ADJ_PHASE_CNT 4
#define PQ_SAT_ADJ_PHASE_CNT 4
#define PQ_PARTIALS_CONTROL 5
#define PURP_TONE_SIZE 3
#define SKIN_TONE_SIZE 8	/* (-6) */
#define GRASS_TONE_SIZE 6	/* (-2) */
#define SKY_TONE_SIZE 3
#define CCORR_COEF_CNT 4 /* ccorr feature */

enum TONE_ENUM {
	PURP_TONE = 0,
	SKIN_TONE = 1,
	GRASS_TONE = 2,
	SKY_TONE = 3
};

typedef struct {
	unsigned int u4SHPGain;	/* 0 : min , 9 : max. */
	unsigned int u4SatGain;	/* 0 : min , 9 : max. */
	unsigned int u4PartialY;   /* 0 : min , 9 : max. */
	unsigned int u4HueAdj[PQ_HUE_ADJ_PHASE_CNT];
	unsigned int u4SatAdj[PQ_SAT_ADJ_PHASE_CNT];
	unsigned int u4Contrast;	/* 0 : min , 9 : max. */
	unsigned int u4Brightness;	/* 0 : min , 9 : max. */
	unsigned int u4Ccorr;      /* 0 : min , 3 : max. ccorr feature */
} DISP_PQ_PARAM;

typedef struct {
	int split_en;
	int start_x;
	int start_y;
	int end_x;
	int end_y;
} DISP_PQ_WIN_PARAM;

typedef struct {
	int image;
	int video;
	int camera;
} DISP_PQ_MAPPING_PARAM;

typedef struct {

	unsigned int GLOBAL_SAT[GLOBAL_SAT_SIZE];
	unsigned int CONTRAST[CONTRAST_SIZE];
	unsigned int BRIGHTNESS[BRIGHTNESS_SIZE];
	unsigned int PARTIAL_Y[PARTIAL_Y_INDEX][PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[COLOR_TUNING_INDEX][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[COLOR_TUNING_INDEX][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[COLOR_TUNING_INDEX][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[COLOR_TUNING_INDEX][SKY_TONE_SIZE];
	unsigned int CCORR_COEF[CCORR_COEF_CNT][3][3];

} DISPLAY_PQ_T;

typedef struct {
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
} DISPLAY_COLOR_REG_T;

typedef struct {

	unsigned int entry[THSHP_TUNING_INDEX][THSHP_PARAM_MAX];

} DISPLAY_TDSHP_T;


typedef enum {
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
	ProtectRegionWeight
} PQ_DC_index_t;

typedef struct {

	int param[39];
	/*
	   int BlackEffectEnable;
	   int WhiteEffectEnable;
	   int StrongBlackEffect;
	   int StrongWhiteEffect;
	   int AdaptiveBlackEffect;
	   int AdaptiveWhiteEffect;
	   int ScenceChangeOnceEn;
	   int ScenceChangeControlEn;
	   int ScenceChangeControl;
	   int ScenceChangeTh1;
	   int ScenceChangeTh2;
	   int ScenceChangeTh3;
	   int ContentSmooth1;
	   int ContentSmooth2;
	   int ContentSmooth3;
	   int MiddleRegionGain1;
	   int MiddleRegionGain2;
	   int BlackRegionGain1;
	   int BlackRegionGain2;
	   int BlackRegionRange;
	   int BlackEffectLevel;
	   int BlackEffectParam1;
	   int BlackEffectParam2;
	   int BlackEffectParam3;
	   int BlackEffectParam4;
	   int WhiteRegionGain1;
	   int WhiteRegionGain2;
	   int WhiteRegionRange;
	   int WhiteRegionLevel;
	   int WhiteEffectParam1;
	   int WhiteEffectParam2;
	   int WhiteEffectParam3;
	   int WhiteEffectParam4;
	   int ContrastAdjust1;
	   int ContrastAdjust2;
	   int DCChangeSpeedLevel;
	   int ProtectRegionEffect;
	   int DCChangeSpeedLevel2;
	   int ProtectRegionWeight;
	 */
} DISP_PQ_DC_PARAM;


/* OD */
typedef struct {
	unsigned int size;
	unsigned int type;
	unsigned int ret;
	unsigned long param0;
	unsigned long param1;
	unsigned long param2;
	unsigned long param3;
} DISP_OD_CMD;

typedef enum {
	DISP_INTERLACE_FORMAT_NONE,
	DISP_INTERLACE_FORMAT_TOP_FIELD,
	DISP_INTERLACE_FORMAT_BOTTOM_FIELD
} DISP_INTERLACE_FORMAT;

#define DISP_IOCTL_MAGIC        'x'

#define DISP_IOCTL_WRITE_REG       _IOW(DISP_IOCTL_MAGIC, 1, DISP_WRITE_REG)	/* also defined in acti_pq_cmd.h */
#define DISP_IOCTL_READ_REG        _IOWR(DISP_IOCTL_MAGIC, 2, DISP_READ_REG)	/* also defined in acti_pq_cmd.h */
/* #define DISP_IOCTL_WAIT_IRQ        _IOR     (DISP_IOCTL_MAGIC, 3, disp_wait_irq_struct) */
#define DISP_IOCTL_DUMP_REG        _IOR(DISP_IOCTL_MAGIC, 4, int)
#define DISP_IOCTL_LOCK_THREAD     _IOR(DISP_IOCTL_MAGIC, 5, int)
#define DISP_IOCTL_UNLOCK_THREAD   _IOR(DISP_IOCTL_MAGIC, 6, int)
#define DISP_IOCTL_MARK_CMQ        _IOR(DISP_IOCTL_MAGIC, 7, int)
#define DISP_IOCTL_WAIT_CMQ        _IOR(DISP_IOCTL_MAGIC, 8, int)
#define DISP_IOCTL_SYNC_REG        _IOR(DISP_IOCTL_MAGIC, 9, int)

#define DISP_IOCTL_LOCK_MUTEX      _IOW(DISP_IOCTL_MAGIC, 20, int)
#define DISP_IOCTL_UNLOCK_MUTEX    _IOR(DISP_IOCTL_MAGIC, 21, int)

#define DISP_IOCTL_LOCK_RESOURCE   _IOW(DISP_IOCTL_MAGIC, 25, int)
#define DISP_IOCTL_UNLOCK_RESOURCE _IOR(DISP_IOCTL_MAGIC, 26, int)

#define DISP_IOCTL_SET_INTR        _IOR(DISP_IOCTL_MAGIC, 10, int)
#define DISP_IOCTL_TEST_PATH       _IOR(DISP_IOCTL_MAGIC, 11, int)

#define DISP_IOCTL_CLOCK_ON        _IOR(DISP_IOCTL_MAGIC, 12, int)
#define DISP_IOCTL_CLOCK_OFF       _IOR(DISP_IOCTL_MAGIC, 13, int)

#define DISP_IOCTL_RUN_DPF         _IOW(DISP_IOCTL_MAGIC, 30, int)
#define DISP_IOCTL_CHECK_OVL       _IOR(DISP_IOCTL_MAGIC, 31, int)
#define DISP_IOCTL_GET_OVL         _IOWR(DISP_IOCTL_MAGIC, 32, DISP_OVL_INFO)

#define DISP_IOCTL_EXEC_COMMAND    _IOW(DISP_IOCTL_MAGIC, 33, DISP_EXEC_COMMAND)
#define DISP_IOCTL_RESOURCE_REQUIRE   _IOR(DISP_IOCTL_MAGIC, 34, int)

/* Add for AAL control - S */
/* 0 : disable AAL event, 1 : enable AAL event */
#define DISP_IOCTL_AAL_EVENTCTL    _IOW(DISP_IOCTL_MAGIC, 15 , int)
/* Get AAL statistics data. */
#define DISP_IOCTL_AAL_GET_HIST    _IOR(DISP_IOCTL_MAGIC, 16 , DISP_AAL_HIST)
/* Update AAL setting */
#define DISP_IOCTL_AAL_SET_PARAM   _IOW(DISP_IOCTL_MAGIC, 17 , DISP_AAL_PARAM)
#define DISP_IOCTL_AAL_INIT_REG    _IOW(DISP_IOCTL_MAGIC, 18 , DISP_AAL_INITREG)
#define DISP_IOCTL_SET_GAMMALUT    _IOW(DISP_IOCTL_MAGIC, 23 , DISP_GAMMA_LUT_T)
#define DISP_IOCTL_SET_CCORR       _IOW(DISP_IOCTL_MAGIC, 24 , DISP_CCORR_COEF_T)


/* Add for AAL control - E */
/*-----------------------------------------------------------------------------
    DDP Kernel Mode API  (for Kernel Trap)
  -----------------------------------------------------------------------------*/
/* DDPK Bitblit */
/* #define DISP_IOCTL_G_WAIT_REQUEST  _IOR     (DISP_IOCTL_MAGIC , 40 , DDPIOCTL_DdpkBitbltConfig) */
/* #define DISP_IOCTL_T_INFORM_DONE   _IOW     (DISP_IOCTL_MAGIC , 41 , DDPIOCTL_DdpkBitbltInformDone) */

#define DISP_IOCTL_SET_CLKON        _IOW(DISP_IOCTL_MAGIC, 50 , DISP_MODULE_ENUM)
#define DISP_IOCTL_SET_CLKOFF       _IOW(DISP_IOCTL_MAGIC, 51 , DISP_MODULE_ENUM)

#define DISP_IOCTL_MUTEX_CONTROL    _IOW(DISP_IOCTL_MAGIC, 55 , int)	/* also defined in acti_pq_cmd.h */
#define DISP_IOCTL_GET_LCMINDEX     _IOR(DISP_IOCTL_MAGIC, 56 , int)

/* PQ setting */
#define DISP_IOCTL_SET_PQPARAM      _IOW(DISP_IOCTL_MAGIC, 60 , DISP_PQ_PARAM)
#define DISP_IOCTL_SET_C0_PQPARAM   _IOW(DISP_IOCTL_MAGIC, 60 , DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQPARAM      _IOR(DISP_IOCTL_MAGIC, 61 , DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQINDEX      _IOR(DISP_IOCTL_MAGIC, 63,  DISPLAY_PQ_T)
#define DISP_IOCTL_GET_C0_PQPARAM   _IOR(DISP_IOCTL_MAGIC, 61 , DISP_PQ_PARAM)
#define DISP_IOCTL_SET_C1_PQPARAM   _IOW(DISP_IOCTL_MAGIC, 62 , DISP_PQ_PARAM)
#define DISP_IOCTL_GET_C1_PQPARAM   _IOR(DISP_IOCTL_MAGIC, 63 , DISP_PQ_PARAM)

#define DISP_IOCTL_SET_PQINDEX      _IOW(DISP_IOCTL_MAGIC, 64 , DISPLAY_PQ_T)
#define DISP_IOCTL_SET_TDSHPINDEX   _IOW(DISP_IOCTL_MAGIC, 65 , DISPLAY_TDSHP_T)
#define DISP_IOCTL_GET_TDSHPINDEX   _IOR(DISP_IOCTL_MAGIC, 66 , DISPLAY_TDSHP_T)
#define DISP_IOCTL_SET_PQ_CAM_PARAM _IOW(DISP_IOCTL_MAGIC, 67 , DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQ_CAM_PARAM _IOR(DISP_IOCTL_MAGIC, 68 , DISP_PQ_PARAM)
#define DISP_IOCTL_SET_PQ_GAL_PARAM _IOW(DISP_IOCTL_MAGIC, 69 , DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQ_GAL_PARAM _IOR(DISP_IOCTL_MAGIC, 70 , DISP_PQ_PARAM)

#define DISP_IOCTL_PQ_SET_BYPASS_COLOR  _IOW(DISP_IOCTL_MAGIC, 71 , int)
#define DISP_IOCTL_PQ_SET_WINDOW    _IOW(DISP_IOCTL_MAGIC, 72 , DISP_PQ_WIN_PARAM)
#define DISP_IOCTL_PQ_GET_TDSHP_FLAG    _IOR(DISP_IOCTL_MAGIC, 73 , int)
#define DISP_IOCTL_PQ_SET_TDSHP_FLAG    _IOW(DISP_IOCTL_MAGIC, 74 , int)
#define DISP_IOCTL_PQ_GET_DC_PARAM  _IOR(DISP_IOCTL_MAGIC, 75 , DISP_PQ_DC_PARAM)
#define DISP_IOCTL_PQ_SET_DC_PARAM  _IOW(DISP_IOCTL_MAGIC, 76 , DISP_PQ_DC_PARAM)
#define DISP_IOCTL_WRITE_SW_REG     _IOW(DISP_IOCTL_MAGIC, 77, DISP_WRITE_REG)	/* also defined in atci_pq_cmd.h */
#define DISP_IOCTL_READ_SW_REG      _IOWR(DISP_IOCTL_MAGIC, 78, DISP_READ_REG)	/* also defined in atci_pq_cmd.h */

#define DISP_IOCTL_SET_COLOR_REG    _IOWR(DISP_IOCTL_MAGIC, 79, DISPLAY_COLOR_REG_T)

/* OD */
#define DISP_IOCTL_OD_CTL           _IOWR(DISP_IOCTL_MAGIC, 80 , DISP_OD_CMD)

#define DISP_IOCTL_SET_PANEL_PARAM  _IOW(DISP_IOCTL_MAGIC, 81, DISP_PQ_DC_PARAM)


#endif
