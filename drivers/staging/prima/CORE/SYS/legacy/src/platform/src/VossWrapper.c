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

/*===========================================================================
  @file VossWrapper.c

  @brief This source file contains the various function definitions for the 
  RTOS abstraction layer, implemented for VOSS

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
  03/31/09    sho    Remove the use of vosTimerIsActive flag as it is not
                     thread-safe
  02/17/08    sho    Fix the timer callback function to work when it is called
                     after the timer has stopped due to a race condition.
  02/10/08    sho    Refactor the TX timer to use VOS timer directly instead
                     of using VOS utility timer
  12/15/08    sho    Resolved errors and warnings from the AMSS compiler when
                     this is ported from WM
  11/20/08    sho    Renamed this to VosWrapper.c; remove all dependencies on
                     WM platform and allow this to work on all VOSS enabled
                     platform
  06/24/08    tbh    Modified the file to remove the dependecy on HDD files as 
                     part of Gen6 bring up process. 
  10/29/02 Neelay Das Created file. 
     
===========================================================================*/ 

/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/
#include "VossWrapper.h"

#ifdef WLAN_DEBUG
#define TIMER_NAME (timer_ptr->timerName)
#else
#define TIMER_NAME "N/A"
#endif

/**---------------------------------------------------------------------
 * tx_time_get() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return current system time in units of miliseconds
 *
 */ 
v_ULONG_t tx_time_get( void )
{
   return(vos_timer_get_system_ticks());

} //* tx_time_get()


/**---------------------------------------------------------------------
 * tx_timer_activate() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return TX_SUCCESS.
 *
 */
v_UINT_t tx_timer_activate(TX_TIMER *timer_ptr)
{
   VOS_STATUS status;
  
    // Uncomment the asserts, if the intention is to debug the occurence of the
    // following anomalous cnditions.

    // Assert that the timer structure pointer passed, is not NULL
    //dbgAssert(NULL != timer_ptr);

    // If the NIC is halting just spoof a successful timer activation, so that all
    // the timers can be cleaned up.

    if(NULL == timer_ptr)
        return TX_TIMER_ERROR;

    // Put a check for the free builds
    if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
        VOS_ASSERT( timer_ptr->tmrSignature == 0 );

        return TX_TIMER_ERROR;

    }

    // Check for an uninitialized timer
    VOS_ASSERT(0 != strlen(TIMER_NAME));

    VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
            "Timer %s being activated\n", TIMER_NAME);

    status = vos_timer_start( &timer_ptr->vosTimer, 
         timer_ptr->initScheduleTimeInMsecs );

   if (VOS_STATUS_SUCCESS == status)
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
            "Timer %s now activated\n", TIMER_NAME);
      return TX_SUCCESS;
   }
   else if (VOS_STATUS_E_ALREADY == status)
   {
      // starting timer fails because timer is already started; this is okay
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
            "Timer %s is already running\n", TIMER_NAME);
      return TX_SUCCESS;
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR, 
            "Timer %s fails to activate\n", TIMER_NAME);
      return TX_TIMER_ERROR;
   }
} /*** tx_timer_activate() ***/


/**---------------------------------------------------------------------
 * tx_timer_change() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return TX_SUCCESS.
 *
 */
v_UINT_t tx_timer_change(TX_TIMER *timer_ptr, 
      v_ULONG_t initScheduleTimeInTicks, v_ULONG_t rescheduleTimeInTicks)
{
   // Put a check for the free builds
   if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
       VOS_ASSERT( timer_ptr->tmrSignature == 0 );

       return TX_TIMER_ERROR;      
   }

    // changes cannot be applied until timer stops running
    if (VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&timer_ptr->vosTimer))
    {
       timer_ptr->initScheduleTimeInMsecs = TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
       timer_ptr->rescheduleTimeInMsecs = TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
       return TX_SUCCESS;
    }
    else
    {
       return TX_TIMER_ERROR;
    }
} /*** tx_timer_change() ***/

/**---------------------------------------------------------------------
 * tx_timer_change_context() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return TX_SUCCESS.
 *
 */
