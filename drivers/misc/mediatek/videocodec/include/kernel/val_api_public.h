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

#ifndef _VAL_API_PUBLIC_H_
#define _VAL_API_PUBLIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_public.h"


/**
 * @par Function
 *   eVideoInitMVA
 * @par Description
 *   Alloc the handle for MVA usage
 * @param
 *   a_pvHandle         [IN] The handle for MVA usage
 * @par Returns
 *   unsigned int       [OUT] Non-Used
 */
unsigned int eVideoInitMVA(void **a_pvHandle);


/**
 * @par Function
 *   eVideoAllocMVA
 * @par Description
 *   Use the given va and size, to get the MVA
 * @param
 *   a_pvHandle         [IN] The handle for MVA usage
 * @param
 *   a_u4Va             [IN] The given va used to get MVA
 * @param
 *   ap_u4Pa            [OUT] The MVA
 * @param
 *   a_u4Size           [IN] The given size used to get MVA
 * @param
 *   a_pvM4uConfig      [IN] The MVA config info
 * @par Returns
 *   unsigned int       [OUT] Non-Used
 */
unsigned int eVideoAllocMVA(
	void *a_pvHandle,
	unsigned int a_u4Va,
	unsigned int *ap_u4Pa,
	unsigned int a_u4Size,
	struct VAL_VCODEC_M4U_BUFFER_CONFIG_T *a_pvM4uConfig
);

/**
 * @par Function
 *   eVideoFreeMVA
 * @par Description
 *   Use the given va, MVA and size, to free the MVA
 * @param
 *   a_pvHandle         [IN] The handle for MVA usage
 * @param
 *   a_u4Va             [IN] The given va used to free MVA
 * @param
 *   a_u4Pa             [IN] The given MVA used to free MVA
 * @param
 *   a_u4Size           [IN] The given size used to get MVA
 * @param
 *   a_pvM4uConfig      [IN] The MVA config info
 * @par Returns
 *   unsigned int       [OUT] Non-Used
 */
unsigned int eVideoFreeMVA(
	void *a_pvHandle,
	unsigned int a_u4Va,
	unsigned int a_u4Pa,
	unsigned int a_u4Size,
	struct VAL_VCODEC_M4U_BUFFER_CONFIG_T *a_pvM4uConfig
);


/**
 * @par Function
 *   eVideoDeInitMVA
 * @par Description
 *   Free the handle for MVA usage
 * @param
 *   a_pvHandle         [IN] The handle for MVA usage
 * @par Returns
 *   unsigned int       [OUT] Non-Used
 */
unsigned int eVideoDeInitMVA(void *a_pvHandle);


/**
 * @par Function
 *   eVideoGetM4UModuleID
 * @par Description
 *   Get the M4U module port ID
 * @param
 *   u4MemType          [IN] The memory usage for VENC or VDEC
 * @par Returns
 *   unsigned int       [OUT] The M4U module port ID for VENC or VDEC
 */
int eVideoGetM4UModuleID(unsigned int u4MemType);


/**
 * @par Function
 *   eVideoAtoi
 * @par Description
 *   The abstraction layer for atoi() function
 * @param
 *   a_prParam          [IN] The structure contains used info for atoi()
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoAtoi(struct VAL_ATOI_T *a_prParam,
		unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eVideoStrStr
 * @par Description
 *   The abstraction layer for strstr() function
 * @param
 *   a_prParam          [IN] The structure contains used info for strstr()
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoStrStr(struct VAL_STRSTR_T *a_prParam,
				unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eVideoFlushCache
 * @par Description
 *   The flush cache usage function
 * @param
 *   a_prParam          [IN] The structure contains used info for flush cache
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @param
 *   optype             [IN] 0 for flush all, 1 for flush by page
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_MEMORY for fail
 */
enum VAL_RESULT_T eVideoFlushCache(struct VAL_MEMORY_T *a_prParam,
			unsigned int a_u4ParamSize, unsigned int optype);


