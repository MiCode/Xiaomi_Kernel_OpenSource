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

/**=========================================================================
  
  \file  wlan_qct_pal_sync.c
  
  \brief Implementation trace/logging APIs PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform Windows and with legacy UMAC.
  
  
  ========================================================================*/

#include "wlan_qct_pal_sync.h"
#include "wlan_qct_pal_trace.h"

#include "wlan_qct_os_status.h"

/**
wpalMutexInit()

@brief
  This function initializes a mutex object

@param pMutex: a pointer to caller allocated object of wpt_mutex

@return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.

*/
wpt_status wpalMutexInit(wpt_mutex *pMutex)
{
    /* Not doing sanity checks since VOS does them anyways */

   if( vos_lock_init( (vos_lock_t*)pMutex  ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " mutex init fail");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief Invalidate a mutex object

    \param pMutex - a pointer to caller allocated object of wpt_mutex

    \return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexDelete(wpt_mutex *pMutex)
{
    /* Not doing sanity checks since VOS does them anyways */

   if( vos_lock_destroy( (vos_lock_t*)pMutex  ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " mutex delete fail");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief Acquire a mutex object. It is blocked until the object is acquired.

    \param pMutex - a pointer to caller allocated object of wpt_mutex

    \return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexAcquire(wpt_mutex *pMutex)
{
    /* Not doing sanity checks since VOS does them anyways */

   if( vos_lock_acquire( (vos_lock_t*)pMutex  ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " mutex acquire fail");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief Release a held mutex object

    \param pMutex - a pointer to caller allocated object of wpt_mutex

    \return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexRelease(wpt_mutex *pMutex)
{
    /* Not doing sanity checks since VOS does them anyways */

   if( vos_lock_release( (vos_lock_t*)pMutex ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " mutex release");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief Initialize an event object

    \param pEvent – a pointer to caller allocated object of wpt_event

    \return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.
------------------------------------------------------------------------*/
wpt_status wpalEventInit(wpt_event *pEvent)
{
   /* Not doing sanity checks since VOS does them anyways */

   if( vos_event_init( (vos_event_t*)pEvent ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " create event fail");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief Invalidate an event object

    \param pEvent – a pointer to caller allocated object of wpt_event

    \return eWLAN_PAL_STATUS_SUCCESS if success. Fail otherwise.
------------------------------------------------------------------------*/

wpt_status wpalEventDelete(wpt_event *pEvent)
{
   /* Not doing sanity checks since VOS does them anyways */

   if( vos_event_destroy( (vos_event_t*)pEvent ) != VOS_STATUS_SUCCESS )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                 " delete event fail");
      return eWLAN_PAL_STATUS_E_FAILURE;
   }

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    @brief wpalEventWait – Wait on an event object

    \param
        pEvent – a pointer to caller allocated object of wpt_event
        timeout - timeout value at unit of milli-seconds. 
                  0xffffffff means infinite wait

     \return eWLAN_PAL_STATUS_SUCCESS - the wait was satisifed by one of the events
             in the event array being set.  The index into the event arry 
             that satisfied the wait can be found at *pEventIndex.
                                  
             eWLAN_PALSTATUS_E_TIMEOUT - the timeout interval elapsed before any of 
             the events were set.
                                    
             eWLAN_PAL_STATUS_E_INVAL - At least one of the values specified in
             the event array refers to an uninitialized event object.  The
             invalid event is identified by the index in *pEventIndex.  Note
             that only the first uninitialized event is detected when this error
             is returned.
             
             eWLAN_PAL_STATUS_E_EMPTY - the events array is empty.  This condition
             is detected by numEvents being 0 on input.
              
             eWLAN_PAL_STATUS_E_FAULT - event or pEventIndex is an invalid pointer.
---------------------------------------------------------------------------*/
wpt_status wpalEventWait(wpt_event *pEvent, wpt_uint32 timeout)
{
   /* Not doing sanity checks since VOS does them anyways */

   wpt_status status = eWLAN_PAL_STATUS_E_FAILURE;
   VOS_STATUS  vos_status = VOS_STATUS_E_FAILURE;

   /* In VOS timeout = 0 corresponds to infinite wait */
   timeout = ( timeout == WLAN_PAL_WAIT_INFINITE ? 0 : timeout );

   vos_status = vos_wait_single_event( (vos_event_t*)pEvent, timeout );

   status = WPAL_VOS_TO_WPAL_STATUS( vos_status );

   return status;
}

/*---------------------------------------------------------------------------
    wpalEventSet – Set an event object to signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventSet(wpt_event *pEvent)
{
   /* Not doing sanity checks since VOS does them anyways */

   return ( WPAL_VOS_TO_WPAL_STATUS(vos_event_set( (vos_event_t*)pEvent )) );
}

/*---------------------------------------------------------------------------
    wpalEventReset – Set an event object to non-signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventReset(wpt_event *pEvent)
{
   /* Not doing sanity checks since VOS does them anyways */

   return ( WPAL_VOS_TO_WPAL_STATUS(vos_event_reset( (vos_event_t*)pEvent )) );
}


