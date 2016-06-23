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




/*========================================================================

  \file  wlan_hdd_main.c

  \brief WLAN Host Device Driver implementation

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  04/5/09     Shailender     Created module.
  02/24/10    Sudhir.S.Kohalli  Added to support param for SoftAP module
  06/03/10    js - Added support to hostapd driven deauth/disassoc/mic failure
  ==========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
//#include <wlan_qct_driver.h>
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#ifdef ANI_BUS_TYPE_PLATFORM
#include <linux/wcnss_wlan.h>
#endif //ANI_BUS_TYPE_PLATFORM
#ifdef ANI_BUS_TYPE_PCI
#include "wcnss_wlan.h"
#endif /* ANI_BUS_TYPE_PCI */
#include <wlan_hdd_tx_rx.h>
#include <palTimer.h>
#include <wniApi.h>
#include <wlan_nlink_srv.h>
#include <wlan_btc_svc.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <wlan_logging_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#ifdef WLAN_BTAMP_FEATURE
#include <bap_hdd_main.h>
#include <bapInternal.h>
#endif // WLAN_BTAMP_FEATURE
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_p2p.h"
#include <linux/rtnetlink.h>
int wlan_hdd_ftm_start(hdd_context_t *pAdapter);
#include "sapApi.h"
#include <linux/semaphore.h>
#include <linux/ctype.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#include <soc/qcom/subsystem_restart.h>
#else
#include <mach/subsystem_restart.h>
#endif
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "cfgApi.h"
#include "wlan_hdd_dev_pwr.h"
#ifdef WLAN_BTAMP_FEATURE
#include "bap_hdd_misc.h"
#endif
#include "wlan_qct_pal_trace.h"
#include "qwlan_version.h"
#include "wlan_qct_wda.h"
#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#endif
#include "wlan_hdd_debugfs.h"

#ifdef MODULE
#define WLAN_MODULE_NAME  module_name(THIS_MODULE)
#else
#define WLAN_MODULE_NAME  "wlan"
#endif

#ifdef TIMER_MANAGER
#define TIMER_MANAGER_STR " +TIMER_MANAGER"
#else
#define TIMER_MANAGER_STR ""
#endif

#ifdef MEMORY_DEBUG
#define MEMORY_DEBUG_STR " +MEMORY_DEBUG"
#else
#define MEMORY_DEBUG_STR ""
#endif
#define MAX_WAIT_FOR_ROC_COMPLETION 3
/* the Android framework expects this param even though we don't use it */
#define BUF_LEN 20
static char fwpath_buffer[BUF_LEN];
static struct kparam_string fwpath = {
   .string = fwpath_buffer,
   .maxlen = BUF_LEN,
};

static char *country_code;
static int   enable_11d = -1;
static int   enable_dfs_chan_scan = -1;
static int   gbcnMissRate = -1;

#ifndef MODULE
static int wlan_hdd_inited;
#endif

/*
 * spinlock for synchronizing asynchronous request/response
 * (full description of use in wlan_hdd_main.h)
 */
DEFINE_SPINLOCK(hdd_context_lock);

/*
 * The rate at which the driver sends RESTART event to supplicant
 * once the function 'vos_wlanRestart()' is called
 *
 */
#define WLAN_HDD_RESTART_RETRY_DELAY_MS 5000  /* 5 second */
#define WLAN_HDD_RESTART_RETRY_MAX_CNT  5     /* 5 retries */

/*
 * Size of Driver command strings from upper layer
 */
#define SIZE_OF_SETROAMMODE             11    /* size of SETROAMMODE */
#define SIZE_OF_GETROAMMODE             11    /* size of GETROAMMODE */

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
#define TID_MIN_VALUE 0
#define TID_MAX_VALUE 15
static VOS_STATUS  hdd_get_tsm_stats(hdd_adapter_t *pAdapter, const tANI_U8 tid,
                                         tAniTrafStrmMetrics* pTsmMetrics);
static VOS_STATUS hdd_parse_ese_beacon_req(tANI_U8 *pValue,
                                     tCsrEseBeaconReq *pEseBcnReq);
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/*
 * Maximum buffer size used for returning the data back to user space
 */
#define WLAN_MAX_BUF_SIZE 1024
#define WLAN_PRIV_DATA_MAX_LEN    8192

//wait time for beacon miss rate.
#define BCN_MISS_RATE_TIME 500

#ifdef WLAN_OPEN_SOURCE
static struct wake_lock wlan_wake_lock;
#endif
/* set when SSR is needed after unload */
static e_hdd_ssr_required isSsrRequired = HDD_SSR_NOT_REQUIRED;

//internal function declaration
static VOS_STATUS wlan_hdd_framework_restart(hdd_context_t *pHddCtx);
static void wlan_hdd_restart_init(hdd_context_t *pHddCtx);
static void wlan_hdd_restart_deinit(hdd_context_t *pHddCtx);
void wlan_hdd_restart_timer_cb(v_PVOID_t usrDataForCallback);
void hdd_set_wlan_suspend_mode(bool suspend);

v_U16_t hdd_select_queue(struct net_device *dev,
    struct sk_buff *skb);

#ifdef WLAN_FEATURE_PACKET_FILTERING
static void hdd_set_multicast_list(struct net_device *dev);
#endif

void hdd_wlan_initial_scan(hdd_adapter_t *pAdapter);

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
void hdd_getBand_helper(hdd_context_t *pHddCtx, int *pBand);
static VOS_STATUS hdd_parse_channellist(tANI_U8 *pValue, tANI_U8 *pChannelList, tANI_U8 *pNumChannels);
static VOS_STATUS hdd_parse_send_action_frame_data(tANI_U8 *pValue, tANI_U8 *pTargetApBssid,
                              tANI_U8 *pChannel, tANI_U8 *pDwellTime,
                              tANI_U8 **pBuf, tANI_U8 *pBufLen);
static VOS_STATUS hdd_parse_reassoc_command_data(tANI_U8 *pValue,
                                                 tANI_U8 *pTargetApBssid,
                                                 tANI_U8 *pChannel);
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
VOS_STATUS hdd_parse_get_cckm_ie(tANI_U8 *pValue, tANI_U8 **pCckmIe, tANI_U8 *pCckmIeLen);
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

static VOS_STATUS wlan_hdd_init_channels(hdd_context_t *pHddCtx);
const char * hdd_device_modetoString(v_U8_t device_mode)
{
   switch(device_mode)
   {
       CASE_RETURN_STRING( WLAN_HDD_INFRA_STATION );
       CASE_RETURN_STRING( WLAN_HDD_SOFTAP );
       CASE_RETURN_STRING( WLAN_HDD_P2P_CLIENT );
       CASE_RETURN_STRING( WLAN_HDD_P2P_GO );
       CASE_RETURN_STRING( WLAN_HDD_MONITOR);
       CASE_RETURN_STRING( WLAN_HDD_FTM );
       CASE_RETURN_STRING( WLAN_HDD_IBSS );
       CASE_RETURN_STRING( WLAN_HDD_P2P_DEVICE );
       default:
           return "device_mode Unknown";
   }
}

static int hdd_netdev_notifier_call(struct notifier_block * nb,
                                         unsigned long state,
                                         void *ndev)
{
   struct net_device *dev = ndev;
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
#ifdef WLAN_BTAMP_FEATURE
   VOS_STATUS status;
#endif
   long result;

   //Make sure that this callback corresponds to our device.
   if ((strncmp(dev->name, "wlan", 4)) &&
      (strncmp(dev->name, "p2p", 3)))
      return NOTIFY_DONE;

   if (!dev->ieee80211_ptr)
      return NOTIFY_DONE;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD Adapter Null Pointer", __func__);
      VOS_ASSERT(0);
      return NOTIFY_DONE;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   if (NULL == pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD Context Null Pointer", __func__);
      VOS_ASSERT(0);
      return NOTIFY_DONE;
   }
   if (pHddCtx->isLogpInProgress)
      return NOTIFY_DONE;


   hddLog(VOS_TRACE_LEVEL_INFO, "%s: %s New Net Device State = %lu",
          __func__, dev->name, state);

   switch (state) {
   case NETDEV_REGISTER:
        break;

   case NETDEV_UNREGISTER:
        break;

   case NETDEV_UP:
        break;

   case NETDEV_DOWN:
        break;

   case NETDEV_CHANGE:
        if(TRUE == pAdapter->isLinkUpSvcNeeded)
           complete(&pAdapter->linkup_event_var);
        break;

   case NETDEV_GOING_DOWN:
        result = wlan_hdd_scan_abort(pAdapter);
        if (result < 0)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: Timeout occurred while waiting for abortscan %ld",
                        __func__, result);
        }
        else
        {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: Scan Abort Successful" , __func__);
        }
#ifdef WLAN_BTAMP_FEATURE
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%s: disabling AMP", __func__);
        status = WLANBAP_StopAmp();
        if(VOS_STATUS_SUCCESS != status )
        {
           pHddCtx->isAmpAllowed = VOS_TRUE;
           hddLog(VOS_TRACE_LEVEL_FATAL,
                  "%s: Failed to stop AMP", __func__);
        }
        else
        {
           //a state m/c implementation in PAL is TBD to avoid this delay
           msleep(500);
           if ( pHddCtx->isAmpAllowed )
           {
                WLANBAP_DeregisterFromHCI();
                pHddCtx->isAmpAllowed = VOS_FALSE;
           }
        }
#endif //WLAN_BTAMP_FEATURE
        break;

   default:
        break;
   }

   return NOTIFY_DONE;
}

struct notifier_block hdd_netdev_notifier = {
   .notifier_call = hdd_netdev_notifier_call,
};

/*---------------------------------------------------------------------------
 *   Function definitions
 *-------------------------------------------------------------------------*/
void hdd_unregister_mcast_bcast_filter(hdd_context_t *pHddCtx);
void hdd_register_mcast_bcast_filter(hdd_context_t *pHddCtx);
//variable to hold the insmod parameters
static int con_mode;
#ifndef MODULE
/* current con_mode - used only for statically linked driver
 * con_mode is changed by userspace to indicate a mode change which will
 * result in calling the module exit and init functions. The module
 * exit function will clean up based on the value of con_mode prior to it
 * being changed by userspace. So curr_con_mode records the current con_mode 
 * for exit when con_mode becomes the next mode for init
 */
static int curr_con_mode;
#endif

/**---------------------------------------------------------------------------

  \brief hdd_vos_trace_enable() - Configure initial VOS Trace enable

  Called immediately after the cfg.ini is read in order to configure
  the desired trace levels.

  \param  - moduleId - module whose trace level is being configured
  \param  - bitmask - bitmask of log levels to be enabled

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_vos_trace_enable(VOS_MODULE_ID moduleId, v_U32_t bitmask)
{
   wpt_tracelevel level;

   /* if the bitmask is the default value, then a bitmask was not
      specified in cfg.ini, so leave the logging level alone (it
      will remain at the "compiled in" default value) */
   if (CFG_VOS_TRACE_ENABLE_DEFAULT == bitmask)
   {
      return;
   }

   /* a mask was specified.  start by disabling all logging */
   vos_trace_setValue(moduleId, VOS_TRACE_LEVEL_NONE, 0);

   /* now cycle through the bitmask until all "set" bits are serviced */
   level = VOS_TRACE_LEVEL_FATAL;
   while (0 != bitmask)
   {
      if (bitmask & 1)
      {
         vos_trace_setValue(moduleId, level, 1);
      }
      level++;
      bitmask >>= 1;
   }
}


/**---------------------------------------------------------------------------

  \brief hdd_wdi_trace_enable() - Configure initial WDI Trace enable

  Called immediately after the cfg.ini is read in order to configure
  the desired trace levels in the WDI.

  \param  - moduleId - module whose trace level is being configured
  \param  - bitmask - bitmask of log levels to be enabled

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_wdi_trace_enable(wpt_moduleid moduleId, v_U32_t bitmask)
{
   wpt_tracelevel level;

   /* if the bitmask is the default value, then a bitmask was not
      specified in cfg.ini, so leave the logging level alone (it
      will remain at the "compiled in" default value) */
   if (CFG_WDI_TRACE_ENABLE_DEFAULT == bitmask)
   {
      return;
   }

   /* a mask was specified.  start by disabling all logging */
   wpalTraceSetLevel(moduleId, eWLAN_PAL_TRACE_LEVEL_NONE, 0);

   /* now cycle through the bitmask until all "set" bits are serviced */
   level = eWLAN_PAL_TRACE_LEVEL_FATAL;
   while (0 != bitmask)
   {
      if (bitmask & 1)
      {
         wpalTraceSetLevel(moduleId, level, 1);
      }
      level++;
      bitmask >>= 1;
   }
}

/*
 * FUNCTION: wlan_hdd_validate_context
 * This function is used to check the HDD context
 */
int wlan_hdd_validate_context(hdd_context_t *pHddCtx)
{
    ENTER();

    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null", __func__);
        return -ENODEV;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP %s. Ignore!!", __func__,
                    vos_is_wlan_in_badState(VOS_MODULE_ID_HDD, NULL)
                    ?"failed":"in Progress");
        return -EAGAIN;
    }

    if (WLAN_HDD_IS_LOAD_UNLOAD_IN_PROGRESS(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
        return -EAGAIN;
    }
    return 0;
}
#ifdef CONFIG_ENABLE_LINUX_REG
void hdd_checkandupdate_phymode( hdd_context_t *pHddCtx)
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_station_ctx_t *pHddStaCtx = NULL;
   eCsrPhyMode phyMode;
   hdd_config_t *cfg_param = NULL;

   if (NULL == pHddCtx)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "HDD Context is null !!");
       return ;
   }

   pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
   if (NULL == pAdapter)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "pAdapter is null !!");
       return ;
   }

   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   if (NULL == pHddStaCtx)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "pHddStaCtx is null !!");
       return ;
   }

   cfg_param = pHddCtx->cfg_ini;
   if (NULL == cfg_param)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "cfg_params not available !!");
       return ;
   }

   phyMode = sme_GetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter));

   if (!pHddCtx->isVHT80Allowed)
   {
       if ((eCSR_DOT11_MODE_AUTO == phyMode) ||
           (eCSR_DOT11_MODE_11ac == phyMode) ||
           (eCSR_DOT11_MODE_11ac_ONLY == phyMode))
       {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "Setting phymode to 11n!!");
           sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter), eCSR_DOT11_MODE_11n);
       }
   }
   else
   {
       /*New country Supports 11ac as well resetting value back from .ini*/
       sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter),
             hdd_cfg_xlate_to_csr_phy_mode(cfg_param->dot11Mode));
       return ;
   }

   if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
       ((eCSR_CFG_DOT11_MODE_11AC_ONLY == pHddStaCtx->conn_info.dot11Mode) ||
        (eCSR_CFG_DOT11_MODE_11AC == pHddStaCtx->conn_info.dot11Mode)))
   {
       VOS_STATUS vosStatus;

       // need to issue a disconnect to CSR.
       INIT_COMPLETION(pAdapter->disconnect_comp_var);
       vosStatus = sme_RoamDisconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
                          pAdapter->sessionId,
                          eCSR_DISCONNECT_REASON_UNSPECIFIED );

       if (VOS_STATUS_SUCCESS == vosStatus)
       {
           long ret;

           ret = wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var,
                 msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
           if (0 >= ret)
               hddLog(LOGE, FL("failure waiting for disconnect_comp_var %ld"),
                                ret);
       }

   }
}
#else
void hdd_checkandupdate_phymode( hdd_adapter_t *pAdapter, char *country_code)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_config_t *cfg_param;
    eCsrPhyMode phyMode;
    long ret;

    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "HDD Context is null !!");
        return ;
    }

    cfg_param = pHddCtx->cfg_ini;

    if (NULL == cfg_param)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "cfg_params not available !!");
        return ;
    }

    phyMode = sme_GetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter));

    if (NULL != strstr(cfg_param->listOfNon11acCountryCode, country_code))
    {
        if ((eCSR_DOT11_MODE_AUTO == phyMode) ||
            (eCSR_DOT11_MODE_11ac == phyMode) ||
            (eCSR_DOT11_MODE_11ac_ONLY == phyMode))
        {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "Setting phymode to 11n!!");
            sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter), eCSR_DOT11_MODE_11n);
        }
    }
    else
    {
        /*New country Supports 11ac as well resetting value back from .ini*/
        sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter),
              hdd_cfg_xlate_to_csr_phy_mode(cfg_param->dot11Mode));
        return ;
    }

    if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
        ((eCSR_CFG_DOT11_MODE_11AC_ONLY == pHddStaCtx->conn_info.dot11Mode) ||
         (eCSR_CFG_DOT11_MODE_11AC == pHddStaCtx->conn_info.dot11Mode)))
    {
        VOS_STATUS vosStatus;

        // need to issue a disconnect to CSR.
        INIT_COMPLETION(pAdapter->disconnect_comp_var);
        vosStatus = sme_RoamDisconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
                           pAdapter->sessionId,
                           eCSR_DISCONNECT_REASON_UNSPECIFIED );

        if (VOS_STATUS_SUCCESS == vosStatus)
        {
            ret = wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var,
                  msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
            if (ret <= 0)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "wait on disconnect_comp_var is failed %ld", ret);
            }
        }

    }
}
#endif //CONFIG_ENABLE_LINUX_REG

void hdd_checkandupdate_dfssetting( hdd_adapter_t *pAdapter, char *country_code)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_config_t *cfg_param;

    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "HDD Context is null !!");
        return ;
    }

    cfg_param = pHddCtx->cfg_ini;

    if (NULL == cfg_param)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "cfg_params not available !!");
        return ;
    }

    if (NULL != strstr(cfg_param->listOfNonDfsCountryCode, country_code) ||
        pHddCtx->disable_dfs_flag == TRUE)
    {
       /*New country doesn't support DFS */
       sme_UpdateDfsSetting(WLAN_HDD_GET_HAL_CTX(pAdapter), 0);
    }
    else
    {
       /*New country Supports DFS as well resetting value back from .ini*/
       sme_UpdateDfsSetting(WLAN_HDD_GET_HAL_CTX(pAdapter), cfg_param->enableDFSChnlScan);
    }

}

#ifdef FEATURE_WLAN_BATCH_SCAN

/**---------------------------------------------------------------------------

  \brief hdd_extract_assigned_int_from_str() - Extracts assigned integer from
                                               input string

  This function extracts assigned integer from string in below format:
  "STRING=10" : extracts integer 10 from this string

  \param  - pInPtr Pointer to input string
  \param  - base  Base for string to int conversion(10 for decimal 16 for hex)
  \param  - pOutPtr Pointer to variable in which extracted integer needs to be
            assigned
  \param  - pLastArg to tell whether it is last arguement in input string or
            not

  \return - NULL for failure cases
            pointer to next arguement in input string for success cases
  --------------------------------------------------------------------------*/
static tANI_U8 *
hdd_extract_assigned_int_from_str
(
    tANI_U8 *pInPtr,
    tANI_U8 base,
    tANI_U32 *pOutPtr,
    tANI_U8 *pLastArg
)
{
    int tempInt;
    int v = 0;
    char buf[32];
    int val = 0;
    *pLastArg = FALSE;

    pInPtr = strnchr(pInPtr, strlen(pInPtr), EQUALS_TO_ASCII_VALUE);
    if (NULL == pInPtr)
    {
        return NULL;
    }

    pInPtr++;

    while ((SPACE_ASCII_VALUE  == *pInPtr) && ('\0' !=  *pInPtr)) pInPtr++;

    val = sscanf(pInPtr, "%32s ", buf);
    if (val < 0 && val > strlen(pInPtr))
    {
        return NULL;
    }
    pInPtr += val;
    v = kstrtos32(buf, base, &tempInt);
    if (v < 0)
    {
        return NULL;
    }
    if (tempInt < 0)
    {
        tempInt = 0;
    }
    *pOutPtr = tempInt;

    pInPtr = strnchr(pInPtr, strlen(pInPtr), SPACE_ASCII_VALUE);
    if (NULL == pInPtr)
    {
        *pLastArg = TRUE;
        return NULL;
    }
    while ((SPACE_ASCII_VALUE  == *pInPtr) && ('\0' !=  *pInPtr)) pInPtr++;

    return pInPtr;
}

/**---------------------------------------------------------------------------

  \brief hdd_extract_assigned_char_from_str() - Extracts assigned char from
                                                input string

  This function extracts assigned character from string in below format:
  "STRING=A" : extracts char 'A' from this string

  \param  - pInPtr Pointer to input string
  \param  - pOutPtr Pointer to variable in which extracted char needs to be
            assigned
  \param  - pLastArg to tell whether it is last arguement in input string or
            not

  \return - NULL for failure cases
            pointer to next arguement in input string for success cases
  --------------------------------------------------------------------------*/
static tANI_U8 *
hdd_extract_assigned_char_from_str
(
    tANI_U8 *pInPtr,
    tANI_U8 *pOutPtr,
    tANI_U8 *pLastArg
)
{
    *pLastArg = FALSE;

    pInPtr = strnchr(pInPtr, strlen(pInPtr), EQUALS_TO_ASCII_VALUE);
    if (NULL == pInPtr)
    {
        return NULL;
    }

    pInPtr++;

    while ((SPACE_ASCII_VALUE  == *pInPtr) && ('\0' !=  *pInPtr)) pInPtr++;

    *pOutPtr = *pInPtr;

    pInPtr = strnchr(pInPtr, strlen(pInPtr), SPACE_ASCII_VALUE);
    if (NULL == pInPtr)
    {
        *pLastArg = TRUE;
        return NULL;
    }
    while ((SPACE_ASCII_VALUE  == *pInPtr) && ('\0' !=  *pInPtr)) pInPtr++;

    return pInPtr;
}


/**---------------------------------------------------------------------------

  \brief hdd_parse_set_batchscan_command () - HDD parse set batch scan command

  This function parses set batch scan command in below format:
  WLS_BATCHING_SET <space> followed by below arguements
  "SCANFREQ=XX"   : Optional defaults to 30 sec
  "MSCAN=XX"      : Required number of scans to attempt to batch
  "BESTN=XX"      : Best Network (RSSI) defaults to 16
  "CHANNEL=<X,Y>" : optional defaults to all channels, can list 'A'or` B.
                    A. implies  only 5 GHz , B. implies only 2.4GHz
  "RTT=X"         : optional defaults to 0
  returns the MIN of MSCAN or the max # of scans firmware can cache or -1 on
  error

  For example input commands:
  1) WLS_BATCHING_SET SCANFREQ=60 MSCAN=10 BESTN=20 CHANNEL=A RTT=0 -> This is
     translated into set batch scan with following parameters:
     a) Frequence 60 seconds
     b) Batch 10 scans together
     c) Best RSSI to be 20
     d) 5GHz band only
     e) RTT is equal to 0

  \param  - pValue Pointer to input channel list
  \param  - pHddSetBatchScanReq Pointer to HDD batch scan request structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_set_batchscan_command
(
    tANI_U8 *pValue,
    tSirSetBatchScanReq *pHddSetBatchScanReq
)
{
    tANI_U8 *inPtr = pValue;
    tANI_U8 val = 0;
    tANI_U8 lastArg = 0;
    tANI_U32 nScanFreq;
    tANI_U32 nMscan;
    tANI_U32 nBestN;
    tANI_U8  ucRfBand;
    tANI_U32 nRtt;
    tANI_U32 temp;

    /*initialize default values*/
    nScanFreq = HDD_SET_BATCH_SCAN_DEFAULT_FREQ;
    ucRfBand = HDD_SET_BATCH_SCAN_DEFAULT_BAND;
    nRtt = 0;
    nBestN = HDD_SET_BATCH_SCAN_BEST_NETWORK;

    /*go to space after WLS_BATCHING_SET command*/
    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*check and parse SCANFREQ*/
    if ((strncmp(inPtr, "SCANFREQ", 8) == 0))
    {
        inPtr = hdd_extract_assigned_int_from_str(inPtr, 10,
                    &temp, &lastArg);

        if (0 != temp)
        {
           nScanFreq = temp;
        }

        if ( (NULL == inPtr) || (TRUE == lastArg))
        {
            return -EINVAL;
        }
    }

    /*check and parse MSCAN*/
    if ((strncmp(inPtr, "MSCAN", 5) == 0))
    {
        inPtr = hdd_extract_assigned_int_from_str(inPtr, 10,
                    &nMscan, &lastArg);

        if (0 == nMscan)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "invalid MSCAN=%d", nMscan);
            return -EINVAL;
        }

        if (TRUE == lastArg)
        {
            goto done;
        }
        else if (NULL == inPtr)
        {
            return -EINVAL;
        }
    }
    else
    {
        return -EINVAL;
    }

    /*check and parse BESTN*/
    if ((strncmp(inPtr, "BESTN", 5) == 0))
    {
        inPtr = hdd_extract_assigned_int_from_str(inPtr, 10,
                    &temp, &lastArg);

        if (0 != temp)
        {
           nBestN = temp;
        }

        if (TRUE == lastArg)
        {
            goto done;
        }
        else if (NULL == inPtr)
        {
            return -EINVAL;
        }
    }

    /*check and parse CHANNEL*/
    if ((strncmp(inPtr, "CHANNEL", 7) == 0))
    {
        inPtr = hdd_extract_assigned_char_from_str(inPtr, &val, &lastArg);

        if (('A' == val) || ('a' == val))
        {
            ucRfBand = HDD_SET_BATCH_SCAN_5GHz_BAND_ONLY;
        }
        else if (('B' == val) || ('b' == val))
        {
            ucRfBand = HDD_SET_BATCH_SCAN_24GHz_BAND_ONLY;
        }
        else
        {
            ucRfBand = HDD_SET_BATCH_SCAN_DEFAULT_BAND;
        }

        if (TRUE == lastArg)
        {
            goto done;
        }
        else if (NULL == inPtr)
        {
            return -EINVAL;
        }
    }

    /*check and parse RTT*/
    if ((strncmp(inPtr, "RTT", 3) == 0))
    {
        inPtr = hdd_extract_assigned_int_from_str(inPtr, 10,
                    &nRtt, &lastArg);
        if (TRUE == lastArg)
        {
            goto done;
        }
        if (NULL == inPtr)
        {
            return -EINVAL;
        }
    }


done:

    pHddSetBatchScanReq->scanFrequency = nScanFreq;
    pHddSetBatchScanReq->numberOfScansToBatch = nMscan;
    pHddSetBatchScanReq->bestNetwork = nBestN;
    pHddSetBatchScanReq->rfBand = ucRfBand;
    pHddSetBatchScanReq->rtt = nRtt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
      "Received WLS_BATCHING_SET with SCANFREQ=%d "
      "MSCAN=%d BESTN=%d CHANNEL=%d RTT=%d",
      pHddSetBatchScanReq->scanFrequency,
      pHddSetBatchScanReq->numberOfScansToBatch,
      pHddSetBatchScanReq->bestNetwork,
      pHddSetBatchScanReq->rfBand,
      pHddSetBatchScanReq->rtt);

    return 0;
}/*End of hdd_parse_set_batchscan_command*/

/**---------------------------------------------------------------------------

  \brief hdd_set_batch_scan_req_callback () - This function is called after
     receiving set batch scan response from FW and it saves set batch scan
     response data FW to HDD context and sets the completion event on
     which hdd_ioctl is waiting

  \param  - callbackContext Pointer to HDD adapter
  \param  - pRsp Pointer to set batch scan response data received from FW

  \return - nothing

  --------------------------------------------------------------------------*/
static void hdd_set_batch_scan_req_callback
(
    void *callbackContext,
    tSirSetBatchScanRsp *pRsp
)
{
    hdd_adapter_t* pAdapter = (hdd_adapter_t*)callbackContext;
    tSirSetBatchScanRsp *pHddSetBatchScanRsp;

    /*sanity check*/
    if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid pAdapter magic", __func__);
        VOS_ASSERT(0);
        return;
    }
    pHddSetBatchScanRsp = &pAdapter->hddSetBatchScanRsp;

    /*save set batch scan response*/
    pHddSetBatchScanRsp->nScansToBatch = pRsp->nScansToBatch;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
      "Received set batch scan rsp from FW with nScansToBatch=%d",
      pHddSetBatchScanRsp->nScansToBatch);

    pAdapter->hdd_wait_for_set_batch_scan_rsp = FALSE;
    complete(&pAdapter->hdd_set_batch_scan_req_var);

    return;
}/*End of hdd_set_batch_scan_req_callback*/


/**---------------------------------------------------------------------------

  \brief hdd_populate_batch_scan_rsp_queue () - This function stores AP meta
     info in hdd batch scan response queue

  \param  - pAdapter Pointer to hdd adapter
  \param  - pAPMetaInfo Pointer to access point meta info
  \param  - scanId scan ID of batch scan response
  \param  - isLastAp tells whether AP is last AP in batch scan response or not

  \return - nothing

  --------------------------------------------------------------------------*/
static void hdd_populate_batch_scan_rsp_queue( hdd_adapter_t* pAdapter,
      tpSirBatchScanNetworkInfo pApMetaInfo, tANI_U32 scanId, v_BOOL_t isLastAp)
{
    tHddBatchScanRsp *pHead;
    tHddBatchScanRsp *pNode;
    tHddBatchScanRsp *pPrev;
    tHddBatchScanRsp *pTemp;
    tANI_U8 ssidLen;

    /*head of hdd batch scan response queue*/
    pHead = pAdapter->pBatchScanRsp;

    pNode = (tHddBatchScanRsp *)vos_mem_malloc(sizeof(tHddBatchScanRsp));
    if (NULL == pNode)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Could not allocate memory", __func__);
        VOS_ASSERT(0);
        return;
    }

    vos_mem_copy(pNode->ApInfo.bssid, pApMetaInfo->bssid,
        sizeof(pNode->ApInfo.bssid));
    ssidLen = strlen(pApMetaInfo->ssid);
    if (SIR_MAX_SSID_SIZE < ssidLen)
    {
       /*invalid scan result*/
       vos_mem_free(pNode);
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
          "%s: Invalid AP meta info ssidlen %d", __func__, ssidLen);
       return;
    }
    vos_mem_copy(pNode->ApInfo.ssid, pApMetaInfo->ssid, ssidLen);
    /*null terminate ssid*/
    pNode->ApInfo.ssid[ssidLen] = '\0';
    pNode->ApInfo.ch = pApMetaInfo->ch;
    pNode->ApInfo.rssi = pApMetaInfo->rssi;
    pNode->ApInfo.age = pApMetaInfo->timestamp;
    pNode->ApInfo.batchId = scanId;
    pNode->ApInfo.isLastAp = isLastAp;

    pNode->pNext = NULL;
    if (NULL == pHead)
    {
         pAdapter->pBatchScanRsp = pNode;
    }
    else
    {
        pTemp = pHead;
        while (NULL != pTemp)
        {
            pPrev = pTemp;
            pTemp = pTemp->pNext;
        }
        pPrev->pNext = pNode;
    }

    return;
}/*End of hdd_populate_batch_scan_rsp_queue*/

/**---------------------------------------------------------------------------

  \brief hdd_batch_scan_result_ind_callback () - This function is called after
     receiving batch scan response indication from FW. It saves get batch scan
     response data in HDD batch scan response queue. This callback sets the
     completion event on which hdd_ioctl is waiting only after getting complete
     batch scan response data from FW

  \param  - callbackContext Pointer to HDD adapter
  \param  - pRsp Pointer to get batch scan response data received from FW

  \return - nothing

  --------------------------------------------------------------------------*/
static void hdd_batch_scan_result_ind_callback
(
    void *callbackContext,
    void *pRsp
)
{
    v_BOOL_t                     isLastAp;
    tANI_U32                     numApMetaInfo;
    tANI_U32                     numNetworkInScanList;
    tANI_U32                     numberScanList;
    tANI_U32                     nextScanListOffset;
    tANI_U32                     nextApMetaInfoOffset;
    hdd_adapter_t*               pAdapter;
    tpSirBatchScanList           pScanList;
    tpSirBatchScanNetworkInfo    pApMetaInfo;
    tpSirBatchScanResultIndParam pBatchScanRsp;/*batch scan rsp data from FW*/
    tSirSetBatchScanReq          *pReq;

    pAdapter = (hdd_adapter_t *)callbackContext;
    /*sanity check*/
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid pAdapter magic", __func__);
        VOS_ASSERT(0);
        return;
    }

    /*initialize locals*/
    pReq = &pAdapter->hddSetBatchScanReq;
    pBatchScanRsp = (tpSirBatchScanResultIndParam)pRsp;
    isLastAp = FALSE;
    numApMetaInfo = 0;
    numNetworkInScanList = 0;
    numberScanList = 0;
    nextScanListOffset = 0;
    nextApMetaInfoOffset = 0;
    pScanList = NULL;
    pApMetaInfo = NULL;

    if ((NULL == pBatchScanRsp) || (NULL == pReq))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s: pBatchScanRsp is %p pReq %p", __func__, pBatchScanRsp, pReq);
            isLastAp = TRUE;
         goto done;
    }

    pAdapter->numScanList = numberScanList =  pBatchScanRsp->numScanLists;
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
        "Batch scan rsp: numberScalList %d", numberScanList);

    if ((!numberScanList) || (numberScanList > pReq->numberOfScansToBatch))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: numberScanList %d", __func__, numberScanList);
         isLastAp = TRUE;
         goto done;
    }

    while (numberScanList)
    {
        pScanList = (tpSirBatchScanList)((tANI_U8 *)pBatchScanRsp->scanResults +
                                          nextScanListOffset);
        if (NULL == pScanList)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: pScanList is %p", __func__, pScanList);
            isLastAp = TRUE;
           goto done;
        }
        numNetworkInScanList = numApMetaInfo = pScanList->numNetworksInScanList;
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "Batch scan rsp: numApMetaInfo %d scanId %d",
            numApMetaInfo, pScanList->scanId);

        if ((!numApMetaInfo) || (numApMetaInfo > pReq->bestNetwork))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: numApMetaInfo %d", __func__, numApMetaInfo);
            isLastAp = TRUE;
           goto done;
        }

        /*Initialize next AP meta info offset for next scan list*/
        nextApMetaInfoOffset = 0;

        while (numApMetaInfo)
        {
            pApMetaInfo = (tpSirBatchScanNetworkInfo)(pScanList->scanList +
                                                nextApMetaInfoOffset);
            if (NULL == pApMetaInfo)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: pApMetaInfo is %p", __func__, pApMetaInfo);
                isLastAp = TRUE;
                goto done;
            }
            /*calculate AP age*/
            pApMetaInfo->timestamp =
                pBatchScanRsp->timestamp - pApMetaInfo->timestamp;

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                      "%s: bssId "MAC_ADDRESS_STR
                      " ch %d rssi %d timestamp %d", __func__,
                      MAC_ADDR_ARRAY(pApMetaInfo->bssid),
                      pApMetaInfo->ch, pApMetaInfo->rssi,
                      pApMetaInfo->timestamp);

            /*mark last AP in batch scan response*/
            if ((TRUE == pBatchScanRsp->isLastResult) &&
                (1 == numberScanList) && (1 == numApMetaInfo))
            {
               isLastAp = TRUE;
            }

            mutex_lock(&pAdapter->hdd_batch_scan_lock);
            /*store batch scan repsonse in hdd queue*/
            hdd_populate_batch_scan_rsp_queue(pAdapter, pApMetaInfo,
                pScanList->scanId, isLastAp);
            mutex_unlock(&pAdapter->hdd_batch_scan_lock);

            nextApMetaInfoOffset += sizeof(tSirBatchScanNetworkInfo);
            numApMetaInfo--;
        }

        nextScanListOffset +=  ((sizeof(tSirBatchScanList) - sizeof(tANI_U8))
                                + (sizeof(tSirBatchScanNetworkInfo)
                                * numNetworkInScanList));
        numberScanList--;
    }

