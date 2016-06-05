/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/*============================================================================
  @file wlan_hdd_wmm.c

  This module (wlan_hdd_wmm.h interface + wlan_hdd_wmm.c implementation)
  houses all the logic for WMM in HDD.

  On the control path, it has the logic to setup QoS, modify QoS and delete
  QoS (QoS here refers to a TSPEC). The setup QoS comes in two flavors: an
  explicit application invoked and an internal HDD invoked.  The implicit QoS
  is for applications that do NOT call the custom QCT WLAN OIDs for QoS but
  which DO mark their traffic for priortization. It also has logic to start,
  update and stop the U-APSD trigger frame generation. It also has logic to
  read WMM related config parameters from the registry.

  On the data path, it has the logic to figure out the WMM AC of an egress
  packet and when to signal TL to serve a particular AC queue. It also has the
  logic to retrieve a packet based on WMM priority in response to a fetch from
  TL.

  The remaining functions are utility functions for information hiding.


============================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_dp_utils.h>
#include <wlan_hdd_wmm.h>
#include <wlan_hdd_ether.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <vos_sched.h>
#include "sme_Api.h"
#include "sapInternal.h"
// change logging behavior based upon debug flag
#ifdef HDD_WMM_DEBUG
#define WMM_TRACE_LEVEL_FATAL      VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_ERROR      VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_WARN       VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_INFO       VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_INFO_HIGH  VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_INFO_LOW   VOS_TRACE_LEVEL_FATAL
#else
#define WMM_TRACE_LEVEL_FATAL      VOS_TRACE_LEVEL_FATAL
#define WMM_TRACE_LEVEL_ERROR      VOS_TRACE_LEVEL_ERROR
#define WMM_TRACE_LEVEL_WARN       VOS_TRACE_LEVEL_WARN
#define WMM_TRACE_LEVEL_INFO       VOS_TRACE_LEVEL_INFO
#define WMM_TRACE_LEVEL_INFO_HIGH  VOS_TRACE_LEVEL_INFO_HIGH
#define WMM_TRACE_LEVEL_INFO_LOW   VOS_TRACE_LEVEL_INFO_LOW
#endif


#define WLAN_HDD_MAX_DSCP 0x3f

// DHCP Port number
#define DHCP_SOURCE_PORT 0x4400
#define DHCP_DESTINATION_PORT 0x4300

#define HDD_WMM_UP_TO_AC_MAP_SIZE 8

const v_U8_t hddWmmUpToAcMap[] = {
   WLANTL_AC_BE,
   WLANTL_AC_BK,
   WLANTL_AC_BK,
   WLANTL_AC_BE,
   WLANTL_AC_VI,
   WLANTL_AC_VI,
   WLANTL_AC_VO,
   WLANTL_AC_VO
};

//Linux based UP -> AC Mapping
const v_U8_t hddLinuxUpToAcMap[8] = {
   HDD_LINUX_AC_BE,
   HDD_LINUX_AC_BK,
   HDD_LINUX_AC_BK,
   HDD_LINUX_AC_BE,
   HDD_LINUX_AC_VI,
   HDD_LINUX_AC_VI,
   HDD_LINUX_AC_VO,
   HDD_LINUX_AC_VO
};

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
/**
  @brief hdd_wmm_enable_tl_uapsd() - function which decides whether and
  how to update UAPSD parameters in TL

  @param pQosContext : [in] the pointer the QoS instance control block

  @return
  None
*/
static void hdd_wmm_enable_tl_uapsd (hdd_wmm_qos_context_t* pQosContext)
{
   hdd_adapter_t* pAdapter = pQosContext->pAdapter;
   WLANTL_ACEnumType acType = pQosContext->acType;
   hdd_wmm_ac_status_t *pAc = NULL;
   VOS_STATUS status;
   v_U32_t service_interval;
   v_U32_t suspension_interval;
   sme_QosWmmDirType direction;
   v_BOOL_t psb;

   if (acType >= WLANTL_MAX_AC)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid AC: %d", __func__, acType);
      return;
   }

   pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];

   // The TSPEC must be valid
   if (pAc->wmmAcTspecValid == VOS_FALSE)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invoked with invalid TSPEC",
                __func__);
      return;
   }

   // determine the service interval
   if (pAc->wmmAcTspecInfo.min_service_interval)
   {
      service_interval = pAc->wmmAcTspecInfo.min_service_interval;
   }
   else if (pAc->wmmAcTspecInfo.max_service_interval)
   {
      service_interval = pAc->wmmAcTspecInfo.max_service_interval;
   }
   else
   {
      // no service interval is present in the TSPEC
      // this is OK, there just won't be U-APSD
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: No service interval supplied",
                __func__);
      service_interval = 0;
   }

   // determine the suspension interval & direction
   suspension_interval = pAc->wmmAcTspecInfo.suspension_interval;
   direction = pAc->wmmAcTspecInfo.ts_info.direction;
   psb = pAc->wmmAcTspecInfo.ts_info.psb;

   // if we have previously enabled U-APSD, have any params changed?
   if ((pAc->wmmAcUapsdInfoValid) &&
       (pAc->wmmAcUapsdServiceInterval == service_interval) &&
       (pAc->wmmAcUapsdSuspensionInterval == suspension_interval) &&
       (pAc->wmmAcUapsdDirection == direction) &&
       (pAc->wmmAcIsUapsdEnabled == psb))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: No change in U-APSD parameters",
                __func__);
      return;
   }

   // are we in the appropriate power save modes?
   if (!sme_IsPowerSaveEnabled(WLAN_HDD_GET_HAL_CTX(pAdapter), ePMC_BEACON_MODE_POWER_SAVE))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: BMPS is not enabled",
                __func__);
      return;
   }

   if (!sme_IsPowerSaveEnabled(WLAN_HDD_GET_HAL_CTX(pAdapter), ePMC_UAPSD_MODE_POWER_SAVE))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: U-APSD is not enabled",
                __func__);
      return;
   }

   // everything is in place to notify TL
   status = WLANTL_EnableUAPSDForAC((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                    (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                    acType,
                                    pAc->wmmAcTspecInfo.ts_info.tid,
                                    pAc->wmmAcTspecInfo.ts_info.up,
                                    service_interval,
                                    suspension_interval,
                                    direction);

   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: Failed to enable U-APSD for AC=%d",
                 __func__, acType );
      return;
   }

   // stash away the parameters that were used
   pAc->wmmAcUapsdInfoValid = VOS_TRUE;
   pAc->wmmAcUapsdServiceInterval = service_interval;
   pAc->wmmAcUapsdSuspensionInterval = suspension_interval;
   pAc->wmmAcUapsdDirection = direction;
   pAc->wmmAcIsUapsdEnabled = psb;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: Enabled UAPSD in TL srv_int=%d "
             "susp_int=%d dir=%d AC=%d",
             __func__,
             service_interval,
             suspension_interval,
             direction,
             acType);

}

/**
  @brief hdd_wmm_disable_tl_uapsd() - function which decides whether
  to disable UAPSD parameters in TL

  @param pQosContext : [in] the pointer the QoS instance control block

  @return
  None
*/
static void hdd_wmm_disable_tl_uapsd (hdd_wmm_qos_context_t* pQosContext)
{
   hdd_adapter_t* pAdapter = pQosContext->pAdapter;
   WLANTL_ACEnumType acType = pQosContext->acType;
   hdd_wmm_ac_status_t *pAc = NULL;
   VOS_STATUS status;
   v_U32_t service_interval;
   v_U32_t suspension_interval;
   v_U8_t uapsd_mask;
   v_U8_t ActiveTspec = INVALID_TSPEC;

   if (acType >= WLANTL_MAX_AC)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid AC: %d", __func__, acType);
      return;
   }

   pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];

   // have we previously enabled UAPSD?
   if (pAc->wmmAcUapsdInfoValid == VOS_TRUE)
   {
      uapsd_mask = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask;

      //Finding uapsd_mask as per AC
      uapsd_mask = uapsd_mask & (1 << (WLANTL_AC_VO - acType));

      sme_QosTspecActive((tpAniSirGlobal)WLAN_HDD_GET_HAL_CTX(pAdapter), acType,
                         pAdapter->sessionId, &ActiveTspec);

      //Call WLANTL_EnableUAPSDForAC only when static uapsd mask is present and
      // no active tspecs. TODO: Need to change naming convention as Enable
      // UAPSD function is called in hdd_wmm_disable_tl_uapsd. Purpose of
      // calling WLANTL_EnableUAPSDForAC is to update UAPSD intervals to fw

      if(uapsd_mask && !ActiveTspec)
      {
         switch(acType)
         {
         case WLANTL_AC_VO:
            service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSrvIntv;
            suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSuspIntv;
            break;
         case WLANTL_AC_VI:
            service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSrvIntv;
            suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSuspIntv;
            break;
         case WLANTL_AC_BE:
            service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSrvIntv;
            suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSuspIntv;
            break;
         case WLANTL_AC_BK:
            service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSrvIntv;
            suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSuspIntv;
            break;
         default:
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                    "%s: Invalid AC %d", __func__, acType );
            return;
         }

         status = WLANTL_EnableUAPSDForAC((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                  (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                   acType,
                   pAc->wmmAcTspecInfo.ts_info.tid,
                   pAc->wmmAcTspecInfo.ts_info.up,
                   service_interval,
                   suspension_interval,
                   pAc->wmmAcTspecInfo.ts_info.direction);

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                    "%s: Failed to update U-APSD params for AC=%d",
                    __func__, acType );
         }
         else
         {
            // TL no longer has valid UAPSD info
            pAc->wmmAcUapsdInfoValid = VOS_FALSE;
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Updated UAPSD params in TL for AC=%d",
                   __func__,
                   acType);
         }
      }
   }
}

#endif

