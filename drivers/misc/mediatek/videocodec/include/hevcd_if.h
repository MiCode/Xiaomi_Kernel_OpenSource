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

#ifndef _HEVC_DECODE_IF_H_
#define _HEVC_DECODE_IF_H_

/* #include "hevcd.h" */
/* #include "hevc_common_if.h" */
#include "vcodec_if_v2.h"
#include "vcodec_dec_demuxer_if_v2.h"
#define MAX_DECODE_BUFFERS 15
#define _FILE_IO_
/* extern int frame_num; */
typedef struct {

	void (*hevc_pfnMalloc)(IN HANDLE                             /* hDrv */,
			       IN unsigned int                       /* u4Size */,
			       IN unsigned int                       /*u4AlignSize*/,
			       IN VCODEC_MEMORY_TYPE_T               /* fgCacheable */,
			       OUT VCODEC_BUFFER_T *                 /* prBuf */
			      );  /* buffer address must cache line align */

	void (*pfnFree)(IN HANDLE                             /* hDrv */,
			IN VCODEC_BUFFER_T *                   /* prBuf */
		       );  /* same memory type with malloc */

} hevcd_callback_t;

typedef struct {
	unsigned char *buffer_origin;
	unsigned char *luma;
	unsigned char *cb, *cr;
	int y_stride, uv_stride;
	int width, height;
	int ref_count;
} decode_picture_buffer_info_t;


typedef struct {
	unsigned char *start_address;
	int           length;
} HEVC_ACCESS_UNIT_T;

#define MAX_ACCESS_UNIT_NUMBER    32

typedef struct {
	HEVC_ACCESS_UNIT_T  au_list[MAX_ACCESS_UNIT_NUMBER];
	int                 au_number;

} HEVC_DECODE_INP_T;

typedef struct {
	unsigned char *y;
	unsigned char *u;
	unsigned char *v;

	int y_stride;
	int uv_stride;

	int width;
	int height;
} HEVC_DECODE_PICTURE_T;

typedef struct {
	int width;
	int height;
} HEVC_PICTURE_INFO_T;


VCODEC_DEC_ERROR_T HEVCDecoderGetMemoryRequired(
	VCODEC_DEC_INPUT_T * prInput,
	VCODEC_MEMORY_SIZE_T *prMemeorySize,
	VCODEC_DEC_OUTPUT_BUFFER_PARAM_T *prYUVBufferParameter,
	VCODEC_MEMORY_TYPE_T * prBitStreamBufferMemType
);
VCODEC_DEC_ERROR_T HEVCDecoderGetMemoryRequiredExtend(
	VCODEC_DEC_INPUT_T * prInput,
	VCODEC_MEMORY_SIZE_T *prMemeorySize,
	VCODEC_DEC_OUTPUT_BUFFER_PARAM_T *prYUVBufferParameter,
	OUT VCODEC_DEC_INPUT_BUFFER_PARAM_T * prBitStreamParam,
	INOUT void *prExtra
);
VCODEC_DEC_ERROR_T HEVCDecoderInitAdapt(IN HANDLE hCodec);
VCODEC_DEC_ERROR_T HEVCDecoderDeInitAdapt(IN HANDLE hCodec);
VCODEC_DEC_ERROR_T HEVCDecoderOpenAdapt(
	IN HANDLE hDrv ,
	IN VCODEC_DEC_CALLBACK_T * pfnCallback,
	IN void *prOpenSetting, OUT HANDLE * hCodec
);
VCODEC_DEC_ERROR_T HEVCDecoderStartAdapt(IN HANDLE hCodec, IN VCODEC_DEC_INPUT_T * prBufferHeader);
VCODEC_DEC_ERROR_T HEVCDecoderCloseAdapt(IN HANDLE hCodec);
VCODEC_DEC_ERROR_T HEVCDecoderGetNextDisplay(IN HANDLE hCodec, OUT VCODEC_DEC_PRIVATE_OUTPUT_T * prPrivateOutput);
VCODEC_DEC_ERROR_T HEVCDecoderGetParameterAdapt(IN HANDLE hCodec, IN VCODEC_DEC_PARAM_TYPE_T eCmd, INOUT void *pParam);
VCODEC_DEC_ERROR_T HEVCDecoderSetParameterAdapt(IN HANDLE hCodec, IN VCODEC_DEC_PARAM_TYPE_T eCmd, INOUT void *pParam);

extern VCODEC_DEC_API_T *GetHEVCDecoderAPI(void);

VCODEC_DEC_API_T *GetHEVCDecoderAPI(void);


#endif
