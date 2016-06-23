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

/*
 * This file limScanResultUtils.cc contains the utility functions
 * LIM uses for maintaining and accessing scan results on STA.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "limTypes.h"
#include "limUtils.h"
#include "limSerDesUtils.h"
#include "limApi.h"
#include "limSession.h"
#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif
#include "vos_utils.h"

/**
 * limDeactiveMinChannelTimerDuringScan()
 *
 *FUNCTION:
 * This function is called during scan upon receiving
 * Beacon/Probe Response frame to deactivate MIN channel
 * timer if running.
 *
 * This function should be called only when pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 *
 * @return eSIR_SUCCESS in case of success
 */

tANI_U32
limDeactivateMinChannelTimerDuringScan(tpAniSirGlobal pMac)
{
    if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) && (pMac->lim.gLimHalScanState == eLIM_HAL_SCANNING_STATE))
    {
        /**
            * Beacon/Probe Response is received during active scanning.
            * Deactivate MIN channel timer if running.
            */
        
        limDeactivateAndChangeTimer(pMac,eLIM_MIN_CHANNEL_TIMER);
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, NO_SESSION, eLIM_MAX_CHANNEL_TIMER));
        if (tx_timer_activate(&pMac->lim.limTimers.gLimMaxChannelTimer)
                                          == TX_TIMER_ERROR)
        {
            /// Could not activate max channel timer.
            // Log error
            limLog(pMac,LOGP, FL("could not activate max channel timer"));

            limCompleteMlmScan(pMac, eSIR_SME_RESOURCES_UNAVAILABLE);
            return TX_TIMER_ERROR;
        }
    }
    return eSIR_SUCCESS;
} /*** end limDeactivateMinChannelTimerDuringScan() ***/



/**
 * limCollectBssDescription()
 *
 *FUNCTION:
 * This function is called during scan upon receiving
 * Beacon/Probe Response frame to check if the received
 * frame matches scan criteria, collect BSS description
 * and add it to cached scan results.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pBPR - Pointer to parsed Beacon/Probe Response structure
 * @param  pRxPacketInfo  - Pointer to Received frame's BD
 * ---------if defined WLAN_FEATURE_VOWIFI------
 * @param  fScanning - flag to indicate if it is during scan.
 * ---------------------------------------------
 *
 * @return None
 */
#if defined WLAN_FEATURE_VOWIFI
eHalStatus
limCollectBssDescription(tpAniSirGlobal pMac,
                         tSirBssDescription *pBssDescr,
                         tpSirProbeRespBeacon pBPR,
                         tANI_U8  *pRxPacketInfo,
                         tANI_U8  fScanning)
#else
eHalStatus
limCollectBssDescription(tpAniSirGlobal pMac,
                         tSirBssDescription *pBssDescr,
                         tpSirProbeRespBeacon pBPR,
                         tANI_U8 *pRxPacketInfo)