/**
  @brief hdd_wmm_free_context() - function which frees a QoS context

  @param pQosContext : [in] the pointer the QoS instance control block

  @return
  None
*/
static void hdd_wmm_free_context (hdd_wmm_qos_context_t* pQosContext)
{
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                FL("pVosContext is NULL"));
      return;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                FL("HddCtx is NULL"));
      return;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered, context %p",
             __func__, pQosContext);

   // take the wmmLock since we're manipulating the context list
   mutex_lock(&pHddCtx->wmmLock);

   if (unlikely((NULL == pQosContext) ||
                (HDD_WMM_CTX_MAGIC != pQosContext->magic)))
   {
      // must have been freed in another thread
      mutex_unlock(&pHddCtx->wmmLock);
      return;
   }

   // make sure nobody thinks this is a valid context
   pQosContext->magic = 0;

   // unlink the context
   list_del(&pQosContext->node);

   // done manipulating the list
   mutex_unlock(&pHddCtx->wmmLock);

   // reclaim memory
   kfree(pQosContext);

}

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
/**
  @brief hdd_wmm_notify_app() - function which notifies an application
                                changes in state of it flow

  @param pQosContext : [in] the pointer the QoS instance control block

  @return
  None
*/
#define MAX_NOTIFY_LEN 50
static void hdd_wmm_notify_app (hdd_wmm_qos_context_t* pQosContext)
{
   hdd_adapter_t* pAdapter;
   union iwreq_data wrqu;
   char buf[MAX_NOTIFY_LEN+1];
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
         return;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered, context %p",
             __func__, pQosContext);

   mutex_lock(&pHddCtx->wmmLock);
   if (unlikely((NULL == pQosContext) ||
                (HDD_WMM_CTX_MAGIC != pQosContext->magic)))
   {
      mutex_unlock(&pHddCtx->wmmLock);
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid QoS Context",
                __func__);
      return;
   }
   // get pointer to the adapter
   pAdapter = pQosContext->pAdapter;
   mutex_unlock(&pHddCtx->wmmLock);

   // create the event
   memset(&wrqu, 0, sizeof(wrqu));
   memset(buf, 0, sizeof(buf));

   snprintf(buf, MAX_NOTIFY_LEN, "QCOM: TS change[%u: %u]",
            (unsigned int)pQosContext->handle,
            (unsigned int)pQosContext->lastStatus);

   wrqu.data.pointer = buf;
   wrqu.data.length = strlen(buf);



   // send the event
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: Sending [%s]", __func__, buf);
   wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, buf);
}


/**
  @brief hdd_wmm_is_access_allowed() - function which determines if access
  is allowed for the given AC.  this is designed to be called during SME
  callback processing since that is when access can be granted or removed

  @param pAdapter    : [in] pointer to adapter context
  @param pAc         : [in] pointer to the per-AC status

  @return            : VOS_TRUE - access is allowed
                     : VOS_FALSE - access is not allowed
  None
*/
static v_BOOL_t hdd_wmm_is_access_allowed(hdd_adapter_t* pAdapter,
                                          hdd_wmm_ac_status_t* pAc)
{
   // if we don't want QoS or the AP doesn't support QoS
   // or we don't want to do implicit QoS
   // or if AP doesn't require admission for this AC
   // then we have access
   if (!hdd_wmm_is_active(pAdapter) ||
       !(WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->bImplicitQosEnabled ||
       !pAc->wmmAcAccessRequired)
   {
      return VOS_TRUE;
   }

   // if implicit QoS has already completed, successfully or not,
   // then access is allowed
   if (pAc->wmmAcAccessGranted || pAc->wmmAcAccessFailed)
   {
      return VOS_TRUE;
   }

   // admission is required and implicit QoS hasn't completed
   // however explicit QoS may have completed and we'll have
   // a Tspec
   // if we don't have a Tspec then access is not allowed
   if (!pAc->wmmAcTspecValid)
   {
      return VOS_FALSE;
   }

   // we have a Tspec -- does it allow upstream or bidirectional traffic?
   // if it only allows downstream traffic then access is not allowed
   if (pAc->wmmAcTspecInfo.ts_info.direction == SME_QOS_WMM_TS_DIR_DOWNLINK)
   {
      return VOS_FALSE;
   }

   // we meet all of the criteria for access
   return VOS_TRUE;
}

#ifdef FEATURE_WLAN_ESE
/**
  @brief hdd_wmm_inactivity_timer_cb() - timer handler function which is
  called for every inactivity interval per AC. This function gets the
  current transmitted packets on the given AC, and checks if there where
  any TX activity from the previous interval. If there was no traffic
  then it would delete the TS that was negotiated on that AC.

  @param pUserData   : [in] pointer to pQosContext

  @return            : NONE
*/
void hdd_wmm_inactivity_timer_cb( v_PVOID_t pUserData )
{
    hdd_wmm_qos_context_t* pQosContext = (hdd_wmm_qos_context_t*)pUserData;
    hdd_adapter_t* pAdapter;
    hdd_wmm_ac_status_t *pAc;
    hdd_wlan_wmm_status_e status;
    VOS_STATUS vos_status;
    v_U32_t currentTrafficCnt = 0;
    WLANTL_ACEnumType acType = 0;
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;

    ENTER();
    if (NULL == pVosContext)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                  "%s: Invalid VOS Context", __func__);
        return;
    }

    pHddCtx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
    if (0 != (wlan_hdd_validate_context(pHddCtx)))
    {
        return;
    }

    mutex_lock(&pHddCtx->wmmLock);
    if (unlikely((NULL == pQosContext) ||
                (HDD_WMM_CTX_MAGIC != pQosContext->magic)))
    {
        mutex_unlock(&pHddCtx->wmmLock);
        VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                  "%s: Invalid QoS Context",
                  __func__);
        return;
    }
    mutex_unlock(&pHddCtx->wmmLock);

    acType = pQosContext->acType;
    pAdapter = pQosContext->pAdapter;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("invalid pAdapter: %p"), pAdapter);
        return;
    }

    pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];

    // Get the Tx stats for this AC.
    currentTrafficCnt = pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[pQosContext->acType];

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
            FL("WMM inactivity Timer for AC=%d, currentCnt=%d, prevCnt=%d"),
            acType, (int)currentTrafficCnt, (int)pAc->wmmPrevTrafficCnt);
    if (pAc->wmmPrevTrafficCnt == currentTrafficCnt)
    {
        // If there is no traffic activity, delete the TSPEC for this AC
        status = hdd_wmm_delts(pAdapter, pQosContext->handle);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                FL("Deleted TS on AC %d, due to inactivity with status = %d!!!"),
                acType, status);
    }
    else
    {
        pAc->wmmPrevTrafficCnt = currentTrafficCnt;
        if (pAc->wmmInactivityTimer.state == VOS_TIMER_STATE_STOPPED)
        {
            // Restart the timer
            vos_status = vos_timer_start(&pAc->wmmInactivityTimer, pAc->wmmInactivityTime);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        FL("Restarting inactivity timer failed on AC %d"), acType);
            }
        }
        else
        {
            VOS_ASSERT(vos_timer_getCurrentState(
                        &pAc->wmmInactivityTimer) == VOS_TIMER_STATE_STOPPED);
        }
    }

    EXIT();
    return;
}


/**
  @brief hdd_wmm_enable_inactivity_timer() - function to enable the
  traffic inactivity timer for the given AC, if the inactivity_interval
  specified in the ADDTS parameters is non-zero

  @param pQosContext   : [in] pointer to pQosContext
  @param inactivityTime: [in] value of the inactivity interval in millisecs

  @return              : VOS_STATUS_E_FAILURE
                         VOS_STATUS_SUCCESS
*/
VOS_STATUS hdd_wmm_enable_inactivity_timer(hdd_wmm_qos_context_t* pQosContext, v_U32_t inactivityTime)
{
    VOS_STATUS vos_status = VOS_STATUS_E_FAILURE;
    hdd_adapter_t* pAdapter = pQosContext->pAdapter;
    WLANTL_ACEnumType acType = pQosContext->acType;
    hdd_wmm_ac_status_t *pAc;

    pAdapter = pQosContext->pAdapter;
    pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];


    // If QoS-Tspec is successfully setup and if the inactivity timer is non-zero,
    // a traffic inactivity timer needs to be started for the given AC
    vos_status = vos_timer_init(
            &pAc->wmmInactivityTimer,
            VOS_TIMER_TYPE_SW,
            hdd_wmm_inactivity_timer_cb,
            (v_PVOID_t)pQosContext );
    if ( !VOS_IS_STATUS_SUCCESS(vos_status))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("Initializing inactivity timer failed on AC %d"), acType);
        return vos_status;
    }

    // Start the inactivity timer
    vos_status = vos_timer_start(
            &pAc->wmmInactivityTimer,
            inactivityTime);
    if ( !VOS_IS_STATUS_SUCCESS(vos_status))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("Starting inactivity timer failed on AC %d"), acType);
        return vos_status;
    }
    pAc->wmmInactivityTime = inactivityTime;
    // Initialize the current tx traffic count on this AC
    pAc->wmmPrevTrafficCnt = pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[pQosContext->acType];

    return vos_status;
}

/**
  @brief hdd_wmm_enable_inactivity_timer() - function to disable the
  traffic inactivity timer for the given AC. This would be called when
  deleting the TS.

  @param pQosContext   : [in] pointer to pQosContext

  @return              : VOS_STATUS_E_FAILURE
                         VOS_STATUS_SUCCESS
*/
VOS_STATUS hdd_wmm_disable_inactivity_timer(hdd_wmm_qos_context_t* pQosContext)
{
    hdd_adapter_t* pAdapter = pQosContext->pAdapter;
    WLANTL_ACEnumType acType = pQosContext->acType;
    hdd_wmm_ac_status_t *pAc  = &pAdapter->hddWmmStatus.wmmAcStatus[acType];
    VOS_STATUS vos_status = VOS_STATUS_E_FAILURE;

    // Clear the timer and the counter
    pAc->wmmInactivityTime = 0;
    pAc->wmmPrevTrafficCnt = 0;
    vos_timer_stop(&pAc->wmmInactivityTimer);
    vos_status = vos_timer_destroy(&pAc->wmmInactivityTimer);

    return vos_status;
}
#endif // FEATURE_WLAN_ESE

/**
  @brief hdd_wmm_sme_callback() - callback registered by HDD with SME for receiving
  QoS notifications. Even though this function has a static scope it gets called
  externally through some function pointer magic (so there is a need for
  rigorous parameter checking)

  @param hHal : [in] the HAL handle
  @param HddCtx : [in] the HDD specified handle
  @param pCurrentQosInfo : [in] the TSPEC params
  @param SmeStatus : [in] the QoS related SME status

  @return
  eHAL_STATUS_SUCCESS if all good, eHAL_STATUS_FAILURE otherwise
*/
static eHalStatus hdd_wmm_sme_callback (tHalHandle hHal,
                                        void * hddCtx,
                                        sme_QosWmmTspecInfo* pCurrentQosInfo,
                                        sme_QosStatusType smeStatus,
                                        v_U32_t qosFlowId)
{
   hdd_wmm_qos_context_t* pQosContext = hddCtx;
   hdd_adapter_t* pAdapter;
   WLANTL_ACEnumType acType;
   hdd_wmm_ac_status_t *pAc;
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
      return eHAL_STATUS_FAILURE;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return eHAL_STATUS_FAILURE;
   }


   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered, context %p",
             __func__, pQosContext);

   mutex_lock(&pHddCtx->wmmLock);
   if (unlikely((NULL == pQosContext) ||
                (HDD_WMM_CTX_MAGIC != pQosContext->magic)))
   {
      mutex_unlock(&pHddCtx->wmmLock);
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid QoS Context",
                __func__);
      return eHAL_STATUS_FAILURE;
   }
   mutex_unlock(&pHddCtx->wmmLock);

   pAdapter = pQosContext->pAdapter;
   acType = pQosContext->acType;
   pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: status %d flowid %d info %p",
             __func__, smeStatus, qosFlowId, pCurrentQosInfo);

   switch (smeStatus)
   {

   case SME_QOS_STATUS_SETUP_SUCCESS_IND:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Setup is complete",
                __func__);

      // there will always be a TSPEC returned with this status, even if
      // a TSPEC is not exchanged OTA
      if (pCurrentQosInfo)
      {
         pAc->wmmAcTspecValid = VOS_TRUE;
         memcpy(&pAc->wmmAcTspecInfo,
                pCurrentQosInfo,
                sizeof(pAc->wmmAcTspecInfo));
      }

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL for TL AC %d",
                   __func__, acType);

         // this was triggered by implicit QoS so we know packets are pending
         // update state
         pAc->wmmAcAccessAllowed = VOS_TRUE;
         pAc->wmmAcAccessGranted = VOS_TRUE;
         pAc->wmmAcAccessPending = VOS_FALSE;

         // notify TL that packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS;
         hdd_wmm_notify_app(pQosContext);
      }

