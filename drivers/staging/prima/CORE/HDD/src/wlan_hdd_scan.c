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

/**========================================================================

  \file  wlan_hdd_scan.c

  \brief WLAN Host Device Driver implementation


  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  04/5/09     Shailender     Created module.

  ==========================================================================*/
      /* To extract the Scan results */

/* Add a stream event */

#include <wlan_qct_driver.h>
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <palTypes.h>
#include <aniGlobal.h>
#include <dot11f.h>
#ifdef WLAN_BTAMP_FEATURE
#include "bap_hdd_misc.h"
#endif

#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <vos_sched.h>

#define WEXT_CSCAN_HEADER               "CSCAN S\x01\x00\x00S\x00"
#define WEXT_CSCAN_HEADER_SIZE          12
#define WEXT_CSCAN_SSID_SECTION         'S'
#define WEXT_CSCAN_CHANNEL_SECTION      'C'
#define WEXT_CSCAN_NPROBE_SECTION       'N'
#define WEXT_CSCAN_ACTV_DWELL_SECTION   'A'
#define WEXT_CSCAN_PASV_DWELL_SECTION   'P'
#define WEXT_CSCAN_HOME_DWELL_SECTION   'H'
#define WEXT_CSCAN_TYPE_SECTION         'T'
#define WEXT_CSCAN_PENDING_SECTION      'O'
#define WEXT_CSCAN_TYPE_DEFAULT         0
#define WEXT_CSCAN_TYPE_PASSIVE         1
#define WEXT_CSCAN_PASV_DWELL_TIME      130
#define WEXT_CSCAN_PASV_DWELL_TIME_DEF  250
#define WEXT_CSCAN_PASV_DWELL_TIME_MAX  3000
#define WEXT_CSCAN_HOME_DWELL_TIME      130
#define MAX_RATES                       12

#define WEXT_CSCAN_SCAN_DONE_WAIT_TIME  2000

typedef struct hdd_scan_info{
    struct net_device *dev;
    struct iw_request_info *info;
    char *start;
    char *end;
} hdd_scan_info_t, *hdd_scan_info_tp;

static v_S31_t hdd_TranslateABGRateToMbpsRate(v_U8_t *pFcRate)
{

    /** Slightly more sophisticated processing has to take place here.
              Basic rates are rounded DOWN.  HT rates are rounded UP.*/
    return ( (( ((v_S31_t) *pFcRate) & 0x007f) * 1000000) / 2);
}


