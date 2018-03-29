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

#ifndef VENC_DRV_IF_DEP_H
#define VENC_DRV_IF_DEP_H

/*=============================================================================
 *                              Include Files
 *===========================================================================*/

#include "val_types_private.h"
#include "vcodec_if_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *                              Type definition
 *===========================================================================*/

/**
 * @par Structure
 *   mhalVdoDrv_t
 * @par Description
 *   This is a structure which store common video enc driver information
 */
typedef struct mhalVdoDrv_s {
	VAL_VOID_T                      *prCodecHandle;
	VAL_UINT32_T                    u4EncodedFrameCount;
	VCODEC_ENC_CALLBACK_T           rCodecCb;
	VIDEO_ENC_API_T                 *prCodecAPI;
	VENC_BS_T                       pBSBUF;

	VCODEC_ENC_BUFFER_INFO_T        EncoderInputParamNC;
	VENC_DRV_PARAM_BS_BUF_T         BSout;
	VENC_HYBRID_ENCSETTING          rVencSetting;
	VAL_UINT8_T                     *ptr;
} mhalVdoDrv_t;

typedef struct __VENC_HANDLE_T {
	VAL_HANDLE_T            hHalHandle;    /* /< HAL data. */
	VAL_HANDLE_T            vdriver_Handle;       /* /< for MMSYS power on/off */
	VAL_MEMORY_T            rHandleMem;    /* /< Save handle memory information to be used in release. */
	VAL_BOOL_T              bFirstDecoded; /* / < already pass first video data to codec */
	VAL_BOOL_T              bHeaderPassed; /* / < already pass video header to codec */
	VAL_BOOL_T              bFlushAll;
	VAL_MEMORY_T            HeaderBuf;
	VAL_HANDLE_T            hCodec;
	/* DRIVER_HANDLER_T        hDrv; */
	VAL_UINT32_T            CustomSetting;
	VCODEC_MEMORY_TYPE_T    rVideoDecMemType;
	VAL_UINT32_T            nYUVBufferIndex;
	VCODEC_OPEN_SETTING_T   codecOpenSetting;

	mhalVdoDrv_t            rMhalVdoDrv;
	VAL_MEMORY_T            bs_driver_workingmem;

	/* Morris Yang 20110411 [ */
	VENC_DRV_VIDEO_FORMAT_T CodecFormat;
	VAL_VOID_T              *prExtraData;  /* /< Driver private data pointer. */
	VAL_MEMORY_T             rExtraDataMem; /* /< Save extra data memory information to be used in release. */
	/* ] */
	VAL_UINT32_T  nOmxTids;
#if 1   /* defined(MT6572)     //VCODEC_MULTI_THREAD */
	/* Jackal Chen [ */
	VAL_VOID_T              *pDrvModule;    /* /< used for dlopen and dlclose */
	/* ] */
#endif
	VIDEO_ENC_WRAP_HANDLE_T hWrapper;
} VENC_HANDLE_T;


#ifdef __cplusplus
}
#endif

#endif /* VENC_DRV_IF_DEP_H */
