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

#ifndef _HAL_API_H_
#define _HAL_API_H_
#include "val_types_public.h"

#define DumpReg__   /* /< Dump Reg for debug */
#ifdef DumpReg__
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ADD_QUEUE(queue, index, q_type, q_address, q_offset, q_value, q_mask)       \
	{                                                                                   \
		queue[index].type     = q_type;                                                   \
		queue[index].address  = q_address;                                                \
		queue[index].offset   = q_offset;                                                 \
		queue[index].value    = q_value;                                                  \
		queue[index].mask     = q_mask;                                                   \
		index = index + 1;                                                                \
	}       /* /< ADD QUEUE command */


/**
 * @par Enumeration
 *   HAL_CODEC_TYPE_T
 * @par Description
 *   This is the item used for codec type
 */
typedef enum __HAL_CODEC_TYPE_T {
	HAL_CODEC_TYPE_VDEC,                /* /< VDEC */
	HAL_CODEC_TYPE_VENC,                /* /< VENC */
	HAL_CODEC_TYPE_MAX = 0xFFFFFFFF     /* /< MAX Value */
}
HAL_CODEC_TYPE_T;


/**
 * @par Enumeration
 *   HAL_CMD_T
 * @par Description
 *   This is the item used for hal command type
 */
typedef enum _HAL_CMD_T {
	HAL_CMD_SET_CMD_QUEUE,              /* /< set command queue */
	HAL_CMD_SET_POWER,                  /* /< set power */
	HAL_CMD_SET_ISR,                    /* /< set ISR */
	HAL_CMD_GET_CACHE_CTRL_ADDR,        /* /< get cahce control address */
	HAL_CMD_MAX = 0xFFFFFFFF            /* /< MAX value */
} HAL_CMD_T;


/**
 * @par Enumeration
 *   REGISTER_GROUP_T
 * @par Description
 *   This is the item used for register group
 */
typedef enum _REGISTER_GROUP_T {
	VDEC_SYS,           /* /< VDEC_SYS */
	VDEC_MISC,          /* /< VDEC_MISC */
	VDEC_VLD,           /* /< VDEC_VLD */
	VDEC_VLD_TOP,       /* /< VDEC_VLD_TOP */
	VDEC_MC,            /* /< VDEC_MC */
	VDEC_AVC_VLD,       /* /< VDEC_AVC_VLD */
	VDEC_AVC_MV,        /* /< VDEC_AVC_MV */
	VDEC_HEVC_VLD,      /* /< VDEC_HEVC_VLD */
	VDEC_HEVC_MV,       /* /< VDEC_HEVC_MV */
	VDEC_PP,            /* /< VDEC_PP */
	/* VDEC_SQT, */
	VDEC_VP8_VLD,       /* /< VDEC_VP8_VLD */
	VDEC_VP6_VLD,       /* /< VDEC_VP6_VLD */
	VDEC_VP8_VLD2,      /* /< VDEC_VP8_VLD2 */
	VENC_HW_BASE,       /* /< VENC_HW_BASE */
	VENC_MP4_HW_BASE,   /* /< VENC_MP4_HW_BASE */
	VCODEC_MAX          /* /< VCODEC_MAX */
} REGISTER_GROUP_T;


/**
 * @par Enumeration
 *   REGISTER_GROUP_T
 * @par Description
 *   This is the item used for driver command type
 */
typedef enum _VCODEC_DRV_CMD_TYPE {
	ENABLE_HW_CMD,              /* /< ENABLE_HW_CMD */
	DISABLE_HW_CMD,             /* /< DISABLE_HW_CMD */
	WRITE_REG_CMD,              /* /< WRITE_REG_CMD */
	READ_REG_CMD,               /* /< READ_REG_CMD */
	WRITE_SYSRAM_CMD,           /* /< WRITE_SYSRAM_CMD */
	READ_SYSRAM_CMD,            /* /< READ_SYSRAM_CMD */
	MASTER_WRITE_CMD,           /* /< MASTER_WRITE_CMD */
	WRITE_SYSRAM_RANGE_CMD,     /* /< WRITE_SYSRAM_RANGE_CMD */
	READ_SYSRAM_RANGE_CMD,      /* /< READ_SYSRAM_RANGE_CMD */
	SETUP_ISR_CMD,              /* /< SETUP_ISR_CMD */
	WAIT_ISR_CMD,               /* /< WAIT_ISR_CMD */
	TIMEOUT_CMD,                /* /< TIMEOUT_CMD */
	MB_CMD,                     /* /< MB_CMD */
	POLL_REG_STATUS_CMD,        /* /< POLL_REG_STATUS_CMD */
	END_CMD                     /* /< END_CMD */
} VCODEC_DRV_CMD_TYPE;


