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
 * This file limSendSmeRspMessages.cc contains the functions
 * for sending SME response/notification messages to applications
 * above MAC software.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "vos_types.h"
#include "wniApi.h"
#include "sirCommon.h"
#include "aniGlobal.h"

#include "wniCfgSta.h"
#include "sysDef.h"
#include "cfgApi.h"


#include "schApi.h"
#include "utilsApi.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limSendSmeRspMessages.h"
#include "limIbssPeerMgmt.h"
#include "limSessionUtils.h"


/**
 * limSendSmeRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() to send
 * eWNI_SME_START_RSP, eWNI_SME_MEASUREMENT_RSP, eWNI_SME_STOP_BSS_RSP
 * or eWNI_SME_SWITCH_CHL_RSP messages to applications above MAC
 * Software.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param msgType      Indicates message type
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_msgType_REQ message
 *
 * @return None
 */

void
limSendSmeRsp(tpAniSirGlobal pMac, tANI_U16 msgType,
              tSirResultCodes resultCode,tANI_U8 smesessionId, tANI_U16 smetransactionId)
{
    tSirMsgQ    mmhMsg;
    tSirSmeRsp  *pSirSmeRsp;

    PELOG1(limLog(pMac, LOG1,
           FL("Sending message %s with reasonCode %s"),
           limMsgStr(msgType), limResultCodeStr(resultCode));)

    pSirSmeRsp = vos_mem_malloc(sizeof(tSirSmeRsp));
    if ( NULL == pSirSmeRsp )
    {
        /// Buffer not available. Log error
        limLog(pMac, LOGP,
               FL("call to AllocateMemory failed for eWNI_SME_*_RSP"));

        return;
    }
  
    pSirSmeRsp->messageType = msgType;
    pSirSmeRsp->length      = sizeof(tSirSmeRsp);
    pSirSmeRsp->statusCode  = resultCode;

    /* Update SME session Id and Transaction Id */
    pSirSmeRsp->sessionId = smesessionId;
    pSirSmeRsp->transactionId = smetransactionId;


    mmhMsg.type = msgType;
    mmhMsg.bodyptr = pSirSmeRsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, smesessionId, mmhMsg.type));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
   {
    tpPESession psessionEntry = peGetValidPowerSaveSession(pMac);
    switch(msgType)
    {
        case eWNI_PMC_ENTER_BMPS_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_BMPS_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_PMC_EXIT_BMPS_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_BMPS_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_PMC_ENTER_IMPS_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_IMPS_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;        
        case eWNI_PMC_EXIT_IMPS_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_IMPS_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_PMC_ENTER_UAPSD_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_UAPSD_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_PMC_EXIT_UAPSD_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_UAPSD_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_SME_SWITCH_CHL_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_SWITCH_CHL_RSP_EVENT, NULL, (tANI_U16)resultCode, 0);
            break;
        case eWNI_SME_STOP_BSS_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_STOP_BSS_RSP_EVENT, NULL, (tANI_U16)resultCode, 0);
            break;      
        case eWNI_PMC_ENTER_WOWL_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_WOWL_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;
        case eWNI_PMC_EXIT_WOWL_RSP:
            limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_WOWL_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
            break;          
    } 
   }  
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
} /*** end limSendSmeRsp() ***/


/**
 * limSendSmeJoinReassocRspAfterResume()
 *
 *FUNCTION:
 * This function is called to send Join/Reassoc rsp
 * message to SME after the resume link.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param status       Resume link status 
 * @param ctx          context passed while calling resmune link.
 *                     (join response to be sent)
 *
 * @return None
 */
static void limSendSmeJoinReassocRspAfterResume( tpAniSirGlobal pMac, 
                                       eHalStatus status, tANI_U32 *ctx)
{
    tSirMsgQ         mmhMsg;
    tpSirSmeJoinRsp  pSirSmeJoinRsp = (tpSirSmeJoinRsp) ctx;

    mmhMsg.type = pSirSmeJoinRsp->messageType;
    mmhMsg.bodyptr = pSirSmeJoinRsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
}

/**
 * limGetMaxRateFlags()
 *
 *FUNCTION:
 *This function is called by limSendSmeJoinReassocRsp get rateFlags.
 *These rateflags are used when MAX link-speed need to be reported
 *to UI.
 *
 *PARAMS:
 * @param  pStaDs - Pointer to internal STA Datastructure
 * @param  psessionEntry - Pointer to the session entry
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @return rateFlags
 */
tANI_U32 limGetMaxRateFlags(tpDphHashNode pStaDs, tpPESession psessionEntry)
{
    tANI_U32 rate_flags = 0;

   if (NULL == psessionEntry)
    {
        return rate_flags;
    }

    if(!IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
       !IS_DOT11_MODE_VHT(psessionEntry->dot11mode))
    {
       rate_flags |= eHAL_TX_RATE_LEGACY;
    }
    else
    {
        if(IS_DOT11_MODE_HT(psessionEntry->dot11mode))
        {
            if (pStaDs->htShortGI20Mhz || pStaDs->htShortGI40Mhz )
                rate_flags |= eHAL_TX_RATE_SGI;

            if (pStaDs->htSupportedChannelWidthSet)
                rate_flags |=eHAL_TX_RATE_HT40;
            else
                rate_flags |=eHAL_TX_RATE_HT20;
        }
#ifdef WLAN_FEATURE_11AC
        if(IS_DOT11_MODE_VHT(psessionEntry->dot11mode))
        {
            if (WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ ==
                            pStaDs->vhtSupportedChannelWidthSet)
            {
                rate_flags |= eHAL_TX_RATE_VHT80;
            }
            else if(WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ ==
                           pStaDs->vhtSupportedChannelWidthSet)
            {
                if (eHT_CHANNEL_WIDTH_40MHZ ==
                               pStaDs->htSupportedChannelWidthSet)
                    rate_flags |= eHAL_TX_RATE_VHT40;
                else
                    rate_flags |= eHAL_TX_RATE_VHT20;
           }
        }
#endif
    }

     return rate_flags;
}

/**
 * limSendSmeJoinReassocRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() to send
 * eWNI_SME_JOIN_RSP or eWNI_SME_REASSOC_RSP messages to applications
 * above MAC Software.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param msgType      Indicates message type
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_msgType_REQ message
 *
 * @return None
 */

