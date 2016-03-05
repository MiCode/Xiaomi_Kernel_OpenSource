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

/**=========================================================================

  \file  vos_timer.c

  \brief virtual Operating System Servies (vOS)

   Definitions for vOSS Timer services
<<<<<<< HEAD:CORE/VOSS/src/vos_timer.c
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
=======

   Copyright 2008 (c) Qualcomm Technologies, Inc.  All Rights Reserved.

   Qualcomm Technologies Confidential and Proprietary.

>>>>>>> f7413b6... wlan: voss: remove obsolete "INTEGRATED_SOC" featurization:prima/CORE/VOSS/src/vos_timer.c
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_timer.h>
#include <vos_lock.h>
#include <vos_api.h>
#include "wlan_qct_sys.h"
#include "vos_sched.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define LINUX_TIMER_COOKIE 0x12341234
#define LINUX_INVALID_TIMER_COOKIE 0xfeedface
#define TMR_INVALID_ID ( 0 )

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/
static unsigned int        persistentTimerCount;
static vos_lock_t          persistentTimerCountLock;
// static sleep_okts_handle   sleepClientHandle;

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
// TBD: Need to add code for deferred timers implementation

// clean up timer states after it has been deactivated
// check and try to allow sleep after a timer has been stopped or expired
static void tryAllowingSleep( VOS_TIMER_TYPE type )
{
   if ( VOS_TIMER_TYPE_WAKE_APPS == type )
   {
     // vos_lock_acquire( &persistentTimerCountLock );
      persistentTimerCount--;
      if ( 0 == persistentTimerCount )
      {
         // since the number of persistent timers has decreased from 1 to 0,
         // the timer should allow sleep
         //sleep_assert_okts( sleepClientHandle );
      }
      //vos_lock_release( &persistentTimerCountLock );
   }
}


/*----------------------------------------------------------------------------
  
  \brief  vos_linux_timer_callback() - internal vos entry point which is 
          called when the timer interval expires 

  This function in turn calls the vOS client callback and changes the 
  state of the timer from running (ACTIVE) to expired (INIT). 
  
  
  \param uTimerID - return value of the timeSetEvent() from the 
      vos_timer_start() API which 

  \param dwUser - this is supplied by the fourth parameter of the timeSetEvent()
      which is the timer structure being passed as the userData

  \param uMsg - Reserved / Not Used

  \param dw1  - Reserved / Not Used

  \param dw2  - Reserved / Not Used
  
  \return  nothing
  --------------------------------------------------------------------------*/