static eHalStatus hdd_AddIwStreamEvent(int cmd, int length, char* data, hdd_scan_info_t *pscanInfo, char **last_event, char **current_event )
{
    struct iw_event event;

    *last_event = *current_event;
    vos_mem_zero(&event, sizeof (struct iw_event));
    event.cmd = cmd;
    event.u.data.flags = 1;
    event.u.data.length = length;
    *current_event = iwe_stream_add_point (pscanInfo->info,*current_event, pscanInfo->end,  &event, data);

    if(*last_event == *current_event)
    {
            /* no space to add event */
        hddLog( LOGW, "%s: no space left to add event", __func__);
        return -E2BIG; /* Error code, may be E2BIG */
    }

    return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_GetWPARSNIEs() -

   This function extract the WPA/RSN IE from the Bss descriptor IEs fields

  \param  - ieFields - Pointer to the Bss Descriptor IEs.
              - ie_length - IE Length.
              - last_event -Points to the last event.
              - current_event - Points to the
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/


/* Extract the WPA and/or RSN IEs */
static eHalStatus hdd_GetWPARSNIEs( v_U8_t *ieFields, v_U16_t ie_length, char **last_event, char **current_event, hdd_scan_info_t *pscanInfo )
{
    v_U8_t eid, elen, *element;
    v_U16_t tie_length=0;

    ENTER();

    element = ieFields;
    tie_length = ie_length;

    while( tie_length > 2 && element != NULL )
    {
        eid = element[0];
        elen = element[1];

        /*If element length is greater than total remaining ie length,
         *break the loop*/
        if ((elen+2) > tie_length)
           break;

        switch(eid)
        {
            case DOT11F_EID_WPA:
            case DOT11F_EID_RSN:
#ifdef FEATURE_WLAN_WAPI
            case DOT11F_EID_WAPI:
#endif
                if(hdd_AddIwStreamEvent( IWEVGENIE,  elen+2, (char*)element, pscanInfo, last_event, current_event ) < 0 )
                    return -E2BIG;
                break;

            default:
                break;
        }

        /* Next element */
        tie_length -= (2 + elen);
        element += 2 + elen;
    }

    return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_IndicateScanResult() -

   This function returns the scan results to the wpa_supplicant

  \param  - scanInfo - Pointer to the scan info structure.
              - descriptor - Pointer to the Bss Descriptor.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
#define MAX_CUSTOM_LEN 64
static eHalStatus hdd_IndicateScanResult(hdd_scan_info_t *scanInfo, tCsrScanResultInfo *scan_result)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(scanInfo->dev) ;
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
   tSirBssDescription *descriptor = &scan_result->BssDescriptor;
   struct iw_event event;
   char *current_event = scanInfo->start;
   char *end = scanInfo->end;
   char *last_event;
   char *current_pad;
   v_U16_t ie_length = 0;
   v_U16_t capabilityInfo;
   char *modestr;
   int error;
   char custom[MAX_CUSTOM_LEN];
   char *p;

   hddLog( LOG1, "hdd_IndicateScanResult " MAC_ADDRESS_STR,
          MAC_ADDR_ARRAY(descriptor->bssId));

   error = 0;
   last_event = current_event;
   vos_mem_zero(&event, sizeof (event));

   /* BSSID */
   event.cmd = SIOCGIWAP;
   event.u.ap_addr.sa_family = ARPHRD_ETHER;
   vos_mem_copy (event.u.ap_addr.sa_data, descriptor->bssId,
                  sizeof (descriptor->bssId));
   current_event = iwe_stream_add_event(scanInfo->info,current_event, end,
                   &event, IW_EV_ADDR_LEN);

   if (last_event == current_event)
   {
      /* no space to add event */
      /* Error code may be E2BIG */
       hddLog(LOGE, "hdd_IndicateScanResult: no space for SIOCGIWAP ");
       return -E2BIG;
   }

   last_event = current_event;
   vos_mem_zero(&event, sizeof (struct iw_event));

 /* Protocol Name */
   event.cmd = SIOCGIWNAME;

   switch (descriptor->nwType)
   {
   case eSIR_11A_NW_TYPE:
       modestr = "a";
       break;
   case eSIR_11B_NW_TYPE:
       modestr = "b";
       break;
   case eSIR_11G_NW_TYPE:
       modestr = "g";
       break;
   case eSIR_11N_NW_TYPE:
       modestr = "n";
       break;
   default:
       hddLog( LOGW, "%s: Unknown network type [%d]",
              __func__, descriptor->nwType);
       modestr = "?";
       break;
   }
   snprintf(event.u.name, IFNAMSIZ, "IEEE 802.11%s", modestr);
   current_event = iwe_stream_add_event(scanInfo->info,current_event, end,
                   &event, IW_EV_CHAR_LEN);

   if (last_event == current_event)
   { /* no space to add event */
       hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWNAME");
      /* Error code, may be E2BIG */
       return -E2BIG;
   }

   last_event = current_event;
   vos_mem_zero( &event, sizeof (struct iw_event));

   /*Freq*/
   event.cmd = SIOCGIWFREQ;

   event.u.freq.m = descriptor->channelId;
   event.u.freq.e = 0;
   event.u.freq.i = 0;
   current_event = iwe_stream_add_event(scanInfo->info,current_event, end,
                                        &event, IW_EV_FREQ_LEN);

   if (last_event == current_event)
   { /* no space to add event */
       hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWFREQ");
       return -E2BIG;
   }

   last_event = current_event;
   vos_mem_zero( &event, sizeof (struct iw_event));

   /* BSS Mode */
   event.cmd = SIOCGIWMODE;

   capabilityInfo = descriptor->capabilityInfo;

   if (SIR_MAC_GET_ESS(capabilityInfo))
   {
       event.u.mode = IW_MODE_INFRA;
   }
   else if (SIR_MAC_GET_IBSS(capabilityInfo))
   {
       event.u.mode = IW_MODE_ADHOC;
   }
   else
   {
       /* neither ESS or IBSS */
       event.u.mode = IW_MODE_AUTO;
   }

   current_event = iwe_stream_add_event(scanInfo->info,current_event, end,
                                        &event, IW_EV_UINT_LEN);

   if (last_event == current_event)
   { /* no space to add event */
       hddLog(LOGE, "hdd_IndicateScanResult: no space for SIOCGIWMODE");
       return -E2BIG;
   }
   /* To extract SSID */
   ie_length = GET_IE_LEN_IN_BSS( descriptor->length );

   if (ie_length > 0)
   {
       /* dot11BeaconIEs is a large struct, so we make it static to
          avoid stack overflow.  This API is only invoked via ioctl,
          so it is serialized by the kernel rtnl_lock and hence does
          not need to be reentrant */
       static tDot11fBeaconIEs dot11BeaconIEs;
       tDot11fIESSID *pDot11SSID;
       tDot11fIESuppRates *pDot11SuppRates;
       tDot11fIEExtSuppRates *pDot11ExtSuppRates;
       tDot11fIEHTCaps *pDot11IEHTCaps;
       int numBasicRates = 0;
       int maxNumRates = 0;

       pDot11IEHTCaps = NULL;

       dot11fUnpackBeaconIEs ((tpAniSirGlobal)
           hHal, (tANI_U8 *) descriptor->ieFields, ie_length,  &dot11BeaconIEs);

       pDot11SSID = &dot11BeaconIEs.SSID;


       if (pDot11SSID->present ) {
          last_event = current_event;
          vos_mem_zero (&event, sizeof (struct iw_event));

          event.cmd = SIOCGIWESSID;
          event.u.data.flags = 1;
          event.u.data.length = scan_result->ssId.length;
          current_event = iwe_stream_add_point (scanInfo->info,current_event, end,
                  &event, (char *)scan_result->ssId.ssId);

          if(last_event == current_event)
          { /* no space to add event */
             hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWESSID");
             return -E2BIG;
          }
       }

      if( hdd_GetWPARSNIEs( ( tANI_U8 *) descriptor->ieFields, ie_length, &last_event, &current_event, scanInfo )  < 0    )
      {
          hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWESSID");
          return -E2BIG;
      }

      last_event = current_event;
      current_pad = current_event + IW_EV_LCP_LEN;
      vos_mem_zero( &event, sizeof (struct iw_event));

      /*Rates*/
      event.cmd = SIOCGIWRATE;


      pDot11SuppRates = &dot11BeaconIEs.SuppRates;

      if (pDot11SuppRates->present )
      {
          int i;

          numBasicRates = pDot11SuppRates->num_rates;
          for (i=0; i<pDot11SuppRates->num_rates; i++)
          {
              if (0 != (pDot11SuppRates->rates[i] & 0x7F))
              {
                  event.u.bitrate.value = hdd_TranslateABGRateToMbpsRate (
                      &pDot11SuppRates->rates[i]);

                  current_pad = iwe_stream_add_value (scanInfo->info,current_event,
                      current_pad, end, &event, IW_EV_PARAM_LEN);
              }
          }

      }

      pDot11ExtSuppRates = &dot11BeaconIEs.ExtSuppRates;

      if (pDot11ExtSuppRates->present )
      {
          int i,no_of_rates;
          maxNumRates = numBasicRates + pDot11ExtSuppRates->num_rates;

          /* Check to make sure the total number of rates
               doesn't exceed IW_MAX_BITRATES */

          maxNumRates = VOS_MIN(maxNumRates , IW_MAX_BITRATES);

          if((maxNumRates - numBasicRates) > MAX_RATES)
          {
             no_of_rates = MAX_RATES;
             hddLog( LOGW, "Accessing array out of bound that array is pDot11ExtSuppRates->rates ");
          }
          else
          {
            no_of_rates = maxNumRates - numBasicRates;
          }
          for ( i=0; i< no_of_rates ; i++ )
          {
              if (0 != (pDot11ExtSuppRates->rates[i] & 0x7F))
              {
                  event.u.bitrate.value = hdd_TranslateABGRateToMbpsRate (
                      &pDot11ExtSuppRates->rates[i]);

                  current_pad = iwe_stream_add_value (scanInfo->info,current_event,
                      current_pad, end, &event, IW_EV_PARAM_LEN);
              }
          }
      }


      if ((current_pad - current_event) >= IW_EV_LCP_LEN)
      {
          current_event = current_pad;
      }
      else
      {
          if (last_event == current_event)
          { /* no space to add event */
              hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWRATE");
              return -E2BIG;
          }
      }

      last_event = current_event;
      vos_mem_zero (&event, sizeof (struct iw_event));


      event.cmd = SIOCGIWENCODE;

      if (SIR_MAC_GET_PRIVACY(capabilityInfo))
      {
         event.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
      }
      else
      {
         event.u.data.flags = IW_ENCODE_DISABLED;
      }
      event.u.data.length = 0;

      current_event = iwe_stream_add_point(scanInfo->info,current_event, end, &event, (char *)pDot11SSID->ssid);


      if(last_event == current_event)
      { /* no space to add event
               Error code, may be E2BIG */
          hddLog( LOGE, "hdd_IndicateScanResult: no space for SIOCGIWENCODE");
          return -E2BIG;
      }
   }

   last_event = current_event;
   vos_mem_zero( &event, sizeof (struct iw_event));

    /*RSSI*/
   event.cmd = IWEVQUAL;
   event.u.qual.qual = descriptor->rssi;
   event.u.qual.noise = descriptor->sinr;

   /*To keep the rssi icon of the connected AP in the scan window
    *and the rssi icon of the wireless networks in sync */   
   if (( eConnectionState_Associated == 
              pAdapter->sessionCtx.station.conn_info.connState ) &&
              ( VOS_TRUE == vos_mem_compare(descriptor->bssId, 
                             pAdapter->sessionCtx.station.conn_info.bssId, 
                             WNI_CFG_BSSID_LEN)))
   {
      event.u.qual.level = pAdapter->rssi;
   }
   else
   {
      event.u.qual.level = VOS_MIN ((descriptor->rssi + descriptor->sinr), 0);
   }
   
   event.u.qual.updated = IW_QUAL_ALL_UPDATED;

   current_event = iwe_stream_add_event(scanInfo->info,current_event,
       end, &event, IW_EV_QUAL_LEN);

   if(last_event == current_event)
   { /* no space to add event */
       hddLog( LOGE, "hdd_IndicateScanResult: no space for IWEVQUAL");
       return -E2BIG;
   }


   /* AGE */
   event.cmd = IWEVCUSTOM;
   p = custom;
   p += scnprintf(p, MAX_CUSTOM_LEN, " Age: %lu",
                 vos_timer_get_system_ticks() - descriptor->nReceivedTime);
   event.u.data.length = p - custom;
   current_event = iwe_stream_add_point (scanInfo->info,current_event, end,
                                         &event, custom);
   if(last_event == current_event)
   { /* no space to add event */
      hddLog( LOGE, "hdd_IndicateScanResult: no space for IWEVCUSTOM (age)");
      return -E2BIG;
   }

   scanInfo->start = current_event;

   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_processSpoofMacAddrRequest() -

   The function is called from scan completion callback and from
   cfg80211 vendor command

  \param  - pHddCtx - Pointer to the HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_processSpoofMacAddrRequest(hdd_context_t *pHddCtx)
{

    ENTER();

    mutex_lock(&pHddCtx->spoofMacAddr.macSpoofingLock);

    if (pHddCtx->spoofMacAddr.isEnabled) {
        if (VOS_STATUS_SUCCESS != vos_randomize_n_bytes(
                (void *)(&pHddCtx->spoofMacAddr.randomMacAddr.bytes[3]),
                VOS_MAC_ADDR_LAST_3_BYTES)) {
                pHddCtx->spoofMacAddr.isEnabled = FALSE;
                mutex_unlock(&pHddCtx->spoofMacAddr.macSpoofingLock);
                hddLog(LOGE, FL("Failed to generate random Mac Addr"));
                return VOS_STATUS_E_FAILURE;
        }
    }

    hddLog(LOG1, FL("New Mac Addr Generated "MAC_ADDRESS_STR),
                 MAC_ADDR_ARRAY(pHddCtx->spoofMacAddr.randomMacAddr.bytes));

    if (pHddCtx->scan_info.mScanPending != TRUE)
    {
        pHddCtx->spoofMacAddr.isReqDeferred = FALSE;
        hddLog(LOG1, FL("Processing Spoof request now"));
        /* Inform SME about spoof mac addr request*/
        if ( eHAL_STATUS_SUCCESS != sme_SpoofMacAddrReq(pHddCtx->hHal,
                &pHddCtx->spoofMacAddr.randomMacAddr))
        {
            hddLog(LOGE, FL("Sending Spoof request failed - Disable spoofing"));
            pHddCtx->spoofMacAddr.isEnabled = FALSE;
        }
    } else
    {
        hddLog(LOG1, FL("Scan in Progress. Spoofing Deferred"));
        pHddCtx->spoofMacAddr.isReqDeferred = TRUE;
    }

    mutex_unlock(&pHddCtx->spoofMacAddr.macSpoofingLock);

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_ScanRequestCallback() -

   The sme module calls this callback function once it finish the scan request
   and this function notifies the scan complete event to the wpa_supplicant.

  \param  - halHandle - Pointer to the Hal Handle.
              - pContext - Pointer to the data context.
              - scanId - Scan ID.
              - status - CSR Status.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static eHalStatus hdd_ScanRequestCallback(tHalHandle halHandle, void *pContext,
                         tANI_U32 scanId, eCsrScanStatus status)
{
    struct net_device *dev = (struct net_device *) pContext;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev) ;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    union iwreq_data wrqu;
    int we_event;
    char *msg;
    
    ENTER();

    hddLog(LOGW,"%s called with halHandle = %p, pContext = %p, scanID = %d,"
           " returned status = %d", __func__, halHandle, pContext,
           (int) scanId, (int) status);

    /* if there is a scan request pending when the wlan driver is unloaded
       we may be invoked as SME flushes its pending queue.  If that is the
       case, the underlying net_device may have already been destroyed, so
       do some quick sanity before proceeding */
    if (pAdapter->dev != dev)
    {
       hddLog(LOGW, "%s: device mismatch %p vs %p",
               __func__, pAdapter->dev, dev);
        return eHAL_STATUS_SUCCESS;
    }

    /* Check the scanId */
    if (pHddCtx->scan_info.scanId != scanId)
    {
        hddLog(LOGW, "%s called with mismatched scanId pHddCtx->scan_info.scanId = %d "
               "scanId = %d ", __func__, (int) pHddCtx->scan_info.scanId,
                (int) scanId);
    }

    /* Scan is no longer pending */
    pHddCtx->scan_info.mScanPending = VOS_FALSE;

    // notify any applications that may be interested
    memset(&wrqu, '\0', sizeof(wrqu));
    we_event = SIOCGIWSCAN;
    msg = NULL;
    wireless_send_event(dev, we_event, &wrqu, msg);

    EXIT();

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: exit !!!",__func__);

    return eHAL_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief __iw_set_scan() -

   This function process the scan request from the wpa_supplicant
   and set the scan request to the SME

  \param  - dev - Pointer to the net device.
              - info - Pointer to the iw_request_info.
              - wrqu - Pointer to the iwreq_data.
              - extra - Pointer to the data.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/


int __iw_set_scan(struct net_device *dev, struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter;
   hdd_context_t *pHddCtx;
   hdd_wext_state_t *pwextBuf;
   tCsrScanRequest scanRequest;
   v_U32_t scanId = 0;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   struct iw_scan_req *scanReq = (struct iw_scan_req *)extra;
   int ret = 0;

   ENTER();

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: enter !!!",__func__);

   pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   if (NULL == pAdapter)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Adapter is NULL",__func__);
       return -EINVAL;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret)
   {
       return ret;
   }
   pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   if (NULL == pwextBuf)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: pwextBuf is NULL",__func__);
       return -EINVAL;
   }
#ifdef WLAN_BTAMP_FEATURE
   //Scan not supported when AMP traffic is on.
   if( VOS_TRUE == WLANBAP_AmpSessionOn() ) 
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: No scanning when AMP is on",__func__);
       return eHAL_STATUS_SUCCESS;
   }
#endif
   if(pHddCtx->scan_info.mScanPending == TRUE)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:mScanPending is TRUE !!!",__func__);
       return eHAL_STATUS_SUCCESS;
   }

   vos_mem_zero( &scanRequest, sizeof(scanRequest));

   if (NULL != wrqu->data.pointer)
   {
       /* set scanType, active or passive */
       if ((IW_SCAN_TYPE_ACTIVE ==  scanReq->scan_type) || (eSIR_ACTIVE_SCAN == pHddCtx->scan_info.scan_mode))
       {
           scanRequest.scanType = eSIR_ACTIVE_SCAN;
       }
       else
       {
           scanRequest.scanType = eSIR_PASSIVE_SCAN;
       }

       /* set bssid using sockaddr from iw_scan_req */
       vos_mem_copy(scanRequest.bssid,
                       &scanReq->bssid.sa_data, sizeof(scanRequest.bssid) );

      if (wrqu->data.flags & IW_SCAN_THIS_ESSID)  {

          if(scanReq->essid_len  &&
               (scanReq->essid_len <= SIR_MAC_MAX_SSID_LENGTH)) {
              scanRequest.SSIDs.numOfSSIDs = 1;
              scanRequest.SSIDs.SSIDList =( tCsrSSIDInfo *)vos_mem_malloc(sizeof(tCsrSSIDInfo));
              if(scanRequest.SSIDs.SSIDList) {
                 scanRequest.SSIDs.SSIDList->SSID.length =             scanReq->essid_len;
                 vos_mem_copy(scanRequest.SSIDs.SSIDList->  SSID.ssId,scanReq->essid,scanReq->essid_len);
              }
              else
              {
                scanRequest.SSIDs.numOfSSIDs = 0;
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: Unable to allocate memory",__func__);
                VOS_ASSERT(0);
              }
          }
          else
          {
            hddLog(LOGE, FL("Invalid essid length : %d"), scanReq->essid_len);
          }
      }

       /* set min and max channel time */
       scanRequest.minChnTime = scanReq->min_channel_time;
       scanRequest.maxChnTime = scanReq->max_channel_time;

   }
   else
   {
       if(pHddCtx->scan_info.scan_mode == eSIR_ACTIVE_SCAN) {
           /* set the scan type to active */
           scanRequest.scanType = eSIR_ACTIVE_SCAN;
       } else {
           scanRequest.scanType = eSIR_PASSIVE_SCAN;
       }

       vos_mem_set( scanRequest.bssid, sizeof( tCsrBssid ), 0xff );

       /* set min and max channel time to zero */
       scanRequest.minChnTime = 0;
       scanRequest.maxChnTime = 0;
   }

   /* set BSSType to default type */
   scanRequest.BSSType = eCSR_BSS_TYPE_ANY;

   /*Scan all the channels */
   scanRequest.ChannelInfo.numOfChannels = 0;

   scanRequest.ChannelInfo.ChannelList = NULL;

   /* set requestType to full scan */
   scanRequest.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;

   /* if previous genIE is not NULL, update ScanIE */
   if (0 != pwextBuf->genIE.length)
   {
       memset( &pHddCtx->scan_info.scanAddIE, 0, sizeof(pHddCtx->scan_info.scanAddIE) );
       memcpy( pHddCtx->scan_info.scanAddIE.addIEdata, pwextBuf->genIE.addIEdata, 
           pwextBuf->genIE.length );
       pHddCtx->scan_info.scanAddIE.length = pwextBuf->genIE.length;
      /* Maximum length of each IE is SIR_MAC_MAX_IE_LENGTH */
       if (SIR_MAC_MAX_IE_LENGTH  >=  pwextBuf->genIE.length)
       {
           memcpy( pwextBuf->roamProfile.addIEScan,
                       pHddCtx->scan_info.scanAddIE.addIEdata,
                       pHddCtx->scan_info.scanAddIE.length);
           pwextBuf->roamProfile.nAddIEScanLength =
                                pHddCtx->scan_info.scanAddIE.length;
       }
       else
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "Invalid ScanIE, Length is %d", pwextBuf->genIE.length);
       }
       /* clear previous genIE after use it */
       memset( &pwextBuf->genIE, 0, sizeof(pwextBuf->genIE) );
   }

   /* push addIEScan in scanRequset if exist */
   if (pHddCtx->scan_info.scanAddIE.addIEdata && 
       pHddCtx->scan_info.scanAddIE.length)
   { 
       scanRequest.uIEFieldLen = pHddCtx->scan_info.scanAddIE.length;
       scanRequest.pIEField = pHddCtx->scan_info.scanAddIE.addIEdata;
   }

   status = sme_ScanRequest( (WLAN_HDD_GET_CTX(pAdapter))->hHal, pAdapter->sessionId,&scanRequest, &scanId, &hdd_ScanRequestCallback, dev ); 
   if (!HAL_STATUS_SUCCESS(status))
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:sme_ScanRequest  fail %d!!!",__func__, status);
       goto error;
   }

   pHddCtx->scan_info.mScanPending = TRUE;
   pHddCtx->scan_info.sessionId = pAdapter->sessionId;

   pHddCtx->scan_info.scanId = scanId;