done:

    /*notify hdd_ioctl only if complete batch scan rsp is received and it was
      requested from hdd_ioctl*/
    if ((TRUE == pAdapter->hdd_wait_for_get_batch_scan_rsp) &&
        (TRUE == isLastAp))
    {
        pAdapter->hdd_wait_for_get_batch_scan_rsp = FALSE;
        complete(&pAdapter->hdd_get_batch_scan_req_var);
    }

    return;
}/*End of hdd_batch_scan_result_ind_callback*/

/**---------------------------------------------------------------------------

  \brief hdd_format_batch_scan_rsp () - This function formats batch scan
     response as per batch scan FR request format by putting proper markers

  \param  - pDest pointer to destination buffer
  \param  - cur_len current length
  \param  - tot_len total remaining size which can be written to user space
  \param  - pApMetaInfo Pointer to get batch scan response AP meta info
  \param  - pAdapter Pointer to HDD adapter

  \return - ret no of characters written

  --------------------------------------------------------------------------*/
static tANI_U32
hdd_format_batch_scan_rsp
(
    tANI_U8 *pDest,
    tANI_U32 cur_len,
    tANI_U32 tot_len,
    tHddBatchScanRsp *pApMetaInfo,
    hdd_adapter_t* pAdapter
)
{
   tANI_U32 ret = 0;
   tANI_U32 rem_len = 0;
   tANI_U8  temp_len = 0;
   tANI_U8  temp_total_len = 0;
   tANI_U8  temp[HDD_BATCH_SCAN_AP_META_INFO_SIZE];
   tANI_U8  *pTemp = temp;

   /*Batch scan reponse needs to be returned to user space in
     following format:
     "scancount=X\n" where X is the number of scans in current batch
     batch
     "trunc\n" optional present if current scan truncated
     "bssid=XX:XX:XX:XX:XX:XX\n"
     "ssid=XXXX\n"
     "freq=X\n" frequency in Mhz
     "level=XX\n"
     "age=X\n" ms
     "dist=X\n" cm (-1 if not available)
     "errror=X\n" (-1if not available)
     "====\n" (end of ap marker)
     "####\n" (end of scan marker)
     "----\n" (end of results)*/
     /*send scan result in above format to user space based on
       available length*/
   /*The GET response may have more data than the driver can return in its
     buffer. In that case the buffer should be filled to the nearest complete
     scan, ending with "%%%%".Subsequent callsshould return the remaining data
     starting with the next scan (optional .trunc\n., .apcount=X\n., etc).
     The final buffer should end with "----\n"*/

   /*sanity*/
   if (cur_len > tot_len)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
          "%s: invaid cur_len %d tot_len %d", __func__, cur_len, tot_len);
       return 0;
   }
   else
   {
      rem_len = (tot_len - cur_len);
   }

   /*end scan marker*/
   if (pApMetaInfo->ApInfo.batchId != pAdapter->prev_batch_id)
   {
       temp_len = snprintf(pTemp, sizeof(temp), "####\n");
       pTemp += temp_len;
       temp_total_len += temp_len;
   }

   /*bssid*/
   temp_len = snprintf(pTemp, sizeof(temp),
                "bssid=0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
                pApMetaInfo->ApInfo.bssid[0], pApMetaInfo->ApInfo.bssid[1],
                pApMetaInfo->ApInfo.bssid[2], pApMetaInfo->ApInfo.bssid[3],
                pApMetaInfo->ApInfo.bssid[4], pApMetaInfo->ApInfo.bssid[5]);
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*ssid*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "ssid=%s\n",
                 pApMetaInfo->ApInfo.ssid);
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*freq*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "freq=%d\n",
                 sme_ChnToFreq(pApMetaInfo->ApInfo.ch));
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*level*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "level=%d\n",
                  pApMetaInfo->ApInfo.rssi);
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*age*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "age=%d\n",
                  pApMetaInfo->ApInfo.age);
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*dist*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "dist=-1\n");
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*error*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "error=-1\n");
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*end AP marker*/
   temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "====\n");
   pTemp += temp_len;
   temp_total_len += temp_len;

   /*last AP in batch scan response*/
   if(TRUE == pApMetaInfo->ApInfo.isLastAp)
   {
       /*end scan marker*/
       temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "####\n");
       pTemp += temp_len;
       temp_total_len += temp_len;

       /*end batch scan result marker*/
       temp_len = snprintf(pTemp, (sizeof(temp) - temp_total_len), "----\n");
       pTemp += temp_len;
       temp_total_len += temp_len;

   }

   if (temp_total_len < rem_len)
   {
       ret = temp_total_len + 1;
       strlcpy(pDest, temp, ret);
       pAdapter->isTruncated = FALSE;
   }
   else
   {
      pAdapter->isTruncated = TRUE;
      if (rem_len >= strlen("%%%%"))
      {
          ret = snprintf(pDest, sizeof(temp), "%%%%");
      }
      else
      {
          ret = 0;
      }
   }

   return ret;

}/*End of hdd_format_batch_scan_rsp*/

/**---------------------------------------------------------------------------

  \brief hdd_populate_user_batch_scan_rsp() - This function populates user data
     buffer starting with head of hdd batch scan response queue

  \param - pAdapter Pointer to HDD adapter
  \param - pDest Pointer to user data buffer
  \param - cur_len current offset in user buffer
  \param - rem_len remaining  no of bytes in user buffer

  \return - number of bytes written in user buffer

  --------------------------------------------------------------------------*/

tANI_U32 hdd_populate_user_batch_scan_rsp
(
    hdd_adapter_t* pAdapter,
    tANI_U8  *pDest,
    tANI_U32 cur_len,
    tANI_U32 rem_len
)
{
    tHddBatchScanRsp *pHead;
    tHddBatchScanRsp *pPrev;
    tANI_U32 len;

    pAdapter->isTruncated = FALSE;

    /*head of hdd batch scan response queue*/
    pHead = pAdapter->pBatchScanRsp;
    while (pHead)
    {
         len = hdd_format_batch_scan_rsp(pDest, cur_len, rem_len, pHead,
                   pAdapter);
         pDest += len;
         pDest--;
         cur_len += len;
         if(TRUE == pAdapter->isTruncated)
         {
              /*result is truncated return rest of scan rsp in next req*/
              cur_len = rem_len;
              break;
         }
         pPrev = pHead;
         pHead = pHead->pNext;
         pAdapter->pBatchScanRsp  = pHead;
         if (TRUE == pPrev->ApInfo.isLastAp)
         {
             pAdapter->prev_batch_id = 0;
         }
         else
         {
             pAdapter->prev_batch_id = pPrev->ApInfo.batchId;
         }
         vos_mem_free(pPrev);
         pPrev = NULL;
   }

   return cur_len;
}/*End of hdd_populate_user_batch_scan_rsp*/

/**---------------------------------------------------------------------------

  \brief hdd_return_batch_scan_rsp_to_user () - This function returns batch
     scan response data from HDD queue to user space
     It does following in detail:
     a) if HDD has enough data in its queue then it 1st copies data to user
        space and then send get batch scan indication message to FW. In this
        case it does not wait on any event and batch scan response data will
        be populated in HDD response queue in MC thread context after receiving
        indication from FW
     b) else send get batch scan indication message to FW and wait on an event
        which will be set once HDD receives complete batch scan response from
        FW and then this function returns batch scan response to user space

  \param  - pAdapter Pointer to HDD adapter
  \param  - pPrivData Pointer to priv_data

  \return - 0 for success -EFAULT for failure

  --------------------------------------------------------------------------*/

int hdd_return_batch_scan_rsp_to_user
(
    hdd_adapter_t* pAdapter,
    hdd_priv_data_t *pPrivData,
    tANI_U8 *command
)
{
    tANI_U8    *pDest;
    tANI_U32   count = 0;
    tANI_U32   len = 0;
    tANI_U32   cur_len = 0;
    tANI_U32   rem_len = 0;
    eHalStatus halStatus;
    unsigned long rc;
    tSirTriggerBatchScanResultInd *pReq;

    pReq = &pAdapter->hddTriggerBatchScanResultInd;
    pReq->param = 0;/*batch scan client*/
    pDest = (tANI_U8 *)(command + pPrivData->used_len);
    pAdapter->hdd_wait_for_get_batch_scan_rsp = FALSE;

    cur_len = pPrivData->used_len;
    if (pPrivData->total_len > pPrivData->used_len)
    {
        rem_len = pPrivData->total_len - pPrivData->used_len;
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s: Invalid user data buffer total_len %d used_len %d",
            __func__, pPrivData->total_len, pPrivData->used_len);
        return -EFAULT;
    }

    mutex_lock(&pAdapter->hdd_batch_scan_lock);
    len = hdd_populate_user_batch_scan_rsp(pAdapter, pDest,
                 cur_len, rem_len);
    mutex_unlock(&pAdapter->hdd_batch_scan_lock);

    /*enough scan result available in cache to return to user space or
      scan result needs to be fetched 1st from fw and then return*/
    if (len == cur_len)
    {
        pAdapter->hdd_wait_for_get_batch_scan_rsp = TRUE;
        halStatus = sme_TriggerBatchScanResultInd(
                        WLAN_HDD_GET_HAL_CTX(pAdapter), pReq,
                        pAdapter->sessionId, hdd_batch_scan_result_ind_callback,
                        pAdapter);
        if ( eHAL_STATUS_SUCCESS == halStatus )
        {
            if (TRUE == pAdapter->hdd_wait_for_get_batch_scan_rsp)
            {
                INIT_COMPLETION(pAdapter->hdd_get_batch_scan_req_var);
                rc = wait_for_completion_timeout(
                     &pAdapter->hdd_get_batch_scan_req_var,
                     msecs_to_jiffies(HDD_GET_BATCH_SCAN_RSP_TIME_OUT));
                if (0 == rc)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Timeout waiting to fetch batch scan rsp from fw",
                    __func__);
                    return -EFAULT;
                }
            }

            len = snprintf(pDest, HDD_BATCH_SCAN_AP_META_INFO_SIZE,
                      "scancount=%u\n", pAdapter->numScanList);
            pDest += len;
            cur_len += len;

            mutex_lock(&pAdapter->hdd_batch_scan_lock);
            len = hdd_populate_user_batch_scan_rsp(pAdapter, pDest,
                 cur_len, rem_len);
            mutex_unlock(&pAdapter->hdd_batch_scan_lock);

            count = 0;
            len = (len - pPrivData->used_len);
            pDest = (command + pPrivData->used_len);
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "NEW BATCH SCAN RESULT:");
            while(count < len)
            {
                printk("%c", *(pDest + count));
                count++;
            }
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: copy %d data to user buffer", __func__, len);
            if (copy_to_user(pPrivData->buf, pDest, len))
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: failed to copy data to user buffer", __func__);
                return -EFAULT;
            }
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "sme_GetBatchScanScan  returned failure halStatus %d",
                halStatus);
             return -EINVAL;
        }
    }
    else
    {
        count = 0;
        len = (len - pPrivData->used_len);
        pDest = (command + pPrivData->used_len);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "REMAINING TRUNCATED BATCH SCAN RESULT:");
        while(count < len)
        {
            printk("%c", *(pDest + count));
            count++;
        }
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
           "%s: copy %d data to user buffer", __func__, len);
        if (copy_to_user(pPrivData->buf, pDest, len))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: failed to copy data to user buffer", __func__);
            return -EFAULT;
        }
    }

   return 0;
} /*End of hdd_return_batch_scan_rsp_to_user*/


/**---------------------------------------------------------------------------

  \brief hdd_handle_batch_scan_ioctl () - This function handles WLS_BATCHING
     IOCTLs from user space. Following BATCH SCAN DEV IOCTs are handled:
     WLS_BATCHING VERSION
     WLS_BATCHING SET
     WLS_BATCHING GET
     WLS_BATCHING STOP

  \param  - pAdapter Pointer to HDD adapter
  \param  - pPrivdata Pointer to priv_data
  \param  - command Pointer to command

  \return - 0 for success -EFAULT for failure

  --------------------------------------------------------------------------*/

int hdd_handle_batch_scan_ioctl
(
    hdd_adapter_t *pAdapter,
    hdd_priv_data_t *pPrivdata,
    tANI_U8 *command
)
{
    int ret = 0;
    hdd_context_t *pHddCtx;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (ret)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid!", __func__);
        goto exit;
    }

    if (strncmp(command, "WLS_BATCHING VERSION", 20) == 0)
    {
         char    extra[32];
         tANI_U8 len = 0;
         tANI_U8 version = HDD_BATCH_SCAN_VERSION;

         if (FALSE == sme_IsFeatureSupportedByFW(BATCH_SCAN))
         {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: Batch scan feature is not supported by FW", __func__);
             ret = -EINVAL;
             goto exit;
         }

         len = scnprintf(extra, sizeof(extra), "WLS_BATCHING_VERSION %d",
                   version);
         if (copy_to_user(pPrivdata->buf, &extra, len + 1))
         {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: failed to copy data to user buffer", __func__);
              ret = -EFAULT;
              goto exit;
         }
         ret = HDD_BATCH_SCAN_VERSION;
    }
    else if (strncmp(command, "WLS_BATCHING SET", 16) == 0)
    {
         int                 status;
         tANI_U8             *value = (command + 16);
         eHalStatus          halStatus;
         unsigned long       rc;
         tSirSetBatchScanReq *pReq = &pAdapter->hddSetBatchScanReq;
         tSirSetBatchScanRsp *pRsp = &pAdapter->hddSetBatchScanRsp;

         if (FALSE == sme_IsFeatureSupportedByFW(BATCH_SCAN))
         {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Batch scan feature is not supported by FW", __func__);
              ret = -EINVAL;
              goto exit;
         }

         if ((WLAN_HDD_INFRA_STATION != pAdapter->device_mode) &&
             (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) &&
             (WLAN_HDD_P2P_GO != pAdapter->device_mode) &&
             (WLAN_HDD_P2P_DEVICE != pAdapter->device_mode))
         {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Received WLS_BATCHING SET command in invalid mode %s (%d) "
                "WLS_BATCHING_SET is only allowed in infra STA/P2P client mode",
                 hdd_device_modetoString(pAdapter->device_mode),
                 pAdapter->device_mode);
             ret = -EINVAL;
             goto exit;
         }

         status = hdd_parse_set_batchscan_command(value, pReq);
         if (status)
         {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid WLS_BATCHING SET command");
             ret = -EINVAL;
             goto exit;
         }


         pAdapter->hdd_wait_for_set_batch_scan_rsp = TRUE;
         halStatus = sme_SetBatchScanReq(WLAN_HDD_GET_HAL_CTX(pAdapter), pReq,
                          pAdapter->sessionId, hdd_set_batch_scan_req_callback,
                          pAdapter);

         if ( eHAL_STATUS_SUCCESS == halStatus )
         {
             char extra[32];
             tANI_U8 len = 0;
             tANI_U8 mScan = 0;

             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "sme_SetBatchScanReq  returned success halStatus %d",
                halStatus);
             if (TRUE == pAdapter->hdd_wait_for_set_batch_scan_rsp)
             {
                 INIT_COMPLETION(pAdapter->hdd_set_batch_scan_req_var);
                 rc = wait_for_completion_timeout(
                      &pAdapter->hdd_set_batch_scan_req_var,
                      msecs_to_jiffies(HDD_SET_BATCH_SCAN_REQ_TIME_OUT));
                 if (0 == rc)
                 {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Timeout waiting for set batch scan to complete",
                    __func__);
                    ret = -EINVAL;
                    goto exit;
                 }
             }
             if ( !pRsp->nScansToBatch )
             {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Received set batch scan failure response from FW",
                     __func__);
                ret = -EINVAL;
                goto exit;
             }
             /*As per the Batch Scan Framework API we should return the MIN of
               either MSCAN or the max # of scans firmware can cache*/
             mScan = MIN(pReq->numberOfScansToBatch , pRsp->nScansToBatch);

             pAdapter->batchScanState = eHDD_BATCH_SCAN_STATE_STARTED;

             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: request MSCAN %d response MSCAN %d ret %d",
                __func__, pReq->numberOfScansToBatch, pRsp->nScansToBatch, mScan);
             len = scnprintf(extra, sizeof(extra), "%d", mScan);
             if (copy_to_user(pPrivdata->buf, &extra, len + 1))
             {
                 VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to copy MSCAN value to user buffer", __func__);
                 ret = -EFAULT;
                 goto exit;
             }
         }
         else
         {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "sme_SetBatchScanReq  returned failure halStatus %d",
                halStatus);
             ret = -EINVAL;
             goto exit;
         }
    }
    else if (strncmp(command, "WLS_BATCHING STOP", 17) == 0)
    {
         eHalStatus halStatus;
         tSirStopBatchScanInd *pInd = &pAdapter->hddStopBatchScanInd;
         pInd->param = 0;

         if (FALSE == sme_IsFeatureSupportedByFW(BATCH_SCAN))
         {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Batch scan feature is not supported by FW", __func__);
              ret = -EINVAL;
              goto exit;
         }

         if (eHDD_BATCH_SCAN_STATE_STARTED !=  pAdapter->batchScanState)
         {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "Batch scan is not yet enabled batch scan state %d",
                pAdapter->batchScanState);
              ret = -EINVAL;
              goto exit;
         }

         mutex_lock(&pAdapter->hdd_batch_scan_lock);
         hdd_deinit_batch_scan(pAdapter);
         mutex_unlock(&pAdapter->hdd_batch_scan_lock);

         pAdapter->batchScanState = eHDD_BATCH_SCAN_STATE_STOPPED;

         halStatus = sme_StopBatchScanInd(WLAN_HDD_GET_HAL_CTX(pAdapter), pInd,
                          pAdapter->sessionId);
         if ( eHAL_STATUS_SUCCESS == halStatus )
         {
             ret = 0;
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "sme_StopBatchScanInd  returned success halStatus %d",
                halStatus);
         }
         else
         {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "sme_StopBatchScanInd  returned failure halStatus %d",
                halStatus);
             ret = -EINVAL;
             goto exit;
         }
    }
    else if (strncmp(command, "WLS_BATCHING GET", 16) == 0)
    {
          tANI_U32 remain_len;

          if (FALSE == sme_IsFeatureSupportedByFW(BATCH_SCAN))
          {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Batch scan feature is not supported by FW", __func__);
              ret = -EINVAL;
              goto exit;
          }

          if (eHDD_BATCH_SCAN_STATE_STARTED !=  pAdapter->batchScanState)
          {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "Batch scan is not yet enabled could not return results"
                "Batch Scan state %d",
                pAdapter->batchScanState);
              ret = -EINVAL;
              goto exit;
          }

          pPrivdata->used_len = 16;
          remain_len = pPrivdata->total_len - pPrivdata->used_len;
          if (remain_len < pPrivdata->total_len)
          {
              /*Clear previous batch scan response data if any*/
              vos_mem_zero((tANI_U8 *)(command + pPrivdata->used_len), remain_len);
          }
          else
          {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid total length from user space can't fetch batch"
                " scan response total_len %d used_len %d remain len %d",
                pPrivdata->total_len, pPrivdata->used_len, remain_len);
              ret = -EINVAL;
              goto exit;
          }
          ret = hdd_return_batch_scan_rsp_to_user(pAdapter, pPrivdata, command);
    }

exit:

    return ret;
}


#endif/*End of FEATURE_WLAN_BATCH_SCAN*/

static void getBcnMissRateCB(VOS_STATUS status, int bcnMissRate, void *data)
{
    bcnMissRateContext_t *pCBCtx;

    if (NULL == data)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("argument data is NULL"));
        return;
    }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
    spin_lock(&hdd_context_lock);

    pCBCtx = (bcnMissRateContext_t *)data;
    gbcnMissRate = -1;

    if (pCBCtx->magic != BCN_MISS_RATE_CONTEXT_MAGIC)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("invalid context magic: %08x"), pCBCtx->magic);
        spin_unlock(&hdd_context_lock);
        return ;
    }

    if (VOS_STATUS_SUCCESS == status)
    {
        gbcnMissRate = bcnMissRate;
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("failed to get bcnMissRate"));
    }

    complete(&(pCBCtx->completion));
    spin_unlock(&hdd_context_lock);

    return;
}

void hdd_FWStatisCB( VOS_STATUS status, void *fwStats, void *data )
{
    fwStatsContext_t *fwStatsCtx;
    fwStatsResult_t  *fwStatsResult;
    hdd_adapter_t *pAdapter;

    hddLog(VOS_TRACE_LEVEL_INFO, FL(" with status = %d"),status);

    if (NULL == data)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("argument data is NULL"));
        return;
    }
    /* there is a race condition that exists between this callback
       function and the caller since the caller could time out either
       before or while this code is executing.  we use a spinlock to
       serialize these actions */
    spin_lock(&hdd_context_lock);
    fwStatsCtx = (fwStatsContext_t *) data;
    if (fwStatsCtx->magic != FW_STATS_CONTEXT_MAGIC)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("invalid context magic: %08x"), fwStatsCtx->magic);
        spin_unlock(&hdd_context_lock);
        return;
    }
    pAdapter = fwStatsCtx->pAdapter;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter returned is NULL or invalid"));
        spin_unlock(&hdd_context_lock);
        return;
    }
    pAdapter->fwStatsRsp.type = 0;
    if ((VOS_STATUS_SUCCESS == status) && (NULL != fwStats))
    {
        fwStatsResult = (fwStatsResult_t *)fwStats;
        switch( fwStatsResult->type )
        {
            case FW_UBSP_STATS:
            {
                 memcpy(&pAdapter->fwStatsRsp,fwStatsResult,sizeof(fwStatsResult_t));

                 hddLog(VOS_TRACE_LEVEL_INFO,
                  FL("ubsp_enter_cnt = %d ubsp_jump_ddr_cnt = %d"),
                  pAdapter->fwStatsRsp.hddFwStatsData.ubspStats.ubsp_enter_cnt,
                  pAdapter->fwStatsRsp.hddFwStatsData.ubspStats.ubsp_jump_ddr_cnt);
            }
            break;
            default:
            {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL(" No handling for stats type %d"),fwStatsResult->type);
            }
         }
    }
    complete(&(fwStatsCtx->completion));
    spin_unlock(&hdd_context_lock);
    return;
}

static int hdd_get_dwell_time(hdd_config_t *pCfg, tANI_U8 *command, char *extra, tANI_U8 n, tANI_U8 *len)
{
    int ret = 0;

    if (!pCfg || !command || !extra || !len)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passsed for GETDWELLTIME is incorrect", __func__);
        ret = -EINVAL;
        return ret;
    }

    if (strncmp(command, "GETDWELLTIME ACTIVE MAX", 23) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MAX %u\n",
                (int)pCfg->nActiveMaxChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME ACTIVE MIN", 23) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MIN %u\n",
                (int)pCfg->nActiveMinChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME PASSIVE MAX", 24) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MAX %u\n",
                (int)pCfg->nPassiveMaxChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME PASSIVE MIN", 24) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MIN %u\n",
                (int)pCfg->nPassiveMinChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME", 12) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME %u \n",
                (int)pCfg->nActiveMaxChnTime);
        return ret;
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

static int hdd_set_dwell_time(hdd_adapter_t *pAdapter, tANI_U8 *command)
{
    tHalHandle hHal;
    hdd_config_t *pCfg;
    tANI_U8 *value = command;
    int val = 0, ret = 0, temp = 0;
    tSmeConfigParams smeConfig;

    if (!pAdapter || !command || !(pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini)
        || !(hHal = (WLAN_HDD_GET_HAL_CTX(pAdapter))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: argument passed for SETDWELLTIME is incorrect", __func__);
        ret = -EINVAL;
        return ret;
    }

    vos_mem_zero(&smeConfig, sizeof(smeConfig));
    sme_GetConfigParam(hHal, &smeConfig);

    if (strncmp(command, "SETDWELLTIME ACTIVE MAX", 23) == 0 )
    {
        value = value + 24;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME ACTIVE MAX is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMaxChnTime = val;
        smeConfig.csrConfig.nActiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME ACTIVE MIN", 23) == 0)
    {
        value = value + 24;
        temp = kstrtou32(value, 10, &val);
        if (temp !=0 || val < CFG_ACTIVE_MIN_CHANNEL_TIME_MIN  ||
                        val > CFG_ACTIVE_MIN_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passsed for SETDWELLTIME ACTIVE MIN is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMinChnTime = val;
        smeConfig.csrConfig.nActiveMinChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME PASSIVE MAX", 24) == 0)
    {
        value = value + 25;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_PASSIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_PASSIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME PASSIVE MAX is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nPassiveMaxChnTime = val;
        smeConfig.csrConfig.nPassiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME PASSIVE MIN", 24) == 0)
    {
        value = value + 25;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_PASSIVE_MIN_CHANNEL_TIME_MIN ||
                         val > CFG_PASSIVE_MIN_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME PASSIVE MIN is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nPassiveMinChnTime = val;
        smeConfig.csrConfig.nPassiveMinChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME", 12) == 0)
    {
        value = value + 13;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMaxChnTime = val;
        smeConfig.csrConfig.nActiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

static int hdd_driver_command(hdd_adapter_t *pAdapter,
                              hdd_priv_data_t *ppriv_data)
{
   hdd_priv_data_t priv_data;
   tANI_U8 *command = NULL;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   hdd_scaninfo_t *pScanInfo = NULL;
   int ret = 0;
   int status;
   /*
    * Note that valid pointers are provided by caller
    */

   /* copy to local struct to avoid numerous changes to legacy code */
   priv_data = *ppriv_data;

   if (priv_data.total_len <= 0  ||
       priv_data.total_len > WLAN_PRIV_DATA_MAX_LEN)
   {
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s:invalid priv_data.total_len(%d)!!!", __func__,
              priv_data.total_len);
       ret = -EINVAL;
       goto exit;
   }
   status = wlan_hdd_validate_context(pHddCtx);
   if (0 != status)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid", __func__);
       return status;
   }
   /* Allocate +1 for '\0' */
   command = kmalloc(priv_data.total_len + 1, GFP_KERNEL);
   if (!command)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: failed to allocate memory", __func__);
       ret = -ENOMEM;
       goto exit;
   }

   if (copy_from_user(command, priv_data.buf, priv_data.total_len))
   {
       ret = -EFAULT;
       goto exit;
   }

   /* Make sure the command is NUL-terminated */
   command[priv_data.total_len] = '\0';

   /* at one time the following block of code was conditional. braces
    * have been retained to avoid re-indenting the legacy code
    */
   {
       hdd_context_t *pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "%s: Received %s cmd from Wi-Fi GUI***", __func__, command);

       if (strncmp(command, "P2P_DEV_ADDR", 12) == 0 )
       {
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_P2P_DEV_ADDR_IOCTL,
                            pAdapter->sessionId, (unsigned)
                            (*(pHddCtx->p2pDeviceAddress.bytes+2)<<24 |
                             *(pHddCtx->p2pDeviceAddress.bytes+3)<<16 |
                             *(pHddCtx->p2pDeviceAddress.bytes+4)<<8  |
                             *(pHddCtx->p2pDeviceAddress.bytes+5))));
           if (copy_to_user(priv_data.buf, pHddCtx->p2pDeviceAddress.bytes,
                                                           sizeof(tSirMacAddr)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
           }
       }
       else if(strncmp(command, "SETBAND", 7) == 0)
       {
           tANI_U8 *ptr = command ;

           /* Change band request received */

           /* First 8 bytes will have "SETBAND " and
            * 9 byte will have band setting value */
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "%s: SetBandCommand Info  comm %s UL %d, TL %d", __func__, command, priv_data.used_len, priv_data.total_len);
           /* Change band request received */
           ret = hdd_setBand_helper(pAdapter->dev, ptr);
           if(ret < 0)
               VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%s: failed to set band ret=%d", __func__, ret);
       }
       else if(strncmp(command, "SETWMMPS", 8) == 0)
       {
           tANI_U8 *ptr = command;
           ret = hdd_wmmps_helper(pAdapter, ptr);
       }
       else if ( strncasecmp(command, "COUNTRY", 7) == 0 )
       {
           char *country_code;

           country_code = command + 8;

           INIT_COMPLETION(pAdapter->change_country_code);
           hdd_checkandupdate_dfssetting(pAdapter, country_code);
#ifndef CONFIG_ENABLE_LINUX_REG
           hdd_checkandupdate_phymode(pAdapter, country_code);
#endif
           ret = (int)sme_ChangeCountryCode(pHddCtx->hHal,
                  (void *)(tSmeChangeCountryCallback)
                    wlan_hdd_change_country_code_callback,
                     country_code, pAdapter, pHddCtx->pvosContext, eSIR_TRUE, eSIR_TRUE);
           if (eHAL_STATUS_SUCCESS == ret)
           {
               ret = wait_for_completion_interruptible_timeout(
                       &pAdapter->change_country_code,
                            msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));
               if (0 >= ret)
               {
                   hddLog(VOS_TRACE_LEVEL_ERROR, "%s: SME while setting country code timed out %d",
                   __func__, ret);
               }
           }
           else
           {
               VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: SME Change Country code fail ret=%d", __func__, ret);
               ret = -EINVAL;
           }

       }
       /*
          command should be a string having format
          SET_SAP_CHANNEL_LIST <num of channels> <the channels seperated by spaces>
       */
       else if(strncmp(command, "SET_SAP_CHANNEL_LIST", 20) == 0)
       {
           tANI_U8 *ptr = command;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      " Received Command to Set Preferred Channels for SAP in %s", __func__);

           ret = sapSetPreferredChannel(ptr);
       }
       else if(strncmp(command, "SETSUSPENDMODE", 14) == 0)
       {
           int suspend = 0;
           tANI_U8 *ptr = (tANI_U8*)command + 15;

           suspend = *ptr - '0';
            MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                             TRACE_CODE_HDD_SETSUSPENDMODE_IOCTL,
                             pAdapter->sessionId, suspend));
           hdd_set_wlan_suspend_mode(suspend);
       }
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
       else if (strncmp(command, "SETROAMTRIGGER", 14) == 0)
       {
           tANI_U8 *value = command;
           tANI_S8 rssi = 0;
           tANI_U8 lookUpThreshold = CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           /* Move pointer to ahead of SETROAMTRIGGER<delimiter> */
           value = value + 15;

           /* Convert the value from ascii to integer */
           ret = kstrtos8(value, 10, &rssi);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           lookUpThreshold = abs(rssi);

           if ((lookUpThreshold < CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN) ||
               (lookUpThreshold > CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Neighbor lookup threshold value %d is out of range"
                      " (Min: %d Max: %d)", lookUpThreshold,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMTRIGGER_IOCTL,
                            pAdapter->sessionId, lookUpThreshold));
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set Roam trigger"
                      " (Neighbor lookup threshold) = %d", __func__, lookUpThreshold);

           pHddCtx->cfg_ini->nNeighborLookupRssiThreshold = lookUpThreshold;
           status = sme_setNeighborLookupRssiThreshold((tHalHandle)(pHddCtx->hHal), lookUpThreshold);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to set roam trigger, try again", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Set Reassoc threshold to (lookup rssi threshold + 5 dBm) */
           pHddCtx->cfg_ini->nNeighborReassocRssiThreshold = lookUpThreshold + 5;
           sme_setNeighborReassocRssiThreshold((tHalHandle)(pHddCtx->hHal), lookUpThreshold + 5);
       }
       else if (strncmp(command, "GETROAMTRIGGER", 14) == 0)
       {
           tANI_U8 lookUpThreshold = sme_getNeighborLookupRssiThreshold((tHalHandle)(pHddCtx->hHal));
           int rssi = (-1) * lookUpThreshold;
           char extra[32];
           tANI_U8 len = 0;
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMTRIGGER_IOCTL,
                            pAdapter->sessionId, lookUpThreshold));
           len = scnprintf(extra, sizeof(extra), "%s %d", command, rssi);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANPERIOD", 17) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamScanPeriod = 0;
           tANI_U16 neighborEmptyScanRefreshPeriod = CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT;

           /* input refresh period is in terms of seconds */
           /* Move pointer to ahead of SETROAMSCANPERIOD<delimiter> */
           value = value + 18;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanPeriod);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000),
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }

           if ((roamScanPeriod < (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000)) ||
               (roamScanPeriod > (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Roam scan period value %d is out of range"
                      " (Min: %d Max: %d)", roamScanPeriod,
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000),
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
              }
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMSCANPERIOD_IOCTL,
                            pAdapter->sessionId, roamScanPeriod));
           neighborEmptyScanRefreshPeriod = roamScanPeriod * 1000;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan period"
                      " (Empty Scan refresh period) = %d", __func__, roamScanPeriod);

           pHddCtx->cfg_ini->nEmptyScanRefreshPeriod = neighborEmptyScanRefreshPeriod;
           sme_UpdateEmptyScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), neighborEmptyScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANPERIOD", 17) == 0)
       {
           tANI_U16 nEmptyScanRefreshPeriod = sme_getEmptyScanRefreshPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANPERIOD_IOCTL,
                            pAdapter->sessionId, nEmptyScanRefreshPeriod));
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANPERIOD", (nEmptyScanRefreshPeriod/1000));
           /* Returned value is in units of seconds */
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANREFRESHPERIOD", 24) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamScanRefreshPeriod = 0;
           tANI_U16 neighborScanRefreshPeriod = CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT;

           /* input refresh period is in terms of seconds */
           /* Move pointer to ahead of SETROAMSCANREFRESHPERIOD<delimiter> */
           value = value + 25;

           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanRefreshPeriod);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000),
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }

           if ((roamScanRefreshPeriod < (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000)) ||
               (roamScanRefreshPeriod > (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Neighbor scan results refresh period value %d is out of range"
                      " (Min: %d Max: %d)", roamScanRefreshPeriod,
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000),
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }
           neighborScanRefreshPeriod = roamScanRefreshPeriod * 1000;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan refresh period"
                      " (Scan refresh period) = %d", __func__, roamScanRefreshPeriod);

           pHddCtx->cfg_ini->nNeighborResultsRefreshPeriod = neighborScanRefreshPeriod;
           sme_setNeighborScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), neighborScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANREFRESHPERIOD", 24) == 0)
       {
           tANI_U16 value = sme_getNeighborScanRefreshPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANREFRESHPERIOD", (value/1000));
           /* Returned value is in units of seconds */
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#ifdef FEATURE_WLAN_LFR
       /* SETROAMMODE */
       else if (strncmp(command, "SETROAMMODE", SIZE_OF_SETROAMMODE) == 0)
       {
           tANI_U8 *value = command;
	   tANI_BOOLEAN roamMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

	   /* Move pointer to ahead of SETROAMMODE<delimiter> */
	   value = value + SIZE_OF_SETROAMMODE + 1;

	   /* Convert the value from ascii to integer */
	   ret = kstrtou8(value, SIZE_OF_SETROAMMODE, &roamMode);
	   if (ret < 0)
	   {
	      /* If the input value is greater than max value of datatype, then also
		  kstrtou8 fails */
	      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
		   "%s: kstrtou8 failed range [%d - %d]", __func__,
		   CFG_LFR_FEATURE_ENABLED_MIN,
		   CFG_LFR_FEATURE_ENABLED_MAX);
              ret = -EINVAL;
	      goto exit;
	   }
           if ((roamMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
	       (roamMode > CFG_LFR_FEATURE_ENABLED_MAX))
           {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
			"Roam Mode value %d is out of range"
			" (Min: %d Max: %d)", roamMode,
			CFG_LFR_FEATURE_ENABLED_MIN,
			CFG_LFR_FEATURE_ENABLED_MAX);
	      ret = -EINVAL;
	      goto exit;
	   }

	   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
		   "%s: Received Command to Set Roam Mode = %d", __func__, roamMode);
           /*
	    * Note that
	    *     SETROAMMODE 0 is to enable LFR while
	    *     SETROAMMODE 1 is to disable LFR, but
	    *     NotifyIsFastRoamIniFeatureEnabled 0/1 is to enable/disable.
	    *     So, we have to invert the value to call sme_UpdateIsFastRoamIniFeatureEnabled.
	    */
	   if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
	       roamMode = CFG_LFR_FEATURE_ENABLED_MAX;    /* Roam enable */
	   else
	       roamMode = CFG_LFR_FEATURE_ENABLED_MIN;    /* Roam disable */

	   pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled = roamMode;
           sme_UpdateIsFastRoamIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), roamMode);
       }
       /* GETROAMMODE */
       else if (strncmp(priv_data.buf, "GETROAMMODE", SIZE_OF_GETROAMMODE) == 0)
       {
	   tANI_BOOLEAN roamMode = sme_getIsLfrFeatureEnabled((tHalHandle)(pHddCtx->hHal));
	   char extra[32];
	   tANI_U8 len = 0;

           /*
            * roamMode value shall be inverted because the sementics is different.
            */
           if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
	       roamMode = CFG_LFR_FEATURE_ENABLED_MAX;
           else
	       roamMode = CFG_LFR_FEATURE_ENABLED_MIN;

	   len = scnprintf(extra, sizeof(extra), "%s %d", command, roamMode);
	   if (copy_to_user(priv_data.buf, &extra, len + 1))
	   {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
	   }
       }
