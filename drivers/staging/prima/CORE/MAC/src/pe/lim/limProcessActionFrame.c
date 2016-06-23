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
 * This file limProcessActionFrame.cc contains the code
 * for processing Action Frame.
 * Author:      Michael Lui
 * Date:        05/23/03
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#include "wniApi.h"
#include "sirApi.h"
#include "aniGlobal.h"
#include "wniCfgSta.h"
#include "schApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limSendSmeRspMessages.h"
#include "parserApi.h"
#include "limAdmitControl.h"
#include "wmmApsd.h"
#include "limSendMessages.h"
#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif
#include "limSessionUtils.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#endif
#include "wlan_qct_wda.h"


#define BA_DEFAULT_TX_BUFFER_SIZE 64

typedef enum
{
  LIM_ADDBA_RSP = 0,
  LIM_ADDBA_REQ = 1
}tLimAddBaValidationReqType;

/* Note: The test passes if the STAUT stops sending any frames, and no further
 frames are transmitted on this channel by the station when the AP has sent
 the last 6 beacons, with the channel switch information elements as seen
 with the sniffer.*/
#define SIR_CHANSW_TX_STOP_MAX_COUNT 6
/**-----------------------------------------------------------------
\fn     limStopTxAndSwitchChannel
\brief  Stops the transmission if channel switch mode is silent and
        starts the channel switch timer.

\param  pMac
\return NONE
-----------------------------------------------------------------*/
void limStopTxAndSwitchChannel(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tANI_U8 isFullPowerRequested = 0;
    tpPESession psessionEntry;

    psessionEntry = peFindSessionBySessionId( pMac , sessionId );

    if( NULL == psessionEntry )
    {
      limLog(pMac, LOGE, FL("Session %d  not active\n "), sessionId);
      return;
    }

    PELOG1(limLog(pMac, LOG1, FL("Channel switch Mode == %d"),
                       psessionEntry->gLimChannelSwitch.switchMode);)

    if (psessionEntry->gLimChannelSwitch.switchMode == eSIR_CHANSW_MODE_SILENT ||
        psessionEntry->gLimChannelSwitch.switchCount <= SIR_CHANSW_TX_STOP_MAX_COUNT)
    {
        /* Freeze the transmission */
        limFrameTransmissionControl(pMac, eLIM_TX_ALL, eLIM_STOP_TX);

        /*Request for Full power only if the device is in powersave*/
        if(!limIsSystemInActiveState(pMac))
        {
            /* Request Full Power */
            limSendSmePreChannelSwitchInd(pMac);
            isFullPowerRequested = 1;
        }
    }
    else
    {
        /* Resume the transmission */
        limFrameTransmissionControl(pMac, eLIM_TX_ALL, eLIM_RESUME_TX);
    }

    pMac->lim.limTimers.gLimChannelSwitchTimer.sessionId = sessionId;
    /* change the channel immediatly only if the channel switch count is 0 and the 
     * device is not in powersave 
     * If the device is in powersave channel switch should happen only after the
     * device comes out of the powersave */
    if (psessionEntry->gLimChannelSwitch.switchCount == 0) 
    {
        if(limIsSystemInActiveState(pMac))
        {
            limProcessChannelSwitchTimeout(pMac);
        }
        else if(!isFullPowerRequested)
        {
            /* If the Full power is already not requested 
             * Request Full Power so the channel switch happens 
             * after device comes to full power */
            limSendSmePreChannelSwitchInd(pMac);
        }
        return;
    }
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, sessionId, eLIM_CHANNEL_SWITCH_TIMER));


    if (tx_timer_activate(&pMac->lim.limTimers.gLimChannelSwitchTimer) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("tx_timer_activate failed"));
    }
    return;
}

/**------------------------------------------------------------
\fn     limStartChannelSwitch
\brief  Switches the channel if switch count == 0, otherwise
        starts the timer for channel switch and stops BG scan
        and heartbeat timer tempororily.

\param  pMac
\param  psessionEntry
\return NONE
------------------------------------------------------------*/
tSirRetStatus limStartChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    PELOG1(limLog(pMac, LOG1, FL("Starting the channel switch"));)
    
    /*If channel switch is already running and it is on a different session, just return*/  
    /*This need to be removed for MCC */
    if( limIsChanSwitchRunning (pMac) &&
        psessionEntry->gLimSpecMgmt.dot11hChanSwState != eLIM_11H_CHANSW_RUNNING )
    {
       limLog(pMac, LOGW, FL("Ignoring channel switch on session %d"), psessionEntry->peSessionId);
       return eSIR_SUCCESS;
    }
    psessionEntry->channelChangeCSA =  LIM_SWITCH_CHANNEL_CSA;
    /* Deactivate and change reconfigure the timeout value */
    //limDeactivateAndChangeTimer(pMac, eLIM_CHANNEL_SWITCH_TIMER);
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, psessionEntry->peSessionId, eLIM_CHANNEL_SWITCH_TIMER));
    if (tx_timer_deactivate(&pMac->lim.limTimers.gLimChannelSwitchTimer) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("tx_timer_deactivate failed!"));
        return eSIR_FAILURE;
    }

    if (tx_timer_change(&pMac->lim.limTimers.gLimChannelSwitchTimer,
                psessionEntry->gLimChannelSwitch.switchTimeoutValue,
                            0) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("tx_timer_change failed "));
        return eSIR_FAILURE;
    }

    /* Follow the channel switch, forget about the previous quiet. */
    //If quiet is running, chance is there to resume tx on its timeout.
    //so stop timer for a safer side.
    if (psessionEntry->gLimSpecMgmt.quietState == eLIM_QUIET_BEGIN)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, psessionEntry->peSessionId, eLIM_QUIET_TIMER));
        if (tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietTimer) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("tx_timer_deactivate failed"));
            return eSIR_FAILURE;
        }
    }
    else if (psessionEntry->gLimSpecMgmt.quietState == eLIM_QUIET_RUNNING)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, psessionEntry->peSessionId, eLIM_QUIET_BSS_TIMER));
        if (tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietBssTimer) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("tx_timer_deactivate failed"));
            return eSIR_FAILURE;
        }
    }
    psessionEntry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;

    /* Prepare for 11h channel switch */
    limPrepareFor11hChannelSwitch(pMac, psessionEntry);

    /** Dont add any more statements here as we posted finish scan request
     * to HAL, wait till we get the response
     */
    return eSIR_SUCCESS;
}


/**
 *  __limProcessChannelSwitchActionFrame
 *
 *FUNCTION:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to packet info structure
 * @return None
 */

static void

__limProcessChannelSwitchActionFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{

    tpSirMacMgmtHdr         pHdr;
    tANI_U8                 *pBody;
    tDot11fChannelSwitch    *pChannelSwitchFrame;
    tANI_U16                beaconPeriod;
    tANI_U32                val;
    tANI_U32                frameLen;
    tANI_U32                nStatus;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    PELOG3(limLog(pMac, LOG3, FL("Received Channel switch action frame"));)
    if (!psessionEntry->lim11hEnable)
        return;

    pChannelSwitchFrame = vos_mem_malloc(sizeof(*pChannelSwitchFrame));
    if (NULL == pChannelSwitchFrame)
    {
        limLog(pMac, LOGE,
            FL("AllocateMemory failed"));
        return;
    }

    /* Unpack channel switch frame */
    nStatus = dot11fUnpackChannelSwitch(pMac, pBody, frameLen, pChannelSwitchFrame);

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGE,
            FL( "Failed to unpack and parse an 11h-CHANSW Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen);
        vos_mem_free(pChannelSwitchFrame);
        return;
    }
    else if(DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
            FL( "There were warnings while unpacking an 11h-CHANSW Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen);
    }

    if (vos_mem_compare((tANI_U8 *) &psessionEntry->bssId,
                        (tANI_U8 *) &pHdr->sa,
                        sizeof(tSirMacAddr)))
    {
        #if 0
        if (wlan_cfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL, &val) != eSIR_SUCCESS)
        {
            vos_mem_free(pChannelSwitchFrame);
            limLog(pMac, LOGP, FL("could not retrieve Beacon interval"));
            return;
        }
        #endif// TO SUPPORT BT-AMP

        /* copy the beacon interval from psessionEntry*/
        val = psessionEntry->beaconParams.beaconInterval;

        beaconPeriod = (tANI_U16) val;

        psessionEntry->gLimChannelSwitch.primaryChannel = pChannelSwitchFrame->ChanSwitchAnn.newChannel;
        psessionEntry->gLimChannelSwitch.switchCount = pChannelSwitchFrame->ChanSwitchAnn.switchCount;
        psessionEntry->gLimChannelSwitch.switchTimeoutValue = SYS_MS_TO_TICKS(beaconPeriod) *
                                                         psessionEntry->gLimChannelSwitch.switchCount;
        psessionEntry->gLimChannelSwitch.switchMode = pChannelSwitchFrame->ChanSwitchAnn.switchMode;
#ifdef WLAN_FEATURE_11AC
        if ( pChannelSwitchFrame->WiderBWChanSwitchAnn.present && psessionEntry->vhtCapability)
        {
            psessionEntry->gLimWiderBWChannelSwitch.newChanWidth = pChannelSwitchFrame->WiderBWChanSwitchAnn.newChanWidth;
            psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq0 = pChannelSwitchFrame->WiderBWChanSwitchAnn.newCenterChanFreq0;
            psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq1 = pChannelSwitchFrame->WiderBWChanSwitchAnn.newCenterChanFreq1;
        }
#endif

       PELOG3(limLog(pMac, LOG3, FL("Rcv Chnl Swtch Frame: Timeout in %d ticks"),
                             psessionEntry->gLimChannelSwitch.switchTimeoutValue);)

        /* Only primary channel switch element is present */
        psessionEntry->gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
        psessionEntry->gLimChannelSwitch.secondarySubBand = PHY_SINGLE_CHANNEL_CENTERED;

        if (psessionEntry->htSupportedChannelWidthSet)
        {
            if ((pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset == PHY_DOUBLE_CHANNEL_LOW_PRIMARY) ||
                (pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY))
            {
                psessionEntry->gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
                psessionEntry->gLimChannelSwitch.secondarySubBand = pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset;
            }
#ifdef WLAN_FEATURE_11AC
            if(psessionEntry->vhtCapability && pChannelSwitchFrame->WiderBWChanSwitchAnn.present)
            {
                if (pChannelSwitchFrame->WiderBWChanSwitchAnn.newChanWidth == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
                {
                    if (pChannelSwitchFrame->ExtChanSwitchAnn.present && ((pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset == PHY_DOUBLE_CHANNEL_LOW_PRIMARY) ||
                        (pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)))
                    {
                        psessionEntry->gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
                        psessionEntry->gLimChannelSwitch.secondarySubBand =
                           limGet11ACPhyCBState(pMac,
                                                psessionEntry->gLimChannelSwitch.primaryChannel,
                                                pChannelSwitchFrame->ExtChanSwitchAnn.secondaryChannelOffset,
                                                pChannelSwitchFrame->WiderBWChanSwitchAnn.newCenterChanFreq0,
                                                psessionEntry);
                    }
                }
            }
#endif
        }

    }
    else
    {
        PELOG1(limLog(pMac, LOG1, FL("LIM: Received action frame not from our BSS, dropping..."));)
    }

    if (eSIR_SUCCESS != limStartChannelSwitch(pMac, psessionEntry))
    {
        PELOG1(limLog(pMac, LOG1, FL("Could not start channel switch"));)
    }

    vos_mem_free(pChannelSwitchFrame);
    return;
} /*** end limProcessChannelSwitchActionFrame() ***/


#ifdef WLAN_FEATURE_11AC
static void
__limProcessOperatingModeActionFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{

    tpSirMacMgmtHdr         pHdr;
    tANI_U8                 *pBody;
    tDot11fOperatingMode    *pOperatingModeframe;
    tANI_U32                frameLen;
    tANI_U32                nStatus;
    tpDphHashNode           pSta;
    tANI_U16                aid;
    tANI_U8                 operMode;
    tANI_U32                channelBondingMode;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    limLog(pMac, LOG1, FL("Received Operating Mode action frame"));

    if( RF_CHAN_14 >= psessionEntry->currentOperChannel )
    {
        channelBondingMode = pMac->roam.configParam.channelBondingMode24GHz;
    }
    else
    {
        channelBondingMode = pMac->roam.configParam.channelBondingMode5GHz;
    }

    /* Do not update the channel bonding mode if channel bonding
     * mode is disabled in INI.
     */
    if(WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == channelBondingMode)
    {
        limLog(pMac, LOGW,
            FL("channel bonding disabled"));
        return;
    }

    pOperatingModeframe = vos_mem_malloc(sizeof(*pOperatingModeframe));
    if (NULL == pOperatingModeframe)
    {
        limLog(pMac, LOGE,
            FL("AllocateMemory failed"));
        return;
    }

    /* Unpack channel switch frame */
    nStatus = dot11fUnpackOperatingMode(pMac, pBody, frameLen, pOperatingModeframe);

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGE,
            FL( "Failed to unpack and parse an 11h-CHANSW Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen);
        vos_mem_free(pOperatingModeframe);
        return;
    }
    else if(DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
            FL( "There were warnings while unpacking an 11h-CHANSW Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen);
    }
    pSta = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);
    
    operMode = pSta->vhtSupportedChannelWidthSet ? eHT_CHANNEL_WIDTH_80MHZ : pSta->htSupportedChannelWidthSet ? eHT_CHANNEL_WIDTH_40MHZ: eHT_CHANNEL_WIDTH_20MHZ;
    if( operMode != pOperatingModeframe->OperatingMode.chanWidth)
    {
        limLog(pMac, LOGE, 
            FL(" received Chanwidth %d, staIdx = %d"),
            (pOperatingModeframe->OperatingMode.chanWidth ),
            pSta->staIndex);

        limLog(pMac, LOGE, 
            FL(" MAC - %0x:%0x:%0x:%0x:%0x:%0x"),
            pHdr->sa[0],
            pHdr->sa[1],
            pHdr->sa[2],
            pHdr->sa[3],
            pHdr->sa[4],
            pHdr->sa[5]);

        if(pOperatingModeframe->OperatingMode.chanWidth == eHT_CHANNEL_WIDTH_80MHZ)
        {
            pSta->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
            pSta->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_40MHZ;
        } 
        else if(pOperatingModeframe->OperatingMode.chanWidth == eHT_CHANNEL_WIDTH_40MHZ)
        {
            pSta->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
            pSta->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_40MHZ;
        }
        else if(pOperatingModeframe->OperatingMode.chanWidth == eHT_CHANNEL_WIDTH_20MHZ)
        {
            pSta->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
            pSta->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_20MHZ;
        }
        limCheckVHTOpModeChange( pMac, psessionEntry, 
                                 (pOperatingModeframe->OperatingMode.chanWidth), pSta->staIndex);\
    }
    vos_mem_free(pOperatingModeframe);
    return;
}
#endif

static void
__limProcessAddTsReq(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
}


static void
__limProcessAddTsRsp(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tSirAddtsRspInfo addts;
    tSirRetStatus    retval;
    tpSirMacMgmtHdr  pHdr;
    tpDphHashNode    pSta;
    tANI_U16         aid;
    tANI_U32         frameLen;
    tANI_U8          *pBody;
    tpLimTspecInfo   tspecInfo;
    tANI_U8          ac; 
    tpDphHashNode    pStaDs = NULL;
    tANI_U8          rspReqd = 1;
    tANI_U32   cfgLen;
    tSirMacAddr  peerMacAddr;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);


    PELOGW(limLog(pMac, LOGW, "Recv AddTs Response");)
    if ((psessionEntry->limSystemRole == eLIM_AP_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE))
    {
        PELOGW(limLog(pMac, LOGW, FL("AddTsRsp recvd at AP: ignoring"));)
        return;
    }

    pSta = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);
    if (pSta == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("Station context not found - ignoring AddTsRsp"));)
        return;
    }

    retval = sirConvertAddtsRsp2Struct(pMac, pBody, frameLen, &addts);
    if (retval != eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("AddTsRsp parsing failed (error %d)"), retval);)
        return;
    }

    // don't have to check for qos/wme capabilities since we wouldn't have this
    // flag set otherwise
    if (! pMac->lim.gLimAddtsSent)
    {
        // we never sent an addts request!
        PELOGW(limLog(pMac, LOGW, "Recvd AddTsRsp but no request was ever sent - ignoring");)
        return;
    }

    if (pMac->lim.gLimAddtsReq.req.dialogToken != addts.dialogToken)
    {
        limLog(pMac, LOGW, "AddTsRsp: token mismatch (got %d, exp %d) - ignoring",
               addts.dialogToken, pMac->lim.gLimAddtsReq.req.dialogToken);
        return;
    }

    /*
     * for successful addts reponse, try to add the classifier.
     * if this fails for any reason, we should send a delts request to the ap
     * for now, its ok not to send a delts since we are going to add support for
     * multiple tclas soon and until then we won't send any addts requests with
     * multiple tclas elements anyway.
     * In case of addClassifier failure, we just let the addts timer run out
     */
    if (((addts.tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA) ||
         (addts.tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_BOTH)) &&
        (addts.status == eSIR_MAC_SUCCESS_STATUS))
    {
        // add the classifier - this should always succeed
        if (addts.numTclas > 1) // currently no support for multiple tclas elements
        {
            limLog(pMac, LOGE, FL("Sta %d: Too many Tclas (%d), only 1 supported"),
                   aid, addts.numTclas);
            return;
        }
        else if (addts.numTclas == 1)
        {
            limLog(pMac, LOGW, "AddTs Response from STA %d: tsid %d, UP %d, OK!", aid,
                   addts.tspec.tsinfo.traffic.tsid, addts.tspec.tsinfo.traffic.userPrio);
        }
    }
    limLog(pMac, LOGW, "Recv AddTsRsp: tsid %d, UP %d, status %d ",
          addts.tspec.tsinfo.traffic.tsid, addts.tspec.tsinfo.traffic.userPrio,
          addts.status);

    // deactivate the response timer
    limDeactivateAndChangeTimer(pMac, eLIM_ADDTS_RSP_TIMER);

    if (addts.status != eSIR_MAC_SUCCESS_STATUS)
    {
        limLog(pMac, LOGW, "Recv AddTsRsp: tsid %d, UP %d, status %d ",
              addts.tspec.tsinfo.traffic.tsid, addts.tspec.tsinfo.traffic.userPrio,
              addts.status);
        limSendSmeAddtsRsp(pMac, true, addts.status, psessionEntry, addts.tspec, 
                psessionEntry->smeSessionId, psessionEntry->transactionId);

        // clear the addts flag
        pMac->lim.gLimAddtsSent = false;

        return;
    }
#ifdef FEATURE_WLAN_ESE
    if (addts.tsmPresent)
    {
        limLog(pMac, LOGW, "TSM IE Present");
        psessionEntry->eseContext.tsm.tid = addts.tspec.tsinfo.traffic.userPrio;
        vos_mem_copy(&psessionEntry->eseContext.tsm.tsmInfo,
                                         &addts.tsmIE,sizeof(tSirMacESETSMIE));
#ifdef FEATURE_WLAN_ESE_UPLOAD
        limSendSmeTsmIEInd(pMac, psessionEntry, addts.tsmIE.tsid,
                           addts.tsmIE.state, addts.tsmIE.msmt_interval);
#else
        limActivateTSMStatsTimer(pMac, psessionEntry);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
    }
