/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#if !defined( __WLAN_QCT_PAL_API_H )
#define __WLAN_QCT_PAL_API_H

/**=========================================================================
  
  \file  wlan_qct_pal_api.h
  
  \brief define general APIs PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"

#ifdef MEMORY_DEBUG
#include "vos_memory.h"
#endif /* MEMORY_DEBUG */

/*********************************MACRO**********************/

// macro to get maximum of two values.
#define WPAL_MAX( _x, _y ) ( ( (_x) > (_y) ) ? (_x) : (_y) )  

// macro to get minimum of two values
#define WPAL_MIN( _x, _y ) ( ( (_x) < (_y) ) ? (_x) : (_y)  )  

// macro to get the ceiling of an integer division operation...
#define WPAL_CEIL_DIV( _a, _b ) (( 0 != (_a) % (_b) ) ? ( (_a) / (_b) + 1 ) : ( (_a) / (_b) ))

// macro to return the floor of an integer division operation
#define WPAL_FLOOR_DIV( _a, _b ) ( ( (_a) - ( (_a) % (_b) ) ) / (_b) )

#define WPAL_SWAP_U16(_x) \
   ( ( ( (_x) << 8 ) & 0xFF00 ) | ( ( (_x) >> 8 ) & 0x00FF ) )

#define WPAL_SWAP_U32(_x) \
  (( ( ( (_x) << 24 ) & 0xFF000000 ) | ( ( (_x) >> 24 ) & 0x000000FF ) ) | \
   ( ( ( (_x) << 8 ) & 0x00FF0000 ) | ( ( (_x) >> 8 ) & 0x0000FF00 ) ))

// Endian operations for Big Endian and Small Endian modes
#ifndef ANI_BIG_BYTE_ENDIAN

//This portion is for little-endian cpu
#define WPAL_CPU_TO_BE32(_x) WPAL_SWAP_U32(_x)
#define WPAL_BE32_TO_CPU(_x) WPAL_SWAP_U32(_x)
#define WPAL_CPU_TO_BE16(_x) WPAL_SWAP_U16(_x)
#define WPAL_BE16_TO_CPU(_x) WPAL_SWAP_U16(_x)
#define WPAL_CPU_TO_LE32(_x) (_x)
#define WPAL_LE32_TO_CPU(_x) (_x)
#define WPAL_CPU_TO_LE16(_x) (_x)
#define WPAL_LE16_TO_CPU(_x) (_x)

#else //#ifndef ANI_BIG_BYTE_ENDIAN

//This portion is for big-endian cpu
#define WPAL_CPU_TO_BE32(_x) (_x)
#define WPAL_BE32_TO_CPU(_x) (_x)
#define WPAL_CPU_TO_BE16(_x) (_x)
#define WPAL_BE16_TO_CPU(_x) (_x)
#define WPAL_CPU_TO_LE32(_x) WPAL_SWAP_U32(_x)
#define WPAL_LE32_TO_CPU(_x) WPAL_SWAP_U32(_x)
#define WPAL_CPU_TO_LE16(_x) WPAL_SWAP_U16(_x)
#define WPAL_LE16_TO_CPU(_x) WPAL_SWAP_U16(_x)

#endif //#ifndef ANI_BIG_BYTE_ENDIAN


/*********************************Generic API*******************************/
/*---------------------------------------------------------------------------
    wpalOpen -  Initialize PAL
    Param: 
       ppPalContext – pointer to a caller allocated pointer. It is opaque to caller.
                      Caller save the returned pointer for future use when calling
                      PAL APIs. If this is NULL, it means that PAL doesn't need it.
       pOSContext - Pointer to a context that is OS specific. This is NULL is a 
                     particular PAL doesn't use it for that OS.
    Return:
       eWLAN_PAL_STATUS_SUCCESS - success. Otherwise fail.
---------------------------------------------------------------------------*/
wpt_status wpalOpen(void **ppPalContext, void *pOSContext);

/*---------------------------------------------------------------------------
    wpalClose - Release PAL
    Param: 
       pPalContext – pointer returned from wpalOpen.
    Return:
       eWLAN_PAL_STATUS_SUCCESS - success. Otherwise fail.
---------------------------------------------------------------------------*/
wpt_status wpalClose(void *pPalContext);


/*********************************Memory API********************************/
#ifdef MEMORY_DEBUG
/* For Memory Debugging, Hook up PAL memory API to VOS memory API */
#define wpalMemoryAllocate vos_mem_malloc
#define wpalMemoryFree     vos_mem_free
#else

/*---------------------------------------------------------------------------
    wpalMemoryAllocate -  Allocate memory
    Param: 
       size – number of bytes to allocate
    Return:
       A pointer to the allocated memory. 
       NULL – fail to allocate memory
---------------------------------------------------------------------------*/
void *wpalMemoryAllocate(wpt_uint32 size);

/*---------------------------------------------------------------------------
    wpalMemoryFree -  Free allocated memory
    Param: 
       pv – pointer to buffer to be freed
    Return:
       None
---------------------------------------------------------------------------*/
void wpalMemoryFree(void *pv);
#endif /* MEMORY_DEBUG */

/*---------------------------------------------------------------------------
    wpalMemoryCopy -  copy memory
    Param: 
       dest – address which data is copied to
       src – address which data is copied from
       size – number of bytes to copy
    Return:
       eWLAN_PAL_STATUS_SUCCESS
       eWLAN_PAL_STATUS_INVALID_PARAM
---------------------------------------------------------------------------*/
wpt_status wpalMemoryCopy(void * dest, void * src, wpt_uint32 size);