void
limSendSmeJoinReassocRsp(tpAniSirGlobal pMac, tANI_U16 msgType,
                         tSirResultCodes resultCode, tANI_U16 protStatusCode,
                         tpPESession psessionEntry,tANI_U8 smesessionId,tANI_U16 smetransactionId)
{
    tpSirSmeJoinRsp  pSirSmeJoinRsp;
    tANI_U32 rspLen;
    tpDphHashNode pStaDs    = NULL;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    if (msgType == eWNI_SME_REASSOC_RSP)
        limDiagEventReport(pMac, WLAN_PE_DIAG_REASSOC_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
    else
        limDiagEventReport(pMac, WLAN_PE_DIAG_JOIN_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    PELOG1(limLog(pMac, LOG1,
           FL("Sending message %s with reasonCode %s"),
           limMsgStr(msgType), limResultCodeStr(resultCode));)

    if(psessionEntry == NULL)
    {

        rspLen = sizeof(tSirSmeJoinRsp);   

        pSirSmeJoinRsp = vos_mem_malloc(rspLen);
        if ( NULL == pSirSmeJoinRsp )
        {
            /// Buffer not available. Log error
            limLog(pMac, LOGP, FL("call to AllocateMemory failed for JOIN/REASSOC_RSP"));
            return;
        }
         
        vos_mem_set((tANI_U8*)pSirSmeJoinRsp, rspLen, 0);
         
         
        pSirSmeJoinRsp->beaconLength = 0;
        pSirSmeJoinRsp->assocReqLength = 0;
        pSirSmeJoinRsp->assocRspLength = 0;
    }

    else
    {
        rspLen = psessionEntry->assocReqLen + psessionEntry->assocRspLen + 
            psessionEntry->bcnLen + 
#ifdef WLAN_FEATURE_VOWIFI_11R
            psessionEntry->RICDataLen +
#endif
#ifdef FEATURE_WLAN_ESE
            psessionEntry->tspecLen + 
#endif            
            sizeof(tSirSmeJoinRsp) - sizeof(tANI_U8) ;

        pSirSmeJoinRsp = vos_mem_malloc(rspLen);
        if ( NULL == pSirSmeJoinRsp )
        {
            /// Buffer not available. Log error
            limLog(pMac, LOGP,
               FL("call to AllocateMemory failed for JOIN/REASSOC_RSP"));

            return;
        }

        vos_mem_set((tANI_U8*)pSirSmeJoinRsp, rspLen, 0);

        if (resultCode == eSIR_SME_SUCCESS)
        {
            pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
            if (pStaDs == NULL)
            {
                PELOGE(limLog(pMac, LOGE, FL("could not Get Self Entry for the station"));)
            }
            else
            {
                //Pass the peer's staId
                pSirSmeJoinRsp->staId = pStaDs->staIndex;
                pSirSmeJoinRsp->ucastSig   = pStaDs->ucUcastSig;
                pSirSmeJoinRsp->bcastSig   = pStaDs->ucBcastSig;
                pSirSmeJoinRsp->maxRateFlags =
                                limGetMaxRateFlags(pStaDs, psessionEntry);
                PELOGE(limLog(pMac, LOG1, FL("maxRateFlags: %x"),
                                              pSirSmeJoinRsp->maxRateFlags);)
            }
        }

        pSirSmeJoinRsp->beaconLength = 0;
        pSirSmeJoinRsp->assocReqLength = 0;
        pSirSmeJoinRsp->assocRspLength = 0;
#ifdef WLAN_FEATURE_VOWIFI_11R
        pSirSmeJoinRsp->parsedRicRspLen = 0;
#endif
#ifdef FEATURE_WLAN_ESE
        pSirSmeJoinRsp->tspecIeLen = 0;
#endif
        
        if(resultCode == eSIR_SME_SUCCESS)
        {

            if(psessionEntry->beacon != NULL)
            {
                pSirSmeJoinRsp->beaconLength = psessionEntry->bcnLen;
                vos_mem_copy( pSirSmeJoinRsp->frames, psessionEntry->beacon,
                              pSirSmeJoinRsp->beaconLength);
                vos_mem_free( psessionEntry->beacon);
                psessionEntry->beacon = NULL;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
                PELOG1(limLog(pMac, LOG1, FL("Beacon=%d"), psessionEntry->bcnLen);)
#endif
            }
        
            if(psessionEntry->assocReq != NULL)
            {
                pSirSmeJoinRsp->assocReqLength = psessionEntry->assocReqLen;
                vos_mem_copy( pSirSmeJoinRsp->frames + psessionEntry->bcnLen,
                              psessionEntry->assocReq, pSirSmeJoinRsp->assocReqLength);
                vos_mem_free( psessionEntry->assocReq);
                psessionEntry->assocReq = NULL;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
                PELOG1(limLog(pMac, LOG1, FL("AssocReq=%d"), psessionEntry->assocReqLen);)
#endif
            }
            if(psessionEntry->assocRsp != NULL)
            {
                pSirSmeJoinRsp->assocRspLength = psessionEntry->assocRspLen;
                vos_mem_copy( pSirSmeJoinRsp->frames + psessionEntry->bcnLen +
                                               psessionEntry->assocReqLen,
                              psessionEntry->assocRsp,
                              pSirSmeJoinRsp->assocRspLength);
                vos_mem_free( psessionEntry->assocRsp);
                psessionEntry->assocRsp = NULL;
            }           
#ifdef WLAN_FEATURE_VOWIFI_11R
            if(psessionEntry->ricData != NULL)
            {
                pSirSmeJoinRsp->parsedRicRspLen = psessionEntry->RICDataLen;
                vos_mem_copy( pSirSmeJoinRsp->frames + psessionEntry->bcnLen +
                                 psessionEntry->assocReqLen + psessionEntry->assocRspLen,
                              psessionEntry->ricData, pSirSmeJoinRsp->parsedRicRspLen);
                vos_mem_free(psessionEntry->ricData);
                psessionEntry->ricData = NULL;
                PELOG1(limLog(pMac, LOG1, FL("RicLength=%d"), pSirSmeJoinRsp->parsedRicRspLen);)
            }
#endif
#ifdef FEATURE_WLAN_ESE
            if(psessionEntry->tspecIes != NULL)
            {
                pSirSmeJoinRsp->tspecIeLen = psessionEntry->tspecLen;
                vos_mem_copy( pSirSmeJoinRsp->frames + psessionEntry->bcnLen +
                                psessionEntry->assocReqLen + psessionEntry->assocRspLen +
                                  psessionEntry->RICDataLen,
                              psessionEntry->tspecIes, pSirSmeJoinRsp->tspecIeLen);
                vos_mem_free(psessionEntry->tspecIes);
                psessionEntry->tspecIes = NULL;
                PELOG1(limLog(pMac, LOG1, FL("ESE-TspecLen=%d"), psessionEntry->tspecLen);)
            }
#endif            
            pSirSmeJoinRsp->aid = psessionEntry->limAID;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
            PELOG1(limLog(pMac, LOG1, FL("AssocRsp=%d"), psessionEntry->assocRspLen);)
#endif
        }
        else
        {

            if(psessionEntry->beacon != NULL)
            {
                vos_mem_free(psessionEntry->beacon);
                psessionEntry->beacon = NULL;
            }

            if(psessionEntry->assocReq != NULL)
            {
                vos_mem_free( psessionEntry->assocReq);
                psessionEntry->assocReq = NULL;
            }

            if(psessionEntry->assocRsp != NULL)
            {
                vos_mem_free( psessionEntry->assocRsp);
                psessionEntry->assocRsp = NULL;
            }

        }
    }


    pSirSmeJoinRsp->messageType = msgType;
    pSirSmeJoinRsp->length = (tANI_U16) rspLen;
    pSirSmeJoinRsp->statusCode = resultCode;
    pSirSmeJoinRsp->protStatusCode = protStatusCode;
    
    /* Update SME session ID and transaction Id */
    pSirSmeJoinRsp->sessionId = smesessionId;
    pSirSmeJoinRsp->transactionId = smetransactionId;
    
    if(IS_MCC_SUPPORTED && limIsLinkSuspended( pMac ) )
    {
        if( psessionEntry && psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE )
        {
            
#ifdef WLAN_FEATURE_11AC
            if (psessionEntry->vhtCapability)
            {
                ePhyChanBondState htSecondaryChannelOffset;
               /*Get 11ac cbState from 11n cbState*/
                 htSecondaryChannelOffset = limGet11ACPhyCBState(pMac, 
                                    psessionEntry->currentOperChannel,
                                    psessionEntry->htSecondaryChannelOffset,
                                    psessionEntry->apCenterChan,
                                    psessionEntry);
                peSetResumeChannel( pMac, psessionEntry->currentOperChannel, htSecondaryChannelOffset);
            }
            else 
#endif
               peSetResumeChannel( pMac, psessionEntry->currentOperChannel, psessionEntry->htSecondaryChannelOffset);
        }
        else
        {
            peSetResumeChannel( pMac, 0, 0);
        }
        limResumeLink( pMac, limSendSmeJoinReassocRspAfterResume, 
                                              (tANI_U32*) pSirSmeJoinRsp );
    }
    else
    {
        limSendSmeJoinReassocRspAfterResume( pMac, eHAL_STATUS_SUCCESS,
                                              (tANI_U32*) pSirSmeJoinRsp );
    }
} /*** end limSendSmeJoinReassocRsp() ***/


/**
 * limSendSmeStartBssRsp()
 *
 *FUNCTION:
 * This function is called to send eWNI_SME_START_BSS_RSP
 * message to applications above MAC Software.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param msgType      Indicates message type
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_msgType_REQ message
 *
 * @return None
 */

void
limSendSmeStartBssRsp(tpAniSirGlobal pMac,
                      tANI_U16 msgType, tSirResultCodes resultCode,tpPESession psessionEntry,
                      tANI_U8 smesessionId,tANI_U16 smetransactionId)
{


    tANI_U16            size = 0;
    tSirMsgQ            mmhMsg;
    tSirSmeStartBssRsp  *pSirSmeRsp;
    tANI_U16            ieLen;
    tANI_U16            ieOffset, curLen;

    PELOG1(limLog(pMac, LOG1, FL("Sending message %s with reasonCode %s"),
           limMsgStr(msgType), limResultCodeStr(resultCode));)

    size = sizeof(tSirSmeStartBssRsp);

    if(psessionEntry == NULL)
    {
         pSirSmeRsp = vos_mem_malloc(size);
         if ( NULL == pSirSmeRsp )
         {
            /// Buffer not available. Log error
            limLog(pMac, LOGP,FL("call to AllocateMemory failed for eWNI_SME_START_BSS_RSP"));
            return;
         }
         vos_mem_set((tANI_U8*)pSirSmeRsp, size, 0);
                      
    }
    else
    {
        //subtract size of beaconLength + Mac Hdr + Fixed Fields before SSID
        ieOffset = sizeof(tAniBeaconStruct) + SIR_MAC_B_PR_SSID_OFFSET;
        ieLen = pMac->sch.schObject.gSchBeaconOffsetBegin + pMac->sch.schObject.gSchBeaconOffsetEnd - ieOffset;
        //calculate the memory size to allocate
        size += ieLen;

        pSirSmeRsp = vos_mem_malloc(size);
        if ( NULL == pSirSmeRsp )
        {
            /// Buffer not available. Log error
            limLog(pMac, LOGP,
            FL("call to AllocateMemory failed for eWNI_SME_START_BSS_RSP"));

            return;
        }
        vos_mem_set((tANI_U8*)pSirSmeRsp, size, 0);
        size = sizeof(tSirSmeStartBssRsp);
        if (resultCode == eSIR_SME_SUCCESS)
        {

                sirCopyMacAddr(pSirSmeRsp->bssDescription.bssId, psessionEntry->bssId);
        
                /* Read beacon interval from session */
                pSirSmeRsp->bssDescription.beaconInterval = (tANI_U16) psessionEntry->beaconParams.beaconInterval;
                pSirSmeRsp->bssType         = psessionEntry->bssType;

                if (cfgGetCapabilityInfo( pMac, &pSirSmeRsp->bssDescription.capabilityInfo,psessionEntry)
                != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("could not retrieve Capabilities value"));

                limGetPhyMode(pMac, (tANI_U32 *)&pSirSmeRsp->bssDescription.nwType, psessionEntry);

#if 0
            if (wlan_cfgGetInt(pMac, WNI_CFG_CURRENT_CHANNEL, &len) != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("could not retrieve CURRENT_CHANNEL from CFG"));
           
#endif// TO SUPPORT BT-AMP 
            
                pSirSmeRsp->bssDescription.channelId = psessionEntry->currentOperChannel;

                pSirSmeRsp->bssDescription.aniIndicator = 1;

                curLen = pMac->sch.schObject.gSchBeaconOffsetBegin - ieOffset;
                vos_mem_copy( (tANI_U8 *) &pSirSmeRsp->bssDescription.ieFields,
                           pMac->sch.schObject.gSchBeaconFrameBegin + ieOffset,
                          (tANI_U32)curLen);

                vos_mem_copy( ((tANI_U8 *) &pSirSmeRsp->bssDescription.ieFields) + curLen,
                           pMac->sch.schObject.gSchBeaconFrameEnd,
                          (tANI_U32)pMac->sch.schObject.gSchBeaconOffsetEnd);


                //subtracting size of length indicator itself and size of pointer to ieFields
                pSirSmeRsp->bssDescription.length = sizeof(tSirBssDescription) -
                                                sizeof(tANI_U16) - sizeof(tANI_U32) +
                                                ieLen;
                //This is the size of the message, subtracting the size of the pointer to ieFields
                size += ieLen - sizeof(tANI_U32);
        }
    }

    pSirSmeRsp->messageType     = msgType;
    pSirSmeRsp->length          = size;

    /* Update SME session Id and transaction Id */
    pSirSmeRsp->sessionId       = smesessionId;
    pSirSmeRsp->transactionId   = smetransactionId;
    pSirSmeRsp->statusCode      = resultCode;
    if(psessionEntry != NULL )
    pSirSmeRsp->staId           = psessionEntry->staId; //else it will be always zero smeRsp StaID = 0 
      

    mmhMsg.type = msgType;
    mmhMsg.bodyptr = pSirSmeRsp;
    mmhMsg.bodyval = 0;
    if(psessionEntry == NULL)
    {
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    }
    else
    {
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_START_BSS_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
} /*** end limSendSmeStartBssRsp() ***/

#define LIM_MAX_NUM_OF_SCAN_RESULTS_REPORTED  20
#define LIM_SIZE_OF_EACH_BSS  400 // this is a rough estimate


/**
 * limSendSmeScanRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() to send
 * eWNI_SME_SCAN_RSP message to applications above MAC
 * Software.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param length       Indicates length of message
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_SCAN_REQ message
 *
 * @return None
 */

void
limSendSmeScanRsp(tpAniSirGlobal pMac, tANI_U16 length,
                  tSirResultCodes resultCode,tANI_U8  smesessionId,tANI_U16 smetranscationId)
{
    tSirMsgQ              mmhMsg;
    tpSirSmeScanRsp       pSirSmeScanRsp=NULL;
    tLimScanResultNode    *ptemp = NULL;
    tANI_U16              msgLen, allocLength, curMsgLen = 0;
    tANI_U16              i, bssCount;
    tANI_U8               *pbBuf;
    tSirBssDescription    *pDesc;

    PELOG1(limLog(pMac, LOG1,
       FL("Sending message SME_SCAN_RSP with length=%d reasonCode %s"),
       length, limResultCodeStr(resultCode));)

    if (resultCode != eSIR_SME_SUCCESS)
    {
        limPostSmeScanRspMessage(pMac, length, resultCode,smesessionId,smetranscationId);
        return;
    }

    mmhMsg.type = eWNI_SME_SCAN_RSP;
    i = 0;
    bssCount = 0;
    msgLen = 0;
    allocLength = LIM_MAX_NUM_OF_SCAN_RESULTS_REPORTED * LIM_SIZE_OF_EACH_BSS;
    pSirSmeScanRsp = vos_mem_malloc(allocLength);
    if ( NULL == pSirSmeScanRsp )
    {
        // Log error
        limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_SCAN_RSP"));

        return;
    }
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
    {
        //when ptemp is not NULL it is a left over
        ptemp = pMac->lim.gLimCachedScanHashTable[i];
        while(ptemp)
        {
            pbBuf = ((tANI_U8 *)pSirSmeScanRsp) + msgLen;
            if(0 == bssCount)
            {
                msgLen = sizeof(tSirSmeScanRsp) -
                           sizeof(tSirBssDescription) +
                           ptemp->bssDescription.length +
                           sizeof(ptemp->bssDescription.length);
                pDesc = pSirSmeScanRsp->bssDescription;
            }
            else
            {
                msgLen += ptemp->bssDescription.length +
                          sizeof(ptemp->bssDescription.length);
                pDesc = (tSirBssDescription *)pbBuf;
            }
            if( (allocLength < msgLen) ||
                 (LIM_MAX_NUM_OF_SCAN_RESULTS_REPORTED <= bssCount++) )
            {
                pSirSmeScanRsp->statusCode  =
                    eSIR_SME_MORE_SCAN_RESULTS_FOLLOW;
                pSirSmeScanRsp->messageType = eWNI_SME_SCAN_RSP;
                pSirSmeScanRsp->length      = curMsgLen;
                mmhMsg.bodyptr = pSirSmeScanRsp;
                mmhMsg.bodyval = 0;
                MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
                limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
                pSirSmeScanRsp = vos_mem_malloc(allocLength);
                if ( NULL == pSirSmeScanRsp )
                {
                    // Log error
                    limLog(pMac, LOGP,
                                 FL("call to AllocateMemory failed for eWNI_SME_SCAN_RSP"));
                    return;
                }
                msgLen = sizeof(tSirSmeScanRsp) -
                           sizeof(tSirBssDescription) +
                           ptemp->bssDescription.length +
                           sizeof(ptemp->bssDescription.length);
                pDesc = pSirSmeScanRsp->bssDescription;
                bssCount = 1;
            }
            curMsgLen = msgLen;

            PELOG2(limLog(pMac, LOG2, FL("ScanRsp : msgLen %d, bssDescr Len=%d"),
                          msgLen, ptemp->bssDescription.length);)
            pDesc->length
                    = ptemp->bssDescription.length;
            vos_mem_copy( (tANI_U8 *) &pDesc->bssId,
                          (tANI_U8 *) &ptemp->bssDescription.bssId,
                           ptemp->bssDescription.length);

            PELOG2(limLog(pMac, LOG2, FL("BssId "));
            limPrintMacAddr(pMac, ptemp->bssDescription.bssId, LOG2);)

            pSirSmeScanRsp->sessionId   = smesessionId;
            pSirSmeScanRsp->transcationId = smetranscationId;

            ptemp = ptemp->next;
        } //while(ptemp)
    } //for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)

    if(0 == bssCount)
    {
       limPostSmeScanRspMessage(pMac, length, resultCode, smesessionId, smetranscationId);
       if (NULL != pSirSmeScanRsp)
       {
           vos_mem_free( pSirSmeScanRsp);
           pSirSmeScanRsp = NULL;
       }
    }
    else
    {
        // send last message
        pSirSmeScanRsp->statusCode  = eSIR_SME_SUCCESS;
        pSirSmeScanRsp->messageType = eWNI_SME_SCAN_RSP;
        pSirSmeScanRsp->length = curMsgLen;

        /* Update SME session Id and SME transcation Id */
        pSirSmeScanRsp->sessionId   = smesessionId;
        pSirSmeScanRsp->transcationId = smetranscationId;

        mmhMsg.type = eWNI_SME_SCAN_RSP;
        mmhMsg.bodyptr = pSirSmeScanRsp;
        mmhMsg.bodyval = 0;
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
        limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
        PELOG2(limLog(pMac, LOG2, FL("statusCode : eSIR_SME_SUCCESS"));)
    }

    return;

} /*** end limSendSmeScanRsp() ***/

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/**
 * limSendSmeLfrScanRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() to send
 * eWNI_SME_SCAN_RSP message to applications above MAC Software
 * only for sending up the roam candidates.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param length       Indicates length of message
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_SCAN_REQ message
 *
 * @return None
 */

void
limSendSmeLfrScanRsp(tpAniSirGlobal pMac, tANI_U16 length,
                     tSirResultCodes resultCode,tANI_U8  smesessionId,tANI_U16 smetranscationId)
{
    tSirMsgQ              mmhMsg;
    tpSirSmeScanRsp       pSirSmeScanRsp=NULL;
    tLimScanResultNode    *ptemp = NULL;
    tANI_U16              msgLen, allocLength, curMsgLen = 0;
    tANI_U16              i, bssCount;
    tANI_U8               *pbBuf;
    tSirBssDescription    *pDesc;

    PELOG1(limLog(pMac, LOG1,
       FL("Sending message SME_SCAN_RSP with length=%d reasonCode %s\n"),
       length, limResultCodeStr(resultCode));)

    if (resultCode != eSIR_SME_SUCCESS)
    {
        limPostSmeScanRspMessage(pMac, length, resultCode,smesessionId,smetranscationId);
        return;
    }

    mmhMsg.type = eWNI_SME_SCAN_RSP;
    i = 0;
    bssCount = 0;
    msgLen = 0;
    allocLength = LIM_MAX_NUM_OF_SCAN_RESULTS_REPORTED * LIM_SIZE_OF_EACH_BSS;
    pSirSmeScanRsp = vos_mem_malloc(allocLength);
    if ( NULL == pSirSmeScanRsp )
    {
        // Log error
        limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_SCAN_RSP\n"));

        return;
    }
    for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)
    {
        //when ptemp is not NULL it is a left over
        ptemp = pMac->lim.gLimCachedLfrScanHashTable[i];
        while(ptemp)
        {
            pbBuf = ((tANI_U8 *)pSirSmeScanRsp) + msgLen;
            if(0 == bssCount)
            {
                msgLen = sizeof(tSirSmeScanRsp) -
                           sizeof(tSirBssDescription) +
                           ptemp->bssDescription.length +
                           sizeof(ptemp->bssDescription.length);
                pDesc = pSirSmeScanRsp->bssDescription;
            }
            else
            {
                msgLen += ptemp->bssDescription.length +
                          sizeof(ptemp->bssDescription.length);
                pDesc = (tSirBssDescription *)pbBuf;
            }
            if ( (allocLength < msgLen) ||
                 (LIM_MAX_NUM_OF_SCAN_RESULTS_REPORTED <= bssCount++) )
            {
                pSirSmeScanRsp->statusCode  =
                    eSIR_SME_MORE_SCAN_RESULTS_FOLLOW;
                pSirSmeScanRsp->messageType = eWNI_SME_SCAN_RSP;
                pSirSmeScanRsp->length      = curMsgLen;
                mmhMsg.bodyptr = pSirSmeScanRsp;
                mmhMsg.bodyval = 0;
                MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
                limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
                pSirSmeScanRsp = vos_mem_malloc(allocLength);
                if ( NULL == pSirSmeScanRsp )
                {
                    // Log error
                    limLog(pMac, LOGP,
                                 FL("call to AllocateMemory failed for eWNI_SME_SCAN_RSP\n"));
                    return;
                }
                msgLen = sizeof(tSirSmeScanRsp) -
                           sizeof(tSirBssDescription) +
                           ptemp->bssDescription.length +
                           sizeof(ptemp->bssDescription.length);
                pDesc = pSirSmeScanRsp->bssDescription;
                bssCount = 1;
            }
            curMsgLen = msgLen;

            PELOG2(limLog(pMac, LOG2, FL("ScanRsp : msgLen %d, bssDescr Len=%d\n"),
                          msgLen, ptemp->bssDescription.length);)
            pDesc->length
                    = ptemp->bssDescription.length;
            vos_mem_copy( (tANI_U8 *) &pDesc->bssId,
                          (tANI_U8 *) &ptemp->bssDescription.bssId,
                           ptemp->bssDescription.length);

            PELOG2(limLog(pMac, LOG2, FL("BssId "));
            limPrintMacAddr(pMac, ptemp->bssDescription.bssId, LOG2);)

            pSirSmeScanRsp->sessionId   = smesessionId;
            pSirSmeScanRsp->transcationId = smetranscationId;

            ptemp = ptemp->next;
        } //while(ptemp)
    } //for (i = 0; i < LIM_MAX_NUM_OF_SCAN_RESULTS; i++)

    if (0 == bssCount)
    {
       limPostSmeScanRspMessage(pMac, length, resultCode, smesessionId, smetranscationId);
       if (NULL != pSirSmeScanRsp)
       {
           vos_mem_free( pSirSmeScanRsp);
           pSirSmeScanRsp = NULL;
       }
    }
    else
    {
        // send last message
        pSirSmeScanRsp->statusCode  = eSIR_SME_SUCCESS;
        pSirSmeScanRsp->messageType = eWNI_SME_SCAN_RSP;
        pSirSmeScanRsp->length = curMsgLen;

        /* Update SME session Id and SME transcation Id */
        pSirSmeScanRsp->sessionId   = smesessionId;
        pSirSmeScanRsp->transcationId = smetranscationId;

        mmhMsg.type = eWNI_SME_SCAN_RSP;
        mmhMsg.bodyptr = pSirSmeScanRsp;
        mmhMsg.bodyval = 0;
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
        limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
        PELOG2(limLog(pMac, LOG2, FL("statusCode : eSIR_SME_SUCCESS\n"));)
    }

    return;

} /*** end limSendSmeLfrScanRsp() ***/
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD

/**
 * limPostSmeScanRspMessage()
 *
 *FUNCTION:
 * This function is called by limSendSmeScanRsp() to send
 * eWNI_SME_SCAN_RSP message with failed result code
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param length       Indicates length of message
 * @param resultCode   failed result code
 *
 * @return None
 */

void
limPostSmeScanRspMessage(tpAniSirGlobal    pMac,     
                      tANI_U16               length,
                      tSirResultCodes   resultCode,tANI_U8 smesessionId, tANI_U16 smetransactionId)
{
    tpSirSmeScanRsp   pSirSmeScanRsp;
    tSirMsgQ          mmhMsg;

    PELOG1(limLog(pMac, LOG1,
       FL("limPostSmeScanRspMessage: send SME_SCAN_RSP (len %d, reasonCode %s). "),
       length, limResultCodeStr(resultCode));)

    pSirSmeScanRsp = vos_mem_malloc(length);
    if ( NULL == pSirSmeScanRsp )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_SCAN_RSP"));
        return;
    }
    vos_mem_set((void*)pSirSmeScanRsp, length, 0);

    pSirSmeScanRsp->messageType = eWNI_SME_SCAN_RSP;
    pSirSmeScanRsp->length      = length;

    if(sizeof(tSirSmeScanRsp) <= length)
    {
        pSirSmeScanRsp->bssDescription->length = sizeof(tSirBssDescription);
    }

    pSirSmeScanRsp->statusCode  = resultCode;

    /*Update SME session Id and transaction Id */
    pSirSmeScanRsp->sessionId = smesessionId;
    pSirSmeScanRsp->transcationId = smetransactionId;
    
    mmhMsg.type = eWNI_SME_SCAN_RSP;
    mmhMsg.bodyptr = pSirSmeScanRsp;
    mmhMsg.bodyval = 0;

    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_SCAN_RSP_EVENT, NULL, (tANI_U16)resultCode, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;

}  /*** limPostSmeScanRspMessage ***/

#ifdef FEATURE_OEM_DATA_SUPPORT

/**
 * limSendSmeOemDataRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() to send
 * eWNI_SME_OEM_DATA_RSP message to applications above MAC
 * Software.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param pMsgBuf      Indicates the mlm message
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_OEM_DATA_RSP message
 *
 * @return None
 */

void limSendSmeOemDataRsp(tpAniSirGlobal pMac, tANI_U32* pMsgBuf, tSirResultCodes resultCode)
{
    tSirMsgQ                      mmhMsg;
    tSirOemDataRsp*               pSirSmeOemDataRsp=NULL;
    tLimMlmOemDataRsp*            pMlmOemDataRsp=NULL;
    tANI_U16                      msgLength;

    
    //get the pointer to the mlm message
    pMlmOemDataRsp = (tLimMlmOemDataRsp*)(pMsgBuf);

    msgLength = sizeof(tSirOemDataRsp);

    //now allocate memory for the char buffer
    pSirSmeOemDataRsp = vos_mem_malloc(msgLength);
    if (NULL == pSirSmeOemDataRsp)
    {
        limLog(pMac, LOGP, FL("call to AllocateMemory failed for pSirSmeOemDataRsp"));
        return;
    }

#if defined (ANI_LITTLE_BYTE_ENDIAN)
    sirStoreU16N((tANI_U8*)&pSirSmeOemDataRsp->length, msgLength);
    sirStoreU16N((tANI_U8*)&pSirSmeOemDataRsp->messageType, eWNI_SME_OEM_DATA_RSP);
#else
    pSirSmeOemDataRsp->length = msgLength;
    pSirSmeOemDataRsp->messageType = eWNI_SME_OEM_DATA_RSP;
#endif

    vos_mem_copy(pSirSmeOemDataRsp->oemDataRsp, pMlmOemDataRsp->oemDataRsp, OEM_DATA_RSP_SIZE);

    //Now free the memory from MLM Rsp Message
    vos_mem_free(pMlmOemDataRsp);

    mmhMsg.type = eWNI_SME_OEM_DATA_RSP;
    mmhMsg.bodyptr = pSirSmeOemDataRsp;
    mmhMsg.bodyval = 0;

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
}  /*** limSendSmeOemDataRsp ***/

#endif


/**
 * limSendSmeAuthRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_AUTH_RSP message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac        Pointer to Global MAC structure
 * @param statusCode   Indicates the result of previously issued
 *                     eWNI_SME_AUTH_REQ message
 *
 * @return None
 */
void
limSendSmeAuthRsp(tpAniSirGlobal pMac,
                  tSirResultCodes statusCode,
                  tSirMacAddr peerMacAddr,
                  tAniAuthType authType,
                  tANI_U16   protStatusCode,
                  tpPESession psessionEntry,tANI_U8 smesessionId,
                  tANI_U16 smetransactionId)
{
#if 0
    tSirMsgQ       mmhMsg;
    tSirSmeAuthRsp *pSirSmeAuthRsp;

    pSirSmeAuthRsp = vos_mem_malloc(sizeof(tSirSmeAuthRsp));
    if (NULL == pSirSmeAuthRsp)
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to AllocateMemory failed for eWNI_SME_AUTH_RSP"));

        return;
    }

   

    if(psessionEntry != NULL)
    {
        vos_mem_copy( (tANI_U8 *) pSirSmeAuthRsp->peerMacAddr,
                  (tANI_U8 *) peerMacAddr, sizeof(tSirMacAddr));
        pSirSmeAuthRsp->authType    = authType;
          
    }

    pSirSmeAuthRsp->messageType = eWNI_SME_AUTH_RSP;
    pSirSmeAuthRsp->length      = sizeof(tSirSmeAuthRsp);
    pSirSmeAuthRsp->statusCode  = statusCode;
    pSirSmeAuthRsp->protStatusCode = protStatusCode;
    
    /* Update SME session and transaction Id*/
    pSirSmeAuthRsp->sessionId = smesessionId;
    pSirSmeAuthRsp->transactionId = smetransactionId;  

    mmhMsg.type = eWNI_SME_AUTH_RSP;
    mmhMsg.bodyptr = pSirSmeAuthRsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, 0, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
#endif
} /*** end limSendSmeAuthRsp() ***/


