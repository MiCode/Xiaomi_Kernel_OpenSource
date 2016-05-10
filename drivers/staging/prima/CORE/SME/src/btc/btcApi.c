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

/******************************************************************************
*
* Name:  btcApi.c
*
* Description: Routines that make up the BTC API.
*
* Copyright 2008 (c) Qualcomm, Incorporated. All Rights Reserved.
* Qualcomm Confidential and Proprietary.
*
******************************************************************************/
#include "wlan_qct_wda.h"
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
#include "aniGlobal.h"
#include "smsDebug.h"
#include "btcApi.h"
#include "cfgApi.h"
#include "pmc.h"
#include "smeQosInternal.h"
#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "vos_diag_core_event.h"
#include "vos_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
static void btcLogEvent (tHalHandle hHal, tpSmeBtEvent pBtEvent);
static void btcRestoreHeartBeatMonitoringHandle(void* hHal);
static void btcUapsdCheck( tpAniSirGlobal pMac, tpSmeBtEvent pBtEvent );
VOS_STATUS btcCheckHeartBeatMonitoring(tHalHandle hHal, tpSmeBtEvent pBtEvent);
static void btcPowerStateCB( v_PVOID_t pContext, tPmcState pmcState );
static VOS_STATUS btcDeferEvent( tpAniSirGlobal pMac, tpSmeBtEvent pEvent );
static VOS_STATUS btcDeferDisconnEvent( tpAniSirGlobal pMac, tpSmeBtEvent pEvent );
#ifdef FEATURE_WLAN_DIAG_SUPPORT
static void btcDiagEventLog (tHalHandle hHal, tpSmeBtEvent pBtEvent);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
/* ---------------------------------------------------------------------------
    \fn btcOpen
    \brief  API to init the BTC Events Layer
    \param  hHal - The handle returned by macOpen.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  success
            VOS_STATUS_SUCCESS  failure
  ---------------------------------------------------------------------------*/
VOS_STATUS btcOpen (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   VOS_STATUS vosStatus;
   int i;

   /* Initialize BTC configuartion. */
   pMac->btc.btcConfig.btcExecutionMode = BTC_SMART_COEXISTENCE;
   pMac->btc.btcConfig.btcConsBtSlotsToBlockDuringDhcp = 0;
   pMac->btc.btcConfig.btcA2DPBtSubIntervalsDuringDhcp = BTC_MAX_NUM_ACL_BT_SUB_INTS;
   pMac->btc.btcConfig.btcBtIntervalMode1 = BTC_BT_INTERVAL_MODE1_DEFAULT;
   pMac->btc.btcConfig.btcWlanIntervalMode1 = BTC_WLAN_INTERVAL_MODE1_DEFAULT;
   pMac->btc.btcConfig.btcActionOnPmFail = BTC_START_NEXT;

   pMac->btc.btcConfig.btcStaticLenInqBt = BTC_STATIC_BT_LEN_INQ_DEF;
   pMac->btc.btcConfig.btcStaticLenPageBt = BTC_STATIC_BT_LEN_PAGE_DEF;
   pMac->btc.btcConfig.btcStaticLenConnBt = BTC_STATIC_BT_LEN_CONN_DEF;
   pMac->btc.btcConfig.btcStaticLenLeBt = BTC_STATIC_BT_LEN_LE_DEF;
   pMac->btc.btcConfig.btcStaticLenInqWlan = BTC_STATIC_WLAN_LEN_INQ_DEF;
   pMac->btc.btcConfig.btcStaticLenPageWlan = BTC_STATIC_WLAN_LEN_PAGE_DEF;
   pMac->btc.btcConfig.btcStaticLenConnWlan = BTC_STATIC_WLAN_LEN_CONN_DEF;
   pMac->btc.btcConfig.btcStaticLenLeWlan = BTC_STATIC_WLAN_LEN_LE_DEF;
   pMac->btc.btcConfig.btcDynMaxLenBt = BTC_DYNAMIC_BT_LEN_MAX_DEF;
   pMac->btc.btcConfig.btcDynMaxLenWlan = BTC_DYNAMIC_WLAN_LEN_MAX_DEF;
   pMac->btc.btcConfig.btcMaxScoBlockPerc = BTC_SCO_BLOCK_PERC_DEF;
   pMac->btc.btcConfig.btcDhcpProtOnA2dp = BTC_DHCP_ON_A2DP_DEF;
   pMac->btc.btcConfig.btcDhcpProtOnSco = BTC_DHCP_ON_SCO_DEF;

   pMac->btc.btcReady = VOS_FALSE;
   pMac->btc.btcEventState = 0;
   pMac->btc.btcHBActive = VOS_TRUE;
   pMac->btc.btcScanCompromise = VOS_FALSE;

   for (i = 0; i < MWS_COEX_MAX_VICTIM_TABLE; i++)
   {
      pMac->btc.btcConfig.mwsCoexVictimWANFreq[i] = 0;
      pMac->btc.btcConfig.mwsCoexVictimWLANFreq[i] = 0;
      pMac->btc.btcConfig.mwsCoexVictimConfig[i] = 0;
      pMac->btc.btcConfig.mwsCoexVictimConfig2[i] = 0;
   }

   for (i = 0; i < MWS_COEX_MAX_CONFIG; i++)
   {
      pMac->btc.btcConfig.mwsCoexConfig[i] = 0;
   }

   pMac->btc.btcConfig.mwsCoexModemBackoff = 0;
   pMac->btc.btcConfig.SARPowerBackoff = 0;

   vosStatus = vos_timer_init( &pMac->btc.restoreHBTimer,
                      VOS_TIMER_TYPE_SW,
                      btcRestoreHeartBeatMonitoringHandle,
                      (void*) hHal);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcOpen: Fail to init timer");
       return VOS_STATUS_E_FAILURE;
   }
   if( !HAL_STATUS_SUCCESS(pmcRegisterDeviceStateUpdateInd( pMac, btcPowerStateCB, pMac )) )
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcOpen: Fail to register PMC callback");
       return VOS_STATUS_E_FAILURE;
   }
   return VOS_STATUS_SUCCESS;
}
/* ---------------------------------------------------------------------------
    \fn btcClose
    \brief  API to exit the BTC Events Layer
    \param  hHal - The handle returned by macOpen.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  success
            VOS_STATUS_SUCCESS  failure
  ---------------------------------------------------------------------------*/
VOS_STATUS btcClose (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   VOS_STATUS vosStatus;
   pMac->btc.btcReady = VOS_FALSE;
   pMac->btc.btcUapsdOk = VOS_FALSE;
   vos_timer_stop(&pMac->btc.restoreHBTimer);
   vosStatus = vos_timer_destroy(&pMac->btc.restoreHBTimer);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcClose: Fail to destroy timer");
       return VOS_STATUS_E_FAILURE;
   }
   if(!HAL_STATUS_SUCCESS(
      pmcDeregisterDeviceStateUpdateInd(pMac, btcPowerStateCB)))
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
         "%s: %d: cannot deregister with pmcDeregisterDeviceStateUpdateInd()",
                __func__, __LINE__);
   }

   return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn btcReady
    \brief  fn to inform BTC that eWNI_SME_SYS_READY_IND has been sent to PE.
            This acts as a trigger to send a message to HAL to update the BTC
            related conig to FW. Note that if HDD configures any power BTC
            related stuff before this API is invoked, BTC will buffer all the
            configutaion.
    \param  hHal - The handle returned by macOpen.
    \return VOS_STATUS
  ---------------------------------------------------------------------------*/
VOS_STATUS btcReady (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    v_U32_t cfgVal = 0;
    v_U8_t i;
    pMac->btc.btcReady = VOS_TRUE;
    pMac->btc.btcUapsdOk = VOS_TRUE;
    for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
    {
        pMac->btc.btcScoHandles[i] = BT_INVALID_CONN_HANDLE;
    }

    // Read heartbeat threshold CFG and save it.
    ccmCfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &cfgVal);
    pMac->btc.btcHBCount = (v_U8_t)cfgVal;
    if (btcSendCfgMsg(hHal, &(pMac->btc.btcConfig)) != VOS_STATUS_SUCCESS)
    {
        return VOS_STATUS_E_FAILURE;
    }
    return VOS_STATUS_SUCCESS;
}

