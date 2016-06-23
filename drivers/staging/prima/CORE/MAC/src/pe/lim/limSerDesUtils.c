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
 *
 * This file limSerDesUtils.cc contains the serializer/deserializer
 * utility functions LIM uses while communicating with upper layer
 * software entities
 * Author:        Chandra Modumudi
 * Date:          10/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "aniSystemDefs.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSerDesUtils.h"



/**
 * limCheckRemainingLength()
 *
 *FUNCTION:
 * This function is called while de-serializing received SME_REQ
 * message.
 *
 *LOGIC:
 * Remaining message length is checked for > 0.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  len     - Remaining message length
 * @return retCode - eSIR_SUCCESS if len > 0, else eSIR_FAILURE
 */

static inline tSirRetStatus
limCheckRemainingLength(tpAniSirGlobal pMac, tANI_S16 len)
{
    if (len > 0)
        return eSIR_SUCCESS;
    else
    {
        limLog(pMac, LOGW,
           FL("Received SME message with invalid rem length=%d"),
           len);
        return eSIR_FAILURE;
    }
} /*** end limCheckRemainingLength(pMac, ) ***/

/**
 * limGetBssDescription()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * BSS description from a tANI_U8* buffer pointer to tSirBssDescription
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pBssDescription  Pointer to the BssDescription to be copied
 * @param  *pBuf            Pointer to the source buffer
 * @param  rLen             Remaining message length being extracted
 * @return retCode          Indicates whether message is successfully
 *                          de-serialized (eSIR_SUCCESS) or
 *                          failure (eSIR_FAILURE).
 */

static tSirRetStatus
limGetBssDescription( tpAniSirGlobal pMac, tSirBssDescription *pBssDescription,
                     tANI_S16 rLen, tANI_S16 *lenUsed, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    pBssDescription->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len   = pBssDescription->length;

    if (rLen < (tANI_S16) (len + sizeof(tANI_U16)))
        return eSIR_FAILURE;

    *lenUsed = len + sizeof(tANI_U16);

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pBssDescription->bssId,
                  pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract timer
    vos_mem_copy( (tANI_U8 *) (&pBssDescription->scanSysTimeMsec),
                  pBuf, sizeof(v_TIME_t));
    pBuf += sizeof(v_TIME_t);
    len  -= sizeof(v_TIME_t);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract timeStamp
    vos_mem_copy( (tANI_U8 *) pBssDescription->timeStamp,
                  pBuf, sizeof(tSirMacTimeStamp));
    pBuf += sizeof(tSirMacTimeStamp);
    len  -= sizeof(tSirMacTimeStamp);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract beaconInterval
    pBssDescription->beaconInterval = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract capabilityInfo
    pBssDescription->capabilityInfo = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract nwType
    pBssDescription->nwType = (tSirNwType) limGetU32(pBuf);
    pBuf += sizeof(tSirNwType);
    len  -= sizeof(tSirNwType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract aniIndicator
    pBssDescription->aniIndicator = *pBuf++;
    len --;

    // Extract rssi
    pBssDescription->rssi = (tANI_S8) *pBuf++;
    len --;

    // Extract sinr
    pBssDescription->sinr = (tANI_S8) *pBuf++;
    len --;

    // Extract channelId
    pBssDescription->channelId = *pBuf++;
    len --;

    // Extract channelIdSelf
    pBssDescription->channelIdSelf = *pBuf++;
    len --;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
 
    // Extract reserved bssDescription
    pBuf += sizeof(pBssDescription->sSirBssDescriptionRsvd);
    len -= sizeof(pBssDescription->sSirBssDescriptionRsvd);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //pass the timestamp
    pBssDescription->nReceivedTime = limGetU32( pBuf );
    pBuf += sizeof(tANI_TIMESTAMP);
    len -= sizeof(tANI_TIMESTAMP);

#if defined WLAN_FEATURE_VOWIFI
    //TSF when the beacon received (parent TSF)
    pBssDescription->parentTSF = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);

    //start TSF of scan during which this BSS desc was updated.
    pBssDescription->startTSF[0] = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);

    //start TSF of scan during which this BSS desc was updated.
    pBssDescription->startTSF[1] = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    // MobilityDomain
    pBssDescription->mdiePresent = *pBuf++;
    len --;
    pBssDescription->mdie[0] = *pBuf++;
    len --;
    pBssDescription->mdie[1] = *pBuf++;
    len --;
    pBssDescription->mdie[2] = *pBuf++;
    len --;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog(pMac, LOG1, FL("mdie=%02x %02x %02x"),
        pBssDescription->mdie[0],
        pBssDescription->mdie[1],
        pBssDescription->mdie[2]);)
#endif
#endif

#ifdef FEATURE_WLAN_ESE
    pBssDescription->QBSSLoad_present = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract QBSSLoad_avail
    pBssDescription->QBSSLoad_avail = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif
    pBssDescription->fProbeRsp = *pBuf++;
    len  -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    /* 3 reserved bytes for padding */
    pBuf += (3 * sizeof(tANI_U8));
    len  -= 3;

    pBssDescription->WscIeLen = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    if (WSCIE_PROBE_RSP_LEN < len)
    {
        /* Do not copy with WscIeLen
         * if WscIeLen is not set properly, memory overwrite happen
         * Ended up with memory corruption and crash
         * Copy with Fixed size */
        vos_mem_copy( (tANI_U8 *) pBssDescription->WscIeProbeRsp,
                       pBuf,
                       WSCIE_PROBE_RSP_LEN);

    }
    else
    {
        limLog(pMac, LOGE,
                     FL("remaining bytes len %d is less than WSCIE_PROBE_RSP_LEN"),
                     pBssDescription->WscIeLen);
        return eSIR_FAILURE;
    }

    /* 1 reserved byte padding */
    pBuf += (WSCIE_PROBE_RSP_LEN + 1);
    len -= (WSCIE_PROBE_RSP_LEN + 1);

    if (len > 0)
    {
        vos_mem_copy( (tANI_U8 *) pBssDescription->ieFields,
                       pBuf,
                       len);
    }
    else if (len < 0)
    {
        limLog(pMac, LOGE, 
                     FL("remaining length is negative. len = %d, actual length = %d"),
                     len, pBssDescription->length);
        return eSIR_FAILURE;
    }    

    return eSIR_SUCCESS;
} /*** end limGetBssDescription() ***/



/**
 * limCopyBssDescription()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * BSS description to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *pBuf            Pointer to the destination buffer
 * @param  pBssDescription  Pointer to the BssDescription being copied
 * @return                  Length of BSSdescription written
 */