void limSendSmeDisassocDeauthNtf( tpAniSirGlobal pMac,
                                eHalStatus status, tANI_U32 *pCtx )
{
    tSirMsgQ                mmhMsg;
    tSirMsgQ                *pMsg = (tSirMsgQ*) pCtx;

    mmhMsg.type = pMsg->type;
    mmhMsg.bodyptr = pMsg;
    mmhMsg.bodyval = 0;

    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
}
/**
 * limSendSmeDisassocNtf()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_DISASSOC_RSP/IND message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * This function is used for sending eWNI_SME_DISASSOC_CNF,
 * or eWNI_SME_DISASSOC_IND to host depending on
 * disassociation trigger.
 *
 * @param peerMacAddr       Indicates the peer MAC addr to which
 *                          disassociate was initiated
 * @param reasonCode        Indicates the reason for Disassociation
 * @param disassocTrigger   Indicates the trigger for Disassociation
 * @param aid               Indicates the STAID. This parameter is
 *                          present only on AP.
 *
 * @return None
 */
void
limSendSmeDisassocNtf(tpAniSirGlobal pMac,
                      tSirMacAddr peerMacAddr,
                      tSirResultCodes reasonCode,
                      tANI_U16 disassocTrigger,
                      tANI_U16 aid,
                      tANI_U8 smesessionId,
                      tANI_U16 smetransactionId,
                      tpPESession psessionEntry)
{

    tANI_U8                     *pBuf;
    tSirSmeDisassocRsp      *pSirSmeDisassocRsp;
    tSirSmeDisassocInd      *pSirSmeDisassocInd;
    tANI_U32 *pMsg;
    
    switch (disassocTrigger)
    {
        case eLIM_PEER_ENTITY_DISASSOC:
            if (reasonCode != eSIR_SME_STA_NOT_ASSOCIATED)
                return;

        case eLIM_HOST_DISASSOC:
            /**
             * Disassociation response due to
             * host triggered disassociation
             */

            pSirSmeDisassocRsp = vos_mem_malloc(sizeof(tSirSmeDisassocRsp));
            if ( NULL == pSirSmeDisassocRsp )
            {
                // Log error
                limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_DISASSOC_RSP"));

                return;
            }
            limLog(pMac, LOG1, FL("send eWNI_SME_DEAUTH_RSP with "
            "retCode: %d for "MAC_ADDRESS_STR),reasonCode,
            MAC_ADDR_ARRAY(peerMacAddr));
            pSirSmeDisassocRsp->messageType = eWNI_SME_DISASSOC_RSP;
            pSirSmeDisassocRsp->length      = sizeof(tSirSmeDisassocRsp);
            //sessionId
            pBuf = (tANI_U8 *) &pSirSmeDisassocRsp->sessionId;
            *pBuf = smesessionId;
            pBuf++;

            //transactionId
            limCopyU16(pBuf, smetransactionId);
            pBuf += sizeof(tANI_U16);

            //statusCode            
            limCopyU32(pBuf, reasonCode);
            pBuf += sizeof(tSirResultCodes);

            //peerMacAddr
            vos_mem_copy( pBuf, peerMacAddr, sizeof(tSirMacAddr));
            pBuf += sizeof(tSirMacAddr);

            // Clear Station Stats
            //for sta, it is always 1, IBSS is handled at halInitSta


          
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 

            limDiagEventReport(pMac, WLAN_PE_DIAG_DISASSOC_RSP_EVENT,
                                      psessionEntry, (tANI_U16)reasonCode, 0);
#endif
            pMsg = (tANI_U32*) pSirSmeDisassocRsp;
            break;

        default:
            /**
             * Disassociation indication due to Disassociation
             * frame reception from peer entity or due to
             * loss of link with peer entity.
             */
            pSirSmeDisassocInd = vos_mem_malloc(sizeof(tSirSmeDisassocInd));
            if ( NULL == pSirSmeDisassocInd )
            {
                // Log error
                limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_DISASSOC_IND"));

                return;
            }
            limLog(pMac, LOG1, FL("send eWNI_SME_DISASSOC_IND with "
            "retCode: %d for "MAC_ADDRESS_STR),reasonCode,
            MAC_ADDR_ARRAY(peerMacAddr));
            pSirSmeDisassocInd->messageType = eWNI_SME_DISASSOC_IND;
            pSirSmeDisassocInd->length      = sizeof(tSirSmeDisassocInd);
            
            /* Update SME session Id and Transaction Id */
            pSirSmeDisassocInd->sessionId = smesessionId;
            pSirSmeDisassocInd->transactionId = smetransactionId;
            pSirSmeDisassocInd->reasonCode = reasonCode;
            pBuf = (tANI_U8 *) &pSirSmeDisassocInd->statusCode;

            limCopyU32(pBuf, reasonCode);
            pBuf += sizeof(tSirResultCodes);

            vos_mem_copy( pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
            pBuf += sizeof(tSirMacAddr);

            vos_mem_copy( pBuf, peerMacAddr, sizeof(tSirMacAddr));


#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
            limDiagEventReport(pMac, WLAN_PE_DIAG_DISASSOC_IND_EVENT,
                                              psessionEntry, (tANI_U16)reasonCode, 0);
#endif
            pMsg = (tANI_U32*) pSirSmeDisassocInd;

            break;
    }

    /* Delete the PE session Created */
    if((psessionEntry != NULL) && ((psessionEntry ->limSystemRole ==  eLIM_STA_ROLE) ||
                                  (psessionEntry ->limSystemRole ==  eLIM_BT_AMP_STA_ROLE)) )
    {
        peDeleteSession(pMac,psessionEntry);
    }
        
    limSendSmeDisassocDeauthNtf( pMac, eHAL_STATUS_SUCCESS,
                                              (tANI_U32*) pMsg );
} /*** end limSendSmeDisassocNtf() ***/


/** -----------------------------------------------------------------
  \brief limSendSmeDisassocInd() - sends SME_DISASSOC_IND
   
  After receiving disassociation frame from peer entity, this 
  function sends a eWNI_SME_DISASSOC_IND to SME with a specific
  reason code.  
    
  \param pMac - global mac structure
  \param pStaDs - station dph hash node 
  \return none 
  \sa
  ----------------------------------------------------------------- */
void
limSendSmeDisassocInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs,tpPESession psessionEntry)
{
    tSirMsgQ  mmhMsg;
    tSirSmeDisassocInd *pSirSmeDisassocInd;

    pSirSmeDisassocInd = vos_mem_malloc(sizeof(tSirSmeDisassocInd));
    if ( NULL == pSirSmeDisassocInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_DISASSOC_IND"));
        return;
    }

    pSirSmeDisassocInd->messageType = eWNI_SME_DISASSOC_IND;
    pSirSmeDisassocInd->length = sizeof(tSirSmeDisassocInd);

    pSirSmeDisassocInd->sessionId     =  psessionEntry->smeSessionId;
    pSirSmeDisassocInd->transactionId =  psessionEntry->transactionId;
    pSirSmeDisassocInd->statusCode    =  pStaDs->mlmStaContext.disassocReason;
    pSirSmeDisassocInd->reasonCode    =  pStaDs->mlmStaContext.disassocReason;

    vos_mem_copy( pSirSmeDisassocInd->bssId, psessionEntry->bssId, sizeof(tSirMacAddr));

    vos_mem_copy( pSirSmeDisassocInd->peerMacAddr, pStaDs->staAddr, sizeof(tSirMacAddr));

    pSirSmeDisassocInd->staId = pStaDs->staIndex;

    mmhMsg.type = eWNI_SME_DISASSOC_IND;
    mmhMsg.bodyptr = pSirSmeDisassocInd;
    mmhMsg.bodyval = 0;

    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DISASSOC_IND_EVENT, psessionEntry, 0, (tANI_U16)pStaDs->mlmStaContext.disassocReason); 
