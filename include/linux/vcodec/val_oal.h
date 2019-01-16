/**
 * @file
 *   val_oal.h
 *
 * @par Project:
 *   Video
 *
 * @par Description:
 *   Video Codec Driver & Codec Liabrary Interface
 *
 * @par Author:
 *   Jackal Chen (mtk02532)
 *
 * @par $Revision: #1 $
 * @par $Modtime:$
 * @par $Log:$
 *
 */

#ifndef _SP5_OAL_H_
#define _SP5_OAL_H_

// SP5 interface
#include "vcodec_OAL_v2.h"

// ME1 interface
#include "val_types_private.h"

/**
 * @par Enumeration
 *   __VAL_OAL_TYPE
 * @par Description
 *   This is the item used to set OAL type
 */
typedef enum ___VAL_OAL_TYPE
{
    _BYTE_  = 0x5000,     ///< BYTE
    _WORD_,               ///< WORD
    _LONG_                ///< LONG
} __VAL_OAL_TYPE;


#define  SP5_VCodecQueryMemType                       VCodecDrvQueryMemType                     ///< VCodecDrvQueryMemType definition for SW/hybrid codec
#define  SP5_VCodecQueryPhysicalAddr                  VCodecDrvQueryPhysicalAddr                ///< VCodecDrvQueryPhysicalAddr definition for SW/hybrid codec
#define  SP5_VCodecSwitchMemType                      VCodecDrvSwitchMemType                    ///< VCodecDrvSwitchMemType definition for SW/hybrid codec
#define  SP5_VCodecFlushCachedBuffer                  VCodecDrvFlushCachedBuffer                ///< VCodecDrvFlushCachedBuffer definition for SW/hybrid codec
#define  SP5_VCodecInvalidateCachedBuffer             VCodecDrvInvalidateCachedBuffer           ///< VCodecDrvInvalidateCachedBuffer definition for SW/hybrid codec
#define  SP5_VCodecFlushCachedBufferAll               VCodecDrvFlushCachedBufferAll             ///< VCodecDrvFlushCachedBufferAll definition for SW/hybrid codec
#define  SP5_VCodecInvalidateCachedBufferAll          VCodecDrvInvalidateCachedBufferAll        ///< VCodecDrvInvalidateCachedBufferAll definition for SW/hybrid codec
#define  SP5_VCodecFlushInvalidateCacheBufferAll      VCodecDrvFlushInvalidateCacheBufferAll    ///< VCodecDrvFlushInvalidateCacheBufferAll definition for SW/hybrid codec
#define  SP5_VCodecMemSet                             VCodecDrvMemSet                           ///< VCodecDrvMemSet definition for SW/hybrid codec
#define  SP5_VCodecMemCopy                            VCodecDrvMemCopy                          ///< VCodecDrvMemCopy definition for SW/hybrid codec
#define  SP5_VCodecAssertFail                         VCodecDrvAssertFail                       ///< VCodecDrvAssertFail definition for SW/hybrid codec
#define  SP5_VCodecMMAP                               VCodecDrvMMAP                             ///< VCodecDrvMMAP definition for SW/hybrid codec
#define  SP5_VCodecUnMMAP                             VCodecDrvUnMMAP                           ///< VCodecDrvUnMMAP definition for SW/hybrid codec
#define  SP5_VCodecWaitISR                            VCodecDrvWaitISR                          ///< VCodecDrvWaitISR definition for SW/hybrid codec
#define  SP5_VCodecLockHW                             VCodecDrvLockHW                           ///< VCodecDrvLockHW definition for SW/hybrid codec
#define  SP5_VCodecUnLockHW                           VCodecDrvUnLockHW                         ///< VCodecDrvUnLockHW definition for SW/hybrid codec
#define  SP5_VCodecInitHWLock                         VCodecDrvInitHWLock                       ///< VCodecDrvInitHWLock definition for SW/hybrid codec
#define  SP5_VCodecDeInitHWLock                       VCodecDrvDeInitHWLock                     ///< VCodecDrvDeInitHWLock definition for SW/hybrid codec
#if 0
#define  SP5_VcodecTraceLog0                          VCodecDrvTraceLog0                        ///< VCodecDrvTraceLog0 definition for SW/hybrid codec
#define  SP5_VcodecTraceLog1                          VCodecDrvTraceLog1                        ///< VCodecDrvTraceLog1 definition for SW/hybrid codec
#define  SP5_VcodecTraceLog2                          VCodecDrvTraceLog2                        ///< VCodecDrvTraceLog2 definition for SW/hybrid codec
#define  SP5_VcodecTraceLog4                          VCodecDrvTraceLog4                        ///< VCodecDrvTraceLog4 definition for SW/hybrid codec
#define  SP5_VcodecTraceLog8                          VCodecDrvTraceLog8                        ///< VCodecDrvTraceLog8 definition for SW/hybrid codec
#else
#define  SP5_VCodecPrintf                             VCodecPrintf                              ///< VCodecPrintf definition for SW/hybrid codec
#endif
#define  SP5_VdoMemAllocAligned                       VCodecDrvMemAllocAligned                  ///< VCodecDrvMemAllocAligned definition for SW/hybrid codec
#define  SP5_VdoMemFree                               VCodecDrvMemFree                          ///< VCodecDrvMemFree definition for SW/hybrid codec
#define  SP5_VdoIntMalloc                             VCodecDrvIntMalloc                        ///< VCodecDrvIntMalloc definition for SW/hybrid codec
#define  SP5_VdoIntFree                               VCodecDrvIntFree                          ///< VCodecDrvIntFree definition for SW/hybrid codec
#define  SP5_RegSync                                  VCodecDrvRegSync                          ///< VCodecDrvRegSync definition for SW/hybrid codec
#define  SP5_RegSyncWriteB                            VCodecDrvRegSyncWriteB                    ///< VCodecDrvRegSyncWriteB definition for SW/hybrid codec
#define  SP5_RegSyncWriteW                            VCodecDrvRegSyncWriteW                    ///< VCodecDrvRegSyncWriteW definition for SW/hybrid codec
#define  SP5_RegSyncWriteL                            VCodecDrvRegSyncWriteL                    ///< VCodecDrvRegSyncWriteL definition for SW/hybrid codec
#define  SP5_VMPEG4EncCodecWaitISR                    VMPEG4EncCodecDrvWaitISR                  ///< VMPEG4EncCodecDrvWaitISR definition for SW/hybrid codec
#define  SP5_VMPEG4EncCodecLockHW                     VMPEG4EncCodecDrvLockHW                   ///< VMPEG4EncCodecDrvLockHW definition for SW/hybrid codec
#define  SP5_VMPEG4EncCodecUnLockHW                   VMPEG4EncCodecDrvUnLockHW                 ///< VMPEG4EncCodecDrvUnLockHW definition for SW/hybrid codec