#endif
    /* Since AddTS response was successful, check for the PSB flag
     * and directional flag inside the TS Info field. 
     * An AC is trigger enabled AC if the PSB subfield is set to 1  
     * in the uplink direction.
     * An AC is delivery enabled AC if the PSB subfield is set to 1 
     * in the downlink direction.
     * An AC is trigger and delivery enabled AC if the PSB subfield  
     * is set to 1 in the bi-direction field.
     */
    if (addts.tspec.tsinfo.traffic.psb == 1)
        limSetTspecUapsdMask(pMac, &addts.tspec.tsinfo, SET_UAPSD_MASK);
    else 
        limSetTspecUapsdMask(pMac, &addts.tspec.tsinfo, CLEAR_UAPSD_MASK);


    /* ADDTS success, so AC is now admitted. We shall now use the default
     * EDCA parameters as advertised by AP and send the updated EDCA params
     * to HAL. 
     */
    ac = upToAc(addts.tspec.tsinfo.traffic.userPrio);
    if(addts.tspec.tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |= (1 << ac);
    }
    else if(addts.tspec.tsinfo.traffic.direction == SIR_MAC_DIRECTION_DNLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |= (1 << ac);
    }
    else if(addts.tspec.tsinfo.traffic.direction == SIR_MAC_DIRECTION_BIDIR)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |= (1 << ac);
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |= (1 << ac);
    }

    limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
    if (pStaDs != NULL)
    {
        if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE) 
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
        else
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
    }
    else
        limLog(pMac, LOGE, FL("Self entry missing in Hash Table "));

        
    sirCopyMacAddr(peerMacAddr,psessionEntry->bssId);

    //if schedule is not present then add TSPEC with svcInterval as 0.
    if(!addts.schedulePresent)
      addts.schedule.svcInterval = 0;
    if(eSIR_SUCCESS != limTspecAdd(pMac, pSta->staAddr, pSta->assocId, &addts.tspec,  addts.schedule.svcInterval, &tspecInfo))
    {
        PELOGE(limLog(pMac, LOGE, FL("Adding entry in lim Tspec Table failed "));)
        limSendDeltsReqActionFrame(pMac, peerMacAddr, rspReqd, &addts.tspec.tsinfo, &addts.tspec,
                psessionEntry);
        pMac->lim.gLimAddtsSent = false;
        return;   //Error handling. send the response with error status. need to send DelTS to tear down the TSPEC status.
    }
    if((addts.tspec.tsinfo.traffic.accessPolicy != SIR_MAC_ACCESSPOLICY_EDCA) ||
       ((upToAc(addts.tspec.tsinfo.traffic.userPrio) < MAX_NUM_AC)))
    {
        retval = limSendHalMsgAddTs(pMac, pSta->staIndex, tspecInfo->idx, addts.tspec, psessionEntry->peSessionId);
        if(eSIR_SUCCESS != retval)
        {
            limAdmitControlDeleteTS(pMac, pSta->assocId, &addts.tspec.tsinfo, NULL, &tspecInfo->idx);
    
            // Send DELTS action frame to AP        
            cfgLen = sizeof(tSirMacAddr);
            limSendDeltsReqActionFrame(pMac, peerMacAddr, rspReqd, &addts.tspec.tsinfo, &addts.tspec,
                    psessionEntry);
            limSendSmeAddtsRsp(pMac, true, retval, psessionEntry, addts.tspec,
                    psessionEntry->smeSessionId, psessionEntry->transactionId);
            pMac->lim.gLimAddtsSent = false;
            return;
        }
        PELOGW(limLog(pMac, LOGW, FL("AddTsRsp received successfully(UP %d, TSID %d)"),
           addts.tspec.tsinfo.traffic.userPrio, addts.tspec.tsinfo.traffic.tsid);)
    }
    else
    {
        PELOGW(limLog(pMac, LOGW, FL("AddTsRsp received successfully(UP %d, TSID %d)"),
               addts.tspec.tsinfo.traffic.userPrio, addts.tspec.tsinfo.traffic.tsid);)
        PELOGW(limLog(pMac, LOGW, FL("no ACM: Bypass sending WDA_ADD_TS_REQ to HAL "));)
        // Use the smesessionId and smetransactionId from the PE session context
        limSendSmeAddtsRsp(pMac, true, eSIR_SME_SUCCESS, psessionEntry, addts.tspec,
                psessionEntry->smeSessionId, psessionEntry->transactionId);
    }

    // clear the addts flag
    pMac->lim.gLimAddtsSent = false;
    return;
}


static void
__limProcessDelTsReq(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tSirRetStatus    retval;
    tSirDeltsReqInfo delts;
    tpSirMacMgmtHdr  pHdr;
    tpDphHashNode    pSta;
    tANI_U32              frameLen;
    tANI_U16              aid;
    tANI_U8              *pBody;
    tANI_U8               tsStatus;
    tSirMacTSInfo   *tsinfo;
    tANI_U8 tspecIdx;
    tANI_U8  ac;
    tpDphHashNode  pStaDs = NULL;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    pSta = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);
    if (pSta == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("Station context not found - ignoring DelTs"));)
        return;
    }

    // parse the delts request
    retval = sirConvertDeltsReq2Struct(pMac, pBody, frameLen, &delts);
    if (retval != eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("DelTs parsing failed (error %d)"), retval);)
        return;
    }

    if (delts.wmeTspecPresent)
    {
        if ((!psessionEntry->limWmeEnabled) || (! pSta->wmeEnabled))
        {
            PELOGW(limLog(pMac, LOGW, FL("Ignoring delts request: wme not enabled/capable"));)
            return;
        }
        PELOG2(limLog(pMac, LOG2, FL("WME Delts received"));)
    }
    else if ((psessionEntry->limQosEnabled) && pSta->lleEnabled)
        {
        PELOG2(limLog(pMac, LOG2, FL("11e QoS Delts received"));)
        }
    else if ((psessionEntry->limWsmEnabled) && pSta->wsmEnabled)
        {
        PELOG2(limLog(pMac, LOG2, FL("WSM Delts received"));)
        }
    else
    {
        PELOGW(limLog(pMac, LOGW, FL("Ignoring delts request: qos not enabled/capable"));)
        return;
    }

    tsinfo = delts.wmeTspecPresent ? &delts.tspec.tsinfo : &delts.tsinfo;

    // if no Admit Control, ignore the request
    if ((tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA))
    {
    
        if (upToAc(tsinfo->traffic.userPrio) >= MAX_NUM_AC)
        {
            limLog(pMac, LOGW, FL("DelTs with UP %d has no AC - ignoring request"),
                   tsinfo->traffic.userPrio);
            return;
        }
    }

    if ((psessionEntry->limSystemRole != eLIM_AP_ROLE) &&
        (psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE))
        limSendSmeDeltsInd(pMac, &delts, aid,psessionEntry);

    // try to delete the TS
    if (eSIR_SUCCESS != limAdmitControlDeleteTS(pMac, pSta->assocId, tsinfo, &tsStatus, &tspecIdx))
    {
        PELOGW(limLog(pMac, LOGW, FL("Unable to Delete TS"));)
        return;
    }

    else if ((tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA) ||
             (tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_BOTH))
    {
      //Edca only for now.
    }
    else
    {
      //send message to HAL to delete TS
      if(eSIR_SUCCESS != limSendHalMsgDelTs(pMac,
                                            pSta->staIndex,
                                            tspecIdx,
                                            delts,
                                            psessionEntry->peSessionId,
                                            psessionEntry->bssId))
      {
        limLog(pMac, LOGW, FL("DelTs with UP %d failed in limSendHalMsgDelTs - ignoring request"),
                         tsinfo->traffic.userPrio);
         return;
      }
    }

    /* We successfully deleted the TSPEC. Update the dynamic UAPSD Mask.
     * The AC for this TSPEC is no longer trigger enabled if this Tspec
     * was set-up in uplink direction only.
     * The AC for this TSPEC is no longer delivery enabled if this Tspec
     * was set-up in downlink direction only.
     * The AC for this TSPEC is no longer triiger enabled and delivery 
     * enabled if this Tspec was a bidirectional TSPEC.
     */
    limSetTspecUapsdMask(pMac, tsinfo, CLEAR_UAPSD_MASK);


    /* We're deleting the TSPEC.
     * The AC for this TSPEC is no longer admitted in uplink/downlink direction
     * if this TSPEC was set-up in uplink/downlink direction only.
     * The AC for this TSPEC is no longer admitted in both uplink and downlink
     * directions if this TSPEC was a bi-directional TSPEC.
     * If ACM is set for this AC and this AC is admitted only in downlink
     * direction, PE needs to downgrade the EDCA parameter 
     * (for the AC for which TS is being deleted) to the
     * next best AC for which ACM is not enabled, and send the
     * updated values to HAL. 
     */ 
    ac = upToAc(tsinfo->traffic.userPrio);

    if(tsinfo->traffic.direction == SIR_MAC_DIRECTION_UPLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &= ~(1 << ac);
    }
    else if(tsinfo->traffic.direction == SIR_MAC_DIRECTION_DNLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &= ~(1 << ac);
    }
    else if(tsinfo->traffic.direction == SIR_MAC_DIRECTION_BIDIR)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &= ~(1 << ac);
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &= ~(1 << ac);
    }

    limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
    if (pStaDs != NULL)
    {
        if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE) 
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
        else
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
    }
    else
        limLog(pMac, LOGE, FL("Self entry missing in Hash Table "));

    PELOG1(limLog(pMac, LOG1, FL("DeleteTS succeeded"));)

#ifdef FEATURE_WLAN_ESE
#ifdef FEATURE_WLAN_ESE_UPLOAD
    limSendSmeTsmIEInd(pMac, psessionEntry, 0, 0, 0);
#else
    limDeactivateAndChangeTimer(pMac,eLIM_TSM_TIMER);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
#endif

}

static void
__limProcessQosMapConfigureFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,
                                                     tpPESession psessionEntry)
{
     tpSirMacMgmtHdr  pHdr;
     tANI_U32         frameLen;
     tANI_U8          *pBody;
     tSirRetStatus    retval;
     pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
     pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
     frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
     retval = sirConvertQosMapConfigureFrame2Struct(pMac, pBody, frameLen,
                                                        &psessionEntry->QosMapSet);
     if (retval != eSIR_SUCCESS)
     {
         PELOGW(limLog(pMac, LOGE,
         FL("QosMapConfigure frame parsing failed (error %d)"), retval);)
         return;
     }
     limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo, psessionEntry, 0);
}

#ifdef ANI_SUPPORT_11H
/**
 *  limProcessBasicMeasReq
 *
 *FUNCTION:
 * This function is called by limProcessMeasurementRequestFrame()
 * when it received a Basic measurement Request action frame.
 * Station/BP receiving this should perform basic measurements
 * and then send Basic Measurement Report. AP should not perform
 * any measurements, and send report indicating refusal.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMeasReqFrame - A pointer to Basic Meas. Req structure
 * @return None
 */