v_UINT_t tx_timer_change_context(TX_TIMER *timer_ptr, tANI_U32 expiration_input)
{

    // Put a check for the free builds
    if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
       VOS_ASSERT( timer_ptr->tmrSignature == 0 );

       return TX_TIMER_ERROR;      
    }

    // changes cannot be applied until timer stops running
    if (VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&timer_ptr->vosTimer))
    {
       timer_ptr->expireInput = expiration_input;
       return TX_SUCCESS;
    }
    else
    {
       return TX_TIMER_ERROR;
    }
} /*** tx_timer_change() ***/


/**---------------------------------------------------------------------
 * tx_main_timer_func() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return None.
 *
 */
static v_VOID_t tx_main_timer_func( v_PVOID_t functionContext )
{
   TX_TIMER *timer_ptr = (TX_TIMER *)functionContext;


   if (NULL == timer_ptr)
   {
       VOS_ASSERT(0);
       return;
   }


   if (NULL == timer_ptr->pExpireFunc)
   {
       VOS_ASSERT(0);
       return;
   }

   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
             "Timer %s triggered", TIMER_NAME);

   // Now call the actual timer function, taking the function pointer,
   // from the timer structure.
   (* timer_ptr->pExpireFunc)( timer_ptr->pMac, timer_ptr->expireInput );

   // check if this needs to be rescheduled
   if (0 != timer_ptr->rescheduleTimeInMsecs)
   {
      VOS_STATUS status;
      status = vos_timer_start( &timer_ptr->vosTimer, 
                                timer_ptr->rescheduleTimeInMsecs );
      timer_ptr->rescheduleTimeInMsecs = 0;

      if (VOS_STATUS_SUCCESS != status)
      {
         VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_WARN, 
             "Unable to reschedule timer %s; status=%d", TIMER_NAME, status);
      }
   }
} /*** tx_timer_change() ***/

#ifdef TIMER_MANAGER
v_UINT_t tx_timer_create_intern_debug( v_PVOID_t pMacGlobal, TX_TIMER *timer_ptr,
   char *name_ptr, 
   v_VOID_t ( *expiration_function )( v_PVOID_t, tANI_U32 ),
   tANI_U32 expiration_input, v_ULONG_t initScheduleTimeInTicks, 
   v_ULONG_t rescheduleTimeInTicks, v_ULONG_t auto_activate, 
   char* fileName, v_U32_t lineNum)
{
    VOS_STATUS status;

    if (NULL == expiration_function)
    {
        VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
                "NULL timer expiration");
        VOS_ASSERT(0);
        return TX_TIMER_ERROR;
    }

    if(NULL == name_ptr)
    {

        VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
                "NULL name pointer for timer");
        VOS_ASSERT(0);
        return TX_TIMER_ERROR;
    }

    if (!initScheduleTimeInTicks)
        return TX_TICK_ERROR;

    if (!timer_ptr)
        return TX_TIMER_ERROR;

    // Initialize timer structure
    timer_ptr->pExpireFunc = expiration_function;
    timer_ptr->expireInput = expiration_input;
    timer_ptr->initScheduleTimeInMsecs =
        TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
    timer_ptr->rescheduleTimeInMsecs =
        TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
    timer_ptr->pMac = pMacGlobal;

    // Set the flag indicating that the timer was created
    timer_ptr->tmrSignature = TX_AIRGO_TMR_SIGNATURE;

#ifdef WLAN_DEBUG
    // Store the timer name
    strlcpy(timer_ptr->timerName, name_ptr, sizeof(timer_ptr->timerName));