/**
 * @par Function
 *   eVideoInvalidateCache
 * @par Description
 *   The invalidate cache usage function
 * @param
 *   a_prParam          [IN] The structure contains used info for
 *				invalidate cache
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @param
 *   optype             [IN] 0 for flush all, 1 for invalidate by page
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_MEMORY for fail
 */
enum VAL_RESULT_T eVideoInvalidateCache
			(struct VAL_MEMORY_T *a_prParam,
			unsigned int a_u4ParamSize, unsigned int optype);


/**
 * @par Function
 *   eVideoMemAlloc
 * @par Description
 *   The memory allocate usage function
 * @param
 *   a_prParam          [IN] The structure contains used info for
 *				allocate memory
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT]
 *   VAL_RESULT_NO_ERROR for success,
 *   VAL_RESULT_INVALID_MEMORY or
 *   VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoMemAlloc(struct VAL_MEMORY_T *a_prParam,
			unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eVideoMemFree
 * @par Description
 *   The memory free usage function
 * @param
 *   a_prParam          [IN] The structure contains used info for free memory
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoMemFree(struct VAL_MEMORY_T *a_prParam,
				unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eVideoMemSet
 * @par Description
 *   The memory set usage function
 * @param
 *   a_prParam          [IN] The structure contains used info for set memory
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @param
 *   a_u4Value          [IN] The value for set to memory
 * @param
 *   a_u4Size           [IN] The size of "memory" want to be set
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoMemSet(
	struct VAL_MEMORY_T *a_prParam,
	unsigned int a_u4ParamSize,
	int a_u4Value,
	unsigned int a_u4Size
);


/**
 * @par Function
 *   eVideoMemCpy
 * @par Description
 *   The memory copy usage function
 * @param
 *   a_prParamDst       [IN] The structure contains destination memory info for
 *				copy memory
 * @param
 *   a_u4ParamDstSize   [IN] The size of a_prParamDst structure
 * @param
 *   a_prParamSrc       [IN] The structure contains source memory info for copy
 *				memory
 * @param
 *   a_u4ParamSrcSize   [IN] The size of a_prParamSrc structure
 * @param
 *   a_u4Size           [IN] The size of "source memory" and "destination
 *				memory" want to be copied
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoMemCpy(
	struct VAL_MEMORY_T *a_prParamDst,
	unsigned int a_u4ParamDstSize,
	struct VAL_MEMORY_T *a_prParamSrc,
	unsigned int a_u4ParamSrcSize,
	unsigned int a_u4Size
);


/**
 * @par Function
 *   eVideoMemCmp
 * @par Description
 *   The memory compare usage function
 * @param
 *   a_prParamSrc1      [IN] The structure contains memory 1 info for compare
 *				memory
 * @param
 *   a_u4ParamSrc1Size  [IN] The size of a_prParamSrc1 structure
 * @param
 *   a_prParamSrc2      [IN] The structure contains memory 2 info for compare
 *				memory
 * @param
 *   a_u4ParamSrc2Size  [IN] The size of a_prParamSrc2 structure
 * @param
 *   a_u4Size           [IN] The size of "memory 1" and "memory 2" want to be
 *				compared
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_INVALID_PARAMETER for fail
 */
enum VAL_RESULT_T eVideoMemCmp(
	struct VAL_MEMORY_T *a_prParamSrc1,
	unsigned int a_u4ParamSrc1Size,
	struct VAL_MEMORY_T *a_prParamSrc2,
	unsigned int a_u4ParamSrc2Size,
	unsigned int a_u4Size
);


/**
 * @par Function
 *   WaitISR
 * @par Description
 *   The ISR usage related function, whene trigger HW,
 *   we will use to wait HW complete
 * @param
 *   a_prParam          [IN] The structure contains used info for ISR usage
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_ISR_TIMEOUT for fail
 */
