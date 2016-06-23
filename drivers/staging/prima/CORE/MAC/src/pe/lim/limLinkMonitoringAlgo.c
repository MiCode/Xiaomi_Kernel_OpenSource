/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * This file limLinkMonitoringAlgo.cc contains the code for
 * Link monitoring algorithm on AP and heart beat failure
 * handling on STA.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "aniGlobal.h"
#include "wniCfgSta.h"
#include "cfgApi.h"


#include "schApi.h"
#include "pmmApi.h"
#include "utilsApi.h"
#include "limAssocUtils.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limPropExtsUtils.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
#include "vos_diag_core_log.h"
#endif //FEATURE_WLAN_DIAG_SUPPORT
#include "limSession.h"
#include "limSerDesUtils.h"


/**
 * limSendKeepAliveToPeer()
 *
 *FUNCTION:
 * This function is called to send Keep alive message to peer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac        - Pointer to Global MAC structure
 * @return None
 */

void
limSendKeepAliveToPeer(tpAniSirGlobal pMac)
{

} /*** limSendKeepAliveToPeer() ***/


/** ---------------------------------------------------------
\fn      limDeleteStaContext
\brief   This function handles the message from HAL:
\        WDA_DELETE_STA_CONTEXT_IND. This function
\        validates that the given station id exist, and if so,
\        deletes the station by calling limTriggerSTAdeletion.
\param   tpAniSirGlobal pMac
\param   tpSirMsgQ      limMsg
\return  none
  -----------------------------------------------------------*/