#endif //FEATURE_WLAN_DIAG_SUPPORT

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
 
} /*** end limSendSmeDisassocInd() ***/


/** -----------------------------------------------------------------
  \brief limSendSmeDeauthInd() - sends SME_DEAUTH_IND
   
  After receiving deauthentication frame from peer entity, this 
  function sends a eWNI_SME_DEAUTH_IND to SME with a specific
  reason code.  
    
  \param pMac - global mac structure
  \param pStaDs - station dph hash node 
  \return none 
  \sa
  ----------------------------------------------------------------- */
void
limSendSmeDeauthInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry)                   
{
    tSirMsgQ  mmhMsg;
    tSirSmeDeauthInd  *pSirSmeDeauthInd;

    pSirSmeDeauthInd = vos_mem_malloc(sizeof(tSirSmeDeauthInd));
    if ( NULL == pSirSmeDeauthInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_DEAUTH_IND "));
        return;
    }

    pSirSmeDeauthInd->messageType = eWNI_SME_DEAUTH_IND;
    pSirSmeDeauthInd->length = sizeof(tSirSmeDeauthInd);

    pSirSmeDeauthInd->sessionId = psessionEntry->smeSessionId;
    pSirSmeDeauthInd->transactionId = psessionEntry->transactionId;
    if(eSIR_INFRA_AP_MODE == psessionEntry->bssType)
    {
        pSirSmeDeauthInd->statusCode = (tSirResultCodes)pStaDs->mlmStaContext.cleanupTrigger;
    }
    else
    {
        //Need to indicatet he reascon code over the air
        pSirSmeDeauthInd->statusCode = (tSirResultCodes)pStaDs->mlmStaContext.disassocReason;
    }
    //BSSID
    vos_mem_copy( pSirSmeDeauthInd->bssId, psessionEntry->bssId, sizeof(tSirMacAddr));
    //peerMacAddr
    vos_mem_copy( pSirSmeDeauthInd->peerMacAddr, pStaDs->staAddr, sizeof(tSirMacAddr));
    pSirSmeDeauthInd->reasonCode = pStaDs->mlmStaContext.disassocReason;


    pSirSmeDeauthInd->staId = pStaDs->staIndex;

    mmhMsg.type = eWNI_SME_DEAUTH_IND;
    mmhMsg.bodyptr = pSirSmeDeauthInd;
    mmhMsg.bodyval = 0;

    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DEAUTH_IND_EVENT, psessionEntry, 0, pStaDs->mlmStaContext.cleanupTrigger);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
} /*** end limSendSmeDeauthInd() ***/