tANI_U16
limCopyBssDescription(tpAniSirGlobal pMac, tANI_U8 *pBuf, tSirBssDescription *pBssDescription)
{
    tANI_U16 len = 0;

    limCopyU16(pBuf, pBssDescription->length);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    vos_mem_copy(  pBuf,
                  (tANI_U8 *) pBssDescription->bssId,
                  sizeof(tSirMacAddr));
    pBuf       += sizeof(tSirMacAddr);
    len        += sizeof(tSirMacAddr);

   PELOG3(limLog(pMac, LOG3,
       FL("Copying BSSdescr:channel is %d, aniInd is %d, bssId is "),
       pBssDescription->channelId, pBssDescription->aniIndicator);
    limPrintMacAddr(pMac, pBssDescription->bssId, LOG3);)

    vos_mem_copy( pBuf,
                  (tANI_U8 *) (&pBssDescription->scanSysTimeMsec),
                  sizeof(v_TIME_t));
    pBuf       += sizeof(v_TIME_t);
    len        += sizeof(v_TIME_t);

    limCopyU32(pBuf, pBssDescription->timeStamp[0]);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    limCopyU32(pBuf, pBssDescription->timeStamp[1]);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    limCopyU16(pBuf, pBssDescription->beaconInterval);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    limCopyU16(pBuf, pBssDescription->capabilityInfo);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    limCopyU32(pBuf, pBssDescription->nwType);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    *pBuf++ = pBssDescription->aniIndicator;
    len++;

    *pBuf++ = pBssDescription->rssi;
    len++;

    *pBuf++ = pBssDescription->sinr;
    len++;

    *pBuf++ = pBssDescription->channelId;
    len++;

    vos_mem_copy( pBuf, (tANI_U8 *) &(pBssDescription->ieFields),
                  limGetIElenFromBssDescription(pBssDescription));

    return (len + sizeof(tANI_U16));
} /*** end limCopyBssDescription() ***/





/**
 * limGetKeysInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * key information from a tANI_U8* buffer pointer to tSirKeys
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pKeyInfo   Pointer to the keyInfo to be copied
 * @param *pBuf       Pointer to the source buffer
 *
 * @return Length of key info extracted
 */

static tANI_U32
limGetKeysInfo(tpAniSirGlobal pMac, tpSirKeys pKeyInfo, tANI_U8 *pBuf)
{
    tANI_U32 len = 0;

    pKeyInfo->keyId        = *pBuf++;
    len++;
    pKeyInfo->unicast      = *pBuf++;
    len++;
    pKeyInfo->keyDirection = (tAniKeyDirection) limGetU32(pBuf);
    len  += sizeof(tAniKeyDirection);
    pBuf += sizeof(tAniKeyDirection);

    vos_mem_copy( pKeyInfo->keyRsc, pBuf, WLAN_MAX_KEY_RSC_LEN);
    pBuf += WLAN_MAX_KEY_RSC_LEN;
    len  += WLAN_MAX_KEY_RSC_LEN;

    pKeyInfo->paeRole      = *pBuf++;
    len++;

    pKeyInfo->keyLength    = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  += sizeof(tANI_U16);
    vos_mem_copy( pKeyInfo->key, pBuf, pKeyInfo->keyLength);
    pBuf += pKeyInfo->keyLength;
    len  += pKeyInfo->keyLength;

   PELOG3(limLog(pMac, LOG3,
           FL("Extracted keyId=%d, keyLength=%d, Key is :"),
           pKeyInfo->keyId, pKeyInfo->keyLength);
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3,
               pKeyInfo->key, pKeyInfo->keyLength);)

    return len;
} /*** end limGetKeysInfo() ***/



/**
 * limStartBssReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_START_BSS_REQ from host
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
 * @param  pStartBssReq  Pointer to tSirSmeStartBssReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limStartBssReqSerDes(tpAniSirGlobal pMac, tpSirSmeStartBssReq pStartBssReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pStartBssReq || !pBuf)
        return eSIR_FAILURE;

    pStartBssReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pStartBssReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_START_BSS_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pStartBssReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pStartBssReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pStartBssReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract selfMacAddr
    vos_mem_copy( (tANI_U8 *) pStartBssReq->selfMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract beaconInterval
    pStartBssReq->beaconInterval = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract dot11Mode
    pStartBssReq->dot11mode = *pBuf++;
    len --;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssType
    pStartBssReq->bssType = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract ssId
    if (*pBuf > SIR_MAC_MAX_SSID_LENGTH)
    {
        // SSID length is more than max allowed 32 bytes
        PELOGW(limLog(pMac, LOGW, FL("Invalid SSID length, len=%d"), *pBuf);)
        return eSIR_FAILURE;
    }

    pStartBssReq->ssId.length = *pBuf++;
    len--;
    if (len < pStartBssReq->ssId.length)
    {
        limLog(pMac, LOGW,
           FL("SSID length is longer that the remaining length. SSID len=%d, remaining len=%d"),
           pStartBssReq->ssId.length, len);
        return eSIR_FAILURE;
    }

    vos_mem_copy( (tANI_U8 *) pStartBssReq->ssId.ssId,
                  pBuf,
                  pStartBssReq->ssId.length);
    pBuf += pStartBssReq->ssId.length;
    len  -= pStartBssReq->ssId.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract channelId
    pStartBssReq->channelId = *pBuf++;
    len--;

    // Extract CB secondary channel info
    pStartBssReq->cbMode = (ePhyChanBondState)limGetU32( pBuf );
    pBuf += sizeof( tANI_U32 );
    len -= sizeof( tANI_U32 );

    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;


    // Extract privacy setting
    pStartBssReq->privacy = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //Extract Uapsd Enable 
    pStartBssReq->apUapsdEnable = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //Extract SSID hidden
    pStartBssReq->ssidHidden = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pStartBssReq->fwdWPSPBCProbeReq = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    //Extract HT Protection Enable 
    pStartBssReq->protEnabled = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;

    //Extract OBSS Protection Enable 
    pStartBssReq->obssProtEnabled = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;

    pStartBssReq->ht_capab = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract AuthType
    pStartBssReq->authType = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract dtimPeriod
    pStartBssReq->dtimPeriod = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract wps state
    pStartBssReq->wps_state = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract isCoalesingInIBSSAllowed
    pStartBssReq->isCoalesingInIBSSAllowed = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssPersona
    pStartBssReq->bssPersona = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;


    // Extract txLdpcIniFeatureEnabled
    pStartBssReq->txLdpcIniFeatureEnabled = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#ifdef WLAN_FEATURE_11W
    // Extract MFP capable/required
    pStartBssReq->pmfCapable = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;
    pStartBssReq->pmfRequired = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;
#endif

    // Extract rsnIe
    pStartBssReq->rsnIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    // Check for RSN IE length (that includes length of type & length
    if (pStartBssReq->rsnIE.length > SIR_MAC_MAX_IE_LENGTH + 2)
    {
        limLog(pMac, LOGW,
               FL("Invalid RSN IE length %d in SME_START_BSS_REQ"),
               pStartBssReq->rsnIE.length);
        return eSIR_FAILURE;
    }

    vos_mem_copy( pStartBssReq->rsnIE.rsnIEdata,
                  pBuf, pStartBssReq->rsnIE.length);

    len  -= (sizeof(tANI_U16) + pStartBssReq->rsnIE.length);
    pBuf += pStartBssReq->rsnIE.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract nwType
    pStartBssReq->nwType = (tSirNwType) limGetU32(pBuf);
    pBuf += sizeof(tSirNwType);
    len  -= sizeof(tSirNwType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract operationalRateSet
    pStartBssReq->operationalRateSet.numRates = *pBuf++;

    // Check for number of rates
    if (pStartBssReq->operationalRateSet.numRates >
        SIR_MAC_MAX_NUMBER_OF_RATES)
    {
        limLog(pMac, LOGW, FL("Invalid numRates %d in SME_START_BSS_REQ"),
               pStartBssReq->operationalRateSet.numRates);
        return eSIR_FAILURE;
    }

    len--;
    if (len < pStartBssReq->operationalRateSet.numRates)
        return eSIR_FAILURE;

    vos_mem_copy( (tANI_U8 *) pStartBssReq->operationalRateSet.rate,
                  pBuf,
                  pStartBssReq->operationalRateSet.numRates);
    pBuf += pStartBssReq->operationalRateSet.numRates;
    len  -= pStartBssReq->operationalRateSet.numRates;

    // Extract extendedRateSet
    if ((pStartBssReq->nwType == eSIR_11G_NW_TYPE) ||
        (pStartBssReq->nwType == eSIR_11N_NW_TYPE ))
    {
        pStartBssReq->extendedRateSet.numRates = *pBuf++;
        len--;
        vos_mem_copy( pStartBssReq->extendedRateSet.rate,
                       pBuf, pStartBssReq->extendedRateSet.numRates);
        pBuf += pStartBssReq->extendedRateSet.numRates;
        len  -= pStartBssReq->extendedRateSet.numRates;
    }

    if (len)
    {
        limLog(pMac, LOGW, FL("Extra bytes left in SME_START_BSS_REQ, len=%d"), len);
    }

    return eSIR_SUCCESS;
} /*** end limStartBssReqSerDes() ***/



