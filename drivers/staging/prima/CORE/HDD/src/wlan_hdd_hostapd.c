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

/**========================================================================
  
  \file  wlan_hdd_hostapd.c
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
  06/03/10    js - Added support to hostapd driven deauth/disassoc/mic failure
  ==========================================================================*/
/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
   
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/semaphore.h>
#include <linux/compat.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include <wlan_hdd_hostapd.h>
#include <sapApi.h>
#include <sapInternal.h>
#include <wlan_qct_tl.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_main.h>
#include <linux/netdevice.h>
#include <linux/mmc/sdio_func.h>
#include "wlan_nlink_common.h"
#include "wlan_btc_svc.h"
#include <bap_hdd_main.h>
#include "wlan_hdd_p2p.h"
#include "cfgApi.h"
#include "wniCfgAp.h"

#ifdef FEATURE_WLAN_CH_AVOID
#include "wcnss_wlan.h"
#endif /* FEATURE_WLAN_CH_AVOID */
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"

#define    IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define    IS_UP_AUTO(_ic) \
    (IS_UP((_ic)->ic_dev) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)
#define WE_WLAN_VERSION     1
#define WE_GET_STA_INFO_SIZE 30
/* WEXT limition: MAX allowed buf len for any *
 * IW_PRIV_TYPE_CHAR is 2Kbytes *
 */
#define WE_SAP_MAX_STA_INFO 0x7FF

#define SAP_24GHZ_CH_COUNT (14)

#ifdef FEATURE_WLAN_CH_AVOID
/* Channle/Freqency table */
extern const tRfChannelProps rfChannels[NUM_RF_CHANNELS];
safeChannelType safeChannels[NUM_20MHZ_RF_CHANNELS] =
{
  /*CH  , SAFE, default safe */
    {1  , VOS_TRUE},      //RF_CHAN_1,
    {2  , VOS_TRUE},      //RF_CHAN_2,
    {3  , VOS_TRUE},      //RF_CHAN_3,
    {4  , VOS_TRUE},      //RF_CHAN_4,
    {5  , VOS_TRUE},      //RF_CHAN_5,
    {6  , VOS_TRUE},      //RF_CHAN_6,
    {7  , VOS_TRUE},      //RF_CHAN_7,
    {8  , VOS_TRUE},      //RF_CHAN_8,
    {9  , VOS_TRUE},      //RF_CHAN_9,
    {10 , VOS_TRUE},      //RF_CHAN_10,
    {11 , VOS_TRUE},      //RF_CHAN_11,
    {12 , VOS_TRUE},      //RF_CHAN_12,
    {13 , VOS_TRUE},      //RF_CHAN_13,
    {14 , VOS_TRUE},      //RF_CHAN_14,
    {240, VOS_TRUE},      //RF_CHAN_240,
    {244, VOS_TRUE},      //RF_CHAN_244,
    {248, VOS_TRUE},      //RF_CHAN_248,
    {252, VOS_TRUE},      //RF_CHAN_252,
    {208, VOS_TRUE},      //RF_CHAN_208,
    {212, VOS_TRUE},      //RF_CHAN_212,
    {216, VOS_TRUE},      //RF_CHAN_216,
    {36 , VOS_TRUE},      //RF_CHAN_36,
    {40 , VOS_TRUE},      //RF_CHAN_40,
    {44 , VOS_TRUE},      //RF_CHAN_44,
    {48 , VOS_TRUE},      //RF_CHAN_48,
    {52 , VOS_TRUE},      //RF_CHAN_52,
    {56 , VOS_TRUE},      //RF_CHAN_56,
    {60 , VOS_TRUE},      //RF_CHAN_60,
    {64 , VOS_TRUE},      //RF_CHAN_64,
    {100, VOS_TRUE},      //RF_CHAN_100,
    {104, VOS_TRUE},      //RF_CHAN_104,
    {108, VOS_TRUE},      //RF_CHAN_108,
    {112, VOS_TRUE},      //RF_CHAN_112,
    {116, VOS_TRUE},      //RF_CHAN_116,
    {120, VOS_TRUE},      //RF_CHAN_120,
    {124, VOS_TRUE},      //RF_CHAN_124,
    {128, VOS_TRUE},      //RF_CHAN_128,
    {132, VOS_TRUE},      //RF_CHAN_132,
    {136, VOS_TRUE},      //RF_CHAN_136,
    {140, VOS_TRUE},      //RF_CHAN_140,
    {149, VOS_TRUE},      //RF_CHAN_149,
    {153, VOS_TRUE},      //RF_CHAN_153,
    {157, VOS_TRUE},      //RF_CHAN_157,
    {161, VOS_TRUE},      //RF_CHAN_161,
    {165, VOS_TRUE},      //RF_CHAN_165,
};
#endif /* FEATURE_WLAN_CH_AVOID */

/*---------------------------------------------------------------------------
 *   Function definitions
 *-------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_open() - HDD Open function for hostapd interface

  This is called in response to ifconfig up
  
  \param  - dev Pointer to net_device structure
  
  \return - 0 for success non-zero for failure
              
  --------------------------------------------------------------------------*/
int hdd_hostapd_open (struct net_device *dev)
{
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);

   ENTER();

   if(!test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
   {
       //WMM_INIT OR BSS_START not completed
       hddLog( LOGW, "Ignore hostadp open request");
       EXIT();
       return 0;
   }

   MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                    TRACE_CODE_HDD_HOSTAPD_OPEN_REQUEST, NO_SESSION, 0));
   //Turn ON carrier state
   netif_carrier_on(dev);
   //Enable all Tx queues  
   netif_tx_start_all_queues(dev);
   
   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------
  
  \brief hdd_hostapd_stop() - HDD stop function for hostapd interface
  
  This is called in response to ifconfig down
  
  \param  - dev Pointer to net_device structure
  
  \return - 0 for success non-zero for failure
              
  --------------------------------------------------------------------------*/
int hdd_hostapd_stop (struct net_device *dev)
{
   ENTER();

   if(NULL != dev) {
       //Stop all tx queues
       netif_tx_disable(dev);

       //Turn OFF carrier state
       netif_carrier_off(dev);
   }

   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_uninit() - HDD uninit function

  This is called during the netdev unregister to uninitialize all data
associated with the device

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_hostapd_uninit (struct net_device *dev)
{
   hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);

   ENTER();

   if (pHostapdAdapter && pHostapdAdapter->pHddCtx)
   {
      hdd_deinit_adapter(pHostapdAdapter->pHddCtx, pHostapdAdapter);

      /* after uninit our adapter structure will no longer be valid */
      pHostapdAdapter->dev = NULL;
   }

   EXIT();
}


/**============================================================================
  @brief hdd_hostapd_hard_start_xmit() - Function registered with the Linux OS for 
  transmitting packets. There are 2 versions of this function. One that uses
  locked queue and other that uses lockless queues. Both have been retained to
  do some performance testing
  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device
  
  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
int hdd_hostapd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    return 0;    
}
int hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
    return 0;
}

static int hdd_hostapd_driver_command(hdd_adapter_t *pAdapter,
                                      hdd_priv_data_t *priv_data)
{
   tANI_U8 *command = NULL;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   hdd_scaninfo_t *pScanInfo = NULL;
   int ret = 0;
   int status;
   /*
    * Note that valid pointers are provided by caller
    */

   if (priv_data->total_len <= 0 ||
       priv_data->total_len > HOSTAPD_IOCTL_COMMAND_STRLEN_MAX)
   {
      /* below we allocate one more byte for command buffer.
       * To avoid addition overflow total_len should be
       * smaller than INT_MAX. */
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: integer out of range len %d",
             __func__, priv_data->total_len);
      ret = -EFAULT;
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
   command = kmalloc((priv_data->total_len + 1), GFP_KERNEL);
   if (!command)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to allocate memory", __func__);
      ret = -ENOMEM;
      goto exit;
   }

   if (copy_from_user(command, priv_data->buf, priv_data->total_len))
   {
      ret = -EFAULT;
      goto exit;
   }

   /* Make sure the command is NUL-terminated */
   command[priv_data->total_len] = '\0';

   hddLog(VOS_TRACE_LEVEL_INFO,
          "***HOSTAPD*** : Received %s cmd from Wi-Fi GUI***", command);

   if (strncmp(command, "P2P_SET_NOA", 11) == 0)
   {
      hdd_setP2pNoa(pAdapter->dev, command);
   }
   else if (strncmp(command, "P2P_SET_PS", 10) == 0)
   {
      hdd_setP2pOpps(pAdapter->dev, command);
   }
#ifdef FEATURE_WLAN_BATCH_SCAN
   else if (strncmp(command, "WLS_BATCHING", 12) == 0)
   {
      ret = hdd_handle_batch_scan_ioctl(pAdapter, priv_data, command);
   }
#endif
   else if (strncmp(command, "SET_SAP_CHANNEL_LIST", 20) == 0)
   {
      /*
       * command should be a string having format
       * SET_SAP_CHANNEL_LIST <num channels> <channels seperated by spaces>
       */
      hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: Received Command to Set Preferred Channels for SAP",
             __func__);

      ret = sapSetPreferredChannel(command);
   }
   else if ( strncasecmp(command, "MIRACAST", 8) == 0 )
   {
       hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
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

exit:
   if (command)
   {
      kfree(command);
   }
   return ret;
}

#ifdef CONFIG_COMPAT
static int hdd_hostapd_driver_compat_ioctl(hdd_adapter_t *pAdapter,
                                           struct ifreq *ifr)
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
   ret = hdd_hostapd_driver_command(pAdapter, &priv_data);
 exit:
   return ret;
}
#else /* CONFIG_COMPAT */
static int hdd_hostapd_driver_compat_ioctl(hdd_adapter_t *pAdapter,
                                           struct ifreq *ifr)
{
   /* will never be invoked */
   return 0;
}
#endif /* CONFIG_COMPAT */

static int hdd_hostapd_driver_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
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
      ret = hdd_hostapd_driver_command(pAdapter, &priv_data);
   }
   return ret;
}

static int hdd_hostapd_ioctl(struct net_device *dev,
                             struct ifreq *ifr, int cmd)
{
   hdd_adapter_t *pAdapter;
   hdd_context_t *pHddCtx;
   int ret;

   pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   if (NULL == pAdapter) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: HDD adapter context is Null", __func__);
      ret = -ENODEV;
      goto exit;
   }
   if (dev != pAdapter->dev) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
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
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: invalid context", __func__);
      ret = -EBUSY;
      goto exit;
   }

   switch (cmd) {
   case (SIOCDEVPRIVATE + 1):
      if (is_compat_task())
         ret = hdd_hostapd_driver_compat_ioctl(pAdapter, ifr);
      else
         ret = hdd_hostapd_driver_ioctl(pAdapter, ifr);
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

/**---------------------------------------------------------------------------
  
  \brief hdd_hostapd_set_mac_address() - 
   This function sets the user specified mac address using 
   the command ifconfig wlanX hw ether <mac adress>.
   
  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/

static int hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
   struct sockaddr *psta_mac_addr = addr;
   ENTER();
   memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);
   EXIT();
   return 0;
}
void hdd_hostapd_inactivity_timer_cb(v_PVOID_t usrDataForCallback)
{
    struct net_device *dev = (struct net_device *)usrDataForCallback;
    v_BYTE_t we_custom_event[64];
    union iwreq_data wrqu;
#ifdef DISABLE_CONCURRENCY_AUTOSAVE    
    VOS_STATUS vos_status;
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */

    /* event_name space-delimiter driver_module_name */
    /* Format of the event is "AUTO-SHUT.indication" " " "module_name" */
    char * autoShutEvent = "AUTO-SHUT.indication" " "  KBUILD_MODNAME;
    int event_len = strlen(autoShutEvent) + 1; /* For the NULL at the end */

    ENTER();

#ifdef DISABLE_CONCURRENCY_AUTOSAVE
    if (vos_concurrent_open_sessions_running())
    {  
       /*
              This timer routine is going to be called only when AP
              persona is up.
              If there are concurrent sessions running we do not want
              to shut down the Bss.Instead we run the timer again so
              that if Autosave is enabled next time and other session
              was down only then we bring down AP 
             */
        pHostapdAdapter = netdev_priv(dev);
        pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
        vos_status = vos_timer_start(
         &pHddApCtx->hdd_ap_inactivity_timer, 
         (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff
          * 1000);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            hddLog(LOGE, FL("Failed to init AP inactivity timer"));
        }
        EXIT();
        return;
    }
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */
    memset(&we_custom_event, '\0', sizeof(we_custom_event));
    memcpy(&we_custom_event, autoShutEvent, event_len);

    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = event_len;

    hddLog(LOG1, FL("Shutting down AP interface due to inactivity"));
    wireless_send_event(dev, IWEVCUSTOM, &wrqu, (char *)we_custom_event);    

    EXIT();
}

VOS_STATUS hdd_change_mcc_go_beacon_interval(hdd_adapter_t *pHostapdAdapter)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    ptSapContext  pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: UPDATE Beacon Params", __func__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pVosContext);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_ChangeMCCBeaconInterval(hHal, pSapCtx->sessionId);
        if(halStatus == eHAL_STATUS_FAILURE ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failed to update Beacon Params", __func__);
            return VOS_STATUS_E_FAILURE;
        }
    }
    return VOS_STATUS_SUCCESS;
}

void hdd_clear_all_sta(hdd_adapter_t *pHostapdAdapter, v_PVOID_t usrDataForCallback)
{
    v_U8_t staId = 0;
    struct net_device *dev;
    dev = (struct net_device *)usrDataForCallback;

    hddLog(LOGE, FL("Clearing all the STA entry...."));
    for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
    {
        if ( pHostapdAdapter->aStaInfo[staId].isUsed && 
           ( staId != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId))
        {
            //Disconnect all the stations
            hdd_softap_sta_disassoc(pHostapdAdapter, &pHostapdAdapter->aStaInfo[staId].macAddrSTA.bytes[0]);
        }
    }
}

