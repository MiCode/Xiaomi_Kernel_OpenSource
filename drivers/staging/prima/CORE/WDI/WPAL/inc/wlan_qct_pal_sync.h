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

#if !defined( __WLAN_QCT_PAL_SYNC_H )
#define __WLAN_QCT_PAL_SYNC_H

/**=========================================================================
  
  \file  wlan_pal_sync.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent. 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_os_sync.h"


#define WLAN_PAL_WAIT_INFINITE    0xFFFFFFFF

/*---------------------------------------------------------------------------
    wpalMutexInit – initialize a mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexInit(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexDelete – invalidate a mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexDelete(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexAcquire – acquire a mutex object. It is blocked until the object is acquired.
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexAcquire(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexRelease – Release a held mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexRelease(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalEventInit – initialize an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventInit(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventDelete – invalidate an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventDelete(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventWait – Wait on an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
        timeout – timerout value at unit of milli-seconds. 0xffffffff means infinite wait
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventWait(wpt_event *pEvent, wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalEventSet – Set an event object to signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventSet(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventReset – Set an event object to non-signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventReset(wpt_event *pEvent);


#endif // __WLAN_QCT_PAL_SYNC_H