/**
 * limStopBssReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_STOP_BSS_REQ from host
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
 * @param  pStopBssReq   Pointer to tSirSmeStopBssReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limStopBssReqSerDes(tpAniSirGlobal pMac, tpSirSmeStopBssReq pStopBssReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    pStopBssReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pStopBssReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_STOP_BSS_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pStopBssReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pStopBssReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16); 

    // Extract reasonCode
    pStopBssReq->reasonCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);
    len -= sizeof(tSirResultCodes);

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pStopBssReq->bssId, pBuf, sizeof(tSirMacAddr));
    len  -= sizeof(tSirMacAddr);
  
    if (len)
        return eSIR_FAILURE;
    else
        return eSIR_SUCCESS;

} /*** end limStopBssReqSerDes() ***/



/**
 * limJoinReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_JOIN_REQ from host
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
 * @param  pJoinReq  Pointer to tSirSmeJoinReq being extracted
 * @param  pBuf      Pointer to serialized buffer
 * @return retCode   Indicates whether message is successfully
 *                   de-serialized (eSIR_SUCCESS) or
 *                   not (eSIR_FAILURE)
 */

tSirRetStatus
limJoinReqSerDes(tpAniSirGlobal pMac, tpSirSmeJoinReq pJoinReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    tANI_S16 lenUsed = 0;

#ifdef PE_DEBUG_LOG1
     tANI_U8  *pTemp = pBuf;
#endif

    if (!pJoinReq || !pBuf)
    {
        PELOGE(limLog(pMac, LOGE, FL("pJoinReq or pBuf is NULL"));)
        return eSIR_FAILURE;
    }

    // Extract messageType
    pJoinReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    // Extract length
    len = pJoinReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (pJoinReq->messageType == eWNI_SME_JOIN_REQ)
        PELOG1(limLog(pMac, LOG3, FL("SME_JOIN_REQ length %d bytes is:"), len);)
    else
        PELOG1(limLog(pMac, LOG3, FL("SME_REASSOC_REQ length %d bytes is:"),
                      len);)

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
    {
        PELOGE(limLog(pMac, LOGE, FL("len %d is too short"), len);)
        return eSIR_FAILURE;
    }

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
    // Extract sessionId
    pJoinReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
    // Extract transactionId
    pJoinReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract ssId
    pJoinReq->ssId.length = *pBuf++;
    len--;
    vos_mem_copy( (tANI_U8 *) pJoinReq->ssId.ssId, pBuf, pJoinReq->ssId.length);
    pBuf += pJoinReq->ssId.length;
    len -= pJoinReq->ssId.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract selfMacAddr
    vos_mem_copy( pJoinReq->selfMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract bsstype
    pJoinReq->bsstype = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract dot11mode
    pJoinReq->dot11mode= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract bssPersona
    pJoinReq->staPersona = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract cbMode
    pJoinReq->cbMode = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // Extract uapsdPerAcBitmask
    pJoinReq->uapsdPerAcBitmask = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }


    // Extract operationalRateSet
    pJoinReq->operationalRateSet.numRates= *pBuf++;
    len--;
    if (pJoinReq->operationalRateSet.numRates)
    {
        vos_mem_copy( (tANI_U8 *) pJoinReq->operationalRateSet.rate, pBuf,
                       pJoinReq->operationalRateSet.numRates);
        pBuf += pJoinReq->operationalRateSet.numRates;
        len -= pJoinReq->operationalRateSet.numRates;
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }

    // Extract extendedRateSet
    pJoinReq->extendedRateSet.numRates = *pBuf++;
    len--;
    if (pJoinReq->extendedRateSet.numRates)
    {
        vos_mem_copy( pJoinReq->extendedRateSet.rate, pBuf, pJoinReq->extendedRateSet.numRates);
        pBuf += pJoinReq->extendedRateSet.numRates;
        len  -= pJoinReq->extendedRateSet.numRates;
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }

    // Extract RSN IE
    pJoinReq->rsnIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    if (pJoinReq->rsnIE.length)
    {
        // Check for RSN IE length (that includes length of type & length)
        if ((pJoinReq->rsnIE.length > SIR_MAC_MAX_IE_LENGTH + 2) ||
             (pJoinReq->rsnIE.length != 2 + *(pBuf + 1)))
        {
            limLog(pMac, LOGW,
                   FL("Invalid RSN IE length %d in SME_JOIN_REQ"),
                   pJoinReq->rsnIE.length);
            return eSIR_FAILURE;
        }
        vos_mem_copy( (tANI_U8 *) pJoinReq->rsnIE.rsnIEdata,
                      pBuf, pJoinReq->rsnIE.length);
        pBuf += pJoinReq->rsnIE.length;
        len  -= pJoinReq->rsnIE.length; // skip RSN IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }

#ifdef FEATURE_WLAN_ESE
    // Extract CCKM IE
    pJoinReq->cckmIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);
    if (pJoinReq->cckmIE.length)
    {
        // Check for CCKM IE length (that includes length of type & length)
        if ((pJoinReq->cckmIE.length > SIR_MAC_MAX_IE_LENGTH) ||
             (pJoinReq->cckmIE.length != (2 + *(pBuf + 1))))
        {
            limLog(pMac, LOGW,
                   FL("Invalid CCKM IE length %d/%d in SME_JOIN/REASSOC_REQ"),
                   pJoinReq->cckmIE.length, 2 + *(pBuf + 1));
            return eSIR_FAILURE;
        }
        vos_mem_copy((tANI_U8 *) pJoinReq->cckmIE.cckmIEdata,
                      pBuf, pJoinReq->cckmIE.length);
        pBuf += pJoinReq->cckmIE.length;
        len  -= pJoinReq->cckmIE.length; // skip CCKM IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }
#endif

    // Extract Add IE for scan
    pJoinReq->addIEScan.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    if (pJoinReq->addIEScan.length)
    {
        // Check for IE length (that includes length of type & length)
        if (pJoinReq->addIEScan.length > SIR_MAC_MAX_IE_LENGTH + 2)
        {
            limLog(pMac, LOGE,
                   FL("Invalid addIE Scan length %d in SME_JOIN_REQ"),
                   pJoinReq->addIEScan.length);
            return eSIR_FAILURE;
        }
        // Check for P2P IE length (that includes length of type & length)
        vos_mem_copy( (tANI_U8 *) pJoinReq->addIEScan.addIEdata,
                      pBuf, pJoinReq->addIEScan.length);
        pBuf += pJoinReq->addIEScan.length;
        len  -= pJoinReq->addIEScan.length; // skip add IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }

    pJoinReq->addIEAssoc.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    // Extract Add IE for assoc
    if (pJoinReq->addIEAssoc.length)
    {
        // Check for IE length (that includes length of type & length)
        if (pJoinReq->addIEAssoc.length > SIR_MAC_MAX_IE_LENGTH + 2)
        {
            limLog(pMac, LOGE,
                   FL("Invalid addIE Assoc length %d in SME_JOIN_REQ"),
                   pJoinReq->addIEAssoc.length);
            return eSIR_FAILURE;
        }
        // Check for P2P IE length (that includes length of type & length)
        vos_mem_copy( (tANI_U8 *) pJoinReq->addIEAssoc.addIEdata,
                      pBuf, pJoinReq->addIEAssoc.length);
        pBuf += pJoinReq->addIEAssoc.length;
        len  -= pJoinReq->addIEAssoc.length; // skip add IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        {
            limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
            return eSIR_FAILURE;
        }
    }

    pJoinReq->UCEncryptionType = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
    
    pJoinReq->MCEncryptionType = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#ifdef WLAN_FEATURE_11W
    pJoinReq->MgmtEncryptionType = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
    //is11Rconnection;
    pJoinReq->is11Rconnection = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif

#ifdef FEATURE_WLAN_ESE
    //ESE version IE
    pJoinReq->isESEFeatureIniEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
       return eSIR_FAILURE;
    }

    //isESEconnection;
    pJoinReq->isESEconnection = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    // TSPEC information
    pJoinReq->eseTspecInfo.numTspecs = *pBuf++;
    len -= sizeof(tANI_U8);
    vos_mem_copy((void*)&pJoinReq->eseTspecInfo.tspec[0], pBuf,
                 (sizeof(tTspecInfo)* pJoinReq->eseTspecInfo.numTspecs));
    pBuf += sizeof(tTspecInfo)*SIR_ESE_MAX_TSPEC_IES;
    len  -= sizeof(tTspecInfo)*SIR_ESE_MAX_TSPEC_IES;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif
    
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    //isFastTransitionEnabled;
    pJoinReq->isFastTransitionEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif

#ifdef FEATURE_WLAN_LFR
    //isFastRoamIniFeatureEnabled;
    pJoinReq->isFastRoamIniFeatureEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif

    //txLdpcIniFeatureEnabled
    pJoinReq->txLdpcIniFeatureEnabled= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#ifdef WLAN_FEATURE_11AC
    //txBFIniFeatureEnabled
    pJoinReq->txBFIniFeatureEnabled= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    //txBFCsnValue
    pJoinReq->txBFCsnValue= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    //MuBformee
    pJoinReq->txMuBformee= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }
#endif

    pJoinReq->isAmsduSupportInAMPDU= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    pJoinReq->isWMEenabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pJoinReq->isQosEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract Titan CB Neighbor BSS info
    pJoinReq->cbNeighbors.cbBssFoundPri = *pBuf;
    pBuf++;
    pJoinReq->cbNeighbors.cbBssFoundSecUp = *pBuf;
    pBuf++;
    pJoinReq->cbNeighbors.cbBssFoundSecDown = *pBuf;
    pBuf++;
    len -= 3;

    // Extract Spectrum Mgt Indicator
    pJoinReq->spectrumMgtIndicator = (tAniBool) limGetU32(pBuf);
    pBuf += sizeof(tAniBool);       
    len -= sizeof(tAniBool);

    pJoinReq->powerCap.minTxPower = *pBuf++;
    pJoinReq->powerCap.maxTxPower = *pBuf++;
    len -=2;

    pJoinReq->supportedChannels.numChnl = *pBuf++;
    len--;
    vos_mem_copy( (tANI_U8 *) pJoinReq->supportedChannels.channelList,
                      pBuf, pJoinReq->supportedChannels.numChnl);
    pBuf += pJoinReq->supportedChannels.numChnl;
    len-= pJoinReq->supportedChannels.numChnl;

    PELOG2(limLog(pMac, LOG2,
            FL("spectrumInd ON: minPower %d, maxPower %d , numChnls %d"),
            pJoinReq->powerCap.minTxPower,
            pJoinReq->powerCap.maxTxPower,
            pJoinReq->supportedChannels.numChnl);)

    // Extract uapsdPerAcBitmask
    pJoinReq->uapsdPerAcBitmask = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
    {
        limLog(pMac, LOGE, FL("remaining len %d is too short"), len);
        return eSIR_FAILURE;
    }

    //
    // NOTE - tSirBssDescription is now moved to the end
    // of tSirSmeJoinReq structure. This is to accomodate
    // the variable length data member ieFields[1]
    //
    if (limGetBssDescription( pMac, &pJoinReq->bssDescription,
                             len, &lenUsed, pBuf) == eSIR_FAILURE)
    {
        PELOGE(limLog(pMac, LOGE, FL("get bss description failed"));)
        return eSIR_FAILURE;
    }
    PELOG3(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3,
                      (tANI_U8 *) &(pJoinReq->bssDescription),
                       pJoinReq->bssDescription.length + 2);)
    pBuf += lenUsed;
    len -= lenUsed;

    return eSIR_SUCCESS;
} /*** end limJoinReqSerDes() ***/


