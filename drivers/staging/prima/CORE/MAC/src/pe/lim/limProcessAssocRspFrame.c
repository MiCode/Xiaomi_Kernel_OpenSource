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
 *
 * This file limProcessAssocRspFrame.cc contains the code
 * for processing Re/Association Response Frame.
 * Author:        Chandra Modumudi
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniApi.h"
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "cfgApi.h"

#include "utilsApi.h"
#include "pmmApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limStaHashApi.h"
#include "limSendMessages.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#endif

extern tSirRetStatus schBeaconEdcaProcess(tpAniSirGlobal pMac, tSirMacEdcaParamSetIE *edca, tpPESession psessionEntry);


/**
 * @function : limUpdateAssocStaDatas
 *
 * @brief :  This function is called to Update the Station Descriptor (dph) Details from
 *                  Association / ReAssociation Response Frame
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs   - Station Descriptor in DPH
 * @param  pAssocRsp    - Pointer to Association Response Structure
 *
 * @return None
 */
void limUpdateAssocStaDatas(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpSirAssocRsp pAssocRsp,tpPESession psessionEntry)
{
    tANI_U32        prop;
    tANI_U32        phyMode;
    tANI_U32        val;
    //tpSirBoardCapabilities pBoardCaps;
    tANI_BOOLEAN    qosMode; 
    tANI_U16        rxHighestRate = 0;

    limGetPhyMode(pMac, &phyMode, psessionEntry);

    pStaDs->staType= STA_ENTRY_SELF;

    limGetQosMode(psessionEntry, &qosMode);    
    // set the ani peer bit, if self mode is one of the proprietary modes
    if(IS_DOT11_MODE_PROPRIETARY(psessionEntry->dot11mode)) 
    {
       wlan_cfgGetInt(pMac, WNI_CFG_PROPRIETARY_ANI_FEATURES_ENABLED, &prop);

       if (prop) 
       {
           pStaDs->aniPeer = eHAL_SET;
           pStaDs->propCapability = pAssocRsp->propIEinfo.capability;
       }
    }
    
       //pMac->lim.gLimMlmState         = eLIM_MLM_LINK_ESTABLISHED_STATE;
       pStaDs->mlmStaContext.authType = psessionEntry->limCurrentAuthType;
    
       // Add capabilities information, rates and AID
       pStaDs->mlmStaContext.capabilityInfo = pAssocRsp->capabilityInfo;
       pStaDs->shortPreambleEnabled= (tANI_U8)pAssocRsp->capabilityInfo.shortPreamble;
    
       //Update HT Capabilites only when the self mode supports HT
       if(IS_DOT11_MODE_HT(psessionEntry->dot11mode)) {
           pStaDs->mlmStaContext.htCapability = pAssocRsp->HTCaps.present;
    
           if ( pAssocRsp->HTCaps.present ) {
               pStaDs->htGreenfield = ( tANI_U8 ) pAssocRsp->HTCaps.greenField;
               if (psessionEntry->htSupportedChannelWidthSet)
               {
                   pStaDs->htSupportedChannelWidthSet = ( tANI_U8 ) (pAssocRsp->HTCaps.supportedChannelWidthSet ?
                                                                               pAssocRsp->HTInfo.recommendedTxWidthSet : 
                                                                               pAssocRsp->HTCaps.supportedChannelWidthSet );
               }
               else
               {
                   pStaDs->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_20MHZ;
               }
                   pStaDs->htLsigTXOPProtection = ( tANI_U8 ) pAssocRsp->HTCaps.lsigTXOPProtection;
                   pStaDs->htMIMOPSState =  (tSirMacHTMIMOPowerSaveState)pAssocRsp->HTCaps.mimoPowerSave;
                   pStaDs->htMaxAmsduLength = ( tANI_U8 ) pAssocRsp->HTCaps.maximalAMSDUsize;
                   pStaDs->htAMpduDensity =             pAssocRsp->HTCaps.mpduDensity;
                   pStaDs->htDsssCckRate40MHzSupport = (tANI_U8)pAssocRsp->HTCaps.dsssCckMode40MHz;
                   pStaDs->htShortGI20Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI20MHz;
                   pStaDs->htShortGI40Mhz = (tANI_U8)pAssocRsp->HTCaps.shortGI40MHz;
                   pStaDs->htMaxRxAMpduFactor = pAssocRsp->HTCaps.maxRxAMPDUFactor;
                   limFillRxHighestSupportedRate(pMac, &rxHighestRate, pAssocRsp->HTCaps.supportedMCSSet);
                   pStaDs->supportedRates.rxHighestDataRate = rxHighestRate;
                   /* This is for AP as peer STA and we are INFRA STA. We will put APs offset in dph node which is peer STA */
                   pStaDs->htSecondaryChannelOffset = (tANI_U8)pAssocRsp->HTInfo.secondaryChannelOffset;

                   //FIXME_AMPDU
                   // In the future, may need to check for "assoc.HTCaps.delayedBA"
                   // For now, it is IMMEDIATE BA only on ALL TID's
                   pStaDs->baPolicyFlag = 0xFF;
           }
       }

#ifdef WLAN_FEATURE_11AC
       if(IS_DOT11_MODE_VHT(psessionEntry->dot11mode))
       {
           pStaDs->mlmStaContext.vhtCapability = pAssocRsp->VHTCaps.present;
           if (pAssocRsp->VHTCaps.present && psessionEntry->htSupportedChannelWidthSet)
               pStaDs->vhtSupportedChannelWidthSet = pAssocRsp->VHTOperation.chanWidth;
       }
       if (limPopulatePeerRateSet(pMac, &pStaDs->supportedRates,
                                pAssocRsp->HTCaps.supportedMCSSet,
                                false,psessionEntry , &pAssocRsp->VHTCaps) != eSIR_SUCCESS)
#else
       if (limPopulatePeerRateSet(pMac, &pStaDs->supportedRates, pAssocRsp->HTCaps.supportedMCSSet, false,psessionEntry) != eSIR_SUCCESS)
#endif
       {
           limLog(pMac, LOGP, FL("could not get rateset and extended rate set"));
           return;
       }
   
       //If one of the rates is 11g rates, set the ERP mode.
       if ((phyMode == WNI_CFG_PHY_MODE_11G) && sirIsArate(pStaDs->supportedRates.llaRates[0] & 0x7f))
           pStaDs->erpEnabled = eHAL_SET;
    
    
       val = WNI_CFG_PROPRIETARY_OPERATIONAL_RATE_SET_LEN;
       if (wlan_cfgGetStr(pMac, WNI_CFG_PROPRIETARY_OPERATIONAL_RATE_SET,
                     (tANI_U8 *) &pStaDs->mlmStaContext.propRateSet.propRate,
                     &val) != eSIR_SUCCESS) {
           /// Could not get prop rateset from CFG. Log error.
           limLog(pMac, LOGP, FL("could not retrieve prop rateset"));
           return;
       }
       pStaDs->mlmStaContext.propRateSet.numPropRates = (tANI_U8) val;
    
       pStaDs->qosMode    = 0;
       pStaDs->lleEnabled = 0;
    
       // update TSID to UP mapping
       //if (pMac->lim.gLimQosEnabled)
       if (qosMode) {
           if (pAssocRsp->edcaPresent) {
               tSirRetStatus status;
               status = schBeaconEdcaProcess(pMac,&pAssocRsp->edca, psessionEntry);
              PELOG2(limLog(pMac, LOG2, "Edca set update based on AssocRsp: status %d",
                      status);)
               if (status != eSIR_SUCCESS) {
                   PELOGE(limLog(pMac, LOGE, FL("Edca error in AssocResp "));)
               } else { // update default tidmap based on ACM
                   pStaDs->qosMode    = 1;
                   pStaDs->lleEnabled = 1;
               }
           }
       }
    
       pStaDs->wmeEnabled = 0;
       pStaDs->wsmEnabled = 0;
       if (psessionEntry->limWmeEnabled && pAssocRsp->wmeEdcaPresent)
       {
           tSirRetStatus status;
           status = schBeaconEdcaProcess(pMac,&pAssocRsp->edca, psessionEntry);
           PELOGW(limLog(pMac, LOGW, "WME Edca set update based on AssocRsp: status %d", status);)

           if (status != eSIR_SUCCESS)
               PELOGE(limLog(pMac, LOGE, FL("WME Edca error in AssocResp - ignoring"));)
           else { // update default tidmap based on HashACM
               pStaDs->qosMode    = 1;
               pStaDs->wmeEnabled = 1;
           }
       } 
       else {
           /* We received assoc rsp from a legacy AP. So fill in the default 
            * local EDCA params. This is needed (refer to bug #14989) as we'll 
            * be passing the gLimEdcaParams to HAL in limProcessStaMlmAddBssRsp().
            */ 
           schSetDefaultEdcaParams(pMac, psessionEntry);
       }

       if(qosMode && (!pStaDs->qosMode) && pStaDs->mlmStaContext.htCapability)
       {
           // Enable QOS for all HT AP's even though WMM or 802.11E IE is not present
           pStaDs->qosMode    = 1;
           pStaDs->wmeEnabled = 1;
       }

#ifdef WLAN_FEATURE_11W
       if(psessionEntry->limRmfEnabled)
       {
           pStaDs->rmfEnabled = 1;
       }
#endif
}

