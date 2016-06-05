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

#if !defined( __WLAN_QCT_PAL_TIMER_H )
#define __WLAN_QCT_PAL_TIMER_H

/**=========================================================================
  
  \file  wlan_qct_pal_timer.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent.
  
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_os_timer.h"


typedef VOS_TIMER_STATE WPAL_TIMER_STATE;

typedef void (*wpal_timer_callback)(void *pUserData);

typedef struct
{
   wpt_os_timer timer;
   wpal_timer_callback callback;
   void *pUserData;
} wpt_timer;


/*---------------------------------------------------------------------------
    wpalTimerInit - initialize a wpt_timer object
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
        callback - A callback function
        pUserData - A pointer to data pass as parameter of the callback function.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerInit(wpt_timer * pTimer, wpal_timer_callback callback, void *pUserData);

/*---------------------------------------------------------------------------
    wpalTimerDelete - invalidate a wpt_timer object
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerDelete(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalTimerStart - start a wpt_timer object with a timeout value
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
        timeout - timeout value of the timer. In unit of milli-seconds.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerStart(wpt_timer * pTimer, wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalTimerStop - stop a wpt_timer object. Stop doesn’t guarantee the timer handler is not called if it is already timeout.
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerStop(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalTimerGetCurStatus - Get the current status of timer

    pTimer - a pointer to caller allocated wpt_timer object

    return
        WPAL_TIMER_STATE
---------------------------------------------------------------------------*/
WPAL_TIMER_STATE wpalTimerGetCurStatus(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalGetSystemTime - Get the system time in milliseconds

    return
        current time in milliseconds
---------------------------------------------------------------------------*/
wpt_uint32 wpalGetSystemTime(void);

/*---------------------------------------------------------------------------
    wpalGetArchCounterTime - Get time from physical counter

    return
        MPM counter value
---------------------------------------------------------------------------*/
wpt_uint64 wpalGetArchCounterTime(void);

/*---------------------------------------------------------------------------
    wpalSleep - sleep for a specified interval
    Param:
        timeout - amount of time to sleep. In unit of milli-seconds.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalSleep(wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalBusyWait - Thread busy wait with specified usec
    Param:
        usecDelay - amount of time to wait. In unit of micro-seconds.
    Return:
        NONE
---------------------------------------------------------------------------*/
void wpalBusyWait(wpt_uint32 usecDelay);

#endif // __WLAN_QCT_PAL_TIMER_H
