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

/*
 *
 * This file limProcessProbeRspFrame.cc contains the code
 * for processing Probe Response Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniApi.h"
#include "wniCfg.h"
#include "aniGlobal.h"
#include "schApi.h"
#include "utilsApi.h"
#include "limApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"
#include "limSendMessages.h"

#include "parserApi.h"

tSirRetStatus
limValidateIEInformationInProbeRspFrame (tpAniSirGlobal pMac,
                                         tANI_U8 *pRxPacketInfo)
{
   tSirRetStatus       status = eSIR_SUCCESS;
   tANI_U8             *pFrame;
   tANI_U32            nFrame;
   tANI_U32            nMissingRsnBytes;

   /* Validate a Probe response frame for malformed frame.
    * If the frame is malformed then do not consider as it
    * may cause problem fetching wrong IE values
    */
   if (WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) < (SIR_MAC_B_PR_SSID_OFFSET + SIR_MAC_MIN_IE_LEN))
   {
      return eSIR_FAILURE;
   }

   pFrame = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
   nFrame = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
   nMissingRsnBytes = 0;

   status = ValidateAndRectifyIEs(pMac, pFrame, nFrame, &nMissingRsnBytes);
   if ( status == eSIR_SUCCESS )
   {
       WDA_GET_RX_MPDU_LEN(pRxPacketInfo) += nMissingRsnBytes;
   }

   return status;
}

/**
 * limProcessProbeRspFrame
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Probe Response frame reception.
 *
 *LOGIC:
 * This function processes received Probe Response frame.
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * 1. Frames with out-of-order IEs are dropped.
 * 2. In case of IBSS, join 'success' makes MLM state machine
 *    transition into 'BSS started' state. This may have to change
 *    depending on supporting what kinda Authentication in IBSS.
 *
 * @param pMac   Pointer to Global MAC structure
 * @param  *pRxPacketInfo  A pointer to Buffer descriptor + associated PDUs
 * @return None
 */