static VOS_STATUS btcSendBTEvent(tpAniSirGlobal pMac, tpSmeBtEvent pBtEvent)
{
   vos_msg_t msg;
   tpSmeBtEvent ptrSmeBtEvent = NULL;
   switch(pBtEvent->btEventType)
   {
      case BT_EVENT_CREATE_SYNC_CONNECTION:
      case BT_EVENT_SYNC_CONNECTION_UPDATED:
         if(pBtEvent->uEventParam.btSyncConnection.linkType != BT_SCO &&
            pBtEvent->uEventParam.btSyncConnection.linkType != BT_eSCO)
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
               "Invalid link type %d for Sync Connection. BT event will be dropped ",
               __func__, pBtEvent->uEventParam.btSyncConnection.linkType);
            return VOS_STATUS_E_FAILURE;
         }
         break;
      case BT_EVENT_SYNC_CONNECTION_COMPLETE:
         if((pBtEvent->uEventParam.btSyncConnection.status == BT_CONN_STATUS_SUCCESS) &&
            ((pBtEvent->uEventParam.btSyncConnection.linkType != BT_SCO && pBtEvent->uEventParam.btSyncConnection.linkType != BT_eSCO) ||
             (pBtEvent->uEventParam.btSyncConnection.connectionHandle == BT_INVALID_CONN_HANDLE)))
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
               "Invalid connection handle %d or link type %d for Sync Connection. BT event will be dropped ",
               __func__,
               pBtEvent->uEventParam.btSyncConnection.connectionHandle,
               pBtEvent->uEventParam.btSyncConnection.linkType);
            return VOS_STATUS_E_FAILURE;
         }
         break;
      case BT_EVENT_MODE_CHANGED:
         if(pBtEvent->uEventParam.btAclModeChange.mode >= BT_ACL_MODE_MAX)
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
               "Invalid mode %d for ACL Connection. BT event will be dropped ",
               __func__,
               pBtEvent->uEventParam.btAclModeChange.mode);
            return VOS_STATUS_E_FAILURE;
         }
         break;
     case BT_EVENT_DEVICE_SWITCHED_OFF:
         pMac->btc.btcEventState = 0;
         break;
      default:
         break;
   }
   ptrSmeBtEvent = vos_mem_malloc(sizeof(tSmeBtEvent));
   if (NULL == ptrSmeBtEvent)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
         "Not able to allocate memory for BT event", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   btcLogEvent(pMac, pBtEvent);
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   btcDiagEventLog(pMac, pBtEvent);
#endif
   vos_mem_copy(ptrSmeBtEvent, pBtEvent, sizeof(tSmeBtEvent));
   msg.type = WDA_SIGNAL_BT_EVENT;
   msg.reserved = 0;
   msg.bodyptr = ptrSmeBtEvent;
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
         "Not able to post WDA_SIGNAL_BT_EVENT message to WDA", __func__);
      vos_mem_free( ptrSmeBtEvent );
      return VOS_STATUS_E_FAILURE;
   }
   // After successfully posting the message, check if heart beat
   // monitoring needs to be turned off
   (void)btcCheckHeartBeatMonitoring(pMac, pBtEvent);
   //Check whether BTC and UAPSD can co-exist
   btcUapsdCheck( pMac, pBtEvent );
   return VOS_STATUS_SUCCESS;
   }

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
/* ---------------------------------------------------------------------------
    \fn btcSignalBTEvent
    \brief  API to signal Bluetooth (BT) event to the WLAN driver. Based on the
            BT event type and the current operating mode of Libra (full power,
            BMPS, UAPSD etc), appropriate Bluetooth Coexistence (BTC) strategy
            would be employed.
    \param  hHal - The handle returned by macOpen.
    \param  pBtEvent -  Pointer to a caller allocated object of type tSmeBtEvent.
                        Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE – BT Event not passed to HAL. This can happen
                                   if driver has not yet been initialized or if BTC
                                   Events Layer has been disabled.
            VOS_STATUS_SUCCESS   – BT Event passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS btcSignalBTEvent (tHalHandle hHal, tpSmeBtEvent pBtEvent)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   VOS_STATUS vosStatus;
   if( NULL == pBtEvent )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
         "Null pointer for SME BT Event", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   if(( BTC_WLAN_ONLY == pMac->btc.btcConfig.btcExecutionMode ) ||
      ( BTC_PTA_ONLY == pMac->btc.btcConfig.btcExecutionMode ))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
         "BTC execution mode not set to BTC_SMART_COEXISTENCE. BT event will be dropped", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   if( pBtEvent->btEventType < 0 || pBtEvent->btEventType >= BT_EVENT_TYPE_MAX )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
         "Invalid BT event %d being passed. BT event will be dropped",
          __func__, pBtEvent->btEventType);
      return VOS_STATUS_E_FAILURE;
   }
   //Check PMC state to make sure whether we need to defer
   //If we already have deferred events, defer the new one as well, in case PMC is in transition state
   if( pMac->btc.fReplayBTEvents || !PMC_IS_CHIP_ACCESSIBLE(pmcGetPmcState( pMac )) )
   {
       //We need to defer the event
       vosStatus = btcDeferEvent(pMac, pBtEvent);
       if( VOS_IS_STATUS_SUCCESS(vosStatus) )
       {
           pMac->btc.fReplayBTEvents = VOS_TRUE;
           return VOS_STATUS_SUCCESS;
       }
       else
       {
           return vosStatus;
       }
   }
    btcSendBTEvent(pMac, pBtEvent);
   return VOS_STATUS_SUCCESS;
}
#endif
/* ---------------------------------------------------------------------------
    \fn btcCheckHeartBeatMonitoring
    \brief  API to check whether heartbeat monitoring is required to be disabled
            for specific BT start events which takes significant time to complete
            during which WLAN misses beacons. To avoid WLAN-MAC from disconnecting
            for the not enough beacons received we stop the heartbeat timer during
            this start BT event till the stop of that BT event.
    \param  hHal - The handle returned by macOpen.
    \param  pBtEvent -  Pointer to a caller allocated object of type tSmeBtEvent.
                        Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  Config not passed to HAL.
            VOS_STATUS_SUCCESS  Config passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS btcCheckHeartBeatMonitoring(tHalHandle hHal, tpSmeBtEvent pBtEvent)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   VOS_STATUS vosStatus;
   switch(pBtEvent->btEventType)
   {
      // Start events which requires heartbeat monitoring be disabled.
      case BT_EVENT_INQUIRY_STARTED:
          pMac->btc.btcEventState |= BT_INQUIRY_STARTED;
          break;
      case BT_EVENT_PAGE_STARTED:
          pMac->btc.btcEventState |= BT_PAGE_STARTED;
          break;
      case BT_EVENT_CREATE_ACL_CONNECTION:
          pMac->btc.btcEventState |= BT_CREATE_ACL_CONNECTION_STARTED;
          break;
      case BT_EVENT_CREATE_SYNC_CONNECTION:
          pMac->btc.btcEventState |= BT_CREATE_SYNC_CONNECTION_STARTED;
          break;
      // Stop/done events which indicates heartbeat monitoring can be enabled
      case BT_EVENT_INQUIRY_STOPPED:
          pMac->btc.btcEventState &= ~(BT_INQUIRY_STARTED);
          break;
      case BT_EVENT_PAGE_STOPPED:
          pMac->btc.btcEventState &= ~(BT_PAGE_STARTED);
          break;
      case BT_EVENT_ACL_CONNECTION_COMPLETE:
          pMac->btc.btcEventState &= ~(BT_CREATE_ACL_CONNECTION_STARTED);
          break;
      case BT_EVENT_SYNC_CONNECTION_COMPLETE:
          pMac->btc.btcEventState &= ~(BT_CREATE_SYNC_CONNECTION_STARTED);
          break;
      default:
          // Ignore other events
          return VOS_STATUS_SUCCESS;
   }
   // Check if any of the BT start events are active
   if (pMac->btc.btcEventState) {
       if (pMac->btc.btcHBActive) {
           // set heartbeat threshold CFG to zero
           ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, 0, NULL, eANI_BOOLEAN_FALSE);
           pMac->btc.btcHBActive = VOS_FALSE;
       }
       // Deactivate and active the restore HB timer
       vos_timer_stop( &pMac->btc.restoreHBTimer);
       vosStatus= vos_timer_start( &pMac->btc.restoreHBTimer, BT_MAX_EVENT_DONE_TIMEOUT );
       if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcCheckHeartBeatMonitoring: Fail to start timer");
           return VOS_STATUS_E_FAILURE;
       }
   } else {
       // Restore CFG back to the original value only if it was disabled
       if (!pMac->btc.btcHBActive) {
           ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, pMac->btc.btcHBCount, NULL, eANI_BOOLEAN_FALSE);
           pMac->btc.btcHBActive = VOS_TRUE;
       }
       // Deactivate the timer
       vosStatus = vos_timer_stop( &pMac->btc.restoreHBTimer);
       if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcCheckHeartBeatMonitoring: Fail to stop timer");
           return VOS_STATUS_E_FAILURE;
       }
   }
   return VOS_STATUS_SUCCESS;
}
/* ---------------------------------------------------------------------------
    \fn btcRestoreHeartBeatMonitoringHandle
    \brief  Timer handler to handlet the timeout condition when a specific BT
            stop event does not come back, in which case to restore back the
            heartbeat timer.
    \param  hHal - The handle returned by macOpen.
    \return VOID
  ---------------------------------------------------------------------------*/
void btcRestoreHeartBeatMonitoringHandle(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    if( !pMac->btc.btcHBActive )
    {
        tPmcState pmcState;
        //Check PMC state to make sure whether we need to defer
        pmcState = pmcGetPmcState( pMac );
        if( PMC_IS_CHIP_ACCESSIBLE(pmcState) )
        {
            // Restore CFG back to the original value
            ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, pMac->btc.btcHBCount, NULL, eANI_BOOLEAN_FALSE);
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "BT event timeout, restoring back HeartBeat timer");
        }
        else
        {
            //defer it
            pMac->btc.btcEventReplay.fRestoreHBMonitor = VOS_TRUE;
        }
    }
}