/**
 * @function : limUpdateReAssocGlobals
 *
 * @brief :  This function is called to Update the Globals (LIM) during ReAssoc.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pAssocRsp    - Pointer to Association Response Structure
 *
 * @return None
 */

void limUpdateReAssocGlobals(tpAniSirGlobal pMac, tpSirAssocRsp pAssocRsp,tpPESession psessionEntry)
{
    /**
     * Update the status for PMM module
     */
    pmmResetPmmState(pMac);

    // Update the current Bss Information
    vos_mem_copy(psessionEntry->bssId,
                 psessionEntry->limReAssocbssId, sizeof(tSirMacAddr));
    psessionEntry->currentOperChannel = psessionEntry->limReassocChannelId;
    psessionEntry->htSecondaryChannelOffset = psessionEntry->reAssocHtSupportedChannelWidthSet;
    psessionEntry->htRecommendedTxWidthSet = psessionEntry->reAssocHtRecommendedTxWidthSet;
    psessionEntry->htSecondaryChannelOffset = psessionEntry->reAssocHtSecondaryChannelOffset;
    psessionEntry->limCurrentBssCaps   = psessionEntry->limReassocBssCaps;
    psessionEntry->limCurrentBssQosCaps = psessionEntry->limReassocBssQosCaps;
    psessionEntry->limCurrentBssPropCap = psessionEntry->limReassocBssPropCap;

    vos_mem_copy((tANI_U8 *) &psessionEntry->ssId,
                 (tANI_U8 *) &psessionEntry->limReassocSSID,
                  psessionEntry->limReassocSSID.length+1);
    
    // Store assigned AID for TIM processing
    psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;
    /** Set the State Back to ReAssoc Rsp*/
    psessionEntry->limMlmState = eLIM_MLM_WT_REASSOC_RSP_STATE; 
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

    
}