#ifdef FEATURE_WLAN_TDLS
/**
 * limSendSmeTDLSDelStaInd()
 *
 *FUNCTION:
 * This function is called to send the TDLS STA context deletion to SME.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac   - Pointer to global MAC structure
 * @param  pStaDs - Pointer to internal STA Datastructure
 * @param  psessionEntry - Pointer to the session entry
 * @param  reasonCode - Reason for TDLS sta deletion
 * @return None
 */
void
limSendSmeTDLSDelStaInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry, tANI_U16 reasonCode)
{
    tSirMsgQ  mmhMsg;
    tSirTdlsDelStaInd  *pSirTdlsDelStaInd;

    pSirTdlsDelStaInd = vos_mem_malloc(sizeof(tSirTdlsDelStaInd));
    if ( NULL == pSirTdlsDelStaInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_TDLS_DEL_STA_IND "));
        return;
    }

    //messageType
    pSirTdlsDelStaInd->messageType = eWNI_SME_TDLS_DEL_STA_IND;
    pSirTdlsDelStaInd->length = sizeof(tSirTdlsDelStaInd);

    //sessionId
    pSirTdlsDelStaInd->sessionId = psessionEntry->smeSessionId;

    //peerMacAddr
    vos_mem_copy( pSirTdlsDelStaInd->peerMac, pStaDs->staAddr, sizeof(tSirMacAddr));

    //staId
    limCopyU16((tANI_U8*)(&pSirTdlsDelStaInd->staId), (tANI_U16)pStaDs->staIndex);

    //reasonCode
    limCopyU16((tANI_U8*)(&pSirTdlsDelStaInd->reasonCode), reasonCode);

    mmhMsg.type = eWNI_SME_TDLS_DEL_STA_IND;
    mmhMsg.bodyptr = pSirTdlsDelStaInd;
    mmhMsg.bodyval = 0;


    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
}/*** end limSendSmeTDLSDelStaInd() ***/

/**
 * limSendSmeTDLSDeleteAllPeerInd()
 *
 *FUNCTION:
 * This function is called to send the eWNI_SME_TDLS_DEL_ALL_PEER_IND
 * message to SME.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac   - Pointer to global MAC structure
 * @param  psessionEntry - Pointer to the session entry
 * @return None
 */
void
limSendSmeTDLSDeleteAllPeerInd(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tSirMsgQ  mmhMsg;
    tSirTdlsDelAllPeerInd  *pSirTdlsDelAllPeerInd;

    pSirTdlsDelAllPeerInd = vos_mem_malloc(sizeof(tSirTdlsDelAllPeerInd));
    if ( NULL == pSirTdlsDelAllPeerInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_TDLS_DEL_ALL_PEER_IND"));
        return;
    }

    //messageType
    pSirTdlsDelAllPeerInd->messageType = eWNI_SME_TDLS_DEL_ALL_PEER_IND;
    pSirTdlsDelAllPeerInd->length = sizeof(tSirTdlsDelAllPeerInd);

    //sessionId
    pSirTdlsDelAllPeerInd->sessionId = psessionEntry->smeSessionId;

    mmhMsg.type = eWNI_SME_TDLS_DEL_ALL_PEER_IND;
    mmhMsg.bodyptr = pSirTdlsDelAllPeerInd;
    mmhMsg.bodyval = 0;


    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
}/*** end limSendSmeTDLSDeleteAllPeerInd() ***/

/**
 * limSendSmeMgmtTXCompletion()
 *
 *FUNCTION:
 * This function is called to send the eWNI_SME_MGMT_FRM_TX_COMPLETION_IND
 * message to SME.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac   - Pointer to global MAC structure
 * @param  psessionEntry - Pointer to the session entry
 * @param  txCompleteStatus - TX Complete Status of Mgmt Frames
 * @return None
 */
void
limSendSmeMgmtTXCompletion(tpAniSirGlobal pMac,
                           tpPESession psessionEntry,
                           tANI_U32 txCompleteStatus)
{
    tSirMsgQ  mmhMsg;
    tSirMgmtTxCompletionInd  *pSirMgmtTxCompletionInd;

    pSirMgmtTxCompletionInd = vos_mem_malloc(sizeof(tSirMgmtTxCompletionInd));
    if ( NULL == pSirMgmtTxCompletionInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_MGMT_FRM_TX_COMPLETION_IND"));
        return;
    }

    //messageType
    pSirMgmtTxCompletionInd->messageType = eWNI_SME_MGMT_FRM_TX_COMPLETION_IND;
    pSirMgmtTxCompletionInd->length = sizeof(tSirMgmtTxCompletionInd);

    //sessionId
    pSirMgmtTxCompletionInd->sessionId = psessionEntry->smeSessionId;

    pSirMgmtTxCompletionInd->txCompleteStatus = txCompleteStatus;

    mmhMsg.type = eWNI_SME_MGMT_FRM_TX_COMPLETION_IND;
    mmhMsg.bodyptr = pSirMgmtTxCompletionInd;
    mmhMsg.bodyval = 0;


    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
}/*** end limSendSmeTDLSDeleteAllPeerInd() ***/
#endif


/**
 * limSendSmeDeauthNtf()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_DISASSOC_RSP/IND message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * This function is used for sending eWNI_SME_DEAUTH_CNF or
 * eWNI_SME_DEAUTH_IND to host depending on deauthentication trigger.
 *
 * @param peerMacAddr       Indicates the peer MAC addr to which
 *                          deauthentication was initiated
 * @param reasonCode        Indicates the reason for Deauthetication
 * @param deauthTrigger     Indicates the trigger for Deauthetication
 * @param aid               Indicates the STAID. This parameter is present
 *                          only on AP.
 *
 * @return None
 */
