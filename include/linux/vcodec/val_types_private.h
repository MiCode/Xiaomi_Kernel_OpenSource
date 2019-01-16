/**
 * @file
 *   val_types_private.h
 *
 * @par Project:
 *   Video
 *
 * @par Description:
 *   Video Abstraction Layer Type Definitions for internal use
 *
 * @par Author:
 *   Jackal Chen (mtk02532)
 *
 * @par $Revision: #1 $
 * @par $Modtime:$
 * @par $Log:$
 *
 */

#ifndef _VAL_TYPES_PRIVATE_H_
#define _VAL_TYPES_PRIVATE_H_

#include "val_types_public.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define __EARLY_PORTING__

#define OALMEM_STATUS_NUM 16

/**
 * @par Enumeration
 *   VAL_HW_COMPLETE_T
 * @par Description
 *   This is polling or interrupt for waiting for HW done
 */
typedef enum _VAL_HW_COMPLETE_T
{
    VAL_POLLING_MODE = 0,                       ///< polling
    VAL_INTERRUPT_MODE,                         ///< interrupt
    VAL_MODE_MAX = 0xFFFFFFFF                   ///< Max result
}
VAL_HW_COMPLETE_T;


/**
 * @par Enumeration
 *   VAL_CODEC_TYPE_T
 * @par Description
 *   This is the item in VAL_OBJECT_T for open driver type and
 *                    in VAL_CLOCK_T for clock setting and
 *                    in VAL_ISR_T for irq line setting
 */
typedef enum _VAL_CODEC_TYPE_T
{
    VAL_CODEC_TYPE_NONE = 0,                    ///< None
    VAL_CODEC_TYPE_MP4_ENC,                     ///< MP4 encoder
    VAL_CODEC_TYPE_MP4_DEC,                     ///< MP4 decoder
    VAL_CODEC_TYPE_H263_ENC,                    ///< H.263 encoder
    VAL_CODEC_TYPE_H263_DEC,                    ///< H.263 decoder
    VAL_CODEC_TYPE_H264_ENC,                    ///< H.264 encoder
    VAL_CODEC_TYPE_H264_DEC,                    ///< H.264 decoder
    VAL_CODEC_TYPE_SORENSON_SPARK_DEC,          ///< Sorenson Spark decoder
    VAL_CODEC_TYPE_VC1_SP_DEC,                  ///< VC-1 simple profile decoder
    VAL_CODEC_TYPE_RV9_DEC,                     ///< RV9 decoder
    VAL_CODEC_TYPE_MP1_MP2_DEC,                 ///< MPEG1/2 decoder
    VAL_CODEC_TYPE_XVID_DEC,                    ///< Xvid decoder
    VAL_CODEC_TYPE_DIVX4_DIVX5_DEC,             ///< Divx4/5 decoder
    VAL_CODEC_TYPE_VC1_MP_WMV9_DEC,             ///< VC-1 main profile (WMV9) decoder
    VAL_CODEC_TYPE_RV8_DEC,                     ///< RV8 decoder
    VAL_CODEC_TYPE_WMV7_DEC,                    ///< WMV7 decoder
    VAL_CODEC_TYPE_WMV8_DEC,                    ///< WMV8 decoder
    VAL_CODEC_TYPE_AVS_DEC,                     ///< AVS decoder
    VAL_CODEC_TYPE_DIVX_3_11_DEC,               ///< Divx3.11 decoder
    VAL_CODEC_TYPE_H264_DEC_MAIN,               ///< H.264 main profile decoder (due to different packet) == 20
    VAL_CODEC_TYPE_MAX = 0xFFFFFFFF             ///< Max driver type
} VAL_CODEC_TYPE_T;


typedef enum _VAL_CACHE_TYPE_T
{

    VAL_CACHE_TYPE_CACHABLE = 0,
    VAL_CACHE_TYPE_NONCACHABLE,
    VAL_CACHE_TYPE_MAX = 0xFFFFFFFF

} VAL_CACHE_TYPE_T;