/*---------------------------------------------------------------------------
    wpalMemoryCompare -  compare memory
    Param: 
       buf1 – address of buffer1
       buf2 – address of buffer2
       size – number of bytes to compare
    Return:
       eWLAN_PAL_TRUE – if two buffers have same content
       eWLAN_PAL_FALSE – not match
---------------------------------------------------------------------------*/
wpt_boolean wpalMemoryCompare(void * buf1, void * buf2, wpt_uint32 size);

/*---------------------------------------------------------------------------
    wpalMemoryZero -  Zero memory
    Param: 
       buf – address of buffer to be zero
       size – number of bytes to zero
    Return:
       None
---------------------------------------------------------------------------*/
void wpalMemoryZero(void *buf, wpt_uint32 size);


/*---------------------------------------------------------------------------
    wpalMemoryFill -  Fill memory with one pattern
    Param: 
       buf – address of buffer to be zero
       size – number of bytes to zero
       bFill - one byte of data to fill in (size) bytes from the start of the buffer
    Return:
       None
---------------------------------------------------------------------------*/
void wpalMemoryFill(void *buf, wpt_uint32 size, wpt_byte bFill);


/*---------------------------------------------------------------------------
    wpalDmaMemoryAllocate -  Allocate memory ready for DMA. Aligned at 4-byte
    Param: 
       pPalContext - PAL context pointer
       size – number of bytes to allocate
       ppPhysicalAddr – Physical address of the buffer if allocation succeeds
    Return:
       A pointer to the allocated memory (virtual address). 
       NULL – fail to allocate memory
-----------------------------------------------------------------------------*/
void *wpalDmaMemoryAllocate(wpt_uint32 size, void **ppPhysicalAddr);

/*---------------------------------------------------------------------------
    wpalDmaMemoryFree -  Free memory ready for DMA
    Param: 
       pPalContext - PAL context pointer
       pv – address for the buffer to be freed
    Return:
       None
---------------------------------------------------------------------------*/
void wpalDmaMemoryFree(void *pv);



/*---------------------------------------------------------------------------
    wpalDbgReadRegister -  Read register from the WiFi BB chip
    Param: 
       regAddr - register address
       pregValue - return value from register if success
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDbgReadRegister(wpt_uint32 regAddr, wpt_uint32 *pregValue);

/*---------------------------------------------------------------------------
    wpalDbgWriteRegister -  Write a value to the register in the WiFi BB chip
    Param: 
       regAddr - register address
       regValue - value to be written
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDbgWriteRegister(wpt_uint32 regAddr, wpt_uint32 regValue);

/*---------------------------------------------------------------------------
    wpalDbgReadMemory -  Read memory from WiFi BB chip space
    Param: 
       memAddr - address of memory
       buf - output 
       len - length to be read
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDbgReadMemory(wpt_uint32 memAddr, wpt_uint8 *buf, wpt_uint32 len);

/*---------------------------------------------------------------------------
    wpalDbgWriteMemory -  Write a value to the memory in the WiFi BB chip space
    Param: 
       memAddr - memory address
       buf - vlaue to be written
       len - length of buf
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDbgWriteMemory(wpt_uint32 memAddr, wpt_uint8 *buf, wpt_uint32 len);

/*---------------------------------------------------------------------------
    wpalDriverShutdown -  Shutdown WLAN driver

    This API is requied by SSR, call in to 'VOS shutdown' to shutdown WLAN 
    driver when Riva crashes.

    Param: 
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDriverShutdown(void);

/*---------------------------------------------------------------------------
    wpalDriverShutdown -  Re-init WLAN driver

    This API is requied by SSR, call in to 'VOS re-init' to re-init WLAN
    driver.

    Param: 
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalDriverReInit(void);

/*---------------------------------------------------------------------------
    wpalRivaSubystemRestart -  Initiate Riva SSR

    This API is called by WLAN driver to initiate Riva SSR

    Param:
       None
    Return:
       eWLAN_PAL_STATUS_SUCCESS - when everything is OK
---------------------------------------------------------------------------*/
wpt_status wpalRivaSubystemRestart(void);

/*---------------------------------------------------------------------------
    wpalWlanReload -  Initiate WLAN Driver reload

    Param:
       None
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalWlanReload(void);

/*---------------------------------------------------------------------------
    wpalWcnssResetIntr -  Trigger the reset FIQ to Riva

    Param:
       None
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalWcnssResetIntr(void);

/*---------------------------------------------------------------------------
    wpalFwDumpReq -  Trigger the dump commands to Firmware

    Param:
       cmd - Command No. to execute
       arg1 - argument 1 to cmd
       arg2 - argument 2 to cmd
       arg3 - argument 3 to cmd
       arg4 - argument 4 to cmd
    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalFwDumpReq(wpt_uint32 cmd, wpt_uint32 arg1, wpt_uint32 arg2,
                    wpt_uint32 arg3, wpt_uint32 arg4);

/*---------------------------------------------------------------------------
    wpalDevicePanic -  Trigger Device Panic
       Trigger device panic to help debug

    Param:
       NONE

    Return:
       NONE
---------------------------------------------------------------------------*/
void wpalDevicePanic(void);

/*---------------------------------------------------------------------------
    wpalIsWDresetInProgress -  calls vos API isWDresetInProgress()

    Param:
       NONE
    Return:
       STATUS
--------------------------------------------------------------------------*/
int  wpalIsWDresetInProgress(void);
#endif // __WLAN_QCT_PAL_API_H