/**
 * @function : limProcessAssocRspFrame
 *
 * @brief :  This function is called by limProcessMessageQueue() upon
 *              Re/Association Response frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  *pRxPacketInfo    - A pointer to Rx packet info structure
 * @param  subType - Indicates whether it is Association Response (=0) or
 *                   Reassociation Response (=1) frame
 *
 * @return None
 */

void
limProcessAssocRspFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tANI_U8 subType,tpPESession psessionEntry)
{
    tANI_U8               *pBody;
    tANI_U16              caps;
    tANI_U32              frameLen;
    tSirMacAddr           currentBssId;
    tpSirMacMgmtHdr       pHdr;
    tSirMacCapabilityInfo localCapabilities;
    tpDphHashNode         pStaDs;
    tpSirAssocRsp         pAssocRsp;
    tLimMlmAssocCnf       mlmAssocCnf;
    
    tSchBeaconStruct *pBeaconStruct;

    //Initialize status code to success.

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);

    mlmAssocCnf.resultCode = eSIR_SME_SUCCESS;
    /* Update PE session Id*/
    mlmAssocCnf.sessionId = psessionEntry->peSessionId;
    limLog(pMac, LOG1,
              FL("received Re/Assoc(%d) resp on sessionid: %d with systemrole: %d "
              "and mlmstate: %d RSSI %d from "MAC_ADDRESS_STR),subType,
              psessionEntry->peSessionId,
              psessionEntry->limSystemRole,psessionEntry->limMlmState,
              (uint)abs((tANI_S8)WDA_GET_RX_RSSI_DB(pRxPacketInfo)),
              MAC_ADDR_ARRAY(pHdr->sa));

    pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
    if (NULL == pBeaconStruct)
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory") );
        return;
    }

   
    if (psessionEntry->limSystemRole == eLIM_AP_ROLE || psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE )
    {
        // Should not have received Re/Association Response
        // frame on AP. Log error
        limLog(pMac, LOGE,
               FL("Should not recieved Re/Assoc Response in role %d "),
               psessionEntry->limSystemRole);

        vos_mem_free(pBeaconStruct);
        return;
    }


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

    if (((subType == LIM_ASSOC) &&
         (psessionEntry->limMlmState != eLIM_MLM_WT_ASSOC_RSP_STATE)) ||
        ((subType == LIM_REASSOC) &&
         ((psessionEntry->limMlmState != eLIM_MLM_WT_REASSOC_RSP_STATE) 
#if defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
         && (psessionEntry->limMlmState != eLIM_MLM_WT_FT_REASSOC_RSP_STATE)
#endif
         )))
    {
        /// Received unexpected Re/Association Response frame

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOG1(limLog(pMac, LOG1,  FL("Recieved Re/Assoc rsp in unexpected "
            "state %d on session=%d"),
            psessionEntry->limMlmState, psessionEntry->peSessionId);)
#endif
        // Log error
        if (!pHdr->fc.retry)
        {
            if ( !(pMac->lim.retryPacketCnt & 0xf))
            {
                limLog(pMac, LOGE,
                   FL("received Re/Assoc rsp frame is not a retry frame, "
                     "frame count %d"), ++pMac->lim.retryPacketCnt);
                limPrintMlmState(pMac, LOGE, psessionEntry->limMlmState);
            }
            else
            {
                pMac->lim.retryPacketCnt++;
            }

        }
        vos_mem_free(pBeaconStruct);
        return;
    }
