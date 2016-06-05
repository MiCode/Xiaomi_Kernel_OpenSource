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

#if !defined( __VOS_TIMER_H )
#define __VOS_TIMER_H

/**=========================================================================
  
  \file  vos_timer.h
  
  \brief virtual Operating System Servies (vOS)
               
   Definitions for vOSS Timer services
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
#include <vos_lock.h>
#include <i_vos_timer.h>

#ifdef TIMER_MANAGER
#include "wlan_hdd_dp_utils.h"
#endif

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_TIMER_STATE_COOKIE 0x12

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
/// vos Timer callback function prototype (well, actually a prototype for 
/// a pointer to this callback function)
typedef v_VOID_t ( *vos_timer_callback_t )( v_PVOID_t userData );

typedef enum
{
   /// pure software timer.  No guarantee the apps processor will
   /// awaken when these timers expire.
   VOS_TIMER_TYPE_SW,
   
   /// These timers can awaken the Apps processor from power collapse
   /// when these timers expire.
   /// \todo I really dont like this name :-)
   VOS_TIMER_TYPE_WAKE_APPS

} VOS_TIMER_TYPE;

typedef enum
{
   VOS_TIMER_STATE_UNUSED = VOS_TIMER_STATE_COOKIE,
   VOS_TIMER_STATE_STOPPED,
   VOS_TIMER_STATE_STARTING,
   VOS_TIMER_STATE_RUNNING,
} VOS_TIMER_STATE;

#ifdef TIMER_MANAGER
struct vos_timer_s;
typedef struct timer_node_s
{
   hdd_list_node_t pNode;
   char* fileName;
   unsigned int lineNum;
   struct vos_timer_s *vosTimer;
}timer_node_t;
#endif

typedef struct vos_timer_s
{
#ifdef TIMER_MANAGER
   timer_node_t *ptimerNode;
#endif

   vos_timer_platform_t platformInfo;
   vos_timer_callback_t callback;
   v_PVOID_t            userData;
   vos_lock_t           lock;
   VOS_TIMER_TYPE       type;
   VOS_TIMER_STATE      state;
} vos_timer_t;

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
#ifdef TIMER_MANAGER
void vos_timer_manager_init(void);
void vos_timer_exit(void);
#endif

/*---------------------------------------------------------------------------
  
  \brief vos_timer_getCurrentState() - Get the current state of the timer

  \param pTimer - the timer object
  
  \return timer state
  
  \sa
  
---------------------------------------------------------------------------*/
VOS_TIMER_STATE vos_timer_getCurrentState( vos_timer_t *pTimer );

/*--------------------------------------------------------------------------
  
  \brief vos_timer_init() - Initialize a vOSS timer.

  This API initializes a vOS Timer object. 
  
  The \a vos_timer_init() initializes a vOS Timer object.  A timer must be 
  initialized by calling vos_timer_initialize() before it may be used in 
  any other timer functions. 
  
  Attempting to initialize timer that is already initialized results in 
  a failure. A destroyed timer object can be re-initialized with a call to
  \a vos_timer_init().  The results of otherwise referencing the object 
  after it has been destroyed are undefined.  
  
  Calls to vOSS timer functions to manipulate the timer such
  as vos_timer_set() will fail if the timer is not initialized or has
  been destroyed.  Therefore, don't use the timer after it has been 
  destroyed until it has been re-initialized.
  
  All callback will be executed within the VOS main thread unless it is 
  initialized from the Tx thread flow, in which case it will be executed
  within the tx thread flow.
  
  \param timer - pointer to the opaque timer object to initialize
  
  \param timerType - specifies the type of timer.  We have two different
                     timer types.
    <ol>
      <li> VOS_TIMER_TYPE_SW - Pure software timer. The Apps processor
           may not be awoken when this timer expires.
      <li> VOS_TIMER_TYPE_WAKE_APPS - The Apps processor will be awoken
           from power collapse when this type of timer expires.
     </ol>                      
  
  \param callback - the callback function to be called when the timer
         expires.
         
  \param userData - a user data (or context) that is returned to the 
         callback function as a parameter when the timer expires.         
  
  \return VOS_STATUS_SUCCESS - timer was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initialize the timer

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the timer

          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to initialize the object referenced by timer, a previously 
          initialized but not yet destroyed timer.

          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
---------------------------------------------------------------------------*/
#ifdef TIMER_MANAGER
#define vos_timer_init(timer, timerType, callback, userdata) \
      vos_timer_init_debug(timer, timerType, callback, userdata, __FILE__, __LINE__)
      
