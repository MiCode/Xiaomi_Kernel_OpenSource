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

#ifndef _VENC_DRV_IF_PRIVATE_H_
#define _VENC_DRV_IF_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_private.h"
#include "venc_drv_if_public.h"


typedef enum __VENC_DRV_COLOR_FORMAT_T {
	VENC_DRV_COLOR_FORMAT_YUV420,
	VENC_DRV_COLOR_FORMAT_YV12,
}
VENC_DRV_COLOR_FORMAT_T;


typedef struct __VENC_DRV_YUV_STRIDE_T {
	unsigned int    u4YStride;
	unsigned int    u4UVStride;
} VENC_DRV_YUV_STRIDE_T;


/**
 * @par Structure
 *   VENC_DRV_PARAM_EIS_T
 * @par Description
 *   This is the EIS information and used as input or output parameter for\n
 *   eVEncDrvSetParam() or eVEncDrvGetParam()\n
 */
typedef struct __VENC_DRV_PARAM_EIS_T {
	VAL_BOOL_T      fgEISEnable;            /* /<: EIS Enable/disable. */
	VAL_UINT32_T    u4EISFrameWidth;        /* /<: EIS FrameWidth */
	VAL_UINT32_T    u4EISFrameHeight;       /* /<: EIS FrameHeight */
	VAL_UINT32_T    u4GMV_X;                /* /<: Golbal Motion Vector (GMV) of the VOP Frame used for EIS */
	VAL_UINT32_T    u4GMV_Y;                /* /<: Golbal Motion Vector (GMV) of the VOP Frame used for EIS */
} VENC_DRV_PARAM_EIS_T;

/**
 * @par Structure
 *   P_VENC_DRV_PARAM_EIS_T
 * @par Description
 *   This is the pointer of VENC_DRV_PARAM_EIS_T
 */
typedef VENC_DRV_PARAM_EIS_T * P_VENC_DRV_PARAM_EIS_T;


/**
 * @par Structure
 *   VENC_DRV_STATISTIC_T
 * @par Description
 *   This is statistic information and used as output parameter for\n
 *   eVEncDrvGetParam()\n
 */
typedef struct __VENC_DRV_STATISTIC_T {
	VAL_UINT32_T    u4EncTimeMax;   /* /<: Encode one frame time. Max */
	VAL_UINT32_T    u4EncTimeMin;   /* /<: Encode one frame time. Min */
	VAL_UINT32_T    u4EncTimeAvg;   /* /<: Encode one frame time. Average */
	VAL_UINT32_T    u4EncTimeSum;   /* /<: Encode one frame time. Sum */
} VENC_DRV_STATISTIC_T;

/**
 * @par Structure
 *   P_VENC_DRV_STATISTIC_T
 * @par Description
 *   This is the pointer of VENC_DRV_STATISTIC_T
 */
typedef VENC_DRV_STATISTIC_T * P_VENC_DRV_STATISTIC_T;


typedef struct __VENC_HYB_ENCSETTING {

	/* used in SetParameter */
	VAL_UINT32_T    u4Width;
	VAL_UINT32_T    u4Height;
	VAL_UINT32_T    u4IntraVOPRate;     /* u4NumPFrm; */
	VAL_UINT32_T    eFrameRate;
	VAL_UINT32_T    u4VEncBitrate;
	VAL_UINT32_T    u4QualityLevel;
	VAL_UINT32_T    u4ShortHeaderMode;
	VAL_UINT32_T    u4CodecType;        /* mepg4, h263, h264... */
	VAL_UINT32_T    u4RotateAngle;

	/* used in QueryFunctions */
	VENC_DRV_COLOR_FORMAT_T     eVEncFormat;    /* YUV420, I420 ..... */
	VENC_DRV_YUV_STRIDE_T       rVCodecYUVStride;
	VAL_UINT32_T    u4Profile;
	VAL_UINT32_T    u4Level;
	VAL_UINT32_T    u4BufWidth;
	VAL_UINT32_T    u4BufHeight;
	VAL_UINT32_T    u4NumBFrm;
	VAL_UINT32_T    fgInterlace;

	/* used in Query */
	VAL_UINT32_T    u4InitQ;
	VAL_UINT32_T    u4MinQ;
	VAL_UINT32_T    u4MaxQ;
	VAL_UINT32_T    u4Algorithm;
	VAL_UINT32_T    u4_Rate_Hard_Limit;
	VAL_UINT32_T    u4RateBalance;
	VAL_UINT32_T    u4ForceIntraEnable;
	VAL_UINT32_T    u4VEncMinBitrate;   /* Min bit-rate */

	/* hardware dependent function settings */
	VAL_BOOL_T      fgUseMCI;
	VAL_UINT32_T    u4VEncThreadNum;
} VENC_HYBRID_ENCSETTING;


typedef struct VENC_BS_s {
	VAL_UINT8_T                     *u4BS_addr;
	VAL_UINT8_T                     *u4BS_addr_PA;
	VAL_UINT32_T                    u4BSSize;
	VAL_UINT32_T                    u4BS_frmSize;
	VAL_UINT32_T                    u4BS_frmCount;
	VAL_UINT32_T                    u4BS_index;
	VAL_UINT32_T                    u4BS_preindex;
	VAL_UINT32_T                    u4Fillcnt;
	VAL_UINT32_T                    Handle;
} VENC_BS_T;


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VENC_DRV_IF_PRIVATE_H_ */