static void
__limProcessBasicMeasReq(tpAniSirGlobal pMac,
                       tpSirMacMeasReqActionFrame pMeasReqFrame,
                       tSirMacAddr peerMacAddr)
{
    // TBD - Station shall perform basic measurements

    if (limSendMeasReportFrame(pMac,
                               pMeasReqFrame,
                               peerMacAddr) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("fail to send Basic Meas report "));)
        return;
    }
}


/**
 *  limProcessCcaMeasReq
 *
 *FUNCTION:
 * This function is called by limProcessMeasurementRequestFrame()
 * when it received a CCA measurement Request action frame.
 * Station/BP receiving this should perform CCA measurements
 * and then send CCA Measurement Report. AP should not perform
 * any measurements, and send report indicating refusal.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMeasReqFrame - A pointer to CCA Meas. Req structure
 * @return None
 */
static void
__limProcessCcaMeasReq(tpAniSirGlobal pMac,
                     tpSirMacMeasReqActionFrame pMeasReqFrame,
                     tSirMacAddr peerMacAddr)
{
    // TBD - Station shall perform cca measurements

    if (limSendMeasReportFrame(pMac,
                               pMeasReqFrame,
                               peerMacAddr) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("fail to send CCA Meas report "));)
        return;
    }
}


/**
 *  __limProcessRpiMeasReq
 *
 *FUNCTION:
 * This function is called by limProcessMeasurementRequestFrame()
 * when it received a RPI measurement Request action frame.
 * Station/BP/AP  receiving this shall not perform any measurements,
 * and send report indicating refusal.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMeasReqFrame - A pointer to RPI Meas. Req structure
 * @return None
 */
static void
__limProcessRpiMeasReq(tpAniSirGlobal pMac,
                     tpSirMacMeasReqActionFrame pMeasReqFrame,
                     tSirMacAddr peerMacAddr)
{
    if (limSendMeasReportFrame(pMac,
                               pMeasReqFrame,
                               peerMacAddr) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("fail to send RPI Meas report "));)
        return;
    }
}


/**
 *  __limProcessMeasurementRequestFrame
 *
 *FUNCTION:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to packet info structure
 * @return None
 */

static void
__limProcessMeasurementRequestFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo)
{
    tpSirMacMgmtHdr                       pHdr;
    tANI_U8                                    *pBody;
    tpSirMacMeasReqActionFrame            pMeasReqFrame;
    tANI_U32                                   frameLen;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    pMeasReqFrame = vos_mem_malloc(sizeof( tSirMacMeasReqActionFrame ));
    if (NULL == pMeasReqFrame)
    {
        limLog(pMac, LOGE,
            FL("limProcessMeasurementRequestFrame: AllocateMemory failed "));
        return;
    }

    if (sirConvertMeasReqFrame2Struct(pMac, pBody, pMeasReqFrame, frameLen) !=
        eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("Rcv invalid Measurement Request Action Frame "));)
        return;
    }


    switch(pMeasReqFrame->measReqIE.measType)
    {
        case SIR_MAC_BASIC_MEASUREMENT_TYPE:
            __limProcessBasicMeasReq(pMac, pMeasReqFrame, pHdr->sa);
            break;

        case SIR_MAC_CCA_MEASUREMENT_TYPE:
            __limProcessCcaMeasReq(pMac, pMeasReqFrame, pHdr->sa);
            break;

        case SIR_MAC_RPI_MEASUREMENT_TYPE:
            __limProcessRpiMeasReq(pMac, pMeasReqFrame, pHdr->sa);
            break;

        default:
            PELOG1(limLog(pMac, LOG1, FL("Unknown Measurement Type %d "),
                   pMeasReqFrame->measReqIE.measType);)
            break;
    }

} /*** end limProcessMeasurementRequestFrame ***/


/**
 *  limProcessTpcRequestFrame
 *
 *FUNCTION:
 *  This function is called upon receiving Tpc Request frame.
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to packet info structure
 * @return None
 */

static void
__limProcessTpcRequestFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo)
{
    tpSirMacMgmtHdr                       pHdr;
    tANI_U8                                    *pBody;
    tpSirMacTpcReqActionFrame             pTpcReqFrame;
    tANI_U32                                   frameLen;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    PELOG1(limLog(pMac, LOG1, FL("****LIM: Processing TPC Request from peer ****"));)

    pTpcReqFrame = vos_mem_malloc(sizeof( tSirMacTpcReqActionFrame ));
    if (NULL == pTpcReqFrame)
    {
        PELOGE(limLog(pMac, LOGE, FL("AllocateMemory failed "));)
        return;
    }

    if (sirConvertTpcReqFrame2Struct(pMac, pBody, pTpcReqFrame, frameLen) !=
        eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("Rcv invalid TPC Req Action Frame "));)
        return;
    }

    if (limSendTpcReportFrame(pMac,
                              pTpcReqFrame,
                              pHdr->sa) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("fail to send TPC Report Frame. "));)
        return;
    }
}
#endif


/**
 * \brief Validate an ADDBA Req from peer with respect
 * to our own BA configuration
 *
 * \sa __limValidateAddBAParameterSet
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param baParameterSet The ADDBA Parameter Set.
 *
 * \param pDelBAFlag this parameter is NULL except for call from processAddBAReq
 *        delBAFlag is set when entry already exists.
 *
 * \param reqType ADDBA Req v/s ADDBA Rsp
 * 1 - ADDBA Req
 * 0 - ADDBA Rsp
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */

static tSirMacStatusCodes
__limValidateAddBAParameterSet( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tDot11fFfAddBAParameterSet baParameterSet,
    tANI_U8 dialogueToken,
    tLimAddBaValidationReqType reqType ,
    tANI_U8* pDelBAFlag /*this parameter is NULL except for call from processAddBAReq*/)
{
  if(baParameterSet.tid >= STACFG_MAX_TC)
  {
      return eSIR_MAC_WME_INVALID_PARAMS_STATUS;
  }

  //check if there is already a BA session setup with this STA/TID while processing AddBaReq
  if((true == pSta->tcCfg[baParameterSet.tid].fUseBARx) &&
        (LIM_ADDBA_REQ == reqType))
  {
      //There is already BA session setup for STA/TID.
      limLog( pMac, LOGE,
          FL( "AddBAReq rcvd when there is already a session for this StaId = %d, tid = %d\n " ),
          pSta->staIndex, baParameterSet.tid);
      limPrintMacAddr( pMac, pSta->staAddr, LOGW );

      if(pDelBAFlag)
        *pDelBAFlag = true;
  }
  return eSIR_MAC_SUCCESS_STATUS;
}

/**
 * \brief Validate a DELBA Ind from peer with respect
 * to our own BA configuration
 *
 * \sa __limValidateDelBAParameterSet
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param baParameterSet The DELBA Parameter Set.
 *
 * \param pSta Runtime, STA-related configuration cached
 * in the HashNode object
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
static tSirMacStatusCodes 
__limValidateDelBAParameterSet( tpAniSirGlobal pMac,
    tDot11fFfDelBAParameterSet baParameterSet,
    tpDphHashNode pSta )
{
tSirMacStatusCodes statusCode = eSIR_MAC_STA_BLK_ACK_NOT_SUPPORTED_STATUS;

  // Validate if a BA is active for the requested TID
    if( pSta->tcCfg[baParameterSet.tid].fUseBATx ||
        pSta->tcCfg[baParameterSet.tid].fUseBARx )
    {
      statusCode = eSIR_MAC_SUCCESS_STATUS;

      limLog( pMac, LOGW,
          FL("Valid DELBA Ind received. Time to send WDA_DELBA_IND to HAL..."));
    }
    else
      limLog( pMac, LOGW,
          FL("Received an INVALID DELBA Ind for TID %d..."),
          baParameterSet.tid );

  return statusCode;
}

/**
 * \brief Process an ADDBA REQ
 *
 * \sa limProcessAddBAReq
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pRxPacketInfo Handle to the Rx packet info from HDD
 *
 * \return none
 *
 */
static void
__limProcessAddBAReq( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tDot11fAddBAReq frmAddBAReq;
    tpSirMacMgmtHdr pHdr;
    tpDphHashNode pSta;
    tSirMacStatusCodes status = eSIR_MAC_SUCCESS_STATUS;
    tANI_U16 aid;
    tANI_U32 frameLen, nStatus,val;
    tANI_U8 *pBody;
    tANI_U8 delBAFlag =0;

    pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
    pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
    frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );
    val = 0;

    // Unpack the received frame
    nStatus = dot11fUnpackAddBAReq( pMac, pBody, frameLen, &frmAddBAReq );
    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGE,
            FL("Failed to unpack and parse an ADDBA Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen );

        PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)

        // Without an unpacked request we cannot respond, so silently ignore the request
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW,
            FL( "There were warnings while unpacking an ADDBA Request (0x%08x, %d bytes):"),
            nStatus,
            frameLen );

        PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
    }

    psessionEntry->amsduSupportedInBA = frmAddBAReq.AddBAParameterSet.amsduSupported;

    pSta = dphLookupHashEntry( pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable );
    if( pSta == NULL )
    {
        limLog( pMac, LOGE,
            FL( "STA context not found - ignoring ADDBA from " ));
        limPrintMacAddr( pMac, pHdr->sa, LOGE );

        // FIXME - Should we do this?
        status = eSIR_MAC_INABLITY_TO_CONFIRM_ASSOC_STATUS;
        goto returnAfterError;
    }
    limLog( pMac, LOG1, FL( "ADDBA Req from STA "MAC_ADDRESS_STR " with AID %d"
                            " tid = %d policy = %d buffsize = %d"
                            " amsduSupported = %d"), MAC_ADDR_ARRAY(pHdr->sa),
                            aid, frmAddBAReq.AddBAParameterSet.tid,
                            frmAddBAReq.AddBAParameterSet.policy,
                            frmAddBAReq.AddBAParameterSet.bufferSize,
                            frmAddBAReq.AddBAParameterSet.amsduSupported);

    limLog( pMac, LOG1, FL( "ssn = %d fragNumber = %d" ),
                            frmAddBAReq.BAStartingSequenceControl.ssn,
                            frmAddBAReq.BAStartingSequenceControl.fragNumber);