/**---------------------------------------------------------------
\fn     limAssocIndSerDes
\brief  This function is called by limProcessMlmAssocInd() to 
\       populate the SME_ASSOC_IND message based on the received
\       MLM_ASSOC_IND.
\
\param pMac
\param pAssocInd - Pointer to the received tLimMlmAssocInd
\param pBuf - Pointer to serialized buffer
\param psessionEntry - pointer to PE session entry
\
\return None
------------------------------------------------------------------*/
void
limAssocIndSerDes(tpAniSirGlobal pMac, tpLimMlmAssocInd pAssocInd, tANI_U8 *pBuf, tpPESession psessionEntry)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif


    mLen   = sizeof(tANI_U32);
    mLen   += sizeof(tANI_U8);
    pBuf  += sizeof(tANI_U16);
    *pBuf = psessionEntry->smeSessionId;
    pBuf   += sizeof(tANI_U8);

     // Fill in peerMacAddr
    vos_mem_copy( pBuf, pAssocInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in aid
    limCopyU16(pBuf, pAssocInd->aid);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

   // Fill in bssId
    vos_mem_copy( pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in staId
    limCopyU16(pBuf, psessionEntry->staId);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    // Fill in authType
    limCopyU32(pBuf, pAssocInd->authType);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    // Fill in ssId
    vos_mem_copy( pBuf, (tANI_U8 *) &(pAssocInd->ssId), pAssocInd->ssId.length + 1);
    pBuf += (1 + pAssocInd->ssId.length);
    mLen += (1 + pAssocInd->ssId.length);

    // Fill in rsnIE
    limCopyU16(pBuf, pAssocInd->rsnIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    vos_mem_copy( pBuf, (tANI_U8 *) &(pAssocInd->rsnIE.rsnIEdata),
                  pAssocInd->rsnIE.length);
    pBuf += pAssocInd->rsnIE.length;
    mLen += pAssocInd->rsnIE.length;


    limCopyU32(pBuf, pAssocInd->spectrumMgtIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    if (pAssocInd->spectrumMgtIndicator == eSIR_TRUE)
    {
        *pBuf = pAssocInd->powerCap.minTxPower;
        pBuf++;
        *pBuf = pAssocInd->powerCap.maxTxPower;
        pBuf++;
        mLen += sizeof(tSirMacPowerCapInfo);

        *pBuf = pAssocInd->supportedChannels.numChnl;
        pBuf++;
        mLen++;

        vos_mem_copy( pBuf,
                     (tANI_U8 *) &(pAssocInd->supportedChannels.channelList),
                     pAssocInd->supportedChannels.numChnl);


        pBuf += pAssocInd->supportedChannels.numChnl;
        mLen += pAssocInd->supportedChannels.numChnl;
    }
    limCopyU32(pBuf, pAssocInd->WmmStaInfoPresent);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);
     // Fill in length of SME_ASSOC_IND message
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_ASSOC_IND length %d bytes:"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limAssocIndSerDes() ***/



/**
 * limAssocCnfSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() when
 * SME_ASSOC_CNF or SME_REASSOC_CNF message is received from
 * upper layer software.
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
 * @param  pAssocCnf  Pointer to tSirSmeAssocCnf being extracted into
 * @param  pBuf       Pointer to serialized buffer
 * @return retCode    Indicates whether message is successfully
 *                    de-serialized (eSIR_SUCCESS) or
 *                    not (eSIR_FAILURE)
 */

tSirRetStatus
limAssocCnfSerDes(tpAniSirGlobal pMac, tpSirSmeAssocCnf pAssocCnf, tANI_U8 *pBuf)
{
#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pAssocCnf || !pBuf)
        return eSIR_FAILURE;

    pAssocCnf->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    pAssocCnf->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (pAssocCnf->messageType == eWNI_SME_ASSOC_CNF)
    {
        PELOG1(limLog(pMac, LOG1, FL("SME_ASSOC_CNF length %d bytes is:"), pAssocCnf->length);)
    }
    else
    {
        PELOG1(limLog(pMac, LOG1, FL("SME_REASSOC_CNF length %d bytes is:"), pAssocCnf->length);)
    }
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, pAssocCnf->length);)

    // status code
    pAssocCnf->statusCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);

    // bssId
    vos_mem_copy( pAssocCnf->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);

    // peerMacAddr
    vos_mem_copy( pAssocCnf->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);


    pAssocCnf->aid = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    // alternateBssId
    vos_mem_copy( pAssocCnf->alternateBssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);

    // alternateChannelId
    pAssocCnf->alternateChannelId = *pBuf;
    pBuf++;

    return eSIR_SUCCESS;
} /*** end limAssocCnfSerDes() ***/



/**
 * limDisassocCnfSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() when
 * SME_DISASSOC_CNF message is received from upper layer software.
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
 * @param  pDisassocCnf  Pointer to tSirSmeDisassocCnf being
 *                       extracted into
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limDisassocCnfSerDes(tpAniSirGlobal pMac, tpSirSmeDisassocCnf pDisassocCnf, tANI_U8 *pBuf)
{
#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDisassocCnf || !pBuf)
        return eSIR_FAILURE;

    pDisassocCnf->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    pDisassocCnf->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DISASSOC_CNF length %d bytes is:"), pDisassocCnf->length);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, pDisassocCnf->length);)

    pDisassocCnf->statusCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);

    vos_mem_copy( pDisassocCnf->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);

    vos_mem_copy( pDisassocCnf->peerMacAddr, pBuf, sizeof(tSirMacAddr));


    return eSIR_SUCCESS;
} /*** end limDisassocCnfSerDes() ***/