void
limProcessProbeRspFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tANI_U8                 *pBody;
    tANI_U32                frameLen = 0;
    tSirMacAddr             currentBssId;
    tpSirMacMgmtHdr         pHdr;
    tSirProbeRespBeacon    *pProbeRsp;
    tANI_U8 qosEnabled =    false;
    tANI_U8 wmeEnabled =    false;

    if (!psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL") );
        return;
    }
    limLog(pMac,LOG1,"SessionId:%d ProbeRsp Frame is received",
                psessionEntry->peSessionId);


    pProbeRsp = vos_mem_vmalloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pProbeRsp )
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory in limProcessProbeRspFrame") );
        return;
    }

    pProbeRsp->ssId.length              = 0;
    pProbeRsp->wpa.length               = 0;
    pProbeRsp->propIEinfo.apName.length = 0;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);


   limLog(pMac, LOG2,
             FL("Received Probe Response frame with length=%d from "),
             WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);

   if (!pMac->fScanOffload)
   {
       if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
       {
           vos_mem_vfree(pProbeRsp);
           return;
       }
   }

   // Validate IE information before processing Probe Response Frame
   if (limValidateIEInformationInProbeRspFrame(pMac, pRxPacketInfo)
       != eSIR_SUCCESS)
   {
       PELOG1(limLog(pMac, LOG1,
                 FL("Parse error ProbeResponse, length=%d"), frameLen);)
       vos_mem_vfree(pProbeRsp);
       return;
   }

    /**
     * Expect Probe Response only when
     * 1. STA is in scan mode waiting for Beacon/Probe response or
     * 2. STA is waiting for Beacon/Probe Response to announce
     *    join success or
     * 3. STA is in IBSS mode in BSS started state or
     * 4. STA/AP is in learn mode
     * 5. STA in link established state. In this state, the probe response is
     *     expected for two scenarios:
     *     -- As part of heart beat mechanism, probe req is sent out
     *     -- If QoS Info IE in beacon has a different count for EDCA Params,
     *         and EDCA IE is not present in beacon,
     *         then probe req is sent out to get the EDCA params.
     *
     * Ignore Probe Response frame in all other states
     */

   // TO SUPPORT BT-AMP
    if (((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||   //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) ||     //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE) ||            //mlm state check should be global - 18th oct 
        (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE) ||
        (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) )||
        ((GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_STA_IN_IBSS_ROLE) &&
        (psessionEntry->limMlmState == eLIM_MLM_BSS_STARTED_STATE)) ||
        pMac->fScanOffload)
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        if (pMac->lim.gLimBackgroundScanMode == eSIR_ROAMING_SCAN)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                      FL("Probe Resp Frame Received: BSSID " MAC_ADDRESS_STR " (RSSI %d)"),
                      MAC_ADDR_ARRAY(pHdr->bssId),
                      (uint)abs((tANI_S8)WDA_GET_RX_RSSI_DB(pRxPacketInfo)));
        }

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp) == eSIR_FAILURE ||
            !pProbeRsp->ssidPresent) // Enforce Mandatory IEs
        {
            PELOG1(limLog(pMac, LOG1,
               FL("Parse error ProbeResponse, length=%d"),
               frameLen);)
            vos_mem_vfree(pProbeRsp);
            return;
        }

        if (pMac->fScanOffload)
        {
            limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo,
                    eANI_BOOLEAN_FALSE, eANI_BOOLEAN_TRUE);
        }

        //To Support BT-AMP
        if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||    //mlm state check should be global - 18th oct
            (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE))
            limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo, 
               ((pMac->lim.gLimHalScanState == eLIM_HAL_SCANNING_STATE) ? eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE), eANI_BOOLEAN_TRUE);
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)           //mlm state check should be global - 18th oct
        {
        }
        else if (psessionEntry->limMlmState ==
                                     eLIM_MLM_WT_JOIN_BEACON_STATE)
        {
            if( psessionEntry->beacon != NULL )//Either Beacon/probe response is required. Hence store it in same buffer.
            {
                vos_mem_free(psessionEntry->beacon);
                psessionEntry->beacon = NULL;
            }
            psessionEntry->bcnLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

            psessionEntry->beacon = vos_mem_malloc(psessionEntry->bcnLen);
            if ( NULL == psessionEntry->beacon )
            {
                PELOGE(limLog(pMac, LOGE,
                              FL("Unable to allocate memory to store beacon"));)
            }
            else
            {
                //Store the Beacon/ProbeRsp. This is sent to csr/hdd in join cnf response. 
                vos_mem_copy(psessionEntry->beacon,
                             WDA_GET_RX_MPDU_DATA(pRxPacketInfo),
                             psessionEntry->bcnLen);
            }

            // STA in WT_JOIN_BEACON_STATE
            limCheckAndAnnounceJoinSuccess(pMac, pProbeRsp, pHdr, psessionEntry);
        }
        else if(psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)
        {
            tpDphHashNode pStaDs = NULL;
            /**
             * Check if this Probe Response is for
            * our Probe Request sent upon reaching
            * heart beat threshold
            */
            #if 0
            if (wlan_cfgGetStr(pMac,
                          WNI_CFG_BSSID,
                          currentBssId,
                          &cfg) != eSIR_SUCCESS)
            {
                /// Could not get BSSID from CFG. Log error.
                limLog(pMac, LOGP, FL("could not retrieve BSSID"));
            }
            #endif //TO SUPPORT BT-AMP
            sirCopyMacAddr(currentBssId,psessionEntry->bssId);

            if ( !vos_mem_compare(currentBssId, pHdr->bssId, sizeof(tSirMacAddr)) )
            {
                vos_mem_vfree(pProbeRsp);
                return;
            }

            if (!LIM_IS_CONNECTION_ACTIVE(psessionEntry))
            {
                limLog(pMac, LOGW,
                    FL("Received Probe Resp from AP. So it is alive!!"));

                if (pProbeRsp->HTInfo.present)
                    limReceivedHBHandler(pMac, (tANI_U8)pProbeRsp->HTInfo.primaryChannel, psessionEntry);
                else
                    limReceivedHBHandler(pMac, (tANI_U8)pProbeRsp->channelNumber, psessionEntry);
            }

            
            if (psessionEntry->limSystemRole == eLIM_STA_ROLE)
            {
                if (pProbeRsp->channelSwitchPresent ||
                    pProbeRsp->propIEinfo.propChannelSwitchPresent)
                {
                    limUpdateChannelSwitch(pMac, pProbeRsp, psessionEntry);
                }
                else if (psessionEntry->gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING)
                {
                    limCancelDot11hChannelSwitch(pMac, psessionEntry);
                }
            }
        
            
            /**
            * Now Process EDCA Parameters, if EDCAParamSet count is different.
            *     -- While processing beacons in link established state if it is determined that
            *         QoS Info IE has a different count for EDCA Params,
            *         and EDCA IE is not present in beacon,
            *         then probe req is sent out to get the EDCA params.
            */

            pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

            limGetQosMode(psessionEntry, &qosEnabled);
            limGetWmeMode(psessionEntry, &wmeEnabled);
           PELOG2(limLog(pMac, LOG2,
                    FL("wmeEdcaPresent: %d wmeEnabled: %d, edcaPresent: %d, qosEnabled: %d,  edcaParams.qosInfo.count: %d schObject.gLimEdcaParamSetCount: %d"),
                          pProbeRsp->wmeEdcaPresent, wmeEnabled, pProbeRsp->edcaPresent, qosEnabled,
                          pProbeRsp->edcaParams.qosInfo.count, psessionEntry->gLimEdcaParamSetCount);)
            if (((pProbeRsp->wmeEdcaPresent && wmeEnabled) ||
                (pProbeRsp->edcaPresent && qosEnabled)) &&
                (pProbeRsp->edcaParams.qosInfo.count != psessionEntry->gLimEdcaParamSetCount))
            {
                if (schBeaconEdcaProcess(pMac, &pProbeRsp->edcaParams, psessionEntry) != eSIR_SUCCESS)
                    PELOGE(limLog(pMac, LOGE, FL("EDCA parameter processing error"));)
                else if (pStaDs != NULL)
                {
                    // If needed, downgrade the EDCA parameters
                    limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry); 

                    if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE)
                        limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
                    else
                        limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
                }
                else
                    PELOGE(limLog(pMac, LOGE, FL("Self Entry missing in Hash Table"));)

            }

           if (psessionEntry->fWaitForProbeRsp == true)
           {
               limLog(pMac, LOGW, FL("Checking probe response for capability change\n") );
               limDetectChangeInApCapabilities(pMac, pProbeRsp, psessionEntry);
           }
        }
        else if ((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) &&
                 (psessionEntry->limMlmState == eLIM_MLM_BSS_STARTED_STATE))
                limHandleIBSScoalescing(pMac, pProbeRsp, pRxPacketInfo,psessionEntry);
    } // if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) || ...

    vos_mem_vfree(pProbeRsp);
    // Ignore Probe Response frame in all other states
    return;
} /*** end limProcessProbeRspFrame() ***/


