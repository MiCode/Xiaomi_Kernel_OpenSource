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
 * This file limIbssPeerMgmt.cc contains the utility functions
 * LIM uses to maintain peers in IBSS.
 * Author:        Chandra Modumudi
 * Date:          03/12/04
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#include "palTypes.h"
#include "aniGlobal.h"
#include "sirCommon.h"
#include "wniCfg.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limStaHashApi.h"
#include "schApi.h"          // schSetFixedBeaconFields for IBSS coalesce
#include "limSecurityUtils.h"
#include "limSendMessages.h"
#include "limSession.h"
#include "limIbssPeerMgmt.h"


/**
 * ibss_peer_find
 *
 *FUNCTION:
 * This function is called while adding a context at
 * DPH & Polaris for a peer in IBSS.
 * If peer is found in the list, capabilities from the
 * returned BSS description are used at DPH node & Polaris.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  macAddr - MAC address of the peer
 *
 * @return Pointer to peer node if found, else NULL
 */

static tLimIbssPeerNode *
ibss_peer_find(
    tpAniSirGlobal  pMac,
    tSirMacAddr     macAddr)
{
    tLimIbssPeerNode *pTempNode = pMac->lim.gLimIbssPeerList;

    while (pTempNode != NULL)
    {
        if (vos_mem_compare((tANI_U8 *) macAddr,
                            (tANI_U8 *) &pTempNode->peerMacAddr,
                            sizeof(tSirMacAddr)))
            break;
        pTempNode = pTempNode->next;
    }
    return pTempNode;
} /*** end ibss_peer_find() ***/

/**
 * ibss_peer_add
 *
 *FUNCTION:
 * This is called on a STA in IBSS upon receiving Beacon/
 * Probe Response from a peer.
 *
 *LOGIC:
 * Node is always added to the front of the list
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      - Pointer to Global MAC structure
 * @param  pPeerNode - Pointer to peer node to be added to the list.
 *
 * @return None
 */

static tSirRetStatus
ibss_peer_add(tpAniSirGlobal pMac, tLimIbssPeerNode *pPeerNode)
{
#ifdef ANI_SIR_IBSS_PEER_CACHING
    tANI_U32 numIbssPeers = (2 * pMac->lim.maxStation);

    if (pMac->lim.gLimNumIbssPeers >= numIbssPeers)
    {
        /**
         * Reached max number of peers to be maintained.
         * Delete last entry & add new entry at the beginning.
         */
        tLimIbssPeerNode *pTemp, *pPrev;
        pTemp = pPrev = pMac->lim.gLimIbssPeerList;
        while (pTemp->next != NULL)
        {
            pPrev = pTemp;
            pTemp = pTemp->next;
        }
        if(pTemp->beacon)
        {
            vos_mem_free(pTemp->beacon);
        }

        vos_mem_free(pTemp);
        pPrev->next = NULL;
    }
    else
#endif
        pMac->lim.gLimNumIbssPeers++;

    pPeerNode->next = pMac->lim.gLimIbssPeerList;
    pMac->lim.gLimIbssPeerList = pPeerNode;

    return eSIR_SUCCESS;

} /*** end limAddIbssPeerToList() ***/

/**
 * ibss_peer_collect
 *
 *FUNCTION:
 * This is called to collect IBSS peer information
 * from received Beacon/Probe Response frame from it.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pBeacon - Parsed Beacon Frame structure
 * @param  pBD     - Pointer to received BD
 * @param  pPeer   - Pointer to IBSS peer node
 *
 * @return None
 */

static void
ibss_peer_collect(
    tpAniSirGlobal      pMac,
    tpSchBeaconStruct   pBeacon,
    tpSirMacMgmtHdr     pHdr,
    tLimIbssPeerNode    *pPeer,
    tpPESession         psessionEntry)
{
    vos_mem_copy(pPeer->peerMacAddr, pHdr->sa, sizeof(tSirMacAddr));

    pPeer->capabilityInfo       = pBeacon->capabilityInfo;
    pPeer->extendedRatesPresent = pBeacon->extendedRatesPresent;
    pPeer->edcaPresent          = pBeacon->edcaPresent;
    pPeer->wmeEdcaPresent       = pBeacon->wmeEdcaPresent;
    pPeer->wmeInfoPresent       = pBeacon->wmeInfoPresent;

    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
        (pBeacon->HTCaps.present))
    {
        pPeer->htCapable =  pBeacon->HTCaps.present;
        vos_mem_copy((tANI_U8 *)pPeer->supportedMCSSet,
                     (tANI_U8 *)pBeacon->HTCaps.supportedMCSSet,
                     sizeof(pPeer->supportedMCSSet));
        pPeer->htGreenfield = (tANI_U8)pBeacon->HTCaps.greenField;
        pPeer->htSupportedChannelWidthSet = ( tANI_U8 ) pBeacon->HTCaps.supportedChannelWidthSet;
        pPeer->htMIMOPSState =  (tSirMacHTMIMOPowerSaveState)pBeacon->HTCaps.mimoPowerSave;
        pPeer->htMaxAmsduLength = ( tANI_U8 ) pBeacon->HTCaps.maximalAMSDUsize;
        pPeer->htAMpduDensity =             pBeacon->HTCaps.mpduDensity;
        pPeer->htDsssCckRate40MHzSupport = (tANI_U8)pBeacon->HTCaps.dsssCckMode40MHz;
        pPeer->htShortGI20Mhz = (tANI_U8)pBeacon->HTCaps.shortGI20MHz;
        pPeer->htShortGI40Mhz = (tANI_U8)pBeacon->HTCaps.shortGI40MHz;
        pPeer->htMaxRxAMpduFactor = pBeacon->HTCaps.maxRxAMPDUFactor;
        pPeer->htSecondaryChannelOffset = pBeacon->HTInfo.secondaryChannelOffset;
    }

    /* Collect peer VHT capabilities based on the received beacon from the peer */
#ifdef WLAN_FEATURE_11AC
    if (IS_BSS_VHT_CAPABLE(pBeacon->VHTCaps))
    {
        pPeer->vhtSupportedChannelWidthSet = pBeacon->VHTOperation.chanWidth;
        pPeer->vhtCapable = pBeacon->VHTCaps.present;

        // Collect VHT capabilities from beacon
        vos_mem_copy((tANI_U8 *) &pPeer->VHTCaps,
                     (tANI_U8 *) &pBeacon->VHTCaps,
                     sizeof(tDot11fIEVHTCaps));
    }
#endif
    pPeer->erpIePresent = pBeacon->erpPresent;

    vos_mem_copy((tANI_U8 *) &pPeer->supportedRates,
                 (tANI_U8 *) &pBeacon->supportedRates,
                 pBeacon->supportedRates.numRates + 1);
    if (pPeer->extendedRatesPresent)
        vos_mem_copy((tANI_U8 *) &pPeer->extendedRates,
                     (tANI_U8 *) &pBeacon->extendedRates,
                     pBeacon->extendedRates.numRates + 1);
    else
        pPeer->extendedRates.numRates = 0;

    // TBD copy EDCA parameters
    // pPeer->edcaParams;

    pPeer->next = NULL;
} /*** end ibss_peer_collect() ***/

// handle change in peer qos/wme capabilities
static void
ibss_sta_caps_update(
    tpAniSirGlobal    pMac,
    tLimIbssPeerNode *pPeerNode,
    tpPESession       psessionEntry)
{
    tANI_U16      peerIdx;
    tpDphHashNode pStaDs;

    pPeerNode->beaconHBCount++; //Update beacon count.

    // if the peer node exists, update its qos capabilities
    if ((pStaDs = dphLookupHashEntry(pMac, pPeerNode->peerMacAddr, &peerIdx, &psessionEntry->dph.dphHashTable)) == NULL)
        return;


    //Update HT Capabilities
    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode))
    {
        pStaDs->mlmStaContext.htCapability = pPeerNode->htCapable;
        if (pPeerNode->htCapable)
        {
            pStaDs->htGreenfield = pPeerNode->htGreenfield;
            pStaDs->htSupportedChannelWidthSet =  pPeerNode->htSupportedChannelWidthSet;
            pStaDs->htSecondaryChannelOffset =  pPeerNode->htSecondaryChannelOffset;
            pStaDs->htMIMOPSState =             pPeerNode->htMIMOPSState;
            pStaDs->htMaxAmsduLength =  pPeerNode->htMaxAmsduLength;
            pStaDs->htAMpduDensity =             pPeerNode->htAMpduDensity;
            pStaDs->htDsssCckRate40MHzSupport = pPeerNode->htDsssCckRate40MHzSupport;
            pStaDs->htShortGI20Mhz = pPeerNode->htShortGI20Mhz;
            pStaDs->htShortGI40Mhz = pPeerNode->htShortGI40Mhz;
            pStaDs->htMaxRxAMpduFactor = pPeerNode->htMaxRxAMpduFactor;
            // In the future, may need to check for "delayedBA"
            // For now, it is IMMEDIATE BA only on ALL TID's
            pStaDs->baPolicyFlag = 0xFF;
        }
    }
