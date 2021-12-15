/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#ifndef __DDP_DRV_H__
#define __DDP_DRV_H__
#include <linux/ioctl.h>
#include "ddp_hal.h"
#include "ddp_info.h"
#include "ddp_aal.h"
#include "ddp_gamma.h"
#include "ddp_pq.h"

struct DISP_WRITE_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct DISP_READ_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct DISP_EXEC_COMMAND {
	int taskID;
	uint32_t scenario;
	uint32_t priority;
	uint32_t engineFlag;
	uint32_t *pFrameBaseSW;
	uint32_t *pTileBaseSW;
	uint32_t blockSize;
};

#if 0
/* PQ */
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
#define SKIN_TONE_SIZE 8	/* (-6) */
#define GRASS_TONE_SIZE 6	/* (-2) */
#define SKY_TONE_SIZE 3
#define CCORR_COEF_CNT 4 /* ccorr feature */

struct DISP_PQ_PARAM {
	unsigned int u4SHPGain;	/* 0 : min , 9 : max. */
	unsigned int u4SatGain;	/* 0 : min , 9 : max. */
	unsigned int u4PartialY; /* 0 : min , 9 : max. */
	unsigned int u4HueAdj[PQ_HUE_ADJ_PHASE_CNT];
	unsigned int u4SatAdj[PQ_SAT_ADJ_PHASE_CNT];
	unsigned int u4Contrast; /* 0 : min , 9 : max. */
	unsigned int u4Brightness; /* 0 : min , 9 : max. */
	unsigned int u4Ccorr; /* 0 : min , 3 : max.  ccorr feature */
};

struct DISP_PQ_WIN_PARAM {
	int split_en;
	int start_x;
	int start_y;
	int end_x;
	int end_y;
};

struct DISP_PQ_MAPPING_PARAM {
	int image;
	int video;
	int camera;
};

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