#if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID"));
        vos_mem_free(pBeaconStruct);
        return;
    }
#endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    if (subType == LIM_ASSOC)
    {
        if (!vos_mem_compare(pHdr->sa, currentBssId, sizeof(tSirMacAddr)))
        {
            /**
             * Received Association Response frame from an entity
             * other than one to which request was initiated.
             * Ignore this and wait until Association Failure Timeout.
             */

            // Log error
            PELOGW(limLog(pMac, LOGW,
                   FL("received AssocRsp frame from unexpected peer "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pHdr->sa));)
            vos_mem_free(pBeaconStruct);
            return;
        }
    }
    else
    {
        if (!vos_mem_compare(pHdr->sa, psessionEntry->limReAssocbssId, sizeof(tSirMacAddr)))
        {
            /**
             * Received Reassociation Response frame from an entity
             * other than one to which request was initiated.
             * Ignore this and wait until Reassociation Failure Timeout.
             */

            // Log error
            PELOGW(limLog(pMac, LOGW,
                   FL("received ReassocRsp frame from unexpected peer "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pHdr->sa));)
            vos_mem_free(pBeaconStruct);

            return;
        }
    }

   pAssocRsp = vos_mem_malloc(sizeof(*pAssocRsp));
   if (NULL == pAssocRsp)
   {
        limLog(pMac, LOGP, FL("Allocate Memory failed in AssocRsp"));
        vos_mem_free(pBeaconStruct);

        return;
    }
   
    // Get pointer to Re/Association Response frame body
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

    // parse Re/Association Response frame.
    if (sirConvertAssocRespFrame2Struct(
                        pMac, pBody, frameLen, pAssocRsp) == eSIR_FAILURE) 
    {
        vos_mem_free(pAssocRsp);
        PELOGE(limLog(pMac, LOGE, FL("Parse error Assoc resp subtype %d,"
                                   "length=%d"), frameLen,subType);)
        vos_mem_free(pBeaconStruct);

        return;
    }
    if(pAssocRsp->ExtCap.present)
    {
        limLog(pMac, LOGE, FL("Filling tdls prohibited in session entry"));
        psessionEntry->tdlsChanSwitProhibited =
                       pAssocRsp->ExtCap.TDLSChanSwitProhibited ;
    }
    if(!pAssocRsp->suppRatesPresent)
    {
        PELOGE(limLog(pMac, LOGE, FL("assoc response does not have supported rate set"));)
        vos_mem_copy(&pAssocRsp->supportedRates,
                      &psessionEntry->rateSet, sizeof(tSirMacRateSet));
    }

    mlmAssocCnf.protStatusCode = pAssocRsp->statusCode;

    if( psessionEntry->assocRsp != NULL )
    {
        limLog(pMac, LOGW, FL("psessionEntry->assocRsp is not NULL freeing it "
        "and setting NULL"));
        vos_mem_free(psessionEntry->assocRsp);
        psessionEntry->assocRsp = NULL;
    }

    psessionEntry->assocRsp = vos_mem_malloc(frameLen);
    if (NULL == psessionEntry->assocRsp)
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc response, len = %d"), frameLen);)
    }
    else
    {
        //Store the Assoc response. This is sent to csr/hdd in join cnf response. 
        vos_mem_copy(psessionEntry->assocRsp, pBody, frameLen);
        psessionEntry->assocRspLen = frameLen;
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->ricData != NULL) 
    {
        vos_mem_free(psessionEntry->ricData);
        psessionEntry->ricData = NULL;
    }
    if(pAssocRsp->ricPresent)
    {
        psessionEntry->RICDataLen = pAssocRsp->num_RICData * sizeof(tDot11fIERICDataDesc);
        psessionEntry->ricData = vos_mem_malloc(psessionEntry->RICDataLen);
        if ( NULL == psessionEntry->ricData )
        {
            PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc response"));)
            psessionEntry->RICDataLen = 0; 
        }
        else
        {
            vos_mem_copy(psessionEntry->ricData,
                         &pAssocRsp->RICData[0], psessionEntry->RICDataLen);
        }
    }
    else
    {
        limLog(pMac, LOG1, FL("Ric is not present Setting RICDataLen 0 and ricData "
        "as NULL"));
        psessionEntry->RICDataLen = 0;
        psessionEntry->ricData = NULL;
    }