#ifdef WLAN_FEATURE_11AC
    if ( IS_DOT11_MODE_VHT(psessionEntry->dot11mode) )
    {
        pStaDs->mlmStaContext.vhtCapability = pPeerNode->vhtCapable;
        if ( pPeerNode->vhtCapable )
        {
           pStaDs->vhtSupportedChannelWidthSet = pPeerNode->vhtSupportedChannelWidthSet;
        }
    }
#endif

    if(IS_DOT11_MODE_PROPRIETARY(psessionEntry->dot11mode) &&
      pPeerNode->aniIndicator)
    {
        pStaDs->aniPeer = pPeerNode->aniIndicator;
        pStaDs->propCapability = pPeerNode->propCapability;
    }


    // peer is 11e capable but is not 11e enabled yet
    // some STA's when joining Airgo IBSS, assert qos capability even when
    // they don't suport qos. however, they do not include the edca parameter
    // set. so let's check for edcaParam in addition to the qos capability
    if (pPeerNode->capabilityInfo.qos && (psessionEntry->limQosEnabled) && pPeerNode->edcaPresent)
    {
        pStaDs->qosMode    = 1;
        pStaDs->wmeEnabled = 0;
        if (! pStaDs->lleEnabled)
        {
            pStaDs->lleEnabled = 1;
            //dphSetACM(pMac, pStaDs);
        }
        return;
    }
    // peer is not 11e capable now but was 11e enabled earlier
    else if (pStaDs->lleEnabled)
    {
        pStaDs->qosMode      = 0;
        pStaDs->lleEnabled   = 0;
    }

    // peer is wme capable but is not wme enabled yet
    if (pPeerNode->wmeInfoPresent &&  psessionEntry->limWmeEnabled)
    {
        pStaDs->qosMode    = 1;
        pStaDs->lleEnabled = 0;
        if (! pStaDs->wmeEnabled)
        {
            pStaDs->wmeEnabled = 1;
            //dphSetACM(pMac, pStaDs);
        }
        return;
    }
    /* When the peer device supports EDCA parameters, then we were not 
      considering. Added this code when we saw that one of the Peer Device
      was advertising WMM param where we were not honouring that. CR# 210756
    */
    if (pPeerNode->wmeEdcaPresent && psessionEntry->limWmeEnabled) {
        pStaDs->qosMode    = 1;
        pStaDs->lleEnabled = 0;
        if (! pStaDs->wmeEnabled) {
            pStaDs->wmeEnabled = 1;
        }
        return;
    }

    // peer is not wme capable now but was wme enabled earlier
    else if (pStaDs->wmeEnabled)
    {
        pStaDs->qosMode      = 0;
        pStaDs->wmeEnabled   = 0;
    }

}

static void
ibss_sta_rates_update(
    tpAniSirGlobal   pMac,
    tpDphHashNode    pStaDs,
    tLimIbssPeerNode *pPeer,
    tpPESession       psessionEntry)
{
#ifdef WLAN_FEATURE_11AC
    limPopulateMatchingRateSet(pMac, pStaDs, &pPeer->supportedRates,
                               &pPeer->extendedRates, pPeer->supportedMCSSet,
                               &pStaDs->mlmStaContext.propRateSet,psessionEntry, &pPeer->VHTCaps);
#else
    // Populate supported rateset
    limPopulateMatchingRateSet(pMac, pStaDs, &pPeer->supportedRates,
                               &pPeer->extendedRates, pPeer->supportedMCSSet,
                               &pStaDs->mlmStaContext.propRateSet,psessionEntry);
#endif

    pStaDs->mlmStaContext.capabilityInfo = pPeer->capabilityInfo;
} /*** end ibss_sta_info_update() ***/

/**
 * ibss_sta_info_update
 *
 *FUNCTION:
 * This is called to program both SW & Polaris context
 * for peer in IBSS.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac   - Pointer to Global MAC structure
 * @param  pStaDs - Pointer to DPH node
 * @param  pPeer  - Pointer to IBSS peer node
 *
 * @return None
 */

static void
ibss_sta_info_update(
    tpAniSirGlobal   pMac,
    tpDphHashNode    pStaDs,
    tLimIbssPeerNode *pPeer,
    tpPESession psessionEntry)
{
    pStaDs->staType = STA_ENTRY_PEER;
    ibss_sta_caps_update(pMac, pPeer,psessionEntry);
    ibss_sta_rates_update(pMac, pStaDs, pPeer,psessionEntry);
} /*** end ibss_sta_info_update() ***/

static void
ibss_coalesce_free(
    tpAniSirGlobal pMac)
{
    if (pMac->lim.ibssInfo.pHdr != NULL)
        vos_mem_free(pMac->lim.ibssInfo.pHdr);
    if (pMac->lim.ibssInfo.pBeacon != NULL)
        vos_mem_free(pMac->lim.ibssInfo.pBeacon);

    pMac->lim.ibssInfo.pHdr    = NULL;
    pMac->lim.ibssInfo.pBeacon = NULL;
}

/*
 * save the beacon params for use when adding the bss
 */
static void
ibss_coalesce_save(
    tpAniSirGlobal      pMac,
    tpSirMacMgmtHdr     pHdr,
    tpSchBeaconStruct   pBeacon)
{
    // get rid of any saved info
    ibss_coalesce_free(pMac);

    pMac->lim.ibssInfo.pHdr = vos_mem_malloc(sizeof(*pHdr));
    if (NULL == pMac->lim.ibssInfo.pHdr)
    {
        PELOGE(limLog(pMac, LOGE, FL("ibbs-save: Failed malloc pHdr"));)
        return;
    }
    pMac->lim.ibssInfo.pBeacon = vos_mem_malloc(sizeof(*pBeacon));
    if (NULL == pMac->lim.ibssInfo.pBeacon)
    {
        PELOGE(limLog(pMac, LOGE, FL("ibbs-save: Failed malloc pBeacon"));)
        ibss_coalesce_free(pMac);
        return;
    }

    vos_mem_copy(pMac->lim.ibssInfo.pHdr, pHdr, sizeof(*pHdr));
    vos_mem_copy(pMac->lim.ibssInfo.pBeacon, pBeacon, sizeof(*pBeacon));
}

/*
 * tries to add a new entry to dph hash node
 * if necessary, an existing entry is eliminated
 */
static tSirRetStatus
ibss_dph_entry_add(
    tpAniSirGlobal  pMac,
    tSirMacAddr     peerAddr,
    tpDphHashNode   *ppSta,
    tpPESession     psessionEntry)
{
    tANI_U16      peerIdx;
    tpDphHashNode pStaDs;

    *ppSta = NULL;

    pStaDs = dphLookupHashEntry(pMac, peerAddr, &peerIdx, &psessionEntry->dph.dphHashTable);
    if (pStaDs != NULL)
    {
        /* Trying to add context for already existing STA in IBSS */
        PELOGE(limLog(pMac, LOGE, FL("STA exists already "));)
        limPrintMacAddr(pMac, peerAddr, LOGE);
        return eSIR_FAILURE;
    }

    /**
     * Assign an AID, delete context existing with that
     * AID and then add an entry to hash table maintained
     * by DPH module.
     */
    peerIdx = limAssignPeerIdx(pMac, psessionEntry);

    pStaDs = dphGetHashEntry(pMac, peerIdx, &psessionEntry->dph.dphHashTable);
    if (pStaDs)
    {
        (void) limDelSta(pMac, pStaDs, false /*asynchronous*/,psessionEntry);
        limDeleteDphHashEntry(pMac, pStaDs->staAddr, peerIdx,psessionEntry);
    }

    pStaDs = dphAddHashEntry(pMac, peerAddr, peerIdx, &psessionEntry->dph.dphHashTable);
    if (pStaDs == NULL)
    {
        // Could not add hash table entry
        PELOGE(limLog(pMac, LOGE, FL("could not add hash entry at DPH for peerIdx/aid=%d MACaddr:"), peerIdx);)
        limPrintMacAddr(pMac, peerAddr, LOGE);
        return eSIR_FAILURE;
    }

    *ppSta = pStaDs;
    return eSIR_SUCCESS;
}