/* TODO need to refine naming size */
struct DISPLAY_PQ_T {
	unsigned int GLOBAL_SAT[GLOBAL_SAT_SIZE];
	unsigned int CONTRAST[CONTRAST_SIZE];
	unsigned int BRIGHTNESS[BRIGHTNESS_SIZE];
	unsigned int PARTIAL_Y[PARTIAL_Y_INDEX][PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[19][PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[19][PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[19][PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[19][PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[19][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[19][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[19][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[19][SKY_TONE_SIZE];
	unsigned int  CCORR_COEF[CCORR_COEF_CNT][3][3];
};

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
};


struct DISPLAY_TDSHP_T {

	unsigned int entry[THSHP_TUNING_INDEX][THSHP_PARAM_MAX];

};

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
	DCEnable
};

struct DISP_PQ_DC_PARAM {
	int param[40];
};


/* OD */
struct DISP_OD_CMD {
	unsigned int size;
	unsigned int type;
	unsigned int ret;
	unsigned int param0;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
};
#endif

enum DISP_INTERLACE_FORMAT {
	DISP_INTERLACE_FORMAT_NONE,
	DISP_INTERLACE_FORMAT_TOP_FIELD,
	DISP_INTERLACE_FORMAT_BOTTOM_FIELD
};


struct device *disp_get_device(void);
#ifdef CONFIG_MTK_IOMMU_V3
#define DISP_LARB_COUNT 1
struct disp_iommu_device {
	struct platform_device *larb_pdev[DISP_LARB_COUNT];
	struct platform_device *iommu_pdev;
	unsigned int inited;
};

struct disp_iommu_device *disp_get_iommu_dev(void);
#endif



#define DISP_IOCTL_MAGIC        'x'

/* also defined in atci_pq_cmd.h */
#define DISP_IOCTL_WRITE_REG \
	_IOW(DISP_IOCTL_MAGIC, 1, struct DISP_WRITE_REG)
#define DISP_IOCTL_READ_REG \
	_IOWR(DISP_IOCTL_MAGIC, 2, struct DISP_READ_REG)
#define DISP_IOCTL_DUMP_REG \
	_IOR(DISP_IOCTL_MAGIC, 4, int)
#define DISP_IOCTL_LOCK_THREAD  \
	_IOR(DISP_IOCTL_MAGIC, 5, int)
#define DISP_IOCTL_UNLOCK_THREAD \
	_IOR(DISP_IOCTL_MAGIC, 6, int)
#define DISP_IOCTL_MARK_CMQ \
	_IOR(DISP_IOCTL_MAGIC, 7, int)
#define DISP_IOCTL_WAIT_CMQ \
	_IOR(DISP_IOCTL_MAGIC, 8, int)
#define DISP_IOCTL_SYNC_REG \
	_IOR(DISP_IOCTL_MAGIC, 9, int)

#define DISP_IOCTL_LOCK_MUTEX \
	_IOW(DISP_IOCTL_MAGIC, 20, int)
#define DISP_IOCTL_UNLOCK_MUTEX \
	_IOR(DISP_IOCTL_MAGIC, 21, int)

#define DISP_IOCTL_LOCK_RESOURCE \
	_IOW(DISP_IOCTL_MAGIC, 25, int)
#define DISP_IOCTL_UNLOCK_RESOURCE \
	_IOR(DISP_IOCTL_MAGIC, 26, int)

#define DISP_IOCTL_SET_INTR \
	_IOR(DISP_IOCTL_MAGIC, 10, int)
#define DISP_IOCTL_TEST_PATH \
	_IOR(DISP_IOCTL_MAGIC, 11, int)

#define DISP_IOCTL_CLOCK_ON \
	_IOR(DISP_IOCTL_MAGIC, 12, int)
#define DISP_IOCTL_CLOCK_OFF \
	_IOR(DISP_IOCTL_MAGIC, 13, int)

#define DISP_IOCTL_RUN_DPF \
	_IOW(DISP_IOCTL_MAGIC, 30, int)
#define DISP_IOCTL_CHECK_OVL \
	_IOR(DISP_IOCTL_MAGIC, 31, int)

#define DISP_IOCTL_EXEC_COMMAND \
	_IOW(DISP_IOCTL_MAGIC, 33, struct DISP_EXEC_COMMAND)
#define DISP_IOCTL_RESOURCE_REQUIRE \
	_IOR(DISP_IOCTL_MAGIC, 34, int)

/* Add for AAL control - S */
/* 0 : disable AAL event, 1 : enable AAL event */
#define DISP_IOCTL_AAL_EVENTCTL \
	_IOW(DISP_IOCTL_MAGIC, 15, int)
/* Get AAL statistics data. */
#define DISP_IOCTL_AAL_GET_HIST \
	_IOR(DISP_IOCTL_MAGIC, 16, struct DISP_AAL_HIST)
/* Update AAL setting */
#define DISP_IOCTL_AAL_SET_PARAM \
	_IOW(DISP_IOCTL_MAGIC, 17, struct DISP_AAL_PARAM)
#define DISP_IOCTL_AAL_INIT_REG \
	_IOW(DISP_IOCTL_MAGIC, 18, struct DISP_AAL_INITREG)
#define DISP_IOCTL_SET_SMARTBACKLIGHT \
	_IOW(DISP_IOCTL_MAGIC, 19, int)
/* Add for AAL control - E */

#define DISP_IOCTL_SET_GAMMALUT \
	_IOW(DISP_IOCTL_MAGIC, 23, struct DISP_GAMMA_LUT_T)
#define DISP_IOCTL_SET_CCORR \
	_IOW(DISP_IOCTL_MAGIC, 24, struct DISP_CCORR_COEF_T)

/* Add for PQ transition control */
/* 0 : disable CCORR event, 1 : enable CCORR event */
#define DISP_IOCTL_CCORR_EVENTCTL \
	_IOW(DISP_IOCTL_MAGIC, 110, int)
/* Get CCORR interrupt */
#define DISP_IOCTL_CCORR_GET_IRQ \
	_IOR(DISP_IOCTL_MAGIC, 111, int)
/* Get color transform support */
#define DISP_IOCTL_SUPPORT_COLOR_TRANSFORM \
	_IOW(DISP_IOCTL_MAGIC, 112, struct DISP_COLOR_TRANSFORM)

/*---------------------------------------------------------------------------*/
/*  DDP Kernel Mode API  (for Kernel Trap) */
/* --------------------------------------------------------------------------*/
/* DDPK Bitblit */

#define DISP_IOCTL_SET_CLKON \
	_IOW(DISP_IOCTL_MAGIC, 50, enum DISP_MODULE_ENUM)
#define DISP_IOCTL_SET_CLKOFF \
	_IOW(DISP_IOCTL_MAGIC, 51, enum DISP_MODULE_ENUM)

#define DISP_IOCTL_MUTEX_CONTROL \
	_IOW(DISP_IOCTL_MAGIC, 55, int)	/* also defined in atci_pq_cmd.h */
#define DISP_IOCTL_GET_LCMINDEX \
	_IOR(DISP_IOCTL_MAGIC, 56, int)

/* PQ setting */
#define DISP_IOCTL_SET_PQPARAM \
	_IOW(DISP_IOCTL_MAGIC, 60, struct DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQPARAM \
	_IOR(DISP_IOCTL_MAGIC, 61, struct DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQINDEX \
	_IOR(DISP_IOCTL_MAGIC, 63, struct DISPLAY_PQ_T)
#define DISP_IOCTL_SET_PQINDEX \
	_IOW(DISP_IOCTL_MAGIC, 64, struct DISPLAY_PQ_T)
#define DISP_IOCTL_SET_TDSHPINDEX \
	_IOW(DISP_IOCTL_MAGIC, 65, struct DISPLAY_TDSHP_T)
#define DISP_IOCTL_GET_TDSHPINDEX \
	_IOR(DISP_IOCTL_MAGIC, 66, struct DISPLAY_TDSHP_T)
#define DISP_IOCTL_SET_PQ_CAM_PARAM \
	_IOW(DISP_IOCTL_MAGIC, 67, struct DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQ_CAM_PARAM \
	_IOR(DISP_IOCTL_MAGIC, 68, struct DISP_PQ_PARAM)
#define DISP_IOCTL_SET_PQ_GAL_PARAM \
	_IOW(DISP_IOCTL_MAGIC, 69, struct DISP_PQ_PARAM)
#define DISP_IOCTL_GET_PQ_GAL_PARAM \
	_IOR(DISP_IOCTL_MAGIC, 70, struct DISP_PQ_PARAM)

#define DISP_IOCTL_PQ_SET_BYPASS_COLOR \
	_IOW(DISP_IOCTL_MAGIC, 71, int)
#define DISP_IOCTL_PQ_SET_WINDOW \
	_IOW(DISP_IOCTL_MAGIC, 72, struct DISP_PQ_WIN_PARAM)
#define DISP_IOCTL_PQ_GET_TDSHP_FLAG \
	_IOR(DISP_IOCTL_MAGIC, 73, int)
#define DISP_IOCTL_PQ_SET_TDSHP_FLAG \
	_IOW(DISP_IOCTL_MAGIC, 74, int)
#define DISP_IOCTL_PQ_GET_DC_PARAM \
	_IOR(DISP_IOCTL_MAGIC, 75, struct DISP_PQ_DC_PARAM)
#define DISP_IOCTL_PQ_SET_DC_PARAM \
	_IOW(DISP_IOCTL_MAGIC, 76, struct DISP_PQ_DC_PARAM)
#define DISP_IOCTL_WRITE_SW_REG \
	_IOW(DISP_IOCTL_MAGIC, 77, struct DISP_WRITE_REG)
#define DISP_IOCTL_READ_SW_REG \
	_IOWR(DISP_IOCTL_MAGIC, 78, struct DISP_READ_REG)
#define DISP_IOCTL_SET_COLOR_REG \
	_IOWR(DISP_IOCTL_MAGIC, 79, struct DISPLAY_COLOR_REG)

/* OD */
#define DISP_IOCTL_OD_CTL \
	_IOWR(DISP_IOCTL_MAGIC, 80, struct DISP_OD_CMD)

/* OVL */
#define DISP_IOCTL_OVL_ENABLE_CASCADE \
	_IOW(DISP_IOCTL_MAGIC, 90, int)
#define DISP_IOCTL_OVL_DISABLE_CASCADE \
	_IOW(DISP_IOCTL_MAGIC, 91, int)

/*PQ setting*/
#define DISP_IOCTL_PQ_GET_DS_PARAM \
	_IOR(DISP_IOCTL_MAGIC, 100, struct DISP_PQ_DS_PARAM)
#define DISP_IOCTL_PQ_GET_MDP_COLOR_CAP \
	_IOR(DISP_IOCTL_MAGIC, 101, struct MDP_COLOR_CAP)
#define DISP_IOCTL_PQ_GET_MDP_TDSHP_REG \
	_IOR(DISP_IOCTL_MAGIC, 102, struct MDP_TDSHP_REG)

/*secure video path implementation: the handle value*/
#define DISP_IOCTL_SET_TPLAY_HANDLE \
	_IOW(DISP_IOCTL_MAGIC, 200, unsigned int)

int disp_get_ovl_bandwidth(unsigned long long in_fps,
			unsigned long long out_fps,
			unsigned long long *bandwidth);
int disp_get_rdma_bandwidth(unsigned long long out_fps,
			unsigned long long *bandwidth);

#endif