#endif    

#ifdef FEATURE_WLAN_ESE
    if (psessionEntry->tspecIes != NULL) 
    {
        vos_mem_free(psessionEntry->tspecIes);
        psessionEntry->tspecIes = NULL;
    }
    if(pAssocRsp->tspecPresent)
    {
        psessionEntry->tspecLen = pAssocRsp->num_tspecs * sizeof(tDot11fIEWMMTSPEC);
        psessionEntry->tspecIes = vos_mem_malloc(psessionEntry->tspecLen);
        if ( NULL == psessionEntry->tspecIes )
        {
            PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc response"));)
            psessionEntry->tspecLen = 0; 
        }
        else
        {
            vos_mem_copy(psessionEntry->tspecIes,
                         &pAssocRsp->TSPECInfo[0], psessionEntry->tspecLen);
        }
        PELOG1(limLog(pMac, LOG1, FL(" Tspec EID present in assoc rsp "));)
    }
    else
    {
        psessionEntry->tspecLen = 0;
        psessionEntry->tspecIes = NULL;
        PELOG1(limLog(pMac, LOG1, FL(" Tspec EID *NOT* present in assoc rsp "));)
    }
#endif

    if (pAssocRsp->capabilityInfo.ibss)
    {
        /**
         * Received Re/Association Response from peer
         * with IBSS capability set.
         * Ignore the frame and wait until Re/assoc
         * failure timeout.
         */
        
        // Log error
        limLog(pMac, LOGE,
               FL("received Re/AssocRsp frame with IBSS capability"));
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        return;
    }

    if (cfgGetCapabilityInfo(pMac, &caps,psessionEntry) != eSIR_SUCCESS)
    {
        /**
         * Could not get Capabilities value
         * from CFG. Log error.
         */         
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        limLog(pMac, LOGP, FL("could not retrieve Capabilities value"));
        return;
    }
    limCopyU16((tANI_U8 *) &localCapabilities, caps);

    if (subType == LIM_ASSOC)        // Stop Association failure timer
        limDeactivateAndChangeTimer(pMac, eLIM_ASSOC_FAIL_TIMER);
    else        // Stop Reassociation failure timer
    {
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        pMac->lim.reAssocRetryAttempt = 0;
        if ((NULL != pMac->lim.pSessionEntry) && (NULL != pMac->lim.pSessionEntry->pLimMlmReassocRetryReq))
        {
            vos_mem_free(pMac->lim.pSessionEntry->pLimMlmReassocRetryReq);
            pMac->lim.pSessionEntry->pLimMlmReassocRetryReq = NULL;
        }
#endif
        limDeactivateAndChangeTimer(pMac, eLIM_REASSOC_FAIL_TIMER);
    }

    if (pAssocRsp->statusCode != eSIR_MAC_SUCCESS_STATUS)
    {
        // Re/Association response was received
        // either with failure code.
        // Log error.
        PELOGE(limLog(pMac, LOGE, FL("received Re/AssocRsp frame failure code %d"), pAssocRsp->statusCode);)
        // Need to update 'association failure' error counter
        // along with STATUS CODE

        // Return Assoc confirm to SME with received failure code

        if (pAssocRsp->propIEinfo.loadBalanceInfoPresent)
        {
            mlmAssocCnf.resultCode = eSIR_SME_TRANSFER_STA;
            vos_mem_copy(pMac->lim.gLimAlternateRadio.bssId,
                         pAssocRsp->propIEinfo.alternateRadio.bssId, sizeof(tSirMacAddr));
            pMac->lim.gLimAlternateRadio.channelId =
                          pAssocRsp->propIEinfo.alternateRadio.channelId;
        }else
            mlmAssocCnf.resultCode = eSIR_SME_ASSOC_REFUSED;

        // Delete Pre-auth context for the associated BSS
        if (limSearchPreAuthList(pMac, pHdr->sa))
            limDeletePreAuthNode(pMac, pHdr->sa);

        goto assocReject;
    }
    else if ((pAssocRsp->aid & 0x3FFF) > 2007)
    {
        // Re/Association response was received
        // with invalid AID value
        // Log error
        PELOGW(limLog(pMac, LOGE, FL("received Re/AssocRsp frame with"
                                 "invalid aid %X"), pAssocRsp->aid);)
        mlmAssocCnf.resultCode = eSIR_SME_INVALID_ASSOC_RSP_RXED;
        mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

        // Send advisory Disassociation frame to AP
        limSendDisassocMgmtFrame(pMac, eSIR_MAC_UNSPEC_FAILURE_REASON,
                                 pHdr->sa, psessionEntry, FALSE);

        goto assocReject;
    }
    // Association Response received with success code
    /*
     * Set the link state to POSTASSOC now that we have received
     * assoc/reassoc response
     * NOTE: for BTAMP case, it is being handled in limProcessMlmAssocReq
     */
    if (!((psessionEntry->bssType == eSIR_BTAMP_STA_MODE) ||
          ((psessionEntry->bssType == eSIR_BTAMP_AP_MODE) &&
          (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))))
    {
            if (limSetLinkState(pMac, eSIR_LINK_POSTASSOC_STATE, psessionEntry->bssId,
                                psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("Set link state to POSTASSOC failed"));)
                vos_mem_free(pBeaconStruct);
                vos_mem_free(pAssocRsp);
                return;
            }
    }
    if (subType == LIM_REASSOC)
    {
        // Log success
        PELOG1(limLog(pMac, LOG1, FL("Successfully Reassociated with BSS"));)
#ifdef FEATURE_WLAN_ESE
        {
            tANI_U8 cnt = 0;
            if (pAssocRsp->tsmPresent)
            {
                limLog(pMac, LOG1, "TSM IE Present in Reassoc Rsp");
                // Start the TSM  timer only if the TSPEC Ie is present in the reassoc rsp
                if (pAssocRsp->tspecPresent) {
                    // Find the TSPEC IE with VO user priority
                    for (cnt=0; cnt<pAssocRsp->num_tspecs; cnt++) {
                        if ( upToAc(pAssocRsp->TSPECInfo[cnt].user_priority) == EDCA_AC_VO) {
                            psessionEntry->eseContext.tsm.tid = pAssocRsp->TSPECInfo[cnt].user_priority;
                            vos_mem_copy(&psessionEntry->eseContext.tsm.tsmInfo,
                                    &pAssocRsp->tsmIE, sizeof(tSirMacESETSMIE));
#ifdef FEATURE_WLAN_ESE_UPLOAD
                            limSendSmeTsmIEInd(pMac,
                                               psessionEntry,
                                               pAssocRsp->tsmIE.tsid,
                                               pAssocRsp->tsmIE.state,
                                               pAssocRsp->tsmIE.msmt_interval);
#else
                            limActivateTSMStatsTimer(pMac, psessionEntry);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
                            if(psessionEntry->eseContext.tsm.tsmInfo.state) {
                                psessionEntry->eseContext.tsm.tsmMetrics.RoamingCount++;
                            }
                            break;
                        }
                    }
                } else {
                    limLog(pMac, LOGE, "TSM present but TSPEC IE not present in Reassoc Rsp");
                }
            }
        }
#endif
        if (psessionEntry->pLimMlmJoinReq)
        {
            vos_mem_free(psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }

        psessionEntry->limAssocResponseData  = (void *) pAssocRsp; /** Store the ReAssocRsp Frame in DphTable to be used 
                                                        during processing DelSta nd DelBss to send AddBss again*/
        pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

        if(!pStaDs)
        {
            PELOGE(limLog(pMac, LOGE, FL("could not get hash entry at DPH for"));)
            limPrintMacAddr(pMac, pHdr->sa, LOGE);
            mlmAssocCnf.resultCode = eSIR_SME_INVALID_ASSOC_RSP_RXED;
            mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
            
            // Send advisory Disassociation frame to AP
            limSendDisassocMgmtFrame(pMac, eSIR_MAC_UNSPEC_FAILURE_REASON,
                                     pHdr->sa, psessionEntry, FALSE);
            
            goto assocReject;
        }

#if defined(WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        if (psessionEntry->limMlmState == eLIM_MLM_WT_FT_REASSOC_RSP_STATE)
        {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
            PELOGE(limLog(pMac, LOG1, FL("Sending self sta"));)
#endif
            pmmResetPmmState(pMac);

            limUpdateAssocStaDatas(pMac, pStaDs, pAssocRsp,psessionEntry);

            // Store assigned AID for TIM processing
            psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;

            // Downgrade the EDCA parameters if needed
            limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

            // Send the active EDCA parameters to HAL
            if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE)
            {
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
            }
            else
            {
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
            }
            limAddFTStaSelf(pMac, (pAssocRsp->aid & 0x3FFF), psessionEntry);
            vos_mem_free(pBeaconStruct);

            return;
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */

        /* If we're re-associating to the same BSS, we don't want to invoke delete
         * STA, delete BSS, as that would remove the already established TSPEC.  
         * Just go ahead and re-add the BSS, STA with new capability information. 
         * However, if we're re-associating to a different BSS, then follow thru
         * with del STA, del BSS, add BSS, add STA.       
         */
        if (sirCompareMacAddr( psessionEntry->bssId, psessionEntry->limReAssocbssId))
            limHandleAddBssInReAssocContext(pMac, pStaDs, psessionEntry);
        else
        {
            // reset the uapsd mask settings since we're re-associating to new AP
            pMac->lim.gUapsdPerAcDeliveryEnableMask = 0;
            pMac->lim.gUapsdPerAcTriggerEnableMask = 0;

            if (limCleanupRxPath(pMac, pStaDs,psessionEntry) != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("Could not cleanup the rx path"));)
                goto assocReject;
            }
        }
        vos_mem_free(pBeaconStruct);

        return;
    }

    // Log success
    PELOG1(limLog(pMac, LOG1, FL("Successfully Associated with BSS "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(pHdr->sa));)
#ifdef FEATURE_WLAN_ESE
    if(psessionEntry->eseContext.tsm.tsmInfo.state)
    {
        psessionEntry->eseContext.tsm.tsmMetrics.RoamingCount = 0;
    }
#endif
    /**
     * Update the status for PMM module
     */
    pmmResetPmmState(pMac);

    // Store assigned AID for TIM processing
    psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;


    //STA entry was created during pre-assoc state.
    if ((pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable)) == NULL) 
    {
        // Could not add hash table entry
        PELOGE(limLog(pMac, LOGE, FL("could not get hash entry at DPH for "));)
        limPrintMacAddr(pMac, pHdr->sa, LOGE);

        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmAssocCnf.protStatusCode = eSIR_SME_SUCCESS;
        

        limPostSmeMessage(pMac, LIM_MLM_ASSOC_CNF,
                              (tANI_U32 *) &mlmAssocCnf);
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        return;
    }
   
     // Delete Pre-auth context for the associated BSS
    if (limSearchPreAuthList(pMac, pHdr->sa))
        limDeletePreAuthNode(pMac, pHdr->sa);

    limUpdateAssocStaDatas(pMac, pStaDs, pAssocRsp,psessionEntry);
    // Extract the AP capabilities from the beacon that was received earlier
    // TODO - Watch out for an error response!
    limExtractApCapabilities( pMac,
                            (tANI_U8 *) psessionEntry->pLimJoinReq->bssDescription.ieFields,
                            limGetIElenFromBssDescription( &psessionEntry->pLimJoinReq->bssDescription ),
                            pBeaconStruct );

    if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideStaProtectionOnAssoc(pMac, pBeaconStruct, psessionEntry);
    
    if(pBeaconStruct->erpPresent) {
        if (pBeaconStruct->erpIEInfo.barkerPreambleMode)
            psessionEntry->beaconParams.fShortPreamble = false;
        else
            psessionEntry->beaconParams.fShortPreamble = true;
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_CONNECTED, psessionEntry, 0, 0);
#endif
    if(pAssocRsp->OBSSScanParameters.present)
    {
        limUpdateOBSSScanParams(psessionEntry , &pAssocRsp->OBSSScanParameters);
    }

    if( pAssocRsp->QosMapSet.present )
    {
        vos_mem_copy(&psessionEntry->QosMapSet,
                     &pAssocRsp->QosMapSet,
                     sizeof(tSirQosMapSet));
    }
    else
    {
       vos_mem_zero(&psessionEntry->QosMapSet, sizeof(tSirQosMapSet));
    }

     //Update the BSS Entry, this entry was added during preassoc.
    if( eSIR_SUCCESS == limStaSendAddBss( pMac, pAssocRsp,  pBeaconStruct,
                   &psessionEntry->pLimJoinReq->bssDescription, true, psessionEntry))  
    {
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);
        return;
    }
    else
    {
        PELOGE(limLog(pMac, LOGE, FL("could not update the bss entry"));)
        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
    }

  