// send a status change notification
static void
ibss_status_chg_notify(
    tpAniSirGlobal          pMac,
    tSirMacAddr             peerAddr,
    tANI_U16                staIndex,
    tANI_U8                 ucastSig, 
    tANI_U8                 bcastSig, 
    tANI_U16                status,
    tANI_U8                 sessionId)
{

    tLimIbssPeerNode *peerNode;
    tANI_U8 *beacon = NULL;
    tANI_U16 bcnLen = 0;


    peerNode = ibss_peer_find(pMac,peerAddr);
    if(peerNode != NULL)
    {
        if(peerNode->beacon == NULL) peerNode->beaconLen = 0;
        beacon = peerNode->beacon;
        bcnLen = peerNode->beaconLen;
        peerNode->beacon = NULL;
        peerNode->beaconLen = 0;
    }

    limSendSmeIBSSPeerInd(pMac,peerAddr, staIndex, ucastSig, bcastSig,
                          beacon, bcnLen, status, sessionId);

    if(beacon != NULL)
    {
        vos_mem_free(beacon);
    }
}


static void
ibss_bss_add(
    tpAniSirGlobal    pMac,
    tpPESession       psessionEntry)
{
    tLimMlmStartReq   mlmStartReq;
    tANI_U32          cfg;
    tpSirMacMgmtHdr   pHdr    = (tpSirMacMgmtHdr)   pMac->lim.ibssInfo.pHdr;
    tpSchBeaconStruct pBeacon = (tpSchBeaconStruct) pMac->lim.ibssInfo.pBeacon;
    tANI_U8 numExtRates = 0;

    if ((pHdr == NULL) || (pBeacon == NULL))
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to add BSS (no cached BSS info)"));)
        return;
    }

    vos_mem_copy(psessionEntry->bssId, pHdr->bssId,
                 sizeof(tSirMacAddr));

    #if 0
    if (cfgSetStr(pMac, WNI_CFG_BSSID, (tANI_U8 *) pHdr->bssId, sizeof(tSirMacAddr))
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not update BSSID at CFG"));
    #endif //TO SUPPORT BT-AMP

    sirCopyMacAddr(pHdr->bssId,psessionEntry->bssId);
    /* We need not use global Mac address since per seesion BSSID is available */
    //limSetBssid(pMac, pHdr->bssId);

#if 0
    if (wlan_cfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL, &cfg) != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("Can't read beacon interval"));
#endif //TO SUPPORT BT-AMP
    /* Copy beacon interval from sessionTable */
    cfg = psessionEntry->beaconParams.beaconInterval;
    if (cfg != pBeacon->beaconInterval)
        #if 0
        if (cfgSetInt(pMac, WNI_CFG_BEACON_INTERVAL, pBeacon->beaconInterval)
            != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("Can't update beacon interval"));
        #endif//TO SUPPORT BT-AMP
        psessionEntry->beaconParams.beaconInterval = pBeacon->beaconInterval;

    /* This function ibss_bss_add (and hence the below code) is only called during ibss coalescing. We need to 
     * adapt to peer's capability with respect to short slot time. Changes have been made to limApplyConfiguration() 
     * so that the IBSS doesnt blindly start with short slot = 1. If IBSS start is part of coalescing then it will adapt
     * to peer's short slot using code below.
     */
    /* If cfg is already set to current peer's capability then no need to set it again */
    if (psessionEntry->shortSlotTimeSupported != pBeacon->capabilityInfo.shortSlotTime)
    {
        psessionEntry->shortSlotTimeSupported = pBeacon->capabilityInfo.shortSlotTime;
    }
    vos_mem_copy((tANI_U8 *) &psessionEntry->pLimStartBssReq->operationalRateSet,
                 (tANI_U8 *) &pBeacon->supportedRates,
                  pBeacon->supportedRates.numRates);

    #if 0
    if (cfgSetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
           (tANI_U8 *) &pMac->lim.gpLimStartBssReq->operationalRateSet.rate,
           pMac->lim.gpLimStartBssReq->operationalRateSet.numRates)
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not update OperRateset at CFG"));
    #endif //TO SUPPORT BT-AMP

    /**
    * WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET CFG needs to be reset, when
    * there is no extended rate IE present in beacon. This is especially important when
    * supportedRateSet IE contains all the extended rates as well and STA decides to coalesce.
    * In this IBSS coalescing scenario LIM will tear down the BSS and Add a new one. So LIM needs to
    * reset this CFG, just in case CSR originally had set this CFG when IBSS was started from the local profile.
    * If IBSS was started by CSR from the BssDescription, then it would reset this CFG before StartBss is issued.
    * The idea is that the count of OpRateSet and ExtendedOpRateSet rates should not be more than 12.
    */

    if(pBeacon->extendedRatesPresent)
        numExtRates = pBeacon->extendedRates.numRates;
    if (cfgSetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
           (tANI_U8 *) &pBeacon->extendedRates.rate, numExtRates) != eSIR_SUCCESS)
    {
            limLog(pMac, LOGP, FL("could not update ExtendedOperRateset at CFG"));
        return;
    } 


    /*
    * Each IBSS node will advertise its own HT Capabilities instead of adapting to the Peer's capabilities
    * If we don't do this then IBSS may not go back to full capabilities when the STA with lower capabilities
    * leaves the IBSS.  e.g. when non-CB STA joins an IBSS and then leaves, the IBSS will be stuck at non-CB mode
    * even though all the nodes are capable of doing CB.
    * so it is decided to leave the self HT capabilties intact. This may change if some issues are found in interop.
    */
    vos_mem_set((void *) &mlmStartReq, sizeof(mlmStartReq), 0);

    vos_mem_copy(mlmStartReq.bssId, pHdr->bssId, sizeof(tSirMacAddr));
    mlmStartReq.rateSet.numRates = psessionEntry->pLimStartBssReq->operationalRateSet.numRates;
    vos_mem_copy(&mlmStartReq.rateSet.rate[0],
                 &psessionEntry->pLimStartBssReq->operationalRateSet.rate[0],
                 mlmStartReq.rateSet.numRates);
    mlmStartReq.bssType             = eSIR_IBSS_MODE;
    mlmStartReq.beaconPeriod        = pBeacon->beaconInterval;
    mlmStartReq.nwType              = psessionEntry->pLimStartBssReq->nwType; //psessionEntry->nwType is also OK????
    mlmStartReq.htCapable           = psessionEntry->htCapability;
    mlmStartReq.htOperMode          = pMac->lim.gHTOperMode;
    mlmStartReq.dualCTSProtection   = pMac->lim.gHTDualCTSProtection;
    mlmStartReq.txChannelWidthSet   = psessionEntry->htRecommendedTxWidthSet;

    #if 0
    if (wlan_cfgGetInt(pMac, WNI_CFG_CURRENT_CHANNEL, &cfg) != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("CurrentChannel CFG get fialed!"));
    #endif

    //mlmStartReq.channelNumber       = (tSirMacChanNum) cfg;

    /* reading the channel num from session Table */
    mlmStartReq.channelNumber = psessionEntry->currentOperChannel;

    mlmStartReq.cbMode              = psessionEntry->pLimStartBssReq->cbMode;

    // Copy the SSID for RxP filtering based on SSID.
    vos_mem_copy((tANI_U8 *) &mlmStartReq.ssId,
                 (tANI_U8 *) &psessionEntry->pLimStartBssReq->ssId,
                  psessionEntry->pLimStartBssReq->ssId.length + 1);

    limLog(pMac, LOG1, FL("invoking ADD_BSS as part of coalescing!"));
    if (limMlmAddBss(pMac, &mlmStartReq,psessionEntry) != eSIR_SME_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("AddBss failure"));)
        return;
    }

    // Update fields in Beacon
    if (schSetFixedBeaconFields(pMac,psessionEntry) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("*** Unable to set fixed Beacon fields ***"));)
        return;
    }

}



/* delete the current BSS */
static void
ibss_bss_delete(
    tpAniSirGlobal pMac,
    tpPESession    psessionEntry)
{
    tSirRetStatus status;
    PELOGW(limLog(pMac, LOGW, FL("Initiating IBSS Delete BSS"));)
    if (psessionEntry->limMlmState != eLIM_MLM_BSS_STARTED_STATE)
    {
        limLog(pMac, LOGW, FL("Incorrect LIM MLM state for delBss (%d)"),
               psessionEntry->limMlmState);
        return;
    }
    status = limDelBss(pMac, NULL, psessionEntry->bssIdx, psessionEntry);
    if (status != eSIR_SUCCESS)
        PELOGE(limLog(pMac, LOGE, FL("delBss failed for bss %d"), psessionEntry->bssIdx);)
}