#ifdef WLAN_SOFTAP_VSTA_FEATURE
    // we can only do BA on "hard" STAs
    if (!(IS_HWSTA_IDX(pSta->staIndex)))
    {
        status = eSIR_MAC_REQ_DECLINED_STATUS;
        goto returnAfterError;
    }
#endif //WLAN_SOFTAP_VSTA_FEATURE

    if (wlan_cfgGetInt(pMac, WNI_CFG_DEL_ALL_RX_TX_BA_SESSIONS_2_4_G_BTC, &val) !=
                    eSIR_SUCCESS)
    {
        limLog(pMac, LOGE,
               FL("Unable to get WNI_CFG_DEL_ALL_RX_TX_BA_SESSIONS_2_4_G_BTC"));
        val = 0;
    }
    if ((SIR_BAND_2_4_GHZ == limGetRFBand(psessionEntry->currentOperChannel)) &&
                    val)
    {
        limLog( pMac, LOGW,
            FL( "BTC disabled aggregation - ignoring ADDBA from " ));
        limPrintMacAddr( pMac, pHdr->sa, LOGW );

        status = eSIR_MAC_REQ_DECLINED_STATUS;
        goto returnAfterError;
    }

    // Now, validate the ADDBA Req
    if( eSIR_MAC_SUCCESS_STATUS !=
      (status = __limValidateAddBAParameterSet( pMac, pSta,
                                              frmAddBAReq.AddBAParameterSet,
                                              0, //dialogue token is don't care in request validation.
                                              LIM_ADDBA_REQ, &delBAFlag)))
        goto returnAfterError;

    //BA already set, so we need to delete it before adding new one.
    if(delBAFlag)
    {
        if( eSIR_SUCCESS != limPostMsgDelBAInd( pMac,
            pSta,
            (tANI_U8)frmAddBAReq.AddBAParameterSet.tid,
            eBA_RECIPIENT,psessionEntry))
        {
            status = eSIR_MAC_UNSPEC_FAILURE_STATUS;
            goto returnAfterError;
        }
    }

  // Check if the ADD BA Declined configuration is Disabled
    if ((pMac->lim.gAddBA_Declined & ( 1 << frmAddBAReq.AddBAParameterSet.tid ) )) {
        limLog( pMac, LOGE, FL( "Declined the ADDBA Req for the TID %d  " ),
                        frmAddBAReq.AddBAParameterSet.tid);
        status = eSIR_MAC_REQ_DECLINED_STATUS;
        goto returnAfterError;
    }

  //
  // Post WDA_ADDBA_REQ to HAL.
  // If HAL/HDD decide to allow this ADDBA Req session,
  // then this BA session is termed active
  //

  // Change the Block Ack state of this STA to wait for
  // ADDBA Rsp from HAL
  LIM_SET_STA_BA_STATE(pSta, frmAddBAReq.AddBAParameterSet.tid, eLIM_BA_STATE_WT_ADD_RSP);

  if (wlan_cfgGetInt(pMac, WNI_CFG_NUM_BUFF_ADVERT , &val) != eSIR_SUCCESS)
  {
        limLog(pMac, LOGP, FL("Unable to get WNI_CFG_NUM_BUFF_ADVERT"));
        return ;
  }


  if (frmAddBAReq.AddBAParameterSet.bufferSize)
  {
      frmAddBAReq.AddBAParameterSet.bufferSize =
            VOS_MIN(val, frmAddBAReq.AddBAParameterSet.bufferSize);
  }
  else
  {
      frmAddBAReq.AddBAParameterSet.bufferSize = val;
  }
  limLog( pMac, LOG1, FL( "ADDBAREQ NUMBUFF %d" ),
                        frmAddBAReq.AddBAParameterSet.bufferSize);

  if( eSIR_SUCCESS != limPostMsgAddBAReq( pMac,
        pSta,
        (tANI_U8) frmAddBAReq.DialogToken.token,
        (tANI_U8) frmAddBAReq.AddBAParameterSet.tid,
        (tANI_U8) frmAddBAReq.AddBAParameterSet.policy,
        frmAddBAReq.AddBAParameterSet.bufferSize,
        frmAddBAReq.BATimeout.timeout,
        (tANI_U16) frmAddBAReq.BAStartingSequenceControl.ssn,
        eBA_RECIPIENT,psessionEntry))
    status = eSIR_MAC_UNSPEC_FAILURE_STATUS;
  else
    return;

returnAfterError:

  //
  // Package LIM_MLM_ADDBA_RSP to MLME, with proper
  // status code. MLME will then send an ADDBA RSP
  // over the air to the peer MAC entity
  //
  if( eSIR_SUCCESS != limPostMlmAddBARsp( pMac,
        pHdr->sa,
        status,
        frmAddBAReq.DialogToken.token,
        (tANI_U8) frmAddBAReq.AddBAParameterSet.tid,
        (tANI_U8) frmAddBAReq.AddBAParameterSet.policy,
        frmAddBAReq.AddBAParameterSet.bufferSize,
        frmAddBAReq.BATimeout.timeout,psessionEntry))
  {
    limLog( pMac, LOGW,
        FL( "Failed to post LIM_MLM_ADDBA_RSP to " ));
    limPrintMacAddr( pMac, pHdr->sa, LOGW );
  }

}

/**
 * \brief Process an ADDBA RSP
 *
 * \sa limProcessAddBARsp
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pRxPacketInfo Handle to the packet info structure from HDD
 *
 * \return none
 *
 */
static void
__limProcessAddBARsp( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
tDot11fAddBARsp frmAddBARsp;
tpSirMacMgmtHdr pHdr;
tpDphHashNode pSta;
tSirMacReasonCodes reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;
tANI_U16 aid;
tANI_U32 frameLen, nStatus;
tANI_U8 *pBody;

  pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
  pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
  frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );

  pSta = dphLookupHashEntry( pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable );
  if( pSta == NULL )
  {
    limLog( pMac, LOGE,
        FL( "STA context not found - ignoring ADDBA from " ));
    limPrintMacAddr( pMac, pHdr->sa, LOGW );
    return;
  }

#ifdef WLAN_SOFTAP_VSTA_FEATURE
  // We can only do BA on "hard" STAs.  We should not have issued an ADDBA
  // Request, so we should never be processing a ADDBA Response
  if (!(IS_HWSTA_IDX(pSta->staIndex)))
  {
    return;
  }
#endif //WLAN_SOFTAP_VSTA_FEATURE

  // Unpack the received frame
  nStatus = dot11fUnpackAddBARsp( pMac, pBody, frameLen, &frmAddBARsp );
  if( DOT11F_FAILED( nStatus ))
  {
    limLog( pMac, LOGE,
        FL( "Failed to unpack and parse an ADDBA Response (0x%08x, %d bytes):"),
        nStatus,
        frameLen );

    PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
    goto returnAfterError;
  }
  else if ( DOT11F_WARNED( nStatus ) )
  {
    limLog( pMac, LOGW,
        FL( "There were warnings while unpacking an ADDBA Response (0x%08x, %d bytes):"),
        nStatus,
        frameLen );

    PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
  }

  limLog( pMac, LOG1, FL( "ADDBA Rsp from STA "MAC_ADDRESS_STR " with AID %d "
                          "tid = %d policy = %d buffsize = %d "
                          "amsduSupported = %d status = %d"),
                          MAC_ADDR_ARRAY(pHdr->sa), aid,
                          frmAddBARsp.AddBAParameterSet.tid,
                          frmAddBARsp.AddBAParameterSet.policy,
                          frmAddBARsp.AddBAParameterSet.bufferSize,
                          frmAddBARsp.AddBAParameterSet.amsduSupported,
                          frmAddBARsp.Status.status);
  //if there is no matchin dialougue token then ignore the response.

  if(eSIR_SUCCESS != limSearchAndDeleteDialogueToken(pMac, frmAddBARsp.DialogToken.token,
        pSta->assocId, frmAddBARsp.AddBAParameterSet.tid))
  {
      PELOGW(limLog(pMac, LOGE, FL("dialogueToken in received addBARsp did not match with outstanding requests"));)
      return;
  }

  // Check first if the peer accepted the ADDBA Req
  if( eSIR_MAC_SUCCESS_STATUS == frmAddBARsp.Status.status )
  {
    tANI_U32 val;
    if (wlan_cfgGetInt(pMac, WNI_CFG_NUM_BUFF_ADVERT , &val) != eSIR_SUCCESS)
    {
        limLog(pMac, LOG1, FL("Unable to get WNI_CFG_NUM_BUFF_ADVERT"));
        goto returnAfterError;
    }
    if (0 == frmAddBARsp.AddBAParameterSet.bufferSize)
        frmAddBARsp.AddBAParameterSet.bufferSize = val;
    else
        frmAddBARsp.AddBAParameterSet.bufferSize =
                    VOS_MIN(val, frmAddBARsp.AddBAParameterSet.bufferSize);
        limLog( pMac, LOG1,
            FL( "ADDBA RSP  Buffsize = %d" ),
            frmAddBARsp.AddBAParameterSet.bufferSize);
    // Now, validate the ADDBA Rsp
    if( eSIR_MAC_SUCCESS_STATUS !=
        __limValidateAddBAParameterSet( pMac, pSta,
                                       frmAddBARsp.AddBAParameterSet,
                                       (tANI_U8)frmAddBARsp.DialogToken.token,
                                       LIM_ADDBA_RSP, NULL))
      goto returnAfterError;
  }
  else
    goto returnAfterError;

  // Change STA state to wait for ADDBA Rsp from HAL
  LIM_SET_STA_BA_STATE(pSta, frmAddBARsp.AddBAParameterSet.tid, eLIM_BA_STATE_WT_ADD_RSP);

  //
  // Post WDA_ADDBA_REQ to HAL.
  // If HAL/HDD decide to allow this ADDBA Rsp session,
  // then this BA session is termed active
  //

  if( eSIR_SUCCESS != limPostMsgAddBAReq( pMac,
        pSta,
        (tANI_U8) frmAddBARsp.DialogToken.token,
        (tANI_U8) frmAddBARsp.AddBAParameterSet.tid,
        (tANI_U8) frmAddBARsp.AddBAParameterSet.policy,
        frmAddBARsp.AddBAParameterSet.bufferSize,
        frmAddBARsp.BATimeout.timeout,
        0,
        eBA_INITIATOR,psessionEntry))
    reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;
  else
    return;

