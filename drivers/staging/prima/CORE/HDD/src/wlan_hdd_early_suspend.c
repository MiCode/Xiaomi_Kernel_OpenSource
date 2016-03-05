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

/**=============================================================================
*     wlan_hdd_early_suspend.c
*
*     \brief      power management functions
*
*     Description
*                 Copyright 2009 (c) Qualcomm, Incorporated.
*                 All Rights Reserved.
*                 Qualcomm Confidential and Proprietary.
*
==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
*   Include files
* ----------------------------------------------------------------------------*/

#include <linux/pm.h>
#include <linux/wait.h>
#include <wlan_hdd_includes.h>
#include <wlan_qct_driver.h>
#include <linux/wakelock.h>

#include "halTypes.h"
#include "sme_Api.h"
#include <vos_api.h>
#include "vos_power.h"
#include <vos_sched.h>
#include <macInitApi.h>
#include <wlan_qct_sys.h>
#include <wlan_btc_svc.h>
#include <wlan_nlink_common.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_dev_pwr.h>
#include <wlan_nlink_srv.h>
#include <wlan_hdd_misc.h>
#include "wlan_qct_wda.h"

#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>
#include "cfgApi.h"

#ifdef WLAN_BTAMP_FEATURE
#include "bapApi.h"
#include "bap_hdd_main.h"
#include "bap_hdd_misc.h"
#endif

#include <linux/wcnss_wlan.h>
#include <linux/inetdevice.h>
#include <wlan_hdd_cfg.h>
#include <wlan_hdd_cfg80211.h>
#include <net/addrconf.h>
/**-----------------------------------------------------------------------------
*   Preprocessor definitions and constants
* ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
*   Type declarations
* ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
*   Function and variables declarations
* ----------------------------------------------------------------------------*/
#include "wlan_hdd_power.h"
#include "wlan_hdd_packet_filtering.h"

#define HDD_SSR_BRING_UP_TIME 180000
#define NS_DEFAULT_SLOT_INDEX 4
#define NS_EXTENDED_SLOT_INDEX 18

static eHalStatus g_full_pwr_status;
static eHalStatus g_standby_status;

extern VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx);
extern VOS_STATUS vos_chipExitDeepSleepVREGHandler(
   vos_call_status_type* status,
   vos_power_cb_type callback,
   v_PVOID_t user_data);
extern void hdd_wlan_initial_scan(hdd_context_t *pHddCtx);

extern struct notifier_block hdd_netdev_notifier;
extern tVOS_CON_MODE hdd_get_conparam ( void );

static struct timer_list ssr_timer;
static bool ssr_timer_started;

//Callback invoked by PMC to report status of standby request
void hdd_suspend_standby_cbk (void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Standby status = %d", __func__, status);
   g_standby_status = status; 

   if(eHAL_STATUS_SUCCESS == status)
   {
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_STANDBY;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed",__func__);
   }

   complete(&pHddCtx->standby_comp_var);
}

//Callback invoked by PMC to report status of full power request
void hdd_suspend_full_pwr_callback(void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Full Power status = %d", __func__, status);
   g_full_pwr_status = status;

   if(eHAL_STATUS_SUCCESS == status)
   {
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
   }

   complete(&pHddCtx->full_pwr_comp_var);
}

eHalStatus hdd_exit_standby(hdd_context_t *pHddCtx)
{  
    eHalStatus status = VOS_STATUS_SUCCESS;
    long ret;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed from standby",__func__);
    INIT_COMPLETION(pHddCtx->full_pwr_comp_var);

   g_full_pwr_status = eHAL_STATUS_FAILURE;
    status = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback, pHddCtx,
      eSME_FULL_PWR_NEEDED_BY_HDD);

   if(status == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      ret = wait_for_completion_interruptible_timeout(
                 &pHddCtx->full_pwr_comp_var,
                  msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (0 >= ret)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s:wait on full_pwr_comp_var failed %ld",
                 __func__, ret);
      }
      status = g_full_pwr_status;
      if(g_full_pwr_status != eHAL_STATUS_SUCCESS)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
         VOS_ASSERT(0);
         goto failure;
      }
    }
    else if(status != eHAL_STATUS_SUCCESS)
    {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed - status %d",
         __func__, status);
      VOS_ASSERT(0);
      goto failure;
    }
    else
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;

failure:
    //No blocking to reduce latency. No other device should be depending on WLAN
    //to finish resume and WLAN won't be instantly on after resume
    return status;
}


//Helper routine to put the chip into standby
VOS_STATUS hdd_enter_standby(hdd_context_t *pHddCtx)
{
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   long ret;

   //Disable IMPS/BMPS as we do not want the device to enter any power
   //save mode on its own during suspend sequence
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   //Note we do not disable queues unnecessarily. Queues should already be disabled
   //if STA is disconnected or the queue will be disabled as and when disconnect
   //happens because of standby procedure.
   
   //Ensure that device is in full power first. There is scope for optimization
   //here especially in scenarios where PMC is already in IMPS or REQUEST_IMPS.
   //Core s/w needs to be optimized to handle this. Until then we request full
   //power before issuing request for standby.
   INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
   g_full_pwr_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback, 
       pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

   if(halStatus == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      ret = wait_for_completion_interruptible_timeout(
                      &pHddCtx->full_pwr_comp_var,
                       msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (0 >= ret)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s:wait on full_pwr_comp_var failed %ld",
                __func__, ret);
      }

      if(g_full_pwr_status != eHAL_STATUS_SUCCESS)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower Failed",__func__);
         VOS_ASSERT(0);
         vosStatus = VOS_STATUS_E_FAILURE;
         goto failure;
      }
   }
   else if(halStatus != eHAL_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed - status %d",
         __func__, halStatus);
      VOS_ASSERT(0);
      vosStatus = VOS_STATUS_E_FAILURE;
      goto failure;
   }

   if(pHddCtx->hdd_mcastbcast_filter_set == TRUE) {
         hdd_conf_mcastbcast_filter(pHddCtx, FALSE);
         pHddCtx->hdd_mcastbcast_filter_set = FALSE;
   }

   //Request standby. Standby will cause the STA to disassociate first. TX queues
   //will be disabled (by HDD) when STA disconnects. You do not want to disable TX
   //queues here. Also do not assert if the failure code is eHAL_STATUS_PMC_NOT_NOW as PMC
   //will send this failure code in case of concurrent sessions. Power Save cannot be supported
   //when there are concurrent sessions.
   INIT_COMPLETION(pHddCtx->standby_comp_var);
   g_standby_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestStandby(pHddCtx->hHal, hdd_suspend_standby_cbk, pHddCtx);

   if (halStatus == eHAL_STATUS_PMC_PENDING) 
   {
      //Wait till WLAN device enters standby mode
      ret = wait_for_completion_timeout(&pHddCtx->standby_comp_var,
         msecs_to_jiffies(WLAN_WAIT_TIME_STANDBY));
      if (0 >= ret)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                 FL("wait on standby_comp_var failed %ld"), ret);
      }
      if (g_standby_status != eHAL_STATUS_SUCCESS && g_standby_status != eHAL_STATUS_PMC_NOT_NOW)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed",__func__);
         VOS_ASSERT(0);
         vosStatus = VOS_STATUS_E_FAILURE;
         goto failure;
      }
   }
   else if (halStatus != eHAL_STATUS_SUCCESS && halStatus != eHAL_STATUS_PMC_NOT_NOW) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed - status %d",
         __func__, halStatus);
      VOS_ASSERT(0);
      vosStatus = VOS_STATUS_E_FAILURE;
      goto failure;
   }
   else
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_STANDBY;

failure:
   //Restore IMPS config
   if(pHddCtx->cfg_ini->fIsImpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);

   //Restore BMPS config
   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   return vosStatus;
}