#endif
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
       else if (strncmp(command, "SETROAMDELTA", 12) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamRssiDiff = CFG_ROAM_RSSI_DIFF_DEFAULT;

           /* Move pointer to ahead of SETROAMDELTA<delimiter> */
           value = value + 13;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamRssiDiff);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_RSSI_DIFF_MIN,
                      CFG_ROAM_RSSI_DIFF_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((roamRssiDiff < CFG_ROAM_RSSI_DIFF_MIN) ||
               (roamRssiDiff > CFG_ROAM_RSSI_DIFF_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Roam rssi diff value %d is out of range"
                      " (Min: %d Max: %d)", roamRssiDiff,
                      CFG_ROAM_RSSI_DIFF_MIN,
                      CFG_ROAM_RSSI_DIFF_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam rssi diff = %d", __func__, roamRssiDiff);

           pHddCtx->cfg_ini->RoamRssiDiff = roamRssiDiff;
           sme_UpdateRoamRssiDiff((tHalHandle)(pHddCtx->hHal), roamRssiDiff);
       }
       else if (strncmp(priv_data.buf, "GETROAMDELTA", 12) == 0)
       {
           tANI_U8 roamRssiDiff = sme_getRoamRssiDiff((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMDELTA_IOCTL,
                            pAdapter->sessionId, roamRssiDiff));
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   command, roamRssiDiff);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
       else if (strncmp(command, "GETBAND", 7) == 0)
       {
           int band = -1;
           char extra[32];
           tANI_U8 len = 0;
           hdd_getBand_helper(pHddCtx, &band);

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETBAND_IOCTL,
                            pAdapter->sessionId, band));
           len = scnprintf(extra, sizeof(extra), "%s %d", command, band);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANCHANNELS", 19) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_channellist(value, ChannelList, &numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMSCANCHANNELS_IOCTL,
                            pAdapter->sessionId, numChannels));
           if (numChannels > WNI_CFG_VALID_CHANNEL_LIST_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: number of channels (%d) supported exceeded max (%d)", __func__,
                   numChannels, WNI_CFG_VALID_CHANNEL_LIST_LEN);
               ret = -EINVAL;
               goto exit;
           }
           status = sme_ChangeRoamScanChannelList((tHalHandle)(pHddCtx->hHal), ChannelList,
                                                  numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to update channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }
       }
       else if (strncmp(command, "GETROAMSCANCHANNELS", 19) == 0)
       {
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           tANI_U8 j = 0;
           char extra[128] = {0};
           int len;

           if (eHAL_STATUS_SUCCESS != sme_getRoamScanChannelList( (tHalHandle)(pHddCtx->hHal),
                                              ChannelList, &numChannels ))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: failed to get roam scan channel list", __func__);
               ret = -EFAULT;
               goto exit;
           }
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANCHANNELS_IOCTL,
                            pAdapter->sessionId, numChannels));
           /* output channel list is of the format
           [Number of roam scan channels][Channel1][Channel2]... */
           /* copy the number of channels in the 0th index */
           len = scnprintf(extra, sizeof(extra), "%s %d", command, numChannels);
           for (j = 0; (j < numChannels); j++)
           {
               len += scnprintf(extra + len, sizeof(extra) - len, " %d",
                       ChannelList[j]);
           }

           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETCCXMODE", 10) == 0)
       {
           tANI_BOOLEAN eseMode = sme_getIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (eseMode &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETCCXMODE", eseMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETOKCMODE", 10) == 0)
       {
           tANI_BOOLEAN okcMode = hdd_is_okc_mode_enabled(pHddCtx);
           char extra[32];
           tANI_U8 len = 0;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (okcMode &&
               sme_getIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETOKCMODE", okcMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTROAM", 11) == 0)
       {
           tANI_BOOLEAN lfrMode = sme_getIsLfrFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETFASTROAM", lfrMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTTRANSITION", 17) == 0)
       {
           tANI_BOOLEAN ft = sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETFASTTRANSITION", ft);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANCHANNELMINTIME", 25) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 minTime = CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT;

           /* Move pointer to ahead of SETROAMSCANCHANNELMINTIME<delimiter> */
           value = value + 26;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &minTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }
           if ((minTime < CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN) ||
               (minTime > CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "scan min channel time value %d is out of range"
                      " (Min: %d Max: %d)", minTime,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMSCANCHANNELMINTIME_IOCTL,
                            pAdapter->sessionId, minTime));
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change channel min time = %d", __func__, minTime);

           pHddCtx->cfg_ini->nNeighborScanMinChanTime = minTime;
           sme_setNeighborScanMinChanTime((tHalHandle)(pHddCtx->hHal), minTime);
       }
       else if (strncmp(command, "SENDACTIONFRAME", 15) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tANI_U8 dwellTime = 0;
           tANI_U8 bufLen = 0;
           tANI_U8 *buf = NULL;
           tSirMacAddr targetApBssid;
           eHalStatus status = eHAL_STATUS_SUCCESS;
           struct ieee80211_channel chan;
           tANI_U8 finalLen = 0;
           tANI_U8 *finalBuf = NULL;
           tANI_U8 temp = 0;
           u64 cookie;
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           /* if not associated, no need to send action frame */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_send_action_frame_data(value, targetApBssid, &channel,
                                                     &dwellTime, &buf, &bufLen);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse send action frame data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           /* if the target bssid is different from currently associated AP,
              then no need to send action frame */
           if (VOS_TRUE != vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:STA is not associated to this AP!",__func__);
               ret = -EINVAL;
               vos_mem_free(buf);
               buf = NULL;
               goto exit;
           }

           /* if the channel number is different from operating channel then
              no need to send action frame */
           if (channel != pHddStaCtx->conn_info.operationChannel)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "%s: channel(%d) is different from operating channel(%d)",
                         __func__, channel, pHddStaCtx->conn_info.operationChannel);
               ret = -EINVAL;
               vos_mem_free(buf);
               buf = NULL;
               goto exit;
           }
           chan.center_freq = sme_ChnToFreq(channel);

           finalLen = bufLen + 24;
           finalBuf = vos_mem_malloc(finalLen);
           if (NULL == finalBuf)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:memory allocation failed",__func__);
               ret = -ENOMEM;
               vos_mem_free(buf);
               buf = NULL;
               goto exit;
           }
           vos_mem_zero(finalBuf, finalLen);

           /* Fill subtype */
           temp = SIR_MAC_MGMT_ACTION << 4;
           vos_mem_copy(finalBuf + 0, &temp, sizeof(temp));

           /* Fill type */
           temp = SIR_MAC_MGMT_FRAME;
           vos_mem_copy(finalBuf + 2, &temp, sizeof(temp));

           /* Fill destination address (bssid of the AP) */
           vos_mem_copy(finalBuf + 4, targetApBssid, sizeof(targetApBssid));

           /* Fill source address (STA mac address) */
           vos_mem_copy(finalBuf + 10, pAdapter->macAddressCurrent.bytes, sizeof(pAdapter->macAddressCurrent.bytes));

           /* Fill BSSID (AP mac address) */
           vos_mem_copy(finalBuf + 16, targetApBssid, sizeof(targetApBssid));

           /* Fill received buffer from 24th address */
           vos_mem_copy(finalBuf + 24, buf, bufLen);

           /* done with the parsed buffer */
           vos_mem_free(buf);
           buf = NULL;

           wlan_hdd_mgmt_tx( NULL,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
                       &(pAdapter->wdev),
#else
                       pAdapter->dev,
#endif
                       &chan, 0,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
                       NL80211_CHAN_HT20, 1,
#endif
                       dwellTime, finalBuf, finalLen,  1,
                       1, &cookie );
           vos_mem_free(finalBuf);
       }
       else if (strncmp(command, "GETROAMSCANCHANNELMINTIME", 25) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMinChanTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANCHANNELMINTIME", val);
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANCHANNELMINTIME_IOCTL,
                            pAdapter->sessionId, val));
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANCHANNELTIME", 18) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 maxTime = CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT;

           /* Move pointer to ahead of SETSCANCHANNELTIME<delimiter> */
           value = value + 19;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &maxTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou16 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou16 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((maxTime < CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN) ||
               (maxTime > CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "lfr mode value %d is out of range"
                      " (Min: %d Max: %d)", maxTime,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change channel max time = %d", __func__, maxTime);

           pHddCtx->cfg_ini->nNeighborScanMaxChanTime = maxTime;
           sme_setNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal), maxTime);
       }
       else if (strncmp(command, "GETSCANCHANNELTIME", 18) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETSCANCHANNELTIME", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANHOMETIME", 15) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 val = CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT;

           /* Move pointer to ahead of SETSCANHOMETIME<delimiter> */
           value = value + 16;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &val);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou16 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou16 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((val < CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN) ||
               (val > CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "scan home time value %d is out of range"
                      " (Min: %d Max: %d)", val,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change scan home time = %d", __func__, val);

           pHddCtx->cfg_ini->nNeighborScanPeriod = val;
           sme_setNeighborScanPeriod((tHalHandle)(pHddCtx->hHal), val);
       }
       else if (strncmp(command, "GETSCANHOMETIME", 15) == 0)
       {
           tANI_U16 val = sme_getNeighborScanPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETSCANHOMETIME", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMINTRABAND", 16) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 val = CFG_ROAM_INTRA_BAND_DEFAULT;

           /* Move pointer to ahead of SETROAMINTRABAND<delimiter> */
           value = value + 17;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &val);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_INTRA_BAND_MIN,
                      CFG_ROAM_INTRA_BAND_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((val < CFG_ROAM_INTRA_BAND_MIN) ||
               (val > CFG_ROAM_INTRA_BAND_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "intra band mode value %d is out of range"
                      " (Min: %d Max: %d)", val,
                      CFG_ROAM_INTRA_BAND_MIN,
                      CFG_ROAM_INTRA_BAND_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change intra band = %d", __func__, val);

           pHddCtx->cfg_ini->nRoamIntraBand = val;
           sme_setRoamIntraBand((tHalHandle)(pHddCtx->hHal), val);
       }
       else if (strncmp(command, "GETROAMINTRABAND", 16) == 0)
       {
           tANI_U16 val = sme_getRoamIntraBand((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMINTRABAND", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANNPROBES", 14) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 nProbes = CFG_ROAM_SCAN_N_PROBES_DEFAULT;

           /* Move pointer to ahead of SETSCANNPROBES<delimiter> */
           value = value + 15;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &nProbes);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_SCAN_N_PROBES_MIN,
                      CFG_ROAM_SCAN_N_PROBES_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((nProbes < CFG_ROAM_SCAN_N_PROBES_MIN) ||
               (nProbes > CFG_ROAM_SCAN_N_PROBES_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "NProbes value %d is out of range"
                      " (Min: %d Max: %d)", nProbes,
                      CFG_ROAM_SCAN_N_PROBES_MIN,
                      CFG_ROAM_SCAN_N_PROBES_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set nProbes = %d", __func__, nProbes);

           pHddCtx->cfg_ini->nProbes = nProbes;
           sme_UpdateRoamScanNProbes((tHalHandle)(pHddCtx->hHal), nProbes);
       }
       else if (strncmp(priv_data.buf, "GETSCANNPROBES", 14) == 0)
       {
           tANI_U8 val = sme_getRoamScanNProbes((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANHOMEAWAYTIME", 19) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 homeAwayTime = CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT;

           /* Move pointer to ahead of SETSCANHOMEAWAYTIME<delimiter> */
           /* input value is in units of msec */
           value = value + 20;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &homeAwayTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((homeAwayTime < CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN) ||
               (homeAwayTime > CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "homeAwayTime value %d is out of range"
                      " (Min: %d Max: %d)", homeAwayTime,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set scan away time = %d", __func__, homeAwayTime);
           if (pHddCtx->cfg_ini->nRoamScanHomeAwayTime != homeAwayTime)
           {
               pHddCtx->cfg_ini->nRoamScanHomeAwayTime = homeAwayTime;
               sme_UpdateRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal), homeAwayTime, eANI_BOOLEAN_TRUE);
           }
       }
       else if (strncmp(priv_data.buf, "GETSCANHOMEAWAYTIME", 19) == 0)
       {
           tANI_U16 val = sme_getRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "REASSOC", 7) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tSirMacAddr targetApBssid;
           eHalStatus status = eHAL_STATUS_SUCCESS;
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           tCsrHandoffRequest handoffInfo;
#endif
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           /* if not associated, no need to proceed with reassoc */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_reassoc_command_data(value, targetApBssid, &channel);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse reassoc command data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           /* if the target bssid is same as currently associated AP,
              then no need to proceed with reassoc */
           if (VOS_TRUE == vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Reassoc BSSID is same as currently associated AP bssid",__func__);
               ret = -EINVAL;
               goto exit;
           }

           /* Check channel number is a valid channel number */
           if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter, channel))
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid Channel [%d]", __func__, channel);
               return -EINVAL;
           }

           /* Proceed with reassoc */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           handoffInfo.channel = channel;
           vos_mem_copy(handoffInfo.bssid, targetApBssid, sizeof(tSirMacAddr));
           sme_HandoffRequest(pHddCtx->hHal, &handoffInfo);
#endif
       }
       else if (strncmp(command, "SETWESMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_BOOLEAN wesMode = CFG_ENABLE_WES_MODE_NAME_DEFAULT;

           /* Move pointer to ahead of SETWESMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &wesMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ENABLE_WES_MODE_NAME_MIN,
                      CFG_ENABLE_WES_MODE_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((wesMode < CFG_ENABLE_WES_MODE_NAME_MIN) ||
               (wesMode > CFG_ENABLE_WES_MODE_NAME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "WES Mode value %d is out of range"
                      " (Min: %d Max: %d)", wesMode,
                      CFG_ENABLE_WES_MODE_NAME_MIN,
                      CFG_ENABLE_WES_MODE_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set WES Mode rssi diff = %d", __func__, wesMode);

           pHddCtx->cfg_ini->isWESModeEnabled = wesMode;
           sme_UpdateWESMode((tHalHandle)(pHddCtx->hHal), wesMode);
       }
       else if (strncmp(priv_data.buf, "GETWESMODE", 10) == 0)
       {
           tANI_BOOLEAN wesMode = sme_GetWESMode((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, wesMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif /* WLAN_FEATURE_VOWIFI_11R || FEATURE_WLAN_ESE || FEATURE_WLAN_LFR */
#ifdef FEATURE_WLAN_LFR
       else if (strncmp(command, "SETFASTROAM", 11) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 lfrMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

           /* Move pointer to ahead of SETFASTROAM<delimiter> */
           value = value + 12;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &lfrMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_LFR_FEATURE_ENABLED_MIN,
                      CFG_LFR_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((lfrMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
               (lfrMode > CFG_LFR_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "lfr mode value %d is out of range"
                      " (Min: %d Max: %d)", lfrMode,
                      CFG_LFR_FEATURE_ENABLED_MIN,
                      CFG_LFR_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change lfr mode = %d", __func__, lfrMode);

           pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled = lfrMode;
           sme_UpdateIsFastRoamIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), lfrMode);
       }
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
       else if (strncmp(command, "SETFASTTRANSITION", 17) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ft = CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT;

           /* Move pointer to ahead of SETFASTROAM<delimiter> */
           value = value + 18;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &ft);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((ft < CFG_FAST_TRANSITION_ENABLED_NAME_MIN) ||
               (ft > CFG_FAST_TRANSITION_ENABLED_NAME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "ft mode value %d is out of range"
                      " (Min: %d Max: %d)", ft,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change ft mode = %d", __func__, ft);

           pHddCtx->cfg_ini->isFastTransitionEnabled = ft;
           sme_UpdateFastTransitionEnabled((tHalHandle)(pHddCtx->hHal), ft);
       }

       else if (strncmp(command, "FASTREASSOC", 11) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tSirMacAddr targetApBssid;
           tANI_U8 trigger = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;
           tHalHandle hHal;
           v_U32_t roamId = 0;
           tCsrRoamModifyProfileFields modProfileFields;
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
           hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

           /* if not associated, no need to proceed with reassoc */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_reassoc_command_data(value, targetApBssid, &channel);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse reassoc command data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           /* if the target bssid is same as currently associated AP,
              issue reassoc to same AP */
           if (VOS_TRUE == vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "%s:11r Reassoc BSSID is same as currently associated AP bssid",
                         __func__);
               sme_GetModifyProfileFields(hHal, pAdapter->sessionId,
                                       &modProfileFields);
               sme_RoamReassoc(hHal, pAdapter->sessionId,
                            NULL, modProfileFields, &roamId, 1);
               return 0;
           }

           /* Check channel number is a valid channel number */
           if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter, channel))
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid Channel  [%d]", __func__, channel);
               return -EINVAL;
           }

           trigger = eSME_ROAM_TRIGGER_SCAN;

           /* Proceed with scan/roam */
           smeIssueFastRoamNeighborAPEvent(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                           &targetApBssid[0],
                                           (tSmeFastRoamTrigger)(trigger));
       }
#endif
#ifdef FEATURE_WLAN_ESE
       else if (strncmp(command, "SETCCXMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 eseMode = CFG_ESE_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Move pointer to ahead of SETCCXMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &eseMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ESE_FEATURE_ENABLED_MIN,
                      CFG_ESE_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           if ((eseMode < CFG_ESE_FEATURE_ENABLED_MIN) ||
               (eseMode > CFG_ESE_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Ese mode value %d is out of range"
                      " (Min: %d Max: %d)", eseMode,
                      CFG_ESE_FEATURE_ENABLED_MIN,
                      CFG_ESE_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change ese mode = %d", __func__, eseMode);

           pHddCtx->cfg_ini->isEseIniFeatureEnabled = eseMode;
           sme_UpdateIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal), eseMode);
       }
#endif
       else if (strncmp(command, "SETROAMSCANCONTROL", 18) == 0)
       {
           tANI_U8 *value = command;
           tANI_BOOLEAN roamScanControl = 0;

           /* Move pointer to ahead of SETROAMSCANCONTROL<delimiter> */
           value = value + 19;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanControl);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed ", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (0 != roamScanControl)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "roam scan control invalid value = %d",
                      roamScanControl);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan control = %d", __func__, roamScanControl);

           sme_SetRoamScanControl((tHalHandle)(pHddCtx->hHal), roamScanControl);
       }
#ifdef FEATURE_WLAN_OKC
       else if (strncmp(command, "SETOKCMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 okcMode = CFG_OKC_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Move pointer to ahead of SETOKCMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &okcMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_OKC_FEATURE_ENABLED_MIN,
                      CFG_OKC_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((okcMode < CFG_OKC_FEATURE_ENABLED_MIN) ||
               (okcMode > CFG_OKC_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Okc mode value %d is out of range"
                      " (Min: %d Max: %d)", okcMode,
                      CFG_OKC_FEATURE_ENABLED_MIN,
                      CFG_OKC_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change okc mode = %d", __func__, okcMode);

           pHddCtx->cfg_ini->isOkcIniFeatureEnabled = okcMode;
       }
#endif  /* FEATURE_WLAN_OKC */
       else if (strncmp(priv_data.buf, "GETROAMSCANCONTROL", 18) == 0)
       {
           tANI_BOOLEAN roamScanControl = sme_GetRoamScanControl((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   command, roamScanControl);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#ifdef WLAN_FEATURE_PACKET_FILTERING
       else if (strncmp(command, "ENABLE_PKTFILTER_IPV6", 21) == 0)
       {
           tANI_U8 filterType = 0;
           tANI_U8 *value = command;

           /* Move pointer to ahead of ENABLE_PKTFILTER_IPV6<delimiter> */
           value = value + 22;

           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &filterType);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype,
                * then also kstrtou8 fails
                */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range ", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (filterType != 0 && filterType != 1)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Accepted Values are 0 and 1 ", __func__);
               ret = -EINVAL;
               goto exit;
           }
           wlan_hdd_setIPv6Filter(WLAN_HDD_GET_CTX(pAdapter), filterType,
                   pAdapter->sessionId);
       }
#endif
       else if (strncmp(command, "BTCOEXMODE", 10) == 0 )
       {
           char *dhcpPhase;
           int ret;

           dhcpPhase = command + 11;
           if ('1' == *dhcpPhase)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         FL("send DHCP START indication"));

               pHddCtx->btCoexModeSet = TRUE;

               ret = wlan_hdd_scan_abort(pAdapter);
               if (ret < 0)
               {
                   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      FL("failed to abort existing scan %d"), ret);
               }

               sme_DHCPStartInd(pHddCtx->hHal, pAdapter->device_mode,
                                pAdapter->sessionId);
           }
           else if ('2' == *dhcpPhase)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         FL("send DHCP STOP indication"));

               pHddCtx->btCoexModeSet = FALSE;

               sme_DHCPStopInd(pHddCtx->hHal, pAdapter->device_mode,
                               pAdapter->sessionId);
           }
       }
       else if (strncmp(command, "SCAN-ACTIVE", 11) == 0)
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              FL("making default scan to ACTIVE"));
           pHddCtx->scan_info.scan_mode = eSIR_ACTIVE_SCAN;
       }
       else if (strncmp(command, "SCAN-PASSIVE", 12) == 0)
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              FL("making default scan to PASSIVE"));
           pHddCtx->scan_info.scan_mode = eSIR_PASSIVE_SCAN;
       }
       else if (strncmp(command, "GETDWELLTIME", 12) == 0)
       {
           hdd_config_t *pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
           char extra[32];
           tANI_U8 len = 0;

           memset(extra, 0, sizeof(extra));
           ret = hdd_get_dwell_time(pCfg, command, extra, sizeof(extra), &len);
           if (ret != 0 || copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
           ret = len;
       }
       else if (strncmp(command, "SETDWELLTIME", 12) == 0)
       {
           ret = hdd_set_dwell_time(pAdapter, command);
       }
       else if ( strncasecmp(command, "MIRACAST", 8) == 0 )
       {
           tANI_U8 filterType = 0;
           tANI_U8 *value;
           value = command + 9;

           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &filterType);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype,
                * then also kstrtou8 fails
                */
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              "%s: kstrtou8 failed range ", __func__);
              ret = -EINVAL;
              goto exit;
           }
           if ((filterType < WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL ) ||
               (filterType > WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Accepted Values are 0 to 2. 0-Disabled, 1-Source,"
                      " 2-Sink ", __func__);
               ret = -EINVAL;
               goto exit;
           }
           //Filtertype value should be either 0-Disabled, 1-Source, 2-sink
           pHddCtx->drvr_miracast = filterType;
           pScanInfo =  &pHddCtx->scan_info;
           if (filterType && pScanInfo != NULL &&
               pHddCtx->scan_info.mScanPending)
           {
              /*Miracast Session started. Abort Scan */
              VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s, Aborting Scan For Miracast",__func__);
              hdd_abort_mac_scan(pHddCtx, pScanInfo->sessionId,
                                 eCSR_SCAN_ABORT_DEFAULT);
           }
           hdd_tx_rx_pkt_cnt_stat_timer_handler(pHddCtx);
           sme_SetMiracastMode(pHddCtx->hHal, pHddCtx->drvr_miracast);
        }
       else if (strncmp(command, "SETMCRATE", 9) == 0)
       {
           tANI_U8 *value = command;
           int      targetRate;
           tSirRateUpdateInd *rateUpdate;
           eHalStatus status;

           /* Only valid for SAP mode */
           if (WLAN_HDD_SOFTAP != pAdapter->device_mode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: SAP mode is not running", __func__);
               ret = -EFAULT;
               goto exit;
           }

           /* Move pointer to ahead of SETMCRATE<delimiter> */
           /* input value is in units of hundred kbps */
           value = value + 10;
           /* Convert the value from ascii to integer, decimal base */
           ret = kstrtouint(value, 10, &targetRate);

           rateUpdate = (tSirRateUpdateInd *)vos_mem_malloc(sizeof(tSirRateUpdateInd));
           if (NULL == rateUpdate)
           {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: SETMCRATE indication alloc fail", __func__);
              ret = -EFAULT;
              goto exit;
           }
           vos_mem_zero(rateUpdate, sizeof(tSirRateUpdateInd ));

           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "MC Target rate %d", targetRate);
           /* Ignore unicast */
           rateUpdate->ucastDataRate = -1;
           rateUpdate->mcastDataRate24GHz = targetRate;
           rateUpdate->mcastDataRate5GHz = targetRate;
           rateUpdate->mcastDataRate24GHzTxFlag = 0;
           rateUpdate->mcastDataRate5GHzTxFlag = 0;
           status = sme_SendRateUpdateInd(pHddCtx->hHal, rateUpdate);
           if (eHAL_STATUS_SUCCESS != status)
           {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: SET_MC_RATE failed", __func__);
              vos_mem_free(rateUpdate);
              ret = -EFAULT;
              goto exit;
           }
       }
#ifdef FEATURE_WLAN_BATCH_SCAN
       else if (strncmp(command, "WLS_BATCHING", 12) == 0)
       {
           ret = hdd_handle_batch_scan_ioctl(pAdapter, &priv_data, command);
       }
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
       else if (strncmp(command, "SETCCXROAMSCANCHANNELS", 22) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_channellist(value, ChannelList, &numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (numChannels > WNI_CFG_VALID_CHANNEL_LIST_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: number of channels (%d) supported exceeded max (%d)", __func__,
                   numChannels, WNI_CFG_VALID_CHANNEL_LIST_LEN);
               ret = -EINVAL;
               goto exit;
           }
           status = sme_SetEseRoamScanChannelList((tHalHandle)(pHddCtx->hHal),
                                                  ChannelList,
                                                  numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to update channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }
       }
       else if (strncmp(command, "GETTSMSTATS", 11) == 0)
       {
           tANI_U8            *value = command;
           char                extra[128] = {0};
           int                 len = 0;
           tANI_U8             tid = 0;
           hdd_station_ctx_t  *pHddStaCtx = NULL;
           tAniTrafStrmMetrics tsmMetrics;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           /* if not associated, return error */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           /* Move pointer to ahead of GETTSMSTATS<delimiter> */
           value = value + 12;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &tid);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      TID_MIN_VALUE,
                      TID_MAX_VALUE);
               ret = -EINVAL;
               goto exit;
           }

           if ((tid < TID_MIN_VALUE) || (tid > TID_MAX_VALUE))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "tid value %d is out of range"
                      " (Min: %d Max: %d)", tid,
                      TID_MIN_VALUE,
                      TID_MAX_VALUE);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to get tsm stats tid = %d", __func__, tid);

           if (VOS_STATUS_SUCCESS != hdd_get_tsm_stats(pAdapter, tid, &tsmMetrics))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to get tsm stats", __func__);
               ret = -EFAULT;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                          "UplinkPktQueueDly(%d)\n"
                          "UplinkPktQueueDlyHist[0](%d)\n"
                          "UplinkPktQueueDlyHist[1](%d)\n"
                          "UplinkPktQueueDlyHist[2](%d)\n"
                          "UplinkPktQueueDlyHist[3](%d)\n"
                          "UplinkPktTxDly(%u)\n"
                          "UplinkPktLoss(%d)\n"
                          "UplinkPktCount(%d)\n"
                          "RoamingCount(%d)\n"
                          "RoamingDly(%d)", tsmMetrics.UplinkPktQueueDly,
                          tsmMetrics.UplinkPktQueueDlyHist[0],
                          tsmMetrics.UplinkPktQueueDlyHist[1],
                          tsmMetrics.UplinkPktQueueDlyHist[2],
                          tsmMetrics.UplinkPktQueueDlyHist[3],
                          tsmMetrics.UplinkPktTxDly, tsmMetrics.UplinkPktLoss,
                          tsmMetrics.UplinkPktCount, tsmMetrics.RoamingCount, tsmMetrics.RoamingDly);

           /* Output TSM stats is of the format
                   GETTSMSTATS [PktQueueDly] [PktQueueDlyHist[0]]:[PktQueueDlyHist[1]] ...[RoamingDly]
                   eg., GETTSMSTATS 10 1:0:0:161 20 1 17 8 39800 */
           len = scnprintf(extra, sizeof(extra), "%s %d %d:%d:%d:%d %u %d %d %d %d", command,
                  tsmMetrics.UplinkPktQueueDly, tsmMetrics.UplinkPktQueueDlyHist[0],
                  tsmMetrics.UplinkPktQueueDlyHist[1], tsmMetrics.UplinkPktQueueDlyHist[2],
                  tsmMetrics.UplinkPktQueueDlyHist[3], tsmMetrics.UplinkPktTxDly,
                  tsmMetrics.UplinkPktLoss, tsmMetrics.UplinkPktCount, tsmMetrics.RoamingCount,
                  tsmMetrics.RoamingDly);

           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETCCKMIE", 9) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 *cckmIe = NULL;
           tANI_U8 cckmIeLen = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_get_cckm_ie(value, &cckmIe, &cckmIeLen);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse cckm ie data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (cckmIeLen > DOT11F_IE_RSN_MAX_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: CCKM Ie input length is more than max[%d]", __func__,
                  DOT11F_IE_RSN_MAX_LEN);
               vos_mem_free(cckmIe);
               cckmIe = NULL;
               ret = -EINVAL;
               goto exit;
           }
           sme_SetCCKMIe((tHalHandle)(pHddCtx->hHal), pAdapter->sessionId, cckmIe, cckmIeLen);
           vos_mem_free(cckmIe);
           cckmIe = NULL;
       }
       else if (strncmp(command, "CCXBEACONREQ", 12) == 0)
       {
           tANI_U8 *value = command;
           tCsrEseBeaconReq eseBcnReq;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_ese_beacon_req(value, &eseBcnReq);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse ese beacon req", __func__);
               ret = -EINVAL;
               goto exit;
           }
           if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) {
               hddLog(VOS_TRACE_LEVEL_INFO, FL("Not associated"));
               hdd_indicateEseBcnReportNoResults (pAdapter,
                                      eseBcnReq.bcnReq[0].measurementToken,
                                      0x02,  //BIT(1) set for measurement done
                                      0);    // no BSS
               goto exit;
           }

           status = sme_SetEseBeaconRequest((tHalHandle)(pHddCtx->hHal), pAdapter->sessionId, &eseBcnReq);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: sme_SetEseBeaconRequest failed (%d)", __func__, status);
               ret = -EINVAL;
               goto exit;
           }
       }
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
       else if (strncmp(command, "GETBCNMISSRATE", 14) == 0)
       {
           eHalStatus status;
           char buf[32], len;
           long waitRet;
           bcnMissRateContext_t getBcnMissRateCtx;

           hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               hddLog(VOS_TRACE_LEVEL_WARN,
                    FL("GETBCNMISSRATE: STA is not in connected state"));
               ret = -1;
               goto exit;
           }

           init_completion(&(getBcnMissRateCtx.completion));
           getBcnMissRateCtx.magic = BCN_MISS_RATE_CONTEXT_MAGIC;

           status = sme_getBcnMissRate((tHalHandle)(pHddCtx->hHal),
                                       pAdapter->sessionId,
                                       (void *)getBcnMissRateCB,
                                       (void *)(&getBcnMissRateCtx));
           if( eHAL_STATUS_SUCCESS != status)
           {
               hddLog(VOS_TRACE_LEVEL_INFO,
                    FL("GETBCNMISSRATE: fail to post WDA cmd"));
                ret = -EINVAL;
                goto exit;
           }

           waitRet = wait_for_completion_interruptible_timeout
                           (&getBcnMissRateCtx.completion, BCN_MISS_RATE_TIME);
           if(waitRet <= 0)
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                         FL("failed to wait on bcnMissRateComp %d"), ret);

               //Make magic number to zero so that callback is not called.
               spin_lock(&hdd_context_lock);
               getBcnMissRateCtx.magic = 0x0;
               spin_unlock(&hdd_context_lock);
               ret = -EINVAL;
               goto exit;
           }

           hddLog(VOS_TRACE_LEVEL_INFO,
                  FL("GETBCNMISSRATE: bcnMissRate: %d"), gbcnMissRate);

           len = snprintf(buf, sizeof(buf), "GETBCNMISSRATE %d", gbcnMissRate);
           if (copy_to_user(priv_data.buf, &buf, len + 1))
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
           ret = len;
       }
