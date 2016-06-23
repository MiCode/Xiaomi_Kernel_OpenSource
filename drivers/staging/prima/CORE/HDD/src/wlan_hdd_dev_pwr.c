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

                       EDIT HISTORY FOR FILE 
   
   
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
   
  $Header:$   $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
  03/29/11    tbh    Created module. 

  ==========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <wlan_hdd_dev_pwr.h>
#ifdef ANI_BUS_TYPE_PLATFORM
#include <linux/wcnss_wlan.h>
#else
#include <wcnss_wlan.h>
#endif // ANI_BUS_TYP_PLATFORM

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------
 * Global variables.
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
 * Local variables.
 *-------------------------------------------------------------------------*/
/* Reference VoIP, 100msec delay make disconnect.
 * So TX sleep must be less than 100msec
 * Every 20msec TX frame will goes out.
 * 10 frame means 2seconds TX operation */
static const hdd_tmLevelAction_t thermalMigrationAction[WLAN_HDD_TM_LEVEL_MAX] =
{
   /* TM Level 0, Do nothing, just normal operaton */
   {1, 0, 0, 0, 0xFFFFF},
   /* Tm Level 1, disable TX AMPDU */
   {0, 0, 0, 0, 0xFFFFF},
   /* TM Level 2, disable AMDPU,
    * TX sleep 100msec if TX frame count is larger than 16 during 300msec */
   {0, 0, 100, 300, 16},
   /* TM Level 3, disable AMDPU,
    * TX sleep 500msec if TX frame count is larger than 11 during 500msec */
   {0, 0, 500, 500, 11},
   /* TM Level 4, MAX TM level, enter IMPS */
   {0, 1, 1000, 500, 10}
};
#ifdef HAVE_WCNSS_SUSPEND_RESUME_NOTIFY
static bool suspend_notify_sent;
#endif


/*----------------------------------------------------------------------------

   @brief Function to suspend the wlan driver.

   @param hdd_context_t pHddCtx
        Global hdd context


   @return None

----------------------------------------------------------------------------*/
static int wlan_suspend(hdd_context_t* pHddCtx)
{
   long rc = 0;

   pVosSchedContext vosSchedContext = NULL;

   /* Get the global VOSS context */
   vosSchedContext = get_vos_sched_ctxt();

   if(!vosSchedContext) {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: Global VOS_SCHED context is Null",__func__);
      return 0;
   }
   if(!vos_is_apps_power_collapse_allowed(pHddCtx))
   {
       /* Fail this suspend */
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: Fail wlan suspend: not in IMPS/BMPS", __func__);
       return -EPERM;
   }

   /*
     Suspending MC Thread, Rx Thread and Tx Thread as the platform driver is going to Suspend.     
   */
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Suspending Mc, Rx and Tx Threads",__func__);

   INIT_COMPLETION(pHddCtx->tx_sus_event_var);

   /* Indicate Tx Thread to Suspend */
   set_bit(TX_SUSPEND_EVENT_MASK, &vosSchedContext->txEventFlag);

   wake_up_interruptible(&vosSchedContext->txWaitQueue);

   /* Wait for Suspend Confirmation from Tx Thread */
   rc = wait_for_completion_interruptible_timeout(&pHddCtx->tx_sus_event_var, msecs_to_jiffies(200));

   if (rc <= 0)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
           "%s: TX Thread: timeout while suspending %ld"
           , __func__, rc);
      /* There is a race condition here, where the TX Thread can process the
       * SUSPEND_EVENT even after the wait_for_completion has timed out.
       * Check the SUSPEND_EVENT_MASK, if it is already cleared by the TX
       * Thread then it means it is going to suspend, so do not return failure
       * from here.
       */
      if (!test_and_clear_bit(TX_SUSPEND_EVENT_MASK,
                              &vosSchedContext->txEventFlag))
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: TX Thread: will still suspend", __func__);
         goto tx_suspend;
      }

      return -ETIME;
   }

