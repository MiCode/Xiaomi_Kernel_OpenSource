/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_SCHED_H )
#define __VOS_SCHED_H

/**=========================================================================
  
  \file  vos_sched.h
  
  \brief virtual Operating System Servies (vOSS)
               
   Definitions for some of the internal data type that is internally used 
   by the vOSS scheduler on Windows Mobile.
   
   This file defines a vOSS message queue on Win Mobile and give some
   insights about how the scheduler implements the execution model supported
   by vOSS.
    
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/*=========================================================================== 
    
                       EDIT HISTORY FOR FILE 
   
   
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
   
  $Header:$ $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
  09/15/08    lac    Removed hardcoded #define for VOS_TRACE.
  06/12/08    hba    Created module. 
     
===========================================================================*/ 

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_event.h>
#include <vos_nvitem.h>
#include <vos_mq.h>
#include "i_vos_types.h"
#include "i_vos_packet.h"
#include <linux/wait.h>
#include <linux/wakelock.h>

#define TX_POST_EVENT_MASK               0x001
#define TX_SUSPEND_EVENT_MASK            0x002
#define MC_POST_EVENT_MASK               0x001
#define MC_SUSPEND_EVENT_MASK            0x002
#define RX_POST_EVENT_MASK               0x001
#define RX_SUSPEND_EVENT_MASK            0x002
#define TX_SHUTDOWN_EVENT_MASK           0x010
#define MC_SHUTDOWN_EVENT_MASK           0x010
#define RX_SHUTDOWN_EVENT_MASK           0x010
#define WD_POST_EVENT_MASK               0x001
#define WD_SHUTDOWN_EVENT_MASK           0x002
#define WD_CHIP_RESET_EVENT_MASK         0x004
#define WD_WLAN_SHUTDOWN_EVENT_MASK      0x008
#define WD_WLAN_REINIT_EVENT_MASK        0x010

 
 
/*
** Maximum number of messages in the system
** These are buffers to account for all current messages 
** with some accounting of what we think is a 
** worst-case scenario.  Must be able to handle all
** incoming frames, as well as overhead for internal
** messaging
*/
#define VOS_CORE_MAX_MESSAGES           (VPKT_NUM_RX_RAW_PACKETS + 32)


/*
** vOSS Message queue definition.
*/
typedef struct _VosMqType
{
  /* Lock use to synchronize access to this message queue */
  spinlock_t       mqLock;

  /* List of vOS Messages waiting on this queue */
  struct list_head  mqList;

} VosMqType, *pVosMqType;


/*
** vOSS Scheduler context
** The scheduler context contains the following:
**   ** the messages queues
**   ** the handle to the tread
**   ** pointer to the events that gracefully shutdown the MC and Tx threads
**    
*/
typedef struct _VosSchedContext
{
  /* Place holder to the VOSS Context */ 
   v_PVOID_t           pVContext; 
  /* WDA Message queue on the Main thread*/
   VosMqType           wdaMcMq;



   /* PE Message queue on the Main thread*/
   VosMqType           peMcMq;

   /* SME Message queue on the Main thread*/
   VosMqType           smeMcMq;

   /* TL Message queue on the Main thread */
   VosMqType           tlMcMq;

   /* SYS Message queue on the Main thread */
   VosMqType           sysMcMq;

  /* WDI Message queue on the Main thread*/
   VosMqType           wdiMcMq;

   /* WDI Message queue on the Tx Thread*/
   VosMqType           wdiTxMq;

   /* WDI Message queue on the Rx Thread*/
   VosMqType           wdiRxMq;

   /* TL Message queue on the Tx thread */
   VosMqType           tlTxMq;

   /* TL Message queue on the Rx thread */
   VosMqType           tlRxMq;

   /* SYS Message queue on the Tx thread */
   VosMqType           sysTxMq;

   VosMqType           sysRxMq;

   /* Handle of Event for MC thread to signal startup */
   struct completion   McStartEvent;

   /* Handle of Event for Tx thread to signal startup */
   struct completion   TxStartEvent;

   /* Handle of Event for Rx thread to signal startup */
   struct completion   RxStartEvent;

   struct task_struct* McThread;

   /* TX Thread handle */
   
   struct task_struct*   TxThread;

   /* RX Thread handle */
   struct task_struct*   RxThread;


   /* completion object for MC thread shutdown */
   struct completion   McShutdown; 

   /* completion object for Tx thread shutdown */
   struct completion   TxShutdown; 

   /* completion object for Rx thread shutdown */
   struct completion   RxShutdown;

   /* Wait queue for MC thread */
   wait_queue_head_t mcWaitQueue;

   unsigned long     mcEventFlag;

   /* Wait queue for Tx thread */
   wait_queue_head_t txWaitQueue;

   unsigned long     txEventFlag;

   /* Wait queue for Rx thread */
   wait_queue_head_t rxWaitQueue;

   unsigned long     rxEventFlag;
   
   /* Completion object to resume Mc thread */
   struct completion ResumeMcEvent;

   /* Completion object to resume Tx thread */
   struct completion ResumeTxEvent;

   /* Completion object to resume Rx thread */
   struct completion ResumeRxEvent;

   /* lock to make sure that McThread and TxThread Suspend/resume mechanism is in sync*/
   spinlock_t McThreadLock;
   spinlock_t TxThreadLock;
   spinlock_t RxThreadLock;

} VosSchedContext, *pVosSchedContext;