/**
 * limIbssInit
 *
 *FUNCTION:
 * This function is called while starting an IBSS
 * to initialize list used to maintain IBSS peers.
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
limIbssInit(
    tpAniSirGlobal pMac)
{
    //pMac->lim.gLimIbssActive = 0;
    pMac->lim.gLimIbssCoalescingHappened = 0;
    pMac->lim.gLimIbssPeerList = NULL;
    pMac->lim.gLimNumIbssPeers = 0;

    // ibss info - params for which ibss to join while coalescing
    vos_mem_set(&pMac->lim.ibssInfo, sizeof(tAniSirLimIbss), 0);
} /*** end limIbssInit() ***/

/**
 * limIbssDeleteAllPeers
 *
 *FUNCTION:
 * This function is called to delete all peers.
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

void limIbssDeleteAllPeers( tpAniSirGlobal pMac ,tpPESession psessionEntry)
{
    tLimIbssPeerNode    *pCurrNode, *pTempNode;
    tpDphHashNode pStaDs;
    tANI_U16 peerIdx;

    pCurrNode = pTempNode = pMac->lim.gLimIbssPeerList;

    while (pCurrNode != NULL)
    {
        if (!pMac->lim.gLimNumIbssPeers)
        {
            limLog(pMac, LOGP,
               FL("Number of peers in the list is zero and node present"));
            return;
        }
        /* Delete the dph entry for the station
              * Since it is called to remove all peers, just delete from dph,
              * no need to do any beacon related params i.e., dont call limDeleteDphHashEntry
              */
        pStaDs = dphLookupHashEntry(pMac, pCurrNode->peerMacAddr, &peerIdx, &psessionEntry->dph.dphHashTable);
        if( pStaDs )
        {

            ibss_status_chg_notify( pMac, pCurrNode->peerMacAddr, pStaDs->staIndex, 
                                    pStaDs->ucUcastSig, pStaDs->ucBcastSig,
                                    eWNI_SME_IBSS_PEER_DEPARTED_IND, psessionEntry->smeSessionId );
            limReleasePeerIdx(pMac, peerIdx, psessionEntry);
            dphDeleteHashEntry(pMac, pStaDs->staAddr, peerIdx, &psessionEntry->dph.dphHashTable);
        }

        pTempNode = pCurrNode->next;

        /* TODO :Sessionize this code */
        /* Fix CR 227642: PeerList should point to the next node since the current node is being
                * freed in the next line. In ibss_peerfind in ibss_status_chg_notify above, we use this
                * peer list to find the next peer. So this list needs to be updated with the no of peers left
                * after each iteration in this while loop since one by one peers are deleted (freed) in this
                * loop causing the lim.gLimIbssPeerList to point to some freed memory.
                */
        pMac->lim.gLimIbssPeerList = pTempNode;

        if(pCurrNode->beacon)
        {
            vos_mem_free(pCurrNode->beacon);
        }
        vos_mem_free(pCurrNode);
        if (pMac->lim.gLimNumIbssPeers > 0) // be paranoid
            pMac->lim.gLimNumIbssPeers--;
        pCurrNode = pTempNode;
    }

    if (pMac->lim.gLimNumIbssPeers)
        limLog(pMac, LOGP, FL("Number of peers[%d] in the list is non-zero"),
                pMac->lim.gLimNumIbssPeers);

    pMac->lim.gLimNumIbssPeers = 0;
    pMac->lim.gLimIbssPeerList = NULL;

}
/**
 * limIbssDelete
 *
 *FUNCTION:
 * This function is called while tearing down an IBSS.
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
limIbssDelete(
    tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    limIbssDeleteAllPeers(pMac,psessionEntry);

    ibss_coalesce_free(pMac);
} /*** end limIbssDelete() ***/

/** Commenting this Code as from no where it is being invoked */
#if 0
/**
 * limIbssPeerDelete
 *
 *FUNCTION:
 * This may be called on a STA in IBSS to delete a peer
 * from the list.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  peerMacAddr - MAC address of the peer STA that
 *                       need to be deleted from peer list.
 *
 * @return None
 */

void
limIbssPeerDelete(tpAniSirGlobal pMac, tSirMacAddr macAddr)
{
    tLimIbssPeerNode    *pPrevNode, *pTempNode;

    pTempNode = pPrevNode = pMac->lim.gLimIbssPeerList;

    if (pTempNode == NULL)
        return;

    while (pTempNode != NULL)
    {
        if (vos_mem_compare((tANI_U8 *) macAddr,
                            (tANI_U8 *) &pTempNode->peerMacAddr,
                            sizeof(tSirMacAddr)) )
        {
            // Found node to be deleted
            if (pMac->lim.gLimIbssPeerList == pTempNode) /** First Node to be deleted*/
                pMac->lim.gLimIbssPeerList = pTempNode->next;
            else
                pPrevNode->next = pTempNode->next;

            if(pTempNode->beacon)
            {
                vos_mem_free(pTempNode->beacon);
                pTempNode->beacon = NULL;
            }
            vos_mem_free(pTempNode);
            pMac->lim.gLimNumIbssPeers--;
            return;
        }

        pPrevNode = pTempNode;
        pTempNode = pTempNode->next;
    }

    // Should not be here
    PELOGE(limLog(pMac, LOGE, FL("peer not found in the list, addr= "));)
    limPrintMacAddr(pMac, macAddr, LOGE);
} /*** end limIbssPeerDelete() ***/

#endif


/** -------------------------------------------------------------
\fn limIbssSetProtection
\brief Decides all the protection related information.
\
\param  tpAniSirGlobal    pMac
\param  tSirMacAddr peerMacAddr
\param  tpUpdateBeaconParams pBeaconParams
\return None
  -------------------------------------------------------------*/
static void
limIbssSetProtection(tpAniSirGlobal pMac, tANI_U8 enable, tpUpdateBeaconParams pBeaconParams,  tpPESession psessionEntry)
{

    if(!pMac->lim.cfgProtection.fromllb)
    {
        limLog(pMac, LOG1, FL("protection from 11b is disabled"));
        return;
    }

    if (enable)
    {
        psessionEntry->gLim11bParams.protectionEnabled = true;
        if(false == psessionEntry->beaconParams.llbCoexist/*pMac->lim.llbCoexist*/)
        {
            PELOGE(limLog(pMac, LOGE, FL("=> IBSS: Enable Protection "));)
            pBeaconParams->llbCoexist = psessionEntry->beaconParams.llbCoexist = true;
            pBeaconParams->paramChangeBitmap |= PARAM_llBCOEXIST_CHANGED;
        }
    }
    else if (true == psessionEntry->beaconParams.llbCoexist/*pMac->lim.llbCoexist*/)
    {
        psessionEntry->gLim11bParams.protectionEnabled = false;
        PELOGE(limLog(pMac, LOGE, FL("===> IBSS: Disable protection "));)
        pBeaconParams->llbCoexist = psessionEntry->beaconParams.llbCoexist = false;
        pBeaconParams->paramChangeBitmap |= PARAM_llBCOEXIST_CHANGED;
    }
    return;
}


/** -------------------------------------------------------------
\fn limIbssUpdateProtectionParams
\brief Decides all the protection related information.
\
\param  tpAniSirGlobal    pMac
\param  tSirMacAddr peerMacAddr
\param  tpUpdateBeaconParams pBeaconParams
\return None
  -------------------------------------------------------------*/
static void
limIbssUpdateProtectionParams(tpAniSirGlobal pMac,
        tSirMacAddr peerMacAddr, tLimProtStaCacheType protStaCacheType,
        tpPESession psessionEntry)
{
  tANI_U32 i;

  limLog(pMac,LOG1, FL("A STA is associated:"));
  limLog(pMac,LOG1, FL("Addr : "));
  limPrintMacAddr(pMac, peerMacAddr, LOG1);

  for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
  {
      if (pMac->lim.protStaCache[i].active)
      {
          limLog(pMac, LOG1, FL("Addr: "));
          limPrintMacAddr(pMac, pMac->lim.protStaCache[i].addr, LOG1);

          if (vos_mem_compare(pMac->lim.protStaCache[i].addr,
              peerMacAddr, sizeof(tSirMacAddr)))
          {
              limLog(pMac, LOG1, FL("matching cache entry at %d already active."), i);
              return;
          }
      }
  }

  for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
  {
      if (!pMac->lim.protStaCache[i].active)
          break;
  }

  if (i >= LIM_PROT_STA_CACHE_SIZE)
  {
      PELOGE(limLog(pMac, LOGE, FL("No space in ProtStaCache"));)
      return;
  }

  vos_mem_copy(pMac->lim.protStaCache[i].addr,
               peerMacAddr,
               sizeof(tSirMacAddr));

  pMac->lim.protStaCache[i].protStaCacheType = protStaCacheType;
  pMac->lim.protStaCache[i].active = true;
  if(eLIM_PROT_STA_CACHE_TYPE_llB == protStaCacheType)
  {
      psessionEntry->gLim11bParams.numSta++;
  }
  else if(eLIM_PROT_STA_CACHE_TYPE_llG == protStaCacheType)
  {
      psessionEntry->gLim11gParams.numSta++;
  }
}