#ifdef FEATURE_WLAN_ESE
      // Check if the inactivity interval is specified
      if (pCurrentQosInfo && pCurrentQosInfo->inactivity_interval) {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                 "%s: Inactivity timer value = %d for AC=%d",
                 __func__, pCurrentQosInfo->inactivity_interval, acType);
         hdd_wmm_enable_inactivity_timer(pQosContext, pCurrentQosInfo->inactivity_interval);
      }
#endif // FEATURE_WLAN_ESE

      // notify TL to enable trigger frames if necessary
      hdd_wmm_enable_tl_uapsd(pQosContext);

      break;

   case SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: Setup is complete (U-APSD set previously)",
                __func__);

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL",
                   __func__);

         // this was triggered by implicit QoS so we know packets are pending
         // update state
         pAc->wmmAcAccessAllowed = VOS_TRUE;
         pAc->wmmAcAccessGranted = VOS_TRUE;
         pAc->wmmAcAccessPending = VOS_FALSE;

         // notify TL that packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_UAPSD_EXISTING;
         hdd_wmm_notify_app(pQosContext);
      }

      break;

   case SME_QOS_STATUS_SETUP_FAILURE_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Setup failed",
                __func__);
      // QoS setup failed

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL",
                   __func__);

         // we note the failure, but we also mark access as allowed so that
         // the packets will flow.  Note that the MAC will "do the right thing"
         pAc->wmmAcAccessPending = VOS_FALSE;
         pAc->wmmAcAccessFailed = VOS_TRUE;
         pAc->wmmAcAccessAllowed = VOS_TRUE;

         // this was triggered by implicit QoS so we know packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED;
         hdd_wmm_notify_app(pQosContext);
      }

      /* Setting up QoS Failed, QoS context can be released.
       * SME is releasing this flow information and if HDD doen't release this context,
       * next time if application uses the same handle to set-up QoS, HDD (as it has
       * QoS context for this handle) will issue Modify QoS request to SME but SME will
       * reject as no it has no information for this flow.
       */
      hdd_wmm_free_context(pQosContext);
      break;

   case SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Setup Invalid Params, notify TL",
                __func__);
      // QoS setup failed

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL",
                   __func__);

         // we note the failure, but we also mark access as allowed so that
         // the packets will flow.  Note that the MAC will "do the right thing"
         pAc->wmmAcAccessPending = VOS_FALSE;
         pAc->wmmAcAccessFailed = VOS_TRUE;
         pAc->wmmAcAccessAllowed = VOS_TRUE;

         // this was triggered by implicit QoS so we know packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP:
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: Setup failed, not a QoS AP",
                 __func__);
      if (!HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_NO_WMM;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_SETUP_REQ_PENDING_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: Setup pending",
                __func__);
      // not a callback status -- ignore if we get it
      break;

   case SME_QOS_STATUS_SETUP_MODIFIED_IND:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: Setup modified",
                __func__);
      if (pCurrentQosInfo)
      {
         // update the TSPEC
         pAc->wmmAcTspecValid = VOS_TRUE;
         memcpy(&pAc->wmmAcTspecInfo,
                pCurrentQosInfo,
                sizeof(pAc->wmmAcTspecInfo));

         if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                      "%s: Explicit Qos, notifying userspace",
                      __func__);

            // this was triggered by an application
            pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFIED;
            hdd_wmm_notify_app(pQosContext);
         }

         // need to tell TL to update its UAPSD handling
         hdd_wmm_enable_tl_uapsd(pQosContext);
      }
      break;

   case SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP:
      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL",
                   __func__);

         // this was triggered by implicit QoS so we know packets are pending
         pAc->wmmAcAccessPending = VOS_FALSE;
         pAc->wmmAcAccessGranted = VOS_TRUE;
         pAc->wmmAcAccessAllowed = VOS_TRUE;

         // notify TL that packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_NO_UAPSD;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING:
      // nothing to do for now
      break;

   case SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_SET_FAILED:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Setup successful but U-APSD failed",
                __func__);

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {

         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Implicit Qos, notifying TL",
                   __func__);

         // QoS setup was successful but setting U=APSD failed
         // Since the OTA part of the request was successful, we don't mark
         // this as a failure.
         // the packets will flow.  Note that the MAC will "do the right thing"
         pAc->wmmAcAccessGranted = VOS_TRUE;
         pAc->wmmAcAccessAllowed = VOS_TRUE;
         pAc->wmmAcAccessFailed = VOS_FALSE;
         pAc->wmmAcAccessPending = VOS_FALSE;

         // this was triggered by implicit QoS so we know packets are pending
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        acType );

         if ( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                       "%s: Failed to signal TL for AC=%d",
                       __func__, acType );
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_SETUP_UAPSD_SET_FAILED;
         hdd_wmm_notify_app(pQosContext);
      }

      // Since U-APSD portion failed disabled trigger frame generation
      hdd_wmm_disable_tl_uapsd(pQosContext);

      break;

   case SME_QOS_STATUS_RELEASE_SUCCESS_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: Release is complete",
                __func__);

      if (pCurrentQosInfo)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: flows still active",
                   __func__);

         // there is still at least one flow active for this AC
         // so update the AC state
         memcpy(&pAc->wmmAcTspecInfo,
                pCurrentQosInfo,
                sizeof(pAc->wmmAcTspecInfo));

         // need to tell TL to update its UAPSD handling
         hdd_wmm_enable_tl_uapsd(pQosContext);
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: last flow",
                   __func__);

         // this is the last flow active for this AC so update the AC state
         pAc->wmmAcTspecValid = VOS_FALSE;

         // need to tell TL to update its UAPSD handling
         hdd_wmm_disable_tl_uapsd(pQosContext);
      }

      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_RELEASE_SUCCESS;
         hdd_wmm_notify_app(pQosContext);
      }

      // we are done with this flow
      hdd_wmm_free_context(pQosContext);
      break;

   case SME_QOS_STATUS_RELEASE_FAILURE_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: Release failure",
                __func__);

      // we don't need to update our state or TL since nothing has changed
      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_RELEASE_FAILED;
         hdd_wmm_notify_app(pQosContext);
      }

      break;

   case SME_QOS_STATUS_RELEASE_QOS_LOST_IND:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: QOS Lost indication received",
                __func__);

      // current TSPEC is no longer valid
      pAc->wmmAcTspecValid = VOS_FALSE;

      // need to tell TL to update its UAPSD handling
      hdd_wmm_disable_tl_uapsd(pQosContext);

      if (HDD_WMM_HANDLE_IMPLICIT == pQosContext->handle)
      {
         // we no longer have implicit access granted
         pAc->wmmAcAccessGranted = VOS_FALSE;
         pAc->wmmAcAccessFailed = VOS_FALSE;
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: Explicit Qos, notifying userspace",
                   __func__);

         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_LOST;
         hdd_wmm_notify_app(pQosContext);
      }

      // we are done with this flow
      hdd_wmm_free_context(pQosContext);
      break;

   case SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Release pending",
                __func__);
      // not a callback status -- ignore if we get it
      break;

   case SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Release Invalid Params",
                __func__);
      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_RELEASE_FAILED_BAD_PARAM;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Modification is complete, notify TL",
                __func__);

      // there will always be a TSPEC returned with this status, even if
      // a TSPEC is not exchanged OTA
      if (pCurrentQosInfo)
      {
         pAc->wmmAcTspecValid = VOS_TRUE;
         memcpy(&pAc->wmmAcTspecInfo,
                pCurrentQosInfo,
                sizeof(pAc->wmmAcTspecInfo));
      }

      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS;
         hdd_wmm_notify_app(pQosContext);
      }

      // notify TL to enable trigger frames if necessary
      hdd_wmm_enable_tl_uapsd(pQosContext);

      break;

   case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY:
      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_UAPSD_EXISTING;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP:
      // the flow modification failed so we'll leave in place
      // whatever existed beforehand

      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_FAILED;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: modification pending",
                __func__);
      // not a callback status -- ignore if we get it
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP:
      // the flow modification was successful but no QoS changes required

      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_NO_UAPSD;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP:
      // invalid params -- notify the application
      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_FAILED_BAD_PARAM;
         hdd_wmm_notify_app(pQosContext);
      }
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_PENDING:
      // nothing to do for now.  when APSD is established we'll have work to do
      break;

   case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_SET_FAILED:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Modify successful but U-APSD failed",
                __func__);

      // QoS modification was successful but setting U=APSD failed.
      // This will always be an explicit QoS instance, so all we can
      // do is notify the application and let it clean up.
      if (HDD_WMM_HANDLE_IMPLICIT != pQosContext->handle)
      {
         // this was triggered by an application
         pQosContext->lastStatus = HDD_WLAN_WMM_STATUS_MODIFY_UAPSD_SET_FAILED;
         hdd_wmm_notify_app(pQosContext);
      }

      // Since U-APSD portion failed disabled trigger frame generation
      hdd_wmm_disable_tl_uapsd(pQosContext);

      break;

   case SME_QOS_STATUS_HANDING_OFF:
      // no roaming so we won't see this
      break;

   case SME_QOS_STATUS_OUT_OF_APSD_POWER_MODE_IND:
      // need to tell TL to stop trigger frame generation
      hdd_wmm_disable_tl_uapsd(pQosContext);
      break;

   case SME_QOS_STATUS_INTO_APSD_POWER_MODE_IND:
      // need to tell TL to start sending trigger frames again
      hdd_wmm_enable_tl_uapsd(pQosContext);
      break;

   default:
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: unexpected SME Status=%d",
                 __func__, smeStatus );
      VOS_ASSERT(0);
   }

   // our access to the particular access category may have changed.
   // some of the implicit QoS cases above may have already set this
   // prior to invoking TL (so that we will properly service the
   // Tx queues) but let's consistently handle all cases here
   pAc->wmmAcAccessAllowed = hdd_wmm_is_access_allowed(pAdapter, pAc);

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: complete, access for TL AC %d is%sallowed",
             __func__,
             acType,
             pAc->wmmAcAccessAllowed ? " " : " not ");

   return eHAL_STATUS_SUCCESS;
}
#endif

