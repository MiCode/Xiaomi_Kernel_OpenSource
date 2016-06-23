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

#if !defined( __VOS_EVENT_H )
#define __VOS_EVENT_H

/**=========================================================================
  
  \file  vos_event.h
  
  \brief virtual Operating System Services (vOSS) Events
               
   Definitions for vOSS Events
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_status.h"
#include "vos_types.h"
#include "i_vos_event.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
  
  \brief vos_event_init() - initialize a vOSS event
  
  The \a vos_lock_event() function initializes the specified event. Upon 
  successful initialization, the state of the event becomes initialized 
  and not 'signaled.

  An event must be initialized before it may be used in any other lock 
  functions. 
  
  Attempting to initialize an already initialized event results in 
  a failure.
 
  \param lock - pointer to the opaque event object to initialize
  
  \return VOS_STATUS_SUCCESS - event was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the event

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the event

          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to reinitialize the object referenced by event, a previously 
          initialized, but not yet destroyed, event.

          VOS_STATUS_E_FAULT  - event is an invalid pointer.     
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_event_init( vos_event_t *event );

/*--------------------------------------------------------------------------
  
  \brief vos_event_set() - set a vOSS event

  The state of the specified event is set to 'signalled by calling 
  \a vos_event_set().  The state of the event remains signalled until an 
  explicit call to vos_event_reset().  
  
  Any threads waiting on the event as a result of a vos_event_wait() will 
  be unblocked and available to be scheduled for execution when the event
  is signaled by a call to \a vos_event_set().  
    
  \param event - the event to set to the signalled state
  
  \return VOS_STATUS_SUCCESS - the event was successfully signalled.
  
          VOS_STATUS_E_INVAL - The value specified by event does not refer 
          to an initialized event object.
          
          VOS_STATUS_E_FAULT  - event is an invalid pointer.     
          
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_event_set( vos_event_t * event );


/*--------------------------------------------------------------------------
  
  \brief vos_event_reset() - reset a vOSS event

  The state of the specified event is set to 'NOT signalled' by calling 
  \a vos_event_reset().  The state of the event remains NOT signalled until an 
  explicit call to vos_event_set().  
  
  This function sets the event to a NOT signalled state even if the event was 
  signalled multiple times before being signaled.
  
  \param event - the event to set to the NOT signalled state
  
  \return VOS_STATUS_SUCCESS - the event state was successfully change to 
          NOT signalled.
  
          VOS_STATUS_E_INVAL - The value specified by event does not refer 
          to an initialized event object.
          
          VOS_STATUS_E_FAULT  - event is an invalid pointer.     
          
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_event_reset( vos_event_t * event );


/*--------------------------------------------------------------------------
  
  \brief vos_event_destroy() - Destroy a vOSS event

  The \a vos_event_destroy() function shall destroy the event object 
  referenced by event.  After a successful return from \a vos_event_destroy()
  the event object becomes, in effect, uninitialized.
   
  A destroyed event object can be reinitialized using vos_event_init(); 
  the results of otherwise referencing the object after it has been destroyed 
  are undefined.  Calls to vOSS event functions to manipulate the lock such
  as vos_event_set() will fail if the event is destroyed.  Therefore, 
  don't use the event after it has been destroyed until it has 
  been re-initialized.
  
  \param event - the event object to be destroyed.
  
  \return VOS_STATUS_SUCCESS - event was successfully destroyed.
  
          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to destroy the object referenced by event while it is still being
          referenced (there are threads waiting on this event) 

          VOS_STATUS_E_INVAL - The value specified by event is invalid.
          
          VOS_STATUS_E_FAULT - event is an invalid pointer.     
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_event_destroy( vos_event_t *event );

// TODO: this is being removed and a stub exists currently to avoid 
// compiler errors
VOS_STATUS vos_wait_events( vos_event_t *events, v_U8_t numEvents,
                            v_U32_t timeout, v_U8_t *pEventIndex );

/*----------------------------------------------------------------------------
  
  \brief vos_wait_single_event() - Waits for a single event to be set.

   This API waits for the event to be set.
   
  \param pEvent - pointer to an event to wait on.
  
  \param timeout - Timeout value (in milliseconds).  This function returns
         if this interval elapses, regardless if any of the events have 
         been set.  An input value of 0 for this timeout parameter means 
         to wait infinitely, meaning a timeout will never occur.
  
  \return VOS_STATUS_SUCCESS - the wait was satisifed by the event being
          set.  
                               
          VOS_STATUS_E_TIMEOUT - the timeout interval elapsed before the
          event was set.
                                 
          VOS_STATUS_E_INVAL - The value specified by event is invalid.
          
          VOS_STATUS_E_FAULT - pEvent is an invalid pointer.     
                                 
  \sa vos_wait_multiple_events()
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_wait_single_event( vos_event_t *pEvent, v_U32_t timeout );

/*----------------------------------------------------------------------------
  
  \brief vos_wait_multiple_events() - Waits for event(s) to be set.

   This API waits for any event in the input array of events to be
   set.  The caller is blocked waiting any event in the array to be 
   set or for the timeout to occur. 
   
   If multiple events in the array are set, only one event is identified
   in the return from this call as satisfying the wait condition.  The
   caller is responsible for calling \a vos_wait_events() again to find
   the other events that are set.
  
  \param pEventList - pointer to an array of event pointers
  
  \param numEvents - Number of events
  
  \param timeout - Timeout value (in milliseconds).  This function returns
         if this interval elapses, regardless if any of the events have 
         been set.  An input value of 0 for this timeout parameter means 
         to wait infinitely, meaning a timeout will never occur.
  
  \param pEventIndex - This is a pointer to the location where the index of 
         the event in the event array that satisfied the wait because 
         the event was set.  
  
  \return VOS_STATUS_SUCCESS - the wait was satisifed by one of the events
          in the event array being set.  The index into the event arry 
          that satisfied the wait can be found at *pEventIndex.
                               
          VOS_STATUS_E_TIMEOUT - the timeout interval elapsed before any of 
          the events were set.
                                 
          VOS_STATUS_E_INVAL - At least one of the values specified in the 
          event array refers to an uninitialized event object.  The invalid
          event is identified by the index in *pEventIndex.  Note that only
          the first uninitialized event is detected when this error is 
          returned.
          
          VOS_STATUS_E_EMPTY - the events array is empty.  This condition
          is detected by numEvents being 0 on input.
           
          VOS_STATUS_E_FAULT - event or pEventIndex is an invalid pointer.     
                                 
  \sa vos_wait_single_events()
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_wait_multiple_events( vos_event_t **pEventList, v_U8_t numEvents,
   v_U32_t timeout, v_U8_t *pEventIndex );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __VOSS_EVENT_H