/** -------------------------------------------------------------
\fn limIbssDecideProtection
\brief Decides all the protection related information.
\
\param  tpAniSirGlobal    pMac
\param  tSirMacAddr peerMacAddr
\param  tpUpdateBeaconParams pBeaconParams
\return None
  -------------------------------------------------------------*/
static void
limIbssDecideProtection(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,  tpPESession psessionEntry)
{
    tSirRFBand rfBand = SIR_BAND_UNKNOWN;
    tANI_U32 phyMode;
    tLimProtStaCacheType protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_INVALID;

    pBeaconParams->paramChangeBitmap = 0;

    if(NULL == pStaDs)
    {
      PELOGE(limLog(pMac, LOGE, FL("pStaDs is NULL"));)
      return;
    }

    limGetRfBand(pMac, &rfBand, psessionEntry);
    if(SIR_BAND_2_4_GHZ== rfBand)
    {
        limGetPhyMode(pMac, &phyMode, psessionEntry);

        //We are 11G or 11n. Check if we need protection from 11b Stations.
        if ((phyMode == WNI_CFG_PHY_MODE_11G) || (psessionEntry->htCapability))
        {
            /* As we found in the past, it is possible that a 11n STA sends
             * Beacon with HT IE but not ERP IE.  So the absense of ERP IE
             * in the Beacon is not enough to conclude that STA is 11b. 
             */
            if ((pStaDs->erpEnabled == eHAL_CLEAR) &&
                (!pStaDs->mlmStaContext.htCapability))
            {
                protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llB;
                PELOGE(limLog(pMac, LOGE, FL("Enable protection from 11B"));)
                limIbssSetProtection(pMac, true, pBeaconParams,psessionEntry);
            }
        }
    }
    limIbssUpdateProtectionParams(pMac, pStaDs->staAddr, protStaCacheType, psessionEntry);
    return;
}


/**
 * limIbssStaAdd()
 *
 *FUNCTION:
 * This function is called to add an STA context in IBSS role
 * whenever a data frame is received from/for a STA that failed
 * hash lookup at DPH.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac       Pointer to Global MAC structure
 * @param  peerAdddr  MAC address of the peer being added
 * @return retCode    Indicates success or failure return code
 * @return
 */

tSirRetStatus
limIbssStaAdd(
    tpAniSirGlobal  pMac,
    void *pBody,
    tpPESession psessionEntry)
{
    tSirRetStatus       retCode = eSIR_SUCCESS;
    tpDphHashNode       pStaDs;
    tLimIbssPeerNode    *pPeerNode;
    tLimMlmStates       prevState;
    tSirMacAddr         *pPeerAddr = (tSirMacAddr *) pBody;
    tUpdateBeaconParams beaconParams; 

    vos_mem_set((tANI_U8 *) &beaconParams, sizeof(tUpdateBeaconParams), 0);

    if (pBody == 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("Invalid IBSS AddSta"));)
        return eSIR_FAILURE;
    }

    PELOGE(limLog(pMac, LOGE, FL("Rx Add-Ibss-Sta for MAC:"));)
    limPrintMacAddr(pMac, *pPeerAddr, LOGE);

    pPeerNode = ibss_peer_find(pMac, *pPeerAddr);
    if (NULL != pPeerNode)
    {
        retCode = ibss_dph_entry_add(pMac, *pPeerAddr, &pStaDs, psessionEntry);
        if (eSIR_SUCCESS == retCode)
        {
            prevState = pStaDs->mlmStaContext.mlmState;
            pStaDs->erpEnabled = pPeerNode->erpIePresent;

            ibss_sta_info_update(pMac, pStaDs, pPeerNode, psessionEntry);
            PELOGW(limLog(pMac, LOGW, FL("initiating ADD STA for the IBSS peer."));)
            retCode = limAddSta(pMac, pStaDs, false, psessionEntry);
            if (retCode != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("ibss-sta-add failed (reason %x)"),
                              retCode);)
                limPrintMacAddr(pMac, *pPeerAddr, LOGE);
                pStaDs->mlmStaContext.mlmState = prevState;
                dphDeleteHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId,
                                   &psessionEntry->dph.dphHashTable);
            }
            else
            {
                if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
                    limIbssDecideProtection(pMac, pStaDs, &beaconParams , psessionEntry);

                if(beaconParams.paramChangeBitmap)
                {
                    PELOGE(limLog(pMac, LOGE, FL("---> Update Beacon Params "));)
                    schSetFixedBeaconFields(pMac, psessionEntry);
                    beaconParams.bssIdx = psessionEntry->bssIdx;
                    limSendBeaconParams(pMac, &beaconParams, psessionEntry );
                }
            }
        }
        else
        {
            PELOGE(limLog(pMac, LOGE, FL("hashTblAdd failed (reason %x)"), retCode);)
            limPrintMacAddr(pMac, *pPeerAddr, LOGE);
        }
    }
    else
    {
        retCode = eSIR_FAILURE;
    }

    return retCode;
}

/* handle the response from HAL for an ADD STA request */
tSirRetStatus
limIbssAddStaRsp(
    tpAniSirGlobal  pMac,
    void *msg,tpPESession psessionEntry)
{
    tpDphHashNode   pStaDs;
    tANI_U16        peerIdx;
    tpAddStaParams  pAddStaParams = (tpAddStaParams) msg;

    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    if (pAddStaParams == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: ADD_STA_RSP with no body!"));)
        return eSIR_FAILURE;
    }

    pStaDs = dphLookupHashEntry(pMac, pAddStaParams->staMac, &peerIdx, &psessionEntry->dph.dphHashTable);
    if (pStaDs == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: ADD_STA_RSP for unknown MAC addr "));)
        limPrintMacAddr(pMac, pAddStaParams->staMac, LOGE);
        vos_mem_free(pAddStaParams);
        return eSIR_FAILURE;
    }

    if (pAddStaParams->status != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: ADD_STA_RSP error (%x) "), pAddStaParams->status);)
        limPrintMacAddr(pMac, pAddStaParams->staMac, LOGE);
        vos_mem_free(pAddStaParams);
        return eSIR_FAILURE;
    }

    pStaDs->bssId                  = pAddStaParams->bssIdx;
    pStaDs->staIndex               = pAddStaParams->staIdx;
    pStaDs->ucUcastSig             = pAddStaParams->ucUcastSig;
    pStaDs->ucBcastSig             = pAddStaParams->ucBcastSig;
    pStaDs->valid                  = 1;
    pStaDs->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;

    PELOGW(limLog(pMac, LOGW, FL("IBSS: sending IBSS_NEW_PEER msg to SME!"));)

    ibss_status_chg_notify(pMac, pAddStaParams->staMac, pStaDs->staIndex, 
                           pStaDs->ucUcastSig, pStaDs->ucBcastSig,
                           eWNI_SME_IBSS_NEW_PEER_IND,
                           psessionEntry->smeSessionId);
    vos_mem_free(pAddStaParams);

    return eSIR_SUCCESS;
}



void limIbssDelBssRspWhenCoalescing(tpAniSirGlobal  pMac,  void *msg,tpPESession psessionEntry)
{
   tpDeleteBssParams pDelBss = (tpDeleteBssParams) msg;

    PELOGW(limLog(pMac, LOGW, FL("IBSS: DEL_BSS_RSP Rcvd during coalescing!"));)

    if (pDelBss == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: DEL_BSS_RSP(coalesce) with no body!"));)
        goto end;
    }

    if (pDelBss->status != eHAL_STATUS_SUCCESS)
    {
        limLog(pMac, LOGE, FL("IBSS: DEL_BSS_RSP(coalesce) error (%x) Bss %d "),
               pDelBss->status, pDelBss->bssIdx);
        goto end;
    }
    //Delete peer entries.
    limIbssDeleteAllPeers(pMac,psessionEntry);

    /* add the new bss */
    ibss_bss_add(pMac,psessionEntry);

    end:
    if(pDelBss != NULL)
        vos_mem_free(pDelBss);
}