static void vos_linux_timer_callback ( v_U32_t data ) 
{
   vos_timer_t *timer = ( vos_timer_t *)data; 
   vos_msg_t msg;
   VOS_STATUS vStatus;
   unsigned long flags;
   
   vos_timer_callback_t callback=NULL;
   v_PVOID_t userData=NULL;
   int threadId;
   VOS_TIMER_TYPE type=VOS_TIMER_TYPE_SW;
   
   VOS_ASSERT(timer);

   if (timer == NULL)
   {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s Null pointer passed in!",__func__);
     return;
   }

   threadId = timer->platformInfo.threadID;
   spin_lock_irqsave( &timer->platformInfo.spinlock,flags );
   
   switch ( timer->state )
   {
   case VOS_TIMER_STATE_STARTING:
      // we are in this state because someone just started the timer, MM timer
      // got started and expired, but the time content have not bee updated
      // this is a rare race condition!
      timer->state = VOS_TIMER_STATE_STOPPED;
      vStatus = VOS_STATUS_E_ALREADY;
      break;
   case VOS_TIMER_STATE_STOPPED:
      vStatus = VOS_STATUS_E_ALREADY;
      break;
   case VOS_TIMER_STATE_UNUSED:
      vStatus = VOS_STATUS_E_EXISTS;
      break;
   case VOS_TIMER_STATE_RUNNING:
      // need to go to stop state here because the call-back function may restart 
      // timer (to emulate periodic timer)
      timer->state = VOS_TIMER_STATE_STOPPED;
      // copy the relevant timer information to local variables;
      // once we exist from this critical section, the timer content may be modified
      // by other tasks
      callback = timer->callback;
      userData = timer->userData;
      threadId = timer->platformInfo.threadID;
      type = timer->type;
      vStatus = VOS_STATUS_SUCCESS;
      break;
   default:
      VOS_ASSERT(0);
      vStatus = VOS_STATUS_E_FAULT;
      break;
   }
   
   spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
   
   if ( VOS_STATUS_SUCCESS != vStatus )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "TIMER callback called in a wrong state=%d", timer->state);
      return;
   }

   tryAllowingSleep( type );

   if (callback == NULL)
   {
       VOS_ASSERT(0);
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: No TIMER callback, Could not enqueue timer to any queue",
                 __func__);
       return;
   }

   // If timer has expired then call vos_client specific callback 
   if ( vos_sched_is_tx_thread( threadId ) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, 
          "TIMER callback: running on TX thread");
         
      //Serialize to the Tx thread
      sysBuildMessageHeader( SYS_MSG_ID_TX_TIMER, &msg );
      msg.bodyptr  = callback;
      msg.bodyval  = (v_U32_t)userData; 
       
      if(vos_tx_mq_serialize( VOS_MQ_ID_SYS, &msg ) == VOS_STATUS_SUCCESS)
         return;
   }
   else if ( vos_sched_is_rx_thread( threadId ) )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, 
          "TIMER callback: running on RX thread");
         
      //Serialize to the Rx thread
      sysBuildMessageHeader( SYS_MSG_ID_RX_TIMER, &msg );
      msg.bodyptr  = callback;
      msg.bodyval  = (v_U32_t)userData; 
       
      if(vos_rx_mq_serialize( VOS_MQ_ID_SYS, &msg ) == VOS_STATUS_SUCCESS)
         return;
   }
   else 
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
          "TIMER callback: running on MC thread");
                    
      // Serialize to the MC thread
      sysBuildMessageHeader( SYS_MSG_ID_MC_TIMER, &msg );
      msg.bodyptr  = callback;
      msg.bodyval  = (v_U32_t)userData; 
       
      if(vos_mq_post_message( VOS_MQ_ID_SYS, &msg ) == VOS_STATUS_SUCCESS)
        return;
   }     

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Could not enqueue timer to any queue", __func__);
   VOS_ASSERT(0);
}

/*---------------------------------------------------------------------------
  
  \brief vos_timer_getCurrentState() - Get the current state of the timer

  \param pTimer - the timer object
  
  \return timer state
  
  \sa
  
---------------------------------------------------------------------------*/
VOS_TIMER_STATE vos_timer_getCurrentState( vos_timer_t *pTimer )
{
   if ( NULL == pTimer )
   {
      VOS_ASSERT(0);
      return VOS_TIMER_STATE_UNUSED;
   }

   switch ( pTimer->state )
   {
      case VOS_TIMER_STATE_STOPPED:
      case VOS_TIMER_STATE_STARTING:
      case VOS_TIMER_STATE_RUNNING:
      case VOS_TIMER_STATE_UNUSED:
         return pTimer->state;
      default:
         VOS_ASSERT(0);
         return VOS_TIMER_STATE_UNUSED;
   }    
}

/*----------------------------------------------------------------------------
  
  \brief vos_timer_module_init() - Initializes a vOSS timer module.

  This API initializes the VOSS timer module.  This needs to be called
  exactly once prior to using any VOSS timers. 

  \sa
  
  --------------------------------------------------------------------------*/

void vos_timer_module_init( void )
{
   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, 
         "Initializing the VOSS timer module");
   vos_lock_init( &persistentTimerCountLock );
}

#ifdef TIMER_MANAGER
#include "wlan_hdd_dp_utils.h"

hdd_list_t vosTimerList;

static void vos_timer_clean(void);

void vos_timer_manager_init()
{
   /* Initalizing the list with maximum size of 60000 */
   hdd_list_init(&vosTimerList, 1000);  
   return;
}