/**
 * @par Structure
 *  VAL_INTMEM_T
 * @par Description
 *  This is a parameter for eVideoIntMemUsed()
 */
typedef struct _VAL_INTMEM_T
{
    VAL_VOID_T      *pvHandle;                  ///< [IN]     The video codec driver handle
    VAL_UINT32_T    u4HandleSize;               ///< [IN]     The size of video codec driver handle
    VAL_UINT32_T    u4MemSize;                  ///< [OUT]    The size of internal memory
    VAL_VOID_T      *pvMemVa;                   ///< [OUT]    The internal memory start virtual address
    VAL_VOID_T      *pvMemPa;                   ///< [OUT]    The internal memory start physical address
    VAL_VOID_T      *pvReserved;                ///< [IN/OUT] The reserved parameter
    VAL_UINT32_T    u4ReservedSize;             ///< [IN]     The size of reserved parameter structure
} VAL_INTMEM_T;


/**
 * @par Structure
 *  VAL_EVENT_T
 * @par Description
 *  This is a parameter for eVideoWaitEvent() and eVideoSetEvent()
 */
typedef struct _VAL_EVENT_T
{
    VAL_VOID_T      *pvHandle;                  ///< [IN]     The video codec driver handle
    VAL_UINT32_T    u4HandleSize;               ///< [IN]     The size of video codec driver handle
    VAL_VOID_T      *pvWaitQueue;               ///< [IN]     The waitqueue discription
    VAL_VOID_T      *pvEvent;                   ///< [IN]     The event discription
    VAL_UINT32_T    u4TimeoutMs;                ///< [IN]     The timeout ms
    VAL_VOID_T      *pvReserved;                ///< [IN/OUT] The reserved parameter
    VAL_UINT32_T    u4ReservedSize;             ///< [IN]     The size of reserved parameter structure
} VAL_EVENT_T;


/**
 * @par Structure
 *  VAL_MUTEX_T
 * @par Description
 *  This is a parameter for eVideoWaitMutex() and eVideoReleaseMutex()
 */
typedef struct _VAL_MUTEX_T
{
    VAL_VOID_T      *pvHandle;                  ///< [IN]     The video codec driver handle
    VAL_UINT32_T    u4HandleSize;               ///< [IN]     The size of video codec driver handle
    VAL_VOID_T      *pvMutex;                   ///< [IN]     The Mutex discriptor
    VAL_UINT32_T    u4TimeoutMs;                ///< [IN]     The timeout ms
    VAL_VOID_T      *pvReserved;                ///< [IN/OUT] The reserved parameter
    VAL_UINT32_T    u4ReservedSize;             ///< [IN]     The size of reserved parameter structure
} VAL_MUTEX_T;


/**
 * @par Structure
 *  VAL_POWER_T
 * @par Description
 *  This is a parameter for eVideoHwPowerCtrl()
 */
typedef struct _VAL_POWER_T
{
    VAL_VOID_T          *pvHandle;              ///< [IN]     The video codec driver handle
    VAL_UINT32_T        u4HandleSize;           ///< [IN]     The size of video codec driver handle
    VAL_DRIVER_TYPE_T   eDriverType;            ///< [IN]     The driver type
    VAL_BOOL_T          fgEnable;               ///< [IN]     Enable or not.
    VAL_VOID_T          *pvReserved;            ///< [IN/OUT] The reserved parameter
    VAL_UINT32_T        u4ReservedSize;         ///< [IN]     The size of reserved parameter structure
    //VAL_UINT32_T        u4L2CUser;              ///< [OUT]    The number of power user right now
} VAL_POWER_T;


/**
 * @par Structure
 *  VAL_MMAP_T
 * @par Description
 *  This is a parameter for eVideoMMAP() and eVideoUNMAP()
 */