void
limSendSmeDeauthNtf(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tSirResultCodes reasonCode,
                    tANI_U16 deauthTrigger, tANI_U16 aid,tANI_U8 smesessionId, tANI_U16 smetransactionId)
{
    tANI_U8             *pBuf;
    tSirSmeDeauthRsp    *pSirSmeDeauthRsp;
    tSirSmeDeauthInd    *pSirSmeDeauthInd;
    tpPESession         psessionEntry;
    tANI_U8             sessionId;
    tANI_U32            *pMsg;

    psessionEntry = peFindSessionByBssid(pMac,peerMacAddr,&sessionId);  
    switch (deauthTrigger)
    {
        case eLIM_PEER_ENTITY_DEAUTH:
            return;
            
        case eLIM_HOST_DEAUTH:
            /**
             * Deauthentication response to host triggered
             * deauthentication.
             */
            pSirSmeDeauthRsp = vos_mem_malloc(sizeof(tSirSmeDeauthRsp));
            if ( NULL == pSirSmeDeauthRsp )
            {
                // Log error
                limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_DEAUTH_RSP"));

                return;
            }
            limLog(pMac, LOG1, FL("send eWNI_SME_DEAUTH_RSP with "
            "retCode: %d for"MAC_ADDRESS_STR),reasonCode,
            MAC_ADDR_ARRAY(peerMacAddr));
            pSirSmeDeauthRsp->messageType = eWNI_SME_DEAUTH_RSP;
            pSirSmeDeauthRsp->length      = sizeof(tSirSmeDeauthRsp);
            pSirSmeDeauthRsp->statusCode = reasonCode;
            pSirSmeDeauthRsp->sessionId = smesessionId;
            pSirSmeDeauthRsp->transactionId = smetransactionId;  

            pBuf  = (tANI_U8 *) pSirSmeDeauthRsp->peerMacAddr;
            vos_mem_copy( pBuf, peerMacAddr, sizeof(tSirMacAddr));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
            limDiagEventReport(pMac, WLAN_PE_DIAG_DEAUTH_RSP_EVENT,
                                      psessionEntry, 0, (tANI_U16)reasonCode);
#endif
            pMsg = (tANI_U32*)pSirSmeDeauthRsp;

            break;

        default:
            /**
             * Deauthentication indication due to Deauthentication
             * frame reception from peer entity or due to
             * loss of link with peer entity.
             */
            pSirSmeDeauthInd = vos_mem_malloc(sizeof(tSirSmeDeauthInd));
            if ( NULL == pSirSmeDeauthInd )
            {
                // Log error
                limLog(pMac, LOGP,
                   FL("call to AllocateMemory failed for eWNI_SME_DEAUTH_Ind"));

                return;
            }
            limLog(pMac, LOG1, FL("send eWNI_SME_DEAUTH_IND with "
            "retCode: %d for "MAC_ADDRESS_STR),reasonCode,
            MAC_ADDR_ARRAY(peerMacAddr));
            pSirSmeDeauthInd->messageType = eWNI_SME_DEAUTH_IND;
            pSirSmeDeauthInd->length      = sizeof(tSirSmeDeauthInd);
            pSirSmeDeauthInd->reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;

            // sessionId
            pBuf = (tANI_U8*) &pSirSmeDeauthInd->sessionId;
            *pBuf++ = smesessionId;

            //transaction ID
            limCopyU16(pBuf, smetransactionId);
            pBuf += sizeof(tANI_U16);

            // status code
            limCopyU32(pBuf, reasonCode);
            pBuf += sizeof(tSirResultCodes);

            //bssId
            vos_mem_copy( pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
            pBuf += sizeof(tSirMacAddr);

            //peerMacAddr
            vos_mem_copy( pSirSmeDeauthInd->peerMacAddr, peerMacAddr, sizeof(tSirMacAddr));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
            limDiagEventReport(pMac, WLAN_PE_DIAG_DEAUTH_IND_EVENT,
                                        psessionEntry, 0, (tANI_U16)reasonCode);
#endif //FEATURE_WLAN_DIAG_SUPPORT
            pMsg = (tANI_U32*)pSirSmeDeauthInd;

            break;
    }
    
    /*Delete the PE session  created */
    if(psessionEntry != NULL)
    {
        peDeleteSession(pMac,psessionEntry);
    }   

    limSendSmeDisassocDeauthNtf( pMac, eHAL_STATUS_SUCCESS,
                                              (tANI_U32*) pMsg );

} /*** end limSendSmeDeauthNtf() ***/


/**
 * limSendSmeWmStatusChangeNtf()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_WM_STATUS_CHANGE_NTF message to host.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param statusChangeCode   Indicates the change in the wireless medium.
 * @param statusChangeInfo   Indicates the information associated with
 *                           change in the wireless medium.
 * @param infoLen            Indicates the length of status change information
 *                           being sent.
 *
 * @return None
 */
void
limSendSmeWmStatusChangeNtf(tpAniSirGlobal pMac, tSirSmeStatusChangeCode statusChangeCode,
                                 tANI_U32 *pStatusChangeInfo, tANI_U16 infoLen, tANI_U8 sessionId)
{
    tSirMsgQ                  mmhMsg;
    tSirSmeWmStatusChangeNtf  *pSirSmeWmStatusChangeNtf;
    pSirSmeWmStatusChangeNtf = vos_mem_malloc(sizeof(tSirSmeWmStatusChangeNtf));
    if ( NULL == pSirSmeWmStatusChangeNtf )
    {
        limLog(pMac, LOGE,
          FL("call to AllocateMemory failed for eWNI_SME_WM_STATUS_CHANGE_NTF"));
          return;
    }


    mmhMsg.type = eWNI_SME_WM_STATUS_CHANGE_NTF;
    mmhMsg.bodyval = 0;
    mmhMsg.bodyptr = pSirSmeWmStatusChangeNtf;

    switch(statusChangeCode)
    {
        case eSIR_SME_RADAR_DETECTED:

            break;

        case eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP:
            break;

        case eSIR_SME_BACKGROUND_SCAN_FAIL:
            limPackBkgndScanFailNotify(pMac,
                                       statusChangeCode,
                                       (tpSirBackgroundScanInfo)pStatusChangeInfo,
                                       pSirSmeWmStatusChangeNtf, sessionId);
            break;

        default:
        pSirSmeWmStatusChangeNtf->messageType = eWNI_SME_WM_STATUS_CHANGE_NTF;
        pSirSmeWmStatusChangeNtf->statusChangeCode = statusChangeCode;
        pSirSmeWmStatusChangeNtf->length = sizeof(tSirSmeWmStatusChangeNtf);
        pSirSmeWmStatusChangeNtf->sessionId = sessionId;
        if(sizeof(pSirSmeWmStatusChangeNtf->statusChangeInfo) >= infoLen)
        {
            vos_mem_copy( (tANI_U8 *)&pSirSmeWmStatusChangeNtf->statusChangeInfo,
                          (tANI_U8 *)pStatusChangeInfo, infoLen);
        }
        limLog(pMac, LOGE, FL("***---*** StatusChg: code %d, length %d ***---***"),
               statusChangeCode, infoLen);
        break;
    }


    MTRACE(macTraceMsgTx(pMac, sessionId, mmhMsg.type));
    if (eSIR_SUCCESS != limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT))
    {
        vos_mem_free(pSirSmeWmStatusChangeNtf);
        limLog( pMac, LOGP, FL("limSysProcessMmhMsgApi failed"));
    }

} /*** end limSendSmeWmStatusChangeNtf() ***/


/**
 * limSendSmeSetContextRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_SETCONTEXT_RSP message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param pMac         Pointer to Global MAC structure
 * @param peerMacAddr  Indicates the peer MAC addr to which
 *                     setContext was performed
 * @param aid          Indicates the aid corresponding to the peer MAC
 *                     address
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_SETCONTEXT_RSP message
 *
 * @return None
 */
void
limSendSmeSetContextRsp(tpAniSirGlobal pMac,
                        tSirMacAddr peerMacAddr, tANI_U16 aid,
                        tSirResultCodes resultCode,
                        tpPESession psessionEntry,tANI_U8 smesessionId,tANI_U16 smetransactionId)
{

    tANI_U8                   *pBuf;
    tSirMsgQ             mmhMsg;
    tSirSmeSetContextRsp *pSirSmeSetContextRsp;

    pSirSmeSetContextRsp = vos_mem_malloc(sizeof(tSirSmeSetContextRsp));
    if ( NULL == pSirSmeSetContextRsp )
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to AllocateMemory failed for SmeSetContextRsp"));

        return;
    }

    pSirSmeSetContextRsp->messageType = eWNI_SME_SETCONTEXT_RSP;
    pSirSmeSetContextRsp->length      = sizeof(tSirSmeSetContextRsp);
    pSirSmeSetContextRsp->statusCode  = resultCode;

    pBuf = pSirSmeSetContextRsp->peerMacAddr;

    vos_mem_copy( pBuf, (tANI_U8 *) peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);


    /* Update SME session and transaction Id*/
    pSirSmeSetContextRsp->sessionId = smesessionId;
    pSirSmeSetContextRsp->transactionId = smetransactionId;

    mmhMsg.type = eWNI_SME_SETCONTEXT_RSP;
    mmhMsg.bodyptr = pSirSmeSetContextRsp;
    mmhMsg.bodyval = 0;
    if(NULL == psessionEntry)
    {
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    }
    else
    {
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_SETCONTEXT_RSP_EVENT, psessionEntry, (tANI_U16)resultCode, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
} /*** end limSendSmeSetContextRsp() ***/

/**
 * limSendSmeRemoveKeyRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_REMOVEKEY_RSP message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param pMac         Pointer to Global MAC structure
 * @param peerMacAddr  Indicates the peer MAC addr to which
 *                     Removekey was performed
 * @param aid          Indicates the aid corresponding to the peer MAC
 *                     address
 * @param resultCode   Indicates the result of previously issued
 *                     eWNI_SME_REMOVEKEY_RSP message
 *
 * @return None
 */
void
limSendSmeRemoveKeyRsp(tpAniSirGlobal pMac,
                        tSirMacAddr peerMacAddr,
                        tSirResultCodes resultCode,
                        tpPESession psessionEntry,tANI_U8 smesessionId,
                        tANI_U16 smetransactionId)
{
    tANI_U8                 *pBuf;
    tSirMsgQ                mmhMsg;
    tSirSmeRemoveKeyRsp     *pSirSmeRemoveKeyRsp;

    pSirSmeRemoveKeyRsp = vos_mem_malloc(sizeof(tSirSmeRemoveKeyRsp));
    if ( NULL == pSirSmeRemoveKeyRsp )
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to AllocateMemory failed for SmeRemoveKeyRsp"));

        return;
    }

    

    if(psessionEntry != NULL)
    {
        pBuf = pSirSmeRemoveKeyRsp->peerMacAddr;
        vos_mem_copy( pBuf, (tANI_U8 *) peerMacAddr, sizeof(tSirMacAddr));
    }
    
    pSirSmeRemoveKeyRsp->messageType = eWNI_SME_REMOVEKEY_RSP;
    pSirSmeRemoveKeyRsp->length      = sizeof(tSirSmeRemoveKeyRsp);
    pSirSmeRemoveKeyRsp->statusCode  = resultCode;
        
    /* Update SME session and transaction Id*/
    pSirSmeRemoveKeyRsp->sessionId = smesessionId;
    pSirSmeRemoveKeyRsp->transactionId = smetransactionId;   
    
    mmhMsg.type = eWNI_SME_REMOVEKEY_RSP;
    mmhMsg.bodyptr = pSirSmeRemoveKeyRsp;
    mmhMsg.bodyval = 0;
    if(NULL == psessionEntry)
    {
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    }
    else
    {
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
    }
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
} /*** end limSendSmeSetContextRsp() ***/


/**
 * limSendSmePromiscuousModeRsp()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() to send
 * eWNI_PROMISCUOUS_MODE_RSP message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * This function is used for sending eWNI_SME_PROMISCUOUS_MODE_RSP to
 * host as a reply to eWNI_SME_PROMISCUOUS_MODE_REQ directive from it.
 *
 * @param None
 * @return None
 */
void
limSendSmePromiscuousModeRsp(tpAniSirGlobal pMac)
{
#if 0
    tSirMsgQ   mmhMsg;
    tSirMbMsg  *pMbMsg;

    pMbMsg = vos_mem_malloc(sizeof(tSirMbMsg));
    if ( NULL == pMbMsg )
    {
        // Log error
        limLog(pMac, LOGP, FL("call to AllocateMemory failed"));

        return;
    }

    pMbMsg->type   = eWNI_SME_PROMISCUOUS_MODE_RSP;
    pMbMsg->msgLen = 4;

    mmhMsg.type = eWNI_SME_PROMISCUOUS_MODE_RSP;
    mmhMsg.bodyptr = pMbMsg;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, 0, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
#endif
} /*** end limSendSmePromiscuousModeRsp() ***/



/**
 * limSendSmeNeighborBssInd()
 *
 *FUNCTION:
 * This function is called by limLookupNaddHashEntry() to send
 * eWNI_SME_NEIGHBOR_BSS_IND message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * This function is used for sending eWNI_SME_NEIGHBOR_BSS_IND to
 * host upon detecting new BSS during background scanning if CFG
 * option is enabled for sending such indication
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limSendSmeNeighborBssInd(tpAniSirGlobal pMac,
                         tLimScanResultNode *pBssDescr)
{
    tSirMsgQ                 msgQ;
    tANI_U32                      val;
    tSirSmeNeighborBssInd    *pNewBssInd;

    if ((pMac->lim.gLimSmeState != eLIM_SME_LINK_EST_WT_SCAN_STATE) ||
        ((pMac->lim.gLimSmeState == eLIM_SME_LINK_EST_WT_SCAN_STATE) &&
         pMac->lim.gLimRspReqd))
    {
        // LIM is not in background scan state OR
        // current scan is initiated by HDD.
        // No need to send new BSS indication to HDD
        return;
    }

    if (wlan_cfgGetInt(pMac, WNI_CFG_NEW_BSS_FOUND_IND, &val) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not get NEIGHBOR_BSS_IND from CFG"));

        return;
    }

    if (val == 0)
        return;

    /**
     * Need to indicate new BSSs found during
     * background scanning to host.
     * Allocate buffer for sending indication.
     * Length of buffer is length of BSS description
     * and length of header itself
     */
    val = pBssDescr->bssDescription.length + sizeof(tANI_U16) + sizeof(tANI_U32) + sizeof(tANI_U8);
    pNewBssInd = vos_mem_malloc(val);
    if ( NULL == pNewBssInd )
    {
        // Log error
        limLog(pMac, LOGP,
           FL("call to AllocateMemory failed for eWNI_SME_NEIGHBOR_BSS_IND"));

        return;
    }

    pNewBssInd->messageType = eWNI_SME_NEIGHBOR_BSS_IND;
    pNewBssInd->length      = (tANI_U16) val;
    pNewBssInd->sessionId = 0;

    vos_mem_copy( (tANI_U8 *) pNewBssInd->bssDescription,
                  (tANI_U8 *) &pBssDescr->bssDescription,
                  pBssDescr->bssDescription.length + sizeof(tANI_U16));

    msgQ.type = eWNI_SME_NEIGHBOR_BSS_IND;
    msgQ.bodyptr = pNewBssInd;
    msgQ.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msgQ.type));
    limSysProcessMmhMsgApi(pMac, &msgQ, ePROT);
} /*** end limSendSmeNeighborBssInd() ***/