VOS_STATUS vos_timer_init_debug( vos_timer_t *timer, VOS_TIMER_TYPE timerType, 
                           vos_timer_callback_t callback, v_PVOID_t userData, 
                           char* fileName, v_U32_t lineNum );      
#else
VOS_STATUS vos_timer_init( vos_timer_t *timer, VOS_TIMER_TYPE timerType, 
                           vos_timer_callback_t callback, v_PVOID_t userData );
#endif

/*---------------------------------------------------------------------------
  
  \brief vos_timer_destroy() - Destroy a vOSS Timer object

  The \a vos_timer_destroy() function shall destroy the timer object.
  After a successful return from \a vos_timer_destroy() the timer 
  object becomes, in effect, uninitialized.
   
  A destroyed timer object can be re-initialized by calling
  vos_timer_init().  The results of otherwise referencing the object 
  after it has been destroyed are undefined.  
  
  Calls to vOSS timer functions to manipulate the timer, such
  as vos_timer_set() will fail if the lock is destroyed.  Therefore, 
  don't use the timer after it has been destroyed until it has 
  been re-initialized.
  
  \param timer - the timer object to be destroyed.
  
  \return VOS_STATUS_SUCCESS - timer was successfully destroyed.
  
          VOS_STATUS_E_BUSY - The implementation has detected an attempt 
          to destroy the object referenced by timer while it is still 
          still referenced.  The timer must be stopped before it can be 
          destroyed.

          VOS_STATUS_E_INVAL - The value specified by timer is invalid.
          
          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
---------------------------------------------------------------------------*/
VOS_STATUS vos_timer_destroy( vos_timer_t *timer );


/*--------------------------------------------------------------------------
  
  \brief vos_timer_start() - Start a vOSS Timer object

  The \a vos_timer_start() function starts a timer to expire after the 
  specified interval, thus running the timer callback function when 
  the interval expires.
   
  A timer only runs once (a one-shot timer).  To re-start the 
  timer, vos_timer_start() has to be called after the timer runs 
  or has been cancelled.
  
  \param timer - the timer object to be started
  
  \param expirationTime - expiration time for the timer (in milliseconds)
                          The expiration time cannot be less than 10 ms.
  
  \return VOS_STATUS_SUCCESS - timer was successfully started.
  
          VOS_STATUS_E_ALREADY - The implementation has detected an attempt 
          to start a timer while it is already started.  The timer must 
          be stopped or expire before it can be started again.

          VOS_STATUS_E_INVAL - The value specified by timer is invalid.
          
          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_timer_start( vos_timer_t *timer, v_U32_t expirationTime );


/*--------------------------------------------------------------------------
  
  \brief vos_timer_stop() - Stop a vOSS Timer

  The \a vos_timer_stop() function stops a timer that has been started but
  has not expired, essentially cancelling the 'start' request.
   
  After a timer is stopped, it goes back to the state it was in after it
  was created and can be started again via a call to vos_timer_start().
  
  \param timer - the timer object to be stopped
    
  \return VOS_STATUS_SUCCESS - timer was successfully stopped.
  
          VOS_STATUS_E_EMPTY - The implementation has detected an attempt 
          to stop a timer that has not been started or has already 
          expired.

          VOS_STATUS_E_INVAL - The value specified by timer is invalid.
          
          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
  ------------------------------------------------------------------------*/
VOS_STATUS vos_timer_stop( vos_timer_t *timer );


/*--------------------------------------------------------------------------
  
  \brief vos_timer_get_system_ticks() - Get the system time in 10ms ticks

  The \a vos_timer_get_system_ticks() function returns the current number
  of timer ticks in 10msec intervals.  This function is suitable timestamping
  and calculating time intervals by calculating the difference between two 
  timestamps.
    
  \returns - The current system tick count (in 10msec intervals).  This 
             function cannot fail.
  
  \sa
  
  ------------------------------------------------------------------------*/
v_TIME_t vos_timer_get_system_ticks( v_VOID_t );


/*--------------------------------------------------------------------------
 
  \brief vos_timer_get_system_time() - Get the system time in milliseconds

  The \a vos_timer_get_system_time() function returns the number of milliseconds 
  that have elapsed since the system was started
    
  \returns - The current system time in milliseconds.
  
  \sa
  
  ------------------------------------------------------------------------*/
v_TIME_t vos_timer_get_system_time( v_VOID_t );

v_BOOL_t vos_timer_is_initialized(vos_timer_t *timer);

#endif // #if !defined __VOSS_TIMER_H