error:
   if ((wrqu->data.flags & IW_SCAN_THIS_ESSID) && (scanReq->essid_len))
       vos_mem_free(scanRequest.SSIDs.SSIDList);

   EXIT();
   return status;
}

int iw_set_scan(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *wrqu, char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_set_scan(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);

   return ret;
}

/**---------------------------------------------------------------------------

  \brief __iw_get_scan() -

   This function returns the scan results to the wpa_supplicant

  \param  - dev - Pointer to the net device.
              - info - Pointer to the iw_request_info.
              - wrqu - Pointer to the iwreq_data.
              - extra - Pointer to the data.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/


int __iw_get_scan(struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter;
   hdd_context_t *pHddCtx;
   tHalHandle hHal;
   tCsrScanResultInfo *pScanResult;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   hdd_scan_info_t scanInfo;
   tScanResultHandle pResult;
   int i = 0, ret = 0;

   ENTER();
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: enter buffer length %d!!!",
       __func__, (wrqu->data.length)?wrqu->data.length:IW_SCAN_MAX_DATA);

   pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   if (NULL == pAdapter)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Adapter is NULL",__func__);
       return -EINVAL;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret)
   {
       return ret;
   }
   hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
   if (NULL == hHal)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Hal Context is NULL",__func__);
       return -EINVAL;
   }

   if (TRUE == pHddCtx->scan_info.mScanPending)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:mScanPending is TRUE !!!",__func__);
       return -EAGAIN;
   }
   scanInfo.dev = dev;
   scanInfo.start = extra;
   scanInfo.info = info;

   if (0 == wrqu->data.length)
   {
       scanInfo.end = extra + IW_SCAN_MAX_DATA;
   }
   else
   {
       scanInfo.end = extra + wrqu->data.length;
   }

   status = sme_ScanGetResult(hHal,pAdapter->sessionId,NULL,&pResult);

   if (NULL == pResult)
   {
       // no scan results
       hddLog(LOG1,"__iw_get_scan: NULL Scan Result ");
       return 0;
   }

   pScanResult = sme_ScanResultGetFirst(hHal, pResult);

   while (pScanResult)
   {
       status = hdd_IndicateScanResult(&scanInfo, pScanResult);
       if (0 != status)
       {
           break;
       }
       i++;
       pScanResult = sme_ScanResultGetNext(hHal, pResult);
   }

   sme_ScanResultPurge(hHal, pResult);

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: exit total %d BSS reported !!!",__func__, i);
   EXIT();
   return status;
}

int iw_get_scan(struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu, char *extra)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __iw_get_scan(dev, info, wrqu, extra);
    vos_ssr_unprotect(__func__);

    return ret;
}

#if 0
static eHalStatus hdd_CscanRequestCallback(tHalHandle halHandle, void *pContext,
                         tANI_U32 scanId, eCsrScanStatus status)
{
    struct net_device *dev = (struct net_device *) pContext;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev) ;
    hdd_wext_state_t *pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    union iwreq_data wrqu;
    int we_event;
    char *msg;
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    ENTER();

    hddLog(LOG1,"%s called with halHandle = %p, pContext = %p, scanID = %d,"
           " returned status = %d", __func__, halHandle, pContext,
            (int) scanId, (int) status);

    /* Check the scanId */
    if (pwextBuf->scanId != scanId)
    {
        hddLog(LOGW, "%s called with mismatched scanId pWextState->scanId = %d "
               "scanId = %d ", __func__, (int) pwextBuf->scanId,
                (int) scanId);
    }

    /* Scan is no longer pending */
    pwextBuf->mScanPending = VOS_FALSE;

    // notify any applications that may be interested
    memset(&wrqu, '\0', sizeof(wrqu));
    we_event = SIOCGIWSCAN;
    msg = NULL;
    wireless_send_event(dev, we_event, &wrqu, msg);

    vos_status = vos_event_set(&pwextBuf->vosevent);

    if (!VOS_IS_STATUS_SUCCESS(vos_status))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: HDD vos_event_set failed!!"));
       return VOS_STATUS_E_FAILURE;
    }

    EXIT();
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: exit !!!",__func__);

    return eHAL_STATUS_SUCCESS;
}
#endif