enum VAL_RESULT_T WaitISR(struct VAL_ISR_T *a_prParam,
				unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eVideoLockHW
 * @par Description
 *   The single/multiple instance usage function, to allow using HW
 * @param
 *   a_prParam          [IN] The structure contains used info for Lock HW
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_UNKNOWN_ERROR for fail
 */
enum VAL_RESULT_T eVideoLockHW(struct VAL_HW_LOCK_T *a_prParam,
			unsigned int  a_u4ParamSize);


/**
 * @par Function
 *   eVideoLockHW
 * @par Description
 *   The single/multiple instance usage function, to release HW for another
 *	instance
 * @param
 *   a_prParam          [IN] The structure contains used info for unLock HW
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success,
 *				VAL_RESULT_UNKNOWN_ERROR for fail
 */
enum VAL_RESULT_T eVideoUnLockHW(struct VAL_HW_LOCK_T *a_prParam,
			unsigned int  a_u4ParamSize);


/**
 * @par Function
 *   eVideoGetTimeOfDay
 * @par Description
 *   The timing usage function, used to performance profiling
 * @param
 *   a_prParam          [IN] The structure contains used info for timing usage
 * @param
 *   a_u4ParamSize      [IN] The size of a_prParam structure
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success
 */
enum VAL_RESULT_T eVideoGetTimeOfDay(struct VAL_TIME_T *a_prParam,
				unsigned int a_u4ParamSize);


/**
 * @par Function
 *   eHalEMICtrlForRecordSize
 * @par Description
 *   The recording info function, to get the record size for setting to EMI
 *	controller
 * @param
 *   a_prDrvRecordSize  [IN] The structure contains used info for recording size
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success
 */
enum VAL_RESULT_T eHalEMICtrlForRecordSize
			(struct VAL_RECORD_SIZE_T *a_prDrvRecordSize);


/**
 * @par Function
 *   eVideoVcodecSetThreadID
 * @par Description
 *   The thread info function, to set thread ID for used to lock/unlock HW and
 *	priority adjustment
 * @param
 *   a_prThreadID       [IN] The structure contains used info for thread info
 * @par Returns
 *   VAL_RESULT_T       [OUT] VAL_RESULT_NO_ERROR for success
 */
enum VAL_RESULT_T eVideoVcodecSetThreadID
			(struct VAL_VCODEC_THREAD_ID_T *a_prThreadID);


/**
 * @par Function
 *   eVideoGetParam
 * @par Description
 *   The parameter info function, to get val parameter
 * @param
 *   a_eType        [IN]    The VAL_GET_TYPE_T enum
 * @param
 *   a_pvInParam    [IN]    The input parameter
 * @param
 *   a_pvOutParam   [OUT]   The output parameter
 * @par Returns
 *   VAL_RESULT_T   [OUT]   VAL_RESULT_NO_ERROR for success
 */
enum VAL_RESULT_T eVideoGetParam(enum VAL_GET_TYPE_T a_eType,
			void *a_pvInParam, void *a_pvOutParam);

/**
 * @par Function
 *   eVideoSetParam
 * @par Description
 *   The parameter info function, to set val parameter
 * @param
 *   a_eType        [IN]    The VAL_SET_TYPE_T enum
 * @param
 *   a_pvInParam    [IN]    The input parameter
 * @param
 *   a_pvOutParam   [OUT]   The output parameter
 * @par Returns
 *   VAL_RESULT_T   [OUT]   VAL_RESULT_NO_ERROR for success
 */
enum VAL_RESULT_T eVideoSetParam(enum VAL_SET_TYPE_T a_eType,
			void *a_pvInParam, void *a_pvOutParam);

enum VAL_RESULT_T eVideoE3TCMPowerON(unsigned int a_u4E3TCMClk);
enum VAL_RESULT_T eVideoE3TCMPowerOFF(unsigned int a_u4E3TCMClk);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_API_PUBLIC_H_ */