/**========================================================================
  @brief hdd_wmmps_helper() - Function to set uapsd psb dynamically

  @param pAdapter     : [in] pointer to adapter structure

  @param ptr          : [in] pointer to command buffer

  @return             : Zero on success, appropriate error on failure.
  =======================================================================*/
int hdd_wmmps_helper(hdd_adapter_t *pAdapter, tANI_U8 *ptr)
{
   if (NULL == pAdapter)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: pAdapter is NULL", __func__);
       return -EINVAL;
   }
   if (NULL == ptr)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: ptr is NULL", __func__);
       return -EINVAL;
   }
   /* convert ASCII to integer */
   pAdapter->configuredPsb = ptr[9] - '0';
   pAdapter->psbChanged = HDD_PSB_CHANGED;

   return 0;
}

/**============================================================================
  @brief hdd_wmm_do_implicit_qos() - Function which will attempt to setup
  QoS for any AC requiring it

  @param work     : [in]  pointer to work structure

  @return         : void
  ===========================================================================*/
static void __hdd_wmm_do_implicit_qos(struct work_struct *work)
{
   hdd_wmm_qos_context_t* pQosContext =
      container_of(work, hdd_wmm_qos_context_t, wmmAcSetupImplicitQos);
   hdd_adapter_t* pAdapter;
   WLANTL_ACEnumType acType;
   hdd_wmm_ac_status_t *pAc;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   VOS_STATUS status;
   sme_QosStatusType smeStatus;
#endif
   sme_QosWmmTspecInfo qosInfo;
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;
   int ret = 0;

   if (NULL == pVosContext)
   {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
         return;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);

   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret)
   {
       hddLog(LOGE, FL("HDD context is invalid"));
       return;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered, context %p",
             __func__, pQosContext);

   mutex_lock(&pHddCtx->wmmLock);
   if (unlikely(HDD_WMM_CTX_MAGIC != pQosContext->magic))
   {
      mutex_unlock(&pHddCtx->wmmLock);
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid QoS Context",
                __func__);
      return;
   }
   mutex_unlock(&pHddCtx->wmmLock);

   pAdapter = pQosContext->pAdapter;
   acType = pQosContext->acType;
   pAc = &pAdapter->hddWmmStatus.wmmAcStatus[acType];

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: pAdapter %p acType %d",
             __func__, pAdapter, acType);

   if (!pAc->wmmAcAccessNeeded)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: AC %d doesn't need service",
                __func__, acType);
      pQosContext->magic = 0;
      kfree(pQosContext);
      return;
   }

   pAc->wmmAcAccessPending = VOS_TRUE;
   pAc->wmmAcAccessNeeded = VOS_FALSE;

   memset(&qosInfo, 0, sizeof(qosInfo));

   qosInfo.ts_info.psb = pAdapter->configuredPsb;

   switch (acType)
   {
   case WLANTL_AC_VO:
      qosInfo.ts_info.up = SME_QOS_WMM_UP_VO;
      /* Check if there is any valid configuration from framework */
      if (HDD_PSB_CFG_INVALID == pAdapter->configuredPsb)
      {
          qosInfo.ts_info.psb = ((WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask &
                                  SME_QOS_UAPSD_VO) ? 1 : 0;
      }
      qosInfo.ts_info.direction = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraDirAcVo;
      qosInfo.ts_info.tid = 255;
      qosInfo.mean_data_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMeanDataRateAcVo;
      qosInfo.min_phy_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMinPhyRateAcVo;
      qosInfo.min_service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSrvIntv;
      qosInfo.nominal_msdu_size = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraNomMsduSizeAcVo;
      qosInfo.surplus_bw_allowance = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraSbaAcVo;
      qosInfo.suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSuspIntv;
      break;
   case WLANTL_AC_VI:
      qosInfo.ts_info.up = SME_QOS_WMM_UP_VI;
      /* Check if there is any valid configuration from framework */
      if (HDD_PSB_CFG_INVALID == pAdapter->configuredPsb)
      {
          qosInfo.ts_info.psb = ((WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask &
                                  SME_QOS_UAPSD_VI) ? 1 : 0;
      }
      qosInfo.ts_info.direction = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraDirAcVi;
      qosInfo.ts_info.tid = 255;
      qosInfo.mean_data_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMeanDataRateAcVi;
      qosInfo.min_phy_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMinPhyRateAcVi;
      qosInfo.min_service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSrvIntv;
      qosInfo.nominal_msdu_size = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraNomMsduSizeAcVi;
      qosInfo.surplus_bw_allowance = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraSbaAcVi;
      qosInfo.suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSuspIntv;
      break;
   case WLANTL_AC_BE:
      qosInfo.ts_info.up = SME_QOS_WMM_UP_BE;
      /* Check if there is any valid configuration from framework */
      if (HDD_PSB_CFG_INVALID == pAdapter->configuredPsb)
      {
          qosInfo.ts_info.psb = ((WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask &
                                  SME_QOS_UAPSD_BE) ? 1 : 0;
      }
      qosInfo.ts_info.direction = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraDirAcBe;
      qosInfo.ts_info.tid = 255;
      qosInfo.mean_data_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMeanDataRateAcBe;
      qosInfo.min_phy_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMinPhyRateAcBe;
      qosInfo.min_service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSrvIntv;
      qosInfo.nominal_msdu_size = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraNomMsduSizeAcBe;
      qosInfo.surplus_bw_allowance = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraSbaAcBe;
      qosInfo.suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSuspIntv;
      break;
   case WLANTL_AC_BK:
      qosInfo.ts_info.up = SME_QOS_WMM_UP_BK;
      /* Check if there is any valid configuration from framework */
      if (HDD_PSB_CFG_INVALID == pAdapter->configuredPsb)
      {
          qosInfo.ts_info.psb = ((WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask &
                                  SME_QOS_UAPSD_BK) ? 1 : 0;
      }
      qosInfo.ts_info.direction = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraDirAcBk;
      qosInfo.ts_info.tid = 255;
      qosInfo.mean_data_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMeanDataRateAcBk;
      qosInfo.min_phy_rate = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraMinPhyRateAcBk;
      qosInfo.min_service_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSrvIntv;
      qosInfo.nominal_msdu_size = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraNomMsduSizeAcBk;
      qosInfo.surplus_bw_allowance = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraSbaAcBk;
      qosInfo.suspension_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSuspIntv;
      break;
   default:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Invalid AC %d", __func__, acType );
      return;
   }
#ifdef FEATURE_WLAN_ESE
   qosInfo.inactivity_interval = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraInactivityInterval;
#endif
   qosInfo.ts_info.burst_size_defn = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->burstSizeDefinition;

   switch ((WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->tsInfoAckPolicy)
   {
     case HDD_WLAN_WMM_TS_INFO_ACK_POLICY_NORMAL_ACK:
       qosInfo.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK;
       break;

     case HDD_WLAN_WMM_TS_INFO_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK:
       qosInfo.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK;
       break;

     default:
       // unknown
       qosInfo.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK;
   }

   if(qosInfo.ts_info.ack_policy == SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK)
   {
     if(!sme_QosIsTSInfoAckPolicyValid((tpAniSirGlobal)WLAN_HDD_GET_HAL_CTX(pAdapter), &qosInfo, pAdapter->sessionId))
     {
       qosInfo.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK;
     }
   }

   mutex_lock(&pHddCtx->wmmLock);
   list_add(&pQosContext->node, &pAdapter->hddWmmStatus.wmmContextList);
   mutex_unlock(&pHddCtx->wmmLock);

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   smeStatus = sme_QosSetupReq(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               pAdapter->sessionId,
                               &qosInfo,
                               hdd_wmm_sme_callback,
                               pQosContext,
                               qosInfo.ts_info.up,
                               &pQosContext->qosFlowId);

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: sme_QosSetupReq returned %d flowid %d",
             __func__, smeStatus, pQosContext->qosFlowId);

   // need to check the return values and act appropriately
   switch (smeStatus)
   {
   case SME_QOS_STATUS_SETUP_REQ_PENDING_RSP:
   case SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING:
      // setup is pending, so no more work to do now.
      // all further work will be done in hdd_wmm_sme_callback()
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Setup is pending, no further work",
                __func__);

      break;


   case SME_QOS_STATUS_SETUP_FAILURE_RSP:
      // we can't tell the difference between when a request fails because
      // AP rejected it versus when SME encountered an internal error

      // in either case SME won't ever reference this context so
      // free the record
      hdd_wmm_free_context(pQosContext);

      // fall through and start packets flowing
   case SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP:
      // no ACM in effect, no need to setup U-APSD
   case SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY:
      // no ACM in effect, U-APSD is desired but was already setup

      // for these cases everything is already setup so we can
      // signal TL that it has work to do
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Setup is complete, notify TL",
                __func__);

      pAc->wmmAcAccessAllowed = VOS_TRUE;
      pAc->wmmAcAccessGranted = VOS_TRUE;
      pAc->wmmAcAccessPending = VOS_FALSE;

      status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                     (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                     acType );

      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                    "%s: Failed to signal TL for AC=%d",
                    __func__, acType );
      }

      break;


   default:
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: unexpected SME Status=%d",
                 __func__, smeStatus );
      VOS_ASSERT(0);
   }
#endif

}

static void hdd_wmm_do_implicit_qos(struct work_struct *work)
{
    vos_ssr_protect(__func__);
    __hdd_wmm_do_implicit_qos( work );
    vos_ssr_unprotect(__func__);
}