typedef struct _VAL_MMAP_T
{
    VAL_VOID_T      *pvHandle;                  ///< [IN]     The video codec driver handle
    VAL_UINT32_T    u4HandleSize;               ///< [IN]     The size of video codec driver handle
    VAL_VOID_T      *pvMemPa;                   ///< [IN]     The physical memory address
    VAL_UINT32_T    u4MemSize;                  ///< [IN]     The memory size
    VAL_VOID_T      *pvMemVa;                   ///< [IN]     The mapped virtual memory address
    VAL_VOID_T      *pvReserved;                ///< [IN/OUT] The reserved parameter
    VAL_UINT32_T    u4ReservedSize;             ///< [IN]     The size of reserved parameter structure
} VAL_MMAP_T;


typedef struct
{
    VAL_UINT32_T    u4ReadAddr;                 /// [IN]  memory source address in VA
    VAL_UINT32_T    u4ReadData;                 /// [OUT] memory data
} VAL_VCODEC_OAL_MEM_STAUTS_T;


typedef struct
{
    VAL_UINT32_T    u4HWIsCompleted;            ///< [IN/OUT] HW is Completed or not, set by driver & clear by codec (0: not completed or still in lock status; 1: HW is completed or in unlock status)
    VAL_UINT32_T    u4HWIsTimeout;              ///< [OUT]    HW is Timeout or not, set by driver & clear by codec (0: not in timeout status; 1: HW is in timeout status)
    VAL_UINT32_T    u4NumOfRegister;            ///< [IN]     Number of HW register need to store;
    VAL_VCODEC_OAL_MEM_STAUTS_T *pHWStatus;     ///< [OUT]    HW status based on input address.
} VAL_VCODEC_OAL_HW_REGISTER_T;


typedef struct
{
    VAL_VCODEC_OAL_HW_REGISTER_T    *Oal_HW_reg;
    VAL_UINT32_T                    *Oal_HW_mem_reg;
    VAL_UINT32_T                    *kva_Oal_HW_mem_reg;
    VAL_UINT32_T                    pa_Oal_HW_mem_reg;
    VAL_ULONG_T                     ObjId;
    VAL_EVENT_T                     IsrEvent;
    VAL_UINT32_T                    slotindex;
    VAL_UINT32_T                    u4VCodecThreadNum;
    VAL_UINT32_T                    u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
    VAL_HANDLE_T                    pvHandle;   // physical address of the owner handle
    VAL_UINT32_T                    u4NumOfRegister;
    VAL_VCODEC_OAL_MEM_STAUTS_T     oalmem_status[OALMEM_STATUS_NUM];  // MAX 16 items could be read; //kernel space access register
    VAL_UINT32_T                    kva_u4HWIsCompleted;
    VAL_UINT32_T                    kva_u4HWIsTimeout;
    VAL_UINT32_T                    tid1;
    VAL_UINT32_T                    tid2;

    // record VA, PA
    VAL_UINT32_T                    *va1;
    VAL_UINT32_T                    *va2;
    VAL_UINT32_T                    *va3;
    VAL_UINT32_T                    pa1;
    VAL_UINT32_T                    pa2;
    VAL_UINT32_T                    pa3;

} VAL_VCODEC_OAL_HW_CONTEXT_T;


typedef struct
{
    int     CPUid;                              //  [in]
    int     Loading;                            //  [out]
} VAL_VCODEC_CORE_LOADING_T;

typedef void (*ena)(int);
typedef void (*disa)(int);
typedef void (*ena_timeout)(int, int);
typedef int (*user_reg)(int, int);
typedef void (*user_unreg)(int);
typedef void (*user_enable)(int);
typedef void (*user_disable)(int);
typedef void (*user_enable_timeout)(int, int);

typedef struct _VAL_INIT_HANDLE
{
    int i4DriverType;
    int i4VENCLivePhoto;
} VAL_INIT_HANDLE;
#ifdef __cplusplus
}
#endif

#endif // #ifndef _VAL_TYPES_PRIVATE_H_