#ifdef FEATURE_WLAN_TDLS
       else if (strncmp(command, "TDLSSECONDARYCHANNELOFFSET", 26) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to ahead of TDLSOFFCH*/
           value += 26;
           sscanf(value, "%d", &set_value);
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "%s: Tdls offchannel offset:%d",
                     __func__, set_value);
           ret = iw_set_tdlssecoffchanneloffset(pHddCtx, set_value);
           if (ret < 0)
           {
               ret = -EINVAL;
               goto exit;
           }

       } else if (strncmp(command, "TDLSOFFCHANNELMODE", 18) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to ahead of tdlsoffchnmode*/
           value += 18;
           sscanf(value, "%d", &set_value);
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "%s: Tdls offchannel mode:%d",
                     __func__, set_value);
           ret = iw_set_tdlsoffchannelmode(pAdapter, set_value);
           if (ret < 0)
           {
               ret = -EINVAL;
               goto exit;
           }
       } else if (strncmp(command, "TDLSOFFCHANNEL", 14) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to ahead of TDLSOFFCH*/
           value += 14;
           sscanf(value, "%d", &set_value);
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "%s: Tdls offchannel num: %d",
                     __func__, set_value);
           ret = iw_set_tdlsoffchannel(pHddCtx, set_value);
           if (ret < 0)
           {
               ret = -EINVAL;
               goto exit;
           }
       }
#endif
       else if (strncmp(command, "GETFWSTATS", 10) == 0)
       {
           eHalStatus status;
           char *buf = NULL;
           char len;
           long waitRet;
           fwStatsContext_t fwStatsCtx;
           fwStatsResult_t *fwStatsRsp = &(pAdapter->fwStatsRsp);
           tANI_U8 *ptr = command;
           int stats = *(ptr + 11) - '0';

           hddLog(VOS_TRACE_LEVEL_INFO, FL("stats = %d "),stats);
           if (!IS_FEATURE_FW_STATS_ENABLE)
           {
               hddLog(VOS_TRACE_LEVEL_INFO,
                     FL("Get Firmware stats feature not supported"));
               ret = -EINVAL;
               goto exit;
           }

           if (FW_STATS_MAX <= stats || 0 >= stats)
           {
               hddLog(VOS_TRACE_LEVEL_INFO,
                        FL(" stats %d not supported"),stats);
               ret = -EINVAL;
               goto exit;
           }

           init_completion(&(fwStatsCtx.completion));
           fwStatsCtx.magic = FW_STATS_CONTEXT_MAGIC;
           fwStatsCtx.pAdapter = pAdapter;
           fwStatsRsp->type = 0;
           status = sme_GetFwStats( (tHalHandle)pHddCtx->hHal, stats,
                                   (&fwStatsCtx), hdd_FWStatisCB);
           if (eHAL_STATUS_SUCCESS != status)
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL(" fail to post WDA cmd status = %d"), status);
               ret = -EINVAL;
               goto exit;
           }
           waitRet = wait_for_completion_timeout
                             (&(fwStatsCtx.completion), FW_STATE_WAIT_TIME);
           if (waitRet <= 0)
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("failed to wait on GwtFwstats"));
               //Make magic number to zero so that callback is not executed.
               spin_lock(&hdd_context_lock);
               fwStatsCtx.magic = 0x0;
               spin_unlock(&hdd_context_lock);
               ret = -EINVAL;
               goto exit;
           }
           if (fwStatsRsp->type)
           {
               buf = kmalloc(FW_STATE_RSP_LEN, GFP_KERNEL);
               if (!buf)
               {
                 hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL(" failed to allocate memory"));
                 ret = -ENOMEM;
                 goto exit;
               }
               switch( fwStatsRsp->type )
               {
                   case FW_UBSP_STATS:
                   {
                        len = snprintf(buf, FW_STATE_RSP_LEN,
                              "GETFWSTATS: ubsp_enter_cnt %d ubsp_jump_ddr_cnt %d",
                              fwStatsRsp->hddFwStatsData.ubspStats.ubsp_enter_cnt,
                              fwStatsRsp->hddFwStatsData.ubspStats.ubsp_jump_ddr_cnt);
                   }
                   break;
                   default:
                   {
                        hddLog(VOS_TRACE_LEVEL_ERROR, FL( "No handling for stats type %d"),fwStatsRsp->type);
                        ret = -EFAULT;
                        kfree(buf);
                        goto exit;
                   }
               }
               if (copy_to_user(priv_data.buf, buf, len + 1))
               {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                      FL(" failed to copy data to user buffer"));
                   ret = -EFAULT;
                   kfree(buf);
                   goto exit;
               }
               ret = len;
               kfree(buf);
           }
           else
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                   FL("failed to fetch the stats"));
               ret = -EFAULT;
               goto exit;
           }

       }
       else {
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_UNSUPPORTED_IOCTL,
                            pAdapter->sessionId, 0));
           hddLog( VOS_TRACE_LEVEL_WARN, FL("Unsupported GUI command %s"),
                   command);
       }
   }
exit:
   if (command)
   {
       kfree(command);
   }
   return ret;
}

#ifdef CONFIG_COMPAT
static int hdd_driver_compat_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   struct {
      compat_uptr_t buf;
      int used_len;
      int total_len;
   } compat_priv_data;
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&compat_priv_data, ifr->ifr_data,
                      sizeof(compat_priv_data))) {
       ret = -EFAULT;
       goto exit;
   }
   priv_data.buf = compat_ptr(compat_priv_data.buf);
   priv_data.used_len = compat_priv_data.used_len;
   priv_data.total_len = compat_priv_data.total_len;
   ret = hdd_driver_command(pAdapter, &priv_data);
 exit:
   return ret;
}
#else /* CONFIG_COMPAT */
static int hdd_driver_compat_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   /* will never be invoked */
   return 0;
}
#endif /* CONFIG_COMPAT */

static int hdd_driver_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(priv_data))) {
       ret = -EFAULT;
   } else {
      ret = hdd_driver_command(pAdapter, &priv_data);
   }
   return ret;
}

int __hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   hdd_adapter_t *pAdapter;
   hdd_context_t *pHddCtx;
   int ret;

   pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   if (NULL == pAdapter) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s: HDD adapter context is Null", __func__);
      ret = -ENODEV;
      goto exit;
   }
   if (dev != pAdapter->dev) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s: HDD adapter/dev inconsistency", __func__);
      ret = -ENODEV;
      goto exit;
   }

   if ((!ifr) || (!ifr->ifr_data)) {
      ret = -EINVAL;
      goto exit;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (ret) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: invalid context", __func__);
      ret = -EBUSY;
      goto exit;
   }

   switch (cmd) {
   case (SIOCDEVPRIVATE + 1):
      if (is_compat_task())
         ret = hdd_driver_compat_ioctl(pAdapter, ifr);
      else
         ret = hdd_driver_ioctl(pAdapter, ifr);
      break;
   default:
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unknown ioctl %d",
             __func__, cmd);
      ret = -EINVAL;
      break;
   }
 exit:
   return ret;
}

int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __hdd_ioctl(dev, ifr, cmd);
    vos_ssr_unprotect(__func__);

    return ret;
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/**---------------------------------------------------------------------------

  \brief hdd_parse_ese_beacon_req() - Parse ese beacon request

  This function parses the ese beacon request passed in the format
  CCXBEACONREQ<space><Number of fields><space><Measurement token>
  <space>Channel 1<space>Scan Mode <space>Meas Duration<space>Channel N
  <space>Scan Mode N<space>Meas Duration N
  if the Number of bcn req fields (N) does not match with the actual number of fields passed
  then take N.
  <Meas Token><Channel><Scan Mode> and <Meas Duration> are treated as one pair
  For example, CCXBEACONREQ 2 1 1 1 30 2 44 0 40.
  This function does not take care of removing duplicate channels from the list

  \param  - pValue Pointer to data
  \param  - pEseBcnReq output pointer to store parsed ie information

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static VOS_STATUS hdd_parse_ese_beacon_req(tANI_U8 *pValue,
                                     tCsrEseBeaconReq *pEseBcnReq)
{
    tANI_U8 *inPtr = pValue;
    int tempInt = 0;
    int j = 0, i = 0, v = 0;
    char buf[32];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }
    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr) return -EINVAL;

    /*getting the first argument ie measurement token*/
    v = sscanf(inPtr, "%31s ", buf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(buf, 10, &tempInt);
    if ( v < 0) return -EINVAL;

    pEseBcnReq->numBcnReqIe = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "Number of Bcn Req Ie fields(%d)", pEseBcnReq->numBcnReqIe);

    for (j = 0; j < (pEseBcnReq->numBcnReqIe); j++)
    {
        for (i = 0; i < 4; i++)
        {
            /*inPtr pointing to the beginning of first space after number of ie fields*/
            inPtr = strpbrk( inPtr, " " );
            /*no ie data after the number of ie fields argument*/
            if (NULL == inPtr) return -EINVAL;

            /*removing empty space*/
            while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr)) inPtr++;

            /*no ie data after the number of ie fields argument and spaces*/
            if ( '\0' == *inPtr ) return -EINVAL;

            v = sscanf(inPtr, "%31s ", buf);
            if (1 != v) return -EINVAL;

            v = kstrtos32(buf, 10, &tempInt);
            if (v < 0) return -EINVAL;

            switch (i)
            {
                case 0:  /* Measurement token */
                if (tempInt <= 0)
                {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Measurement Token(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].measurementToken = tempInt;
                break;

                case 1:  /* Channel number */
                if ((tempInt <= 0) ||
                    (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX))
                {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Channel Number(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].channel = tempInt;
                break;

                case 2:  /* Scan mode */
                if ((tempInt < eSIR_PASSIVE_SCAN) || (tempInt > eSIR_BEACON_TABLE))
                {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Scan Mode(%d) Expected{0|1|2}", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].scanMode= tempInt;
                break;

                case 3:  /* Measurement duration */
                if (((tempInt <= 0) && (pEseBcnReq->bcnReq[j].scanMode != eSIR_BEACON_TABLE)) ||
                    ((tempInt < 0) && (pEseBcnReq->bcnReq[j].scanMode == eSIR_BEACON_TABLE)))
                {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Measurement Duration(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].measurementDuration = tempInt;
                break;
            }
        }
    }

    for (j = 0; j < pEseBcnReq->numBcnReqIe; j++)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "Index(%d) Measurement Token(%u)Channel(%u) Scan Mode(%u) Measurement Duration(%u)\n",
                   j,
                   pEseBcnReq->bcnReq[j].measurementToken,
                   pEseBcnReq->bcnReq[j].channel,
                   pEseBcnReq->bcnReq[j].scanMode,
                   pEseBcnReq->bcnReq[j].measurementDuration);
    }

    return VOS_STATUS_SUCCESS;
}

static void hdd_GetTsmStatsCB( tAniTrafStrmMetrics tsmMetrics, const tANI_U32 staId, void *pContext )
{
   struct statsContext *pStatsContext = NULL;
   hdd_adapter_t       *pAdapter = NULL;

   if (NULL == pContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pContext [%p]",
             __func__, pContext);
      return;
   }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;
   if ((NULL == pAdapter) || (STATS_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the tsm stats */
   pAdapter->tsmStats.UplinkPktQueueDly = tsmMetrics.UplinkPktQueueDly;
   vos_mem_copy(pAdapter->tsmStats.UplinkPktQueueDlyHist,
                 tsmMetrics.UplinkPktQueueDlyHist,
                 sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist)/
                 sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist[0]));
   pAdapter->tsmStats.UplinkPktTxDly = tsmMetrics.UplinkPktTxDly;
   pAdapter->tsmStats.UplinkPktLoss = tsmMetrics.UplinkPktLoss;
   pAdapter->tsmStats.UplinkPktCount = tsmMetrics.UplinkPktCount;
   pAdapter->tsmStats.RoamingCount = tsmMetrics.RoamingCount;
   pAdapter->tsmStats.RoamingDly = tsmMetrics.RoamingDly;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}



static VOS_STATUS  hdd_get_tsm_stats(hdd_adapter_t *pAdapter, const tANI_U8 tid,
                                         tAniTrafStrmMetrics* pTsmMetrics)
{
   hdd_station_ctx_t *pHddStaCtx = NULL;
   eHalStatus         hstatus;
   VOS_STATUS         vstatus = VOS_STATUS_SUCCESS;
   long               lrc;
   struct statsContext context;
   hdd_context_t     *pHddCtx = NULL;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter is NULL", __func__);
       return VOS_STATUS_E_FAULT;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   /* we are connected prepare our callback context */
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;

   /* query tsm stats */
   hstatus = sme_GetTsmStats(pHddCtx->hHal, hdd_GetTsmStatsCB,
                         pHddStaCtx->conn_info.staId[ 0 ],
                         pHddStaCtx->conn_info.bssId,
                         &context, pHddCtx->pvosContext, tid);

   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unable to retrieve statistics",
             __func__);
      vstatus = VOS_STATUS_E_FAULT;
   }
   else
   {
      /* request was sent -- wait for the response */
      lrc = wait_for_completion_interruptible_timeout(&context.completion,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      if (lrc <= 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: SME %s while retrieving statistics",
                __func__, (0 == lrc) ? "timeout" : "interrupt");
         vstatus = VOS_STATUS_E_TIMEOUT;
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   if (VOS_STATUS_SUCCESS == vstatus)
   {
      pTsmMetrics->UplinkPktQueueDly = pAdapter->tsmStats.UplinkPktQueueDly;
      vos_mem_copy(pTsmMetrics->UplinkPktQueueDlyHist,
                   pAdapter->tsmStats.UplinkPktQueueDlyHist,
                   sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist)/
                   sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist[0]));
      pTsmMetrics->UplinkPktTxDly = pAdapter->tsmStats.UplinkPktTxDly;
      pTsmMetrics->UplinkPktLoss = pAdapter->tsmStats.UplinkPktLoss;
      pTsmMetrics->UplinkPktCount = pAdapter->tsmStats.UplinkPktCount;
      pTsmMetrics->RoamingCount = pAdapter->tsmStats.RoamingCount;
      pTsmMetrics->RoamingDly = pAdapter->tsmStats.RoamingDly;
   }
   return vstatus;
}
#endif /*FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
void hdd_getBand_helper(hdd_context_t *pHddCtx, int *pBand)
{
    eCsrBand band = -1;
    sme_GetFreqBand((tHalHandle)(pHddCtx->hHal), &band);
    switch (band)
    {
        case eCSR_BAND_ALL:
            *pBand = WLAN_HDD_UI_BAND_AUTO;
            break;

        case eCSR_BAND_24:
            *pBand = WLAN_HDD_UI_BAND_2_4_GHZ;
            break;

        case eCSR_BAND_5G:
            *pBand = WLAN_HDD_UI_BAND_5_GHZ;
            break;

        default:
            hddLog( VOS_TRACE_LEVEL_WARN, "%s: Invalid Band %d", __func__, band);
            *pBand = -1;
            break;
    }
}

/**---------------------------------------------------------------------------

  \brief hdd_parse_send_action_frame_data() - HDD Parse send action frame data

  This function parses the send action frame data passed in the format
  SENDACTIONFRAME<space><bssid><space><channel><space><dwelltime><space><data>

  \param  - pValue Pointer to input data
  \param  - pTargetApBssid Pointer to target Ap bssid
  \param  - pChannel Pointer to the Target AP channel
  \param  - pDwellTime Pointer to the time to stay off-channel after transmitting action frame
  \param  - pBuf Pointer to data
  \param  - pBufLen Pointer to data length

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_send_action_frame_data(tANI_U8 *pValue, tANI_U8 *pTargetApBssid, tANI_U8 *pChannel,
                                            tANI_U8 *pDwellTime, tANI_U8 **pBuf, tANI_U8 *pBufLen)
{
    tANI_U8 *inPtr = pValue;
    tANI_U8 *dataEnd;
    int tempInt;
    int j = 0;
    int i = 0;
    int v = 0;
    tANI_U8 tempBuf[32];
    tANI_U8 tempByte = 0;
    /* 12 hexa decimal digits, 5 ':' and '\0' */
    tANI_U8 macAddress[18];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    v = sscanf(inPtr, "%17s", macAddress);
    if (!((1 == v) && hdd_is_valid_mac_address(macAddress)))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Invalid MAC address or All hex inputs are not read (%d)", v);
        return -EINVAL;
    }

    pTargetApBssid[0] = hdd_parse_hex(macAddress[0]) << 4 | hdd_parse_hex(macAddress[1]);
    pTargetApBssid[1] = hdd_parse_hex(macAddress[3]) << 4 | hdd_parse_hex(macAddress[4]);
    pTargetApBssid[2] = hdd_parse_hex(macAddress[6]) << 4 | hdd_parse_hex(macAddress[7]);
    pTargetApBssid[3] = hdd_parse_hex(macAddress[9]) << 4 | hdd_parse_hex(macAddress[10]);
    pTargetApBssid[4] = hdd_parse_hex(macAddress[12]) << 4 | hdd_parse_hex(macAddress[13]);
    pTargetApBssid[5] = hdd_parse_hex(macAddress[15]) << 4 | hdd_parse_hex(macAddress[16]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the channel number */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0 || tempInt <= 0 || tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX )
     return -EINVAL;

    *pChannel = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;
    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the dwell time */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0 || tempInt < 0) return -EINVAL;

    *pDwellTime = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;
    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /* find the length of data */
    dataEnd = inPtr;
    while(('\0' !=  *dataEnd) )
    {
        dataEnd++;
    }
    *pBufLen = dataEnd - inPtr ;
    if ( *pBufLen <= 0)  return -EINVAL;

    /* Allocate the number of bytes based on the number of input characters
       whether it is even or odd.
       if the number of input characters are even, then we need N/2 byte.
       if the number of input characters are odd, then we need do (N+1)/2 to
       compensate rounding off.
       For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
       If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes */
    *pBuf = vos_mem_malloc((*pBufLen + 1)/2);
    if (NULL == *pBuf)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return -EINVAL;
    }

    /* the buffer received from the upper layer is character buffer,
       we need to prepare the buffer taking 2 characters in to a U8 hex decimal number
       for example 7f0000f0...form a buffer to contain 7f in 0th location, 00 in 1st
       and f0 in 3rd location */
    for (i = 0, j = 0; j < *pBufLen; j += 2)
    {
        if( j+1 == *pBufLen)
        {
             tempByte = hdd_parse_hex(inPtr[j]);
        }
        else
        {
              tempByte = (hdd_parse_hex(inPtr[j]) << 4) | (hdd_parse_hex(inPtr[j + 1]));
        }
        (*pBuf)[i++] = tempByte;
    }
    *pBufLen = i;
    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_parse_channellist() - HDD Parse channel list

  This function parses the channel list passed in the format
  SETROAMSCANCHANNELS<space><Number of channels><space>Channel 1<space>Channel 2<space>Channel N
  if the Number of channels (N) does not match with the actual number of channels passed
  then take the minimum of N and count of (Ch1, Ch2, ...Ch M)
  For example, if SETROAMSCANCHANNELS 3 36 40 44 48, only 36, 40 and 44 shall be taken.
  If SETROAMSCANCHANNELS 5 36 40 44 48, ignore 5 and take 36, 40, 44 and 48.
  This function does not take care of removing duplicate channels from the list

  \param  - pValue Pointer to input channel list
  \param  - ChannelList Pointer to local output array to record channel list
  \param  - pNumChannels Pointer to number of roam scan channels

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_channellist(tANI_U8 *pValue, tANI_U8 *pChannelList, tANI_U8 *pNumChannels)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int j = 0;
    int v = 0;
    char buf[32];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the first argument ie the number of channels*/
    v = sscanf(inPtr, "%31s ", buf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(buf, 10, &tempInt);
    if ((v < 0) ||
        (tempInt <= 0) ||
        (tempInt > WNI_CFG_VALID_CHANNEL_LIST_LEN))
    {
       return -EINVAL;
    }

    *pNumChannels = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "Number of channels are: %d", *pNumChannels);

    for (j = 0; j < (*pNumChannels); j++)
    {
        /*inPtr pointing to the beginning of first space after number of channels*/
        inPtr = strpbrk( inPtr, " " );
        /*no channel list after the number of channels argument*/
        if (NULL == inPtr)
        {
            if (0 != j)
            {
                *pNumChannels = j;
                return VOS_STATUS_SUCCESS;
            }
            else
            {
                return -EINVAL;
            }
        }

        /*removing empty space*/
        while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr)) inPtr++;

        /*no channel list after the number of channels argument and spaces*/
        if ( '\0' == *inPtr )
        {
            if (0 != j)
            {
                *pNumChannels = j;
                return VOS_STATUS_SUCCESS;
            }
            else
            {
                return -EINVAL;
            }
        }

        v = sscanf(inPtr, "%31s ", buf);
        if (1 != v) return -EINVAL;

        v = kstrtos32(buf, 10, &tempInt);
        if ((v < 0) ||
            (tempInt <= 0) ||
            (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX))
        {
           return -EINVAL;
        }
        pChannelList[j] = tempInt;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Channel %d added to preferred channel list",
                   pChannelList[j] );
    }

    return VOS_STATUS_SUCCESS;
}


/**---------------------------------------------------------------------------

  \brief hdd_parse_reassoc_command_data() - HDD Parse reassoc command data

  This function parses the reasoc command data passed in the format
  REASSOC<space><bssid><space><channel>

  \param  - pValue Pointer to input data (its a NUL terminated string)
  \param  - pTargetApBssid Pointer to target Ap bssid
  \param  - pChannel Pointer to the Target AP channel

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_reassoc_command_data(tANI_U8 *pValue,
                            tANI_U8 *pTargetApBssid, tANI_U8 *pChannel)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    tANI_U8 tempBuf[32];
    /* 12 hexa decimal digits, 5 ':' and '\0' */
    tANI_U8 macAddress[18];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    v = sscanf(inPtr, "%17s", macAddress);
    if (!((1 == v) && hdd_is_valid_mac_address(macAddress)))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Invalid MAC address or All hex inputs are not read (%d)", v);
        return -EINVAL;
    }

    pTargetApBssid[0] = hdd_parse_hex(macAddress[0]) << 4 | hdd_parse_hex(macAddress[1]);
    pTargetApBssid[1] = hdd_parse_hex(macAddress[3]) << 4 | hdd_parse_hex(macAddress[4]);
    pTargetApBssid[2] = hdd_parse_hex(macAddress[6]) << 4 | hdd_parse_hex(macAddress[7]);
    pTargetApBssid[3] = hdd_parse_hex(macAddress[9]) << 4 | hdd_parse_hex(macAddress[10]);
    pTargetApBssid[4] = hdd_parse_hex(macAddress[12]) << 4 | hdd_parse_hex(macAddress[13]);
    pTargetApBssid[5] = hdd_parse_hex(macAddress[15]) << 4 | hdd_parse_hex(macAddress[16]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the channel number */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(tempBuf, 10, &tempInt);
    if ((v < 0) ||
        (tempInt < 0) ||
        (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX))
    {
        return -EINVAL;
    }

    *pChannel = tempInt;
    return VOS_STATUS_SUCCESS;
}

#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/**---------------------------------------------------------------------------

  \brief hdd_parse_get_cckm_ie() - HDD Parse and fetch the CCKM IE

  This function parses the SETCCKM IE command
  SETCCKMIE<space><ie data>

  \param  - pValue Pointer to input data
  \param  - pCckmIe Pointer to output cckm Ie
  \param  - pCckmIeLen Pointer to output cckm ie length

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_get_cckm_ie(tANI_U8 *pValue, tANI_U8 **pCckmIe,
                                 tANI_U8 *pCckmIeLen)
{
    tANI_U8 *inPtr = pValue;
    tANI_U8 *dataEnd;
    int      j = 0;
    int      i = 0;
    tANI_U8  tempByte = 0;

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /* find the length of data */
    dataEnd = inPtr;
    while(('\0' !=  *dataEnd) )
    {
        dataEnd++;
        ++(*pCckmIeLen);
    }
    if ( *pCckmIeLen <= 0)  return -EINVAL;

    /* Allocate the number of bytes based on the number of input characters
       whether it is even or odd.
       if the number of input characters are even, then we need N/2 byte.
       if the number of input characters are odd, then we need do (N+1)/2 to
       compensate rounding off.
       For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
       If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes */
    *pCckmIe = vos_mem_malloc((*pCckmIeLen + 1)/2);
    if (NULL == *pCckmIe)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return -EINVAL;
    }
    vos_mem_zero(*pCckmIe, (*pCckmIeLen + 1)/2);
    /* the buffer received from the upper layer is character buffer,
       we need to prepare the buffer taking 2 characters in to a U8 hex decimal number
       for example 7f0000f0...form a buffer to contain 7f in 0th location, 00 in 1st
       and f0 in 3rd location */
    for (i = 0, j = 0; j < *pCckmIeLen; j += 2)
    {
        tempByte = (hdd_parse_hex(inPtr[j]) << 4) | (hdd_parse_hex(inPtr[j + 1]));
        (*pCckmIe)[i++] = tempByte;
    }
    *pCckmIeLen = i;

    return VOS_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/**---------------------------------------------------------------------------

  \brief hdd_is_valid_mac_address() - Validate MAC address

  This function validates whether the given MAC address is valid or not
  Expected MAC address is of the format XX:XX:XX:XX:XX:XX
  where X is the hexa decimal digit character and separated by ':'
  This algorithm works even if MAC address is not separated by ':'

  This code checks given input string mac contains exactly 12 hexadecimal digits.
  and a separator colon : appears in the input string only after
  an even number of hex digits.

  \param  - pMacAddr pointer to the input MAC address
  \return - 1 for valid and 0 for invalid

  --------------------------------------------------------------------------*/

v_BOOL_t hdd_is_valid_mac_address(const tANI_U8 *pMacAddr)
{
    int xdigit = 0;
    int separator = 0;
    while (*pMacAddr)
    {
        if (isxdigit(*pMacAddr))
        {
            xdigit++;
        }
        else if (':' == *pMacAddr)
        {
            if (0 == xdigit || ((xdigit / 2) - 1) != separator)
                break;

            ++separator;
        }
        else
        {
            separator = -1;
            /* Invalid MAC found */
            return 0;
        }
        ++pMacAddr;
    }
    return (xdigit == 12 && (separator == 5 || separator == 0));
}

/**---------------------------------------------------------------------------

  \brief __hdd_open() - HDD Open function

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
int __hdd_open(struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t in_standby = TRUE;

   if (NULL == pAdapter) 
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: pAdapter is Null", __func__);
      return -ENODEV;
   }
   
   pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;
   MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_OPEN_REQUEST,
                    pAdapter->sessionId, pAdapter->device_mode));
   if (NULL == pHddCtx)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD context is Null", __func__);
      return -ENODEV;
   }

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( (NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status) )
   {
      if (test_bit(DEVICE_IFACE_OPENED, &pAdapterNode->pAdapter->event_flags))
      {
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: chip already out of standby",
                __func__);
         in_standby = FALSE;
         break;
      }
      else
      {
         status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
   }
 
   if (TRUE == in_standby)
   {
       if (VOS_STATUS_SUCCESS != wlan_hdd_exit_lowpower(pHddCtx, pAdapter))
       {
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to bring " 
                   "wlan out of power save", __func__);
           return -EINVAL;
       }
   }
   
   set_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   if (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) 
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: Enabling Tx Queues", __func__);
       /* Enable TX queues only when we are connected */
       netif_tx_start_all_queues(dev);
   }

   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_open() - Wrapper function for __hdd_open to protect it from SSR

  This is called in response to ifconfig up

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
int hdd_open(struct net_device *dev)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __hdd_open(dev);
   vos_ssr_unprotect(__func__);

   return ret;
}

int hdd_mon_open (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

   if(pAdapter == NULL) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD adapter context is Null", __func__);
      return -EINVAL;
   }

   netif_start_queue(dev);

   return 0;
}
/**---------------------------------------------------------------------------

  \brief __hdd_stop() - HDD stop function

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/

int __hdd_stop (struct net_device *dev)
{
   int ret = 0;
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t enter_standby = TRUE;
   
   ENTER();
   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: pAdapter is Null", __func__);
      return -ENODEV;
   }
   MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_OPEN_REQUEST,
                    pAdapter->sessionId, pAdapter->device_mode));

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (ret)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is not valid!", __func__);
      return ret;
   }

   /* Nothing to be done if the interface is not opened */
   if (VOS_FALSE == test_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: NETDEV Interface is not OPENED", __func__);
      return -ENODEV;
   }

   /* Make sure the interface is marked as closed */
   clear_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Disabling OS Tx queues", __func__);

   /* Disable TX on the interface, after this hard_start_xmit() will not
    * be called on that interface
    */
   netif_tx_disable(pAdapter->dev);

   /* Mark the interface status as "down" for outside world */
   netif_carrier_off(pAdapter->dev);

   /* The interface is marked as down for outside world (aka kernel)
    * But the driver is pretty much alive inside. The driver needs to
    * tear down the existing connection on the netdev (session)
    * cleanup the data pipes and wait until the control plane is stabilized
    * for this interface. The call also needs to wait until the above
    * mentioned actions are completed before returning to the caller.
    * Notice that the hdd_stop_adapter is requested not to close the session
    * That is intentional to be able to scan if it is a STA/P2P interface
    */
   hdd_stop_adapter(pHddCtx, pAdapter, VOS_FALSE);

   /* DeInit the adapter. This ensures datapath cleanup as well */
   hdd_deinit_adapter(pHddCtx, pAdapter);
   /* SoftAP ifaces should never go in power save mode
      making sure same here. */
   if ( (WLAN_HDD_SOFTAP == pAdapter->device_mode )
                 || (WLAN_HDD_MONITOR == pAdapter->device_mode )
                 || (WLAN_HDD_P2P_GO == pAdapter->device_mode )
      )
   {
      /* SoftAP mode, so return from here */
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "%s: In SAP MODE", __func__);
      EXIT();
      return 0;
   }
   /* Find if any iface is up. If any iface is up then can't put device to
    * sleep/power save mode
    */
   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( (NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status) )
   {
      if (test_bit(DEVICE_IFACE_OPENED, &pAdapterNode->pAdapter->event_flags))
      {
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: Still other ifaces are up cannot "
                "put device to sleep", __func__);
         enter_standby = FALSE;
         break;
      }
      else
      {
         status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
   }

   if (TRUE == enter_standby)
   {
       hddLog(VOS_TRACE_LEVEL_INFO, "%s: All Interfaces are Down " 
                 "entering standby", __func__);
       if (VOS_STATUS_SUCCESS != wlan_hdd_enter_lowpower(pHddCtx))
       {
           /*log and return success*/
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to put "
                   "wlan in power save", __func__);
       }
   }
   
   EXIT();
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_stop() - wrapper_function for __hdd_stop to protect it from SSR

  This is called in response to ifconfig down

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure
-----------------------------------------------------------------------------*/
int hdd_stop (struct net_device *dev)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __hdd_stop(dev);
    vos_ssr_unprotect(__func__);

    return ret;
}

/**---------------------------------------------------------------------------

  \brief __hdd_uninit() - HDD uninit function

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void __hdd_uninit (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

   ENTER();

   do
   {
      if (NULL == pAdapter)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: NULL pAdapter", __func__);
         break;
      }

      if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Invalid magic", __func__);
         break;
      }

      if (NULL == pAdapter->pHddCtx)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: NULL pHddCtx", __func__);
         break;
      }

      if (dev != pAdapter->dev)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Invalid device reference", __func__);
         /* we haven't validated all cases so let this go for now */
      }

      hdd_deinit_adapter(pAdapter->pHddCtx, pAdapter);

      /* after uninit our adapter structure will no longer be valid */
      pAdapter->dev = NULL;
      pAdapter->magic = 0;
   } while (0);

   EXIT();
}