/**============================================================================
  @brief hdd_wmm_init() - Function which will initialize the WMM configuation
  and status to an initial state.  The configuration can later be overwritten
  via application APIs

  @param pAdapter : [in]  pointer to Adapter context

  @return         : VOS_STATUS_SUCCESS if successful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_init ( hdd_adapter_t *pAdapter )
{
   sme_QosWmmUpType* hddWmmDscpToUpMap = pAdapter->hddWmmDscpToUpMap;
   v_U8_t dscp;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);

   // DSCP to User Priority Lookup Table
   for (dscp = 0; dscp <= WLAN_HDD_MAX_DSCP; dscp++)
   {
      hddWmmDscpToUpMap[dscp] = SME_QOS_WMM_UP_BE;
   }
   hddWmmDscpToUpMap[8]  = SME_QOS_WMM_UP_BK;
   hddWmmDscpToUpMap[16] = SME_QOS_WMM_UP_RESV;
   hddWmmDscpToUpMap[24] = SME_QOS_WMM_UP_EE;
   hddWmmDscpToUpMap[32] = SME_QOS_WMM_UP_CL;
   hddWmmDscpToUpMap[40] = SME_QOS_WMM_UP_VI;
   hddWmmDscpToUpMap[48] = SME_QOS_WMM_UP_VO;
   hddWmmDscpToUpMap[56] = SME_QOS_WMM_UP_NC;
   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_wmm_adapter_init() - Function which will initialize the WMM configuation
  and status to an initial state.  The configuration can later be overwritten
  via application APIs

  @param pAdapter : [in]  pointer to Adapter context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_init( hdd_adapter_t *pAdapter )
{
   hdd_wmm_ac_status_t *pAcStatus;
   WLANTL_ACEnumType acType;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);

   pAdapter->hddWmmStatus.wmmQap = VOS_FALSE;
   INIT_LIST_HEAD(&pAdapter->hddWmmStatus.wmmContextList);

   for (acType = 0; acType < WLANTL_MAX_AC; acType++)
   {
      pAcStatus = &pAdapter->hddWmmStatus.wmmAcStatus[acType];
      pAcStatus->wmmAcAccessRequired = VOS_FALSE;
      pAcStatus->wmmAcAccessNeeded = VOS_FALSE;
      pAcStatus->wmmAcAccessPending = VOS_FALSE;
      pAcStatus->wmmAcAccessFailed = VOS_FALSE;
      pAcStatus->wmmAcAccessGranted = VOS_FALSE;
      pAcStatus->wmmAcAccessAllowed = VOS_FALSE;
      pAcStatus->wmmAcTspecValid = VOS_FALSE;
      pAcStatus->wmmAcUapsdInfoValid = VOS_FALSE;
   }
   // Invalid value(0xff) to indicate psb not configured through framework initially.
   pAdapter->configuredPsb = HDD_PSB_CFG_INVALID;

   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_wmm_adapter_clear() - Function which will clear the WMM status
  for all the ACs

  @param pAdapter : [in]  pointer to Adapter context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_clear( hdd_adapter_t *pAdapter )
{
   hdd_wmm_ac_status_t *pAcStatus;
   WLANTL_ACEnumType acType;
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);
   for (acType = 0; acType < WLANTL_MAX_AC; acType++)
   {
      pAcStatus = &pAdapter->hddWmmStatus.wmmAcStatus[acType];
      pAcStatus->wmmAcAccessRequired = VOS_FALSE;
      pAcStatus->wmmAcAccessNeeded = VOS_FALSE;
      pAcStatus->wmmAcAccessPending = VOS_FALSE;
      pAcStatus->wmmAcAccessFailed = VOS_FALSE;
      pAcStatus->wmmAcAccessGranted = VOS_FALSE;
      pAcStatus->wmmAcAccessAllowed = VOS_FALSE;
      pAcStatus->wmmAcTspecValid = VOS_FALSE;
      pAcStatus->wmmAcUapsdInfoValid = VOS_FALSE;
   }
   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_wmm_close() - Function which will perform any necessary work to
  to clean up the WMM functionality prior to the kernel module unload

  @param pAdapter : [in]  pointer to adapter context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_close ( hdd_adapter_t* pAdapter )
{
   hdd_wmm_qos_context_t* pQosContext;
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return VOS_STATUS_E_FAILURE;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);

   // free any context records that we still have linked
   while (!list_empty(&pAdapter->hddWmmStatus.wmmContextList))
   {
      pQosContext = list_first_entry(&pAdapter->hddWmmStatus.wmmContextList,
                                     hdd_wmm_qos_context_t, node);
#ifdef FEATURE_WLAN_ESE
      hdd_wmm_disable_inactivity_timer(pQosContext);
#endif
   mutex_lock(&pHddCtx->wmmLock);
   if (pQosContext->handle == HDD_WMM_HANDLE_IMPLICIT
       && pQosContext->magic == HDD_WMM_CTX_MAGIC)
   {

      vos_flush_work(&pQosContext->wmmAcSetupImplicitQos);
   }
   mutex_unlock(&pHddCtx->wmmLock);

      hdd_wmm_free_context(pQosContext);
   }

   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_is_dhcp_packet() - Function which will check OS packet for
  DHCP packet

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @return         : VOS_TRUE if the OS packet is DHCP packet
                  : otherwise VOS_FALSE
  ===========================================================================*/
v_BOOL_t hdd_is_dhcp_packet(struct sk_buff *skb)
{
   if (*((u16*)((u8*)skb->data+34)) == DHCP_SOURCE_PORT ||
       *((u16*)((u8*)skb->data+34)) == DHCP_DESTINATION_PORT)
      return VOS_TRUE;

   return VOS_FALSE;
}

/**============================================================================
  @brief hdd_skb_is_eapol_or_wai_packet() - Function which will check OS packet
  for Eapol/Wapi packet

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @return         : VOS_TRUE if the OS packet is an Eapol or a Wapi packet
                  : otherwise VOS_FALSE
  ===========================================================================*/
v_BOOL_t hdd_skb_is_eapol_or_wai_packet(struct sk_buff *skb)
{
    if ((*((u16*)((u8*)skb->data+HDD_ETHERTYPE_802_1_X_FRAME_OFFSET))
         == vos_cpu_to_be16(HDD_ETHERTYPE_802_1_X))
#ifdef FEATURE_WLAN_WAPI
         || (*((u16*)((u8*)skb->data+HDD_ETHERTYPE_802_1_X_FRAME_OFFSET))
         == vos_cpu_to_be16(HDD_ETHERTYPE_WAI))
#endif
       )
       return VOS_TRUE;

    return VOS_FALSE;
}

/**============================================================================
  @brief hdd_wmm_classify_pkt() - Function which will classify an OS packet
  into a WMM AC based on either 802.1Q or DSCP

  @param pAdapter : [in]  pointer to adapter context
  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param pAcType  : [out] pointer to WMM AC type of OS packet

  @return         : None
  ===========================================================================*/
v_VOID_t hdd_wmm_classify_pkt ( hdd_adapter_t* pAdapter,
                                struct sk_buff *skb,
                                WLANTL_ACEnumType* pAcType,
                                sme_QosWmmUpType *pUserPri)
{
   unsigned char * pPkt;
   union generic_ethhdr *pHdr;
   struct iphdr *pIpHdr;
   unsigned char tos;
   unsigned char dscp;
   sme_QosWmmUpType userPri;
   WLANTL_ACEnumType acType;

   // this code is executed for every packet therefore
   // all debug code is kept conditional

#ifdef HDD_WMM_DEBUG
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);
#endif // HDD_WMM_DEBUG

   pPkt = skb->data;
   pHdr = (union generic_ethhdr *)pPkt;

#ifdef HDD_WMM_DEBUG
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: proto/length is 0x%04x",
             __func__, pHdr->eth_II.h_proto);
#endif // HDD_WMM_DEBUG

   if (HDD_WMM_CLASSIFICATION_DSCP == (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->PktClassificationBasis)
   {
      if (pHdr->eth_II.h_proto == htons(ETH_P_IP))
      {
         // case 1: Ethernet II IP packet
         pIpHdr = (struct iphdr *)&pPkt[sizeof(pHdr->eth_II)];
         tos = pIpHdr->tos;
#ifdef HDD_WMM_DEBUG
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: Ethernet II IP Packet, tos is %d",
                   __func__, tos);
#endif // HDD_WMM_DEBUG

      }
      else if ((ntohs(pHdr->eth_II.h_proto) < WLAN_MIN_PROTO) &&
               (pHdr->eth_8023.h_snap.dsap == WLAN_SNAP_DSAP) &&
               (pHdr->eth_8023.h_snap.ssap == WLAN_SNAP_SSAP) &&
               (pHdr->eth_8023.h_snap.ctrl == WLAN_SNAP_CTRL) &&
               (pHdr->eth_8023.h_proto == htons(ETH_P_IP)))
      {
         // case 2: 802.3 LLC/SNAP IP packet
         pIpHdr = (struct iphdr *)&pPkt[sizeof(pHdr->eth_8023)];
         tos = pIpHdr->tos;
#ifdef HDD_WMM_DEBUG
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: 802.3 LLC/SNAP IP Packet, tos is %d",
                   __func__, tos);
#endif // HDD_WMM_DEBUG
      }
      else if (pHdr->eth_II.h_proto == htons(ETH_P_8021Q))
      {
         // VLAN tagged

         if (pHdr->eth_IIv.h_vlan_encapsulated_proto == htons(ETH_P_IP))
         {
            // case 3: Ethernet II vlan-tagged IP packet
            pIpHdr = (struct iphdr *)&pPkt[sizeof(pHdr->eth_IIv)];
            tos = pIpHdr->tos;
#ifdef HDD_WMM_DEBUG
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                      "%s: Ethernet II VLAN tagged IP Packet, tos is %d",
                      __func__, tos);
#endif // HDD_WMM_DEBUG
         }
         else if ((ntohs(pHdr->eth_IIv.h_vlan_encapsulated_proto) < WLAN_MIN_PROTO) &&
                  (pHdr->eth_8023v.h_snap.dsap == WLAN_SNAP_DSAP) &&
                  (pHdr->eth_8023v.h_snap.ssap == WLAN_SNAP_SSAP) &&
                  (pHdr->eth_8023v.h_snap.ctrl == WLAN_SNAP_CTRL) &&
                  (pHdr->eth_8023v.h_proto == htons(ETH_P_IP)))
         {
            // case 4: 802.3 LLC/SNAP vlan-tagged IP packet
            pIpHdr = (struct iphdr *)&pPkt[sizeof(pHdr->eth_8023v)];
            tos = pIpHdr->tos;
#ifdef HDD_WMM_DEBUG
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                      "%s: 802.3 LLC/SNAP VLAN tagged IP Packet, tos is %d",
                      __func__, tos);
#endif // HDD_WMM_DEBUG
         }
         else
         {
            // default
#ifdef HDD_WMM_DEBUG
            VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_WARN,
                      "%s: VLAN tagged Unhandled Protocol, using default tos",
                      __func__);
#endif // HDD_WMM_DEBUG
            tos = 0;
         }
      }
      else
      {
          v_BOOL_t toggleArpBDRates =
                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->toggleArpBDRates;
          // default
#ifdef HDD_WMM_DEBUG
          VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_WARN,
                  "%s: Unhandled Protocol, using default tos",
                  __func__);