/**
 * @par Function
 *   SP5_VCodecQueryMemType
 * @par Description
 *   The function used to query memory type for SW/hybrid codec
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   u4Size             [IN] The size of buffer
 * @param
 *   peMemType          [OUT] The memory type
 * @par Returns
 *   void
 */
void SP5_VCodecQueryMemType(IN void            *pBuffer_VA,
                            IN unsigned int    u4Size,
                            OUT VCODEC_MEMORY_TYPE_T *peMemType);


/**
 * @par Function
 *   SP5_VCodecQueryPhysicalAddr
 * @par Description
 *   The function used to query physical address
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   pBufferOut_PA      [OUT] The physical address
 * @par Returns
 *   void
 */
void SP5_VCodecQueryPhysicalAddr(IN void       *pBuffer_VA,
                                 OUT void     **pBufferOut_PA);


/**
 * @par Function
 *   SP5_VCodecSwitchMemType
 * @par Description
 *   The function used to switch memory type for SW/hybrid codec
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   u4Size             [IN] The size of buffer
 * @param
 *   eMemType           [IN] The memory type
 * @param
 *   pBufferOut_VA      [OUT] The pointer of buffer address
 * @par Returns
 *   int, return 0 if success, return -1 if failed
 */
int SP5_VCodecSwitchMemType(IN void            *pBuffer_VA,
                            IN unsigned int    u4Size,
                            IN VCODEC_MEMORY_TYPE_T eMemType,
                            OUT void           **pBufferOut_VA);


/**
 * @par Function
 *   SP5_VCodecFlushCachedBuffer
 * @par Description
 *   The function used to flush cache by size
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   u4Size             [IN] The size of buffer
 * @par Returns
 *   void
 */
void SP5_VCodecFlushCachedBuffer(IN void         *pBuffer_VA,
                                 IN unsigned int u4Size);


/**
 * @par Function
 *   SP5_VCodecInvalidateCachedBuffer
 * @par Description
 *   The function used to invalidate cache by size
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   u4Size             [IN] The size of buffer
 * @par Returns
 *   void
 */
void SP5_VCodecInvalidateCachedBuffer(IN void         *pBuffer_VA,
                                      IN unsigned int   u4Size);


/**
 * @par Function
 *   SP5_VCodecFlushCachedBufferAll
 * @par Description
 *   The function used to flush all cache
 * @par Returns
 *   void
 */
void SP5_VCodecFlushCachedBufferAll();


/**
 * @par Function
 *   SP5_VCodecInvalidateCachedBufferAll
 * @par Description
 *   The function used to invalidate all cache
 * @par Returns
 *   void
 */
void SP5_VCodecInvalidateCachedBufferAll();


/**
 * @par Function
 *   SP5_VCodecFlushInvalidateCacheBufferAll
 * @par Description
 *   The function used to flush & invalidate all cache
 * @par Returns
 *   void
 */
void SP5_VCodecFlushInvalidateCacheBufferAll();


/**
 * @par Function
 *   SP5_VCodecMemSet
 * @par Description
 *   The function used to memory set
 * @param
 *   pBuffer_VA         [IN] The pointer of buffer address
 * @param
 *   cValue             [IN] The value will be set to memory
 * @param
 *   u4Length           [IN] The length of memory will be set
 * @par Returns
 *   void
 */
void  SP5_VCodecMemSet(IN void                *pBuffer_VA,
                       IN char                cValue,
                       IN unsigned int        u4Length);


/**
 * @par Function
 *   SP5_VCodecMemCopy
 * @par Description
 *   The function used to memory copy
 * @param
 *   pvDest             [IN] The pointer of destination memory
 * @param
 *   pvSrc              [IN] The pointer of source memory
 * @param
 *   u4Length           [IN] The length of memory will be copied
 * @par Returns
 *   void
 */