/**---------------------------------------------------------------------------

  \brief hdd_uninit() - Wrapper function to protect __hdd_uninit from SSR

  This is called during the netdev unregister to uninitialize all data
associated with the device

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_uninit (struct net_device *dev)
{
   vos_ssr_protect(__func__);
   __hdd_uninit(dev);
   vos_ssr_unprotect(__func__);
}

/**---------------------------------------------------------------------------

  \brief hdd_release_firmware() -

   This function calls the release firmware API to free the firmware buffer.

  \param  - pFileName Pointer to the File Name.
                  pCtx - Pointer to the adapter .


  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_release_firmware(char *pFileName,v_VOID_t *pCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;
   ENTER();


   if (!strcmp(WLAN_FW_FILE, pFileName)) {
   
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"%s: Loaded firmware file is %s",__func__,pFileName);

       if(pHddCtx->fw) {
          release_firmware(pHddCtx->fw);
          pHddCtx->fw = NULL;
       }
       else
          status = VOS_STATUS_E_FAILURE;
   }
   else if (!strcmp(WLAN_NV_FILE,pFileName)) {
       if(pHddCtx->nv) {
          release_firmware(pHddCtx->nv);
          pHddCtx->nv = NULL;
       }
       else
          status = VOS_STATUS_E_FAILURE;

   }

   EXIT();
   return status;
}

/**---------------------------------------------------------------------------

  \brief hdd_request_firmware() -

   This function reads the firmware file using the request firmware
   API and returns the the firmware data and the firmware file size.

  \param  - pfileName - Pointer to the file name.
              - pCtx - Pointer to the adapter .
              - ppfw_data - Pointer to the pointer of the firmware data.
              - pSize - Pointer to the file size.

  \return - VOS_STATUS_SUCCESS for success, VOS_STATUS_E_FAILURE for failure

  --------------------------------------------------------------------------*/


VOS_STATUS hdd_request_firmware(char *pfileName,v_VOID_t *pCtx,v_VOID_t **ppfw_data, v_SIZE_t *pSize)
{
   int status;
   VOS_STATUS retval = VOS_STATUS_SUCCESS;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;
   ENTER();

   if( (!strcmp(WLAN_FW_FILE, pfileName)) ) {

       status = request_firmware(&pHddCtx->fw, pfileName, pHddCtx->parent_dev);

       if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
           hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Firmware %s download failed",
                  __func__, pfileName);
           retval = VOS_STATUS_E_FAILURE;
       }

       else {
         *ppfw_data = (v_VOID_t *)pHddCtx->fw->data;
         *pSize = pHddCtx->fw->size;
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: Firmware size = %d",
                 __func__, *pSize);
       }
   }
   else if(!strcmp(WLAN_NV_FILE, pfileName)) {

       status = request_firmware(&pHddCtx->nv, pfileName, pHddCtx->parent_dev);

       if(status || !pHddCtx->nv || !pHddCtx->nv->data) {
           hddLog(VOS_TRACE_LEVEL_FATAL, "%s: nv %s download failed",
                  __func__, pfileName);
           retval = VOS_STATUS_E_FAILURE;
       }

       else {
         *ppfw_data = (v_VOID_t *)pHddCtx->nv->data;
         *pSize = pHddCtx->nv->size;
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: nv file size = %d",
                 __func__, *pSize);
       }
   }

   EXIT();
   return retval;
}
/**---------------------------------------------------------------------------
     \brief hdd_full_pwr_cbk() - HDD full power callbackfunction

      This is the function invoked by SME to inform the result of a full power
      request issued by HDD

     \param  - callbackcontext - Pointer to cookie
               status - result of request

     \return - None

--------------------------------------------------------------------------*/
void hdd_full_pwr_cbk(void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"HDD full Power callback status = %d", status);
   if(&pHddCtx->full_pwr_comp_var)
   {
      complete(&pHddCtx->full_pwr_comp_var);
   }
}

/**---------------------------------------------------------------------------

    \brief hdd_req_bmps_cbk() - HDD Request BMPS callback function

     This is the function invoked by SME to inform the result of BMPS
     request issued by HDD

    \param  - callbackcontext - Pointer to cookie
               status - result of request

    \return - None

--------------------------------------------------------------------------*/
void hdd_req_bmps_cbk(void *callbackContext, eHalStatus status)
{

   struct completion *completion_var = (struct completion*) callbackContext;

   hddLog(VOS_TRACE_LEVEL_ERROR, "HDD BMPS request Callback, status = %d", status);
   if(completion_var != NULL)
   {
      complete(completion_var);
   }
}

/**---------------------------------------------------------------------------

  \brief hdd_get_cfg_file_size() -

   This function reads the configuration file using the request firmware
   API and returns the configuration file size.

  \param  - pCtx - Pointer to the adapter .
              - pFileName - Pointer to the file name.
              - pBufSize - Pointer to the buffer size.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_get_cfg_file_size(v_VOID_t *pCtx, char *pFileName, v_SIZE_t *pBufSize)
{
   int status;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;

   ENTER();

   status = request_firmware(&pHddCtx->fw, pFileName, pHddCtx->parent_dev);

   if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: CFG download failed",__func__);
      status = VOS_STATUS_E_FAILURE;
   }
   else {
      *pBufSize = pHddCtx->fw->size;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: CFG size = %d", __func__, *pBufSize);
      release_firmware(pHddCtx->fw);
      pHddCtx->fw = NULL;
   }

   EXIT();
   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_read_cfg_file() -

   This function reads the configuration file using the request firmware
   API and returns the cfg data and the buffer size of the configuration file.

  \param  - pCtx - Pointer to the adapter .
              - pFileName - Pointer to the file name.
              - pBuffer - Pointer to the data buffer.
              - pBufSize - Pointer to the buffer size.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_read_cfg_file(v_VOID_t *pCtx, char *pFileName,
    v_VOID_t *pBuffer, v_SIZE_t *pBufSize)
{
   int status;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;

   ENTER();

   status = request_firmware(&pHddCtx->fw, pFileName, pHddCtx->parent_dev);

   if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: CFG download failed",__func__);
      return VOS_STATUS_E_FAILURE;
   }
   else {
      if(*pBufSize != pHddCtx->fw->size) {
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Caller sets invalid CFG "
             "file size", __func__);
         release_firmware(pHddCtx->fw);
         pHddCtx->fw = NULL;
         return VOS_STATUS_E_FAILURE;
      }
        else {
         if(pBuffer) {
            vos_mem_copy(pBuffer,pHddCtx->fw->data,*pBufSize);
         }
         release_firmware(pHddCtx->fw);
         pHddCtx->fw = NULL;
        }
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief __hdd_set_mac_address() -

   This function sets the user specified mac address using
   the command ifconfig wlanX hw ether <mac adress>.

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int __hdd_set_mac_address(struct net_device *dev, void *addr)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   struct sockaddr *psta_mac_addr = addr;
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;

   ENTER();

   memcpy(&pAdapter->macAddressCurrent, psta_mac_addr->sa_data, ETH_ALEN);
   memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);

   EXIT();
   return halStatus;
}

/**---------------------------------------------------------------------------

  \brief hdd_set_mac_address() -

   Wrapper function to protect __hdd_set_mac_address() function from ssr

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int hdd_set_mac_address(struct net_device *dev, void *addr)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __hdd_set_mac_address(dev, addr);
   vos_ssr_unprotect(__func__);

   return ret;
}

tANI_U8* wlan_hdd_get_intf_addr(hdd_context_t* pHddCtx)
{
   int i;
   for ( i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
   {
      if( 0 == ((pHddCtx->cfg_ini->intfAddrMask) & (1 << i)) )
         break;
   }

   if( VOS_MAX_CONCURRENCY_PERSONA == i)
      return NULL;

   pHddCtx->cfg_ini->intfAddrMask |= (1 << i);
   return &pHddCtx->cfg_ini->intfMacAddr[i].bytes[0];
}

void wlan_hdd_release_intf_addr(hdd_context_t* pHddCtx, tANI_U8* releaseAddr)
{
   int i;
   for ( i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
   {
      if ( !memcmp(releaseAddr, &pHddCtx->cfg_ini->intfMacAddr[i].bytes[0], 6) )
      {
         pHddCtx->cfg_ini->intfAddrMask &= ~(1 << i);
         break;
      } 
   }
   return;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
  static struct net_device_ops wlan_drv_ops = {
      .ndo_open = hdd_open,
      .ndo_stop = hdd_stop,
      .ndo_uninit = hdd_uninit,
      .ndo_start_xmit = hdd_hard_start_xmit,
      .ndo_tx_timeout = hdd_tx_timeout,
      .ndo_get_stats = hdd_stats,
      .ndo_do_ioctl = hdd_ioctl,
      .ndo_set_mac_address = hdd_set_mac_address,
      .ndo_select_queue    = hdd_select_queue,
#ifdef WLAN_FEATURE_PACKET_FILTERING
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,1,0))
      .ndo_set_rx_mode = hdd_set_multicast_list,
#else
      .ndo_set_multicast_list = hdd_set_multicast_list,
#endif //LINUX_VERSION_CODE
#endif
 };
 static struct net_device_ops wlan_mon_drv_ops = {
      .ndo_open = hdd_mon_open,
      .ndo_stop = hdd_stop,
      .ndo_uninit = hdd_uninit,
      .ndo_start_xmit = hdd_mon_hard_start_xmit,  
      .ndo_tx_timeout = hdd_tx_timeout,
      .ndo_get_stats = hdd_stats,
      .ndo_do_ioctl = hdd_ioctl,
      .ndo_set_mac_address = hdd_set_mac_address,
 };

#endif

void hdd_set_station_ops( struct net_device *pWlanDev )
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
      pWlanDev->netdev_ops = &wlan_drv_ops;
#else
      pWlanDev->open = hdd_open;
      pWlanDev->stop = hdd_stop;
      pWlanDev->uninit = hdd_uninit;
      pWlanDev->hard_start_xmit = NULL;
      pWlanDev->tx_timeout = hdd_tx_timeout;
      pWlanDev->get_stats = hdd_stats;
      pWlanDev->do_ioctl = hdd_ioctl;
      pWlanDev->set_mac_address = hdd_set_mac_address;
#endif
}

static hdd_adapter_t* hdd_alloc_station_adapter( hdd_context_t *pHddCtx, tSirMacAddr macAddr, const char* name )
{
   struct net_device *pWlanDev = NULL;
   hdd_adapter_t *pAdapter = NULL;
   /*
    * cfg80211 initialization and registration....
    */ 
   pWlanDev = alloc_netdev_mq(sizeof( hdd_adapter_t ), name, ether_setup, NUM_TX_QUEUES);
   
   if(pWlanDev != NULL)
   {

      //Save the pointer to the net_device in the HDD adapter
      pAdapter = (hdd_adapter_t*) netdev_priv( pWlanDev );

      vos_mem_zero( pAdapter, sizeof( hdd_adapter_t ) );

      pAdapter->dev = pWlanDev;
      pAdapter->pHddCtx = pHddCtx; 
      pAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;
      spin_lock_init(&pAdapter->lock_for_active_session);

      init_completion(&pAdapter->session_open_comp_var);
      init_completion(&pAdapter->session_close_comp_var);
      init_completion(&pAdapter->disconnect_comp_var);
      init_completion(&pAdapter->linkup_event_var);
      init_completion(&pAdapter->cancel_rem_on_chan_var);
      init_completion(&pAdapter->rem_on_chan_ready_event);
      init_completion(&pAdapter->pno_comp_var);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
      init_completion(&pAdapter->offchannel_tx_event);
#endif
      init_completion(&pAdapter->tx_action_cnf_event);
#ifdef FEATURE_WLAN_TDLS
      init_completion(&pAdapter->tdls_add_station_comp);
      init_completion(&pAdapter->tdls_del_station_comp);
      init_completion(&pAdapter->tdls_mgmt_comp);
      init_completion(&pAdapter->tdls_link_establish_req_comp);
#endif
      init_completion(&pHddCtx->mc_sus_event_var);
      init_completion(&pHddCtx->tx_sus_event_var);
      init_completion(&pHddCtx->rx_sus_event_var);
      init_completion(&pAdapter->ula_complete);
      init_completion(&pAdapter->change_country_code);

#ifdef FEATURE_WLAN_BATCH_SCAN
      init_completion(&pAdapter->hdd_set_batch_scan_req_var);
      init_completion(&pAdapter->hdd_get_batch_scan_req_var);
      pAdapter->pBatchScanRsp = NULL;
      pAdapter->numScanList = 0;
      pAdapter->batchScanState = eHDD_BATCH_SCAN_STATE_STOPPED;
      pAdapter->prev_batch_id = 0;
      mutex_init(&pAdapter->hdd_batch_scan_lock);
#endif

      pAdapter->isLinkUpSvcNeeded = FALSE; 
      pAdapter->higherDtimTransition = eANI_BOOLEAN_TRUE;
      //Init the net_device structure
      strlcpy(pWlanDev->name, name, IFNAMSIZ);

      vos_mem_copy(pWlanDev->dev_addr, (void *)macAddr, sizeof(tSirMacAddr));
      vos_mem_copy( pAdapter->macAddressCurrent.bytes, macAddr, sizeof(tSirMacAddr));
      pWlanDev->watchdog_timeo = HDD_TX_TIMEOUT;
      pWlanDev->hard_header_len += LIBRA_HW_NEEDED_HEADROOM;

      hdd_set_station_ops( pAdapter->dev );

      pWlanDev->destructor = free_netdev;
      pWlanDev->ieee80211_ptr = &pAdapter->wdev ;
      pAdapter->wdev.wiphy = pHddCtx->wiphy;  
      pAdapter->wdev.netdev =  pWlanDev;
      /* set pWlanDev's parent to underlying device */
      SET_NETDEV_DEV(pWlanDev, pHddCtx->parent_dev);

      hdd_wmm_init( pAdapter );
   }

   return pAdapter;
}

VOS_STATUS hdd_register_interface( hdd_adapter_t *pAdapter, tANI_U8 rtnl_lock_held )
{
   struct net_device *pWlanDev = pAdapter->dev;
   //hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   //hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
   //eHalStatus halStatus = eHAL_STATUS_SUCCESS;

   if( rtnl_lock_held )
   {
     if (strnchr(pWlanDev->name, strlen(pWlanDev->name), '%')) {
         if( dev_alloc_name(pWlanDev, pWlanDev->name) < 0 )
         {
            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:dev_alloc_name",__func__);
            return VOS_STATUS_E_FAILURE;            
         }
      }
      if (register_netdevice(pWlanDev))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:register_netdev",__func__);
         return VOS_STATUS_E_FAILURE;         
      }
   }
   else
   {
      if(register_netdev(pWlanDev))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed:register_netdev",__func__);
         return VOS_STATUS_E_FAILURE;         
      }
   }
   set_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);

   return VOS_STATUS_SUCCESS;
}

static eHalStatus hdd_smeCloseSessionCallback(void *pContext)
{
   hdd_adapter_t *pAdapter = pContext;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: NULL pAdapter", __func__);
      return eHAL_STATUS_INVALID_PARAMETER;
   }

   if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Invalid magic", __func__);
      return eHAL_STATUS_NOT_INITIALIZED;
   }

   clear_bit(SME_SESSION_OPENED, &pAdapter->event_flags);

#ifndef WLAN_OPEN_SOURCE
   /* need to make sure all of our scheduled work has completed.
    * This callback is called from MC thread context, so it is safe to
    * to call below flush workqueue API from here.
    *
    * Even though this is called from MC thread context, if there is a faulty
    * work item in the system, that can hang this call forever.  So flushing
    * this global work queue is not safe; and now we make sure that
    * individual work queues are stopped correctly. But the cancel work queue
    * is a GPL only API, so the proprietary  version of the driver would still
    * rely on the global work queue flush.
    */
   flush_scheduled_work();
#endif

   /* We can be blocked while waiting for scheduled work to be
    * flushed, and the adapter structure can potentially be freed, in
    * which case the magic will have been reset.  So make sure the
    * magic is still good, and hence the adapter structure is still
    * valid, before signaling completion */
   if (WLAN_HDD_ADAPTER_MAGIC == pAdapter->magic)
   {
      complete(&pAdapter->session_close_comp_var);
   }

   return eHAL_STATUS_SUCCESS;
}

VOS_STATUS hdd_init_station_mode( hdd_adapter_t *pAdapter )
{
   struct net_device *pWlanDev = pAdapter->dev;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   long rc = 0;

   INIT_COMPLETION(pAdapter->session_open_comp_var);
   sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);
   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pAdapter,
         (tANI_U8 *)&pAdapter->macAddressCurrent, &pAdapter->sessionId);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "sme_OpenSession() failed with status code %08d [x%08x]",
                                                 halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }
   
   //Block on a completion variable. Can't wait forever though.
   rc = wait_for_completion_timeout(
                        &pAdapter->session_open_comp_var,
                        msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
   if (rc <= 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "Session is not opened within timeout period code %ld", rc );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }

   // Register wireless extensions
   if( eHAL_STATUS_SUCCESS !=  (halStatus = hdd_register_wext(pWlanDev)))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
              "hdd_register_wext() failed with status code %08d [x%08x]",
                                                   halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_register_wext;
   }
   //Safe to register the hard_start_xmit function again
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
   wlan_drv_ops.ndo_start_xmit = hdd_hard_start_xmit;
#else
   pWlanDev->hard_start_xmit = hdd_hard_start_xmit;
#endif

   //Set the Connection State to Not Connected
   hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: Set HDD connState to eConnectionState_NotConnected",
                   __func__);
   pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;

   //Set the default operation channel
   pHddStaCtx->conn_info.operationChannel = pHddCtx->cfg_ini->OperatingChannel;

   /* Make the default Auth Type as OPEN*/
   pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;

   if( VOS_STATUS_SUCCESS != ( status = hdd_init_tx_rx( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_init_tx_rx() failed with status code %08d [x%08x]",
                            status, status );
      goto error_init_txrx;
   }

   set_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);

   if( VOS_STATUS_SUCCESS != ( status = hdd_wmm_adapter_init( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_wmm_adapter_init() failed with status code %08d [x%08x]",
                            status, status );
      goto error_wmm_init;
   }

   set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

#ifdef FEATURE_WLAN_TDLS
   if(0 != wlan_hdd_sta_tdls_init(pAdapter))
   {
       status = VOS_STATUS_E_FAILURE;
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wlan_hdd_sta_tdls_init failed",__func__);
       goto error_tdls_init;
   }
   set_bit(TDLS_INIT_DONE, &pAdapter->event_flags);
#endif

   return VOS_STATUS_SUCCESS;

#ifdef FEATURE_WLAN_TDLS
error_tdls_init:
   clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
   hdd_wmm_adapter_close(pAdapter);
#endif
error_wmm_init:
   clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
   hdd_deinit_tx_rx(pAdapter);
error_init_txrx:
   hdd_UnregisterWext(pWlanDev);
error_register_wext:
   if (test_bit(SME_SESSION_OPENED, &pAdapter->event_flags))
   {
      INIT_COMPLETION(pAdapter->session_close_comp_var);
      if (eHAL_STATUS_SUCCESS == sme_CloseSession(pHddCtx->hHal,
                                    pAdapter->sessionId,
                                    hdd_smeCloseSessionCallback, pAdapter))
      {
         unsigned long rc;

         //Block on a completion variable. Can't wait forever though.
         rc = wait_for_completion_timeout(
                          &pAdapter->session_close_comp_var,
                          msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
         if (rc <= 0)
             hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("Session is not opened within timeout period code %ld"), rc);
      }
}
error_sme_open:
   return status;
}

void hdd_cleanup_actionframe( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   hdd_cfg80211_state_t *cfgState;

   cfgState = WLAN_HDD_GET_CFG_STATE_PTR( pAdapter );

   if( NULL != cfgState->buf )
   {
      long rc;
      INIT_COMPLETION(pAdapter->tx_action_cnf_event);
      rc = wait_for_completion_interruptible_timeout(
                     &pAdapter->tx_action_cnf_event,
                     msecs_to_jiffies(ACTION_FRAME_TX_TIMEOUT));
      if (rc <= 0)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s ERROR: HDD Wait for Action Confirmation Failed!! %ld"
               , __func__, rc);
      }
   }
   return;
}

void hdd_deinit_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   ENTER();
   switch ( pAdapter->device_mode )
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      {
         if(test_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags))
         {
            hdd_deinit_tx_rx( pAdapter );
            clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         }

         if(test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
         {
            hdd_wmm_adapter_close( pAdapter );
            clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
         }

         hdd_cleanup_actionframe(pHddCtx, pAdapter);
#ifdef FEATURE_WLAN_TDLS
         if(test_bit(TDLS_INIT_DONE, &pAdapter->event_flags))
         {
            wlan_hdd_tdls_exit(pAdapter);
            clear_bit(TDLS_INIT_DONE, &pAdapter->event_flags);
         }
#endif

         break;
      }

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
      {

         if (test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
         {
            hdd_wmm_adapter_close( pAdapter );
            clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
         }

         hdd_cleanup_actionframe(pHddCtx, pAdapter);

         hdd_unregister_hostapd(pAdapter);
         hdd_set_conparam( 0 );
         wlan_hdd_set_monitor_tx_adapter( WLAN_HDD_GET_CTX(pAdapter), NULL );
         break;
      }

      case WLAN_HDD_MONITOR:
      {
          hdd_adapter_t* pAdapterforTx = pAdapter->sessionCtx.monitor.pAdapterForTx;
         if(test_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags))
         {
            hdd_deinit_tx_rx( pAdapter );
            clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         }
         if(NULL != pAdapterforTx)
         {
            hdd_cleanup_actionframe(pHddCtx, pAdapterforTx);
         }
         break;
      }


      default:
      break;
   }

   EXIT();
}

void hdd_cleanup_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter, tANI_U8 rtnl_held )
{
   struct net_device *pWlanDev = NULL;

   ENTER();
   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: HDD adapter is Null", __func__);
      return;
   }

   pWlanDev = pAdapter->dev;

#ifdef FEATURE_WLAN_BATCH_SCAN
   if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
     || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
     || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
     || (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE)
     )
   {
      if (pAdapter)
      {
          if (eHDD_BATCH_SCAN_STATE_STARTED == pAdapter->batchScanState)
          {
              hdd_deinit_batch_scan(pAdapter);
          }
      }
   }
#endif

   if(test_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags)) {
      if( rtnl_held )
      {
         unregister_netdevice(pWlanDev);
      }
      else
      {
         unregister_netdev(pWlanDev);
      }
      // note that the pAdapter is no longer valid at this point
      // since the memory has been reclaimed
   }

   EXIT();
}

void hdd_set_pwrparams(hdd_context_t *pHddCtx)
{
   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   /*loop through all adapters.*/
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )

       {  // we skip this registration for modes other than STA and P2P client modes.
           status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
           pAdapterNode = pNext;
           continue;
       }

       //Apply Dynamic DTIM For P2P
       //Only if ignoreDynamicDtimInP2pMode is not set in ini
      if ((pHddCtx->cfg_ini->enableDynamicDTIM ||
           pHddCtx->cfg_ini->enableModulatedDTIM) &&
          ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
          ((WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) &&
          !(pHddCtx->cfg_ini->ignoreDynamicDtimInP2pMode))) &&
          (eANI_BOOLEAN_TRUE == pAdapter->higherDtimTransition) &&
          (eConnectionState_Associated ==
          (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) &&
          (pHddCtx->cfg_ini->fIsBmpsEnabled))
      {
           tSirSetPowerParamsReq powerRequest = { 0 };

           powerRequest.uIgnoreDTIM = 1;
           powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

           if (pHddCtx->cfg_ini->enableModulatedDTIM)
           {
               powerRequest.uDTIMPeriod = pHddCtx->cfg_ini->enableModulatedDTIM;
               powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
           }
           else
           {
               powerRequest.uListenInterval = pHddCtx->cfg_ini->enableDynamicDTIM;
           }

           /* Update ignoreDTIM and ListedInterval in CFG to remain at the DTIM
            * specified during Enter/Exit BMPS when LCD off*/
            ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                       NULL, eANI_BOOLEAN_FALSE);
            ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval,
                       NULL, eANI_BOOLEAN_FALSE);

           /* switch to the DTIM specified in cfg.ini */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "Switch to DTIM %d", powerRequest.uListenInterval);
            sme_SetPowerParams( pHddCtx->hHal, &powerRequest, TRUE);
            break;

      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
    }
}

void hdd_reset_pwrparams(hdd_context_t *pHddCtx)
{
   /*Switch back to DTIM 1*/
   tSirSetPowerParamsReq powerRequest = { 0 };

   powerRequest.uIgnoreDTIM = pHddCtx->hdd_actual_ignore_DTIM_value;
   powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
   powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

   /* Update ignoreDTIM and ListedInterval in CFG with default values */
   ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                    NULL, eANI_BOOLEAN_FALSE);
   ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval,
                    NULL, eANI_BOOLEAN_FALSE);

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "Switch to DTIM%d",powerRequest.uListenInterval);
   sme_SetPowerParams( pHddCtx->hHal, &powerRequest, TRUE);

}

VOS_STATUS hdd_enable_bmps_imps(hdd_context_t *pHddCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
   {
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }

   if(pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled)
   {
      sme_StartAutoBmpsTimer(pHddCtx->hHal); 
   }

   if (pHddCtx->cfg_ini->fIsImpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }

   return status;
}

VOS_STATUS hdd_disable_bmps_imps(hdd_context_t *pHddCtx, tANI_U8 session_type)
{
   hdd_adapter_t *pAdapter = NULL;
   eHalStatus halStatus;
   VOS_STATUS status = VOS_STATUS_E_INVAL;
   v_BOOL_t disableBmps = FALSE;
   v_BOOL_t disableImps = FALSE;
   
   switch(session_type)
   {
       case WLAN_HDD_INFRA_STATION:
       case WLAN_HDD_SOFTAP:
       case WLAN_HDD_P2P_CLIENT:
       case WLAN_HDD_P2P_GO:
          //Exit BMPS -> Is Sta/P2P Client is already connected
          pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
          if((NULL != pAdapter)&&
              hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
          {
             disableBmps = TRUE;
          }

          pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_CLIENT);
          if((NULL != pAdapter)&&
              hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
          {
             disableBmps = TRUE;
          }

          //Exit both Bmps and Imps incase of Go/SAP Mode
          if((WLAN_HDD_SOFTAP == session_type) ||
              (WLAN_HDD_P2P_GO == session_type))
          {
             disableBmps = TRUE;
             disableImps = TRUE;
          }

          if(TRUE == disableImps)
          {
             if (pHddCtx->cfg_ini->fIsImpsEnabled)
             {
                sme_DisablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
             }
          }

          if(TRUE == disableBmps)
          {
             if(pHddCtx->cfg_ini->fIsBmpsEnabled)
             {
                 halStatus = sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

                 if(eHAL_STATUS_SUCCESS != halStatus)
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Disable Power Save", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }

              if(pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled)
              {
                 halStatus = sme_StopAutoBmpsTimer(pHddCtx->hHal);

                 if(eHAL_STATUS_SUCCESS != halStatus)
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Stop Auto Bmps Timer", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }
          }

          if((TRUE == disableBmps) ||
              (TRUE == disableImps))
          {
              /* Now, get the chip into Full Power now */
              INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
              halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_full_pwr_cbk,
                                   pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

              if(halStatus != eHAL_STATUS_SUCCESS)
              {
                 if(halStatus == eHAL_STATUS_PMC_PENDING)
                 {
                    long ret;
                    //Block on a completion variable. Can't wait forever though
                    ret = wait_for_completion_interruptible_timeout(
                                   &pHddCtx->full_pwr_comp_var,
                                    msecs_to_jiffies(1000));
                    if (ret <= 0)
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                               "%s: wait on full_pwr_comp_var failed %ld",
                               __func__, ret);
                    }
                 }
                 else
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Request for Full Power failed", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }

              status = VOS_STATUS_SUCCESS;
          }

          break;
   }
   return status;
}

hdd_adapter_t* hdd_open_adapter( hdd_context_t *pHddCtx, tANI_U8 session_type,
                                 const char *iface_name, tSirMacAddr macAddr,
                                 tANI_U8 rtnl_held )
{
   int ret = 0;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pHddAdapterNode = NULL;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   VOS_STATUS exitbmpsStatus;

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s iface =%s type = %d",__func__,iface_name,session_type);

   if(macAddr == NULL)
   {
         /* Not received valid macAddr */
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s:Unable to add virtual intf: Not able to get"
                             "valid mac address",__func__);
         return NULL;
   }

   //Disable BMPS incase of Concurrency
   exitbmpsStatus = hdd_disable_bmps_imps(pHddCtx, session_type);

   if(VOS_STATUS_E_FAILURE == exitbmpsStatus)
   {
      //Fail to Exit BMPS
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Exit BMPS", __func__);
      VOS_ASSERT(0);
      return NULL;
   }

   switch(session_type)
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
              FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

#ifdef FEATURE_WLAN_TDLS
         /* A Mutex Lock is introduced while changing/initializing the mode to
          * protect the concurrent access for the Adapters by TDLS module.
          */
          mutex_lock(&pHddCtx->tdls_lock);
#endif

         pAdapter->wdev.iftype = (session_type == WLAN_HDD_P2P_CLIENT) ?
                                  NL80211_IFTYPE_P2P_CLIENT:
                                  NL80211_IFTYPE_STATION;

         pAdapter->device_mode = session_type;
#ifdef FEATURE_WLAN_TDLS
         mutex_unlock(&pHddCtx->tdls_lock);
#endif

         status = hdd_init_station_mode( pAdapter );
         if( VOS_STATUS_SUCCESS != status )
            goto err_free_netdev;

         status = hdd_register_interface( pAdapter, rtnl_held );
         if( VOS_STATUS_SUCCESS != status )
         {
            hdd_deinit_adapter(pHddCtx, pAdapter);
            goto err_free_netdev;
         }

         // Workqueue which gets scheduled in IPv4 notification callback.
         INIT_WORK(&pAdapter->ipv4NotifierWorkQueue, hdd_ipv4_notifier_work_queue);
         // Register IPv4 notifier to notify if any change in IP
         // So that we can reconfigure the offload parameters
         pAdapter->ipv4_notifier.notifier_call = wlan_hdd_ipv4_changed;
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

#ifdef WLAN_NS_OFFLOAD
         // Workqueue which gets scheduled in IPv6 notification callback.
         INIT_WORK(&pAdapter->ipv6NotifierWorkQueue, hdd_ipv6_notifier_work_queue);
         // Register IPv6 notifier to notify if any change in IP
         // So that we can reconfigure the offload parameters
         pAdapter->ipv6_notifier.notifier_call = wlan_hdd_ipv6_changed;
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
#endif
         //Stop the Interface TX queue.
         netif_tx_disable(pAdapter->dev);
         //netif_tx_disable(pWlanDev);
         netif_carrier_off(pAdapter->dev);

         break;
      }

      case WLAN_HDD_P2P_GO:
      case WLAN_HDD_SOFTAP:
      {
         pAdapter = hdd_wlan_create_ap_dev( pHddCtx, macAddr, (tANI_U8 *)iface_name );
         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
              FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

         pAdapter->wdev.iftype = (session_type == WLAN_HDD_SOFTAP) ?
                                  NL80211_IFTYPE_AP:
                                  NL80211_IFTYPE_P2P_GO;
         pAdapter->device_mode = session_type;

         status = hdd_init_ap_mode(pAdapter);
         if( VOS_STATUS_SUCCESS != status )
            goto err_free_netdev;

         status = hdd_register_hostapd( pAdapter, rtnl_held );
         if( VOS_STATUS_SUCCESS != status )
         {
            hdd_deinit_adapter(pHddCtx, pAdapter);
            goto err_free_netdev;
         }

         netif_tx_disable(pAdapter->dev);
         netif_carrier_off(pAdapter->dev);

         hdd_set_conparam( 1 );
         break;
      }
      case WLAN_HDD_MONITOR:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );
         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
              FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

         pAdapter->wdev.iftype = NL80211_IFTYPE_MONITOR; 
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
         pAdapter->dev->netdev_ops = &wlan_mon_drv_ops;
#else
         pAdapter->dev->open = hdd_mon_open;
         pAdapter->dev->hard_start_xmit = hdd_mon_hard_start_xmit;
#endif
         hdd_init_tx_rx( pAdapter );
         set_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         //Set adapter to be used for data tx. It will use either GO or softap.
         pAdapter->sessionCtx.monitor.pAdapterForTx = 
                           hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_SOFTAP);
         if (NULL == pAdapter->sessionCtx.monitor.pAdapterForTx)
         {
            pAdapter->sessionCtx.monitor.pAdapterForTx = 
                           hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_P2P_GO);
         }
         /* This workqueue will be used to transmit management packet over
          * monitor interface. */
         if (NULL == pAdapter->sessionCtx.monitor.pAdapterForTx) {
             hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:hdd_get_adapter",__func__);
             return NULL;
         }

         INIT_WORK(&pAdapter->sessionCtx.monitor.pAdapterForTx->monTxWorkQueue,
                   hdd_mon_tx_work_queue);
      }
         break;
      case WLAN_HDD_FTM:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
              FL("failed to allocate adapter for session %d"), session_type);
             return NULL;
         }

         /* Assign NL80211_IFTYPE_STATION as interface type to resolve Kernel Warning
          * message while loading driver in FTM mode. */
         pAdapter->wdev.iftype = NL80211_IFTYPE_STATION;
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );

         hdd_init_tx_rx( pAdapter );

         //Stop the Interface TX queue.
         netif_tx_disable(pAdapter->dev);
         netif_carrier_off(pAdapter->dev);
      }
         break;
      default:
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s Invalid session type %d",
           __func__, session_type);
         VOS_ASSERT(0);
         return NULL;
      }
   }

   if( VOS_STATUS_SUCCESS == status )
   {
      //Add it to the hdd's session list. 
      pHddAdapterNode = vos_mem_malloc( sizeof( hdd_adapter_list_node_t ) );
      if( NULL == pHddAdapterNode )
      {
         status = VOS_STATUS_E_NOMEM;
      }
      else
      {
         pHddAdapterNode->pAdapter = pAdapter;
         status = hdd_add_adapter_back ( pHddCtx, 
                                         pHddAdapterNode );
      }
   }

   if( VOS_STATUS_SUCCESS != status )
   {
      if( NULL != pAdapter )
      {
         hdd_cleanup_adapter( pHddCtx, pAdapter, rtnl_held );
         pAdapter = NULL;   
      }
      if( NULL != pHddAdapterNode )
      {
         vos_mem_free( pHddAdapterNode );
      }

      goto resume_bmps;
   }

   if(VOS_STATUS_SUCCESS == status)
   {
      wlan_hdd_set_concurrency_mode(pHddCtx, session_type);

      //Initialize the WoWL service
      if(!hdd_init_wowl(pAdapter))
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_init_wowl failed",__func__);
          goto err_free_netdev;
      }
   }
   return pAdapter;

err_free_netdev:
   free_netdev(pAdapter->dev);
   wlan_hdd_release_intf_addr( pHddCtx,
                               pAdapter->macAddressCurrent.bytes );