//Helper routine for Deep sleep entry
VOS_STATUS hdd_enter_deep_sleep(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter)
{
   eHalStatus halStatus;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   vos_call_status_type callType;
   long ret;

   //Stop the Interface TX queue.
   netif_tx_disable(pAdapter->dev);
   netif_carrier_off(pAdapter->dev);

   //Disable IMPS,BMPS as we do not want the device to enter any power
   //save mode on it own during suspend sequence
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   //Ensure that device is in full power as we will touch H/W during vos_Stop
   INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
   g_full_pwr_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback, 
       pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

   if(halStatus == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      ret = wait_for_completion_interruptible_timeout(
                    &pHddCtx->full_pwr_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (0 >= ret)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("wait on full_pwr_comp_var failed %ld"), ret);
      }
      if(g_full_pwr_status != eHAL_STATUS_SUCCESS){
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
         VOS_ASSERT(0);
      }
   }
   else if(halStatus != eHAL_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Request for Full Power failed",__func__);
      VOS_ASSERT(0);
   }

   //Issue a disconnect. This is required to inform the supplicant that
   //STA is getting disassociated and for GUI to be updated properly
   INIT_COMPLETION(pAdapter->disconnect_comp_var);
   halStatus = sme_RoamDisconnect(pHddCtx->hHal, pAdapter->sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);

   //Success implies disconnect command got queued up successfully
   if(halStatus == eHAL_STATUS_SUCCESS)
   {
      //Block on a completion variable. Can't wait forever though.
      ret = wait_for_completion_interruptible_timeout(
                     &pAdapter->disconnect_comp_var,
                      msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
      if (0 >= ret)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("wait on disconnect_comp_var failed %ld"), ret);
      }
   }


   //None of the steps should fail after this. Continue even in case of failure
   vosStatus = vos_stop( pHddCtx->pvosContext );
   if( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: vos_stop return failed %d",
                __func__, vosStatus);
       VOS_ASSERT(0);
   }

   vosStatus = vos_chipAssertDeepSleep( &callType, NULL, NULL );
   if( VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
                FL("vos_chipAssertDeepSleep return failed %d"), vosStatus);
       VOS_ASSERT(0);
   }

   //Vote off any PMIC voltage supplies
   vosStatus = vos_chipPowerDown(NULL, NULL, NULL);
   if( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: vos_chipPowerDown return failed %d",
                __func__, vosStatus);
       VOS_ASSERT(0);
   }

   pHddCtx->hdd_ps_state = eHDD_SUSPEND_DEEP_SLEEP;

   //Restore IMPS config
   if(pHddCtx->cfg_ini->fIsImpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);

   //Restore BMPS config
   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   return vosStatus;
}

VOS_STATUS hdd_exit_deep_sleep(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter)
{
   VOS_STATUS vosStatus;
   eHalStatus halStatus;

   //Power Up Libra WLAN card first if not already powered up
   vosStatus = vos_chipPowerUp(NULL,NULL,NULL);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN not Powered Up."
          "exiting", __func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling hdd_set_sme_config",__func__);
   vosStatus = hdd_set_sme_config( pHddCtx );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in hdd_set_sme_config",__func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
      "%s: calling vos_start",__func__);
   vosStatus = vos_start( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in vos_start",__func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
      "%s: calling hdd_post_voss_start_config",__func__);
   vosStatus = hdd_post_voss_start_config( pHddCtx );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in hdd_post_voss_start_config",__func__);
      goto err_voss_stop;
   }


   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pHddCtx,
                                (tANI_U8 *)&pAdapter->macAddressCurrent,
                                &pAdapter->sessionId);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"sme_OpenSession() failed with status code %08d [x%08lx]",
                    halStatus, halStatus );
      goto err_voss_stop;

   }

   pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;

   //Trigger the initial scan
   hdd_wlan_initial_scan(pHddCtx);

   return VOS_STATUS_SUCCESS;

err_voss_stop:
   vos_stop(pHddCtx->pvosContext);
err_deep_sleep:
   return VOS_STATUS_E_FAILURE;

}

void hdd_ipv6_notifier_work_queue(struct work_struct *work)
{
    hdd_adapter_t* pAdapter =
             container_of(work, hdd_adapter_t, ipv6NotifierWorkQueue);
    hdd_context_t *pHddCtx;
    int status;

    hddLog(LOG1, FL("Reconfiguring NS Offload"));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(LOGE, FL("HDD context is invalid"));
        return;
    }

    if ( VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
    {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        hddLog(LOG1, FL("saving configuredMcastBcastFilter = %d"),
                        pHddCtx->sus_res_mcastbcast_filter);
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
    }

    if ((eConnectionState_Associated ==
                (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)
        && (pHddCtx->hdd_wlan_suspended))
    {
        // This invocation being part of the IPv6 registration callback,
        // set the newly generated ip address to f/w in suspend mode.
#ifdef WLAN_NS_OFFLOAD
            //Disable NSOFFLOAD
            if (pHddCtx->cfg_ini->fhostNSOffload)
            {
                hdd_conf_ns_offload(pAdapter, 1);
            }
#endif
    }
#ifdef WLAN_FEATURE_PACKET_FILTERING
    /* wlan_hdd_set_mc_addr_list() is called from the early susspend
     * only so when new ipv6 address is generated the screen may not
     * on so we need to call it here to update the list in f/w.
     */
     wlan_hdd_set_mc_addr_list(pAdapter, TRUE);
#endif

}


static int wlan_hdd_ipv6_changed(struct notifier_block *nb,
                                   unsigned long data, void *arg)
{
    struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)arg;
    struct net_device *ndev = ifa->idev->dev;
    hdd_adapter_t *pAdapter =
             container_of(nb, struct hdd_adapter_s, ipv6_notifier);
    hdd_context_t *pHddCtx;
    int status;

    if (pAdapter && pAdapter->dev == ndev)
    {
        pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
        status = wlan_hdd_validate_context(pHddCtx);
        if (0 != status)
        {
            hddLog(LOGE, FL("HDD context is invalid"));
            return NOTIFY_DONE;
        }

        schedule_work(&pAdapter->ipv6NotifierWorkQueue);
    }

    return NOTIFY_DONE;
}

void hdd_ipv6_callback_register(hdd_adapter_t *pAdapter, int fenable)
{

    int ret = 0;

    if (fenable)
    {

         if(!pAdapter->ipv6_notifier_registered)
         {

             // Register IPv6 notifier to notify if any change in IP
             // So that we can reconfigure the offload parameters
             pAdapter->ipv6_notifier.notifier_call =
                             wlan_hdd_ipv6_changed;
             ret = register_inet6addr_notifier(&pAdapter->ipv6_notifier);
             if (ret)
             {
                 hddLog(LOGE, FL("Failed to register IPv6 notifier"));
             }
             else
             {
                 hddLog(LOG1, FL("Registered IPv6 notifier"));
                 pAdapter->ipv6_notifier_registered = true;
             }
         }
    }
    else
    {

        if (pAdapter->ipv6_notifier_registered)
        {
            hddLog(LOG1, FL("Unregistered IPv6 notifier"));
            unregister_inet6addr_notifier(&pAdapter->ipv6_notifier);
            pAdapter->ipv6_notifier_registered = false;
        }

    }

}

/*
 * Function: hdd_conf_hostoffload
 *           Central function to configure the supported offloads,
 *           either enable or disable them.
 */
void hdd_conf_hostoffload(hdd_adapter_t *pAdapter, v_BOOL_t fenable)
{
    hdd_context_t *pHddCtx = NULL;
    v_CONTEXT_t *pVosContext = NULL;
    VOS_STATUS vstatus = VOS_STATUS_E_FAILURE;

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Configuring offloads with flag: %d"),
            fenable);

    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL == pVosContext)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL(" Global VOS context is Null"));
        return;
    }

    //Get the HDD context.
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: HDD context is Null", __func__);
        return;
    }

    if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
    {
        if (fenable)
        {
            if (eConnectionState_Associated ==
                    (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)
            {
                if ((pHddCtx->cfg_ini->fhostArpOffload))
                {
                    /*
                     * Configure the ARP Offload.
                     * Even if it fails we have to reconfigure the MC/BC
                     * filter flag as we want RIVA not to drop BroadCast
                     * Packets
                     */
                    hddLog(VOS_TRACE_LEVEL_INFO,
                            FL("Calling ARP Offload with flag: %d"), fenable);
                    vstatus = hdd_conf_arp_offload(pAdapter, fenable);
                    pHddCtx->configuredMcastBcastFilter &=
                            ~(HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST);

                    if (!VOS_IS_STATUS_SUCCESS(vstatus))
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                                "Failed to enable ARPOFfloadFeature %d",
                                vstatus);
                    }
                }
                //Configure GTK_OFFLOAD
#ifdef WLAN_FEATURE_GTK_OFFLOAD
                hdd_conf_gtk_offload(pAdapter, fenable);
#endif

#ifdef WLAN_NS_OFFLOAD
                if (pHddCtx->cfg_ini->fhostNSOffload)
                {
                    /*
                     * Configure the NS Offload.
                     * Even if it fails we have to reconfigure the MC/BC filter flag
                     * as we want RIVA not to drop Multicast Packets
                     */

                    hddLog(VOS_TRACE_LEVEL_INFO,
                            FL("Calling NS Offload with flag: %d"), fenable);
                    hdd_conf_ns_offload(pAdapter, fenable);
                    pHddCtx->configuredMcastBcastFilter &=
                            ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
                }
#endif

            }
            hdd_ipv6_callback_register(pAdapter, fenable);
        }
        else
        {
            //Disable ARPOFFLOAD
            if (pHddCtx->cfg_ini->fhostArpOffload)
            {
                vstatus = hdd_conf_arp_offload(pAdapter, fenable);
                if (!VOS_IS_STATUS_SUCCESS(vstatus))
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                          "Failed to disable ARPOffload Feature %d", vstatus);
                }
            }
            //Disable GTK_OFFLOAD