void  SP5_VCodecMemCopy(IN void             *pvDest ,
                        IN const void      *pvSrc ,
                        IN unsigned int      u4Length);


/**
 * @par Function
 *   SP5_VCodecAssertFail
 * @par Description
 *   The function used to assert when occur error
 * @param
 *   ptr                [IN] The name of error source file
 * @param
 *   i4Line             [IN] The line of error source file
 * @param
 *   i4Arg              [IN] The argumnet of error source file
 * @par Returns
 *   void
 */
void SP5_VCodecAssertFail(IN char *ptr,
                          IN int i4Line,
                          IN int i4Arg);


/**
 * @par Function
 *   SP5_VCodecMMAP
 * @par Description
 *   The function used to memory map
 * @param
 *   prParam            [IN/OUT] The structure contains memory info for memory map
 * @par Returns
 *   void
 */
void SP5_VCodecMMAP(VCODEC_OAL_MMAP_T *prParam);


/**
 * @par Function
 *   SP5_VCodecUnMMAP
 * @par Description
 *   The function used to memory unmap
 * @param
 *   prParam            [IN/OUT] The structure contains memory info for memory unmap
 * @par Returns
 *   void
 */
void SP5_VCodecUnMMAP(VCODEC_OAL_MMAP_T *prParam);


/**
 * @par Function
 *   SP5_VCodecWaitISR
 * @par Description
 *   The ISR usage related function, whene trigger HW, we will use to wait HW complete
 * @param
 *   prParam            [IN/OUT] The structure contains used info for ISR usage
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VCodecWaitISR(VCODEC_OAL_ISR_T *prParam);


/**
 * @par Function
 *   SP5_VCodecLockHW
 * @par Description
 *   The single/multiple instance usage function, to allow using HW
 * @param
 *   prParam            [IN/OUT] The structure contains used info for Lock HW
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VCodecLockHW(VCODEC_OAL_HW_LOCK_T *prParam);


/**
 * @par Function
 *   SP5_VCodecUnLockHW
 * @par Description
 *   The single/multiple instance usage function, to release HW for another instance
 * @param
 *   prParam            [IN/OUT] The structure contains used info for unLock HW
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VCodecUnLockHW(VCODEC_OAL_HW_LOCK_T *prParam);


/**
 * @par Function
 *   SP5_VCodecInitHWLock
 * @par Description
 *   The function used to init HW lock
 * @param
 *   prParam            [IN/OUT] The structure contains used info for init HW lock
 * @par Returns
 *   void
 */
void SP5_VCodecInitHWLock(VCODEC_OAL_HW_REGISTER_T *prParam);


/**
 * @par Function
 *   SP5_VCodecDeInitHWLock
 * @par Description
 *   The function used to deinit HW lock
 * @param
 *   prParam            [IN/OUT] The structure contains used info for deinit HW lock
 * @par Returns
 *   void
 */
void SP5_VCodecDeInitHWLock(VCODEC_OAL_HW_REGISTER_T *prParam);


#if 0
/**
 * @par Function
 *   SP5_VcodecTraceLog0
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   eGroup             [IN] The value to define log importance priority
 * @param
 *   eIndex             [IN] The value to define log type
 * @par Returns
 *   void
 */
void SP5_VcodecTraceLog0(IN VCODEC_LOG_GROUP_T eGroup,
                         IN VCODEC_LOG_INDEX_T eIndex
                        );


/**
 * @par Function
 *   SP5_VcodecTraceLog1
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   eGroup             [IN] The value to define log importance priority
 * @param
 *   eIndex             [IN] The value to define log type
 * @param
 *   arg                [IN] The input argument
 * @par Returns
 *   void
 */
void SP5_VcodecTraceLog1(IN VCODEC_LOG_GROUP_T eGroup,
                         IN VCODEC_LOG_INDEX_T eIndex,
                         IN UINT64 arg
                        );


/**
 * @par Function
 *   SP5_VcodecTraceLog2
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   eGroup             [IN] The value to define log importance priority
 * @param
 *   eIndex             [IN] The value to define log type
 * @param
 *   arg1               [IN] The input argument1
 * @param
 *   arg2               [IN] The input argument2
 * @par Returns
 *   void
 */
void SP5_VcodecTraceLog2(IN VCODEC_LOG_GROUP_T eGroup,
                         IN  VCODEC_LOG_INDEX_T eIndex,
                         IN  UINT64 arg1,
                         IN  UINT64 arg2
                        );


/**
 * @par Function
 *   SP5_VcodecTraceLog4
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   eGroup             [IN] The value to define log importance priority
 * @param
 *   eIndex             [IN] The value to define log type
 * @param
 *   arg1               [IN] The input argument1
 * @param
 *   arg2               [IN] The input argument2
 * @param
 *   arg3               [IN] The input argument3
 * @param
 *   arg4               [IN] The input argument4
 * @par Returns
 *   void
 */
void SP5_VcodecTraceLog4(IN VCODEC_LOG_GROUP_T eGroup,
                         IN  VCODEC_LOG_INDEX_T eIndex,
                         IN  UINT64 arg1,
                         IN  UINT64 arg2, IN  UINT64 arg3,
                         IN  UINT64 arg4
                        );