resume_bmps:
   //If bmps disabled enable it
   if(VOS_STATUS_SUCCESS == exitbmpsStatus)
   {
       if (pHddCtx->hdd_wlan_suspended)
       {
           hdd_set_pwrparams(pHddCtx);
       }
       hdd_enable_bmps_imps(pHddCtx);
   }
   return NULL;
}

VOS_STATUS hdd_close_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                              tANI_U8 rtnl_held )
{
   hdd_adapter_list_node_t *pAdapterNode, *pCurrent, *pNext;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pCurrent );
   if( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_WARN,"%s: adapter list empty %d",
          __func__, status);
      return status;
   }

   while ( pCurrent->pAdapter != pAdapter )
   {
      status = hdd_get_next_adapter ( pHddCtx, pCurrent, &pNext );
      if( VOS_STATUS_SUCCESS != status )
         break;

      pCurrent = pNext;
   }
   pAdapterNode = pCurrent;
   if( VOS_STATUS_SUCCESS == status )
   {
      wlan_hdd_clear_concurrency_mode(pHddCtx, pAdapter->device_mode);
      hdd_cleanup_adapter( pHddCtx, pAdapterNode->pAdapter, rtnl_held );

#ifdef FEATURE_WLAN_TDLS

      /* A Mutex Lock is introduced while changing/initializing the mode to
       * protect the concurrent access for the Adapters by TDLS module.
       */
       mutex_lock(&pHddCtx->tdls_lock);
#endif

      hdd_remove_adapter( pHddCtx, pAdapterNode );
      vos_mem_free( pAdapterNode );
      pAdapterNode = NULL;

#ifdef FEATURE_WLAN_TDLS
       mutex_unlock(&pHddCtx->tdls_lock);
#endif


      /* If there is a single session of STA/P2P client, re-enable BMPS */
      if ((!vos_concurrent_open_sessions_running()) &&
           ((pHddCtx->no_of_open_sessions[VOS_STA_MODE] >= 1) ||
           (pHddCtx->no_of_open_sessions[VOS_P2P_CLIENT_MODE] >= 1)))
      {
          if (pHddCtx->hdd_wlan_suspended)
          {
              hdd_set_pwrparams(pHddCtx);
          }
          hdd_enable_bmps_imps(pHddCtx);
      }

      return VOS_STATUS_SUCCESS;
   }

   return VOS_STATUS_E_FAILURE;
}

VOS_STATUS hdd_close_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pHddAdapterNode;
   VOS_STATUS status;

   ENTER();

   do
   {
      status = hdd_remove_front_adapter( pHddCtx, &pHddAdapterNode );
      if( pHddAdapterNode && VOS_STATUS_SUCCESS == status )
      {
         hdd_cleanup_adapter( pHddCtx, pHddAdapterNode->pAdapter, FALSE );
         vos_mem_free( pHddAdapterNode );
      }
   }while( NULL != pHddAdapterNode && VOS_STATUS_E_EMPTY != status );
   
   EXIT();

   return VOS_STATUS_SUCCESS;
}

void wlan_hdd_reset_prob_rspies(hdd_adapter_t* pHostapdAdapter)
{
    v_U8_t addIE[1] = {0};

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA1,(tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA1 to CCM");
    }

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA2, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA2 to CCM");
    }

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA3, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA3 to CCM");
    }
}

VOS_STATUS hdd_stop_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                             const v_BOOL_t bCloseSession )
{
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   hdd_scaninfo_t *pScanInfo = NULL;
   union iwreq_data wrqu;
   v_U8_t retry = 0;
   long ret;

   ENTER();
   pScanInfo =  &pHddCtx->scan_info;
   switch(pAdapter->device_mode)
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      {
         hdd_station_ctx_t *pstation = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
         if( hdd_connIsConnected(pstation) ||
             (pstation->conn_info.connState == eConnectionState_Connecting) )
         {
            if (pWextState->roamProfile.BSSType == eCSR_BSS_TYPE_START_IBSS)
                halStatus = sme_RoamDisconnect(pHddCtx->hHal,
                                             pAdapter->sessionId,
                                             eCSR_DISCONNECT_REASON_IBSS_LEAVE);
            else
                halStatus = sme_RoamDisconnect(pHddCtx->hHal,
                                            pAdapter->sessionId, 
                                            eCSR_DISCONNECT_REASON_UNSPECIFIED);
            //success implies disconnect command got queued up successfully
            if(halStatus == eHAL_STATUS_SUCCESS)
            {
               ret = wait_for_completion_interruptible_timeout(
                          &pAdapter->disconnect_comp_var,
                           msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
               if (ret <= 0)
               {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: wait on disconnect_comp_var failed %ld",
                           __func__, ret);
               }
            }
            else
            {
                hddLog(LOGE, "%s: failed to post disconnect event to SME",
                         __func__);
            }
            memset(&wrqu, '\0', sizeof(wrqu));
            wrqu.ap_addr.sa_family = ARPHRD_ETHER;
            memset(wrqu.ap_addr.sa_data,'\0',ETH_ALEN);
            wireless_send_event(pAdapter->dev, SIOCGIWAP, &wrqu, NULL);
         }
         else if(pstation->conn_info.connState ==
                    eConnectionState_Disconnecting)
         {
             ret = wait_for_completion_interruptible_timeout(
                   &pAdapter->disconnect_comp_var,
                   msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
             if (ret <= 0)
             {
                 hddLog(VOS_TRACE_LEVEL_ERROR,
                 FL("wait on disconnect_comp_var failed %ld"), ret);
             }
         }
         else if(pScanInfo != NULL && pHddCtx->scan_info.mScanPending)
         {
            hdd_abort_mac_scan(pHddCtx, pScanInfo->sessionId,
                               eCSR_SCAN_ABORT_DEFAULT);
         }
       if (pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
       {
          while (pAdapter->is_roc_inprogress)
          {
              VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s: ROC in progress for session %d!!!",
                         __func__, pAdapter->sessionId);
              // waiting for ROC to expire
               msleep(500);
              /* In GO present case , if retry exceeds 3,
                 it means something went wrong. */
               if ( retry++ > MAX_WAIT_FOR_ROC_COMPLETION )
               {
                  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                            "%s: ROC completion is not received.!!!", __func__);
                  sme_CancelRemainOnChannel(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                            pAdapter->sessionId);
                  wait_for_completion_interruptible_timeout(
                                       &pAdapter->cancel_rem_on_chan_var,
                                       msecs_to_jiffies(WAIT_CANCEL_REM_CHAN));
                  break;
               }
            }
       }
#ifdef WLAN_NS_OFFLOAD
#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->ipv6NotifierWorkQueue);
#endif
         if (pAdapter->ipv6_notifier_registered)
         {
            hddLog(LOG1, FL("Unregistered IPv6 notifier"));
            unregister_inet6addr_notifier(&pAdapter->ipv6_notifier);
            pAdapter->ipv6_notifier_registered = false;
         }
#endif
         if (pAdapter->ipv4_notifier_registered)
         {
            hddLog(LOG1, FL("Unregistered IPv4 notifier"));
            unregister_inetaddr_notifier(&pAdapter->ipv4_notifier);
            pAdapter->ipv4_notifier_registered = false;
         }
#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->ipv4NotifierWorkQueue);
#endif
         /* It is possible that the caller of this function does not
          * wish to close the session
          */
         if (VOS_TRUE == bCloseSession &&
              test_bit(SME_SESSION_OPENED, &pAdapter->event_flags))
         {
            INIT_COMPLETION(pAdapter->session_close_comp_var);
            if (eHAL_STATUS_SUCCESS ==
                  sme_CloseSession(pHddCtx->hHal, pAdapter->sessionId,
                     hdd_smeCloseSessionCallback, pAdapter))
            {
               unsigned long ret;

               //Block on a completion variable. Can't wait forever though.
               ret = wait_for_completion_timeout(
                     &pAdapter->session_close_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
               if ( 0 >= ret)
               {
                  hddLog(LOGE, "%s: failure waiting for session_close_comp_var %ld",
                        __func__, ret);
               }
            }
         }
      }
         break;

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
         //Any softap specific cleanup here...
         if (pAdapter->device_mode == WLAN_HDD_P2P_GO) {
            while (pAdapter->is_roc_inprogress) {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s: ROC in progress for session %d!!!",
                         __func__, pAdapter->sessionId);
               msleep(500);
               if ( retry++ > MAX_WAIT_FOR_ROC_COMPLETION ) {
                  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                            "%s: ROC completion is not received.!!!", __func__);
                  WLANSAP_CancelRemainOnChannel(
                                     (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);
                  wait_for_completion_interruptible_timeout(
                                       &pAdapter->cancel_rem_on_chan_var,
                                       msecs_to_jiffies(WAIT_CANCEL_REM_CHAN));
                  break;
               }
            }
         }
         mutex_lock(&pHddCtx->sap_lock);
         if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) 
         {
            VOS_STATUS status;
            hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

            //Stop Bss.
            status = WLANSAP_StopBss(pHddCtx->pvosContext);
            if (VOS_IS_STATUS_SUCCESS(status))
            {
               hdd_hostapd_state_t *pHostapdState = 
                  WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

               status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
   
               if (!VOS_IS_STATUS_SUCCESS(status))
               {
                  hddLog(LOGE, "%s: failure waiting for WLANSAP_StopBss %d",
                         __func__, status);
               }
            }
            else
            {
               hddLog(LOGE, "%s: failure in WLANSAP_StopBss", __func__);
            }
            clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);
            wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);

            if (eHAL_STATUS_FAILURE ==
                ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG,
                             0, NULL, eANI_BOOLEAN_FALSE))
            {
               hddLog(LOGE,
                      "%s: Failed to set WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG",
                      __func__);
            }

            if ( eHAL_STATUS_FAILURE == ccmCfgSetInt((WLAN_HDD_GET_CTX(pAdapter))->hHal,
                     WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 0, NULL,
                     eANI_BOOLEAN_FALSE) )
            {
               hddLog(LOGE,
                     "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_FLAG to CCM");
            }

            // Reset WNI_CFG_PROBE_RSP Flags
            wlan_hdd_reset_prob_rspies(pAdapter);
            kfree(pAdapter->sessionCtx.ap.beacon);
            pAdapter->sessionCtx.ap.beacon = NULL;
         }
         mutex_unlock(&pHddCtx->sap_lock);
         break;

      case WLAN_HDD_MONITOR:
#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->sessionCtx.monitor.pAdapterForTx->monTxWorkQueue);
#endif
         break;

      default:
         break;
   }

   EXIT();
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_stop_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      hdd_stop_adapter( pHddCtx, pAdapter, VOS_TRUE );

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}


#ifdef FEATURE_WLAN_BATCH_SCAN
/**---------------------------------------------------------------------------

  \brief hdd_deinit_batch_scan () - This function cleans up batch scan data
   structures

  \param  - pAdapter Pointer to HDD adapter

  \return - None

  --------------------------------------------------------------------------*/
void hdd_deinit_batch_scan(hdd_adapter_t *pAdapter)
{
    tHddBatchScanRsp *pNode;
    tHddBatchScanRsp *pPrev;

    if (NULL == pAdapter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Adapter context is Null", __func__);
        return;
    }

    pNode = pAdapter->pBatchScanRsp;
    while (pNode)
    {
        pPrev = pNode;
        pNode = pNode->pNext;
        vos_mem_free((v_VOID_t * )pPrev);
    }

    pAdapter->pBatchScanRsp = NULL;
    pAdapter->numScanList = 0;
    pAdapter->batchScanState = eHDD_BATCH_SCAN_STATE_STOPPED;
    pAdapter->prev_batch_id = 0;

    return;
}
#endif


VOS_STATUS hdd_reset_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t *pAdapter;

   ENTER();

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;
      netif_tx_disable(pAdapter->dev);
      netif_carrier_off(pAdapter->dev);

      pAdapter->sessionCtx.station.hdd_ReassocScenario = VOS_FALSE;

      hdd_deinit_tx_rx(pAdapter);

      wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);

      if (test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
      {
          hdd_wmm_adapter_close( pAdapter );
          clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
      }

      if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
      {
          clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);
      }

#ifdef FEATURE_WLAN_BATCH_SCAN
      if (eHDD_BATCH_SCAN_STATE_STARTED == pAdapter->batchScanState)
      {
          hdd_deinit_batch_scan(pAdapter);
      }
#endif

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_start_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;
   eConnectionState  connState;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      hdd_wmm_init( pAdapter );

      switch(pAdapter->device_mode)
      {
         case WLAN_HDD_INFRA_STATION:
         case WLAN_HDD_P2P_CLIENT:
         case WLAN_HDD_P2P_DEVICE:

            connState = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState;

            hdd_init_station_mode(pAdapter);
            /* Open the gates for HDD to receive Wext commands */
            pAdapter->isLinkUpSvcNeeded = FALSE; 
            pHddCtx->scan_info.mScanPending = FALSE;
            pHddCtx->scan_info.waitScanResult = FALSE;

            //Trigger the initial scan
            hdd_wlan_initial_scan(pAdapter);

            //Indicate disconnect event to supplicant if associated previously
            if (eConnectionState_Associated == connState ||
                eConnectionState_IbssConnected == connState )
            {
               union iwreq_data wrqu;
               memset(&wrqu, '\0', sizeof(wrqu));
               wrqu.ap_addr.sa_family = ARPHRD_ETHER;
               memset(wrqu.ap_addr.sa_data,'\0',ETH_ALEN);
               wireless_send_event(pAdapter->dev, SIOCGIWAP, &wrqu, NULL);
               pAdapter->sessionCtx.station.hdd_ReassocScenario = VOS_FALSE;

               /* indicate disconnected event to nl80211 */
               cfg80211_disconnected(pAdapter->dev, WLAN_REASON_UNSPECIFIED,
                                     NULL, 0, GFP_KERNEL); 
            }
            else if (eConnectionState_Connecting == connState)
            {
              /*
               * Indicate connect failure to supplicant if we were in the
               * process of connecting
               */
               cfg80211_connect_result(pAdapter->dev, NULL,
                                       NULL, 0, NULL, 0,
                                       WLAN_STATUS_ASSOC_DENIED_UNSPEC,
                                       GFP_KERNEL);
            }
            break;

         case WLAN_HDD_SOFTAP:
            /* softAP can handle SSR */
            break;

         case WLAN_HDD_P2P_GO:
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s [SSR] send stop ap to supplicant",
                                                       __func__);
            cfg80211_ap_stopped(pAdapter->dev, GFP_KERNEL);
            break;

         case WLAN_HDD_MONITOR:
            /* monitor interface start */
            break;
         default:
            break;
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_reconnect_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;
   v_U32_t roamId;
   long ret;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( (WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
             (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
      {
         hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
         hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

         hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: Set HDD connState to eConnectionState_NotConnected",
                   __func__);
         pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;
         init_completion(&pAdapter->disconnect_comp_var);
         sme_RoamDisconnect(pHddCtx->hHal, pAdapter->sessionId,
                             eCSR_DISCONNECT_REASON_UNSPECIFIED);

         ret = wait_for_completion_interruptible_timeout(
                                &pAdapter->disconnect_comp_var,
                                msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
         if (0 >= ret)
             hddLog(LOGE, "%s: failure waiting for disconnect_comp_var %ld",
                     __func__, ret);

         pWextState->roamProfile.csrPersona = pAdapter->device_mode; 
         pHddCtx->isAmpAllowed = VOS_FALSE;
         sme_RoamConnect(pHddCtx->hHal,
                         pAdapter->sessionId, &(pWextState->roamProfile),
                         &roamId); 
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

void hdd_dump_concurrency_info(hdd_context_t *pHddCtx)
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t *pAdapter;
   hdd_station_ctx_t *pHddStaCtx;
   hdd_ap_ctx_t *pHddApCtx;
   hdd_hostapd_state_t * pHostapdState;
   tCsrBssid staBssid = { 0 }, p2pBssid = { 0 }, apBssid = { 0 };
   v_U8_t staChannel = 0, p2pChannel = 0, apChannel = 0;
   const char *p2pMode = "DEV";
   const char *ccMode = "Standalone";
   int n;

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;
      switch (pAdapter->device_mode) {
      case WLAN_HDD_INFRA_STATION:
          pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
          if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
              staChannel = pHddStaCtx->conn_info.operationChannel;
              memcpy(staBssid, pHddStaCtx->conn_info.bssId, sizeof(staBssid));
          }
          break;
      case WLAN_HDD_P2P_CLIENT:
          pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
          if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
              p2pChannel = pHddStaCtx->conn_info.operationChannel;
              memcpy(p2pBssid, pHddStaCtx->conn_info.bssId, sizeof(p2pBssid));
              p2pMode = "CLI";
          }
          break;
      case WLAN_HDD_P2P_GO:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              p2pChannel = pHddApCtx->operatingChannel;
              memcpy(p2pBssid, pAdapter->macAddressCurrent.bytes, sizeof(p2pBssid));
          }
          p2pMode = "GO";
          break;
      case WLAN_HDD_SOFTAP:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              apChannel = pHddApCtx->operatingChannel;
              memcpy(apBssid, pAdapter->macAddressCurrent.bytes, sizeof(apBssid));
          }
          break;
      default:
          break;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }
   if (staChannel > 0 && (apChannel > 0 || p2pChannel > 0)) {
       ccMode = (p2pChannel==staChannel||apChannel==staChannel) ? "SCC" : "MCC";
   }
   n = pr_info("wlan(%d) " MAC_ADDRESS_STR " %s",
                staChannel, MAC_ADDR_ARRAY(staBssid), ccMode);
   if (p2pChannel > 0) {
       n +=  pr_info("p2p-%s(%d) " MAC_ADDRESS_STR,
                     p2pMode, p2pChannel, MAC_ADDR_ARRAY(p2pBssid));
   }
   if (apChannel > 0) {
       n += pr_info("AP(%d) " MAC_ADDRESS_STR,
                     apChannel, MAC_ADDR_ARRAY(apBssid));
   }

   if (p2pChannel > 0 && apChannel > 0) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "Error concurrent SAP %d and P2P %d which is not support", apChannel, p2pChannel);
   }
}

bool hdd_is_ssr_required( void)
{
    return (isSsrRequired == HDD_SSR_REQUIRED);
}

/* Once SSR is disabled then it cannot be set. */
void hdd_set_ssr_required( e_hdd_ssr_required value)
{
    if (HDD_SSR_DISABLED == isSsrRequired)
        return;

    isSsrRequired = value;
}

VOS_STATUS hdd_get_front_adapter( hdd_context_t *pHddCtx,
                                  hdd_adapter_list_node_t** ppAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_peek_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_get_next_adapter( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode,
                                 hdd_adapter_list_node_t** pNextAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status = hdd_list_peek_next ( &pHddCtx->hddAdapters,
                                  (hdd_list_node_t*) pAdapterNode,
                                  (hdd_list_node_t**)pNextAdapterNode );

    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_adapter( hdd_context_t *pHddCtx,
                               hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_node ( &pHddCtx->hddAdapters,
                                     &pAdapterNode->node );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_front_adapter( hdd_context_t *pHddCtx,
                                     hdd_adapter_list_node_t** ppAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_front( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_back( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_back ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_front( hdd_context_t *pHddCtx,
                                  hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

hdd_adapter_t * hdd_get_adapter_by_macaddr( hdd_context_t *pHddCtx,
                                            tSirMacAddr macAddr )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && vos_mem_compare( pAdapter->macAddressCurrent.bytes,
                                       macAddr, sizeof(tSirMacAddr) ) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

} 

hdd_adapter_t * hdd_get_adapter_by_name( hdd_context_t *pHddCtx, tANI_U8 *name )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && !strncmp( pAdapter->dev->name, (const char *)name,
          IFNAMSIZ ) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

} 

hdd_adapter_t * hdd_get_adapter( hdd_context_t *pHddCtx, device_mode_t mode )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && (mode == pAdapter->device_mode) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

} 

//Remove this function later
hdd_adapter_t * hdd_get_mon_adapter( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && WLAN_HDD_MONITOR == pAdapter->device_mode )
      {
         return pAdapter;
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

} 

/**---------------------------------------------------------------------------
  
  \brief hdd_set_monitor_tx_adapter() - 

   This API initializes the adapter to be used while transmitting on monitor
   adapter. 
   
  \param  - pHddCtx - Pointer to the HDD context.
            pAdapter - Adapter that will used for TX. This can be NULL.
  \return - None. 
  --------------------------------------------------------------------------*/
void wlan_hdd_set_monitor_tx_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   hdd_adapter_t *pMonAdapter;

   pMonAdapter = hdd_get_adapter( pHddCtx, WLAN_HDD_MONITOR );

   if( NULL != pMonAdapter )
   {
      pMonAdapter->sessionCtx.monitor.pAdapterForTx = pAdapter;
   }
}
/**---------------------------------------------------------------------------
  
  \brief hdd_get_operating_channel() -

   This API returns the operating channel of the requested device mode 
   
  \param  - pHddCtx - Pointer to the HDD context.
              - mode - Device mode for which operating channel is required
                suported modes - WLAN_HDD_INFRA_STATION, WLAN_HDD_P2P_CLIENT
                                 WLAN_HDD_SOFTAP, WLAN_HDD_P2P_GO.
  \return - channel number. "0" id the requested device is not found OR it is not connected. 
  --------------------------------------------------------------------------*/
v_U8_t hdd_get_operating_channel( hdd_context_t *pHddCtx, device_mode_t mode )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;
   v_U8_t operatingChannel = 0;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( mode == pAdapter->device_mode )
      {
        switch(pAdapter->device_mode)
        {
          case WLAN_HDD_INFRA_STATION:
          case WLAN_HDD_P2P_CLIENT: 
            if( hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR( pAdapter )) )
              operatingChannel = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.operationChannel;
            break;
          case WLAN_HDD_SOFTAP:
          case WLAN_HDD_P2P_GO:
            /*softap connection info */
            if(test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) 
              operatingChannel = (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->operatingChannel;
            break;
          default:
            break;
        }

        break; //Found the device of interest. break the loop
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }
   return operatingChannel;
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**---------------------------------------------------------------------------

  \brief hdd_set_multicast_list() - 

  This used to set the multicast address list.

  \param  - dev - Pointer to the WLAN device.
  - skb - Pointer to OS packet (sk_buff).
  \return - success/fail 

  --------------------------------------------------------------------------*/
static void hdd_set_multicast_list(struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   int mc_count;
   int i = 0;
   struct netdev_hw_addr *ha;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Adapter context is Null", __func__);
      return;
   }

   if (dev->flags & IFF_ALLMULTI)
   {
      hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: allow all multicast frames", __func__);
      pAdapter->mc_addr_list.mc_cnt = 0;
   }
   else 
   {
      mc_count = netdev_mc_count(dev);
      hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: mc_count = %u", __func__, mc_count);
      if (mc_count > WLAN_HDD_MAX_MC_ADDR_LIST)
      {
         hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: No free filter available; allow all multicast frames", __func__);
         pAdapter->mc_addr_list.mc_cnt = 0;
         return;
      }

      pAdapter->mc_addr_list.mc_cnt = mc_count;

      netdev_for_each_mc_addr(ha, dev) {
         if (i == mc_count)
            break;
         memset(&(pAdapter->mc_addr_list.addr[i][0]), 0, ETH_ALEN);
         memcpy(&(pAdapter->mc_addr_list.addr[i][0]), ha->addr, ETH_ALEN);
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: mlist[%d] = "MAC_ADDRESS_STR,
               __func__, i, 
               MAC_ADDR_ARRAY(pAdapter->mc_addr_list.addr[i]));
         i++;
      }
   }
   return;
}
#endif

/**---------------------------------------------------------------------------
  
  \brief hdd_select_queue() - 

   This function is registered with the Linux OS for network
   core to decide which queue to use first.
   
  \param  - dev - Pointer to the WLAN device.
              - skb - Pointer to OS packet (sk_buff).
  \return - ac, Queue Index/access category corresponding to UP in IP header 
  
  --------------------------------------------------------------------------*/
v_U16_t hdd_select_queue(struct net_device *dev,
    struct sk_buff *skb)
{
   return hdd_wmm_select_queue(dev, skb);
}


/**---------------------------------------------------------------------------

  \brief hdd_wlan_initial_scan() -

   This function triggers the initial scan

  \param  - pAdapter - Pointer to the HDD adapter.

  --------------------------------------------------------------------------*/
void hdd_wlan_initial_scan(hdd_adapter_t *pAdapter)
{
   tCsrScanRequest scanReq;
   tCsrChannelInfo channelInfo;
   eHalStatus halStatus;
   tANI_U32 scanId;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   vos_mem_zero(&scanReq, sizeof(tCsrScanRequest));
   vos_mem_set(&scanReq.bssid, sizeof(tCsrBssid), 0xff);
   scanReq.BSSType = eCSR_BSS_TYPE_ANY;

   if(sme_Is11dSupported(pHddCtx->hHal))
   {
      halStatus = sme_ScanGetBaseChannels( pHddCtx->hHal, &channelInfo );
      if ( HAL_STATUS_SUCCESS( halStatus ) )
      {
         scanReq.ChannelInfo.ChannelList = vos_mem_malloc(channelInfo.numOfChannels);
         if( !scanReq.ChannelInfo.ChannelList )
         {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s kmalloc failed", __func__);
            vos_mem_free(channelInfo.ChannelList);
            channelInfo.ChannelList = NULL;
            return;
         }
         vos_mem_copy(scanReq.ChannelInfo.ChannelList, channelInfo.ChannelList,
            channelInfo.numOfChannels);
         scanReq.ChannelInfo.numOfChannels = channelInfo.numOfChannels;
         vos_mem_free(channelInfo.ChannelList);
         channelInfo.ChannelList = NULL;
      }

      scanReq.scanType = eSIR_PASSIVE_SCAN;
      scanReq.requestType = eCSR_SCAN_REQUEST_11D_SCAN;
      scanReq.maxChnTime = pHddCtx->cfg_ini->nPassiveMaxChnTime;
      scanReq.minChnTime = pHddCtx->cfg_ini->nPassiveMinChnTime;
   }
   else
   {
      scanReq.scanType = eSIR_ACTIVE_SCAN;
      scanReq.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;
      scanReq.maxChnTime = pHddCtx->cfg_ini->nActiveMaxChnTime;
      scanReq.minChnTime = pHddCtx->cfg_ini->nActiveMinChnTime;
   }

   halStatus = sme_ScanRequest(pHddCtx->hHal, pAdapter->sessionId, &scanReq, &scanId, NULL, NULL);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: sme_ScanRequest failed status code %d",
         __func__, halStatus );
   }

   if(sme_Is11dSupported(pHddCtx->hHal))
        vos_mem_free(scanReq.ChannelInfo.ChannelList);
}

/**---------------------------------------------------------------------------

  \brief hdd_full_power_callback() - HDD full power callback function

  This is the function invoked by SME to inform the result of a full power
  request issued by HDD

  \param  - callbackcontext - Pointer to cookie
  \param  - status - result of request

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_full_power_callback(void *callbackContext, eHalStatus status)
{
   struct statsContext *pContext = callbackContext;

   hddLog(VOS_TRACE_LEVEL_INFO,
          "%s: context = %p, status = %d", __func__, pContext, status);

   if (NULL == callbackContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, context [%p]",
             __func__, callbackContext);
      return;
   }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if (POWER_CONTEXT_MAGIC != pContext->magic)
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, magic [%08x]",
              __func__, pContext->magic);
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pContext->magic = 0;

   /* notify the caller */
   complete(&pContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

/**---------------------------------------------------------------------------

  \brief hdd_wlan_exit() - HDD WLAN exit function

  This is the driver exit point (invoked during rmmod)

  \param  - pHddCtx - Pointer to the HDD Context

  \return - None

  --------------------------------------------------------------------------*/
void hdd_wlan_exit(hdd_context_t *pHddCtx)
{
   eHalStatus halStatus;
   v_CONTEXT_t pVosContext = pHddCtx->pvosContext;
   VOS_STATUS vosStatus;
   struct wiphy *wiphy = pHddCtx->wiphy;
   hdd_adapter_t* pAdapter = NULL;
   struct statsContext powerContext;
   long lrc;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;

   ENTER();

   if (VOS_FTM_MODE != hdd_get_conparam())
   {
      // Unloading, restart logic is no more required.
      wlan_hdd_restart_deinit(pHddCtx);

      vosStatus = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
      while (NULL != pAdapterNode && VOS_STATUS_E_EMPTY != vosStatus)
      {
         pAdapter = pAdapterNode->pAdapter;
         if (NULL != pAdapter)
         {
            /* Disable TX on the interface, after this hard_start_xmit() will
             * not be called on that interface
             */
            netif_tx_disable(pAdapter->dev);

            /* Mark the interface status as "down" for outside world */
            netif_carrier_off(pAdapter->dev);

            /* DeInit the adapter. This ensures that all data packets
             * are freed.
             */
            hdd_deinit_adapter(pHddCtx, pAdapter);

            if (WLAN_HDD_INFRA_STATION ==  pAdapter->device_mode ||
                WLAN_HDD_P2P_CLIENT == pAdapter->device_mode)
            {
                wlan_hdd_cfg80211_deregister_frames(pAdapter);
                hdd_UnregisterWext(pAdapter->dev);
            }

         }
         vosStatus = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
      // Cancel any outstanding scan requests.  We are about to close all
      // of our adapters, but an adapter structure is what SME passes back
      // to our callback function. Hence if there are any outstanding scan
      // requests then there is a race condition between when the adapter
      // is closed and when the callback is invoked.We try to resolve that
      // race condition here by canceling any outstanding scans before we
      // close the adapters.
      // Note that the scans may be cancelled in an asynchronous manner,
      // so ideally there needs to be some kind of synchronization. Rather
      // than introduce a new synchronization here, we will utilize the
      // fact that we are about to Request Full Power, and since that is
      // synchronized, the expectation is that by the time Request Full
      // Power has completed all scans will be cancelled.
      if (pHddCtx->scan_info.mScanPending)
      {
          hddLog(VOS_TRACE_LEVEL_INFO,
                 FL("abort scan mode: %d sessionId: %d"),
                     pAdapter->device_mode,
                     pAdapter->sessionId);
           hdd_abort_mac_scan(pHddCtx,
                              pHddCtx->scan_info.sessionId,
                              eCSR_SCAN_ABORT_DEFAULT);
      }
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_INFO,"%s: FTM MODE",__func__);
      wlan_hdd_ftm_close(pHddCtx);
      goto free_hdd_ctx;
   }

   /* DeRegister with platform driver as client for Suspend/Resume */
   vosStatus = hddDeregisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDeregisterPmOps failed",__func__);
      VOS_ASSERT(0);
   }

   vosStatus = hddDevTmUnregisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmUnregisterNotifyCallback failed",__func__);
   }

   //Stop the traffic monitor timer
   if ( VOS_TIMER_STATE_RUNNING ==
                        vos_timer_getCurrentState(&pHddCtx->tx_rx_trafficTmr))
   {
        vos_timer_stop(&pHddCtx->tx_rx_trafficTmr);
   }

   // Destroy the traffic monitor timer
   if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                         &pHddCtx->tx_rx_trafficTmr)))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
           "%s: Cannot deallocate Traffic monitor timer", __func__);
   }

   //Disable IMPS/BMPS as we do not want the device to enter any power
   //save mode during shutdown
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_UAPSD_MODE_POWER_SAVE);

   //Ensure that device is in full power as we will touch H/W during vos_Stop
   init_completion(&powerContext.completion);
   powerContext.magic = POWER_CONTEXT_MAGIC;

   halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_full_power_callback,
                                    &powerContext, eSME_FULL_PWR_NEEDED_BY_HDD);

   if (eHAL_STATUS_SUCCESS != halStatus)
   {
      if (eHAL_STATUS_PMC_PENDING == halStatus)
      {
         /* request was sent -- wait for the response */
         lrc = wait_for_completion_interruptible_timeout(
                                      &powerContext.completion,
                                      msecs_to_jiffies(WLAN_WAIT_TIME_POWER));
         if (lrc <= 0)
         {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: %s while requesting full power",
                   __func__, (0 == lrc) ? "timeout" : "interrupt");
         }
      }
      else
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Request for Full Power failed, status %d",
                __func__, halStatus);
         /* continue -- need to clean up as much as possible */
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   powerContext.magic = 0;
   spin_unlock(&hdd_context_lock);

   hdd_debugfs_exit(pHddCtx);

   // Unregister the Net Device Notifier
   unregister_netdevice_notifier(&hdd_netdev_notifier);
   
   hdd_stop_all_adapters( pHddCtx );

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop BAP",__func__);
   }
#endif //WLAN_BTAMP_FEATURE

   //Stop all the modules
   vosStatus = vos_stop( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to stop VOSS",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }

   //This requires pMac access, Call this before vos_close().
   hdd_unregister_mcast_bcast_filter(pHddCtx);

   //Close the scheduler before calling vos_close to make sure no thread is 
   // scheduled after the each module close is called i.e after all the data 
   // structures are freed.
   vosStatus = vos_sched_close( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))    {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to close VOSS Scheduler",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
#ifdef WLAN_OPEN_SOURCE
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Destroy the wake lock */
   wake_lock_destroy(&pHddCtx->rx_wake_lock);
#endif
   /* Destroy the wake lock */
   wake_lock_destroy(&pHddCtx->sap_wake_lock);
#endif

#ifdef CONFIG_ENABLE_LINUX_REG
   vosStatus = vos_nv_close();
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Failed to close NV", __func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
#endif

   //Close VOSS
   //This frees pMac(HAL) context. There should not be any call that requires pMac access after this.
   vos_close(pVosContext);

   //Close Watchdog
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

   //Clean up HDD Nlink Service
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   if (pHddCtx->cfg_ini->wlanLoggingEnable)
   {
       wlan_logging_sock_deactivate_svc();
   }
#endif

#ifdef WLAN_KD_READY_NOTIFIER
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */


   hdd_close_all_adapters( pHddCtx );

   /* free the power on lock from platform driver */
   if (free_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to free power on lock",
                                           __func__);
   }