#endif
{
    tANI_U8             *pBody;
    tANI_U32            ieLen = 0;
    tpSirMacMgmtHdr     pHdr;
    tANI_U8             channelNum;
    tANI_U8             rxChannel;
    tANI_U8             rfBand = 0;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    VOS_ASSERT(WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) >= SIR_MAC_B_PR_SSID_OFFSET);
    ieLen    = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) - SIR_MAC_B_PR_SSID_OFFSET;
    rxChannel = WDA_GET_RX_CH(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    rfBand = WDA_GET_RX_RFBAND(pRxPacketInfo);

    /**
     * Drop all the beacons and probe response without P2P IE during P2P search
     */
    if (NULL != pMac->lim.gpLimMlmScanReq && pMac->lim.gpLimMlmScanReq->p2pSearch)
    {
        if (NULL == limGetP2pIEPtr(pMac, (pBody + SIR_MAC_B_PR_SSID_OFFSET), ieLen))
        {
            limLog( pMac, LOG3, MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pHdr->bssId));
            return eHAL_STATUS_FAILURE;
        }
    }

    /**
     * Length of BSS desription is without length of
     * length itself and length of pointer
     * that holds the next BSS description
     */
    pBssDescr->length = (tANI_U16)(
                    sizeof(tSirBssDescription) - sizeof(tANI_U16) -
                    sizeof(tANI_U32) + ieLen);

    // Copy BSS Id
    vos_mem_copy((tANI_U8 *) &pBssDescr->bssId,
                 (tANI_U8 *) pHdr->bssId,
                  sizeof(tSirMacAddr));

    // Copy Timestamp, Beacon Interval and Capability Info
    pBssDescr->scanSysTimeMsec = vos_timer_get_system_time();

    pBssDescr->timeStamp[0]   = pBPR->timeStamp[0];
    pBssDescr->timeStamp[1]   = pBPR->timeStamp[1];
    pBssDescr->beaconInterval = pBPR->beaconInterval;
    pBssDescr->capabilityInfo = limGetU16((tANI_U8 *) &pBPR->capabilityInfo);

     if(!pBssDescr->beaconInterval )
    {
			        limLog(pMac, LOGW,
            FL("Beacon Interval is ZERO, making it to default 100 "
            MAC_ADDRESS_STR), MAC_ADDR_ARRAY(pHdr->bssId));
        pBssDescr->beaconInterval= 100;
    }	
    /*
    * There is a narrow window after Channel Switch msg is sent to HAL and before the AGC is shut
    * down and beacons/Probe Rsps can trickle in and we may report the incorrect channel in 5Ghz
    * band, so not relying on the 'last Scanned Channel' stored in LIM.
    * Instead use the value returned by RXP in BD. This the the same value which HAL programs into
    * RXP before every channel switch.
    * Right now there is a problem in 5Ghz, where we are receiving beacons from a channel different from
    * the currently scanned channel. so incorrect channel is reported to CSR and association does not happen.
    * So for now we keep on looking for the channel info in the beacon (DSParamSet IE OR HT Info IE), and only if it
    * is not present in the beacon, we go for the channel info present in RXP.
    * This fix will work for 5Ghz 11n devices, but for 11a devices, we have to rely on RXP routing flag to get the correct channel.
    * So The problem of incorrect channel reporting in 5Ghz will still remain for 11a devices.
    */
    pBssDescr->channelId = limGetChannelFromBeacon(pMac, pBPR);

    if (pBssDescr->channelId == 0)
    {
       /* If the channel Id is not retrieved from Beacon, extract the channel from BD */
       /* Unmapped the channel.This We have to do since we have done mapping in the hal to
         overcome  the limitation of RXBD of not able to accomodate the bigger channel number.*/
       if ((!rfBand) || IS_5G_BAND(rfBand))
       {
          rxChannel = limUnmapChannel(rxChannel);
       }
       if (!rxChannel)
       {
          rxChannel = pMac->lim.gLimCurrentScanChannelId;
       }
       pBssDescr->channelId = rxChannel;
    }

    pBssDescr->channelIdSelf = pBssDescr->channelId;
    //set the network type in bss description
    channelNum = pBssDescr->channelId;
    pBssDescr->nwType = limGetNwType(pMac, channelNum, SIR_MAC_MGMT_FRAME, pBPR);

    pBssDescr->aniIndicator = pBPR->propIEinfo.aniIndicator;

    // Copy RSSI & SINR from BD

    PELOG4(limLog(pMac, LOG4, "***********BSS Description for BSSID:*********** ");
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, pBssDescr->bssId, 6 );
    sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOG4, (tANI_U8*)pRxPacketInfo, 36 );)

    pBssDescr->rssi = (tANI_S8)WDA_GET_RX_RSSI_DB(pRxPacketInfo);
    
    //SINR no longer reported by HW
    pBssDescr->sinr = 0;

    pBssDescr->nReceivedTime = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);

#if defined WLAN_FEATURE_VOWIFI
    if( fScanning )
    {
       rrmGetStartTSF( pMac, pBssDescr->startTSF );
       pBssDescr->parentTSF = WDA_GET_RX_TIMESTAMP(pRxPacketInfo); 
    }
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
    // MobilityDomain
    pBssDescr->mdie[0] = 0;
    pBssDescr->mdie[1] = 0;
    pBssDescr->mdie[2] = 0;
    pBssDescr->mdiePresent = FALSE;
    // If mdie is present in the probe resp we 
    // fill it in the bss description
    if( pBPR->mdiePresent) 
    {
        pBssDescr->mdiePresent = TRUE;
        pBssDescr->mdie[0] = pBPR->mdie[0];
        pBssDescr->mdie[1] = pBPR->mdie[1];
        pBssDescr->mdie[2] = pBPR->mdie[2];
    }
#endif

#ifdef FEATURE_WLAN_ESE
    pBssDescr->QBSSLoad_present = FALSE;
    pBssDescr->QBSSLoad_avail = 0; 
    if( pBPR->QBSSLoad.present) 
    {
        pBssDescr->QBSSLoad_present = TRUE;
        pBssDescr->QBSSLoad_avail = pBPR->QBSSLoad.avail;
    }
#endif
    // Copy IE fields
    vos_mem_copy((tANI_U8 *) &pBssDescr->ieFields,
                  pBody + SIR_MAC_B_PR_SSID_OFFSET,
                  ieLen);

    //sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOGW, (tANI_U8 *) pBssDescr, pBssDescr->length + 2 );
    limLog( pMac, LOG3,
        FL("Collected BSS Description for Channel(%1d), length(%u), aniIndicator(%d), IE Fields(%u)"),
        pBssDescr->channelId,
        pBssDescr->length,
        pBssDescr->aniIndicator,
        ieLen );

    return eHAL_STATUS_SUCCESS;
} /*** end limCollectBssDescription() ***/

/**
 * limIsScanRequestedSSID()
 *
 *FUNCTION:
 * This function is called during scan upon receiving
 * Beacon/Probe Response frame to check if the received
 * SSID is present in the list of requested SSIDs in scan
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  ssId - SSID Received in beacons/Probe responses that is compared against the 
                            requeusted SSID in scan list
 * ---------------------------------------------
 *
 * @return boolean - TRUE if SSID is present in requested list, FALSE otherwise
 */

tANI_BOOLEAN limIsScanRequestedSSID(tpAniSirGlobal pMac, tSirMacSSid *ssId)
{
    tANI_U8 i = 0;

    for (i = 0; i < pMac->lim.gpLimMlmScanReq->numSsid; i++)
    {
        if ( eANI_BOOLEAN_TRUE == vos_mem_compare((tANI_U8 *) ssId,
                   (tANI_U8 *) &pMac->lim.gpLimMlmScanReq->ssId[i],
                   (tANI_U8) (pMac->lim.gpLimMlmScanReq->ssId[i].length + 1)))
        {
            return eANI_BOOLEAN_TRUE;
        }
    }
    return eANI_BOOLEAN_FALSE;
}

/**
 * limCheckAndAddBssDescription()
 *
 *FUNCTION:
 * This function is called during scan upon receiving
 * Beacon/Probe Response frame to check if the received
 * frame matches scan criteria, collect BSS description
 * and add it to cached scan results.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pBPR - Pointer to parsed Beacon/Probe Response structure
 * @param  pRxPacketInfo  - Pointer to Received frame's BD
 * @param  fScanning - boolean to indicate whether the BSS is from current scan or just happen to receive a beacon
 *
 * @return None
 */

void
limCheckAndAddBssDescription(tpAniSirGlobal pMac,
                             tpSirProbeRespBeacon pBPR,
                             tANI_U8 *pRxPacketInfo,
                             tANI_BOOLEAN fScanning,
                             tANI_U8 fProbeRsp)
{
    tLimScanResultNode   *pBssDescr;
    tANI_U32              frameLen, ieLen = 0;
    tANI_U8               rxChannelInBeacon = 0;
    eHalStatus            status;
    tANI_U8               dontUpdateAll = 0;
    tANI_U8               rfBand = 0;
    tANI_U8               rxChannelInBD = 0;

    tSirMacAddr bssid = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tSirMacAddr bssid_zero =  {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    tANI_BOOLEAN fFound = FALSE;
    tpSirMacDataHdr3a pHdr;

    pHdr = WDA_GET_RX_MPDUHEADER3A((tANI_U8 *)pRxPacketInfo);

    // Check For Null BSSID; Skip in case of P2P.
    if (vos_mem_compare(bssid_zero, &pHdr->addr3, 6))
    {
        return ;
    }

    //Checking if scanning for a particular BSSID
    if ((fScanning) && (pMac->lim.gpLimMlmScanReq)) 
    {
        fFound = vos_mem_compare(pHdr->addr3, &pMac->lim.gpLimMlmScanReq->bssId, 6);
        if (!fFound)
        {
            if ((pMac->lim.gpLimMlmScanReq->p2pSearch) &&
               (vos_mem_compare(pBPR->P2PProbeRes.P2PDeviceInfo.P2PDeviceAddress,
               &pMac->lim.gpLimMlmScanReq->bssId, 6)))
            {
                fFound = eANI_BOOLEAN_TRUE;
            }
        }
    }

    /**
     * Compare SSID with the one sent in
     * Probe Request frame, if any.
     * If they don't match, ignore the
     * Beacon frame.
     * pMac->lim.gLimMlmScanReq->ssId.length == 0
     * indicates Broadcast SSID.
     * When gLimReturnAfterFirstMatch is set, it means the scan has to match 
     * a SSID (if it is also set). Ignore the other BSS in that case.
     */

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (!(WDA_GET_OFFLOADSCANLEARN(pRxPacketInfo)))
    {
#endif
      if ((pMac->lim.gpLimMlmScanReq) &&
         (((fScanning) &&
           ( pMac->lim.gLimReturnAfterFirstMatch & 0x01 ) &&
           (pMac->lim.gpLimMlmScanReq->numSsid) &&
           !limIsScanRequestedSSID(pMac, &pBPR->ssId)) ||
          (!fFound && (pMac->lim.gpLimMlmScanReq &&
                       pMac->lim.gpLimMlmScanReq->bssId) &&
           !vos_mem_compare(bssid,
                           &pMac->lim.gpLimMlmScanReq->bssId, 6))))
    {
        /**
         * Received SSID does not match with
         * the one we're scanning for.
         * Ignore received Beacon frame
         */

        return;
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    }
#endif

    /* There is no point in caching & reporting the scan results for APs
     * which are in the process of switching the channel. So, we are not
     * caching the scan results for APs which are adverzing the channel-switch
     * element in their beacons and probe responses.
     */
    if(pBPR->channelSwitchPresent)
    {
        return;
    }

    /* If beacon/probe resp DS param channel does not match with 
     * RX BD channel then don't save the results. It might be a beacon
     * from another channel heard as noise on the current scanning channel
     */

    if ((pBPR->dsParamsPresent) || (pBPR->HTInfo.present))
    {
       /* This means that we are in 2.4GHz mode or 5GHz 11n mode */
       rxChannelInBeacon = limGetChannelFromBeacon(pMac, pBPR);
       rfBand = WDA_GET_RX_RFBAND(pRxPacketInfo);
       rxChannelInBD = WDA_GET_RX_CH(pRxPacketInfo);

       if ((!rfBand) || IS_5G_BAND(rfBand))
       {
          rxChannelInBD = limUnmapChannel(rxChannelInBD);
       }

       if(rxChannelInBD != rxChannelInBeacon)
       {
          /* BCAST Frame, if CH do not match, Drop */
           if(WDA_IS_RX_BCAST(pRxPacketInfo))
           {
                limLog(pMac, LOG3, FL("Beacon/Probe Rsp dropped. Channel in BD %d. "
                                      "Channel in beacon" " %d"),
                       WDA_GET_RX_CH(pRxPacketInfo),limGetChannelFromBeacon(pMac, pBPR));
                return;
           }
           /* Unit cast frame, Probe RSP, do not drop */
           else
           {
              dontUpdateAll = 1;
              limLog(pMac, LOG3, FL("SSID %s, CH in ProbeRsp %d, CH in BD %d, miss-match, Do Not Drop"),
                                     pBPR->ssId.ssId,
                                     rxChannelInBeacon,
                                     WDA_GET_RX_CH(pRxPacketInfo));
              WDA_GET_RX_CH(pRxPacketInfo) = rxChannelInBeacon;
           }
        }
    }

    /**
     * Allocate buffer to hold BSS description from
     * received Beacon frame.
     * Include size of fixed fields and IEs length
     */

    ieLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
    if (ieLen <= SIR_MAC_B_PR_SSID_OFFSET)
    {
        limLog(pMac, LOGP,
               FL("RX packet has invalid length %d"), ieLen);
        return;
    }

    ieLen -= SIR_MAC_B_PR_SSID_OFFSET;

    frameLen = sizeof(tLimScanResultNode) + ieLen - sizeof(tANI_U32); //Sizeof(tANI_U32) is for ieFields[1]

    pBssDescr = vos_mem_malloc(frameLen);
    if ( NULL == pBssDescr )
    {
        // Log error
        limLog(pMac, LOGP,
           FL("call for AllocateMemory failed for storing BSS description"));

        return;
    }

    // In scan state, store scan result.
#if defined WLAN_FEATURE_VOWIFI
    status = limCollectBssDescription(pMac, &pBssDescr->bssDescription,
                             pBPR, pRxPacketInfo, fScanning);
    if (eHAL_STATUS_SUCCESS != status)
    {
        goto last;
    }
#else
    status = limCollectBssDescription(pMac, &pBssDescr->bssDescription,
                             pBPR, pRxPacketInfo);
    if (eHAL_STATUS_SUCCESS != status)
    {
        goto last;
    }
#endif
    pBssDescr->bssDescription.fProbeRsp = fProbeRsp;

    pBssDescr->next = NULL;

    /**
     * Depending on whether to store unique or all
     * scan results, pass hash update/add parameter
     * For LFR candidates just add them on it's own cache
     */

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (WDA_GET_OFFLOADSCANLEARN(pRxPacketInfo))
    {
       limLog(pMac, LOG2, FL(" pHdr->addr1:"MAC_ADDRESS_STR),
              MAC_ADDR_ARRAY(pHdr->addr1));
       limLog(pMac, LOG2, FL(" pHdr->addr2:"MAC_ADDRESS_STR),
              MAC_ADDR_ARRAY(pHdr->addr2));
       limLog(pMac, LOG2, FL(" pHdr->addr3:"MAC_ADDRESS_STR),
              MAC_ADDR_ARRAY(pHdr->addr3));
       limLog( pMac, LOG2, FL("Save this entry in LFR cache"));
       status = limLookupNaddLfrHashEntry(pMac, pBssDescr, LIM_HASH_ADD, dontUpdateAll);
    }
    else
#endif
    //If it is not scanning, only save unique results
    if (pMac->lim.gLimReturnUniqueResults || (!fScanning))
    {
        status = limLookupNaddHashEntry(pMac, pBssDescr, LIM_HASH_UPDATE, dontUpdateAll);
    }
    else
    {
        status = limLookupNaddHashEntry(pMac, pBssDescr, LIM_HASH_ADD, dontUpdateAll);
    }

    if(fScanning)
    {
        if ((pBssDescr->bssDescription.channelId <= 14) &&
            (pMac->lim.gLimReturnAfterFirstMatch & 0x40) &&
            pBPR->countryInfoPresent)
            pMac->lim.gLim24Band11dScanDone = 1;

        if ((pBssDescr->bssDescription.channelId > 14) &&
            (pMac->lim.gLimReturnAfterFirstMatch & 0x80) &&
            pBPR->countryInfoPresent)
            pMac->lim.gLim50Band11dScanDone = 1;

        if ( ( pMac->lim.gLimReturnAfterFirstMatch & 0x01 ) ||
             ( pMac->lim.gLim24Band11dScanDone && ( pMac->lim.gLimReturnAfterFirstMatch & 0x40 ) ) ||
             ( pMac->lim.gLim50Band11dScanDone && ( pMac->lim.gLimReturnAfterFirstMatch & 0x80 ) ) ||
              fFound )
        {
            /**
             * Stop scanning and return the BSS description(s)
             * collected so far.
             */
            limLog(pMac,
                   LOGW,
                   FL("Completed scan: 24Band11dScan = %d, 50Band11dScan = %d BSS id"),
                   pMac->lim.gLim24Band11dScanDone,
                   pMac->lim.gLim50Band11dScanDone);

            //Need to disable the timers. If they fire, they will send END_SCAN
            //while we already send FINISH_SCAN here. This may mess up the gLimHalScanState
            limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);
            limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);
            //Set the resume channel to Any valid channel (invalid). 
            //This will instruct HAL to set it to any previous valid channel.
            peSetResumeChannel(pMac, 0, 0);
            limSendHalFinishScanReq( pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE );
            //limSendHalFinishScanReq( pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE );
        }
    }//(eANI_BOOLEAN_TRUE == fScanning)

last:
    if( eHAL_STATUS_SUCCESS != status )
    {
        vos_mem_free( pBssDescr );
    }
    return;
} /****** end limCheckAndAddBssDescription() ******/