#endif // HDD_WMM_DEBUG
          //Give the highest priority to 802.1x packet
          if (pHdr->eth_II.h_proto == htons(HDD_ETHERTYPE_802_1_X))
              tos = 0xC0;
          else if (toggleArpBDRates &&
                   pHdr->eth_II.h_proto == htons(HDD_ETHERTYPE_ARP))
          {
              tos = TID3;
          }
          else
              tos = 0;
      }

      dscp = (tos>>2) & 0x3f;
      userPri = pAdapter->hddWmmDscpToUpMap[dscp];

#ifdef HDD_WMM_DEBUG
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                "%s: tos is %d, dscp is %d, up is %d",
                __func__, tos, dscp, userPri);
#endif // HDD_WMM_DEBUG

   }
   else if (HDD_WMM_CLASSIFICATION_802_1Q == (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->PktClassificationBasis)
   {
      if (pHdr->eth_IIv.h_vlan_proto == htons(ETH_P_8021Q))
      {
         // VLAN tagged
         userPri = (ntohs(pHdr->eth_IIv.h_vlan_TCI)>>13) & 0x7;
#ifdef HDD_WMM_DEBUG
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                   "%s: Tagged frame, UP is %d",
                   __func__, userPri);
#endif // HDD_WMM_DEBUG
      }
      else
      {
          // not VLAN tagged, use default
#ifdef HDD_WMM_DEBUG
          VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_WARN,
                  "%s: Untagged frame, using default UP",
                  __func__);
#endif // HDD_WMM_DEBUG
          //Give the highest priority to 802.1x packet
          if (pHdr->eth_II.h_proto == htons(HDD_ETHERTYPE_802_1_X))
              userPri = SME_QOS_WMM_UP_VO;
          else
              userPri = SME_QOS_WMM_UP_BE;
      }
   }
   else
   {
      // default
#ifdef HDD_WMM_DEBUG
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Unknown classification scheme, using default UP",
                __func__);
#endif // HDD_WMM_DEBUG
      userPri = SME_QOS_WMM_UP_BE;
   }

   acType = hddWmmUpToAcMap[userPri];

#ifdef HDD_WMM_DEBUG
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: UP is %d, AC is %d",
             __func__, userPri, acType);
#endif // HDD_WMM_DEBUG

   *pUserPri = userPri;
   *pAcType = acType;

   return;
}

/**============================================================================
  @brief hdd_hostapd_select_quueue() - Function which will classify the packet
         according to linux qdisc expectation.


  @param dev      : [in]  pointer to net_device structure
  @param skb      : [in]  pointer to os packet

  @return         : Qdisc queue index
  ===========================================================================*/
v_U16_t hdd_hostapd_select_queue(struct net_device * dev, struct sk_buff *skb
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0))
                                 , void *accel_priv
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
                                 , select_queue_fallback_t fallbac
#endif
)
{
   WLANTL_ACEnumType ac;
   sme_QosWmmUpType up = SME_QOS_WMM_UP_BE;
   v_USHORT_t queueIndex;
   v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)netdev_priv(dev);
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   v_U8_t STAId;
   v_CONTEXT_t pVosContext = ( WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
   ptSapContext pSapCtx = NULL;
   int status = 0;

   status = wlan_hdd_validate_context(pHddCtx);
   if (status !=0 )
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  FL("called during WDReset/unload"));
       skb->priority = SME_QOS_WMM_UP_BE;
       return HDD_LINUX_AC_BE;
   }

   pSapCtx = VOS_GET_SAP_CB(pVosContext);
   if(pSapCtx == NULL){
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 FL("psapCtx is NULL"));
       STAId = HDD_WLAN_INVALID_STA_ID;
       goto done;
   }
   /*Get the Station ID*/
   STAId = hdd_sta_id_find_from_mac_addr(pAdapter, pDestMacAddress);
   if (STAId == HDD_WLAN_INVALID_STA_ID) {
       VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_INFO,
                 "%s: Failed to find right station", __func__);
       goto done;
   }

   spin_lock_bh( &pSapCtx->staInfo_lock );
   if (FALSE == vos_is_macaddr_equal(&pSapCtx->aStaInfo[STAId].macAddrSTA, pDestMacAddress))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_INFO,
                   "%s: Station MAC address does not matching", __func__);

      STAId = HDD_WLAN_INVALID_STA_ID;
      goto release_lock;
   }
   if (pSapCtx->aStaInfo[STAId].isUsed && pSapCtx->aStaInfo[STAId].isQosEnabled && (HDD_WMM_USER_MODE_NO_QOS != pHddCtx->cfg_ini->WmmMode))
   {
      /* Get the user priority from IP header & corresponding AC */
      hdd_wmm_classify_pkt (pAdapter, skb, &ac, &up);
      //If 3/4th of Tx queue is used then place the DHCP packet in VOICE AC queue
      if (pSapCtx->aStaInfo[STAId].vosLowResource && hdd_is_dhcp_packet(skb))
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_WARN,
                    "%s: Making priority of DHCP packet as VOICE", __func__);
         up = SME_QOS_WMM_UP_VO;
         ac = hddWmmUpToAcMap[up];
      }
   }

release_lock:
    spin_unlock_bh( &pSapCtx->staInfo_lock );
done:
   skb->priority = up;
   if(skb->priority < SME_QOS_WMM_UP_MAX)
         queueIndex = hddLinuxUpToAcMap[skb->priority];
   else
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                 "%s: up=%d is going beyond max value", __func__, up);
      queueIndex = hddLinuxUpToAcMap[SME_QOS_WMM_UP_BE];
   }

   return queueIndex;
}

/**============================================================================
  @brief hdd_wmm_select_quueue() - Function which will classify the packet
         according to linux qdisc expectation.


  @param dev      : [in]  pointer to net_device structure
  @param skb      : [in]  pointer to os packet

  @return         : Qdisc queue index
  ===========================================================================*/
v_U16_t hdd_wmm_select_queue(struct net_device * dev, struct sk_buff *skb)
{
   WLANTL_ACEnumType ac;
   sme_QosWmmUpType up = SME_QOS_WMM_UP_BE;
   v_USHORT_t queueIndex;
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   int status = 0;

   status = wlan_hdd_validate_context(pHddCtx);
   if (status !=0) {
       skb->priority = SME_QOS_WMM_UP_BE;
       return HDD_LINUX_AC_BE;
   }

   /*Get the Station ID*/
   if (WLAN_HDD_IBSS == pAdapter->device_mode)
   {
       v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;
       v_U8_t STAId;

       STAId = hdd_sta_id_find_from_mac_addr(pAdapter, pDestMacAddress);
       if ((STAId == HDD_WLAN_INVALID_STA_ID) &&
            !vos_is_macaddr_broadcast( pDestMacAddress ) &&
            !vos_is_macaddr_group(pDestMacAddress))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "%s: Failed to find right station pDestMacAddress: "
                     MAC_ADDRESS_STR , __func__,
                     MAC_ADDR_ARRAY(pDestMacAddress->bytes));
           goto done;
       }
   }
   /* All traffic will get equal opportuniy to transmit data frames. */
   /* Get the user priority from IP header & corresponding AC */
   hdd_wmm_classify_pkt (pAdapter, skb, &ac, &up);

   /* If 3/4th of BE AC Tx queue is full,
    * then place the DHCP packet in VOICE AC queue.
    * Doing this for IBSS alone, since for STA interface
    * types, these packets will be queued to the new queue.
    */
   if ((WLAN_HDD_IBSS == pAdapter->device_mode) &&
       pAdapter->isVosLowResource && hdd_is_dhcp_packet(skb))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_WARN,
                "%s: BestEffort Tx Queue is 3/4th full"
                " Make DHCP packet's pri as VO", __func__);
      up = SME_QOS_WMM_UP_VO;
      ac = hddWmmUpToAcMap[up];
   }

done:
   skb->priority = up;
   if(skb->priority < SME_QOS_WMM_UP_MAX)
         queueIndex = hddLinuxUpToAcMap[skb->priority];
   else
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
                 "%s: up=%d is going beyond max value", __func__, up);
      queueIndex = hddLinuxUpToAcMap[SME_QOS_WMM_UP_BE];
   }

   if ((WLAN_HDD_IBSS != pAdapter->device_mode) &&
       (hdd_is_dhcp_packet(skb) ||
        hdd_skb_is_eapol_or_wai_packet(skb)))
   {
       /* If the packet is a DHCP packet or a Eapol packet or
        * a Wapi packet, then queue it to the new queue for
        * STA interfaces alone.
        */
       queueIndex = WLANTL_AC_HIGH_PRIO;
       VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                 "%s: up=%d QIndex:%d", __func__, up, queueIndex);
   }

   return queueIndex;
}

/**==========================================================================
  @brief hdd_wmm_acquire_access_required() - Function which will determine
  acquire admittance for a WMM AC is required or not based on psb configuration
  done in framework

  @param pAdapter : [in]  pointer to adapter structure

  @param acType  : [in]  WMM AC type of OS packet

  @return        : void
  ===========================================================================*/
void hdd_wmm_acquire_access_required(hdd_adapter_t *pAdapter,
                                     WLANTL_ACEnumType acType)
{
/* Each bit in the LSB nibble indicates 1 AC.
 * Clearing the particular bit in LSB nibble to indicate
 * access required
 */
   switch(acType)
   {
   case WLANTL_AC_BK:
      pAdapter->psbChanged &= ~SME_QOS_UAPSD_CFG_BK_CHANGED_MASK; /* clear first bit */
      break;
   case WLANTL_AC_BE:
      pAdapter->psbChanged &= ~SME_QOS_UAPSD_CFG_BE_CHANGED_MASK; /* clear second bit */
      break;
   case WLANTL_AC_VI:
      pAdapter->psbChanged &= ~SME_QOS_UAPSD_CFG_VI_CHANGED_MASK; /* clear third bit */
      break;
   case WLANTL_AC_VO:
      pAdapter->psbChanged &= ~SME_QOS_UAPSD_CFG_VO_CHANGED_MASK; /* clear fourth bit */
      break;
   default:
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
               "%s: Invalid AC Type", __func__);
     break;
   }
}

