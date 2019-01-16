/**
 * @file
 *   val_api_public.h
 *
 * @par Project:
 *   Video
 *
 * @par Description:
 *   Video Abstraction Layer API for internal use
 *
 * @par Author:
 *   Jackal Chen (mtk02532)
 *
 * @par $Revision: #1 $
 * @par $Modtime:$
 * @par $Log:$
 *
 */

#ifndef _VAL_API_PRIVATE_H_
#define _VAL_API_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_private.h"
#include "val_api_public.h"
#include "libmtk_cipher_export.h"


VAL_RESULT_T eValInit(VAL_HANDLE_T *a_phHalHandle);
VAL_RESULT_T eValDeInit(VAL_HANDLE_T *a_phHalHandle);

VAL_RESULT_T eVideoIntMemAlloc(VAL_INTMEM_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoIntMemFree(VAL_INTMEM_T *a_prParam, VAL_UINT32_T a_u4ParamSize);

VAL_RESULT_T eVideoCreateEvent(VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoSetEvent(VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoCloseEvent(VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoWaitEvent(VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize);

VAL_RESULT_T eVideoCreateMutex(VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoCloseMutex(VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoWaitMutex(VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoReleaseMutex(VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize);

VAL_RESULT_T eVideoMMAP(VAL_MMAP_T *a_prParam, VAL_UINT32_T a_u4ParamSize);
VAL_RESULT_T eVideoUnMMAP(VAL_MMAP_T *a_prParam, VAL_UINT32_T a_u4ParamSize);

VAL_RESULT_T eVideoInitLockHW(VAL_VCODEC_OAL_HW_REGISTER_T *prParam, int size);
VAL_RESULT_T eVideoDeInitLockHW(VAL_VCODEC_OAL_HW_REGISTER_T *prParam, int size);

VAL_RESULT_T eVideoVCodecCoreLoading(int CPUid, int *Loading);
VAL_RESULT_T eVideoVCodecCoreNumber(int *CPUNums);

VAL_RESULT_T eVideoConfigMCIPort(VAL_UINT32_T u4PortConfig, VAL_UINT32_T *pu4PortResult, VAL_MEM_CODEC_T eMemCodec);

VAL_UINT32_T eVideoHwM4UEnable(VAL_BOOL_T bEnable); // MTK_SEC_VIDEO_PATH_SUPPORT
VAL_UINT32_T eVideoLibDecrypt(VIDEO_ENCRYPT_CODEC_T a_eVIDEO_ENCRYPT_CODEC);

/* for DirectLink Meta Mode + */
VAL_RESULT_T eVideoAllocMetaHandleList(VAL_HANDLE_T *a_MetaHandleList);
VAL_RESULT_T eVideoGetBufInfoFromMetaHandle(VAL_HANDLE_T a_MetaHandleList, VAL_VOID_T *a_pvInParam, VAL_VOID_T *a_pvOutParam);
VAL_RESULT_T eVideoFreeMetaHandleList(VAL_HANDLE_T a_MetaHandleList);
/* for DirectLink Meta Mode - */

#ifdef __cplusplus
}
#endif

#endif // #ifndef _VAL_API_PRIVATE_H_
