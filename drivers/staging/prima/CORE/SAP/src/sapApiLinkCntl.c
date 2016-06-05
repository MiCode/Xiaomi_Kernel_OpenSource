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

/*===========================================================================

                      s a p A p i L i n k C n t l . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN SAP modules
  Link Control functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) 

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/c/Dropbox/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_SAP_PAL/CORE/SAP/src/sapApiLinkCntl.c,v 1.7 2008/12/18 19:44:11 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2010-03-15              Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_trace.h"
// Pick up the CSR callback definition
#include "csrApi.h"
#include "sme_Api.h"
// SAP Internal API header file
#include "sapInternal.h"
#ifdef WLAN_FEATURE_AP_HT40_24G
#include "vos_utils.h"
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_CH_AVOID
   extern safeChannelType safeChannels[];
#endif /* FEATURE_WLAN_CH_AVOID */

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*==========================================================================
  FUNCTION    sapSetOperatingChannel()

  DESCRIPTION
    Set SAP Operating Channel

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    *pContext   : The second context pass in for the caller (sapContext)
    operChannel : SAP Operating Channel

  RETURN VALUE

  SIDE EFFECTS
============================================================================*/

void sapSetOperatingChannel(ptSapContext psapContext, v_U8_t operChannel)
{
    v_U8_t i = 0;
    v_U32_t event;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("SAP Channel : %d"), psapContext->channel);

    if (operChannel == SAP_CHANNEL_NOT_SELECTED)
#ifdef SOFTAP_CHANNEL_RANGE
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("No suitable channel selected"));

        if ( eCSR_BAND_ALL ==  psapContext->scanBandPreference ||
                psapContext->allBandScanned == eSAP_TRUE)
        {
            if(psapContext->channelList != NULL)
            {
                 psapContext->channel = SAP_DEFAULT_CHANNEL;
                 for ( i = 0 ; i < psapContext->numofChannel ; i++)
                 {
                    if (NV_CHANNEL_ENABLE ==
                     vos_nv_getChannelEnabledState(psapContext->channelList[i]))
                    {
                        psapContext->channel = psapContext->channelList[i];
                        break;
                    }
                 }
            }
            else
            {
                /* if the channel list is empty then there is no valid channel
                   in the selected sub-band so select default channel in the
                   BAND(2.4GHz) as 2.4 channels are available in all the
                   countries*/
                   psapContext->channel = SAP_DEFAULT_CHANNEL;

#ifdef FEATURE_WLAN_CH_AVOID
                for( i = 0; i < NUM_20MHZ_RF_CHANNELS; i++ )
                {
                    if((NV_CHANNEL_ENABLE ==
                        vos_nv_getChannelEnabledState(safeChannels[i].channelNumber))
                            && (VOS_TRUE == safeChannels[i].isSafe))
                    {
                        psapContext->channel = safeChannels[i].channelNumber;
                        break;
                    }
                }
#endif
            }
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                    FL("Has scan band preference"));
            if (eCSR_BAND_24 == psapContext->currentPreferredBand)
                psapContext->currentPreferredBand = eCSR_BAND_5G;
            else
                psapContext->currentPreferredBand = eCSR_BAND_24;

            psapContext->allBandScanned = eSAP_TRUE;
            //go back to DISCONNECT state, scan next band
            psapContext->sapsMachine = eSAP_DISCONNECTED;
            event = eSAP_CHANNEL_SELECTION_FAILED;
        }
    }
#else
       psapContext->channel = SAP_DEFAULT_CHANNEL;
#endif
    else
    {
      psapContext->channel = operChannel;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("SAP Channel : %d"), psapContext->channel);
}

#ifdef WLAN_FEATURE_AP_HT40_24G
/*==========================================================================
  FUNCTION    sap_ht2040_timer_cb()

  DESCRIPTION
    SAP HT40 timer CallBack, Once this function execute it will move SAP
    from HT20 to HT40

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    usrDataForCallback   : The second context pass in for the caller (sapContext)

  RETURN VALUE
  SIDE EFFECTS
============================================================================*/
void sap_ht2040_timer_cb(v_PVOID_t usrDataForCallback)
{
    v_U8_t cbMode;
    tHalHandle hHal;
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;
    ptSapContext sapContext = (ptSapContext)usrDataForCallback;
    eSapPhyMode sapPhyMode;

    hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME,
                                             sapContext->pvosGCtx);
    if (NULL == hHal)
    {
        /* we have a serious problem */
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   FL("Invalid hHal"));
       return;
    }

    cbMode = sme_GetChannelBondingMode24G(hHal);

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Current Channel bonding : %d"), cbMode);

    if(cbMode)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                  FL("Already in HT40 Channel bonding : %d"), cbMode);
       return;
    }

    sapPhyMode =
        sapConvertSapPhyModeToCsrPhyMode(sapContext->csrRoamProfile.phyMode);

    sme_SelectCBMode(hHal, sapPhyMode, sapContext->channel);

    cbMode = sme_GetChannelBondingMode24G(hHal);

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Selected Channel bonding : %d"), cbMode);

    if(cbMode)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("Move SAP from HT20 to HT40"));

        if (cbMode == eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY)
            cbMode = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
        else if (cbMode == eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY)
            cbMode = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;

        halStatus = sme_SetHT2040Mode(hHal, sapContext->sessionId, cbMode);

        if (halStatus == eHAL_STATUS_FAILURE )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                              FL("Failed to change HT20/40 mode"));
        }
    }
}