/* ---------------------------------------------------------------------------
    \fn btcSetConfig
    \brief  API to change the current Bluetooth Coexistence (BTC) configuration
            This function should be invoked only after CFG download has completed.
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtcConfig. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  Config not passed to HAL.
            VOS_STATUS_SUCCESS  Config passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS btcSetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   //Save a copy in the global BTC config
   vos_mem_copy(&(pMac->btc.btcConfig), pSmeBtcConfig, sizeof(tSmeBtcConfig));
   //Send the config down only if SME_HddReady has been invoked. If not ready,
   //BTC config will plumbed down when btcReady is eventually invoked.
   if(pMac->btc.btcReady)
   {
      if(VOS_STATUS_SUCCESS != btcSendCfgMsg(hHal, pSmeBtcConfig))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
            "Failure to send BTC config down");
         return VOS_STATUS_E_FAILURE;
      }
   }
   return VOS_STATUS_SUCCESS;
}
/* ---------------------------------------------------------------------------
    \fn btcPostBtcCfgMsg
    \brief  Private API to post BTC config message to HAL
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtcConfig. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  Config not passed to HAL.
            VOS_STATUS_SUCCESS  Config passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS btcSendCfgMsg(tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig)
{
   tpSmeBtcConfig ptrSmeBtcConfig = NULL;
   vos_msg_t msg;
   if( NULL == pSmeBtcConfig )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcSendCfgMsg: "
         "Null pointer for BTC Config");
      return VOS_STATUS_E_FAILURE;
   }
   if( pSmeBtcConfig->btcExecutionMode >= BT_EXEC_MODE_MAX )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcSendCfgMsg: "
         "Invalid BT execution mode %d being set",
          pSmeBtcConfig->btcExecutionMode);
      return VOS_STATUS_E_FAILURE;
   }
   ptrSmeBtcConfig = vos_mem_malloc(sizeof(tSmeBtcConfig));
   if (NULL == ptrSmeBtcConfig)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcSendCfgMsg: "
         "Not able to allocate memory for SME BTC Config");
      return VOS_STATUS_E_FAILURE;
   }
   vos_mem_copy(ptrSmeBtcConfig, pSmeBtcConfig, sizeof(tSmeBtcConfig));
   msg.type = WDA_BTC_SET_CFG;
   msg.reserved = 0;
   msg.bodyptr = ptrSmeBtcConfig;
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcSendCfgMsg: "
         "Not able to post WDA_BTC_SET_CFG message to WDA");
      vos_mem_free( ptrSmeBtcConfig );
      return VOS_STATUS_E_FAILURE;
   }
   return VOS_STATUS_SUCCESS;
}
/* ---------------------------------------------------------------------------
    \fn btcGetConfig
    \brief  API to retrieve the current Bluetooth Coexistence (BTC) configuration
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtcConfig. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS btcGetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   if( NULL == pSmeBtcConfig )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "btcGetConfig: "
         "Null pointer for BTC Config");
      return VOS_STATUS_E_FAILURE;
   }
   vos_mem_copy(pSmeBtcConfig, &(pMac->btc.btcConfig), sizeof(tSmeBtcConfig));
   return VOS_STATUS_SUCCESS;
}
/*
    btcFindAclEventHist find a suited ACL event buffer
    Param: bdAddr - NULL meaning not care.
                    pointer to caller alocated buffer containing the BD address to find a match
           handle - BT_INVALID_CONN_HANDLE == not care
                    otherwise, a handle to match
    NOPTE: Either bdAddr or handle can be valid, if both of them are valid, use bdAddr only. If neither 
           bdAddr nor handle is valid, return the next free slot.
*/
static tpSmeBtAclEventHist btcFindAclEventHist( tpAniSirGlobal pMac, v_U8_t *bdAddr, v_U16_t handle )
{
    int i, j;
    tpSmeBtAclEventHist pRet = NULL;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    for( i = 0; (i < BT_MAX_ACL_SUPPORT) && (NULL == pRet); i++ )
    {
        if( NULL != bdAddr )
        {
            //try to match addr
            if( pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx )
            {
                for(j = 0; j < pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx; j++)
                {
                    if( vos_mem_compare(pReplay->btcEventHist.btAclConnectionEvent[i].btAclConnection[j].bdAddr,
                        bdAddr, 6) )
                    {
                        //found it
                        pRet = &pReplay->btcEventHist.btAclConnectionEvent[i];
                        break;
                    }
                }
            }
        }
        else if( BT_INVALID_CONN_HANDLE != handle )
        {
            //try to match handle
            if( pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx )
            {
                for(j = 0; j < pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx; j++)
                {
                    if( pReplay->btcEventHist.btAclConnectionEvent[i].btAclConnection[j].connectionHandle ==
                        handle )
                    {
                        //found it
                        pRet = &pReplay->btcEventHist.btAclConnectionEvent[i];
                        break;
                    }
                }
            }
        }
        else if( 0 == pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx )
        {
            pRet = &pReplay->btcEventHist.btAclConnectionEvent[i];
            break;
        }
    }
    return (pRet);
}

/*
    btcFindSyncEventHist find a suited SYNC event buffer
    Param: bdAddr - NULL meaning not care.
                    pointer to caller alocated buffer containing the BD address to find a match
           handle - BT_INVALID_CONN_HANDLE == not care
                    otherwise, a handle to match
    NOPTE: Either bdAddr or handle can be valid, if both of them are valid, use bdAddr only. If neither 
           bdAddr nor handle is valid, return the next free slot.
*/
static tpSmeBtSyncEventHist btcFindSyncEventHist( tpAniSirGlobal pMac, v_U8_t *bdAddr, v_U16_t handle )
{
    int i, j;
    tpSmeBtSyncEventHist pRet = NULL;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    for( i = 0; (i < BT_MAX_SCO_SUPPORT) && (NULL == pRet); i++ )
    {
        if( NULL != bdAddr )
        {
            //try to match addr
            if( pReplay->btcEventHist.btSyncConnectionEvent[i].bNextEventIdx )
            {
                for(j = 0; j < pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx; j++)
                {
                    if( vos_mem_compare(pReplay->btcEventHist.btSyncConnectionEvent[i].btSyncConnection[j].bdAddr,
                        bdAddr, 6) )
                    {
                        //found it
                        pRet = &pReplay->btcEventHist.btSyncConnectionEvent[i];
                        break;
                    }
                }
            }
        }
        else if( BT_INVALID_CONN_HANDLE != handle )
        {
            //try to match handle
            if( pReplay->btcEventHist.btSyncConnectionEvent[i].bNextEventIdx )
            {
                for(j = 0; j < pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx; j++)
                {
                    if( pReplay->btcEventHist.btSyncConnectionEvent[i].btSyncConnection[j].connectionHandle ==
                        handle )
                    {
                        //found it
                        pRet = &pReplay->btcEventHist.btSyncConnectionEvent[i];
                        break;
                    }
                }
            }
        }
        else if( !pReplay->btcEventHist.btSyncConnectionEvent[i].bNextEventIdx )
        {
            pRet = &pReplay->btcEventHist.btSyncConnectionEvent[i];
            break;
        }
    }
    return (pRet);
}

/*
    btcFindDisconnEventHist find a slot for the deferred disconnect event
    If handle is invlid, it returns a free slot, if any. 
    If handle is valid, it tries to find a match first in case same disconnect event comes down again.
*/
static tpSmeBtDisconnectEventHist btcFindDisconnEventHist( tpAniSirGlobal pMac, v_U16_t handle )
{
    tpSmeBtDisconnectEventHist pRet = NULL;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    int i;
    if( BT_INVALID_CONN_HANDLE != handle )
    {
        for(i = 0; i < BT_MAX_DISCONN_SUPPORT; i++)
        {
            if( pReplay->btcEventHist.btDisconnectEvent[i].fValid &&
                (handle == pReplay->btcEventHist.btDisconnectEvent[i].btDisconnect.connectionHandle) )
            {
                pRet = &pReplay->btcEventHist.btDisconnectEvent[i];
                break;
            }
        }
    }
    if( NULL == pRet )
    {
        //Find a free slot
        for(i = 0; i < BT_MAX_DISCONN_SUPPORT; i++)
        {
            if( !pReplay->btcEventHist.btDisconnectEvent[i].fValid )
            {
                pRet = &pReplay->btcEventHist.btDisconnectEvent[i];
                break;
            }
        }
    }
    return (pRet);
}

/*
    btcFindModeChangeEventHist find a slot for the deferred mopde change event
    If handle is invalid, it returns a free slot, if any. 
    If handle is valid, it tries to find a match first in case same disconnect event comes down again.
*/
tpSmeBtAclModeChangeEventHist btcFindModeChangeEventHist( tpAniSirGlobal pMac, v_U16_t handle )
{
    tpSmeBtAclModeChangeEventHist pRet = NULL;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    int i;
    if( BT_INVALID_CONN_HANDLE != handle )
    {
        for(i = 0; i < BT_MAX_ACL_SUPPORT; i++)
        {
            if( pReplay->btcEventHist.btAclModeChangeEvent[i].fValid &&
                (handle == pReplay->btcEventHist.btAclModeChangeEvent[i].btAclModeChange.connectionHandle) )
            {
                pRet = &pReplay->btcEventHist.btAclModeChangeEvent[i];
                break;
            }
        }
    }
    if( NULL == pRet )
    {
        //Find a free slot
        for(i = 0; i < BT_MAX_ACL_SUPPORT; i++)
        {
            if( !pReplay->btcEventHist.btAclModeChangeEvent[i].fValid )
            {
                pRet = &pReplay->btcEventHist.btAclModeChangeEvent[i];
                break;
            }
        }
    }
    return (pRet);
}

/*
    btcFindSyncUpdateEventHist find a slot for the deferred SYNC_UPDATE event
    If handle is invalid, it returns a free slot, if any. 
    If handle is valid, it tries to find a match first in case same disconnect event comes down again.
*/
tpSmeBtSyncUpdateHist btcFindSyncUpdateEventHist( tpAniSirGlobal pMac, v_U16_t handle )
{
    tpSmeBtSyncUpdateHist pRet = NULL;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    int i;
    if( BT_INVALID_CONN_HANDLE != handle )
    {
        for(i = 0; i < BT_MAX_SCO_SUPPORT; i++)
        {
            if( pReplay->btcEventHist.btSyncUpdateEvent[i].fValid &&
                (handle == pReplay->btcEventHist.btSyncUpdateEvent[i].btSyncConnection.connectionHandle) )
            {
                pRet = &pReplay->btcEventHist.btSyncUpdateEvent[i];
                break;
            }
        }
    }
    if( NULL == pRet )
    {
        //Find a free slot
        for(i = 0; i < BT_MAX_SCO_SUPPORT; i++)
        {
            if( !pReplay->btcEventHist.btSyncUpdateEvent[i].fValid )
            {
                pRet = &pReplay->btcEventHist.btSyncUpdateEvent[i];
                break;
            }
        }
    }
    return (pRet);
}

/*
    Call must validate pAclEventHist
*/
static void btcReleaseAclEventHist( tpAniSirGlobal pMac, tpSmeBtAclEventHist pAclEventHist )
{
    vos_mem_zero( pAclEventHist, sizeof(tSmeBtAclEventHist) );
}

/*
    Call must validate pSyncEventHist
*/
static void btcReleaseSyncEventHist( tpAniSirGlobal pMac, tpSmeBtSyncEventHist pSyncEventHist )
{
    vos_mem_zero( pSyncEventHist, sizeof(tSmeBtSyncEventHist) );
}