#ifdef WLAN_FEATURE_GTK_OFFLOAD
            hdd_conf_gtk_offload(pAdapter, fenable);
#endif

#ifdef WLAN_NS_OFFLOAD
            //Disable NSOFFLOAD
            if (pHddCtx->cfg_ini->fhostNSOffload)
            {
                hdd_conf_ns_offload(pAdapter, fenable);
            }
#endif
            hdd_ipv6_callback_register(pAdapter, fenable);
        }
    }
    return;
}


#ifdef WLAN_NS_OFFLOAD
/**----------------------------------------------------------------------------

  \brief hdd_conf_ns_offload() - Configure NS offload

  Called during SUSPEND to configure the NS offload (MC BC filter) which
  reduces power consumption.

  \param  - pAdapter - Adapter context for which NS offload is to be configured
  \param  - fenable - 0 - disable.
                      1 - enable. (with IPv6 notifier registration)
                      2 - enable. (without IPv6 notifier registration)

  \return - void

  ---------------------------------------------------------------------------*/
void hdd_conf_ns_offload(hdd_adapter_t *pAdapter, int fenable)
{
    struct inet6_dev *in6_dev;
    struct inet6_ifaddr *ifp;
    struct list_head *p;
    int slot_index = NS_DEFAULT_SLOT_INDEX;
    tANI_U8 **selfIPv6Addr = NULL;
    tANI_U8 *selfIPv6AddrValid = NULL;
    tSirHostOffloadReq offLoadRequest;
    hdd_context_t *pHddCtx;
    tHalHandle halHandle;

    int i = 0, slot = 0;
    int ret = 0;
    eHalStatus returnStatus;

    ENTER();
    hddLog(LOG1, FL(" fenable = %d"), fenable);

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD adapter is Null"));
        return;
    }

    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    ret = wlan_hdd_validate_context(pHddCtx);

    if (0 != ret)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("HDD context is not valid"));
        return;
    }

    if (sme_IsFeatureSupportedByFW(EXTENDED_NSOFFLOAD_SLOT))
    {
        slot_index = NS_EXTENDED_SLOT_INDEX;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, FL("slot_idex = %d"), slot_index);

    selfIPv6AddrValid =
            (tANI_U8 *)vos_mem_malloc(sizeof(tANI_U8) * slot_index);

    if (NULL == selfIPv6AddrValid)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, FL("Failed to allocate memory for"
                                         " selfIPv6AddrValid"));
        goto end;
    }

    vos_mem_zero(selfIPv6AddrValid, slot_index * sizeof(tANI_U8));

    selfIPv6Addr = (tANI_U8 **)vos_mem_malloc(sizeof(tANI_U8 *) * slot_index);

    if (NULL == selfIPv6Addr)
    {
        hddLog (VOS_TRACE_LEVEL_ERROR, FL("Failed to allocate memory for"
                                         " selfIPv6Addr"));
        goto end;
    }

    vos_mem_zero(selfIPv6Addr, slot_index * sizeof(tANI_U8 *));

    for (slot = 0; slot < slot_index; slot++)
    {
        selfIPv6Addr[slot] =
           (tANI_U8 *)vos_mem_malloc(SIR_MAC_IPV6_ADDR_LEN);
        if (NULL == selfIPv6Addr[slot])
        {
            hddLog (VOS_TRACE_LEVEL_ERROR, FL("Failed to allocate memory"
                                              "for selfIPv6Addr"));
            goto end;
        }
        vos_mem_zero(selfIPv6Addr[slot], SIR_MAC_IPV6_ADDR_LEN);
    }

    i = 0;

    if (fenable)
    {
        in6_dev = __in6_dev_get(pAdapter->dev);
        if (NULL != in6_dev)
        {
            list_for_each(p, &in6_dev->addr_list)
            {
                if (i >= slot_index)
                {
                    hddLog (VOS_TRACE_LEVEL_ERROR,
                            FL("IPv6 address list is greater than IPv6"
                               "address supported by firmware"));
                    hddLog (VOS_TRACE_LEVEL_ERROR,
                            FL("FW supported IPv6 address = %d"), slot_index);
                    break;
                }
                ifp = list_entry(p, struct inet6_ifaddr, if_list);
                switch(ipv6_addr_src_scope(&ifp->addr))
                {
                    case IPV6_ADDR_SCOPE_LINKLOCAL:
                        vos_mem_copy(selfIPv6Addr[i], &ifp->addr.s6_addr,
                                sizeof(ifp->addr.s6_addr));
                        selfIPv6AddrValid[i] = SIR_IPV6_ADDR_VALID;
                        hddLog (VOS_TRACE_LEVEL_INFO,
                           FL("Found IPV6_ADDR_SCOPE_LINKLOCAL Address : %pI6"),
                               selfIPv6Addr[i]);
                        break;
                    case IPV6_ADDR_SCOPE_GLOBAL:
                        vos_mem_copy(selfIPv6Addr[i], &ifp->addr.s6_addr,
                                sizeof(ifp->addr.s6_addr));
                        selfIPv6AddrValid[i] = SIR_IPV6_ADDR_VALID;
                        hddLog (VOS_TRACE_LEVEL_INFO,
                           FL("Found IPV6_ADDR_SCOPE_GLOBAL Address : %pI6"),
                               selfIPv6Addr[i]);
                        break;
                    default:
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                           FL("The Scope %d is not supported"),
                               ipv6_addr_src_scope(&ifp->addr));
                }
                if (selfIPv6AddrValid[i] == SIR_IPV6_ADDR_VALID)
                {
                    i++;
                }
            }

            vos_mem_zero(&offLoadRequest, sizeof(offLoadRequest));
            for (i =0; i < slot_index; i++)
            {
                if (selfIPv6AddrValid[i])
                {
                    //Filling up the request structure
                    /* Filling the selfIPv6Addr with solicited address
                     * A Solicited-Node multicast address is created by
                     * taking the last 24 bits of a unicast or anycast
                     * address and appending them to the prefix
                     *
                     * FF02:0000:0000:0000:0000:0001:FFXX:XX
                     *
                     * here XX is the unicast/anycast bits
                     */
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[0] = 0xFF;
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[1] = 0x02;
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[11] = 0x01;
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[12] = 0xFF;
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[13] =
                                                          selfIPv6Addr[i][13];
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[14] =
                                                          selfIPv6Addr[i][14];
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr[15] =
                                                          selfIPv6Addr[i][15];
                    offLoadRequest.nsOffloadInfo.slotIdx = i;

                    vos_mem_copy(&offLoadRequest.nsOffloadInfo.targetIPv6Addr[0],
                        selfIPv6Addr[i], sizeof(tANI_U8)*SIR_MAC_IPV6_ADDR_LEN);
                    vos_mem_copy(&offLoadRequest.nsOffloadInfo.selfMacAddr,
                                &pAdapter->macAddressCurrent.bytes,
                                sizeof(tANI_U8)*SIR_MAC_ADDR_LEN);

                    offLoadRequest.nsOffloadInfo.targetIPv6AddrValid[0] =
                                                          SIR_IPV6_ADDR_VALID;
                    offLoadRequest.offloadType =  SIR_IPV6_NS_OFFLOAD;
                    offLoadRequest.enableOrDisable = SIR_OFFLOAD_ENABLE;

                    hddLog (VOS_TRACE_LEVEL_INFO,
                       FL("configuredMcastBcastFilter: %d"
                       "NSOffload Slot = %d"),
                       pHddCtx->configuredMcastBcastFilter, i);

                    if ((VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid)
                        && ((HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST ==
                             pHddCtx->sus_res_mcastbcast_filter) ||
                            (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                             pHddCtx->sus_res_mcastbcast_filter)) &&
                        (!WDA_IS_MCAST_FLT_ENABLE_IN_FW ||
                         (WDA_IS_MCAST_FLT_ENABLE_IN_FW &&
                          !(pHddCtx->cfg_ini->fEnableMCAddrList))))
                    {
                        offLoadRequest.enableOrDisable =
                            SIR_OFFLOAD_NS_AND_MCAST_FILTER_ENABLE;
                        hddLog (VOS_TRACE_LEVEL_INFO,
                                FL("Set offLoadRequest with %d"),
                                   offLoadRequest.enableOrDisable);
                    }

                    vos_mem_copy(&offLoadRequest.params.hostIpv6Addr,
                                &offLoadRequest.nsOffloadInfo.targetIPv6Addr[0],
                                sizeof(tANI_U8)*SIR_MAC_IPV6_ADDR_LEN);

                    hddLog (VOS_TRACE_LEVEL_INFO,
                    FL("Setting NSOffload with solicitedIp: %pI6,"
                       "targetIp: %pI6"),
                    offLoadRequest.nsOffloadInfo.selfIPv6Addr,
                    offLoadRequest.nsOffloadInfo.targetIPv6Addr[0]);

                    //Configure the Firmware with this
                    returnStatus = sme_SetHostOffload(halHandle,
                                    pAdapter->sessionId, &offLoadRequest);
                    if(eHAL_STATUS_SUCCESS != returnStatus)
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("Failed to enable HostOffload feature with"
                           " status: %d"), returnStatus);
                    }
                    vos_mem_zero(&offLoadRequest, sizeof(offLoadRequest));
                }
            }
        }
        else
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("IPv6 dev does not exist. Failed to request NSOffload"));
              goto end;
        }
    }
    else
    {
        //Disable NSOffload
        vos_mem_zero((void *)&offLoadRequest, sizeof(tSirHostOffloadReq));
        offLoadRequest.enableOrDisable = SIR_OFFLOAD_DISABLE;
        offLoadRequest.offloadType =  SIR_IPV6_NS_OFFLOAD;

        for (i = 0; i < slot_index; i++)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("Disable Slot= %d"), i);
            offLoadRequest.nsOffloadInfo.slotIdx = i;
            if (eHAL_STATUS_SUCCESS !=
                 sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, &offLoadRequest))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("Failure to disable"
                                                 " %d Slot"), i);
            }
        }
    }