static int hdd_stop_p2p_link(hdd_adapter_t *pHostapdAdapter,v_PVOID_t usrDataForCallback)
{
    struct net_device *dev;
    hdd_context_t     *pHddCtx = NULL;
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    dev = (struct net_device *)usrDataForCallback;
    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) 
    {
        if ( VOS_STATUS_SUCCESS == (status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext) ) )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("Deleting P2P link!!!!!!"));
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
        wlan_hdd_decr_active_session(pHddCtx, pHostapdAdapter->device_mode);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

VOS_STATUS hdd_hostapd_SAPEventCB( tpSap_Event pSapEvent, v_PVOID_t usrDataForCallback)
{
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
    hdd_hostapd_state_t *pHostapdState;
    struct net_device *dev;
    eSapHddEvent sapEvent;
    union iwreq_data wrqu;
    v_BYTE_t *we_custom_event_generic = NULL;
    int we_event = 0;
    int i = 0;
    v_U8_t staId;
    VOS_STATUS vos_status; 
    v_BOOL_t bWPSState;
    v_BOOL_t bApActive = FALSE;
    v_BOOL_t bAuthRequired = TRUE;
    tpSap_AssocMacAddr pAssocStasArray = NULL;
    char unknownSTAEvent[IW_CUSTOM_MAX+1];
    char maxAssocExceededEvent[IW_CUSTOM_MAX+1];
    v_BYTE_t we_custom_start_event[64];
    char *startBssEvent; 
    hdd_context_t *pHddCtx;
    hdd_scaninfo_t *pScanInfo  = NULL;
    struct iw_michaelmicfailure msg;

    dev = (struct net_device *)usrDataForCallback;
    pHostapdAdapter = netdev_priv(dev);

    if ((NULL == pHostapdAdapter) ||
        (WLAN_HDD_ADAPTER_MAGIC != pHostapdAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "invalid adapter or adapter has invalid magic");
        return eHAL_STATUS_FAILURE;
    }

    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter); 
    pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    sapEvent = pSapEvent->sapHddEventCode;
    memset(&wrqu, '\0', sizeof(wrqu));
    pHddCtx = (hdd_context_t*)(pHostapdAdapter->pHddCtx);

    switch(sapEvent)
    {
        case eSAP_START_BSS_EVENT :
            hddLog(LOG1, FL("BSS configured status = %s, channel = %u, bc sta Id = %d"),
                            pSapEvent->sapevt.sapStartBssCompleteEvent.status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS",
                            pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel,
                              pSapEvent->sapevt.sapStartBssCompleteEvent.staId);

            pHostapdState->vosStatus = pSapEvent->sapevt.sapStartBssCompleteEvent.status;
            vos_status = vos_event_set(&pHostapdState->vosEvent);
   
            if (!VOS_IS_STATUS_SUCCESS(vos_status) || pHostapdState->vosStatus)
            {     
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: startbss event failed!!"));
                goto stopbss;
            }
            else
            {                
                pHddApCtx->uBCStaId = pSapEvent->sapevt.sapStartBssCompleteEvent.staId;
                //@@@ need wep logic here to set privacy bit
                vos_status = hdd_softap_Register_BC_STA(pHostapdAdapter, pHddApCtx->uPrivacy);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGW, FL("Failed to register BC STA %d"), vos_status);
            }
            
            if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
            {
                // AP Inactivity timer init and start
                vos_status = vos_timer_init( &pHddApCtx->hdd_ap_inactivity_timer, VOS_TIMER_TYPE_SW, 
                                            hdd_hostapd_inactivity_timer_cb, (v_PVOID_t)dev );
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer"));

                vos_status = vos_timer_start( &pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer"));

            }
            pHddApCtx->operatingChannel = pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel;
            pHostapdState->bssState = BSS_START;

            // Send current operating channel of SoftAP to BTC-ES
            send_btc_nlink_msg(WLAN_BTC_SOFTAP_BSS_START, 0);

            //Check if there is any group key pending to set.
            if( pHddApCtx->groupKey.keyLength )
            {
                 if( VOS_STATUS_SUCCESS !=  WLANSAP_SetKeySta( 
                               (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                               &pHddApCtx->groupKey ) )
                 {
                      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
                             "%s: WLANSAP_SetKeySta failed", __func__);
                 }
                 pHddApCtx->groupKey.keyLength = 0;
            }
            else if ( pHddApCtx->wepKey[0].keyLength )
            {
                int i=0;
                for ( i = 0; i < CSR_MAX_NUM_KEY; i++ ) 
                {
                    if( VOS_STATUS_SUCCESS !=  WLANSAP_SetKeySta(
                                (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                                &pHddApCtx->wepKey[i] ) )
                    {   
                          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: WLANSAP_SetKeySta failed idx %d", __func__, i);
                    }
                    pHddApCtx->wepKey[i].keyLength = 0;
                }
           }
            //Fill the params for sending IWEVCUSTOM Event with SOFTAP.enabled
            startBssEvent = "SOFTAP.enabled";
            memset(&we_custom_start_event, '\0', sizeof(we_custom_start_event));
            memcpy(&we_custom_start_event, startBssEvent, strlen(startBssEvent));
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = strlen(startBssEvent);
            we_event = IWEVCUSTOM;
            we_custom_event_generic = we_custom_start_event;
            hdd_dump_concurrency_info(pHddCtx);
            break; //Event will be sent after Switch-Case stmt 

        case eSAP_STOP_BSS_EVENT:
            hddLog(LOG1, FL("BSS stop status = %s"),pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
                             "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

            //Free up Channel List incase if it is set
            sapCleanupChannelList();

            pHddApCtx->operatingChannel = 0; //Invalidate the channel info.
            goto stopbss;
        case eSAP_STA_SET_KEY_EVENT:
            //TODO: forward the message to hostapd once implementtation is done for now just print
            hddLog(LOG1, FL("SET Key: configured status = %s"),pSapEvent->sapevt.sapStationSetKeyCompleteEvent.status ?
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");
            return VOS_STATUS_SUCCESS;
        case eSAP_STA_DEL_KEY_EVENT:
           //TODO: forward the message to hostapd once implementtation is done for now just print
           hddLog(LOG1, FL("Event received %s"),"eSAP_STA_DEL_KEY_EVENT");
           return VOS_STATUS_SUCCESS;
        case eSAP_STA_MIC_FAILURE_EVENT:
        {
            memset(&msg, '\0', sizeof(msg));
            msg.src_addr.sa_family = ARPHRD_ETHER;
            memcpy(msg.src_addr.sa_data, &pSapEvent->sapevt.sapStationMICFailureEvent.staMac, sizeof(v_MACADDR_t));
            hddLog(LOG1, "MIC MAC "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(msg.src_addr.sa_data));
            if(pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE)
             msg.flags = IW_MICFAILURE_GROUP;
            else 
             msg.flags = IW_MICFAILURE_PAIRWISE;
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = sizeof(msg);
            we_event = IWEVMICHAELMICFAILURE;
            we_custom_event_generic = (v_BYTE_t *)&msg;
        }
      /* inform mic failure to nl80211 */
        cfg80211_michael_mic_failure(dev, 
                                     pSapEvent->sapevt.
                                     sapStationMICFailureEvent.staMac.bytes,
                                     ((pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE) ? 
                                      NL80211_KEYTYPE_GROUP :
                                      NL80211_KEYTYPE_PAIRWISE),
                                     pSapEvent->sapevt.sapStationMICFailureEvent.keyId, 
                                     pSapEvent->sapevt.sapStationMICFailureEvent.TSC, 
                                     GFP_KERNEL);
            break;
        
        case eSAP_STA_ASSOC_EVENT:
        case eSAP_STA_REASSOC_EVENT:
            wrqu.addr.sa_family = ARPHRD_ETHER;
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac, 
                sizeof(v_MACADDR_t));
            hddLog(LOG1, " associated "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            we_event = IWEVREGISTERED;
            
            WLANSAP_Get_WPS_State((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext, &bWPSState);
         
            if ( (eCSR_ENCRYPT_TYPE_NONE == pHddApCtx->ucEncryptType) ||
                 ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pHddApCtx->ucEncryptType ) || 
                 ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pHddApCtx->ucEncryptType ) )
            {
                bAuthRequired = FALSE;
            }

            if (bAuthRequired || bWPSState == eANI_BOOLEAN_TRUE )
            {
                vos_status = hdd_softap_RegisterSTA( pHostapdAdapter,
                                       TRUE,
                                       pHddApCtx->uPrivacy,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                       0,
                                       0,
                                       (v_MACADDR_t *)wrqu.addr.sa_data,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);

                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGW, FL("Failed to register STA %d "MAC_ADDRESS_STR""),
                                     vos_status, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            }
            else
            {
                vos_status = hdd_softap_RegisterSTA( pHostapdAdapter,
                                       FALSE,
                                       pHddApCtx->uPrivacy,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                       0,
                                       0,
                                       (v_MACADDR_t *)wrqu.addr.sa_data,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGW, FL("Failed to register STA %d "MAC_ADDRESS_STR""),
                                     vos_status, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            }

            // Stop AP inactivity timer
            if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_RUNNING)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to start AP inactivity timer"));
            }
#ifdef WLAN_OPEN_SOURCE
            if (wake_lock_active(&pHddCtx->sap_wake_lock))
            {
               wake_unlock(&pHddCtx->sap_wake_lock);
            }
            wake_lock_timeout(&pHddCtx->sap_wake_lock, msecs_to_jiffies(HDD_SAP_WAKE_LOCK_DURATION));
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
            {
                struct station_info staInfo;
                v_U16_t iesLen =  pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.iesLen;

                memset(&staInfo, 0, sizeof(staInfo));
                if (iesLen <= MAX_ASSOC_IND_IE_LEN )
                {
                    staInfo.assoc_req_ies =
                        (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.ies[0];
                    staInfo.assoc_req_ies_len = iesLen;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,31))
                    staInfo.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
                    cfg80211_new_sta(dev,
                                 (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac.bytes[0],
                                 &staInfo, GFP_KERNEL);
                }
                else
                {
                    hddLog(LOGE, FL(" Assoc Ie length is too long"));
                }
             }
#endif
            pScanInfo =  &pHddCtx->scan_info;
            // Lets do abort scan to ensure smooth authentication for client
            if ((pScanInfo != NULL) && pScanInfo->mScanPending)
            {
                hdd_abort_mac_scan(pHddCtx, pScanInfo->sessionId,
                                   eCSR_SCAN_ABORT_DEFAULT);
            }

            break;
        case eSAP_STA_DISASSOC_EVENT:
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac,
                   sizeof(v_MACADDR_t));
            hddLog(LOG1, " disassociated "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            if (pSapEvent->sapevt.sapStationDisassocCompleteEvent.reason == eSAP_USR_INITATED_DISASSOC)
                hddLog(LOG1," User initiated disassociation");
            else
                hddLog(LOG1," MAC initiated disassociation");
            we_event = IWEVEXPIRED;
            vos_status = hdd_softap_GetStaId(pHostapdAdapter, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac, &staId);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("ERROR: HDD Failed to find sta id!!"));
                return VOS_STATUS_E_FAILURE;
            }
            hdd_softap_DeregisterSTA(pHostapdAdapter, staId);

            if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
            {
                spin_lock_bh( &pHostapdAdapter->staInfo_lock );
                // Start AP inactivity timer if no stations associated with it
                for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
                {
                    if (pHostapdAdapter->aStaInfo[i].isUsed && i != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId)
                    {
                        bApActive = TRUE;
                        break;
                    }
                }
                spin_unlock_bh( &pHostapdAdapter->staInfo_lock );

                if (bApActive == FALSE)
                {
                    if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_STOPPED)
                    {
                        vos_status = vos_timer_start(&pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                        if (!VOS_IS_STATUS_SUCCESS(vos_status))
                            hddLog(LOGE, FL("Failed to init AP inactivity timer"));
                    }
                    else
                        VOS_ASSERT(vos_timer_getCurrentState(&pHddApCtx->hdd_ap_inactivity_timer) == VOS_TIMER_STATE_STOPPED);
                }
            }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
            cfg80211_del_sta(dev,
                            (const u8 *)&pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac.bytes[0],
                            GFP_KERNEL);