void
limDeleteStaContext(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpDeleteStaContext  pMsg = (tpDeleteStaContext)limMsg->bodyptr;
    tpDphHashNode       pStaDs;
    tpPESession psessionEntry ;
    tANI_U8     sessionId;

    if(NULL == pMsg)
    {
        PELOGE(limLog(pMac, LOGE,FL("Invalid body pointer in message"));)
        return;
    }
    if((psessionEntry = peFindSessionByBssid(pMac,pMsg->bssId,&sessionId))== NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("session does not exist for given BSSId"));)
        vos_mem_free(pMsg);
        return;
    }

    switch(pMsg->reasonCode)
    {
        case HAL_DEL_STA_REASON_CODE_KEEP_ALIVE:
        case HAL_DEL_STA_REASON_CODE_TIM_BASED:
             PELOGE(limLog(pMac, LOGE, FL(" Deleting station: staId = %d, reasonCode = %d"), pMsg->staId, pMsg->reasonCode);)
             if (eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole)
             {
                 vos_mem_free(pMsg);
                 return;
             }

             pStaDs = dphLookupAssocId(pMac, pMsg->staId, &pMsg->assocId, &psessionEntry->dph.dphHashTable);

             if (!pStaDs)
             {
                 PELOGE(limLog(pMac, LOGE, FL("Skip STA deletion (invalid STA) limSystemRole=%d"),psessionEntry->limSystemRole);)
                 vos_mem_free(pMsg);
                 return;
             }

             /* check and see if same staId. This is to avoid the scenario
              * where we're trying to delete a staId we just added.
              */
             if (pStaDs->staIndex != pMsg->staId)
             {
                 PELOGE(limLog(pMac, LOGE, FL("staid mismatch: %d vs %d "), pStaDs->staIndex, pMsg->staId);)
                 vos_mem_free(pMsg);
                 return;
             }

             if((eLIM_BT_AMP_AP_ROLE == psessionEntry->limSystemRole) ||
                     (eLIM_AP_ROLE == psessionEntry->limSystemRole))
             {
                 PELOG1(limLog(pMac, LOG1, FL("SAP:lim Delete Station Context (staId: %d, assocId: %d) "),
                             pMsg->staId, pMsg->assocId);)
                 /*
                  * Check if Deauth/Disassoc is triggered from Host.
                  * If mlmState is in some transient state then
                  * don't trigger STA deletion to avoid the race
                  * condition.
                  */
                  if ((pStaDs &&
                      ((pStaDs->mlmStaContext.mlmState !=
                        eLIM_MLM_LINK_ESTABLISHED_STATE) &&
                       (pStaDs->mlmStaContext.mlmState !=
                        eLIM_MLM_WT_ASSOC_CNF_STATE) &&
                       (pStaDs->mlmStaContext.mlmState !=
                        eLIM_MLM_ASSOCIATED_STATE))))
                 {
                     PELOGE(limLog(pMac, LOGE, FL("SAP:received Del STA context in some transit state(staId: %d, assocId: %d)"),
                            pMsg->staId, pMsg->assocId);)
                     vos_mem_free(pMsg);
                     return;
                 }
                 else
                     limTriggerSTAdeletion(pMac, pStaDs, psessionEntry);
             }
             else
             {
#ifdef FEATURE_WLAN_TDLS
                if(eLIM_STA_ROLE == psessionEntry->limSystemRole &&
                    STA_ENTRY_TDLS_PEER == pStaDs->staType)
                {
                    //TeardownLink with PEER
                    //Reason code HAL_DEL_STA_REASON_CODE_KEEP_ALIVE means
                    //eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE
                    limSendSmeTDLSDelStaInd(pMac, pStaDs, psessionEntry,
                    /*pMsg->reasonCode*/ eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE);
                }
                else
                {
#endif
                    //TearDownLink with AP
                    tLimMlmDeauthInd  mlmDeauthInd;
                    PELOGW(limLog(pMac, LOGW, FL("lim Delete Station Context (staId: %d, assocId: %d) "),
                                pMsg->staId, pMsg->assocId);)
                    if ((pStaDs &&
                            ((pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_LINK_ESTABLISHED_STATE) &&
                             (pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_WT_ASSOC_CNF_STATE) &&
                             (pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_ASSOCIATED_STATE))))
                    {
                        /**
                         * Received WDA_DELETE_STA_CONTEXT_IND for STA that does not
                         * have context or in some transit state.
                         * Log error
                         */
                         PELOGE(limLog(pMac, LOGE,
                                FL("received WDA_DELETE_STA_CONTEXT_IND for STA that either has no context or in some transit state, Addr= "
                                    MAC_ADDRESS_STR), MAC_ADDR_ARRAY(pMsg->bssId));)
                         vos_mem_free(pMsg);
                         return;
                    }

                    if( pMac->roam.configParam.sendDeauthBeforeCon )
                    {
                       tANI_U8 apCount = pMac->lim.gLimHeartBeatApMacIndex;

                       if(pMac->lim.gLimHeartBeatApMacIndex)
                          pMac->lim.gLimHeartBeatApMacIndex = 0;
                       else
                          pMac->lim.gLimHeartBeatApMacIndex = 1;

                       limLog(pMac, LOG1, FL("lim Delete Station Context for MAC "
                                          MAC_ADDRESS_STR" Store it on Index %d"),
                                          MAC_ADDR_ARRAY(pStaDs->staAddr),apCount);

                       sirCopyMacAddr(pMac->lim.gLimHeartBeatApMac[apCount],pStaDs->staAddr);
                    }
                    pStaDs->mlmStaContext.disassocReason = eSIR_MAC_UNSPEC_FAILURE_REASON;
                    pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DEAUTH;

                   /** Set state to mlm State to eLIM_MLM_WT_DEL_STA_RSP_STATE
                    * This is to address the issue of race condition between
                    * disconnect request from the HDD and deauth from
                    * Tx inactivity timer by FWR. This will make sure that we will not
                    * process disassoc if deauth is in progress for the station
                    * and thus mlmStaContext.cleanupTrigger will not be overwritten.
                    */
                    pStaDs->mlmStaContext.mlmState   = eLIM_MLM_WT_DEL_STA_RSP_STATE;

                    // Issue Deauth Indication to SME.
                    vos_mem_copy((tANI_U8 *) &mlmDeauthInd.peerMacAddr,
                                  pStaDs->staAddr, sizeof(tSirMacAddr));
                    mlmDeauthInd.reasonCode    = (tANI_U8) pStaDs->mlmStaContext.disassocReason;
                    mlmDeauthInd.deauthTrigger =  pStaDs->mlmStaContext.cleanupTrigger;

#ifdef FEATURE_WLAN_TDLS
                    /* Delete all TDLS peers connected before leaving BSS*/
                    limDeleteTDLSPeers(pMac, psessionEntry);
#endif
                    limPostSmeMessage(pMac, LIM_MLM_DEAUTH_IND, (tANI_U32 *) &mlmDeauthInd);

                    limSendSmeDeauthInd(pMac, pStaDs, psessionEntry);
#ifdef FEATURE_WLAN_TDLS
                 }
#endif
             }
             break;        

        case HAL_DEL_STA_REASON_CODE_UNKNOWN_A2:
             PELOGE(limLog(pMac, LOGE, FL(" Deleting Unknown station "));)
             limPrintMacAddr(pMac, pMsg->addr2, LOGE);
             limSendDeauthMgmtFrame( pMac, eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON, pMsg->addr2, psessionEntry, FALSE);
             break;

        default:
             PELOGE(limLog(pMac, LOGE, FL(" Unknown reason code "));)
             break;

    }
    vos_mem_free(pMsg);
    return;
}


/**
 * limTriggerSTAdeletion()
 *
 *FUNCTION:
 * This function is called to trigger STA context deletion
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
 * @return None
 */
void
limTriggerSTAdeletion(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry)
{
    tSirSmeDeauthReq    *pSmeDeauthReq;
    tANI_U8             *pBuf;
    tANI_U8             *pLen;
    tANI_U16            msgLength = 0;

    if (! pStaDs)
    {
        PELOGW(limLog(pMac, LOGW, FL("Skip STA deletion (invalid STA)"));)
        return;
    }
    /**
     * MAC based Authentication was used. Trigger
     * Deauthentication frame to peer since it will
     * take care of disassociation as well.
     */

    pSmeDeauthReq = vos_mem_malloc(sizeof(tSirSmeDeauthReq));
    if (NULL == pSmeDeauthReq)
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for eWNI_SME_DEAUTH_REQ "));
        return;
    }

    pBuf = (tANI_U8 *) &pSmeDeauthReq->messageType;

    //messageType
    limCopyU16((tANI_U8*)pBuf, eWNI_SME_DISASSOC_REQ);
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //length
    pLen = pBuf;
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //sessionId
    *pBuf = psessionEntry->smeSessionId;
    pBuf++;
    msgLength++;

    //transactionId
    limCopyU16((tANI_U8*)pBuf, psessionEntry->transactionId);
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //bssId
    vos_mem_copy(pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    msgLength += sizeof(tSirMacAddr);

    //peerMacAddr
    vos_mem_copy(pBuf, pStaDs->staAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    msgLength += sizeof(tSirMacAddr);

    //reasonCode 
    limCopyU16((tANI_U8*)pBuf, (tANI_U16)eLIM_LINK_MONITORING_DISASSOC);
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //Do not send disassoc OTA
    //pBuf[0] = 1 means do not send the disassoc frame over the air
    //pBuf[0] = 0 means send the disassoc frame over the air
    pBuf[0]= 0;
    pBuf += sizeof(tANI_U8);
    msgLength += sizeof(tANI_U8);


  
    //Fill in length
    limCopyU16((tANI_U8*)pLen , msgLength);

    limPostSmeMessage(pMac, eWNI_SME_DISASSOC_REQ, (tANI_U32 *) pSmeDeauthReq);
    vos_mem_free(pSmeDeauthReq);

} /*** end limTriggerSTAdeletion() ***/



/**
 * limTearDownLinkWithAp()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limTearDownLinkWithAp(tpAniSirGlobal pMac, tANI_U8 sessionId, tSirMacReasonCodes reasonCode)
{
    tpDphHashNode pStaDs = NULL;

    //tear down the following sessionEntry
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, sessionId))== NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }
    /**
     * Heart beat failed for upto threshold value
     * and AP did not respond for Probe request.
     * Trigger link tear down.
     */

    pMac->pmm.inMissedBeaconScenario = FALSE;
    limLog(pMac, LOGW,
       FL("No ProbeRsp from AP after HB failure. Tearing down link"));

    // Deactivate heartbeat timer
    limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);

    // Announce loss of link to Roaming algorithm
    // and cleanup by sending SME_DISASSOC_REQ to SME

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

    
    if (pStaDs != NULL)
    {
        tLimMlmDeauthInd  mlmDeauthInd;

#ifdef FEATURE_WLAN_TDLS
        /* Delete all TDLS peers connected before leaving BSS*/
        limDeleteTDLSPeers(pMac, psessionEntry);
#endif

        pStaDs->mlmStaContext.disassocReason = reasonCode;
        pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DEAUTH;

        /// Issue Deauth Indication to SME.
        vos_mem_copy((tANI_U8 *) &mlmDeauthInd.peerMacAddr,
                      pStaDs->staAddr,
                      sizeof(tSirMacAddr));
        /* if sendDeauthBeforeCon is enabled and reasoncode is Beacon Missed
         * Store the MAC of AP in the flip flop buffer. This MAC will be
         * used to send Deauth before connection, if we connect to same AP
         * after HB failure.
         */
        if(pMac->roam.configParam.sendDeauthBeforeCon &&
            eSIR_BEACON_MISSED == reasonCode)
        {
           int apCount = pMac->lim.gLimHeartBeatApMacIndex;

           if(pMac->lim.gLimHeartBeatApMacIndex)
              pMac->lim.gLimHeartBeatApMacIndex = 0;
           else
              pMac->lim.gLimHeartBeatApMacIndex = 1;

           limLog(pMac, LOGE, FL("HB Failure on MAC "
                 MAC_ADDRESS_STR" Store it on Index %d"),
                   MAC_ADDR_ARRAY(pStaDs->staAddr),apCount);

           sirCopyMacAddr(pMac->lim.gLimHeartBeatApMac[apCount],pStaDs->staAddr);
        }
        mlmDeauthInd.reasonCode    = (tANI_U8) pStaDs->mlmStaContext.disassocReason;
        mlmDeauthInd.deauthTrigger =  pStaDs->mlmStaContext.cleanupTrigger;

        limPostSmeMessage(pMac, LIM_MLM_DEAUTH_IND, (tANI_U32 *) &mlmDeauthInd);

        limSendSmeDeauthInd(pMac, pStaDs, psessionEntry);
    }    
} /*** limTearDownLinkWithAp() ***/




/**
 * limHandleHeartBeatFailure()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void limHandleHeartBeatFailure(tpAniSirGlobal pMac,tpPESession psessionEntry)
{

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    vos_log_beacon_update_pkt_type *log_ptr = NULL;
#endif //FEATURE_WLAN_DIAG_SUPPORT 

    /* If gLimHeartBeatTimer fires between the interval of sending WDA_ENTER_BMPS_REQUEST 
     * to the HAL and receiving WDA_ENTER_BMPS_RSP from the HAL, then LIM (PE) tries to Process the
     * SIR_LIM_HEAR_BEAT_TIMEOUT message but The PE state is ePMM_STATE_BMPS_SLEEP so PE dont
     * want to handle heartbeat timeout in the BMPS, because Firmware handles it in BMPS.
     * So just return from heartbeatfailure handler
     */
    if(!IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE && (!limIsSystemInActiveState(pMac)))
       return;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_beacon_update_pkt_type, LOG_WLAN_BEACON_UPDATE_C);
    if(log_ptr)
        log_ptr->bcn_rx_cnt = psessionEntry->LimRxedBeaconCntDuringHB;
    WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* Ensure HB Status for the session has been reseted */
    psessionEntry->LimHBFailureStatus = eANI_BOOLEAN_FALSE;

    if (((psessionEntry->limSystemRole == eLIM_STA_ROLE)||
         (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
         (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)&&
         (psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
         (psessionEntry->limSmeState != eLIM_SME_WT_DEAUTH_STATE))
    {
        if (!pMac->sys.gSysEnableLinkMonitorMode)
            return;

        /**
         * Beacon frame not received within heartbeat timeout.
         */
        PELOGW(limLog(pMac, LOGW, FL("Heartbeat Failure"));)
        pMac->lim.gLimHBfailureCntInLinkEstState++;

        /**
         * Check if connected on the DFS channel, if not connected on
         * DFS channel then only send the probe request otherwise tear down the link
         */
        if(!limIsconnectedOnDFSChannel(psessionEntry->currentOperChannel))
        {
            /*** Detected continuous Beacon Misses ***/
             psessionEntry->LimHBFailureStatus= eANI_BOOLEAN_TRUE;
             /*Reset the HB packet count before sending probe*/
             limResetHBPktCount(psessionEntry);
            /**
             * Send Probe Request frame to AP to see if
             * it is still around. Wait until certain
             * timeout for Probe Response from AP.
             */
            PELOGW(limLog(pMac, LOGW, FL("Heart Beat missed from AP. Sending Probe Req"));)
            /* for searching AP, we don't include any additional IE */
            limSendProbeReqMgmtFrame(pMac, &psessionEntry->ssId, psessionEntry->bssId,
                                      psessionEntry->currentOperChannel,psessionEntry->selfMacAddr,
                                      psessionEntry->dot11mode, 0, NULL);
        }
        else
        {
            PELOGW(limLog(pMac, LOGW,
              FL("Heart Beat missed from AP on DFS chanel moving to passive"));)
            if (psessionEntry->currentOperChannel < SIR_MAX_24G_5G_CHANNEL_RANGE){
               limCovertChannelScanType(pMac, psessionEntry->currentOperChannel, false);
               pMac->lim.dfschannelList.timeStamp[psessionEntry->currentOperChannel] = 0;
            }
            /* Connected on DFS channel so should not send the probe request
            * tear down the link directly */
            limTearDownLinkWithAp(pMac, psessionEntry->peSessionId, eSIR_BEACON_MISSED);
        }
    }
    else
    {
        /**
             * Heartbeat timer may have timed out
            * while we're doing background scanning/learning
            * or in states other than link-established state.
            * Log error.
            */
        PELOG1(limLog(pMac, LOG1, FL("received heartbeat timeout in state %d"),
               psessionEntry->limMlmState);)
        limPrintMlmState(pMac, LOG1, psessionEntry->limMlmState);
        pMac->lim.gLimHBfailureCntInOtherStates++;
        limReactivateHeartBeatTimer(pMac, psessionEntry);
    }
} /*** limHandleHeartBeatFailure() ***/