/**
 * @par Function
 *   SP5_VcodecTraceLog4
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   eGroup             [IN] The value to define log importance priority
 * @param
 *   eIndex             [IN] The value to define log type
 * @param
 *   arg1               [IN] The input argument1
 * @param
 *   arg2               [IN] The input argument2
 * @param
 *   arg3               [IN] The input argument3
 * @param
 *   arg4               [IN] The input argument4
 * @param
 *   arg5               [IN] The input argument5
 * @param
 *   arg6               [IN] The input argument6
 * @param
 *   arg7               [IN] The input argument7
 * @param
 *   arg8               [IN] The input argument8
 * @par Returns
 *   void
 */
void SP5_VcodecTraceLog8(IN VCODEC_LOG_GROUP_T eGroup,
                         IN  VCODEC_LOG_INDEX_T eIndex,
                         IN  UINT64 arg1,
                         IN  UINT64 arg2,
                         IN  UINT64 arg3,
                         IN UINT64 arg4,
                         IN  UINT64 arg5,
                         IN  UINT64 arg6,
                         IN  UINT64 arg7,
                         IN  UINT64 arg8
                        );
#else
/**
 * @par Function
 *   SP5_VCodecPrintf
 * @par Description
 *   The function used to trace log for debug
 * @param
 *   format             [IN] log string
 * @param
 *   ...                   [IN] log argument
 */
VCODEC_OAL_ERROR_T SP5_VCodecPrintf(IN const char *_Format, ...);
#endif


/**
 * @par Function
 *   SP5_VdoMemAllocAligned
 * @par Description
 *   The function used to alloc external working memry
 * @param
 *   handle             [IN] codec/driver handle
 * @param
 *   size               [IN] allocated memory size
 * @param
 *   u4AlignSize        [IN] allocated memory byte alignment
 * @param
 *   cachable           [IN] memory type
 * @param
 *   pBuf               [OUT] allocated memory buffer info
 * @param
 *   eMemCodec          [IN] allocated memory used for venc/vdec
 * @par Returns
 *   VAL_VOID_T
 */
VAL_VOID_T SP5_VdoMemAllocAligned(VAL_VOID_T *handle, VAL_UINT32_T size, unsigned int u4AlignSize, VCODEC_MEMORY_TYPE_T cachable, VCODEC_BUFFER_T *pBuf, VAL_MEM_CODEC_T eMemCodec);


/**
 * @par Function
 *   SP5_VdoMemFree
 * @par Description
 *   The function used to free external working memry
 * @param
 *   handle             [IN] codec/driver handle
 * @param
 *   pBuf               [IN] allocated memory buffer info
 * @par Returns
 *   VAL_VOID_T
 */
VAL_VOID_T SP5_VdoMemFree(VAL_VOID_T *handle, VCODEC_BUFFER_T *pBuf);


/**
 * @par Function
 *   SP5_VdoIntMalloc
 * @par Description
 *   The function used to alloc internal working memry
 * @param
 *   handle             [IN] codec/driver handle
 * @param
 *   size               [IN] allocated memory size
 * @param
 *   alignedsize        [IN] allocated memory byte alignment
 * @param
 *   prBuffer_adr       [OUT] allocated memory buffer info
 * @par Returns
 *   VAL_VOID_T
 */
VAL_VOID_T SP5_VdoIntMalloc(HANDLE  handle, unsigned int size, unsigned int alignedsize, VCODEC_BUFFER_T *prBuffer_adr);


/**
 * @par Function
 *   SP5_VdoIntFree
 * @par Description
 *   The function used to free internal working memry
 * @param
 *   handle             [IN] codec/driver handle
 * @param
 *   prBuffer_adr       [IN] allocated memory buffer info
 * @par Returns
 *   VAL_VOID_T
 */
VAL_VOID_T SP5_VdoIntFree(HANDLE handle, VCODEC_BUFFER_T *prBuffer_adr);


/**
 * @par Function
 *   SP5_RegSync
 * @par Description
 *   The function used to set register sync
 * @param
 *   type               [IN] BYTE/WORD/LONG
 * @param
 *   v                  [IN] register value
 * @param
 *   a                  [IN] register address
 * @par Returns
 *   VAL_VOID_T
 */
VAL_VOID_T SP5_RegSync(int type, unsigned int v, unsigned int a);


/**
 * @par Function
 *   SP5_VMPEG4EncCodecWaitISR
 * @par Description
 *   The ISR usage related function, whene trigger HW, we will use to wait HW complete
 * @param
 *   prParam            [IN/OUT] The structure contains used info for ISR usage
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VMPEG4EncCodecWaitISR(VCODEC_OAL_ISR_T *prParam);


/**
 * @par Function
 *   SP5_VMPEG4EncCodecLockHW
 * @par Description
 *   The single/multiple instance usage function, to allow using HW
 * @param
 *   prParam            [IN/OUT] The structure contains used info for Lock HW
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VMPEG4EncCodecLockHW(VCODEC_OAL_HW_LOCK_T *prParam);


/**
 * @par Function
 *   SP5_VMPEG4EncCodecUnLockHW
 * @par Description
 *   The single/multiple instance usage function, to release HW for another instance
 * @param
 *   prParam            [IN/OUT] The structure contains used info for unLock HW
 * @par Returns
 *   int, return 1 if success, return 0 if failed
 */