/*To defer a ACL creation event
    We only support one ACL per BD address.
    If the last cached event another ACL create event, replace that event with the new event
    If a completion event with success status code, and the new ACL creation 
    on same address, defer a new disconnect event(fake one), then cache this ACL creation event.
    Otherwise, save this create event.
*/
static VOS_STATUS btcDeferAclCreate( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtAclEventHist pAclEventHist;
    tSmeBtAclConnectionParam *pAclEvent = NULL;
    do
    {
        //Find a match
        pAclEventHist = btcFindAclEventHist( pMac, pEvent->uEventParam.btAclConnection.bdAddr, 
                                    BT_INVALID_CONN_HANDLE );
        if( NULL == pAclEventHist )
        {
            //No cached ACL event on this address
            //Find a free slot and save it
            pAclEventHist = btcFindAclEventHist( pMac, NULL, BT_INVALID_CONN_HANDLE );
            if( NULL != pAclEventHist )
            {
                vos_mem_copy(&pAclEventHist->btAclConnection[0], &pEvent->uEventParam.btAclConnection, 
                                sizeof(tSmeBtAclConnectionParam));
                pAclEventHist->btEventType[0] = BT_EVENT_CREATE_ACL_CONNECTION;
                pAclEventHist->bNextEventIdx = 1;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" failed to find ACL event slot"));
                status = VOS_STATUS_E_RESOURCES;
            }
            //done
            break;
        }
        else
        {
            //There is history on this BD address
            if ((pAclEventHist->bNextEventIdx <= 0) ||
                (pAclEventHist->bNextEventIdx > BT_MAX_NUM_EVENT_ACL_DEFERRED))
            {
                VOS_ASSERT(0);
                status = VOS_STATUS_E_FAILURE;
                break;
            }
            pAclEvent = &pAclEventHist->btAclConnection[pAclEventHist->bNextEventIdx - 1];
            if(BT_EVENT_CREATE_ACL_CONNECTION == pAclEventHist->btEventType[pAclEventHist->bNextEventIdx - 1])
            {
                //The last cached event is creation, replace it with the new one
                if (pAclEvent)
                {
                    vos_mem_copy(pAclEvent,
                                 &pEvent->uEventParam.btAclConnection,
                                 sizeof(tSmeBtAclConnectionParam));
                }
                //done
                break;
            }
            else if(BT_EVENT_ACL_CONNECTION_COMPLETE == 
                        pAclEventHist->btEventType[pAclEventHist->bNextEventIdx - 1])
            {
                //The last cached event is completion, check the status.
                if(BT_CONN_STATUS_SUCCESS == pAclEvent->status)
                {
                    tSmeBtEvent btEvent;
                    //The last event we have is success completion event. 
                    //Should not get a creation event before creation.
                    smsLog(pMac, LOGE, FL("  Missing disconnect event on handle %d"), pAclEvent->connectionHandle);
                    //Fake a disconnect event
                    btEvent.btEventType = BT_EVENT_DISCONNECTION_COMPLETE;
                    btEvent.uEventParam.btDisconnect.connectionHandle = pAclEvent->connectionHandle;
                    btcDeferDisconnEvent(pMac, &btEvent);
                }
            }
            //Need to save the new event
            if(pAclEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_ACL_DEFERRED)
            {
                pAclEventHist->btEventType[pAclEventHist->bNextEventIdx] = BT_EVENT_CREATE_ACL_CONNECTION;
                vos_mem_copy(&pAclEventHist->btAclConnection[pAclEventHist->bNextEventIdx], 
                                &pEvent->uEventParam.btAclConnection, 
                                sizeof(tSmeBtAclConnectionParam));
                pAclEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" ACL event overflow"));
                VOS_ASSERT(0);
            }
        }
    }while(0);
    return status;
}

/*Defer a ACL completion event
  If there is cached event on this BD address, check completion status.
  If status is fail and last cached event is creation, remove the creation event and drop
  this completion event. Otherwise, cache this completion event as the latest one.
*/
static VOS_STATUS btcDeferAclComplete( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtAclEventHist pAclEventHist;
    do
    {
        //Find a match
        pAclEventHist = btcFindAclEventHist( pMac, pEvent->uEventParam.btAclConnection.bdAddr, 
                                    BT_INVALID_CONN_HANDLE );
        if(pAclEventHist)
        {
            VOS_ASSERT(pAclEventHist->bNextEventIdx >0);
            //Found one
            if(BT_CONN_STATUS_SUCCESS != pEvent->uEventParam.btAclConnection.status)
            {
                //If completion fails, and the last one is creation, remove the creation event
                if(BT_EVENT_CREATE_ACL_CONNECTION == pAclEventHist->btEventType[pAclEventHist->bNextEventIdx-1])
                {
                    vos_mem_zero(&pAclEventHist->btAclConnection[pAclEventHist->bNextEventIdx-1], 
                                    sizeof(tSmeBtAclConnectionParam));
                    pAclEventHist->bNextEventIdx--;
                    //Done with this event
                    break;
                }
                else
                {
                    smsLog(pMac, LOGE, FL(" ACL completion fail but last event(%d) not creation"),
                        pAclEventHist->btEventType[pAclEventHist->bNextEventIdx-1]);
                }
            }
        }
        if( NULL == pAclEventHist )
        {
            pAclEventHist = btcFindAclEventHist( pMac, NULL, BT_INVALID_CONN_HANDLE );
        }
        if(pAclEventHist)
        {
            if(pAclEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_ACL_DEFERRED)
            {
                //Save this event
                pAclEventHist->btEventType[pAclEventHist->bNextEventIdx] = BT_EVENT_ACL_CONNECTION_COMPLETE;
                vos_mem_copy(&pAclEventHist->btAclConnection[pAclEventHist->bNextEventIdx], 
                                &pEvent->uEventParam.btAclConnection, 
                                sizeof(tSmeBtAclConnectionParam));
                pAclEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" ACL event overflow"));
                VOS_ASSERT(0);
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL(" cannot find match for failed "
                   "BT_EVENT_ACL_CONNECTION_COMPLETE of bdAddr "
                   MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pEvent->uEventParam.btAclConnection.bdAddr));
            status = VOS_STATUS_E_EMPTY;
        }
    }while(0);
    return (status);
}

/*To defer a SYNC creation event
    If the last cached event is another SYNC create event, replace 
    that event with the new event.
    If there is a completion event with success status code, cache a new 
    disconnect event(fake) first, then cache this SYNC creation event.
    Otherwise, cache this create event.
*/
static VOS_STATUS btcDeferSyncCreate( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtSyncEventHist pSyncEventHist;
    tSmeBtSyncConnectionParam *pSyncEvent = NULL;
    do
    {
        //Find a match
        pSyncEventHist = btcFindSyncEventHist( pMac, pEvent->uEventParam.btSyncConnection.bdAddr, 
                                    BT_INVALID_CONN_HANDLE );
        if( NULL == pSyncEventHist )
        {
            //No cached ACL event on this address
            //Find a free slot and save it
            pSyncEventHist = btcFindSyncEventHist( pMac, NULL, BT_INVALID_CONN_HANDLE );
            if( NULL != pSyncEventHist )
            {
                vos_mem_copy(&pSyncEventHist->btSyncConnection[0], &pEvent->uEventParam.btSyncConnection, 
                                sizeof(tSmeBtSyncConnectionParam));
                pSyncEventHist->btEventType[0] = BT_EVENT_CREATE_SYNC_CONNECTION;
                pSyncEventHist->bNextEventIdx = 1;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" failed to find SYNC event slot"));
                status = VOS_STATUS_E_RESOURCES;
            }
            //done
            break;
        }
        else
        {
            //There is history on this BD address
            if ((pSyncEventHist->bNextEventIdx <= 0) ||
                (pSyncEventHist->bNextEventIdx > BT_MAX_NUM_EVENT_SCO_DEFERRED))
            {
                VOS_ASSERT(0);
                status = VOS_STATUS_E_FAILURE;
                return status;
            }
            pSyncEvent = &pSyncEventHist->btSyncConnection[pSyncEventHist->bNextEventIdx - 1];
            if(BT_EVENT_CREATE_SYNC_CONNECTION == 
                pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx - 1])
            {
                //The last cached event is creation, replace it with the new one
                if(pSyncEvent)
                {
                    vos_mem_copy(pSyncEvent,
                                 &pEvent->uEventParam.btSyncConnection,
                                 sizeof(tSmeBtSyncConnectionParam));
                }
                //done
                break;
            }
            else if(BT_EVENT_SYNC_CONNECTION_COMPLETE == 
                        pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx - 1])
            {
                //The last cached event is completion, check the status.
                if(BT_CONN_STATUS_SUCCESS == pSyncEvent->status)
                {
                    tSmeBtEvent btEvent;
                    //The last event we have is success completion event. 
                    //Should not get a creation event before creation.
                    smsLog(pMac, LOGE, FL("  Missing disconnect event on handle %d"), pSyncEvent->connectionHandle);
                    //Fake a disconnect event
                    btEvent.btEventType = BT_EVENT_DISCONNECTION_COMPLETE;
                    btEvent.uEventParam.btDisconnect.connectionHandle = pSyncEvent->connectionHandle;
                    btcDeferDisconnEvent(pMac, &btEvent);
                }
            }
            //Need to save the new event
            if(pSyncEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_SCO_DEFERRED)
            {
                pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx] = BT_EVENT_CREATE_SYNC_CONNECTION;
                vos_mem_copy(&pSyncEventHist->btSyncConnection[pSyncEventHist->bNextEventIdx], 
                                &pEvent->uEventParam.btSyncConnection, 
                                sizeof(tSmeBtSyncConnectionParam));
                pSyncEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" SYNC event overflow"));
            }
        }
    }while(0);
    return status;
}

/*Defer a SYNC completion event
  If there is cached event on this BD address, check completion status.
  If status is fail and last cached event is creation, remove te creation event and drop
  this completion event. 
  Otherwise, cache this completion event as the latest one.
*/
static VOS_STATUS btcDeferSyncComplete( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtSyncEventHist pSyncEventHist;
    do
    {
        //Find a match
        pSyncEventHist = btcFindSyncEventHist( pMac, pEvent->uEventParam.btSyncConnection.bdAddr, 
                                    BT_INVALID_CONN_HANDLE );
        if(pSyncEventHist)
        {
            VOS_ASSERT(pSyncEventHist->bNextEventIdx >0);
            //Found one
            if(BT_CONN_STATUS_SUCCESS != pEvent->uEventParam.btSyncConnection.status)
            {
                //If completion fails, and the last one is creation, remove the creation event
                if(BT_EVENT_CREATE_SYNC_CONNECTION == pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx-1])
                {
                    vos_mem_zero(&pSyncEventHist->btSyncConnection[pSyncEventHist->bNextEventIdx-1], 
                                    sizeof(tSmeBtSyncConnectionParam));
                    pSyncEventHist->bNextEventIdx--;
                    //Done with this event
                    break;
                }
                else
                {
                    smsLog(pMac, LOGE, FL(" SYNC completion fail but last event(%d) not creation"),
                        pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx-1]);
                }
            }
        }
        if(NULL == pSyncEventHist)
        {
            //In case we don't defer the creation event
            pSyncEventHist = btcFindSyncEventHist( pMac, NULL, BT_INVALID_CONN_HANDLE );
        }
        if(pSyncEventHist)
        {
            if(pSyncEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_ACL_DEFERRED)
            {
                //Save this event
                pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx] = BT_EVENT_SYNC_CONNECTION_COMPLETE;
                vos_mem_copy(&pSyncEventHist->btSyncConnection[pSyncEventHist->bNextEventIdx], 
                                &pEvent->uEventParam.btSyncConnection, 
                                sizeof(tSmeBtSyncConnectionParam));
                pSyncEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" SYNC event overflow"));
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL(" cannot find match for "
                   "BT_EVENT_SYNC_CONNECTION_COMPLETE of bdAddr "
                   MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pEvent->uEventParam.btSyncConnection.bdAddr));
            status = VOS_STATUS_E_EMPTY;
        }
    }while(0);
    return (status);
}