assocReject:
    if ((subType == LIM_ASSOC)
#ifdef WLAN_FEATURE_VOWIFI_11R
                    || ((subType == LIM_REASSOC) && (psessionEntry->limMlmState == eLIM_MLM_WT_FT_REASSOC_RSP_STATE))
#endif
       ) {
        PELOGE(limLog(pMac, LOGE,  FL("Assoc Rejected by the peer. "
                    "mlmestate: %d sessionid %d Reason: %d MACADDR:"
                    MAC_ADDRESS_STR), psessionEntry->limMlmState,
                    psessionEntry->peSessionId, mlmAssocCnf.resultCode,
                    MAC_ADDR_ARRAY(pHdr->sa));)
        psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

        if (psessionEntry->pLimMlmJoinReq)
        {
            vos_mem_free(psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }

        if (subType == LIM_ASSOC)
        {
           limPostSmeMessage(pMac, LIM_MLM_ASSOC_CNF, (tANI_U32 *) &mlmAssocCnf);
        }
#ifdef WLAN_FEATURE_VOWIFI_11R
        else
        {
                mlmAssocCnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
                limPostSmeMessage(pMac, LIM_MLM_REASSOC_CNF, (tANI_U32 *) &mlmAssocCnf);
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */
    } else {
        limRestorePreReassocState( pMac, 
                  eSIR_SME_REASSOC_REFUSED, mlmAssocCnf.protStatusCode,psessionEntry); 
    }

    /* CR: vos packet memory is leaked when assoc rsp timeouted/failed. */
    /* notify TL that association is failed so that TL can flush the cached frame  */
    PELOG1(limLog(pMac, LOG1,  FL("notify TL that association is failed"));)
    WLANTL_AssocFailed (psessionEntry->staId);

    vos_mem_free(pBeaconStruct);
    vos_mem_free(pAssocRsp);
    return;
} /*** end limProcessAssocRspFrame() ***/