#endif
            //Update the beacon Interval if it is P2P GO
            vos_status = hdd_change_mcc_go_beacon_interval(pHostapdAdapter);
            if (VOS_STATUS_SUCCESS != vos_status)
            {
                hddLog(LOGE, "%s: failed to update Beacon interval %d",
                        __func__, vos_status);
            }
            break;
        case eSAP_WPS_PBC_PROBE_REQ_EVENT:
        {
                static const char * message ="MLMEWPSPBCPROBEREQ.indication";
                union iwreq_data wreq;
               
                down(&pHddApCtx->semWpsPBCOverlapInd);
                pHddApCtx->WPSPBCProbeReq.probeReqIELen = pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIELen;
                
                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.probeReqIE, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIE, 
                    pHddApCtx->WPSPBCProbeReq.probeReqIELen);
                     
                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.peerMacAddr, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.peerMacAddr, sizeof(v_MACADDR_t));
                hddLog(LOG1, "WPS PBC probe req "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pHddApCtx->WPSPBCProbeReq.peerMacAddr));
                memset(&wreq, 0, sizeof(wreq));
                wreq.data.length = strlen(message); // This is length of message
                wireless_send_event(dev, IWEVCUSTOM, &wreq, (char *)message); 
                
                return VOS_STATUS_SUCCESS;
        }
        case eSAP_ASSOC_STA_CALLBACK_EVENT:
            pAssocStasArray = pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas;
            if (pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta != 0)
            {   // List of associated stations
                for (i = 0; i < pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta; i++)
                {
                    hddLog(LOG1,"Associated Sta Num %d:assocId=%d, staId=%d, staMac="MAC_ADDRESS_STR,
                        i+1,
                        pAssocStasArray->assocId,
                        pAssocStasArray->staId,
                                    MAC_ADDR_ARRAY(pAssocStasArray->staMac.bytes));
                        pAssocStasArray++;             
            }
            }
            vos_mem_free(pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas);// Release caller allocated memory here
            pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas = NULL;
            return VOS_STATUS_SUCCESS;
        case eSAP_INDICATE_MGMT_FRAME:
           hdd_indicateMgmtFrame( pHostapdAdapter, 
                                 pSapEvent->sapevt.sapManagementFrameInfo.nFrameLength,
                                 pSapEvent->sapevt.sapManagementFrameInfo.pbFrames,
                                 pSapEvent->sapevt.sapManagementFrameInfo.frameType, 
                                 pSapEvent->sapevt.sapManagementFrameInfo.rxChan, 0);
           return VOS_STATUS_SUCCESS;
        case eSAP_REMAIN_CHAN_READY:
           hdd_remainChanReadyHandler( pHostapdAdapter );
           return VOS_STATUS_SUCCESS;
        case eSAP_SEND_ACTION_CNF:
           hdd_sendActionCnf( pHostapdAdapter, 
                              ( eSAP_STATUS_SUCCESS == 
                                pSapEvent->sapevt.sapActionCnf.actionSendSuccess ) ? 
                                TRUE : FALSE );
           return VOS_STATUS_SUCCESS;
        case eSAP_UNKNOWN_STA_JOIN:
            snprintf(unknownSTAEvent, IW_CUSTOM_MAX, "JOIN_UNKNOWN_STA-%02x:%02x:%02x:%02x:%02x:%02x",
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[0],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[1],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[2],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[3],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[4],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
            wrqu.data.pointer = unknownSTAEvent;
            wrqu.data.length = strlen(unknownSTAEvent);
            we_custom_event_generic = (v_BYTE_t *)unknownSTAEvent;
            hddLog(LOGE,"%s", unknownSTAEvent);
            break;

        case eSAP_MAX_ASSOC_EXCEEDED:
            snprintf(maxAssocExceededEvent, IW_CUSTOM_MAX, "Peer %02x:%02x:%02x:%02x:%02x:%02x denied"
                    " assoc due to Maximum Mobile Hotspot connections reached. Please disconnect"
                    " one or more devices to enable the new device connection",
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[0],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[1],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[2],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[3],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[4],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
            wrqu.data.pointer = maxAssocExceededEvent;
            wrqu.data.length = strlen(maxAssocExceededEvent);
            we_custom_event_generic = (v_BYTE_t *)maxAssocExceededEvent;
            hddLog(LOG1,"%s", maxAssocExceededEvent);
            break;
        case eSAP_STA_ASSOC_IND:
            return VOS_STATUS_SUCCESS;

        case eSAP_DISCONNECT_ALL_P2P_CLIENT:
            hddLog(LOG1, FL(" Disconnecting all the P2P Clients...."));
            hdd_clear_all_sta(pHostapdAdapter, usrDataForCallback);
            return VOS_STATUS_SUCCESS;

        case eSAP_MAC_TRIG_STOP_BSS_EVENT :
            vos_status = hdd_stop_p2p_link(pHostapdAdapter, usrDataForCallback);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                hddLog(LOGW, FL("hdd_stop_p2p_link failed %d"), vos_status);
            }
            return VOS_STATUS_SUCCESS;

        default:
            hddLog(LOG1,"SAP message is not handled");
            goto stopbss;
            return VOS_STATUS_SUCCESS;
    }
    wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
    return VOS_STATUS_SUCCESS;

stopbss :
    {
        v_BYTE_t we_custom_event[64];
        char *stopBssEvent = "STOP-BSS.response";//17
        int event_len = strlen(stopBssEvent);

        hddLog(LOG1, FL("BSS stop status = %s"),
               pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

        /* Change the BSS state now since, as we are shutting things down,
         * we don't want interfaces to become re-enabled */
        pHostapdState->bssState = BSS_STOP;

        if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
        {
            if (VOS_TIMER_STATE_RUNNING == pHddApCtx->hdd_ap_inactivity_timer.state)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGE, FL("Failed to stop AP inactivity timer"));
            }

            vos_status = vos_timer_destroy(&pHddApCtx->hdd_ap_inactivity_timer);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
                hddLog(LOGE, FL("Failed to Destroy AP inactivity timer"));
        }

        /* Stop the pkts from n/w stack as we are going to free all of
         * the TX WMM queues for all STAID's */
        hdd_hostapd_stop(dev);

        /* reclaim all resources allocated to the BSS */
        vos_status = hdd_softap_stop_bss(pHostapdAdapter);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
             hddLog(LOGW, FL("hdd_softap_stop_bss failed %d"), vos_status);

        /* once the event is set, structure dev/pHostapdAdapter should
         * not be touched since they are now subject to being deleted
         * by another thread */
        if (eSAP_STOP_BSS_EVENT == sapEvent)
            vos_event_set(&pHostapdState->vosEvent);

        /* notify userspace that the BSS has stopped */
        memset(&we_custom_event, '\0', sizeof(we_custom_event));
        memcpy(&we_custom_event, stopBssEvent, event_len);
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = event_len;
        we_event = IWEVCUSTOM;
        we_custom_event_generic = we_custom_event;
        wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
        hdd_dump_concurrency_info(pHddCtx);
    }
    return VOS_STATUS_SUCCESS;
}

int hdd_softap_unpackIE(
                tHalHandle halHandle,
                eCsrEncryptionType *pEncryptType,
                eCsrEncryptionType *mcEncryptType,
                eCsrAuthType *pAuthType,
                v_BOOL_t *pMFPCapable,
                v_BOOL_t *pMFPRequired,
                u_int16_t gen_ie_len,
                u_int8_t *gen_ie )
{
    tDot11fIERSN dot11RSNIE; 
    tDot11fIEWPA dot11WPAIE; 
 
    tANI_U8 *pRsnIe; 
    tANI_U16 RSNIeLen;
    
    if (NULL == halHandle)
    {
        hddLog(LOGE, FL("Error haHandle returned NULL"));
        return -EINVAL;
    }
    
    // Validity checks
    if ((gen_ie_len < VOS_MIN(DOT11F_IE_RSN_MIN_LEN, DOT11F_IE_WPA_MIN_LEN)) ||  
        (gen_ie_len > VOS_MAX(DOT11F_IE_RSN_MAX_LEN, DOT11F_IE_WPA_MAX_LEN)) ) 
        return -EINVAL;
    // Type check
    if ( gen_ie[0] ==  DOT11F_EID_RSN) 
    {         
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_RSN_MIN_LEN ) ||  
            (gen_ie_len > DOT11F_IE_RSN_MAX_LEN) )
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte  
        pRsnIe = gen_ie + 2; 
        RSNIeLen = gen_ie_len - 2; 
        // Unpack the RSN IE
        memset(&dot11RSNIE, 0, sizeof(tDot11fIERSN));
        dot11fUnpackIeRSN((tpAniSirGlobal) halHandle, 
                            pRsnIe, 
                            RSNIeLen, 
                            &dot11RSNIE);
        // Copy out the encryption and authentication types 
        hddLog(LOG1, FL("%s: pairwise cipher suite count: %d"),
                __func__, dot11RSNIE.pwise_cipher_suite_count );
        hddLog(LOG1, FL("%s: authentication suite count: %d"),
                __func__, dot11RSNIE.akm_suite_count);
        /*Here we have followed the apple base code, 
          but probably I suspect we can do something different*/
        //dot11RSNIE.akm_suite_count
        // Just translate the FIRST one 
        *pAuthType =  hdd_TranslateRSNToCsrAuthType(dot11RSNIE.akm_suites[0]); 
        //dot11RSNIE.pwise_cipher_suite_count 
        *pEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.pwise_cipher_suites[0]);                     
        //dot11RSNIE.gp_cipher_suite_count 
        *mcEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.gp_cipher_suite);                     
        // Set the PMKSA ID Cache for this interface
        *pMFPCapable = 0 != (dot11RSNIE.RSN_Cap[0] & 0x80);
        *pMFPRequired = 0 != (dot11RSNIE.RSN_Cap[0] & 0x40);
          
        // Calling csrRoamSetPMKIDCache to configure the PMKIDs into the cache
    } else 
    if (gen_ie[0] == DOT11F_EID_WPA) 
    {         
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_WPA_MIN_LEN ) ||  
            (gen_ie_len > DOT11F_IE_WPA_MAX_LEN))
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte - and four byte WiFi OUI  
        pRsnIe = gen_ie + 2 + 4; 
        RSNIeLen = gen_ie_len - (2 + 4); 
        // Unpack the WPA IE
        memset(&dot11WPAIE, 0, sizeof(tDot11fIEWPA));
        dot11fUnpackIeWPA((tpAniSirGlobal) halHandle, 
                            pRsnIe, 
                            RSNIeLen, 
                            &dot11WPAIE);
        // Copy out the encryption and authentication types 
        hddLog(LOG1, FL("%s: WPA unicast cipher suite count: %d"),
                __func__, dot11WPAIE.unicast_cipher_count );
        hddLog(LOG1, FL("%s: WPA authentication suite count: %d"),
                __func__, dot11WPAIE.auth_suite_count);
        //dot11WPAIE.auth_suite_count
        // Just translate the FIRST one 
        *pAuthType =  hdd_TranslateWPAToCsrAuthType(dot11WPAIE.auth_suites[0]); 
        //dot11WPAIE.unicast_cipher_count 
        *pEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.unicast_ciphers[0]);                       
        //dot11WPAIE.unicast_cipher_count 
        *mcEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.multicast_cipher);                       
        *pMFPCapable = VOS_FALSE;
        *pMFPRequired = VOS_FALSE;
    } 
    else 
    { 
        hddLog(LOGW, FL("%s: gen_ie[0]: %d"), __func__, gen_ie[0]);
        return VOS_STATUS_E_FAILURE; 
    }
    return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_CH_AVOID
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_freq_to_chn() -

  Input frequency translated into channel number

  \param  - freq input frequency with order of kHz

  \return - corresponding channel number.
            incannot find correct channel number, return 0

  --------------------------------------------------------------------------*/
v_U16_t hdd_hostapd_freq_to_chn
(
   v_U16_t   freq
)
{
   int   loop;

   for (loop = 0; loop < NUM_20MHZ_RF_CHANNELS; loop++)
   {
      if (rfChannels[loop].targetFreq == freq)
      {
         return rfChannels[loop].channelNum;
      }
   }

   return (0);
}

/*==========================================================================
  FUNCTION    sapUpdateUnsafeChannelList

  DESCRIPTION
    Function  Undate unsafe channel list table

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pSapCtx : SAP context pointer, include unsafe channel list

  RETURN VALUE
    NONE
============================================================================*/
void hdd_hostapd_update_unsafe_channel_list(hdd_context_t *pHddCtx,
                        v_U16_t *unsafeChannelList, v_U16_t unsafeChannelCount)
{
   v_U16_t   i, j;

   vos_mem_zero((void *)pHddCtx->unsafeChannelList,
                sizeof(pHddCtx->unsafeChannelList));
   if (0 == unsafeChannelCount)
   {
      pHddCtx->unsafeChannelCount = 0;
   }
   else
   {
      if (unsafeChannelCount > NUM_20MHZ_RF_CHANNELS)
      {
          VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    FL("unsafeChannelCount%hd greater than %d"),
                        unsafeChannelCount, NUM_20MHZ_RF_CHANNELS);
          unsafeChannelCount = NUM_20MHZ_RF_CHANNELS;
      }
      vos_mem_copy((void *)pHddCtx->unsafeChannelList,
                   unsafeChannelList,
                   unsafeChannelCount * sizeof(tANI_U16));
      pHddCtx->unsafeChannelCount = unsafeChannelCount;
   }

   /* Flush, default set all channel safe */
   for (i = 0; i < NUM_20MHZ_RF_CHANNELS; i++)
   {
      safeChannels[i].isSafe = VOS_TRUE;
   }

   /* Try to find unsafe channel */
   for (i = 0; i < pHddCtx->unsafeChannelCount; i++)
   {
      for (j = 0; j < NUM_20MHZ_RF_CHANNELS; j++)
      {
         if(safeChannels[j].channelNumber == pHddCtx->unsafeChannelList[i])
         {
            /* Found unsafe channel, update it */
            safeChannels[j].isSafe = VOS_FALSE;
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                      "%s : CH %d is not safe",
                      __func__, pHddCtx->unsafeChannelList[i]);
            break;
         }
      }
   }

   return;
}

/**---------------------------------------------------------------------------
  \brief hdd_restart_softap() -
   Restart SAP  on STA channel to support
   STA + SAP concurrency.

  --------------------------------------------------------------------------*/
void hdd_restart_softap
(
   hdd_context_t *pHddCtx,
   hdd_adapter_t *pHostapdAdapter
)
{
   tSirChAvoidIndType *chAvoidInd;

   chAvoidInd =
         (tSirChAvoidIndType *)vos_mem_malloc(sizeof(tSirChAvoidIndType));
   if (NULL == chAvoidInd)
   {
       hddLog(VOS_TRACE_LEVEL_INFO, FL("CH_AVOID IND buffer alloc Fail"));
       return ;
   }
   chAvoidInd->avoidRangeCount = 1;
   chAvoidInd->avoidFreqRange[0].startFreq =
    vos_chan_to_freq(pHostapdAdapter->sessionCtx.ap.operatingChannel);
   chAvoidInd->avoidFreqRange[0].endFreq =
     vos_chan_to_freq(pHostapdAdapter->sessionCtx.ap.operatingChannel);
   hdd_hostapd_ch_avoid_cb((void *)pHddCtx, (void *)chAvoidInd);
   vos_mem_free(chAvoidInd);
}
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_ch_avoid_cb() -

  Avoid channel notification from FW handler.
  FW will send un-safe channle list to avoid overwrapping.
  hostapd should not use notified channel

  \param  - pAdapter HDD adapter pointer
            indParam channel avoid notification parameter

  \return - None

  --------------------------------------------------------------------------*/