/*
** VOSS watchdog context
** The watchdog context contains the following:
** The messages queues and events
** The handle to the thread
**    
*/
typedef struct _VosWatchdogContext
{

   /* Place holder to the VOSS Context */ 
   v_PVOID_t pVContext; 

   /* Handle of Event for Watchdog thread to signal startup */
   struct completion WdStartEvent;

   /* Watchdog Thread handle */
  
   struct task_struct* WdThread;

   /* completion object for Watchdog thread shutdown */
   struct completion WdShutdown; 

   /* Wait queue for Watchdog thread */
   wait_queue_head_t wdWaitQueue;

   /* Event flag for events handled by Watchdog */
   unsigned long wdEventFlag;

   v_BOOL_t resetInProgress;

   v_BOOL_t isFatalError;

   /* Lock for preventing multiple reset being triggered simultaneously */
   spinlock_t wdLock;

} VosWatchdogContext, *pVosWatchdogContext;

/*
** vOSS Sched Msg Wrapper
** Wrapper messages so that they can be chained to their respective queue
** in the scheduler.
*/
typedef struct _VosMsgWrapper
{
   /* Message node */
   struct list_head  msgNode;

   /* the Vos message it is associated to */
   vos_msg_t    *pVosMsg;

} VosMsgWrapper, *pVosMsgWrapper;



typedef struct _VosContextType
{                                                  
   /* Messages buffers */
   vos_msg_t           aMsgBuffers[VOS_CORE_MAX_MESSAGES];

   VosMsgWrapper       aMsgWrappers[VOS_CORE_MAX_MESSAGES];
   
   /* Free Message queue*/
   VosMqType           freeVosMq;

   /* Scheduler Context */
   VosSchedContext     vosSched;

   /* Watchdog Context */
   VosWatchdogContext  vosWatchdog;

   /* HDD Module Context  */
   v_VOID_t           *pHDDContext;

   /* HDD SoftAP Module Context  */
   v_VOID_t           *pHDDSoftAPContext;

   /* TL Module Context  */
   v_VOID_t           *pTLContext;

   /* MAC Module Context  */
   v_VOID_t           *pMACContext;

   /* BAP Context */
   v_VOID_t           *pBAPContext;

   /* SAP Context */
   v_VOID_t           *pSAPContext;
   
   /* VOS Packet Context */
   vos_pkt_context_t   vosPacket; 

   vos_event_t         ProbeEvent;

   volatile v_U8_t     isLogpInProgress;

   vos_event_t         wdaCompleteEvent;

   /* WDA Context */
   v_VOID_t            *pWDAContext;

   volatile v_U8_t    isLoadUnloadInProgress;

   /* SSR re-init in progress */
   volatile v_U8_t     isReInitInProgress;

   /* NV BIN Version */
   eNvVersionType     nvVersion;

} VosContextType, *pVosContextType;



/*--------------------------------------------------------------------------- 
  Function declarations and documenation
---------------------------------------------------------------------------*/
 
int vos_sched_is_tx_thread(int threadID);
int vos_sched_is_rx_thread(int threadID);
/*---------------------------------------------------------------------------
  
  \brief vos_sched_open() - initialize the vOSS Scheduler  
    
  The \a vos_sched_open() function initializes the vOSS Scheduler
  Upon successful initialization:
  
     - All the message queues are initialized
     
     - The Main Controller thread is created and ready to receive and
       dispatch messages.
     
     - The Tx thread is created and ready to receive and dispatch messages
      
  
  \param  pVosContext - pointer to the global vOSS Context
  
  \param  pVosSchedContext - pointer to a previously allocated buffer big
          enough to hold a scheduler context.       
  \
  
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the scheduler

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the scheduler
          
          VOS_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
          function 
          
          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/   
          
  \sa vos_sched_open()
  
  -------------------------------------------------------------------------*/