/*==========================================================================
  FUNCTION    sapCheckHT2040CoexAction()

  DESCRIPTION
    Check 20/40 Coex Info

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    ptSapContext   : The second context pass in for the caller (sapContext)
    tpSirHT2040CoexInfoInd : 20/40 Coex info

  RETURN VALUE
    The eHalStatus code associated with performing the operation

    eHAL_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
void sapCheckHT2040CoexAction(ptSapContext psapCtx,
                       tpSirHT2040CoexInfoInd pSmeHT2040CoexInfoInd)
{
    v_U8_t i;
    v_U8_t isHT40Allowed = 1;
    tHalHandle hHal;
    v_U8_t cbMode;
    eHalStatus halStatus;
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    unsigned int delay;

    /* tHalHandle */
    hHal = VOS_GET_HAL_CB(psapCtx->pvosGCtx);

    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  FL("In invalid hHal"));
        return;
    }

    // Get Channel Bonding Mode
    cbMode = sme_GetChannelBondingMode24G(hHal);

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Current Channel Bonding Mode: %d"),
               cbMode);

    if (pSmeHT2040CoexInfoInd->HT20MHzBssWidthReq)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("20 MHz BSS width request bit is "
                   "set in 20/40 coexistence info"));
        isHT40Allowed = 0;
    }

    if (pSmeHT2040CoexInfoInd->HT40MHzIntolerant)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("40 MHz intolerant bit is set in "
                    "20/40 coexistence info"));
        isHT40Allowed = 0;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("Total Reported 20/40 BSS Intolerant Channels :%d"),
                pSmeHT2040CoexInfoInd->channel_num);

    if ((0 == psapCtx->affected_start) && (0 == psapCtx->affected_end))
    {
        if (!sapGet24GOBSSAffectedChannel(hHal, psapCtx))
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  FL("Failed to get OBSS Affected Channel "
                     "Range for Channel: %d"), psapCtx->channel);
            return;
        }
        /* Update to Original Channel Bonding for 2.4GHz */
        sme_UpdateChannelBondingMode24G(hHal, cbMode);
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("OBSS Affected Channel Range : [%d %d]"),
               psapCtx->affected_start, psapCtx->affected_end);

    if (pSmeHT2040CoexInfoInd->channel_num)
    {
        for(i = 0; i < pSmeHT2040CoexInfoInd->channel_num; i++)
        {
            if ((pSmeHT2040CoexInfoInd->HT2040BssIntoChanReport[i] >
                                                  psapCtx->affected_start)
              && (pSmeHT2040CoexInfoInd->HT2040BssIntoChanReport[i] <
                                                  psapCtx->affected_end))
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                           FL("BSS Intolerant Channel: %d within OBSS"
                             " Affected Channel Range : [%d %d]"),
                           pSmeHT2040CoexInfoInd->HT2040BssIntoChanReport[i],
                           psapCtx->affected_start, psapCtx->affected_end);
                isHT40Allowed = 0;
                break;
            }

            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       FL("Reported 20/40 BSS Intolerant Channel:%d "
                        " is Out of OBSS Affected Channel Range : [%d %d]"),
                       pSmeHT2040CoexInfoInd->HT2040BssIntoChanReport[i],
                       psapCtx->affected_start, psapCtx->affected_end);
        }
    }

    if (!isHT40Allowed)
    {
        if(cbMode)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("Move SAP from HT40 to HT20"));

            halStatus = sme_SetHT2040Mode(hHal, psapCtx->sessionId,
                                            PHY_SINGLE_CHANNEL_CENTERED);
            if (halStatus == eHAL_STATUS_FAILURE)
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        FL("Failed to change HT20/40 mode"));
                return;
            }

            /* Disable Channel Bonding for 2.4GHz */
            sme_UpdateChannelBondingMode24G(hHal,
                                PHY_SINGLE_CHANNEL_CENTERED);
        }

        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("SAP is Already in HT20"));

        if ((!sapCheckHT40SecondaryIsNotAllowed(psapCtx))
           && (!psapCtx->numHT40IntoSta))
        {
            /* Stop Previous Running HT20/40 Timer & Start timer
               with (OBSS TransitionDelayFactor * obss interval)
               delay after time out move AP from HT20 -> HT40
               mode
             */
            if (VOS_TIMER_STATE_RUNNING == psapCtx->sap_HT2040_timer.state)
            {
                vosStatus = vos_timer_stop(&psapCtx->sap_HT2040_timer);
                if (!VOS_IS_STATUS_SUCCESS(vosStatus))
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                               FL("Failed to Stop HT20/40 timer"));
            }

            delay = psapCtx->ObssScanInterval * psapCtx->ObssTransitionDelayFactor;

            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       FL("Start HT20/40 itransition"
                       " timer (%d sec)"), delay);

            vosStatus = vos_timer_start( &psapCtx->sap_HT2040_timer,
                                                (delay * 1000));

            if (!VOS_IS_STATUS_SUCCESS(vosStatus))
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            FL("Failed to Start HT20/40 timer"));
        }
    }
    return;
}