end:
    while (slot > 0 && selfIPv6Addr[--slot])
    {
       vos_mem_free(selfIPv6Addr[slot]);
    }
    if (selfIPv6Addr)
    {
       vos_mem_free(selfIPv6Addr);
    }
    if (selfIPv6AddrValid)
    {
       vos_mem_free(selfIPv6AddrValid);
    }
    return;
}
#endif

void hdd_ipv4_notifier_work_queue(struct work_struct *work)
{
    hdd_adapter_t* pAdapter =
             container_of(work, hdd_adapter_t, ipv4NotifierWorkQueue);
    hdd_context_t *pHddCtx;
    int status;

    hddLog(LOG1, FL("Reconfiguring ARP Offload"));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(LOGE, FL("HDD context is invalid"));
        return;
    }

    if ( VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
    {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        hddLog(LOG1, FL("saving configuredMcastBcastFilter = %d"),
                        pHddCtx->sus_res_mcastbcast_filter);
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
    }

    if ((eConnectionState_Associated ==
                (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)
        && (pHddCtx->hdd_wlan_suspended))
    {
        // This invocation being part of the IPv4 registration callback,
        // we are passing second parameter as 2 to avoid registration
        // of IPv4 notifier again.
        hdd_conf_arp_offload(pAdapter, 2);
    }
}

static int wlan_hdd_ipv4_changed(struct notifier_block *nb,
                                   unsigned long data, void *arg)
{
    struct in_ifaddr *ifa = (struct in_ifaddr *)arg;
    struct in_ifaddr **ifap = NULL;
    struct in_device *in_dev;

    struct net_device *ndev = ifa->ifa_dev->dev;
    hdd_adapter_t *pAdapter =
             container_of(nb, struct hdd_adapter_s, ipv4_notifier);
    hdd_context_t *pHddCtx;
    int status;
    if (pAdapter && pAdapter->dev == ndev)
    {
       pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
       status = wlan_hdd_validate_context(pHddCtx);
       if (0 != status)
       {
           hddLog(LOGE, FL("HDD context is invalid"));
           return NOTIFY_DONE;
       }
       if ((in_dev = __in_dev_get_rtnl(pAdapter->dev)) != NULL)
       {
           for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
                   ifap = &ifa->ifa_next)
           {
               if (!strcmp(pAdapter->dev->name, ifa->ifa_label))
               {
                   break; /* found */
               }
           }
       }
       if(ifa && ifa->ifa_local)
       {
           schedule_work(&pAdapter->ipv4NotifierWorkQueue);
       }
    }

    return NOTIFY_DONE;
}

/**----------------------------------------------------------------------------

  \brief hdd_conf_arp_offload() - Configure ARP offload

  Called during SUSPEND to configure the ARP offload (MC BC filter) which
  reduces power consumption.

  \param  - pAdapter -Adapter context for which ARP offload is to be configured
  \param  - fenable - 0 - disable.
                      1 - enable. (with IPv4 notifier registration)
                      2 - enable. (without IPv4 notifier registration)

  \return -
            VOS_STATUS_SUCCESS - on successful operation
            VOS_STATUS_E_FAILURE - on failure of operation
-----------------------------------------------------------------------------*/
VOS_STATUS hdd_conf_arp_offload(hdd_adapter_t *pAdapter, int fenable)
{
   struct in_ifaddr **ifap = NULL;
   struct in_ifaddr *ifa = NULL;
   struct in_device *in_dev;
   int i = 0;
   int ret = 0;
   tSirHostOffloadReq  offLoadRequest;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   hddLog(VOS_TRACE_LEVEL_ERROR, FL(" fenable = %d"), fenable);

   if(fenable)
   {
       if ((in_dev = __in_dev_get_rtnl(pAdapter->dev)) != NULL)
       {
           for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
                   ifap = &ifa->ifa_next)
           {
               if (!strcmp(pAdapter->dev->name, ifa->ifa_label))
               {
                   break; /* found */
               }
           }
       }
       if(ifa && ifa->ifa_local)
       {
           offLoadRequest.offloadType =  SIR_IPV4_ARP_REPLY_OFFLOAD;
           offLoadRequest.enableOrDisable = SIR_OFFLOAD_ENABLE;

           hddLog(VOS_TRACE_LEVEL_INFO, "%s: Enabled", __func__);

           if (((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
                pHddCtx->sus_res_mcastbcast_filter) ||
               (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                pHddCtx->sus_res_mcastbcast_filter)) &&
               (VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid))
           {
               offLoadRequest.enableOrDisable =
                   SIR_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE;
               hddLog(VOS_TRACE_LEVEL_INFO,
                      "offload: inside arp offload conditional check");
           }

           hddLog(VOS_TRACE_LEVEL_INFO, "offload: arp filter programmed = %d",
                  offLoadRequest.enableOrDisable);

           //converting u32 to IPV4 address
           for(i = 0 ; i < 4; i++)
           {
              offLoadRequest.params.hostIpv4Addr[i] =
                      (ifa->ifa_local >> (i*8) ) & 0xFF ;
           }
           hddLog(VOS_TRACE_LEVEL_INFO, " Enable SME HostOffload: %d.%d.%d.%d",
                  offLoadRequest.params.hostIpv4Addr[0],
                  offLoadRequest.params.hostIpv4Addr[1],
                  offLoadRequest.params.hostIpv4Addr[2],
                  offLoadRequest.params.hostIpv4Addr[3]);

          if (eHAL_STATUS_SUCCESS !=
                    sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                    pAdapter->sessionId, &offLoadRequest))
          {
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to enable HostOffload "
                      "feature", __func__);
              return VOS_STATUS_E_FAILURE;
          }
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_ERROR, FL("IP Address is not assigned"));
       }

       if (fenable == 1 && !pAdapter->ipv4_notifier_registered)
       {
           // Register IPv4 notifier to notify if any change in IP
           // So that we can reconfigure the offload parameters
           pAdapter->ipv4_notifier.notifier_call =
                         wlan_hdd_ipv4_changed;
           ret = register_inetaddr_notifier(&pAdapter->ipv4_notifier);
           if (ret)
           {
               hddLog(LOGE, FL("Failed to register IPv4 notifier"));
           }
           else
           {
               hddLog(LOG1, FL("Registered IPv4 notifier"));
               pAdapter->ipv4_notifier_registered = true;
           }
       }
       return VOS_STATUS_SUCCESS;
   }
   else
   {
       if (pAdapter->ipv4_notifier_registered)
       {
           hddLog(LOG1, FL("Unregistered IPv4 notifier"));
           unregister_inetaddr_notifier(&pAdapter->ipv4_notifier);
           pAdapter->ipv4_notifier_registered = false;
       }
       vos_mem_zero((void *)&offLoadRequest, sizeof(tSirHostOffloadReq));
       offLoadRequest.enableOrDisable = SIR_OFFLOAD_DISABLE;
       offLoadRequest.offloadType =  SIR_IPV4_ARP_REPLY_OFFLOAD;

       if (eHAL_STATUS_SUCCESS !=
                 sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, &offLoadRequest))
       {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to disable host "
                             "offload feature", __func__);
            return VOS_STATUS_E_FAILURE;
       }
       return VOS_STATUS_SUCCESS;
   }
}

