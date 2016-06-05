/*
 * Copyright (c) 2012,2014-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**=========================================================================
  
  \file  wlan_qct_pal_api.c
  
  \brief Implementation general APIs PAL exports.
   wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform Windows.
  
  
  ========================================================================*/

#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_trace.h"
#include "wlan_qct_pal_device.h"
#include "vos_trace.h"
#ifndef MEMORY_DEBUG
#include "vos_memory.h"
#endif /* MEMORY_DEBUG */
#include "vos_sched.h"
#include "vos_api.h"

#include "dma-mapping.h"
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#include <soc/qcom/subsystem_restart.h>
#else
#include <mach/subsystem_restart.h>
#endif
#include <linux/wcnss_wlan.h>


#define WPAL_GET_NDIS_HANDLE(p)  ( ((tPalContext *)(p))->devHandle )

tPalContext gContext;

//This structure need to be 4-byte aligned. No packing.
typedef struct
{
   wpt_uint32 length;
   //The offset from beginning of the buffer where it is allocated
   wpt_uint32 offset;   
   wpt_uint32 phyAddr;
} tPalDmaMemInfo;

/*===========================================================================

                            FUNCTIONS

===========================================================================*/

/**
 * @brief Initialize PAL
 *        In case of QNP, this does nothing.
 * @param ppPalContext pointer to a caller allocated pointer. It 
 *                     is opaque to caller.
 *                    Caller save the returned pointer for future use when
 *                    calling PAL APIs.
 * @param devHandle pointer to the OS specific device handle.
 * 
 * @return wpt_status eWLAN_PAL_STATUS_SUCCESS - success. Otherwise fail.
 */
wpt_status wpalOpen(void **ppPalContext, void *devHandle)
{
   wpt_status status;

   gContext.devHandle = devHandle;

   status = wpalDeviceInit(devHandle);
   if (!WLAN_PAL_IS_STATUS_SUCCESS(status))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: wpalDeviceInit failed with status %u",
                 __func__, status);
   }

   return status;
}

/**
 * @brief wpalClose - Release PAL
 *                    In case of QNP, this does nothing.
 * @param pPalContext pointer returned from wpalOpen.
 * 
 * @return wpt_status eWLAN_PAL_STATUS_SUCCESS - success. Otherwise fail.
 */
wpt_status wpalClose(void *pPalContext)
{
   wpalDeviceClose(gContext.devHandle);
   gContext.devHandle = NULL;

   return eWLAN_PAL_STATUS_SUCCESS;
}

#ifndef MEMORY_DEBUG
/**
 * @brief wpalMemoryAllocate -  Allocate memory
 * @param size number of bytes to allocate
 * 
 * @return void* A pointer to the allocated memory.
 * NULL - fail to allocate memory 
 */
void *wpalMemoryAllocate(wpt_uint32 size)
{
   return vos_mem_malloc( size );
}

/**
 * @brief wpalMemoryFree -  Free allocated memory
 * @param pv pointer to buffer to be freed
 */
void wpalMemoryFree(void *pv)
{
   vos_mem_free( pv );
}
#endif /* MEMORY_DEBUG */
/**
 * @brief wpalMemoryCopy -  copy memory
 * @param dest address which data is copied to
 * @param src address which data is copied from
 * @param size number of bytes to copy
 * 
 * @return wpt_status
 *         eWLAN_PAL_STATUS_SUCCESS
 *         eWLAN_PAL_STATUS_INVALID_PARAM
 */
wpt_status wpalMemoryCopy(void * dest, void * src, wpt_uint32 size)
{
   vos_mem_copy( dest, src, size );

   return eWLAN_PAL_STATUS_SUCCESS;
}

/**
 * @brief wpalMemoryCompare -  compare memory
 * @param buf1 address of buffer1
 * @param buf2 address of buffer2
 * @param size number of bytes to compare
 * 
 * @return wpt_boolean
 *        eWLAN_PAL_TRUE - if two buffers have same content
 *        eWLAN_PAL_FALSE - not match
 */
wpt_boolean wpalMemoryCompare(void * buf1, void * buf2, wpt_uint32 size)
{
   return (wpt_boolean)vos_mem_compare( buf1, buf2, size );
}


/*---------------------------------------------------------------------------
    wpalMemoryZero -  Zero memory
    Param: 
       buf - address of buffer to be zero
       size - number of bytes to zero
    Return:
       None
---------------------------------------------------------------------------*/
void wpalMemoryZero(void *buf, wpt_uint32 size)
{
   vos_mem_zero( buf, size );
}

/**
 * @brief wpalMemoryFill -  Fill memory with one pattern
 * @param buf address of buffer to be filled
 * @param size number of bytes to fill
 * @param bFill one byte of data to fill in (size) bytes from the start of the 
 * buffer
 */
void wpalMemoryFill(void *buf, wpt_uint32 size, wpt_byte bFill)
{
   vos_mem_set( buf, size, bFill );
}

/**
 * @brief wpalDmaMemoryAllocate -  Allocate memory ready for DMA. Aligned at 4-byte
 * @param size number of bytes to allocate
 * @param ppPhysicalAddr Physical address of the buffer if allocation succeeds
 * 
 * @return void* A pointer to the allocated memory (virtual address). 
 *               NULL - fail to allocate memory
 */
void *wpalDmaMemoryAllocate(wpt_uint32 size, void **ppPhysicalAddr)
{
   struct device *wcnss_device = (struct device *) gContext.devHandle;
   void *pv = NULL;
   dma_addr_t PhyAddr;
   wpt_uint32 uAllocLen = size + sizeof(tPalDmaMemInfo);
   
   pv = dma_alloc_coherent(wcnss_device, uAllocLen, &PhyAddr, GFP_KERNEL);
   if ( NULL == pv ) 
   {
     WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 "%s Unable to allocate DMA buffer", __func__);
     return NULL;
   }
   wpalMemoryZero(pv, uAllocLen);

   
   ((tPalDmaMemInfo *)pv)->length  = uAllocLen;
   ((tPalDmaMemInfo *)pv)->phyAddr = PhyAddr;
   ((tPalDmaMemInfo *)pv)->offset  = sizeof(tPalDmaMemInfo);
   pv                              = (wpt_byte *)pv + sizeof(tPalDmaMemInfo);
   *ppPhysicalAddr                 = (void*)PhyAddr + sizeof(tPalDmaMemInfo);


   return (pv);
}/*wpalDmaMemoryAllocate*/


/**
 * @brief wpalDmaMemoryFree -  Free memory ready for DMA
 * @param pv address for the buffer to be freed
 */
void wpalDmaMemoryFree(void *pv)
{
   struct device *wcnss_device = (struct device *) gContext.devHandle;

   tPalDmaMemInfo *pMemInfo = (tPalDmaMemInfo *)(((wpt_byte *)pv) -
                                      sizeof(tPalDmaMemInfo));
    if(pv)
    { 
        pv = (wpt_byte *)pv - pMemInfo->offset;
        dma_free_coherent(wcnss_device, pMemInfo->length, pv, pMemInfo->phyAddr);
    }

}/*wpalDmaMemoryFree*/

/**
 * @brief wpalDbgReadRegister -  Read register from the WiFi BB 
              chip
 * @param regAddr - register address
 * @param  pregValue - return value from register if success
 * @return
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
 */
wpt_status wpalDbgReadRegister(wpt_uint32 regAddr, wpt_uint32 *pregValue)
{
   if (NULL == pregValue)
   {
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   return wpalReadRegister(regAddr, pregValue);
}

/** 
 * @brief wpalDbgWriteRegister -  Write a value to the register
 *  in the WiFi BB chip Param:
 * @param regAddr - register address
 * @param regValue - value to be written
 * @return
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
*/
wpt_status wpalDbgWriteRegister(wpt_uint32 regAddr, wpt_uint32 regValue)
{
   return wpalWriteRegister(regAddr, regValue);
}

/** 
 * @brief 
     wpalDbgReadMemory -  Read memory from WiFi BB chip space
 * @param memAddr - address of memory 
 * @param buf - output 
 * @param len - length to be read
 * @return
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
*/
wpt_status wpalDbgReadMemory(wpt_uint32 memAddr, wpt_uint8 *buf, wpt_uint32 len)
{
   return wpalReadDeviceMemory(memAddr, buf, len);
}

/** 
 * @brief 
    wpalDbgWriteMemory -  Write a value to the memory in the WiFi BB chip space
 * @param memAddr - memory address 
 * @param buf - vlaue to be written
 * @param len - length of buf
 * @return
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
*/
wpt_status wpalDbgWriteMemory(wpt_uint32 memAddr, wpt_uint8 *buf, wpt_uint32 len)
{
   return wpalWriteDeviceMemory(memAddr, buf, len);
}

/*---------------------------------------------------------------------------
    wpalDriverShutdown -  Shutdown WLAN driver

    This API is requied by SSR, call in to 'VOS shutdown' to shutdown WLAN 
    driver when Riva crashes.

    Param: 
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDriverShutdown(void)
{
    VOS_STATUS vosStatus;
    vosStatus = vos_wlanShutdown();

    if (VOS_STATUS_SUCCESS == vosStatus) {
        return eWLAN_PAL_STATUS_SUCCESS; 
    }
    return eWLAN_PAL_STATUS_E_FAILURE; 
}

/*---------------------------------------------------------------------------
    wpalDriverShutdown -  Re-init WLAN driver

    This API is requied by SSR, call in to 'VOS re-init' to re-init WLAN
    driver.

    Param: 
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDriverReInit(void)
{
    VOS_STATUS vosStatus;

    vosStatus = vos_wlanReInit();
    if (VOS_STATUS_SUCCESS == vosStatus) {
        return eWLAN_PAL_STATUS_SUCCESS; 
    }
    return eWLAN_PAL_STATUS_E_FAILURE; 
}

/*---------------------------------------------------------------------------
    wpalRivaSubystemRestart -  Initiate Riva SSR

    This API is called by WLAN driver to initiate Riva SSR

    Param:
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalRivaSubystemRestart(void)
{
    /* call SSR only if driver is not in load/unload process.
     * A WDI timeout during load/unload cannot be fixed thru
     * SSR */
    if (vos_is_load_unload_in_progress(VOS_MODULE_ID_WDI, NULL))
    {
         WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: loading/unloading in progress,"
                 " SSR will be done at the end of unload", __func__);
         return eWLAN_PAL_STATUS_E_FAILURE;
    }
    if (0 == subsystem_restart("wcnss")) 
    {
        return eWLAN_PAL_STATUS_SUCCESS;
    }
    return eWLAN_PAL_STATUS_E_FAILURE;
}

