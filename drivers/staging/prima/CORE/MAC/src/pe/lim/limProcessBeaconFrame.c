/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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
 * This file limProcessBeaconFrame.cc contains the code
 * for processing Received Beacon Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniCfg.h"
#include "aniGlobal.h"
#include "cfgApi.h"
#include "schApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"

/**
 * limProcessBeaconFrame
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon Beacon
 * frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * 1. Beacons received in 'normal' state in IBSS are handled by
 *    Beacon Processing module.
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to RX packet info structure
 * @return None
 */

void
limProcessBeaconFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tpSirMacMgmtHdr      pHdr;
    tSchBeaconStruct    *pBeacon;

    pMac->lim.gLimNumBeaconsRcvd++;

    /* here is it required to increment session specific heartBeat beacon counter */  


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);


    limLog(pMac, LOG2, FL("Received Beacon frame with length=%d from "),
           WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);

    if (!pMac->fScanOffload)
    {
        if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
            return;
    }

    /**
     * Expect Beacon only when
     * 1. STA is in Scan mode waiting for Beacon/Probe response or
     * 2. STA is waiting for Beacon/Probe Respose Frame
     *    to announce join success.
     * 3. STA/AP is in Learn mode
     */
    if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE) ||
        (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE)
        || pMac->fScanOffload
        )
    {
        pBeacon = vos_mem_vmalloc(sizeof(tSchBeaconStruct));
        if ( NULL == pBeacon )
        {
            limLog(pMac, LOGE, FL("Unable to allocate memory in limProcessBeaconFrame") );
            return;
        }

        // Parse received Beacon
        if (sirConvertBeaconFrame2Struct(pMac, (tANI_U8 *) pRxPacketInfo,
                                         pBeacon) != eSIR_SUCCESS)
        {
            // Received wrongly formatted/invalid Beacon.
            // Ignore it and move on.
            limLog(pMac, LOGW,
                   FL("Received invalid Beacon in state %d"),
                   psessionEntry->limMlmState);
            limPrintMlmState(pMac, LOGW,  psessionEntry->limMlmState);
            if ((!psessionEntry->currentBssBeaconCnt) &&
               (sirCompareMacAddr( psessionEntry->bssId, pHdr->sa)))
                limParseBeaconForTim(pMac, (tANI_U8 *) pRxPacketInfo, psessionEntry);

            vos_mem_vfree(pBeacon);
            return;
        }
        /*during scanning, when any session is active, and beacon/Pr belongs to
          one of the session, fill up the following, TBD - HB couter */
        if ((!psessionEntry->lastBeaconDtimPeriod) &&
            (sirCompareMacAddr( psessionEntry->bssId, pBeacon->bssid)))
        {
            vos_mem_copy(( tANI_U8* )&psessionEntry->lastBeaconTimeStamp,
                         ( tANI_U8* )pBeacon->timeStamp, sizeof(tANI_U64) );
            psessionEntry->lastBeaconDtimCount = pBeacon->tim.dtimCount;
            psessionEntry->lastBeaconDtimPeriod= pBeacon->tim.dtimPeriod;
            psessionEntry->currentBssBeaconCnt++;
        }

        MTRACE(macTrace(pMac, TRACE_CODE_RX_MGMT_TSF, 0, pBeacon->timeStamp[0]);)
        MTRACE(macTrace(pMac, TRACE_CODE_RX_MGMT_TSF, 0, pBeacon->timeStamp[1]);)

        if (pMac->fScanOffload)
        {
            limCheckAndAddBssDescription(pMac, pBeacon, pRxPacketInfo,
                    eANI_BOOLEAN_FALSE, eANI_BOOLEAN_TRUE);

        }

        if ((pMac->lim.gLimMlmState  == eLIM_MLM_WT_PROBE_RESP_STATE) ||
            (pMac->lim.gLimMlmState  == eLIM_MLM_PASSIVE_SCAN_STATE))
        {
            limCheckAndAddBssDescription(pMac, pBeacon, pRxPacketInfo,
                  ((pMac->lim.gLimHalScanState == eLIM_HAL_SCANNING_STATE) ?
                    eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE),
                    eANI_BOOLEAN_FALSE);
            /* Calling dfsChannelList which will convert DFS channel
             * to Active channel for x secs if this channel is DFS channel */
             limSetDFSChannelList(pMac, pBeacon->channelNumber,
                                    &pMac->lim.dfschannelList);
        }
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)
        {
        }
        else if (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE)
        {
            if( psessionEntry->beacon != NULL )
            {
                vos_mem_free(psessionEntry->beacon);
                psessionEntry->beacon = NULL;
             }
             psessionEntry->bcnLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
             psessionEntry->beacon = vos_mem_malloc(psessionEntry->bcnLen);
             if ( NULL == psessionEntry->beacon )
             {
                PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store beacon"));)
              }
              else
              {
                //Store the Beacon/ProbeRsp. This is sent to csr/hdd in join cnf response. 
                vos_mem_copy(psessionEntry->beacon, WDA_GET_RX_MPDU_DATA(pRxPacketInfo),
                             psessionEntry->bcnLen);

               }
             
             // STA in WT_JOIN_BEACON_STATE (IBSS)
            limCheckAndAnnounceJoinSuccess(pMac, pBeacon, pHdr,psessionEntry);
        } // if (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE)
        vos_mem_vfree(pBeacon);
    } // if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) || ...
    else
    {
        // Ignore Beacon frame in all other states
        if (psessionEntry->limMlmState == eLIM_MLM_JOINED_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_BSS_STARTED_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_WT_AUTH_FRAME2_STATE ||
            psessionEntry->limMlmState == eLIM_MLM_WT_AUTH_FRAME3_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_WT_AUTH_FRAME4_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_AUTH_RSP_TIMEOUT_STATE ||
            psessionEntry->limMlmState == eLIM_MLM_AUTHENTICATED_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_WT_ASSOC_RSP_STATE ||
            psessionEntry->limMlmState == eLIM_MLM_WT_REASSOC_RSP_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_ASSOCIATED_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_REASSOCIATED_STATE ||
            psessionEntry->limMlmState  == eLIM_MLM_WT_ASSOC_CNF_STATE ||
            limIsReassocInProgress(pMac,psessionEntry)) {
            // nothing unexpected about beacon in these states
            pMac->lim.gLimNumBeaconsIgnored++;
        }
        else
        {
            limLog(pMac, LOG1, FL("Received Beacon in unexpected state %d"),
                   psessionEntry->limMlmState);
            limPrintMlmState(pMac, LOG1, psessionEntry->limMlmState);
#ifdef WLAN_DEBUG                    
            pMac->lim.gLimUnexpBcnCnt++;
#endif
        }
    }

    return;
} /*** end limProcessBeaconFrame() ***/