int SP5_VMPEG4EncCodecUnLockHW(VCODEC_OAL_HW_LOCK_T *prParam);


/**
 * @par Function
 *   eValInit
 * @par Description
 *   The init driver function
 * @param
 *   a_phHalHandle      [IN/OUT] The codec/driver handle
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return VAL_RESULT_UNKNOWN_ERROR if failed
 */
VAL_RESULT_T eValInit(VAL_HANDLE_T *a_phHalHandle);


/**
 * @par Function
 *   eValDeInit
 * @par Description
 *   The deinit driver function
 * @param
 *   a_phHalHandle      [IN/OUT] The codec/driver handle
 * @par Returns
 *   VAL_RESULT_T, return VAL_RESULT_NO_ERROR if success, return VAL_RESULT_UNKNOWN_ERROR if failed
 */
VAL_RESULT_T eValDeInit(VAL_HANDLE_T *a_phHalHandle);


/**
 * @par Function
 *   VCodecDrvCheck_Version
 * @par Description
 *   The function used to check codec library version
 * @param
 *   version            [IN/OUT] The codec library version
 * @par Returns
 *   int, return 0 if success, return -1 if failed
 */
int VCodecDrvCheck_Version(int version);

/************  Multi-thread function ***********/

/***** Thread Management Functions ******/


/**
 * @par Function
 *   VCodecDrvPthread_attr_init
 * @par Description
 *   The pthread_attr_init wrapper function
 * @param
 *   attr               [OUT] attr
 * @par Returns
 *   int, pthread_attr_init((pthread_attr_t *)attr);
 */