returnAfterError:

  // TODO: Do we need to signal an error status to SME,
  // if status != eSIR_MAC_SUCCESS_STATUS

  // Restore STA "BA" State
  LIM_SET_STA_BA_STATE(pSta, frmAddBARsp.AddBAParameterSet.tid, eLIM_BA_STATE_IDLE);
  //
  // Need to send a DELBA IND to peer, who
  // would have setup a BA session with this STA
  //
  if( eSIR_MAC_SUCCESS_STATUS == frmAddBARsp.Status.status )
  {
    //
    // Package LIM_MLM_DELBA_REQ to MLME, with proper
    // status code. MLME will then send a DELBA IND
    // over the air to the peer MAC entity
    //
    if( eSIR_SUCCESS != limPostMlmDelBAReq( pMac,
          pSta,
          eBA_INITIATOR,
          (tANI_U8) frmAddBARsp.AddBAParameterSet.tid,
          reasonCode, psessionEntry))
    {
      limLog( pMac, LOGW,
          FL( "Failed to post LIM_MLM_DELBA_REQ to " ));
      limPrintMacAddr( pMac, pHdr->sa, LOGW );
    }
  }
}

/**
 * \brief Process a DELBA Indication
 *
 * \sa limProcessDelBAInd
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pRxPacketInfo Handle to the Rx packet info from HDD
 *
 * \return none
 *
 */
static void
__limProcessDelBAReq( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
tDot11fDelBAInd frmDelBAInd;
tpSirMacMgmtHdr pHdr;
tpDphHashNode pSta;
tANI_U16 aid;
tANI_U32 frameLen, nStatus;
tANI_U8 *pBody;

  pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
  pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
  frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );

  pSta = dphLookupHashEntry( pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable );
  if( pSta == NULL )
  {
    limLog( pMac, LOGE, FL( "STA context not found - ignoring DELBA from "));
    limPrintMacAddr( pMac, pHdr->sa, LOGW );
    return;
  }

  limLog( pMac, LOG1, FL( "DELBA Ind from STA with AID %d" ), aid );

  // Unpack the received frame
  nStatus = dot11fUnpackDelBAInd( pMac, pBody, frameLen, &frmDelBAInd );
  if( DOT11F_FAILED( nStatus ))
  {
    limLog( pMac, LOGE,
        FL( "Failed to unpack and parse a DELBA Indication (0x%08x, %d bytes):"),
        nStatus,
        frameLen );

    PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
    return;
  }
  else if ( DOT11F_WARNED( nStatus ) )
  {
    limLog( pMac, LOGW,
        FL( "There were warnings while unpacking a DELBA Indication (0x%08x, %d bytes):"),
        nStatus,
        frameLen );

    PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
  }

  limLog( pMac, LOG1,
      FL( "Received DELBA from: "MAC_ADDRESS_STR" for TID %d, Reason code %d" ),
      MAC_ADDR_ARRAY(pHdr->sa),
      frmDelBAInd.DelBAParameterSet.tid,
      frmDelBAInd.Reason.code );

  // Now, validate the DELBA Ind
  if( eSIR_MAC_SUCCESS_STATUS != __limValidateDelBAParameterSet( pMac,
                                             frmDelBAInd.DelBAParameterSet,
                                             pSta ))
      return;

  //
  // Post WDA_DELBA_IND to HAL and delete the
  // existing BA session
  //
  // NOTE - IEEE 802.11-REVma-D8.0, Section 7.3.1.16
  // is kind of confusing...
  //
  if( eSIR_SUCCESS != limPostMsgDelBAInd( pMac,
        pSta,
        (tANI_U8) frmDelBAInd.DelBAParameterSet.tid,
        (eBA_RECIPIENT == frmDelBAInd.DelBAParameterSet.initiator)?
          eBA_INITIATOR: eBA_RECIPIENT,psessionEntry))
    limLog( pMac, LOGE, FL( "Posting WDA_DELBA_IND to HAL failed "));

  return;

}

static void
__limProcessSMPowerSaveUpdate(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo ,tpPESession psessionEntry)
{

#if 0
        tpSirMacMgmtHdr                           pHdr;
        tDot11fSMPowerSave                    frmSMPower;
        tSirMacHTMIMOPowerSaveState  state;
        tpDphHashNode                             pSta;
        tANI_U16                                        aid;
        tANI_U32                                        frameLen, nStatus;
        tANI_U8                                          *pBody;

        pHdr = SIR_MAC_BD_TO_MPDUHEADER( pBd );
        pBody = SIR_MAC_BD_TO_MPDUDATA( pBd );
        frameLen = SIR_MAC_BD_TO_PAYLOAD_LEN( pBd );

        pSta = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable );
        if( pSta == NULL ) {
            limLog( pMac, LOGE,FL( "STA context not found - ignoring UpdateSM PSave Mode from " ));
            limPrintMacAddr( pMac, pHdr->sa, LOGW );
            return;
        }

        /**Unpack the received frame */
        nStatus = dot11fUnpackSMPowerSave( pMac, pBody, frameLen, &frmSMPower);

        if( DOT11F_FAILED( nStatus )) {
            limLog( pMac, LOGE, FL( "Failed to unpack and parse a Update SM Power (0x%08x, %d bytes):"),
                                                    nStatus, frameLen );
            PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
            return;
        }else if ( DOT11F_WARNED( nStatus ) ) {
            limLog(pMac, LOGW, FL( "There were warnings while unpacking a SMPower Save update (0x%08x, %d bytes):"),
                                nStatus, frameLen );
            PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
        }

        limLog(pMac, LOGW, FL("Received SM Power save Mode update Frame with PS_Enable:%d"
                            "PS Mode: %d"), frmSMPower.SMPowerModeSet.PowerSave_En,
                                                    frmSMPower.SMPowerModeSet.Mode);

        /** Update in the DPH Table about the Update in the SM Power Save mode*/
        if (frmSMPower.SMPowerModeSet.PowerSave_En && frmSMPower.SMPowerModeSet.Mode)
            state = eSIR_HT_MIMO_PS_DYNAMIC;
        else if ((frmSMPower.SMPowerModeSet.PowerSave_En) && (frmSMPower.SMPowerModeSet.Mode ==0))
            state = eSIR_HT_MIMO_PS_STATIC;
        else if ((frmSMPower.SMPowerModeSet.PowerSave_En == 0) && (frmSMPower.SMPowerModeSet.Mode == 0))
            state = eSIR_HT_MIMO_PS_NO_LIMIT;
        else {
            PELOGW(limLog(pMac, LOGW, FL("Received SM Power save Mode update Frame with invalid mode"));)
            return;
        }

        if (state == pSta->htMIMOPSState) {
            PELOGE(limLog(pMac, LOGE, FL("The PEER is already set in the same mode"));)
            return;
        }

        /** Update in the HAL Station Table for the Update of the Protection Mode */
        pSta->htMIMOPSState = state;
        limPostSMStateUpdate(pMac,pSta->staIndex, pSta->htMIMOPSState);

#endif
        
}

#if defined WLAN_FEATURE_VOWIFI