void
limProcessProbeRspFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo)
{
    tANI_U8                 *pBody;
    tANI_U32                frameLen = 0;
    tpSirMacMgmtHdr         pHdr;
    tSirProbeRespBeacon    *pProbeRsp;

    pProbeRsp = vos_mem_vmalloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pProbeRsp )
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory in limProcessProbeRspFrameNoSession") );
        return;
    }

    pProbeRsp->ssId.length              = 0;
    pProbeRsp->wpa.length               = 0;
    pProbeRsp->propIEinfo.apName.length = 0;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);


    limLog(pMac, LOG2,
             FL("Received Probe Response frame with length=%d from "),
             WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (!(WDA_GET_OFFLOADSCANLEARN(pRxPacketInfo) ||
          WDA_GET_ROAMCANDIDATEIND(pRxPacketInfo)))
    {
#endif
        if (!pMac->fScanOffload)
        {
            if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
            {
                vos_mem_vfree(pProbeRsp);
                return;
            }
        }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    }
#endif
     // Validate IE information before processing Probe Response Frame
    if (limValidateIEInformationInProbeRspFrame(pMac, pRxPacketInfo)
        != eSIR_SUCCESS)
    {
       PELOG1(limLog(pMac, LOG1,FL("Parse error ProbeResponse, length=%d"),
              frameLen);)
       vos_mem_vfree(pProbeRsp);
       return;
    }
    /*  Since there is no psessionEntry, PE cannot be in the following states:
     *   - eLIM_MLM_WT_JOIN_BEACON_STATE
     *   - eLIM_MLM_LINK_ESTABLISHED_STATE
     *   - eLIM_MLM_BSS_STARTED_STATE
     *  Hence, expect Probe Response only when
     *   1. STA is in scan mode waiting for Beacon/Probe response 
     *   2. LFR logic in FW sends up candidate frames
     *  
     *  Ignore Probe Response frame in all other states
     */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (WDA_GET_OFFLOADSCANLEARN(pRxPacketInfo))
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        if (pMac->lim.gLimBackgroundScanMode == eSIR_ROAMING_SCAN)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                      FL("Probe Resp Frame Received: BSSID " MAC_ADDRESS_STR " (RSSI %d)"),
                      MAC_ADDR_ARRAY(pHdr->bssId),
                      (uint)abs((tANI_S8)WDA_GET_RX_RSSI_DB(pRxPacketInfo)));
        }

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp) == eSIR_FAILURE)
        {
            limLog(pMac, LOG1, FL("Parse error ProbeResponse, length=%d\n"), frameLen);
            vos_mem_vfree(pProbeRsp);
            return;
        }
        limLog( pMac, LOG2, FL("Save this probe rsp in LFR cache"));
        limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo,
                                     eANI_BOOLEAN_FALSE, eANI_BOOLEAN_TRUE);
    }
    else
#endif
    if (pMac->fScanOffload)
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp)
                == eSIR_FAILURE)
        {
            limLog(pMac, LOG1,
                    FL("Parse error ProbeResponse, length=%d\n"), frameLen);
            vos_mem_vfree(pProbeRsp);
            return;
        }
        limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo,
                eANI_BOOLEAN_FALSE, eANI_BOOLEAN_TRUE);
    }
    else if( (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE)  ||     //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE) )
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        if (pMac->lim.gLimBackgroundScanMode == eSIR_ROAMING_SCAN)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                      FL("Probe Resp Frame Received: BSSID " MAC_ADDRESS_STR " (RSSI %d)"),
                      MAC_ADDR_ARRAY(pHdr->bssId),
                      (uint)abs((tANI_S8)WDA_GET_RX_RSSI_DB(pRxPacketInfo)));
        }

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp) == eSIR_FAILURE)
        {
            limLog(pMac, LOG1, FL("Parse error ProbeResponse, length=%d"), frameLen);
            vos_mem_vfree(pProbeRsp);
            return;
        }

        if( (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
             (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) )
            limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_TRUE);
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)
        {
        }
    } 
    vos_mem_vfree(pProbeRsp);
    return;
} /*** end limProcessProbeRspFrameNew() ***/