/**============================================================================
  @brief hdd_wmm_acquire_access() - Function which will attempt to acquire
  admittance for a WMM AC

  @param pAdapter : [in]  pointer to adapter context
  @param acType   : [in]  WMM AC type of OS packet
  @param pGranted : [out] pointer to boolean flag when indicates if access
                          has been granted or not

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_acquire_access( hdd_adapter_t* pAdapter,
                                   WLANTL_ACEnumType acType,
                                   v_BOOL_t * pGranted )
{
   hdd_wmm_qos_context_t *pQosContext;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered for AC %d", __func__, acType);

   if (!hdd_wmm_is_active(pAdapter) || !(WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->bImplicitQosEnabled)
   {
      // either we don't want QoS or the AP doesn't support QoS
      // or we don't want to do implicit QoS
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: QoS not configured on both ends ", __func__);

      pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessAllowed = VOS_TRUE;
      *pGranted = VOS_TRUE;
      return VOS_STATUS_SUCCESS;
   }

   // do we already have an implicit QoS request pending for this AC?
   if ((pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessNeeded) ||
       (pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessPending))
   {
      // request already pending so we need to wait for that response
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Implicit QoS for TL AC %d already scheduled",
                __func__, acType);

      *pGranted = VOS_FALSE;
      return VOS_STATUS_SUCCESS;
   }

   // did we already fail to establish implicit QoS for this AC?
   // (if so, access should have been granted when the failure was handled)
   if (pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessFailed)
   {
      // request previously failed
      // allow access, but we'll be downgraded
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Implicit QoS for TL AC %d previously failed",
                __func__, acType);

      pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessAllowed = VOS_TRUE;
      *pGranted = VOS_TRUE;
      return VOS_STATUS_SUCCESS;
   }

   // we need to establish implicit QoS
   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: Need to schedule implicit QoS for TL AC %d, pAdapter is %p",
             __func__, acType, pAdapter);

   pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessNeeded = VOS_TRUE;

   pQosContext = kmalloc(sizeof(*pQosContext), GFP_ATOMIC);
   if (NULL == pQosContext)
   {
      // no memory for QoS context.  Nothing we can do but let data flow
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Unable to allocate context", __func__);
      pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcAccessAllowed = VOS_TRUE;
      *pGranted = VOS_TRUE;
      return VOS_STATUS_SUCCESS;
   }

   pQosContext->acType = acType;
   pQosContext->pAdapter = pAdapter;
   pQosContext->qosFlowId = 0;
   pQosContext->handle = HDD_WMM_HANDLE_IMPLICIT;
   pQosContext->magic = HDD_WMM_CTX_MAGIC;
   vos_init_work(&pQosContext->wmmAcSetupImplicitQos,
             hdd_wmm_do_implicit_qos);

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: Scheduling work for AC %d, context %p",
             __func__, acType, pQosContext);

   schedule_work(&pQosContext->wmmAcSetupImplicitQos);

   // caller will need to wait until the work takes place and
   // TSPEC negotiation completes
   *pGranted = VOS_FALSE;
   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_wmm_assoc() - Function which will handle the housekeeping
  required by WMM when association takes place

  @param pAdapter : [in]  pointer to adapter context
  @param pRoamInfo: [in]  pointer to roam information
  @param eBssType : [in]  type of BSS

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_assoc( hdd_adapter_t* pAdapter,
                          tCsrRoamInfo *pRoamInfo,
                          eCsrRoamBssType eBssType )
{
   tANI_U8 uapsdMask;
   VOS_STATUS status;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   // when we associate we need to notify TL if it needs to enable
   // UAPSD for any access categories

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);

   if (pRoamInfo->fReassocReq)
   {
      // when we reassociate we should continue to use whatever
      // parameters were previously established.  if we are
      // reassociating due to a U-APSD change for a particular
      // Access Category, then the change will be communicated
      // to HDD via the QoS callback associated with the given
      // flow, and U-APSD parameters will be updated there

      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: Reassoc so no work, Exiting", __func__);

      return VOS_STATUS_SUCCESS;
   }

   // get the negotiated UAPSD Mask
   uapsdMask = pRoamInfo->u.pConnectedProfile->modifyProfileFields.uapsd_mask;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: U-APSD mask is 0x%02x", __func__, (int) uapsdMask);

   if (uapsdMask & HDD_AC_VO)
   {
      status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        WLANTL_AC_VO,
                                        7,
                                        7,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSrvIntv,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSuspIntv,
                                        WLANTL_BI_DIR );

      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ));
   }

   if (uapsdMask & HDD_AC_VI)
   {
      status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        WLANTL_AC_VI,
                                        5,
                                        5,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSrvIntv,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSuspIntv,
                                        WLANTL_BI_DIR );

      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ));
   }

   if (uapsdMask & HDD_AC_BK)
   {
      status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        WLANTL_AC_BK,
                                        2,
                                        2,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSrvIntv,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSuspIntv,
                                        WLANTL_BI_DIR );

      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ));
   }

   if (uapsdMask & HDD_AC_BE)
   {
      status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                                        WLANTL_AC_BE,
                                        3,
                                        3,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSrvIntv,
                                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSuspIntv,
                                        WLANTL_BI_DIR );

      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ));
   }

   status = sme_UpdateDSCPtoUPMapping(pHddCtx->hHal,
       pAdapter->hddWmmDscpToUpMap, pAdapter->sessionId);

   if (!VOS_IS_STATUS_SUCCESS( status ))
   {
       hdd_wmm_init( pAdapter );
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Exiting", __func__);

   return VOS_STATUS_SUCCESS;
}



static const v_U8_t acmMaskBit[WLANTL_MAX_AC] =
   {
      0x4, /* WLANTL_AC_BK */
      0x8, /* WLANTL_AC_BE */
      0x2, /* WLANTL_AC_VI */
      0x1  /* WLANTL_AC_VO */
   };

/**============================================================================
  @brief hdd_wmm_connect() - Function which will handle the housekeeping
  required by WMM when a connection is established

  @param pAdapter : [in]  pointer to adapter context
  @param pRoamInfo: [in]  pointer to roam information
  @param eBssType : [in]  type of BSS

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_connect( hdd_adapter_t* pAdapter,
                            tCsrRoamInfo *pRoamInfo,
                            eCsrRoamBssType eBssType )
{
   int ac;
   v_BOOL_t qap;
   v_BOOL_t qosConnection;
   v_U8_t acmMask;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered", __func__);

   if ((eCSR_BSS_TYPE_INFRASTRUCTURE == eBssType) &&
       pRoamInfo &&
       pRoamInfo->u.pConnectedProfile)
   {
      qap = pRoamInfo->u.pConnectedProfile->qap;
      qosConnection = pRoamInfo->u.pConnectedProfile->qosConnection;
      acmMask = pRoamInfo->u.pConnectedProfile->acm_mask;
   }
   else
   {
      /* TODO: if a non-qos IBSS peer joins the group make qap and qosConnection false.
       */
      qap = VOS_TRUE;
      qosConnection = VOS_TRUE;
      acmMask = 0x0;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: qap is %d, qosConnection is %d, acmMask is 0x%x",
             __func__, qap, qosConnection, acmMask);

   pAdapter->hddWmmStatus.wmmQap = qap;
   pAdapter->hddWmmStatus.wmmQosConnection = qosConnection;

   for (ac = 0; ac < WLANTL_MAX_AC; ac++)
   {
      if (qap &&
          qosConnection &&
          (acmMask & acmMaskBit[ac]))
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: ac %d on",
                   __func__, ac);

         // admission is required
         pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessRequired = VOS_TRUE;
         //Mark wmmAcAccessAllowed as True if implicit Qos is disabled as there
         //is no need to hold packets in queue during hdd_tx_fetch_packet_cbk
         if (!(WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->bImplicitQosEnabled)
              pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed =
                                                                     VOS_TRUE;
         else
              pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed =
                                                                    VOS_FALSE;
         pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessGranted = VOS_FALSE;

         /* Making TSPEC invalid here so downgrading can be happen while roaming
          * It is expected this will be SET in hdd_wmm_sme_callback,once sme is
          * done with the AddTspec.Here we avoid 11r and ccx based association.
            This change is done only when reassoc to different AP.
          */
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      FL( "fReassocReq = %d"
#if defined (FEATURE_WLAN_ESE)
                          "isESEAssoc = %d"
#endif
#if defined (WLAN_FEATURE_VOWIFI_11R)
                          "is11rAssoc = %d"
#endif
                        ),
                        pRoamInfo->fReassocReq
#if defined (FEATURE_WLAN_ESE)
                        ,pRoamInfo->isESEAssoc
#endif
#if defined (WLAN_FEATURE_VOWIFI_11R)
                        ,pRoamInfo->is11rAssoc
#endif
                  );

         if ( !pRoamInfo->fReassocReq
#if defined (WLAN_FEATURE_VOWIFI_11R)
            &&
            !pRoamInfo->is11rAssoc
#endif
#if defined (FEATURE_WLAN_ESE)
            &&
            !pRoamInfo->isESEAssoc
#endif
            )
         {
            pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcTspecValid = VOS_FALSE;
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: ac %d off",
                   __func__, ac);
         // admission is not required so access is allowed
         pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessRequired = VOS_FALSE;
         pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed = VOS_TRUE;
      }

   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Exiting", __func__);

   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_wmm_get_uapsd_mask() - Function which will calculate the
  initial value of the UAPSD mask based upon the device configuration

  @param pAdapter  : [in]  pointer to adapter context
  @param pUapsdMask: [in]  pointer to where the UAPSD Mask is to be stored

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_get_uapsd_mask( hdd_adapter_t* pAdapter,
                                   tANI_U8 *pUapsdMask )
{
   tANI_U8 uapsdMask;

   if (HDD_WMM_USER_MODE_NO_QOS == (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->WmmMode)
   {
      // no QOS then no UAPSD
      uapsdMask = 0;
   }
   else
   {
      // start with the default mask
      uapsdMask = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask;

      // disable UAPSD for any ACs with a 0 Service Interval
      if( (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdVoSrvIntv == 0 )
      {
         uapsdMask &= ~HDD_AC_VO;
      }

      if( (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdViSrvIntv == 0 )
      {
         uapsdMask &= ~HDD_AC_VI;
      }

      if( (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBkSrvIntv == 0 )
      {
         uapsdMask &= ~HDD_AC_BK;
      }

      if( (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->InfraUapsdBeSrvIntv == 0 )
      {
         uapsdMask &= ~HDD_AC_BE;
      }
   }

   // return calculated mask
   *pUapsdMask = uapsdMask;
   return VOS_STATUS_SUCCESS;
}


/**============================================================================
  @brief hdd_wmm_is_active() - Function which will determine if WMM is
  active on the current connection

  @param pAdapter  : [in]  pointer to adapter context

  @return         : VOS_TRUE if WMM is enabled
                  : VOS_FALSE if WMM is not enabled
  ===========================================================================*/
v_BOOL_t hdd_wmm_is_active( hdd_adapter_t* pAdapter )
{
   if ((!pAdapter->hddWmmStatus.wmmQosConnection) ||
       (!pAdapter->hddWmmStatus.wmmQap))
   {
      return VOS_FALSE;
   }
   else
   {
      return VOS_TRUE;
   }
}

/**============================================================================
  @brief hdd_wmm_addts() - Function which will add a traffic spec at the
  request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS
  @param pTspec    : [in]  pointer to the traffic spec

  @return          : HDD_WLAN_WMM_STATUS_*
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_addts( hdd_adapter_t* pAdapter,
                                     v_U32_t handle,
                                     sme_QosWmmTspecInfo* pTspec )
{
   hdd_wmm_qos_context_t *pQosContext;
   hdd_wlan_wmm_status_e status = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS ;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   sme_QosStatusType smeStatus;
#endif
   v_BOOL_t found = VOS_FALSE;
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered with handle 0x%x", __func__, handle);

   // see if a context already exists with the given handle
   mutex_lock(&pHddCtx->wmmLock);
   list_for_each_entry(pQosContext,
                       &pAdapter->hddWmmStatus.wmmContextList,
                       node)
   {
      if (pQosContext->handle == handle)
      {
         found = VOS_TRUE;
         break;
      }
   }
   mutex_unlock(&pHddCtx->wmmLock);
   if (found)
   {
      // record with that handle already exists
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Record already exists with handle 0x%x",
                __func__, handle);

      /* Application is trying to modify some of the Tspec params. Allow it */
      smeStatus = sme_QosModifyReq(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  pTspec,
                                  pQosContext->qosFlowId);

      // need to check the return value and act appropriately
      switch (smeStatus)
      {
        case SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP:
          status = HDD_WLAN_WMM_STATUS_MODIFY_PENDING;
          break;
        case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP:
          status = HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_NO_UAPSD;
          break;
        case SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY:
          status = HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_UAPSD_EXISTING;
          break;
        case SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP:
          status = HDD_WLAN_WMM_STATUS_MODIFY_FAILED_BAD_PARAM;
          break;
        case SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP:
          status = HDD_WLAN_WMM_STATUS_MODIFY_FAILED;
          break;
        case SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP:
          status = HDD_WLAN_WMM_STATUS_SETUP_FAILED_NO_WMM;
          break;
        default:
          // we didn't get back one of the SME_QOS_STATUS_MODIFY_* status codes
          VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                     "%s: unexpected SME Status=%d", __func__, smeStatus );
          VOS_ASSERT(0);
          return HDD_WLAN_WMM_STATUS_MODIFY_FAILED;
      }

      mutex_lock(&pHddCtx->wmmLock);
      if (pQosContext->magic == HDD_WMM_CTX_MAGIC)
      {
          pQosContext->lastStatus = status;
      }
      mutex_unlock(&pHddCtx->wmmLock);
      return status;
   }

   pQosContext = kmalloc(sizeof(*pQosContext), GFP_KERNEL);
   if (NULL == pQosContext)
   {
      // no memory for QoS context.  Nothing we can do
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: Unable to allocate QoS context", __func__);
      return HDD_WLAN_WMM_STATUS_INTERNAL_FAILURE;
   }

   // we assume the tspec has already been validated by the caller

   pQosContext->handle = handle;
   if (pTspec->ts_info.up < HDD_WMM_UP_TO_AC_MAP_SIZE)
      pQosContext->acType = hddWmmUpToAcMap[pTspec->ts_info.up];
   else {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                "%s: ts_info.up (%d) larger than max value (%d), "
                "use default acType (%d)",
                __func__, pTspec->ts_info.up,
                HDD_WMM_UP_TO_AC_MAP_SIZE - 1, hddWmmUpToAcMap[0]);
      pQosContext->acType = hddWmmUpToAcMap[0];
   }

   pQosContext->pAdapter = pAdapter;
   pQosContext->qosFlowId = 0;

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: Setting up QoS, context %p",
             __func__, pQosContext);

   mutex_lock(&pHddCtx->wmmLock);
   pQosContext->magic = HDD_WMM_CTX_MAGIC;
   list_add(&pQosContext->node, &pAdapter->hddWmmStatus.wmmContextList);
   mutex_unlock(&pHddCtx->wmmLock);

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   smeStatus = sme_QosSetupReq(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               pAdapter->sessionId,
                               pTspec,
                               hdd_wmm_sme_callback,
                               pQosContext,
                               pTspec->ts_info.up,
                               &pQosContext->qosFlowId);

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO,
             "%s: sme_QosSetupReq returned %d flowid %d",
             __func__, smeStatus, pQosContext->qosFlowId);

   // need to check the return value and act appropriately
   switch (smeStatus)
   {
   case SME_QOS_STATUS_SETUP_REQ_PENDING_RSP:
      status = HDD_WLAN_WMM_STATUS_SETUP_PENDING;
      break;
   case SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP:
      status = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_NO_UAPSD;
      break;
   case SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY:
      status = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_UAPSD_EXISTING;
      break;
   case SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING:
      status = HDD_WLAN_WMM_STATUS_SETUP_PENDING;
      break;
   case SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP:
      hdd_wmm_free_context(pQosContext);
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
   case SME_QOS_STATUS_SETUP_FAILURE_RSP:
      // we can't tell the difference between when a request fails because
      // AP rejected it versus when SME encounterd an internal error
      hdd_wmm_free_context(pQosContext);
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED;
   case SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP:
      hdd_wmm_free_context(pQosContext);
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED_NO_WMM;
   default:
      // we didn't get back one of the SME_QOS_STATUS_SETUP_* status codes
      hdd_wmm_free_context(pQosContext);
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: unexpected SME Status=%d", __func__, smeStatus );
      VOS_ASSERT(0);
      return HDD_WLAN_WMM_STATUS_SETUP_FAILED;
   }