static void vos_timer_clean()
{
    v_SIZE_t listSize;
    unsigned long flags;
        
    hdd_list_size(&vosTimerList, &listSize);
    
    if (listSize)
    {
       hdd_list_node_t* pNode;
       VOS_STATUS vosStatus;

       timer_node_t *ptimerNode;
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: List is not Empty. listSize %d ",
                 __func__, (int)listSize);

       do
       {
          spin_lock_irqsave(&vosTimerList.lock, flags);
          vosStatus = hdd_list_remove_front(&vosTimerList, &pNode);
          spin_unlock_irqrestore(&vosTimerList.lock, flags);
          if (VOS_STATUS_SUCCESS == vosStatus)
          {
             ptimerNode = (timer_node_t*)pNode;
             VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                       "Timer Leak@ File %s, @Line %d", 
                       ptimerNode->fileName, (int)ptimerNode->lineNum);

             vos_mem_free(ptimerNode);
          }
       } while (vosStatus == VOS_STATUS_SUCCESS);
    }
}

void vos_timer_exit()
{
    vos_timer_clean();
    hdd_list_destroy(&vosTimerList);
}
#endif
  
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
VOS_STATUS vos_timer_init_debug( vos_timer_t *timer, VOS_TIMER_TYPE timerType, 
                           vos_timer_callback_t callback, v_PVOID_t userData, 
                           char* fileName, v_U32_t lineNum )
{
   VOS_STATUS vosStatus;
    unsigned long flags;
   // Check for invalid pointer
   if ((timer == NULL) || (callback == NULL)) 
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Null params being passed",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
   }

   timer->ptimerNode = vos_mem_malloc(sizeof(timer_node_t));

   if(timer->ptimerNode == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Not able to allocate memory for timeNode",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
   }

   vos_mem_set(timer->ptimerNode, sizeof(timer_node_t), 0);

    timer->ptimerNode->fileName = fileName;
    timer->ptimerNode->lineNum   = lineNum;
    timer->ptimerNode->vosTimer = timer;

    spin_lock_irqsave(&vosTimerList.lock, flags);
    vosStatus = hdd_list_insert_front(&vosTimerList, &timer->ptimerNode->pNode);
    spin_unlock_irqrestore(&vosTimerList.lock, flags);
    if(VOS_STATUS_SUCCESS != vosStatus)
    {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Unable to insert node into List vosStatus %d", __func__, vosStatus);
    }
   
   // set the various members of the timer structure 
   // with arguments passed or with default values
   spin_lock_init(&timer->platformInfo.spinlock);
   init_timer(&(timer->platformInfo.Timer));
   timer->platformInfo.Timer.function = vos_linux_timer_callback;
   timer->platformInfo.Timer.data = (unsigned long)timer;
   timer->callback = callback;
   timer->userData = userData;
   timer->type = timerType;
   timer->platformInfo.cookie = LINUX_TIMER_COOKIE;
   timer->platformInfo.threadID = 0;
   timer->state = VOS_TIMER_STATE_STOPPED;
   
   return VOS_STATUS_SUCCESS;
}
#else
VOS_STATUS vos_timer_init( vos_timer_t *timer, VOS_TIMER_TYPE timerType, 
                           vos_timer_callback_t callback, v_PVOID_t userData )
{
   // Check for invalid pointer
   if ((timer == NULL) || (callback == NULL)) 
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Null params being passed",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
   }
   
   // set the various members of the timer structure 
   // with arguments passed or with default values
   spin_lock_init(&timer->platformInfo.spinlock);
   init_timer(&(timer->platformInfo.Timer));
   timer->platformInfo.Timer.function = vos_linux_timer_callback;
   timer->platformInfo.Timer.data = (unsigned long)timer;
   timer->callback = callback;
   timer->userData = userData;
   timer->type = timerType;
   timer->platformInfo.cookie = LINUX_TIMER_COOKIE;
   timer->platformInfo.threadID = 0;
   timer->state = VOS_TIMER_STATE_STOPPED;
   
   return VOS_STATUS_SUCCESS;
}
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
#ifdef TIMER_MANAGER
VOS_STATUS vos_timer_destroy ( vos_timer_t *timer )
{
   VOS_STATUS vStatus=VOS_STATUS_SUCCESS;
   unsigned long flags;
   
   // Check for invalid pointer
   if ( NULL == timer )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Null timer pointer being passed",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
   }
       
   // Check if timer refers to an uninitialized object
   if ( LINUX_TIMER_COOKIE != timer->platformInfo.cookie )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Cannot destroy uninitialized timer",__func__);
      return VOS_STATUS_E_INVAL;
   }
   
   spin_lock_irqsave(&vosTimerList.lock, flags);
   vStatus = hdd_list_remove_node(&vosTimerList, &timer->ptimerNode->pNode);
   spin_unlock_irqrestore(&vosTimerList.lock, flags);
   if(vStatus != VOS_STATUS_SUCCESS)
   {
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }
   vos_mem_free(timer->ptimerNode);
   

   spin_lock_irqsave( &timer->platformInfo.spinlock,flags );
   
   switch ( timer->state )
   {
      case VOS_TIMER_STATE_STARTING:
         vStatus = VOS_STATUS_E_BUSY;
         break;
      case VOS_TIMER_STATE_RUNNING:
         /* Stop the timer first */
         del_timer(&(timer->platformInfo.Timer));
         vStatus = VOS_STATUS_SUCCESS;
         break;
      case VOS_TIMER_STATE_STOPPED:
         vStatus = VOS_STATUS_SUCCESS;
         break;
      case VOS_TIMER_STATE_UNUSED:
         vStatus = VOS_STATUS_E_ALREADY;
         break;
      default:
         vStatus = VOS_STATUS_E_FAULT;
         break;
   }

   if ( VOS_STATUS_SUCCESS == vStatus )
   {
      timer->platformInfo.cookie = LINUX_INVALID_TIMER_COOKIE;
      timer->state = VOS_TIMER_STATE_UNUSED;
      spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
      return vStatus;
   }

   spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );


   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Cannot destroy timer in state = %d",__func__, timer->state);
   VOS_ASSERT(0);

   return vStatus;   
}