free_hdd_ctx:

   //Free up dynamically allocated members inside HDD Adapter
   if (pHddCtx->cfg_ini)
   {
       kfree(pHddCtx->cfg_ini);
       pHddCtx->cfg_ini= NULL;
   }

   /* FTM mode, WIPHY did not registered
      If un-register here, system crash will happen */
   if (VOS_FTM_MODE != hdd_get_conparam())
   {
      wiphy_unregister(wiphy) ;
   }
   wiphy_free(wiphy) ;
   if (hdd_is_ssr_required())
   {
       /* WDI timeout had happened during unload, so SSR is needed here */
       subsystem_restart("wcnss");
       msleep(5000);
   }
   hdd_set_ssr_required (VOS_FALSE);
}


/**---------------------------------------------------------------------------

  \brief hdd_update_config_from_nv() - Function to update the contents of
         the running configuration with parameters taken from NV storage

  \param  - pHddCtx - Pointer to the HDD global context

  \return - VOS_STATUS_SUCCESS if successful

  --------------------------------------------------------------------------*/
static VOS_STATUS hdd_update_config_from_nv(hdd_context_t* pHddCtx)
{
   v_BOOL_t itemIsValid = VOS_FALSE;
   VOS_STATUS status;
   v_MACADDR_t macFromNV[VOS_MAX_CONCURRENCY_PERSONA];
   v_U8_t      macLoop;

   /*If the NV is valid then get the macaddress from nv else get it from qcom_cfg.ini*/
   status = vos_nv_getValidity(VNV_FIELD_IMAGE, &itemIsValid);
   if(status != VOS_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR," vos_nv_getValidity() failed");
       return VOS_STATUS_E_FAILURE;
   }

   if (itemIsValid == VOS_TRUE) 
   {
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH," Reading the Macaddress from NV");
      status = vos_nv_readMultiMacAddress((v_U8_t *)&macFromNV[0].bytes[0],
                                          VOS_MAX_CONCURRENCY_PERSONA);
        if(status != VOS_STATUS_SUCCESS)
        {
         /* Get MAC from NV fail, not update CFG info
          * INI MAC value will be used for MAC setting */
         hddLog(VOS_TRACE_LEVEL_ERROR," vos_nv_readMacAddress() failed");
            return VOS_STATUS_E_FAILURE;
        }

      /* If first MAC is not valid, treat all others are not valid
       * Then all MACs will be got from ini file */
      if(vos_is_macaddr_zero(&macFromNV[0]))
      {
         /* MAC address in NV file is not configured yet */
         hddLog(VOS_TRACE_LEVEL_WARN, "Invalid MAC in NV file");
         return VOS_STATUS_E_INVAL;
   }

      /* Get MAC address from NV, update CFG info */
      for(macLoop = 0; macLoop < VOS_MAX_CONCURRENCY_PERSONA; macLoop++)
      {
         if(vos_is_macaddr_zero(&macFromNV[macLoop]))
         {
            hddLog(VOS_TRACE_LEVEL_ERROR,"not valid MAC from NV for %d", macLoop);
            /* This MAC is not valid, skip it
             * This MAC will be got from ini file */
         }
         else
         {
            vos_mem_copy((v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[macLoop].bytes[0],
                         (v_U8_t *)&macFromNV[macLoop].bytes[0],
                   VOS_MAC_ADDR_SIZE);
         }
      }
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "NV ITEM, MAC Not valid");
      return VOS_STATUS_E_FAILURE;
   }


   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_post_voss_start_config() - HDD post voss start config helper

  \param  - pAdapter - Pointer to the HDD

  \return - None

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx)
{
   eHalStatus halStatus;
   v_U32_t listenInterval;
   tANI_U32    ignoreDtim;


   // Send ready indication to the HDD.  This will kick off the MAC
   // into a 'running' state and should kick off an initial scan.
   halStatus = sme_HDDReadyInd( pHddCtx->hHal );
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: sme_HDDReadyInd() failed with status "
          "code %08d [x%08x]",__func__, halStatus, halStatus );
      return VOS_STATUS_E_FAILURE;
   }

   // Set default LI and ignoreDtim into HDD context,
   // otherwise under some race condition, HDD will set 0 LI value into RIVA,
   // And RIVA will crash
   wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, &listenInterval);
   pHddCtx->hdd_actual_LI_value = listenInterval;
   wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, &ignoreDtim);
   pHddCtx->hdd_actual_ignore_DTIM_value = ignoreDtim;


   return VOS_STATUS_SUCCESS;
}

/* wake lock APIs for HDD */
void hdd_prevent_suspend(void)
{
#ifdef WLAN_OPEN_SOURCE
    wake_lock(&wlan_wake_lock);
#else
    wcnss_prevent_suspend();
#endif
}

void hdd_allow_suspend(void)
{
#ifdef WLAN_OPEN_SOURCE
    wake_unlock(&wlan_wake_lock);
#else
    wcnss_allow_suspend();
#endif
}

void hdd_prevent_suspend_timeout(v_U32_t timeout)
{
#ifdef WLAN_OPEN_SOURCE
    wake_lock_timeout(&wlan_wake_lock, msecs_to_jiffies(timeout));
#else
    /* Do nothing as there is no API in wcnss for timeout*/
#endif
}

/**---------------------------------------------------------------------------

  \brief hdd_exchange_version_and_caps() - HDD function to exchange version and capability
                                                                 information between Host and Riva

  This function gets reported version of FW
  It also finds the version of Riva headers used to compile the host
  It compares the above two and prints a warning if they are different
  It gets the SW and HW version string
  Finally, it exchanges capabilities between host and Riva i.e. host and riva exchange a msg
  indicating the features they support through a bitmap

  \param  - pHddCtx - Pointer to HDD context

  \return -  void

  --------------------------------------------------------------------------*/

void hdd_exchange_version_and_caps(hdd_context_t *pHddCtx)
{

   tSirVersionType versionCompiled;
   tSirVersionType versionReported;
   tSirVersionString versionString;
   tANI_U8 fwFeatCapsMsgSupported = 0;
   VOS_STATUS vstatus;

   memset(&versionCompiled, 0, sizeof(versionCompiled));
   memset(&versionReported, 0, sizeof(versionReported));

   /* retrieve and display WCNSS version information */
   do {

      vstatus = sme_GetWcnssWlanCompiledVersion(pHddCtx->hHal,
                                                &versionCompiled);
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS WLAN compiled version",
                __func__);
         break;
      }

      vstatus = sme_GetWcnssWlanReportedVersion(pHddCtx->hHal,
                                                &versionReported);
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS WLAN reported version",
                __func__);
         break;
      }

      if ((versionCompiled.major != versionReported.major) ||
          (versionCompiled.minor != versionReported.minor) ||
          (versionCompiled.version != versionReported.version) ||
          (versionCompiled.revision != versionReported.revision))
      {
         pr_err("%s: WCNSS WLAN Version %u.%u.%u.%u, "
                "Host expected %u.%u.%u.%u\n",
                WLAN_MODULE_NAME,
                (int)versionReported.major,
                (int)versionReported.minor,
                (int)versionReported.version,
                (int)versionReported.revision,
                (int)versionCompiled.major,
                (int)versionCompiled.minor,
                (int)versionCompiled.version,
                (int)versionCompiled.revision);
      }
      else
      {
         pr_info("%s: WCNSS WLAN version %u.%u.%u.%u\n",
                 WLAN_MODULE_NAME,
                 (int)versionReported.major,
                 (int)versionReported.minor,
                 (int)versionReported.version,
                 (int)versionReported.revision);
      }

      vstatus = sme_GetWcnssSoftwareVersion(pHddCtx->hHal,
                                            versionString,
                                            sizeof(versionString));
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS software version string",
                __func__);
         break;
      }

      pr_info("%s: WCNSS software version %s\n",
              WLAN_MODULE_NAME, versionString);

      vstatus = sme_GetWcnssHardwareVersion(pHddCtx->hHal,
                                            versionString,
                                            sizeof(versionString));
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS hardware version string",
                __func__);
         break;
      }

      pr_info("%s: WCNSS hardware version %s\n",
              WLAN_MODULE_NAME, versionString);

      /* 1.Check if FW version is greater than 0.1.1.0. Only then send host-FW capability exchange message 
         2.Host-FW capability exchange message  is only present on riva 1.1 so 
            send the message only if it the riva is 1.1
            minor numbers for different riva branches:
                0 -> (1.0)Mainline Build
                1 -> (1.1)Mainline Build
                2->(1.04) Stability Build
       */
      if (((versionReported.major>0) || (versionReported.minor>1) || 
         ((versionReported.minor>=1) && (versionReported.version>=1)))
         && ((versionReported.major == 1) && (versionReported.minor >= 1)))
         fwFeatCapsMsgSupported = 1;
 
      if (fwFeatCapsMsgSupported)
      {
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
         if(!pHddCtx->cfg_ini->fEnableActiveModeOffload)
            sme_disableFeatureCapablity(WLANACTIVE_OFFLOAD);
#endif
         /* Indicate if IBSS heartbeat monitoring needs to be offloaded */
         if (!pHddCtx->cfg_ini->enableIbssHeartBeatOffload)
         {
            sme_disableFeatureCapablity(IBSS_HEARTBEAT_OFFLOAD);
         }

         sme_featureCapsExchange(pHddCtx->hHal);
      }

   } while (0);

}
void wlan_hdd_send_svc_nlink_msg(int type, void *data, int len)
{
       struct sk_buff *skb;
       struct nlmsghdr *nlh;
       tAniMsgHdr *ani_hdr;

       skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), GFP_KERNEL);

       if(skb == NULL) {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: alloc_skb failed", __func__);
               return;
       }

       nlh = (struct nlmsghdr *)skb->data;
       nlh->nlmsg_pid = 0;  /* from kernel */
       nlh->nlmsg_flags = 0;
       nlh->nlmsg_seq = 0;
       nlh->nlmsg_type = WLAN_NL_MSG_SVC;

       ani_hdr = NLMSG_DATA(nlh);
       ani_hdr->type = type;

       switch(type) {
               case WLAN_SVC_SAP_RESTART_IND:
                       ani_hdr->length = 0;
                       nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr)));
                       skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr)));
                       break;
               default:
                       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "Attempt to send unknown nlink message %d", type);
                       kfree_skb(skb);
                       return;
       }

    nl_srv_bcast(skb);

    return;
}



/**---------------------------------------------------------------------------

  \brief hdd_is_5g_supported() - HDD function to know if hardware supports  5GHz

  \param  - pHddCtx - Pointer to the hdd context

  \return -  true if hardware supports 5GHz

  --------------------------------------------------------------------------*/
boolean hdd_is_5g_supported(hdd_context_t * pHddCtx)
{
   /* If wcnss_wlan_iris_xo_mode() returns WCNSS_XO_48MHZ(1);
    * then hardware support 5Ghz.
   */
   if (WCNSS_XO_48MHZ == wcnss_wlan_iris_xo_mode())
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Hardware supports 5Ghz", __func__);
      return true;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Hardware doesn't supports 5Ghz",
                    __func__);
      return false;
   }
}

/**---------------------------------------------------------------------------

  \brief hdd_generate_iface_mac_addr_auto() - HDD Mac Interface Auto
                                              generate function

  This is generate the random mac address for WLAN interface

  \param  - pHddCtx  - Pointer to HDD context
            idx      - Start interface index to get auto
                       generated mac addr.
            mac_addr - Mac address

  \return -  0 for success, < 0 for failure

  --------------------------------------------------------------------------*/

static int hdd_generate_iface_mac_addr_auto(hdd_context_t *pHddCtx,
                                            int idx, v_MACADDR_t mac_addr)
{
   int i;
   unsigned int serialno;
   serialno = wcnss_get_serial_number();

   if (0 != serialno)
   {
      /* MAC address has 3 bytes of OUI so we have a maximum of 3
         bytes of the serial number that can be used to generate
         the other 3 bytes of the MAC address.  Mask off all but
         the lower 3 bytes (this will also make sure we don't
         overflow in the next step) */
      serialno &= 0x00FFFFFF;

      /* we need a unique address for each session */
      serialno *= VOS_MAX_CONCURRENCY_PERSONA;

      /* autogen other Mac addresses */
      for (i = idx; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
      {
         /* start with the entire default address */
         pHddCtx->cfg_ini->intfMacAddr[i] = mac_addr;
         /* then replace the lower 3 bytes */
         pHddCtx->cfg_ini->intfMacAddr[i].bytes[3] = (serialno >> 16) & 0xFF;
         pHddCtx->cfg_ini->intfMacAddr[i].bytes[4] = (serialno >> 8) & 0xFF;
         pHddCtx->cfg_ini->intfMacAddr[i].bytes[5] = serialno & 0xFF;

         serialno++;
         hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Derived Mac Addr: "
                   MAC_ADDRESS_STR, __func__,
                   MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[i].bytes));
      }

   }
   else
   {
      hddLog(LOGE, FL("Failed to Get Serial NO"));
      return -1;
   }
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_11d_scan_done - callback to be executed when 11d scan is
                             completed to flush out the scan results

  11d scan is done during driver load and is a passive scan on all
  channels supported by the device, 11d scans may find some APs on
  frequencies which are forbidden to be used in the regulatory domain
  the device is operating in. If these APs are notified to the supplicant
  it may try to connect to these APs, thus flush out all the scan results
  which are present in SME after 11d scan is done.

  \return -  eHalStatus

  --------------------------------------------------------------------------*/
static eHalStatus hdd_11d_scan_done(tHalHandle halHandle, void *pContext,
                         tANI_U32 scanId, eCsrScanStatus status)
{
    ENTER();

    sme_ScanFlushResult(halHandle, 0);

    EXIT();

    return eHAL_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_wlan_startup() - HDD init function

  This is the driver startup code executed once a WLAN device has been detected

  \param  - dev - Pointer to the underlying device

  \return -  0 for success, < 0 for failure

  --------------------------------------------------------------------------*/

int hdd_wlan_startup(struct device *dev )
{
   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_t *pP2pAdapter = NULL;
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext= NULL;
#ifdef WLAN_BTAMP_FEATURE
   VOS_STATUS vStatus = VOS_STATUS_SUCCESS;
   WLANBAP_ConfigType btAmpConfig;
   hdd_config_t *pConfig;
#endif
   int ret;
   struct wiphy *wiphy;
   v_MACADDR_t mac_addr;

   ENTER();
   /*
    * cfg80211: wiphy allocation
    */
   wiphy = wlan_hdd_cfg80211_wiphy_alloc(sizeof(hdd_context_t)) ;

   if(wiphy == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: cfg80211 init failed", __func__);
      return -EIO;
   }

   pHddCtx = wiphy_priv(wiphy);

   //Initialize the adapter context to zeros.
   vos_mem_zero(pHddCtx, sizeof( hdd_context_t ));

   pHddCtx->wiphy = wiphy;
   hdd_prevent_suspend();
   pHddCtx->isLoadUnloadInProgress = WLAN_HDD_LOAD_IN_PROGRESS;

   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   /*Get vos context here bcoz vos_open requires it*/
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(pVosContext == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed vos_get_global_context",__func__);
      goto err_free_hdd_context;
   }

   //Save the Global VOSS context in adapter context for future.
   pHddCtx->pvosContext = pVosContext;

   //Save the adapter context in global context for future.
   ((VosContextType*)(pVosContext))->pHDDContext = (v_VOID_t*)pHddCtx;

   pHddCtx->parent_dev = dev;

   init_completion(&pHddCtx->full_pwr_comp_var);
   init_completion(&pHddCtx->standby_comp_var);
   init_completion(&pHddCtx->req_bmps_comp_var);
   init_completion(&pHddCtx->scan_info.scan_req_completion_event);
   init_completion(&pHddCtx->scan_info.abortscan_event_var);
   init_completion(&pHddCtx->wiphy_channel_update_event);
   init_completion(&pHddCtx->ssr_comp_var);

#ifdef CONFIG_ENABLE_LINUX_REG
   init_completion(&pHddCtx->linux_reg_req);
#else
   init_completion(&pHddCtx->driver_crda_req);
#endif

   spin_lock_init(&pHddCtx->schedScan_lock);

   hdd_list_init( &pHddCtx->hddAdapters, MAX_NUMBER_OF_ADAPTERS );

#ifdef FEATURE_WLAN_TDLS
   /* tdls_lock is initialized before an hdd_open_adapter ( which is
    * invoked by other instances also) to protect the concurrent
    * access for the Adapters by TDLS module.
    */
   mutex_init(&pHddCtx->tdls_lock);
#endif
   /* By default Strict Regulatory For FCC should be false */

   pHddCtx->nEnableStrictRegulatoryForFCC = FALSE;
   // Load all config first as TL config is needed during vos_open
   pHddCtx->cfg_ini = (hdd_config_t*) kmalloc(sizeof(hdd_config_t), GFP_KERNEL);
   if(pHddCtx->cfg_ini == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed kmalloc hdd_config_t",__func__);
      goto err_free_hdd_context;
   }

   vos_mem_zero(pHddCtx->cfg_ini, sizeof( hdd_config_t ));

   // Read and parse the qcom_cfg.ini file
   status = hdd_parse_config_ini( pHddCtx );
   if ( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: error parsing %s",
             __func__, WLAN_INI_FILE);
      goto err_config;
   }
#ifdef MEMORY_DEBUG
   if (pHddCtx->cfg_ini->IsMemoryDebugSupportEnabled)
      vos_mem_init();

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: gEnableMemoryDebug=%d",
          __func__, pHddCtx->cfg_ini->IsMemoryDebugSupportEnabled);
#endif

   /* INI has been read, initialise the configuredMcastBcastFilter with
    * INI value as this will serve as the default value
    */
   pHddCtx->configuredMcastBcastFilter = pHddCtx->cfg_ini->mcastBcastFilterSetting;
   hddLog(VOS_TRACE_LEVEL_INFO, "Setting configuredMcastBcastFilter: %d",
                   pHddCtx->cfg_ini->mcastBcastFilterSetting);

   if (false == hdd_is_5g_supported(pHddCtx))
   {
      //5Ghz is not supported.
      if (1 != pHddCtx->cfg_ini->nBandCapability)
      {
         hddLog(VOS_TRACE_LEVEL_INFO,
                "%s: Setting pHddCtx->cfg_ini->nBandCapability = 1", __func__);
         pHddCtx->cfg_ini->nBandCapability = 1;
      }
   }

   /* If SNR Monitoring is enabled, FW has to parse all beacons
    * for calcaluting and storing the average SNR, so set Nth beacon
    * filter to 1 to enable FW to parse all the beaocons
    */
   if (1 == pHddCtx->cfg_ini->fEnableSNRMonitoring)
   {
      /* The log level is deliberately set to WARN as overriding
       * nthBeaconFilter to 1 will increase power cosumption and this
       * might just prove helpful to detect the power issue.
       */
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Setting pHddCtx->cfg_ini->nthBeaconFilter = 1", __func__);
      pHddCtx->cfg_ini->nthBeaconFilter = 1;
   }
   /*
    * cfg80211: Initialization  ...
    */
   if (VOS_FTM_MODE != hdd_get_conparam())
   {
      if (0 < wlan_hdd_cfg80211_init(dev, wiphy, pHddCtx->cfg_ini))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                 "%s: wlan_hdd_cfg80211_init return failure", __func__);
         goto err_config;
      }
   }

   // Update VOS trace levels based upon the cfg.ini
   hdd_vos_trace_enable(VOS_MODULE_ID_BAP,
                        pHddCtx->cfg_ini->vosTraceEnableBAP);
   hdd_vos_trace_enable(VOS_MODULE_ID_TL,
                        pHddCtx->cfg_ini->vosTraceEnableTL);
   hdd_vos_trace_enable(VOS_MODULE_ID_WDI,
                        pHddCtx->cfg_ini->vosTraceEnableWDI);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD,
                        pHddCtx->cfg_ini->vosTraceEnableHDD);
   hdd_vos_trace_enable(VOS_MODULE_ID_SME,
                        pHddCtx->cfg_ini->vosTraceEnableSME);
   hdd_vos_trace_enable(VOS_MODULE_ID_PE,
                        pHddCtx->cfg_ini->vosTraceEnablePE);
   hdd_vos_trace_enable(VOS_MODULE_ID_PMC,
                         pHddCtx->cfg_ini->vosTraceEnablePMC);
   hdd_vos_trace_enable(VOS_MODULE_ID_WDA,
                        pHddCtx->cfg_ini->vosTraceEnableWDA);
   hdd_vos_trace_enable(VOS_MODULE_ID_SYS,
                        pHddCtx->cfg_ini->vosTraceEnableSYS);
   hdd_vos_trace_enable(VOS_MODULE_ID_VOSS,
                        pHddCtx->cfg_ini->vosTraceEnableVOSS);
   hdd_vos_trace_enable(VOS_MODULE_ID_SAP,
                        pHddCtx->cfg_ini->vosTraceEnableSAP);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD_SOFTAP,
                        pHddCtx->cfg_ini->vosTraceEnableHDDSAP);

   // Update WDI trace levels based upon the cfg.ini
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL,
                        pHddCtx->cfg_ini->wdiTraceEnableDAL);
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL_CTRL,
                        pHddCtx->cfg_ini->wdiTraceEnableCTL);
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL_DATA,
                        pHddCtx->cfg_ini->wdiTraceEnableDAT);
   hdd_wdi_trace_enable(eWLAN_MODULE_PAL,
                        pHddCtx->cfg_ini->wdiTraceEnablePAL);

   if (VOS_FTM_MODE == hdd_get_conparam())
   {
      if ( VOS_STATUS_SUCCESS != wlan_hdd_ftm_open(pHddCtx) )
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: wlan_hdd_ftm_open Failed",__func__);
          goto err_free_hdd_context;
      }
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: FTM driver loaded success fully",__func__);

      vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, FALSE);
      return VOS_STATUS_SUCCESS;
   }

   //Open watchdog module
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
   {
      status = vos_watchdog_open(pVosContext,
         &((VosContextType*)pVosContext)->vosWatchdog, sizeof(VosWatchdogContext));

      if(!VOS_IS_STATUS_SUCCESS( status ))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_watchdog_open failed",__func__);
         goto err_wdclose;
      }
   }

   pHddCtx->isLogpInProgress = FALSE;
   vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);

#ifdef CONFIG_ENABLE_LINUX_REG
   /* initialize the NV module. This is required so that
      we can initialize the channel information in wiphy
      from the NV.bin data. The channel information in
      wiphy needs to be initialized before wiphy registration */

   status = vos_nv_open();
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
       /* NV module cannot be initialized */
       hddLog( VOS_TRACE_LEVEL_FATAL,
                "%s: vos_nv_open failed", __func__);
       goto err_wdclose;
   }

   status = vos_init_wiphy_from_nv_bin();
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
       /* NV module cannot be initialized */
       hddLog( VOS_TRACE_LEVEL_FATAL,
               "%s: vos_init_wiphy failed", __func__);
       goto err_vos_nv_close;
   }

#endif

   status = vos_open( &pVosContext, pHddCtx->parent_dev);
   if ( !VOS_IS_STATUS_SUCCESS( status ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_open failed", __func__);
      goto err_vos_nv_close;
   }

   pHddCtx->hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pVosContext );

   if ( NULL == pHddCtx->hHal )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HAL context is null", __func__);
      goto err_vosclose;
   }

   status = vos_preStart( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_preStart failed", __func__);
      goto err_vosclose;
   }

   if (0 == enable_dfs_chan_scan || 1 == enable_dfs_chan_scan)
   {
      pHddCtx->cfg_ini->enableDFSChnlScan = enable_dfs_chan_scan;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: module enable_dfs_chan_scan set to %d",
             __func__, enable_dfs_chan_scan);
   }
   if (0 == enable_11d || 1 == enable_11d)
   {
      pHddCtx->cfg_ini->Is11dSupportEnabled = enable_11d;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: module enable_11d set to %d",
             __func__, enable_11d);
   }

   /* Note that the vos_preStart() sequence triggers the cfg download.
      The cfg download must occur before we update the SME config
      since the SME config operation must access the cfg database */
   status = hdd_set_sme_config( pHddCtx );

   if ( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed hdd_set_sme_config", __func__);
      goto err_vosclose;
   }

   /* In the integrated architecture we update the configuration from
      the INI file and from NV before vOSS has been started so that
      the final contents are available to send down to the cCPU   */

   // Apply the cfg.ini to cfg.dat
   if (FALSE == hdd_update_config_dat(pHddCtx))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: config update failed",__func__ );
      goto err_vosclose;
   }

   // Get mac addr from platform driver
   ret = wcnss_get_wlan_mac_address((char*)&mac_addr.bytes);

   if ((0 == ret) && (!vos_is_macaddr_zero(&mac_addr)))
   {
      /* Store the mac addr for first interface */
      pHddCtx->cfg_ini->intfMacAddr[0] = mac_addr;

      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: WLAN Mac Addr: "
             MAC_ADDRESS_STR, __func__,
             MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));

      /* Here, passing Arg2 as 1 because we do not want to change the
         last 3 bytes (means non OUI bytes) of first interface mac
         addr.
       */
      if (0 != hdd_generate_iface_mac_addr_auto(pHddCtx, 1, mac_addr))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to generate wlan interface mac addr "
                "using MAC from ini file ", __func__);
      }
   }
   else if (VOS_STATUS_SUCCESS != hdd_update_config_from_nv(pHddCtx))
   {
      // Apply the NV to cfg.dat
      /* Prima Update MAC address only at here */
#ifdef WLAN_AUTOGEN_MACADDR_FEATURE
      /* There was not a valid set of MAC Addresses in NV.  See if the
         default addresses were modified by the cfg.ini settings.  If so,
         we'll use them, but if not, we'll autogenerate a set of MAC
         addresses based upon the device serial number */

      static const v_MACADDR_t default_address =
         {{0x00, 0x0A, 0xF5, 0x89, 0x89, 0xFF}};

      if (0 == memcmp(&default_address, &pHddCtx->cfg_ini->intfMacAddr[0],
                   sizeof(default_address)))
      {
         /* cfg.ini has the default address, invoke autogen logic */

         /* Here, passing Arg2 as 0 because we want to change the
            last 3 bytes (means non OUI bytes) of all the interfaces
            mac addr.
          */
         if (0 != hdd_generate_iface_mac_addr_auto(pHddCtx, 0,
                                                            default_address))
         {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to generate wlan interface mac addr "
                   "using MAC from ini file " MAC_ADDRESS_STR, __func__,
                   MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));
         }
      }
      else
#endif //WLAN_AUTOGEN_MACADDR_FEATURE
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid MAC address in NV, using MAC from ini file "
                MAC_ADDRESS_STR, __func__,
                MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));
      }
   }
   {
      eHalStatus halStatus;

      /* Set the MAC Address Currently this is used by HAL to
       * add self sta. Remove this once self sta is added as
       * part of session open.
       */
      halStatus = cfgSetStr( pHddCtx->hHal, WNI_CFG_STA_ID,
                             (v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[0],
                             sizeof( pHddCtx->cfg_ini->intfMacAddr[0]) );

      if (!HAL_STATUS_SUCCESS( halStatus ))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed to set MAC Address. "
                "HALStatus is %08d [x%08x]",__func__, halStatus, halStatus );
         goto err_vosclose;
      }
   }

   /*Start VOSS which starts up the SME/MAC/HAL modules and everything else
     Note: Firmware image will be read and downloaded inside vos_start API */
   status = vos_start( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
      goto err_vosclose;
   }

#ifdef FEATURE_WLAN_CH_AVOID
    /* Plug in avoid channel notification callback
     * This should happen before ADD_SELF_STA
     * FW will send first IND with ADD_SELF_STA REQ from host */

    /* check the Channel Avoidance is enabled */
   if (TRUE == pHddCtx->cfg_ini->fenableCHAvoidance)
   {
       sme_AddChAvoidCallback(pHddCtx->hHal,
                              hdd_hostapd_ch_avoid_cb);
   }
#endif /* FEATURE_WLAN_CH_AVOID */

   /* Exchange capability info between Host and FW and also get versioning info from FW */
   hdd_exchange_version_and_caps(pHddCtx);

#ifdef CONFIG_ENABLE_LINUX_REG
   status = wlan_hdd_init_channels(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: wlan_hdd_init_channels failed",
             __func__);
      goto err_vosstop;
   }
#endif

   status = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed", 
         __func__);
      goto err_vosstop;
   }

#ifndef CONFIG_ENABLE_LINUX_REG
   wlan_hdd_cfg80211_update_reg_info( wiphy );

   /* registration of wiphy dev with cfg80211 */
   if (0 > wlan_hdd_cfg80211_register(wiphy))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy register failed", __func__);
       goto err_vosstop;
   }
#endif

#ifdef CONFIG_ENABLE_LINUX_REG
   /* registration of wiphy dev with cfg80211 */
   if (0 > wlan_hdd_cfg80211_register(wiphy))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy register failed", __func__);
       goto err_vosstop;
   }

   status = wlan_hdd_init_channels_for_cc(pHddCtx, INIT);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: wlan_hdd_init_channels_for_cc failed",
             __func__);
      goto err_unregister_wiphy;
   }
#endif

   if (VOS_STA_SAP_MODE == hdd_get_conparam())
   {
     pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_SOFTAP, "softap.%d", 
         wlan_hdd_get_intf_addr(pHddCtx), FALSE );
   }
   else
   {
     pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_INFRA_STATION, "wlan%d",
         wlan_hdd_get_intf_addr(pHddCtx), FALSE );
     if (pAdapter != NULL)
     {
         if (pHddCtx->cfg_ini->isP2pDeviceAddrAdministrated && !(pHddCtx->cfg_ini->intfMacAddr[0].bytes[0] & 0x02))
         {
               vos_mem_copy( pHddCtx->p2pDeviceAddress.bytes,
                       pHddCtx->cfg_ini->intfMacAddr[0].bytes,
                       sizeof(tSirMacAddr));

                /* Generate the P2P Device Address.  This consists of the device's
                 * primary MAC address with the locally administered bit set.
                */
                pHddCtx->p2pDeviceAddress.bytes[0] |= 0x02;
         }
         else
         {
             tANI_U8* p2p_dev_addr = wlan_hdd_get_intf_addr(pHddCtx);
             if (p2p_dev_addr != NULL)
             {
                 vos_mem_copy(&pHddCtx->p2pDeviceAddress.bytes[0],
                             p2p_dev_addr, VOS_MAC_ADDR_SIZE);
             }
             else
             {
                   hddLog(VOS_TRACE_LEVEL_FATAL,
                           "%s: Failed to allocate mac_address for p2p_device",
                   __func__);
                   goto err_close_adapter;
             }
         }

         pP2pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_P2P_DEVICE, "p2p%d",
                           &pHddCtx->p2pDeviceAddress.bytes[0], FALSE );
         if ( NULL == pP2pAdapter )
         {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to do hdd_open_adapter for P2P Device Interface",
                __func__);
             goto err_close_adapter;
         }
     }
   }

   if( pAdapter == NULL )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: hdd_open_adapter failed", __func__);
      goto err_close_adapter;
   }

   if (country_code)
   {
      eHalStatus ret;
      INIT_COMPLETION(pAdapter->change_country_code);
      hdd_checkandupdate_dfssetting(pAdapter, country_code);
#ifndef CONFIG_ENABLE_LINUX_REG
      hdd_checkandupdate_phymode(pAdapter, country_code);
#endif
      ret = sme_ChangeCountryCode(pHddCtx->hHal,
                                  (void *)(tSmeChangeCountryCallback)
                                  wlan_hdd_change_country_code_callback,
                                  country_code,
                                  pAdapter, pHddCtx->pvosContext,
                                  eSIR_TRUE, eSIR_TRUE);
      if (eHAL_STATUS_SUCCESS == ret)
      {
         ret = wait_for_completion_interruptible_timeout(
                       &pAdapter->change_country_code,
                       msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));

         if (0 >= ret)
         {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s: SME while setting country code timed out", __func__);
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%s: SME Change Country code from module param fail ret=%d",
                   __func__, ret);
      }
   }

#ifdef WLAN_BTAMP_FEATURE
   vStatus = WLANBAP_Open(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to open BAP",__func__);
      goto err_close_adapter;
   }

   vStatus = BSL_Init(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to Init BSL",__func__);
     goto err_bap_close;
   }
   vStatus = WLANBAP_Start(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to start TL",__func__);
       goto err_bap_close;
   }

   pConfig = pHddCtx->cfg_ini;
   btAmpConfig.ucPreferredChannel = pConfig->preferredChannel;
   status = WLANBAP_SetConfig(&btAmpConfig);

#endif //WLAN_BTAMP_FEATURE

   /*
    * UapsdMask is 0xf if U-APSD is enbaled for all AC's...
    * The value of CFG_QOS_WMM_UAPSD_MASK_DEFAULT is 0xaa(Magic Value)
    * which is greater than 0xf. So the below check is safe to make
    * sure that there is no entry for UapsdMask in the ini
    */
   if (CFG_QOS_WMM_UAPSD_MASK_DEFAULT == pHddCtx->cfg_ini->UapsdMask)
   {
       if(IS_DYNAMIC_WMM_PS_ENABLED)
       {
           hddLog(VOS_TRACE_LEVEL_DEBUG,"%s: Enable UAPSD for VI & VO",
                     __func__);
           pHddCtx->cfg_ini->UapsdMask =
                   CFG_QOS_WMM_UAPSD_MASK_DYMANIC_WMM_PS_DEFAULT;
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_DEBUG,"%s: Do not enable UAPSD",
                     __func__);
           pHddCtx->cfg_ini->UapsdMask =
                   CFG_QOS_WMM_UAPSD_MASK_LEGACY_WMM_PS_DEFAULT;
       }
   }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   if(!(IS_ROAM_SCAN_OFFLOAD_FEATURE_ENABLE))
   {
      hddLog(VOS_TRACE_LEVEL_DEBUG,"%s: ROAM_SCAN_OFFLOAD Feature not supported",__func__);
      pHddCtx->cfg_ini->isRoamOffloadScanEnabled = 0;
      sme_UpdateRoamScanOffloadEnabled((tHalHandle)(pHddCtx->hHal),
                       pHddCtx->cfg_ini->isRoamOffloadScanEnabled);
   }
#endif

   wlan_hdd_tdls_init(pHddCtx);

   sme_Register11dScanDoneCallback(pHddCtx->hHal, hdd_11d_scan_done);

   /* Register with platform driver as client for Suspend/Resume */
   status = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
#ifdef WLAN_BTAMP_FEATURE
      goto err_bap_stop;
#else
      goto err_close_adapter; 