void limIbssAddBssRspWhenCoalescing(tpAniSirGlobal  pMac, void *msg, tpPESession pSessionEntry)
{
    tANI_U8           infoLen;
    tSirSmeNewBssInfo newBssInfo;

    tpAddBssParams pAddBss = (tpAddBssParams) msg;

    tpSirMacMgmtHdr   pHdr    = (tpSirMacMgmtHdr)   pMac->lim.ibssInfo.pHdr;
    tpSchBeaconStruct pBeacon = (tpSchBeaconStruct) pMac->lim.ibssInfo.pBeacon;

    if ((pHdr == NULL) || (pBeacon == NULL))
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to handle AddBssRspWhenCoalescing (no cached BSS info)"));)
        goto end;
    }

    // Inform Host of IBSS coalescing
    infoLen = sizeof(tSirMacAddr) + sizeof(tSirMacChanNum) +
              sizeof(tANI_U8) + pBeacon->ssId.length + 1;

    vos_mem_set((void *) &newBssInfo, sizeof(newBssInfo), 0);
    vos_mem_copy(newBssInfo.bssId, pHdr->bssId, sizeof(tSirMacAddr));
    newBssInfo.channelNumber = (tSirMacChanNum) pAddBss->currentOperChannel;
    vos_mem_copy((tANI_U8 *) &newBssInfo.ssId,
                 (tANI_U8 *) &pBeacon->ssId, pBeacon->ssId.length + 1);

    PELOGW(limLog(pMac, LOGW, FL("Sending JOINED_NEW_BSS notification to SME."));)

    limSendSmeWmStatusChangeNtf(pMac, eSIR_SME_JOINED_NEW_BSS,
                                (tANI_U32 *) &newBssInfo,
                                infoLen,pSessionEntry->smeSessionId);
    {
        //Configure beacon and send beacons to HAL
        limSendBeaconInd(pMac, pSessionEntry);
    }

 end:
    ibss_coalesce_free(pMac);
}



void
limIbssDelBssRsp(
    tpAniSirGlobal  pMac,
    void *msg,tpPESession psessionEntry)
{
    tSirResultCodes rc = eSIR_SME_SUCCESS;
    tpDeleteBssParams pDelBss = (tpDeleteBssParams) msg;
    tSirMacAddr             nullBssid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    if (pDelBss == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: DEL_BSS_RSP with no body!"));)
        rc = eSIR_SME_REFUSED;
        goto end;
    }

    if((psessionEntry = peFindSessionBySessionId(pMac,pDelBss->sessionId))==NULL)
    {
           limLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
           goto end;
    }


    /*
    * If delBss was issued as part of IBSS Coalescing, gLimIbssCoalescingHappened flag will be true.
    * BSS has to be added again in this scenario, so this case needs to be handled separately.
    * If delBss was issued as a result of trigger from SME_STOP_BSS Request, then limSme state changes to
    * 'IDLE' and gLimIbssCoalescingHappened flag will be false. In this case STOP BSS RSP has to be sent to SME.
    */
    if(true == pMac->lim.gLimIbssCoalescingHappened)
    {
       
        limIbssDelBssRspWhenCoalescing(pMac,msg,psessionEntry);
        return;
    }



    if (pDelBss->status != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: DEL_BSS_RSP error (%x) Bss %d "),
               pDelBss->status, pDelBss->bssIdx);)
        rc = eSIR_SME_STOP_BSS_FAILURE;
        goto end;
    }



    if(limSetLinkState(pMac, eSIR_LINK_IDLE_STATE, nullBssid,  
        psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("IBSS: DEL_BSS_RSP setLinkState failed"));)
        rc = eSIR_SME_REFUSED;
        goto end;
    }

    limIbssDelete(pMac,psessionEntry);

    dphHashTableClassInit(pMac, &psessionEntry->dph.dphHashTable);
    limDeletePreAuthList(pMac);

    psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;

    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

    psessionEntry->limSystemRole = eLIM_STA_ROLE;

    /* Change the short slot operating mode to Default (which is 1 for now) so that when IBSS starts next time with Libra
     * as originator, it picks up the default. This enables us to remove hard coding of short slot = 1 from limApplyConfiguration 
     */
    psessionEntry->shortSlotTimeSupported = WNI_CFG_SHORT_SLOT_TIME_STADEF;

    end:
    if(pDelBss != NULL)
        vos_mem_free(pDelBss);
    /* Delete PE session once BSS is deleted */
    if (NULL != psessionEntry) {
        limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, rc,psessionEntry->smeSessionId,psessionEntry->transactionId);
        peDeleteSession(pMac, psessionEntry);
        psessionEntry = NULL;
    }
}

static void
__limIbssSearchAndDeletePeer(tpAniSirGlobal    pMac,
                             tpPESession psessionEntry,
                             tSirMacAddr macAddr)
{
   tLimIbssPeerNode *pTempNode, *pPrevNode;
   tLimIbssPeerNode *pTempNextNode = NULL;
   tpDphHashNode     pStaDs=NULL;
   tANI_U16          peerIdx=0;
   tANI_U16          staIndex=0;
   tANI_U8           ucUcastSig;
   tANI_U8           ucBcastSig;

   pPrevNode = pTempNode  = pMac->lim.gLimIbssPeerList;

   limLog(pMac, LOG1, FL(" PEER ADDR :" MAC_ADDRESS_STR ),MAC_ADDR_ARRAY(macAddr));

   /** Compare Peer */
   while (NULL != pTempNode)
   {
      pTempNextNode = pTempNode->next;

      /* Delete the STA with MAC address */
      if (vos_mem_compare( (tANI_U8 *) macAddr,
               (tANI_U8 *) &pTempNode->peerMacAddr,
               sizeof(tSirMacAddr)) )
      {
         pStaDs = dphLookupHashEntry(pMac, macAddr,
               &peerIdx, &psessionEntry->dph.dphHashTable);
         if (pStaDs)
         {
            staIndex = pStaDs->staIndex;
            ucUcastSig = pStaDs->ucUcastSig;
            ucBcastSig = pStaDs->ucBcastSig;

            (void) limDelSta(pMac, pStaDs, false /*asynchronous*/, psessionEntry);
            limDeleteDphHashEntry(pMac, pStaDs->staAddr, peerIdx, psessionEntry);
            limReleasePeerIdx(pMac, peerIdx, psessionEntry);

            /* Send indication to upper layers */
            ibss_status_chg_notify(pMac, macAddr, staIndex,
                                   ucUcastSig, ucBcastSig,
                                   eWNI_SME_IBSS_PEER_DEPARTED_IND,
                                   psessionEntry->smeSessionId );
            if (pTempNode == pMac->lim.gLimIbssPeerList)
            {
               pMac->lim.gLimIbssPeerList = pTempNode->next;
               pPrevNode = pMac->lim.gLimIbssPeerList;
            }
            else
               pPrevNode->next = pTempNode->next;

            vos_mem_free(pTempNode);
            pMac->lim.gLimNumIbssPeers--;

            pTempNode = pTempNextNode;
            break;
         }
      }
      pPrevNode = pTempNode;
      pTempNode = pTempNextNode;
   }
   /*
    * if it is the last peer walking out, we better
    * we set IBSS state to inactive.
    */
   if (0 == pMac->lim.gLimNumIbssPeers)
   {
       VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
            "Last STA from IBSS walked out");
       psessionEntry->limIbssActive = false;
   }
}

/**
 * limIbssCoalesce()
 *
 *FUNCTION:
 * This function is called upon receiving Beacon/Probe Response
 * while operating in IBSS mode.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pBeacon - Parsed Beacon Frame structure
 * @param  pBD     - Pointer to received BD
 *
 * @return Status whether to process or ignore received Beacon Frame
 */