//return VOS_STATUS_E_EXISTS if the event handle cannot be found
//VOS_STATUS_SUCCESS if the event is processed
//Other error status meaning it cannot continue due to other errors
/*
  Defer a disconnect event for ACL
  Check if any history on this event handle.
  If both ACL_CREATION and ACL_COMPLETION is cached, remove both those events and drop
  this disconnect event.
  Otherwise save disconnect event in this ACL's bin.
  If not ACL match on this handle, not to do anything.
  Either way, remove any cached MODE_CHANGE event matches this disconnect event's handle.
*/
static VOS_STATUS btcDeferDisconnectEventForACL( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtAclEventHist pAclEventHist;
    tpSmeBtAclModeChangeEventHist pModeChangeEventHist;
    v_BOOL_t fDone = VOS_FALSE;
    int i;
    pAclEventHist = btcFindAclEventHist( pMac, NULL, 
                                pEvent->uEventParam.btDisconnect.connectionHandle );
    if(pAclEventHist)
    {
        if( pAclEventHist->bNextEventIdx > BT_MAX_NUM_EVENT_ACL_DEFERRED)
        {
            smsLog(pMac, LOGE, FL(" ACL event history index:%d overflow, resetting to BT_MAX_NUM_EVENT_ACL_DEFERRED"), pAclEventHist->bNextEventIdx);
            pAclEventHist->bNextEventIdx = BT_MAX_NUM_EVENT_ACL_DEFERRED;
        }
        //Looking backwords
        for(i = pAclEventHist->bNextEventIdx - 1; i >= 0; i--)
        {
            if( BT_EVENT_ACL_CONNECTION_COMPLETE == pAclEventHist->btEventType[i] )
            {
                //make sure we can cancel the link
                if( (i > 0) && (BT_EVENT_CREATE_ACL_CONNECTION == pAclEventHist->btEventType[i - 1]) )
                {
                    fDone = VOS_TRUE;
                    if(i == 1)
                    {
                        //All events can be wiped off
                        btcReleaseAclEventHist(pMac, pAclEventHist);
                        break;
                    }
                    //we have both ACL creation and completion, wipe out all of them
                    pAclEventHist->bNextEventIdx = (tANI_U8)(i - 1);
                    vos_mem_zero(&pAclEventHist->btAclConnection[i-1], sizeof(tSmeBtAclConnectionParam));
                    vos_mem_zero(&pAclEventHist->btAclConnection[i], sizeof(tSmeBtAclConnectionParam));
                    break;
                }
            }
        }//for loop
        if(!fDone)
        {
            //Save this disconnect event
            if(pAclEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_ACL_DEFERRED)
            {
                pAclEventHist->btEventType[pAclEventHist->bNextEventIdx] = 
                    BT_EVENT_DISCONNECTION_COMPLETE;
                pAclEventHist->btAclConnection[pAclEventHist->bNextEventIdx].connectionHandle =
                    pEvent->uEventParam.btDisconnect.connectionHandle;
                pAclEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" ACL event overflow"));
                status = VOS_STATUS_E_FAILURE;
            }
        }
    }
    else
    {
        status = VOS_STATUS_E_EXISTS;
    }
    //Wipe out the related mode change event if it is there
    pModeChangeEventHist = btcFindModeChangeEventHist( pMac,  
                            pEvent->uEventParam.btDisconnect.connectionHandle );
    if( pModeChangeEventHist && pModeChangeEventHist->fValid )
    {
        pModeChangeEventHist->fValid = VOS_FALSE;
    }
    return status;
}

//This function works the same as btcDeferDisconnectEventForACL except it hanldes SYNC events
//return VOS_STATUS_E_EXISTS if the event handle cannot be found
//VOS_STATUS_SUCCESS if the event is processed
//Other error status meaning it cannot continue due to other errors
/*
  Defer a disconnect event for SYNC
  Check if any SYNC history on this event handle.
  If yes and if both SYNC_CREATION and SYNC_COMPLETION is cached, remove both those events and drop
  this disconnect event.
  Otherwise save disconnect event in this SYNC's bin.
  If no match found, not to save this event here.
  Either way, remove any cached SYNC_UPDATE event matches this disconnect event's handle.
*/
static VOS_STATUS btcDeferDisconnectEventForSync( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtSyncEventHist pSyncEventHist;
    tpSmeBtSyncUpdateHist pSyncUpdateHist;
    v_BOOL_t fDone = VOS_FALSE;
    int i;
    pSyncEventHist = btcFindSyncEventHist( pMac, NULL, 
                                pEvent->uEventParam.btDisconnect.connectionHandle );
    if(pSyncEventHist)
    {
        if( pSyncEventHist->bNextEventIdx > BT_MAX_NUM_EVENT_SCO_DEFERRED)
        {
            smsLog(pMac, LOGE, FL(" SYNC event history index:%d overflow, resetting to BT_MAX_NUM_EVENT_SCO_DEFERRED"), pSyncEventHist->bNextEventIdx);
            pSyncEventHist->bNextEventIdx = BT_MAX_NUM_EVENT_SCO_DEFERRED;
        }
        //Looking backwords
        for(i = pSyncEventHist->bNextEventIdx - 1; i >= 0; i--)
        {
            //if a mode change event exists, drop it
            if( BT_EVENT_SYNC_CONNECTION_COMPLETE == pSyncEventHist->btEventType[i] )
            {
                //make sure we can cancel the link
                if( (i > 0) && (BT_EVENT_CREATE_SYNC_CONNECTION == pSyncEventHist->btEventType[i - 1]) )
                {
                    fDone = VOS_TRUE;
                    if(i == 1)
                    {
                        //All events can be wiped off
                        btcReleaseSyncEventHist(pMac, pSyncEventHist);
                        break;
                    }
                    //we have both ACL creation and completion, wipe out all of them
                    pSyncEventHist->bNextEventIdx = (tANI_U8)(i - 1);
                    vos_mem_zero(&pSyncEventHist->btSyncConnection[i-1], sizeof(tSmeBtSyncConnectionParam));
                    vos_mem_zero(&pSyncEventHist->btSyncConnection[i], sizeof(tSmeBtSyncConnectionParam));
                    break;
                }
            }
        }//for loop
        if(!fDone)
        {
            //Save this disconnect event
            if(pSyncEventHist->bNextEventIdx < BT_MAX_NUM_EVENT_SCO_DEFERRED)
            {
                pSyncEventHist->btEventType[pSyncEventHist->bNextEventIdx] = 
                    BT_EVENT_DISCONNECTION_COMPLETE;
                pSyncEventHist->btSyncConnection[pSyncEventHist->bNextEventIdx].connectionHandle =
                    pEvent->uEventParam.btDisconnect.connectionHandle;
                pSyncEventHist->bNextEventIdx++;
            }
            else
            {
                smsLog(pMac, LOGE, FL(" SYNC event overflow"));
                status = VOS_STATUS_E_FAILURE;
            }
        }
    }
    else
    {
        status = VOS_STATUS_E_EXISTS;
    }
    //Wipe out the related mode change event if it is there
    pSyncUpdateHist = btcFindSyncUpdateEventHist( pMac,  
                            pEvent->uEventParam.btDisconnect.connectionHandle );
    if( pSyncUpdateHist && pSyncUpdateHist->fValid )
    {
        pSyncUpdateHist->fValid = VOS_FALSE;
    }
    return status;
}

/*
  Defer a disconnect event.
  Try to defer it as part of the ACL event first. 
  If no match is found, try SYNC. 
  If still no match found, defer it at DISCONNECT event bin.
*/
static VOS_STATUS btcDeferDisconnEvent( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtDisconnectEventHist pDisconnEventHist;
    if( BT_INVALID_CONN_HANDLE == pEvent->uEventParam.btDisconnect.connectionHandle )
    {
        smsLog( pMac, LOGE, FL(" invalid handle") );
        return (VOS_STATUS_E_INVAL);
    }
    //Check ACL first
    status = btcDeferDisconnectEventForACL(pMac, pEvent);
    if(!VOS_IS_STATUS_SUCCESS(status))
    {
        status = btcDeferDisconnectEventForSync(pMac, pEvent);
    }
    if( !VOS_IS_STATUS_SUCCESS(status) )
    {
        //Save the disconnect event
        pDisconnEventHist = btcFindDisconnEventHist( pMac, 
            pEvent->uEventParam.btDisconnect.connectionHandle );
        if( pDisconnEventHist )
        {
            pDisconnEventHist->fValid = VOS_TRUE;
            vos_mem_copy( &pDisconnEventHist->btDisconnect, &pEvent->uEventParam.btDisconnect,
                sizeof(tSmeBtDisconnectParam) );
            status = VOS_STATUS_SUCCESS;
        }
        else
        {
            smsLog( pMac, LOGE, FL(" cannot find match for BT_EVENT_DISCONNECTION_COMPLETE of handle (%d)"),
                pEvent->uEventParam.btDisconnect.connectionHandle);
            status = VOS_STATUS_E_EMPTY;
        }
    }
    return (status);
}