static void
__limProcessRadioMeasureRequest( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo ,tpPESession psessionEntry )
{
     tpSirMacMgmtHdr                pHdr;
     tDot11fRadioMeasurementRequest frm;
     tANI_U32                       frameLen, nStatus;
     tANI_U8                        *pBody;

     pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
     pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
     frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );

     if( psessionEntry == NULL )
     {
          return;
     }

     /**Unpack the received frame */
     nStatus = dot11fUnpackRadioMeasurementRequest( pMac, pBody, frameLen, &frm );

     if( DOT11F_FAILED( nStatus )) {
          limLog( pMac, LOGE, FL( "Failed to unpack and parse a Radio Measure request (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
               return;
     }else if ( DOT11F_WARNED( nStatus ) ) {
          limLog(pMac, LOGW, FL( "There were warnings while unpacking a Radio Measure request (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
     }

     // Call rrm function to handle the request.

     rrmProcessRadioMeasurementRequest( pMac, pHdr->sa, &frm, psessionEntry );
}

static void
__limProcessLinkMeasurementReq( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo ,tpPESession psessionEntry )
{
     tpSirMacMgmtHdr               pHdr;
     tDot11fLinkMeasurementRequest frm;
     tANI_U32                      frameLen, nStatus;
     tANI_U8                       *pBody;

     pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
     pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
     frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );

     if( psessionEntry == NULL )
     {
          return;
     }

     /**Unpack the received frame */
     nStatus = dot11fUnpackLinkMeasurementRequest( pMac, pBody, frameLen, &frm );

     if( DOT11F_FAILED( nStatus )) {
          limLog( pMac, LOGE, FL( "Failed to unpack and parse a Link Measure request (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
               return;
     }else if ( DOT11F_WARNED( nStatus ) ) {
          limLog(pMac, LOGW, FL( "There were warnings while unpacking a Link Measure request (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
     }

     // Call rrm function to handle the request.

     rrmProcessLinkMeasurementRequest( pMac, pRxPacketInfo, &frm, psessionEntry );

}

static void
__limProcessNeighborReport( tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo ,tpPESession psessionEntry )
{
     tpSirMacMgmtHdr               pHdr;
     tDot11fNeighborReportResponse *pFrm;
     tANI_U32                      frameLen, nStatus;
     tANI_U8                       *pBody;

     pHdr = WDA_GET_RX_MAC_HEADER( pRxPacketInfo );
     pBody = WDA_GET_RX_MPDU_DATA( pRxPacketInfo );
     frameLen = WDA_GET_RX_PAYLOAD_LEN( pRxPacketInfo );

     pFrm = vos_mem_malloc(sizeof(tDot11fNeighborReportResponse));
     if (NULL == pFrm)
     {
         limLog(pMac, LOGE, FL("Unable to allocate memory in __limProcessNeighborReport") );
         return;
     }

     if(psessionEntry == NULL)
     {
          vos_mem_free(pFrm);
          return;
     }

     /**Unpack the received frame */
     nStatus = dot11fUnpackNeighborReportResponse( pMac, pBody, frameLen,pFrm );

     if( DOT11F_FAILED( nStatus )) {
          limLog( pMac, LOGE, FL( "Failed to unpack and parse a Neighbor report response (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
          vos_mem_free(pFrm);
          return;
     }else if ( DOT11F_WARNED( nStatus ) ) {
          limLog(pMac, LOGW, FL( "There were warnings while unpacking a Neighbor report response (0x%08x, %d bytes):"),
                    nStatus, frameLen );
          PELOG2(sirDumpBuf( pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frameLen );)
     }

     //Call rrm function to handle the request.
     rrmProcessNeighborReportResponse( pMac, pFrm, psessionEntry ); 
     
     vos_mem_free(pFrm);
}

#endif

#ifdef WLAN_FEATURE_11W
/**
 * limProcessSAQueryRequestActionFrame
 *
 *FUNCTION:
 * This function is called by limProcessActionFrame() upon
 * SA query request Action frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - Handle to the Rx packet info
 * @param  psessionEntry - PE session entry
 *
 * @return None
 */
static void __limProcessSAQueryRequestActionFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tpPESession psessionEntry)
{
    tpSirMacMgmtHdr     pHdr;
    tANI_U8             *pBody;
    tANI_U8             transId[2];

    /* Prima  --- Below Macro not available in prima 
       pHdr = SIR_MAC_BD_TO_MPDUHEADER(pBd);
       pBody = SIR_MAC_BD_TO_MPDUDATA(pBd); */

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

    /* If this is an unprotected SA Query Request, then ignore it. */
    if (pHdr->fc.wep == 0)
        return;

    /*Extract 11w trsansId from SA query request action frame
      In SA query response action frame we will send same transId
      In SA query request action frame:
      Category       : 1 byte
      Action         : 1 byte
      Transaction ID : 2 bytes */
    vos_mem_copy(&transId[0], &pBody[2], 2);

    //Send 11w SA query response action frame
    if (limSendSaQueryResponseFrame(pMac,
                              transId,
                              pHdr->sa,psessionEntry) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("fail to send SA query response action frame."));)
        return;
    }
}

/**
 * __limProcessSAQueryResponseActionFrame
 *
 *FUNCTION:
 * This function is called by limProcessActionFrame() upon
 * SA query response Action frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - Handle to the Rx packet info
 * @param  psessionEntry - PE session entry
 * @return None
 */
static void __limProcessSAQueryResponseActionFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tpPESession psessionEntry)
{
    tpSirMacMgmtHdr     pHdr;
    tANI_U8             *pBody;
    tpDphHashNode       pSta;
    tANI_U16            aid;
    tANI_U16            transId;
    tANI_U8             retryNum;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                         ("SA Query Response received...")) ;

    /* When a station, supplicant handles SA Query Response.
       Forward to SME to HDD to wpa_supplicant. */
    if (eLIM_STA_ROLE == psessionEntry->limSystemRole)
    {
        limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo, psessionEntry, 0);
        return;
    }

    /* If this is an unprotected SA Query Response, then ignore it. */
    if (pHdr->fc.wep == 0)
        return;

    pSta = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);
    if (NULL == pSta)
        return;

    limLog(pMac, LOG1,
           FL("SA Query Response source addr - %0x:%0x:%0x:%0x:%0x:%0x"),
           pHdr->sa[0], pHdr->sa[1], pHdr->sa[2], pHdr->sa[3],
           pHdr->sa[4], pHdr->sa[5]);
    limLog(pMac, LOG1,
           FL("SA Query state for station - %d"), pSta->pmfSaQueryState);

    if (DPH_SA_QUERY_IN_PROGRESS != pSta->pmfSaQueryState)
        return;

    /* Extract 11w trsansId from SA query reponse action frame
       In SA query response action frame:
          Category       : 1 byte
          Action         : 1 byte
          Transaction ID : 2 bytes */
    vos_mem_copy(&transId, &pBody[2], 2);

    /* If SA Query is in progress with the station and the station
       responds then the association request that triggered the SA
       query is from a rogue station, just go back to initial state. */
    for (retryNum = 0; retryNum <= pSta->pmfSaQueryRetryCount; retryNum++)
        if (transId == pSta->pmfSaQueryStartTransId + retryNum)
        {
            limLog(pMac, LOG1,
                   FL("Found matching SA Query Request - transaction ID %d"), transId);
            tx_timer_deactivate(&pSta->pmfSaQueryTimer);
            pSta->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
            break;
        }
}
#endif

#ifdef WLAN_FEATURE_11W
/**
 * limDropUnprotectedActionFrame
 *
 *FUNCTION:
 * This function checks if an Action frame should be dropped since it is
 * a Robust Managment Frame, it is unprotected, and it is received on a
 * connection where PMF is enabled.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Global MAC structure
 * @param  psessionEntry - PE session entry
 * @param  pHdr - Frame header
 * @param  category - Action frame category
 * @return TRUE if frame should be dropped
 */

static tANI_BOOLEAN
limDropUnprotectedActionFrame (tpAniSirGlobal pMac, tpPESession psessionEntry,
                               tpSirMacMgmtHdr pHdr, tANI_U8 category)
{
    tANI_U16 aid;
    tpDphHashNode pStaDs;
    tANI_BOOLEAN rmfConnection = eANI_BOOLEAN_FALSE;

    if ((psessionEntry->limSystemRole == eLIM_AP_ROLE) ||
        (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE))
    {
        pStaDs = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);
        if (pStaDs != NULL)
            if (pStaDs->rmfEnabled)
                rmfConnection = eANI_BOOLEAN_TRUE;
    }
    else if (psessionEntry->limRmfEnabled)
        rmfConnection = eANI_BOOLEAN_TRUE;

    if (rmfConnection && (pHdr->fc.wep == 0))
    {
        PELOGE(limLog(pMac, LOGE, FL("Dropping unprotected Action category %d frame "
                                     "since RMF is enabled."), category);)
        return eANI_BOOLEAN_TRUE;
    }
    else
        return eANI_BOOLEAN_FALSE;
}
#endif

/**
 * limProcessActionFrame
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Action frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to packet info structure
 * @return None
 */

void
limProcessActionFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tANI_U8 *pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
    tpSirMacActionFrameHdr pActionHdr = (tpSirMacActionFrameHdr) pBody;
#ifdef WLAN_FEATURE_11W
    tpSirMacMgmtHdr pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