/*==========================================================================
  FUNCTION    sapCheckFor20MhzObss()

  DESCRIPTION
    Check 20 MHz Overlapping BSS

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    channelNumber : Peer BSS Operating Channel
    tpSirProbeRespBeacon: Pointer to Beacon Struct
    ptSapContext: Pointer to SAP Context

  RETURN VALUE
    v_U8_t          : Success - Found OBSS BSS, Fail - zero

  SIDE EFFECTS
============================================================================*/

eHalStatus sapCheckFor20MhzObss(v_U8_t channelNumber,
                            tpSirProbeRespBeacon  pBeaconStruct,
                            ptSapContext psapCtx)
{

    v_U16_t secondaryChannelOffset;
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             FL("channelNumber: %d BSS: %s"), channelNumber,
             pBeaconStruct->ssId.ssId);

    if (channelNumber < psapCtx->affected_start
      || channelNumber > psapCtx->affected_end)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("channelNumber: %d out of Affetced Channel Range: [%d,%d]"),
                channelNumber, psapCtx->affected_start,
                psapCtx->affected_end);
        return halStatus;
    }

    if (!pBeaconStruct->HTCaps.present)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("Found overlapping legacy BSS: %s on Channel : %d"),
                   pBeaconStruct->ssId.ssId, channelNumber);
        halStatus = eHAL_STATUS_FAILURE;
        return halStatus;
    }

    if (pBeaconStruct->HTInfo.present)
    {
        secondaryChannelOffset = pBeaconStruct->HTInfo.secondaryChannelOffset;
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("BSS: %s secondaryChannelOffset: %d on Channel : %d"),
                   pBeaconStruct->ssId.ssId, secondaryChannelOffset,
                   channelNumber);
        if (PHY_SINGLE_CHANNEL_CENTERED == secondaryChannelOffset)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("Found overlapping 20 MHz HT BSS: %s on Channel : %d"),
                   pBeaconStruct->ssId.ssId, channelNumber);
            halStatus = eHAL_STATUS_FAILURE;
            return halStatus;
        }
     }
     return halStatus;
}

/*==========================================================================
  FUNCTION    sapGetPrimarySecondaryChannelOfBss()

  DESCRIPTION
    Get Primary & Seconary Channel of Overlapping BSS

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    tpSirProbeRespBeacon: Pointer to Beacon Struct
    pri_chan : Primary Operating Channel
    sec_chan : Seconary Operating Channel

  RETURN VALUE

  SIDE EFFECTS
============================================================================*/

void sapGetPrimarySecondaryChannelOfBss(tpSirProbeRespBeacon pBeaconStruct,
                                        v_U32_t *pri_chan, v_U32_t *sec_chan)
{
    v_U16_t secondaryChannelOffset;
    *pri_chan = 0;
    *sec_chan = 0;

    if (pBeaconStruct->HTInfo.present)
    {
        *pri_chan = pBeaconStruct->HTInfo.primaryChannel;
        secondaryChannelOffset = pBeaconStruct->HTInfo.secondaryChannelOffset;
        if (PHY_DOUBLE_CHANNEL_LOW_PRIMARY == secondaryChannelOffset)
            *sec_chan = *pri_chan + 4;
        else if (PHY_DOUBLE_CHANNEL_HIGH_PRIMARY == secondaryChannelOffset)
            *sec_chan = *pri_chan - 4;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("BSS Primary & Secondary Channel : %d %d "),
               *pri_chan, *sec_chan);
}

/*==========================================================================
  FUNCTION    sapCheckHT40SecondaryIsNotAllowed()

  DESCRIPTION
    Check HT40 Channel Secondary is Allowed

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    ptSapContext: Pointer to SAP Context

  RETURN VALUE
    v_U8_t          : Success - HT40 Allowed in Selected Channale Pair
                      Fail - HT40 Not Allowed

  SIDE EFFECTS
============================================================================*/

eHalStatus sapCheckHT40SecondaryIsNotAllowed(ptSapContext psapCtx)
{
    v_U8_t count;
    v_U8_t fValidChannel = 0;
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;
#ifdef FEATURE_WLAN_CH_AVOID
    int i;
    v_U16_t unsafeChannelList[NUM_20MHZ_RF_CHANNELS];
    v_U16_t unsafeChannelCount;
#endif /* FEATURE_WLAN_CH_AVOID */

    /* Verify that HT40 secondary channel is an allowed 20 MHz
     * channel */
    for (count = RF_CHAN_1; count <= RF_CHAN_14; count++)
    {
        if ((regChannels[count].enabled)
         && (rfChannels[count].channelNum == psapCtx->sap_sec_chan))
        {
            fValidChannel = TRUE;
            break;
        }
    }

#ifdef FEATURE_WLAN_CH_AVOID
    /* Get unsafe cahnnel list from cached location */
    vos_get_wlan_unsafe_channel(unsafeChannelList,
                                  sizeof(unsafeChannelList),
                                  &unsafeChannelCount);

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
        FL("Unsafe Channel count %d"
           " SAP Secondary Channel: %d"),
           unsafeChannelCount, psapCtx->sap_sec_chan);

    for (i = 0; i < unsafeChannelCount; i++)
    {
        if ((psapCtx->sap_sec_chan == unsafeChannelList[i]))
        {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("Unsafe Channel %d SAP Secondary Channel: %d"),
                    unsafeChannelList[i], psapCtx->sap_sec_chan);
            fValidChannel = FALSE;
            break;
        }
    }