tSirRetStatus
limIbssCoalesce(
    tpAniSirGlobal      pMac,
    tpSirMacMgmtHdr     pHdr,
    tpSchBeaconStruct   pBeacon,
    tANI_U8              *pIEs,
    tANI_U32            ieLen,
    tANI_U16            fTsfLater,
    tpPESession         psessionEntry)
{
    tANI_U16            peerIdx;
    tSirMacAddr         currentBssId;
    tLimIbssPeerNode    *pPeerNode;
    tpDphHashNode       pStaDs;
    tUpdateBeaconParams beaconParams;

    vos_mem_set((tANI_U8 *)&beaconParams, sizeof(tUpdateBeaconParams), 0);

    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    limLog(pMac, LOG1, FL("Current BSSID :" MAC_ADDRESS_STR " Received BSSID :" MAC_ADDRESS_STR ),
                                  MAC_ADDR_ARRAY(currentBssId), MAC_ADDR_ARRAY(pHdr->bssId));

    /* Check for IBSS Coalescing only if Beacon is from different BSS */
    if ( !vos_mem_compare(currentBssId, pHdr->bssId, sizeof( tSirMacAddr ))
          && psessionEntry->isCoalesingInIBSSAllowed)
    {
       /*
        * If STA entry is already available in the LIM hash table, then it is
        * possible that the peer may have left and rejoined within the heartbeat
        * timeout. In the offloaded case with 32 peers, the HB timeout is whopping
        * 128 seconds. In that case, the FW will not let any frames come in until
        * atleast the last sequence number is received before the peer is left
        * Hence, if the coalescing peer is already there in the peer list and if
        * the BSSID matches then, invoke delSta() to cleanup the entries. We will
        * let the peer coalesce when we receive next beacon from the peer
        */
       pPeerNode = ibss_peer_find(pMac, pHdr->sa);
       if (NULL != pPeerNode)
       {
          __limIbssSearchAndDeletePeer (pMac, psessionEntry, pHdr->sa);
          PELOGW(limLog(pMac, LOGW,
               FL("** Peer attempting to reconnect before HB timeout, deleted **"));)
          return eSIR_LIM_IGNORE_BEACON;
       }

       if (! fTsfLater) // No Coalescing happened.
       {
          PELOGW(limLog(pMac, LOGW, FL("No Coalescing happened"));)
          return eSIR_LIM_IGNORE_BEACON;
       }
       /*
        * IBSS Coalescing happened.
        * save the received beacon, and delete the current BSS. The rest of the
        * processing will be done in the delBss response processing
        */
       pMac->lim.gLimIbssCoalescingHappened = true;
       PELOGW(limLog(pMac, LOGW, FL("IBSS Coalescing happened"));)
          ibss_coalesce_save(pMac, pHdr, pBeacon);
       limLog(pMac, LOGW, FL("Delete BSSID :" MAC_ADDRESS_STR ),
             MAC_ADDR_ARRAY(currentBssId));
       ibss_bss_delete(pMac,psessionEntry);
       return eSIR_SUCCESS;
    }
    else
    {
       if (!vos_mem_compare(currentBssId, pHdr->bssId, sizeof( tSirMacAddr )))
           return eSIR_LIM_IGNORE_BEACON;
    }


    // STA in IBSS mode and SSID matches with ours
    pPeerNode = ibss_peer_find(pMac, pHdr->sa);
    if (pPeerNode == NULL)
    {
        /* Peer not in the list - Collect BSS description & add to the list */
        tANI_U32      frameLen;
        tSirRetStatus retCode;

        /*
         * Limit the Max number of IBSS Peers allowed as the max
         * number of STA's allowed
         * pMac->lim.gLimNumIbssPeers will be increamented after exiting
         * this function. so we will add additional 1 to compare against
         * pMac->lim.gLimIbssStaLimit
         */
        if ((pMac->lim.gLimNumIbssPeers+1) >= pMac->lim.gLimIbssStaLimit)
        {   /*Print every 100th time */
            if (pMac->lim.gLimIbssRetryCnt % 100 == 0)
            {
               limLog(pMac, LOG1, FL("**** MAX STA LIMIT HAS REACHED ****"));
            }
            pMac->lim.gLimIbssRetryCnt++;
            return eSIR_LIM_MAX_STA_REACHED_ERROR;
        }
        PELOGW(limLog(pMac, LOGW, FL("IBSS Peer node does not exist, adding it***"));)
        frameLen = sizeof(tLimIbssPeerNode) + ieLen - sizeof(tANI_U32);

        pPeerNode = vos_mem_malloc((tANI_U16)frameLen);
        if (NULL == pPeerNode)
        {
            limLog(pMac, LOGP, FL("alloc fail (%d bytes) storing IBSS peer info"),
                   frameLen);
            return eSIR_MEM_ALLOC_FAILED;
        }

        pPeerNode->beacon = NULL;
        pPeerNode->beaconLen = 0;

        ibss_peer_collect(pMac, pBeacon, pHdr, pPeerNode,psessionEntry);
        pPeerNode->beacon = vos_mem_malloc(ieLen);
        if (NULL == pPeerNode->beacon)
        {
                PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store beacon"));)
        }
        else
        {
            vos_mem_copy(pPeerNode->beacon, pIEs, ieLen);
            pPeerNode->beaconLen = (tANI_U16)ieLen;
        }
        ibss_peer_add(pMac, pPeerNode);

        pStaDs = dphLookupHashEntry(pMac, pPeerNode->peerMacAddr, &peerIdx, &psessionEntry->dph.dphHashTable);
        if (pStaDs != NULL)
        {
            /// DPH node already exists for the peer
            limLog(pMac, LOG1, FL("DPH Node present for just learned peer"));
            limPrintMacAddr(pMac, pPeerNode->peerMacAddr, LOG1);
            ibss_sta_info_update(pMac, pStaDs, pPeerNode,psessionEntry);
            return eSIR_SUCCESS;
        }
        retCode = limIbssStaAdd(pMac, pPeerNode->peerMacAddr,psessionEntry);
        if (retCode != eSIR_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE, FL("lim-ibss-sta-add failed (reason %x)"), retCode);)
            limPrintMacAddr(pMac, pPeerNode->peerMacAddr, LOGE);
            return retCode;
        }

        // Decide protection mode
        pStaDs = dphLookupHashEntry(pMac, pPeerNode->peerMacAddr, &peerIdx, &psessionEntry->dph.dphHashTable);
        if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
            limIbssDecideProtection(pMac, pStaDs, &beaconParams, psessionEntry);

        if(beaconParams.paramChangeBitmap)
        {
            PELOGE(limLog(pMac, LOGE, FL("beaconParams.paramChangeBitmap=1 ---> Update Beacon Params "));)
            schSetFixedBeaconFields(pMac, psessionEntry);
            beaconParams.bssIdx = psessionEntry->bssIdx;
            limSendBeaconParams(pMac, &beaconParams, psessionEntry );
        }
    }
    else
        ibss_sta_caps_update(pMac, pPeerNode,psessionEntry);

    if (psessionEntry->limSmeState != eLIM_SME_NORMAL_STATE)
        return eSIR_SUCCESS;

    // Received Beacon from same IBSS we're
    // currently part of. Inform Roaming algorithm
    // if not already that IBSS is active.
    if (psessionEntry->limIbssActive == false)
    {
        limResetHBPktCount(psessionEntry);
        PELOGW(limLog(pMac, LOGW, FL("Partner joined our IBSS, Sending IBSS_ACTIVE Notification to SME"));)
        psessionEntry->limIbssActive = true;
        limSendSmeWmStatusChangeNtf(pMac, eSIR_SME_IBSS_ACTIVE, NULL, 0, psessionEntry->smeSessionId);
        limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId, eLIM_HEART_BEAT_TIMER));
        if (limActivateHearBeatTimer(pMac, psessionEntry) != TX_SUCCESS)
            limLog(pMac, LOGP, FL("could not activate Heartbeat timer"));
    }

    return eSIR_SUCCESS;
} /*** end limHandleIBSScoalescing() ***/