/**---------------------------------------------------------------
\fn     limProcessBeaconFrameNoSession
\brief  This function is called by limProcessMessageQueue()
\       upon Beacon reception. 
\
\param pMac
\param *pRxPacketInfo    - A pointer to Rx packet info structure
\return None
------------------------------------------------------------------*/
void
limProcessBeaconFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo)
{
    tpSirMacMgmtHdr      pHdr;
    tSchBeaconStruct    *pBeacon;

    pMac->lim.gLimNumBeaconsRcvd++;
    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);

    limLog(pMac, LOG2, FL("Received Beacon frame with length=%d from "),
           WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);

    if (!pMac->fScanOffload)
    {
        if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
            return;
    }

    /**
     * No session has been established. Expect Beacon only when
     * 1. STA is in Scan mode waiting for Beacon/Probe response or
     * 2. STA/AP is in Learn mode
     */
    if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE))
    {
        pBeacon = vos_mem_vmalloc(sizeof(tSchBeaconStruct));
        if ( NULL == pBeacon )
        {
            limLog(pMac, LOGE, FL("Unable to allocate memory in limProcessBeaconFrameNoSession") );
            return;
        }

        if (sirConvertBeaconFrame2Struct(pMac, (tANI_U8 *) pRxPacketInfo, pBeacon) != eSIR_SUCCESS)
        {
            // Received wrongly formatted/invalid Beacon. Ignore and move on. 
            limLog(pMac, LOGW, FL("Received invalid Beacon in global MLM state %d"), pMac->lim.gLimMlmState);
            limPrintMlmState(pMac, LOGW,  pMac->lim.gLimMlmState);
            vos_mem_vfree(pBeacon);
            return;
        }

        if ( (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
             (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) )
        {
            limCheckAndAddBssDescription(pMac, pBeacon, pRxPacketInfo,
                                         eANI_BOOLEAN_TRUE, eANI_BOOLEAN_FALSE);
            /* Calling dfsChannelList which will convert DFS channel
             * to Active channel for x secs if this channel is DFS channel */
            limSetDFSChannelList(pMac, pBeacon->channelNumber,
                                    &pMac->lim.dfschannelList);
        }
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)
        {
        }  // end of eLIM_MLM_LEARN_STATE)       
        vos_mem_vfree(pBeacon);
    } // end of (eLIM_MLM_WT_PROBE_RESP_STATE) || (eLIM_MLM_PASSIVE_SCAN_STATE)
    else
    {
        limLog(pMac, LOG1, FL("Rcvd Beacon in unexpected MLM state %s (%d)"),
               limMlmStateStr(pMac->lim.gLimMlmState), pMac->lim.gLimMlmState);
        limPrintMlmState(pMac, LOG1, pMac->lim.gLimMlmState);
#ifdef WLAN_DEBUG                    
        pMac->lim.gLimUnexpBcnCnt++;
#endif
    }

    return;
} /*** end limProcessBeaconFrameNoSession() ***/