#endif /* FEATURE_WLAN_CH_AVOID */

    if (!fValidChannel)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("HT40 In Secondary Channel : %d not allowed"),
                   psapCtx->sap_sec_chan);

        halStatus = eHAL_STATUS_FAILURE;
        return halStatus;
    }

    return halStatus;
}

/*==========================================================================
  FUNCTION    sapGet24GOBSSAffectedChannel()

  DESCRIPTION
    Get OBSS Affected Channel Range

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    tHalHandle  : tHalHandle passed in with Affected Channel
    ptSapContext: Pointer to SAP Context

  RETURN VALUE
    v_U8_t          : Success - Able to get AffectedChannel
                      Fail - Fail to get AffectedChannel

  SIDE EFFECTS
============================================================================*/

eHalStatus sapGet24GOBSSAffectedChannel(tHalHandle halHandle,
                                          ptSapContext psapCtx)
{

    v_U8_t cbMode;
    v_U32_t pri_freq, sec_freq;
    v_U32_t affected_start_freq, affected_end_freq;
    eSapPhyMode sapPhyMode;
    eHalStatus halStatus;

    pri_freq = vos_chan_to_freq(psapCtx->channel);

    sapPhyMode =
     sapConvertSapPhyModeToCsrPhyMode(psapCtx->csrRoamProfile.phyMode);

    sme_SelectCBMode(halHandle, sapPhyMode, psapCtx->channel);

    cbMode = sme_GetChannelBondingMode24G(halHandle);

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Selected Channel bonding : %d"), cbMode);

    if (cbMode == eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY)
        sec_freq = pri_freq - 20;
    else if (cbMode == eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY)
        sec_freq = pri_freq + 20;
    else
        sec_freq = eCSR_INI_SINGLE_CHANNEL_CENTERED;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Primary Freq : %d MHz Secondary Freq : %d MHz"),
               pri_freq, sec_freq);

    if (sec_freq)
    {
        /* As per 802.11 Std, Section 10.15.3.2 */
        affected_start_freq = (pri_freq + sec_freq) / 2 - 25;
        affected_end_freq = (pri_freq + sec_freq) / 2 + 25;

        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                FL("Affected Start Freq: %d MHz Affected End Freq : %d MHz"),
                affected_start_freq, affected_end_freq);

        psapCtx->affected_start = vos_freq_to_chan(affected_start_freq);

        /* As there is no channel availabe for 2397 & 2402 Frequency
         * Hence taking valid channel 1 (Freq 2412) here
         */
        if (affected_start_freq < 2412)
            psapCtx->affected_start = 1;

        psapCtx->affected_end = vos_freq_to_chan(affected_end_freq);

        /* As there is no channel availabe for 2477 & 2482 Frequency
         * Hence taking lower channel 13 (Freq 2472) here.
         */
        if ((2477 == (affected_end_freq)) || (2482 == affected_end_freq))
        {
            psapCtx->affected_end = 13;
        }
        else if (2487 == affected_end_freq)
        {
           /* As there is no channel availabe for 2487 Frequency
            * Hence taking lower channel 14 (Freq 2484) here.
            */
            psapCtx->affected_end = 14;
        }

        psapCtx->sap_sec_chan = vos_freq_to_chan(sec_freq);

        halStatus = eHAL_STATUS_SUCCESS;
        return halStatus;
    }
    else
    {
        psapCtx->affected_start = 0;
        psapCtx->affected_end = 0;
        psapCtx->sap_sec_chan = 0;
        halStatus = eHAL_STATUS_FAILURE;
        return halStatus;
    }
}

/*==========================================================================
  FUNCTION    sapCheck40Mhz24G

  DESCRIPTION
  Check HT40 is possible in 2.4GHz mode

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    halHandle       : Pointer to HAL handle
    ptSapContext    : Pointer to SAP Context
    pResult         : Pointer to tScanResultHandle

  RETURN VALUE
    v_U8_t          : Success - HT40 Possible, Fail - zero

  SIDE EFFECTS
============================================================================*/