/**
 * limScanHashFunction()
 *
 *FUNCTION:
 * This function is called during scan hash entry operations
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  bssId - Received BSSid
 *
 * @return Hash index
 */

tANI_U8
limScanHashFunction(tSirMacAddr bssId)
{
    tANI_U16    i, hash = 0;

    for (i = 0; i < sizeof(tSirMacAddr); i++)
        hash += bssId[i];

    return hash % LIM_MAX_NUM_OF_SCAN_RESULTS;
} /****** end limScanHashFunction() ******/



/**
 * limInitHashTable()
 *
 *FUNCTION:
 * This function is called upon receiving SME_START_REQ
 * to initialize global cached scan hash table
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limInitHashTable(tpAniSirGlobal pMac)
{
    tANI_U16 i;
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
        pMac->lim.gLimCachedScanHashTable[i] = NULL;
} /****** end limInitHashTable() ******/



/**
 * limLookupNaddHashEntry()
 *
 *FUNCTION:
 * This function is called upon receiving a Beacon or
 * Probe Response frame during scan phase to store
 * received BSS description into scan result hash table.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pBssDescr - Pointer to BSS description to be
 *         added to the scan result hash table.
 * @param  action - Indicates action to be performed
 *         when same BSS description is found. This is
 *         dependent on whether unique scan result to
 *         be stored or not.
 *
 * @return None
 */