#else
VOS_STATUS vos_timer_destroy ( vos_timer_t *timer )
{
   VOS_STATUS vStatus=VOS_STATUS_SUCCESS;
   unsigned long flags;
   
   // Check for invalid pointer
   if ( NULL == timer )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Null timer pointer being passed",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
   }
       
   // Check if timer refers to an uninitialized object
   if ( LINUX_TIMER_COOKIE != timer->platformInfo.cookie )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
                "%s: Cannot destroy uninitialized timer",__func__);
      return VOS_STATUS_E_INVAL;
   }
   spin_lock_irqsave( &timer->platformInfo.spinlock,flags );
   
   switch ( timer->state )
   {
      case VOS_TIMER_STATE_STARTING:
         vStatus = VOS_STATUS_E_BUSY;
         break;
      case VOS_TIMER_STATE_RUNNING:
         /* Stop the timer first */
         del_timer(&(timer->platformInfo.Timer));
         vStatus = VOS_STATUS_SUCCESS;
         break;
      case VOS_TIMER_STATE_STOPPED:
         vStatus = VOS_STATUS_SUCCESS;
         break;
      case VOS_TIMER_STATE_UNUSED:
         vStatus = VOS_STATUS_E_ALREADY;
         break;
      default:
         vStatus = VOS_STATUS_E_FAULT;
         break;
   }

   if ( VOS_STATUS_SUCCESS == vStatus )
   {
      timer->platformInfo.cookie = LINUX_INVALID_TIMER_COOKIE;
      timer->state = VOS_TIMER_STATE_UNUSED;
      spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
      return vStatus;
   }

   spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
             "%s: Cannot destroy timer in state = %d",__func__, timer->state);
   VOS_ASSERT(0);

   return vStatus;   
}
#endif

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
  
  \return VOS_STATUS_SUCCESS - timer was successfully started.
  
          VOS_STATUS_E_ALREADY - The implementation has detected an attempt 
          to start a timer while it is already started.  The timer must 
          be stopped or expire before it can be started again.

          VOS_STATUS_E_INVAL - The value specified by timer is invalid.
          
          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_timer_start( vos_timer_t *timer, v_U32_t expirationTime )
{
   unsigned long flags;
     
   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH, 
             "Timer Addr inside voss_start : 0x%p ", timer );
   
   // Check for invalid pointer
   if ( NULL == timer )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s Null timer pointer being passed", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }
      
   // Check if timer refers to an uninitialized object
   if ( LINUX_TIMER_COOKIE != timer->platformInfo.cookie )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Cannot start uninitialized timer",__func__);
      if ( LINUX_INVALID_TIMER_COOKIE != timer->platformInfo.cookie )
      {
         VOS_ASSERT(0);
      }
      return VOS_STATUS_E_INVAL;
   }

   // Check if timer has expiration time less than 10 ms
   if ( expirationTime < 10 )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Cannot start a "
                "timer with expiration less than 10 ms", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }
      
   // make sure the remainer of the logic isn't interrupted
   spin_lock_irqsave( &timer->platformInfo.spinlock,flags );

   // Ensure if the timer can be started
   if ( VOS_TIMER_STATE_STOPPED != timer->state )
   {  
      spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: Cannot start timer in state = %d ",__func__, timer->state);
      return VOS_STATUS_E_ALREADY;
   }
      
   // Start the timer
   mod_timer( &(timer->platformInfo.Timer),
              jiffies + msecs_to_jiffies(expirationTime)); 

   timer->state = VOS_TIMER_STATE_RUNNING;

   // Get the thread ID on which the timer is being started
   timer->platformInfo.threadID  = current->pid;

   if ( VOS_TIMER_TYPE_WAKE_APPS == timer->type )
   {
      persistentTimerCount++;
      if ( 1 == persistentTimerCount )
      {
         // Since we now have one persistent timer, we need to disallow sleep
         // sleep_negate_okts( sleepClientHandle );
      }
   }

   spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
  
   return VOS_STATUS_SUCCESS;
}


