/**
 * @file
 *   val.c
 *
 * @par Project:
 *   MFlexVideo
 *
 * @par Description:
 *   Video Abstraction Layer API implementations
 *
 * @par Author:
 *   Jackal Chen (mtk02532)
 *
 * @par $Revision: #2 $
 * @par $Modtime:$
 * @par $Log:$
 *
 */

/*=============================================================================
 *                              Include Files
 *===========================================================================*/
#include "val_types_private.h"
#include "val_api_private.h"
#include "val_log.h"
//#include "mfv_reg.h"
//#include "mfv_config.h"
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
/*
#define MFV_LOG printk
#define MFV_LOGE printk
#define MFV_LOGD printk
*/
#define MAX_HEAP_SIZE   (0x1000000)
#ifdef _6573_FPGA
#define GMC             0xC8000000
#else /* _6573_REAL_CHIP E1 address */
#define GMC             0x40000000
#endif



/*=============================================================================
 *                              Function Body
 *===========================================================================*/

VAL_RESULT_T eVideoMemAlloc(
    VAL_MEMORY_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{

    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoMemFree(
    VAL_MEMORY_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    //MFV_LOGD("!!Free Mem Size:%d!!\n",a_prParam->u4MemSize);

    return VAL_RESULT_NO_ERROR;

}

//mimic internal memory buffer, 128K bytes.
VAL_RESULT_T eVideoIntMemUsed(
    VAL_INTMEM_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{

    return VAL_RESULT_NO_ERROR;
}

//mimic internal memory buffer, 128K bytes.
VAL_RESULT_T eVideoIntMemAlloc(
    VAL_INTMEM_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{

    return VAL_RESULT_NO_ERROR;
}

//mimic internal memory buffer, 128K bytes.
VAL_RESULT_T eVideoIntMemFree(
    VAL_INTMEM_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{

    return VAL_RESULT_NO_ERROR;
}



VAL_RESULT_T eVideoCreateEvent(
    VAL_EVENT_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
    )
{
    wait_queue_head_t *pWaitQueue;
    VAL_UINT8_T *pFlag;
    pWaitQueue = (wait_queue_head_t *)kmalloc(sizeof(wait_queue_head_t),GFP_ATOMIC);
    pFlag      = (VAL_UINT8_T *)kmalloc(sizeof(VAL_UINT8_T),GFP_ATOMIC);
    if(pWaitQueue != VAL_NULL)
    {
        init_waitqueue_head(pWaitQueue);
        a_prParam->pvWaitQueue = (VAL_VOID_T*)pWaitQueue;
    }
    else
    {
        MFV_LOGE("[MFV][Error]Event wait Queue failed to create\n");
    }
    if(pFlag!= VAL_NULL)
    {
        a_prParam->pvReserved = (VAL_VOID_T *)pFlag;
        *((VAL_UINT8_T *)a_prParam->pvReserved) = VAL_FALSE;
    }
    else
    {
        MFV_LOGE("[MFV][Error]Event flag failed to create\n");
    }

    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoCloseEvent(
    VAL_EVENT_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
    )
{
    wait_queue_head_t *pWaitQueue;
    VAL_UINT8_T *pFlag;
    pWaitQueue = (wait_queue_head_t *)a_prParam->pvWaitQueue;
    pFlag      = (VAL_UINT8_T *)a_prParam->pvReserved;
    if (pWaitQueue)
        kfree(pWaitQueue);
    if (pFlag)
        kfree(pFlag);
    a_prParam->pvWaitQueue = VAL_NULL;
    a_prParam->pvReserved  = VAL_NULL;
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoWaitEvent(
    VAL_EVENT_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    wait_queue_head_t *pWaitQueue;
    long               timeout_jiff, i4Ret;
    VAL_RESULT_T       status;

    pWaitQueue   = (wait_queue_head_t *)a_prParam->pvWaitQueue;
    timeout_jiff = (a_prParam->u4TimeoutMs) * HZ / 1000;
    //MFV_LOGD("[MFV]eVideoWaitEvent,a_prParam->u4TimeoutMs=%d, timeout = %ld\n",a_prParam->u4TimeoutMs,timeout_jiff);
    i4Ret = wait_event_interruptible_timeout(*pWaitQueue, *((VAL_UINT8_T *)a_prParam->pvReserved)/*g_mflexvideo_interrupt_handler*/, timeout_jiff);
    if (0 == i4Ret) {
        status = VAL_RESULT_INVALID_ISR;//timeout
    }
    else if (-ERESTARTSYS == i4Ret) {
        MFV_LOGE("eVideoWaitEvent wake up by ERESTARTSYS");
        status = VAL_RESULT_RESTARTSYS;
    }
    else if (i4Ret > 0) {
        status = VAL_RESULT_NO_ERROR;
    }
    else {
        MFV_LOGE("eVideoWaitEvent wake up by %ld", i4Ret);
        status = VAL_RESULT_NO_ERROR;
    }
    *((VAL_UINT8_T *)a_prParam->pvReserved) = VAL_FALSE;
    return status;
}

VAL_RESULT_T eVideoSetEvent(
    VAL_EVENT_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    wait_queue_head_t *pWaitQueue;
    //MFV_LOGD("[MFV]eVideoSetEvent\n");
    pWaitQueue = (wait_queue_head_t *)a_prParam->pvWaitQueue;
    if (a_prParam->pvReserved!=VAL_NULL)
        *((VAL_UINT8_T *)a_prParam->pvReserved) = VAL_TRUE;
    else
        MFV_LOGE("[MFV][Error]Event flag should not be null\n");
    if (pWaitQueue != VAL_NULL)
    wake_up_interruptible(pWaitQueue);
    else
        MFV_LOGE("[MFV][Error]Wait Queue should not be null\n");
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoCreateMutex(
    VAL_MUTEX_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    struct semaphore *pLock;
    pLock = (struct semaphore *)kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
    if(pLock != VAL_NULL)
        a_prParam->pvMutex = (VAL_VOID_T *)pLock;
    else
        MFV_LOGE("[ERROR]Enable to create mutex!\n");
    //init_MUTEX(pLock);
    sema_init(pLock, 1);
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoCloseMutex(
    VAL_MUTEX_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    if(a_prParam->pvMutex!=VAL_NULL)
        kfree(a_prParam->pvMutex);
    a_prParam->pvMutex = VAL_NULL;
    return VAL_RESULT_NO_ERROR;
}


VAL_RESULT_T eVideoWaitMutex(
    VAL_MUTEX_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    struct semaphore *pLock;
    pLock = (struct semaphore *)a_prParam->pvMutex;
    down(pLock);
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoReleaseMutex(
    VAL_MUTEX_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    struct semaphore *pLock;
    pLock = (struct semaphore *)a_prParam->pvMutex;
    up(pLock);
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoMemSet(
    VAL_MEMORY_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize,
    VAL_INT32_T a_u4Value,
    VAL_UINT32_T a_u4Size
)
{
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoMemCpy(
    VAL_MEMORY_T *a_prParamDst,
    VAL_UINT32_T a_u4ParamDstSize,
    VAL_MEMORY_T *a_prParamSrc,
    VAL_UINT32_T a_u4ParamSrcSize,
    VAL_UINT32_T a_u4Size
)
{
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoMemCmp(
    VAL_MEMORY_T *a_prParamSrc1,
    VAL_UINT32_T a_u4ParamSrc1Size,
    VAL_MEMORY_T *a_prParamSrc2,
    VAL_UINT32_T a_u4ParamSrc2Size,
    VAL_UINT32_T a_u4Size
)
{
    return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eVideoGetTimeOfDay(
    VAL_TIME_T *a_prParam,
    VAL_UINT32_T a_u4ParamSize
)
{
    struct timeval t1;
    do_gettimeofday(&t1);
    a_prParam->u4Sec = t1.tv_sec;
    a_prParam->u4uSec = t1.tv_usec;
    return VAL_RESULT_NO_ERROR;
}