eHalStatus
limLookupNaddHashEntry(tpAniSirGlobal pMac,
                       tLimScanResultNode *pBssDescr, tANI_U8 action,
                       tANI_U8 dontUpdateAll)
{
    tANI_U8                  index, ssidLen = 0;
    tANI_U8                found = false;
    tLimScanResultNode *ptemp, *pprev;
    tSirMacCapabilityInfo *pSirCap, *pSirCapTemp;
    int idx, len;
    tANI_U8 *pbIe;
    tANI_S8  rssi = 0;

    index = limScanHashFunction(pBssDescr->bssDescription.bssId);
    ptemp = pMac->lim.gLimCachedScanHashTable[index];

    //ieFields start with TLV of SSID IE
    ssidLen = * ((tANI_U8 *) &pBssDescr->bssDescription.ieFields + 1);
    pSirCap = (tSirMacCapabilityInfo *)&pBssDescr->bssDescription.capabilityInfo;

    for (pprev = ptemp; ptemp; pprev = ptemp, ptemp = ptemp->next)
    {
        //For infrastructure, check BSSID and SSID. For IBSS, check more
        pSirCapTemp = (tSirMacCapabilityInfo *)&ptemp->bssDescription.capabilityInfo;
        if ((pSirCapTemp->ess == pSirCap->ess) && //matching ESS type first
            (vos_mem_compare( (tANI_U8 *) pBssDescr->bssDescription.bssId,
                      (tANI_U8 *) ptemp->bssDescription.bssId,
                      sizeof(tSirMacAddr))) &&   //matching BSSID
             // matching band to update new channel info
            (vos_chan_to_band(pBssDescr->bssDescription.channelId) ==
                      vos_chan_to_band(ptemp->bssDescription.channelId)) &&
            vos_mem_compare( ((tANI_U8 *) &pBssDescr->bssDescription.ieFields + 1),
                           ((tANI_U8 *) &ptemp->bssDescription.ieFields + 1),
                           (tANI_U8) (ssidLen + 1)) &&
            ((pSirCapTemp->ess) || //we are done for infrastructure
            //For IBSS, nwType and channelId
            (((pBssDescr->bssDescription.nwType ==
                                         ptemp->bssDescription.nwType) &&
            (pBssDescr->bssDescription.channelId ==
                                      ptemp->bssDescription.channelId))))
        )
        {
            // Found the same BSS description
            if (action == LIM_HASH_UPDATE)
            {
                if(dontUpdateAll)
                {
                   rssi = ptemp->bssDescription.rssi;
                }

                if(pBssDescr->bssDescription.fProbeRsp != ptemp->bssDescription.fProbeRsp)
                {
                    //We get a different, save the old frame WSC IE if it is there
                    idx = 0;
                    len = ptemp->bssDescription.length - sizeof(tSirBssDescription) + 
                       sizeof(tANI_U16) + sizeof(tANI_U32) - DOT11F_IE_WSCPROBERES_MIN_LEN - 2;
                    pbIe = (tANI_U8 *)ptemp->bssDescription.ieFields;
                    //Save WPS IE if it exists
                    pBssDescr->bssDescription.WscIeLen = 0;
                    while(idx < len)
                    {
                        if((DOT11F_EID_WSCPROBERES == pbIe[0]) &&
                           (0x00 == pbIe[2]) && (0x50 == pbIe[3]) && (0xf2 == pbIe[4]) && (0x04 == pbIe[5]))
                        {
                            //Found it
                            if((DOT11F_IE_WSCPROBERES_MAX_LEN - 2) >= pbIe[1])
                            {
                                vos_mem_copy(pBssDescr->bssDescription.WscIeProbeRsp,
                                   pbIe, pbIe[1] + 2);
                                pBssDescr->bssDescription.WscIeLen = pbIe[1] + 2;
                            }
                            break;
                        }
                        idx += pbIe[1] + 2;
                        pbIe += pbIe[1] + 2;
                    }
                }


                if(NULL != pMac->lim.gpLimMlmScanReq)
                {
                   if((pMac->lim.gpLimMlmScanReq->numSsid)&&
                      ( limIsNullSsid((tSirMacSSid *)((tANI_U8 *)
                      &pBssDescr->bssDescription.ieFields + 1))))
                      return eHAL_STATUS_FAILURE;
                }

                // Delete this entry
                if (ptemp == pMac->lim.gLimCachedScanHashTable[index])
                    pprev = pMac->lim.gLimCachedScanHashTable[index] = ptemp->next;
                else
                    pprev->next = ptemp->next;

                pMac->lim.gLimMlmScanResultLength -=
                    ptemp->bssDescription.length + sizeof(tANI_U16);

                vos_mem_free(ptemp);
            }
            found = true;
            break;
        }
    }

    //for now, only rssi, we can add more if needed
    if ((action == LIM_HASH_UPDATE) && dontUpdateAll && rssi)
    {
        pBssDescr->bssDescription.rssi = rssi;
    }

    // Add this BSS description at same index
    if (pprev == pMac->lim.gLimCachedScanHashTable[index])
    {
        pBssDescr->next = pMac->lim.gLimCachedScanHashTable[index];
        pMac->lim.gLimCachedScanHashTable[index] = pBssDescr;
    }
    else
    {
        pBssDescr->next = pprev->next;
        pprev->next = pBssDescr;
    }
    pMac->lim.gLimMlmScanResultLength +=
        pBssDescr->bssDescription.length + sizeof(tANI_U16);

    PELOG2(limLog(pMac, LOG2, FL("Added new BSS description size %d TOT %d BSS id"),
           pBssDescr->bssDescription.length,
           pMac->lim.gLimMlmScanResultLength);
    limPrintMacAddr(pMac, pBssDescr->bssDescription.bssId, LOG2);)

    // Send new BSS found indication to HDD if CFG option is set
    if (!found) limSendSmeNeighborBssInd(pMac, pBssDescr);

    //
    // TODO: IF applicable, do we need to send:
    // Mesg - eWNI_SME_WM_STATUS_CHANGE_NTF
    // Status change code - eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP
    //
    return eHAL_STATUS_SUCCESS;
}