VOS_STATUS vos_sched_open( v_PVOID_t pVosContext, 
                           pVosSchedContext pSchedCxt,
                           v_SIZE_t SchedCtxSize);

/*---------------------------------------------------------------------------
  
  \brief vos_watchdog_open() - initialize the vOSS watchdog  
    
  The \a vos_watchdog_open() function initializes the vOSS watchdog. Upon successful 
        initialization, the watchdog thread is created and ready to receive and  process messages.
     
   
  \param  pVosContext - pointer to the global vOSS Context
  
  \param  pWdContext - pointer to a previously allocated buffer big
          enough to hold a watchdog context.       

  \return VOS_STATUS_SUCCESS - Watchdog was successfully initialized and 
          is ready to be used.
  
          VOS_STATUS_E_RESOURCES - System resources (other than memory) 
          are unavailable to initilize the Watchdog

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize 
          the Watchdog
          
          VOS_STATUS_E_INVAL - Invalid parameter passed to the Watchdog Open
          function 
          
          VOS_STATUS_E_FAILURE - Failure to initialize the Watchdog/   

  \sa vos_watchdog_open()
  
  -------------------------------------------------------------------------*/

VOS_STATUS vos_watchdog_open

(
  v_PVOID_t           pVosContext,
  pVosWatchdogContext pWdContext,
  v_SIZE_t            wdCtxSize
);

/*---------------------------------------------------------------------------
  
  \brief vos_sched_close() - Close the vOSS Scheduler  
    
  The \a vos_sched_closes() function closes the vOSS Scheduler
  Upon successful closing:
  
     - All the message queues are flushed
     
     - The Main Controller thread is closed
     
     - The Tx thread is closed
      
  
  \param  pVosContext - pointer to the global vOSS Context
  
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and 
          is ready to be used.
          
          VOS_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
          function 
          
          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/   
          
  \sa vos_sched_close()
  
---------------------------------------------------------------------------*/
VOS_STATUS vos_sched_close( v_PVOID_t pVosContext);

/*---------------------------------------------------------------------------
  
  \brief vos_watchdog_close() - Close the vOSS Watchdog  
    
  The \a vos_watchdog_close() function closes the vOSS Watchdog
  Upon successful closing:
  
     - The Watchdog thread is closed
     
      
  \param  pVosContext - pointer to the global vOSS Context
  
  \return VOS_STATUS_SUCCESS - Watchdog was successfully initialized and 
          is ready to be used.
          
          VOS_STATUS_E_INVAL - Invalid parameter passed 
          
          VOS_STATUS_E_FAILURE - Failure to initialize the Watchdog/   
          
  \sa vos_watchdog_close()
  
---------------------------------------------------------------------------*/
VOS_STATUS vos_watchdog_close ( v_PVOID_t pVosContext );

/* Helper routines provided to other VOS API's */
VOS_STATUS vos_mq_init(pVosMqType pMq);
void vos_mq_deinit(pVosMqType pMq);
void vos_mq_put(pVosMqType pMq, pVosMsgWrapper pMsgWrapper);
pVosMsgWrapper vos_mq_get(pVosMqType pMq);
v_BOOL_t vos_is_mq_empty(pVosMqType pMq);
pVosSchedContext get_vos_sched_ctxt(void);
pVosWatchdogContext get_vos_watchdog_ctxt(void);
VOS_STATUS vos_sched_init_mqs   (pVosSchedContext pSchedContext);
void vos_sched_deinit_mqs (pVosSchedContext pSchedContext);
void vos_sched_flush_mc_mqs  (pVosSchedContext pSchedContext);
void vos_sched_flush_tx_mqs  (pVosSchedContext pSchedContext);
void vos_sched_flush_rx_mqs  (pVosSchedContext pSchedContext);
void clearWlanResetReason(void);

void vos_timer_module_init( void );
VOS_STATUS vos_watchdog_wlan_shutdown(void);
VOS_STATUS vos_watchdog_wlan_re_init(void);
v_BOOL_t isWDresetInProgress(void);
v_BOOL_t isSsrPanicOnFailure(void);
void vos_ssr_protect(const char *caller_func);
void vos_ssr_unprotect(const char *caller_func);

#endif // #if !defined __VOSS_SCHED_H