void hdd_hostapd_ch_avoid_cb
(
   void *pAdapter,
   void *indParam
)
{
   hdd_adapter_t      *pHostapdAdapter = NULL;
   hdd_context_t      *hddCtxt;
   tSirChAvoidIndType *chAvoidInd;
   v_U8_t              rangeLoop;
   v_U16_t             channelLoop;
   v_U16_t             dupCheck;
   v_U16_t             startChannel;
   v_U16_t             endChannel;
   v_U16_t             unsafeChannelCount = 0;
   v_U16_t             unsafeChannelList[NUM_20MHZ_RF_CHANNELS];
   v_CONTEXT_t         pVosContext;
   tHddAvoidFreqList   hddAvoidFreqList;
   tANI_U32            i;

   /* Basic sanity */
   if ((NULL == pAdapter) || (NULL == indParam))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s : Invalid arguments", __func__);
      return;
   }

   hddCtxt     = (hdd_context_t *)pAdapter;
   chAvoidInd  = (tSirChAvoidIndType *)indParam;
   pVosContext = hddCtxt->pvosContext;

   /* Make unsafe channel list */
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s : band count %d",
             __func__, chAvoidInd->avoidRangeCount);
   vos_mem_zero((void *)unsafeChannelList,
                NUM_20MHZ_RF_CHANNELS * sizeof(v_U16_t));
   for (rangeLoop = 0; rangeLoop < chAvoidInd->avoidRangeCount; rangeLoop++)
   {
      startChannel = hdd_hostapd_freq_to_chn(
                      chAvoidInd->avoidFreqRange[rangeLoop].startFreq);
      endChannel   = hdd_hostapd_freq_to_chn(
                      chAvoidInd->avoidFreqRange[rangeLoop].endFreq);
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s : start %d : %d, end %d : %d",
                __func__,
                chAvoidInd->avoidFreqRange[rangeLoop].startFreq,
                startChannel,
                chAvoidInd->avoidFreqRange[rangeLoop].endFreq,
                endChannel);
      for (channelLoop = startChannel;
           channelLoop < (endChannel + 1);
           channelLoop++)
      {
         /* Channel duplicate check routine */
         for (dupCheck = 0; dupCheck < unsafeChannelCount; dupCheck++)
         {
            if (unsafeChannelList[dupCheck] == channelLoop)
            {
               /* This channel is duplicated */
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s : found duplicated channel %d",
                      __func__, channelLoop);
               break;
            }
         }
         if (dupCheck == unsafeChannelCount)
         {
             int ii;
             for(ii=0; ii<NUM_20MHZ_RF_CHANNELS; ii++)
             {
                 if (channelLoop == safeChannels[ii].channelNumber)
                 {
                     unsafeChannelList[unsafeChannelCount] = channelLoop;
                     unsafeChannelCount++;
                     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "%s : unsafe channel %d, count %d",
                           __func__,
                           channelLoop, unsafeChannelCount);
                 }
             }
         }
         else
         {
            /* DUP, do nothing */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s : duplicated channel %d",
                      __func__, channelLoop);
         }
      }
   }
   /* Update unsafe channel cache
    * WCN Platform Driver cache */
   wcnss_set_wlan_unsafe_channel(unsafeChannelList,
                                 unsafeChannelCount);

   /* Store into local cache
    * Start with STA and later start SAP
    * in this scenario, local cache will be used */
   hdd_hostapd_update_unsafe_channel_list(hddCtxt,
                                          unsafeChannelList,
                                          unsafeChannelCount);

   /* generate vendor specific event */
   vos_mem_zero((void *)&hddAvoidFreqList, sizeof(tHddAvoidFreqList));
   for (i = 0; i < chAvoidInd->avoidRangeCount; i++)
   {
      hddAvoidFreqList.avoidFreqRange[i].startFreq =
            chAvoidInd->avoidFreqRange[i].startFreq;
      hddAvoidFreqList.avoidFreqRange[i].endFreq =
            chAvoidInd->avoidFreqRange[i].endFreq;
   }
   hddAvoidFreqList.avoidFreqRangeCount = chAvoidInd->avoidRangeCount;

   wlan_hdd_send_avoid_freq_event(hddCtxt, &hddAvoidFreqList);

   /* Get SAP context first
    * SAP and P2PGO would not concurrent */
   pHostapdAdapter = hdd_get_adapter(hddCtxt, WLAN_HDD_SOFTAP);
   if ((pHostapdAdapter) &&
       (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) &&
       (unsafeChannelCount))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s : Current operation channel %d",
                __func__,
                pHostapdAdapter->sessionCtx.ap.operatingChannel);
      for (channelLoop = 0; channelLoop < unsafeChannelCount; channelLoop++)
      {
         if (((unsafeChannelList[channelLoop] ==
               pHostapdAdapter->sessionCtx.ap.operatingChannel)) &&
             (AUTO_CHANNEL_SELECT ==
               pHostapdAdapter->sessionCtx.ap.sapConfig.channel))
         {
            /* current operating channel is un-safe channel
             * restart driver */
            hdd_hostapd_stop(pHostapdAdapter->dev);
            /* On LE, this event is handled by wlan-services to restart SAP.
               On android, this event would be ignored. */
            wlan_hdd_send_svc_nlink_msg(WLAN_SVC_SAP_RESTART_IND, NULL, 0);
            break;
         }
      }
   }

   return;
}

#endif /* FEATURE_WLAN_CH_AVOID */

int
static __iw_softap_setparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal;
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int set_value = value[1];
    eHalStatus status;
    int ret = 0; /* success */
    v_CONTEXT_t pVosContext;

    if (!pHostapdAdapter || !pHostapdAdapter->pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: either hostapd Adapter is null or HDD ctx is null",
                  __func__);
        return -1;
    }

    hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    if (!hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Hal ctx is null", __func__);
        return -1;
    }

    pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    if (!pVosContext)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Vos ctx is null", __func__);
        return -1;
    }

    switch(sub_cmd)
    {

        case QCSAP_PARAM_CLR_ACL:
            if ( VOS_STATUS_SUCCESS != WLANSAP_ClearACL( pVosContext ))
            {
               ret = -EIO;            
            }
            break;

        case QCSAP_PARAM_ACL_MODE:
            if ((eSAP_ALLOW_ALL < (eSapMacAddrACL)set_value) || 
                (eSAP_ACCEPT_UNLESS_DENIED > (eSapMacAddrACL)set_value))
            {
                hddLog(LOGE, FL("Invalid ACL Mode value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                WLANSAP_SetMode(pVosContext, set_value);
            }
            break;

        case QCSAP_PARAM_SET_AUTO_CHANNEL:
            if ((0 != set_value) && (1 != set_value))
            {
                hddLog(LOGE, FL("Invalid setAutoChannel value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apAutoChannelSelection = set_value;
            }
            break;

        case QCSAP_PARAM_MAX_ASSOC:
            if (WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value)
            {
                hddLog(LOGE, FL("Invalid setMaxAssoc value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                if (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value)
                {
                    hddLog(LOGW, FL("setMaxAssoc value %d higher than max allowed %d."
                                "Setting it to max allowed and continuing"),
                                set_value, WNI_CFG_ASSOC_STA_LIMIT_STAMAX);
                    set_value = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
                }
                status = ccmCfgSetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT,
                                      set_value, NULL, eANI_BOOLEAN_FALSE);
                if ( status != eHAL_STATUS_SUCCESS ) 
                {
                    hddLog(LOGE, FL("setMaxAssoc failure, status %d"),
                            status);
                    ret = -EIO;
                }
            }
            break;

        case QCSAP_PARAM_HIDE_SSID:
            {
                eHalStatus status = eHAL_STATUS_SUCCESS;
                status = sme_HideSSID(hHal, pHostapdAdapter->sessionId, set_value);
                if(eHAL_STATUS_SUCCESS != status)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: QCSAP_PARAM_HIDE_SSID failed",
                            __func__);
                    return status;
                }
                break;
            }

        case QCSAP_PARAM_SET_MC_RATE:
            {
                tSirRateUpdateInd *rateUpdate;

                rateUpdate = (tSirRateUpdateInd *)
                             vos_mem_malloc(sizeof(tSirRateUpdateInd));
                if (NULL == rateUpdate)
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: SET_MC_RATE indication alloc fail", __func__);
                   ret = -1;
                   break;
                }
                vos_mem_zero(rateUpdate, sizeof(tSirRateUpdateInd ));

                hddLog(VOS_TRACE_LEVEL_INFO, "MC Target rate %d", set_value);
                /* Ignore unicast */
                rateUpdate->ucastDataRate = -1;
                rateUpdate->mcastDataRate24GHz = set_value;
                rateUpdate->mcastDataRate5GHz = set_value;
                rateUpdate->mcastDataRate24GHzTxFlag = 0;
                rateUpdate->mcastDataRate5GHzTxFlag = 0;
                status = sme_SendRateUpdateInd(hHal, rateUpdate);
                if (eHAL_STATUS_SUCCESS != status)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: SET_MC_RATE failed", __func__);
                    vos_mem_free(rateUpdate);
                    ret = -1;
                }
                break;
            }

        default:
            hddLog(LOGE, FL("Invalid setparam command %d value %d"),
                    sub_cmd, set_value);
            ret = -EINVAL;
            break;
    }

    return ret;
}

int
static iw_softap_setparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_setparam(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

int
static __iw_softap_getparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    eHalStatus status;
    int ret = 0; /* success */
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 

    switch (sub_cmd)
    {
    case QCSAP_PARAM_MAX_ASSOC:
        status = ccmCfgGetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT, (tANI_U32 *)value);
        if (eHAL_STATUS_SUCCESS != status)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to get WNI_CFG_ASSOC_STA_LIMIT from cfg %d"),status);
            ret = -EIO;
        }

#ifdef WLAN_SOFTAP_VSTA_FEATURE
        if (pHddCtx->cfg_ini->fEnableVSTASupport)
        {
            if (*value > VSTA_NUM_ASSOC_STA)
            {
                *value = VSTA_NUM_ASSOC_STA;
            }
            if ((pHddCtx->hddAdapters.count > VSTA_NUM_RESV_SELFSTA) &&
                (*value > (VSTA_NUM_ASSOC_STA -
                          (pHddCtx->hddAdapters.count - VSTA_NUM_RESV_SELFSTA))))
            {
                *value = (VSTA_NUM_ASSOC_STA -
                         (pHddCtx->hddAdapters.count - VSTA_NUM_RESV_SELFSTA));
            }
        }
        else
#endif
        {
            if (*value > NUM_ASSOC_STA)
            {
                *value = NUM_ASSOC_STA;
            }
            if ((pHddCtx->hddAdapters.count > NUM_RESV_SELFSTA) &&
                (*value > (NUM_ASSOC_STA -
                          (pHddCtx->hddAdapters.count - NUM_RESV_SELFSTA))))
            {
                *value = (NUM_ASSOC_STA -
                         (pHddCtx->hddAdapters.count - NUM_RESV_SELFSTA));
            }
        }
        break;
        
    case QCSAP_PARAM_CLR_ACL:
        if ( VOS_STATUS_SUCCESS != WLANSAP_ClearACL( pVosContext ))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("WLANSAP_ClearACL failed"));
               ret = -EIO;            
        }               
        *value = 0;
        break;
        
    case QCSAP_PARAM_GET_WLAN_DBG:
        {
            vos_trace_display();
            *value = 0;
            break;
        }

    case QCSAP_PARAM_AUTO_CHANNEL:
        {
            *value = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apAutoChannelSelection;
             break;
        }

    default:
        hddLog(LOGE, FL("Invalid getparam command %d"), sub_cmd);
        ret = -EINVAL;
        break;

    }

    return ret;
}

int
static iw_softap_getparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_getparam(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}
/* Usage:
    BLACK_LIST  = 0
    WHITE_LIST  = 1 
    ADD MAC = 0
    REMOVE MAC  = 1

    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 modify_acl 
    <6 octet mac addr> <list type> <cmd type>

    Examples:
    eg 1. to add a mac addr 00:0a:f5:89:89:90 to the black list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 0 0
    eg 2. to delete a mac addr 00:0a:f5:89:89:90 from white list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 1 1
*/
int __iw_softap_modify_acl(struct net_device *dev,
                         struct iw_request_info *info,
                         union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 
    v_BYTE_t *value = (v_BYTE_t*)extra;
    v_U8_t pPeerStaMac[VOS_MAC_ADDR_SIZE];
    int listType, cmd, i;
    int ret = 0; /* success */

    ENTER();
    for (i=0; i<VOS_MAC_ADDR_SIZE; i++)
    {
        pPeerStaMac[i] = *(value+i);
    }
    listType = (int)(*(value+i));
    i++;
    cmd = (int)(*(value+i));

    hddLog(LOG1, "%s: SAP Modify ACL arg0 " MAC_ADDRESS_STR " arg1 %d arg2 %d",
            __func__, MAC_ADDR_ARRAY(pPeerStaMac), listType, cmd);

    if (WLANSAP_ModifyACL(pVosContext, pPeerStaMac,(eSapACLType)listType,(eSapACLCmdType)cmd)
            != VOS_STATUS_SUCCESS)
    {
        hddLog(LOGE, FL("Modify ACL failed"));
        ret = -EIO;
    }
    EXIT();
    return ret;
}

