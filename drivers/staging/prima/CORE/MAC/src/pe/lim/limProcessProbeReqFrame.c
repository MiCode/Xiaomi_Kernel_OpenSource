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
 * This file limProcessProbeReqFrame.cc contains the code
 * for processing Probe Request Frame.
 * Author:        Chandra Modumudi
 * Date:          02/28/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniCfg.h"
#include "aniGlobal.h"
#include "cfgApi.h"

#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSerDesUtils.h"
#include "parserApi.h"
#include "limSession.h"

#ifdef WLAN_FEATURE_P2P_INTERNAL
void limSendP2PProbeResponse(tpAniSirGlobal pMac, tANI_U8 *pBd, 
                      tpPESession psessionEntry);
#endif
void

limSendSmeProbeReqInd(tpAniSirGlobal pMac,
                      tSirMacAddr peerMacAddr,
                      tANI_U8 *pProbeReqIE,
                      tANI_U32 ProbeReqIELen,
                      tpPESession psessionEntry);
                      
/**
 * limGetWPSPBCSessions
 *
 *FUNCTION:
 * This function is called to query the WPS PBC overlap
 *
 *LOGIC:
 * This function check WPS PBC probe request link list for PBC overlap 
 *
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  addr   A pointer to probe request source MAC addresss
 * @param  uuid_e A pointer to UUIDE element of WPS IE in WPS PBC probe request  
 * @param  psessionEntry   A pointer to station PE session
 *
 * @return None
 */

void limGetWPSPBCSessions(tpAniSirGlobal pMac, tANI_U8 *addr, 
                          tANI_U8 *uuid_e, eWPSPBCOverlap *overlap, 
                          tpPESession psessionEntry)
{
    int count = 0;
    tSirWPSPBCSession *pbc;
    tANI_TIMESTAMP curTime;

    curTime = (tANI_TIMESTAMP)(palGetTickCount(pMac->hHdd) / PAL_TICKS_PER_SECOND);

    vos_mem_set((tANI_U8 *)addr, sizeof(tSirMacAddr), 0);
    vos_mem_set((tANI_U8 *)uuid_e, SIR_WPS_UUID_LEN, 0);

    for (pbc = psessionEntry->pAPWPSPBCSession; pbc; pbc = pbc->next) {

        if (curTime > pbc->timestamp + SIR_WPS_PBC_WALK_TIME)
            break;

        count++;
        if(count > 1)
            break;
            
        vos_mem_copy((tANI_U8 *)addr, (tANI_U8 *)pbc->addr, sizeof(tSirMacAddr));
        vos_mem_copy((tANI_U8 *)uuid_e, (tANI_U8 *)pbc->uuid_e, SIR_WPS_UUID_LEN);
        }

    if (count > 1)
    {
        *overlap = eSAP_WPSPBC_OVERLAP_IN120S;    // Overlap  
    }
    else if(count == 0)
    {
        *overlap = eSAP_WPSPBC_NO_WPSPBC_PROBE_REQ_IN120S;    // no WPS probe request in 120 second    
    } else
    {
         *overlap = eSAP_WPSPBC_ONE_WPSPBC_PROBE_REQ_IN120S;   // One WPS probe request in 120 second
    }

    PELOGE(limLog(pMac, LOGE, FL("overlap = %d"), *overlap);)
    PELOGE(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOGE, addr, sizeof(tSirMacAddr));)
    PELOGE(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOGE, uuid_e, SIR_WPS_UUID_LEN);)

    return;
}

/**
 * limRemoveTimeoutPBCsessions
 *
 *FUNCTION:
 * This function is called to remove the WPS PBC probe request entires from specific entry to end.
 *
 *LOGIC:
 *
 * 
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  pbc    The beginning entry in WPS PBC probe request link list
 *
 * @return None
 */
static void limRemoveTimeoutPBCsessions(tpAniSirGlobal pMac, tSirWPSPBCSession *pbc)
{
     tSirWPSPBCSession *prev;

        while (pbc) {
                prev = pbc;
                pbc = pbc->next;

        PELOG4(limLog(pMac, LOG4, FL("WPS PBC sessions remove"));)
        PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, prev->addr, sizeof(tSirMacAddr));)
        PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, prev->uuid_e, SIR_WPS_UUID_LEN);)
        
        vos_mem_free(prev);
       }
}

void limRemovePBCSessions(tpAniSirGlobal pMac, tSirMacAddr pRemoveMac,tpPESession psessionEntry)
{
    tSirWPSPBCSession *pbc, *prev = NULL;
    prev = pbc = psessionEntry->pAPWPSPBCSession;

    while (pbc) {
        if (vos_mem_compare((tANI_U8 *)pbc->addr,
                            (tANI_U8 *)pRemoveMac, sizeof(tSirMacAddr))) {
          prev->next = pbc->next;
          if (pbc == psessionEntry->pAPWPSPBCSession)
            psessionEntry->pAPWPSPBCSession = pbc->next;
            vos_mem_free(pbc);
            return;
        }
        prev = pbc;
        pbc = pbc->next;
    }

}

/**
 * limUpdatePBCSessionEntry
 *
 *FUNCTION:
 * This function is called when probe request with WPS PBC IE is received
 *
 *LOGIC:
 * This function add the WPS PBC probe request in the WPS PBC probe request link list 
 * The link list is in decreased time order of probe request that is received.
 * The entry that is more than 120 second is removed.
 * 
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  addr   A pointer to probe request source MAC addresss
 * @param  uuid_e A pointer to UUIDE element of WPS IE 
 * @param  psessionEntry   A pointer to station PE session
 *
 * @return None
 */

static void limUpdatePBCSessionEntry(tpAniSirGlobal pMac,
                                     tANI_U8 *addr, tANI_U8 *uuid_e, 
                                     tpPESession psessionEntry)
{
    tSirWPSPBCSession *pbc, *prev = NULL;

    tANI_TIMESTAMP curTime;

    curTime = (tANI_TIMESTAMP)(palGetTickCount(pMac->hHdd) / PAL_TICKS_PER_SECOND);
            
    PELOG4(limLog(pMac, LOG4, FL("Receive WPS probe reques curTime=%d"), curTime);)
    PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, addr, sizeof(tSirMacAddr));)
    PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, uuid_e, SIR_WPS_UUID_LEN);)

    pbc = psessionEntry->pAPWPSPBCSession;

    while (pbc) {
        if (vos_mem_compare((tANI_U8 *)pbc->addr, (tANI_U8 *)addr, sizeof(tSirMacAddr)) &&
            vos_mem_compare((tANI_U8 *)pbc->uuid_e, (tANI_U8 *)uuid_e, SIR_WPS_UUID_LEN)) {
            if (prev)
                prev->next = pbc->next;
            else
                psessionEntry->pAPWPSPBCSession = pbc->next;
            break;
        }
        prev = pbc;
        pbc = pbc->next;
    }

    if (!pbc) {
        pbc = vos_mem_malloc(sizeof(tSirWPSPBCSession));
        if ( NULL == pbc )
        {
            PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!"));)
            return;
        }
        vos_mem_copy((tANI_U8 *)pbc->addr, (tANI_U8 *)addr, sizeof(tSirMacAddr));
    
        if (uuid_e)
            vos_mem_copy((tANI_U8 *)pbc->uuid_e, (tANI_U8 *)uuid_e, SIR_WPS_UUID_LEN);
    }
    
    pbc->next = psessionEntry->pAPWPSPBCSession;
    psessionEntry->pAPWPSPBCSession = pbc;
    pbc->timestamp = curTime;
    
    /* remove entries that have timed out */
    prev = pbc;
    pbc = pbc->next;

    while (pbc) {
        if (curTime > pbc->timestamp + SIR_WPS_PBC_WALK_TIME) {
            prev->next = NULL;
            limRemoveTimeoutPBCsessions(pMac, pbc);
           break;
        }
        prev = pbc;
        pbc = pbc->next;
    }
}
#if 0
/**
 * limWPSPBCTimeout
 *
 *FUNCTION:
 * This function is called when WPS PBC enrtries clean up timer is expired
 *
 *LOGIC:
 * This function remove all the entryies that more than 120 second old
 *
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  psessionEntry   A pointer to station PE session
 *
 * @return None
 */

void limWPSPBCTimeout(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tANI_TIMESTAMP curTime;
    tSirWPSPBCSession *pbc, *prev = NULL;

    curTime = (tANI_TIMESTAMP)(palGetTickCount(pMac->hHdd) / PAL_TICKS_PER_SECOND);
    
    PELOG3(limLog(pMac, LOG3, FL("WPS PBC cleanup timeout curTime=%d"), curTime);)

    prev = psessionEntry->pAPWPSPBCSession; 
    if(prev)
        pbc = prev->next;
    else
        return;
            
    while (pbc) {
        if (curTime > pbc->timestamp + SIR_WPS_PBC_WALK_TIME) {
            prev->next = NULL;
            limRemoveTimeoutPBCsessions(pMac, pbc);
            break;
        }
        prev = pbc;
        pbc = pbc->next;
    }

    if(prev)
    {
         if (curTime > prev->timestamp + SIR_WPS_PBC_WALK_TIME) {
            psessionEntry->pAPWPSPBCSession = NULL;  
            limRemoveTimeoutPBCsessions(pMac, prev);
         }
    }

}
#endif
/**
 * limWPSPBCClose
 *
 *FUNCTION:
 * This function is called when BSS is closed
 *
 *LOGIC:
 * This function remove all the WPS PBC entries
 *
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  psessionEntry   A pointer to station PE session
 *
 * @return None
 */

void limWPSPBCClose(tpAniSirGlobal pMac, tpPESession psessionEntry)
{

    limRemoveTimeoutPBCsessions(pMac, psessionEntry->pAPWPSPBCSession);

}

/**
 * limCheck11bRates
 *
 *FUNCTION:
 * This function is called by limProcessProbeReqFrame() upon
 * Probe Request frame reception.
 *
 *LOGIC:
 * This function check 11b rates in supportedRates and extendedRates rates
 * 
 *NOTE:
 *
 * @param  rate   
 *
 * @return BOOLEAN
 */

tANI_BOOLEAN limCheck11bRates(tANI_U8 rate)
{
    if ( ( 0x02 == (rate))
      || ( 0x04 == (rate))
      || ( 0x0b == (rate))
      || ( 0x16 == (rate))
       )
       {
           return TRUE;
       } 
    return FALSE;
}
  
/**
 * limProcessProbeReqFrame
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Probe Request frame reception.
 *
 *LOGIC:
 * This function processes received Probe Request frame and responds
 * with Probe Response.
 * Only AP or STA in IBSS mode that sent last Beacon will respond to
 * Probe Request.
 *
 *ASSUMPTIONS:
 * 1. AP or STA in IBSS mode that sent last Beacon will always respond
 *    to Probe Request received with broadcast SSID.
 *
 *NOTE:
 * 1. Dunno what to do with Rates received in Probe Request frame
 * 2. Frames with out-of-order fields/IEs are dropped.
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  *pRxPacketInfo   A pointer to Buffer descriptor + associated PDUs
 *
 * @return None
 */

void
limProcessProbeReqFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tANI_U8             *pBody;
    tpSirMacMgmtHdr     pHdr;
    tANI_U32            frameLen;
    tSirProbeReq        probeReq;
    tAniSSID            ssId;
    tSirMsgQ            msgQ;
    tSirSmeProbeReq     *pSirSmeProbeReq;
    tANI_U32            wpsApEnable=0, tmp;

    do{
        // Don't send probe responses if disabled
        if (pMac->lim.gLimProbeRespDisableFlag)
            break;

        // Don't send probe response if P2P go is scanning till scan come to idle state. 
        if((psessionEntry->pePersona == VOS_P2P_GO_MODE) && ((pMac->lim.gpLimRemainOnChanReq )
                                  || (pMac->lim.gLimHalScanState != eLIM_HAL_IDLE_SCAN_STATE)))
        {
           limLog(pMac, LOG3,
              FL("While GO is scanning, don't send probe response"
                 " on diff channel"));
           break;
        }

       pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);

        if ( (psessionEntry->limSystemRole == eLIM_AP_ROLE) ||
             (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)||
             (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)|| 
             ( (psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) &&
             (WDA_GET_RX_BEACON_SENT(pRxPacketInfo)) ) )
        {
           frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

            PELOG3(limLog(pMac, LOG3, FL("Received Probe Request %d bytes from "), frameLen);
            limPrintMacAddr(pMac, pHdr->sa, LOG3);)

            // Get pointer to Probe Request frame body
           pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

            // Parse Probe Request frame
            if (sirConvertProbeReqFrame2Struct(pMac, pBody, frameLen, &probeReq)==eSIR_FAILURE)
            {
                PELOGW(limLog(pMac, LOGE, FL("Parse error ProbeRequest,"
                " length=%d, SA is:" MAC_ADDRESS_STR),
                 frameLen,MAC_ADDR_ARRAY(pHdr->sa));)
                pMac->sys.probeError++;
                break;
            }
            else
            {
                if (psessionEntry->pePersona == VOS_P2P_GO_MODE)
                {
                    tANI_U8 i = 0, rate_11b = 0, other_rates = 0;
                    // Check 11b rates in supported rates
                    for ( i = 0 ; i < probeReq.supportedRates.numRates;
                                                                  i++ )
                    {
                        if (limCheck11bRates(probeReq.supportedRates.rate[i] & 0x7f))
                        {
                            rate_11b++; 
                        }
                        else
                        {
                            other_rates++;
                        }
                    }

                    // Check 11b rates in extended rates
                    for ( i = 0 ; i < probeReq.extendedRates.numRates; i++ )
                    {
                        if (limCheck11bRates(probeReq.extendedRates.rate[i] & 0x7f))
                        {
                            rate_11b++; 
                        }
                        else
                        {
                            other_rates++;
                        }
                    }

                    if ( (rate_11b > 0) && (other_rates == 0) )
                    {
                        PELOG3(limLog(pMac, LOG3, 
                               FL("Received a probe request frame with only 11b rates, SA is: "));
                               limPrintMacAddr(pMac, pHdr->sa, LOG3);)
                        return;
                    }
                }
                if ((psessionEntry->limSystemRole == eLIM_AP_ROLE))
                {
                  
                    if ( (psessionEntry->APWPSIEs.SirWPSProbeRspIE.FieldPresent &
                                               SIR_WPS_PROBRSP_VER_PRESENT) &&
                         (probeReq.wscIePresent ==  1) &&
                         (probeReq.probeReqWscIeInfo.DevicePasswordID.id == 
                                                  WSC_PASSWD_ID_PUSH_BUTTON) &&
                         (probeReq.probeReqWscIeInfo.UUID_E.present == 1))
                    {
                        if(psessionEntry->fwdWPSPBCProbeReq)
                        {
                            PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4,
                                              pHdr->sa, sizeof(tSirMacAddr));)
                            PELOG4(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, pBody, frameLen);)
                            limSendSmeProbeReqInd(pMac, pHdr->sa, pBody, frameLen, psessionEntry);
                        } 
                        else
                        {
                            limUpdatePBCSessionEntry(pMac,
                                pHdr->sa, probeReq.probeReqWscIeInfo.UUID_E.uuid, psessionEntry);
                        }
                    }
                }
                else
                {
                    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_ENABLE, &tmp) != eSIR_SUCCESS)
                        limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_ENABLE );

                    wpsApEnable = tmp & WNI_CFG_WPS_ENABLE_AP;
                    if ((wpsApEnable) &&
                        (probeReq.wscIePresent ==  1) &&
                        (probeReq.probeReqWscIeInfo.DevicePasswordID.id == WSC_PASSWD_ID_PUSH_BUTTON))
                    {
                        // send the probe req to WSM when it is from a PBC station 
                        pSirSmeProbeReq = vos_mem_malloc(sizeof(tSirSmeProbeReq));
                        if ( NULL == pSirSmeProbeReq )
                        {
                            // Log error
                            limLog(pMac, LOGP,
                               FL("call to AllocateMemory failed for eWNI_SME_PROBE_REQ"));
                            return;
                        }
                        msgQ.type = eWNI_SME_PROBE_REQ;
                        msgQ.bodyval = 0;
                        msgQ.bodyptr = pSirSmeProbeReq;

                        pSirSmeProbeReq->messageType = eWNI_SME_PROBE_REQ;
                        pSirSmeProbeReq->length = sizeof(tSirSmeProbeReq);
                        pSirSmeProbeReq->sessionId = psessionEntry->smeSessionId;
                        vos_mem_copy(pSirSmeProbeReq->peerMacAddr, pHdr->sa, sizeof(tSirMacAddr));
                        pSirSmeProbeReq->devicePasswdId = probeReq.probeReqWscIeInfo.DevicePasswordID.id;
                        MTRACE(macTrace(pMac, TRACE_CODE_TX_SME_MSG,
                            psessionEntry->peSessionId, msgQ.type));
                       if (limSysProcessMmhMsgApi(pMac, &msgQ,  ePROT) != eSIR_SUCCESS){
                            PELOG3(limLog(pMac, LOG3, FL("couldnt send the probe req to wsm "));)
                        }
                    }
                }
            }

            ssId.length = psessionEntry->ssId.length;
             /* Copy the SSID from sessio entry to local variable */
            vos_mem_copy(ssId.ssId,
                         psessionEntry->ssId.ssId,
                         psessionEntry->ssId.length);

            // Compare received SSID with current SSID. If they
            // match, reply with Probe Response.
            if (probeReq.ssId.length)
            {
                if (!ssId.length)
                    goto multipleSSIDcheck;

                if (vos_mem_compare((tANI_U8 *) &ssId,
                              (tANI_U8 *) &(probeReq.ssId), (tANI_U8) (ssId.length + 1)) )
                {
                    limSendProbeRspMgmtFrame(pMac, pHdr->sa, &ssId, DPH_USE_MGMT_STAID,
                                             DPH_NON_KEEPALIVE_FRAME, psessionEntry,
                                             probeReq.p2pIePresent);
                    break;
                }
                else if (psessionEntry->pePersona == VOS_P2P_GO_MODE)
                {
                    tANI_U8   direct_ssid[7] = "DIRECT-";
                    tANI_U8   direct_ssid_len = 7;
                    if (vos_mem_compare((tANI_U8 *) &direct_ssid,
                              (tANI_U8 *) &(probeReq.ssId.ssId), (tANI_U8) (direct_ssid_len)) )
                    {
                        limSendProbeRspMgmtFrame(pMac, pHdr->sa, &ssId, DPH_USE_MGMT_STAID,
                                             DPH_NON_KEEPALIVE_FRAME, psessionEntry,
                                             probeReq.p2pIePresent);
                        break;
                    }
                }
                else
                {
                   PELOG3(limLog(pMac, LOG3,
                       FL("Ignoring ProbeReq frame with unmatched SSID received from "));
                    limPrintMacAddr(pMac, pHdr->sa, LOG3);)
                    pMac->sys.probeBadSsid++;
                }
            }
            else
            {
                {
                    if ((VOS_P2P_GO_MODE == psessionEntry->pePersona) &&
                                                        pMac->miracastVendorConfig)
                        return;

                    // Broadcast SSID in the Probe Request.
                    // Reply with SSID we're configured with.
                    //Turn off the SSID length to 0 if hidden SSID feature is present
                    if(psessionEntry->ssidHidden)
                      /*We are returning from here as probe request contains the broadcast SSID.
                        So no need to send the probe resp*/
                        //ssId.length = 0;
                           return;
                    limSendProbeRspMgmtFrame(pMac, pHdr->sa, &ssId, DPH_USE_MGMT_STAID,
                                             DPH_NON_KEEPALIVE_FRAME, psessionEntry,
                                             probeReq.p2pIePresent);
                }
                break;
            }
multipleSSIDcheck:
            {
               PELOG3(limLog(pMac, LOG3,
                   FL("Ignoring ProbeReq frame with unmatched SSID received from "));
                limPrintMacAddr(pMac, pHdr->sa, LOG3);)
                pMac->sys.probeBadSsid++;
            }
        }
        else
        {
            // Ignore received Probe Request frame
            PELOG3(limLog(pMac, LOG3, FL("Ignoring Probe Request frame received from "));
            limPrintMacAddr(pMac, pHdr->sa, LOG3);)
            pMac->sys.probeIgnore++;
            break;
        }
    }while(0);

    return;
} /*** end limProcessProbeReqFrame() ***/

/**
 * limIndicateProbeReqToHDD
 *
 *FUNCTION:
 * This function is called by limProcessProbeReqFrame_multiple_BSS() upon
 * Probe Request frame reception.
 *
 *LOGIC:
 * This function processes received Probe Request frame and Pass 
 * Probe Request Frame to HDD.
 *
 * @param  pMac              Pointer to Global MAC structure
 * @param  *pBd              A pointer to Buffer descriptor + associated PDUs
 * @param  psessionEntry     A pointer to PE session
 *
 * @return None
 */

static void
limIndicateProbeReqToHDD(tpAniSirGlobal pMac, tANI_U8 *pBd,
                         tpPESession psessionEntry)
{
    limLog( pMac, LOG1, FL("Received a probe request frame"));

    //send the probe req to SME.
    limSendSmeMgmtFrameInd( pMac, psessionEntry->smeSessionId, pBd,
                            psessionEntry, 0);
#ifdef WLAN_FEATURE_P2P_INTERNAL
    limSendP2PProbeResponse(pMac, pBd, psessionEntry);
#endif
} /*** end limIndicateProbeReqToHDD() ***/

/**
 * limProcessProbeReqFrame_multiple_BSS
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Probe Request frame reception.
 *
 *LOGIC:
 * This function call limIndicateProbeReqToHDD function to indicate 
 * Probe Request frame to HDD. It also call limProcessProbeReqFrame 
 * function which process received Probe Request frame and responds
 * with Probe Response.
 *
 * @param  pMac              Pointer to Global MAC structure
 * @param  *pBd              A pointer to Buffer descriptor + associated PDUs
 * @param  psessionEntry     A pointer to PE session
 *
 * @return None
 */

void
limProcessProbeReqFrame_multiple_BSS(tpAniSirGlobal pMac, tANI_U8 *pBd,  tpPESession psessionEntry)
{
    tANI_U8 i;

    if (psessionEntry != NULL)
    {
        if ((eLIM_AP_ROLE == psessionEntry->limSystemRole)
#ifdef WLAN_FEATURE_P2P_INTERNAL
         || (psessionEntry->limSystemRole == eLIM_P2P_DEVICE_ROLE)
#endif
          )
        {
            limIndicateProbeReqToHDD(pMac, pBd, psessionEntry);
        }
        limProcessProbeReqFrame(pMac,pBd,psessionEntry);
        return;
    }

    for(i =0; i < pMac->lim.maxBssId;i++)
    {
        psessionEntry = peFindSessionBySessionId(pMac,i);
        if ( (psessionEntry != NULL) )
        {
            if ((eLIM_AP_ROLE == psessionEntry->limSystemRole)
#ifdef WLAN_FEATURE_P2P_INTERNAL
             || (psessionEntry->limSystemRole == eLIM_P2P_DEVICE_ROLE)
#endif
              )
            {
                limIndicateProbeReqToHDD(pMac, pBd, psessionEntry);
            }
            if ( (eLIM_AP_ROLE == psessionEntry->limSystemRole) ||
                (eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole) ||
                (eLIM_BT_AMP_AP_ROLE == psessionEntry->limSystemRole) ||
                (eLIM_BT_AMP_STA_ROLE == psessionEntry->limSystemRole)
               )
            {
                limProcessProbeReqFrame(pMac,pBd,psessionEntry);
            }
        }
    }

} /*** end limProcessProbeReqFrame_multiple_BSS() ***/

/**
 * limSendSmeProbeReqInd()
 *
 *FUNCTION:
 * This function is to send
 *  eWNI_SME_WPS_PBC_PROBE_REQ_IND message to host
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * This function is used for sending  eWNI_SME_WPS_PBC_PROBE_REQ_IND
 * to host.
 *
 * @param peerMacAddr       Indicates the peer MAC addr that the probe request
 *                          is generated.
 * @param pProbeReqIE       pointer to RAW probe request IE
 * @param ProbeReqIELen     The length of probe request IE.
 * @param psessionEntry     A pointer to PE session
 *
 * @return None
 */
void
limSendSmeProbeReqInd(tpAniSirGlobal pMac,
                      tSirMacAddr peerMacAddr,
                      tANI_U8 *pProbeReqIE,
                      tANI_U32 ProbeReqIELen, 
                      tpPESession psessionEntry)
{
    tSirSmeProbeReqInd     *pSirSmeProbeReqInd;
    tSirMsgQ                msgQ;   

    pSirSmeProbeReqInd = vos_mem_malloc(sizeof(tSirSmeProbeReqInd));
    if ( NULL == pSirSmeProbeReqInd )
    {
        // Log error
        limLog(pMac, LOGP,
            FL("call to AllocateMemory failed for eWNI_SME_PROBE_REQ"));
            return;
    }
    
    msgQ.type =  eWNI_SME_WPS_PBC_PROBE_REQ_IND;
    msgQ.bodyval = 0;
    msgQ.bodyptr = pSirSmeProbeReqInd;
    
    pSirSmeProbeReqInd->messageType =  eWNI_SME_WPS_PBC_PROBE_REQ_IND;
    pSirSmeProbeReqInd->length = sizeof(tSirSmeProbeReq);
    pSirSmeProbeReqInd->sessionId = psessionEntry->smeSessionId;

    vos_mem_copy(pSirSmeProbeReqInd->bssId, psessionEntry->bssId, sizeof(tSirMacAddr));
    vos_mem_copy(pSirSmeProbeReqInd->WPSPBCProbeReq.peerMacAddr, peerMacAddr, sizeof(tSirMacAddr));

    MTRACE(macTrace(pMac, TRACE_CODE_TX_SME_MSG, psessionEntry->peSessionId,
                                                               msgQ.type));
    pSirSmeProbeReqInd->WPSPBCProbeReq.probeReqIELen = (tANI_U16)ProbeReqIELen;
    vos_mem_copy(pSirSmeProbeReqInd->WPSPBCProbeReq.probeReqIE, pProbeReqIE, ProbeReqIELen);
    
    if (limSysProcessMmhMsgApi(pMac, &msgQ,  ePROT) != eSIR_SUCCESS){
                            PELOGE(limLog(pMac, LOGE, FL("couldnt send the probe req to hdd"));)
    } 
        
} /*** end limSendSmeProbeReqInd() ***/
#ifdef WLAN_FEATURE_P2P_INTERNAL
void limSendP2PProbeResponse(tpAniSirGlobal pMac, tANI_U8 *pBd, 
                      tpPESession psessionEntry)
{
    tAniSSID            ssId = { P2P_WILDCARD_SSID_LEN, P2P_WILDCARD_SSID };
    tANI_U8             *pBody;
    tpSirMacMgmtHdr     pHdr;
    tANI_U32            frameLen;
    tSirProbeReq        probeReq;

    pHdr =  WDA_GET_RX_MAC_HEADER(pBd);
    // Get pointer to Probe Request frame body
    pBody =  WDA_GET_RX_MPDU_DATA(pBd);   

    if( (pBody[0] == 0) && (pBody[1] == ssId.length) &&
      (vos_mem_compare(ssId.ssId, pBody + 2,
                       ssId.length)))
    {
        // Parse Probe Request frame
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pBd);
        if (eSIR_FAILURE == sirConvertProbeReqFrame2Struct(pMac, pBody, frameLen, &probeReq))
        {
            PELOGW(limLog(pMac, LOGW, FL("Parse error ProbeRequest, length=%d, SA is:"), frameLen);)
            limPrintMacAddr(pMac, pHdr->sa, LOGW);
            pMac->sys.probeError++;
            return;
        }

        if (psessionEntry->pePersona == VOS_P2P_GO_MODE) 
        {
            ssId.length = psessionEntry->ssId.length;
            vos_mem_copy(ssId.ssId, psessionEntry->ssId.ssId,psessionEntry->ssId.length);
            limSendProbeRspMgmtFrame(pMac, pHdr->sa, &ssId, DPH_USE_MGMT_STAID, DPH_NON_KEEPALIVE_FRAME, 
                                     psessionEntry, probeReq.p2pIePresent );
        } 
        else 
        {
            limSendProbeRspMgmtFrame(pMac, pHdr->sa, &ssId, DPH_USE_MGMT_STAID, DPH_NON_KEEPALIVE_FRAME,
                                     psessionEntry, probeReq.p2pIePresent );
        }
    }
}
#endif //#ifdef WLAN_FEATURE_P2P_INTERNAL