/**---------------------------------------------------------------
\fn     limReassocIndSerDes
\brief  This function is called by limProcessMlmReassocInd() to 
\       populate the SME_REASSOC_IND message based on the received
\       MLM_REASSOC_IND.
\
\param pMac
\param pReassocInd - Pointer to the received tLimMlmReassocInd
\param pBuf - Pointer to serialized buffer
\param psessionEntry - pointer to PE session entry
\
\return None
------------------------------------------------------------------*/
void
limReassocIndSerDes(tpAniSirGlobal pMac, tpLimMlmReassocInd pReassocInd, tANI_U8 *pBuf, tpPESession psessionEntry)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif


    mLen   = sizeof(tANI_U32);
    pBuf  += sizeof(tANI_U16);
    *pBuf++ = psessionEntry->smeSessionId;
    mLen += sizeof(tANI_U8);

    // Fill in peerMacAddr
    vos_mem_copy( pBuf, pReassocInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in oldMacAddr
    vos_mem_copy( pBuf, pReassocInd->currentApAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in aid
    limCopyU16(pBuf, pReassocInd->aid);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
 
    // Fill in bssId
    vos_mem_copy( pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in staId
    limCopyU16(pBuf, psessionEntry->staId);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    // Fill in authType
    limCopyU32(pBuf, pReassocInd->authType);
    pBuf += sizeof(tAniAuthType);
    mLen += sizeof(tAniAuthType);

    // Fill in ssId
    vos_mem_copy( pBuf, (tANI_U8 *) &(pReassocInd->ssId),
                  pReassocInd->ssId.length + 1);
    pBuf += 1 + pReassocInd->ssId.length;
    mLen += pReassocInd->ssId.length + 1;

    // Fill in rsnIE
    limCopyU16(pBuf, pReassocInd->rsnIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    vos_mem_copy( pBuf, (tANI_U8 *) &(pReassocInd->rsnIE.rsnIEdata),
                  pReassocInd->rsnIE.length);
    pBuf += pReassocInd->rsnIE.length;
    mLen += pReassocInd->rsnIE.length;

    // Fill in addIE
    limCopyU16(pBuf, pReassocInd->addIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    vos_mem_copy( pBuf, (tANI_U8*) &(pReassocInd->addIE.addIEdata),
                   pReassocInd->addIE.length);
    pBuf += pReassocInd->addIE.length;
    mLen += pReassocInd->addIE.length;


    limCopyU32(pBuf, pReassocInd->spectrumMgtIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    if (pReassocInd->spectrumMgtIndicator == eSIR_TRUE)
    {
        *pBuf = pReassocInd->powerCap.minTxPower;
        pBuf++;
        *pBuf = pReassocInd->powerCap.maxTxPower;
        pBuf++;
        mLen += sizeof(tSirMacPowerCapInfo);

        *pBuf = pReassocInd->supportedChannels.numChnl;
        pBuf++;
        mLen++;

        vos_mem_copy( pBuf,
                       (tANI_U8 *) &(pReassocInd->supportedChannels.channelList),
                       pReassocInd->supportedChannels.numChnl);

        pBuf += pReassocInd->supportedChannels.numChnl;
        mLen += pReassocInd->supportedChannels.numChnl;
    }
    limCopyU32(pBuf, pReassocInd->WmmStaInfoPresent);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    // Fill in length of SME_REASSOC_IND message
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_REASSOC_IND length %d bytes:"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limReassocIndSerDes() ***/


/**
 * limAuthIndSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessMlmAuthInd() while sending
 * SME_AUTH_IND to host
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
 * @param  pAuthInd          Pointer to tSirSmeAuthInd being sent
 * @param  pBuf         Pointer to serialized buffer
 *
 * @return None
 */

void
limAuthIndSerDes(tpAniSirGlobal pMac, tpLimMlmAuthInd pAuthInd, tANI_U8 *pBuf)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    mLen   = sizeof(tANI_U32);
    pBuf  += sizeof(tANI_U16);
    *pBuf++ = pAuthInd->sessionId;
    mLen += sizeof(tANI_U8);

    // BTAMP TODO:  Fill in bssId
    vos_mem_set(pBuf, sizeof(tSirMacAddr), 0);
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    vos_mem_copy( pBuf, pAuthInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    limCopyU32(pBuf, pAuthInd->authType);
    pBuf += sizeof(tAniAuthType);
    mLen += sizeof(tAniAuthType);
  
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_AUTH_IND length %d bytes:"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limAuthIndSerDes() ***/



/**
 * limSetContextReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_SETCONTEXT_REQ from host
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
 * @param  pSetContextReq  Pointer to tSirSmeSetContextReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limSetContextReqSerDes(tpAniSirGlobal pMac, tpSirSmeSetContextReq pSetContextReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    tANI_U16 totalKeySize = sizeof(tANI_U8); // initialized to sizeof numKeys
    tANI_U8  numKeys;
    tANI_U8 *pKeys;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif
    if (!pSetContextReq || !pBuf)
        return eSIR_FAILURE;

    pSetContextReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pSetContextReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_SETCONTEXT_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pSetContextReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pSetContextReq->transactionId = sirReadU16N(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    vos_mem_copy( (tANI_U8 *) pSetContextReq->peerMacAddr,
                   pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    vos_mem_copy( pSetContextReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;


//    pSetContextReq->qosInfoPresent = limGetU32(pBuf);
//    pBuf += sizeof(tAniBool);

//    if (pSetContextReq->qosInfoPresent)
//    {
//        len   = limGetQosInfo(&pSetContextReq->qos, pBuf);
//        pBuf += len;
//    }

    pSetContextReq->keyMaterial.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pSetContextReq->keyMaterial.edType = (tAniEdType) limGetU32(pBuf);
    pBuf += sizeof(tAniEdType);
    len  -= sizeof(tAniEdType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    numKeys = pSetContextReq->keyMaterial.numKeys = *pBuf++;
    len  -= sizeof(numKeys);

    if (numKeys > SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)
        return eSIR_FAILURE;

    /** Initialize the Default Keys if no Keys are being sent from the upper layer*/
    if( limCheckRemainingLength(pMac, len) == eSIR_FAILURE) {
        tpSirKeys pKeyinfo = pSetContextReq->keyMaterial.key;

        pKeyinfo->keyId = 0;
        pKeyinfo->keyDirection = eSIR_TX_RX; 
        pKeyinfo->keyLength = 0;
            
        if (!limIsAddrBC(pSetContextReq->peerMacAddr))
            pKeyinfo->unicast = 1;
        else
            pKeyinfo->unicast = 0;             
    }else {
        pKeys  = (tANI_U8 *) pSetContextReq->keyMaterial.key;
        do {
            tANI_U32 keySize   = limGetKeysInfo(pMac, (tpSirKeys) pKeys,
                                       pBuf);
            vos_mem_zero(pBuf, keySize);
            pBuf         += keySize;
            pKeys        += sizeof(tSirKeys);
            totalKeySize += (tANI_U16) keySize;
            if (numKeys == 0)
                break;
            numKeys--;            
        }while (numKeys);
    }
    return eSIR_SUCCESS;
} /*** end limSetContextReqSerDes() ***/

/**
 * limRemoveKeyReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_REMOVEKEY_REQ from host
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
 * @param  pRemoveKeyReq  Pointer to tSirSmeRemoveKeyReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limRemoveKeyReqSerDes(tpAniSirGlobal pMac, tpSirSmeRemoveKeyReq pRemoveKeyReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef    PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif
    if (!pRemoveKeyReq || !pBuf)
        return eSIR_FAILURE;

    pRemoveKeyReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pRemoveKeyReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_REMOVEKEY_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    vos_mem_copy( (tANI_U8 *) pRemoveKeyReq->peerMacAddr,
                  pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;


    pRemoveKeyReq->edType = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->wepType = *pBuf;

    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->keyId = *pBuf;
    
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->unicast = *pBuf;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( pRemoveKeyReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pRemoveKeyReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pRemoveKeyReq->transactionId = sirReadU16N(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    return eSIR_SUCCESS;
} /*** end limRemoveKeyReqSerDes() ***/



/**
 * limDisassocReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_DISASSOC_REQ from host
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
 * @param  pDisassocReq  Pointer to tSirSmeDisassocReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 *
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limDisassocReqSerDes(tpAniSirGlobal pMac, tSirSmeDisassocReq *pDisassocReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDisassocReq || !pBuf)
        return eSIR_FAILURE;

    pDisassocReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pDisassocReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DISASSOC_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionID
    pDisassocReq->sessionId = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionid
    pDisassocReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pDisassocReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract peerMacAddr
    vos_mem_copy( pDisassocReq->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract reasonCode
    pDisassocReq->reasonCode = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pDisassocReq->doNotSendOverTheAir = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);


    return eSIR_SUCCESS;
} /*** end limDisassocReqSerDes() ***/



/**
 * limDeauthReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_DEAUTH_REQ from host
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
 * @param  pDeauthReq           Pointer to tSirSmeDeauthReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 *
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */
tSirRetStatus
limDeauthReqSerDes(tpAniSirGlobal pMac, tSirSmeDeauthReq *pDeauthReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDeauthReq || !pBuf)
        return eSIR_FAILURE;

    pDeauthReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pDeauthReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DEAUTH_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pDeauthReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pDeauthReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( pDeauthReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract peerMacAddr
    vos_mem_copy( pDeauthReq->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract reasonCode
    pDeauthReq->reasonCode = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);

    
    return eSIR_SUCCESS;
} /*** end limDisassocReqSerDes() ***/






/**
 * limStatSerDes()
 *
 *FUNCTION:
 * This function is called by limSendSmeDisassocNtf() while sending
 * SME_DISASSOC_IND/eWNI_SME_DISASSOC_RSP to host
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
 * @param  pAssocInd  Pointer to tpAniStaStatStruct being sent
 * @param  pBuf       Pointer to serialized buffer
 *
 * @return None
 */

void
limStatSerDes(tpAniSirGlobal pMac, tpAniStaStatStruct pStat, tANI_U8 *pBuf)
{
#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    limCopyU32(pBuf, pStat->sentAesBlksUcastHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->sentAesBlksUcastLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->recvAesBlksUcastHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->recvAesBlksUcastLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesFormatErrorUcastCnts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesReplaysUcast);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesDecryptErrUcast);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->singleRetryPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->failedTxPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->ackTimeouts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->multiRetryPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->fragTxCntsHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->fragTxCntsLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->transmittedPktsHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->transmittedPktsLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->phyStatHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->phyStatLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkRssi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkSinr);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkRate);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkRssi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkSinr);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkRate);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->nRcvBytes);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->nXmitBytes);
    pBuf += sizeof(tANI_U32);

    PELOG1(limLog(pMac, LOG1, FL("STAT: length %d bytes is:"), sizeof(tAniStaStatStruct));)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp,  sizeof(tAniStaStatStruct));)

} /*** end limStatSerDes() ***/




/**
 * limPackBkgndScanFailNotify()
 *
 *FUNCTION:
 * This function is called by limSendSmeWmStatusChangeNtf()
 * to pack the tSirBackgroundScanInfo message
 *
 */
void
limPackBkgndScanFailNotify(tpAniSirGlobal pMac,
                           tSirSmeStatusChangeCode statusChangeCode,
                           tpSirBackgroundScanInfo pScanInfo,
                           tSirSmeWmStatusChangeNtf *pSmeNtf,
                           tANI_U8 sessionId)
{

    tANI_U16    length = (sizeof(tANI_U16) * 2) + sizeof(tANI_U8) +
                    sizeof(tSirSmeStatusChangeCode) +
                    sizeof(tSirBackgroundScanInfo);

        pSmeNtf->messageType = eWNI_SME_WM_STATUS_CHANGE_NTF;
        pSmeNtf->statusChangeCode = statusChangeCode;
        pSmeNtf->length = length;
        pSmeNtf->sessionId = sessionId;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanSuccess = pScanInfo->numOfScanSuccess;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanFailure = pScanInfo->numOfScanFailure;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.reserved = pScanInfo->reserved;
}


/**
 * limIsSmeGetAssocSTAsReqValid()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() upon
 * receiving SME_GET_ASSOC_STAS_REQ message from application.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pBuf    - Pointer to a serialized SME_GET_ASSOC_STAS_REQ message
 * @param  pSmeMsg - Pointer to a tSirSmeGetAssocSTAsReq structure
 * @return true if SME_GET_ASSOC_STAS_REQ message is formatted correctly
 *                false otherwise
 */
tANI_BOOLEAN
limIsSmeGetAssocSTAsReqValid(tpAniSirGlobal pMac, tpSirSmeGetAssocSTAsReq pGetAssocSTAsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    pGetAssocSTAsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pGetAssocSTAsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eANI_BOOLEAN_FALSE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eANI_BOOLEAN_FALSE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pGetAssocSTAsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eANI_BOOLEAN_FALSE;

    // Extract modId
    pGetAssocSTAsReq->modId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eANI_BOOLEAN_FALSE;

    // Extract pUsrContext
    vos_mem_copy((tANI_U8 *)pGetAssocSTAsReq->pUsrContext, pBuf, sizeof(void*));
    pBuf += sizeof(void*);
    len  -= sizeof(void*);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eANI_BOOLEAN_FALSE;

    // Extract pSapEventCallback
    vos_mem_copy((tANI_U8 *)pGetAssocSTAsReq->pSapEventCallback, pBuf, sizeof(void*));
    pBuf += sizeof(void*);
    len  -= sizeof(void*);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eANI_BOOLEAN_FALSE;

    // Extract pAssocStasArray
    vos_mem_copy((tANI_U8 *)pGetAssocSTAsReq->pAssocStasArray, pBuf, sizeof(void*));
    pBuf += sizeof(void*);
    len  -= sizeof(void*);

    PELOG1(limLog(pMac, LOG1, FL("SME_GET_ASSOC_STAS_REQ length consumed %d bytes "), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_ASSOC_STAS_REQ invalid length"));)
        return eANI_BOOLEAN_FALSE;
    }

    return eANI_BOOLEAN_TRUE;
}

/**
 * limTkipCntrMeasReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * eWNI_SME_TKIP_CNTR_MEAS_REQ from host HDD
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
 * @param  tpSirSmeTkipCntrMeasReq  Pointer to tSirSmeTkipCntrMeasReq being extracted
 * @param  pBuf                     Pointer to serialized buffer
 * @return retCode                  Indicates whether message is successfully
 *                                  de-serialized (eSIR_SUCCESS) or
 *                                  not (eSIR_FAILURE)
 */
tSirRetStatus
limTkipCntrMeasReqSerDes(tpAniSirGlobal pMac, tpSirSmeTkipCntrMeasReq  pTkipCntrMeasReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    pTkipCntrMeasReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pTkipCntrMeasReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_TKIP_CNTR_MEAS_REQ length %d bytes is:"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pTkipCntrMeasReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pTkipCntrMeasReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16); 
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pTkipCntrMeasReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bEnable
    pTkipCntrMeasReq->bEnable = *pBuf++;
    len --;

    PELOG1(limLog(pMac, LOG1, FL("SME_TKIP_CNTR_MEAS_REQ length consumed %d bytes "), len);)
    
    if (len)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_TKIP_CNTR_MEAS_REQ invalid "));)
        return eSIR_FAILURE;
    }
    else
        return eSIR_SUCCESS;
}

/**
 * limIsSmeGetWPSPBCSessionsReqValid()
 *
 *FUNCTION:
 * This function is called by limProcessSmeGetWPSPBCSessions() upon
 * receiving query WPS PBC overlap information message from application.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pBuf    - Pointer to a serialized SME_GET_WPSPBC_SESSION_REQ message
 * @param  pGetWPSPBCSessionsReq - Pointer to a tSirSmeGetWPSPBCSessionsReq structure
 * @return true if SME_GET_WPSPBC_SESSION_REQ message is formatted correctly
 *                false otherwise
 */
 
tSirRetStatus
limIsSmeGetWPSPBCSessionsReqValid(tpAniSirGlobal pMac, tSirSmeGetWPSPBCSessionsReq *pGetWPSPBCSessionsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirSmeGetWPSPBCSessionsReq));)
    
    pGetWPSPBCSessionsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pGetWPSPBCSessionsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

   // Extract pUsrContext
    vos_mem_copy((tANI_U8 *)pGetWPSPBCSessionsReq->pUsrContext, pBuf, sizeof(void*));
    pBuf += sizeof(void*);
    len  -= sizeof(void*);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract pSapEventCallback
    vos_mem_copy((tANI_U8 *)pGetWPSPBCSessionsReq->pSapEventCallback, pBuf, sizeof(void*));
    pBuf += sizeof(void*);
    len  -= sizeof(void*);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pGetWPSPBCSessionsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
 
    // Extract MAC address of Station to be removed
    vos_mem_copy( (tANI_U8 *) pGetWPSPBCSessionsReq->pRemoveMac, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    
    PELOG1(limLog(pMac, LOG1, FL("SME_GET_ASSOC_STAS_REQ length consumed %d bytes "), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_WPSPBC_SESSION_REQ invalid length"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
}


/**---------------------------------------------------------------
\fn     limGetSessionInfo
\brief  This function returns the sessionId and transactionId
\       of a message. This assumes that the message structure 
\       is of format:
\          tANI_U16   messageType
\          tANI_U16   messageLength
\          tANI_U8    sessionId
\          tANI_U16   transactionId
\param  pMac          - pMac global structure
\param  *pBuf         - pointer to the message buffer
\param  sessionId     - returned session id value
\param  transactionId - returned transaction ID value
\return None
------------------------------------------------------------------*/
void
limGetSessionInfo(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U8 *sessionId, tANI_U16 *transactionId)
{
    if (!pBuf)
    {
        limLog(pMac, LOGE, FL("NULL ptr received. "));
        return;
    }

    pBuf += sizeof(tANI_U16);   // skip message type 
    pBuf += sizeof(tANI_U16);   // skip message length

    *sessionId = *pBuf;            // get sessionId
    pBuf++;    
    *transactionId = limGetU16(pBuf);  // get transactionId

    return;
}


/**
 * limUpdateAPWPSIEsReqSerDes()
 *
 *FUNCTION:
 * This function is to deserialize UpdateAPWPSIEs message
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
 * @param  pUpdateAPWPSIEsReq  Pointer to tSirUpdateAPWPSIEsReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limUpdateAPWPSIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPSIEsReq pUpdateAPWPSIEsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirUpdateAPWPSIEsReq));)

    if (!pUpdateAPWPSIEsReq || !pBuf)
        return eSIR_FAILURE;

    pUpdateAPWPSIEsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pUpdateAPWPSIEsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
        
    // Extract transactionId
    pUpdateAPWPSIEsReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pUpdateAPWPSIEsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pUpdateAPWPSIEsReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract APWPSIEs
    vos_mem_copy( (tSirAPWPSIEs *) &pUpdateAPWPSIEsReq->APWPSIEs, pBuf, sizeof(tSirAPWPSIEs));
    pBuf += sizeof(tSirAPWPSIEs);
    len  -= sizeof(tSirAPWPSIEs);

    PELOG1(limLog(pMac, LOG1, FL("SME_UPDATE_APWPSIE_REQ length consumed %d bytes "), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_UPDATE_APWPSIE_REQ invalid length"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} /*** end limSetContextReqSerDes() ***/

/**
 * limUpdateAPWPARSNIEsReqSerDes ()
 *
 *FUNCTION:
 * This function is to deserialize UpdateAPWPSIEs message
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
 * @param  pUpdateAPWPARSNIEsReq  Pointer to tpSirUpdateAPWPARSNIEsReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limUpdateAPWPARSNIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPARSNIEsReq pUpdateAPWPARSNIEsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirUpdateAPWPARSNIEsReq));)

    if (!pUpdateAPWPARSNIEsReq || !pBuf)
        return eSIR_FAILURE;

    pUpdateAPWPARSNIEsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pUpdateAPWPARSNIEsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pUpdateAPWPARSNIEsReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof(tANI_U16);
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    vos_mem_copy( (tANI_U8 *) pUpdateAPWPARSNIEsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pUpdateAPWPARSNIEsReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract APWPARSNIEs
    vos_mem_copy( (tSirRSNie *) &pUpdateAPWPARSNIEsReq->APWPARSNIEs, pBuf, sizeof(tSirRSNie));
    pBuf += sizeof(tSirRSNie);
    len  -= sizeof(tSirRSNie);

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_WPSPBC_SESSION_REQ invalid length"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} /*** end limUpdateAPWPARSNIEsReqSerDes() ***/