/*--------------------------------------------------------------------------
  
  \brief vos_timer_stop() - Stop a vOSS Timer

  The \a vos_timer_stop() function stops a timer that has been started but
  has not expired, essentially cancelling the 'start' request.
   
  After a timer is stopped, it goes back to the state it was in after it
  was created and can be started again via a call to vos_timer_start().
  
  \param timer - the timer object to be stopped
    
  \return VOS_STATUS_SUCCESS - timer was successfully stopped.
  
          VOS_STATUS_E_INVAL - The value specified by timer is invalid.
          
          VOS_STATUS_E_FAULT  - timer is an invalid pointer.     
  \sa
  
  ------------------------------------------------------------------------*/
VOS_STATUS vos_timer_stop ( vos_timer_t *timer )
{
   unsigned long flags;

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH, 
               "%s: Timer Addr inside voss_stop : 0x%p",__func__,timer );

   // Check for invalid pointer
   if ( NULL == timer )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s Null timer pointer being passed", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }

   // Check if timer refers to an uninitialized object
   if ( LINUX_TIMER_COOKIE != timer->platformInfo.cookie )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
          "%s: Cannot stop uninitialized timer",__func__);
      if ( LINUX_INVALID_TIMER_COOKIE != timer->platformInfo.cookie )
      {
         VOS_ASSERT(0);
      }
      return VOS_STATUS_E_INVAL;
   }
      
   // Ensure the timer state is correct
   spin_lock_irqsave( &timer->platformInfo.spinlock,flags );

   if ( VOS_TIMER_STATE_RUNNING != timer->state )
   {
      spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: Cannot stop timer in state = %d",
                __func__, timer->state);
      return VOS_STATUS_SUCCESS;
   }
   
   timer->state = VOS_TIMER_STATE_STOPPED;

   del_timer(&(timer->platformInfo.Timer));
       
   spin_unlock_irqrestore( &timer->platformInfo.spinlock,flags );
      
   tryAllowingSleep( timer->type );
   
   return VOS_STATUS_SUCCESS;
}


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
v_TIME_t vos_timer_get_system_ticks( v_VOID_t )
{
   return( jiffies_to_msecs(jiffies) / 10 );
}


/*--------------------------------------------------------------------------
 
  \brief vos_timer_get_system_time() - Get the system time in milliseconds

  The \a vos_timer_get_system_time() function returns the number of milliseconds 
  that have elapsed since the system was started
    
  \returns - The current system time in milliseconds.
  
  \sa
  
  ------------------------------------------------------------------------*/
v_TIME_t vos_timer_get_system_time( v_VOID_t )
{
   struct timeval tv;
   do_gettimeofday(&tv);
   return tv.tv_sec*1000 + tv.tv_usec/1000;  
}