#endif // Store the timer name, for Debug build only

    status = vos_timer_init_debug( &timer_ptr->vosTimer, VOS_TIMER_TYPE_SW, 
          tx_main_timer_func, (v_PVOID_t)timer_ptr, fileName, lineNum);
    if (VOS_STATUS_SUCCESS != status)
    {
       VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Cannot create timer for %s\n", TIMER_NAME);
       return TX_TIMER_ERROR;
    }

    if(0 != rescheduleTimeInTicks)
    {
        VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
                  "Creating periodic timer for %s\n", TIMER_NAME);
    }

    // Activate this timer if required
    if (auto_activate)
    {
        tx_timer_activate(timer_ptr);
    }

    return TX_SUCCESS;

} //** tx_timer_create() ***/
#else
v_UINT_t tx_timer_create_intern( v_PVOID_t pMacGlobal, TX_TIMER *timer_ptr,
   char *name_ptr, 
   v_VOID_t ( *expiration_function )( v_PVOID_t, tANI_U32 ),
   tANI_U32 expiration_input, v_ULONG_t initScheduleTimeInTicks, 
   v_ULONG_t rescheduleTimeInTicks, v_ULONG_t auto_activate )
{
    VOS_STATUS status;

    if((NULL == name_ptr) || (NULL == expiration_function))
        return TX_TIMER_ERROR;

    if (!initScheduleTimeInTicks)
        return TX_TICK_ERROR;

    if (!timer_ptr)
        return TX_TIMER_ERROR;

    // Initialize timer structure
    timer_ptr->pExpireFunc = expiration_function;
    timer_ptr->expireInput = expiration_input;
    timer_ptr->initScheduleTimeInMsecs =
        TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
    timer_ptr->rescheduleTimeInMsecs =
        TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
    timer_ptr->pMac = pMacGlobal;

    // Set the flag indicating that the timer was created
    timer_ptr->tmrSignature = TX_AIRGO_TMR_SIGNATURE;

#ifdef WLAN_DEBUG
    // Store the timer name
    strlcpy(timer_ptr->timerName, name_ptr, sizeof(timer_ptr->timerName));
#endif // Store the timer name, for Debug build only

    status = vos_timer_init( &timer_ptr->vosTimer, VOS_TIMER_TYPE_SW, 
          tx_main_timer_func, (v_PVOID_t)timer_ptr );
    if (VOS_STATUS_SUCCESS != status)
    {
       VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Cannot create timer for %s\n", TIMER_NAME);
       return TX_TIMER_ERROR;
    }

    if(0 != rescheduleTimeInTicks)
    {
        VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
                  "Creating periodic timer for %s\n", TIMER_NAME);
    }

    // Activate this timer if required
    if (auto_activate)
    {
        tx_timer_activate(timer_ptr);
    }

    return TX_SUCCESS;

} //** tx_timer_create() ***/
#endif


/**---------------------------------------------------------------------
 * tx_timer_deactivate() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return TX_SUCCESS.
 *
 */
v_UINT_t tx_timer_deactivate(TX_TIMER *timer_ptr)
{
   VOS_STATUS vStatus;
   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
             "tx_timer_deactivate() called for timer %s\n", TIMER_NAME);

   // Put a check for the free builds
   if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature)
   {
      return TX_TIMER_ERROR;      
   }

   // if the timer is not running then we do not need to do anything here
   vStatus = vos_timer_stop( &timer_ptr->vosTimer );
   if (VOS_STATUS_SUCCESS != vStatus)
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO_HIGH, 
                "Unable to stop timer %s; status =%d\n", 
                TIMER_NAME, vStatus);
   }

   return TX_SUCCESS;

} /*** tx_timer_deactivate() ***/

v_UINT_t tx_timer_delete( TX_TIMER *timer_ptr )
{
   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
             "tx_timer_delete() called for timer %s\n", TIMER_NAME);

   // Put a check for the free builds
   if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature)
   {
      return TX_TIMER_ERROR;      
   }

   vos_timer_destroy( &timer_ptr->vosTimer );
   return TX_SUCCESS;     
} /*** tx_timer_delete() ***/



/**---------------------------------------------------------------------
 * tx_timer_running() 
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param  
 *
 * @return TX_SUCCESS.
 *
 */
v_BOOL_t tx_timer_running(TX_TIMER *timer_ptr)
{
   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO, 
             "tx_timer_running() called for timer %s\n", TIMER_NAME);

   // Put a check for the free builds
   if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature)
      return VOS_FALSE;      

   if (VOS_TIMER_STATE_RUNNING == 
       vos_timer_getCurrentState( &timer_ptr->vosTimer ))
   {
       return VOS_TRUE;
   }
   return VOS_FALSE;

} /*** tx_timer_running() ***/