void limIbssHeartBeatHandle(tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    tLimIbssPeerNode *pTempNode, *pPrevNode;
    tLimIbssPeerNode *pTempNextNode = NULL;
    tANI_U16      peerIdx=0;
    tpDphHashNode pStaDs=0;
    tANI_U32 threshold=0;
    tANI_U16 staIndex=0;
    tANI_U8 ucUcastSig=0;
    tANI_U8 ucBcastSig=0;

    /** MLM BSS is started and if PE in scanmode then MLM state will be waiting for probe resp.
     *  If Heart beat timeout triggers during this corner case then we need to reactivate HeartBeat timer 
     */
    if(psessionEntry->limMlmState != eLIM_MLM_BSS_STARTED_STATE) {
        /****** 
         * Note: Use this code once you have converted all  
         * limReactivateHeartBeatTimer() calls to 
         * limReactivateTimer() calls.
         * 
         ******/
        //limReactivateTimer(pMac, eLIM_HEART_BEAT_TIMER, psessionEntry);
        limReactivateHeartBeatTimer(pMac, psessionEntry);
        return;
    }
    /** If LinkMonitor is Disabled */
    if(!pMac->sys.gSysEnableLinkMonitorMode)
        return;

    pPrevNode = pTempNode  = pMac->lim.gLimIbssPeerList;
    threshold = (pMac->lim.gLimNumIbssPeers / 4 ) + 1;

    /** Monitor the HeartBeat with the Individual PEERS in the IBSS */
    while (pTempNode != NULL)
    {
        pTempNextNode = pTempNode->next;
        if(pTempNode->beaconHBCount) //There was a beacon for this peer during heart beat.
        {
            pTempNode->beaconHBCount = 0;
            pTempNode->heartbeatFailure = 0;
        }
        else //There wasnt any beacon received during heartbeat timer.
        {
            pTempNode->heartbeatFailure++;
            PELOGE(limLog(pMac, LOGE, FL("Heartbeat fail = %d  thres = %d"), pTempNode->heartbeatFailure, pMac->lim.gLimNumIbssPeers);)
            if(pTempNode->heartbeatFailure >= threshold )
            {
                //Remove this entry from the list.
                pStaDs = dphLookupHashEntry(pMac, pTempNode->peerMacAddr, &peerIdx, &psessionEntry->dph.dphHashTable);
                if (pStaDs)
                {
                    staIndex = pStaDs->staIndex;
                    ucUcastSig = pStaDs->ucUcastSig;
                    ucBcastSig = pStaDs->ucBcastSig;

                    (void) limDelSta(pMac, pStaDs, false /*asynchronous*/,psessionEntry);
                    limDeleteDphHashEntry(pMac, pStaDs->staAddr, peerIdx,psessionEntry);
                    limReleasePeerIdx(pMac, peerIdx, psessionEntry);
                    //Send indication.
                    ibss_status_chg_notify( pMac, pTempNode->peerMacAddr, staIndex, 
                                            ucUcastSig, ucBcastSig,
                                            eWNI_SME_IBSS_PEER_DEPARTED_IND,
                                            psessionEntry->smeSessionId );
                }
                if(pTempNode == pMac->lim.gLimIbssPeerList)
                {
                    pMac->lim.gLimIbssPeerList = pTempNode->next;
                    pPrevNode = pMac->lim.gLimIbssPeerList;
                }
                else
                    pPrevNode->next = pTempNode->next;

                vos_mem_free(pTempNode);
                pMac->lim.gLimNumIbssPeers--;

                pTempNode = pTempNextNode; //Since we deleted current node, prevNode remains same.
                continue;
             }
         }

        pPrevNode = pTempNode;
        pTempNode = pTempNextNode;
    }

    /** General IBSS Activity Monitor, check if in IBSS Mode we are received any Beacons */
    if(pMac->lim.gLimNumIbssPeers)
    {
        if(psessionEntry->LimRxedBeaconCntDuringHB < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL)
            pMac->lim.gLimHeartBeatBeaconStats[psessionEntry->LimRxedBeaconCntDuringHB]++;
        else
            pMac->lim.gLimHeartBeatBeaconStats[0]++;

        limReactivateHeartBeatTimer(pMac, psessionEntry);

        // Reset number of beacons received
        limResetHBPktCount(psessionEntry);
        return;
    }
    else
    {

        PELOGW(limLog(pMac, LOGW, FL("Heartbeat Failure"));)
        pMac->lim.gLimHBfailureCntInLinkEstState++;

        if (psessionEntry->limIbssActive == true)
        {
            // We don't receive Beacon frames from any
            // other STA in IBSS. Announce IBSS inactive
            // to Roaming algorithm
            PELOGW(limLog(pMac, LOGW, FL("Alone in IBSS"));)
            psessionEntry->limIbssActive = false;

            limSendSmeWmStatusChangeNtf(pMac, eSIR_SME_IBSS_INACTIVE,
                                          NULL, 0, psessionEntry->smeSessionId);
        }
    }
}


/** -------------------------------------------------------------
\fn limIbssDecideProtectionOnDelete
\brief Decides all the protection related information.
\
\param  tpAniSirGlobal    pMac
\param  tSirMacAddr peerMacAddr
\param  tpUpdateBeaconParams pBeaconParams
\return None
  -------------------------------------------------------------*/
void
limIbssDecideProtectionOnDelete(tpAniSirGlobal pMac, 
      tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,  tpPESession psessionEntry)
{
    tANI_U32 phyMode;
    tHalBitVal erpEnabled = eHAL_CLEAR;
    tSirRFBand rfBand = SIR_BAND_UNKNOWN;
    tANI_U32 i;    
    
    if(NULL == pStaDs)
      return;

    limGetRfBand(pMac, &rfBand, psessionEntry);
    if(SIR_BAND_2_4_GHZ == rfBand)
    {
        limGetPhyMode(pMac, &phyMode, psessionEntry);
        erpEnabled = pStaDs->erpEnabled;
        //we are HT or 11G and 11B station is getting deleted.
        if ( ((phyMode == WNI_CFG_PHY_MODE_11G) || psessionEntry->htCapability)
              && (erpEnabled == eHAL_CLEAR))
        {
            PELOGE(limLog(pMac, LOGE, FL("(%d) A legacy STA is disassociated. Addr is "),
                   psessionEntry->gLim11bParams.numSta);
            limPrintMacAddr(pMac, pStaDs->staAddr, LOGE);)
            if (psessionEntry->gLim11bParams.numSta > 0)
            {
                for (i=0; i<LIM_PROT_STA_CACHE_SIZE; i++)
                {
                    if (pMac->lim.protStaCache[i].active)
                    {
                        if (vos_mem_compare(pMac->lim.protStaCache[i].addr,
                                            pStaDs->staAddr, sizeof(tSirMacAddr)))
                        {
                            psessionEntry->gLim11bParams.numSta--;
                            pMac->lim.protStaCache[i].active = false;
                            break;
                        }
                    }
                }
            }

            if (psessionEntry->gLim11bParams.numSta == 0)
            {
                PELOGE(limLog(pMac, LOGE, FL("No more 11B STA exists. Disable protection. "));)
                limIbssSetProtection(pMac, false, pBeaconParams,psessionEntry);
            }
        }
    }
}

/** -----------------------------------------------------------------
\fn __limIbssPeerInactivityHandler
\brief Internal function. Deletes FW indicated peer which is inactive
\
\param  tpAniSirGlobal    pMac
\param  tpPESession       psessionEntry
\param  tpSirIbssPeerInactivityInd peerInactivityInd
\return None
  -----------------------------------------------------------------*/
static void
__limIbssPeerInactivityHandler(tpAniSirGlobal    pMac,
                               tpPESession psessionEntry,
                               tpSirIbssPeerInactivityInd peerInactivityInd)
{
   if(psessionEntry->limMlmState != eLIM_MLM_BSS_STARTED_STATE)
   {
      limReactivateHeartBeatTimer(pMac, psessionEntry);
      return;
   }

   /* delete the peer for which heartbeat is observed */
   __limIbssSearchAndDeletePeer (pMac, psessionEntry, peerInactivityInd->peerAddr);

}

/** -------------------------------------------------------------
\fn limProcessIbssPeerInactivity
\brief Peer inactivity message handler
\
\param  tpAniSirGlobal    pMac
\param  void*             buf
\return None
  -------------------------------------------------------------*/
void
limProcessIbssPeerInactivity(tpAniSirGlobal pMac, void *buf)
{
   /*
    * --------------- HEARTBEAT OFFLOAD CASE ------------------
    * This message handler is executed when the firmware identifies
    * inactivity from one or more peer devices. We will come here
    * for every inactive peer device
    */
   tANI_U8       i;

   tSirIbssPeerInactivityInd *peerInactivityInd =
      (tSirIbssPeerInactivityInd *) buf;

   /*
    * If IBSS is not started or heartbeat offload is not enabled
    * we should not handle this request
    */
   if (eLIM_STA_IN_IBSS_ROLE != pMac->lim.gLimSystemRole &&
         !IS_IBSS_HEARTBEAT_OFFLOAD_FEATURE_ENABLE)
   {
      return;
   }

   /** If LinkMonitor is Disabled */
   if (!pMac->sys.gSysEnableLinkMonitorMode)
   {
      return;
   }

   for (i = 0; i < pMac->lim.maxBssId; i++)
   {
      if (VOS_TRUE == pMac->lim.gpSession[i].valid &&
            eSIR_IBSS_MODE == pMac->lim.gpSession[i].bssType)
      {
         __limIbssPeerInactivityHandler(pMac,
               &pMac->lim.gpSession[i],
               peerInactivityInd);
         break;
      }
   }
}