#endif //WLAN_BTAMP_FEATURE
   }

   /* Open debugfs interface */
   if (VOS_STATUS_SUCCESS != hdd_debugfs_init(pAdapter))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: hdd_debugfs_init failed!", __func__);
   }

   /* Register TM level change handler function to the platform */
   status = hddDevTmRegisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmRegisterNotifyCallback failed",__func__);
      goto err_unregister_pmops;
   }

   /* register for riva power on lock to platform driver */
   if (req_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: req riva power on lock failed",
                                     __func__);
      goto err_unregister_pmops;
   }

   // register net device notifier for device change notification
   ret = register_netdevice_notifier(&hdd_netdev_notifier);

   if(ret < 0)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: register_netdevice_notifier failed",__func__);
      goto err_free_power_on_lock;
   }

   //Initialize the nlink service
   if(nl_srv_init() != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: nl_srv_init failed", __func__);
      goto err_reg_netdev;
   }

#ifdef WLAN_KD_READY_NOTIFIER
   pHddCtx->kd_nl_init = 1;
#endif /* WLAN_KD_READY_NOTIFIER */

   //Initialize the BTC service
   if(btc_activate_service(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: btc_activate_service failed",__func__);
      goto err_nl_srv;
   }

#ifdef PTT_SOCK_SVC_ENABLE
   //Initialize the PTT service
   if(ptt_sock_activate_svc(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: ptt_sock_activate_svc failed",__func__);
      goto err_nl_srv;
   }
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   if(pHddCtx->cfg_ini && pHddCtx->cfg_ini->wlanLoggingEnable)
   {
       if(wlan_logging_sock_activate_svc(
                   pHddCtx->cfg_ini->wlanLoggingFEToConsole,
                   pHddCtx->cfg_ini->wlanLoggingNumBuf))
       {
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: wlan_logging_sock_activate_svc"
                   " failed", __func__);
           goto err_nl_srv;
       }
       //TODO: To Remove enableDhcpDebug and use gEnableDebugLog for
       //EAPOL and DHCP
       pHddCtx->cfg_ini->enableDhcpDebug = CFG_DEBUG_DHCP_ENABLE;
       pHddCtx->cfg_ini->gEnableDebugLog = VOS_PKT_PROTO_TYPE_EAPOL;
   }
#endif

   hdd_register_mcast_bcast_filter(pHddCtx);
   if (VOS_STA_SAP_MODE != hdd_get_conparam())
   {
      /* Action frame registered in one adapter which will
       * applicable to all interfaces 
       */
      wlan_hdd_cfg80211_register_frames(pAdapter);
   }

   mutex_init(&pHddCtx->sap_lock);
   mutex_init(&pHddCtx->roc_lock);


#ifdef WLAN_OPEN_SOURCE
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Initialize the wake lcok */
   wake_lock_init(&pHddCtx->rx_wake_lock,
           WAKE_LOCK_SUSPEND,
           "qcom_rx_wakelock");
#endif
   /* Initialize the wake lcok */
   wake_lock_init(&pHddCtx->sap_wake_lock,
           WAKE_LOCK_SUSPEND,
           "qcom_sap_wakelock");
#endif

   vos_event_init(&pHddCtx->scan_info.scan_finished_event);
   pHddCtx->scan_info.scan_pending_option = WEXT_SCAN_PENDING_GIVEUP;

   pHddCtx->isLoadUnloadInProgress = WLAN_HDD_NO_LOAD_UNLOAD_IN_PROGRESS;
   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   hdd_allow_suspend();

#ifdef FEATURE_WLAN_SCAN_PNO
   /*SME must send channel update configuration to RIVA*/
   sme_UpdateChannelConfig(pHddCtx->hHal);
#endif
   /* Send the update default channel list to the FW*/
   sme_UpdateChannelList(pHddCtx->hHal);
#ifndef CONFIG_ENABLE_LINUX_REG
   /*updating wiphy so that regulatory user hints can be processed*/
   if (wiphy)
   {
       regulatory_hint(wiphy, "00");
   }
#endif
   // Initialize the restart logic
   wlan_hdd_restart_init(pHddCtx);

   //Register the traffic monitor timer now
   if ( pHddCtx->cfg_ini->dynSplitscan)
   {
       vos_timer_init(&pHddCtx->tx_rx_trafficTmr,
                     VOS_TIMER_TYPE_SW,
                     hdd_tx_rx_pkt_cnt_stat_timer_handler,
                     (void *)pHddCtx);
   }
#ifdef WLAN_FEATURE_EXTSCAN
    sme_EXTScanRegisterCallback(pHddCtx->hHal,
            wlan_hdd_cfg80211_extscan_callback,
                           pHddCtx);
#endif /* WLAN_FEATURE_EXTSCAN */
   goto success;

err_nl_srv:
#ifdef WLAN_KD_READY_NOTIFIER
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
err_reg_netdev:
   unregister_netdevice_notifier(&hdd_netdev_notifier);

err_free_power_on_lock:
   free_riva_power_on_lock("wlan");

err_unregister_pmops:
   hddDevTmUnregisterNotifyCallback(pHddCtx);
   hddDeregisterPmOps(pHddCtx);

   hdd_debugfs_exit(pHddCtx);

#ifdef WLAN_BTAMP_FEATURE
err_bap_stop:
  WLANBAP_Stop(pVosContext);
#endif

#ifdef WLAN_BTAMP_FEATURE
err_bap_close:
   WLANBAP_Close(pVosContext);
#endif

err_close_adapter:
   hdd_close_all_adapters( pHddCtx );
#ifdef CONFIG_ENABLE_LINUX_REG
err_unregister_wiphy:
#endif
   wiphy_unregister(wiphy) ;
err_vosstop:
   vos_stop(pVosContext);

err_vosclose:
   status = vos_sched_close( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(status))    {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to close VOSS Scheduler", __func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );
   }
   vos_close(pVosContext );

err_vos_nv_close:

#ifdef CONFIG_ENABLE_LINUX_REG
   vos_nv_close();

#endif

err_wdclose:
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

err_config:
   kfree(pHddCtx->cfg_ini);
   pHddCtx->cfg_ini= NULL;

err_free_hdd_context:
   hdd_allow_suspend();
   wiphy_free(wiphy) ;
   //kfree(wdev) ;
   VOS_BUG(1);

   if (hdd_is_ssr_required())
   {
       /* WDI timeout had happened during load, so SSR is needed here */
       subsystem_restart("wcnss");
       msleep(5000);
   }
   hdd_set_ssr_required (VOS_FALSE);

   return -EIO;

success:
   EXIT();
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_driver_init() - Core Driver Init Function

   This is the driver entry point - called in different timeline depending
   on whether the driver is statically or dynamically linked

  \param  - None

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int hdd_driver_init( void)
{
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;
   struct device *dev = NULL;
   int ret_status = 0;
#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   int max_retries = 0;
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   wlan_logging_sock_init_svc();
#endif

   ENTER();

#ifdef WLAN_OPEN_SOURCE
   wake_lock_init(&wlan_wake_lock, WAKE_LOCK_SUSPEND, "wlan");
#endif

   hddTraceInit();
   pr_info("%s: loading driver v%s\n", WLAN_MODULE_NAME,
           QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR);

#ifdef ANI_BUS_TYPE_PCI

   dev = wcnss_wlan_get_device();

#endif // ANI_BUS_TYPE_PCI

#ifdef ANI_BUS_TYPE_PLATFORM

#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   /* wait until WCNSS driver downloads NV */
   while (!wcnss_device_ready() && 5 >= ++max_retries) {
       msleep(1000);
   }
   if (max_retries >= 5) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WCNSS driver not ready", __func__);
#ifdef WLAN_OPEN_SOURCE
      wake_lock_destroy(&wlan_wake_lock);
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
      wlan_logging_sock_deinit_svc();
#endif

      return -ENODEV;
   }
#endif

   dev = wcnss_wlan_get_device();
#endif // ANI_BUS_TYPE_PLATFORM


   do {
      if (NULL == dev) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN device not found!!",__func__);
         ret_status = -1;
         break;
   }

#ifdef TIMER_MANAGER
      vos_timer_manager_init();
#endif

      /* Preopen VOSS so that it is ready to start at least SAL */
      status = vos_preOpen(&pVosContext);

   if (!VOS_IS_STATUS_SUCCESS(status))
   {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed to preOpen VOSS", __func__);
         ret_status = -1;
         break;
   }

#ifndef MODULE
      /* For statically linked driver, call hdd_set_conparam to update curr_con_mode
       */
      hdd_set_conparam((v_UINT_t)con_mode);
#endif

      // Call our main init function
      if (hdd_wlan_startup(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WLAN Driver Initialization failed",
                __func__);
         vos_preClose( &pVosContext );
         ret_status = -1;
         break;
      }

   } while (0);

   if (0 != ret_status)
   {
#ifdef TIMER_MANAGER
      vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
      vos_mem_exit();
#endif

#ifdef WLAN_OPEN_SOURCE
      wake_lock_destroy(&wlan_wake_lock);
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
      wlan_logging_sock_deinit_svc();
#endif

      pr_err("%s: driver load failure\n", WLAN_MODULE_NAME);
   }
   else
   {
      //Send WLAN UP indication to Nlink Service
      send_btc_nlink_msg(WLAN_MODULE_UP_IND, 0);

      pr_info("%s: driver loaded\n", WLAN_MODULE_NAME);
   }

   EXIT();

   return ret_status;
}

/**---------------------------------------------------------------------------

  \brief hdd_module_init() - Init Function

   This is the driver entry point (invoked when module is loaded using insmod)

  \param  - None

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
#ifdef MODULE
static int __init hdd_module_init ( void)
{
   return hdd_driver_init();
}
#else /* #ifdef MODULE */
static int __init hdd_module_init ( void)
{
   /* Driver initialization is delayed to fwpath_changed_handler */
   return 0;
}
#endif /* #ifdef MODULE */


/**---------------------------------------------------------------------------

  \brief hdd_driver_exit() - Exit function

  This is the driver exit point (invoked when module is unloaded using rmmod
  or con_mode was changed by userspace)

  \param  - None

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_driver_exit(void)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;
   v_REGDOMAIN_t regId;
   unsigned long rc = 0;

   pr_info("%s: unloading driver v%s\n", WLAN_MODULE_NAME, QWLAN_VERSIONSTR);

   //Get the global vos context
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(!pVosContext)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      goto done;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: module exit called before probe",__func__);
   }
   else
   {
       INIT_COMPLETION(pHddCtx->ssr_comp_var);

       if ((pHddCtx->isLogpInProgress) &&
           (FALSE == vos_is_wlan_in_badState(VOS_MODULE_ID_HDD, NULL)))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                     "%s:SSR in Progress; block rmmod !!!", __func__);
           rc = wait_for_completion_timeout(&pHddCtx->ssr_comp_var,
                                             msecs_to_jiffies(30000));
           if(!rc)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                         "%s:SSR timedout, fatal error", __func__);
               VOS_BUG(0);
           }
       }

       rtnl_lock();
       pHddCtx->isLoadUnloadInProgress = WLAN_HDD_UNLOAD_IN_PROGRESS;
       vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);
       rtnl_unlock();

       /* Driver Need to send country code 00 in below condition
        * 1) If gCountryCodePriority is set to 1; and last country
        * code set is through 11d. This needs to be done in case
        * when NV country code is 00.
        * This Needs to be done as when kernel store last country
        * code and if stored  country code is not through 11d,
        * in sme_HandleChangeCountryCodeByUser we will disable 11d
        * in next load/unload as soon as we get any country through
        * 11d. In sme_HandleChangeCountryCodeByUser
        * pMsg->countryCode will be last countryCode and
        * pMac->scan.countryCode11d will be country through 11d so
        * due to mismatch driver will disable 11d.
        *
        */

        if ((eANI_BOOLEAN_TRUE == sme_Is11dCountrycode(pHddCtx->hHal) &&
              pHddCtx->cfg_ini->fSupplicantCountryCodeHasPriority  &&
              sme_Is11dSupported(pHddCtx->hHal)))
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                     FL("CountryCode 00 is being set while unloading driver"));
            vos_nv_getRegDomainFromCountryCode(&regId , "00", COUNTRY_USER);
        }

        //Do all the cleanup before deregistering the driver
        hdd_wlan_exit(pHddCtx);
   }

   vos_preClose( &pVosContext );

#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   wlan_logging_sock_deinit_svc();
#endif

done:
#ifdef WLAN_OPEN_SOURCE
   wake_lock_destroy(&wlan_wake_lock);
#endif

   pr_info("%s: driver unloaded\n", WLAN_MODULE_NAME);
}

/**---------------------------------------------------------------------------

  \brief hdd_module_exit() - Exit function

  This is the driver exit point (invoked when module is unloaded using rmmod)

  \param  - None

  \return - None

  --------------------------------------------------------------------------*/
static void __exit hdd_module_exit(void)
{
   hdd_driver_exit();
}

#ifdef MODULE
static int fwpath_changed_handler(const char *kmessage,
                                 struct kernel_param *kp)
{
   return param_set_copystring(kmessage, kp);
}

static int con_mode_handler(const char *kmessage,
                                 struct kernel_param *kp)
{
   return param_set_int(kmessage, kp);
}
#else /* #ifdef MODULE */
/**---------------------------------------------------------------------------

  \brief kickstart_driver

   This is the driver entry point
   - delayed driver initialization when driver is statically linked
   - invoked when module parameter fwpath is modified from userspace to signal
     initializing the WLAN driver or when con_mode is modified from userspace
     to signal a switch in operating mode

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int kickstart_driver(void)
{
   int ret_status;

   if (!wlan_hdd_inited) {
      ret_status = hdd_driver_init();
      wlan_hdd_inited = ret_status ? 0 : 1;
      return ret_status;
   }

   hdd_driver_exit();

   msleep(200);

   ret_status = hdd_driver_init();
   wlan_hdd_inited = ret_status ? 0 : 1;
   return ret_status;
}

/**---------------------------------------------------------------------------

  \brief fwpath_changed_handler() - Handler Function

   Handle changes to the fwpath parameter

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int fwpath_changed_handler(const char *kmessage,
                                  struct kernel_param *kp)
{
   int ret;

   ret = param_set_copystring(kmessage, kp);
   if (0 == ret)
      ret = kickstart_driver();
   return ret;
}

/**---------------------------------------------------------------------------

  \brief con_mode_handler() -

  Handler function for module param con_mode when it is changed by userspace
  Dynamically linked - do nothing
  Statically linked - exit and init driver, as in rmmod and insmod

  \param  -

  \return -

  --------------------------------------------------------------------------*/
static int con_mode_handler(const char *kmessage, struct kernel_param *kp)
{
   int ret;

   ret = param_set_int(kmessage, kp);
   if (0 == ret)
      ret = kickstart_driver();
   return ret;
}
#endif /* #ifdef MODULE */

/**---------------------------------------------------------------------------

  \brief hdd_get_conparam() -

  This is the driver exit point (invoked when module is unloaded using rmmod)

  \param  - None

  \return - tVOS_CON_MODE

  --------------------------------------------------------------------------*/
tVOS_CON_MODE hdd_get_conparam ( void )
{
#ifdef MODULE
    return (tVOS_CON_MODE)con_mode;
#else
    return (tVOS_CON_MODE)curr_con_mode;
#endif
}
void hdd_set_conparam ( v_UINT_t newParam )
{
  con_mode = newParam;
#ifndef MODULE
  curr_con_mode = con_mode;
#endif
}
/**---------------------------------------------------------------------------

  \brief hdd_softap_sta_deauth() - function

  This to take counter measure to handle deauth req from HDD

  \param  - pAdapter - Pointer to the HDD

  \param  - enable - boolean value

  \return - None

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_softap_sta_deauth(hdd_adapter_t *pAdapter,
                                 struct tagCsrDelStaParams *pDelStaParams)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    ENTER();

    hddLog(LOG1, "hdd_softap_sta_deauth:(%p, false)",
           (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to deauth bcmc station
    if (pDelStaParams->peerMacAddr[0] & 0x1)
       return vosStatus;

    vosStatus = WLANSAP_DeauthSta(pVosContext, pDelStaParams);

    EXIT();
    return vosStatus;
}

/**---------------------------------------------------------------------------

  \brief hdd_del_all_sta() - function

  This function removes all the stations associated on stopping AP/P2P GO.

  \param  - pAdapter - Pointer to the HDD

  \return - None

  --------------------------------------------------------------------------*/

int hdd_del_all_sta(hdd_adapter_t *pAdapter)
{
    v_U16_t i;
    VOS_STATUS vos_status;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: Delete all STAs associated.",__func__);
    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
     || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        for(i = 0; i < WLAN_MAX_STA_COUNT; i++)
        {
            if ((pAdapter->aStaInfo[i].isUsed) &&
                (!pAdapter->aStaInfo[i].isDeauthInProgress))
            {
                struct tagCsrDelStaParams delStaParams;

                WLANSAP_PopulateDelStaParams(
                            pAdapter->aStaInfo[i].macAddrSTA.bytes,
                            eCsrForcedDeauthSta, SIR_MAC_MGMT_DEAUTH >> 4,
                            &delStaParams);
                vos_status = hdd_softap_sta_deauth(pAdapter, &delStaParams);
                if (VOS_IS_STATUS_SUCCESS(vos_status))
                    pAdapter->aStaInfo[i].isDeauthInProgress = TRUE;
            }
        }
    }

    EXIT();
    return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_softap_sta_disassoc() - function

  This to take counter measure to handle deauth req from HDD

  \param  - pAdapter - Pointer to the HDD

  \param  - enable - boolean value

  \return - None

  --------------------------------------------------------------------------*/

void hdd_softap_sta_disassoc(hdd_adapter_t *pAdapter,v_U8_t *pDestMacAddress)
{
        v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;

    ENTER();

    hddLog( LOGE, "hdd_softap_sta_disassoc:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to disassoc bcmc station
    if( pDestMacAddress[0] & 0x1 )
       return;

    WLANSAP_DisassocSta(pVosContext,pDestMacAddress);
}

void hdd_softap_tkip_mic_fail_counter_measure(hdd_adapter_t *pAdapter,v_BOOL_t enable)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;

    ENTER();

    hddLog( LOGE, "hdd_softap_tkip_mic_fail_counter_measure:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    WLANSAP_SetCounterMeasure(pVosContext, (v_BOOL_t)enable);
}

/**---------------------------------------------------------------------------
 *
 *   \brief hdd_get__concurrency_mode() -
 *
 *
 *   \param  - None
 *
 *   \return - CONCURRENCY MODE
 *
 * --------------------------------------------------------------------------*/
tVOS_CONCURRENCY_MODE hdd_get_concurrency_mode ( void )
{
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;

    if (NULL != pVosContext)
    {
       pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
       if (NULL != pHddCtx)
       {
          return (tVOS_CONCURRENCY_MODE)pHddCtx->concurrency_mode;
       }
    }

    /* we are in an invalid state :( */
    hddLog(LOGE, "%s: Invalid context", __func__);
    return VOS_STA;
}
v_BOOL_t
wlan_hdd_is_GO_power_collapse_allowed (hdd_context_t* pHddCtx)
{
     hdd_adapter_t *pAdapter = NULL;

     pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_GO);
     if (pAdapter == NULL)
     {
         hddLog(VOS_TRACE_LEVEL_INFO,
                FL("GO doesn't exist"));
         return TRUE;
     }
     if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
     {
          hddLog(VOS_TRACE_LEVEL_INFO,
                 FL("GO started"));
          return TRUE;
     }
     else
          /* wait till GO changes its interface to p2p device */
          hddLog(VOS_TRACE_LEVEL_INFO,
                 FL("Del_bss called, avoid apps suspend"));
          return FALSE;

}
/* Decide whether to allow/not the apps power collapse. 
 * Allow apps power collapse if we are in connected state.
 * if not, allow only if we are in IMPS  */
v_BOOL_t hdd_is_apps_power_collapse_allowed(hdd_context_t* pHddCtx)
{
    tPmcState pmcState = pmcGetPmcState(pHddCtx->hHal);
    tANI_BOOLEAN scanRspPending = csrNeighborRoamScanRspPending(pHddCtx->hHal);
    tANI_BOOLEAN inMiddleOfRoaming = csrNeighborMiddleOfRoaming(pHddCtx->hHal);
    hdd_config_t *pConfig = pHddCtx->cfg_ini;
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL; 
    hdd_adapter_t *pAdapter = NULL; 
    VOS_STATUS status;
    tVOS_CONCURRENCY_MODE concurrent_state = 0;

    if (VOS_STA_SAP_MODE == hdd_get_conparam())
        return TRUE;

    concurrent_state = hdd_get_concurrency_mode();

    if ((concurrent_state == (VOS_STA | VOS_P2P_GO)) &&
          !(wlan_hdd_is_GO_power_collapse_allowed(pHddCtx)))
        return FALSE;
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE

    if(((concurrent_state == (VOS_STA | VOS_P2P_CLIENT)) || 
        (concurrent_state == (VOS_STA | VOS_P2P_GO)))&&
        (IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE))
        return TRUE;
#endif

    /*loop through all adapters. TBD fix for Concurrency */
    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;
        if ( (WLAN_HDD_INFRA_STATION == pAdapter->device_mode)
          || (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
        {
            if (((pConfig->fIsImpsEnabled || pConfig->fIsBmpsEnabled)
                 && (pmcState != IMPS && pmcState != BMPS && pmcState != UAPSD
                  &&  pmcState != STOPPED && pmcState != STANDBY)) ||
                 (eANI_BOOLEAN_TRUE == scanRspPending) ||
                 (eANI_BOOLEAN_TRUE == inMiddleOfRoaming))
            {
                hddLog( LOGE, "%s: do not allow APPS power collapse-"
                    "pmcState = %d scanRspPending = %d inMiddleOfRoaming = %d",
                    __func__, pmcState, scanRspPending, inMiddleOfRoaming );
                return FALSE;
            }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    return TRUE;
}

/* Decides whether to send suspend notification to Riva
 * if any adapter is in BMPS; then it is required */
v_BOOL_t hdd_is_suspend_notify_allowed(hdd_context_t* pHddCtx)
{
    tPmcState pmcState = pmcGetPmcState(pHddCtx->hHal);
    hdd_config_t *pConfig = pHddCtx->cfg_ini;

    if (pConfig->fIsBmpsEnabled && (pmcState == BMPS))
    {
        return TRUE;
    }
    return FALSE;
}

void wlan_hdd_set_concurrency_mode(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch(mode)
   {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
            pHddCtx->concurrency_mode |= (1 << mode);
            pHddCtx->no_of_open_sessions[mode]++;
            break;
       default:
            break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("concurrency_mode = 0x%x "
          "Number of open sessions for mode %d = %d"),
           pHddCtx->concurrency_mode, mode,
           pHddCtx->no_of_open_sessions[mode]);
}


void wlan_hdd_clear_concurrency_mode(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch(mode)
   {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
            pHddCtx->no_of_open_sessions[mode]--;
            if (!(pHddCtx->no_of_open_sessions[mode]))
                pHddCtx->concurrency_mode &= (~(1 << mode));
            break;
       default:
            break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("concurrency_mode = 0x%x "
          "Number of open sessions for mode %d = %d"),
          pHddCtx->concurrency_mode, mode, pHddCtx->no_of_open_sessions[mode]);

}
/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_incr_active_session()
 *
 *   This function increments the number of active sessions
 *   maintained per device mode
 *   Incase of STA/P2P CLI/IBSS upon connection indication it is incremented
 *   Incase of SAP/P2P GO upon bss start it is incremented
 *
 *   \param  pHddCtx - HDD Context
 *   \param  mode    - device mode
 *
 *   \return - None
 *
 * --------------------------------------------------------------------------*/
void wlan_hdd_incr_active_session(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch (mode) {
   case VOS_STA_MODE:
   case VOS_P2P_CLIENT_MODE:
   case VOS_P2P_GO_MODE:
   case VOS_STA_SAP_MODE:
        pHddCtx->no_of_active_sessions[mode]++;
        break;
   default:
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Not Expected Mode %d"), mode);
        break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("No.# of active sessions for mode %d = %d"),
                                mode,
                                pHddCtx->no_of_active_sessions[mode]);
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_decr_active_session()
 *
 *   This function decrements the number of active sessions
 *   maintained per device mode
 *   Incase of STA/P2P CLI/IBSS upon disconnection it is decremented
 *   Incase of SAP/P2P GO upon bss stop it is decremented
 *
 *   \param  pHddCtx - HDD Context
 *   \param  mode    - device mode
 *
 *   \return - None
 *
 * --------------------------------------------------------------------------*/
void wlan_hdd_decr_active_session(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch (mode) {
   case VOS_STA_MODE:
   case VOS_P2P_CLIENT_MODE:
   case VOS_P2P_GO_MODE:
   case VOS_STA_SAP_MODE:
        if (pHddCtx->no_of_active_sessions[mode] > 0)
            pHddCtx->no_of_active_sessions[mode]--;
        else
            hddLog(VOS_TRACE_LEVEL_INFO, FL(" No.# of Active sessions"
                                     "is already Zero"));
        break;
   default:
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Not Expected Mode %d"), mode);
        break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("No.# of active sessions for mode %d = %d"),
                                mode,
                                pHddCtx->no_of_active_sessions[mode]);
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_init
 *
 *   This function initalizes restart timer/flag. An internal function.
 *
 *   \param  - pHddCtx
 *
 *   \return - None
 *             
 * --------------------------------------------------------------------------*/

static void wlan_hdd_restart_init(hdd_context_t *pHddCtx)
{
   /* Initialize */
   pHddCtx->hdd_restart_retries = 0;
   atomic_set(&pHddCtx->isRestartInProgress, 0);
   vos_timer_init(&pHddCtx->hdd_restart_timer, 
                     VOS_TIMER_TYPE_SW, 
                     wlan_hdd_restart_timer_cb,
                     pHddCtx);
}
/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_deinit
 *
 *   This function cleans up the resources used. An internal function.
 *
 *   \param  - pHddCtx
 *
 *   \return - None
 *             
 * --------------------------------------------------------------------------*/

static void wlan_hdd_restart_deinit(hdd_context_t* pHddCtx)
{
 
   VOS_STATUS vos_status;
   /* Block any further calls */
   atomic_set(&pHddCtx->isRestartInProgress, 1);
   /* Cleanup */
   vos_status = vos_timer_stop( &pHddCtx->hdd_restart_timer );
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGE, FL("Failed to stop HDD restart timer"));
   vos_status = vos_timer_destroy(&pHddCtx->hdd_restart_timer);
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGE, FL("Failed to destroy HDD restart timer"));

}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_framework_restart
 *
 *   This function uses a cfg80211 API to start a framework initiated WLAN
 *   driver module unload/load.
 *
 *   Also this API keep retrying (WLAN_HDD_RESTART_RETRY_MAX_CNT).
 *
 *
 *   \param  - pHddCtx
 *
 *   \return - VOS_STATUS_SUCCESS: Success
 *             VOS_STATUS_E_EMPTY: Adapter is Empty
 *             VOS_STATUS_E_NOMEM: No memory

 * --------------------------------------------------------------------------*/

static VOS_STATUS wlan_hdd_framework_restart(hdd_context_t *pHddCtx) 
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   int len = (sizeof (struct ieee80211_mgmt));
   struct ieee80211_mgmt *mgmt = NULL; 
   
   /* Prepare the DEAUTH managment frame with reason code */
   mgmt =  kzalloc(len, GFP_KERNEL);
   if(mgmt == NULL) 
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, 
            "%s: memory allocation failed (%d bytes)", __func__, len);
      return VOS_STATUS_E_NOMEM;
   }
   mgmt->u.deauth.reason_code = WLAN_REASON_DISASSOC_LOW_ACK;

   /* Iterate over all adapters/devices */
   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   do 
   {
      if( (status == VOS_STATUS_SUCCESS) && 
                           pAdapterNode  && 
                           pAdapterNode->pAdapter)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, 
               "restarting the driver(intf:\'%s\' mode:%d :try %d)",
               pAdapterNode->pAdapter->dev->name,
               pAdapterNode->pAdapter->device_mode,
               pHddCtx->hdd_restart_retries + 1);
         /* 
          * CFG80211 event to restart the driver
          * 
          * 'cfg80211_send_unprot_deauth' sends a 
          * NL80211_CMD_UNPROT_DEAUTHENTICATE event to supplicant at any state 
          * of SME(Linux Kernel) state machine.
          *
          * Reason code WLAN_REASON_DISASSOC_LOW_ACK is currently used to restart
          * the driver.
          *
          */
         
         cfg80211_send_unprot_deauth(pAdapterNode->pAdapter->dev, (u_int8_t*)mgmt, len );  
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   } while((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status));


   /* Free the allocated management frame */
   kfree(mgmt);

   /* Retry until we unload or reach max count */
   if(++pHddCtx->hdd_restart_retries < WLAN_HDD_RESTART_RETRY_MAX_CNT) 
      vos_timer_start(&pHddCtx->hdd_restart_timer, WLAN_HDD_RESTART_RETRY_DELAY_MS);

   return status;

}
/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_timer_cb
 *
 *   Restart timer callback. An internal function.
 *
 *   \param  - User data:
 *
 *   \return - None
 *             
 * --------------------------------------------------------------------------*/

void wlan_hdd_restart_timer_cb(v_PVOID_t usrDataForCallback)
{
   hdd_context_t *pHddCtx = usrDataForCallback;
   wlan_hdd_framework_restart(pHddCtx);
   return;

}


/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_driver
 *
 *   This function sends an event to supplicant to restart the WLAN driver. 
 *   
 *   This function is called from vos_wlanRestart.
 *
 *   \param  - pHddCtx
 *
 *   \return - VOS_STATUS_SUCCESS: Success
 *             VOS_STATUS_E_EMPTY: Adapter is Empty
 *             VOS_STATUS_E_ALREADY: Request already in progress

 * --------------------------------------------------------------------------*/
VOS_STATUS wlan_hdd_restart_driver(hdd_context_t *pHddCtx) 
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   /* A tight check to make sure reentrancy */
   if(atomic_xchg(&pHddCtx->isRestartInProgress, 1))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s: WLAN restart is already in progress", __func__);

      return VOS_STATUS_E_ALREADY;
   }
   /* Send reset FIQ to WCNSS to invoke SSR. */
#ifdef HAVE_WCNSS_RESET_INTR
   wcnss_reset_intr();
#endif
 
   return status;
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_init_channels
 *
 *   This function is used to initialize the channel list in CSR
 *
 *   This function is called from hdd_wlan_startup
 *
 *   \param  - pHddCtx: HDD context
 *
 *   \return - VOS_STATUS_SUCCESS: Success
 *             VOS_STATUS_E_FAULT: Failure reported by SME

 * --------------------------------------------------------------------------*/
static VOS_STATUS wlan_hdd_init_channels(hdd_context_t *pHddCtx)
{
   eHalStatus status;

   status = sme_InitChannels(pHddCtx->hHal);
   if (HAL_STATUS_SUCCESS(status))
   {
      return VOS_STATUS_SUCCESS;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Channel initialization failed(%d)",
             __func__, status);
      return VOS_STATUS_E_FAULT;
   }
}

#ifdef CONFIG_ENABLE_LINUX_REG
VOS_STATUS wlan_hdd_init_channels_for_cc(hdd_context_t *pHddCtx, driver_load_type init )
{
   eHalStatus status;

   status = sme_InitChannelsForCC(pHddCtx->hHal, init);
   if (HAL_STATUS_SUCCESS(status))
   {
      return VOS_STATUS_SUCCESS;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Issue reg hint failed(%d)",
             __func__, status);
      return VOS_STATUS_E_FAULT;
   }
}
#endif
/*
 * API to find if there is any STA or P2P-Client is connected
 */
VOS_STATUS hdd_issta_p2p_clientconnected(hdd_context_t *pHddCtx)
{
    return sme_isSta_p2p_clientConnected(pHddCtx->hHal);
}

/*
 * API to find if there is any session connected
 */
VOS_STATUS hdd_is_any_session_connected(hdd_context_t *pHddCtx)
{
    return sme_is_any_session_connected(pHddCtx->hHal);
}


int wlan_hdd_scan_abort(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_scaninfo_t *pScanInfo = NULL;
    long status = 0;

    pScanInfo = &pHddCtx->scan_info;
    if (pScanInfo->mScanPending)
    {
        INIT_COMPLETION(pScanInfo->abortscan_event_var);
        hdd_abort_mac_scan(pHddCtx, pScanInfo->sessionId,
                                    eCSR_SCAN_ABORT_DEFAULT);

        status = wait_for_completion_interruptible_timeout(
                           &pScanInfo->abortscan_event_var,
                           msecs_to_jiffies(5000));
        if (0 >= status)
        {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Timeout or Interrupt occurred while waiting for abort"
                  "scan, status- %ld", __func__, status);
            return -ETIMEDOUT;
        }
    }
    return 0;
}

VOS_STATUS wlan_hdd_cancel_remain_on_channel(hdd_context_t *pHddCtx)
{
    hdd_adapter_t *pAdapter;
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS vosStatus;

    vosStatus = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while (NULL != pAdapterNode && VOS_STATUS_E_EMPTY != vosStatus)
    {
        pAdapter = pAdapterNode->pAdapter;
        if (NULL != pAdapter)
        {
            if (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode ||
                WLAN_HDD_P2P_CLIENT == pAdapter->device_mode ||
                WLAN_HDD_P2P_GO == pAdapter->device_mode)
            {
                hddLog(LOG1, FL("abort ROC deviceMode: %d"),
                                 pAdapter->device_mode);
                if (VOS_STATUS_SUCCESS !=
                       wlan_hdd_cancel_existing_remain_on_channel(pAdapter))
                {
                    hddLog(LOGE, FL("failed to abort ROC"));
                    return VOS_STATUS_E_FAILURE;
                }
            }
        }
        vosStatus = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    return VOS_STATUS_SUCCESS;
}
//Register the module init/exit functions
module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qualcomm Atheros, Inc.");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

module_param_call(con_mode, con_mode_handler, param_get_int, &con_mode,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_call(fwpath, fwpath_changed_handler, param_get_string, &fwpath,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param(enable_dfs_chan_scan, int,
             S_IRUSR | S_IRGRP | S_IROTH);

module_param(enable_11d, int,
             S_IRUSR | S_IRGRP | S_IROTH);

module_param(country_code, charp,
             S_IRUSR | S_IRGRP | S_IROTH);