/*
 * This function is called before setting mcbc filters
 * to modify filter value considering Different Offloads
*/
void hdd_mcbc_filter_modification(hdd_context_t* pHddCtx,
                                  tANI_U8 *pMcBcFilter)
{
    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("NULL HDD context passed"));
        return;
    }

    *pMcBcFilter = pHddCtx->configuredMcastBcastFilter;
    if (pHddCtx->cfg_ini->fhostArpOffload)
    {
        /* ARP offload is enabled, do not block bcast packets at RXP
         * Will be using Bitmasking to reset the filter. As we have
         * disable Broadcast filtering, Anding with the negation
         * of Broadcast BIT
         */
        *pMcBcFilter &= ~(HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST);
    }

#ifdef WLAN_NS_OFFLOAD
    if (pHddCtx->cfg_ini->fhostNSOffload)
    {
        /* NS offload is enabled, do not block mcast packets at RXP
         * Will be using Bitmasking to reset the filter. As we have
         * disable Multicast filtering, Anding with the negation
         * of Multicast BIT
         */
        *pMcBcFilter &= ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
    }
#endif

    if ((pHddCtx->cfg_ini->fEnableMCAddrList) && WDA_IS_MCAST_FLT_ENABLE_IN_FW)
    {
        *pMcBcFilter &= ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
    }

    pHddCtx->configuredMcastBcastFilter = *pMcBcFilter;
}

void hdd_conf_mcastbcast_filter(hdd_context_t* pHddCtx, v_BOOL_t setfilter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    tpSirWlanSetRxpFilters wlanRxpFilterParam =
                     vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
    if(NULL == wlanRxpFilterParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO,
        "%s: Configuring Mcast/Bcast Filter Setting. setfilter %d", __func__, setfilter);
    if (TRUE == setfilter)
    {
            hdd_mcbc_filter_modification(pHddCtx,
                  &wlanRxpFilterParam->configuredMcstBcstFilterSetting);
    }
    else
    {
        /*Use the current configured value to clear*/
        wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                              pHddCtx->configuredMcastBcastFilter;
    }

    wlanRxpFilterParam->setMcstBcstFilter = setfilter;
    halStatus = sme_ConfigureRxpFilter(pHddCtx->hHal, wlanRxpFilterParam);
    if (eHAL_STATUS_SUCCESS != halStatus)
        vos_mem_free(wlanRxpFilterParam);
    if(setfilter && (eHAL_STATUS_SUCCESS == halStatus))
       pHddCtx->hdd_mcastbcast_filter_set = TRUE;
}

static void hdd_conf_suspend_ind(hdd_context_t* pHddCtx,
                                 hdd_adapter_t *pAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    tpSirWlanSuspendParam wlanSuspendParam =
      vos_mem_malloc(sizeof(tSirWlanSuspendParam));

    if (VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid) {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
        hddLog(VOS_TRACE_LEVEL_INFO, "offload: hdd_conf_suspend_ind");
        hddLog(VOS_TRACE_LEVEL_INFO, "configuredMCastBcastFilter saved = %d",
               pHddCtx->configuredMcastBcastFilter);

    }


    if(NULL == wlanSuspendParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
      "%s: send wlan suspend indication", __func__);

    if((pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER))
    {
        //Configure supported OffLoads
        hdd_conf_hostoffload(pAdapter, TRUE);
        wlanSuspendParam->configuredMcstBcstFilterSetting = pHddCtx->configuredMcastBcastFilter;
        hddLog(VOS_TRACE_LEVEL_INFO,
              FL("saving configuredMcastBcastFilterSetting = %d"),
              wlanSuspendParam->configuredMcstBcstFilterSetting);
#ifdef WLAN_FEATURE_PACKET_FILTERING
        /* During suspend, configure MC Addr list filter to the firmware
         * function takes care of checking necessary conditions before
         * configuring.
         */
        wlan_hdd_set_mc_addr_list(pAdapter, TRUE);
#endif

        if( (pHddCtx->cfg_ini->fEnableMCAddrList) && WDA_IS_MCAST_FLT_ENABLE_IN_FW)
        {

            hddLog(VOS_TRACE_LEVEL_INFO, "offload: MCaddrlist: FW capability set ");
            pHddCtx->configuredMcastBcastFilter &=
                ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
        }

        wlanSuspendParam->configuredMcstBcstFilterSetting =
            pHddCtx->configuredMcastBcastFilter;
    }

    halStatus = sme_ConfigureSuspendInd(pHddCtx->hHal, wlanSuspendParam);
    if(eHAL_STATUS_SUCCESS == halStatus)
    {
        pHddCtx->hdd_mcastbcast_filter_set = TRUE;
    } else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
            FL("sme_ConfigureSuspendInd returned failure %d"), halStatus);
        vos_mem_free(wlanSuspendParam);
    }
}

static void hdd_conf_resume_ind(hdd_adapter_t *pAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    hdd_context_t* pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWlanResumeParam wlanResumeParam;

    hddLog(VOS_TRACE_LEVEL_INFO,
      "%s: send wlan resume indication", __func__);

    wlanResumeParam = vos_mem_malloc(sizeof(tSirWlanResumeParam));

    if (NULL == wlanResumeParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: memory allocation failed for wlanResumeParam ", __func__);
        return;
    }

    //Disable supported OffLoads
    hdd_conf_hostoffload(pAdapter, FALSE);

    wlanResumeParam->configuredMcstBcstFilterSetting =
                               pHddCtx->configuredMcastBcastFilter;
    halStatus = sme_ConfigureResumeReq(pHddCtx->hHal, wlanResumeParam);
    if (eHAL_STATUS_SUCCESS != halStatus)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: sme_ConfigureResumeReq return failure %d", __func__, halStatus);
        vos_mem_free(wlanResumeParam);
    }

    pHddCtx->hdd_mcastbcast_filter_set = FALSE;

    if (VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid) {
        pHddCtx->configuredMcastBcastFilter =
            pHddCtx->sus_res_mcastbcast_filter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_FALSE;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "offload: in hdd_conf_resume_ind, restoring configuredMcastBcastFilter");
    hddLog(VOS_TRACE_LEVEL_INFO, "configuredMcastBcastFilter = %d",
                  pHddCtx->configuredMcastBcastFilter);


#ifdef WLAN_FEATURE_PACKET_FILTERING
    /* Filer was applied during suspend inditication
     * clear it when we resume.
     */
    wlan_hdd_set_mc_addr_list(pAdapter, FALSE);
#endif
}

//Suspend routine registered with Android OS
void hdd_suspend_wlan(void)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;

   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   bool hdd_enter_bmps = FALSE;

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being suspended by Android OS",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return;
   }

   if (pHddCtx->isLogpInProgress) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Ignore suspend wlan, LOGP in progress!", __func__);
      return;
   }

   hdd_set_pwrparams(pHddCtx);
   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;

