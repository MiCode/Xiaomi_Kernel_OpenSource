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

#define DumpInput__
#ifdef DumpInput__
#include <stdio.h>
#endif
#include "vdec_drv_if_private.h"

#ifndef _VDEC_DRV_BASE_
#define _VDEC_DRV_BASE_
#define MAX_BUFFER_SIZE 37

#if 1
#define WAITING_MODE VAL_INTERRUPT_MODE
#else
#define WAITING_MODE VAL_POLLING_MODE
#endif

#define DO_VCODEC_RESET(cmd, index)                                                             \
	{                                                                                               \
		ADD_QUEUE(cmd, index, WRITE_REG_CMD, MT6589_VDEC_MISC, VDEC_INT_CFG, 0 , WAITING_MODE);     \
	}

typedef enum __VDDRV_MRESULT_T {
	VDDRV_MRESULT_SUCCESS = VAL_TRUE,  /* /< Represent success */
	VDDRV_MRESULT_FAIL = VAL_FALSE,    /* /< Represent failure */
	VDDRV_MRESULT_RESOLUTION_CHANGED = VAL_RESOLUTION_CHANGED,   /* /< Represent resoluion changed */
	VDDRV_MRESULT_NEED_MORE_OUTPUT_BUFFER,   /* /< Represent need more output buffer */
	VDDRV_MRESULT_FATAL
} VDDRV_MRESULT_T;

typedef struct __VDEC_DRV_BASE_T {
	VAL_UINT32_T (*Init)(
		VAL_HANDLE_T *handle,
		VAL_HANDLE_T halhandle,
		VAL_HANDLE_T valhandle,
		P_VDEC_DRV_RINGBUF_T pBitstream,
		P_VDEC_DRV_SEQINFO_T pSeqinfo,
		VDEC_DRV_VIDEO_FORMAT_T eFormat
	);
	VAL_UINT32_T (*Decode)(
		VAL_HANDLE_T handle,
		P_VDEC_DRV_RINGBUF_T
		pBitstream,
		P_VDEC_DRV_FRAMEBUF_T pFrame
	); /* /< Driver Decode Main Funciton */
	P_VDEC_DRV_FRAMEBUF_T(*GetDisplayBuffer)(
		VAL_HANDLE_T handle
	); /* /< Get Buffer ready to display */
	P_VDEC_DRV_FRAMEBUF_T(*GetFreeBuffer)(
		VAL_HANDLE_T handle
	);  /* /< Get Buffer ready to release */
	VAL_UINT32_T (*GetParam)(
		VAL_HANDLE_T handle,
		VDEC_DRV_GET_TYPE_T a_eType,
		VAL_VOID_T *a_pvInParam,
		VAL_VOID_T *a_pvOutParam
	);
	VAL_UINT32_T (*SetParam)(
		VAL_HANDLE_T handle,
		VDEC_DRV_SET_TYPE_T a_eType,
		VAL_VOID_T *a_pvInParam,
		VAL_VOID_T *a_pvOutParam
	);
	VAL_UINT32_T (*DeInit)(
		VAL_HANDLE_T handle
	); /* /< Function to do driver de-initialization */
	P_VDEC_DRV_RINGBUF_T (*GetFreeInputBuffer)(
		VAL_HANDLE_T handle
	);
	VAL_UINT32_T (*Init_ex)(
		VAL_HANDLE_T *handle,
		VAL_HANDLE_T halhandle,
		VAL_HANDLE_T valhandle,
		P_VDEC_DRV_RINGBUF_T pBitstream,
		P_VDEC_DRV_SEQINFO_T pSeqinfo,
		VDEC_DRV_VIDEO_FORMAT_T eFormat,
		VAL_VOID_T *pConfig
	);
} VDEC_DRV_BASE_T;

typedef struct __VDEC_DRV_BUF_STATUS_T {
	VAL_BOOL_T          bDisplay;
	VAL_BOOL_T          bFree;
	VDEC_DRV_FRAMEBUF_T *pFrameBuf;
} VDEC_DRV_BUF_STATUS_T, *P_VDEC_DRV_BUF_STATUS_T;

typedef struct __VDEC_DRV_INPUT_BUF_STATUS_T {
	VDEC_DRV_RINGBUF_T *pInputBuf;
	VAL_UINT32_T    u4RefCnt;
} VDEC_DRV_INPUT_BUF_STATUS_T, *P_VDEC_DRV_INPUT_BUF_STATUS_T;

typedef struct __VDEC_HANDLE_T {
	VDEC_DRV_VIDEO_FORMAT_T CodecFormat;
	VDEC_DRV_BASE_T         rFuncPtr;      /* /< Point to driver's proprietary function. */
	VAL_HANDLE_T            hDrvHandle;    /* /< Handle of each format driver */
	VAL_BOOL_T              fgDrvInitFlag; /* /< hDrvHandle is available or not */
	VAL_HANDLE_T            hHalHandle;    /* /< HAL handle */
	VAL_BOOL_T              fgHalInitFlag; /* /< hHalHandle is available or not */
	VAL_HANDLE_T            hValHandle;    /* /< VAL handle */
	VAL_BOOL_T              fgValInitFlag; /* /< hValHandle is available or not */
	VAL_MEMORY_T            rHandleMem;    /* /< Memory for vdec handle */
	P_VDEC_DRV_FRAMEBUF_T   pDispFrameBuf;
	P_VDEC_DRV_FRAMEBUF_T   pFreeFrameBuf;
	P_VDEC_DRV_RINGBUF_T    pInputFreeBuf;
	VDEC_DRV_BUF_STATUS_T   pFrameBufArray[MAX_BUFFER_SIZE];
	VDEC_DRV_INPUT_BUF_STATUS_T pInputBufArray[MAX_BUFFER_SIZE];
	VAL_BOOL_T              bFlushAll;
	VAL_BOOL_T              bInputFlushAll;
	/* for no VOS header when MPEG4 */
	VAL_UINT16_T            nDefWidth;
	VAL_UINT16_T            nDefHeight;
	VDEC_DRV_SET_DECODE_MODE_T      rSetDecodeMode;
	VAL_INT32_T             nPerfServiceHandle;
#ifdef DumpInput__
	FILE *pf_out;
	VAL_MEM_ADDR_T          rDumpBase;
#endif
	VAL_UINT32_T            nOmxTids;
} VDEC_HANDLE_T;

#endif