/*
    btcDeferEvent save the event for possible replay when chip can be accessed
    This function is called only when in IMPS/Standby state
*/
static VOS_STATUS btcDeferEvent( tpAniSirGlobal pMac, tpSmeBtEvent pEvent )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpSmeBtSyncUpdateHist pSyncUpdateHist;
    tpSmeBtAclModeChangeEventHist pModeChangeEventHist;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    switch(pEvent->btEventType)
    {
    case BT_EVENT_DEVICE_SWITCHED_ON:
        //Clear all events first
        vos_mem_zero( &pReplay->btcEventHist, sizeof(tSmeBtcEventHist) );
        pReplay->fBTSwitchOn = VOS_TRUE;
        pReplay->fBTSwitchOff = VOS_FALSE;
        break;
    case BT_EVENT_DEVICE_SWITCHED_OFF:
        //Clear all events first
        vos_mem_zero( &pReplay->btcEventHist, sizeof(tSmeBtcEventHist) );
        pReplay->fBTSwitchOff = VOS_TRUE;
        pReplay->fBTSwitchOn = VOS_FALSE;
        break;
    case BT_EVENT_INQUIRY_STARTED:
        pReplay->btcEventHist.nInquiryEvent++;
        break;
    case BT_EVENT_INQUIRY_STOPPED:
        pReplay->btcEventHist.nInquiryEvent--;
        break;
    case BT_EVENT_PAGE_STARTED:
        pReplay->btcEventHist.nPageEvent++;
        break;
    case BT_EVENT_PAGE_STOPPED:
        pReplay->btcEventHist.nPageEvent--;
        break;
    case BT_EVENT_CREATE_ACL_CONNECTION:
        status = btcDeferAclCreate(pMac, pEvent);
        break;
    case BT_EVENT_ACL_CONNECTION_COMPLETE:
        status = btcDeferAclComplete( pMac, pEvent );
        break;
    case BT_EVENT_CREATE_SYNC_CONNECTION:
        status = btcDeferSyncCreate(pMac, pEvent);
        break;
    case BT_EVENT_SYNC_CONNECTION_COMPLETE:
        status = btcDeferSyncComplete( pMac, pEvent );
        break;
    case BT_EVENT_SYNC_CONNECTION_UPDATED:
        if( BT_INVALID_CONN_HANDLE == pEvent->uEventParam.btDisconnect.connectionHandle )
        {
            smsLog( pMac, LOGE, FL(" invalid handle") );
            status = VOS_STATUS_E_INVAL;
            break;
        }
        //Find a match on handle. If not found, get a free slot.
        pSyncUpdateHist = btcFindSyncUpdateEventHist( pMac,  
                                    pEvent->uEventParam.btSyncConnection.connectionHandle );
        if(pSyncUpdateHist)
        {
            pSyncUpdateHist->fValid = VOS_TRUE;
            vos_mem_copy(&pSyncUpdateHist->btSyncConnection, &pEvent->uEventParam.btSyncConnection, 
                            sizeof(tSmeBtSyncConnectionParam));
        }
        else
        {
            smsLog( pMac, LOGE, FL(" cannot find match for BT_EVENT_SYNC_CONNECTION_UPDATED of handle (%d)"),
                pEvent->uEventParam.btSyncConnection.connectionHandle );
            status = VOS_STATUS_E_EMPTY;
        }
        break;
    case BT_EVENT_DISCONNECTION_COMPLETE:
        status = btcDeferDisconnEvent( pMac, pEvent );
        break;
    case BT_EVENT_MODE_CHANGED:
        if( BT_INVALID_CONN_HANDLE == pEvent->uEventParam.btDisconnect.connectionHandle )
        {
            smsLog( pMac, LOGE, FL(" invalid handle") );
            status = VOS_STATUS_E_INVAL;
            break;
        }
        //Find a match on handle, If not found, return a free slot
        pModeChangeEventHist = btcFindModeChangeEventHist( pMac,  
                                    pEvent->uEventParam.btAclModeChange.connectionHandle );
        if(pModeChangeEventHist)
        {
            pModeChangeEventHist->fValid = VOS_TRUE;
            vos_mem_copy( &pModeChangeEventHist->btAclModeChange,
                            &pEvent->uEventParam.btAclModeChange, sizeof(tSmeBtAclModeChangeParam) );
        }
        else
        {
            smsLog( pMac, LOGE, FL(" cannot find match for BT_EVENT_MODE_CHANGED of handle (%d)"),
                pEvent->uEventParam.btAclModeChange.connectionHandle);
            status = VOS_STATUS_E_EMPTY;
        }
        break;
    case BT_EVENT_A2DP_STREAM_START:
        pReplay->btcEventHist.fA2DPStarted = VOS_TRUE;
        pReplay->btcEventHist.fA2DPStopped = VOS_FALSE;
        break;
    case BT_EVENT_A2DP_STREAM_STOP:
        pReplay->btcEventHist.fA2DPStopped = VOS_TRUE;
        pReplay->btcEventHist.fA2DPStarted = VOS_FALSE;
        break;
    default:
        smsLog( pMac, LOGE, FL(" event (%d) is not deferred"), pEvent->btEventType );
        status = VOS_STATUS_E_NOSUPPORT;
        break;
    }
    return (status);
}

/*
    Replay all cached events in the following order
    1. If BT_SWITCH_OFF event, send it.
    2. Send INQUIRY event (START or STOP),if available
    3. Send PAGE event (START or STOP), if available
    4. Send DISCONNECT events, these DISCONNECT events are not tied to 
        any ACL/SYNC event that we have cached
    5. Send ACL events (possible events, CREATION, COMPLETION, DISCONNECT)
    6. Send MODE_CHANGE events, if available
    7. Send A2DP event(START or STOP), if available
    8. Send SYNC events (possible events, CREATION, COMPLETION, DISCONNECT)
    9. Send SYNC_UPDATE events, if available
*/
static void btcReplayEvents( tpAniSirGlobal pMac )
{
    int i, j;
    tSmeBtEvent btEvent;
    tpSmeBtAclEventHist pAclHist;
    tpSmeBtSyncEventHist pSyncHist;
    tSmeBtcEventReplay *pReplay = &pMac->btc.btcEventReplay;
    //Always turn on HB monitor first. 
    //It is independent of BT events even though BT event causes this
    if( pReplay->fRestoreHBMonitor )
    {
        pReplay->fRestoreHBMonitor = VOS_FALSE;
        //Only do it when needed
        if( !pMac->btc.btcHBActive ) 
        {
            ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, pMac->btc.btcHBCount, NULL, eANI_BOOLEAN_FALSE);
            pMac->btc.btcHBActive = VOS_TRUE;
        }
    }
    if( pMac->btc.fReplayBTEvents )
    {
        /*Set the flag to false here so btcSignalBTEvent won't defer any further.
          This works because SME has it global lock*/
        pMac->btc.fReplayBTEvents = VOS_FALSE;
        if( pReplay->fBTSwitchOff )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_DEVICE_SWITCHED_OFF;
            btcSendBTEvent( pMac, &btEvent );
            pReplay->fBTSwitchOff = VOS_FALSE;
        }
        else if( pReplay->fBTSwitchOn )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_DEVICE_SWITCHED_ON;
            btcSendBTEvent( pMac, &btEvent );
            pReplay->fBTSwitchOn = VOS_FALSE;
        }
        //Do inquire first
        if( pReplay->btcEventHist.nInquiryEvent > 0 )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_INQUIRY_STARTED;
            i = pReplay->btcEventHist.nInquiryEvent;
            while(i--)
            {
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        else if( pReplay->btcEventHist.nInquiryEvent < 0 )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_INQUIRY_STOPPED;
            i = pReplay->btcEventHist.nInquiryEvent;
            while(i++)
            {
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        //Page
        if( pReplay->btcEventHist.nPageEvent > 0 )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_PAGE_STARTED;
            i = pReplay->btcEventHist.nPageEvent;
            while(i--)
            {
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        else if( pReplay->btcEventHist.nPageEvent < 0 )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_PAGE_STOPPED;
            i = pReplay->btcEventHist.nPageEvent;
            while(i++)
            {
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        //Replay non-completion disconnect events first
        //Disconnect
        for( i = 0; i < BT_MAX_DISCONN_SUPPORT; i++ )
        {
            if( pReplay->btcEventHist.btDisconnectEvent[i].fValid )
            {
                vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                btEvent.btEventType = BT_EVENT_DISCONNECTION_COMPLETE;
                vos_mem_copy( &btEvent.uEventParam.btDisconnect, 
                    &pReplay->btcEventHist.btDisconnectEvent[i].btDisconnect, sizeof(tSmeBtDisconnectParam) );
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        //ACL
        for( i = 0; i < BT_MAX_ACL_SUPPORT; i++ )
        {
            if( pReplay->btcEventHist.btAclConnectionEvent[i].bNextEventIdx )
            {
                pAclHist = &pReplay->btcEventHist.btAclConnectionEvent[i];
                //Replay all ACL events for this BD address/handle
                for(j = 0; j < pAclHist->bNextEventIdx; j++)
                {
                    vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                    vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                    btEvent.btEventType = pAclHist->btEventType[j];
                    if(BT_EVENT_DISCONNECTION_COMPLETE != btEvent.btEventType)
                    {
                        //It must be CREATE or CONNECTION_COMPLETE
                       vos_mem_copy( &btEvent.uEventParam.btAclConnection, 
                                     &pAclHist->btAclConnection[j], sizeof(tSmeBtAclConnectionParam) );
                    }
                    else
                    {
                       btEvent.uEventParam.btDisconnect.connectionHandle = pAclHist->btAclConnection[j].connectionHandle;
                    }
                    btcSendBTEvent( pMac, &btEvent );
                }
            }
        }
        //Mode change
        for( i = 0; i < BT_MAX_ACL_SUPPORT; i++ )
        {
            if( pReplay->btcEventHist.btAclModeChangeEvent[i].fValid )
            {
                vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                btEvent.btEventType = BT_EVENT_MODE_CHANGED;
                vos_mem_copy( &btEvent.uEventParam.btAclModeChange, 
                    &pReplay->btcEventHist.btAclModeChangeEvent[i].btAclModeChange, sizeof(tSmeBtAclModeChangeParam) );
                btcSendBTEvent( pMac, &btEvent );
            }
        }
       //A2DP
        if( pReplay->btcEventHist.fA2DPStarted )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_A2DP_STREAM_START;
            btcSendBTEvent( pMac, &btEvent );
        }
        else if( pReplay->btcEventHist.fA2DPStopped )
        {
            vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
            btEvent.btEventType = BT_EVENT_A2DP_STREAM_STOP;
            btcSendBTEvent( pMac, &btEvent );
        }
        //SCO
        for( i = 0; i < BT_MAX_SCO_SUPPORT; i++ )
        {
            if( pReplay->btcEventHist.btSyncConnectionEvent[i].bNextEventIdx )
            {
                pSyncHist = &pReplay->btcEventHist.btSyncConnectionEvent[i];
                //Replay all SYNC events for this BD address/handle
                for(j = 0; j < pSyncHist->bNextEventIdx; j++)
                {
                    vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                    vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                    btEvent.btEventType = pSyncHist->btEventType[j];
                    if(BT_EVENT_DISCONNECTION_COMPLETE != btEvent.btEventType)
                    {
                        //Must be CREATION or CONNECTION_COMPLETE
                       vos_mem_copy( &btEvent.uEventParam.btSyncConnection, 
                                     &pSyncHist->btSyncConnection[j], sizeof(tSmeBtSyncConnectionParam) );
                    }
                    else
                    {
                        btEvent.uEventParam.btDisconnect.connectionHandle = pSyncHist->btSyncConnection[j].connectionHandle;
                    }
                    btcSendBTEvent( pMac, &btEvent );
                }
            }
        }
        //SYNC update
        for( i = 0; i < BT_MAX_SCO_SUPPORT; i++ )
        {
            if( pReplay->btcEventHist.btSyncUpdateEvent[i].fValid )
            {
                vos_mem_zero( &btEvent, sizeof(tSmeBtEvent) );
                btEvent.btEventType = BT_EVENT_SYNC_CONNECTION_UPDATED;
                vos_mem_copy( &btEvent.uEventParam.btSyncConnection, 
                            &pReplay->btcEventHist.btSyncUpdateEvent[i].btSyncConnection, 
                            sizeof(tSmeBtSyncConnectionParam) );
                btcSendBTEvent( pMac, &btEvent );
            }
        }
        //Clear all events
        vos_mem_zero( &pReplay->btcEventHist, sizeof(tSmeBtcEventHist) );
    }
}

static void btcPowerStateCB( v_PVOID_t pContext, tPmcState pmcState )
{
    tpAniSirGlobal pMac = PMAC_STRUCT(pContext);
    if( FULL_POWER == pmcState )
    {
        btcReplayEvents( pMac );
    }
}

/* ---------------------------------------------------------------------------
    \fn btcLogEvent
    \brief  API to log the the current Bluetooth event
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtEvent. Caller owns the memory and is responsible
                            for freeing it.
    \return None
  ---------------------------------------------------------------------------*/
static void btcLogEvent (tHalHandle hHal, tpSmeBtEvent pBtEvent)
{
   v_U8_t bdAddrRev[6];
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
               "Bluetooth Event %d received", __func__, pBtEvent->btEventType);
   switch(pBtEvent->btEventType)
   {
      case BT_EVENT_CREATE_SYNC_CONNECTION:
      case BT_EVENT_SYNC_CONNECTION_COMPLETE:
      case BT_EVENT_SYNC_CONNECTION_UPDATED:
          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "SCO Connection: "
               "connectionHandle = %d status = %d linkType %d "
               "scoInterval %d scoWindow %d retransmisisonWindow = %d ",
               pBtEvent->uEventParam.btSyncConnection.connectionHandle,
               pBtEvent->uEventParam.btSyncConnection.status,
               pBtEvent->uEventParam.btSyncConnection.linkType,
               pBtEvent->uEventParam.btSyncConnection.scoInterval,
               pBtEvent->uEventParam.btSyncConnection.scoWindow,
               pBtEvent->uEventParam.btSyncConnection.retransmisisonWindow);

          bdAddrRev[0] = pBtEvent->uEventParam.btSyncConnection.bdAddr[5];
          bdAddrRev[1] = pBtEvent->uEventParam.btSyncConnection.bdAddr[4];
          bdAddrRev[2] = pBtEvent->uEventParam.btSyncConnection.bdAddr[3];
          bdAddrRev[3] = pBtEvent->uEventParam.btSyncConnection.bdAddr[2];
          bdAddrRev[4] = pBtEvent->uEventParam.btSyncConnection.bdAddr[1];
          bdAddrRev[5] = pBtEvent->uEventParam.btSyncConnection.bdAddr[0];

          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "BD ADDR = "
                    MAC_ADDRESS_STR, MAC_ADDR_ARRAY(bdAddrRev));
          break;
      case BT_EVENT_CREATE_ACL_CONNECTION:
      case BT_EVENT_ACL_CONNECTION_COMPLETE:
          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "ACL Connection: "
               "connectionHandle = %d status = %d ",
               pBtEvent->uEventParam.btAclConnection.connectionHandle,
               pBtEvent->uEventParam.btAclConnection.status);

          bdAddrRev[0] = pBtEvent->uEventParam.btAclConnection.bdAddr[5];
          bdAddrRev[1] = pBtEvent->uEventParam.btAclConnection.bdAddr[4];
          bdAddrRev[2] = pBtEvent->uEventParam.btAclConnection.bdAddr[3];
          bdAddrRev[3] = pBtEvent->uEventParam.btAclConnection.bdAddr[2];
          bdAddrRev[4] = pBtEvent->uEventParam.btAclConnection.bdAddr[1];
          bdAddrRev[5] = pBtEvent->uEventParam.btAclConnection.bdAddr[0];

          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "BD ADDR = "
                    MAC_ADDRESS_STR, MAC_ADDR_ARRAY(bdAddrRev));
          break;
      case BT_EVENT_MODE_CHANGED:
          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "ACL Mode change : "
               "connectionHandle %d mode %d ",
               pBtEvent->uEventParam.btAclModeChange.connectionHandle,
               pBtEvent->uEventParam.btAclModeChange.mode);
          break;
      case BT_EVENT_DISCONNECTION_COMPLETE:
          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "Disconnect Event : "
               "connectionHandle %d ", pBtEvent->uEventParam.btAclModeChange.connectionHandle);
          break;
      default:
         break;
   }
 }

/*
   Caller can check whether BTC's current event allows UAPSD. This doesn't affect
   BMPS.
   return:  VOS_TRUE -- BTC is ready for UAPSD
            VOS_FALSE -- certain BT event is active, cannot enter UAPSD
*/
v_BOOL_t btcIsReadyForUapsd( tHalHandle hHal )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return( pMac->btc.btcUapsdOk );
}

/*
    Base on the BT event, this function sets the flag on whether to allow UAPSD
    At this time, we are only interested in SCO and A2DP.
    A2DP tracking is through BT_EVENT_A2DP_STREAM_START and BT_EVENT_A2DP_STREAM_STOP
    SCO is through BT_EVENT_SYNC_CONNECTION_COMPLETE and BT_EVENT_DISCONNECTION_COMPLETE
    BT_EVENT_DEVICE_SWITCHED_OFF overwrites them all
*/
void btcUapsdCheck( tpAniSirGlobal pMac, tpSmeBtEvent pBtEvent )
{
   v_U8_t i;
   v_BOOL_t fLastUapsdState = pMac->btc.btcUapsdOk, fMoreSCO = VOS_FALSE;
   switch( pBtEvent->btEventType )
   {
   case BT_EVENT_DISCONNECTION_COMPLETE:
       if( (VOS_FALSE == pMac->btc.btcUapsdOk) && 
           BT_INVALID_CONN_HANDLE != pBtEvent->uEventParam.btDisconnect.connectionHandle )
       {
           //Check whether all SCO connections are gone
           for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
           {
               if( (BT_INVALID_CONN_HANDLE != pMac->btc.btcScoHandles[i]) &&
                   (pMac->btc.btcScoHandles[i] != pBtEvent->uEventParam.btDisconnect.connectionHandle) )
               {
                   //We still have outstanding SCO connection
                   fMoreSCO = VOS_TRUE;
               }
               else if( pMac->btc.btcScoHandles[i] == pBtEvent->uEventParam.btDisconnect.connectionHandle )
               {
                   pMac->btc.btcScoHandles[i] = BT_INVALID_CONN_HANDLE;
               }
           }
           if( !fMoreSCO && !pMac->btc.fA2DPUp )
           {
               //All SCO is disconnected
               pMac->btc.btcUapsdOk = VOS_TRUE;
               smsLog( pMac, LOGE, "BT event (DISCONNECTION) happens, UAPSD-allowed flag (%d) change to TRUE",
                        pBtEvent->btEventType, pMac->btc.btcUapsdOk );
           }
       }
       break;
   case BT_EVENT_DEVICE_SWITCHED_OFF:
       smsLog( pMac, LOGE, "BT event (DEVICE_OFF) happens, UAPSD-allowed flag (%d) change to TRUE",
                        pBtEvent->btEventType, pMac->btc.btcUapsdOk );
       //Clean up SCO
       for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
       {
           pMac->btc.btcScoHandles[i] = BT_INVALID_CONN_HANDLE;
       }
       pMac->btc.fA2DPUp = VOS_FALSE;
       pMac->btc.btcUapsdOk = VOS_TRUE;
       break;
   case BT_EVENT_A2DP_STREAM_STOP:
       smsLog( pMac, LOGE, "BT event  (A2DP_STREAM_STOP) happens, UAPSD-allowed flag (%d)",
            pMac->btc.btcUapsdOk );
       pMac->btc.fA2DPUp = VOS_FALSE;
       //Check whether SCO is on
       for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
       {
           if(pMac->btc.btcScoHandles[i] != BT_INVALID_CONN_HANDLE)
           {
              break;
           }
       }
       if( BT_MAX_SCO_SUPPORT == i )
       {
            pMac->btc.fA2DPTrafStop = VOS_TRUE;
           smsLog( pMac, LOGE, "BT_EVENT_A2DP_STREAM_STOP: UAPSD-allowed flag is now %d",
                   pMac->btc.btcUapsdOk );
       }
       break;

   case BT_EVENT_MODE_CHANGED:
       smsLog( pMac, LOGE, "BT event (BT_EVENT_MODE_CHANGED) happens, Mode (%d) UAPSD-allowed flag (%d)",
               pBtEvent->uEventParam.btAclModeChange.mode, pMac->btc.btcUapsdOk );
       if(pBtEvent->uEventParam.btAclModeChange.mode == BT_ACL_SNIFF)
       {
           //Check whether SCO is on
           for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
           {
               if(pMac->btc.btcScoHandles[i] != BT_INVALID_CONN_HANDLE)
               {
                   break;
               }
           }
           if( BT_MAX_SCO_SUPPORT == i )
           {
               if(VOS_TRUE == pMac->btc.fA2DPTrafStop)
               {
                   pMac->btc.btcUapsdOk = VOS_TRUE;
                   pMac->btc.fA2DPTrafStop = VOS_FALSE;
               }
               smsLog( pMac, LOGE, "BT_EVENT_MODE_CHANGED with Mode:%d UAPSD-allowed flag is now %d",
                       pBtEvent->uEventParam.btAclModeChange.mode,pMac->btc.btcUapsdOk );
           }
       }
       break;
   case BT_EVENT_CREATE_SYNC_CONNECTION:
       {
           pMac->btc.btcUapsdOk = VOS_FALSE;
           smsLog( pMac, LOGE, "BT_EVENT_CREATE_SYNC_CONNECTION (%d) happens, UAPSD-allowed flag (%d) change to FALSE",
                   pBtEvent->btEventType, pMac->btc.btcUapsdOk );
       }
       break;
   case BT_EVENT_SYNC_CONNECTION_COMPLETE:
       //Make sure it is a success
       if( BT_CONN_STATUS_FAIL != pBtEvent->uEventParam.btSyncConnection.status )
       {
           //Save te handle for later use
           for( i = 0; i < BT_MAX_SCO_SUPPORT; i++)
           {
               VOS_ASSERT(BT_INVALID_CONN_HANDLE != pBtEvent->uEventParam.btSyncConnection.connectionHandle);
               if( (BT_INVALID_CONN_HANDLE == pMac->btc.btcScoHandles[i]) &&
                   (BT_INVALID_CONN_HANDLE != pBtEvent->uEventParam.btSyncConnection.connectionHandle))
               {
                   pMac->btc.btcScoHandles[i] = pBtEvent->uEventParam.btSyncConnection.connectionHandle;
                   break;
               }
           }

           if( i >= BT_MAX_SCO_SUPPORT )
           {
               smsLog(pMac, LOGE, FL("Too many SCO, ignore this one"));
           }
       }
       else
       {
            //Check whether SCO is on
           for(i=0; i < BT_MAX_SCO_SUPPORT; i++)
           {
               if(pMac->btc.btcScoHandles[i] != BT_INVALID_CONN_HANDLE)
               {
                   break;
               }
       }
           /*If No Other Sco/A2DP is ON reenable UAPSD*/
           if( (BT_MAX_SCO_SUPPORT == i)  && !pMac->btc.fA2DPUp)           
           {
               pMac->btc.btcUapsdOk = VOS_TRUE;
           }
           smsLog(pMac, LOGE, FL("TSYNC complete failed"));
       }
       break;
   case BT_EVENT_A2DP_STREAM_START:
       smsLog( pMac, LOGE, "BT_EVENT_A2DP_STREAM_START (%d) happens, UAPSD-allowed flag (%d) change to FALSE",
                pBtEvent->btEventType, pMac->btc.btcUapsdOk );
       pMac->btc.fA2DPTrafStop = VOS_FALSE;
       pMac->btc.btcUapsdOk = VOS_FALSE;
       pMac->btc.fA2DPUp = VOS_TRUE;
       break;
   default:
       //No change for these events
       smsLog( pMac, LOGE, "BT event (%d) happens, UAPSD-allowed flag (%d) no change",
                    pBtEvent->btEventType, pMac->btc.btcUapsdOk );
       break;
   }
   if(fLastUapsdState != pMac->btc.btcUapsdOk)
   {
      sme_QosTriggerUapsdChange( pMac );
   }
}