tx_suspend:
   /* Set the Tx Thread as Suspended */
   pHddCtx->isTxThreadSuspended = TRUE;

   INIT_COMPLETION(pHddCtx->rx_sus_event_var);

   /* Indicate Rx Thread to Suspend */
   set_bit(RX_SUSPEND_EVENT_MASK, &vosSchedContext->rxEventFlag);

   wake_up_interruptible(&vosSchedContext->rxWaitQueue);

   /* Wait for Suspend Confirmation from Rx Thread */
   rc = wait_for_completion_interruptible_timeout(&pHddCtx->rx_sus_event_var, msecs_to_jiffies(200));

   if (rc <= 0)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s: RX Thread: timeout while suspending %ld", __func__, rc);
       /* There is a race condition here, where the RX Thread can process the
        * SUSPEND_EVENT even after the wait_for_completion has timed out.
        * Check the SUSPEND_EVENT_MASK, if it is already cleared by the RX
        * Thread then it means it is going to suspend, so do not return failure
        * from here.
        */
       if (!test_and_clear_bit(RX_SUSPEND_EVENT_MASK,
                               &vosSchedContext->rxEventFlag))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "%s: RX Thread: will still suspend", __func__);
           goto rx_suspend;
       }

       /* Indicate Tx Thread to Resume */
       complete(&vosSchedContext->ResumeTxEvent);

       /* Set the Tx Thread as Resumed */
       pHddCtx->isTxThreadSuspended = FALSE;

       return -ETIME;
   }

rx_suspend:
   /* Set the Rx Thread as Suspended */
   pHddCtx->isRxThreadSuspended = TRUE;

   INIT_COMPLETION(pHddCtx->mc_sus_event_var);

   /* Indicate MC Thread to Suspend */
   set_bit(MC_SUSPEND_EVENT_MASK, &vosSchedContext->mcEventFlag);

   wake_up_interruptible(&vosSchedContext->mcWaitQueue);

   /* Wait for Suspend Confirmation from MC Thread */
   rc = wait_for_completion_interruptible_timeout(&pHddCtx->mc_sus_event_var, msecs_to_jiffies(200));

   if(!rc)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s: MC Thread: timeout while suspending %ld",
            __func__, rc);
       /* There is a race condition here, where the MC Thread can process the
        * SUSPEND_EVENT even after the wait_for_completion has timed out.
        * Check the SUSPEND_EVENT_MASK, if it is already cleared by the MC
        * Thread then it means it is going to suspend, so do not return failure
        * from here.
        */
       if (!test_and_clear_bit(MC_SUSPEND_EVENT_MASK,
                               &vosSchedContext->mcEventFlag))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "%s: MC Thread: will still suspend", __func__);
           goto mc_suspend;
       }

       /* Indicate Rx Thread to Resume */
       complete(&vosSchedContext->ResumeRxEvent);

       /* Set the Rx Thread as Resumed */
       pHddCtx->isRxThreadSuspended = FALSE;

       /* Indicate Tx Thread to Resume */
       complete(&vosSchedContext->ResumeTxEvent);

       /* Set the Tx Thread as Resumed */
       pHddCtx->isTxThreadSuspended = FALSE;

       return -ETIME;
   }

mc_suspend:
   /* Set the Mc Thread as Suspended */
   pHddCtx->isMcThreadSuspended = TRUE;
   
   /* Set the Station state as Suspended */
   pHddCtx->isWlanSuspended = TRUE;

   return 0;
}

/*----------------------------------------------------------------------------

   @brief Function to resume the wlan driver.

   @param hdd_context_t pHddCtx
        Global hdd context


   @return None

----------------------------------------------------------------------------*/
static void wlan_resume(hdd_context_t* pHddCtx)
{
   pVosSchedContext vosSchedContext = NULL;

   //Get the global VOSS context.
   vosSchedContext = get_vos_sched_ctxt();

   if(!vosSchedContext) {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: Global VOS_SCHED context is Null",__func__);
      return;
   }

   /*
     Resuming Mc, Rx and Tx Thread as platform Driver is resuming.
   */
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Resuming Mc, Rx and Tx Thread",__func__);

   /* Indicate MC Thread to Resume */
   complete(&vosSchedContext->ResumeMcEvent);

   /* Set the Mc Thread as Resumed */
   pHddCtx->isMcThreadSuspended = FALSE;

   /* Indicate Rx Thread to Resume */
   complete(&vosSchedContext->ResumeRxEvent);

   /* Set the Rx Thread as Resumed */
   pHddCtx->isRxThreadSuspended = FALSE;

   /* Indicate Tx Thread to Resume */
   complete(&vosSchedContext->ResumeTxEvent);

   /* Set the Tx Thread as Resumed */
   pHddCtx->isTxThreadSuspended = FALSE;

   /* Set the Station state as Suspended */
   pHddCtx->isWlanSuspended = FALSE;
}