eHalStatus sapCheck40Mhz24G(tHalHandle halHandle, ptSapContext psapCtx,
                                         tScanResultHandle pResult)
{
    v_U32_t pri_chan, sec_chan;
    v_U32_t ieLen = 0;
    v_U8_t channelNumber = 0;
    tSirProbeRespBeacon *pBeaconStruct;
    tCsrScanResultInfo *pScanResult;
    tpAniSirGlobal  pMac = (tpAniSirGlobal) halHandle;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if ( (0 == psapCtx->affected_start) && (0 == psapCtx->affected_end)
       && (0 == psapCtx->sap_sec_chan))
    {
        if (eHAL_STATUS_SUCCESS !=
                 sapGet24GOBSSAffectedChannel(halHandle, psapCtx))
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                FL("Failed to get OBSS Affected Channel Range for Channel: %d"),
                                psapCtx->channel);
            return halStatus;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("40 MHz affected channel range: [%d,%d] MHz"),
               psapCtx->affected_start, psapCtx->affected_end);

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("SAP Primary & Secondary Channel : [%d,%d] MHz"),
               psapCtx->channel, psapCtx->sap_sec_chan);

    pBeaconStruct = vos_mem_malloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pBeaconStruct )
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   FL("Unable to allocate memory "));
        return halStatus;
    }

    /* Check neighboring BSSes from scan result to see whether 40 MHz is
     * allowed per IEEE Std 802.11-2012, 10.15.3.2 */
    pScanResult = sme_ScanResultGetFirst(halHandle, pResult);

    while (pScanResult)
    {

        /* if the Beacon has channel ID, use it other wise we will
         * rely on the channelIdSelf
         */
        if(pScanResult->BssDescriptor.channelId == 0)
            channelNumber = pScanResult->BssDescriptor.channelIdSelf;
        else
            channelNumber = pScanResult->BssDescriptor.channelId;

        if (channelNumber > SIR_11B_CHANNEL_END)
        {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                   FL("channelNumber: %d BSS: %s"),
                   channelNumber,  pBeaconStruct->ssId.ssId);
            goto NextResult;
        }

        if ((pScanResult->BssDescriptor.ieFields != NULL))
        {
            ieLen = (pScanResult->BssDescriptor.length + sizeof(tANI_U16));
            ieLen += (sizeof(tANI_U32) - sizeof(tSirBssDescription));
            vos_mem_set((tANI_U8 *) pBeaconStruct,
                               sizeof(tSirProbeRespBeacon), 0);

            if ((eSIR_SUCCESS == sirParseBeaconIE(pMac, pBeaconStruct,
                     (tANI_U8 *)( pScanResult->BssDescriptor.ieFields), ieLen)))
            {
                /* Check Peer BSS is HT20 or Legacy AP */
                if (eHAL_STATUS_SUCCESS !=
                          sapCheckFor20MhzObss(channelNumber, pBeaconStruct,
                                                                    psapCtx))
                {
                    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                              FL("Overlapping 20 MHz BSS is found"));
                    vos_mem_free(pBeaconStruct);
                    return halStatus;
                }

                sapGetPrimarySecondaryChannelOfBss(pBeaconStruct,
                                                &pri_chan, &sec_chan);

                /* Check peer BSS Operating channel is not within OBSS affected
                 * channel range
                 */
                if ((pri_chan < psapCtx->affected_start
                    || pri_chan > psapCtx->affected_end)
                   && (sec_chan < psapCtx->affected_start
                    || sec_chan > psapCtx->affected_end))
                {
                    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                     FL("Peer BSS: %s Primary & Secondary Channel [%d %d]"
                        "is out of affected Range: [%d %d]"),
                     pBeaconStruct->ssId.ssId, pri_chan, sec_chan,
                     psapCtx->affected_start, psapCtx->affected_end);
                    goto NextResult; /* not within affected channel range */
                }

                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                 FL("Neighboring BSS: %s Primary & Secondary Channel [%d %d]"),
                 pBeaconStruct->ssId.ssId, pri_chan, sec_chan);

                if (sec_chan)
                {
                    /* Peer BSS is HT40 capable then check peer BSS
                     * primary & secondary channel with SAP
                     * Primary & Secondary channel.
                     */
                    if ((psapCtx->channel !=  pri_chan)
                       || (psapCtx->sap_sec_chan != sec_chan))
                    {
                        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                                  FL("40 MHz Pri/Sec channel : [%d %d]"
                                  " missmatch with  BSS: %s"
                                  " Pri/Sec channel : [%d %d]"),
                                  psapCtx->channel, psapCtx->sap_sec_chan,
                                  pBeaconStruct->ssId.ssId, pri_chan, sec_chan);
                         vos_mem_free(pBeaconStruct);
                         return halStatus;
                    }
                }

                if (pBeaconStruct->HTCaps.present)
                {
                    /* Check Peer BSS HT capablity has 40MHz Intolerant bit */
                    if (pBeaconStruct->HTCaps.stbcControlFrame)
                    {
                        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                                      FL("Found BSS: %s with 40 MHz"
                                      "Intolerant is set on Channel : %d"),
                                      pBeaconStruct->ssId.ssId,
                                      channelNumber);
                        vos_mem_free(pBeaconStruct);
                        return halStatus;
                    }
                }
            }
            else
            {
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                          FL("Failed to Parse the Beacon IEs"));
            }
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       FL("BSS IEs Failed is NULL in Scan"));
        }

NextResult:
        pScanResult = sme_ScanResultGetNext(halHandle, pResult);
    }
    vos_mem_free(pBeaconStruct);

    if (psapCtx->sap_sec_chan)
    {
        if (eHAL_STATUS_SUCCESS == sapCheckHT40SecondaryIsNotAllowed(psapCtx))
        {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       FL("Start SAP/P2P GO in HT 40MHz "
                       "Primary & Secondary Channel: [%d %d]"),
                       psapCtx->channel, psapCtx->sap_sec_chan);
            halStatus = eHAL_STATUS_SUCCESS;
            return halStatus;
        }
    }

    return halStatus;
}
#endif

