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

#ifndef _VAL_API_PRIVATE_H_
#define _VAL_API_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_private.h"
#include "val_api_public.h"
#include "libmtk_cipher_export.h"


enum VAL_RESULT_T eValInit(unsigned long *a_phHalHandle);
enum VAL_RESULT_T eValDeInit(unsigned long *a_phHalHandle);

enum VAL_RESULT_T eVideoIntMemAlloc
	(struct VAL_INTMEM_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoIntMemFree
	(struct VAL_INTMEM_T *a_prParam, unsigned int a_u4ParamSize);

enum VAL_RESULT_T eVideoCreateEvent
	(struct VAL_EVENT_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoSetEvent
	(struct VAL_EVENT_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoCloseEvent
	(struct VAL_EVENT_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoWaitEvent
	(struct VAL_EVENT_T *a_prParam, unsigned int a_u4ParamSize);

enum VAL_RESULT_T eVideoCreateMutex
	(struct VAL_MUTEX_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoCloseMutex
	(struct VAL_MUTEX_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoWaitMutex
	(struct VAL_MUTEX_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoReleaseMutex
	(struct VAL_MUTEX_T *a_prParam, unsigned int a_u4ParamSize);

enum VAL_RESULT_T eVideoMMAP
	(struct VAL_MMAP_T *a_prParam, unsigned int a_u4ParamSize);
enum VAL_RESULT_T eVideoUnMMAP
	(struct VAL_MMAP_T *a_prParam, unsigned int a_u4ParamSize);

enum VAL_RESULT_T eVideoInitLockHW
	(struct VAL_VCODEC_OAL_HW_REGISTER_T *prParam, int size);
enum VAL_RESULT_T eVideoDeInitLockHW
	(struct VAL_VCODEC_OAL_HW_REGISTER_T *prParam, int size);

enum VAL_RESULT_T eVideoVCodecCoreLoading(int CPUid, int *Loading);
enum VAL_RESULT_T eVideoVCodecCoreNumber(int *CPUNums);

enum VAL_RESULT_T eVideoConfigMCIPort
	(unsigned int u4PortConfig, unsigned int *pu4PortResult,
	enum VAL_MEM_CODEC_T eMemCodec);

unsigned int eVideoHwM4UEnable(char bEnable);
/* MTK_SEC_VIDEO_PATH_SUPPORT */
unsigned int eVideoLibDecrypt
	(enum VIDEO_ENCRYPT_CODEC_T a_eVIDEO_ENCRYPT_CODEC);

/* for DirectLink Meta Mode + */
enum VAL_RESULT_T eVideoAllocMetaHandleList(unsigned long *a_MetaHandleList);
enum VAL_RESULT_T eVideoGetBufInfoFromMetaHandle(
	unsigned long a_MetaHandleList,
	void *a_pvInParam,
	void *a_pvOutParam
);
enum VAL_RESULT_T eVideoFreeMetaHandleList(unsigned long a_MetaHandleList);
/* for DirectLink Meta Mode - */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_API_PRIVATE_H_ */