int iw_softap_modify_acl(struct net_device *dev,
                         struct iw_request_info *info,
                         union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_modify_acl(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

int
static __iw_softap_getchannel(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    int *value = (int *)extra;

    *value = (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->operatingChannel;
    return 0;
}


int
static iw_softap_getchannel(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_getchannel(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

int
static __iw_softap_set_max_tx_power(struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    if (NULL == value)
        return -ENOMEM;

    /* Assign correct slef MAC address */
    vos_mem_copy(bssid, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);
    vos_mem_copy(selfMac, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetMaxTxPower(hHal, bssid, selfMac, set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting maximum tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

int
static iw_softap_set_max_tx_power(struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_set_max_tx_power(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}


int
static __iw_display_data_path_snapshot(struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu, char *extra)
{

    /* Function intitiating dumping states of
     *  HDD(WMM Tx Queues)
     *  TL State (with Per Client infor)
     *  DXE Snapshot (Called at the end of TL Snapshot)
     */
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hddLog(LOGE, "%s: called for SAP",__func__);
    hdd_wmm_tx_snapshot(pHostapdAdapter);
    WLANTL_TLDebugMessage(WLANTL_DEBUG_TX_SNAPSHOT);
    return 0;
}

int
static iw_display_data_path_snapshot(struct net_device *dev,
                                     struct iw_request_info *info,
                                     union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_display_data_path_snapshot(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

int
static __iw_softap_set_tx_power(struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    ptSapContext  pSapCtx = NULL;

    if (NULL == value)
        return -ENOMEM;

    pSapCtx = VOS_GET_SAP_CB(pVosContext);
    if (NULL == pSapCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetTxPower(hHal, pSapCtx->sessionId, set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

int
static iw_softap_set_tx_power(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_set_tx_power(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

/**---------------------------------------------------------------------------

  \brief iw_softap_set_trafficmonitor() -
   This function dynamically enable/disable traffic monitor functonality
   the command iwpriv wlanX setTrafficMon <value>.

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int __iw_softap_set_trafficmonitor(struct net_device *dev,
                                          struct iw_request_info *info,
                                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int *isSetTrafficMon = (int *)extra;
    hdd_context_t *pHddCtx;
    int status;

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s : ", __func__);

    if (NULL == isSetTrafficMon)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from extra", __func__);
        return -ENOMEM;
    }

    if (TRUE == *isSetTrafficMon)
    {
        pHddCtx->cfg_ini->enableTrafficMonitor= TRUE;
        if (VOS_STATUS_SUCCESS != hdd_start_trafficMonitor(pAdapter))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: failed to Start Traffic Monitor timer ", __func__ );
            return -EIO;
        }
    }
    else if (FALSE == *isSetTrafficMon)
    {
        pHddCtx->cfg_ini->enableTrafficMonitor= FALSE;
        if (VOS_STATUS_SUCCESS != hdd_stop_trafficMonitor(pAdapter))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: failed to Stop Traffic Monitor timer ", __func__ );
            return -EIO;
        }

    }
    return 0;
}

static int iw_softap_set_trafficmonitor(struct net_device *dev,
                                        struct iw_request_info *info,
                                        union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_set_trafficmonitor(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

#define IS_BROADCAST_MAC(x) (((x[0] & x[1] & x[2] & x[3] & x[4] & x[5]) == 0xff) ? 1 : 0)

int
static __iw_softap_getassoc_stamacaddr(struct net_device *dev,
                                     struct iw_request_info *info,
                                     union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hdd_station_info_t *pStaInfo = pHostapdAdapter->aStaInfo;
    char *buf;
    int cnt = 0;
    int left;
    int ret = 0;
    /* maclist_index must be u32 to match userspace */
    u32 maclist_index;

    /*
     * NOTE WELL: this is a "get" ioctl but it uses an even ioctl
     * number, and even numbered iocts are supposed to have "set"
     * semantics.  Hence the wireless extensions support in the kernel
     * won't correctly copy the result to userspace, so the ioctl
     * handler itself must copy the data.  Output format is 32-bit
     * record length, followed by 0 or more 6-byte STA MAC addresses.
     *
     * Further note that due to the incorrect semantics, the "iwpriv"
     * userspace application is unable to correctly invoke this API,
     * hence it is not registered in the hostapd_private_args.  This
     * API can only be invoked by directly invoking the ioctl() system
     * call.
     */

    /* make sure userspace allocated a reasonable buffer size */
    if (wrqu->data.length < sizeof(maclist_index)) {
        hddLog(LOG1, "%s: invalid userspace buffer", __func__);
        return -EINVAL;
    }

    /* allocate local buffer to build the response */
    buf = kmalloc(wrqu->data.length, GFP_KERNEL);
    if (!buf) {
        hddLog(LOG1, "%s: failed to allocate response buffer", __func__);
        return -ENOMEM;
    }

    /* start indexing beyond where the record count will be written */
    maclist_index = sizeof(maclist_index);
    left = wrqu->data.length - maclist_index;

    spin_lock_bh(&pHostapdAdapter->staInfo_lock);
    while ((cnt < WLAN_MAX_STA_COUNT) && (left >= VOS_MAC_ADDR_SIZE)) {
        if ((pStaInfo[cnt].isUsed) &&
            (!IS_BROADCAST_MAC(pStaInfo[cnt].macAddrSTA.bytes))) {
            memcpy(&buf[maclist_index], &(pStaInfo[cnt].macAddrSTA),
                   VOS_MAC_ADDR_SIZE);
            maclist_index += VOS_MAC_ADDR_SIZE;
            left -= VOS_MAC_ADDR_SIZE;
        }
        cnt++;
    }
    spin_unlock_bh(&pHostapdAdapter->staInfo_lock);

    *((u32 *)buf) = maclist_index;
    wrqu->data.length = maclist_index;
    if (copy_to_user(wrqu->data.pointer, buf, maclist_index)) {
        hddLog(LOG1, "%s: failed to copy response to user buffer", __func__);
        ret = -EFAULT;
    }
    kfree(buf);
    return ret;
}

int
static iw_softap_getassoc_stamacaddr(struct net_device *dev,
                                     struct iw_request_info *info,
                                     union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_getassoc_stamacaddr(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

/* Usage:
    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 disassoc_sta <6 octet mac address>

    e.g.
    disassociate sta with mac addr 00:0a:f5:11:22:33 from softap
    iwpriv softap.0 disassoc_sta 0x00 0x0a 0xf5 0x11 0x22 0x33
*/

int
static __iw_softap_disassoc_sta(struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U8_t *peerMacAddr;    
    
    ENTER();
    /* iwpriv tool or framework calls this ioctl with
     * data passed in extra (less than 16 octets);
     */
    peerMacAddr = (v_U8_t *)(extra);

    hddLog(LOG1, "%s data "  MAC_ADDRESS_STR,
           __func__, MAC_ADDR_ARRAY(peerMacAddr));
    hdd_softap_sta_disassoc(pHostapdAdapter, peerMacAddr);
    EXIT();
    return 0;
}

int
static iw_softap_disassoc_sta(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_disassoc_sta(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

int
static __iw_softap_ap_stats(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    WLANTL_TRANSFER_STA_TYPE  statBuffer;
    char *pstatbuf;
    int len;

    memset(&statBuffer, 0, sizeof(statBuffer));
    WLANSAP_GetStatistics((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                           &statBuffer, (v_BOOL_t)wrqu->data.flags);

    pstatbuf = kzalloc(QCSAP_MAX_WSC_IE, GFP_KERNEL);
    if(NULL == pstatbuf) {
        hddLog(LOG1, "unable to allocate memory");
        return -ENOMEM;
    }

    len = scnprintf(pstatbuf, QCSAP_MAX_WSC_IE,
                    "RUF=%d RMF=%d RBF=%d "
                    "RUB=%d RMB=%d RBB=%d "
                    "TUF=%d TMF=%d TBF=%d "
                    "TUB=%d TMB=%d TBB=%d ",
                    (int)statBuffer.rxUCFcnt, (int)statBuffer.rxMCFcnt,
                    (int)statBuffer.rxBCFcnt, (int)statBuffer.rxUCBcnt,
                    (int)statBuffer.rxMCBcnt, (int)statBuffer.rxBCBcnt,
                    (int)statBuffer.txUCFcnt, (int)statBuffer.txMCFcnt,
                    (int)statBuffer.txBCFcnt, (int)statBuffer.txUCBcnt,
                    (int)statBuffer.txMCBcnt, (int)statBuffer.txBCBcnt);

    if (len >= QCSAP_MAX_WSC_IE) {
        hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
        kfree(pstatbuf);
        return -EFAULT;
    }

    strlcpy(extra, pstatbuf, len);
    wrqu->data.length = len;
    kfree(pstatbuf);
    return 0;
}

int
static iw_softap_ap_stats(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_ap_stats(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_softap_set_channel_range(struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

    int *value = (int *)extra;
    int startChannel = value[0];
    int endChannel = value[1];
    int band = value[2];
    VOS_STATUS status;
    int ret = 0; /* success */

    status = WLANSAP_SetChannelRange(hHal,startChannel,endChannel,band);
    if(status != VOS_STATUS_SUCCESS)
    {
      hddLog( LOGE, FL("iw_softap_set_channel_range:  startChannel = %d, endChannel = %d band = %d"),
                                  startChannel,endChannel, band);
      ret = -EINVAL;
    }

    pHddCtx->is_dynamic_channel_range_set = 1;

    return ret;
}

static int iw_softap_set_channel_range(struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_set_channel_range(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}


int __iw_softap_get_channel_list(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
    v_U32_t num_channels = 0;
    v_U8_t i = 0;
    v_U8_t bandStartChannel = RF_CHAN_1;
    v_U8_t bandEndChannel = RF_CHAN_165;
    v_U32_t temp_num_channels = 0;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    v_REGDOMAIN_t domainIdCurrentSoftap;
    tpChannelListInfo channel_list = (tpChannelListInfo) extra;
    eCsrBand curBand = eCSR_BAND_ALL;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

    if (eHAL_STATUS_SUCCESS != sme_GetFreqBand(hHal, &curBand))
    {
        hddLog(LOGE,FL("not able get the current frequency band"));
        return -EIO;
    }
    wrqu->data.length = sizeof(tChannelListInfo);
    ENTER();

    if (eCSR_BAND_24 == curBand)
    {
        bandStartChannel = RF_CHAN_1;
        bandEndChannel = RF_CHAN_14;
    }
    else if (eCSR_BAND_5G == curBand)
    {
        bandStartChannel = RF_CHAN_36;
        bandEndChannel = RF_CHAN_165;
    }

    hddLog(LOG1, FL("curBand = %d, bandStartChannel = %hu, "
                "bandEndChannel = %hu "), curBand,
                bandStartChannel, bandEndChannel );

    for( i = bandStartChannel; i <= bandEndChannel; i++ )
    {
        if( NV_CHANNEL_ENABLE == regChannels[i].enabled )
        {
            channel_list->channels[num_channels] = rfChannels[i].channelNum; 
            num_channels++;
        }
    }

    /* remove indoor channels if the domain is FCC, channels 36 - 48 */

    temp_num_channels = num_channels;

    if(eHAL_STATUS_SUCCESS != sme_getSoftApDomain(hHal,(v_REGDOMAIN_t *) &domainIdCurrentSoftap))
    {
        hddLog(LOGE,FL("Failed to get Domain ID, %d"),domainIdCurrentSoftap);
        return -EIO;
    }

    if(REGDOMAIN_FCC == domainIdCurrentSoftap &&
             pHddCtx->cfg_ini->gEnableStrictRegulatoryForFCC )
    {
        for(i = 0; i < temp_num_channels; i++)
        {
      
           if((channel_list->channels[i] > 35) && 
              (channel_list->channels[i] < 49))
           {
               vos_mem_move(&channel_list->channels[i], 
                            &channel_list->channels[i+1], 
                            temp_num_channels - (i-1));
               num_channels--;
               temp_num_channels--;
               i--;
           } 
        }
    }

    hddLog(LOG1,FL(" number of channels %d"), num_channels);

    if (num_channels > IW_MAX_FREQUENCIES)
    {
        num_channels = IW_MAX_FREQUENCIES;
    }

    channel_list->num_channels = num_channels;
    EXIT();

    return 0;
}

int iw_softap_get_channel_list(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_get_channel_list(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static
int __iw_get_genie(struct net_device *dev,
                 struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 
    eHalStatus status;
    v_U32_t length = DOT11F_IE_RSN_MAX_LEN;
    v_U8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];
    ENTER();
    hddLog(LOG1,FL("getGEN_IE ioctl"));
    // Actually retrieve the RSN IE from CSR.  (We previously sent it down in the CSR Roam Profile.)
    status = WLANSap_getstationIE_information(pVosContext, 
                                   &length,
                                   genIeBytes);
    length = VOS_MIN((u_int16_t) length, DOT11F_IE_RSN_MAX_LEN);
    if (wrqu->data.length < length ||
        copy_to_user(wrqu->data.pointer,
                      (v_VOID_t*)genIeBytes, length))
    {
        hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
        return -EFAULT;
    }
    wrqu->data.length = length;
    
    hddLog(LOG1,FL(" RSN IE of %d bytes returned"), wrqu->data.length );
    
   
    EXIT();
    return 0;
}

static
int iw_get_genie(struct net_device *dev,
                 struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_genie(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static
int __iw_get_WPSPBCProbeReqIEs(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));    
    sQcSapreq_WPSPBCProbeReqIES_t WPSPBCProbeReqIEs;
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    ENTER();

    hddLog(LOG1,FL("get_WPSPBCProbeReqIEs ioctl"));
    memset((void*)&WPSPBCProbeReqIEs, 0, sizeof(WPSPBCProbeReqIEs));

    WPSPBCProbeReqIEs.probeReqIELen = pHddApCtx->WPSPBCProbeReq.probeReqIELen;
    vos_mem_copy(&WPSPBCProbeReqIEs.probeReqIE,
                 pHddApCtx->WPSPBCProbeReq.probeReqIE,
                 WPSPBCProbeReqIEs.probeReqIELen);
    vos_mem_copy(&WPSPBCProbeReqIEs.macaddr,
                 pHddApCtx->WPSPBCProbeReq.peerMacAddr,
                 sizeof(v_MACADDR_t));
    if (copy_to_user(wrqu->data.pointer,
                     (void *)&WPSPBCProbeReqIEs,
                      sizeof(WPSPBCProbeReqIEs)))
    {
         hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
         return -EFAULT;
    }
    wrqu->data.length = 12 + WPSPBCProbeReqIEs.probeReqIELen;
    hddLog(LOG1, FL("Macaddress : "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(WPSPBCProbeReqIEs.macaddr));
    up(&pHddApCtx->semWpsPBCOverlapInd);
    EXIT();
    return 0;
}

static
int iw_get_WPSPBCProbeReqIEs(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_WPSPBCProbeReqIEs(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

/**---------------------------------------------------------------------------
  
  \brief __iw_set_auth_hostap() -
   This function sets the auth type received from the wpa_supplicant.
   
  \param  - dev - Pointer to the net device.
              - info - Pointer to the iw_request_info.
              - wrqu - Pointer to the iwreq_data.
              - extra - Pointer to the data.        
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/
int __iw_set_auth_hostap(struct net_device *dev,
                         struct iw_request_info *info,
                         union iwreq_data *wrqu,char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter); 

   ENTER();
   switch(wrqu->param.flags & IW_AUTH_INDEX)
   {
      case IW_AUTH_TKIP_COUNTERMEASURES:
      {
         if(wrqu->param.value) {
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure started %d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STARTED;
         }  
         else {   
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure stopped=%d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STOPED;
         }

         hdd_softap_tkip_mic_fail_counter_measure(pAdapter,
                                                  wrqu->param.value);
      }   
      break;
         
      default:
         
         hddLog(LOGW, "%s called with unsupported auth type %d", __func__, 
               wrqu->param.flags & IW_AUTH_INDEX);
      break;
   }
   
   EXIT();
   return 0;
}

int iw_set_auth_hostap(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu,char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_set_auth_hostap(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

static int __iw_set_ap_encodeext(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;    
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    int retval = 0;
    VOS_STATUS vstatus;
    struct iw_encode_ext *ext = (struct iw_encode_ext*)extra;
    v_U8_t groupmacaddr[WNI_CFG_BSSID_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int key_index;
    struct iw_point *encoding = &wrqu->encoding;
    tCsrRoamSetKey  setKey;   
//    tCsrRoamRemoveKey RemoveKey;
    int i;

    ENTER();    
   
    key_index = encoding->flags & IW_ENCODE_INDEX;
   
    if(key_index > 0) {
      
         /*Convert from 1-based to 0-based keying*/
        key_index--;
    }
    if(!ext->key_len) {
#if 0     
      /*Set the encrytion type to NONE*/
#if 0
       pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
#endif
     
         RemoveKey.keyId = key_index;
         if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
              /*Key direction for group is RX only*/
             vos_mem_copy(RemoveKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);
         }
         else {
             vos_mem_copy(RemoveKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
         }
         switch(ext->alg)
         {
           case IW_ENCODE_ALG_NONE:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
           case IW_ENCODE_ALG_WEP:
              RemoveKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
              break;
           case IW_ENCODE_ALG_TKIP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_TKIP;
              break;
           case IW_ENCODE_ALG_CCMP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_AES;
              break;
          default:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
         }
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Remove key cipher_alg:%d key_len%d *pEncryptionType :%d",
                    __func__,(int)ext->alg,(int)ext->key_len,RemoveKey.encType);
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Peer Mac = "MAC_ADDRESS_STR,
                    __func__, MAC_ADDR_ARRAY(RemoveKey.peerMac));
          );
         vstatus = WLANSAP_DelKeySta( pVosContext, &RemoveKey);
         if ( vstatus != VOS_STATUS_SUCCESS )
         {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "[%4d] WLANSAP_DeleteKeysSta returned ERROR status= %d",
                        __LINE__, vstatus );
             retval = -EINVAL;
         }
#endif
         return retval;

    }
    
    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
   
    setKey.keyId = key_index;
    setKey.keyLength = ext->key_len;
   
    if(ext->key_len <= CSR_MAX_KEY_LEN) {
       vos_mem_copy(&setKey.Key[0],ext->key,ext->key_len);
    }   
   
    if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
      /*Key direction for group is RX only*/
       setKey.keyDirection = eSIR_RX_ONLY;
       vos_mem_copy(setKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);
    }
    else {   
      
       setKey.keyDirection =  eSIR_TX_RX;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
    }
    if(ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
    {
       setKey.keyDirection = eSIR_TX_DEFAULT;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
    }
 
    /*For supplicant pae role is zero*/
    setKey.paeRole = 0;
      
    switch(ext->alg)
    {   
       case IW_ENCODE_ALG_NONE:   
         setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
         break;
         
       case IW_ENCODE_ALG_WEP:
         setKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
         pHddApCtx->uPrivacy = 1;
         hddLog(LOG1, "(%s) uPrivacy=%d", __func__, pHddApCtx->uPrivacy);
         break;
      
       case IW_ENCODE_ALG_TKIP:
       {
          v_U8_t *pKey = &setKey.Key[0];
  
          setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;
  
          vos_mem_zero(pKey, CSR_MAX_KEY_LEN);
  
          /*Supplicant sends the 32bytes key in this order 
          
                |--------------|----------|----------|
                |   Tk1        |TX-MIC    |  RX Mic  | 
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
                
                */
          /*Sme expects the 32 bytes key to be in the below order
  
                |--------------|----------|----------|
                |   Tk1        |RX-MIC    |  TX Mic  | 
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
               */
          /* Copy the Temporal Key 1 (TK1) */
          vos_mem_copy(pKey,ext->key,16);
           
         /*Copy the rx mic first*/
          vos_mem_copy(&pKey[16],&ext->key[24],8); 
          
         /*Copy the tx mic */
          vos_mem_copy(&pKey[24],&ext->key[16],8); 
  
       }     
       break;
      
       case IW_ENCODE_ALG_CCMP:
          setKey.encType = eCSR_ENCRYPT_TYPE_AES;
          break;
          
       default:
          setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
          break;
    }
         
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%s:EncryptionType:%d key_len:%d, KeyId:%d"), __func__, setKey.encType, setKey.keyLength,
            setKey.keyId);
    for(i=0; i< ext->key_len; i++)
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%02x"), setKey.Key[i]);    

    vstatus = WLANSAP_SetKeySta( pVosContext, &setKey);
    if ( vstatus != VOS_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] WLANSAP_SetKeySta returned ERROR status= %d", __LINE__, vstatus );
       retval = -EINVAL;
    }
   
   return retval;
}

static int iw_set_ap_encodeext(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_set_ap_encodeext(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int iw_set_ap_mlme(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu,
                       char *extra)
{
#if 0
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    struct iw_mlme *mlme = (struct iw_mlme *)extra;
 
    ENTER();    
   
    //reason_code is unused. By default it is set to eCSR_DISCONNECT_REASON_UNSPECIFIED
    switch (mlme->cmd) {
        case IW_MLME_DISASSOC:
        case IW_MLME_DEAUTH:
            hddLog(LOG1, "Station disassociate");    
            if( pAdapter->conn_info.connState == eConnectionState_Associated ) 
            {
                eCsrRoamDisconnectReason reason = eCSR_DISCONNECT_REASON_UNSPECIFIED;
                
                if( mlme->reason_code == HDD_REASON_MICHAEL_MIC_FAILURE )
                    reason = eCSR_DISCONNECT_REASON_MIC_ERROR;
                
                status = sme_RoamDisconnect( pAdapter->hHal,pAdapter->sessionId, reason);
                
                //clear all the reason codes
                if (status != 0)
                {
                    hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate : csrRoamDisconnect failure returned %d", __func__, (int)mlme->cmd, (int)status);
                }
                
               netif_stop_queue(dev);
               netif_carrier_off(dev);
            }
            else
            {
                hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate called but station is not in associated state", __func__, (int)mlme->cmd);
            }
        default:
            hddLog(LOGE,"%s %d Command should be Disassociate/Deauthenticate", __func__, (int)mlme->cmd);
            return -EINVAL;
    }//end of switch
    EXIT();
#endif    
    return 0;
//    return status;
}

static int __iw_get_ap_rts_threshold(struct net_device *dev,
                                     struct iw_request_info *info,
                                     union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   v_U32_t status = 0;

   status = hdd_wlan_get_rts_threshold(pHostapdAdapter, wrqu);

   return status;
}

static int iw_get_ap_rts_threshold(struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu, char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_get_ap_rts_threshold(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

static int __iw_get_ap_frag_threshold(struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U32_t status = 0;

    status = hdd_wlan_get_frag_threshold(pHostapdAdapter, wrqu);

    return status;
}

static int iw_get_ap_frag_threshold(struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_ap_frag_threshold(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_get_ap_freq(struct net_device *dev,
                            struct iw_request_info *info,
                            struct iw_freq *fwrq, char *extra)
{
   v_U32_t status = FALSE, channel = 0, freq = 0;
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   tHalHandle hHal;
   hdd_hostapd_state_t *pHostapdState;
   hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pHostapdAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
   }

   pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
   hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);

   if(pHostapdState->bssState == BSS_STOP )
   {
       if (ccmCfgGetInt(hHal, WNI_CFG_CURRENT_CHANNEL, &channel)
                                                  != eHAL_STATUS_SUCCESS)
       {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to get WNI_CFG_CURRENT_CHANNEL from cfg"));
           return -EIO;
       }
       else
       {
          status = hdd_wlan_get_freq(channel, &freq);
          if( TRUE == status)
          {
              /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
               * iwlist & iwconfig command shows frequency into proper
               * format (2.412 GHz instead of 246.2 MHz)*/
              fwrq->m = freq;
              fwrq->e = MHZ;
          }
       }
    }
    else
    {
       channel = pHddApCtx->operatingChannel;
       status = hdd_wlan_get_freq(channel, &freq);
       if( TRUE == status)
       {
          /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
           * iwlist & iwconfig command shows frequency into proper
           * format (2.412 GHz instead of 246.2 MHz)*/
           fwrq->m = freq;
           fwrq->e = MHZ;
       }
    }
   return 0;
}

static int iw_get_ap_freq(struct net_device *dev,
                          struct iw_request_info *info,
                          struct iw_freq *fwrq, char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_get_ap_freq(dev, info, fwrq, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

static int __iw_get_mode(struct net_device *dev,
                         struct iw_request_info *info,
                         union iwreq_data *wrqu, char *extra)
{
    int status = 0;

    wrqu->mode = IW_MODE_MASTER;

    return status;
}

static int iw_get_mode(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_mode(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_softap_setwpsie(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu,
                              char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
   hdd_hostapd_state_t *pHostapdState;
   eHalStatus halStatus= eHAL_STATUS_SUCCESS;
   u_int8_t *wps_genie;
   u_int8_t *fwps_genie;
   u_int8_t *pos;
   tpSap_WPSIE pSap_WPSIe;
   u_int8_t WPSIeType;
   u_int16_t length;   
   struct iw_point s_priv_data;
   ENTER();

   /* helper function to get iwreq_data with compat handling. */
   if (hdd_priv_get_data(&s_priv_data, wrqu))
   {
      return -EINVAL;
   }

   if ((NULL == s_priv_data.pointer) || (s_priv_data.length < QCSAP_MAX_WSC_IE))
   {
      return -EINVAL;
   }

   wps_genie = mem_alloc_copy_from_user_helper(s_priv_data.pointer,
                                               s_priv_data.length);

   if(NULL == wps_genie)
   {
       hddLog(LOG1, "%s: failed to alloc memory "
                    "and copy data from user buffer", __func__);
       return -EFAULT;
   }

   fwps_genie = wps_genie;

   pSap_WPSIe = vos_mem_malloc(sizeof(tSap_WPSIE));
   if (NULL == pSap_WPSIe) 
   {
      hddLog(LOGE, "VOS unable to allocate memory");
      kfree(fwps_genie);
      return -ENOMEM;
   }
   vos_mem_zero(pSap_WPSIe, sizeof(tSap_WPSIE));
 
   hddLog(LOG1,"%s WPS IE type[0x%X] IE[0x%X], LEN[%d]", __func__, wps_genie[0], wps_genie[1], wps_genie[2]);
   WPSIeType = wps_genie[0];
   if ( wps_genie[0] == eQC_WPS_BEACON_IE)
   {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_BEACON_IE; 
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] ) 
      {
         case DOT11F_EID_WPA: 
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe); 
               kfree(fwps_genie);
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0) 
            {
             hddLog (LOG1, "%s Set WPS BEACON IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
                switch((u_int16_t)(*pos<<8) | *(pos+1))
                {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version = *pos;   
                      hddLog(LOG1, "WPS version %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_VER_PRESENT;   
                      pos += 1;
                      break;
                   
                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRA_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %x", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_DEVICEPASSWORDID_PRESENT;
                      pos += 2; 
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2; 
                      break;
                
                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2; 
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSBeaconIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_UUIDE_PRESENT; 
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_RF_BANDS_PRESENT;
                      pos += 1;
                      break;
                   
                   default:
                      hddLog (LOGW, "UNKNOWN TLV in WPS IE(%x)", (*pos<<8 | *(pos+1)));
                      vos_mem_free(pSap_WPSIe);
                      kfree(fwps_genie);
                      return -EINVAL; 
                }
              }  
            }
            else { 
                 hddLog (LOGE, "%s WPS IE Mismatch %X",
                         __func__, wps_genie[0]);
            }     
            break;
                 
         default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, wps_genie[0]);
            vos_mem_free(pSap_WPSIe);
            kfree(fwps_genie);
            return 0;
      }
    } 
    else if( wps_genie[0] == eQC_WPS_PROBE_RSP_IE)
    {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_PROBE_RSP_IE; 
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] ) 
      {
         case DOT11F_EID_WPA: 
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe); 
               kfree(fwps_genie);
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0) 
            {
             hddLog (LOG1, "%s Set WPS PROBE RSP IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
              switch((u_int16_t)(*pos<<8) | *(pos+1))
              {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version = *pos;   
                      hddLog(LOG1, "WPS version %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_VER_PRESENT;   
                      pos += 1;
                      break;
                   
                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRA_PRESENT;                      
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICEPASSWORDID_PRESENT;
                      pos += 2; 
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2; 
                      break;
                  case HDD_WPS_ELEM_RSP_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType = *pos;
                      hddLog(LOG1, "Config Methods: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RESPONSETYPE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2; 
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_UUIDE_PRESENT;
                      pos += length;
                      break;
                   
                   case HDD_WPS_ELEM_MANUFACTURER:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.num_name = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.name, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MANUFACTURE_PRESENT;
                      pos += length;
                      break;
 
                   case HDD_WPS_ELEM_MODEL_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNAME_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_MODEL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_SERIAL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SERIALNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_PRIMARY_DEVICE_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev category: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory);
                      pos += 2;
                      
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceOUI, pos, HDD_WPS_DEVICE_OUI_LEN);
                      hddLog(LOG1, "primary dev oui: %02x, %02x, %02x, %02x", pos[0], pos[1], pos[2], pos[3]);
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev sub category: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT;                      
                      break;
                   case HDD_WPS_ELEM_DEVICE_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.text, pos, length);
                      pos += length;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICENAME_PRESENT;
                      break;
                   case HDD_WPS_ELEM_CONFIG_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ConfigMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Config Methods: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pos += 2; 
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_CONFIGMETHODS_PRESENT;
                      break;
 
                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand);
                      pos += 1;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RF_BANDS_PRESENT;
                      break;
              }  // switch
            }
         } 
         else
         {
            hddLog (LOGE, "%s WPS IE Mismatch %X",__func__, wps_genie[0]);
         }
         
      } // switch
    }
    halStatus = WLANSAP_Set_WpsIe(pVosContext, pSap_WPSIe);
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
    if( pHostapdState->bCommit && WPSIeType == eQC_WPS_PROBE_RSP_IE)
    {
        //hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
        //v_CONTEXT_t pVosContext = pHostapdAdapter->pvosContext;
        WLANSAP_Update_WpsIe ( pVosContext );
    }
 
    vos_mem_free(pSap_WPSIe);   
    kfree(fwps_genie);
    EXIT();
    return halStatus;
}

static int iw_softap_setwpsie(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu,
                              char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_setwpsie(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_softap_stopbss(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    hdd_context_t *pHddCtx         = NULL;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) 
    {
        if ( VOS_STATUS_SUCCESS == (status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext) ) )
        {
            hdd_hostapd_state_t *pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

            status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
   
            if (!VOS_IS_STATUS_SUCCESS(status))
            {  
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
                         ("ERROR: HDD vos wait for single_event failed!!"));
                VOS_ASSERT(0);
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
        wlan_hdd_decr_active_session(pHddCtx, pHostapdAdapter->device_mode);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

static int iw_softap_stopbss(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_stopbss(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __iw_softap_version(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    ENTER();
    hdd_wlan_get_version(pHostapdAdapter, wrqu, extra);
    EXIT();
    return 0;
}

static int iw_softap_version(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_softap_version(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

VOS_STATUS hdd_softap_get_sta_info(hdd_adapter_t *pAdapter, v_U8_t *pBuf, int buf_len)
{
    v_U8_t i;
    int len = 0;
    const char sta_info_header[] = "staId staAddress\n";

    len = scnprintf(pBuf, buf_len, sta_info_header);
    pBuf += len;
    buf_len -= len;

    for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
    {
        if(pAdapter->aStaInfo[i].isUsed)
        {
            len = scnprintf(pBuf, buf_len, "%5d .%02x:%02x:%02x:%02x:%02x:%02x\n",
                                       pAdapter->aStaInfo[i].ucSTAId,
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[0],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[1],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[2],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[3],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[4],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[5]);
            pBuf += len;
            buf_len -= len;
        }
        if(WE_GET_STA_INFO_SIZE > buf_len)
        {
            break;
        }
    }
    return VOS_STATUS_SUCCESS;
}

static int iw_softap_get_sta_info(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, 
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status;
    ENTER();
    status = hdd_softap_get_sta_info(pHostapdAdapter, extra, WE_SAP_MAX_STA_INFO);
    if ( !VOS_IS_STATUS_SUCCESS( status ) ) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s Failed!!!",__func__);
       return -EINVAL;
    }
    wrqu->data.length = strlen(extra);
    EXIT();
    return 0;
}

static int __iw_set_ap_genie(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu, char *extra)
{
 
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    eHalStatus halStatus= eHAL_STATUS_SUCCESS;
    u_int8_t *genie = (u_int8_t *)extra;
    
    ENTER();
    
    if(!wrqu->data.length)
    {
        EXIT();
        return 0;
    }

    switch (genie[0]) 
    {
        case DOT11F_EID_WPA: 
        case DOT11F_EID_RSN:
            if((WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy == 0)
            {
                hdd_softap_Deregister_BC_STA(pHostapdAdapter);
                hdd_softap_Register_BC_STA(pHostapdAdapter, 1);
            }   
            (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = 1;
            halStatus = WLANSAP_Set_WPARSNIes(pVosContext, genie, wrqu->data.length);
            break;
            
        default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, genie[0]);
            halStatus = 0;
    }
    
    EXIT();
    return halStatus; 
}

static int iw_set_ap_genie(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_set_ap_genie(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

static VOS_STATUS  wlan_hdd_get_classAstats_for_station(hdd_adapter_t *pAdapter, u8 staid)
{
   eHalStatus hstatus;
   long lrc;
   struct statsContext context;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: pAdapter is NULL", __func__);
      return VOS_STATUS_E_FAULT;
   }

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;
   hstatus = sme_GetStatistics( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  eCSR_HDD,
                                  SME_GLOBAL_CLASSA_STATS,
                                  hdd_GetClassA_statisticsCB,
                                  0, // not periodic
                                  FALSE, //non-cached results
                                  staid,
                                  &context);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Unable to retrieve statistics for link speed",
            __func__);
   }
   else
   {
      lrc = wait_for_completion_interruptible_timeout(&context.completion,
            msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      if (lrc <= 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: SME %s while retrieving link speed",
              __func__, (0 == lrc) ? "timeout" : "interrupt");
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

   return VOS_STATUS_SUCCESS;
}

int __iw_get_softap_linkspeed(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu,
                            char *extra)

{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   hdd_context_t *pHddCtx;
   char *pLinkSpeed = (char*)extra;
   char *pmacAddress;
   v_U32_t link_speed;
   unsigned short staId;
   int len = sizeof(v_U32_t)+1;
   v_BYTE_t macAddress[VOS_MAC_ADDR_SIZE];
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   int rc, valid;

   pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

   valid = wlan_hdd_validate_context(pHddCtx);

   if (0 != valid)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context not valid"));
       return valid;
   }

   hddLog(VOS_TRACE_LEVEL_INFO, "%s wrqu->data.length= %d", __func__, wrqu->data.length);

   if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1)
   {
       pmacAddress = kmalloc(MAC_ADDRESS_STR_LEN, GFP_KERNEL);
       if (NULL == pmacAddress) {
           hddLog(LOG1, "unable to allocate memory");
           return -ENOMEM;
       }
       if (copy_from_user((void *)pmacAddress,
          wrqu->data.pointer, MAC_ADDRESS_STR_LEN))
       {
           hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
           kfree(pmacAddress);
           return -EFAULT;
       }
       pmacAddress[MAC_ADDRESS_STR_LEN] = '\0';

       status = hdd_string_to_hex (pmacAddress, MAC_ADDRESS_STR_LEN, macAddress );
       kfree(pmacAddress);

       if (!VOS_IS_STATUS_SUCCESS(status ))
       {
           hddLog(VOS_TRACE_LEVEL_ERROR, FL("String to Hex conversion Failed"));
       }
   }
   /* If no mac address is passed and/or its length is less than 17,
    * link speed for first connected client will be returned.
    */
   if (wrqu->data.length < 17 || !VOS_IS_STATUS_SUCCESS(status ))
   {
      status = hdd_softap_GetConnectedStaId(pHostapdAdapter, (void *)(&staId));
   }
   else
   {
      status = hdd_softap_GetStaId(pHostapdAdapter,
                               (v_MACADDR_t *)macAddress, (void *)(&staId));
   }

   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, FL("ERROR: HDD Failed to find sta id!!"));
      link_speed = 0;
   }
   else
   {
      status = wlan_hdd_get_classAstats_for_station(pHostapdAdapter , staId);

      if (!VOS_IS_STATUS_SUCCESS(status ))
      {
          hddLog(VOS_TRACE_LEVEL_ERROR, FL("Unable to retrieve SME statistics"));
          return -EINVAL;
      }

      WLANTL_GetSTALinkCapacity(pHddCtx->pvosContext,
                                staId, &link_speed);

      link_speed = link_speed / 10;

      if (0 == link_speed)
      {
          /* The linkspeed returned by HAL is in units of 500kbps.
           * converting it to mbps.
           * This is required to support legacy firmware which does
           * not return link capacity.
           */
          link_speed =(int)pHostapdAdapter->hdd_stats.ClassA_stat.tx_rate/2;
      }
   }

   wrqu->data.length = len;
   rc = snprintf(pLinkSpeed, len, "%u", link_speed);

   if ((rc < 0) || (rc >= len))
   {
      // encoding or length error?
      hddLog(VOS_TRACE_LEVEL_ERROR,FL( "Unable to encode link speed"));
      return -EIO;
   }

   return 0;
}

int iw_get_softap_linkspeed(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu,
                            char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_softap_linkspeed(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}


static const iw_handler      hostapd_handler[] =
{
   (iw_handler) NULL,           /* SIOCSIWCOMMIT */
   (iw_handler) NULL,           /* SIOCGIWNAME */
   (iw_handler) NULL,           /* SIOCSIWNWID */
   (iw_handler) NULL,           /* SIOCGIWNWID */
   (iw_handler) NULL,           /* SIOCSIWFREQ */
   (iw_handler) iw_get_ap_freq,    /* SIOCGIWFREQ */
   (iw_handler) NULL,           /* SIOCSIWMODE */
   (iw_handler) iw_get_mode,    /* SIOCGIWMODE */
   (iw_handler) NULL,           /* SIOCSIWSENS */
   (iw_handler) NULL,           /* SIOCGIWSENS */
   (iw_handler) NULL,           /* SIOCSIWRANGE */
   (iw_handler) NULL,           /* SIOCGIWRANGE */
   (iw_handler) NULL,           /* SIOCSIWPRIV */
   (iw_handler) NULL,           /* SIOCGIWPRIV */
   (iw_handler) NULL,           /* SIOCSIWSTATS */
   (iw_handler) NULL,           /* SIOCGIWSTATS */
   (iw_handler) NULL,           /* SIOCSIWSPY */
   (iw_handler) NULL,           /* SIOCGIWSPY */
   (iw_handler) NULL,           /* SIOCSIWTHRSPY */
   (iw_handler) NULL,           /* SIOCGIWTHRSPY */
   (iw_handler) NULL,           /* SIOCSIWAP */
   (iw_handler) NULL,           /* SIOCGIWAP */
   (iw_handler) iw_set_ap_mlme,    /* SIOCSIWMLME */
   (iw_handler) NULL,           /* SIOCGIWAPLIST */
   (iw_handler) NULL,           /* SIOCSIWSCAN */
   (iw_handler) NULL,           /* SIOCGIWSCAN */
   (iw_handler) NULL,           /* SIOCSIWESSID */
   (iw_handler) NULL,           /* SIOCGIWESSID */
   (iw_handler) NULL,           /* SIOCSIWNICKN */
   (iw_handler) NULL,           /* SIOCGIWNICKN */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* SIOCSIWRATE */
   (iw_handler) NULL,           /* SIOCGIWRATE */
   (iw_handler) NULL,           /* SIOCSIWRTS */
   (iw_handler) iw_get_ap_rts_threshold,     /* SIOCGIWRTS */
   (iw_handler) NULL,           /* SIOCSIWFRAG */
   (iw_handler) iw_get_ap_frag_threshold,    /* SIOCGIWFRAG */
   (iw_handler) NULL,           /* SIOCSIWTXPOW */
   (iw_handler) NULL,           /* SIOCGIWTXPOW */
   (iw_handler) NULL,           /* SIOCSIWRETRY */
   (iw_handler) NULL,           /* SIOCGIWRETRY */
   (iw_handler) NULL,           /* SIOCSIWENCODE */
   (iw_handler) NULL,           /* SIOCGIWENCODE */
   (iw_handler) NULL,           /* SIOCSIWPOWER */
   (iw_handler) NULL,           /* SIOCGIWPOWER */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) iw_set_ap_genie,     /* SIOCSIWGENIE */
   (iw_handler) NULL,           /* SIOCGIWGENIE */
   (iw_handler) iw_set_auth_hostap,    /* SIOCSIWAUTH */
   (iw_handler) NULL,           /* SIOCGIWAUTH */
   (iw_handler) iw_set_ap_encodeext,     /* SIOCSIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCGIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCSIWPMKSA */
};

/*
 * Note that the following ioctls were defined with semantics which
 * cannot be handled by the "iwpriv" userspace application and hence
 * they are not included in the hostapd_private_args array
 *     QCSAP_IOCTL_ASSOC_STA_MACADDR
 */

static const struct iw_priv_args hostapd_private_args[] = {
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
  { QCSAP_PARAM_MAX_ASSOC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setMaxAssoc" },
   { QCSAP_PARAM_HIDE_SSID,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "hideSSID" },
   { QCSAP_PARAM_SET_MC_RATE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setMcRate" },
  { QCSAP_IOCTL_GETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getparam" },
  { QCSAP_IOCTL_GETPARAM, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "" },
  { QCSAP_PARAM_MAX_ASSOC, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getMaxAssoc" },
  { QCSAP_PARAM_GET_WLAN_DBG, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getwlandbg" },
  { QCSAP_PARAM_AUTO_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getAutoChannel" },
  { QCSAP_PARAM_SET_AUTO_CHANNEL,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setAutoChannel" },
  { QCSAP_PARAM_CLR_ACL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "setClearAcl" },
  { QCSAP_PARAM_ACL_MODE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setAclMode" },
  { QCSAP_IOCTL_GET_STAWPAIE,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "get_staWPAIE" },
  { QCSAP_IOCTL_SETWPAIE,
      IW_PRIV_TYPE_BYTE | QCSAP_MAX_WSC_IE | IW_PRIV_SIZE_FIXED, 0, "setwpaie" },
  { QCSAP_IOCTL_STOPBSS,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED, 0, "stopbss" },
  { QCSAP_IOCTL_VERSION, 0,
      IW_PRIV_TYPE_CHAR | QCSAP_MAX_WSC_IE, "version" },
  { QCSAP_IOCTL_GET_STA_INFO, 0,
      IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_sta_info" },
  { QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES,
      IW_PRIV_TYPE_BYTE | sizeof(sQcSapreq_WPSPBCProbeReqIES_t) | IW_PRIV_SIZE_FIXED, 0, "getProbeReqIEs" },
  { QCSAP_IOCTL_GET_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getchannel" },
  { QCSAP_IOCTL_DISASSOC_STA,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6 , 0, "disassoc_sta" },
  { QCSAP_IOCTL_AP_STATS, 0,
        IW_PRIV_TYPE_CHAR | QCSAP_MAX_WSC_IE, "ap_stats" },
  { QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED,
        IW_PRIV_TYPE_CHAR | 18,
        IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed" },

  { QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "" },
   /* handlers for sub-ioctl */
   {   WE_SET_WLAN_DBG,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
       0, 
       "setwlandbg" },

   /* handlers for main ioctl */
   {   QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "" },

   /* handlers for sub-ioctl */
   {   WE_LOG_DUMP_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "dump" },
   {   WE_P2P_NOA_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "SetP2pPs" },
     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_CREDENTIAL,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccCrdnl" },

     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_PARAMS,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccConfig" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_MODIFY_ACL,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 8,
        0, 
        "modify_acl" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_GET_CHANNEL_LIST,
        0, 
        IW_PRIV_TYPE_BYTE | sizeof(tChannelListInfo),
        "getChannelList" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxPower" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_MAX_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower" },

    {   QCSAP_IOCTL_DATAPATH_SNAP_SHOT,
        IW_PRIV_TYPE_NONE | IW_PRIV_TYPE_NONE,
        0,
        "dataSnapshot" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_TRAFFIC_MONITOR,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTrafficMon" },
};

static const iw_handler hostapd_private[] = {
   [QCSAP_IOCTL_SETPARAM - SIOCIWFIRSTPRIV] = iw_softap_setparam,  //set priv ioctl
   [QCSAP_IOCTL_GETPARAM - SIOCIWFIRSTPRIV] = iw_softap_getparam,  //get priv ioctl   
   [QCSAP_IOCTL_GET_STAWPAIE - SIOCIWFIRSTPRIV] = iw_get_genie, //get station genIE
   [QCSAP_IOCTL_SETWPAIE - SIOCIWFIRSTPRIV] = iw_softap_setwpsie,
   [QCSAP_IOCTL_STOPBSS - SIOCIWFIRSTPRIV] = iw_softap_stopbss,       // stop bss
   [QCSAP_IOCTL_VERSION - SIOCIWFIRSTPRIV] = iw_softap_version,       // get driver version
   [QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES - SIOCIWFIRSTPRIV] = iw_get_WPSPBCProbeReqIEs,
   [QCSAP_IOCTL_GET_CHANNEL - SIOCIWFIRSTPRIV] = iw_softap_getchannel,
   [QCSAP_IOCTL_ASSOC_STA_MACADDR - SIOCIWFIRSTPRIV] = iw_softap_getassoc_stamacaddr,
   [QCSAP_IOCTL_DISASSOC_STA - SIOCIWFIRSTPRIV] = iw_softap_disassoc_sta,
   [QCSAP_IOCTL_AP_STATS - SIOCIWFIRSTPRIV] = iw_softap_ap_stats,
   [QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE - SIOCIWFIRSTPRIV]  = iw_set_three_ints_getnone,   
   [QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE - SIOCIWFIRSTPRIV]     = iw_set_var_ints_getnone,
   [QCSAP_IOCTL_SET_CHANNEL_RANGE - SIOCIWFIRSTPRIV] = iw_softap_set_channel_range,
   [QCSAP_IOCTL_MODIFY_ACL - SIOCIWFIRSTPRIV]   = iw_softap_modify_acl,
   [QCSAP_IOCTL_GET_CHANNEL_LIST - SIOCIWFIRSTPRIV]   = iw_softap_get_channel_list,
   [QCSAP_IOCTL_GET_STA_INFO - SIOCIWFIRSTPRIV] = iw_softap_get_sta_info,
   [QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED - SIOCIWFIRSTPRIV]     = iw_get_softap_linkspeed,
   [QCSAP_IOCTL_SET_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_tx_power,
   [QCSAP_IOCTL_SET_MAX_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_max_tx_power,
   [QCSAP_IOCTL_DATAPATH_SNAP_SHOT - SIOCIWFIRSTPRIV]  =   iw_display_data_path_snapshot,
   [QCSAP_IOCTL_SET_TRAFFIC_MONITOR - SIOCIWFIRSTPRIV]  =  iw_softap_set_trafficmonitor,
};
const struct iw_handler_def hostapd_handler_def = {
   .num_standard     = sizeof(hostapd_handler) / sizeof(hostapd_handler[0]),
   .num_private      = sizeof(hostapd_private) / sizeof(hostapd_private[0]),
   .num_private_args = sizeof(hostapd_private_args) / sizeof(hostapd_private_args[0]),
   .standard         = (iw_handler *)hostapd_handler,
   .private          = (iw_handler *)hostapd_private,
   .private_args     = hostapd_private_args,
   .get_wireless_stats = NULL,
};
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
struct net_device_ops net_ops_struct  = {
    .ndo_open = hdd_hostapd_open,
    .ndo_stop = hdd_hostapd_stop,
    .ndo_uninit = hdd_hostapd_uninit,
    .ndo_start_xmit = hdd_softap_hard_start_xmit,
    .ndo_tx_timeout = hdd_softap_tx_timeout,
    .ndo_get_stats = hdd_softap_stats,
    .ndo_set_mac_address = hdd_hostapd_set_mac_address,
    .ndo_do_ioctl = hdd_hostapd_ioctl,
    .ndo_change_mtu = hdd_hostapd_change_mtu,
    .ndo_select_queue = hdd_hostapd_select_queue,
 };
#endif

int hdd_set_hostapd(hdd_adapter_t *pAdapter)
{
    return VOS_STATUS_SUCCESS;
} 

void hdd_set_ap_ops( struct net_device *pWlanHostapdDev )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
  pWlanHostapdDev->netdev_ops = &net_ops_struct;
#else
  pWlanHostapdDev->open = hdd_hostapd_open;
  pWlanHostapdDev->stop = hdd_hostapd_stop;
  pWlanHostapdDev->uninit = hdd_hostapd_uninit;
  pWlanHostapdDev->hard_start_xmit = hdd_softap_hard_start_xmit;
  pWlanHostapdDev->tx_timeout = hdd_softap_tx_timeout;
  pWlanHostapdDev->get_stats = hdd_softap_stats;
  pWlanHostapdDev->set_mac_address = hdd_hostapd_set_mac_address;
  pWlanHostapdDev->do_ioctl = hdd_hostapd_ioctl;
#endif
}

VOS_STATUS hdd_init_ap_mode( hdd_adapter_t *pAdapter )
{   
    hdd_hostapd_state_t * phostapdBuf;
    struct net_device *dev = pAdapter->dev;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_STATUS status;
#ifdef FEATURE_WLAN_CH_AVOID
    v_U16_t unsafeChannelList[NUM_20MHZ_RF_CHANNELS];
    v_U16_t unsafeChannelCount;
#endif /* FEATURE_WLAN_CH_AVOID */

    ENTER();
       // Allocate the Wireless Extensions state structure   
    phostapdBuf = WLAN_HDD_GET_HOSTAP_STATE_PTR( pAdapter );
 
    sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);

#ifdef FEATURE_WLAN_CH_AVOID
    /* Get unsafe cahnnel list from cached location */
    wcnss_get_wlan_unsafe_channel(unsafeChannelList,
                                  sizeof(unsafeChannelList),
                                  &unsafeChannelCount);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s : Unsafe Channel count %d",
              __func__, unsafeChannelCount);
    hdd_hostapd_update_unsafe_channel_list(pHddCtx,
                                  unsafeChannelList,
                                  unsafeChannelCount);
#endif /* FEATURE_WLAN_CH_AVOID */

    // Zero the memory.  This zeros the profile structure.
    memset(phostapdBuf, 0,sizeof(hdd_hostapd_state_t));
    
    // Set up the pointer to the Wireless Extensions state structure
    // NOP
    status = hdd_set_hostapd(pAdapter);
    if(!VOS_IS_STATUS_SUCCESS(status)) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: hdd_set_hostapd failed!!"));
         return status;
    }
 
    status = vos_event_init(&phostapdBuf->vosEvent);
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: Hostapd HDD vos event init failed!!"));
         return status;
    }
    
    init_completion(&pAdapter->session_close_comp_var);
    init_completion(&pAdapter->session_open_comp_var);

    sema_init(&(WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->semWpsPBCOverlapInd, 1);
 
     // Register as a wireless device
    dev->wireless_handlers = (struct iw_handler_def *)& hostapd_handler_def;

    //Initialize the data path module
    status = hdd_softap_init_tx_rx(pAdapter);
    if ( !VOS_IS_STATUS_SUCCESS( status ))
    {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_softap_init_tx_rx failed", __func__);
    }

    status = hdd_wmm_adapter_init( pAdapter );
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
             "hdd_wmm_adapter_init() failed with status code %08d [x%08x]",
                             status, status );
       goto error_wmm_init;
    }

    set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

    wlan_hdd_set_monitor_tx_adapter( WLAN_HDD_GET_CTX(pAdapter), pAdapter );

    return status;

error_wmm_init:
    hdd_softap_deinit_tx_rx( pAdapter );
    EXIT();
    return status;
}

hdd_adapter_t* hdd_wlan_create_ap_dev( hdd_context_t *pHddCtx, tSirMacAddr macAddr, tANI_U8 *iface_name )
{
    struct net_device *pWlanHostapdDev = NULL;
    hdd_adapter_t *pHostapdAdapter = NULL;
    v_CONTEXT_t pVosContext= NULL;

   pWlanHostapdDev = alloc_netdev_mq(sizeof(hdd_adapter_t), iface_name, ether_setup, NUM_TX_QUEUES);

    if (pWlanHostapdDev != NULL)
    {
        pHostapdAdapter = netdev_priv(pWlanHostapdDev);

        //Init the net_device structure
        ether_setup(pWlanHostapdDev);

        //Initialize the adapter context to zeros.
        vos_mem_zero(pHostapdAdapter, sizeof( hdd_adapter_t ));
        pHostapdAdapter->dev = pWlanHostapdDev;
        pHostapdAdapter->pHddCtx = pHddCtx; 
        pHostapdAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;

        //Get the Global VOSS context.
        pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
        //Save the adapter context in global context for future.
        ((VosContextType*)(pVosContext))->pHDDSoftAPContext = (v_VOID_t*)pHostapdAdapter;

        //Init the net_device structure
        strlcpy(pWlanHostapdDev->name, (const char *)iface_name, IFNAMSIZ);

        hdd_set_ap_ops( pHostapdAdapter->dev );

        pWlanHostapdDev->watchdog_timeo = HDD_TX_TIMEOUT;
        pWlanHostapdDev->mtu = HDD_DEFAULT_MTU;
    
        vos_mem_copy(pWlanHostapdDev->dev_addr, (void *)macAddr,sizeof(tSirMacAddr));
        vos_mem_copy(pHostapdAdapter->macAddressCurrent.bytes, (void *)macAddr, sizeof(tSirMacAddr));

        pWlanHostapdDev->destructor = free_netdev;
        pWlanHostapdDev->ieee80211_ptr = &pHostapdAdapter->wdev ;
        pHostapdAdapter->wdev.wiphy = pHddCtx->wiphy;  
        pHostapdAdapter->wdev.netdev =  pWlanHostapdDev;
        init_completion(&pHostapdAdapter->tx_action_cnf_event);
        init_completion(&pHostapdAdapter->cancel_rem_on_chan_var);
        init_completion(&pHostapdAdapter->rem_on_chan_ready_event);
        init_completion(&pHostapdAdapter->ula_complete);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
        init_completion(&pHostapdAdapter->offchannel_tx_event);
#endif

        SET_NETDEV_DEV(pWlanHostapdDev, pHddCtx->parent_dev);
    }
    return pHostapdAdapter;
}

VOS_STATUS hdd_register_hostapd( hdd_adapter_t *pAdapter, tANI_U8 rtnl_lock_held )
{
   struct net_device *dev = pAdapter->dev;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   ENTER();
   
   if( rtnl_lock_held )
   {
     if (strnchr(dev->name, strlen(dev->name), '%')) {
         if( dev_alloc_name(dev, dev->name) < 0 )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s:Failed:dev_alloc_name", __func__);
            return VOS_STATUS_E_FAILURE;            
         }
      }
      if (register_netdevice(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s:Failed:register_netdevice", __func__);
         return VOS_STATUS_E_FAILURE;         
      }
   }
   else
   {
      if (register_netdev(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed:register_netdev", __func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   set_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);
     
   EXIT();
   return status;
}
    
VOS_STATUS hdd_unregister_hostapd(hdd_adapter_t *pAdapter)
{
   ENTER();
   
   hdd_softap_deinit_tx_rx(pAdapter);

   /* if we are being called during driver unload, then the dev has already
      been invalidated.  if we are being called at other times, then we can
      detatch the wireless device handlers */
   if (pAdapter->dev)
   {
      pAdapter->dev->wireless_handlers = NULL;
   }
   EXIT();
   return 0;
}
