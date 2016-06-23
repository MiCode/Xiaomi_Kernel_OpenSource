/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

#ifndef __VOSS_WRAPPER_H
#define __VOSS_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
  @file VossWrapper.h

  @brief This header file contains the various structure definitions and
  function prototypes for the RTOS abstraction layer, implemented for VOSS

  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  12/15/08    sho    Resolved warnings and errors from AMSS compiler when
                     this is ported to WM
  11/20/08    sho    Renamed this to VossWrapper.h; remove all dependencies
                     on WM platform and allow this to work on all VOSS enabled
                     platforms
  06/24/08    tbh    Modified the file to remove the dependecy on HDD files as
                     part of Gen6 bring up process.
  10/29/02 Neelay Das Created file.

===========================================================================*/

/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/

#include "sirTypes.h"
#include "sirParams.h"
#include "sysDef.h"
#include "aniDbgTest.h"
#include "vos_timer.h"
#include "palApi.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "vos_memory.h"

/* Interlocked Compare Exchange related definitions */



/* Define basic constants for the ThreadX kernel.  */

#define TX_NO_WAIT          0
#define TX_WAIT_FOREVER     0xFFFFFFFFUL
#define TX_AUTO_ACTIVATE    1
#define TX_NO_ACTIVATE      0



/* API return values.  */
#define TX_SUCCESS          0x00
#define TX_QUEUE_FULL    0x01
// ...
#define TX_NO_INSTANCE      0x0D
// ...
#define TX_TIMER_ERROR      0x15
#define TX_TICK_ERROR       0x16
// ...


#ifndef true
#define true                1
#endif

#ifndef false
#define false               0
#endif

/* Following macro specifies the number of milliseconds which constitute 1 ThreadX timer tick. Used
   for mimicking the ThreadX timer behaviour on VOSS. */
// Use the same MACRO used by firmware modules to calculate TICKs from mSec
// Mismatch would cause worng timer value to be programmed
#define TX_MSECS_IN_1_TICK  SYS_TICK_DUR_MS

// Signature with which the TX_TIMER struct is initialized, when the timer is created
#define TX_AIRGO_TMR_SIGNATURE   0xDEADBEEF

#ifdef TIMER_MANAGER
#define  tx_timer_create(a, b, c, d, e, f, g)   tx_timer_create_intern_debug((v_PVOID_t)pMac, a, b, c, d, e, f, g, __FILE__, __LINE__)
#else
#define  tx_timer_create(a, b, c, d, e, f, g)   tx_timer_create_intern((v_PVOID_t)pMac, a, b, c, d, e, f, g)
#endif

/*--------------------------------------------------------------------*/
/* Timer structure                                                    */
/* This structure is used to implement ThreadX timer facility.  Just  */
/* like ThreadX, timer expiration handler executes at the highest     */
/* possible priority level, i.e. DISPATCH_LEVEL.                      */
/*--------------------------------------------------------------------*/
typedef struct TX_TIMER_STRUCT
{
#ifdef WLAN_DEBUG
#define TIMER_MAX_NAME_LEN 50
    char timerName[TIMER_MAX_NAME_LEN];
#endif
    v_ULONG_t  tmrSignature;
    v_VOID_t   (*pExpireFunc)(v_PVOID_t, tANI_U32);
    tANI_U32   expireInput;
    v_ULONG_t  initScheduleTimeInMsecs;
    v_ULONG_t  rescheduleTimeInMsecs;
    vos_timer_t vosTimer;

    // Pointer to the MAC global structure, which stores the context for the NIC,
    // for which this timer is supposed to operate.
    v_PVOID_t  pMac;
    tANI_U8 sessionId;

} TX_TIMER;

#define TX_TIMER_VALID(timer) (timer.pMac != 0)

extern v_ULONG_t tx_time_get(v_VOID_t);
extern v_UINT_t  tx_timer_activate(TX_TIMER*);
extern v_UINT_t  tx_timer_change(TX_TIMER*, v_ULONG_t, v_ULONG_t);
extern v_UINT_t  tx_timer_change_context(TX_TIMER*, tANI_U32);
#ifdef TIMER_MANAGER
extern v_UINT_t  tx_timer_create_intern_debug(v_PVOID_t, TX_TIMER*, char *, v_VOID_t(*)(v_PVOID_t, tANI_U32),
               tANI_U32, v_ULONG_t, v_ULONG_t, v_ULONG_t, char* fileName, v_U32_t lineNum );
#else
extern v_UINT_t  tx_timer_create_intern(v_PVOID_t, TX_TIMER*, char *, v_VOID_t(*)(v_PVOID_t, tANI_U32), tANI_U32, v_ULONG_t, v_ULONG_t, v_ULONG_t);
#endif
extern v_UINT_t  tx_timer_deactivate(TX_TIMER*);
extern v_UINT_t  tx_timer_delete(TX_TIMER*);
extern v_BOOL_t  tx_timer_running(TX_TIMER*);

#ifdef __cplusplus
}
#endif


#endif