/*==========================================================================
  FUNCTION    WLANSAP_ScanCallback()

  DESCRIPTION
    Callback for Scan (scan results) Events

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    tHalHandle  : tHalHandle passed in with the scan request
    *pContext   : The second context pass in for the caller (sapContext)
    scanID      : scanID got after the scan
    status      : Status of scan -success, failure or abort

  RETURN VALUE
    The eHalStatus code associated with performing the operation

    eHAL_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
eHalStatus
WLANSAP_ScanCallback
(
  tHalHandle halHandle,
  void *pContext,           /* Opaque SAP handle */
  v_U32_t scanID,
  eCsrScanStatus scanStatus
)
{
    tScanResultHandle pResult = NULL;
    eHalStatus scanGetResultStatus = eHAL_STATUS_FAILURE;
    ptSapContext psapContext = (ptSapContext)pContext;
    void *pTempHddCtx;
    tWLAN_SAPEvent sapEvent; /* State machine event */
    v_U8_t operChannel = 0;
    VOS_STATUS sapstatus;
    v_U32_t event;
    eSapPhyMode sapPhyMode;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    pTempHddCtx = vos_get_context( VOS_MODULE_ID_HDD,
                                     psapContext->pvosGCtx);
    if (NULL == pTempHddCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "HDD context is NULL");
        return eHAL_STATUS_FAILURE;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
         "In %s, before switch on scanStatus = %d", __func__, scanStatus);

    switch (scanStatus)
    {
        case eCSR_SCAN_SUCCESS:
            // sapScanCompleteCallback with eCSR_SCAN_SUCCESS
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "In %s, CSR scanStatus = %s (%d)", __func__,
               "eCSR_SCAN_SUCCESS", scanStatus);

            /* Get scan results, Run channel selection algorithm,
             * select channel and keep in pSapContext->Channel
             */
            scanGetResultStatus = sme_ScanGetResult(halHandle, 0, NULL,
                                                               &pResult);

            event = eSAP_MAC_SCAN_COMPLETE;

            if ((scanGetResultStatus != eHAL_STATUS_SUCCESS)
               && (scanGetResultStatus != eHAL_STATUS_E_NULL_VALUE))
            {
                // No scan results
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                     "In %s, Get scan result failed! ret = %d",
                                __func__, scanGetResultStatus);
                sapSetOperatingChannel(psapContext, operChannel);
                break;
            }

#ifdef WLAN_FEATURE_AP_HT40_24G
            if (psapContext->channel == AUTO_CHANNEL_SELECT)
#endif
            {
                operChannel = sapSelectChannel(halHandle, psapContext, pResult);
                sapSetOperatingChannel(psapContext, operChannel);
            }

#ifdef WLAN_FEATURE_AP_HT40_24G
            if ((psapContext->channel <= SIR_11B_CHANNEL_END)
               && (psapContext->channel > 0))
            {
                if (eHAL_STATUS_SUCCESS !=
                         sapCheck40Mhz24G(halHandle, psapContext, pResult))
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                               FL("Starting SAP into HT20"));
                    /* Disable Channel Bonding for 2.4GHz */
                    sme_UpdateChannelBondingMode24G(halHandle,
                                          PHY_SINGLE_CHANNEL_CENTERED);
                }
             }
#endif
            sme_ScanResultPurge(halHandle, pResult);
            break;

        default:
            event = eSAP_CHANNEL_SELECTION_FAILED;
            if (psapContext->channel == AUTO_CHANNEL_SELECT)
                sapSetOperatingChannel(psapContext, operChannel);
#ifdef WLAN_FEATURE_AP_HT40_24G
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            FL("Starting SAP into HT20"));
            /* Disable Channel Bonding for 2.4GHz */
            sme_UpdateChannelBondingMode24G(halHandle,
                                   PHY_SINGLE_CHANNEL_CENTERED);
#endif
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                 FL("CSR scanStatus = %s (%d)"),
                 "eCSR_SCAN_ABORT/FAILURE", scanStatus);
    }


    sapPhyMode =
     sapConvertSapPhyModeToCsrPhyMode(psapContext->csrRoamProfile.phyMode);

#ifdef WLAN_FEATURE_AP_HT40_24G
    if (psapContext->channel > SIR_11B_CHANNEL_END)
#endif
        sme_SelectCBMode(halHandle, sapPhyMode, psapContext->channel);

#ifdef SOFTAP_CHANNEL_RANGE
    if(psapContext->channelList != NULL)
    {
        /* Always free up the memory for channel selection whatever
         * the result */
        vos_mem_free(psapContext->channelList);
        psapContext->channelList = NULL;
    }
#endif    

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
           "In %s, Channel selected = %d", __func__, psapContext->channel);

    /* Fill in the event structure */
    sapEvent.event = event;
    sapEvent.params = 0;        // pCsrRoamInfo;
    sapEvent.u1 = scanStatus;   // roamstatus
    sapEvent.u2 = 0;            // roamResult

    /* Handle event */ 
    sapstatus = sapFsm(psapContext, &sapEvent);

    return sapstatus;
}// WLANSAP_ScanCallback