#endif

   // we were successful, save the status
   mutex_lock(&pHddCtx->wmmLock);
   if (pQosContext->magic == HDD_WMM_CTX_MAGIC)
   {
         pQosContext->lastStatus = status;
   }
   mutex_unlock(&pHddCtx->wmmLock);

   return status;
}

/**============================================================================
  @brief hdd_wmm_delts() - Function which will delete a traffic spec at the
  request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS

  @return          : HDD_WLAN_WMM_STATUS_*
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_delts( hdd_adapter_t* pAdapter,
                                     v_U32_t handle )
{
   hdd_wmm_qos_context_t *pQosContext;
   v_BOOL_t found = VOS_FALSE;
   WLANTL_ACEnumType acType = 0;
   v_U32_t qosFlowId = 0;
   hdd_wlan_wmm_status_e status = HDD_WLAN_WMM_STATUS_SETUP_SUCCESS ;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   sme_QosStatusType smeStatus;
#endif
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
      return HDD_WLAN_WMM_STATUS_RELEASE_FAILED;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return HDD_WLAN_WMM_STATUS_RELEASE_FAILED;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered with handle 0x%x", __func__, handle);

   // locate the context with the given handle
   mutex_lock(&pHddCtx->wmmLock);
   list_for_each_entry(pQosContext,
                       &pAdapter->hddWmmStatus.wmmContextList,
                       node)
   {
      if (pQosContext->handle == handle)
      {
         found = VOS_TRUE;
         acType = pQosContext->acType;
         qosFlowId = pQosContext->qosFlowId;
         break;
      }
   }
   mutex_unlock(&pHddCtx->wmmLock);

   if (VOS_FALSE == found)
   {
      // we didn't find the handle
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                "%s: handle 0x%x not found", __func__, handle);
      return HDD_WLAN_WMM_STATUS_RELEASE_FAILED_BAD_PARAM;
   }


   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: found handle 0x%x, flow %d, AC %d, context %p",
             __func__, handle, qosFlowId, acType, pQosContext);

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   smeStatus = sme_QosReleaseReq( WLAN_HDD_GET_HAL_CTX(pAdapter), qosFlowId );

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: SME flow %d released, SME status %d",
             __func__, qosFlowId, smeStatus);

   switch(smeStatus)
   {
   case SME_QOS_STATUS_RELEASE_SUCCESS_RSP:
      // this flow is the only one on that AC, so go ahead and update
      // our TSPEC state for the AC
      pAdapter->hddWmmStatus.wmmAcStatus[acType].wmmAcTspecValid = VOS_FALSE;

      // need to tell TL to stop trigger timer, etc
      hdd_wmm_disable_tl_uapsd(pQosContext);

#ifdef FEATURE_WLAN_ESE
      // disable the inactivity timer
      hdd_wmm_disable_inactivity_timer(pQosContext);
#endif
      // we are done with this context
      hdd_wmm_free_context(pQosContext);

      // SME must not fire any more callbacks for this flow since the context
      // is no longer valid

      return HDD_WLAN_WMM_STATUS_RELEASE_SUCCESS;

   case SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP:
      // do nothing as we will get a response from SME
      status = HDD_WLAN_WMM_STATUS_RELEASE_PENDING;
      break;

   case SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP:
      // nothing we can do with the existing flow except leave it
      status = HDD_WLAN_WMM_STATUS_RELEASE_FAILED_BAD_PARAM;
      break;

   case SME_QOS_STATUS_RELEASE_FAILURE_RSP:
      // nothing we can do with the existing flow except leave it
      status = HDD_WLAN_WMM_STATUS_RELEASE_FAILED;

   default:
      // we didn't get back one of the SME_QOS_STATUS_RELEASE_* status codes
      VOS_TRACE( VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                 "%s: unexpected SME Status=%d", __func__, smeStatus );
      VOS_ASSERT(0);
      status = HDD_WLAN_WMM_STATUS_RELEASE_FAILED;
   }

#endif
   mutex_lock(&pHddCtx->wmmLock);
   if (pQosContext->magic == HDD_WMM_CTX_MAGIC)
   {
         pQosContext->lastStatus = status;
   }
   mutex_unlock(&pHddCtx->wmmLock);
   return status;
}

/**============================================================================
  @brief hdd_wmm_checkts() - Function which will return the status of a traffic
  spec at the request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS

  @return          : HDD_WLAN_WMM_STATUS_*
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_checkts( hdd_adapter_t* pAdapter,
                                       v_U32_t handle )
{
   hdd_wmm_qos_context_t *pQosContext;
   hdd_wlan_wmm_status_e status = HDD_WLAN_WMM_STATUS_LOST;
   v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
   hdd_context_t *pHddCtx;

   if (NULL == pVosContext)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("pVosContext is NULL"));
      return HDD_WLAN_WMM_STATUS_LOST;
   }

   pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_ERROR,
                   FL("HddCtx is NULL"));
      return HDD_WLAN_WMM_STATUS_LOST;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
             "%s: Entered with handle 0x%x", __func__, handle);

   // locate the context with the given handle
   mutex_lock(&pHddCtx->wmmLock);
   list_for_each_entry(pQosContext,
                       &pAdapter->hddWmmStatus.wmmContextList,
                       node)
   {
      if (pQosContext->handle == handle)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, WMM_TRACE_LEVEL_INFO_LOW,
                   "%s: found handle 0x%x, context %p",
                   __func__, handle, pQosContext);

         status = pQosContext->lastStatus;
         break;
      }
   }
   mutex_unlock(&pHddCtx->wmmLock);
   return status;
}