/** -----------------------------------------------------------------
  \brief limSendSmeAddtsRsp() - sends SME ADDTS RSP    
  \      This function sends a eWNI_SME_ADDTS_RSP to SME.   
  \      SME only looks at rc and tspec field. 
  \param pMac - global mac structure
  \param rspReqd - is SmeAddTsRsp required
  \param status - status code of SME_ADD_TS_RSP
  \return tspec
  \sa
  ----------------------------------------------------------------- */
void
limSendSmeAddtsRsp(tpAniSirGlobal pMac, tANI_U8 rspReqd, tANI_U32 status, tpPESession psessionEntry, 
        tSirMacTspecIE tspec, tANI_U8 smesessionId, tANI_U16 smetransactionId)  
{
    tpSirAddtsRsp     rsp;
    tSirMsgQ          mmhMsg;

    if (! rspReqd)
        return;

    rsp = vos_mem_malloc(sizeof(tSirAddtsRsp));
    if ( NULL == rsp )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for ADDTS_RSP"));
        return;
    }

    vos_mem_set( (tANI_U8 *) rsp, sizeof(*rsp), 0);
    rsp->messageType = eWNI_SME_ADDTS_RSP;
    rsp->rc = status;
    rsp->rsp.status = (enum eSirMacStatusCodes) status;
    //vos_mem_copy( (tANI_U8 *) &rsp->rsp.tspec, (tANI_U8 *) &addts->tspec, sizeof(addts->tspec));
    rsp->rsp.tspec = tspec;
    /* Update SME session Id and transcation Id */
    rsp->sessionId = smesessionId;
    rsp->transactionId = smetransactionId;
    
    mmhMsg.type = eWNI_SME_ADDTS_RSP;
    mmhMsg.bodyptr = rsp;
    mmhMsg.bodyval = 0;
    if(NULL == psessionEntry)
    {
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    }
    else
    {
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ADDTS_RSP_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
}

void
limSendSmeAddtsInd(tpAniSirGlobal pMac, tpSirAddtsReqInfo addts)
{
    tpSirAddtsRsp rsp;
    tSirMsgQ      mmhMsg;

    limLog(pMac, LOGW, "SendSmeAddtsInd (token %d, tsid %d, up %d)",
           addts->dialogToken,
           addts->tspec.tsinfo.traffic.tsid,
           addts->tspec.tsinfo.traffic.userPrio);

    rsp = vos_mem_malloc(sizeof(tSirAddtsRsp));
    if ( NULL == rsp )
    {
        // Log error
        limLog(pMac, LOGP, FL("AllocateMemory failed for ADDTS_IND"));
        return;
    }
    vos_mem_set( (tANI_U8 *) rsp, sizeof(*rsp), 0);

    rsp->messageType     = eWNI_SME_ADDTS_IND;

    vos_mem_copy( (tANI_U8 *) &rsp->rsp, (tANI_U8 *) addts, sizeof(*addts));

    mmhMsg.type = eWNI_SME_ADDTS_IND;
    mmhMsg.bodyptr = rsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
}

void
limSendSmeDeltsRsp(tpAniSirGlobal pMac, tpSirDeltsReq delts, tANI_U32 status,tpPESession psessionEntry,tANI_U8 smesessionId,tANI_U16 smetransactionId)
{
    tpSirDeltsRsp rsp;
    tSirMsgQ      mmhMsg;

    limLog(pMac, LOGW, "SendSmeDeltsRsp (aid %d, tsid %d, up %d) status %d",
           delts->aid,
           delts->req.tsinfo.traffic.tsid,
           delts->req.tsinfo.traffic.userPrio,
           status);
    if (! delts->rspReqd)
        return;

    rsp = vos_mem_malloc(sizeof(tSirDeltsRsp));
    if ( NULL == rsp )
    {
        // Log error
        limLog(pMac, LOGP, FL("AllocateMemory failed for DELTS_RSP"));
        return;
    }
    vos_mem_set( (tANI_U8 *) rsp, sizeof(*rsp), 0);
  
    if(psessionEntry != NULL)
    {
        
        rsp->aid             = delts->aid;
        vos_mem_copy( (tANI_U8 *) &rsp->macAddr[0], (tANI_U8 *) &delts->macAddr[0], 6);
        vos_mem_copy( (tANI_U8 *) &rsp->rsp, (tANI_U8 *) &delts->req, sizeof(tSirDeltsReqInfo));
    } 

    
    rsp->messageType     = eWNI_SME_DELTS_RSP;
    rsp->rc              = status;

    /* Update SME session Id and transcation Id */
    rsp->sessionId = smesessionId;
    rsp->transactionId = smetransactionId;

    mmhMsg.type = eWNI_SME_DELTS_RSP;
    mmhMsg.bodyptr = rsp;
    mmhMsg.bodyval = 0;
    if(NULL == psessionEntry)
    {
        MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    }
    else
    {
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DELTS_RSP_EVENT, psessionEntry, (tANI_U16)status, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
}

void
limSendSmeDeltsInd(tpAniSirGlobal pMac, tpSirDeltsReqInfo delts, tANI_U16 aid,tpPESession psessionEntry)
{
    tpSirDeltsRsp rsp;
    tSirMsgQ      mmhMsg;

    limLog(pMac, LOGW, "SendSmeDeltsInd (aid %d, tsid %d, up %d)",
           aid,
           delts->tsinfo.traffic.tsid,
           delts->tsinfo.traffic.userPrio);

    rsp = vos_mem_malloc(sizeof(tSirDeltsRsp));
    if ( NULL == rsp )
    {
        // Log error
        limLog(pMac, LOGP, FL("AllocateMemory failed for DELTS_IND"));
        return;
    }
    vos_mem_set( (tANI_U8 *) rsp, sizeof(*rsp), 0);

    rsp->messageType     = eWNI_SME_DELTS_IND;
    rsp->rc              = eSIR_SUCCESS;
    rsp->aid             = aid;
    vos_mem_copy( (tANI_U8 *) &rsp->rsp, (tANI_U8 *) delts, sizeof(*delts));

    /* Update SME  session Id and SME transaction Id */

    rsp->sessionId = psessionEntry->smeSessionId;
    rsp->transactionId = psessionEntry->transactionId;

    mmhMsg.type = eWNI_SME_DELTS_IND;
    mmhMsg.bodyptr = rsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DELTS_IND_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
}

/**
 * limSendSmeStatsRsp()
 *
 *FUNCTION:
 * This function is called to send 802.11 statistics response to HDD.
 * This function posts the result back to HDD. This is a response to
 * HDD's request for statistics.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param p80211Stats  Statistics sent in response 
 * @param resultCode   TODO:
 * 
 *
 * @return none
 */

void
limSendSmeStatsRsp(tpAniSirGlobal pMac, tANI_U16 msgType, void* stats)
{
    tSirMsgQ              mmhMsg;
    tSirSmeRsp           *pMsgHdr = (tSirSmeRsp*) stats;

    switch(msgType)
    {
        case WDA_STA_STAT_RSP:
            mmhMsg.type = eWNI_SME_STA_STAT_RSP;
            break;
        case WDA_AGGR_STAT_RSP:
            mmhMsg.type = eWNI_SME_AGGR_STAT_RSP;
            break;
        case WDA_GLOBAL_STAT_RSP:
            mmhMsg.type = eWNI_SME_GLOBAL_STAT_RSP;
            break;
        case WDA_STAT_SUMM_RSP:
            mmhMsg.type = eWNI_SME_STAT_SUMM_RSP;
            break;      
        default:
            mmhMsg.type = msgType; //Response from within PE
            break;
    }

    pMsgHdr->messageType = mmhMsg.type; 

    mmhMsg.bodyptr = stats;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);                                                  

    return;

} /*** end limSendSmeStatsRsp() ***/

/**
 * limSendSmePEStatisticsRsp()
 *
 *FUNCTION:
 * This function is called to send 802.11 statistics response to HDD.
 * This function posts the result back to HDD. This is a response to
 * HDD's request for statistics.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param p80211Stats  Statistics sent in response 
 * @param resultCode   TODO:
 * 
 *
 * @return none
 */

void
limSendSmePEStatisticsRsp(tpAniSirGlobal pMac, tANI_U16 msgType, void* stats)
{
    tSirMsgQ              mmhMsg;
    tANI_U8 sessionId;
    tAniGetPEStatsRsp *pPeStats = (tAniGetPEStatsRsp *) stats;
    tpPESession pPeSessionEntry;

    //Get the Session Id based on Sta Id
    pPeSessionEntry = peFindSessionByStaId(pMac, pPeStats->staId, &sessionId);

    //Fill the Session Id
    if(NULL != pPeSessionEntry)
    {
      //Fill the Session Id
      pPeStats->sessionId = pPeSessionEntry->smeSessionId;
    }
 
    pPeStats->msgType = eWNI_SME_GET_STATISTICS_RSP;
    

    //msgType should be WDA_GET_STATISTICS_RSP
    mmhMsg.type = eWNI_SME_GET_STATISTICS_RSP;

    mmhMsg.bodyptr = stats;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);                                                  

    return;

} /*** end limSendSmePEStatisticsRsp() ***/

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
/**
 * limSendSmePEGetRoamRssiRsp()
 *
 *FUNCTION:
 * This function is called to send roam rssi response to HDD.
 * This function posts the result back to HDD. This is a response to
 * HDD's request to get roam rssi.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac         Pointer to Global MAC structure
 * @param p80211Stats  Statistics sent in response
 * @param resultCode   TODO:
 *
 *
 * @return none
 */

void
limSendSmePEGetRoamRssiRsp(tpAniSirGlobal pMac, tANI_U16 msgType, void* stats)
{
    tSirMsgQ              mmhMsg;
    tANI_U8 sessionId;
    tAniGetRoamRssiRsp *pPeStats = (tAniGetRoamRssiRsp *) stats;
    tpPESession pPeSessionEntry = NULL;

    //Get the Session Id based on Sta Id
    pPeSessionEntry = peFindSessionByStaId(pMac, pPeStats->staId, &sessionId);

    //Fill the Session Id
    if(NULL != pPeSessionEntry)
    {
      //Fill the Session Id
      pPeStats->sessionId = pPeSessionEntry->smeSessionId;
    }

    pPeStats->msgType = eWNI_SME_GET_ROAM_RSSI_RSP;

    //msgType should be WDA_GET_STATISTICS_RSP
    mmhMsg.type = eWNI_SME_GET_ROAM_RSSI_RSP;

    mmhMsg.bodyptr = stats;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, sessionId, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;

} /*** end limSendSmePEGetRoamRssiRsp() ***/

#endif


#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/**
 * limSendSmePEEseTsmRsp()
 *
 *FUNCTION:
 * This function is called to send tsm stats response to HDD.
 * This function posts the result back to HDD. This is a response to
 * HDD's request to get tsm stats.
 *
 *PARAMS:
 * @param pMac   - Pointer to global pMac structure
 * @param pStats - Pointer to TSM Stats
 *
 * @return none
 */

void
limSendSmePEEseTsmRsp(tpAniSirGlobal pMac, tAniGetTsmStatsRsp *pStats)
{
    tSirMsgQ            mmhMsg;
    tANI_U8             sessionId;
    tAniGetTsmStatsRsp *pPeStats = (tAniGetTsmStatsRsp *) pStats;
    tpPESession         pPeSessionEntry = NULL;

    //Get the Session Id based on Sta Id
    pPeSessionEntry = peFindSessionByStaId(pMac, pPeStats->staId, &sessionId);

    //Fill the Session Id
    if(NULL != pPeSessionEntry)
    {
      //Fill the Session Id
      pPeStats->sessionId = pPeSessionEntry->smeSessionId;
    }
    else
    {
        PELOGE(limLog(pMac, LOGE, FL("Session not found for the Sta id(%d)"),
            pPeStats->staId);)
        return;
    }

    pPeStats->msgType = eWNI_SME_GET_TSM_STATS_RSP;
    pPeStats->tsmMetrics.RoamingCount = pPeSessionEntry->eseContext.tsm.tsmMetrics.RoamingCount;
    pPeStats->tsmMetrics.RoamingDly = pPeSessionEntry->eseContext.tsm.tsmMetrics.RoamingDly;

    mmhMsg.type = eWNI_SME_GET_TSM_STATS_RSP;
    mmhMsg.bodyptr = pStats;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, sessionId, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
} /*** end limSendSmePEEseTsmRsp() ***/

#endif /* FEATURE_WLAN_ESE) && FEATURE_WLAN_ESE_UPLOAD */


void
limSendSmeIBSSPeerInd(
    tpAniSirGlobal      pMac,
    tSirMacAddr peerMacAddr,
    tANI_U16    staIndex, 
    tANI_U8     ucastIdx,
    tANI_U8     bcastIdx,
    tANI_U8  *beacon, 
    tANI_U16 beaconLen, 
    tANI_U16 msgType,
    tANI_U8 sessionId)
{
    tSirMsgQ                  mmhMsg;
    tSmeIbssPeerInd *pNewPeerInd;
    
    pNewPeerInd = vos_mem_malloc(sizeof(tSmeIbssPeerInd) + beaconLen);
    if ( NULL == pNewPeerInd )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return;
    }
    
    vos_mem_set((void *) pNewPeerInd, (sizeof(tSmeIbssPeerInd) + beaconLen), 0);

    vos_mem_copy( (tANI_U8 *) pNewPeerInd->peerAddr,
                   peerMacAddr, sizeof(tSirMacAddr));
    pNewPeerInd->staId= staIndex;
    pNewPeerInd->ucastSig = ucastIdx;
    pNewPeerInd->bcastSig = bcastIdx;
    pNewPeerInd->mesgLen = sizeof(tSmeIbssPeerInd) + beaconLen;
    pNewPeerInd->mesgType = msgType;
    pNewPeerInd->sessionId = sessionId;

    if ( beacon != NULL )
    {
        vos_mem_copy((void*) ((tANI_U8*)pNewPeerInd+sizeof(tSmeIbssPeerInd)),
                     (void*)beacon, beaconLen);
    }

    mmhMsg.type    = msgType;
//    mmhMsg.bodyval = (tANI_U32) pNewPeerInd;
    mmhMsg.bodyptr = pNewPeerInd;
    MTRACE(macTraceMsgTx(pMac, sessionId, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    
}


/** -----------------------------------------------------------------
  \brief limSendExitBmpsInd() - sends exit bmps indication
   
  This function sends a eWNI_PMC_EXIT_BMPS_IND with a specific reason
  code to SME. This will trigger SME to get out of BMPS mode. 
    
  \param pMac - global mac structure
  \param reasonCode - reason for which PE wish to exit BMPS
  \return none 
  \sa
  ----------------------------------------------------------------- */
void limSendExitBmpsInd(tpAniSirGlobal pMac, tExitBmpsReason reasonCode)
{
    tSirMsgQ  mmhMsg;
    tANI_U16  msgLen = 0;
    tpSirSmeExitBmpsInd  pExitBmpsInd;
 
    msgLen = sizeof(tSirSmeExitBmpsInd);
    pExitBmpsInd = vos_mem_malloc(msgLen);
    if ( NULL == pExitBmpsInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for PMC_EXIT_BMPS_IND "));
        return;
    }
    vos_mem_set(pExitBmpsInd, msgLen, 0);

    pExitBmpsInd->mesgType = eWNI_PMC_EXIT_BMPS_IND;
    pExitBmpsInd->mesgLen = msgLen;
    pExitBmpsInd->exitBmpsReason = reasonCode;
    pExitBmpsInd->statusCode = eSIR_SME_SUCCESS;

    mmhMsg.type = eWNI_PMC_EXIT_BMPS_IND;
    mmhMsg.bodyptr = pExitBmpsInd;
    mmhMsg.bodyval = 0;
  
    PELOG1(limLog(pMac, LOG1, FL("Sending eWNI_PMC_EXIT_BMPS_IND to SME. "));)
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_BMPS_IND_EVENT, peGetValidPowerSaveSession(pMac), 0, (tANI_U16)reasonCode);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
    return;

} /*** end limSendExitBmpsInd() ***/




/*--------------------------------------------------------------------------
  \brief peDeleteSession() - Handle the Delete BSS Response from HAL.

    
  \param pMac                   - pointer to global adapter context
  \param sessionId             - Message pointer.
    
  \sa
  --------------------------------------------------------------------------*/

void limHandleDeleteBssRsp(tpAniSirGlobal pMac,tpSirMsgQ MsgQ)
{
    tpPESession psessionEntry;
    tpDeleteBssParams pDelBss = (tpDeleteBssParams)(MsgQ->bodyptr);
    if((psessionEntry = peFindSessionBySessionId(pMac,pDelBss->sessionId))==NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }
    if (psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE)
    {
        limIbssDelBssRsp(pMac, MsgQ->bodyptr,psessionEntry);
    }    
    else if(psessionEntry->limSystemRole == eLIM_UNKNOWN_ROLE)
    {
         limProcessSmeDelBssRsp(pMac, MsgQ->bodyval,psessionEntry);
    }
           
    else
         limProcessMlmDelBssRsp(pMac,MsgQ,psessionEntry);
    
}

#ifdef WLAN_FEATURE_VOWIFI_11R
/** -----------------------------------------------------------------
  \brief limSendSmeAggrQosRsp() - sends SME FT AGGR QOS RSP    
  \      This function sends a eWNI_SME_FT_AGGR_QOS_RSP to SME.   
  \      SME only looks at rc and tspec field. 
  \param pMac - global mac structure
  \param rspReqd - is SmeAddTsRsp required
  \param status - status code of eWNI_SME_FT_AGGR_QOS_RSP
  \return tspec
  \sa
  ----------------------------------------------------------------- */
void
limSendSmeAggrQosRsp(tpAniSirGlobal pMac, tpSirAggrQosRsp aggrQosRsp, 
                     tANI_U8 smesessionId)
{
    tSirMsgQ         mmhMsg;

    mmhMsg.type = eWNI_SME_FT_AGGR_QOS_RSP;
    mmhMsg.bodyptr = aggrQosRsp;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, smesessionId, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
}
#endif

/** -----------------------------------------------------------------
  \brief limSendSmePreChannelSwitchInd() - sends an indication to SME 
  before switching channels for spectrum manangement.
   
  This function sends a eWNI_SME_PRE_SWITCH_CHL_IND to SME.
    
  \param pMac - global mac structure
  \return none 
  \sa
  ----------------------------------------------------------------- */
void
limSendSmePreChannelSwitchInd(tpAniSirGlobal pMac)
{
    tSirMsgQ         mmhMsg;

    mmhMsg.type = eWNI_SME_PRE_SWITCH_CHL_IND;
    mmhMsg.bodyptr = NULL;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
}

/** -----------------------------------------------------------------
  \brief limSendSmePostChannelSwitchInd() - sends an indication to SME 
  after channel switch for spectrum manangement is complete.
   
  This function sends a eWNI_SME_POST_SWITCH_CHL_IND to SME.
    
  \param pMac - global mac structure
  \return none 
  \sa
  ----------------------------------------------------------------- */
void
limSendSmePostChannelSwitchInd(tpAniSirGlobal pMac)
{
    tSirMsgQ         mmhMsg;

    mmhMsg.type = eWNI_SME_POST_SWITCH_CHL_IND;
    mmhMsg.bodyptr = NULL;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
}

void limSendSmeMaxAssocExceededNtf(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,
                    tANI_U8 smesessionId)
{
    tSirMsgQ         mmhMsg;
    tSmeMaxAssocInd *pSmeMaxAssocInd;

    pSmeMaxAssocInd = vos_mem_malloc(sizeof(tSmeMaxAssocInd));
    if ( NULL == pSmeMaxAssocInd )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return;
    }    
    vos_mem_set((void *) pSmeMaxAssocInd, sizeof(tSmeMaxAssocInd),0);
    vos_mem_copy( (tANI_U8 *)pSmeMaxAssocInd->peerMac,
                  (tANI_U8 *)peerMacAddr, sizeof(tSirMacAddr));
    pSmeMaxAssocInd->mesgType  = eWNI_SME_MAX_ASSOC_EXCEEDED;
    pSmeMaxAssocInd->mesgLen    = sizeof(tSmeMaxAssocInd); 
    pSmeMaxAssocInd->sessionId = smesessionId;
    mmhMsg.type = pSmeMaxAssocInd->mesgType;
    mmhMsg.bodyptr = pSmeMaxAssocInd;
    PELOG1(limLog(pMac, LOG1, FL("msgType %s peerMacAddr "MAC_ADDRESS_STR
                  " sme session id %d"), "eWNI_SME_MAX_ASSOC_EXCEEDED", MAC_ADDR_ARRAY(peerMacAddr));)
    MTRACE(macTraceMsgTx(pMac, smesessionId, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return;
}
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/** -----------------------------------------------------------------
  \brief limSendSmeCandidateFoundInd() - sends
         eWNI_SME_CANDIDATE_FOUND_IND

  After receiving candidate found indication frame from FW, this
  function sends a eWNI_SME_CANDIDATE_FOUND_IND to SME to notify
  roam candidate(s) are available.

  \param pMac - global mac structure
  \param psessionEntry - session info
  \return none
  \sa
  ----------------------------------------------------------------- */
void
limSendSmeCandidateFoundInd(tpAniSirGlobal pMac, tANI_U8  sessionId)
{
    tSirMsgQ  mmhMsg;
    tSirSmeCandidateFoundInd *pSirSmeCandidateFoundInd;

    pSirSmeCandidateFoundInd = vos_mem_malloc(sizeof(tSirSmeCandidateFoundInd));
    if ( NULL == pSirSmeCandidateFoundInd )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_CANDIDATE_FOUND_IND\n"));
        return;
    }

    pSirSmeCandidateFoundInd->messageType = eWNI_SME_CANDIDATE_FOUND_IND;
    pSirSmeCandidateFoundInd->length = sizeof(tSirSmeDisassocInd);

    pSirSmeCandidateFoundInd->sessionId     =  sessionId;


    limLog( pMac, LOGE, FL("posting candidate ind to SME"));
    mmhMsg.type = eWNI_SME_CANDIDATE_FOUND_IND;
    mmhMsg.bodyptr = pSirSmeCandidateFoundInd;
    mmhMsg.bodyval = 0;

    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

} /*** end limSendSmeCandidateFoundInd() ***/
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD
