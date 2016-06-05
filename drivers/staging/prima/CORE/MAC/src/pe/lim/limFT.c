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

#ifdef WLAN_FEATURE_VOWIFI_11R
/**=========================================================================
  
  \brief implementation for PE 11r VoWiFi FT Protocol
  
  ========================================================================*/

/* $Header$ */


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <limSendMessages.h>
#include <limTypes.h>
#include <limFT.h>
#include <limFTDefs.h>
#include <limUtils.h>
#include <limPropExtsUtils.h>
#include <limAssocUtils.h>
#include <limSession.h>
#include <limAdmitControl.h>
#include "wmmApsd.h"
#include "vos_utils.h"

#define LIM_FT_RIC_BA_SSN                       1
#define LIM_FT_RIC_BA_DIALOG_TOKEN_TID_0         248
#define LIM_FT_RIC_DESCRIPTOR_RESOURCE_TYPE_BA  1
#define LIM_FT_RIC_DESCRIPTOR_MAX_VAR_DATA_LEN   255

/*--------------------------------------------------------------------------
  Initialize the FT variables. 
  ------------------------------------------------------------------------*/
void limFTOpen(tpAniSirGlobal pMac)
{
    pMac->ft.ftPEContext.pFTPreAuthReq = NULL;
    pMac->ft.ftPEContext.psavedsessionEntry = NULL;
}

/*--------------------------------------------------------------------------
  Cleanup FT variables. 
  ------------------------------------------------------------------------*/
void limFTCleanup(tpAniSirGlobal pMac)
{
    if (pMac->ft.ftPEContext.pFTPreAuthReq) 
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Freeing pFTPreAuthReq= %p",
            __func__, pMac->ft.ftPEContext.pFTPreAuthReq);) 
#endif
        if (pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription)
        {
            vos_mem_free(pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription);
            pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription = NULL;
        }
        vos_mem_free(pMac->ft.ftPEContext.pFTPreAuthReq);
        pMac->ft.ftPEContext.pFTPreAuthReq = NULL;
    }

    // This is the old session, should be deleted else where.
    // We should not be cleaning it here, just set it to NULL.
    if (pMac->ft.ftPEContext.psavedsessionEntry)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Setting psavedsessionEntry= %p to NULL",
            __func__, pMac->ft.ftPEContext.psavedsessionEntry);) 
#endif
        pMac->ft.ftPEContext.psavedsessionEntry = NULL;
    }

    // This is the extra session we added as part of Auth resp
    // clean it up.
    if (pMac->ft.ftPEContext.pftSessionEntry)
    {
        if ((((tpPESession)(pMac->ft.ftPEContext.pftSessionEntry))->valid) &&
            (((tpPESession)(pMac->ft.ftPEContext.pftSessionEntry))->limSmeState == eLIM_SME_WT_REASSOC_STATE))
        {
            PELOGE(limLog( pMac, LOGE, "%s: Deleting Preauth Session %d", __func__, ((tpPESession)pMac->ft.ftPEContext.pftSessionEntry)->peSessionId);)
            peDeleteSession(pMac, pMac->ft.ftPEContext.pftSessionEntry);
        }
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Setting pftSessionEntry= %p to NULL",
            __func__, pMac->ft.ftPEContext.pftSessionEntry);)
#endif
        pMac->ft.ftPEContext.pftSessionEntry = NULL;
    }

    if (pMac->ft.ftPEContext.pAddBssReq)
    {
        vos_mem_zero(pMac->ft.ftPEContext.pAddBssReq, sizeof(tAddBssParams));
        vos_mem_free(pMac->ft.ftPEContext.pAddBssReq);
        pMac->ft.ftPEContext.pAddBssReq = NULL;
    }

    if (pMac->ft.ftPEContext.pAddStaReq)
    {
        vos_mem_free(pMac->ft.ftPEContext.pAddStaReq);
        pMac->ft.ftPEContext.pAddStaReq = NULL;
    }

    vos_mem_zero(&pMac->ft.ftPEContext, sizeof(tftPEContext));
}

/*--------------------------------------------------------------------------
  Init FT variables. 
  ------------------------------------------------------------------------*/
void limFTInit(tpAniSirGlobal pMac)
{
    if (pMac->ft.ftPEContext.pFTPreAuthReq) 
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Freeing pFTPreAuthReq= %p",
            __func__, pMac->ft.ftPEContext.pFTPreAuthReq);) 
#endif
        if (pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription)
        {
            vos_mem_free(pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription);
            pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription = NULL;
        }

        vos_mem_free(pMac->ft.ftPEContext.pFTPreAuthReq);
        pMac->ft.ftPEContext.pFTPreAuthReq = NULL;
    }

    // This is the old session, should be deleted else where.
    // We should not be cleaning it here, just set it to NULL.
    if (pMac->ft.ftPEContext.psavedsessionEntry)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Setting psavedsessionEntry= %p to NULL",
            __func__, pMac->ft.ftPEContext.psavedsessionEntry);) 
#endif
        pMac->ft.ftPEContext.psavedsessionEntry = NULL;
    }

    // This is the extra session we added as part of Auth resp
    // clean it up.
    if (pMac->ft.ftPEContext.pftSessionEntry)
    {

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Deleting session = %p ",
            __func__, pMac->ft.ftPEContext.pftSessionEntry);) 
#endif
        /* Delete the previous valid preauth pesession if it is still in
         * mMlmState= eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE
         * and limSmeState = eLIM_SME_WT_REASSOC_STATE. This means last
         * preauth didnt went through and its Session was not deleted.
         */
        if ((((tpPESession)(pMac->ft.ftPEContext.pftSessionEntry))->valid) &&
            (((tpPESession)(pMac->ft.ftPEContext.pftSessionEntry))->limSmeState
                                               == eLIM_SME_WT_REASSOC_STATE) &&
            (((tpPESession)(pMac->ft.ftPEContext.pftSessionEntry))->limMlmState
                                   == eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE) )
        {
            limLog( pMac, LOGE, FL("Deleting Preauth Session %d"),
               ((tpPESession)pMac->ft.ftPEContext.pftSessionEntry)->peSessionId);
            peDeleteSession(pMac, pMac->ft.ftPEContext.pftSessionEntry);
        }

        pMac->ft.ftPEContext.pftSessionEntry = NULL;
    }

    if (pMac->ft.ftPEContext.pAddBssReq)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Freeing AddBssReq = %p ",
            __func__, pMac->ft.ftPEContext.pAddBssReq);) 
#endif
        vos_mem_free(pMac->ft.ftPEContext.pAddBssReq);
        pMac->ft.ftPEContext.pAddBssReq = NULL;
    }


    if (pMac->ft.ftPEContext.pAddStaReq)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: Freeing AddStaReq = %p ",
            __func__, pMac->ft.ftPEContext.pAddStaReq);) 
#endif
        vos_mem_free(pMac->ft.ftPEContext.pAddStaReq);
        pMac->ft.ftPEContext.pAddStaReq = NULL;
    }

    pMac->ft.ftPEContext.ftPreAuthStatus = eSIR_SUCCESS; 

}

/*------------------------------------------------------------------
 * 
 * This is the handler after suspending the link.
 * We suspend the link and then now proceed to switch channel.
 *
 *------------------------------------------------------------------*/
void FTPreAuthSuspendLinkHandler(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data)
{
    tpPESession psessionEntry;
    
    // The link is suspended of not ?
    if (status != eHAL_STATUS_SUCCESS) 
    {
        PELOGE(limLog( pMac, LOGE, "%s: Returning ", __func__);)
        // Post the FT Pre Auth Response to SME
        limPostFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, (tpPESession)data);

        return;
    }

    psessionEntry = (tpPESession)data;
    // Suspended, now move to a different channel.
    // Perform some sanity check before proceeding.
    if ((pMac->ft.ftPEContext.pFTPreAuthReq) && psessionEntry)
    {
        limChangeChannelWithCallback(pMac, 
            pMac->ft.ftPEContext.pFTPreAuthReq->preAuthchannelNum,
            limPerformFTPreAuth, NULL, psessionEntry);
        return;
    }

    // Else return error.
    limPostFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
}


/*--------------------------------------------------------------------------
  In this function, we process the FT Pre Auth Req.
  We receive Pre-Auth
  Suspend link
  Register a call back
  In the call back, we will need to accept frames from the new bssid
  Send out the auth req to new AP.
  Start timer and when the timer is done or if we receive the Auth response
  We change channel
  Resume link
  ------------------------------------------------------------------------*/
int limProcessFTPreAuthReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    int bufConsumed = FALSE;
    tpPESession psessionEntry;
    tANI_U8 sessionId;

    // Now we are starting fresh make sure all's cleanup.
    limFTInit(pMac);
    // Can set it only after sending auth
    pMac->ft.ftPEContext.ftPreAuthStatus = eSIR_FAILURE;

    // We need information from the Pre-Auth Req. Lets save that
    pMac->ft.ftPEContext.pFTPreAuthReq = (tpSirFTPreAuthReq)pMsg->bodyptr;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog( pMac, LOG1, "%s: PRE Auth ft_ies_length=%02x%02x%02x", __func__,
        pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies[0],
        pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies[1],
        pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies[2]);)
#endif

    // Get the current session entry
    psessionEntry = peFindSessionByBssid(pMac, 
        pMac->ft.ftPEContext.pFTPreAuthReq->currbssId, &sessionId);
    if (psessionEntry == NULL)
    {
        PELOGE(limLog( pMac, LOGE, "%s: Unable to find session for the following bssid",
            __func__);)
        limPrintMacAddr( pMac, pMac->ft.ftPEContext.pFTPreAuthReq->currbssId, LOGE );
        // Post the FT Pre Auth Response to SME
        limPostFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, NULL);

        /* return FALSE, since the Pre-Auth Req will be freed in
         * limPostFTPreAuthRsp on failure
         */
        return bufConsumed;
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
        limDiagEventReport(pMac, WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT, psessionEntry, 0, 0);
#endif

    // Dont need to suspend if APs are in same channel
    if (psessionEntry->currentOperChannel != pMac->ft.ftPEContext.pFTPreAuthReq->preAuthchannelNum) 
    {
        // Need to suspend link only if the channels are different
        limLog(pMac, LOG1, FL(" Performing pre-auth on different"
               " channel (session %p)"), psessionEntry);
        limSuspendLink(pMac, eSIR_CHECK_ROAMING_SCAN, FTPreAuthSuspendLinkHandler, 
                       (tANI_U32 *)psessionEntry); 
    }
    else 
    {
        limLog(pMac, LOG1, FL(" Performing pre-auth on same"
               " channel (session %p)"), psessionEntry);
        // We are in the same channel. Perform pre-auth
        limPerformFTPreAuth(pMac, eHAL_STATUS_SUCCESS, NULL, psessionEntry);
    }

    return bufConsumed;
}

/*------------------------------------------------------------------
 * Send the Auth1 
 * Receive back Auth2
 *------------------------------------------------------------------*/
void limPerformFTPreAuth(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
    tpPESession psessionEntry)
{
    tSirMacAuthFrameBody authFrame;

    if (psessionEntry->is11Rconnection)
    {
        // Only 11r assoc has FT IEs.
        if (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies == NULL) 
        {
            PELOGE(limLog( pMac, LOGE,
                           "%s: FTIEs for Auth Req Seq 1 is absent",
                           __func__);)
            goto preauth_fail;
        }
    }
    if (status != eHAL_STATUS_SUCCESS) 
    {
        PELOGE(limLog( pMac, LOGE,
                       "%s: Change channel not successful for FT pre-auth",
                       __func__);)
        goto preauth_fail;
    }
    pMac->ft.ftPEContext.psavedsessionEntry = psessionEntry;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog(pMac, LOG1, FL("Entered wait auth2 state for FT"
           " (old session %p)"),
           pMac->ft.ftPEContext.psavedsessionEntry);
#endif


    if (psessionEntry->is11Rconnection)
    {
        // Now we are on the right channel and need to send out Auth1 and 
        // receive Auth2.
        authFrame.authAlgoNumber = eSIR_FT_AUTH; // Set the auth type to FT
    }
#if defined FEATURE_WLAN_ESE || defined FEATURE_WLAN_LFR
    else
    {
        // Will need to make isESEconnection a enum may be for further
        // improvements to this to match this algorithm number
        authFrame.authAlgoNumber = eSIR_OPEN_SYSTEM; // For now if its ESE and 11r FT.
    }
#endif
    authFrame.authTransactionSeqNumber = SIR_MAC_AUTH_FRAME_1;
    authFrame.authStatusCode = 0;

    // Start timer here to come back to operating channel.
    pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId = psessionEntry->peSessionId;
    if(TX_SUCCESS !=  tx_timer_activate(&pMac->lim.limTimers.gLimFTPreAuthRspTimer))
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, "%s: FT Auth Rsp Timer Start Failed", __func__);)
#endif
        pMac->ft.ftPEContext.psavedsessionEntry = NULL;
        goto preauth_fail;
    }
MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId, eLIM_FT_PREAUTH_RSP_TIMER));

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog( pMac, LOG1, "%s: FT Auth Rsp Timer Started", __func__);)
#endif

    limSendAuthMgmtFrame(pMac, &authFrame,
        pMac->ft.ftPEContext.pFTPreAuthReq->preAuthbssId,
        LIM_NO_WEP_IN_FC, psessionEntry, eSIR_FALSE);

    return;
preauth_fail:
    limHandleFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
    return;
}


/*------------------------------------------------------------------
 *
 * Create the new Add Bss Req to the new AP.
 * This will be used when we are ready to FT to the new AP.
 * The newly created ft Session entry is passed to this function
 *
 *------------------------------------------------------------------*/
tSirRetStatus limFTPrepareAddBssReq( tpAniSirGlobal pMac, 
    tANI_U8 updateEntry, tpPESession pftSessionEntry, 
    tpSirBssDescription bssDescription )
{
    tpAddBssParams pAddBssParams = NULL;
    tANI_U8 i;
    tANI_U8 chanWidthSupp = 0;
    tSchBeaconStruct *pBeaconStruct;

    pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
    if (NULL == pBeaconStruct)
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory for creating ADD_BSS") );
        return eSIR_MEM_ALLOC_FAILED;
    }

    // Package SIR_HAL_ADD_BSS_REQ message parameters
    pAddBssParams = vos_mem_malloc(sizeof( tAddBssParams ));
    if (NULL == pAddBssParams)
    {
        vos_mem_free(pBeaconStruct);
        limLog( pMac, LOGP,
                FL( "Unable to allocate memory for creating ADD_BSS" ));
        return (eSIR_MEM_ALLOC_FAILED);
    }
    
    vos_mem_set((tANI_U8 *) pAddBssParams, sizeof( tAddBssParams ), 0);


    limExtractApCapabilities( pMac,
        (tANI_U8 *) bssDescription->ieFields,
        limGetIElenFromBssDescription( bssDescription ), pBeaconStruct );

    if (pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideStaProtectionOnAssoc(pMac, pBeaconStruct, pftSessionEntry);

    vos_mem_copy(pAddBssParams->bssId, bssDescription->bssId,
                 sizeof(tSirMacAddr));

    // Fill in tAddBssParams selfMacAddr
    vos_mem_copy(pAddBssParams->selfMacAddr, pftSessionEntry->selfMacAddr,
                 sizeof(tSirMacAddr));

    pAddBssParams->bssType = pftSessionEntry->bssType;//eSIR_INFRASTRUCTURE_MODE;
    pAddBssParams->operMode = BSS_OPERATIONAL_MODE_STA;

    pAddBssParams->beaconInterval = bssDescription->beaconInterval;
    
    pAddBssParams->dtimPeriod = pBeaconStruct->tim.dtimPeriod;
    pAddBssParams->updateBss = updateEntry;


    pAddBssParams->cfParamSet.cfpCount = pBeaconStruct->cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod = pBeaconStruct->cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration = pBeaconStruct->cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining = pBeaconStruct->cfParamSet.cfpDurRemaining;


    pAddBssParams->rateSet.numRates = pBeaconStruct->supportedRates.numRates;
    vos_mem_copy(pAddBssParams->rateSet.rate,
                 pBeaconStruct->supportedRates.rate, pBeaconStruct->supportedRates.numRates);

    pAddBssParams->nwType = bssDescription->nwType;
    
    pAddBssParams->shortSlotTimeSupported = (tANI_U8)pBeaconStruct->capabilityInfo.shortSlotTime; 
    pAddBssParams->llaCoexist = (tANI_U8) pftSessionEntry->beaconParams.llaCoexist;
    pAddBssParams->llbCoexist = (tANI_U8) pftSessionEntry->beaconParams.llbCoexist;
    pAddBssParams->llgCoexist = (tANI_U8) pftSessionEntry->beaconParams.llgCoexist;
    pAddBssParams->ht20Coexist = (tANI_U8) pftSessionEntry->beaconParams.ht20Coexist;

    // Use the advertised capabilities from the received beacon/PR
    if (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode) && ( pBeaconStruct->HTCaps.present ))
    {
        pAddBssParams->htCapable = pBeaconStruct->HTCaps.present;

        if ( pBeaconStruct->HTInfo.present )
        {
            pAddBssParams->htOperMode = (tSirMacHTOperatingMode)pBeaconStruct->HTInfo.opMode;
            pAddBssParams->dualCTSProtection = ( tANI_U8 ) pBeaconStruct->HTInfo.dualCTSProtection;

            chanWidthSupp = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, pftSessionEntry);
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->txChannelWidthSet = ( tANI_U8 ) pBeaconStruct->HTInfo.recommendedTxWidthSet;
                pAddBssParams->currentExtChannel = pBeaconStruct->HTInfo.secondaryChannelOffset;
            }
            else
            {
                pAddBssParams->txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                pAddBssParams->currentExtChannel = PHY_SINGLE_CHANNEL_CENTERED;
            }
            pAddBssParams->llnNonGFCoexist = (tANI_U8)pBeaconStruct->HTInfo.nonGFDevicesPresent;
            pAddBssParams->fLsigTXOPProtectionFullSupport = (tANI_U8)pBeaconStruct->HTInfo.lsigTXOPProtectionFullSupport;
            pAddBssParams->fRIFSMode = pBeaconStruct->HTInfo.rifsMode;
        }
    }

    pAddBssParams->currentOperChannel = bssDescription->channelId;
    pftSessionEntry->htSecondaryChannelOffset = pAddBssParams->currentExtChannel;

#ifdef WLAN_FEATURE_11AC
    if (pftSessionEntry->vhtCapability && pftSessionEntry->vhtCapabilityPresentInBeacon)
    {
        pAddBssParams->vhtCapable = pBeaconStruct->VHTCaps.present;
        pAddBssParams->vhtTxChannelWidthSet = pBeaconStruct->VHTOperation.chanWidth;
        pAddBssParams->currentExtChannel = limGet11ACPhyCBState ( pMac,
                                                                  pAddBssParams->currentOperChannel,
                                                                  pAddBssParams->currentExtChannel,
                                                                  pftSessionEntry->apCenterChan,
                                                                  pftSessionEntry);
    }
    else
    {
        pAddBssParams->vhtCapable = 0;
    }
#endif

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL( "SIR_HAL_ADD_BSS_REQ with channel = %d..." ),
        pAddBssParams->currentOperChannel);
#endif


    // Populate the STA-related parameters here
    // Note that the STA here refers to the AP
    {
        pAddBssParams->staContext.staType = STA_ENTRY_OTHER; // Identifying AP as an STA

        vos_mem_copy(pAddBssParams->staContext.bssId,
                     bssDescription->bssId,
                     sizeof(tSirMacAddr));
        pAddBssParams->staContext.listenInterval = bssDescription->beaconInterval;

        pAddBssParams->staContext.assocId = 0; // Is SMAC OK with this?
        pAddBssParams->staContext.uAPSD = 0;
        pAddBssParams->staContext.maxSPLen = 0;
        pAddBssParams->staContext.shortPreambleSupported = (tANI_U8)pBeaconStruct->capabilityInfo.shortPreamble;
        pAddBssParams->staContext.updateSta = updateEntry;
        pAddBssParams->staContext.encryptType = pftSessionEntry->encryptType;

        if (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode) && ( pBeaconStruct->HTCaps.present ))
        {
            pAddBssParams->staContext.us32MaxAmpduDuration = 0;
            pAddBssParams->staContext.htCapable = 1;
            pAddBssParams->staContext.greenFieldCapable  = ( tANI_U8 ) pBeaconStruct->HTCaps.greenField;
            pAddBssParams->staContext.lsigTxopProtection = ( tANI_U8 ) pBeaconStruct->HTCaps.lsigTXOPProtection;
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->staContext.txChannelWidthSet = ( tANI_U8 )pBeaconStruct->HTInfo.recommendedTxWidthSet;
            }
            else
            {
                pAddBssParams->staContext.txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }                                                           
#ifdef WLAN_FEATURE_11AC
            if (pftSessionEntry->vhtCapability &&
                        IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps))
            {
                pAddBssParams->staContext.vhtCapable = 1;
                if ((pBeaconStruct->VHTCaps.suBeamFormerCap ||
                     pBeaconStruct->VHTCaps.muBeamformerCap) &&
                     pftSessionEntry->txBFIniFeatureEnabled)
                {
                    pAddBssParams->staContext.vhtTxBFCapable = 1;
                }
                if (pBeaconStruct->VHTCaps.muBeamformerCap &&
                                    pftSessionEntry->txMuBformee )
                {
                    pAddBssParams->staContext.vhtTxMUBformeeCapable = 1;
                    limLog(pMac, LOG1, FL("Enabling MUBformee for peer"));
                }
            }
#endif
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->staContext.txChannelWidthSet =
                        ( tANI_U8 )pBeaconStruct->HTInfo.recommendedTxWidthSet;
#ifdef WLAN_FEATURE_11AC
                if (pAddBssParams->staContext.vhtCapable)
                {
                    pAddBssParams->staContext.vhtTxChannelWidthSet =
                            pBeaconStruct->VHTOperation.chanWidth;
                }
#endif
            }
            else
            {
                pAddBssParams->staContext.txChannelWidthSet = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }
            pAddBssParams->staContext.mimoPS             = (tSirMacHTMIMOPowerSaveState)pBeaconStruct->HTCaps.mimoPowerSave;
            pAddBssParams->staContext.delBASupport       = ( tANI_U8 ) pBeaconStruct->HTCaps.delayedBA;
            pAddBssParams->staContext.maxAmsduSize       = ( tANI_U8 ) pBeaconStruct->HTCaps.maximalAMSDUsize;
            pAddBssParams->staContext.maxAmpduDensity    =             pBeaconStruct->HTCaps.mpduDensity;
            pAddBssParams->staContext.fDsssCckMode40Mhz = (tANI_U8)pBeaconStruct->HTCaps.dsssCckMode40MHz;
            pAddBssParams->staContext.fShortGI20Mhz = (tANI_U8)pBeaconStruct->HTCaps.shortGI20MHz;
            pAddBssParams->staContext.fShortGI40Mhz = (tANI_U8)pBeaconStruct->HTCaps.shortGI40MHz;
            pAddBssParams->staContext.maxAmpduSize= pBeaconStruct->HTCaps.maxRxAMPDUFactor;
            
            if( pBeaconStruct->HTInfo.present )
                pAddBssParams->staContext.rifsMode = pBeaconStruct->HTInfo.rifsMode;
        }

        if ((pftSessionEntry->limWmeEnabled && pBeaconStruct->wmeEdcaPresent) ||
                (pftSessionEntry->limQosEnabled && pBeaconStruct->edcaPresent))
            pAddBssParams->staContext.wmmEnabled = 1;
        else 
            pAddBssParams->staContext.wmmEnabled = 0;

        //Update the rates
#ifdef WLAN_FEATURE_11AC
        limPopulatePeerRateSet(pMac, &pAddBssParams->staContext.supportedRates,
                             pBeaconStruct->HTCaps.supportedMCSSet,
                             false,pftSessionEntry,&pBeaconStruct->VHTCaps);
#else
        limPopulatePeerRateSet(pMac, &pAddBssParams->staContext.supportedRates,
                                                    beaconStruct.HTCaps.supportedMCSSet, false,pftSessionEntry);
#endif
        if (pftSessionEntry->htCapability)
        {
           pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11n;
           if (pftSessionEntry->vhtCapability)
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11ac;
        }
        else
        {
           if (pftSessionEntry->limRFBand == SIR_BAND_5_GHZ)
           {
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11a;
           }
           else
           {
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11bg;
           }
        }
    }

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
        pAddBssParams->staContext.staTCParams[i].txUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].rxUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].txBApolicy = eBA_POLICY_IMMEDIATE;
        pAddBssParams->staContext.staTCParams[i].rxBApolicy = eBA_POLICY_IMMEDIATE;
    }

#if defined WLAN_FEATURE_VOWIFI  
    pAddBssParams->maxTxPower = pftSessionEntry->maxTxPower;
#endif

#ifdef WLAN_FEATURE_11W
    if (pftSessionEntry->limRmfEnabled)
    {
        pAddBssParams->rmfEnabled = 1;
        pAddBssParams->staContext.rmfEnabled = 1;
    }
#endif

    pAddBssParams->status = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = true;

    pAddBssParams->staContext.sessionId = pftSessionEntry->peSessionId;
    pAddBssParams->sessionId = pftSessionEntry->peSessionId;
    
    // Set a new state for MLME

    pftSessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, pftSessionEntry->peSessionId, eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE));
    pAddBssParams->halPersona=(tANI_U8)pftSessionEntry->pePersona; //pass on the session persona to hal
    
    pMac->ft.ftPEContext.pAddBssReq = pAddBssParams;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL( "Saving SIR_HAL_ADD_BSS_REQ for pre-auth ap..." ));
#endif

    vos_mem_free(pBeaconStruct);
    return 0;
}

/*------------------------------------------------------------------
 *
 * Setup the new session for the pre-auth AP. 
 * Return the newly created session entry.
 *
 *------------------------------------------------------------------*/
tpPESession limFillFTSession(tpAniSirGlobal pMac,
    tpSirBssDescription  pbssDescription, tpPESession psessionEntry)
{
    tpPESession      pftSessionEntry;
    tANI_U8          currentBssUapsd;
    tPowerdBm        localPowerConstraint;
    tPowerdBm        regMax;
    tSchBeaconStruct *pBeaconStruct;
    uint32           selfDot11Mode;
    ePhyChanBondState cbMode;

    pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
    if (NULL == pBeaconStruct)
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory for creating limFillFTSession") );
        return NULL;
    }


       
    /* Retrieve the session that has already been created and update the entry */
    pftSessionEntry = pMac->ft.ftPEContext.pftSessionEntry;
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    limPrintMacAddr(pMac, pbssDescription->bssId, LOG1);
#endif
    pftSessionEntry->limWmeEnabled = psessionEntry->limWmeEnabled;
    pftSessionEntry->limQosEnabled = psessionEntry->limQosEnabled;
    pftSessionEntry->limWsmEnabled = psessionEntry->limWsmEnabled;
    pftSessionEntry->lim11hEnable = psessionEntry->lim11hEnable;

    // Fields to be filled later
    pftSessionEntry->pLimJoinReq = NULL; 
    pftSessionEntry->smeSessionId = 0; 
    pftSessionEntry->transactionId = 0; 

    limExtractApCapabilities( pMac,
                            (tANI_U8 *) pbssDescription->ieFields,
                            limGetIElenFromBssDescription( pbssDescription ),
                            pBeaconStruct );

    pftSessionEntry->rateSet.numRates = pBeaconStruct->supportedRates.numRates;
    vos_mem_copy(pftSessionEntry->rateSet.rate,
        pBeaconStruct->supportedRates.rate, pBeaconStruct->supportedRates.numRates);

    pftSessionEntry->extRateSet.numRates = pBeaconStruct->extendedRates.numRates;
    vos_mem_copy(pftSessionEntry->extRateSet.rate,
        pBeaconStruct->extendedRates.rate, pftSessionEntry->extRateSet.numRates);


    pftSessionEntry->ssId.length = pBeaconStruct->ssId.length;
    vos_mem_copy(pftSessionEntry->ssId.ssId, pBeaconStruct->ssId.ssId,
        pftSessionEntry->ssId.length);

    wlan_cfgGetInt(pMac, WNI_CFG_DOT11_MODE, &selfDot11Mode);
    limLog(pMac, LOG1, FL("selfDot11Mode %d"),selfDot11Mode );
    pftSessionEntry->dot11mode = selfDot11Mode;
    pftSessionEntry->vhtCapability =
               (IS_DOT11_MODE_VHT(pftSessionEntry->dot11mode)
                && IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps));
    pftSessionEntry->htCapability = (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode)
                                     && pBeaconStruct->HTCaps.present);
#ifdef WLAN_FEATURE_11AC
    if (IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps)
                    && pBeaconStruct->VHTOperation.present)
    {
       pftSessionEntry->vhtCapabilityPresentInBeacon = 1;
       pftSessionEntry->apCenterChan = pBeaconStruct->VHTOperation.chanCenterFreqSeg1;
       pftSessionEntry->apChanWidth = pBeaconStruct->VHTOperation.chanWidth;

       pftSessionEntry->txBFIniFeatureEnabled =
                                      pMac->roam.configParam.txBFEnable;

       limLog(pMac, LOG1, FL("txBFIniFeatureEnabled=%d"),
                pftSessionEntry->txBFIniFeatureEnabled);

       if (pftSessionEntry->txBFIniFeatureEnabled)
       {
           if (cfgSetInt(pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP,
                             pftSessionEntry->txBFIniFeatureEnabled)
                                                          != eSIR_SUCCESS)
           {
               limLog(pMac, LOGE, FL("could not set  "
                              "WNI_CFG_VHT_SU_BEAMFORMEE_CAP at CFG"));
           }
           limLog(pMac, LOG1, FL("txBFCsnValue=%d"),
                    pMac->roam.configParam.txBFCsnValue);

           if (cfgSetInt(pMac, WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED,
                                     pMac->roam.configParam.txBFCsnValue)
                                                             != eSIR_SUCCESS)
           {
               limLog(pMac, LOGE, FL("could not set "
                    "WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED at CFG"));
           }

           if (IS_MUMIMO_BFORMEE_CAPABLE)
               pftSessionEntry->txMuBformee =
                                            pMac->roam.configParam.txMuBformee;
        }

        limLog(pMac, LOG1, FL("txMuBformee = %d"),
                                       pftSessionEntry->txMuBformee);

        if (cfgSetInt(pMac, WNI_CFG_VHT_MU_BEAMFORMEE_CAP,
                                          pftSessionEntry->txMuBformee)
                                                             != eSIR_SUCCESS)
        {
           limLog(pMac, LOGE, FL("could not set "
                                  "WNI_CFG_VHT_MU_BEAMFORMEE_CAP at CFG"));
        }
    }
    else
    {
       pftSessionEntry->vhtCapabilityPresentInBeacon = 0;
    }
#endif
    // Self Mac
    sirCopyMacAddr(pftSessionEntry->selfMacAddr, psessionEntry->selfMacAddr);
    sirCopyMacAddr(pftSessionEntry->limReAssocbssId, pbssDescription->bssId);
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    limPrintMacAddr(pMac, pftSessionEntry->limReAssocbssId, LOG1);
#endif

    /* Store beaconInterval */
    pftSessionEntry->beaconParams.beaconInterval = pbssDescription->beaconInterval;
    pftSessionEntry->bssType = psessionEntry->bssType;

    pftSessionEntry->statypeForBss = STA_ENTRY_PEER;
    pftSessionEntry->nwType = pbssDescription->nwType;

    /* Copy The channel Id to the session Table */
    pftSessionEntry->limReassocChannelId = pbssDescription->channelId;
    pftSessionEntry->currentOperChannel = pbssDescription->channelId;
            
            
    if (pftSessionEntry->bssType == eSIR_INFRASTRUCTURE_MODE)
    {
        pftSessionEntry->limSystemRole = eLIM_STA_ROLE;
    }
    else if(pftSessionEntry->bssType == eSIR_BTAMP_AP_MODE)
    {
        pftSessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
    }
    else
    {   
        /* Throw an error and return and make sure to delete the session.*/
        limLog(pMac, LOGE, FL("Invalid bss type"));
    }    
                       
    pftSessionEntry->limCurrentBssCaps = pbssDescription->capabilityInfo;
    pftSessionEntry->limReassocBssCaps = pbssDescription->capabilityInfo;
    if( pMac->roam.configParam.shortSlotTime &&
        SIR_MAC_GET_SHORT_SLOT_TIME(pftSessionEntry->limReassocBssCaps))
    {
        pftSessionEntry->shortSlotTimeSupported = TRUE;
    }

    regMax = cfgGetRegulatoryMaxTransmitPower( pMac, pftSessionEntry->currentOperChannel ); 
    localPowerConstraint = regMax;
    limExtractApCapability( pMac, (tANI_U8 *) pbssDescription->ieFields, 
        limGetIElenFromBssDescription(pbssDescription),
        &pftSessionEntry->limCurrentBssQosCaps,
        &pftSessionEntry->limCurrentBssPropCap,
        &currentBssUapsd , &localPowerConstraint, psessionEntry);

    pftSessionEntry->limReassocBssQosCaps =
        pftSessionEntry->limCurrentBssQosCaps;
    pftSessionEntry->limReassocBssPropCap =
        pftSessionEntry->limCurrentBssPropCap;


#ifdef FEATURE_WLAN_ESE
    pftSessionEntry->maxTxPower = limGetMaxTxPower(regMax, localPowerConstraint, pMac->roam.configParam.nTxPowerCap);
#else
    pftSessionEntry->maxTxPower = VOS_MIN( regMax , (localPowerConstraint) );
#endif

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, "%s: Regulatory max = %d, local power constraint = %d, ini tx power = %d, max tx = %d",
        __func__, regMax, localPowerConstraint, pMac->roam.configParam.nTxPowerCap, pftSessionEntry->maxTxPower );
#endif

    pftSessionEntry->limRFBand = limGetRFBand(pftSessionEntry->currentOperChannel);

    pftSessionEntry->limPrevSmeState = pftSessionEntry->limSmeState;
    pftSessionEntry->limSmeState = eLIM_SME_WT_REASSOC_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, pftSessionEntry->peSessionId, pftSessionEntry->limSmeState));

    pftSessionEntry->encryptType = psessionEntry->encryptType;

    if (pftSessionEntry->limRFBand == SIR_BAND_2_4_GHZ)
    {
       cbMode = pMac->roam.configParam.channelBondingMode24GHz;
    }
    else
    {
       cbMode = pMac->roam.configParam.channelBondingMode5GHz;
    }
    pftSessionEntry->htSupportedChannelWidthSet =
               cbMode && pBeaconStruct->HTCaps.supportedChannelWidthSet;
    pftSessionEntry->htRecommendedTxWidthSet =
               pftSessionEntry->htSupportedChannelWidthSet;
    if ((pftSessionEntry->limRFBand == SIR_BAND_2_4_GHZ)&&
       (pftSessionEntry->htSupportedChannelWidthSet == 1))
    {
       limInitOBSSScanParams(pMac, pftSessionEntry);
    }
    vos_mem_free(pBeaconStruct);
    return pftSessionEntry;
}

/*------------------------------------------------------------------
 *
 * Setup the session and the add bss req for the pre-auth AP. 
 *
 *------------------------------------------------------------------*/
void limFTSetupAuthSession(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tpPESession pftSessionEntry;

    // Prepare the session right now with as much as possible.
    pftSessionEntry = limFillFTSession(pMac, pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription, psessionEntry);

    if (pftSessionEntry)
    {
        pftSessionEntry->is11Rconnection = psessionEntry->is11Rconnection;
#ifdef FEATURE_WLAN_ESE
        pftSessionEntry->isESEconnection = psessionEntry->isESEconnection;
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
        pftSessionEntry->isFastTransitionEnabled = psessionEntry->isFastTransitionEnabled;
#endif

#ifdef FEATURE_WLAN_LFR
        pftSessionEntry->isFastRoamIniFeatureEnabled = psessionEntry->isFastRoamIniFeatureEnabled; 
#endif
#ifdef WLAN_FEATURE_11W
        pftSessionEntry->limRmfEnabled = psessionEntry->limRmfEnabled;
#endif
        limFTPrepareAddBssReq( pMac, FALSE, pftSessionEntry, 
            pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription );
        pMac->ft.ftPEContext.pftSessionEntry = pftSessionEntry;
    }
}

/*------------------------------------------------------------------
 * Resume Link Call Back 
 *------------------------------------------------------------------*/
void limFTProcessPreAuthResult(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data)
{
    tpPESession psessionEntry;

    if (!pMac->ft.ftPEContext.pFTPreAuthReq)
        return;

    psessionEntry = (tpPESession)data;

    if (pMac->ft.ftPEContext.ftPreAuthStatus == eSIR_SUCCESS)
    {
        limFTSetupAuthSession(pMac, psessionEntry);
    }

    // Post the FT Pre Auth Response to SME
    limPostFTPreAuthRsp(pMac, pMac->ft.ftPEContext.ftPreAuthStatus,
        pMac->ft.ftPEContext.saved_auth_rsp,
        pMac->ft.ftPEContext.saved_auth_rsp_length, psessionEntry);

}

/*------------------------------------------------------------------
 * Resume Link Call Back 
 *------------------------------------------------------------------*/
void limPerformPostFTPreAuthAndChannelChange(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
    tpPESession psessionEntry)
{
    //Set the resume channel to Any valid channel (invalid). 
    //This will instruct HAL to set it to any previous valid channel.
    peSetResumeChannel(pMac, 0, 0);
    limResumeLink(pMac, limFTProcessPreAuthResult, (tANI_U32 *)psessionEntry);
}

tSirRetStatus limCreateRICBlockAckIE(tpAniSirGlobal pMac, tANI_U8 tid, tCfgTrafficClass *pTrafficClass, 
                                                                    tANI_U8 *ric_ies, tANI_U32 *ieLength)
{
    /* BlockACK + RIC is not supported now, TODO later to support this */
#if 0
    tDot11fIERICDataDesc ricIe;
    tDot11fFfBAStartingSequenceControl baSsnControl;
    tDot11fFfAddBAParameterSet baParamSet;
    tDot11fFfBATimeout  baTimeout;

    vos_mem_zero(&ricIe, sizeof(tDot11fIERICDataDesc));
    vos_mem_zero(&baSsnControl, sizeof(tDot11fFfBAStartingSequenceControl));
    vos_mem_zero(&baParamSet, sizeof(tDot11fFfAddBAParameterSet));
    vos_mem_zero(&baTimeout, sizeof(tDot11fFfBATimeout));

    ricIe.present = 1;
    ricIe.RICData.present = 1;
    ricIe.RICData.resourceDescCount = 1;
    ricIe.RICData.Identifier = LIM_FT_RIC_BA_DIALOG_TOKEN_TID_0 + tid;
    ricIe.RICDescriptor.present = 1;
    ricIe.RICDescriptor.resourceType = LIM_FT_RIC_DESCRIPTOR_RESOURCE_TYPE_BA;
    baParamSet.tid = tid;
    baParamSet.policy = pTrafficClass->fTxBApolicy;  // Immediate Block Ack
    baParamSet.bufferSize = pTrafficClass->txBufSize;
    vos_mem_copy((v_VOID_t *)&baTimeout, (v_VOID_t *)&pTrafficClass->tuTxBAWaitTimeout, sizeof(baTimeout));
    baSsnControl.fragNumber = 0;
    baSsnControl.ssn = LIM_FT_RIC_BA_SSN;
    if (ricIe.RICDescriptor.num_variableData < sizeof (ricIe.RICDescriptor.variableData)) {
        dot11fPackFfAddBAParameterSet(pMac, &baParamSet, &ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData]);
        //vos_mem_copy(&ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData], &baParamSet, sizeof(tDot11fFfAddBAParameterSet));
        ricIe.RICDescriptor.num_variableData += sizeof(tDot11fFfAddBAParameterSet);
    }
    if (ricIe.RICDescriptor.num_variableData < sizeof (ricIe.RICDescriptor.variableData)) {
        dot11fPackFfBATimeout(pMac, &baTimeout, &ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData]);
        //vos_mem_copy(&ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData], &baTimeout, sizeof(tDot11fFfBATimeout));
        ricIe.RICDescriptor.num_variableData += sizeof(tDot11fFfBATimeout);
    }
    if (ricIe.RICDescriptor.num_variableData < sizeof (ricIe.RICDescriptor.variableData)) {
        dot11fPackFfBAStartingSequenceControl(pMac, &baSsnControl, &ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData]);
        //vos_mem_copy(&ricIe.RICDescriptor.variableData[ricIe.RICDescriptor.num_variableData], &baSsnControl, sizeof(tDot11fFfBAStartingSequenceControl));
        ricIe.RICDescriptor.num_variableData += sizeof(tDot11fFfBAStartingSequenceControl);
    }
    return (tSirRetStatus) dot11fPackIeRICDataDesc(pMac, &ricIe, ric_ies, sizeof(tDot11fIERICDataDesc), ieLength);
#endif

    return eSIR_FAILURE;
}

tSirRetStatus limFTFillRICBlockAckInfo(tpAniSirGlobal pMac, tANI_U8 *ric_ies, tANI_U32 *ric_ies_length)
{
    tANI_U8 tid = 0;
    tpDphHashNode pSta;
    tANI_U16 numBA = 0, aid = 0;
    tpPESession psessionEntry = pMac->ft.ftPEContext.psavedsessionEntry;
    tANI_U32 offset = 0, ieLength = 0;
    tSirRetStatus status = eSIR_SUCCESS;
    
    // First, extract the DPH entry
    pSta = dphLookupHashEntry( pMac, pMac->ft.ftPEContext.pFTPreAuthReq->currbssId, &aid, &psessionEntry->dph.dphHashTable);
    if( NULL == pSta )
    {
        PELOGE(limLog( pMac, LOGE,
            FL( "STA context not found for saved session's BSSID " MAC_ADDRESS_STR ),
            MAC_ADDR_ARRAY(pMac->ft.ftPEContext.pFTPreAuthReq->currbssId));)
        return eSIR_FAILURE;
    }

    for (tid = 0; tid < STACFG_MAX_TC; tid++)
    {
        if (pSta->tcCfg[tid].fUseBATx)
        {
            status = limCreateRICBlockAckIE(pMac, tid, &pSta->tcCfg[tid], ric_ies + offset, &ieLength);
            if (eSIR_SUCCESS == status)
            {
                // TODO RIC
                if ( ieLength > MAX_FTIE_SIZE )
                {
                    ieLength = 0;
                    return status;
                }
                offset += ieLength;
                *ric_ies_length += ieLength;
                numBA++;
            }
            else
            {
                PELOGE(limLog(pMac, LOGE, FL("BA RIC IE creation for TID %d failed with status %d"), tid, status);)
            }
        }
    }

    PELOGE(limLog(pMac, LOGE, FL("Number of BA RIC IEs created = %d: Total length = %d"), numBA, *ric_ies_length);)
    return status;
}

/*------------------------------------------------------------------
 *
 *  Will post pre auth response to SME.
 *
 *------------------------------------------------------------------*/
void limPostFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
    tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
    tpPESession psessionEntry)
{
    tpSirFTPreAuthRsp pFTPreAuthRsp;
    tSirMsgQ          mmhMsg;
    tANI_U16 rspLen = sizeof(tSirFTPreAuthRsp);   
    // TODO: RIC Support
    //tSirRetStatus   sirStatus = eSIR_SUCCESS;

    pFTPreAuthRsp = (tpSirFTPreAuthRsp)vos_mem_malloc(rspLen);
    if (NULL == pFTPreAuthRsp)
    {
       PELOGE(limLog( pMac, LOGE, "Failed to allocate memory");)
       VOS_ASSERT(pFTPreAuthRsp != NULL);
       return;
    }

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog( pMac, LOG1, FL("Auth Rsp = %p"), pFTPreAuthRsp);)
#endif

    vos_mem_zero(pFTPreAuthRsp, rspLen);
    pFTPreAuthRsp->messageType = eWNI_SME_FT_PRE_AUTH_RSP;
    pFTPreAuthRsp->length = (tANI_U16) rspLen;
    pFTPreAuthRsp->status = status;
    if (psessionEntry)
        pFTPreAuthRsp->smeSessionId = psessionEntry->smeSessionId;

    // The bssid of the AP we are sending Auth1 to.
    if (pMac->ft.ftPEContext.pFTPreAuthReq)
        sirCopyMacAddr(pFTPreAuthRsp->preAuthbssId, 
            pMac->ft.ftPEContext.pFTPreAuthReq->preAuthbssId);
    
    // Attach the auth response now back to SME
    pFTPreAuthRsp->ft_ies_length = 0;
    if ((auth_rsp != NULL) && (auth_rsp_length < MAX_FTIE_SIZE))
    {
        // Only 11r assoc has FT IEs.
        vos_mem_copy(pFTPreAuthRsp->ft_ies, auth_rsp, auth_rsp_length); 
        pFTPreAuthRsp->ft_ies_length = auth_rsp_length;
    }
    
#ifdef WLAN_FEATURE_VOWIFI_11R
    if ((psessionEntry) && (psessionEntry->is11Rconnection))
    {
        /* TODO: RIC SUPPORT Fill in the Block Ack RIC IEs in the preAuthRsp */
        /*
        sirStatus = limFTFillRICBlockAckInfo(pMac, pFTPreAuthRsp->ric_ies, 
                                         (tANI_U32 *)&pFTPreAuthRsp->ric_ies_length);
        if (eSIR_SUCCESS != sirStatus)
        {
            PELOGE(limLog(pMac, LOGE, FL("Fill RIC BA Info failed with status %d"), sirStatus);)
        }
        */
    }
#endif

    if (status != eSIR_SUCCESS)
    {
        /* Ensure that on Pre-Auth failure the cached Pre-Auth Req and
         * other allocated memory is freed up before returning.
         */
        limLog(pMac, LOG1, "Pre-Auth Failed, Cleanup!");
        limFTCleanup(pMac);
    }

    mmhMsg.type = pFTPreAuthRsp->messageType;
    mmhMsg.bodyptr = pFTPreAuthRsp;
    mmhMsg.bodyval = 0;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog( pMac, LOG1, "Posted Auth Rsp to SME with status of 0x%x", status);)
#endif
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    if (status == eSIR_SUCCESS)
        limDiagEventReport(pMac, WLAN_PE_DIAG_PREAUTH_DONE, psessionEntry,
                           status, 0);
#endif
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
}

/*------------------------------------------------------------------
 *
 * Send the FT Pre Auth Response to SME when ever we have a status
 * ready to be sent to SME
 *
 * SME will be the one to send it up to the supplicant to receive 
 * FTIEs which will be required for Reassoc Req.
 *
 *------------------------------------------------------------------*/
void limHandleFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
    tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
    tpPESession psessionEntry)
{

    tpPESession      pftSessionEntry;
    tANI_U8 sessionId;
    tpSirBssDescription  pbssDescription;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT, psessionEntry, (tANI_U16)status, 0);
#endif

    // Save the status of pre-auth
    pMac->ft.ftPEContext.ftPreAuthStatus = status; 

    // Save the auth rsp, so we can send it to 
    // SME once we resume link. 
    pMac->ft.ftPEContext.saved_auth_rsp_length = 0; 
    if ((auth_rsp != NULL) && (auth_rsp_length < MAX_FTIE_SIZE))
    {
        vos_mem_copy(pMac->ft.ftPEContext.saved_auth_rsp,
            auth_rsp, auth_rsp_length); 
        pMac->ft.ftPEContext.saved_auth_rsp_length = auth_rsp_length;
    }

    /* Create FT session for the re-association at this point */
    if (pMac->ft.ftPEContext.ftPreAuthStatus == eSIR_SUCCESS)
    {
        pbssDescription = pMac->ft.ftPEContext.pFTPreAuthReq->pbssDescription;
        if((pftSessionEntry = peCreateSession(pMac, pbssDescription->bssId,
                                              &sessionId, pMac->lim.maxStation)) == NULL)
        {
            limLog(pMac, LOGE, FL("Session Can not be created for pre-auth 11R AP"));
            status = eSIR_FAILURE;
            pMac->ft.ftPEContext.ftPreAuthStatus = status;
            goto out;
        }
        pftSessionEntry->peSessionId = sessionId;
        sirCopyMacAddr(pftSessionEntry->selfMacAddr, psessionEntry->selfMacAddr);
        sirCopyMacAddr(pftSessionEntry->limReAssocbssId, pbssDescription->bssId);
        pftSessionEntry->bssType = psessionEntry->bssType;

        if (pftSessionEntry->bssType == eSIR_INFRASTRUCTURE_MODE)
        {
            pftSessionEntry->limSystemRole = eLIM_STA_ROLE;
        }
        else if(pftSessionEntry->bssType == eSIR_BTAMP_AP_MODE)
        {
            pftSessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
        }
        else
        {
            limLog(pMac, LOGE, FL("Invalid bss type"));
        }
        pftSessionEntry->limPrevSmeState = pftSessionEntry->limSmeState;
        pftSessionEntry->limSmeState = eLIM_SME_WT_REASSOC_STATE;
        pMac->ft.ftPEContext.pftSessionEntry = pftSessionEntry;
        PELOGE(limLog(pMac, LOG1,"%s:created session (%p) with id = %d",
                      __func__, pftSessionEntry, pftSessionEntry->peSessionId);)

        /* Update the ReAssoc BSSID of the current session */
        sirCopyMacAddr(psessionEntry->limReAssocbssId, pbssDescription->bssId);
        limPrintMacAddr(pMac, psessionEntry->limReAssocbssId, LOG1);
    }
out:
    if (psessionEntry->currentOperChannel != 
        pMac->ft.ftPEContext.pFTPreAuthReq->preAuthchannelNum) 
    {
        // Need to move to the original AP channel
        limChangeChannelWithCallback(pMac, psessionEntry->currentOperChannel, 
                limPerformPostFTPreAuthAndChannelChange, NULL, psessionEntry);
    }
    else 
    {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOG1, "Pre auth on same channel as connected AP channel %d",
            pMac->ft.ftPEContext.pFTPreAuthReq->preAuthchannelNum);)
#endif
        limFTProcessPreAuthResult(pMac, status, (tANI_U32 *)psessionEntry);
    }
}

/*------------------------------------------------------------------
 *
 *  This function handles the 11R Reassoc Req from SME
 *
 *------------------------------------------------------------------*/
void limProcessMlmFTReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf,
    tpPESession psessionEntry)
{
    tANI_U8 smeSessionId = 0;
    tANI_U16 transactionId = 0;
    tANI_U8 chanNum = 0;
    tLimMlmReassocReq  *pMlmReassocReq;
    tANI_U16 caps;
    tANI_U32 val;
    tSirMsgQ msgQ;
    tSirRetStatus retCode;
    tANI_U32 teleBcnEn = 0;

    chanNum = psessionEntry->currentOperChannel;
    limGetSessionInfo(pMac,(tANI_U8*)pMsgBuf, &smeSessionId, &transactionId);
    psessionEntry->smeSessionId = smeSessionId;
    psessionEntry->transactionId = transactionId;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_REASSOCIATING, psessionEntry, 0, 0);
#endif

    if (NULL == pMac->ft.ftPEContext.pAddBssReq)
    {
        limLog(pMac, LOGE, FL("pAddBssReq is NULL"));
        return;
    }
    pMlmReassocReq = vos_mem_malloc(sizeof(tLimMlmReassocReq));
    if (NULL == pMlmReassocReq)
    {
        // Log error
        limLog(pMac, LOGE, FL("call to AllocateMemory failed for mlmReassocReq"));
        return;
    }

    vos_mem_copy(pMlmReassocReq->peerMacAddr,
                 psessionEntry->bssId,
                 sizeof(tSirMacAddr));

    if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                  (tANI_U32 *) &pMlmReassocReq->reassocFailureTimeout)
                           != eSIR_SUCCESS)
    {
        /**
         * Could not get ReassocFailureTimeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGE, FL("could not retrieve ReassocFailureTimeout value"));
        vos_mem_free(pMlmReassocReq);
        return;
    }

    if (cfgGetCapabilityInfo(pMac, &caps,psessionEntry) != eSIR_SUCCESS)
    {
        /**
         * Could not get Capabilities value
         * from CFG. Log error.
         */
        limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
        vos_mem_free(pMlmReassocReq);
        return;
    }
    pMlmReassocReq->capabilityInfo = caps;

    /* Update PE sessionId*/
    pMlmReassocReq->sessionId = psessionEntry->peSessionId;

    /* If telescopic beaconing is enabled, set listen interval to WNI_CFG_TELE_BCN_MAX_LI */
    if (wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_WAKEUP_EN, &teleBcnEn) !=
       eSIR_SUCCESS)
    {
       limLog(pMac, LOGP, FL("Couldn't get WNI_CFG_TELE_BCN_WAKEUP_EN"));
       vos_mem_free(pMlmReassocReq);
       return;
    }

    if (teleBcnEn)
    {
       if (wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_MAX_LI, &val) != eSIR_SUCCESS)
       {
          /**
            * Could not get ListenInterval value
            * from CFG. Log error.
          */
          limLog(pMac, LOGE, FL("could not retrieve ListenInterval"));
          vos_mem_free(pMlmReassocReq);
          return;
       }
    }
    else
    {
      if (wlan_cfgGetInt(pMac, WNI_CFG_LISTEN_INTERVAL, &val) != eSIR_SUCCESS)
      {
         /**
            * Could not get ListenInterval value
            * from CFG. Log error.
            */
         limLog(pMac, LOGE, FL("could not retrieve ListenInterval"));
         vos_mem_free(pMlmReassocReq);
         return;
      }
    }
    if (limSetLinkState(pMac, eSIR_LINK_PREASSOC_STATE, psessionEntry->bssId,
                        psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
    {
        vos_mem_free(pMlmReassocReq);
        return;
    }

    pMlmReassocReq->listenInterval = (tANI_U16) val;

    psessionEntry->pLimMlmReassocReq = pMlmReassocReq;


    //we need to defer the message until we get the response back from HAL.
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.type = SIR_HAL_ADD_BSS_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pMac->ft.ftPEContext.pAddBssReq;
    msgQ.bodyval = 0;


#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL( "Sending SIR_HAL_ADD_BSS_REQ..." ));
#endif
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));

    retCode = wdaPostCtrlMsg( pMac, &msgQ );
    if( eSIR_SUCCESS != retCode)
    {
        vos_mem_free(pMac->ft.ftPEContext.pAddBssReq);
        limLog( pMac, LOGE, FL("Posting ADD_BSS_REQ to HAL failed, reason=%X"),
                retCode );
    }
    // Dont need this anymore
    pMac->ft.ftPEContext.pAddBssReq = NULL;
    if (pMac->roam.configParam.roamDelayStatsEnabled)
    {
        vos_record_roam_event(e_LIM_ADD_BS_REQ, NULL, 0);
    }
    return;
}

/*------------------------------------------------------------------
 *
 * This function is called if preauth response is not received from the AP
 * within this timeout while FT in progress
 *
 *------------------------------------------------------------------*/
void limProcessFTPreauthRspTimeout(tpAniSirGlobal pMac)
{
    tpPESession psessionEntry;

    // We have failed pre auth. We need to resume link and get back on
    // home channel.
    limLog(pMac, LOG1, FL("FT Pre-Auth Time Out!!!!"));

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGE, FL("Session Does not exist for given sessionID"));
        return;
    }

    /* To handle the race condition where we recieve preauth rsp after
     * timer has expired.
     */

    if (pMac->ft.ftPEContext.pFTPreAuthReq == NULL)
    {
       limLog(pMac, LOGE, FL("Auth Rsp might already be posted to SME"
              " and ftcleanup done! sessionId:%d"),
              pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId);
       return;
    }

    if (eANI_BOOLEAN_TRUE ==
        pMac->ft.ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed)
    {
        limLog(pMac,LOGE,FL("Auth rsp already posted to SME"
               " (session %p)"), psessionEntry);
        return;
    }
    else
    {
        /* Here we are sending preauth rsp with failure state
         * and which is forwarded to SME. Now, if we receive an preauth
         * resp from AP with success it would create a FT pesession, but
         * will be dropped in SME leaving behind the pesession.
         * Mark Preauth rsp processed so that any rsp from AP is dropped in
         * limProcessAuthFrameNoSession.
         */
        limLog(pMac,LOG1,FL("Auth rsp not yet posted to SME"
               " (session %p)"), psessionEntry);
        pMac->ft.ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed =
            eANI_BOOLEAN_TRUE;
    }
    // Ok, so attempted at Pre-Auth and failed. If we are off channel. We need
    // to get back.
    limHandleFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
}


/*------------------------------------------------------------------
 *
 * This function is called to process the update key request from SME
 *
 *------------------------------------------------------------------*/
tANI_BOOLEAN limProcessFTUpdateKey(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf )
{
    tAddBssParams * pAddBssParams;
    tSirFTUpdateKeyInfo * pKeyInfo;
    tANI_U32 val = 0;

    /* Sanity Check */
    if( pMac == NULL || pMsgBuf == NULL )
    {
        return TRUE;
    }
    if(pMac->ft.ftPEContext.pAddBssReq == NULL)
    {
        limLog( pMac, LOGE,
                FL( "pAddBssReq is NULL" ));
        return TRUE;
    }

    pAddBssParams = pMac->ft.ftPEContext.pAddBssReq;
    pKeyInfo = (tSirFTUpdateKeyInfo *)pMsgBuf;

    /* Store the key information in the ADD BSS parameters */
    pAddBssParams->extSetStaKeyParamValid = 1;
    pAddBssParams->extSetStaKeyParam.encType = pKeyInfo->keyMaterial.edType;
    vos_mem_copy((tANI_U8 *) &pAddBssParams->extSetStaKeyParam.key,
                 (tANI_U8 *) &pKeyInfo->keyMaterial.key, sizeof(tSirKeys));
    if(eSIR_SUCCESS != wlan_cfgGetInt(pMac, WNI_CFG_SINGLE_TID_RC, &val))
    {
        limLog( pMac, LOGP, FL( "Unable to read WNI_CFG_SINGLE_TID_RC" ));
    }

    pAddBssParams->extSetStaKeyParam.singleTidRc = val;
    limLog(pMac, LOG1, FL("Key valid %d key len = %d"),
                pAddBssParams->extSetStaKeyParamValid,
                pAddBssParams->extSetStaKeyParam.key[0].keyLength);

    pAddBssParams->extSetStaKeyParam.staIdx = 0;

    limLog(pMac, LOG1,
           FL("BSSID = "MAC_ADDRESS_STR), MAC_ADDR_ARRAY(pKeyInfo->bssId));

    if(pAddBssParams->extSetStaKeyParam.key[0].keyLength == 16)
    {
        limLog(pMac, LOG1,
        FL("BSS key = %02X-%02X-%02X-%02X-%02X-%02X-%02X- "
        "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X"),
        pAddBssParams->extSetStaKeyParam.key[0].key[0],
        pAddBssParams->extSetStaKeyParam.key[0].key[1],
        pAddBssParams->extSetStaKeyParam.key[0].key[2],
        pAddBssParams->extSetStaKeyParam.key[0].key[3],
        pAddBssParams->extSetStaKeyParam.key[0].key[4],
        pAddBssParams->extSetStaKeyParam.key[0].key[5],
        pAddBssParams->extSetStaKeyParam.key[0].key[6],
        pAddBssParams->extSetStaKeyParam.key[0].key[7],
        pAddBssParams->extSetStaKeyParam.key[0].key[8],
        pAddBssParams->extSetStaKeyParam.key[0].key[9],
        pAddBssParams->extSetStaKeyParam.key[0].key[10],
        pAddBssParams->extSetStaKeyParam.key[0].key[11],
        pAddBssParams->extSetStaKeyParam.key[0].key[12],
        pAddBssParams->extSetStaKeyParam.key[0].key[13],
        pAddBssParams->extSetStaKeyParam.key[0].key[14],
        pAddBssParams->extSetStaKeyParam.key[0].key[15]);
    }

    return TRUE;
}

tSirRetStatus
limProcessFTAggrQosReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf )
{
    tSirMsgQ msg;
    tSirAggrQosReq * aggrQosReq = (tSirAggrQosReq *)pMsgBuf;
    tpAggrAddTsParams pAggrAddTsParam;
    tpPESession  psessionEntry = NULL;
    tpLimTspecInfo   tspecInfo;
    tANI_U8          ac; 
    tpDphHashNode    pSta;
    tANI_U16         aid;
    tANI_U8 sessionId;
    int i;

    pAggrAddTsParam = vos_mem_malloc(sizeof(tAggrAddTsParams));
    if (NULL == pAggrAddTsParam)
    {
        PELOGE(limLog(pMac, LOGE, FL("AllocateMemory() failed"));)
        return eSIR_MEM_ALLOC_FAILED;
    }

    psessionEntry = peFindSessionByBssid(pMac, aggrQosReq->bssId, &sessionId);

    if (psessionEntry == NULL) {
        PELOGE(limLog(pMac, LOGE, FL("psession Entry Null for sessionId = %d"), aggrQosReq->sessionId);)
        vos_mem_free(pAggrAddTsParam);
        return eSIR_FAILURE;
    }

    pSta = dphLookupHashEntry(pMac, aggrQosReq->bssId, &aid, &psessionEntry->dph.dphHashTable);
    if (pSta == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("Station context not found - ignoring AddTsRsp"));)
        vos_mem_free(pAggrAddTsParam);
        return eSIR_FAILURE;
    }

    vos_mem_set((tANI_U8 *)pAggrAddTsParam,
                 sizeof(tAggrAddTsParams), 0);
    pAggrAddTsParam->staIdx = psessionEntry->staId;
    // Fill in the sessionId specific to PE
    pAggrAddTsParam->sessionId = sessionId;
    pAggrAddTsParam->tspecIdx = aggrQosReq->aggrInfo.tspecIdx;

    for( i = 0; i < HAL_QOS_NUM_AC_MAX; i++ )
    {
        if (aggrQosReq->aggrInfo.tspecIdx & (1<<i)) 
        {
            tSirMacTspecIE *pTspec = &aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
            /* Since AddTS response was successful, check for the PSB flag
             * and directional flag inside the TS Info field. 
             * An AC is trigger enabled AC if the PSB subfield is set to 1  
             * in the uplink direction.
             * An AC is delivery enabled AC if the PSB subfield is set to 1 
             * in the downlink direction.
             * An AC is trigger and delivery enabled AC if the PSB subfield  
             * is set to 1 in the bi-direction field.
             */
            if (pTspec->tsinfo.traffic.psb == 1)
            {
                limSetTspecUapsdMask(pMac, &pTspec->tsinfo, SET_UAPSD_MASK);
            }
            else
            { 
                limSetTspecUapsdMask(pMac, &pTspec->tsinfo, CLEAR_UAPSD_MASK);
            }
            /* ADDTS success, so AC is now admitted. We shall now use the default
             * EDCA parameters as advertised by AP and send the updated EDCA params
             * to HAL. 
             */
            ac = upToAc(pTspec->tsinfo.traffic.userPrio);
            if(pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK)
            {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |= (1 << ac);
            }
            else if(pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_DNLINK)
            {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |= (1 << ac);
            }
            else if(pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_BIDIR)
            {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |= (1 << ac);
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |= (1 << ac);
            }

            limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

            if (pSta->aniPeer == eANI_BOOLEAN_TRUE) 
            {
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pSta->bssId, eANI_BOOLEAN_TRUE);
            }
            else 
            {
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pSta->bssId, eANI_BOOLEAN_FALSE);
            }

            if(eSIR_SUCCESS != limTspecAdd(pMac, pSta->staAddr, pSta->assocId, pTspec,  0, &tspecInfo))
            {
                PELOGE(limLog(pMac, LOGE, FL("Adding entry in lim Tspec Table failed "));)
                pMac->lim.gLimAddtsSent = false;
                vos_mem_free(pAggrAddTsParam);
                return eSIR_FAILURE; //Error handling. send the response with error status. need to send DelTS to tear down the TSPEC status.
            }

            // Copy the TSPEC paramters
        pAggrAddTsParam->tspec[i] = aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
    }
    }

    msg.type = WDA_AGGR_QOS_REQ;
    msg.bodyptr = pAggrAddTsParam;
    msg.bodyval = 0;

    /* We need to defer any incoming messages until we get a
     * WDA_AGGR_QOS_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msg.type));

    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
       PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg() failed"));)
       SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
       vos_mem_free(pAggrAddTsParam);
       return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
}

void
limFTSendAggrQosRsp(tpAniSirGlobal pMac, tANI_U8 rspReqd,
                    tpAggrAddTsParams aggrQosRsp, tANI_U8 smesessionId)
{
    tpSirAggrQosRsp  rsp;
    int i = 0;

    if (! rspReqd)
    {
        return;
    }

    rsp = vos_mem_malloc(sizeof(tSirAggrQosRsp));
    if (NULL == rsp)
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for tSirAggrQosRsp"));
        return;
    }

    vos_mem_set((tANI_U8 *) rsp, sizeof(*rsp), 0);
    rsp->messageType = eWNI_SME_FT_AGGR_QOS_RSP;
    rsp->sessionId = smesessionId;
    rsp->length = sizeof(*rsp);
    rsp->aggrInfo.tspecIdx = aggrQosRsp->tspecIdx;

    for( i = 0; i < SIR_QOS_NUM_AC_MAX; i++ )
    {
        if( (1 << i) & aggrQosRsp->tspecIdx )
        {
            rsp->aggrInfo.aggrRsp[i].status = aggrQosRsp->status[i];
            rsp->aggrInfo.aggrRsp[i].tspec = aggrQosRsp->tspec[i];
        }
    }

    limSendSmeAggrQosRsp(pMac, rsp, smesessionId);
    return;
}


void limProcessFTAggrQoSRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpAggrAddTsParams pAggrQosRspMsg = NULL;
    //tpAggrQosParams  pAggrQosRspMsg = NULL;
    tAddTsParams     addTsParam = {0};
    tpDphHashNode  pSta = NULL;
    tANI_U16  assocId =0;
    tSirMacAddr  peerMacAddr;
    tANI_U8   rspReqd = 1;
    tpPESession  psessionEntry = NULL;
    int i = 0;

    limLog(pMac, LOG1, FL(" Received AGGR_QOS_RSP from HAL"));

    /* Need to process all the deferred messages enqueued since sending the
       SIR_HAL_AGGR_ADD_TS_REQ */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pAggrQosRspMsg = (tpAggrAddTsParams) (limMsg->bodyptr);
    if (NULL == pAggrQosRspMsg)
    {
        PELOGE(limLog(pMac, LOGE, FL("NULL pAggrQosRspMsg"));)
        return;
    }

    psessionEntry = peFindSessionBySessionId(pMac, pAggrQosRspMsg->sessionId);
    if (NULL == psessionEntry)
    {
        // Cant find session entry
        PELOGE(limLog(pMac, LOGE, FL("Cant find session entry for %s"), __func__);)
        if( pAggrQosRspMsg != NULL )
        {
            vos_mem_free(pAggrQosRspMsg);
        }
        return;
    }

    for( i = 0; i < HAL_QOS_NUM_AC_MAX; i++ )
    {
        if((((1 << i) & pAggrQosRspMsg->tspecIdx)) &&
                (pAggrQosRspMsg->status[i] != eHAL_STATUS_SUCCESS))
        {
            /* send DELTS to the station */
            sirCopyMacAddr(peerMacAddr,psessionEntry->bssId);

            addTsParam.staIdx = pAggrQosRspMsg->staIdx;
            addTsParam.sessionId = pAggrQosRspMsg->sessionId;
            addTsParam.tspec = pAggrQosRspMsg->tspec[i];
            addTsParam.tspecIdx = pAggrQosRspMsg->tspecIdx;

            limSendDeltsReqActionFrame(pMac, peerMacAddr, rspReqd,
                    &addTsParam.tspec.tsinfo,
                    &addTsParam.tspec, psessionEntry);

            pSta = dphLookupAssocId(pMac, addTsParam.staIdx, &assocId,
                    &psessionEntry->dph.dphHashTable);
            if (pSta != NULL)
            {
                limAdmitControlDeleteTS(pMac, assocId, &addTsParam.tspec.tsinfo,
                        NULL, (tANI_U8 *)&addTsParam.tspecIdx);
            }
        }
    }

    /* Send the Aggr QoS response to SME */

    limFTSendAggrQosRsp(pMac, rspReqd, pAggrQosRspMsg,
            psessionEntry->smeSessionId);
    if( pAggrQosRspMsg != NULL )
    {
        vos_mem_free(pAggrQosRspMsg);
    }
    return;
}

#endif /* WLAN_FEATURE_VOWIFI_11R */