/**
 * limDeleteHashEntry()
 *
 *FUNCTION:
 * This function is called upon to delete
 * a BSS description from scan result hash table.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * Yet to find the utility of the function
 *
 * @param  pBssDescr - Pointer to BSS description to be
 *         deleted from the scan result hash table.
 *
 * @return None
 */

void    limDeleteHashEntry(tLimScanResultNode *pBssDescr)
{
} /****** end limDeleteHashEntry() ******/


#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/**
 * limInitLfrHashTable()
 *
 *FUNCTION:
 * This function is called upon receiving SME_START_REQ
 * to initialize global cached Lfr scan hash table
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limInitLfrHashTable(tpAniSirGlobal pMac)
{
    tANI_U16 i;
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
        pMac->lim.gLimCachedLfrScanHashTable[i] = NULL;
} /****** end limInitLfrHashTable() ******/



/**
 * limLookupNaddLfrHashEntry()
 *
 *FUNCTION:
 * This function is called upon receiving a Beacon or
 * Probe Response frame during Lfr scan phase from FW to store
 * received BSS description into Lfr scan result hash table.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pBssDescr - Pointer to BSS description to be
 *         added to the Lfr scan result hash table.
 * @param  action - Indicates action to be performed
 *         when same BSS description is found. This is
 *         dependent on whether unique scan result to
 *         be stored or not.
 *
 * @return None
 */