#ifdef FEATURE_WLAN_LPHB
       if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
           (pHddCtx->lphbEnableReq.enable))
       {
           tSirLPHBReq *hb_params = NULL;

           hb_params = (tSirLPHBReq *)vos_mem_malloc(sizeof(tSirLPHBReq));
           if (NULL == hb_params)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s: hb_params alloc failed", __func__);
           }
           else
           {
               eHalStatus smeStatus;

               hb_params->cmd    = LPHB_SET_EN_PARAMS_INDID;
               hb_params->params.lphbEnableReq.enable  =
                                    pHddCtx->lphbEnableReq.enable;
               hb_params->params.lphbEnableReq.item    =
                                    pHddCtx->lphbEnableReq.item;
               hb_params->params.lphbEnableReq.session =
                                    pHddCtx->lphbEnableReq.session;
               /* If WLAN is suspend state, send enable command immediately */
               smeStatus = sme_LPHBConfigReq((tHalHandle)(pHddCtx->hHal),
                                             hb_params,
                                             NULL);
               if (eHAL_STATUS_SUCCESS != smeStatus)
               {
                   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "LPHB Config Fail, disable");
                   pHddCtx->lphbEnableReq.enable = 0;
                   vos_mem_free(hb_params);
               }
           }
       }
#endif /* FEATURE_WLAN_LPHB */

       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_SOFTAP != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )

       {  // we skip this registration for modes other than STA, SAP and P2P client modes.
           status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
           pAdapterNode = pNext;
           continue;
       }
       /* Avoid multiple enter/exit BMPS in this while loop using
        * hdd_enter_bmps flag
        */
       if (FALSE == hdd_enter_bmps && (BMPS == pmcGetPmcState(pHddCtx->hHal)))
       {
            hdd_enter_bmps = TRUE;

           /* If device was already in BMPS, and dynamic DTIM is set,
            * exit(set the device to full power) and enter BMPS again
            * to reflect new DTIM value */
           wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

           wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

           pHddCtx->hdd_ignore_dtim_enabled = TRUE;
       }
#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
       if (pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_STANDBY)
       {
          //stop the interface before putting the chip to standby
          netif_tx_disable(pAdapter->dev);
          netif_carrier_off(pAdapter->dev);
       }
       else if (pHddCtx->cfg_ini->nEnableSuspend ==
               WLAN_MAP_SUSPEND_TO_DEEP_SLEEP)
       {
          //Execute deep sleep procedure
          hdd_enter_deep_sleep(pHddCtx, pAdapter);
       }
#endif

       /*Suspend notification sent down to driver*/
       hdd_conf_suspend_ind(pHddCtx, pAdapter);

       status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
       pAdapterNode = pNext;
   }
   pHddCtx->hdd_wlan_suspended = TRUE;

#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
  if(pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_STANDBY)
  {
      hdd_enter_standby(pHddCtx);
  }
#endif

   return;
}

static void hdd_PowerStateChangedCB
(
   v_PVOID_t callbackContext,
   tPmcState newState
)
{
   hdd_context_t *pHddCtx = callbackContext;

   /* if the driver was not in BMPS during early suspend,
    * the dynamic DTIM is now updated at Riva */
   if ((newState == BMPS) && pHddCtx->hdd_wlan_suspended
           && pHddCtx->cfg_ini->enableDynamicDTIM
           && (pHddCtx->hdd_ignore_dtim_enabled == FALSE))
   {
       pHddCtx->hdd_ignore_dtim_enabled = TRUE;
   }
   spin_lock(&pHddCtx->filter_lock);
   if ((newState == BMPS) &&  pHddCtx->hdd_wlan_suspended)
   {
      spin_unlock(&pHddCtx->filter_lock);
      if (VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
      {
          pHddCtx->sus_res_mcastbcast_filter =
              pHddCtx->configuredMcastBcastFilter;
          pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;

          hddLog(VOS_TRACE_LEVEL_INFO, "offload: callback to associated");
          hddLog(VOS_TRACE_LEVEL_INFO, "saving configuredMcastBcastFilter = %d",
                 pHddCtx->configuredMcastBcastFilter);
          hddLog(VOS_TRACE_LEVEL_INFO,
                 "offload: calling hdd_conf_mcastbcast_filter");

      }

      hdd_conf_mcastbcast_filter(pHddCtx, TRUE);
      if(pHddCtx->hdd_mcastbcast_filter_set != TRUE)
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Not able to set mcast/bcast filter ", __func__);
    }
   else
   {
      /* Android framework can send resume request when the WCN chip is
       * in IMPS mode. When the chip exits IMPS mode the firmware will
       * restore all the registers to the state they were before the chip
       * entered IMPS and so our hardware filter settings confgured by the
       * resume request will be lost. So reconfigure the filters on detecting
       * a change in the power state of the WCN chip.
       */
      spin_unlock(&pHddCtx->filter_lock);
      if (IMPS != newState)
      {
           spin_lock(&pHddCtx->filter_lock);
           if (FALSE == pHddCtx->hdd_wlan_suspended)
           {
                spin_unlock(&pHddCtx->filter_lock);
                hddLog(VOS_TRACE_LEVEL_INFO,
                          "Not in IMPS/BMPS and suspended state");
                hdd_conf_mcastbcast_filter(pHddCtx, FALSE);
           }
           else
           {
               spin_unlock(&pHddCtx->filter_lock);
           }
      }
   }
}

void hdd_register_mcast_bcast_filter(hdd_context_t *pHddCtx)
{
   v_CONTEXT_t pVosContext;
   tHalHandle smeContext;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (NULL == pVosContext)
   {
      hddLog(LOGE, "%s: Invalid pContext", __func__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __func__);
      return;
   }

   spin_lock_init(&pHddCtx->filter_lock);
   if (WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER ==
                                            pHddCtx->cfg_ini->nEnableSuspend)
   {
      pmcRegisterDeviceStateUpdateInd(smeContext,
                                      hdd_PowerStateChangedCB, pHddCtx);
   }
}

void hdd_unregister_mcast_bcast_filter(hdd_context_t *pHddCtx)
{
   v_CONTEXT_t pVosContext;
   tHalHandle smeContext;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (NULL == pVosContext)
   {
      hddLog(LOGE, "%s: Invalid pContext", __func__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __func__);
      return;
   }

   if (WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER == 
                                            pHddCtx->cfg_ini->nEnableSuspend)
   {
      pmcDeregisterDeviceStateUpdateInd(smeContext, hdd_PowerStateChangedCB);
   }
}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
void hdd_conf_gtk_offload(hdd_adapter_t *pAdapter, v_BOOL_t fenable)
{
    eHalStatus ret;
    tSirGtkOffloadParams hddGtkOffloadReqParams;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if(fenable)
    {
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
           (GTK_OFFLOAD_ENABLE == pHddStaCtx->gtkOffloadReqParams.ulFlags ))
        {
            vos_mem_copy(&hddGtkOffloadReqParams,
                 &pHddStaCtx->gtkOffloadReqParams,
                 sizeof (tSirGtkOffloadParams));

            ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                          &hddGtkOffloadReqParams, pAdapter->sessionId);
            if (eHAL_STATUS_SUCCESS != ret)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: sme_SetGTKOffload failed, returned %d",
                       __func__, ret);
                return;
            }

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: sme_SetGTKOffload successfull", __func__);
        }

    }
    else
    {
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
            (0 ==  memcmp(&pHddStaCtx->gtkOffloadReqParams.bssId,
                     &pHddStaCtx->conn_info.bssId, WNI_CFG_BSSID_LEN)) &&
            (GTK_OFFLOAD_ENABLE == pHddStaCtx->gtkOffloadReqParams.ulFlags))
        {

            /* Host driver has previously  offloaded GTK rekey  */
            ret = sme_GetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                wlan_hdd_cfg80211_update_replayCounterCallback,
                                pAdapter, pAdapter->sessionId);
            if (eHAL_STATUS_SUCCESS != ret)

            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: sme_GetGTKOffload failed, returned %d",
                       __func__, ret);
                return;
            }
            else
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       "%s: sme_GetGTKOffload successful",
                       __func__);

                /* Sending GTK offload dissable */
                memcpy(&hddGtkOffloadReqParams, &pHddStaCtx->gtkOffloadReqParams,
                      sizeof (tSirGtkOffloadParams));
                hddGtkOffloadReqParams.ulFlags = GTK_OFFLOAD_DISABLE;
                ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                &hddGtkOffloadReqParams, pAdapter->sessionId);
                if (eHAL_STATUS_SUCCESS != ret)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                            "%s: failed to dissable GTK offload, returned %d",
                            __func__, ret);
                    return;
                }
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                        "%s: successfully dissabled GTK offload request to HAL",
                        __func__);
            }
        }
    }
    return;
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD*/