/* ---------------------------------------------------------------------------
    \fn btcHandleCoexInd
    \brief  API to handle Coex indication from WDI
    \param  pMac - The handle returned by macOpen.
    \return eHalStatus
            eHAL_STATUS_FAILURE  success
            eHAL_STATUS_SUCCESS  failure
  ---------------------------------------------------------------------------*/
eHalStatus btcHandleCoexInd(tHalHandle hHal, void* pMsg)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tSirSmeCoexInd *pSmeCoexInd = (tSirSmeCoexInd *)pMsg;

   if (NULL == pMsg)
   {
      smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
      status = eHAL_STATUS_FAILURE;
   }
   else
   {
      // DEBUG
      smsLog(pMac, LOG1, "Coex indication in %s(), type %d",
             __func__, pSmeCoexInd->coexIndType);

     // suspend heartbeat monitoring
     if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_DISABLE_HB_MONITOR)
     {
        // set heartbeat threshold CFG to zero
        ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, 0, NULL, eANI_BOOLEAN_FALSE);
        pMac->btc.btcHBActive = VOS_FALSE;
     }

     // resume heartbeat monitoring
     else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_ENABLE_HB_MONITOR)
     {
        if (!pMac->btc.btcHBActive) 
        {
           ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, pMac->btc.btcHBCount, NULL, eANI_BOOLEAN_FALSE);
           pMac->btc.btcHBActive = VOS_TRUE;
        }
     }
     else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_SCAN_COMPROMISED)
     {
         pMac->btc.btcScanCompromise = VOS_TRUE;
         smsLog(pMac, LOGW, "Coex indication in %s(), type - SIR_COEX_IND_TYPE_SCAN_COMPROMISED",
             __func__);
     }
     else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_SCAN_NOT_COMPROMISED)
     {
         pMac->btc.btcScanCompromise = VOS_FALSE;
         smsLog(pMac, LOGW, "Coex indication in %s(), type - SIR_COEX_IND_TYPE_SCAN_NOT_COMPROMISED",
             __func__);
     }
     else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4)
     {
         if (pMac->roam.configParam.disableAggWithBtc)
         {
             ccmCfgSetInt(pMac, WNI_CFG_DEL_ALL_RX_BA_SESSIONS_2_4_G_BTC, 1,
                             NULL, eANI_BOOLEAN_FALSE);
             pMac->btc.btcBssfordisableaggr[0] = pSmeCoexInd->coexIndData[0] & 0xFF;
             pMac->btc.btcBssfordisableaggr[1] = pSmeCoexInd->coexIndData[0] >> 8;
             pMac->btc.btcBssfordisableaggr[2] = pSmeCoexInd->coexIndData[1] & 0xFF;
             pMac->btc.btcBssfordisableaggr[3] = pSmeCoexInd->coexIndData[1]  >> 8;
             pMac->btc.btcBssfordisableaggr[4] = pSmeCoexInd->coexIndData[2] & 0xFF;
             pMac->btc.btcBssfordisableaggr[5] = pSmeCoexInd->coexIndData[2] >> 8;
             smsLog(pMac, LOGW, "Coex indication in %s(), "
                    "type - SIR_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4 "
                    "for BSSID "MAC_ADDRESS_STR,__func__,
                    MAC_ADDR_ARRAY(pMac->btc.btcBssfordisableaggr));
         }
     }
     else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4)
     {
         if (pMac->roam.configParam.disableAggWithBtc)
         {
             ccmCfgSetInt(pMac, WNI_CFG_DEL_ALL_RX_BA_SESSIONS_2_4_G_BTC, 0,
                             NULL, eANI_BOOLEAN_FALSE);
             smsLog(pMac, LOGW,
             "Coex indication in %s(), type - SIR_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4",
                 __func__);
         }
     }
     // unknown indication type
     else
     {
        smsLog(pMac, LOGE, "unknown Coex indication type in %s()", __func__);
     }
   }

   return(status);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/* ---------------------------------------------------------------------------
    \fn btcDiagEventLog
    \brief  API to log the the current Bluetooth event
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtEvent. Caller owns the memory and is responsible
                            for freeing it.
    \return None
  ---------------------------------------------------------------------------*/
static void btcDiagEventLog (tHalHandle hHal, tpSmeBtEvent pBtEvent)
{
   //vos_event_wlan_btc_type *log_ptr = NULL;
   WLAN_VOS_DIAG_EVENT_DEF(btDiagEvent, vos_event_wlan_btc_type);
   {
       btDiagEvent.eventId = pBtEvent->btEventType;
       switch(pBtEvent->btEventType)
       {
            case BT_EVENT_CREATE_SYNC_CONNECTION:
            case BT_EVENT_SYNC_CONNECTION_COMPLETE:
            case BT_EVENT_SYNC_CONNECTION_UPDATED:
                btDiagEvent.connHandle = pBtEvent->uEventParam.btSyncConnection.connectionHandle;
                btDiagEvent.connStatus = pBtEvent->uEventParam.btSyncConnection.status;
                btDiagEvent.linkType   = pBtEvent->uEventParam.btSyncConnection.linkType;
                btDiagEvent.scoInterval = pBtEvent->uEventParam.btSyncConnection.scoInterval;
                btDiagEvent.scoWindow  = pBtEvent->uEventParam.btSyncConnection.scoWindow;
                btDiagEvent.retransWindow = pBtEvent->uEventParam.btSyncConnection.retransmisisonWindow;
                vos_mem_copy(btDiagEvent.btAddr, pBtEvent->uEventParam.btSyncConnection.bdAddr,
                              sizeof(btDiagEvent.btAddr));
                break;
            case BT_EVENT_CREATE_ACL_CONNECTION:
            case BT_EVENT_ACL_CONNECTION_COMPLETE:
                btDiagEvent.connHandle = pBtEvent->uEventParam.btAclConnection.connectionHandle;
                btDiagEvent.connStatus = pBtEvent->uEventParam.btAclConnection.status;
                vos_mem_copy(btDiagEvent.btAddr, pBtEvent->uEventParam.btAclConnection.bdAddr,
                             sizeof(btDiagEvent.btAddr));
                break;
            case BT_EVENT_MODE_CHANGED:
                btDiagEvent.connHandle = pBtEvent->uEventParam.btAclModeChange.connectionHandle;
                btDiagEvent.mode = pBtEvent->uEventParam.btAclModeChange.mode;
                break;
            case BT_EVENT_DISCONNECTION_COMPLETE:
                btDiagEvent.connHandle = pBtEvent->uEventParam.btAclModeChange.connectionHandle;
                break;
            default:
                break;
       }
   }
   WLAN_VOS_DIAG_EVENT_REPORT(&btDiagEvent, EVENT_WLAN_BTC);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