/*----------------------------------------------------------------------------

   @brief Function to suspend the wlan driver.
   This function will get called by platform driver Suspend on System Suspend

   @param dev    platform_func_device


   @return None

----------------------------------------------------------------------------*/
int hddDevSuspendHdlr(struct device *dev)
{
   int ret = 0;
   hdd_context_t* pHddCtx = NULL;

   pHddCtx =  (hdd_context_t*)wcnss_wlan_get_drvdata(dev);

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: WLAN suspended by platform driver",__func__);

   /* Get the HDD context */
   if(!pHddCtx) {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return 0;
   }

   if(pHddCtx->isWlanSuspended == TRUE)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: WLAN is already in suspended state",__func__);
      return 0;
   }

   /* Suspend the wlan driver */
   ret = wlan_suspend(pHddCtx);
   if(ret != 0)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: Not able to suspend wlan",__func__);
      return ret;
   }

#ifdef HAVE_WCNSS_SUSPEND_RESUME_NOTIFY
   if(hdd_is_suspend_notify_allowed(pHddCtx))
   {
      wcnss_suspend_notify();
      suspend_notify_sent = true;
   }
#endif
   return 0;
}

/*----------------------------------------------------------------------------

   @brief Function to resume the wlan driver.
   This function will get called by platform driver Resume on System Resume 

   @param dev    platform_func_device


   @return None

----------------------------------------------------------------------------*/
int hddDevResumeHdlr(struct device *dev)
{
   hdd_context_t* pHddCtx = NULL;

   pHddCtx =  (hdd_context_t*)wcnss_wlan_get_drvdata(dev);

   VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed by Android OS",__func__);

   if(pHddCtx->isWlanSuspended != TRUE)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_FATAL,"%s: WLAN is already in resumed state",__func__);
      return 0;
   }

   /* Resume the wlan driver */
   wlan_resume(pHddCtx);
#ifdef HAVE_WCNSS_SUSPEND_RESUME_NOTIFY
   if(suspend_notify_sent == true)
   {
      wcnss_resume_notify();
      suspend_notify_sent = false;
   }
#endif

   return 0;
}

static const struct dev_pm_ops pm_ops = {
   .suspend = hddDevSuspendHdlr,
   .resume = hddDevResumeHdlr,
};

/*----------------------------------------------------------------------------
 *

   @brief Registration function.
        Register suspend, resume callback functions with platform driver. 

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddRegisterPmOps(hdd_context_t *pHddCtx)
{
    wcnss_wlan_set_drvdata(pHddCtx->parent_dev, pHddCtx);
#ifndef FEATURE_R33D
    wcnss_wlan_register_pm_ops(pHddCtx->parent_dev, &pm_ops);
#endif /* FEATURE_R33D */
    return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

   @brief De-registration function.
        Deregister the suspend, resume callback functions with platform driver

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       De-Registration Success
        VOS_STATUS_E_FAILURE     De-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDeregisterPmOps(hdd_context_t *pHddCtx)
{
#ifndef FEATURE_R33D
    wcnss_wlan_unregister_pm_ops(pHddCtx->parent_dev, &pm_ops);
#endif /* FEATURE_R33D */
    return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

   @brief TX frame block timeout handler
          Resume TX, and reset TX frame count

   @param hdd_context_t pHddCtx
        Global hdd context

   @return NONE

----------------------------------------------------------------------------*/
void hddDevTmTxBlockTimeoutHandler(void *usrData)
{
   hdd_context_t        *pHddCtx = (hdd_context_t *)usrData;
   hdd_adapter_t        *staAdapater;
   /* Sanity, This should not happen */
   if(NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: NULL Context", __func__);
      VOS_ASSERT(0);
      return;
   }

   staAdapater = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);

   if(NULL == staAdapater)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: NULL Adapter", __func__);
      VOS_ASSERT(0);
      return;
   }

   if(mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: Acquire lock fail", __func__);
      return;
   }
   pHddCtx->tmInfo.txFrameCount = 0;

   /* Resume TX flow */
    
   netif_tx_wake_all_queues(staAdapater->dev);
   pHddCtx->tmInfo.qBlocked = VOS_FALSE;
   mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);

   return;
}