eHalStatus
limLookupNaddLfrHashEntry(tpAniSirGlobal pMac,
                          tLimScanResultNode *pBssDescr, tANI_U8 action,
                          tANI_U8 dontUpdateAll)
{
    tANI_U8                  index, ssidLen = 0;
    tLimScanResultNode *ptemp, *pprev;
    tSirMacCapabilityInfo *pSirCap, *pSirCapTemp;
    int idx, len;
    tANI_U8 *pbIe;
    tANI_S8  rssi = 0;

    index = limScanHashFunction(pBssDescr->bssDescription.bssId);
    ptemp = pMac->lim.gLimCachedLfrScanHashTable[index];

    //ieFields start with TLV of SSID IE
    ssidLen = * ((tANI_U8 *) &pBssDescr->bssDescription.ieFields + 1);
    pSirCap = (tSirMacCapabilityInfo *)&pBssDescr->bssDescription.capabilityInfo;

    for (pprev = ptemp; ptemp; pprev = ptemp, ptemp = ptemp->next)
    {
        //For infrastructure, check BSSID and SSID. For IBSS, check more
        pSirCapTemp = (tSirMacCapabilityInfo *)&ptemp->bssDescription.capabilityInfo;
        if ((pSirCapTemp->ess == pSirCap->ess) && //matching ESS type first
            (vos_mem_compare( (tANI_U8 *) pBssDescr->bssDescription.bssId,
                      (tANI_U8 *) ptemp->bssDescription.bssId,
                      sizeof(tSirMacAddr))) &&   //matching BSSID
            (pBssDescr->bssDescription.channelId ==
                                      ptemp->bssDescription.channelId) &&
            vos_mem_compare( ((tANI_U8 *) &pBssDescr->bssDescription.ieFields + 1),
                           ((tANI_U8 *) &ptemp->bssDescription.ieFields + 1),
                           (tANI_U8) (ssidLen + 1)) &&
            ((pSirCapTemp->ess) || //we are done for infrastructure
            //For IBSS, nwType and channelId
            (((pBssDescr->bssDescription.nwType ==
                                         ptemp->bssDescription.nwType) &&
            (pBssDescr->bssDescription.channelId ==
                                      ptemp->bssDescription.channelId))))
        )
        {
            // Found the same BSS description
            if (action == LIM_HASH_UPDATE)
            {
                if(dontUpdateAll)
                {
                   rssi = ptemp->bssDescription.rssi;
                }

                if(pBssDescr->bssDescription.fProbeRsp != ptemp->bssDescription.fProbeRsp)
                {
                    //We get a different, save the old frame WSC IE if it is there
                    idx = 0;
                    len = ptemp->bssDescription.length - sizeof(tSirBssDescription) +
                       sizeof(tANI_U16) + sizeof(tANI_U32) - DOT11F_IE_WSCPROBERES_MIN_LEN - 2;
                    pbIe = (tANI_U8 *)ptemp->bssDescription.ieFields;
                    //Save WPS IE if it exists
                    pBssDescr->bssDescription.WscIeLen = 0;
                    while(idx < len)
                    {
                        if((DOT11F_EID_WSCPROBERES == pbIe[0]) &&
                           (0x00 == pbIe[2]) && (0x50 == pbIe[3]) &&
                           (0xf2 == pbIe[4]) && (0x04 == pbIe[5]))
                        {
                            //Found it
                            if((DOT11F_IE_WSCPROBERES_MAX_LEN - 2) >= pbIe[1])
                            {
                                vos_mem_copy( pBssDescr->bssDescription.WscIeProbeRsp,
                                   pbIe, pbIe[1] + 2);
                                pBssDescr->bssDescription.WscIeLen = pbIe[1] + 2;
                            }
                            break;
                        }
                        idx += pbIe[1] + 2;
                        pbIe += pbIe[1] + 2;
                    }
                }


                if(NULL != pMac->lim.gpLimMlmScanReq)
                {
                   if((pMac->lim.gpLimMlmScanReq->numSsid)&&
                      ( limIsNullSsid((tSirMacSSid *)((tANI_U8 *)
                      &pBssDescr->bssDescription.ieFields + 1))))
                      return eHAL_STATUS_FAILURE;
                }

                // Delete this entry
                if (ptemp == pMac->lim.gLimCachedLfrScanHashTable[index])
                    pprev = pMac->lim.gLimCachedLfrScanHashTable[index] = ptemp->next;
                else
                    pprev->next = ptemp->next;

                pMac->lim.gLimMlmLfrScanResultLength -=
                    ptemp->bssDescription.length + sizeof(tANI_U16);

                vos_mem_free(ptemp);
            }
            break;
        }
    }

    //for now, only rssi, we can add more if needed
    if ((action == LIM_HASH_UPDATE) && dontUpdateAll && rssi)
    {
        pBssDescr->bssDescription.rssi = rssi;
    }

    // Add this BSS description at same index
    if (pprev == pMac->lim.gLimCachedLfrScanHashTable[index])
    {
        pBssDescr->next = pMac->lim.gLimCachedLfrScanHashTable[index];
        pMac->lim.gLimCachedLfrScanHashTable[index] = pBssDescr;
    }
    else
    {
        pBssDescr->next = pprev->next;
        pprev->next = pBssDescr;
    }
    pMac->lim.gLimMlmLfrScanResultLength +=
        pBssDescr->bssDescription.length + sizeof(tANI_U16);

    PELOG2(limLog(pMac, LOG2, FL("Added new BSS description size %d TOT %d BSS id\n"),
           pBssDescr->bssDescription.length,
           pMac->lim.gLimMlmLfrScanResultLength);
    limPrintMacAddr(pMac, pBssDescr->bssDescription.bssId, LOG2);)

    //
    // TODO: IF applicable, do we need to send:
    // Mesg - eWNI_SME_WM_STATUS_CHANGE_NTF
    // Status change code - eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP
    //
    return eHAL_STATUS_SUCCESS;
}