void hdd_resume_wlan(void)
{
   hdd_context_t *pHddCtx = NULL;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed by Android OS",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return;
   }
   
   if (pHddCtx->isLogpInProgress)
   {
      hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: Ignore resume wlan, LOGP in progress!", __func__);
      return;
   }

   pHddCtx->hdd_wlan_suspended = FALSE;
   /*loop through all adapters. Concurrency */
   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_SOFTAP != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )
       {  // we skip this registration for modes other than STA, SAP and P2P client modes.
            status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
            continue;
       }


#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP   
       if(pHddCtx->hdd_ps_state == eHDD_SUSPEND_DEEP_SLEEP) 
       {
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed from deep sleep",__func__);
          hdd_exit_deep_sleep(pAdapter);
       }
#endif

      if(pHddCtx->hdd_ignore_dtim_enabled == TRUE)
      {
         /*Switch back to DTIM 1*/
         tSirSetPowerParamsReq powerRequest = { 0 }; 

         powerRequest.uIgnoreDTIM = pHddCtx->hdd_actual_ignore_DTIM_value;
         powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
         powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

         /*Disabled ModulatedDTIM if enabled on suspend*/
         if(pHddCtx->cfg_ini->enableModulatedDTIM)
             powerRequest.uDTIMPeriod = 0;

         /* Update ignoreDTIM and ListedInterval in CFG with default values */
         ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                          NULL, eANI_BOOLEAN_FALSE);
         ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval, 
                          NULL, eANI_BOOLEAN_FALSE);

         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
                        "Switch to DTIM%d",powerRequest.uListenInterval);
         sme_SetPowerParams( WLAN_HDD_GET_HAL_CTX(pAdapter), &powerRequest, FALSE);

         if (BMPS == pmcGetPmcState(pHddCtx->hHal))
         {
             /* put the device into full power */
             wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

             /* put the device back into BMPS */
             wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

             pHddCtx->hdd_ignore_dtim_enabled = FALSE;
         }
      }

      hdd_conf_resume_ind(pAdapter);

#ifdef FEATURE_WLAN_LPHB
      if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
          (pHddCtx->lphbEnableReq.enable))
      {
         tSirLPHBReq *hb_params = NULL;

         hb_params = (tSirLPHBReq *)vos_mem_malloc(sizeof(tSirLPHBReq));
         if (NULL == hb_params)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: hb_params alloc failed", __func__);
         }
         else
         {
            eHalStatus smeStatus;

            hb_params->cmd    = LPHB_SET_EN_PARAMS_INDID;
            hb_params->params.lphbEnableReq.enable  = 0;
            hb_params->params.lphbEnableReq.item    =
                              pHddCtx->lphbEnableReq.item;
            hb_params->params.lphbEnableReq.session =
                              pHddCtx->lphbEnableReq.session;
            /* If WLAN is suspend state, send enable command immediately */
            smeStatus = sme_LPHBConfigReq((tHalHandle)(pHddCtx->hHal),
                                          hb_params,
                                          NULL);
            if (eHAL_STATUS_SUCCESS != smeStatus)
            {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "LPHB Config Fail, disable");
               pHddCtx->lphbEnableReq.enable = 0;
               vos_mem_free(hb_params);
            }
         }
      }
#endif /* FEATURE_WLAN_LPHB */

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP   
   if(pHddCtx->hdd_ps_state == eHDD_SUSPEND_STANDBY) 
   {
       hdd_exit_standby(pHddCtx);
   }    
#endif

   return;
}

VOS_STATUS hdd_wlan_reset_initialization(void) 
{
   v_CONTEXT_t pVosContext = NULL;

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN being reset",__func__);  

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) 
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Preventing the phone from going to suspend",__func__);

   // Prevent the phone from going to sleep
   hdd_prevent_suspend();

   return VOS_STATUS_SUCCESS;
}


/*
 * Based on the ioctl command recieved by HDD, put WLAN driver
 * into the quiet mode. This is the same as the early suspend
 * notification that driver used to listen
 */
void hdd_set_wlan_suspend_mode(bool suspend)
{
    vos_ssr_protect(__func__);
    if (suspend)
        hdd_suspend_wlan();
    else
        hdd_resume_wlan();
    vos_ssr_unprotect(__func__);
}

static void hdd_ssr_timer_init(void)
{
    init_timer(&ssr_timer);
}

static void hdd_ssr_timer_del(void)
{
    del_timer(&ssr_timer);
    ssr_timer_started = false;
}

static void hdd_ssr_timer_cb(unsigned long data)
{
    hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD SSR timer expired", __func__);

#ifdef WCN_PRONTO
    if (wcnss_hardware_type() == WCNSS_PRONTO_HW)
        wcnss_pronto_log_debug_regs();
#endif

    VOS_BUG(0);
}

static void hdd_ssr_timer_start(int msec)
{
    if(ssr_timer_started)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: trying to start SSR timer when it's running"
                ,__func__);
    }
    ssr_timer.expires = jiffies + msecs_to_jiffies(msec);
    ssr_timer.function = hdd_ssr_timer_cb;
    add_timer(&ssr_timer);
    ssr_timer_started = true;
}

/* the HDD interface to WLAN driver shutdown,
 * the primary shutdown function in SSR
 */
VOS_STATUS hdd_wlan_shutdown(void)
{
   VOS_STATUS       vosStatus;
   v_CONTEXT_t      pVosContext = NULL;
   hdd_context_t    *pHddCtx = NULL;
   pVosSchedContext vosSchedContext = NULL;
   long ret;

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN driver shutting down! ",__func__);

   /* if re-init never happens, then do SSR1 */
   hdd_ssr_timer_init();
   hdd_ssr_timer_start(HDD_SSR_BRING_UP_TIME);

   /* Get the global VOSS context. */
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   /* Get the HDD context. */
   pHddCtx = (hdd_context_t*)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return VOS_STATUS_E_FAILURE;
   }

   //Stop the traffic monitor timer
   if ( VOS_TIMER_STATE_RUNNING ==
                        vos_timer_getCurrentState(&pHddCtx->tx_rx_trafficTmr))
   {
        vos_timer_stop(&pHddCtx->tx_rx_trafficTmr);
   }

   hdd_reset_all_adapters(pHddCtx);
   /* DeRegister with platform driver as client for Suspend/Resume */
   vosStatus = hddDeregisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDeregisterPmOps failed",__func__);
   }

   vosStatus = hddDevTmUnregisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmUnregisterNotifyCallback failed",__func__);
   }

   /* Disable IMPS/BMPS as we do not want the device to enter any power
    * save mode on its own during reset sequence
    */
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_UAPSD_MODE_POWER_SAVE);

   vosSchedContext = get_vos_sched_ctxt();

   /* Wakeup all driver threads */
   if(TRUE == pHddCtx->isMcThreadSuspended){
      complete(&vosSchedContext->ResumeMcEvent);
      pHddCtx->isMcThreadSuspended= FALSE;
   }
   if(TRUE == pHddCtx->isTxThreadSuspended){
      complete(&vosSchedContext->ResumeTxEvent);
      pHddCtx->isTxThreadSuspended= FALSE;
   }
   if(TRUE == pHddCtx->isRxThreadSuspended){
      complete(&vosSchedContext->ResumeRxEvent);
      pHddCtx->isRxThreadSuspended= FALSE;
   }
   /* Reset the Suspend Variable */
   pHddCtx->isWlanSuspended = FALSE;

   /* Stop all the threads; we do not want any messages to be a processed,
    * any more and the best way to ensure that is to terminate the threads
    * gracefully.
    */
   /* Wait for MC to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down MC thread",__func__);
   set_bit(MC_SHUTDOWN_EVENT_MASK, &vosSchedContext->mcEventFlag);
   set_bit(MC_POST_EVENT_MASK, &vosSchedContext->mcEventFlag);
   wake_up_interruptible(&vosSchedContext->mcWaitQueue);
   ret = wait_for_completion_interruptible(&vosSchedContext->McShutdown);
   if (0 >= ret)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s:wait on McShutdown failed %ld",
           __func__, ret);
   }

   /* Wait for TX to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down TX thread",__func__);
   set_bit(TX_SHUTDOWN_EVENT_MASK, &vosSchedContext->txEventFlag);
   set_bit(TX_POST_EVENT_MASK, &vosSchedContext->txEventFlag);
   wake_up_interruptible(&vosSchedContext->txWaitQueue);
   ret = wait_for_completion_interruptible(&vosSchedContext->TxShutdown);
   if (0 >= ret)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s:wait on TxShutdown failed %ld",
           __func__, ret);
   }

   /* Wait for RX to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down RX thread",__func__);
   set_bit(RX_SHUTDOWN_EVENT_MASK, &vosSchedContext->rxEventFlag);
   set_bit(RX_POST_EVENT_MASK, &vosSchedContext->rxEventFlag);
   wake_up_interruptible(&vosSchedContext->rxWaitQueue);

   ret = wait_for_completion_interruptible(&vosSchedContext->RxShutdown);
   if (0 >= ret)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s:wait on RxShutdown failed %ld",
           __func__, ret);
   }

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop BAP",__func__);
   }
#endif //WLAN_BTAMP_FEATURE
   vosStatus = vos_wda_shutdown(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop wda %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing SME STOP",__func__);
   /* Stop SME - Cannot invoke vos_stop as vos_stop relies
    * on threads being running to process the SYS Stop
    */
   vosStatus = sme_Stop(pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop sme %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing MAC STOP",__func__);
   /* Stop MAC (PE and HAL) */
   vosStatus = macStop(pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop mac %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing TL STOP",__func__);
   /* Stop TL */
   vosStatus = WLANTL_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop TL %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hdd_unregister_mcast_bcast_filter(pHddCtx);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Flush Queues",__func__);
   /* Clean up message queues of TX and MC thread */
   vos_sched_flush_mc_mqs(vosSchedContext);
   vos_sched_flush_tx_mqs(vosSchedContext);
   vos_sched_flush_rx_mqs(vosSchedContext);

   /* Deinit all the TX and MC queues */
   vos_sched_deinit_mqs(vosSchedContext);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Doing VOS Shutdown",__func__);

   /* shutdown VOSS */
   vos_shutdown(pVosContext);

   /*mac context has already been released in mac_close call
     so setting it to NULL in hdd context*/
   pHddCtx->hHal = (tHalHandle)NULL;

   if (free_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to free power on lock",
                                           __func__);
   }
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN driver shutdown complete"
                                   ,__func__);
   return VOS_STATUS_SUCCESS;
}