#endif

    switch (pActionHdr->category)
    {
        case SIR_MAC_ACTION_QOS_MGMT:
#ifdef WLAN_FEATURE_11W
            if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
                break;
#endif
            if ( (psessionEntry->limQosEnabled) ||
                  (pActionHdr->actionID == SIR_MAC_QOS_MAP_CONFIGURE) )
            {
                switch (pActionHdr->actionID)
                {
                    case SIR_MAC_QOS_ADD_TS_REQ:
                        __limProcessAddTsReq(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                        break;

                    case SIR_MAC_QOS_ADD_TS_RSP:
                        __limProcessAddTsRsp(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                        break;

                    case SIR_MAC_QOS_DEL_TS_REQ:
                        __limProcessDelTsReq(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                        break;

                    case SIR_MAC_QOS_MAP_CONFIGURE:
                        __limProcessQosMapConfigureFrame(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                    break;
                    default:
                        PELOGE(limLog(pMac, LOGE, FL("Qos action %d not handled"), pActionHdr->actionID);)
                        break;
                }
                break ;
            }

           break;

        case SIR_MAC_ACTION_SPECTRUM_MGMT:
#ifdef WLAN_FEATURE_11W
            if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
                break;
#endif
            switch (pActionHdr->actionID)
            {
#ifdef ANI_SUPPORT_11H
                case SIR_MAC_ACTION_MEASURE_REQUEST_ID:
                    if(psessionEntry->lim11hEnable)
                    {
                        __limProcessMeasurementRequestFrame(pMac, pRxPacketInfo);
                    }
                    break;

                case SIR_MAC_ACTION_TPC_REQUEST_ID:
                    if ((psessionEntry->limSystemRole == eLIM_STA_ROLE) ||
                        (pessionEntry->limSystemRole == eLIM_AP_ROLE))
                    {
                        if(psessionEntry->lim11hEnable)
                        {
                            __limProcessTpcRequestFrame(pMac, pRxPacketInfo);
                        }
                    }
                    break;

#endif
                case SIR_MAC_ACTION_CHANNEL_SWITCH_ID:
                    if (psessionEntry->limSystemRole == eLIM_STA_ROLE)
                    {
                        __limProcessChannelSwitchActionFrame(pMac, pRxPacketInfo,psessionEntry);
                    }
                    break;
                default:
                    PELOGE(limLog(pMac, LOGE, FL("Spectrum mgmt action id %d not handled"), pActionHdr->actionID);)
                    break;
            }
            break;

        case SIR_MAC_ACTION_WME:
            if (! psessionEntry->limWmeEnabled)
            {
                limLog(pMac, LOGW, FL("WME mode disabled - dropping action frame %d"),
                       pActionHdr->actionID);
                break;
            }
            switch(pActionHdr->actionID)
            {
                case SIR_MAC_QOS_ADD_TS_REQ:
                    __limProcessAddTsReq(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_QOS_ADD_TS_RSP:
                    __limProcessAddTsRsp(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_QOS_DEL_TS_REQ:
                    __limProcessDelTsReq(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_QOS_MAP_CONFIGURE:
                    __limProcessQosMapConfigureFrame(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                    break;

                default:
                    PELOGE(limLog(pMac, LOGE, FL("WME action %d not handled"), pActionHdr->actionID);)
                    break;
            }
            break;

        case SIR_MAC_ACTION_BLKACK:
            // Determine the "type" of BA Action Frame
#ifdef WLAN_FEATURE_11W
            if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
                break;
#endif
            switch(pActionHdr->actionID)
            {
              case SIR_MAC_BLKACK_ADD_REQ:
                __limProcessAddBAReq( pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                break;

              case SIR_MAC_BLKACK_ADD_RSP:
                __limProcessAddBARsp( pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                break;

              case SIR_MAC_BLKACK_DEL:
                __limProcessDelBAReq( pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
                break;

              default:
                break;
            }

            break;
    case SIR_MAC_ACTION_HT:
        /** Type of HT Action to be performed*/
        switch(pActionHdr->actionID) {
        case SIR_MAC_SM_POWER_SAVE:
            __limProcessSMPowerSaveUpdate(pMac, (tANI_U8 *) pRxPacketInfo,psessionEntry);
            break;
        default:
            PELOGE(limLog(pMac, LOGE, FL("Action ID %d not handled in HT Action category"), pActionHdr->actionID);)
            break;
        }
        break;

    case SIR_MAC_ACTION_WNM:
    {
#ifdef WLAN_FEATURE_11W
        if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
            break;
#endif
        PELOGE(limLog(pMac, LOG1, FL("WNM Action category %d action %d."),
                                pActionHdr->category, pActionHdr->actionID);)
        switch (pActionHdr->actionID)
        {
            case SIR_MAC_WNM_BSS_TM_QUERY:
            case SIR_MAC_WNM_BSS_TM_REQUEST:
            case SIR_MAC_WNM_BSS_TM_RESPONSE:
            case SIR_MAC_WNM_NOTIF_REQUEST:
            case SIR_MAC_WNM_NOTIF_RESPONSE:
            {
               tpSirMacMgmtHdr     pHdr;
               tANI_S8 rssi = WDA_GET_RX_RSSI_DB(pRxPacketInfo);
               pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
               /* Forward to the SME to HDD to wpa_supplicant */
               limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo,
                                       psessionEntry, rssi);
               break;
            }
        }
        break;
    }
#if defined WLAN_FEATURE_VOWIFI
    case SIR_MAC_ACTION_RRM:
#ifdef WLAN_FEATURE_11W
        if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
            break;
#endif
        if( pMac->rrm.rrmPEContext.rrmEnable )
        {
            switch(pActionHdr->actionID) {
                case SIR_MAC_RRM_RADIO_MEASURE_REQ:
                    __limProcessRadioMeasureRequest( pMac, (tANI_U8 *) pRxPacketInfo, psessionEntry );
                    break;
                case SIR_MAC_RRM_LINK_MEASUREMENT_REQ:
                    __limProcessLinkMeasurementReq( pMac, (tANI_U8 *) pRxPacketInfo, psessionEntry );
                    break;
                case SIR_MAC_RRM_NEIGHBOR_RPT:   
                    __limProcessNeighborReport( pMac, (tANI_U8*) pRxPacketInfo, psessionEntry );
                    break;
                default:
                    PELOGE( limLog( pMac, LOGE, FL("Action ID %d not handled in RRM"), pActionHdr->actionID);)
                    break;

            }
        }
        else
        {
            // Else we will just ignore the RRM messages.
            PELOGE( limLog( pMac, LOGE, FL("RRM Action frame ignored as RRM is disabled in cfg"));)
        }
        break;
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        case SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY:
            {
              tpSirMacVendorSpecificFrameHdr pVendorSpecific = (tpSirMacVendorSpecificFrameHdr) pActionHdr;
              tpSirMacMgmtHdr     pHdr;
              tANI_U8 Oui[] = { 0x00, 0x00, 0xf0 };

              pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);

              //Check if it is a vendor specific action frame.
              if ((eLIM_STA_ROLE == psessionEntry->limSystemRole) &&
                  (VOS_TRUE == vos_mem_compare(psessionEntry->selfMacAddr,
                    &pHdr->da[0], sizeof(tSirMacAddr))) &&
                    IS_WES_MODE_ENABLED(pMac) &&
                    vos_mem_compare(pVendorSpecific->Oui, Oui, 3))
              {
                  PELOGE( limLog( pMac, LOGW, FL("Received Vendor specific action frame, OUI %x %x %x"),
                         pVendorSpecific->Oui[0], pVendorSpecific->Oui[1], pVendorSpecific->Oui[2]);)
                 /* Forward to the SME to HDD to wpa_supplicant */
                 // type is ACTION
                  limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo,
                                         psessionEntry, 0);
              }
              else
              {
                 limLog( pMac, LOGE, FL("Dropping the vendor specific action frame because of( "
                                        "WES Mode not enabled (WESMODE = %d) or OUI mismatch (%02x %02x %02x) or "
                                        "not received with SelfSta Mac address) system role = %d"),
                                        IS_WES_MODE_ENABLED(pMac),
                                        pVendorSpecific->Oui[0], pVendorSpecific->Oui[1],
                                        pVendorSpecific->Oui[2],
                                        psessionEntry->limSystemRole );
              }
           }
           break;
#endif
    case SIR_MAC_ACTION_PUBLIC_USAGE:
        switch(pActionHdr->actionID) {
        case SIR_MAC_ACTION_VENDOR_SPECIFIC:
            {
              tpSirMacVendorSpecificPublicActionFrameHdr pPubAction = (tpSirMacVendorSpecificPublicActionFrameHdr) pActionHdr;
              tANI_U8 P2POui[] = { 0x50, 0x6F, 0x9A, 0x09 };

              //Check if it is a P2P public action frame.
              if (vos_mem_compare(pPubAction->Oui, P2POui, 4))
              {
                 /* Forward to the SME to HDD to wpa_supplicant */
                 // type is ACTION
                 limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo,
                                        psessionEntry, 0);
              }
              else
              {
                 limLog( pMac, LOGE, FL("Unhandled public action frame (Vendor specific). OUI %x %x %x %x"),
                      pPubAction->Oui[0], pPubAction->Oui[1], pPubAction->Oui[2], pPubAction->Oui[3] );
              }
           }
            break;
#ifdef FEATURE_WLAN_TDLS
           case SIR_MAC_TDLS_DIS_RSP:
           {
#ifdef FEATURE_WLAN_TDLS_INTERNAL
               //LIM_LOG_TDLS(printk("Public Action TDLS Discovery RSP ..")) ;
               limProcessTdlsPublicActionFrame(pMac, (tANI_U32*)pRxPacketInfo, psessionEntry) ;
#else
               tANI_S8             rssi;

               rssi = WDA_GET_RX_RSSI_DB(pRxPacketInfo);
               VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                    ("Public Action TDLS Discovery RSP ..")) ;
               limSendSmeMgmtFrameInd(pMac, 0, pRxPacketInfo,
                                      psessionEntry, rssi);
#endif
           }
               break;
#endif

        default:
            PELOGE(limLog(pMac, LOGE, FL("Unhandled public action frame -- %x "), pActionHdr->actionID);)
            break;
        }
        break;

#ifdef WLAN_FEATURE_11W
    case SIR_MAC_ACTION_SA_QUERY:
    {
        PELOGE(limLog(pMac, LOG1, FL("SA Query Action category %d action %d."), pActionHdr->category, pActionHdr->actionID);)
        if (limDropUnprotectedActionFrame(pMac, psessionEntry, pHdr, pActionHdr->category))
            break;
        switch (pActionHdr->actionID)
        {
            case  SIR_MAC_SA_QUERY_REQ:
                /**11w SA query request action frame received**/
                /* Respond directly to the incoming request in LIM */
                __limProcessSAQueryRequestActionFrame(pMac,(tANI_U8*) pRxPacketInfo, psessionEntry );
                break;
            case  SIR_MAC_SA_QUERY_RSP:
                /**11w SA query response action frame received**/
                /* Handle based on the current SA Query state */
                __limProcessSAQueryResponseActionFrame(pMac,(tANI_U8*) pRxPacketInfo, psessionEntry );
                break;
            default:
                break;
        }
        break;
     }
#endif
#ifdef WLAN_FEATURE_11AC
    case SIR_MAC_ACTION_VHT:
    {
        if (psessionEntry->vhtCapability)
        {
            switch (pActionHdr->actionID)
            {
                case  SIR_MAC_VHT_OPMODE_NOTIFICATION:
                    __limProcessOperatingModeActionFrame(pMac,pRxPacketInfo,psessionEntry);                
                break;
                default:
                break;
            }
        }
        break;
    }
#endif
    default:
       PELOGE(limLog(pMac, LOGE, FL("Action category %d not handled"), pActionHdr->category);)
       break;
    }
}

/**
 * limProcessActionFrameNoSession
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Action frame reception and no session.
 * Currently only public action frames can be received from
 * a non-associated station.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  *pBd - A pointer to Buffer descriptor + associated PDUs
 * @return None
 */

void
limProcessActionFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *pBd)
{
   tANI_U8 *pBody = WDA_GET_RX_MPDU_DATA(pBd);
   tpSirMacVendorSpecificPublicActionFrameHdr pActionHdr = (tpSirMacVendorSpecificPublicActionFrameHdr) pBody;

   limLog( pMac, LOG1, "Received a Action frame -- no session");

   switch ( pActionHdr->category )
   {
      case SIR_MAC_ACTION_PUBLIC_USAGE:
         switch(pActionHdr->actionID) {
            case SIR_MAC_ACTION_VENDOR_SPECIFIC:
              {
                tANI_U8 P2POui[] = { 0x50, 0x6F, 0x9A, 0x09 };

                //Check if it is a P2P public action frame.
                if (vos_mem_compare(pActionHdr->Oui, P2POui, 4))
                {
                  /* Forward to the SME to HDD to wpa_supplicant */
                  // type is ACTION
                  limSendSmeMgmtFrameInd(pMac, 0, pBd, NULL, 0);
                }
                else
                {
                  limLog( pMac, LOGE, FL("Unhandled public action frame (Vendor specific). OUI %x %x %x %x"),
                      pActionHdr->Oui[0], pActionHdr->Oui[1], pActionHdr->Oui[2], pActionHdr->Oui[3] );
                }
              }
               break;
            default:
               PELOGE(limLog(pMac, LOGE, FL("Unhandled public action frame -- %x "), pActionHdr->actionID);)
                  break;
         }
         break;
      default:
         PELOGE(limLog(pMac, LOG1, FL("Unhandled action frame without session -- %x "), pActionHdr->category);)
            break;

   }
}