/*==========================================================================
  FUNCTION    WLANSAP_RoamCallback()

  DESCRIPTION 
    Callback for Roam (connection status) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
      pContext      : pContext passed in with the roam request
      pCsrRoamInfo  : Pointer to a tCsrRoamInfo, see definition of eRoamCmdStatus and
      eRoamCmdResult: For detail valid members. It may be NULL
      roamId        : To identify the callback related roam request. 0 means unsolicited
      roamStatus    : Flag indicating the status of the callback
      roamResult    : Result
   
  RETURN VALUE
    The eHalStatus code associated with performing the operation  

    eHAL_STATUS_SUCCESS: Success
  
  SIDE EFFECTS 
============================================================================*/
eHalStatus
WLANSAP_RoamCallback
(
    void *pContext,           /* Opaque SAP handle */ 
    tCsrRoamInfo *pCsrRoamInfo,
    v_U32_t roamId, 
    eRoamCmdStatus roamStatus, 
    eCsrRoamResult roamResult
)
{
    /* sapContext value */    
    ptSapContext sapContext = (ptSapContext) pContext; 
    tWLAN_SAPEvent sapEvent; /* State machine event */
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                      FL("Before switch on roamStatus = %d"),
                                 roamStatus);
    switch(roamStatus)
    {
        case eCSR_ROAM_SESSION_OPENED:
        {
            /* tHalHandle */
            tHalHandle hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                           "eCSR_ROAM_SESSION_OPENED", roamStatus);

            if (NULL == hHal)
            {
               VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                          "In %s invalid hHal", __func__);
               halStatus = eHAL_STATUS_FAILED_ALLOC;
            }
            else
            {
               VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          "In %s calling sme_RoamConnect with eCSR_BSS_TYPE_INFRA_AP", __func__);
               sapContext->isSapSessionOpen = eSAP_TRUE;
               halStatus = sme_RoamConnect(hHal, sapContext->sessionId,
                                           &sapContext->csrRoamProfile,
                                           &sapContext->csrRoamId);
            }
            break;
        }

        case eCSR_ROAM_INFRA_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                           "eCSR_ROAM_INFRA_IND", roamStatus);
            if(roamResult == eCSR_ROAM_RESULT_INFRA_START_FAILED)
            {
                /* Fill in the event structure */ 
                sapEvent.event = eSAP_MAC_START_FAILS; 
                sapEvent.params = pCsrRoamInfo;
                sapEvent.u1 = roamStatus;
                sapEvent.u2 = roamResult; 
                
                /* Handle event */ 
                vosStatus = sapFsm(sapContext, &sapEvent);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                    halStatus = eHAL_STATUS_FAILURE;
                }
            }
            break;

        case eCSR_ROAM_LOSTLINK:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_LOSTLINK", roamStatus);
            break;

        case eCSR_ROAM_MIC_ERROR_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_MIC_ERROR_IND", roamStatus);
            break;

        case eCSR_ROAM_SET_KEY_COMPLETE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_SET_KEY_COMPLETE", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_SET_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_REMOVE_KEY_COMPLETE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_REMOVE_KEY_COMPLETE", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_DEL_KEY_EVENT, (v_PVOID_t)eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_ASSOCIATION_COMPLETION:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_ASSOCIATION_COMPLETION", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_REASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_DISASSOCIATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_DISASSOCIATED", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_MIC_FAILURE)
            {
                /* Format the MIC failure event to return... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) eSAP_STATUS_FAILURE);
            }
            break;
                        
        case eCSR_ROAM_WPS_PBC_PROBE_REQ_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_WPS_PBC_PROBE_REQ_IND", roamStatus);
            break;        
        case eCSR_ROAM_REMAIN_CHAN_READY:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_REMAIN_CHAN_READY", roamStatus);
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                              eSAP_REMAIN_CHAN_READY, 
                              (v_PVOID_t) eSAP_STATUS_SUCCESS);
            break;
        case eCSR_ROAM_SEND_ACTION_CNF:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_SEND_ACTION_CNF", roamStatus);
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_SEND_ACTION_CNF, 
                            (v_PVOID_t)((eSapStatus)((roamResult == eCSR_ROAM_RESULT_NONE)
                            ? eSAP_STATUS_SUCCESS : eSAP_STATUS_FAILURE)));
            break;

       case eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                        "eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS", roamStatus);
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_DISCONNECT_ALL_P2P_CLIENT, 
                            (v_PVOID_t) eSAP_STATUS_SUCCESS );
            break;
            
       case eCSR_ROAM_SEND_P2P_STOP_BSS:
           VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                             FL("Received stopbss"));
           sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_MAC_TRIG_STOP_BSS_EVENT, 
                            (v_PVOID_t) eSAP_STATUS_SUCCESS );
        break;

#ifdef WLAN_FEATURE_AP_HT40_24G
        case eCSR_ROAM_2040_COEX_INFO_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("CSR roamStatus = %s (%d)"),
                           "eCSR_ROAM_2040_COEX_INFO_IND",
                           roamStatus);

           sapCheckHT2040CoexAction(sapContext, pCsrRoamInfo->pSmeHT2040CoexInfoInd);
           break;
#endif

        default:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                         FL("CSR roamStatus not handled roamStatus = %s (%d)"),
                         get_eRoamCmdStatus_str(roamStatus), roamStatus);
            break;
    }


    switch (roamResult)
    {
        case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                         FL( "CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND",
                              roamResult);
            sapContext->nStaWPARSnReqIeLength = pCsrRoamInfo->rsnIELen;
             
            if(sapContext->nStaWPARSnReqIeLength)
                vos_mem_copy( sapContext->pStaWpaRsnReqIE,
                              pCsrRoamInfo->prsnIE, sapContext->nStaWPARSnReqIeLength);

            sapContext->nStaAddIeLength = pCsrRoamInfo->addIELen;
             
            if(sapContext->nStaAddIeLength)
                vos_mem_copy( sapContext->pStaAddIE,
                        pCsrRoamInfo->paddIE, sapContext->nStaAddIeLength);

            sapContext->SapQosCfg.WmmIsEnabled = pCsrRoamInfo->wmmEnabledSta;
            // MAC filtering
            vosStatus = sapIsPeerMacAllowed(sapContext, (v_U8_t *)pCsrRoamInfo->peerMac);
            
            if ( VOS_STATUS_SUCCESS == vosStatus )
            {
                vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_ASSOC_IND, (v_PVOID_t)eSAP_STATUS_SUCCESS);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                             FL("CSR roamResult = (%d) MAC ("
                             MAC_ADDRESS_STR") fail"),
                             roamResult,
                             MAC_ADDR_ARRAY(pCsrRoamInfo->peerMac));
                    halStatus = eHAL_STATUS_FAILURE;
                }
            }
            else
            {
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                          FL("CSR roamResult = (%d) MAC ("
                          MAC_ADDRESS_STR") not allowed"),
                          roamResult,
                          MAC_ADDR_ARRAY(pCsrRoamInfo->peerMac));
                halStatus = eHAL_STATUS_FAILURE;
            } 

            break;

        case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF",
                              roamResult);

            sapContext->nStaWPARSnReqIeLength = pCsrRoamInfo->rsnIELen;
            if (sapContext->nStaWPARSnReqIeLength)
                vos_mem_copy( sapContext->pStaWpaRsnReqIE,
                              pCsrRoamInfo->prsnIE, sapContext->nStaWPARSnReqIeLength);

            sapContext->nStaAddIeLength = pCsrRoamInfo->addIELen;
            if(sapContext->nStaAddIeLength)
                vos_mem_copy( sapContext->pStaAddIE,
                    pCsrRoamInfo->paddIE, sapContext->nStaAddIeLength);

            sapContext->SapQosCfg.WmmIsEnabled = pCsrRoamInfo->wmmEnabledSta;
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_ASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
#ifdef WLAN_FEATURE_AP_HT40_24G
            else
            {
                if (pCsrRoamInfo->HT40MHzIntoEnabledSta)
                {
                    sapAddHT40IntolerantSta(sapContext, pCsrRoamInfo);
                }
            }
#endif
            break;

        case eCSR_ROAM_RESULT_DISASSOC_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_DISASSOC_IND",
                              roamResult);
#ifdef WLAN_FEATURE_AP_HT40_24G
            sapRemoveHT40IntolerantSta(sapContext, pCsrRoamInfo);
#endif
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_DEAUTH_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_DEAUTH_IND",
                              roamResult);
#ifdef WLAN_FEATURE_AP_HT40_24G
            sapRemoveHT40IntolerantSta(sapContext, pCsrRoamInfo);
#endif
            /* Fill in the event structure */
            //TODO: we will use the same event inorder to inform HDD to disassociate the station
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_GROUP:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_MIC_ERROR_GROUP",
                              roamResult);
            /* Fill in the event structure */
            //TODO: support for group key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_UNICAST: 
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_MIC_ERROR_UNICAST",
                              roamResult);
            /* Fill in the event structure */
            //TODO: support for unicast key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_AUTHENTICATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_AUTHENTICATED",
                              roamResult);
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_SET_KEY_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_ASSOCIATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_ASSOCIATED",
                              roamResult);
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_REASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            break;

        case eCSR_ROAM_RESULT_INFRA_STARTED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_INFRA_STARTED",
                              roamResult);
            /* Fill in the event structure */ 
            sapEvent.event = eSAP_MAC_START_BSS_SUCCESS;
            sapEvent.params = pCsrRoamInfo;
            sapEvent.u1 = roamStatus;
            sapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = sapFsm(sapContext, &sapEvent);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_INFRA_STOPPED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_INFRA_STOPPED",
                              roamResult);
            /* Fill in the event structure */ 
            sapEvent.event = eSAP_MAC_READY_FOR_CONNECTIONS;
            sapEvent.params = pCsrRoamInfo;
            sapEvent.u1 = roamStatus;
            sapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = sapFsm(sapContext, &sapEvent);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND",
                              roamResult);
            /* Fill in the event structure */
            //TODO: support for group key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_WPS_PBC_PROBE_REQ_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_FORCED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_FORCED",
                              roamResult);
#ifdef WLAN_FEATURE_AP_HT40_24G
            sapRemoveHT40IntolerantSta(sapContext, pCsrRoamInfo);
#endif
            //This event can be used to inform hdd about user triggered disassoc event
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            break;

        case eCSR_ROAM_RESULT_NONE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_NONE",
                              roamResult);
            //This event can be used to inform hdd about user triggered disassoc event
            /* Fill in the event structure */
            if ( roamStatus == eCSR_ROAM_SET_KEY_COMPLETE)
            {
                sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_SET_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_SUCCESS);
            }
            else if (roamStatus == eCSR_ROAM_REMOVE_KEY_COMPLETE )
            {
                sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_DEL_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_SUCCESS);
            }
            break;

        case eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          FL("CSR roamResult = %s (%d)"),
                             "eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED",
                              roamResult);
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_MAX_ASSOC_EXCEEDED, (v_PVOID_t)NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }

            break;
        default:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                          FL("CSR roamResult = %s (%d) not handled"),
                             get_eCsrRoamResult_str(roamResult),
                             roamResult);
            break;
    }

    return halStatus;
}