int VCodecDrvPthread_attr_init(OUT VCODEC_PTHREAD_ATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_attr_destroy
 * @par Description
 *   The pthread_attr_destroy wrapper function
 * @param
 *   attr               [IN] attr
 * @par Returns
 *   int, pthread_attr_destroy((pthread_attr_t *)attr);
 */
int VCodecDrvPthread_attr_destroy(IN VCODEC_PTHREAD_ATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_attr_getdetachstate
 * @par Description
 *   The pthread_attr_getdetachstate wrapper function
 * @param
 *   attr               [IN] attr
 * @param
 *   detachstate        [OUT] detachstate
 * @par Returns
 *   int, pthread_attr_getdetachstate((pthread_attr_t const *)attr, detachstate);
 */
int VCodecDrvPthread_attr_getdetachstate(IN const VCODEC_PTHREAD_ATTR_T *attr, OUT int *detachstate);


/**
 * @par Function
 *   VCodecDrvPthread_attr_getdetachstate
 * @par Description
 *   The pthread_attr_getdetachstate wrapper function
 * @param
 *   attr               [IN] attr
 * @param
 *   detachstate        [OUT] detachstate
 * @par Returns
 *   int, pthread_attr_getdetachstate((pthread_attr_t const *)attr, detachstate);
 */
int VCodecDrvPthread_attr_setdetachstate(IN VCODEC_PTHREAD_ATTR_T *attr, IN  int detachstate);


/**
 * @par Function
 *   VCodecDrvPthread_create
 * @par Description
 *   The pthread_create wrapper function
 * @param
 *   thread             [OUT] thread
 * @param
 *   attr               [IN] attr
 * @param
 *   start_routine      [IN] start_routine
 * @param
 *   arg                [IN] arg
 * @par Returns
 *   int, pthread_create((pthread_t *)thread, (pthread_attr_t const *)attr, start_routine, arg);
 */
int VCodecDrvPthread_create(OUT VCODEC_PTHREAD_T *thread, IN  const VCODEC_PTHREAD_ATTR_T *attr, IN  void * (*start_routine)(void *), IN  void *arg);


/**
 * @par Function
 *   VCodecDrvPthread_kill
 * @par Description
 *   The pthread_kill wrapper function
 * @param
 *   tid                [IN] tid
 * @param
 *   sig                [IN] sig
 * @par Returns
 *   int, pthread_kill((pthread_t)tid, SIGUSR1);
 */
int VCodecDrvPthread_kill(IN VCODEC_PTHREAD_T tid, IN  int sig);


/**
 * @par Function
 *   VCodecDrvPthread_exit
 * @par Description
 *   The pthread_exit wrapper function
 * @param
 *   retval             [OUT] retval
 * @par Returns
 *   void
 */
void VCodecDrvPthread_exit(OUT void *retval);


/**
 * @par Function
 *   VCodecDrvPthread_join
 * @par Description
 *   The pthread_join wrapper function
 * @param
 *   thid               [IN] thid
 * @param
 *   ret_val            [OUT] ret_val
 * @par Returns
 *   int, pthread_join((pthread_t)thid, ret_val);
 */
int VCodecDrvPthread_join(IN  VCODEC_PTHREAD_T thid, OUT void **ret_val);

//int VCodecDrvPthread_detach(IN VCODEC_PTHREAD_T  thid);


/**
 * @par Function
 *   VCodecDrvPthread_once
 * @par Description
 *   The pthread_once wrapper function
 * @param
 *   once_control       [IN] once_control
 * @param
 *   init_routine       [IN] init_routine
 * @par Returns
 *   int, pthread_once((pthread_once_t *)once_control, init_routine);
 */
int VCodecDrvPthread_once(IN VCODEC_PTHREAD_ONCE_T  *once_control, IN void (*init_routine)(void));


/**
 * @par Function
 *   VCodecDrvPthread_self
 * @par Description
 *   The pthread_self wrapper function
 * @par Returns
 *   VCODEC_PTHREAD_T, (VCODEC_PTHREAD_T)pthread_self()
 */
VCODEC_PTHREAD_T VCodecDrvPthread_self(void);

//VCODEC_OAL_ERROR_T VCodecDrvPthread_equal(IN VCODEC_PTHREAD_T one,IN  VCODEC_PTHREAD_T two);

/***** Mutex Functions ******/


/**
 * @par Function
 *   VCodecDrvPthread_mutexattr_init
 * @par Description
 *   The pthread_mutexattr_init wrapper function
 * @param
 *   attr               [OUT] attr
 * @par Returns
 *   int, pthread_mutexattr_init((pthread_mutexattr_t *)attr);
 */
int VCodecDrvPthread_mutexattr_init(OUT VCODEC_PTHREAD_MUTEXATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_mutexattr_destroy
 * @par Description
 *   The pthread_mutexattr_destroy wrapper function
 * @param
 *   attr               [IN] attr
 * @par Returns
 *   int, pthread_mutexattr_destroy((pthread_mutexattr_t *)attr);
 */
int VCodecDrvPthread_mutexattr_destroy(IN VCODEC_PTHREAD_MUTEXATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_mutex_init
 * @par Description
 *   The pthread_mutex_init wrapper function
 * @param
 *   mutex              [OUT] mutex
 * @param
 *   attr               [IN] attr
 * @par Returns
 *   int, pthread_mutex_init((pthread_mutex_t *)mutex, (const pthread_mutexattr_t *)attr);
 */
int VCodecDrvPthread_mutex_init(OUT VCODEC_PTHREAD_MUTEX_T *mutex, IN  const VCODEC_PTHREAD_MUTEXATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_mutex_destroy
 * @par Description
 *   The pthread_mutex_destroy wrapper function
 * @param
 *   mutex              [IN] mutex
 * @par Returns
 *   int, pthread_mutex_destroy((pthread_mutex_t *)mutex);
 */
int VCodecDrvPthread_mutex_destroy(IN VCODEC_PTHREAD_MUTEX_T *mutex);


/**
 * @par Function
 *   VCodecDrvPthread_mutex_lock
 * @par Description
 *   The pthread_mutex_lock wrapper function
 * @param
 *   mutex              [IN] mutex
 * @par Returns
 *   int, pthread_mutex_lock((pthread_mutex_t *)mutex);
 */
int VCodecDrvPthread_mutex_lock(IN VCODEC_PTHREAD_MUTEX_T *mutex);


/**
 * @par Function
 *   VCodecDrvPthread_mutex_unlock
 * @par Description
 *   The pthread_mutex_unlock wrapper function
 * @param
 *   mutex              [IN] mutex
 * @par Returns
 *   int, pthread_mutex_unlock((pthread_mutex_t *)mutex);
 */
int VCodecDrvPthread_mutex_unlock(IN VCODEC_PTHREAD_MUTEX_T *mutex);


/**
 * @par Function
 *   VCodecDrvPthread_mutex_trylock
 * @par Description
 *   The pthread_mutex_trylock wrapper function
 * @param
 *   mutex              [IN] mutex
 * @par Returns
 *   int, pthread_mutex_trylock((pthread_mutex_t *)mutex);
 */
int VCodecDrvPthread_mutex_trylock(IN VCODEC_PTHREAD_MUTEX_T *mutex);

/***** Spin Functions ******/


/**
 * @par Function
 *   VCodecDrvPthread_spin_init
 * @par Description
 *   The pthread_spin_init wrapper function
 * @param
 *   lock               [OUT] lock
 * @param
 *   pshared            [IN] pshared
 * @par Returns
 *   int, -1, NOT implement
 */
int VCodecDrvPthread_spin_init(OUT VCODEC_PTHREAD_SPINLOCK_T *lock, IN  int pshared);


/**
 * @par Function
 *   VCodecDrvPthread_spin_destroy
 * @par Description
 *   The pthread_spin_destroy wrapper function
 * @param
 *   lock               [IN] lock
 * @par Returns
 *   int, -1, NOT implement
 */
int VCodecDrvPthread_spin_destroy(IN VCODEC_PTHREAD_SPINLOCK_T *lock);


/**
 * @par Function
 *   VCodecDrvPthread_spin_lock
 * @par Description
 *   The pthread_spin_lock wrapper function
 * @param
 *   lock               [IN] lock
 * @par Returns
 *   int, -1, NOT implement
 */
int VCodecDrvPthread_spin_lock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);


/**
 * @par Function
 *   VCodecDrvPthread_spin_trylock
 * @par Description
 *   The pthread_spin_trylock wrapper function
 * @param
 *   lock               [IN] lock
 * @par Returns
 *   int, -1, NOT implement
 */
int VCodecDrvPthread_spin_trylock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);


/**
 * @par Function
 *   VCodecDrvPthread_spin_unlock
 * @par Description
 *   The pthread_spin_unlock wrapper function
 * @param
 *   lock               [IN] lock
 * @par Returns
 *   int, -1, NOT implement
 */
int VCodecDrvPthread_spin_unlock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);

/***** Condition Variable Functions ******/


/**
 * @par Function
 *   VCodecDrvPthread_condattr_init
 * @par Description
 *   The pthread_condattr_init wrapper function
 * @param
 *   attr               [OUT] attr
 * @par Returns
 *   int, pthread_condattr_init((pthread_condattr_t *)attr);
 */
int VCodecDrvPthread_condattr_init(OUT VCODEC_PTHREAD_CONDATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_condattr_destroy
 * @par Description
 *   The pthread_condattr_destroy wrapper function
 * @param
 *   attr               [IN] attr
 * @par Returns
 *   int, pthread_condattr_destroy((pthread_condattr_t *)attr);
 */
int VCodecDrvPthread_condattr_destroy(IN VCODEC_PTHREAD_CONDATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_cond_init
 * @par Description
 *   The pthread_cond_init wrapper function
 * @param
 *   cond               [OUT] cond
 * @param
 *   attr               [IN] attr
 * @par Returns
 *   int, pthread_cond_init((pthread_cond_t *)cond, (const pthread_condattr_t *)attr);
 */
int VCodecDrvPthread_cond_init(OUT VCODEC_PTHREAD_COND_T *cond, IN  const VCODEC_PTHREAD_CONDATTR_T *attr);


/**
 * @par Function
 *   VCodecDrvPthread_cond_destroy
 * @par Description
 *   The pthread_cond_destroy wrapper function
 * @param
 *   cond               [IN] cond
 * @par Returns
 *   int, pthread_cond_destroy((pthread_cond_t *)cond);
 */
int VCodecDrvPthread_cond_destroy(IN VCODEC_PTHREAD_COND_T *cond);


/**
 * @par Function
 *   VCodecDrvPthread_cond_broadcast
 * @par Description
 *   The pthread_cond_broadcast wrapper function
 * @param
 *   cond               [IN] cond
 * @par Returns
 *   int, pthread_cond_broadcast((pthread_cond_t *)cond);
 */
int VCodecDrvPthread_cond_broadcast(IN VCODEC_PTHREAD_COND_T *cond);


/**
 * @par Function
 *   VCodecDrvPthread_cond_signal
 * @par Description
 *   The pthread_cond_signal wrapper function
 * @param
 *   cond               [IN] cond
 * @par Returns
 *   int, pthread_cond_signal((pthread_cond_t *)cond);
 */
int VCodecDrvPthread_cond_signal(IN VCODEC_PTHREAD_COND_T *cond);


/**
 * @par Function
 *   VCodecDrvPthread_cond_signal
 * @par Description
 *   The pthread_cond_wait wrapper function
 * @param
 *   cond               [IN] cond
 * @param
 *   mutex              [IN] mutex
 * @par Returns
 *   int, pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
 */
int VCodecDrvPthread_cond_wait(IN VCODEC_PTHREAD_COND_T *cond, IN  VCODEC_PTHREAD_MUTEX_T *mutex);

/************  End of Multi-thread function ***********/

/***** Semaphore Functions ******/


/**
 * @par Function
 *   VCodecDrv_sem_init
 * @par Description
 *   The sem_init wrapper function
 * @param
 *   sem                [IN] sem
 * @param
 *   pshared            [IN] pshared
 * @param
 *   value              [IN] value
 * @par Returns
 *   int, sem_init((sem_t*)sem, pshared, value);
 */
int VCodecDrv_sem_init(IN VCODEC_OAL_SEM_T *sem, IN int pshared, IN unsigned int value);


/**
 * @par Function
 *   VCodecDrv_sem_destroy
 * @par Description
 *   The sem_destroy wrapper function
 * @param
 *   sem                [IN] sem
 * @par Returns
 *   int, sem_destroy((sem_t*)sem);
 */
int VCodecDrv_sem_destroy(IN VCODEC_OAL_SEM_T *sem);


/**
 * @par Function
 *   VCodecDrv_sem_post
 * @par Description
 *   The sem_post wrapper function
 * @param
 *   sem                [IN] sem
 * @par Returns
 *   int, sem_post((sem_t*)sem);
 */
int VCodecDrv_sem_post(IN VCODEC_OAL_SEM_T *sem);


/**
 * @par Function
 *   VCodecDrv_sem_wait
 * @par Description
 *   The sem_wait wrapper function
 * @param
 *   sem                [IN] sem
 * @par Returns
 *   int, sem_wait((sem_t*)sem);
 */
int VCodecDrv_sem_wait(IN VCODEC_OAL_SEM_T *sem);

/***** Binding Functions ******/


/**
 * @par Function
 *   VCodecDrvBindingCore
 * @par Description
 *   The function used to set given thread to binding specific CPU Core
 * @param
 *   ThreadHandle       [IN] given thread
 * @param
 *   u4Mask             [IN] specific CPU Core
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecDrvBindingCore(IN  VCODEC_PTHREAD_T ThreadHandle, IN  unsigned int u4Mask);


/**
 * @par Function
 *   VCodecDrvDeBindingCore
 * @par Description
 *   The function used to set given thread to debinding specific CPU Core
 * @param
 *   ThreadHandle       [IN] given thread
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecDrvDeBindingCore(IN  VCODEC_PTHREAD_T ThreadHandle);


/**
 * @par Function
 *   VCodecDrvGetAffinity
 * @par Description
 *   The function used to set given thread to get specific CPU Core affinity
 * @param
 *   ThreadHandle       [IN] given thread
 * @param
 *   pu4Mask            [OUT] CPU mask
 * @param
 *   pu4SetMask         [OUT] Set CPU mask
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecDrvGetAffinity(IN  VCODEC_PTHREAD_T ThreadHandle, OUT  unsigned int *pu4Mask, OUT  unsigned int *pu4SetMask);


/**
 * @par Function
 *   VCodecDrvGetAffinity
 * @par Description
 *   The function used to get specific CPU Core loading
 * @param
 *   s4CPUid            [IN] COU id
 * @param
 *   ps4Loading         [OUT] CPU loading
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecDrvCoreLoading(IN  int s4CPUid, OUT int *ps4Loading);


/**
 * @par Function
 *   VCodecDrvGetAffinity
 * @par Description
 *   The function used to get total CPU Core number
 * @param
 *   ps4CPUNums         [OUT] CPU number
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecDrvCoreNumber(OUT int *ps4CPUNums);


/**
 * @par Function
 *   VCodecDrvSleep
 * @par Description
 *   The function used to sleep a while
 * @param
 *   u4Tick             [IN] unit: us
 * @par Returns
 *   void
 */
void VCodecDrvSleep(IN unsigned int u4Tick);


/**
 * @par Function
 *   OAL_SMP_BindingCore
 * @par Description
 *   The function used to set given thread to binding specific CPU Core (only for test)
 * @param
 *   aCurrentTid        [IN] given thread id
 * @param
 *   aCPUid             [IN] specific CPU Core
 * @par Returns
 *   int, return 0 if success, return -1 if failed
 */
int OAL_SMP_BindingCore(int aCurrentTid, int aCPUid);   //ONLY used for TEST in main.c

/***** MCI Functions ******/


/**
 * @par Function
 *   VCodecConfigMCIPort
 * @par Description
 *   The function used to config MCI port
 * @param
 *   u4PortConfig       [IN] port config
 * @param
 *   pu4PortResult      [OUT] port result
 * @param
 *   eCodecType         [OUT] VDEC or VENC
 * @par Returns
 *   VCODEC_OAL_ERROR_T, return VCODEC_OAL_ERROR_NONE if success, return VCODEC_OAL_ERROR_ERROR or VAL_RESULT_UNKNOWN_ERROR if failed
 */
VCODEC_OAL_ERROR_T VCodecConfigMCIPort(IN unsigned int u4PortConfig, OUT unsigned int *pu4PortResult, IN VCODEC_CODEC_TYPE_T eCodecType);


/***** Software vdec lib Functions ******/


/**
 * @par Function
 *   VCodecDrvMemAllocAligned_NC
 * @par Description
 *   The function used to alloc external working memry for non-cacheable
 * @param
 *   hDrv               [IN] codec/driver handle
 * @param
 *   u4Size             [IN] allocated memory size
 * @param
 *   u4AlignSize        [IN] allocated memory byte alignment
 * @param
 *   fgCacheable        [IN] memory type
 * @param
 *   prBuf              [OUT] allocated memory buffer info
 * @par Returns
 *   void
 */
void VCodecDrvMemAllocAligned_NC(IN HANDLE hDrv, IN unsigned int u4Size, unsigned int u4AlignSize, IN VCODEC_MEMORY_TYPE_T fgCacheable, OUT VCODEC_BUFFER_T *prBuf);


/**
 * @par Function
 *   VCodecDrvMemFree_NC
 * @par Description
 *   The function used to free external working memry
 * @param
 *   hDrv               [IN] codec/driver handle
 * @param
 *   prBuf              [IN] allocated memory buffer info
 * @par Returns
 *   void
 */
void VCodecDrvMemFree_NC(IN HANDLE hDrv, IN VCODEC_BUFFER_T *prBuf);


/**
 * @par Function
 *   VDecCodecQueryInfo
 * @par Description
 *   The function used to query info
 * @param
 *   hDrv               [IN] codec/driver handle
 * @param
 *   ID                 [IN] query info type
 * @param
 *   pvQueryData        [OUT] query data
 * @par Returns
 *   void
 */
VCODEC_DEC_ERROR_T VDecCodecQueryInfo(IN HANDLE hDrv, IN VCODEC_DEC_QUERY_INFO_TYPE_T ID, OUT void *pvQueryData);
#if 0
// MACRO

#include "mach/sync_write.h"

#define SP5_REGSYNC_WriteB(v, a) \
    mt65xx_reg_sync_writeb(v, a);

#define SP5_REGSYNC_WriteW(v, a) \
    mt65xx_reg_sync_writew(v, a);

#define SP5_REGSYNC_WriteL(v, a) \
    mt65xx_reg_sync_writel(v, a);


VAL_VOID_T SP5_RegSyncWriteB(VAL_UINT32_T v, VAL_UINT32_T a);
VAL_VOID_T SP5_RegSyncWriteW(VAL_UINT32_T v, VAL_UINT32_T a);
VAL_VOID_T SP5_RegSyncWriteL(VAL_UINT32_T v, VAL_UINT32_T a);
#endif
#endif