/*---------------------------------------------------------------------------
    wpalWlanReload -  Initiate WLAN Driver reload

    Param:
       None
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalWlanReload(void)
{
   vos_wlanRestart();
   return;
}

/*---------------------------------------------------------------------------
    wpalWcnssResetIntr -  Trigger the reset FIQ to Riva

    Param:
       None
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalWcnssResetIntr(void)
{
#ifdef HAVE_WCNSS_RESET_INTR
   wcnss_reset_fiq(true);
#endif
   return;
}

/*---------------------------------------------------------------------------
    wpalWcnssIsProntoHwVer3 -  Check if Pronto Hw ver3

    Param:
       None
    Return:
       TRUE if Ponto Hw Ver 3
       Therefore use WQ6 instead of WQ23 for TX Low/High Priority Channel
---------------------------------------------------------------------------*/
int wpalWcnssIsProntoHwVer3(void)
{
   return wcnss_is_hw_pronto_ver3();
}

/*---------------------------------------------------------------------------
    wpalIsFwLoggingEnabled -  Check if Firmware will send logs using DXE

    Param:
       None
    Return:
        Check the documentation of vos_is_fw_logging_enabled
---------------------------------------------------------------------------*/
wpt_uint8 wpalIsFwLoggingEnabled(void)
{
  return vos_is_fw_logging_enabled();
}

/*---------------------------------------------------------------------------
    wpalIsFwLoggingEnabled -  Check if Firmware will send running
                              logs using DXE

    Param:
       None
    Return:
        Check the documentation of vos_is_fw_logging_enabled
---------------------------------------------------------------------------*/
wpt_uint8 wpalIsFwEvLoggingEnabled(void)
{
  return vos_is_fw_ev_logging_enabled();
}
/*---------------------------------------------------------------------------
    wpalIsFwLoggingSupported -  Check if Firmware supports the fw->host
                                logging infrastructure
                                This API can only be called after fw caps
                                are exchanged.

    Param:
       None
    Return:
        Check the documentation of vos_is_fw_logging_supported
---------------------------------------------------------------------------*/
wpt_uint8 wpalIsFwLoggingSupported(void)
{
  return vos_is_fw_logging_supported();
}

/*---------------------------------------------------------------------------
    wpalFwDumpReq -  Trigger the dump commands to Firmware
     
    Param:
       cmd -   Command No. to execute
       arg1 -  argument 1 to cmd
       arg2 -  argument 2 to cmd
       arg3 -  argument 3 to cmd
       arg4 -  argument 4 to cmd
       async -asynchronous event. Don't wait for completion.
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalFwDumpReq(wpt_uint32 cmd, wpt_uint32 arg1, wpt_uint32 arg2,
                    wpt_uint32 arg3, wpt_uint32 arg4, wpt_boolean async)
{
   vos_fwDumpReq(cmd, arg1, arg2, arg3, arg4, async);
   return;
}

/*---------------------------------------------------------------------------
    wpalDevicePanic -  Trigger Device Panic
       Trigger device panic to help debug

    Param:
       NONE

    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalDevicePanic(void)
{
   BUG_ON(1);
   return;
}
/*---------------------------------------------------------------------------
    wpalIslogPInProgress -  calls vos API vos_is_logp_in_progress()

    Param:
       NONE
    Return:
       STATUS
 ---------------------------------------------------------------------------*/
int  wpalIslogPInProgress(void)
{
   return vos_is_logp_in_progress(VOS_MODULE_ID_WDI, NULL);
}

/*---------------------------------------------------------------------------
    wpalIsSsrPanicOnFailure -  calls vos API isSsrPanicOnFailure()

    Param:
       NONE
    Return:
       STATUS
 ---------------------------------------------------------------------------*/
int  wpalIsSsrPanicOnFailure(void)
{
   return isSsrPanicOnFailure();
}

int  wpalGetDxeReplenishRXTimerVal(void)
{
   return vos_get_dxeReplenishRXTimerVal();
}

int  wpalIsDxeSSREnable(void)
{
   return vos_get_dxeSSREnable();
}