/**
 * @par Structure
 *  P_VCODEC_DRV_CMD_T
 * @par Description
 *  Pointer of VCODEC_DRV_CMD_T
 */
typedef struct __VCODEC_DRV_CMD_T *P_VCODEC_DRV_CMD_T;


/**
 * @par Structure
 *  VCODEC_DRV_CMD_T
 * @par Description
 *  driver command information
 */
typedef struct __VCODEC_DRV_CMD_T {
	VAL_UINT32_T type;          /* /< type */
	VAL_ULONG_T  address;       /* /< address */
	VAL_ULONG_T  offset;        /* /< offset */
	VAL_ULONG_T  value;         /* /< value */
	VAL_ULONG_T  mask;          /* /< mask */
} VCODEC_DRV_CMD_T;


/**
 * @par Structure
 *  HAL_HANDLE_T
 * @par Description
 *  hal handle information
 */
typedef struct _HAL_HANDLE_T_ {
	VAL_INT32_T     fd_vdec;            /* /< fd_vdec */
	VAL_INT32_T     fd_venc;            /* /< fd_venc */
	VAL_MEMORY_T    rHandleMem;         /* /< rHandleMem */
	VAL_ULONG_T     mmap[VCODEC_MAX];   /* /< mmap[VCODEC_MAX] */
	VAL_DRIVER_TYPE_T    driverType;    /* /< driverType */
	VAL_UINT32_T    u4TimeOut;          /* /< u4TimeOut */
	VAL_UINT32_T    u4FrameCount;       /* /< u4FrameCount */
#ifdef DumpReg__
	FILE *pf_out;
#endif
	VAL_BOOL_T      bProfHWTime;        /* /< bProfHWTime */
	VAL_UINT64_T    u8HWTime[2];        /* /< u8HWTime */
} HAL_HANDLE_T;


/**
 * @par Function
 *   eHalInit
 * @par Description
 *   The init hal driver function
 * @param
 *   a_phHalHandle      [IN/OUT] The hal handle
 * @param
 *   a_eHalCodecType    [IN] VDEC or VENC
 * @par Returns
 *   VAL_RESULT_T,
 *   return VAL_RESULT_NO_ERROR if success,
 *   return VAL_RESULT_INVALID_DRIVER or VAL_RESULT_INVALID_MEMORY if failed
 */
VAL_RESULT_T eHalInit(VAL_HANDLE_T *a_phHalHandle, HAL_CODEC_TYPE_T a_eHalCodecType);


/**
 * @par Function
 *   eHalDeInit
 * @par Description
 *   The deinit hal driver function
 * @param
 *   a_phHalHandle      [IN/OUT] The hal handle
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return else if failed
 */
VAL_RESULT_T eHalDeInit(VAL_HANDLE_T *a_phHalHandle);


/**
 * @par Function
 *   eHalGetMMAP
 * @par Description
 *   The get hw register memory map to vitural address function
 * @param
 *   a_hHalHandle       [IN/OUT] The hal handle
 * @param
 *   RegAddr            [IN] hw register address
 * @par Returns
 *   VAL_UINT32_T, vitural address of hw register memory mapping
 */
VAL_ULONG_T eHalGetMMAP(VAL_HANDLE_T *a_hHalHandle, VAL_UINT32_T RegAddr);


/**
 * @par Function
 *   eHalCmdProc
 * @par Description
 *   The hal command processing function
 * @param
 *   a_hHalHandle       [IN/OUT] The hal handle
 * @param
 *   a_eHalCmd          [IN] The hal command structure
 * @param
 *   a_pvInParam        [IN] The hal input parameter
 * @param
 *   a_pvOutParam       [OUT] The hal output parameter
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return else if failed
 */
VAL_RESULT_T eHalCmdProc(
	VAL_HANDLE_T *a_hHalHandle,
	HAL_CMD_T a_eHalCmd,
	VAL_VOID_T *a_pvInParam,
	VAL_VOID_T *a_pvOutParam
);



#ifdef __cplusplus
}
#endif

#endif /* #ifndef _HAL_API_H_ */