/*----------------------------------------------------------------------------

   @brief TM Level Change handler
          Received Tm Level changed notification

   @param dev : Device context
          changedTmLevel : Changed new TM level

   @return 

----------------------------------------------------------------------------*/
void hddDevTmLevelChangedHandler(struct device *dev, int changedTmLevel)
{
   hdd_context_t        *pHddCtx = NULL;
   WLAN_TmLevelEnumType  newTmLevel = changedTmLevel;
   hdd_adapter_t        *staAdapater;

   pHddCtx =  (hdd_context_t*)wcnss_wlan_get_drvdata(dev);

   if ((pHddCtx->tmInfo.currentTmLevel == newTmLevel) ||
       (!pHddCtx->cfg_ini->thermalMitigationEnable))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_WARN,
                "%s: TM Not enabled %d or Level does not changed %d",
                __func__, pHddCtx->cfg_ini->thermalMitigationEnable, newTmLevel);
      /* TM Level does not changed,
       * Or feature does not enabled
       * do nothing */
      return;
   }

   /* Only STA mode support TM now
    * all other mode, TM feature should be disabled */
   if (~VOS_STA & pHddCtx->concurrency_mode)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: CMODE 0x%x, TM disable",
                __func__, pHddCtx->concurrency_mode);
      newTmLevel = WLAN_HDD_TM_LEVEL_0;
   }

   if ((newTmLevel < WLAN_HDD_TM_LEVEL_0) ||
       (newTmLevel >= WLAN_HDD_TM_LEVEL_MAX))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: TM level %d out of range",
                __func__, newTmLevel);
      return;
   }

   if (newTmLevel != WLAN_HDD_TM_LEVEL_4)
      sme_SetTmLevel(pHddCtx->hHal, newTmLevel, 0);

   if (mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: Acquire lock fail", __func__);
      return;
   }

   pHddCtx->tmInfo.currentTmLevel = newTmLevel;
   pHddCtx->tmInfo.txFrameCount = 0;
   vos_mem_copy(&pHddCtx->tmInfo.tmAction,
                &thermalMigrationAction[newTmLevel],
                sizeof(hdd_tmLevelAction_t));


   if (pHddCtx->tmInfo.tmAction.enterImps)
   {
      staAdapater = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
      if (staAdapater)
      {
         if (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(staAdapater)))
         {
            sme_RoamDisconnect(pHddCtx->hHal,
                               staAdapater->sessionId, 
                               eCSR_DISCONNECT_REASON_UNSPECIFIED);
         }
      }
   }

   mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);

   return;
}

/*----------------------------------------------------------------------------

   @brief Register function
        Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmRegisterNotifyCallback(hdd_context_t *pHddCtx)
{
   VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_INFO,
             "%s: Register TM Handler", __func__);

   wcnss_register_thermal_mitigation(pHddCtx->parent_dev ,hddDevTmLevelChangedHandler);

   /* Set Default TM Level as Lowest, do nothing */
   pHddCtx->tmInfo.currentTmLevel = WLAN_HDD_TM_LEVEL_0;
   vos_mem_zero(&pHddCtx->tmInfo.tmAction, sizeof(hdd_tmLevelAction_t)); 
   vos_timer_init(&pHddCtx->tmInfo.txSleepTimer,
                  VOS_TIMER_TYPE_SW,
                  hddDevTmTxBlockTimeoutHandler,
                  (void *)pHddCtx);
   mutex_init(&pHddCtx->tmInfo.tmOperationLock);
   pHddCtx->tmInfo.txFrameCount = 0;
   pHddCtx->tmInfo.blockedQueue = NULL;
   pHddCtx->tmInfo.qBlocked     = VOS_FALSE;
   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

   @brief Un-Register function
        Un-Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Un-Registration Success
        VOS_STATUS_E_FAILURE     Un-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmUnregisterNotifyCallback(hdd_context_t *pHddCtx)
{
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;

   wcnss_unregister_thermal_mitigation(hddDevTmLevelChangedHandler);

   if(VOS_TIMER_STATE_RUNNING ==
           vos_timer_getCurrentState(&pHddCtx->tmInfo.txSleepTimer))
   {
       vosStatus = vos_timer_stop(&pHddCtx->tmInfo.txSleepTimer);
       if(!VOS_IS_STATUS_SUCCESS(vosStatus))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                "%s: Timer stop fail", __func__);
       }
   }

   // Destroy the vos timer...
   vosStatus = vos_timer_destroy(&pHddCtx->tmInfo.txSleepTimer);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                            "%s: Fail to destroy timer", __func__);
   }

   return VOS_STATUS_SUCCESS;
}