int iw_set_cscan(struct net_device *dev, struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev) ;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_wext_state_t *pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrScanRequest scanRequest;
    v_U32_t scanId = 0;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    v_U8_t channelIdx;

    ENTER();
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: enter !!!",__func__);

#ifdef WLAN_BTAMP_FEATURE
    //Scan not supported when AMP traffic is on.
    if( VOS_TRUE == WLANBAP_AmpSessionOn() )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: No scanning when AMP is on",__func__);
        return eHAL_STATUS_SUCCESS;
    }
#endif

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
        return eHAL_STATUS_SUCCESS;
    }

    vos_mem_zero( &scanRequest, sizeof(scanRequest));
    if (NULL != wrqu->data.pointer)
    {
        char *str_ptr = NULL;
        tCsrSSIDInfo *SsidInfo = NULL;
        int num_ssid = 0;
        int i, j, ssid_start;
        hdd_scan_pending_option_e scanPendingOption = WEXT_SCAN_PENDING_GIVEUP;

        str_ptr = extra;

        i = WEXT_CSCAN_HEADER_SIZE;

        if( WEXT_CSCAN_PENDING_SECTION == str_ptr[i] )
        {
            scanPendingOption = (hdd_scan_pending_option_e)str_ptr[++i];
            ++i;
        }
        pHddCtx->scan_info.scan_pending_option = scanPendingOption;

        if(pHddCtx->scan_info.mScanPending == TRUE)
        {
            hddLog(LOG1,"%s: mScanPending is TRUE",__func__);
            /* If any scan is pending, just giveup this scan request */
            if(WEXT_SCAN_PENDING_GIVEUP == scanPendingOption)
            {
                pHddCtx->scan_info.waitScanResult = FALSE;
                return eHAL_STATUS_SUCCESS; 
            }
            /* If any scan pending, wait till finish current scan,
               and try this scan request when previous scan finish */
            else if(WEXT_SCAN_PENDING_DELAY == scanPendingOption)
            {
                pHddCtx->scan_info.waitScanResult = TRUE;
                vos_event_reset(&pHddCtx->scan_info.scan_finished_event);
                if(vos_wait_single_event(&pHddCtx->scan_info.scan_finished_event,
                                          WEXT_CSCAN_SCAN_DONE_WAIT_TIME))
                {
                    hddLog(LOG1,"%s: Previous SCAN does not finished on time",__func__);
                    return eHAL_STATUS_SUCCESS; 
                }
            }
            /* Piggyback previous scan result */
            else if(WEXT_SCAN_PENDING_PIGGYBACK == scanPendingOption)
            {
                pHddCtx->scan_info.waitScanResult = TRUE;
                return eHAL_STATUS_SUCCESS; 
            }
        }
        pHddCtx->scan_info.waitScanResult = FALSE;

        /* Check for scan IE */
        while( WEXT_CSCAN_SSID_SECTION == str_ptr[i] ) 
        {
            /* ssid_len */
            if(str_ptr[++i] != WEXT_CSCAN_CHANNEL_SECTION) 
            {
                /* total number of ssid's */
                num_ssid++;
                /* increment length filed */
                i += str_ptr[i] + 1;
            }  
            /* i should be saved and it will be pointing to 'C' */
        }

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: numSsid %d !!!",__func__, num_ssid);
        if( num_ssid ) 
        {
            /* To be fixed in SME and PE: override the number of ssid with 1,
            * as SME and PE does not handle multiple SSID in scan request
            * */
          scanRequest.SSIDs.numOfSSIDs = num_ssid;
          /* Allocate num_ssid tCsrSSIDInfo structure */
          SsidInfo = scanRequest.SSIDs.SSIDList =( tCsrSSIDInfo *)vos_mem_malloc(num_ssid*sizeof(tCsrSSIDInfo));
          if(NULL == scanRequest.SSIDs.SSIDList) {
             hddLog(VOS_TRACE_LEVEL_ERROR, "memory alloc failed SSIDInfo buffer");
             return -ENOMEM;
          }

          /* copy all the ssid's and their length */
          ssid_start = WEXT_CSCAN_HEADER_SIZE + 1;/* skipping 'S' */
          for(j = 0; j < num_ssid; j++) {
             if( SIR_MAC_MAX_SSID_LENGTH < str_ptr[ssid_start]){
                scanRequest.SSIDs.numOfSSIDs -= 1;
             } else{
                /* get the ssid length */
                SsidInfo->SSID.length = str_ptr[ssid_start++];
                vos_mem_copy(SsidInfo->SSID.ssId, &str_ptr[ssid_start], SsidInfo->SSID.length);
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "SSID number %d:  %s", j, SsidInfo->SSID.ssId);
             }
                /* skipping length */
             ssid_start += str_ptr[ssid_start - 1] + 1;
             /* Store next ssid info */
             SsidInfo++;
          }
       }

        /* Check for Channel IE */
        if ( WEXT_CSCAN_CHANNEL_SECTION == str_ptr[i]) 
        {
            if( str_ptr[++i] == 0 ) 
            {
                scanRequest.ChannelInfo.numOfChannels = 0;
                scanRequest.ChannelInfo.ChannelList = NULL;
                i++;
            }
            else {

                /* increment the counter */
                scanRequest.ChannelInfo.numOfChannels = str_ptr[i++];
                /* store temp channel list */
                /* SME expects 1 byte channel content */
                scanRequest.ChannelInfo.ChannelList = vos_mem_malloc(scanRequest.ChannelInfo.numOfChannels * sizeof(v_U8_t));
                if(NULL == scanRequest.ChannelInfo.ChannelList) 
                {
                    hddLog(LOGE, "memory alloc failed for channel list creation");
                    status = -ENOMEM;
                    goto exit_point;
                }

                for(channelIdx = 0; channelIdx < scanRequest.ChannelInfo.numOfChannels; channelIdx++)
                {
                   /* SCAN request from upper layer has 2 bytes channel */
                   scanRequest.ChannelInfo.ChannelList[channelIdx] = (v_U8_t)str_ptr[i];
                   i += sizeof(v_U16_t);
                }
            }
        }

        /* Set default */
        scanRequest.scanType = eSIR_ACTIVE_SCAN;
        scanRequest.minChnTime = 0;
        scanRequest.maxChnTime = 0;

        /* Now i is pointing to passive dwell dwell time */
        /* 'P',min dwell time, max dwell time */
        /* next two offsets contain min and max channel time */
        if( WEXT_CSCAN_PASV_DWELL_SECTION == (str_ptr[i]) ) 
        {
            /* No SSID specified, num_ssid == 0, then start paasive scan */
            if (!num_ssid || (eSIR_PASSIVE_SCAN == pHddCtx->scan_info.scan_mode))
            {
                scanRequest.scanType = eSIR_PASSIVE_SCAN;
                scanRequest.minChnTime = (v_U8_t)str_ptr[++i];//scanReq->min_channel_time;
                scanRequest.maxChnTime = (v_U8_t)str_ptr[++i];//scanReq->max_channel_time;
                i++;
            }
            else
            {
                i += 3;
            }    
        }   

        /* H indicates active channel time */
        if( WEXT_CSCAN_HOME_DWELL_SECTION == (str_ptr[i]) ) 
        {
            if (num_ssid || (eSIR_ACTIVE_SCAN == pHddCtx->scan_info.scan_mode))
            {
                scanRequest.scanType = eSIR_ACTIVE_SCAN;
                scanRequest.minChnTime = str_ptr[++i];//scanReq->min_channel_time;
                scanRequest.maxChnTime = str_ptr[++i];//scanReq->max_channel_time;
                i++;
            }
            else
            {
                i +=3;
            }
        }
        scanRequest.BSSType = eCSR_BSS_TYPE_ANY;
        /* set requestType to full scan */
        scanRequest.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;
        pHddCtx->scan_info.mScanPending = TRUE;

        /* if previous genIE is not NULL, update ScanIE */
        if(0 != pwextBuf->genIE.length)
        {
            memset( &pHddCtx->scan_info.scanAddIE, 0, sizeof(pHddCtx->scan_info.scanAddIE) );
            memcpy( pHddCtx->scan_info.scanAddIE.addIEdata, pwextBuf->genIE.addIEdata, 
                pwextBuf->genIE.length );
            pHddCtx->scan_info.scanAddIE.length = pwextBuf->genIE.length;
            if (SIR_MAC_MAX_IE_LENGTH  >=  pwextBuf->genIE.length)
            {
                memcpy( pwextBuf->roamProfile.addIEScan,
                           pHddCtx->scan_info.scanAddIE.addIEdata,
                           pHddCtx->scan_info.scanAddIE.length);
                pwextBuf->roamProfile.nAddIEScanLength =
                                  pHddCtx->scan_info.scanAddIE.length;
            }
            else
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "Invalid ScanIE, Length is %d",
                          pwextBuf->genIE.length);
            }

            /* clear previous genIE after use it */
            memset( &pwextBuf->genIE, 0, sizeof(pwextBuf->genIE) );
        }

        /* push addIEScan in scanRequset if exist */
        if (pHddCtx->scan_info.scanAddIE.addIEdata && 
            pHddCtx->scan_info.scanAddIE.length)
        {
            scanRequest.uIEFieldLen = pHddCtx->scan_info.scanAddIE.length;
            scanRequest.pIEField = pHddCtx->scan_info.scanAddIE.addIEdata;
        }

        status = sme_ScanRequest( (WLAN_HDD_GET_CTX(pAdapter))->hHal, 
            pAdapter->sessionId,&scanRequest, &scanId, &hdd_ScanRequestCallback, dev ); 
        if( !HAL_STATUS_SUCCESS(status) )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s: SME scan fail status %d !!!",__func__, status);
            pHddCtx->scan_info.mScanPending = FALSE;
            status = -EINVAL;
            goto exit_point;
        }

        pHddCtx->scan_info.scanId = scanId;
        pHddCtx->scan_info.sessionId = pAdapter->sessionId;

    } //end of data->pointer
    else {
        status = -1;
    }

exit_point:

    /* free ssidlist */
    if (scanRequest.SSIDs.SSIDList) 
    {
        vos_mem_free(scanRequest.SSIDs.SSIDList);
    }
    /* free the channel list */
    if(scanRequest.ChannelInfo.ChannelList)
    {
        vos_mem_free((void*)scanRequest.ChannelInfo.ChannelList);
    }

    EXIT();
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: exit !!!",__func__);
    return status;
}

/* Abort any MAC scan if in progress */
tSirAbortScanStatus hdd_abort_mac_scan(hdd_context_t* pHddCtx,
                                       tANI_U8 sessionId,
                                       eCsrAbortReason reason)
{
    return sme_AbortMacScan(pHddCtx->hHal, sessionId, reason);
}