/**
 * limDeleteLfrHashEntry()
 *
 *FUNCTION:
 * This function is called upon to delete
 * a BSS description from LFR scan result hash table.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * Yet to find the utility of the function
 *
 * @param  pBssDescr - Pointer to BSS description to be
 *         deleted from the LFR scan result hash table.
 *
 * @return None
 */

void    limDeleteLfrHashEntry(tLimScanResultNode *pBssDescr)
{
} /****** end limDeleteLfrHashEntry() ******/

#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD

/**
 * limCopyScanResult()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() while
 * sending SME_SCAN_RSP with scan result to HDD.
 *
 *LOGIC:
 * This function traverses the scan list stored in scan hash table
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pDest - Destination pointer
 *
 * @return None
 */

void
limCopyScanResult(tpAniSirGlobal pMac, tANI_U8 *pDest)
{
    tLimScanResultNode    *ptemp;
    tANI_U16 i;
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
    {
        if ((ptemp = pMac->lim.gLimCachedScanHashTable[i]) != NULL)
        {
            while(ptemp)
            {
                /// Copy entire BSS description including length
                vos_mem_copy( pDest,
                              (tANI_U8 *) &ptemp->bssDescription,
                              ptemp->bssDescription.length + 2);
                pDest += ptemp->bssDescription.length + 2;
                ptemp = ptemp->next;
            }
        }
    }
} /****** end limCopyScanResult() ******/



/**
 * limDeleteCachedScanResults()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_SCAN_REQ with fresh scan result flag set.
 *
 *LOGIC:
 * This function traverses the scan list stored in scan hash table
 * and deletes the entries if any
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limDeleteCachedScanResults(tpAniSirGlobal pMac)
{
    tLimScanResultNode    *pNode, *pNextNode;
    tANI_U16 i;

    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
    {
        if ((pNode = pMac->lim.gLimCachedScanHashTable[i]) != NULL)
        {
            while (pNode)
            {
                pNextNode = pNode->next;

                // Delete the current node
                vos_mem_free(pNode);

                pNode = pNextNode;
            }
        }
    }

    pMac->lim.gLimSmeScanResultLength = 0;
} /****** end limDeleteCachedScanResults() ******/



/**
 * limReInitScanResults()
 *
 *FUNCTION:
 * This function is called delete exisiting scan results
 * and initialize the scan hash table
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limReInitScanResults(tpAniSirGlobal pMac)
{
    limLog(pMac, LOG1, FL("Re initialize scan hash table."));
    limDeleteCachedScanResults(pMac);
    limInitHashTable(pMac);

    // !!LAC - need to clear out the global scan result length
    // since the list was just purged from the hash table.
    pMac->lim.gLimMlmScanResultLength = 0;

} /****** end limReInitScanResults() ******/
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/**
 * limDeleteCachedLfrScanResults()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_SCAN_REQ with flush scan result flag set for LFR.
 *
 *LOGIC:
 * This function traverses the scan list stored in lfr scan hash
 * table and deletes the entries if any
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limDeleteCachedLfrScanResults(tpAniSirGlobal pMac)
{
    tLimScanResultNode    *pNode, *pNextNode;
    tANI_U16 i;
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
    {
        if ((pNode = pMac->lim.gLimCachedLfrScanHashTable[i]) != NULL)
        {
            while (pNode)
            {
                pNextNode = pNode->next;

                // Delete the current node
                vos_mem_free(pNode);

                pNode = pNextNode;
            }
        }
    }

    pMac->lim.gLimSmeLfrScanResultLength = 0;
} /****** end limDeleteCachedLfrScanResults() ******/



/**
 * limReInitLfrScanResults()
 *
 *FUNCTION:
 * This function is called delete exisiting scan results
 * and initialize the lfr scan hash table
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limReInitLfrScanResults(tpAniSirGlobal pMac)
{
    limLog(pMac, LOG1, FL("Re initialize lfr scan hash table."));
    limDeleteCachedLfrScanResults(pMac);
    limInitLfrHashTable(pMac);

    // !!LAC - need to clear out the global scan result length
    // since the list was just purged from the hash table.
    pMac->lim.gLimMlmLfrScanResultLength = 0;

} /****** end limReInitLfrScanResults() ******/
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD
