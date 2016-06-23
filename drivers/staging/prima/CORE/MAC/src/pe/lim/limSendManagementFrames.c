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




/**
 * \file limSendManagementFrames.c
 *
 * \brief Code for preparing and sending 802.11 Management frames
 *

 *
 */

#include "sirApi.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"
#include "cfgApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "limPropExtsUtils.h"
#include "dot11f.h"
#include "limStaHashApi.h"
#include "schApi.h"
#include "limSendMessages.h"
#include "limAssocUtils.h"
#include "limFT.h"
#ifdef WLAN_FEATURE_11W
#include "wniCfgAp.h"
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif

#include "wlan_qct_wda.h"


////////////////////////////////////////////////////////////////////////

tSirRetStatus limStripOffExtCapIE(tpAniSirGlobal pMac,
                                  tANI_U8 *addIE,
                                  tANI_U16 *addnIELen,
                                  tANI_U8 *pExtractedExtCapIEBuf )
{
    tANI_U8* tempbuf = NULL;
    tANI_U16 tempLen = 0;
    int left = *addnIELen;
    tANI_U8 *ptr = addIE;
    tANI_U8 elem_id, elem_len;

    if (NULL == addIE)
    {
        PELOGE(limLog(pMac, LOG1, FL("NULL addIE pointer"));)
        return eSIR_IGNORE_IE ;
    }

    tempbuf = vos_mem_malloc(left);
    if ( NULL == tempbuf )
    {
        PELOGE(limLog(pMac, LOGE,
             FL("Unable to allocate memory to store addn IE"));)
        return eSIR_MEM_ALLOC_FAILED;
    }

    while(left >= 2)
    {
        elem_id  = ptr[0];
        elem_len = ptr[1];
        left -= 2;
        if (elem_len > left)
        {
            limLog( pMac, LOGE,
               FL("Invalid IEs eid = %d elem_len=%d left=%d"),
                                               elem_id,elem_len,left);
            vos_mem_free(tempbuf);
            return eSIR_FAILURE;
        }
        if ( !(DOT11F_EID_EXTCAP == elem_id) )
        {
            vos_mem_copy (tempbuf + tempLen, &ptr[0], elem_len + 2);
            tempLen += (elem_len + 2);
        }
        else
        { /*Est Cap present size is 8 + 2 byte at present*/
            if ( NULL != pExtractedExtCapIEBuf )
            {
                vos_mem_set(pExtractedExtCapIEBuf,
                    DOT11F_IE_EXTCAP_MAX_LEN + 2, 0);
                if (elem_len <= DOT11F_IE_EXTCAP_MAX_LEN )
                {
                    vos_mem_copy (pExtractedExtCapIEBuf, &ptr[0],
                        elem_len + 2);
                }
            }
        }
        left -= elem_len;
        ptr += (elem_len + 2);
    }
    vos_mem_copy (addIE, tempbuf, tempLen);
    *addnIELen = tempLen;
    vos_mem_free(tempbuf);
    return eSIR_SUCCESS;
}

void limUpdateExtCapIEtoStruct(tpAniSirGlobal pMac,
                            tANI_U8 *pBuf,
                            tDot11fIEExtCap *pDst)
{
    tANI_U8 pOut[DOT11F_IE_EXTCAP_MAX_LEN];

    if ( NULL == pBuf )
    {
        limLog( pMac, LOGE,
               FL("Invalid Buffer Address"));
        return;
    }
    if(NULL == pDst)
    {
        PELOGE(limLog(pMac, LOGE,
             FL("NULL pDst pointer"));)
        return ;
    }

    if ( DOT11F_EID_EXTCAP != pBuf[0] ||
         pBuf[1] > DOT11F_IE_EXTCAP_MAX_LEN )
    {
        limLog( pMac, LOG1,
               FL("Invalid IEs eid = %d elem_len=%d "),
                                               pBuf[0],pBuf[1]);
        return;
    }
    vos_mem_set(( tANI_U8* )&pOut[0], DOT11F_IE_EXTCAP_MAX_LEN, 0);
    /* conversion should follow 4, 2, 2 byte order */
    limUtilsframeshtonl(pMac, &pOut[0],*((tANI_U32*)&pBuf[2]),0);
    limUtilsframeshtons(pMac, &pOut[4],*((tANI_U16*)&pBuf[6]),0);
    limUtilsframeshtons(pMac, &pOut[6],*((tANI_U16*)&pBuf[8]),0);

    if ( DOT11F_PARSE_SUCCESS != dot11fUnpackIeExtCap( pMac,
                &pOut[0], DOT11F_IE_EXTCAP_MAX_LEN, pDst) )
    {
        limLog( pMac, LOGE,
               FL("dot11fUnpackIeExtCap Parse Error "));
    }
}

tSirRetStatus limStripOffExtCapIEAndUpdateStruct(tpAniSirGlobal pMac,
                                  tANI_U8* addIE,
                                  tANI_U16 *addnIELen,
                                  tDot11fIEExtCap * pDst )
{
    tANI_U8 pExtractedExtCapIEBuf[DOT11F_IE_EXTCAP_MAX_LEN + 2];
    tSirRetStatus       nSirStatus;

    vos_mem_set(( tANI_U8* )&pExtractedExtCapIEBuf[0],
        DOT11F_IE_EXTCAP_MAX_LEN + 2, 0);
    nSirStatus = limStripOffExtCapIE(pMac, addIE, addnIELen,
                                         pExtractedExtCapIEBuf);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOG1, FL("Failed to strip off in"
                        "limStripOffExtCapIE status = (%d)."),
                        nSirStatus );
        return nSirStatus;
    }
    /* update the extracted ExtCap to struct*/
    limUpdateExtCapIEtoStruct(pMac, pExtractedExtCapIEBuf, pDst);
    return nSirStatus;
}

void limMergeExtCapIEStruct(tDot11fIEExtCap *pDst,
                            tDot11fIEExtCap *pSrc)
{
    tANI_U8 *tempDst = (tANI_U8 *)pDst;
    tANI_U8 *tempSrc = (tANI_U8 *)pSrc;
    tANI_U8 structlen = sizeof(tDot11fIEExtCap);

    while(tempDst && tempSrc && structlen--)
    {
        *tempDst |= *tempSrc;
        tempDst++;
        tempSrc++;
    }
}

/**
 *
 * \brief This function is called by various LIM modules to prepare the
 * 802.11 frame MAC header
 *
 *
 * \param  pMac Pointer to Global MAC structure
 *
 * \param pBD Pointer to the frame buffer that needs to be populate
 *
 * \param type Type of the frame
 *
 * \param subType Subtype of the frame
 *
 * \return eHalStatus
 *
 *
 * The pFrameBuf argument points to the beginning of the frame buffer to
 * which - a) The 802.11 MAC header is set b) Following this MAC header
 * will be the MGMT frame payload The payload itself is populated by the
 * caller API
 *
 *
 */

tSirRetStatus limPopulateMacHeader( tpAniSirGlobal pMac,
                             tANI_U8* pBD,
                             tANI_U8 type,
                             tANI_U8 subType,
                             tSirMacAddr peerAddr, tSirMacAddr selfMacAddr)
{
    tSirRetStatus   statusCode = eSIR_SUCCESS;
    tpSirMacMgmtHdr pMacHdr;
    
    /// Prepare MAC management header
    pMacHdr = (tpSirMacMgmtHdr) (pBD);

    // Prepare FC
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = type;
    pMacHdr->fc.subType = subType;

    // Prepare Address 1
    vos_mem_copy(  (tANI_U8 *) pMacHdr->da,
                   (tANI_U8 *) peerAddr,
                   sizeof( tSirMacAddr ));

    // Prepare Address 2
    sirCopyMacAddr(pMacHdr->sa,selfMacAddr);

    // Prepare Address 3
    vos_mem_copy(  (tANI_U8 *) pMacHdr->bssId,
                   (tANI_U8 *) peerAddr,
                   sizeof( tSirMacAddr ));
    return statusCode;
} /*** end limPopulateMacHeader() ***/

/**
 * \brief limSendProbeReqMgmtFrame
 *
 *
 * \param  pMac Pointer to Global MAC structure
 *
 * \param  pSsid SSID to be sent in Probe Request frame
 *
 * \param  bssid BSSID to be sent in Probe Request frame
 *
 * \param  nProbeDelay probe delay to be used before sending Probe Request
 * frame
 *
 * \param nChannelNum Channel # on which the Probe Request is going out
 *
 * \param nAdditionalIELen if non-zero, include pAdditionalIE in the Probe Request frame
 *
 * \param pAdditionalIE if nAdditionalIELen is non zero, include this field in the Probe Request frame
 *
 * This function is called by various LIM modules to send Probe Request frame
 * during active scan/learn phase.
 * Probe request is sent out in the following scenarios:
 * --heartbeat failure:  session needed
 * --join req:           session needed
 * --foreground scan:    no session
 * --background scan:    no session
 * --schBeaconProcessing:  to get EDCA parameters:  session needed
 *
 *
 */
tSirRetStatus
limSendProbeReqMgmtFrame(tpAniSirGlobal pMac,
                         tSirMacSSid   *pSsid,
                         tSirMacAddr    bssid,
                         tANI_U8        nChannelNum,
                         tSirMacAddr    SelfMacAddr,
                         tANI_U32 dot11mode,
                         tANI_U32 nAdditionalIELen, 
                         tANI_U8 *pAdditionalIE)
{
    tDot11fProbeRequest  pr;
    tANI_U32             nStatus, nBytes, nPayload;
    tSirRetStatus        nSirStatus;
    tANI_U8             *pFrame;
    void                *pPacket;
    eHalStatus           halstatus;
    tpPESession          psessionEntry;
    tANI_U8              sessionId;
    tANI_U8             *p2pIe = NULL;
    tANI_U32             txFlag = 0;
    tANI_U32             chanbond24G = 0;

#ifndef GEN4_SCAN
    return eSIR_FAILURE;
#endif

#if defined ( ANI_DVT_DEBUG )
    return eSIR_FAILURE;
#endif

    /* The probe req should not send 11ac capabilieties if band is 2.4GHz,
     * unless enableVhtFor24GHz is enabled in INI. So if enableVhtFor24GHz
     * is false and dot11mode is 11ac set it to 11n.
     */
    if ( nChannelNum <= SIR_11B_CHANNEL_END &&
        ( FALSE == pMac->roam.configParam.enableVhtFor24GHz ) &&
         ( WNI_CFG_DOT11_MODE_11AC == dot11mode ||
           WNI_CFG_DOT11_MODE_11AC_ONLY == dot11mode ) )
            dot11mode = WNI_CFG_DOT11_MODE_11N;
    /*
    * session context may or may not be present, when probe request needs to be sent out.
    * following cases exist:
        * --heartbeat failure:  session needed
    * --join req:           session needed
    * --foreground scan:    no session
    * --background scan:    no session
    * --schBeaconProcessing:  to get EDCA parameters:  session needed
    * If session context does not exist, some IEs will be populated from CFGs, 
    * e.g. Supported and Extended rate set IEs
    */
    psessionEntry = peFindSessionByBssid(pMac,bssid,&sessionId);

    // The scheme here is to fill out a 'tDot11fProbeRequest' structure
    // and then hand it off to 'dot11fPackProbeRequest' (for
    // serialization).  We start by zero-initializing the structure:
    vos_mem_set(( tANI_U8* )&pr, sizeof( pr ), 0);

    // & delegating to assorted helpers:
    PopulateDot11fSSID( pMac, pSsid, &pr.SSID );

    if( nAdditionalIELen && pAdditionalIE )
    {
        p2pIe = limGetP2pIEPtr(pMac, pAdditionalIE, nAdditionalIELen);
    }
    /* Don't include 11b rate only when device is doing P2P Search */
    if( ( WNI_CFG_DOT11_MODE_11B != dot11mode ) && 
        ( p2pIe != NULL ) && 
    /* Don't include 11b rate if it is a P2P serach or probe request is sent by P2P Client */
        ( ( ( pMac->lim.gpLimMlmScanReq != NULL ) &&
              pMac->lim.gpLimMlmScanReq->p2pSearch ) || 
          ( ( psessionEntry != NULL ) && 
            ( VOS_P2P_CLIENT_MODE == psessionEntry->pePersona ) )
         )
      )
    {
        /* In the below API pass channel number > 14, do that it fills only
         * 11a rates in supported rates */
        PopulateDot11fSuppRates( pMac, 15, &pr.SuppRates,psessionEntry);
    }
    else
    {
        PopulateDot11fSuppRates( pMac, nChannelNum, 
                                               &pr.SuppRates,psessionEntry);

        if ( WNI_CFG_DOT11_MODE_11B != dot11mode )
        {
            PopulateDot11fExtSuppRates1( pMac, nChannelNum, &pr.ExtSuppRates );
        }
    }

#if defined WLAN_FEATURE_VOWIFI
    //Table 7-14 in IEEE Std. 802.11k-2008 says
    //DS params "can" be present in RRM is disabled and "is" present if 
    //RRM is enabled. It should be ok even if we add it into probe req when 
    //RRM is not enabled. 
    PopulateDot11fDSParams( pMac, &pr.DSParams, nChannelNum, psessionEntry );
    //Call RRM module to get the tx power for management used.
    {
       tANI_U8 txPower = (tANI_U8) rrmGetMgmtTxPower( pMac, psessionEntry );
       PopulateDot11fWFATPC( pMac, &pr.WFATPC, txPower, 0 );
    }
#endif

    if (psessionEntry != NULL ) {
       psessionEntry->htCapability = IS_DOT11_MODE_HT(dot11mode);
       //Include HT Capability IE
       if (psessionEntry->htCapability)
       {
           PopulateDot11fHTCaps( pMac, psessionEntry, &pr.HTCaps );
       }
    } else { //psessionEntry == NULL
           if (IS_DOT11_MODE_HT(dot11mode))
           {
               PopulateDot11fHTCaps( pMac, psessionEntry, &pr.HTCaps );
           }
    }

    /* Get HT40 capability  for 2.4GHz band */
    wlan_cfgGetInt(pMac,WNI_CFG_CHANNEL_BONDING_24G,&chanbond24G);
    if( (nChannelNum <= SIR_11B_CHANNEL_END) && chanbond24G != TRUE)
    {
        pr.HTCaps.supportedChannelWidthSet = eHT_CHANNEL_WIDTH_20MHZ;
        pr.HTCaps.shortGI40MHz = 0;
    }
#ifdef WLAN_FEATURE_11AC
    if (psessionEntry != NULL ) {
       psessionEntry->vhtCapability = IS_DOT11_MODE_VHT(dot11mode);
       //Include HT Capability IE
       if (psessionEntry->vhtCapability)
       {
          PopulateDot11fVHTCaps( pMac, &pr.VHTCaps, eSIR_FALSE );
       }
    }  else {
       if (IS_DOT11_MODE_VHT(dot11mode))
       {
          PopulateDot11fVHTCaps( pMac, &pr.VHTCaps, eSIR_FALSE );
       }
    }
#endif


    // That's it-- now we pack it.  First, how much space are we going to
    // need?
    nStatus = dot11fGetPackedProbeRequestSize( pMac, &pr, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Probe Request (0x%08x)."), nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Probe Request ("
                               "0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAdditionalIELen;
  
    // Ok-- try to allocate some memory:
    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Pro"
                               "be Request."), nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_REQ, bssid, SelfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return nSirStatus;      // allocated!
    }

    // That done, pack the Probe Request:
    nStatus = dot11fPackProbeRequest( pMac, &pr, pFrame +
                                      sizeof( tSirMacMgmtHdr ),
                                      nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Probe Request (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a P"
                               "robe Request (0x%08x)."), nStatus );
    }

    // Append any AddIE if present.
    if( nAdditionalIELen )
    {
        vos_mem_copy( pFrame+sizeof(tSirMacMgmtHdr)+nPayload,
                                                    pAdditionalIE, nAdditionalIELen );
        nPayload += nAdditionalIELen;
    }

    /* If this probe request is sent during P2P Search State, then we need 
     * to send it at OFDM rate. 
     */
    if( ( SIR_BAND_5_GHZ == limGetRFBand(nChannelNum))
      || (( pMac->lim.gpLimMlmScanReq != NULL) &&
          pMac->lim.gpLimMlmScanReq->p2pSearch )
      /* For unicast probe req mgmt from Join function
         we don't set above variables. So we need to add
         one more check whether it is pePersona is P2P_CLIENT or not */
      || ( ( psessionEntry != NULL ) &&
           ( VOS_P2P_CLIENT_MODE == psessionEntry->pePersona ) )
      ) 
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME; 
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) sizeof(tSirMacMgmtHdr) + nPayload,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("could not send Probe Request frame!" ));
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} // End limSendProbeReqMgmtFrame.

tSirRetStatus limGetAddnIeForProbeResp(tpAniSirGlobal pMac,
                              tANI_U8* addIE, tANI_U16 *addnIELen,
                              tANI_U8 probeReqP2pIe)
{
    /* If Probe request doesn't have P2P IE, then take out P2P IE
       from additional IE */
    if(!probeReqP2pIe)
    {
        tANI_U8* tempbuf = NULL;
        tANI_U16 tempLen = 0;
        int left = *addnIELen;
        v_U8_t *ptr = addIE;
        v_U8_t elem_id, elem_len;

        if(NULL == addIE)
        {
           PELOGE(limLog(pMac, LOGE,
                 FL(" NULL addIE pointer"));)
            return eSIR_FAILURE;
        }

        tempbuf = vos_mem_malloc(left);
        if ( NULL == tempbuf )
        {
            PELOGE(limLog(pMac, LOGE,
                 FL("Unable to allocate memory to store addn IE"));)
            return eSIR_MEM_ALLOC_FAILED;
        }

        while(left >= 2)
        {
            elem_id  = ptr[0];
            elem_len = ptr[1];
            left -= 2;
            if(elem_len > left)
            {
                limLog( pMac, LOGE,
                   FL("****Invalid IEs eid = %d elem_len=%d left=%d*****"),
                                                   elem_id,elem_len,left);
                vos_mem_free(tempbuf);
                return eSIR_FAILURE;
            }
            if ( !( (SIR_MAC_EID_VENDOR == elem_id) &&
                   (memcmp(&ptr[2], SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE)==0) ) )
            {
                vos_mem_copy (tempbuf + tempLen, &ptr[0], elem_len + 2);
                tempLen += (elem_len + 2);
            }
            left -= elem_len;
            ptr += (elem_len + 2);
       }
       vos_mem_copy (addIE, tempbuf, tempLen);
       *addnIELen = tempLen;
       vos_mem_free(tempbuf);
    }
    return eSIR_SUCCESS;
}

void
limSendProbeRspMgmtFrame(tpAniSirGlobal pMac,
                         tSirMacAddr    peerMacAddr,
                         tpAniSSID      pSsid,
                         short          nStaId,
                         tANI_U8        nKeepAlive,
                         tpPESession psessionEntry,
                         tANI_U8        probeReqP2pIe)
{
    tDot11fProbeResponse *pFrm;
    tSirRetStatus         nSirStatus;
    tANI_U32              cfg, nPayload, nStatus;
    tpSirMacMgmtHdr       pMacHdr;
    tANI_U8              *pFrame;
    void                 *pPacket;
    eHalStatus            halstatus;
    tANI_U32              addnIEPresent;
    tANI_U32              addnIE1Len=0;
    tANI_U32              addnIE2Len=0;
    tANI_U32              addnIE3Len=0;
    tANI_U16              totalAddnIeLen = 0;
    tANI_U32              wpsApEnable=0, tmp;
    tANI_U32              txFlag = 0;
    tANI_U8              *addIE = NULL;
    tANI_U8              *pP2pIe = NULL;
    tANI_U8               noaLen = 0;
    tANI_U8               total_noaLen = 0;
    tANI_U8               noaStream[SIR_MAX_NOA_ATTR_LEN
                                           + SIR_P2P_IE_HEADER_LEN];
    tANI_U8               noaIe[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
    tDot11fIEExtCap      extractedExtCap;
    tANI_BOOLEAN         extractedExtCapFlag = eANI_BOOLEAN_TRUE;
    tANI_U32             nBytes = 0;

    if(pMac->gDriverType == eDRIVER_TYPE_MFG)         // We don't answer requests
    {
        return;                     // in this case.
    }

    if(NULL == psessionEntry)
    {
        return;
    }

    pFrm = vos_mem_malloc(sizeof(tDot11fProbeResponse));
    if ( NULL == pFrm )
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory in limSendProbeRspMgmtFrame") );
        return;
    }

    vos_mem_set(( tANI_U8* )&extractedExtCap, sizeof( tDot11fIEExtCap ), 0);

    // Fill out 'frm', after which we'll just hand the struct off to
    // 'dot11fPackProbeResponse'.
    vos_mem_set(( tANI_U8* )pFrm, sizeof( tDot11fProbeResponse ), 0);

    // Timestamp to be updated by TFP, below.

    // Beacon Interval:
    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        pFrm->BeaconInterval.interval = pMac->sch.schObject.gSchBeaconInterval;        
    }
    else
    {
        nSirStatus = wlan_cfgGetInt( pMac, WNI_CFG_BEACON_INTERVAL, &cfg);
        if (eSIR_SUCCESS != nSirStatus)
        {
            limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BEACON_INTERVAL from CFG (%d)."),
                    nSirStatus );
            vos_mem_free(pFrm);
            return;
        }
        pFrm->BeaconInterval.interval = ( tANI_U16 ) cfg;
    }

    PopulateDot11fCapabilities( pMac, &pFrm->Capabilities, psessionEntry );
    PopulateDot11fSSID( pMac, ( tSirMacSSid* )pSsid, &pFrm->SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                             &pFrm->SuppRates,psessionEntry);

    PopulateDot11fDSParams( pMac, &pFrm->DSParams, psessionEntry->currentOperChannel,psessionEntry);
    PopulateDot11fIBSSParams( pMac, &pFrm->IBSSParams, psessionEntry );


    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            PopulateDot11fProbeResWPSIEs(pMac, &pFrm->WscProbeRes, psessionEntry);
        }
    }
    else
    {
        if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_ENABLE, &tmp) != eSIR_SUCCESS)
            limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_ENABLE );

        wpsApEnable = tmp & WNI_CFG_WPS_ENABLE_AP;

        if (wpsApEnable)
        {
            PopulateDot11fWscInProbeRes(pMac, &pFrm->WscProbeRes);
        }

        if (pMac->lim.wscIeInfo.probeRespWscEnrollmentState == eLIM_WSC_ENROLL_BEGIN)
        {
            PopulateDot11fWscRegistrarInfoInProbeRes(pMac, &pFrm->WscProbeRes);
            pMac->lim.wscIeInfo.probeRespWscEnrollmentState = eLIM_WSC_ENROLL_IN_PROGRESS;
        }

        if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_END)
        {
            DePopulateDot11fWscRegistrarInfoInProbeRes(pMac, &pFrm->WscProbeRes);
            pMac->lim.wscIeInfo.probeRespWscEnrollmentState = eLIM_WSC_ENROLL_NOOP;
        }
    }

    PopulateDot11fCountry( pMac, &pFrm->Country, psessionEntry);
    PopulateDot11fEDCAParamSet( pMac, &pFrm->EDCAParamSet, psessionEntry);


    if (psessionEntry->dot11mode != WNI_CFG_DOT11_MODE_11B)
        PopulateDot11fERPInfo( pMac, &pFrm->ERPInfo, psessionEntry);


    // N.B. In earlier implementations, the RSN IE would be placed in
    // the frame here, before the WPA IE, if 'RSN_BEFORE_WPA' was defined.
    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &pFrm->ExtSuppRates, psessionEntry );

    //Populate HT IEs, when operating in 11n or Taurus modes.
    if ( psessionEntry->htCapability )
    {
        PopulateDot11fHTCaps( pMac, psessionEntry, &pFrm->HTCaps );
        PopulateDot11fHTInfo( pMac, &pFrm->HTInfo, psessionEntry );
    }
#ifdef WLAN_FEATURE_11AC
    if(psessionEntry->vhtCapability)
    {
        limLog( pMac, LOG1, FL("Populate VHT IE in Probe Response"));
        PopulateDot11fVHTCaps( pMac, &pFrm->VHTCaps, eSIR_TRUE );
        PopulateDot11fVHTOperation( pMac, &pFrm->VHTOperation );
        // we do not support multi users yet
        //PopulateDot11fVHTExtBssLoad( pMac, &frm.VHTExtBssLoad );
        PopulateDot11fExtCap( pMac, &pFrm->ExtCap, psessionEntry);
    }
#endif


    if ( psessionEntry->pLimStartBssReq ) 
    {
      PopulateDot11fWPA( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &pFrm->WPA );
      PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &pFrm->RSNOpaque );
    }

    PopulateDot11fWMM( pMac, &pFrm->WMMInfoAp, &pFrm->WMMParams, &pFrm->WMMCaps, psessionEntry );

#if defined(FEATURE_WLAN_WAPI)
    if( psessionEntry->pLimStartBssReq ) 
    {
      PopulateDot11fWAPI( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &pFrm->WAPI );
    }

#endif // defined(FEATURE_WLAN_WAPI)

    addnIEPresent = false;
    if( pMac->lim.gpLimRemainOnChanReq )
    {
        nBytes += (pMac->lim.gpLimRemainOnChanReq->length - sizeof( tSirRemainOnChnReq ) );
    }
    //Only use CFG for non-listen mode. This CFG is not working for concurrency
    //In listening mode, probe rsp IEs is passed in the message from SME to PE
    else
    {

        if (wlan_cfgGetInt(pMac, WNI_CFG_PROBE_RSP_ADDNIE_FLAG,
                           &addnIEPresent) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_FLAG"));
            vos_mem_free(pFrm);
            return;
        }
    }

    if (addnIEPresent)
    {

        addIE = vos_mem_malloc(WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN*3);
        if ( NULL == addIE )
        {
            PELOGE(limLog(pMac, LOGE,
                 FL("Unable to allocate memory to store addn IE"));)
            vos_mem_free(pFrm);
            return;
        }
        
        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA1, &addnIE1Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA1 length"));
            vos_mem_free(addIE);
            vos_mem_free(pFrm);
            return;
        }
        if (addnIE1Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN && addnIE1Len &&
                     (nBytes + addnIE1Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA1, &addIE[0],
                                     &addnIE1Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA1 String"));
                vos_mem_free(addIE);
                vos_mem_free(pFrm);
                return;
            }
        }

        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA2, &addnIE2Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA2 length"));
            vos_mem_free(addIE);
            vos_mem_free(pFrm);
            return;
        }
        if (addnIE2Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA2_LEN && addnIE2Len &&
                     (nBytes + addnIE2Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA2, &addIE[addnIE1Len],
                                     &addnIE2Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA2 String"));
                vos_mem_free(addIE);
                vos_mem_free(pFrm);
                return;
            }
        }

        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA3, &addnIE3Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA3 length"));
            vos_mem_free(addIE);
            vos_mem_free(pFrm);
            return;
        }
        if (addnIE3Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA3_LEN && addnIE3Len &&
                     (nBytes + addnIE3Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA3,
                                     &addIE[addnIE1Len + addnIE2Len],
                                     &addnIE3Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA3 String"));
                vos_mem_free(addIE);
                vos_mem_free(pFrm);
                return;
            }
        }
        totalAddnIeLen = addnIE1Len + addnIE2Len + addnIE3Len;

        if(eSIR_SUCCESS != limGetAddnIeForProbeResp(pMac, addIE, &totalAddnIeLen, probeReqP2pIe))
        {
            limLog(pMac, LOGP,
                 FL("Unable to get final Additional IE for Probe Req"));
            vos_mem_free(addIE);
            vos_mem_free(pFrm);
            return;
        }

       nSirStatus = limStripOffExtCapIEAndUpdateStruct(pMac,
                                  addIE,
                                  &totalAddnIeLen,
                                  &extractedExtCap );
        if(eSIR_SUCCESS != nSirStatus )
        {
            extractedExtCapFlag = eANI_BOOLEAN_FALSE;
            limLog(pMac, LOG1,
                FL("Unable to Stripoff ExtCap IE from Probe Rsp"));
        }

        nBytes = nBytes + totalAddnIeLen;
        limLog(pMac, LOG1,
            FL("probe rsp packet size is %d "), nBytes);
        if (probeReqP2pIe)
        {
            pP2pIe = limGetP2pIEPtr(pMac, &addIE[0], totalAddnIeLen);
            if (pP2pIe != NULL)
            {
                //get NoA attribute stream P2P IE
                noaLen = limGetNoaAttrStream(pMac, noaStream, psessionEntry);
                if (noaLen != 0)
                {
                    total_noaLen = limBuildP2pIe(pMac, &noaIe[0], 
                                            &noaStream[0], noaLen); 
                    nBytes = nBytes + total_noaLen;
                    limLog(pMac, LOG1,
                        FL("p2p probe rsp packet size is  %d, noalength is %d"),
                            nBytes, total_noaLen);
                }
            }
        }
    }

    /*merge ExtCap IE*/
    if (extractedExtCapFlag && extractedExtCap.present)
    {
        limMergeExtCapIEStruct(&pFrm->ExtCap, &extractedExtCap);
    }

    nStatus = dot11fGetPackedProbeResponseSize( pMac, pFrm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Probe Response (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeResponse );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Probe Response "
                               "(0x%08x)."), nStatus );
    }

    nBytes += nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Pro"
                               "be Response."), nBytes );
        if ( addIE != NULL )
        {
            vos_mem_free(addIE);
        }
        vos_mem_free(pFrm);
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_RSP, peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Response (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        if ( addIE != NULL )
        {
            vos_mem_free(addIE);
        }
        vos_mem_free(pFrm);
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
  
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    // That done, pack the Probe Response:
    nStatus = dot11fPackProbeResponse( pMac, pFrm, pFrame + sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Probe Response (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        if ( addIE != NULL )
        {
            vos_mem_free(addIE);
        }
        vos_mem_free(pFrm);
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a P"
                               "robe Response (0x%08x)."), nStatus );
    }

    PELOG3(limLog( pMac, LOG3, FL("Sending Probe Response frame to ") );
    limPrintMacAddr( pMac, peerMacAddr, LOG3 );)

    pMac->sys.probeRespond++;

    if( pMac->lim.gpLimRemainOnChanReq )
    {
        vos_mem_copy ( pFrame+sizeof(tSirMacMgmtHdr)+nPayload,
          pMac->lim.gpLimRemainOnChanReq->probeRspIe, (pMac->lim.gpLimRemainOnChanReq->length - sizeof( tSirRemainOnChnReq )) );
    }

    if ( addnIEPresent )
    {
        vos_mem_copy(pFrame+sizeof(tSirMacMgmtHdr)+nPayload, &addIE[0], totalAddnIeLen);
    }
    if (noaLen != 0)
    {
        if (total_noaLen > (SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN))
        {
            limLog(pMac, LOGE,
                  FL("Not able to insert NoA because of length constraint."
                                        "Total Length is :%d"),total_noaLen);
            vos_mem_free(addIE);
            vos_mem_free(pFrm);
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                       ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
        else
        {
            vos_mem_copy( &pFrame[nBytes - (total_noaLen)],
                            &noaIe[0], total_noaLen);
        }
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue Probe Response frame in high priority WQ
    halstatus = halTxFrame( ( tHalHandle ) pMac, pPacket,
                            ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_LOW,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Could not send Probe Response.") );
        //Pkt will be freed up by the callback
    }

    if ( addIE != NULL )
    {
        vos_mem_free(addIE);
    }

    vos_mem_free(pFrm);
    return;


} // End limSendProbeRspMgmtFrame.

void
limSendAddtsReqActionFrame(tpAniSirGlobal    pMac,
                           tSirMacAddr       peerMacAddr,
                           tSirAddtsReqInfo *pAddTS,
                           tpPESession       psessionEntry)
{
    tANI_U16               i;
    tANI_U8               *pFrame;
    tSirRetStatus          nSirStatus;
    tDot11fAddTSRequest    AddTSReq;
    tDot11fWMMAddTSRequest WMMAddTSReq;
    tANI_U32               nPayload, nBytes, nStatus;
    tpSirMacMgmtHdr        pMacHdr;
    void                  *pPacket;
#ifdef FEATURE_WLAN_ESE
    tANI_U32               phyMode;
#endif
    eHalStatus             halstatus;
    tANI_U32               txFlag = 0;

    if(NULL == psessionEntry)
    {
           return;
    }

    if ( ! pAddTS->wmeTspecPresent )
    {
        vos_mem_set(( tANI_U8* )&AddTSReq, sizeof( AddTSReq ), 0);

        AddTSReq.Action.action     = SIR_MAC_QOS_ADD_TS_REQ;
        AddTSReq.DialogToken.token = pAddTS->dialogToken;
        AddTSReq.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        if ( pAddTS->lleTspecPresent )
        {
            PopulateDot11fTSPEC( &pAddTS->tspec, &AddTSReq.TSPEC );
        }
        else
        {
            PopulateDot11fWMMTSPEC( &pAddTS->tspec, &AddTSReq.WMMTSPEC );
        }

        if ( pAddTS->lleTspecPresent )
        {
            AddTSReq.num_WMMTCLAS = 0;
            AddTSReq.num_TCLAS = pAddTS->numTclas;
            for ( i = 0; i < pAddTS->numTclas; ++i)
            {
                PopulateDot11fTCLAS( pMac, &pAddTS->tclasInfo[i],
                                     &AddTSReq.TCLAS[i] );
            }
        }
        else
        {
            AddTSReq.num_TCLAS = 0;
            AddTSReq.num_WMMTCLAS = pAddTS->numTclas;
            for ( i = 0; i < pAddTS->numTclas; ++i)
            {
                PopulateDot11fWMMTCLAS( pMac, &pAddTS->tclasInfo[i],
                                        &AddTSReq.WMMTCLAS[i] );
            }
        }

        if ( pAddTS->tclasProcPresent )
        {
            if ( pAddTS->lleTspecPresent )
            {
                AddTSReq.TCLASSPROC.processing = pAddTS->tclasProc;
                AddTSReq.TCLASSPROC.present    = 1;
            }
            else
            {
                AddTSReq.WMMTCLASPROC.version    = 1;
                AddTSReq.WMMTCLASPROC.processing = pAddTS->tclasProc;
                AddTSReq.WMMTCLASPROC.present    = 1;
            }
        }

        nStatus = dot11fGetPackedAddTSRequestSize( pMac, &AddTSReq, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                                   "or an Add TS Request (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSRequest );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calculating"
                                   "the packed size for an Add TS Request"
                                   " (0x%08x)."), nStatus );
        }
    }
    else
    {
        vos_mem_set(( tANI_U8* )&WMMAddTSReq, sizeof( WMMAddTSReq ), 0);

        WMMAddTSReq.Action.action     = SIR_MAC_QOS_ADD_TS_REQ;
        WMMAddTSReq.DialogToken.token = pAddTS->dialogToken;
        WMMAddTSReq.Category.category = SIR_MAC_ACTION_WME;

        // WMM spec 2.2.10 - status code is only filled in for ADDTS response
        WMMAddTSReq.StatusCode.statusCode = 0;

        PopulateDot11fWMMTSPEC( &pAddTS->tspec, &WMMAddTSReq.WMMTSPEC );
#ifdef FEATURE_WLAN_ESE
        limGetPhyMode(pMac, &phyMode, psessionEntry);

        if( phyMode == WNI_CFG_PHY_MODE_11G || phyMode == WNI_CFG_PHY_MODE_11A)
        {
            pAddTS->tsrsIE.rates[0] = TSRS_11AG_RATE_6MBPS;
        }
        else 
        {
            pAddTS->tsrsIE.rates[0] = TSRS_11B_RATE_5_5MBPS;
        }
        PopulateDot11TSRSIE(pMac,&pAddTS->tsrsIE, &WMMAddTSReq.ESETrafStrmRateSet,sizeof(tANI_U8));
#endif
        // fillWmeTspecIE

        nStatus = dot11fGetPackedWMMAddTSRequestSize( pMac, &WMMAddTSReq, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                                   "or a WMM Add TS Request (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSRequest );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calculating"
                                   "the packed size for a WMM Add TS Requ"
                                   "est (0x%08x)."), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Request."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    #if 0
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Request.") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }
    #endif //TO SUPPORT BT-AMP
    
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peerMacAddr, pMacHdr);
#endif

    // That done, pack the struct:
    if ( ! pAddTS->wmeTspecPresent )
    {
        nStatus = dot11fPackAddTSRequest( pMac, &AddTSReq,
                                          pFrame + sizeof(tSirMacMgmtHdr),
                                          nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack an Add TS Request "
                                   "(0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "an Add TS Request (0x%08x)."), nStatus );
        }
    }
    else
    {
        nStatus = dot11fPackWMMAddTSRequest( pMac, &WMMAddTSReq,
                                             pFrame + sizeof(tSirMacMgmtHdr),
                                             nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Add TS Reque"
                                   "st (0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;            // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "a WMM Add TS Request (0x%08x)."), nStatus );
        }
    }

    PELOG3(limLog( pMac, LOG3, FL("Sending an Add TS Request frame to ") );
    limPrintMacAddr( pMac, peerMacAddr, LOG3 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    // Queue Addts Response frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL( "*** Could not send an Add TS Request"
                                " (%X) ***" ), halstatus );
        //Pkt will be freed up by the callback
    }

} // End limSendAddtsReqActionFrame.



void
limSendAssocRspMgmtFrame(tpAniSirGlobal pMac,
                         tANI_U16       statusCode,
                         tANI_U16       aid,
                         tSirMacAddr    peerMacAddr,
                         tANI_U8        subType,
                         tpDphHashNode  pSta,tpPESession psessionEntry)
{
    static tDot11fAssocResponse frm;
    tANI_U8             *pFrame, *macAddr;
    tpSirMacMgmtHdr      pMacHdr;
    tSirRetStatus        nSirStatus;
    tANI_U8              lleMode = 0, fAddTS, edcaInclude = 0;
    tHalBitVal           qosMode, wmeMode;
    tANI_U32             nPayload, nStatus;
    void                *pPacket;
    eHalStatus           halstatus;
    tUpdateBeaconParams  beaconParams;
    tANI_U32             txFlag = 0;
    tANI_U32             addnIEPresent = false;
    tANI_U32             addnIELen=0;
    tANI_U8              addIE[WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN];
    tpSirAssocReq        pAssocReq = NULL; 
    tANI_U16             addStripoffIELen = 0;
    tDot11fIEExtCap      extractedExtCap;
    tANI_BOOLEAN         extractedExtCapFlag = eANI_BOOLEAN_FALSE;
    tANI_U32             nBytes = 0;

#ifdef WLAN_FEATURE_11W
    tANI_U32 retryInterval;
    tANI_U32 maxRetries;
#endif

    if(NULL == psessionEntry)
    {
        limLog( pMac, LOGE, FL("psessionEntry is NULL"));
        return;
    }

    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    limGetQosMode(psessionEntry, &qosMode);
    limGetWmeMode(psessionEntry, &wmeMode);

    // An Add TS IE is added only if the AP supports it and the requesting
    // STA sent a traffic spec.
    fAddTS = ( qosMode && pSta && pSta->qos.addtsPresent ) ? 1 : 0;

    PopulateDot11fCapabilities( pMac, &frm.Capabilities, psessionEntry );

    frm.Status.status = statusCode;

    frm.AID.associd = aid | LIM_AID_MASK;

    if ( NULL == pSta )
    {
       PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &frm.SuppRates,psessionEntry);
       PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &frm.ExtSuppRates, psessionEntry );
    }
    else
    {
       PopulateDot11fAssocRspRates( pMac, &frm.SuppRates, &frm.ExtSuppRates,
                      pSta->supportedRates.llbRates, pSta->supportedRates.llaRates );
    }

    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if( pSta != NULL && eSIR_SUCCESS == statusCode )
        {
            pAssocReq = 
                (tpSirAssocReq) psessionEntry->parsedAssocReq[pSta->assocId];
            /* populate P2P IE in AssocRsp when assocReq from the peer includes P2P IE */
            if( pAssocReq != NULL && pAssocReq->addIEPresent ) {
                PopulateDot11AssocResP2PIE(pMac, &frm.P2PAssocRes, pAssocReq);
            }
        }
    }

    if ( NULL != pSta )
    {
        if ( eHAL_SET == qosMode )
        {
            if ( pSta->lleEnabled )
            {
                lleMode = 1;
                if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) )
                {
                    PopulateDot11fEDCAParamSet( pMac, &frm.EDCAParamSet, psessionEntry);

//                     FramesToDo:...
//                     if ( fAddTS )
//                     {
//                         tANI_U8 *pAf = pBody;
//                         *pAf++ = SIR_MAC_QOS_ACTION_EID;
//                         tANI_U32 tlen;
//                         status = sirAddtsRspFill(pMac, pAf, statusCode, &pSta->qos.addts, NULL,
//                                                  &tlen, bufLen - frameLen);
//                     } // End if on Add TS.
                }
            } // End if on .11e enabled in 'pSta'.
        } // End if on QOS Mode on.

        if ( ( ! lleMode ) && ( eHAL_SET == wmeMode ) && pSta->wmeEnabled )
        {
            if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {

                PopulateDot11fWMMParams( pMac, &frm.WMMParams, psessionEntry);

                if ( pSta->wsmEnabled )
                {
                    PopulateDot11fWMMCaps(&frm.WMMCaps );
                }
            }
        }

        if ( pSta->aniPeer )
        {
            if ( ( lleMode && PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) ||
                 ( pSta->wmeEnabled && PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {
                edcaInclude = 1;
            }

        } // End if on Airgo peer.

        if ( pSta->mlmStaContext.htCapability  && 
             psessionEntry->htCapability )
        {
            PopulateDot11fHTCaps( pMac, psessionEntry, &frm.HTCaps );
            /*
             *Check the STA capability and update the HTCaps accordingly
             */
            frm.HTCaps.supportedChannelWidthSet =
            (pSta->htSupportedChannelWidthSet < psessionEntry->htSupportedChannelWidthSet) ?
            pSta->htSupportedChannelWidthSet : psessionEntry->htSupportedChannelWidthSet ;

            if (!frm.HTCaps.supportedChannelWidthSet)
                frm.HTCaps.shortGI40MHz = 0;

            PopulateDot11fHTInfo( pMac, &frm.HTInfo, psessionEntry );
        }

#ifdef WLAN_FEATURE_11AC
        if( pSta->mlmStaContext.vhtCapability && 
            psessionEntry->vhtCapability )
        {
            limLog( pMac, LOG1, FL("Populate VHT IEs in Assoc Response"));
            PopulateDot11fVHTCaps( pMac, &frm.VHTCaps, eSIR_TRUE );
            PopulateDot11fVHTOperation( pMac, &frm.VHTOperation);
            PopulateDot11fExtCap( pMac, &frm.ExtCap, psessionEntry);
        }
#endif

#ifdef WLAN_FEATURE_11W
        if( eSIR_MAC_TRY_AGAIN_LATER == statusCode )
        {
            if ( wlan_cfgGetInt(pMac, WNI_CFG_PMF_SA_QUERY_MAX_RETRIES,
                                &maxRetries ) != eSIR_SUCCESS )
                limLog( pMac, LOGE,
                        FL("Could not retrieve PMF SA Query maximum retries value") );
            else
                if ( wlan_cfgGetInt(pMac, WNI_CFG_PMF_SA_QUERY_RETRY_INTERVAL,
                                    &retryInterval ) != eSIR_SUCCESS)
                    limLog( pMac, LOGE,
                            FL("Could not retrieve PMF SA Query timer interval value") );
                else
                    PopulateDot11fTimeoutInterval(
                        pMac, &frm.TimeoutInterval, SIR_MAC_TI_TYPE_ASSOC_COMEBACK,
                        (maxRetries - pSta->pmfSaQueryRetryCount) * retryInterval );
        }
#endif
    } // End if on non-NULL 'pSta'.

    vos_mem_set(( tANI_U8* )&beaconParams, sizeof( tUpdateBeaconParams), 0);

    if( psessionEntry->limSystemRole == eLIM_AP_ROLE ){
        if(psessionEntry->gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideApProtection(pMac, peerMacAddr, &beaconParams,psessionEntry);
    }

    limUpdateShortPreamble(pMac, peerMacAddr, &beaconParams, psessionEntry);
    limUpdateShortSlotTime(pMac, peerMacAddr, &beaconParams, psessionEntry);

    beaconParams.bssIdx = psessionEntry->bssIdx;

    //Send message to HAL about beacon parameter change.
    if(beaconParams.paramChangeBitmap)
    {
        schSetFixedBeaconFields(pMac,psessionEntry);
        limSendBeaconParams(pMac, &beaconParams, psessionEntry );
    }

    if ( pAssocReq != NULL ) 
    {
        if (wlan_cfgGetInt(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 
                    &addnIEPresent) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Unable to get "
                                  "WNI_CFG_ASSOC_RSP_ADDNIE_FLAG"));
            return;
        }

        if (addnIEPresent)
        {
            //Assoc rsp IE available
            if (wlan_cfgGetStrLen(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_DATA,
                        &addnIELen) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP, FL("Unable to get "
                           "WNI_CFG_ASSOC_RSP_ADDNIE_DATA length"));
                return;
            }

            if (addnIELen <= WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN && addnIELen &&
                    (nBytes + addnIELen) <= SIR_MAX_PACKET_SIZE)
            {
                if (wlan_cfgGetStr(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_DATA,
                            &addIE[0], &addnIELen) == eSIR_SUCCESS)
                {

                    vos_mem_set(( tANI_U8* )&extractedExtCap,
                        sizeof( tDot11fIEExtCap ), 0);
                    addStripoffIELen = addnIELen;
                    nSirStatus = limStripOffExtCapIEAndUpdateStruct(pMac,
                                      &addIE[0],
                                      &addStripoffIELen,
                                      &extractedExtCap );
                    if(eSIR_SUCCESS != nSirStatus)
                    {
                        limLog(pMac, LOG1,
                            FL("Unable to Stripoff ExtCap IE from Assoc Rsp"));
                    }
                    else
                    {
                        addnIELen = addStripoffIELen;
                        extractedExtCapFlag = eANI_BOOLEAN_TRUE;
                    }
                    nBytes = nBytes + addnIELen;
                }
            }
        }
    }

    /* merge the ExtCap struct*/
    if (extractedExtCapFlag && extractedExtCap.present)
    {
        limMergeExtCapIEStruct(&(frm.ExtCap), &extractedExtCap);
    }

    nStatus = dot11fGetPackedAssocResponseSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to calculate the packed size f"
                               "or an Association Response (0x%08x)."),
                nStatus );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for an Association Re"
                               "sponse (0x%08x)."), nStatus );
    }

    nBytes += sizeof( tSirMacMgmtHdr ) + nPayload;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGP, FL("Call to bufAlloc failed for RE/ASSOC RSP."));
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac,
                                pFrame,
                                SIR_MAC_MGMT_FRAME,
                                ( LIM_ASSOC == subType ) ?
                                    SIR_MAC_MGMT_ASSOC_RSP :
                                    SIR_MAC_MGMT_REASSOC_RSP,
                                peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Association Response (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    nStatus = dot11fPackAssocResponse( pMac, &frm,
                                       pFrame + sizeof( tSirMacMgmtHdr ),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack an Association Response"
                               " (0x%08x)."), nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing an "
                               "Association Response (0x%08x)."), nStatus );
    }

    macAddr = pMacHdr->da;

    if (subType == LIM_ASSOC)
    {
        PELOG1(limLog(pMac, LOG1,
               FL("*** Sending Assoc Resp status %d aid %d to "),
               statusCode, aid);)
    }    
    else{
        PELOG1(limLog(pMac, LOG1,
               FL("*** Sending ReAssoc Resp status %d aid %d to "),
               statusCode, aid);)
    }
    PELOG1(limPrintMacAddr(pMac, pMacHdr->da, LOG1);)

    if ( addnIEPresent )
    {
        vos_mem_copy (  pFrame+sizeof(tSirMacMgmtHdr)+nPayload, &addIE[0], addnIELen ) ;
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    limLog( pMac, LOG1, FL("Sending Assoc resp over WQ5 to "MAC_ADDRESS_STR
                " From " MAC_ADDRESS_STR),MAC_ADDR_ARRAY(pMacHdr->da),
              MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));

    txFlag |= HAL_USE_FW_IN_TX_PATH;

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    /// Queue Association Response frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("*** Could not Send Re/AssocRsp, retCode=%X ***"),
               nSirStatus);

        //Pkt will be freed up by the callback
    }

    // update the ANI peer station count
    //FIXME_PROTECTION : take care of different type of station
    // counter inside this function.
    limUtilCountStaAdd(pMac, pSta, psessionEntry);

} // End limSendAssocRspMgmtFrame.


 
void
limSendAddtsRspActionFrame(tpAniSirGlobal     pMac,
                           tSirMacAddr        peer,
                           tANI_U16           nStatusCode,
                           tSirAddtsReqInfo  *pAddTS,
                           tSirMacScheduleIE *pSchedule,
                           tpPESession        psessionEntry)
{
    tANI_U8                *pFrame;
    tpSirMacMgmtHdr         pMacHdr;
    tDot11fAddTSResponse    AddTSRsp;
    tDot11fWMMAddTSResponse WMMAddTSRsp;
    tSirRetStatus           nSirStatus;
    tANI_U32                i, nBytes, nPayload, nStatus;
    void                   *pPacket;
    eHalStatus              halstatus;
    tANI_U32                txFlag = 0;

    if(NULL == psessionEntry)
    {
              return;
    }

    if ( ! pAddTS->wmeTspecPresent )
    {
        vos_mem_set( ( tANI_U8* )&AddTSRsp, sizeof( AddTSRsp ), 0 );

        AddTSRsp.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        AddTSRsp.Action.action     = SIR_MAC_QOS_ADD_TS_RSP;
        AddTSRsp.DialogToken.token = pAddTS->dialogToken;
        AddTSRsp.Status.status     = nStatusCode;

        // The TsDelay information element is only filled in for a specific
        // status code:
        if ( eSIR_MAC_TS_NOT_CREATED_STATUS == nStatusCode )
        {
            if ( pAddTS->wsmTspecPresent )
            {
                AddTSRsp.WMMTSDelay.version = 1;
                AddTSRsp.WMMTSDelay.delay   = 10;
                AddTSRsp.WMMTSDelay.present = 1;
            }
            else
            {
                AddTSRsp.TSDelay.delay   = 10;
                AddTSRsp.TSDelay.present = 1;
            }
        }

        if ( pAddTS->wsmTspecPresent )
        {
            PopulateDot11fWMMTSPEC( &pAddTS->tspec, &AddTSRsp.WMMTSPEC );
        }
        else
        {
            PopulateDot11fTSPEC( &pAddTS->tspec, &AddTSRsp.TSPEC );
        }

        if ( pAddTS->wsmTspecPresent )
        {
            AddTSRsp.num_WMMTCLAS = 0;
            AddTSRsp.num_TCLAS = pAddTS->numTclas;
            for ( i = 0; i < AddTSRsp.num_TCLAS; ++i)
            {
                PopulateDot11fTCLAS( pMac, &pAddTS->tclasInfo[i],
                                     &AddTSRsp.TCLAS[i] );
            }
        }
        else
        {
            AddTSRsp.num_TCLAS = 0;
            AddTSRsp.num_WMMTCLAS = pAddTS->numTclas;
            for ( i = 0; i < AddTSRsp.num_WMMTCLAS; ++i)
            {
                PopulateDot11fWMMTCLAS( pMac, &pAddTS->tclasInfo[i],
                                        &AddTSRsp.WMMTCLAS[i] );
            }
        }

        if ( pAddTS->tclasProcPresent )
        {
            if ( pAddTS->wsmTspecPresent )
            {
                AddTSRsp.WMMTCLASPROC.version    = 1;
                AddTSRsp.WMMTCLASPROC.processing = pAddTS->tclasProc;
                AddTSRsp.WMMTCLASPROC.present    = 1;
            }
            else
            {
                AddTSRsp.TCLASSPROC.processing = pAddTS->tclasProc;
                AddTSRsp.TCLASSPROC.present    = 1;
            }
        }

        // schedule element is included only if requested in the tspec and we are
        // using hcca (or both edca and hcca)
        // 11e-D8.0 is inconsistent on whether the schedule element is included
        // based on tspec schedule bit or not. Sec 7.4.2.2. says one thing but
        // pg 46, line 17-18 says something else. So just include it and let the
        // sta figure it out
        if ((pSchedule != NULL) &&
            ((pAddTS->tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA) ||
             (pAddTS->tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_BOTH)))
        {
            if ( pAddTS->wsmTspecPresent )
            {
                PopulateDot11fWMMSchedule( pSchedule, &AddTSRsp.WMMSchedule );
            }
            else
            {
                PopulateDot11fSchedule( pSchedule, &AddTSRsp.Schedule );
            }
        }

        nStatus = dot11fGetPackedAddTSResponseSize( pMac, &AddTSRsp, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for an Add TS Response (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSResponse );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for an Add TS"
                                   " Response (0x%08x)."), nStatus );
        }
    }
    else
    {
        vos_mem_set( ( tANI_U8* )&WMMAddTSRsp, sizeof( WMMAddTSRsp ), 0 );

        WMMAddTSRsp.Category.category = SIR_MAC_ACTION_WME;
        WMMAddTSRsp.Action.action     = SIR_MAC_QOS_ADD_TS_RSP;
        WMMAddTSRsp.DialogToken.token = pAddTS->dialogToken;
        WMMAddTSRsp.StatusCode.statusCode = (tANI_U8)nStatusCode;

        PopulateDot11fWMMTSPEC( &pAddTS->tspec, &WMMAddTSRsp.WMMTSPEC );

        nStatus = dot11fGetPackedWMMAddTSResponseSize( pMac, &WMMAddTSRsp, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a WMM Add TS Response (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fWMMAddTSResponse );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for a WMM Add"
                                   "TS Response (0x%08x)."), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Response."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set(  pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Response (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

     
    #if 0
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Response.") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    // That done, pack the struct:
    if ( ! pAddTS->wmeTspecPresent )
    {
        nStatus = dot11fPackAddTSResponse( pMac, &AddTSRsp,
                                           pFrame + sizeof( tSirMacMgmtHdr ),
                                           nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack an Add TS Response "
                                   "(0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "an Add TS Response (0x%08x)."), nStatus );
        }
    }
    else
    {
        nStatus = dot11fPackWMMAddTSResponse( pMac, &WMMAddTSRsp,
                                              pFrame + sizeof( tSirMacMgmtHdr ),
                                              nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Add TS Response "
                                   "(0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "a WMM Add TS Response (0x%08x)."), nStatus );
        }
    }

    PELOG1(limLog( pMac, LOG1, FL("Sending an Add TS Response (status %d) to "),
            nStatusCode );
    limPrintMacAddr( pMac, pMacHdr->da, LOG1 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    // Queue the frame in high priority WQ:
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Add TS Response (%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
    }

} // End limSendAddtsRspActionFrame.

void
limSendDeltsReqActionFrame(tpAniSirGlobal  pMac,
                           tSirMacAddr  peer,
                           tANI_U8  wmmTspecPresent,
                           tSirMacTSInfo  *pTsinfo,
                           tSirMacTspecIE  *pTspecIe,
                           tpPESession psessionEntry)
{
    tANI_U8         *pFrame;
    tpSirMacMgmtHdr  pMacHdr;
    tDot11fDelTS     DelTS;
    tDot11fWMMDelTS  WMMDelTS;
    tSirRetStatus    nSirStatus;
    tANI_U32         nBytes, nPayload, nStatus;
    void            *pPacket;
    eHalStatus       halstatus;
    tANI_U32         txFlag = 0;

    if(NULL == psessionEntry)
    {
              return;
    }

    if ( ! wmmTspecPresent )
    {
        vos_mem_set( ( tANI_U8* )&DelTS, sizeof( DelTS ), 0 );

        DelTS.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        DelTS.Action.action     = SIR_MAC_QOS_DEL_TS_REQ;
        PopulateDot11fTSInfo( pTsinfo, &DelTS.TSInfo );

        nStatus = dot11fGetPackedDelTSSize( pMac, &DelTS, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a Del TS (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fDelTS );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for a Del TS"
                                   " (0x%08x)."), nStatus );
        }
    }
    else
    {
        vos_mem_set( ( tANI_U8* )&WMMDelTS, sizeof( WMMDelTS ), 0 );

        WMMDelTS.Category.category = SIR_MAC_ACTION_WME;
        WMMDelTS.Action.action     = SIR_MAC_QOS_DEL_TS_REQ;
        WMMDelTS.DialogToken.token = 0;
        WMMDelTS.StatusCode.statusCode = 0;
        PopulateDot11fWMMTSPEC( pTspecIe, &WMMDelTS.WMMTSPEC );
        nStatus = dot11fGetPackedWMMDelTSSize( pMac, &WMMDelTS, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a WMM Del TS (0x%08x)."),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fDelTS );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for a WMM De"
                                   "l TS (0x%08x)."), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Response."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer,
                                psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Response (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    #if 0

    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Response.") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId, psessionEntry->bssId);
    
#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    // That done, pack the struct:
    if ( !wmmTspecPresent )
    {
        nStatus = dot11fPackDelTS( pMac, &DelTS,
                                   pFrame + sizeof( tSirMacMgmtHdr ),
                                   nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a Del TS frame (0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "a Del TS frame (0x%08x)."), nStatus );
        }
    }
    else
    {
        nStatus = dot11fPackWMMDelTS( pMac, &WMMDelTS,
                                      pFrame + sizeof( tSirMacMgmtHdr ),
                                      nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Del TS frame (0x%08x)."),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing "
                                   "a WMM Del TS frame (0x%08x)."), nStatus );
        }
    }

    PELOG1(limLog(pMac, LOG1, FL("Sending DELTS REQ (size %d) to "), nBytes);
    limPrintMacAddr(pMac, pMacHdr->da, LOG1);)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Del TS (%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
    }

} // End limSendDeltsReqActionFrame.

void
limSendAssocReqMgmtFrame(tpAniSirGlobal   pMac,
                         tLimMlmAssocReq *pMlmAssocReq,
                         tpPESession psessionEntry)
{
    tDot11fAssocRequest *pFrm;
    tANI_U16            caps;
    tANI_U8            *pFrame;
    tSirRetStatus       nSirStatus;
    tLimMlmAssocCnf     mlmAssocCnf;
    tANI_U32            nPayload, nStatus;
    tANI_U8             fQosEnabled, fWmeEnabled, fWsmEnabled;
    void               *pPacket;
    eHalStatus          halstatus;
    tANI_U16            nAddIELen; 
    tANI_U8             *pAddIE;
    tANI_U8             *wpsIe = NULL;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8             PowerCapsPopulated = FALSE;
#endif
    tANI_U32            txFlag = 0;
    tpSirMacMgmtHdr     pMacHdr;
    tDot11fIEExtCap     extractedExtCap;
    tANI_BOOLEAN        extractedExtCapFlag = eANI_BOOLEAN_TRUE;
    tANI_U32            nBytes = 0;

    if(NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL") );
        return;
    }

    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimJoinReq)
    {
        limLog(pMac, LOGE, FL("psessionEntry->pLimJoinReq is NULL") );
        return;
    }
    nAddIELen = psessionEntry->pLimJoinReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimJoinReq->addIEAssoc.addIEdata;

    pFrm = vos_mem_malloc(sizeof(tDot11fAssocRequest));
    if ( NULL == pFrm )
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory") );
        return;
    }


    vos_mem_set( ( tANI_U8* )pFrm, sizeof( tDot11fAssocRequest ), 0 );

    vos_mem_set(( tANI_U8* )&extractedExtCap, sizeof( tDot11fIEExtCap ), 0);
    nSirStatus = limStripOffExtCapIEAndUpdateStruct(pMac, pAddIE,
                                  &nAddIELen,
                                  &extractedExtCap );
    if(eSIR_SUCCESS != nSirStatus )
    {
        extractedExtCapFlag = eANI_BOOLEAN_FALSE;
        limLog(pMac, LOG1,
             FL("Unable to Stripoff ExtCap IE from Assoc Req"));
    }
    /* TODO:remove this code once driver provides the call back function
     * to supplicant for set_qos_map
     */
    else
    {
        if(extractedExtCap.interworkingService)
        {
            extractedExtCap.qosMap = 1;
        }
    }

    caps = pMlmAssocReq->capabilityInfo;
    if ( PROP_CAPABILITY_GET( 11EQOS, psessionEntry->limCurrentBssPropCap ) )
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
       According to WAPI standard:
       7.3.1.4 Capability Information field
       In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
       Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
       Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&pFrm->Capabilities );

    pFrm->ListenInterval.interval = pMlmAssocReq->listenInterval;
    PopulateDot11fSSID2( pMac, &pFrm->SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &pFrm->SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled) &&
        SIR_MAC_GET_QOS( psessionEntry->limCurrentBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limCurrentBssQosCaps );

    // We prefer .11e asociations:
    if ( fQosEnabled ) fWmeEnabled = false;

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limCurrentBssQosCaps );

    if ( psessionEntry->lim11hEnable  &&
            psessionEntry->pLimJoinReq->spectrumMgtIndicator == eSIR_TRUE ) 
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;

        PopulateDot11fPowerCaps( pMac, &pFrm->PowerCaps, LIM_ASSOC,psessionEntry);
#endif
        PopulateDot11fSuppChannels( pMac, &pFrm->SuppChannels, LIM_ASSOC,psessionEntry);

    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &pFrm->PowerCaps, LIM_ASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
            ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limCurrentBssPropCap)))
        PopulateDot11fQOSCapsStation( pMac, &pFrm->QOSCapsStation );

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &pFrm->ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &pFrm->RRMEnabledCap, psessionEntry );       
    }
#endif
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if( nAddIELen && pAddIE )
    {
        wpsIe = limGetWscIEPtr (pMac, pAddIE, nAddIELen);
    }
    if ( NULL == wpsIe )
    {
        PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &pFrm->RSNOpaque );
        PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &pFrm->WPAOpaque );
#if defined(FEATURE_WLAN_WAPI)
        PopulateDot11fWAPIOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &pFrm->WAPIOpaque );
#endif // defined(FEATURE_WLAN_WAPI)
    }

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limCurrentBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &pFrm->WMMInfoStation );
        }

        if ( fWsmEnabled &&
                ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limCurrentBssPropCap )))
        {
            PopulateDot11fWMMCaps( &pFrm->WMMCaps );
        }
    }

    //Populate HT IEs, when operating in 11n or Taurus modes AND
    //when AP is also operating in 11n mode.
    if ( psessionEntry->htCapability &&
            pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, psessionEntry, &pFrm->HTCaps );
#ifdef DISABLE_GF_FOR_INTEROP

        /*
         * To resolve the interop problem with Broadcom AP, 
         * where TQ STA could not pass traffic with GF enabled,
         * TQ STA will do Greenfield only with TQ AP, for 
         * everybody else it will be turned off.
         */

        if( (psessionEntry->pLimJoinReq != NULL) && (!psessionEntry->pLimJoinReq->bssDescription.aniIndicator))
        {
                limLog( pMac, LOG1, FL("Sending Assoc Req to Non-TQ AP,"
                                        " Turning off Greenfield"));
            pFrm->HTCaps.greenField = WNI_CFG_GREENFIELD_CAPABILITY_DISABLE;
        }
#endif

    }
#ifdef WLAN_FEATURE_11AC
    if ( psessionEntry->vhtCapability &&
        psessionEntry->vhtCapabilityPresentInBeacon)
    {
        limLog( pMac, LOG1, FL("Populate VHT IEs in Assoc Request"));
        PopulateDot11fVHTCaps( pMac, &pFrm->VHTCaps, eSIR_FALSE );
    }
#endif
    PopulateDot11fExtCap( pMac, &pFrm->ExtCap, psessionEntry);

#if defined WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->pLimJoinReq->is11Rconnection)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        limLog( pMac, LOG1, FL("mdie = %02x %02x %02x"),
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[0],
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[1],
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[2]);
#endif
        PopulateMDIE( pMac, &pFrm->MobilityDomain,
                                 psessionEntry->pLimJoinReq->bssDescription.mdie);
    }
    else
    {
        // No 11r IEs dont send any MDIE
        limLog( pMac, LOG1, FL("MDIE not present"));
    }
#endif

#ifdef FEATURE_WLAN_ESE
    /* For ESE Associations fill the ESE IEs */
    if (psessionEntry->isESEconnection &&
        psessionEntry->pLimJoinReq->isESEFeatureIniEnabled)
    {
#ifndef FEATURE_DISABLE_RM
        PopulateDot11fESERadMgmtCap(&pFrm->ESERadMgmtCap);
#endif
        PopulateDot11fESEVersion(&pFrm->ESEVersion);
    }
#endif

    /* merge the ExtCap struct*/
    if (extractedExtCapFlag && extractedExtCap.present)
    {
        limMergeExtCapIEStruct(&pFrm->ExtCap, &extractedExtCap);
    }

    nStatus = dot11fGetPackedAssocRequestSize( pMac, pFrm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                    "or an Association Request (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                    "the packed size for an Association Re "
                    "quest(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
            ( tANI_U16 )nBytes, ( void** ) &pFrame,
            ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an As"
                    "sociation Request."), nBytes );

        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));


        /* Update PE session id*/
        mlmAssocCnf.sessionId = psessionEntry->peSessionId;

        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;

        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                ( void* ) pFrame, ( void* ) pPacket );

        limPostSmeMessage( pMac, LIM_MLM_ASSOC_CNF,
                ( tANI_U32* ) &mlmAssocCnf);

        vos_mem_free(pFrm);
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
            SIR_MAC_MGMT_ASSOC_REQ, psessionEntry->bssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                    "tor for an Association Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        vos_mem_free(pFrm);
        return;
    }

    // That done, pack the Assoc Request:
    nStatus = dot11fPackAssocRequest( pMac, pFrm, pFrame +
            sizeof(tSirMacMgmtHdr),
            nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Assoc Request (0x%0"
                    "8x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                ( void* ) pFrame, ( void* ) pPacket );
        vos_mem_free(pFrm);
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a Assoc"
                               "Request (0x%08x)."), nStatus );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending Association Request length %d"
                    "to "),
                nBytes );)
        //   limPrintMacAddr( pMac, bssid, LOG1 );

        if( psessionEntry->assocReq != NULL )
        {
            vos_mem_free(psessionEntry->assocReq);
            psessionEntry->assocReq = NULL;
        }

    if( nAddIELen )
    {
        vos_mem_copy( pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
                      pAddIE,
                      nAddIELen );
        nPayload += nAddIELen;
    }

    psessionEntry->assocReq = vos_mem_malloc(nPayload);
    if ( NULL == psessionEntry->assocReq )
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store "
                                     "assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        vos_mem_copy( psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if(psessionEntry->pePersona == VOS_P2P_CLIENT_MODE)
    {
        txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    limLog( pMac, LOG1, FL("Sending Assoc req over WQ5 to "MAC_ADDRESS_STR
              " From " MAC_ADDRESS_STR),MAC_ADDR_ARRAY(pMacHdr->da),
              MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));
    txFlag |= HAL_USE_FW_IN_TX_PATH;

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));

    // enable caching
    WLANTL_EnableCaching(psessionEntry->staId);

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (sizeof(tSirMacMgmtHdr) + nPayload),
            HAL_TXRX_FRM_802_11_MGMT,
            ANI_TXDIR_TODS,
            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Association Request (%X)!"),
                halstatus );
        //Pkt will be freed up by the callback
        vos_mem_free(pFrm);
        return;
    }

    // Free up buffer allocated for mlmAssocReq
    vos_mem_free(pMlmAssocReq);
    pMlmAssocReq = NULL;
    vos_mem_free(pFrm);
    return;
} // End limSendAssocReqMgmtFrame


#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
/*------------------------------------------------------------------------------------
 *
 * Send Reassoc Req with FTIEs.
 *
 *-----------------------------------------------------------------------------------
 */
void
limSendReassocReqWithFTIEsMgmtFrame(tpAniSirGlobal     pMac,
                           tLimMlmReassocReq *pMlmReassocReq,tpPESession psessionEntry)
{
    static tDot11fReAssocRequest frm;
    tANI_U16              caps;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tANI_U32              nBytes, nPayload, nStatus;
    tANI_U8               fQosEnabled, fWmeEnabled, fWsmEnabled;
    void                 *pPacket;
    eHalStatus            halstatus;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8               PowerCapsPopulated = FALSE;
#endif
    tANI_U16              ft_ies_length = 0;
    tANI_U8               *pBody;
    tANI_U16              nAddIELen; 
    tANI_U8               *pAddIE;
#if defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    tANI_U8               *wpsIe = NULL;
#endif
    tANI_U32              txFlag = 0;
    tpSirMacMgmtHdr       pMacHdr;

    if (NULL == psessionEntry)
    {
        return;
    }

    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimReAssocReq)
    {
        return;
    }
    nAddIELen = psessionEntry->pLimReAssocReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimReAssocReq->addIEAssoc.addIEdata;
    limLog( pMac, LOG1, FL("limSendReassocReqWithFTIEsMgmtFrame received in "
                           "state (%d)."), psessionEntry->limMlmState);

    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    caps = pMlmReassocReq->capabilityInfo;
    if (PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap))
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
       According to WAPI standard:
       7.3.1.4 Capability Information field
       In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
       Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
       Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&frm.Capabilities );

    frm.ListenInterval.interval = pMlmReassocReq->listenInterval;

    // Get the old bssid of the older AP.
    vos_mem_copy( ( tANI_U8* )frm.CurrentAPAddress.mac,
            pMac->ft.ftPEContext.pFTPreAuthReq->currbssId, 6); 

    PopulateDot11fSSID2( pMac, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled) &&
        SIR_MAC_GET_QOS( psessionEntry->limReassocBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limReassocBssQosCaps );

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limReassocBssQosCaps );

    if ( psessionEntry->lim11hEnable  &&
            psessionEntry->pLimReAssocReq->spectrumMgtIndicator == eSIR_TRUE )
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;

        PopulateDot11fPowerCaps( pMac, &frm.PowerCaps, LIM_REASSOC,psessionEntry);
        PopulateDot11fSuppChannels( pMac, &frm.SuppChannels, LIM_REASSOC,psessionEntry);
#endif
    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &frm.PowerCaps, LIM_REASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
            ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap ) ))
    {
        PopulateDot11fQOSCapsStation( pMac, &frm.QOSCapsStation );
    }

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limReassocBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &frm.RRMEnabledCap, psessionEntry );       
    }
#endif

    // Ideally this should be enabled for 11r also. But 11r does
    // not follow the usual norm of using the Opaque object
    // for rsnie and fties. Instead we just add
    // the rsnie and fties at the end of the pack routine for 11r.
    // This should ideally! be fixed.
#if defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    //
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if (!psessionEntry->is11Rconnection)
    {
        if( nAddIELen && pAddIE )
        {
            wpsIe = limGetWscIEPtr(pMac, pAddIE, nAddIELen);
        }
        if ( NULL == wpsIe )
        {
            PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                    &frm.RSNOpaque );
            PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                    &frm.WPAOpaque );
        }

#ifdef FEATURE_WLAN_ESE
        if (psessionEntry->pLimReAssocReq->cckmIE.length)
        {
            PopulateDot11fESECckmOpaque( pMac, &( psessionEntry->pLimReAssocReq->cckmIE ),
                    &frm.ESECckmOpaque );
        }
#endif //FEATURE_WLAN_ESE
    }

#ifdef FEATURE_WLAN_ESE
    // For ESE Associations fill the ESE IEs
    if (psessionEntry->isESEconnection &&
        psessionEntry->pLimReAssocReq->isESEFeatureIniEnabled)
    {
#ifndef FEATURE_DISABLE_RM
        PopulateDot11fESERadMgmtCap(&frm.ESERadMgmtCap);
#endif
        PopulateDot11fESEVersion(&frm.ESEVersion);
    }
#endif //FEATURE_WLAN_ESE
#endif //FEATURE_WLAN_ESE || FEATURE_WLAN_LFR

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limReassocBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &frm.WMMInfoStation );
        }

        if ( fWsmEnabled &&
                ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limReassocBssPropCap )))
        {
            PopulateDot11fWMMCaps( &frm.WMMCaps );
        }
#ifdef FEATURE_WLAN_ESE
        if (psessionEntry->isESEconnection)
        {
            PopulateDot11fReAssocTspec(pMac, &frm, psessionEntry);

            // Populate the TSRS IE if TSPEC is included in the reassoc request
            if (psessionEntry->pLimReAssocReq->eseTspecInfo.numTspecs)
            {
                tANI_U32 phyMode;
                tSirMacESETSRSIE    tsrsIE;
                limGetPhyMode(pMac, &phyMode, psessionEntry);

                tsrsIE.tsid = 0;
                if( phyMode == WNI_CFG_PHY_MODE_11G || phyMode == WNI_CFG_PHY_MODE_11A)
                {
                    tsrsIE.rates[0] = TSRS_11AG_RATE_6MBPS;
                }
                else
                {
                    tsrsIE.rates[0] = TSRS_11B_RATE_5_5MBPS;
                }
                PopulateDot11TSRSIE(pMac,&tsrsIE, &frm.ESETrafStrmRateSet, sizeof(tANI_U8));
            }
        }
#endif
    }

    if ( psessionEntry->htCapability &&
            pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, psessionEntry, &frm.HTCaps );
    }

#if defined WLAN_FEATURE_VOWIFI_11R
    if ( psessionEntry->pLimReAssocReq->bssDescription.mdiePresent &&
         (pMac->ft.ftSmeContext.addMDIE == TRUE)
#if defined FEATURE_WLAN_ESE
           && !psessionEntry->isESEconnection
#endif
       )
    {
        PopulateMDIE( pMac, &frm.MobilityDomain, psessionEntry->pLimReAssocReq->bssDescription.mdie);
    }
#endif

#ifdef WLAN_FEATURE_11AC
    if ( psessionEntry->vhtCapability &&
             psessionEntry->vhtCapabilityPresentInBeacon)
    {
        limLog( pMac, LOG1, FL("Populate VHT IEs in Re-Assoc Request"));
        PopulateDot11fVHTCaps( pMac, &frm.VHTCaps, eSIR_FALSE );
    }
#endif
    PopulateDot11fExtCap( pMac, &frm.ExtCap, psessionEntry);

    nStatus = dot11fGetPackedReAssocRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                    "or a Re-Association Request (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fReAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                    "the packed size for a Re-Association Re "
                    "quest(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL("FT IE Reassoc Req (%d)."),
            pMac->ft.ftSmeContext.reassoc_ft_ies_length);
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->is11Rconnection)
    {
        ft_ies_length = pMac->ft.ftSmeContext.reassoc_ft_ies_length;
    }
#endif

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
            ( tANI_U16 )nBytes+ft_ies_length, ( void** ) &pFrame,
            ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Re-As"
                    "sociation Request."), nBytes );
        goto end;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes + ft_ies_length, 0);

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    limPrintMacAddr(pMac, psessionEntry->limReAssocbssId, LOG1);
#endif
    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
            SIR_MAC_MGMT_REASSOC_REQ,
            psessionEntry->limReAssocbssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                    "tor for an Association Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }

    pMacHdr = (tpSirMacMgmtHdr) pFrame;
    // That done, pack the ReAssoc Request:
    nStatus = dot11fPackReAssocRequest( pMac, &frm, pFrame +
            sizeof(tSirMacMgmtHdr),
            nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Re-Association Reque"
                    "st (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a R"
                               "e-Association Request (0x%08x)."), nStatus );
    }

    PELOG3(limLog( pMac, LOG3, 
            FL("*** Sending Re-Association Request length %d %d to "),
            nBytes, nPayload );)
    if( psessionEntry->assocReq != NULL )
    {
        vos_mem_free(psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    if( nAddIELen )
    {
        vos_mem_copy( pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
                      pAddIE,
                      nAddIELen );
        nPayload += nAddIELen;
    }

    psessionEntry->assocReq = vos_mem_malloc(nPayload);
    if ( NULL == psessionEntry->assocReq )
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        vos_mem_copy( psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if (psessionEntry->is11Rconnection)
    {
        {
            int i = 0;

            pBody = pFrame + nBytes;
            for (i=0; i<ft_ies_length; i++)
            {
                *pBody = pMac->ft.ftSmeContext.reassoc_ft_ies[i];
                pBody++;
            }
        }
    }

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog(pMac, LOG1, FL("Re-assoc Req Frame is: "));
            sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1,
                (tANI_U8 *)pFrame,
                (nBytes + ft_ies_length));)
#endif


    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }
 
    if( NULL != psessionEntry->assocReq )
    {
        vos_mem_free(psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    psessionEntry->assocReq = vos_mem_malloc(ft_ies_length);
    if ( NULL == psessionEntry->assocReq )
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
        psessionEntry->assocReqLen = 0;
    }
    else
    {
       //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
       vos_mem_copy( psessionEntry->assocReq, pMac->ft.ftSmeContext.reassoc_ft_ies,
                    (ft_ies_length));
       psessionEntry->assocReqLen = (ft_ies_length);
    }


    // Enable TL cahching in case of roaming
    WLANTL_EnableCaching(psessionEntry->staId);

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (nBytes + ft_ies_length),
            HAL_TXRX_FRM_802_11_MGMT,
            ANI_TXDIR_TODS,
            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Re-Association Request"
                    "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        goto end;
    }

end:
    // Free up buffer allocated for mlmAssocReq
    vos_mem_free( pMlmReassocReq );
    psessionEntry->pLimMlmReassocReq = NULL;

}

void limSendRetryReassocReqFrame(tpAniSirGlobal     pMac,
                                 tLimMlmReassocReq *pMlmReassocReq,
                                 tpPESession psessionEntry)
{
    tLimMlmReassocCnf       mlmReassocCnf; // keep sme
    tLimMlmReassocReq       *pTmpMlmReassocReq = NULL;
    if(NULL == pTmpMlmReassocReq)
    {
        pTmpMlmReassocReq = vos_mem_malloc(sizeof(tLimMlmReassocReq));
        if ( NULL == pTmpMlmReassocReq ) goto end;
        vos_mem_set( pTmpMlmReassocReq, sizeof(tLimMlmReassocReq), 0);
        vos_mem_copy( pTmpMlmReassocReq, pMlmReassocReq, sizeof(tLimMlmReassocReq));
    }

    // Prepare and send Reassociation request frame
    // start reassoc timer.
    pMac->lim.limTimers.gLimReassocFailureTimer.sessionId = psessionEntry->peSessionId;
    // Start reassociation failure timer
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId, eLIM_REASSOC_FAIL_TIMER));
    if (tx_timer_activate(&pMac->lim.limTimers.gLimReassocFailureTimer)
                                               != TX_SUCCESS)
    {
        // Could not start reassoc failure timer.
        // Log error
        limLog(pMac, LOGP,
           FL("could not start Reassociation failure timer"));
        // Return Reassoc confirm with
        // Resources Unavailable
        mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
        goto end;
    }

    limSendReassocReqWithFTIEsMgmtFrame(pMac, pTmpMlmReassocReq, psessionEntry);
    return;

end:
    // Free up buffer allocated for reassocReq
    if (pMlmReassocReq != NULL)
    {
        vos_mem_free(pMlmReassocReq);
        pMlmReassocReq = NULL;
    }
    if (pTmpMlmReassocReq != NULL)
    {
        vos_mem_free(pTmpMlmReassocReq);
        pTmpMlmReassocReq = NULL;
    }
    mlmReassocCnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
    mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
    /* Update PE sessio Id*/
    mlmReassocCnf.sessionId = psessionEntry->peSessionId;

    limPostSmeMessage(pMac, LIM_MLM_REASSOC_CNF, (tANI_U32 *) &mlmReassocCnf);
}

#endif /* WLAN_FEATURE_VOWIFI_11R */


void
limSendReassocReqMgmtFrame(tpAniSirGlobal     pMac,
                           tLimMlmReassocReq *pMlmReassocReq,tpPESession psessionEntry)
{
    static tDot11fReAssocRequest frm;
    tANI_U16              caps;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tANI_U32              nBytes, nPayload, nStatus;
    tANI_U8               fQosEnabled, fWmeEnabled, fWsmEnabled;
    void                 *pPacket;
    eHalStatus            halstatus;
    tANI_U16              nAddIELen; 
    tANI_U8               *pAddIE;
    tANI_U8               *wpsIe = NULL;
    tANI_U32              txFlag = 0;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8               PowerCapsPopulated = FALSE;
#endif
    tpSirMacMgmtHdr     pMacHdr;

    if(NULL == psessionEntry)
    {
        return;
    }

    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimReAssocReq)
    {
        return;
    }
    nAddIELen = psessionEntry->pLimReAssocReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimReAssocReq->addIEAssoc.addIEdata;
    
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    caps = pMlmReassocReq->capabilityInfo;
    if (PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap))
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
    According to WAPI standard:
    7.3.1.4 Capability Information field
    In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
    Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
    Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&frm.Capabilities );

    frm.ListenInterval.interval = pMlmReassocReq->listenInterval;

    vos_mem_copy(( tANI_U8* )frm.CurrentAPAddress.mac,
                 ( tANI_U8* )psessionEntry->bssId, 6 );

    PopulateDot11fSSID2( pMac, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                             &frm.SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled ) &&
        SIR_MAC_GET_QOS( psessionEntry->limReassocBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limReassocBssQosCaps );

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limReassocBssQosCaps );


    if ( psessionEntry->lim11hEnable  &&
         psessionEntry->pLimReAssocReq->spectrumMgtIndicator == eSIR_TRUE )
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;
        PopulateDot11fPowerCaps( pMac, &frm.PowerCaps, LIM_REASSOC,psessionEntry);
        PopulateDot11fSuppChannels( pMac, &frm.SuppChannels, LIM_REASSOC,psessionEntry);
#endif
    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
        SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &frm.PowerCaps, LIM_REASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
         ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap ) ))
    {
        PopulateDot11fQOSCapsStation( pMac, &frm.QOSCapsStation );
    }

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &frm.ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
        SIR_MAC_GET_RRM( psessionEntry->limReassocBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &frm.RRMEnabledCap, psessionEntry );       
    }
#endif
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if( nAddIELen && pAddIE )
    {
        wpsIe = limGetWscIEPtr(pMac, pAddIE, nAddIELen);
    }
    if ( NULL == wpsIe )
    {
        PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.RSNOpaque );
        PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.WPAOpaque );
#if defined(FEATURE_WLAN_WAPI)
        PopulateDot11fWAPIOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.WAPIOpaque );
#endif // defined(FEATURE_WLAN_WAPI)
    }

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limReassocBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &frm.WMMInfoStation );
        }

        if ( fWsmEnabled &&
             ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limReassocBssPropCap )))
        {
            PopulateDot11fWMMCaps( &frm.WMMCaps );
        }
    }

    if ( psessionEntry->htCapability &&
          pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, psessionEntry, &frm.HTCaps );
    }
#ifdef WLAN_FEATURE_11AC
    if ( psessionEntry->vhtCapability &&
             psessionEntry->vhtCapabilityPresentInBeacon)
    {
        limLog( pMac, LOG1, FL("Populate VHT IEs in Re-Assoc Request"));
        PopulateDot11fVHTCaps( pMac, &frm.VHTCaps, eSIR_FALSE );
        PopulateDot11fExtCap( pMac, &frm.ExtCap, psessionEntry);
    }
#endif

    nStatus = dot11fGetPackedReAssocRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Re-Association Request (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fReAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Re-Association Re "
                               "quest(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Re-As"
                               "sociation Request."), nBytes );
        goto end;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_REASSOC_REQ,
                                psessionEntry->limReAssocbssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Association Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }

    pMacHdr = (tpSirMacMgmtHdr) pFrame;
    // That done, pack the Probe Request:
    nStatus = dot11fPackReAssocRequest( pMac, &frm, pFrame +
                                        sizeof(tSirMacMgmtHdr),
                                        nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Re-Association Reque"
                               "st (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a R"
                               "e-Association Request (0x%08x)."), nStatus );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending Re-Association Request length %d"
                           "to "),
            nBytes );)

    if( psessionEntry->assocReq != NULL )
    {
        vos_mem_free(psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    if( nAddIELen )
    {
        vos_mem_copy( pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
                      pAddIE,
                      nAddIELen );
        nPayload += nAddIELen;
    }

    psessionEntry->assocReq = vos_mem_malloc(nPayload);
    if ( NULL == psessionEntry->assocReq )
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        vos_mem_copy(psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if(psessionEntry->pePersona == VOS_P2P_CLIENT_MODE)
    {
        txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));

    // enable caching
    WLANTL_EnableCaching(psessionEntry->staId);

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (sizeof(tSirMacMgmtHdr) + nPayload),
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Re-Association Request"
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        goto end;
    }

end:
    // Free up buffer allocated for mlmAssocReq
    vos_mem_free( pMlmReassocReq );
    psessionEntry->pLimMlmReassocReq = NULL;

} // limSendReassocReqMgmtFrame

/**
 * \brief Send an Authentication frame
 *
 *
 * \param pMac Pointer to Global MAC structure
 *
 * \param pAuthFrameBody Pointer to Authentication frame structure that need
 * to be sent
 *
 * \param peerMacAddr MAC address of the peer entity to which Authentication
 * frame is destined
 *
 * \param wepBit Indicates whether wep bit to be set in FC while sending
 * Authentication frame3
 *
 *
 * This function is called by limProcessMlmMessages().  Authentication frame
 * is formatted and sent when this function is called.
 *
 *
 */

void
limSendAuthMgmtFrame(tpAniSirGlobal pMac,
                     tpSirMacAuthFrameBody pAuthFrameBody,
                     tSirMacAddr           peerMacAddr,
                     tANI_U8               wepBit,
                     tpPESession           psessionEntry 
                                                       )
{
    tANI_U8            *pFrame, *pBody;
    tANI_U32            frameLen = 0, bodyLen = 0;
    tpSirMacMgmtHdr     pMacHdr;
    tANI_U16            i;
    void               *pPacket;
    eHalStatus          halstatus;
    tANI_U32            txFlag = 0;

    if(NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("Error: psession Entry is NULL"));
        return;
    }

    limLog(pMac, LOG1,
           FL("Sending Auth seq# %d status %d (%d) to "MAC_ADDRESS_STR),
           pAuthFrameBody->authTransactionSeqNumber,
           pAuthFrameBody->authStatusCode,
           (pAuthFrameBody->authStatusCode == eSIR_MAC_SUCCESS_STATUS),
            MAC_ADDR_ARRAY(peerMacAddr));
    if (wepBit == LIM_WEP_IN_FC)
    {
        /// Auth frame3 to be sent with encrypted framebody
        /**
         * Allocate buffer for Authenticaton frame of size equal
         * to management frame header length plus 2 bytes each for
         * auth algorithm number, transaction number, status code,
         * 128 bytes for challenge text and 4 bytes each for
         * IV & ICV.
         */

        frameLen = sizeof(tSirMacMgmtHdr) + LIM_ENCR_AUTH_BODY_LEN;

        bodyLen = LIM_ENCR_AUTH_BODY_LEN;
    } // if (wepBit == LIM_WEP_IN_FC)
    else
    {
        switch (pAuthFrameBody->authTransactionSeqNumber)
        {
            case SIR_MAC_AUTH_FRAME_1:
                /**
                 * Allocate buffer for Authenticaton frame of size
                 * equal to management frame header length plus 2 bytes
                 * each for auth algorithm number, transaction number
                 * and status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

#if defined WLAN_FEATURE_VOWIFI_11R
            if (pAuthFrameBody->authAlgoNumber == eSIR_FT_AUTH)
            {
                if (0 != pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length) 
                {
                    frameLen += pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length;
                    limLog(pMac, LOG3, FL("Auth frame, FTIES length added=%d"),
                    pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length);
                }
                else
                {
                    limLog(pMac, LOG3, FL("Auth frame, Does not contain "
                                          "FTIES!!!"));
                    frameLen += (2+SIR_MDIE_SIZE);
                }
            }
#endif
                break;

            case SIR_MAC_AUTH_FRAME_2:
                if ((pAuthFrameBody->authAlgoNumber == eSIR_OPEN_SYSTEM) ||
                    ((pAuthFrameBody->authAlgoNumber == eSIR_SHARED_KEY) &&
                     (pAuthFrameBody->authStatusCode != eSIR_MAC_SUCCESS_STATUS)))
                {
                    /**
                     * Allocate buffer for Authenticaton frame of size
                     * equal to management frame header length plus
                     * 2 bytes each for auth algorithm number,
                     * transaction number and status code.
                     */

                    frameLen = sizeof(tSirMacMgmtHdr) +
                               SIR_MAC_AUTH_CHALLENGE_OFFSET;
                    bodyLen = SIR_MAC_AUTH_CHALLENGE_OFFSET;
                }
                else
                {
                    // Shared Key algorithm with challenge text
                    // to be sent
                    /**
                     * Allocate buffer for Authenticaton frame of size
                     * equal to management frame header length plus
                     * 2 bytes each for auth algorithm number,
                     * transaction number, status code and 128 bytes
                     * for challenge text.
                     */

                    frameLen = sizeof(tSirMacMgmtHdr) +
                               sizeof(tSirMacAuthFrame);
                    bodyLen  = sizeof(tSirMacAuthFrameBody);
                }

                break;

            case SIR_MAC_AUTH_FRAME_3:
                /// Auth frame3 to be sent without encrypted framebody
                /**
                 * Allocate buffer for Authenticaton frame of size equal
                 * to management frame header length plus 2 bytes each
                 * for auth algorithm number, transaction number and
                 * status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

                break;

            case SIR_MAC_AUTH_FRAME_4:
                /**
                 * Allocate buffer for Authenticaton frame of size equal
                 * to management frame header length plus 2 bytes each
                 * for auth algorithm number, transaction number and
                 * status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

                break;
        } // switch (pAuthFrameBody->authTransactionSeqNumber)
    } // end if (wepBit == LIM_WEP_IN_FC)


    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )frameLen, ( void** ) &pFrame, ( void** ) &pPacket );

    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        // Log error
        limLog(pMac, LOGP, FL("call to bufAlloc failed for AUTH frame"));

        return;
    }

    for (i = 0; i < frameLen; i++)
        pFrame[i] = 0;

    // Prepare BD
    if (limPopulateMacHeader(pMac, pFrame, SIR_MAC_MGMT_FRAME,
                      SIR_MAC_MGMT_AUTH, peerMacAddr,psessionEntry->selfMacAddr) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGE, FL("call to limPopulateMacHeader failed for "
                              "AUTH frame"));
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    pMacHdr->fc.wep = wepBit;

    // Prepare BSSId
    if(  (psessionEntry->limSystemRole == eLIM_AP_ROLE)|| (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE) )
    {
        vos_mem_copy( (tANI_U8 *) pMacHdr->bssId,
                      (tANI_U8 *) psessionEntry->bssId,
                      sizeof( tSirMacAddr ));
    }

    /// Prepare Authentication frame body
    pBody    = pFrame + sizeof(tSirMacMgmtHdr);

    if (wepBit == LIM_WEP_IN_FC)
    {
        vos_mem_copy(pBody, (tANI_U8 *) pAuthFrameBody, bodyLen);

        PELOG1(limLog(pMac, LOG1,
           FL("*** Sending Auth seq# 3 status %d (%d) to"MAC_ADDRESS_STR),
           pAuthFrameBody->authStatusCode,
           (pAuthFrameBody->authStatusCode == eSIR_MAC_SUCCESS_STATUS),
           MAC_ADDR_ARRAY(pMacHdr->da));)

    }
    else
    {
        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authAlgoNumber);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);

        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authTransactionSeqNumber);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);

        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authStatusCode);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);
        if ( bodyLen <= (sizeof (pAuthFrameBody->type) +
                         sizeof (pAuthFrameBody->length) +
                         sizeof (pAuthFrameBody->challengeText)))
            vos_mem_copy(pBody, (tANI_U8 *) &pAuthFrameBody->type, bodyLen);

#if defined WLAN_FEATURE_VOWIFI_11R
        if ((pAuthFrameBody->authAlgoNumber == eSIR_FT_AUTH) && 
                (pAuthFrameBody->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_1))
        {

            {
                int i = 0;
                if (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length) 
                {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
                    PELOG2(limLog(pMac, LOG2, FL("Auth1 Frame FTIE is: "));
                        sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2,
                            (tANI_U8 *)pBody,
                            (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length));)
#endif
                    for (i=0; i<pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length; i++)
                    {
                        *pBody = pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies[i];
                        pBody++;
                    }
                }
                else
                { 
                    /* MDID attr is 54*/
                    *pBody = 54;
                    pBody++;
                    *pBody = SIR_MDIE_SIZE;
                    pBody++;
                    for(i=0;i<SIR_MDIE_SIZE;i++)
                    {
                      *pBody = pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription->mdie[i];
                       pBody++;
                    }
                }
            }
        }
#endif

        PELOG1(limLog(pMac, LOG1,
           FL("*** Sending Auth seq# %d status %d (%d) to "MAC_ADDRESS_STR),
           pAuthFrameBody->authTransactionSeqNumber,
           pAuthFrameBody->authStatusCode,
           (pAuthFrameBody->authStatusCode == eSIR_MAC_SUCCESS_STATUS),
           MAC_ADDR_ARRAY(pMacHdr->da));)
    }
    PELOG2(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2, pFrame, frameLen);)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
       || ((NULL != pMac->ft.ftPEContext.pFTPreAuthReq)
           && ( SIR_BAND_5_GHZ == limGetRFBand(pMac->ft.ftPEContext.pFTPreAuthReq->preAuthchannelNum)))
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if(psessionEntry->pePersona == VOS_P2P_CLIENT_MODE)
    {
        txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
    }

    limLog( pMac, LOG1, FL("Sending Auth Frame over WQ5 to "MAC_ADDRESS_STR
                   " From " MAC_ADDRESS_STR),MAC_ADDR_ARRAY(pMacHdr->da),
              MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));

    txFlag |= HAL_USE_FW_IN_TX_PATH;

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    /// Queue Authentication frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) frameLen,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("*** Could not send Auth frame, retCode=%X ***"),
               halstatus);

        //Pkt will be freed up by the callback
    }

    return;
} /*** end limSendAuthMgmtFrame() ***/

eHalStatus limSendDeauthCnf(tpAniSirGlobal pMac)
{
    tANI_U16                aid;
    tpDphHashNode           pStaDs;
    tLimMlmDeauthReq        *pMlmDeauthReq;
    tLimMlmDeauthCnf        mlmDeauthCnf;
    tpPESession             psessionEntry;

    pMlmDeauthReq = pMac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq;
    if (pMlmDeauthReq)
    {
        if (tx_timer_running(&pMac->lim.limTimers.gLimDeauthAckTimer))
        {
            limDeactivateAndChangeTimer(pMac, eLIM_DEAUTH_ACK_TIMER);
        }

        if((psessionEntry = peFindSessionBySessionId(pMac, pMlmDeauthReq->sessionId))== NULL)
        {

            PELOGE(limLog(pMac, LOGE,
                        FL("session does not exist for given sessionId"));)
                mlmDeauthCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }

        pStaDs = dphLookupHashEntry(pMac, pMlmDeauthReq->peerMacAddr, &aid, &psessionEntry->dph.dphHashTable);
        if (pStaDs == NULL)
        {
            mlmDeauthCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }

        /// Receive path cleanup with dummy packet
        limCleanupRxPath(pMac, pStaDs,psessionEntry);

#ifdef WLAN_FEATURE_VOWIFI_11R
        if  ( psessionEntry->limSystemRole == eLIM_STA_ROLE )
        {
            PELOGE(limLog(pMac, LOG1,
                   FL("FT Preauth SessionId %d Cleanup"
#ifdef FEATURE_WLAN_ESE
                   " isESE %d"
#endif
#ifdef FEATURE_WLAN_LFR
                   " isLFR %d"
#endif
                   " is11r %d, Deauth reason %d Trigger = %d"),
                   psessionEntry->peSessionId,
#ifdef FEATURE_WLAN_ESE
                   psessionEntry->isESEconnection,
#endif
#ifdef FEATURE_WLAN_LFR
                   psessionEntry->isFastRoamIniFeatureEnabled,
#endif
                   psessionEntry->is11Rconnection,
                   pMlmDeauthReq->reasonCode,
                   pMlmDeauthReq->deauthTrigger););

            limFTCleanup(pMac);
        }
#endif

        /// Free up buffer allocated for mlmDeauthReq
        vos_mem_free(pMlmDeauthReq);
        pMac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
    }
    return eHAL_STATUS_SUCCESS;
end:
    vos_mem_copy( (tANI_U8 *) &mlmDeauthCnf.peerMacAddr,
            (tANI_U8 *) pMlmDeauthReq->peerMacAddr,
            sizeof(tSirMacAddr));
    mlmDeauthCnf.deauthTrigger = pMlmDeauthReq->deauthTrigger;
    mlmDeauthCnf.aid           = pMlmDeauthReq->aid;
    mlmDeauthCnf.sessionId = pMlmDeauthReq->sessionId;

    // Free up buffer allocated
    // for mlmDeauthReq
    vos_mem_free(pMlmDeauthReq);

    limPostSmeMessage(pMac,
            LIM_MLM_DEAUTH_CNF,
            (tANI_U32 *) &mlmDeauthCnf);
    return eHAL_STATUS_SUCCESS;
}

eHalStatus limSendDisassocCnf(tpAniSirGlobal pMac)
{
    tANI_U16                 aid;
    tpDphHashNode            pStaDs;
    tLimMlmDisassocCnf       mlmDisassocCnf;
    tpPESession              psessionEntry;
    tLimMlmDisassocReq       *pMlmDisassocReq;

    pMlmDisassocReq = pMac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq;
    if (pMlmDisassocReq)
    {
        if (tx_timer_running(&pMac->lim.limTimers.gLimDisassocAckTimer))
        {
            limDeactivateAndChangeTimer(pMac, eLIM_DISASSOC_ACK_TIMER);
        }

        if((psessionEntry = peFindSessionBySessionId(pMac, pMlmDisassocReq->sessionId))== NULL)
        {

            PELOGE(limLog(pMac, LOGE,
                        FL("session does not exist for given sessionId"));)
                mlmDisassocCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }

        pStaDs = dphLookupHashEntry(pMac, pMlmDisassocReq->peerMacAddr, &aid, &psessionEntry->dph.dphHashTable);
        if (pStaDs == NULL)
        {
            mlmDisassocCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }

        /// Receive path cleanup with dummy packet
        if(eSIR_SUCCESS != limCleanupRxPath(pMac, pStaDs, psessionEntry))
        {
            mlmDisassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            goto end;
        }

#ifdef WLAN_FEATURE_VOWIFI_11R
        if  ( (psessionEntry->limSystemRole == eLIM_STA_ROLE ) && 
                (pMlmDisassocReq->reasonCode !=
                 eSIR_MAC_DISASSOC_DUE_TO_FTHANDOFF_REASON))
        {
            PELOGE(limLog(pMac, LOG1,
                   FL("FT Preauth SessionId %d Cleanup"
#ifdef FEATURE_WLAN_ESE
                   " isESE %d"
#endif
#ifdef FEATURE_WLAN_LFR
                   " isLFR %d"
#endif
                   " is11r %d reason %d"),
                   psessionEntry->peSessionId,
#ifdef FEATURE_WLAN_ESE
                   psessionEntry->isESEconnection,
#endif
#ifdef FEATURE_WLAN_LFR
                   psessionEntry->isFastRoamIniFeatureEnabled,
#endif
                   psessionEntry->is11Rconnection,
                   pMlmDisassocReq->reasonCode););
            limFTCleanup(pMac);
        }
#endif

        /// Free up buffer allocated for mlmDisassocReq
        vos_mem_free(pMlmDisassocReq);
        pMac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
        return eHAL_STATUS_SUCCESS;
    }
    else
    {
        return eHAL_STATUS_SUCCESS;
    }
end:
    vos_mem_copy( (tANI_U8 *) &mlmDisassocCnf.peerMacAddr,
            (tANI_U8 *) pMlmDisassocReq->peerMacAddr,
            sizeof(tSirMacAddr));
    mlmDisassocCnf.aid = pMlmDisassocReq->aid;
    mlmDisassocCnf.disassocTrigger = pMlmDisassocReq->disassocTrigger;

    /* Update PE session ID*/
    mlmDisassocCnf.sessionId = pMlmDisassocReq->sessionId;

    if(pMlmDisassocReq != NULL)
    {
        /// Free up buffer allocated for mlmDisassocReq
        vos_mem_free(pMlmDisassocReq);
        pMac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
    }

    limPostSmeMessage(pMac,
            LIM_MLM_DISASSOC_CNF,
            (tANI_U32 *) &mlmDisassocCnf);
    return eHAL_STATUS_SUCCESS;
}

eHalStatus limDisassocTxCompleteCnf(tpAniSirGlobal pMac, tANI_U32 txCompleteSuccess)
{
    return limSendDisassocCnf(pMac);
}

eHalStatus limDeauthTxCompleteCnf(tpAniSirGlobal pMac, tANI_U32 txCompleteSuccess)
{
    return limSendDeauthCnf(pMac);
}

/**
 * \brief This function is called to send Disassociate frame.
 *
 *
 * \param pMac Pointer to Global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in
 * Disassociation frame
 *
 * \param peerMacAddr MAC address of the STA to which Disassociation frame is
 * sent
 *
 *
 */

void
limSendDisassocMgmtFrame(tpAniSirGlobal pMac,
                         tANI_U16       nReason,
                         tSirMacAddr    peer,
                         tpPESession psessionEntry,
                         tANI_BOOLEAN waitForAck)
{
    tDot11fDisassociation frm;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tpSirMacMgmtHdr       pMacHdr;
    tANI_U32              nBytes, nPayload, nStatus;
    void                 *pPacket;
    eHalStatus            halstatus;
    tANI_U32              txFlag = 0;
    tANI_U32              val = 0;
    if(NULL == psessionEntry)
    {
        return;
    }
    
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0);

    frm.Reason.code = nReason;

    nStatus = dot11fGetPackedDisassociationSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Disassociation (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDisassociation );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Disassociation "
                               "(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Dis"
                               "association."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_DISASSOC, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Disassociation (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    // Prepare the BSSID
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);
    
#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackDisassociation( pMac, &frm, pFrame +
                                        sizeof(tSirMacMgmtHdr),
                                        nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Disassociation (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a D"
                               "isassociation (0x%08x)."), nStatus );
    }

    limLog( pMac, LOG1, FL("***Sessionid %d Sending Disassociation frame with "
          "reason %u and waitForAck %d to "MAC_ADDRESS_STR" ,From "
          MAC_ADDRESS_STR), psessionEntry->peSessionId, nReason, waitForAck,
          MAC_ADDR_ARRAY(pMacHdr->da),
          MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if((psessionEntry->pePersona == VOS_P2P_CLIENT_MODE) ||
       (psessionEntry->pePersona == VOS_P2P_GO_MODE) ||
       (psessionEntry->pePersona == VOS_STA_SAP_MODE))
    {
        txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
    }

    if( IS_FW_IN_TX_PATH_FEATURE_ENABLE )
    {
        /* This frame will be sent on air by firmware,
           which will ensure that this frame goes out
           even though DEL_STA is sent immediately */
        /* Without this for DEL_STA command there is
           risk of flushing frame in BTQM queue without
           sending on air */
        limLog( pMac, LOG1, FL("Sending Disassoc Frame over WQ5 to "MAC_ADDRESS_STR
                " From " MAC_ADDRESS_STR),MAC_ADDR_ARRAY(pMacHdr->da),
              MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));
        txFlag |= HAL_USE_FW_IN_TX_PATH;
    }

    if (waitForAck)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
               psessionEntry->peSessionId,
               pMacHdr->fc.subType));
        // Queue Disassociation frame in high priority WQ
        /* get the duration from the request */
        halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                HAL_TXRX_FRM_802_11_MGMT,
                ANI_TXDIR_TODS,
                7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                limTxComplete, pFrame, limDisassocTxCompleteCnf,
                txFlag );
        MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
               psessionEntry->peSessionId,
               halstatus));
        val = SYS_MS_TO_TICKS(LIM_DISASSOC_DEAUTH_ACK_TIMEOUT);

        if (tx_timer_change(
                    &pMac->lim.limTimers.gLimDisassocAckTimer, val, 0)
                != TX_SUCCESS)
        {
            limLog(pMac, LOGP,
                    FL("Unable to change Disassoc ack Timer val"));
            return;
        }
        else if(TX_SUCCESS != tx_timer_activate(
                    &pMac->lim.limTimers.gLimDisassocAckTimer))
        {
            limLog(pMac, LOGP,
                    FL("Unable to activate Disassoc ack Timer"));
            limDeactivateAndChangeTimer(pMac, eLIM_DISASSOC_ACK_TIMER);
            return;
        }
    }
    else 
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
               psessionEntry->peSessionId,
               pMacHdr->fc.subType));
        // Queue Disassociation frame in high priority WQ
        halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                HAL_TXRX_FRM_802_11_MGMT,
                ANI_TXDIR_TODS,
                7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                limTxComplete, pFrame, txFlag );
        MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
               psessionEntry->peSessionId,
               halstatus));
        if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to send Disassociation "
                        "(%X)!"),
                    nSirStatus );
            //Pkt will be freed up by the callback
            return;
        }
    }
} // End limSendDisassocMgmtFrame.

/**
 * \brief This function is called to send a Deauthenticate frame
 *
 *
 * \param pMac Pointer to global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in the
 * Deauthenticate frame
 *
 * \param peeer address of the STA to which the frame is to be sent
 *
 *
 */

void
limSendDeauthMgmtFrame(tpAniSirGlobal pMac,
                       tANI_U16       nReason,
                       tSirMacAddr    peer,
                       tpPESession psessionEntry,
                       tANI_BOOLEAN waitForAck)
{
    tDot11fDeAuth    frm;
    tANI_U8         *pFrame;
    tSirRetStatus    nSirStatus;
    tpSirMacMgmtHdr  pMacHdr;
    tANI_U32         nBytes, nPayload, nStatus;
    void            *pPacket;
    eHalStatus       halstatus;
    tANI_U32         txFlag = 0;
    tANI_U32         val = 0;
#ifdef FEATURE_WLAN_TDLS
    tANI_U16          aid;
    tpDphHashNode     pStaDs;
#endif

    if(NULL == psessionEntry)
    {
        return;
    }
    
    vos_mem_set( ( tANI_U8* ) &frm, sizeof( frm ), 0 );

    frm.Reason.code = nReason;

    nStatus = dot11fGetPackedDeAuthSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a De-Authentication (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDeAuth );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a De-Authentication "
                               "(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a De-"
                               "Authentication."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set(  pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_DEAUTH, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a De-Authentication (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    // Prepare the BSSID
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackDeAuth( pMac, &frm, pFrame +
                                sizeof(tSirMacMgmtHdr),
                                nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a DeAuthentication (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a D"
                               "e-Authentication (0x%08x)."), nStatus );
    }
     limLog( pMac, LOG1, FL("***Sessionid %d Sending Deauth frame with "
          "reason %u and waitForAck %d to "MAC_ADDRESS_STR" ,From "
          MAC_ADDRESS_STR), psessionEntry->peSessionId, nReason, waitForAck,
          MAC_ADDR_ARRAY(pMacHdr->da),
          MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if((psessionEntry->pePersona == VOS_P2P_CLIENT_MODE) ||
       (psessionEntry->pePersona == VOS_P2P_GO_MODE) ||
       (psessionEntry->pePersona == VOS_STA_SAP_MODE))
    {
        txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
    }

    if( IS_FW_IN_TX_PATH_FEATURE_ENABLE )
    {
        /* This frame will be sent on air by firmware,
           which will ensure that this frame goes out
           even though DEL_STA is sent immediately */
        /* Without this for DEL_STA command there is
           risk of flushing frame in BTQM queue without
           sending on air */
        limLog( pMac, LOG1, FL("Sending Deauth Frame over WQ5 to "MAC_ADDRESS_STR
               " From " MAC_ADDRESS_STR),MAC_ADDR_ARRAY(pMacHdr->da),
              MAC_ADDR_ARRAY(psessionEntry->selfMacAddr));
        txFlag |= HAL_USE_FW_IN_TX_PATH;
    }

#ifdef FEATURE_WLAN_TDLS
    pStaDs = dphLookupHashEntry(pMac, peer, &aid, &psessionEntry->dph.dphHashTable);
#endif

    if (waitForAck)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
               psessionEntry->peSessionId,
               pMacHdr->fc.subType));
        // Queue Disassociation frame in high priority WQ
        halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                HAL_TXRX_FRM_802_11_MGMT,
                ANI_TXDIR_TODS,
                7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                limTxComplete, pFrame, limDeauthTxCompleteCnf, txFlag );
        MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
               psessionEntry->peSessionId,
               halstatus));
        if (!HAL_STATUS_SUCCESS(halstatus))
        {
            limLog( pMac, LOGE, FL("Failed to send De-Authentication "
                    "(%X)!"),
                    nSirStatus );
            //Pkt will be freed up by the callback limTxComplete

            /*Call limProcessDeauthAckTimeout which will send
            * DeauthCnf for this frame
            */
            limProcessDeauthAckTimeout(pMac);
            return;
        }

        val = SYS_MS_TO_TICKS(LIM_DISASSOC_DEAUTH_ACK_TIMEOUT);

        if (tx_timer_change(
                    &pMac->lim.limTimers.gLimDeauthAckTimer, val, 0)
                != TX_SUCCESS)
        {
            limLog(pMac, LOGP,
                    FL("Unable to change Deauth ack Timer val"));
            return;
        }
        else if(TX_SUCCESS != tx_timer_activate(
                    &pMac->lim.limTimers.gLimDeauthAckTimer))
        {
            limLog(pMac, LOGP,
                    FL("Unable to activate Deauth ack Timer"));
            limDeactivateAndChangeTimer(pMac, eLIM_DEAUTH_ACK_TIMER);
            return;
        }
    }
    else
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
               psessionEntry->peSessionId,
               pMacHdr->fc.subType));
#ifdef FEATURE_WLAN_TDLS
        if ((NULL != pStaDs) && (STA_ENTRY_TDLS_PEER == pStaDs->staType))
        {
            // Queue Disassociation frame in high priority WQ
            halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                HAL_TXRX_FRM_802_11_MGMT,
                ANI_TXDIR_IBSS,
                7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                limTxComplete, pFrame, txFlag );
        }
        else
        {
#endif
            // Queue Disassociation frame in high priority WQ
            halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                    HAL_TXRX_FRM_802_11_MGMT,
                    ANI_TXDIR_TODS,
                    7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                    limTxComplete, pFrame, txFlag );
#ifdef FEATURE_WLAN_TDLS
        }
#endif
        MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
               psessionEntry->peSessionId,
               halstatus));
        if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to send De-Authentication "
                        "(%X)!"),
                    nSirStatus );
            //Pkt will be freed up by the callback
            return;
        }
    }

} // End limSendDeauthMgmtFrame.


#ifdef ANI_SUPPORT_11H
/**
 * \brief Send a Measurement Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pMeasReqFrame Address of a tSirMacMeasReqActionFrame
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendMeasReportFrame(tpAniSirGlobal             pMac,
                       tpSirMacMeasReqActionFrame pMeasReqFrame,
                       tSirMacAddr                peer)
{
    tDot11fMeasurementReport  frm;
    tANI_U8                  *pFrame;
    tSirRetStatus             nSirStatus;
    tpSirMacMgmtHdr           pMacHdr;
    tANI_U32                  nBytes, nPayload, nStatus, nCfg;
    void                     *pPacket;
    eHalStatus                halstatus;
   
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    frm.Category.category = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action     = SIR_MAC_ACTION_MEASURE_REPORT_ID;
    frm.DialogToken.token = pMeasReqFrame->actionHeader.dialogToken;

    switch ( pMeasReqFrame->measReqIE.measType )
    {
    case SIR_MAC_BASIC_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport0( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    case SIR_MAC_CCA_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport1( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    case SIR_MAC_RPI_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport2( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    default:
        limLog( pMac, LOGE, FL("Unknown measurement type %d in limSen"
                               "dMeasReportFrame."),
                pMeasReqFrame->measReqIE.measType );
        return eSIR_FAILURE;
    }

    if ( eSIR_SUCCESS != nSirStatus ) return eSIR_FAILURE;

    nStatus = dot11fGetPackedMeasurementReportSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Measurement Report (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fMeasurementReport );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Measurement Rep"
                               "ort (0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a De-"
                               "Authentication."), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Measurement Report (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackMeasurementReport( pMac, &frm, pFrame +
                                           sizeof(tSirMacMgmtHdr),
                                           nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Measurement Report (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a M"
                               "easurement Report (0x%08x)."), nStatus );
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Measurement Report  "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;    // just allocated...
    }

    return eSIR_SUCCESS;

} // End limSendMeasReportFrame.


/**
 * \brief Send a TPC Request Action frame
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which the frame should be sent
 *
 *
 */

void
limSendTpcRequestFrame(tpAniSirGlobal pMac,
                       tSirMacAddr    peer)
{
    tDot11fTPCRequest  frm;
    tANI_U8           *pFrame;
    tSirRetStatus      nSirStatus;
    tpSirMacMgmtHdr    pMacHdr;
    tANI_U32           nBytes, nPayload, nStatus, nCfg;
    void              *pPacket;
    eHalStatus         halstatus;
   
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    frm.Category.category  = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action      = SIR_MAC_ACTION_TPC_REQUEST_ID;
    frm.DialogToken.token  = 1;
    frm.TPCRequest.present = 1;

    nStatus = dot11fGetPackedTPCRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a TPC Request (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fTPCRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a TPC Request (0x"
                               "%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Request."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set(pFrame, nBytes,0);

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a TPC Request (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackTPCRequest( pMac, &frm, pFrame +
                                    sizeof(tSirMacMgmtHdr),
                                    nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a TPC Request (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a T"
                               "PC Request (0x%08x)."), nStatus );
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a TPC Request "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return;
    }

} // End limSendTpcRequestFrame.


/**
 * \brief Send a TPC Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param pTpcReqFrame Pointer to the received TPC Request
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendTpcReportFrame(tpAniSirGlobal            pMac,
                      tpSirMacTpcReqActionFrame pTpcReqFrame,
                      tSirMacAddr               peer)
{
    tDot11fTPCReport  frm;
    tANI_U8          *pFrame;
    tSirRetStatus     nSirStatus;
    tpSirMacMgmtHdr   pMacHdr;
    tANI_U32          nBytes, nPayload, nStatus, nCfg;
    void             *pPacket;
    eHalStatus        halstatus;
   
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    frm.Category.category  = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action      = SIR_MAC_ACTION_TPC_REPORT_ID;
    frm.DialogToken.token  = pTpcReqFrame->actionHeader.dialogToken;

    // FramesToDo: On the Gen4_TVM branch, there was a comment:
    // "misplaced this function, need to replace:
    // txPower = halGetRateToPwrValue(pMac, staid,
    //     pMac->lim.gLimCurrentChannelId, 0);
    frm.TPCReport.tx_power    = 0;
    frm.TPCReport.link_margin = 0;
    frm.TPCReport.present     = 1;

    nStatus = dot11fGetPackedTPCReportSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a TPC Report (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fTPCReport );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a TPC Report (0x"
                               "%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Report."), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a TPC Report (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackTPCReport( pMac, &frm, pFrame +
                                   sizeof(tSirMacMgmtHdr),
                                   nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a TPC Report (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a T"
                               "PC Report (0x%08x)."), nStatus );
    }


    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           ((psessionEntry)? psessionEntry->peSessionId : NO_SESSION),
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a TPC Report "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;    // just allocated...
    }

    return eSIR_SUCCESS;

} // End limSendTpcReportFrame.
#endif  //ANI_SUPPORT_11H


/**
 * \brief Send a Channel Switch Announcement
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which this frame will be sent
 *
 * \param nMode
 *
 * \param nNewChannel
 *
 * \param nCount
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendChannelSwitchMgmtFrame(tpAniSirGlobal pMac,
                              tSirMacAddr    peer,
                              tANI_U8        nMode,
                              tANI_U8        nNewChannel,
                              tANI_U8        nCount,
                              tpPESession    psessionEntry )
{
    tDot11fChannelSwitch   frm;
    tANI_U8                *pFrame;
    tSirRetStatus          nSirStatus;
    tpSirMacMgmtHdr        pMacHdr;
    tANI_U32               nBytes, nPayload, nStatus;//, nCfg;
    void                   *pPacket;
    eHalStatus             halstatus;
    tANI_U32               txFlag = 0;
    
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    frm.Category.category     = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action         = SIR_MAC_ACTION_CHANNEL_SWITCH_ID;
    frm.ChanSwitchAnn.switchMode    = nMode;
    frm.ChanSwitchAnn.newChannel    = nNewChannel;
    frm.ChanSwitchAnn.switchCount   = nCount;
    frm.ChanSwitchAnn.present = 1;

    nStatus = dot11fGetPackedChannelSwitchSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Channel Switch (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fChannelSwitch );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Channel Switch (0x"
                               "%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Report."), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer, psessionEntry->selfMacAddr);
    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    vos_mem_copy( (tANI_U8 *) pMacHdr->bssId,
                  (tANI_U8 *) psessionEntry->bssId,
                  sizeof( tSirMacAddr ));
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Channel Switch (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

#if 0
    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }
#endif

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

    nStatus = dot11fPackChannelSwitch( pMac, &frm, pFrame +
                                       sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Channel Switch (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a C"
                               "hannel Switch (0x%08x)."), nStatus );
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Channel Switch "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

} // End limSendChannelSwitchMgmtFrame.



#ifdef WLAN_FEATURE_11AC    
tSirRetStatus
limSendVHTOpmodeNotificationFrame(tpAniSirGlobal pMac,
                              tSirMacAddr    peer,
                              tANI_U8        nMode,
                              tpPESession    psessionEntry )
{
    tDot11fOperatingMode   frm;
    tANI_U8                *pFrame;
    tSirRetStatus          nSirStatus;
    tpSirMacMgmtHdr        pMacHdr;
    tANI_U32               nBytes, nPayload = 0, nStatus;//, nCfg;
    void                   *pPacket;
    eHalStatus             halstatus;
    tANI_U32               txFlag = 0;
    
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

    frm.Category.category     = SIR_MAC_ACTION_VHT;
    frm.Action.action         = SIR_MAC_VHT_OPMODE_NOTIFICATION;
    frm.OperatingMode.chanWidth    = nMode;
    frm.OperatingMode.rxNSS   = 0;
    frm.OperatingMode.rxNSSType    = 0;

    nStatus = dot11fGetPackedOperatingModeSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Operating Mode (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fOperatingMode);
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Operating Mode (0x"
                               "%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Operating Mode"
                               " Report."), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );


    // Next, we fill out the buffer descriptor:
    if(psessionEntry->pePersona == VOS_STA_SAP_MODE) {
        nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                           SIR_MAC_MGMT_ACTION, peer, psessionEntry->selfMacAddr);
    } else
        nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                           SIR_MAC_MGMT_ACTION, psessionEntry->bssId, psessionEntry->selfMacAddr);
    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    vos_mem_copy( (tANI_U8 *) pMacHdr->bssId,
                  (tANI_U8 *) psessionEntry->bssId,
                  sizeof( tSirMacAddr ));
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Operating Mode (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }
    nStatus = dot11fPackOperatingMode( pMac, &frm, pFrame +
                                       sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Operating Mode (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a Operating Mode"
                               " (0x%08x)."), nStatus );
    }
    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Channel Switch "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
}

/**
 * \brief Send a VHT Channel Switch Announcement
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which this frame will be sent
 *
 * \param nChanWidth
 *
 * \param nNewChannel
 *
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendVHTChannelSwitchMgmtFrame(tpAniSirGlobal pMac,
                              tSirMacAddr    peer,
                              tANI_U8        nChanWidth,
                              tANI_U8        nNewChannel,
                              tANI_U8        ncbMode,
                              tpPESession    psessionEntry )
{
    tDot11fChannelSwitch   frm;
    tANI_U8                *pFrame;
    tSirRetStatus          nSirStatus;
    tpSirMacMgmtHdr        pMacHdr;
    tANI_U32               nBytes, nPayload, nStatus;//, nCfg;
    void                   *pPacket;
    eHalStatus             halstatus;
    tANI_U32               txFlag = 0;
    
    vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );
                

    frm.Category.category     = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action         = SIR_MAC_ACTION_CHANNEL_SWITCH_ID;
    frm.ChanSwitchAnn.switchMode    = 1;
    frm.ChanSwitchAnn.newChannel    = nNewChannel;
    frm.ChanSwitchAnn.switchCount   = 1;
    frm.ExtChanSwitchAnn.secondaryChannelOffset =  limGetHTCBState(ncbMode); 
    frm.ExtChanSwitchAnn.present = 1; 
    frm.WiderBWChanSwitchAnn.newChanWidth = nChanWidth;
    frm.WiderBWChanSwitchAnn.newCenterChanFreq0 = limGetCenterChannel(pMac,nNewChannel,ncbMode,nChanWidth);
    frm.WiderBWChanSwitchAnn.newCenterChanFreq1 = 0;
    frm.ChanSwitchAnn.present = 1;
    frm.WiderBWChanSwitchAnn.present = 1;

    nStatus = dot11fGetPackedChannelSwitchSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Channel Switch (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fChannelSwitch );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating "
                               "the packed size for a Channel Switch (0x"
                               "%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Report."), nBytes );
        return eSIR_FAILURE;
    }
   // Paranoia:
    vos_mem_set( pFrame, nBytes, 0 );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer, psessionEntry->selfMacAddr);
    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    vos_mem_copy( (tANI_U8 *) pMacHdr->bssId,
                  (tANI_U8 *) psessionEntry->bssId,
                  sizeof( tSirMacAddr ));
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Channel Switch (%d)."),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }
    nStatus = dot11fPackChannelSwitch( pMac, &frm, pFrame +
                                       sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Channel Switch (0x%08x)."),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a C"
                               "hannel Switch (0x%08x)."), nStatus );
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halstatus));
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Channel Switch "
                               "(%X)!"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

} // End limSendVHTChannelSwitchMgmtFrame.
    
    

#endif

/**
 * \brief Send an ADDBA Req Action Frame to peer
 *
 * \sa limSendAddBAReq
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMlmAddBAReq A pointer to tLimMlmAddBAReq. This contains
 * the necessary parameters reqd by PE send the ADDBA Req Action
 * Frame to the peer
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendAddBAReq( tpAniSirGlobal pMac,
    tpLimMlmAddBAReq pMlmAddBAReq, tpPESession psessionEntry)
{
    tDot11fAddBAReq   frmAddBAReq;
    tANI_U8           *pAddBAReqBuffer = NULL;
    tpSirMacMgmtHdr   pMacHdr;
    tANI_U32          frameLen = 0, nStatus, nPayload;
    tSirRetStatus     statusCode;
    eHalStatus        halStatus;
    void              *pPacket;
    tANI_U32          txFlag = 0;

     if(NULL == psessionEntry)
    {
        return eSIR_FAILURE;
    }

    vos_mem_set( (void *) &frmAddBAReq, sizeof( frmAddBAReq ), 0);

    // Category - 3 (BA)
    frmAddBAReq.Category.category = SIR_MAC_ACTION_BLKACK;
    
    // Action - 0 (ADDBA Req)
    frmAddBAReq.Action.action = SIR_MAC_BLKACK_ADD_REQ;

    // FIXME - Dialog Token, generalize this...
    frmAddBAReq.DialogToken.token = pMlmAddBAReq->baDialogToken;

    // Fill the ADDBA Parameter Set
    frmAddBAReq.AddBAParameterSet.tid = pMlmAddBAReq->baTID;
    frmAddBAReq.AddBAParameterSet.policy = pMlmAddBAReq->baPolicy;
    frmAddBAReq.AddBAParameterSet.bufferSize = pMlmAddBAReq->baBufferSize;

    // BA timeout
    // 0 - indicates no BA timeout
    frmAddBAReq.BATimeout.timeout = pMlmAddBAReq->baTimeout;

    /* Send SSN whatever we get from FW.
     */
    frmAddBAReq.BAStartingSequenceControl.ssn = pMlmAddBAReq->baSSN;

    nStatus = dot11fGetPackedAddBAReqSize( pMac, &frmAddBAReq, &nPayload );

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGW,
        FL( "Failed to calculate the packed size for "
          "an ADDBA Request (0x%08x)."),
        nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAddBAReq );
    }
    else if( DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
        FL( "There were warnings while calculating "
          "the packed size for an ADDBA Req (0x%08x)."),
        nStatus );
    }

    // Add the MGMT header to frame length
    frameLen = nPayload + sizeof( tSirMacMgmtHdr );

    // Need to allocate a buffer for ADDBA AF
    if( eHAL_STATUS_SUCCESS !=
      (halStatus = palPktAlloc( pMac->hHdd,
                                HAL_TXRX_FRM_802_11_MGMT,
                                (tANI_U16) frameLen,
                                (void **) &pAddBAReqBuffer,
                                (void **) &pPacket )))
    {
        // Log error
        limLog( pMac, LOGP,
        FL("palPktAlloc FAILED! Length [%d], Status [%d]"),
        frameLen,
        halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
    }

    vos_mem_set( (void *) pAddBAReqBuffer, frameLen, 0 );

    // Copy necessary info to BD
    if( eSIR_SUCCESS !=
      (statusCode = limPopulateMacHeader( pMac,
                                   pAddBAReqBuffer,
                                   SIR_MAC_MGMT_FRAME,
                                   SIR_MAC_MGMT_ACTION,
                                   pMlmAddBAReq->peerMacAddr,psessionEntry->selfMacAddr)))
    goto returnAfterError;

    // Update A3 with the BSSID
    pMacHdr = ( tpSirMacMgmtHdr ) pAddBAReqBuffer;
    
    #if 0
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if( eSIR_SUCCESS != cfgGetStr( pMac,
        WNI_CFG_BSSID,
        (tANI_U8 *) pMacHdr->bssId,
        &cfgLen ))
    {
        limLog( pMac, LOGP,
        FL( "Failed to retrieve WNI_CFG_BSSID while"
          "sending an ACTION Frame" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
    }
    #endif//TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
    limSetProtectedBit(pMac, psessionEntry, pMlmAddBAReq->peerMacAddr, pMacHdr);
#endif

    // Now, we're ready to "pack" the frames
    nStatus = dot11fPackAddBAReq( pMac,
      &frmAddBAReq,
      pAddBAReqBuffer + sizeof( tSirMacMgmtHdr ),
      nPayload,
      &nPayload );

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGE,
        FL( "Failed to pack an ADDBA Req (0x%08x)." ),
        nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
    }
    else if( DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
                FL( "There were warnings while packing an ADDBA Req (0x%08x)."),
                nStatus );
    }

    limLog( pMac, LOG1, FL( "Sending an ADDBA REQ to "MAC_ADDRESS_STR " with"
                            " tid = %d policy = %d buffsize = %d "
                            " amsduSupported = %d"),
                            MAC_ADDR_ARRAY(pMlmAddBAReq->peerMacAddr),
                            frmAddBAReq.AddBAParameterSet.tid,
                            frmAddBAReq.AddBAParameterSet.policy,
                            frmAddBAReq.AddBAParameterSet.bufferSize,
                            frmAddBAReq.AddBAParameterSet.amsduSupported);

    limLog( pMac, LOG1, FL( "ssn = %d fragNum = %d" ),
                          frmAddBAReq.BAStartingSequenceControl.ssn,
                          frmAddBAReq.BAStartingSequenceControl.fragNumber);

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halStatus = halTxFrame( pMac,
                            pPacket,
                            (tANI_U16) frameLen,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete,
                            pAddBAReqBuffer, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halStatus));
    if( eHAL_STATUS_SUCCESS != halStatus )
    {
        limLog( pMac, LOGE,
        FL( "halTxFrame FAILED! Status [%d]"),
        halStatus );

    // FIXME - Need to convert eHalStatus to tSirRetStatus
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

returnAfterError:

  // Release buffer, if allocated
  if( NULL != pAddBAReqBuffer )
    palPktFree( pMac->hHdd,
        HAL_TXRX_FRM_802_11_MGMT,
        (void *) pAddBAReqBuffer,
        (void *) pPacket );

  return statusCode;
}

/**
 * \brief Send an ADDBA Rsp Action Frame to peer
 *
 * \sa limSendAddBARsp
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMlmAddBARsp A pointer to tLimMlmAddBARsp. This contains
 * the necessary parameters reqd by PE send the ADDBA Rsp Action
 * Frame to the peer
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendAddBARsp( tpAniSirGlobal pMac,
    tpLimMlmAddBARsp pMlmAddBARsp,
    tpPESession      psessionEntry)
{
    tDot11fAddBARsp   frmAddBARsp;
    tANI_U8          *pAddBARspBuffer = NULL;
    tpSirMacMgmtHdr   pMacHdr;
    tANI_U32          frameLen = 0, nStatus, nPayload;
    tSirRetStatus     statusCode;
    eHalStatus        halStatus;
    void             *pPacket;
    tANI_U32          txFlag = 0;

     if(NULL == psessionEntry)
    {
        PELOGE(limLog(pMac, LOGE, FL("Session entry is NULL!!!"));)
        return eSIR_FAILURE;
    }

      vos_mem_set( (void *) &frmAddBARsp, sizeof( frmAddBARsp ), 0);

      // Category - 3 (BA)
      frmAddBARsp.Category.category = SIR_MAC_ACTION_BLKACK;
      // Action - 1 (ADDBA Rsp)
      frmAddBARsp.Action.action = SIR_MAC_BLKACK_ADD_RSP;

      // Should be same as the one we received in the ADDBA Req
      frmAddBARsp.DialogToken.token = pMlmAddBARsp->baDialogToken;

      // ADDBA Req status
      frmAddBARsp.Status.status = pMlmAddBARsp->addBAResultCode;

      // Fill the ADDBA Parameter Set as provided by caller
      frmAddBARsp.AddBAParameterSet.tid = pMlmAddBARsp->baTID;
      frmAddBARsp.AddBAParameterSet.policy = pMlmAddBARsp->baPolicy;
      frmAddBARsp.AddBAParameterSet.bufferSize = pMlmAddBARsp->baBufferSize;

      if(psessionEntry->isAmsduSupportInAMPDU)
      {
         frmAddBARsp.AddBAParameterSet.amsduSupported =
                                          psessionEntry->amsduSupportedInBA;
      }
      else
      {
         frmAddBARsp.AddBAParameterSet.amsduSupported = 0;
      }

      // BA timeout
      // 0 - indicates no BA timeout
      frmAddBARsp.BATimeout.timeout = pMlmAddBARsp->baTimeout;

      nStatus = dot11fGetPackedAddBARspSize( pMac, &frmAddBARsp, &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "Failed to calculate the packed size for "
              "an ADDBA Response (0x%08x)."),
            nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAddBARsp );
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while calculating "
              "the packed size for an ADDBA Rsp (0x%08x)."),
            nStatus );
      }

      // Need to allocate a buffer for ADDBA AF
      frameLen = nPayload + sizeof( tSirMacMgmtHdr );

      // Allocate shared memory
      if( eHAL_STATUS_SUCCESS !=
          (halStatus = palPktAlloc( pMac->hHdd,
                                    HAL_TXRX_FRM_802_11_MGMT,
                                    (tANI_U16) frameLen,
                                    (void **) &pAddBARspBuffer,
                                    (void **) &pPacket )))
      {
        // Log error
        limLog( pMac, LOGP,
            FL("palPktAlloc FAILED! Length [%d], Status [%d]"),
            frameLen,
            halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
      }

      vos_mem_set( (void *) pAddBARspBuffer, frameLen, 0 );

      // Copy necessary info to BD
      if( eSIR_SUCCESS !=
          (statusCode = limPopulateMacHeader( pMac,
                                       pAddBARspBuffer,
                                       SIR_MAC_MGMT_FRAME,
                                       SIR_MAC_MGMT_ACTION,
                                       pMlmAddBARsp->peerMacAddr,psessionEntry->selfMacAddr)))
        goto returnAfterError;

      // Update A3 with the BSSID
      
      pMacHdr = ( tpSirMacMgmtHdr ) pAddBARspBuffer;
      
      #if 0
      cfgLen = SIR_MAC_ADDR_LENGTH;
      if( eSIR_SUCCESS != wlan_cfgGetStr( pMac,
            WNI_CFG_BSSID,
            (tANI_U8 *) pMacHdr->bssId,
            &cfgLen ))
      {
        limLog( pMac, LOGP,
            FL( "Failed to retrieve WNI_CFG_BSSID while"
              "sending an ACTION Frame" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      #endif // TO SUPPORT BT-AMP
      sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
      limSetProtectedBit(pMac, psessionEntry, pMlmAddBARsp->peerMacAddr, pMacHdr);
#endif

      // Now, we're ready to "pack" the frames
      nStatus = dot11fPackAddBARsp( pMac,
          &frmAddBARsp,
          pAddBARspBuffer + sizeof( tSirMacMgmtHdr ),
          nPayload,
          &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGE,
            FL( "Failed to pack an ADDBA Rsp (0x%08x)." ),
            nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
                FL( "There were warnings while packing an ADDBA Rsp (0x%08x)." ),
                nStatus);
      }

      limLog( pMac, LOG1, FL( "Sending an ADDBA RSP to "MAC_ADDRESS_STR " with"
                              " tid = %d policy = %d buffsize = %d"
                              " amsduSupported = %d status %d"),
                              MAC_ADDR_ARRAY(pMlmAddBARsp->peerMacAddr),
                              frmAddBARsp.AddBAParameterSet.tid,
                              frmAddBARsp.AddBAParameterSet.policy,
                              frmAddBARsp.AddBAParameterSet.bufferSize,
                              frmAddBARsp.AddBAParameterSet.amsduSupported,
                              frmAddBARsp.Status.status);


    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
           psessionEntry->peSessionId,
           pMacHdr->fc.subType));
    halStatus = halTxFrame( pMac,
                            pPacket,
                            (tANI_U16) frameLen,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete,
                            pAddBARspBuffer, txFlag );
    MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
           psessionEntry->peSessionId,
           halStatus));
    if( eHAL_STATUS_SUCCESS != halStatus )
    {
    limLog( pMac, LOGE,
        FL( "halTxFrame FAILED! Status [%d]" ),
        halStatus );

    // FIXME - HAL error codes are different from PE error
    // codes!! And, this routine is returning tSirRetStatus
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

    returnAfterError:
      // Release buffer, if allocated
      if( NULL != pAddBARspBuffer )
        palPktFree( pMac->hHdd,
            HAL_TXRX_FRM_802_11_MGMT,
            (void *) pAddBARspBuffer,
            (void *) pPacket );

      return statusCode;
}

/**
 * \brief Send a DELBA Indication Action Frame to peer
 *
 * \sa limSendDelBAInd
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param peerMacAddr MAC Address of peer
 *
 * \param reasonCode Reason for the DELBA notification
 *
 * \param pBAParameterSet The DELBA Parameter Set.
 * This identifies the TID for which the BA session is
 * being deleted.
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendDelBAInd( tpAniSirGlobal pMac,
    tpLimMlmDelBAReq pMlmDelBAReq,tpPESession psessionEntry)
{
    tDot11fDelBAInd   frmDelBAInd;
    tANI_U8           *pDelBAIndBuffer = NULL;
    //tANI_U32 val;
    tpSirMacMgmtHdr   pMacHdr;
    tANI_U32          frameLen = 0, nStatus, nPayload;
    tSirRetStatus     statusCode;
    eHalStatus        halStatus;
    void              *pPacket;
    tANI_U32          txFlag = 0;

     if(NULL == psessionEntry)
    {
        return eSIR_FAILURE;
    }

    vos_mem_set( (void *) &frmDelBAInd, sizeof( frmDelBAInd ), 0);

      // Category - 3 (BA)
      frmDelBAInd.Category.category = SIR_MAC_ACTION_BLKACK;
      // Action - 2 (DELBA)
      frmDelBAInd.Action.action = SIR_MAC_BLKACK_DEL;

      // Fill the DELBA Parameter Set as provided by caller
      frmDelBAInd.DelBAParameterSet.tid = pMlmDelBAReq->baTID;
      frmDelBAInd.DelBAParameterSet.initiator = pMlmDelBAReq->baDirection;

      // BA Starting Sequence Number
      // Fragment number will always be zero
      frmDelBAInd.Reason.code = pMlmDelBAReq->delBAReasonCode;

      nStatus = dot11fGetPackedDelBAIndSize( pMac, &frmDelBAInd, &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "Failed to calculate the packed size for "
              "an DELBA Indication (0x%08x)."),
            nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDelBAInd );
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while calculating "
              "the packed size for an DELBA Ind (0x%08x)."),
            nStatus );
      }

      // Add the MGMT header to frame length
      frameLen = nPayload + sizeof( tSirMacMgmtHdr );

      // Allocate shared memory
      if( eHAL_STATUS_SUCCESS !=
          (halStatus = palPktAlloc( pMac->hHdd,
                                    HAL_TXRX_FRM_802_11_MGMT,
                                    (tANI_U16) frameLen,
                                    (void **) &pDelBAIndBuffer,
                                    (void **) &pPacket )))
      {
        // Log error
        limLog( pMac, LOGP,
            FL("palPktAlloc FAILED! Length [%d], Status [%d]"),
            frameLen,
            halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
      }

      vos_mem_set( (void *) pDelBAIndBuffer, frameLen, 0 );

      // Copy necessary info to BD
      if( eSIR_SUCCESS !=
          (statusCode = limPopulateMacHeader( pMac,
                                       pDelBAIndBuffer,
                                       SIR_MAC_MGMT_FRAME,
                                       SIR_MAC_MGMT_ACTION,
                                       pMlmDelBAReq->peerMacAddr,psessionEntry->selfMacAddr)))
        goto returnAfterError;

      // Update A3 with the BSSID
      pMacHdr = ( tpSirMacMgmtHdr ) pDelBAIndBuffer;
      
      #if 0
      cfgLen = SIR_MAC_ADDR_LENGTH;
      if( eSIR_SUCCESS != cfgGetStr( pMac,
            WNI_CFG_BSSID,
            (tANI_U8 *) pMacHdr->bssId,
            &cfgLen ))
      {
        limLog( pMac, LOGP,
            FL( "Failed to retrieve WNI_CFG_BSSID while"
              "sending an ACTION Frame" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      #endif //TO SUPPORT BT-AMP
      sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

#ifdef WLAN_FEATURE_11W
      limSetProtectedBit(pMac, psessionEntry, pMlmDelBAReq->peerMacAddr, pMacHdr);
#endif

      // Now, we're ready to "pack" the frames
      nStatus = dot11fPackDelBAInd( pMac,
          &frmDelBAInd,
          pDelBAIndBuffer + sizeof( tSirMacMgmtHdr ),
          nPayload,
          &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGE,
            FL( "Failed to pack an DELBA Ind (0x%08x)." ),
            nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
                FL( "There were warnings while packing an DELBA Ind (0x%08x)." ),
                nStatus);
      }

      limLog( pMac, LOG1,
            FL( "Sending a DELBA IND to: "MAC_ADDRESS_STR" with Tid = %d"
            " initiator = %d reason = %d" ),
            MAC_ADDR_ARRAY(pMlmDelBAReq->peerMacAddr),
            frmDelBAInd.DelBAParameterSet.tid,
            frmDelBAInd.DelBAParameterSet.initiator,
            frmDelBAInd.Reason.code);


    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
          psessionEntry->peSessionId,
          pMacHdr->fc.subType));
   halStatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) frameLen,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pDelBAIndBuffer, txFlag );
   MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
          psessionEntry->peSessionId,
          halStatus));
  if( eHAL_STATUS_SUCCESS != halStatus )
  {
    PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halStatus );)
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

    returnAfterError:

      // Release buffer, if allocated
      if( NULL != pDelBAIndBuffer )
        palPktFree( pMac->hHdd,
            HAL_TXRX_FRM_802_11_MGMT,
            (void *) pDelBAIndBuffer,
            (void *) pPacket );

      return statusCode;
}

#if defined WLAN_FEATURE_VOWIFI

/**
 * \brief Send a Neighbor Report Request Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pNeighborReq Address of a tSirMacNeighborReportReq
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendNeighborReportRequestFrame(tpAniSirGlobal        pMac,
                       tpSirMacNeighborReportReq pNeighborReq,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus                statusCode = eSIR_SUCCESS;
   tDot11fNeighborReportRequest frm;
   tANI_U8                      *pFrame;
   tpSirMacMgmtHdr              pMacHdr;
   tANI_U32                     nBytes, nPayload, nStatus;
   void                         *pPacket;
   eHalStatus                   halstatus;
   tANI_U32                     txFlag = 0;

   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Neighbor Report request action frame") );
      return eSIR_FAILURE;
   }
   vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

   frm.Category.category = SIR_MAC_ACTION_RRM;
   frm.Action.action     = SIR_MAC_RRM_NEIGHBOR_REQ;
   frm.DialogToken.token = pNeighborReq->dialogToken;


   if( pNeighborReq->ssid_present )
   {
      PopulateDot11fSSID( pMac, &pNeighborReq->ssid, &frm.SSID );
   }

   nStatus = dot11fGetPackedNeighborReportRequestSize( pMac, &frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Neighbor Report Request(0x%08x)."),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fNeighborReportRequest );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating "
               "the packed size for a Neighbor Rep"
               "ort Request(0x%08x)."), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Neighbor "
               "Report Request."), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   vos_mem_set( pFrame, nBytes, 0 );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

#ifdef WLAN_FEATURE_11W
   limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackNeighborReportRequest( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Neighbor Report Request (0x%08x)." ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
              FL( "There were warnings while packing Neighbor Report "
                  "Request (0x%08x)." ), nStatus);
   }

   limLog( pMac, LOGW,
         FL( "Sending a Neighbor Report Request to " ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
          psessionEntry->peSessionId,
          pMacHdr->fc.subType));
   halstatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) nBytes,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pFrame, txFlag );
   MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
          psessionEntry->peSessionId,
          halstatus));
   if( eHAL_STATUS_SUCCESS != halstatus )
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return statusCode;
   }
   else
      return eSIR_SUCCESS;

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );

   return statusCode;
} // End limSendNeighborReportRequestFrame.

/**
 * \brief Send a Link Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pLinkReport Address of a tSirMacLinkReport
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendLinkReportActionFrame(tpAniSirGlobal        pMac,
                       tpSirMacLinkReport pLinkReport,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus                statusCode = eSIR_SUCCESS;
   tDot11fLinkMeasurementReport frm;
   tANI_U8                      *pFrame;
   tpSirMacMgmtHdr              pMacHdr;
   tANI_U32                     nBytes, nPayload, nStatus;
   void                         *pPacket;
   eHalStatus                   halstatus;
   tANI_U32                     txFlag = 0;


   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Link Report action frame") );
      return eSIR_FAILURE;
   }

   vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );

   frm.Category.category = SIR_MAC_ACTION_RRM;
   frm.Action.action     = SIR_MAC_RRM_LINK_MEASUREMENT_RPT;
   frm.DialogToken.token = pLinkReport->dialogToken;


   //IEEE Std. 802.11 7.3.2.18. for the report element.
   //Even though TPC report an IE, it is represented using fixed fields since it is positioned
   //in the middle of other fixed fields in the link report frame(IEEE Std. 802.11k section7.4.6.4
   //and frame parser always expects IEs to come after all fixed fields. It is easier to handle 
   //such case this way than changing the frame parser.
   frm.TPCEleID.TPCId = SIR_MAC_TPC_RPT_EID; 
   frm.TPCEleLen.TPCLen = 2;
   frm.TxPower.txPower = pLinkReport->txPower;
   frm.LinkMargin.linkMargin = 0;

   frm.RxAntennaId.antennaId = pLinkReport->rxAntenna;
   frm.TxAntennaId.antennaId = pLinkReport->txAntenna;
   frm.RCPI.rcpi = pLinkReport->rcpi;
   frm.RSNI.rsni = pLinkReport->rsni;

   nStatus = dot11fGetPackedLinkMeasurementReportSize( pMac, &frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Link Report (0x%08x)."),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fLinkMeasurementReport );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating "
               "the packed size for a Link Rep"
               "ort (0x%08x)."), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Link "
               "Report."), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   vos_mem_set( pFrame, nBytes, 0 );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

#ifdef WLAN_FEATURE_11W
   limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackLinkMeasurementReport( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Link Report (0x%08x)." ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
              FL( "There were warnings while packing Link Report (0x%08x)." ),
              nStatus );
   }

   limLog( pMac, LOGW,
         FL( "Sending a Link Report to " ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
          psessionEntry->peSessionId,
          pMacHdr->fc.subType));
   halstatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) nBytes,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pFrame, txFlag );
   MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
          psessionEntry->peSessionId,
          halstatus));
   if( eHAL_STATUS_SUCCESS != halstatus )
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return statusCode;
   }
   else
      return eSIR_SUCCESS;

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );

   return statusCode;
} // End limSendLinkReportActionFrame.

/**
 * \brief Send a Beacon Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param dialog_token dialog token to be used in the action frame.
 *
 * \param num_report number of reports in pRRMReport.
 *
 * \param pRRMReport Address of a tSirMacRadioMeasureReport.
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendRadioMeasureReportActionFrame(tpAniSirGlobal        pMac,
                       tANI_U8              dialog_token,
                       tANI_U8              num_report,
                       tpSirMacRadioMeasureReport pRRMReport,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus      statusCode = eSIR_SUCCESS;
   tANI_U8            *pFrame;
   tpSirMacMgmtHdr    pMacHdr;
   tANI_U32           nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus         halstatus;
   tANI_U8            i;
   tANI_U32           txFlag = 0;

   tDot11fRadioMeasurementReport *frm =
         vos_mem_malloc(sizeof(tDot11fRadioMeasurementReport));
   if (!frm) {
      limLog( pMac, LOGE, FL("Not enough memory to allocate tDot11fRadioMeasurementReport") );
      return eSIR_FAILURE;
   }

   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Beacon Report action frame") );
      vos_mem_free(frm);
      return eSIR_FAILURE;
   }
   vos_mem_set( ( tANI_U8* )frm, sizeof( *frm ), 0 );

   frm->Category.category = SIR_MAC_ACTION_RRM;
   frm->Action.action     = SIR_MAC_RRM_RADIO_MEASURE_RPT;
   frm->DialogToken.token = dialog_token;

   frm->num_MeasurementReport = (num_report > RADIO_REPORTS_MAX_IN_A_FRAME ) ? RADIO_REPORTS_MAX_IN_A_FRAME  : num_report;

   for( i = 0 ; i < frm->num_MeasurementReport ; i++ )
   {
      frm->MeasurementReport[i].type = pRRMReport[i].type;
      frm->MeasurementReport[i].token = pRRMReport[i].token;
      frm->MeasurementReport[i].late = 0; //IEEE 802.11k section 7.3.22. (always zero in rrm)
      switch( pRRMReport[i].type )
      {
         case SIR_MAC_RRM_BEACON_TYPE:
            PopulateDot11fBeaconReport( pMac, &frm->MeasurementReport[i], &pRRMReport[i].report.beaconReport );
            frm->MeasurementReport[i].incapable = pRRMReport[i].incapable;
            frm->MeasurementReport[i].refused = pRRMReport[i].refused;
            frm->MeasurementReport[i].present = 1;
            break;
         default:
            frm->MeasurementReport[i].incapable = pRRMReport[i].incapable;
            frm->MeasurementReport[i].refused = pRRMReport[i].refused;
            frm->MeasurementReport[i].present = 1;
            break;
      }
   }

   nStatus = dot11fGetPackedRadioMeasurementReportSize( pMac, frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Radio Measure Report (0x%08x)."),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fLinkMeasurementReport );
      vos_mem_free(frm);
      return eSIR_FAILURE;
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating "
               "the packed size for a Radio Measure Rep"
               "ort (0x%08x)."), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Radio Measure "
               "Report."), nBytes );
      vos_mem_free(frm);
      return eSIR_FAILURE;
   }

   // Paranoia:
   vos_mem_set( pFrame, nBytes, 0 );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

#ifdef WLAN_FEATURE_11W
   limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);
#endif

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackRadioMeasurementReport( pMac,
         frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Radio Measure Report (0x%08x)." ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
              FL( "There were warnings while packing Radio "
                  "Measure Report (0x%08x)." ), nStatus);
   }

   limLog( pMac, LOGW,
         FL( "Sending a Radio Measure Report to " ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
          psessionEntry->peSessionId,
          pMacHdr->fc.subType));
   halstatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) nBytes,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pFrame, txFlag );
   MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
          psessionEntry->peSessionId,
          halstatus));
   if( eHAL_STATUS_SUCCESS != halstatus )
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      vos_mem_free(frm);
      return statusCode;
   }
   else {
      vos_mem_free(frm);
      return eSIR_SUCCESS;
   }

returnAfterError:
   vos_mem_free(frm);
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
   return statusCode;
} // End limSendBeaconReportActionFrame.

#endif

#ifdef WLAN_FEATURE_11W
/**
 * \brief Send SA query request action frame to peer
 *
 * \sa limSendSaQueryRequestFrame
 *
 *
 * \param pMac    The global tpAniSirGlobal object
 *
 * \param transId Transaction identifier
 *
 * \param peer    The Mac address of the station to which this action frame is addressed
 *
 * \param psessionEntry The PE session entry
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */

tSirRetStatus limSendSaQueryRequestFrame( tpAniSirGlobal pMac, tANI_U8 *transId,
                                          tSirMacAddr peer, tpPESession psessionEntry )
{

   tDot11fSaQueryReq  frm; // SA query request action frame
   tANI_U8            *pFrame;
   tSirRetStatus      nSirStatus;
   tpSirMacMgmtHdr    pMacHdr;
   tANI_U32           nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus         halstatus;
   tANI_U8            txFlag = 0;

   vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );
   frm.Category.category  = SIR_MAC_ACTION_SA_QUERY;
   /* 11w action  field is :
    action: 0 --> SA Query Request action frame
    action: 1 --> SA Query Response action frame */
   frm.Action.action    = SIR_MAC_SA_QUERY_REQ;
   /* 11w SA Query Request transId */
   vos_mem_copy( &frm.TransactionId.transId[0], &transId[0], 2 );

   nStatus = dot11fGetPackedSaQueryReqSize(pMac, &frm, &nPayload);
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size "
               "for an SA Query Request (0x%08x)."),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fSaQueryReq );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating "
               "the packed size for an SA Query Request"
               " (0x%08x)."), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );
   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,  nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a SA Query Request "
                               "action frame"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   vos_mem_set( pFrame, nBytes, 0 );

   // Copy necessary info to BD
   nSirStatus = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr );
   if ( eSIR_SUCCESS != nSirStatus )
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

   // Since this is a SA Query Request, set the "protect" (aka WEP) bit
   // in the FC
   limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);

   // Pack 11w SA Query Request frame
   nStatus = dot11fPackSaQueryReq( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if ( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an SA Query Request (0x%08x)." ),
            nStatus );
      // FIXME - Need to convert to tSirRetStatus
      nSirStatus = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if ( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
            FL( "There were warnings while packing SA Query Request (0x%08x)." ),
            nStatus);
   }

   limLog( pMac, LOG1,
         FL( "Sending an SA Query Request to " ));
   limPrintMacAddr( pMac, peer, LOG1 );
   limPrintMacAddr( pMac, peer, LOGE );
   limLog( pMac, LOGE,
         FL( "Sending an SA Query Request from " ));
   limPrintMacAddr( pMac, psessionEntry->selfMacAddr, LOGE );

   if ( ( SIR_BAND_5_GHZ == limGetRFBand( psessionEntry->currentOperChannel ) )
#ifdef WLAN_FEATURE_P2P
        || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
        ( psessionEntry->pePersona == VOS_P2P_GO_MODE )
#endif
      )
   {
      txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
   }

   halstatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) nBytes,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pFrame, txFlag );
   if ( eHAL_STATUS_SUCCESS != halstatus )
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halstatus );)
      nSirStatus = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return nSirStatus;
   }
   else {
      return eSIR_SUCCESS;
   }

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
   return nSirStatus;
} // End limSendSaQueryRequestFrame

/**
 * \brief Send SA query response action frame to peer 
 *
 * \sa limSendSaQueryResponseFrame
 * 
 *
 * \param pMac    The global tpAniSirGlobal object
 *
 * \param transId Transaction identifier received in SA query request action frame
 *
 * \param peer    The Mac address of the AP to which this action frame is addressed
 *
 * \param psessionEntry The PE session entry
 * 
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */

tSirRetStatus limSendSaQueryResponseFrame( tpAniSirGlobal pMac, tANI_U8 *transId,
tSirMacAddr peer,tpPESession psessionEntry)
{

   tDot11fSaQueryRsp  frm; // SA query reponse action frame
   tANI_U8            *pFrame;
   tSirRetStatus      nSirStatus;
   tpSirMacMgmtHdr    pMacHdr;
   tANI_U32           nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus         halstatus;
   tANI_U32           txFlag = 0;
   
   vos_mem_set( ( tANI_U8* )&frm, sizeof( frm ), 0 );
   frm.Category.category  = SIR_MAC_ACTION_SA_QUERY;
   /*11w action  field is :
    action: 0 --> SA query request action frame
    action: 1 --> SA query response action frame */ 
   frm.Action.action    = SIR_MAC_SA_QUERY_RSP;
   /*11w SA query response transId is same as
     SA query request transId*/
   vos_mem_copy( &frm.TransactionId.transId[0], &transId[0], 2 );

   nStatus = dot11fGetPackedSaQueryRspSize(pMac, &frm, &nPayload);
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a SA Query Response (0x%08x)."),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fSaQueryRsp );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating "
               "the packed size for an SA Query Response"
               " (0x%08x)."), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );
   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,  nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a SA query response"
                               " action frame"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   vos_mem_set( pFrame, nBytes, 0 );

   // Copy necessary info to BD
   nSirStatus = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr );
   if ( eSIR_SUCCESS != nSirStatus )
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

   // Since this is a SA Query Response, set the "protect" (aka WEP) bit
   // in the FC
   limSetProtectedBit(pMac, psessionEntry, peer, pMacHdr);

   // Pack 11w SA query response frame
   nStatus = dot11fPackSaQueryRsp( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if ( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an SA Query Response (0x%08x)." ),
            nStatus );
      // FIXME - Need to convert to tSirRetStatus
      nSirStatus = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if ( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
            FL( "There were warnings while packing SA Query Response (0x%08x)." ),
            nStatus);
   }

   limLog( pMac, LOG1,
         FL( "Sending a SA Query Response to " ));
   limPrintMacAddr( pMac, peer, LOGW );

   if ( ( SIR_BAND_5_GHZ == limGetRFBand( psessionEntry->currentOperChannel ) )
#ifdef WLAN_FEATURE_P2P
        || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
        ( psessionEntry->pePersona == VOS_P2P_GO_MODE )
#endif
      )
   {
      txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
   }

   MTRACE(macTrace(pMac, TRACE_CODE_TX_MGMT,
          psessionEntry->peSessionId,
          pMacHdr->fc.subType));
   halstatus = halTxFrame( pMac,
                           pPacket,
                           (tANI_U16) nBytes,
                           HAL_TXRX_FRM_802_11_MGMT,
                           ANI_TXDIR_TODS,
                           7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                           limTxComplete,
                           pFrame, txFlag );
   MTRACE(macTrace(pMac, TRACE_CODE_TX_COMPLETE,
          psessionEntry->peSessionId,
          halstatus));
   if ( eHAL_STATUS_SUCCESS != halstatus )
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]" ), halstatus );)
      nSirStatus = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return nSirStatus;
   }
   else {
      return eSIR_SUCCESS;
   }

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
   return nSirStatus;
} // End limSendSaQueryResponseFrame
#endif
