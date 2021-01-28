/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _VAL_API_PRIVATE_H_
#define _VAL_API_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_private.h"
#include "val_api_public.h"


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