/* the HDD interface to WLAN driver re-init.
 * This is called to initialize/start WLAN driver after a shutdown.
 */
VOS_STATUS hdd_wlan_re_init(void)
{
   VOS_STATUS       vosStatus;
   v_CONTEXT_t      pVosContext = NULL;
   hdd_context_t    *pHddCtx = NULL;
   eHalStatus       halStatus;
#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   int              max_retries = 0;
#endif
#ifdef WLAN_BTAMP_FEATURE
   hdd_config_t     *pConfig = NULL;
   WLANBAP_ConfigType btAmpConfig;
#endif

   hdd_ssr_timer_del();
   hdd_prevent_suspend();

#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   /* wait until WCNSS driver downloads NV */
   while (!wcnss_device_ready() && 10 >= ++max_retries) {
       msleep(1000);
   }
   if (max_retries >= 10) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WCNSS driver not ready", __func__);
      goto err_re_init;
   }
#endif

   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   /* The driver should always be initialized in STA mode after SSR */
   hdd_set_conparam(0);

   /* Re-open VOSS, it is a re-open b'se control transport was never closed. */
   vosStatus = vos_open(&pVosContext, 0);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed",__func__);
      goto err_re_init;
   }

   /* Get the HDD context. */
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      goto err_vosclose;
   }

   /* Save the hal context in Adapter */
   pHddCtx->hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pVosContext );
   if ( NULL == pHddCtx->hHal )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HAL context is null",__func__);
      goto err_vosclose;
   }

   /* Set the SME configuration parameters. */
   vosStatus = hdd_set_sme_config(pHddCtx);
   if ( VOS_STATUS_SUCCESS != vosStatus )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed hdd_set_sme_config",__func__);
      goto err_vosclose;
   }

   /* Initialize the WMM module */
   vosStatus = hdd_wmm_init(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_wmm_init failed", __func__);
      goto err_vosclose;
   }

   vosStatus = vos_preStart( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_preStart failed",__func__);
      goto err_vosclose;
   }

   /* In the integrated architecture we update the configuration from
      the INI file and from NV before vOSS has been started so that
      the final contents are available to send down to the cCPU   */
   /* Apply the cfg.ini to cfg.dat */
   if (FALSE == hdd_update_config_dat(pHddCtx))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: config update failed",__func__ );
      goto err_vosclose;
   }

   /* Set the MAC Address, currently this is used by HAL to add self sta.
    * Remove this once self sta is added as part of session open. */
   halStatus = cfgSetStr(pHddCtx->hHal, WNI_CFG_STA_ID,
         (v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[0],
           sizeof(pHddCtx->cfg_ini->intfMacAddr[0]));
   if (!HAL_STATUS_SUCCESS(halStatus))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed to set MAC Address. "
            "HALStatus is %08d [x%08x]",__func__, halStatus, halStatus);
      goto err_vosclose;
   }

   /* Start VOSS which starts up the SME/MAC/HAL modules and everything else
      Note: Firmware image will be read and downloaded inside vos_start API */
   vosStatus = vos_start( pVosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
      goto err_vosclose;
   }

   /* Exchange capability info between Host and FW and also get versioning info from FW */
   hdd_exchange_version_and_caps(pHddCtx);

   vosStatus = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed",
         __func__);
      goto err_vosstop;
   }

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Open(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to open BAP",__func__);
      goto err_vosstop;
   }
   vosStatus = BSL_Init(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to Init BSL",__func__);
     goto err_bap_close;
   }
   vosStatus = WLANBAP_Start(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to start TL",__func__);
       goto err_bap_close;
   }
   pConfig = pHddCtx->cfg_ini;
   btAmpConfig.ucPreferredChannel = pConfig->preferredChannel;
   vosStatus = WLANBAP_SetConfig(&btAmpConfig);
#endif //WLAN_BTAMP_FEATURE

    /* Restart all adapters */
   hdd_start_all_adapters(pHddCtx);
   pHddCtx->isLogpInProgress = FALSE;
   vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   pHddCtx->hdd_mcastbcast_filter_set = FALSE;
   hdd_register_mcast_bcast_filter(pHddCtx);

   /* Register with platform driver as client for Suspend/Resume */
   vosStatus = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
      goto err_bap_stop;
   }
   /* Allow the phone to go to sleep */
   hdd_allow_suspend();
   /* register for riva power on lock */
   if (req_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: req riva power on lock failed",
                                        __func__);
      goto err_unregister_pmops;
   }
   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   goto success;

err_unregister_pmops:
   hddDeregisterPmOps(pHddCtx);

err_bap_stop:
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_unregister_mcast_bcast_filter(pHddCtx);
#endif
   hdd_close_all_adapters(pHddCtx);
#ifdef WLAN_BTAMP_FEATURE
   WLANBAP_Stop(pVosContext);
#endif

#ifdef WLAN_BTAMP_FEATURE
err_bap_close:
   WLANBAP_Close(pVosContext);
#endif

err_vosstop:
   vos_stop(pVosContext);

err_vosclose:
   vos_close(pVosContext);
   vos_sched_close(pVosContext);
   if (pHddCtx)
   {
       /* Unregister the Net Device Notifier */
       unregister_netdevice_notifier(&hdd_netdev_notifier);
       /* Clean up HDD Nlink Service */
       send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
#ifdef WLAN_KD_READY_NOTIFIER
       nl_srv_exit(pHddCtx->ptt_pid);
#else
       nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
       /* Free up dynamically allocated members inside HDD Adapter */
       kfree(pHddCtx->cfg_ini);
       pHddCtx->cfg_ini= NULL;

       wiphy_unregister(pHddCtx->wiphy);
       wiphy_free(pHddCtx->wiphy);
   }
   vos_preClose(&pVosContext);

#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

err_re_init:
   /* Allow the phone to go to sleep */
   hdd_allow_suspend();
   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   VOS_BUG(0);
   return -EPERM;

success:
   /* Trigger replay of BTC events */
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
   return VOS_STATUS_SUCCESS;
}
