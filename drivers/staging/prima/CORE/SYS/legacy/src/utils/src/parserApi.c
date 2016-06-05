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
 * This file parserApi.cc contains the code for parsing
 * 802.11 messages.
 * Author:        Pierre Vandwalle
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "sirApi.h"
#include "aniGlobal.h"
#include "parserApi.h"
#include "cfgApi.h"
#include "limUtils.h"
#include "utilsParser.h"
#include "limSerDesUtils.h"
#include "schApi.h"
#include "palApi.h"
#include "wmmApsd.h"
#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif



////////////////////////////////////////////////////////////////////////
void dot11fLog(tpAniSirGlobal pMac, int loglevel, const char *pString,...) 
{
#ifdef WLAN_DEBUG
    if( (tANI_U32)loglevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( SIR_DBG_MODULE_ID )] )
    {
        return;
    }
    else
    {
        va_list         marker;

        va_start( marker, pString );     /* Initialize variable arguments. */

        logDebug(pMac, SIR_DBG_MODULE_ID, loglevel, pString, marker);

        va_end( marker );              /* Reset variable arguments.      */
    }
#endif
}

void
swapBitField16(tANI_U16 in, tANI_U16 *out)
{
#   ifdef ANI_LITTLE_BIT_ENDIAN
    *out = in;
#   else // Big-Endian...
    *out = ( ( in & 0x8000 ) >> 15 ) |
           ( ( in & 0x4000 ) >> 13 ) |
           ( ( in & 0x2000 ) >> 11 ) |
           ( ( in & 0x1000 ) >>  9 ) |
           ( ( in & 0x0800 ) >>  7 ) |
           ( ( in & 0x0400 ) >>  5 ) |
           ( ( in & 0x0200 ) >>  3 ) |
           ( ( in & 0x0100 ) >>  1 ) |
           ( ( in & 0x0080 ) <<  1 ) |
           ( ( in & 0x0040 ) <<  3 ) |
           ( ( in & 0x0020 ) <<  5 ) |
           ( ( in & 0x0010 ) <<  7 ) |
           ( ( in & 0x0008 ) <<  9 ) |
           ( ( in & 0x0004 ) << 11 ) |
           ( ( in & 0x0002 ) << 13 ) |
           ( ( in & 0x0001 ) << 15 );
#   endif // ANI_LITTLE_BIT_ENDIAN
}

void
swapBitField32(tANI_U32 in, tANI_U32 *out)
{
#   ifdef ANI_LITTLE_BIT_ENDIAN
    *out = in;
#   else // Big-Endian...
    *out = ( ( in & 0x80000000 ) >> 31 ) |
           ( ( in & 0x40000000 ) >> 29 ) |
           ( ( in & 0x20000000 ) >> 27 ) |
           ( ( in & 0x10000000 ) >> 25 ) |
           ( ( in & 0x08000000 ) >> 23 ) |
           ( ( in & 0x04000000 ) >> 21 ) |
           ( ( in & 0x02000000 ) >> 19 ) |
           ( ( in & 0x01000000 ) >> 17 ) |
           ( ( in & 0x00800000 ) >> 15 ) |
           ( ( in & 0x00400000 ) >> 13 ) |
           ( ( in & 0x00200000 ) >> 11 ) |
           ( ( in & 0x00100000 ) >>  9 ) |
           ( ( in & 0x00080000 ) >>  7 ) |
           ( ( in & 0x00040000 ) >>  5 ) |
           ( ( in & 0x00020000 ) >>  3 ) |
           ( ( in & 0x00010000 ) >>  1 ) |
           ( ( in & 0x00008000 ) <<  1 ) |
           ( ( in & 0x00004000 ) <<  3 ) |
           ( ( in & 0x00002000 ) <<  5 ) |
           ( ( in & 0x00001000 ) <<  7 ) |
           ( ( in & 0x00000800 ) <<  9 ) |
           ( ( in & 0x00000400 ) << 11 ) |
           ( ( in & 0x00000200 ) << 13 ) |
           ( ( in & 0x00000100 ) << 15 ) |
           ( ( in & 0x00000080 ) << 17 ) |
           ( ( in & 0x00000040 ) << 19 ) |
           ( ( in & 0x00000020 ) << 21 ) |
           ( ( in & 0x00000010 ) << 23 ) |
           ( ( in & 0x00000008 ) << 25 ) |
           ( ( in & 0x00000004 ) << 27 ) |
           ( ( in & 0x00000002 ) << 29 ) |
           ( ( in & 0x00000001 ) << 31 );
#   endif // ANI_LITTLE_BIT_ENDIAN
}

inline static void __printWMMParams(tpAniSirGlobal  pMac, tDot11fIEWMMParams *pWmm)
{
    limLog(pMac, LOG1, FL("WMM Parameters Received: "));
    limLog(pMac, LOG1, FL("BE: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d "),
           pWmm->acbe_aifsn, pWmm->acbe_acm, pWmm->acbe_aci, pWmm->acbe_acwmin, pWmm->acbe_acwmax, pWmm->acbe_txoplimit);

    limLog(pMac, LOG1, FL("BK: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d "),
           pWmm->acbk_aifsn, pWmm->acbk_acm, pWmm->acbk_aci, pWmm->acbk_acwmin, pWmm->acbk_acwmax, pWmm->acbk_txoplimit);

    limLog(pMac, LOG1, FL("VI: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d "),
           pWmm->acvi_aifsn, pWmm->acvi_acm, pWmm->acvi_aci, pWmm->acvi_acwmin, pWmm->acvi_acwmax, pWmm->acvi_txoplimit);

    limLog(pMac, LOG1, FL("VO: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d "),
           pWmm->acvo_aifsn, pWmm->acvo_acm, pWmm->acvo_aci, pWmm->acvo_acwmin, pWmm->acvo_acwmax, pWmm->acvo_txoplimit);

    return;
}

////////////////////////////////////////////////////////////////////////
// Functions for populating "dot11f" style IEs


// return: >= 0, the starting location of the IE in rsnIEdata inside tSirRSNie
//         < 0, cannot find
int FindIELocation( tpAniSirGlobal pMac,
                           tpSirRSNie pRsnIe,
                           tANI_U8 EID)
{
    int idx, ieLen, bytesLeft;
    int ret_val = -1;

    // Here's what's going on: 'rsnIe' looks like this:

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // other code records both the WPA & RSN IEs (including their EIDs &
    // lengths) into the array 'rsnIEdata'.  We may have:

    //     With WAPI support, there may be 3 IEs here
    //     It can be only WPA IE, or only RSN IE or only WAPI IE
    //     Or two or all three of them with no particular ordering

    // The if/then/else statements that follow are here to figure out
    // whether we have the WPA IE, and where it is if we *do* have it.

    //Save the first IE length 
    ieLen = pRsnIe->rsnIEdata[ 1 ] + 2;
    idx = 0;
    bytesLeft = pRsnIe->length;

    while( 1 )
    {
        if ( EID == pRsnIe->rsnIEdata[ idx ] )
        {
            //Found it
            return (idx);
        }
        else if ( EID != pRsnIe->rsnIEdata[ idx ] &&
             // & if no more IE, 
             bytesLeft <= (tANI_U16)( ieLen ) )
        {
            dot11fLog( pMac, LOG3, FL("No IE (%d) in FindIELocation."), EID );
            return ret_val;
        }
        bytesLeft -= ieLen;
        ieLen = pRsnIe->rsnIEdata[ idx + 1 ] + 2;
        idx += ieLen;
    }

    return ret_val;
}


tSirRetStatus
PopulateDot11fCapabilities(tpAniSirGlobal         pMac,
                           tDot11fFfCapabilities *pDot11f,
                           tpPESession            psessionEntry)
{
    tANI_U16           cfg;
    tSirRetStatus nSirStatus;

    nSirStatus = cfgGetCapabilityInfo( pMac, &cfg,psessionEntry );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        dot11fLog( pMac, LOGP, FL("Failed to retrieve the Capabilities b"
                               "itfield from CFG (%d)."), nSirStatus );
        return nSirStatus;
    }

#if 0
    if ( sirIsPropCapabilityEnabled( pMac, SIR_MAC_PROP_CAPABILITY_11EQOS ) )
    {
        SIR_MAC_CLEAR_CAPABILITY( cfg, QOS );
    }
#endif
    swapBitField16( cfg, ( tANI_U16* )pDot11f );

    return eSIR_SUCCESS;
} // End PopulateDot11fCapabilities.

tSirRetStatus
PopulateDot11fCapabilities2(tpAniSirGlobal         pMac,
                            tDot11fFfCapabilities *pDot11f,
                            tpDphHashNode          pSta, 
                            tpPESession            psessionEntry)
{
    tANI_U16           cfg;
    tSirRetStatus nSirStatus;
    nSirStatus = cfgGetCapabilityInfo( pMac, &cfg ,psessionEntry);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        dot11fLog( pMac, LOGP, FL("Failed to retrieve the Capabilities b"
                               "itfield from CFG (%d)."), nSirStatus );
        return nSirStatus;
    }

    if ( ( NULL != pSta ) && pSta->aniPeer &&
         PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) )
    {
        SIR_MAC_CLEAR_CAPABILITY( cfg, QOS );
    }

    swapBitField16( cfg, ( tANI_U16* )pDot11f );

    return eSIR_SUCCESS;

} // End PopulateDot11fCapabilities2.

void
PopulateDot11fChanSwitchAnn(tpAniSirGlobal          pMac,
                            tDot11fIEChanSwitchAnn *pDot11f,
                            tpPESession psessionEntry)
{
    pDot11f->switchMode = psessionEntry->gLimChannelSwitch.switchMode;
    pDot11f->newChannel = psessionEntry->gLimChannelSwitch.primaryChannel;
    pDot11f->switchCount = ( tANI_U8 ) psessionEntry->gLimChannelSwitch.switchCount;

    pDot11f->present = 1;
} // End PopulateDot11fChanSwitchAnn.

void
PopulateDot11fExtChanSwitchAnn(tpAniSirGlobal pMac,
                               tDot11fIEExtChanSwitchAnn *pDot11f,
                               tpPESession psessionEntry)
{
    //Has to be updated on the cb state basis
    pDot11f->secondaryChannelOffset = 
             psessionEntry->gLimChannelSwitch.secondarySubBand;

    pDot11f->present = 1;
}

#ifdef WLAN_FEATURE_11AC
void
PopulateDot11fWiderBWChanSwitchAnn(tpAniSirGlobal pMac,
                                   tDot11fIEWiderBWChanSwitchAnn *pDot11f,
                                   tpPESession psessionEntry)
{
    pDot11f->present = 1;
    pDot11f->newChanWidth = psessionEntry->gLimWiderBWChannelSwitch.newChanWidth;
    pDot11f->newCenterChanFreq0 = psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq0;
    pDot11f->newCenterChanFreq1 = psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq1;
}
#endif

tSirRetStatus
PopulateDot11fCountry(tpAniSirGlobal    pMac,
                      tDot11fIECountry *pDot11f,
                      tpPESession psessionEntry)
{
    tANI_U32           len, maxlen, codelen;
    tANI_U16           item;
    tSirRetStatus nSirStatus;
    tSirRFBand         rfBand;
    tANI_U8            temp[CFG_MAX_STR_LEN], code[3];

    if (psessionEntry->lim11dEnabled )
    {
        limGetRfBand(pMac, &rfBand, psessionEntry);
        if (rfBand == SIR_BAND_5_GHZ)
        {
            item   = WNI_CFG_MAX_TX_POWER_5;
            maxlen = WNI_CFG_MAX_TX_POWER_5_LEN;
        }
        else
        {
            item   = WNI_CFG_MAX_TX_POWER_2_4;
            maxlen = WNI_CFG_MAX_TX_POWER_2_4_LEN;
        }

        CFG_GET_STR( nSirStatus, pMac, item, temp, len, maxlen );

        if ( 3 > len )
        {
            // no limit on tx power, cannot include the IE because at least
            // one (channel,num,tx power) must be present
            return eSIR_SUCCESS;
        }

        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_COUNTRY_CODE,
                     code, codelen, 3 );

        vos_mem_copy( pDot11f->country, code, codelen );

        if(len > MAX_SIZE_OF_TRIPLETS_IN_COUNTRY_IE)
        {
            dot11fLog( pMac, LOGE, FL("len:%d is out of bounds, resetting."), len);
            len = MAX_SIZE_OF_TRIPLETS_IN_COUNTRY_IE;
        }

        pDot11f->num_triplets = ( tANI_U8 ) ( len / 3 );
        vos_mem_copy( ( tANI_U8* )pDot11f->triplets, temp, len );

        pDot11f->present = 1;
    }

    return eSIR_SUCCESS;
} // End PopulateDot11fCountry.

tSirRetStatus
PopulateDot11fDSParams(tpAniSirGlobal     pMac,
                       tDot11fIEDSParams *pDot11f, tANI_U8 channel,
                       tpPESession psessionEntry)
{
    if (IS_24G_CH(channel))
    {
        // .11b/g mode PHY => Include the DS Parameter Set IE:
        pDot11f->curr_channel = channel;
        pDot11f->present = 1;
    }

    return eSIR_SUCCESS;
} // End PopulateDot11fDSParams.

#define SET_AIFSN(aifsn) (((aifsn) < 2) ? 2 : (aifsn))


void
PopulateDot11fEDCAParamSet(tpAniSirGlobal         pMac,
                           tDot11fIEEDCAParamSet *pDot11f, 
                           tpPESession psessionEntry)
{

    if (  psessionEntry->limQosEnabled )
    {
        //change to bitwise operation, after this is fixed in frames.
        pDot11f->qos = (tANI_U8)(0xf0 & (psessionEntry->gLimEdcaParamSetCount << 4) );

        // Fill each EDCA parameter set in order: be, bk, vi, vo
        pDot11f->acbe_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[0].aci.aifsn) );
        pDot11f->acbe_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[0].aci.acm );
        pDot11f->acbe_aci       = ( 0x3 & SIR_MAC_EDCAACI_BESTEFFORT );
        pDot11f->acbe_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[0].cw.min );
        pDot11f->acbe_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[0].cw.max );
        pDot11f->acbe_txoplimit = psessionEntry->gLimEdcaParamsBC[0].txoplimit;

        pDot11f->acbk_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[1].aci.aifsn) );
        pDot11f->acbk_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[1].aci.acm );
        pDot11f->acbk_aci       = ( 0x3 & SIR_MAC_EDCAACI_BACKGROUND );
        pDot11f->acbk_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[1].cw.min );
        pDot11f->acbk_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[1].cw.max );
        pDot11f->acbk_txoplimit = psessionEntry->gLimEdcaParamsBC[1].txoplimit;

        pDot11f->acvi_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[2].aci.aifsn) );
        pDot11f->acvi_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[2].aci.acm );
        pDot11f->acvi_aci       = ( 0x3 & SIR_MAC_EDCAACI_VIDEO );
        pDot11f->acvi_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[2].cw.min );
        pDot11f->acvi_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[2].cw.max );
        pDot11f->acvi_txoplimit = psessionEntry->gLimEdcaParamsBC[2].txoplimit;

        pDot11f->acvo_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[3].aci.aifsn) );
        pDot11f->acvo_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[3].aci.acm );
        pDot11f->acvo_aci       = ( 0x3 & SIR_MAC_EDCAACI_VOICE );
        pDot11f->acvo_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[3].cw.min );
        pDot11f->acvo_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[3].cw.max );
        pDot11f->acvo_txoplimit = psessionEntry->gLimEdcaParamsBC[3].txoplimit;

        pDot11f->present = 1;
    }

} // End PopluateDot11fEDCAParamSet.

tSirRetStatus
PopulateDot11fERPInfo(tpAniSirGlobal    pMac,
                      tDot11fIEERPInfo *pDot11f,
                      tpPESession    psessionEntry)
{
    tSirRetStatus nSirStatus;
    tANI_U32            val;
    tSirRFBand          rfBand = SIR_BAND_UNKNOWN;

    limGetRfBand(pMac, &rfBand, psessionEntry);
    if(SIR_BAND_2_4_GHZ == rfBand)
    {
        pDot11f->present = 1;

        val  = psessionEntry->cfgProtection.fromllb;
        if(!val ){
            dot11fLog( pMac, LOGE, FL("11B protection not enabled. Not populating ERP IE %d" ),val );
            return eSIR_SUCCESS;
        }

        if (psessionEntry->gLim11bParams.protectionEnabled)
        {
            pDot11f->non_erp_present = 1;
            pDot11f->use_prot        = 1;
        }

        if ( psessionEntry->gLimOlbcParams.protectionEnabled )
        {
            //FIXME_PROTECTION: we should be setting non_erp present also.
            //check the test plan first.
            pDot11f->use_prot = 1;
        }


        if((psessionEntry->gLimNoShortParams.numNonShortPreambleSta) 
                 || !psessionEntry->beaconParams.fShortPreamble){ 
                pDot11f->barker_preamble = 1;
         
        }
        // if protection always flag is set, advertise protection enabled
        // regardless of legacy stations presence
        CFG_GET_INT( nSirStatus, pMac, WNI_CFG_11G_PROTECTION_ALWAYS, val );

        if ( val )
        {
            pDot11f->use_prot = 1;
        }
    }

    return eSIR_SUCCESS;
} // End PopulateDot11fERPInfo.

tSirRetStatus
PopulateDot11fExtSuppRates(tpAniSirGlobal pMac, tANI_U8 nChannelNum,
                           tDot11fIEExtSuppRates *pDot11f, 
                           tpPESession psessionEntry)
{
    tSirRetStatus nSirStatus;
    tANI_U32           nRates = 0;
    tANI_U8            rates[SIR_MAC_RATESET_EID_MAX];

   /* Use the ext rates present in session entry whenever nChannelNum is set to OPERATIONAL
       else use the ext supported rate set from CFG, which is fixed and does not change dynamically and is used for
       sending mgmt frames (lile probe req) which need to go out before any session is present.
   */
    if(POPULATE_DOT11F_RATES_OPERATIONAL == nChannelNum )
    {
        if(psessionEntry != NULL)
        {
            nRates = psessionEntry->extRateSet.numRates;
            vos_mem_copy( rates, psessionEntry->extRateSet.rate,
                          nRates);
        }
        else
        {
            dot11fLog( pMac, LOGE, FL("no session context exists while"
                        " populating Operational Rate Set"));
        }
    }
    else if ( HIGHEST_24GHZ_CHANNEL_NUM >= nChannelNum )
    {
        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                     rates, nRates, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN );
    }

    if ( 0 != nRates )
    {
        pDot11f->num_rates = ( tANI_U8 )nRates;
        vos_mem_copy( pDot11f->rates, rates, nRates );
        pDot11f->present   = 1;
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fExtSuppRates.

tSirRetStatus
PopulateDot11fExtSuppRates1(tpAniSirGlobal         pMac,
                            tANI_U8                     nChannelNum,
                            tDot11fIEExtSuppRates *pDot11f)
{
    tANI_U32           nRates;
    tSirRetStatus nSirStatus;
    tANI_U8            rates[SIR_MAC_MAX_NUMBER_OF_RATES];

    if ( 14 < nChannelNum )
    {
        pDot11f->present = 0;
        return eSIR_SUCCESS;
    }

    // N.B. I have *no* idea why we're calling 'wlan_cfgGetStr' with an argument
    // of WNI_CFG_SUPPORTED_RATES_11A here, but that's what was done
    // previously & I'm afraid to change it!
    CFG_GET_STR( nSirStatus, pMac, WNI_CFG_SUPPORTED_RATES_11A,
                 rates, nRates, SIR_MAC_MAX_NUMBER_OF_RATES );

    if ( 0 != nRates )
    {
        pDot11f->num_rates = ( tANI_U8 ) nRates;
        vos_mem_copy( pDot11f->rates, rates, nRates );
        pDot11f->present   = 1;
    }

    return eSIR_SUCCESS;
} // PopulateDot11fExtSuppRates1.

tSirRetStatus
PopulateDot11fHTCaps(tpAniSirGlobal           pMac,
                           tpPESession      psessionEntry,
                           tDot11fIEHTCaps *pDot11f)
{
    tANI_U32                         nCfgValue, nCfgLen;
    tANI_U8                          nCfgValue8;
    tSirRetStatus                    nSirStatus;
    tSirMacHTParametersInfo         *pHTParametersInfo;
    union {
        tANI_U16                        nCfgValue16;
        tSirMacHTCapabilityInfo         htCapInfo;
        tSirMacExtendedHTCapabilityInfo extHtCapInfo;
    } uHTCapabilityInfo;

    tSirMacTxBFCapabilityInfo       *pTxBFCapabilityInfo;
    tSirMacASCapabilityInfo         *pASCapabilityInfo;

    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_CAP_INFO, nCfgValue );

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->advCodingCap             = uHTCapabilityInfo.htCapInfo.advCodingCap;
    pDot11f->mimoPowerSave            = uHTCapabilityInfo.htCapInfo.mimoPowerSave;
    pDot11f->greenField               = uHTCapabilityInfo.htCapInfo.greenField;
    pDot11f->shortGI20MHz             = uHTCapabilityInfo.htCapInfo.shortGI20MHz;
    pDot11f->shortGI40MHz             = uHTCapabilityInfo.htCapInfo.shortGI40MHz;
    pDot11f->txSTBC                   = uHTCapabilityInfo.htCapInfo.txSTBC;
    pDot11f->rxSTBC                   = uHTCapabilityInfo.htCapInfo.rxSTBC;
    pDot11f->delayedBA                = uHTCapabilityInfo.htCapInfo.delayedBA;
    pDot11f->maximalAMSDUsize         = uHTCapabilityInfo.htCapInfo.maximalAMSDUsize;
    pDot11f->dsssCckMode40MHz         = uHTCapabilityInfo.htCapInfo.dsssCckMode40MHz;
    pDot11f->psmp                     = uHTCapabilityInfo.htCapInfo.psmp;
    pDot11f->stbcControlFrame         = uHTCapabilityInfo.htCapInfo.stbcControlFrame;
    pDot11f->lsigTXOPProtection       = uHTCapabilityInfo.htCapInfo.lsigTXOPProtection;

    // All sessionized entries will need the check below
    if (psessionEntry == NULL) // Only in case of NO session
    {
        pDot11f->supportedChannelWidthSet = uHTCapabilityInfo.htCapInfo.supportedChannelWidthSet;
    }
    else
    {
        pDot11f->supportedChannelWidthSet = psessionEntry->htSupportedChannelWidthSet;
    }

    /* Ensure that shortGI40MHz is Disabled if supportedChannelWidthSet is
       eHT_CHANNEL_WIDTH_20MHZ */
    if(pDot11f->supportedChannelWidthSet == eHT_CHANNEL_WIDTH_20MHZ)
    {
       pDot11f->shortGI40MHz = 0;
    }



    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_AMPDU_PARAMS, nCfgValue );

    nCfgValue8 = ( tANI_U8 ) nCfgValue;
    pHTParametersInfo = ( tSirMacHTParametersInfo* ) &nCfgValue8;

    pDot11f->maxRxAMPDUFactor = pHTParametersInfo->maxRxAMPDUFactor;
    pDot11f->mpduDensity      = pHTParametersInfo->mpduDensity;
    pDot11f->reserved1        = pHTParametersInfo->reserved;

    CFG_GET_STR( nSirStatus, pMac, WNI_CFG_SUPPORTED_MCS_SET,
                 pDot11f->supportedMCSSet, nCfgLen,
                 SIZE_OF_SUPPORTED_MCS_SET );


    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_EXT_HT_CAP_INFO, nCfgValue );

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->pco            = uHTCapabilityInfo.extHtCapInfo.pco;
    pDot11f->transitionTime = uHTCapabilityInfo.extHtCapInfo.transitionTime;
    pDot11f->mcsFeedback    = uHTCapabilityInfo.extHtCapInfo.mcsFeedback;


    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_TX_BF_CAP, nCfgValue );

    pTxBFCapabilityInfo = ( tSirMacTxBFCapabilityInfo* ) &nCfgValue;
    pDot11f->txBF                                       = pTxBFCapabilityInfo->txBF;
    pDot11f->rxStaggeredSounding                        = pTxBFCapabilityInfo->rxStaggeredSounding;
    pDot11f->txStaggeredSounding                        = pTxBFCapabilityInfo->txStaggeredSounding;
    pDot11f->rxZLF                                      = pTxBFCapabilityInfo->rxZLF;
    pDot11f->txZLF                                      = pTxBFCapabilityInfo->txZLF;
    pDot11f->implicitTxBF                               = pTxBFCapabilityInfo->implicitTxBF;
    pDot11f->calibration                                = pTxBFCapabilityInfo->calibration;
    pDot11f->explicitCSITxBF                            = pTxBFCapabilityInfo->explicitCSITxBF;
    pDot11f->explicitUncompressedSteeringMatrix         = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrix;
    pDot11f->explicitBFCSIFeedback                      = pTxBFCapabilityInfo->explicitBFCSIFeedback;
    pDot11f->explicitUncompressedSteeringMatrixFeedback = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrixFeedback;
    pDot11f->explicitCompressedSteeringMatrixFeedback   = pTxBFCapabilityInfo->explicitCompressedSteeringMatrixFeedback;
    pDot11f->csiNumBFAntennae                           = pTxBFCapabilityInfo->csiNumBFAntennae;
    pDot11f->uncompressedSteeringMatrixBFAntennae       = pTxBFCapabilityInfo->uncompressedSteeringMatrixBFAntennae;
    pDot11f->compressedSteeringMatrixBFAntennae         = pTxBFCapabilityInfo->compressedSteeringMatrixBFAntennae;

    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_AS_CAP, nCfgValue );

    nCfgValue8 = ( tANI_U8 ) nCfgValue;

    pASCapabilityInfo = ( tSirMacASCapabilityInfo* ) &nCfgValue8;
    pDot11f->antennaSelection         = pASCapabilityInfo->antennaSelection;
    pDot11f->explicitCSIFeedbackTx    = pASCapabilityInfo->explicitCSIFeedbackTx;
    pDot11f->antennaIndicesFeedbackTx = pASCapabilityInfo->antennaIndicesFeedbackTx;
    pDot11f->explicitCSIFeedback      = pASCapabilityInfo->explicitCSIFeedback;
    pDot11f->antennaIndicesFeedback   = pASCapabilityInfo->antennaIndicesFeedback;
    pDot11f->rxAS                     = pASCapabilityInfo->rxAS;
    pDot11f->txSoundingPPDUs          = pASCapabilityInfo->txSoundingPPDUs;

    pDot11f->present = 1;

    return eSIR_SUCCESS;

} // End PopulateDot11fHTCaps.
#ifdef WLAN_FEATURE_11AC

void limLogVHTCap(tpAniSirGlobal pMac,
                              tDot11fIEVHTCaps *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
    limLog(pMac, LOG1, FL("maxMPDULen (2): %d"), pDot11f->maxMPDULen);
    limLog(pMac, LOG1, FL("supportedChannelWidthSet (2): %d"), pDot11f->supportedChannelWidthSet);
    limLog(pMac, LOG1, FL("ldpcCodingCap (1): %d"), pDot11f->ldpcCodingCap);
    limLog(pMac, LOG1, FL("shortGI80MHz (1): %d"), pDot11f->shortGI80MHz);
    limLog(pMac, LOG1, FL("shortGI160and80plus80MHz (1): %d"), pDot11f->shortGI160and80plus80MHz);
    limLog(pMac, LOG1, FL("txSTBC (1): %d"), pDot11f->txSTBC);
    limLog(pMac, LOG1, FL("rxSTBC (3): %d"), pDot11f->rxSTBC);
    limLog(pMac, LOG1, FL("suBeamFormerCap (1): %d"), pDot11f->suBeamFormerCap);
    limLog(pMac, LOG1, FL("suBeamformeeCap (1): %d"), pDot11f->suBeamformeeCap);
    limLog(pMac, LOG1, FL("csnofBeamformerAntSup (3): %d"), pDot11f->csnofBeamformerAntSup);
    limLog(pMac, LOG1, FL("numSoundingDim (3): %d"), pDot11f->numSoundingDim);
    limLog(pMac, LOG1, FL("muBeamformerCap (1): %d"), pDot11f->muBeamformerCap);
    limLog(pMac, LOG1, FL("muBeamformeeCap (1): %d"), pDot11f->muBeamformeeCap);
    limLog(pMac, LOG1, FL("vhtTXOPPS (1): %d"), pDot11f->vhtTXOPPS);
    limLog(pMac, LOG1, FL("htcVHTCap (1): %d"), pDot11f->htcVHTCap);
    limLog(pMac, LOG1, FL("maxAMPDULenExp (3): %d"), pDot11f->maxAMPDULenExp);
    limLog(pMac, LOG1, FL("vhtLinkAdaptCap (2): %d"), pDot11f->vhtLinkAdaptCap);
    limLog(pMac, LOG1, FL("rxAntPattern (1): %d"), pDot11f->vhtLinkAdaptCap);
    limLog(pMac, LOG1, FL("txAntPattern (1): %d"), pDot11f->vhtLinkAdaptCap);
    limLog(pMac, LOG1, FL("reserved1 (2): %d"), pDot11f->reserved1);
    limLog(pMac, LOG1, FL("rxMCSMap (16): %d"), pDot11f->rxMCSMap);
    limLog(pMac, LOG1, FL("rxHighSupDataRate (13): %d"), pDot11f->rxHighSupDataRate);
    limLog(pMac, LOG1, FL("reserve (3): %d"), pDot11f->reserved2);
    limLog(pMac, LOG1, FL("txMCSMap (16): %d"), pDot11f->txMCSMap);
    limLog(pMac, LOG1, FL("txSupDataRate (13): %d"), pDot11f->txSupDataRate);
    limLog(pMac, LOG1, FL("reserv (3): %d"), pDot11f->reserved3);
#endif /* DUMP_MGMT_CNTNTS */
}

void limLogVHTOperation(tpAniSirGlobal pMac,
                              tDot11fIEVHTOperation *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
    limLog(pMac, LOG1, FL("chanWidth : %d"), pDot11f->chanWidth);
    limLog(pMac, LOG1, FL("chanCenterFreqSeg1: %d"), pDot11f->chanCenterFreqSeg1);
    limLog(pMac, LOG1, FL("chanCenterFreqSeg2: %d"), pDot11f->chanCenterFreqSeg2);
    limLog(pMac, LOG1, FL("basicMCSSet: %d"), pDot11f->basicMCSSet);
#endif /* DUMP_MGMT_CNTNTS */
}

void limLogVHTExtBssLoad(tpAniSirGlobal pMac,
                              tDot11fIEVHTExtBssLoad *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
    limLog(pMac, LOG1, FL("muMIMOCapStaCount : %d"), pDot11f->muMIMOCapStaCount);
    limLog(pMac, LOG1, FL("ssUnderUtil: %d"), pDot11f->ssUnderUtil);
    limLog(pMac, LOG1, FL("FortyMHzUtil: %d"), pDot11f->FortyMHzUtil);
    limLog(pMac, LOG1, FL("EightyMHzUtil: %d"), pDot11f->EightyMHzUtil);
    limLog(pMac, LOG1, FL("OneSixtyMHzUtil: %d"), pDot11f->OneSixtyMHzUtil);
#endif /* DUMP_MGMT_CNTNTS */
}


void limLogOperatingMode( tpAniSirGlobal pMac, 
                               tDot11fIEOperatingMode *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
    limLog(pMac, LOG1, FL("ChanWidth : %d"), pDot11f->chanWidth);
    limLog(pMac, LOG1, FL("reserved: %d"), pDot11f->reserved);
    limLog(pMac, LOG1, FL("rxNSS: %d"), pDot11f->rxNSS);
    limLog(pMac, LOG1, FL("rxNSS Type: %d"), pDot11f->rxNSSType);
#endif /* DUMP_MGMT_CNTNTS */
}

void limLogQosMapSet(tpAniSirGlobal pMac, tSirQosMapSet *pQosMapSet)
{
    tANI_U8 i;
    limLog(pMac, LOG1, FL("num of dscp exceptions : %d"),
                                   pQosMapSet->num_dscp_exceptions);
    for (i=0; i < pQosMapSet->num_dscp_exceptions; i++)
    {
        limLog(pMac, LOG1, FL("dscp value: %d"),
                                 pQosMapSet->dscp_exceptions[i][0]);
        limLog(pMac, LOG1, FL("User priority value: %d"),
                                 pQosMapSet->dscp_exceptions[i][1]);
    }
    for (i=0;i<8;i++)
    {
        limLog(pMac, LOG1, FL("dscp low for up %d: %d"),i,
                                      pQosMapSet->dscp_range[i][0]);
        limLog(pMac, LOG1, FL("dscp high for up %d: %d"),i,
                                      pQosMapSet->dscp_range[i][1]);
    }
}

tSirRetStatus
PopulateDot11fVHTCaps(tpAniSirGlobal           pMac,
                      tDot11fIEVHTCaps *pDot11f,
                      tANI_U8 nChannelNum,
                      tAniBool isProbeRspAssocRspBeacon)
{
    tSirRetStatus        nStatus;
    tANI_U32             nCfgValue=0;
    tAniBool             disableMcs9 = eSIR_FALSE;

    if (nChannelNum <= SIR_11B_CHANNEL_END)
        disableMcs9 =  pMac->roam.configParam.channelBondingMode24GHz?
                       eSIR_FALSE:eSIR_TRUE;
    else
        disableMcs9 =
            pMac->roam.configParam.channelBondingMode5GHz?
                                      eSIR_FALSE: eSIR_TRUE;
    pDot11f->present = 1;

    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_MAX_MPDU_LENGTH, nCfgValue );
    pDot11f->maxMPDULen =  (nCfgValue & 0x0003);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
                                                             nCfgValue );
    pDot11f->supportedChannelWidthSet = (nCfgValue & 0x0003);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_LDPC_CODING_CAP, nCfgValue );
    pDot11f->ldpcCodingCap = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SHORT_GI_80MHZ, nCfgValue );
    pDot11f->shortGI80MHz= (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
                                                                nCfgValue );
    pDot11f->shortGI160and80plus80MHz = (nCfgValue & 0x0001);

    if (nChannelNum && (SIR_BAND_2_4_GHZ == limGetRFBand(nChannelNum)))
    {
        pDot11f->shortGI80MHz = 0;
        pDot11f->shortGI160and80plus80MHz = 0;
    }

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_TXSTBC, nCfgValue );
    pDot11f->txSTBC = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_RXSTBC, nCfgValue );
    pDot11f->rxSTBC = (nCfgValue & 0x0007);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SU_BEAMFORMER_CAP, nCfgValue );
    pDot11f->suBeamFormerCap = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP, nCfgValue );
    pDot11f->suBeamformeeCap = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED,
                                                               nCfgValue );
    pDot11f->csnofBeamformerAntSup = (nCfgValue & 0x0007);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_NUM_SOUNDING_DIMENSIONS,
                                                               nCfgValue );
    pDot11f->numSoundingDim = (nCfgValue & 0x0007);

    /* muBeamformerCap should be 0 for non AP and
     * muBeamformeeCap should be 0 for AP
     */
    if(eSIR_TRUE == isProbeRspAssocRspBeacon)
    {
       nCfgValue = 0;
       CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_MU_BEAMFORMER_CAP, nCfgValue );
       pDot11f->muBeamformerCap = (nCfgValue & 0x0001);
       pDot11f->muBeamformeeCap = 0;
    }
    else
    {
       pDot11f->muBeamformerCap = 0;
       nCfgValue = 0;
       CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_MU_BEAMFORMEE_CAP, nCfgValue );
       /* Enable only if FW and host both support the MU_MIMO feature
        */
       pDot11f->muBeamformeeCap = IS_MUMIMO_BFORMEE_CAPABLE ? (nCfgValue & 0x0001): 0;
    }

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_TXOP_PS, nCfgValue );
    pDot11f->vhtTXOPPS = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_HTC_VHTC_CAP, nCfgValue );
    pDot11f->htcVHTCap = (nCfgValue & 0x0001);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_AMPDU_LEN_EXPONENT, nCfgValue );
    pDot11f->maxAMPDULenExp = (nCfgValue & 0x0007);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_LINK_ADAPTATION_CAP, nCfgValue );
    pDot11f->vhtLinkAdaptCap = (nCfgValue & 0x0003);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_RX_ANT_PATTERN, nCfgValue );
    pDot11f->rxAntPattern = nCfgValue;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_TX_ANT_PATTERN, nCfgValue );
    pDot11f->txAntPattern = nCfgValue;

    pDot11f->reserved1= 0;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_RX_MCS_MAP, nCfgValue );

    if (eSIR_TRUE == disableMcs9)
       nCfgValue = (nCfgValue & 0xFFFC) | 0x1;
    pDot11f->rxMCSMap = (nCfgValue & 0x0000FFFF);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE,
                                                                  nCfgValue );
    pDot11f->rxHighSupDataRate = (nCfgValue & 0x00001FFF);

    pDot11f->reserved2= 0;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_TX_MCS_MAP, nCfgValue );

    if (eSIR_TRUE == disableMcs9)
       nCfgValue = (nCfgValue & 0xFFFC) | 0x1;
    pDot11f->txMCSMap = (nCfgValue & 0x0000FFFF);

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_TX_HIGHEST_SUPPORTED_DATA_RATE,
                                                                  nCfgValue );
    pDot11f->txSupDataRate = (nCfgValue & 0x00001FFF);

    pDot11f->reserved3= 0;

    limLogVHTCap(pMac, pDot11f);

    return eSIR_SUCCESS;

}

tSirRetStatus
PopulateDot11fVHTOperation(tpAniSirGlobal   pMac,
                               tDot11fIEVHTOperation  *pDot11f,
                               tANI_U8 nChannelNum)
{
    tSirRetStatus        nStatus;
    tANI_U32             nCfgValue=0;
    tAniBool             disableMcs9 = eSIR_FALSE;

    if (nChannelNum <= SIR_11B_CHANNEL_END)
        disableMcs9 =  pMac->roam.configParam.channelBondingMode24GHz?
                       eSIR_FALSE:eSIR_TRUE;
    else
        disableMcs9 =
            pMac->roam.configParam.channelBondingMode5GHz?
                                      eSIR_FALSE: eSIR_TRUE;

    pDot11f->present = 1;

    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_CHANNEL_WIDTH, nCfgValue );
    pDot11f->chanWidth = (tANI_U8)nCfgValue;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_CHANNEL_CENTER_FREQ_SEGMENT1,
                                                               nCfgValue );
    pDot11f->chanCenterFreqSeg1 = (tANI_U8)nCfgValue;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_CHANNEL_CENTER_FREQ_SEGMENT2,
                                                               nCfgValue );
    pDot11f->chanCenterFreqSeg2 = (tANI_U8)nCfgValue;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_BASIC_MCS_SET,nCfgValue );

    if (eSIR_TRUE == disableMcs9)
       nCfgValue = (nCfgValue & 0xFFFC) | 0x1;
    pDot11f->basicMCSSet = (tANI_U16)nCfgValue;

    limLogVHTOperation(pMac,pDot11f);

    return eSIR_SUCCESS;

}

tSirRetStatus
PopulateDot11fVHTExtBssLoad(tpAniSirGlobal      pMac,
                           tDot11fIEVHTExtBssLoad   *pDot11f)
{
    tSirRetStatus    nStatus;
    tANI_U32         nCfgValue=0;

    pDot11f->present = 1;

    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_MU_MIMO_CAP_STA_COUNT,
                                                         nCfgValue );
    pDot11f->muMIMOCapStaCount = (tANI_U8)nCfgValue;

    nCfgValue = 0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_SS_UNDER_UTIL,nCfgValue );
    pDot11f->ssUnderUtil = (tANI_U8)nCfgValue;

    nCfgValue=0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_40MHZ_UTILIZATION,nCfgValue );
    pDot11f->FortyMHzUtil = (tANI_U8)nCfgValue;

    nCfgValue=0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_80MHZ_UTILIZATION,nCfgValue );
    pDot11f->EightyMHzUtil = (tANI_U8)nCfgValue;

    nCfgValue=0;
    CFG_GET_INT( nStatus, pMac, WNI_CFG_VHT_160MHZ_UTILIZATION,nCfgValue );
    pDot11f->EightyMHzUtil = (tANI_U8)nCfgValue;

    limLogVHTExtBssLoad(pMac,pDot11f);

    return eSIR_SUCCESS;
}

#ifdef WLAN_FEATURE_AP_HT40_24G
tSirRetStatus
PopulateDot11fOBSSScanParameters(tpAniSirGlobal pMac,
                                 tDot11fIEOBSSScanParameters *pDot11f,
                                 tpPESession psessionEntry)
{
    pDot11f->present = 1;

    pDot11f->obssScanPassiveDwell =
          psessionEntry->obssHT40ScanParam.OBSSScanPassiveDwellTime;

    pDot11f->obssScanActiveDwell =
          psessionEntry->obssHT40ScanParam.OBSSScanActiveDwellTime;

    pDot11f->bssChannelWidthTriggerScanInterval =
          psessionEntry->obssHT40ScanParam.BSSChannelWidthTriggerScanInterval;

    pDot11f->obssScanPassiveTotalPerChannel =
          psessionEntry->obssHT40ScanParam.OBSSScanPassiveTotalPerChannel;

    pDot11f->obssScanActiveTotalPerChannel =
          psessionEntry->obssHT40ScanParam.OBSSScanActiveTotalPerChannel;

    pDot11f->bssWidthChannelTransitionDelayFactor =
          psessionEntry->obssHT40ScanParam.BSSWidthChannelTransitionDelayFactor;

    pDot11f->obssScanActivityThreshold =
          psessionEntry->obssHT40ScanParam.OBSSScanActivityThreshold;

    return eSIR_SUCCESS;
}
#endif

tSirRetStatus
PopulateDot11fExtCap(tpAniSirGlobal      pMac,
                           tDot11fIEExtCap  *pDot11f,
                           tpPESession   psessionEntry)
{

#ifdef WLAN_FEATURE_11AC
    if (psessionEntry->vhtCapability &&
        psessionEntry->limSystemRole != eLIM_STA_IN_IBSS_ROLE )
    {
        pDot11f->operModeNotification = 1;
        pDot11f->present = 1;
    }
#endif
       /* while operating in 2.4GHz only then STA need to advertize
               the bss co-ex capability*/
    if (psessionEntry->currentOperChannel <= RF_CHAN_14)
    {
#ifdef WLAN_FEATURE_AP_HT40_24G
       if(((IS_HT40_OBSS_SCAN_FEATURE_ENABLE)
         && pMac->roam.configParam.channelBondingMode24GHz)
         || pMac->roam.configParam.apHT40_24GEnabled)
#else
       if((IS_HT40_OBSS_SCAN_FEATURE_ENABLE)
         && pMac->roam.configParam.channelBondingMode24GHz)
#endif
       {
           pDot11f->bssCoexistMgmtSupport = 1;
           pDot11f->present = 1;
       }
    }
    return eSIR_SUCCESS;
}

tSirRetStatus
PopulateDot11fOperatingMode(tpAniSirGlobal      pMac,
                           tDot11fIEOperatingMode  *pDot11f,
                           tpPESession   psessionEntry)
{
    pDot11f->present = 1;
    
    pDot11f->chanWidth = psessionEntry->gLimOperatingMode.chanWidth;
    pDot11f->rxNSS = psessionEntry->gLimOperatingMode.rxNSS;
    pDot11f->rxNSSType = psessionEntry->gLimOperatingMode.rxNSSType;
    
    return eSIR_SUCCESS;
}

#endif
tSirRetStatus
PopulateDot11fHTInfo(tpAniSirGlobal   pMac,
                     tDot11fIEHTInfo *pDot11f,
                     tpPESession      psessionEntry )
{
    tANI_U32             nCfgValue, nCfgLen;
    tANI_U8              htInfoField1;
    tANI_U16            htInfoField2;
    tSirRetStatus        nSirStatus;
    tSirMacHTInfoField1 *pHTInfoField1;
    tSirMacHTInfoField2 *pHTInfoField2;
    union {
        tANI_U16         nCfgValue16;
        tSirMacHTInfoField3 infoField3;
    }uHTInfoField;
    union {
        tANI_U16         nCfgValue16;
        tSirMacHTInfoField2 infoField2;
    }uHTInfoField2={0};


    #if 0
    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_CURRENT_CHANNEL, nCfgValue );
    #endif // TO SUPPORT BT-AMP

    if (NULL == psessionEntry)
    {
        PELOGE(limLog(pMac, LOG1,
                FL("Invalid session entry in PopulateDot11fHTInfo()"));)
        return eSIR_FAILURE;
    }

    pDot11f->primaryChannel = psessionEntry->currentOperChannel;

    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_INFO_FIELD1, nCfgValue );

    htInfoField1 = ( tANI_U8 ) nCfgValue;

    pHTInfoField1 = ( tSirMacHTInfoField1* ) &htInfoField1;
    pHTInfoField1->rifsMode                   = psessionEntry->beaconParams.fRIFSMode;
    pHTInfoField1->serviceIntervalGranularity = pMac->lim.gHTServiceIntervalGranularity;

    if (psessionEntry == NULL)
    {
        PELOGE(limLog(pMac, LOG1,
            FL("Keep the value retrieved from cfg for secondary channel offset and recommended Tx Width set"));)
    }
    else
    {
        pHTInfoField1->secondaryChannelOffset     = psessionEntry->htSecondaryChannelOffset;
        pHTInfoField1->recommendedTxWidthSet      = psessionEntry->htRecommendedTxWidthSet;
    }

    if((psessionEntry) && (psessionEntry->limSystemRole == eLIM_AP_ROLE)){
    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_INFO_FIELD2, nCfgValue );

    uHTInfoField2.nCfgValue16 = nCfgValue & 0xFFFF; // this is added for fixing CRs on MDM9K platform - 257951, 259577

    uHTInfoField2.infoField2.opMode   =  psessionEntry->htOperMode;
    uHTInfoField2.infoField2.nonGFDevicesPresent = psessionEntry->beaconParams.llnNonGFCoexist;
    uHTInfoField2.infoField2.obssNonHTStaPresent = psessionEntry->beaconParams.gHTObssMode;   /*added for Obss  */

    uHTInfoField2.infoField2.reserved = 0;

   }else{
        CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_INFO_FIELD2, nCfgValue );

        htInfoField2 = ( tANI_U16 ) nCfgValue;

        pHTInfoField2 = ( tSirMacHTInfoField2* ) &htInfoField2;
        pHTInfoField2->opMode   = pMac->lim.gHTOperMode;
        pHTInfoField2->nonGFDevicesPresent = pMac->lim.gHTNonGFDevicesPresent;
        pHTInfoField2->obssNonHTStaPresent = pMac->lim.gHTObssMode;   /*added for Obss  */

        pHTInfoField2->reserved = 0;
    }

    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_HT_INFO_FIELD3, nCfgValue );


    uHTInfoField.nCfgValue16 = nCfgValue & 0xFFFF;


    uHTInfoField.infoField3.basicSTBCMCS                  = pMac->lim.gHTSTBCBasicMCS;
    uHTInfoField.infoField3.dualCTSProtection             = pMac->lim.gHTDualCTSProtection;
    uHTInfoField.infoField3.secondaryBeacon               = pMac->lim.gHTSecondaryBeacon;
    uHTInfoField.infoField3.lsigTXOPProtectionFullSupport = psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport;
    uHTInfoField.infoField3.pcoActive                     = pMac->lim.gHTPCOActive;
    uHTInfoField.infoField3.pcoPhase                      = pMac->lim.gHTPCOPhase;
    uHTInfoField.infoField3.reserved                      = 0;


    pDot11f->secondaryChannelOffset        = pHTInfoField1->secondaryChannelOffset;
    pDot11f->recommendedTxWidthSet         = pHTInfoField1->recommendedTxWidthSet;
    pDot11f->rifsMode                      = pHTInfoField1->rifsMode;
    pDot11f->controlledAccessOnly          = pHTInfoField1->controlledAccessOnly;
    pDot11f->serviceIntervalGranularity    = pHTInfoField1->serviceIntervalGranularity;

    pDot11f->opMode                        = uHTInfoField2.infoField2.opMode;
    pDot11f->nonGFDevicesPresent           = uHTInfoField2.infoField2.nonGFDevicesPresent;
    pDot11f->obssNonHTStaPresent           = uHTInfoField2.infoField2.obssNonHTStaPresent;
    pDot11f->reserved                      = uHTInfoField2.infoField2.reserved;


    pDot11f->basicSTBCMCS                  = uHTInfoField.infoField3.basicSTBCMCS;
    pDot11f->dualCTSProtection             = uHTInfoField.infoField3.dualCTSProtection;
    pDot11f->secondaryBeacon               = uHTInfoField.infoField3.secondaryBeacon;
    pDot11f->lsigTXOPProtectionFullSupport = uHTInfoField.infoField3.lsigTXOPProtectionFullSupport;
    pDot11f->pcoActive                     = uHTInfoField.infoField3.pcoActive;
    pDot11f->pcoPhase                      = uHTInfoField.infoField3.pcoPhase;
    pDot11f->reserved2                     = uHTInfoField.infoField3.reserved;
    CFG_GET_STR( nSirStatus, pMac, WNI_CFG_BASIC_MCS_SET,
                 pDot11f->basicMCSSet, nCfgLen,
                 SIZE_OF_BASIC_MCS_SET );

    pDot11f->present = 1;

    return eSIR_SUCCESS;

} // End PopulateDot11fHTInfo.

void
PopulateDot11fIBSSParams(tpAniSirGlobal       pMac,
       tDot11fIEIBSSParams *pDot11f, tpPESession psessionEntry)
{
    if ( eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole )
    {
        pDot11f->present = 1;
        // ATIM duration is always set to 0
        pDot11f->atim = 0;
    }

} // End PopulateDot11fIBSSParams.


#ifdef ANI_SUPPORT_11H
tSirRetStatus
PopulateDot11fMeasurementReport0(tpAniSirGlobal              pMac,
                                 tpSirMacMeasReqActionFrame  pReq,
                                 tDot11fIEMeasurementReport *pDot11f)
{
    pDot11f->token     = pReq->measReqIE.measToken;
    pDot11f->late      = 0;
    pDot11f->incapable = 0;
    pDot11f->refused   = 1;
    pDot11f->type      = SIR_MAC_BASIC_MEASUREMENT_TYPE;

    pDot11f->present   = 1;

    return eSIR_SUCCESS;

} // End PopulatedDot11fMeasurementReport0.

tSirRetStatus
PopulateDot11fMeasurementReport1(tpAniSirGlobal              pMac,
                                  tpSirMacMeasReqActionFrame  pReq,
                                  tDot11fIEMeasurementReport *pDot11f)
{
    pDot11f->token     = pReq->measReqIE.measToken;
    pDot11f->late      = 0;
    pDot11f->incapable = 0;
    pDot11f->refused   = 1;
    pDot11f->type      = SIR_MAC_CCA_MEASUREMENT_TYPE;

    pDot11f->present   = 1;

    return eSIR_SUCCESS;

} // End PopulatedDot11fMeasurementReport1.

tSirRetStatus
PopulateDot11fMeasurementReport2(tpAniSirGlobal              pMac,
                                 tpSirMacMeasReqActionFrame  pReq,
                                 tDot11fIEMeasurementReport *pDot11f)
{
    pDot11f->token     = pReq->measReqIE.measToken;
    pDot11f->late      = 0;
    pDot11f->incapable = 0;
    pDot11f->refused   = 1;
    pDot11f->type      = SIR_MAC_RPI_MEASUREMENT_TYPE;

    pDot11f->present   = 1;

    return eSIR_SUCCESS;

} // End PopulatedDot11fMeasurementReport2.
#endif

void
PopulateDot11fPowerCaps(tpAniSirGlobal      pMac,
                        tDot11fIEPowerCaps *pCaps,
                        tANI_U8 nAssocType,
                        tpPESession psessionEntry)
{
    if (nAssocType == LIM_REASSOC)
    {
        pCaps->minTxPower = psessionEntry->pLimReAssocReq->powerCap.minTxPower;
        pCaps->maxTxPower = psessionEntry->pLimReAssocReq->powerCap.maxTxPower;
    }else
    {
        pCaps->minTxPower = psessionEntry->pLimJoinReq->powerCap.minTxPower;
        pCaps->maxTxPower = psessionEntry->pLimJoinReq->powerCap.maxTxPower;

    }
    
    pCaps->present    = 1;
} // End PopulateDot11fPowerCaps.

tSirRetStatus
PopulateDot11fPowerConstraints(tpAniSirGlobal             pMac,
                               tDot11fIEPowerConstraints *pDot11f)
{
    tANI_U32           cfg;
    tSirRetStatus nSirStatus;

    CFG_GET_INT( nSirStatus, pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, cfg );

    pDot11f->localPowerConstraints = ( tANI_U8 )cfg;
    pDot11f->present = 1;

    return eSIR_SUCCESS;
} // End PopulateDot11fPowerConstraints.

void
PopulateDot11fQOSCapsAp(tpAniSirGlobal      pMac,
                        tDot11fIEQOSCapsAp *pDot11f, tpPESession psessionEntry)
{
    pDot11f->count    = psessionEntry->gLimEdcaParamSetCount;
    pDot11f->reserved = 0;
    pDot11f->txopreq  = 0;
    pDot11f->qreq     = 0;
    pDot11f->qack     = 0;
    pDot11f->present  = 1;
} // End PopulatedDot11fQOSCaps.

void
PopulateDot11fQOSCapsStation(tpAniSirGlobal    pMac,
                      tDot11fIEQOSCapsStation *pDot11f)
{
    tANI_U32  val = 0;

    if(wlan_cfgGetInt(pMac, WNI_CFG_MAX_SP_LENGTH, &val) != eSIR_SUCCESS) 
        PELOGE(limLog(pMac, LOGE, FL("could not retrieve Max SP Length "));)
   
    pDot11f->more_data_ack = 0;
    pDot11f->max_sp_length = (tANI_U8)val;
    pDot11f->qack    = 0;

    if (pMac->lim.gUapsdEnable)
    {
        pDot11f->acbe_uapsd = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcBitmask);
        pDot11f->acbk_uapsd = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcBitmask);
        pDot11f->acvi_uapsd = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcBitmask);
        pDot11f->acvo_uapsd = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcBitmask);
    }   
    pDot11f->present = 1;
} // End PopulatedDot11fQOSCaps.

tSirRetStatus
PopulateDot11fRSN(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIERSN   *pDot11f)
{
    tANI_U32        status;
    int  idx;

    if ( pRsnIe->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_RSN ) ) )
            {
        status = dot11fUnpackIeRSN( pMac,
                                    pRsnIe->rsnIEdata + idx + 2, //EID, length
                                    pRsnIe->rsnIEdata[ idx + 1 ],
                                    pDot11f );
        if ( DOT11F_FAILED( status ) )
        {
            dot11fLog( pMac, LOGE, FL("Parse failure in PopulateDot11fRS"
                                   "N (0x%08x)."),
                    status );
            return eSIR_FAILURE;
        }
        dot11fLog( pMac, LOG2, FL("dot11fUnpackIeRSN returned 0x%08x in "
                               "PopulateDot11fRSN."), status );
        }

    }

    return eSIR_SUCCESS;
} // End PopulateDot11fRSN.

tSirRetStatus PopulateDot11fRSNOpaque( tpAniSirGlobal      pMac,
                                       tpSirRSNie          pRsnIe,
                                       tDot11fIERSNOpaque *pDot11f )
{
    int idx;

    if ( pRsnIe->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_RSN ) ) )
        {
            pDot11f->present  = 1;
            pDot11f->num_data = pRsnIe->rsnIEdata[ idx + 1 ];
            vos_mem_copy(  pDot11f->data,
                           pRsnIe->rsnIEdata + idx + 2,    // EID, len
                           pRsnIe->rsnIEdata[ idx + 1 ] );
        }
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fRSNOpaque.



#if defined(FEATURE_WLAN_WAPI)

tSirRetStatus
PopulateDot11fWAPI(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIEWAPI   *pDot11f)
        {
    tANI_U32        status;
    int  idx;

    if ( pRsnIe->length )
        {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_WAPI ) ) )
        {
            status = dot11fUnpackIeWAPI( pMac,
                                        pRsnIe->rsnIEdata + idx + 2, //EID, length
                                        pRsnIe->rsnIEdata[ idx + 1 ],
                                        pDot11f );
            if ( DOT11F_FAILED( status ) )
            {
                dot11fLog( pMac, LOGE, FL("Parse failure in PopulateDot11fWAPI (0x%08x)."),
                        status );
                return eSIR_FAILURE;
            }
            dot11fLog( pMac, LOG2, FL("dot11fUnpackIeRSN returned 0x%08x in "
                               "PopulateDot11fWAPI."), status );
        }
    }

    return eSIR_SUCCESS;
} // End PopulateDot11fWAPI.

tSirRetStatus PopulateDot11fWAPIOpaque( tpAniSirGlobal      pMac,
                                       tpSirRSNie          pRsnIe,
                                       tDot11fIEWAPIOpaque *pDot11f )
{
    int idx;

    if ( pRsnIe->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_WAPI ) ) )
        {
        pDot11f->present  = 1;
        pDot11f->num_data = pRsnIe->rsnIEdata[ idx + 1 ];
        vos_mem_copy ( pDot11f->data,
                       pRsnIe->rsnIEdata + idx + 2,    // EID, len
                       pRsnIe->rsnIEdata[ idx + 1 ] );
    }
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fWAPIOpaque.


#endif //defined(FEATURE_WLAN_WAPI)

void
PopulateDot11fSSID(tpAniSirGlobal pMac,
                   tSirMacSSid   *pInternal,
                   tDot11fIESSID *pDot11f)
{
    pDot11f->present = 1;
    pDot11f->num_ssid = pInternal->length;
    if ( pInternal->length )
    {
        vos_mem_copy( ( tANI_U8* )pDot11f->ssid, ( tANI_U8* )&pInternal->ssId,
                       pInternal->length );
    }
} // End PopulateDot11fSSID.

tSirRetStatus
PopulateDot11fSSID2(tpAniSirGlobal pMac,
                    tDot11fIESSID *pDot11f)
{
    tANI_U32           nCfg;
    tSirRetStatus nSirStatus;

    CFG_GET_STR( nSirStatus, pMac, WNI_CFG_SSID, pDot11f->ssid, nCfg, 32 );
    pDot11f->num_ssid = ( tANI_U8 )nCfg;
    pDot11f->present  = 1;
    return eSIR_SUCCESS;
} // End PopulateDot11fSSID2.

void
PopulateDot11fSchedule(tSirMacScheduleIE *pSchedule,
                       tDot11fIESchedule *pDot11f)
{
    pDot11f->aggregation        = pSchedule->info.aggregation;
    pDot11f->tsid               = pSchedule->info.tsid;
    pDot11f->direction          = pSchedule->info.direction;
    pDot11f->reserved           = pSchedule->info.rsvd;
    pDot11f->service_start_time = pSchedule->svcStartTime;
    pDot11f->service_interval   = pSchedule->svcInterval;
    pDot11f->max_service_dur    = pSchedule->maxSvcDuration;
    pDot11f->spec_interval      = pSchedule->specInterval;

    pDot11f->present = 1;
} // End PopulateDot11fSchedule.

void
PopulateDot11fSuppChannels(tpAniSirGlobal         pMac,
                           tDot11fIESuppChannels *pDot11f,
                           tANI_U8 nAssocType,
                           tpPESession psessionEntry)
{
    tANI_U8  i;
    tANI_U8 *p;

    if (nAssocType == LIM_REASSOC)
    {
        p = ( tANI_U8* )psessionEntry->pLimReAssocReq->supportedChannels.channelList;
        pDot11f->num_bands = psessionEntry->pLimReAssocReq->supportedChannels.numChnl;
    }else
    {
        p = ( tANI_U8* )psessionEntry->pLimJoinReq->supportedChannels.channelList;
        pDot11f->num_bands = psessionEntry->pLimJoinReq->supportedChannels.numChnl;
    }
    for ( i = 0U; i < pDot11f->num_bands; ++i, ++p)
    {
        pDot11f->bands[i][0] = *p;
        pDot11f->bands[i][1] = 1;
    }

    pDot11f->present = 1;

} // End PopulateDot11fSuppChannels.

tSirRetStatus
PopulateDot11fSuppRates(tpAniSirGlobal      pMac,
                        tANI_U8                  nChannelNum,
                        tDot11fIESuppRates *pDot11f,tpPESession psessionEntry)
{
    tSirRetStatus nSirStatus;
    tANI_U32           nRates;
    tANI_U8            rates[SIR_MAC_MAX_NUMBER_OF_RATES];

   /* Use the operational rates present in session entry whenever nChannelNum is set to OPERATIONAL
       else use the supported rate set from CFG, which is fixed and does not change dynamically and is used for
       sending mgmt frames (lile probe req) which need to go out before any session is present.
   */
    if(POPULATE_DOT11F_RATES_OPERATIONAL == nChannelNum )
    {
        #if 0
        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                     rates, nRates, SIR_MAC_MAX_NUMBER_OF_RATES );
        #endif //TO SUPPORT BT-AMP
        if(psessionEntry != NULL)
        {
            nRates = psessionEntry->rateSet.numRates;
            vos_mem_copy( rates, psessionEntry->rateSet.rate,
                          nRates);
        }
        else
        {
            dot11fLog( pMac, LOGE, FL("no session context exists while populating Operational Rate Set"));
            nRates = 0;
        }
    }
    else if ( 14 >= nChannelNum )
    {
        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_SUPPORTED_RATES_11B,
                     rates, nRates, SIR_MAC_MAX_NUMBER_OF_RATES );
    }
    else
    {
        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_SUPPORTED_RATES_11A,
                     rates, nRates, SIR_MAC_MAX_NUMBER_OF_RATES );
    }

    if ( 0 != nRates )
    {
        pDot11f->num_rates = ( tANI_U8 )nRates;
        vos_mem_copy( pDot11f->rates, rates, nRates );
        pDot11f->present   = 1;
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fSuppRates.

/**
 * PopulateDot11fRatesTdls() - populate supported rates and
 *                                extended supported rates IE.
 * @p_mac gloabl - header.
 * @p_supp_rates - pointer to supported rates IE
 * @p_ext_supp_rates - pointer to extended supported rates IE
 *
 * This function populates the supported rates and extended supported
 * rates IE based in the STA capability. If the number of rates
 * supported is less than MAX_NUM_SUPPORTED_RATES, only supported rates
 * IE is populated.
 *
 * Return: tSirRetStatus eSIR_SUCCESS on Success and eSIR_FAILURE
 *         on failure.
 */

tSirRetStatus
PopulateDot11fRatesTdls(tpAniSirGlobal p_mac,
                           tDot11fIESuppRates *p_supp_rates,
                           tDot11fIEExtSuppRates *p_ext_supp_rates)
{
    tSirMacRateSet temp_rateset;
    tSirMacRateSet temp_rateset2;
    uint32_t val, i;
    uint32_t self_dot11mode = 0;

    wlan_cfgGetInt(p_mac, WNI_CFG_DOT11_MODE, &self_dot11mode);

    /**
     * Include 11b rates only when the device configured in
     * auto, 11a/b/g or 11b_only
     */
    if ((self_dot11mode == WNI_CFG_DOT11_MODE_ALL) ||
        (self_dot11mode == WNI_CFG_DOT11_MODE_11A) ||
        (self_dot11mode == WNI_CFG_DOT11_MODE_11AC) ||
        (self_dot11mode == WNI_CFG_DOT11_MODE_11N) ||
        (self_dot11mode == WNI_CFG_DOT11_MODE_11G) ||
        (self_dot11mode == WNI_CFG_DOT11_MODE_11B) )
    {
            val = WNI_CFG_SUPPORTED_RATES_11B_LEN;
            wlan_cfgGetStr(p_mac, WNI_CFG_SUPPORTED_RATES_11B,
                            (tANI_U8 *)&temp_rateset.rate, &val);
            temp_rateset.numRates = (tANI_U8) val;
    }
    else
    {
            temp_rateset.numRates = 0;
    }

    /* Include 11a rates when the device configured in non-11b mode */
    if (!IS_DOT11_MODE_11B(self_dot11mode))
    {
            val = WNI_CFG_SUPPORTED_RATES_11A_LEN;
            wlan_cfgGetStr(p_mac, WNI_CFG_SUPPORTED_RATES_11A,
                    (tANI_U8 *)&temp_rateset2.rate, &val);
            temp_rateset2.numRates = (tANI_U8) val;
    }
    else
    {
            temp_rateset2.numRates = 0;
    }

    if ((temp_rateset.numRates + temp_rateset2.numRates) >
                                     SIR_MAC_MAX_NUMBER_OF_RATES)
    {
            limLog(p_mac, LOGP, FL("more than %d rates in CFG"),
                                SIR_MAC_MAX_NUMBER_OF_RATES);
            return eSIR_FAILURE;
    }

    /**
     * copy all rates in temp_rateset,
     * there are SIR_MAC_MAX_NUMBER_OF_RATES rates max
     */
    for (i = 0; i < temp_rateset2.numRates; i++)
            temp_rateset.rate[i + temp_rateset.numRates] =
                                          temp_rateset2.rate[i];

    temp_rateset.numRates += temp_rateset2.numRates;

    if (temp_rateset.numRates <= MAX_NUM_SUPPORTED_RATES)
    {
            p_supp_rates->num_rates = temp_rateset.numRates;
            vos_mem_copy(p_supp_rates->rates, temp_rateset.rate,
                         p_supp_rates->num_rates);
            p_supp_rates->present = 1;
    }
    else  /* Populate extended capability as well */
    {
            p_supp_rates->num_rates = MAX_NUM_SUPPORTED_RATES;
            vos_mem_copy(p_supp_rates->rates, temp_rateset.rate,
                         p_supp_rates->num_rates);
            p_supp_rates->present = 1;
            p_ext_supp_rates->num_rates = temp_rateset.numRates -
                                 MAX_NUM_SUPPORTED_RATES;
            vos_mem_copy(p_ext_supp_rates->rates,
                         (tANI_U8 *)temp_rateset.rate +
                         MAX_NUM_SUPPORTED_RATES,
                         p_ext_supp_rates->num_rates);
            p_ext_supp_rates->present = 1;
    }

    return eSIR_SUCCESS;

} /* End PopulateDot11fRatesTdls */

tSirRetStatus
PopulateDot11fTPCReport(tpAniSirGlobal      pMac,
                        tDot11fIETPCReport *pDot11f,
                        tpPESession psessionEntry)
{
    tANI_U16 staid, txPower;
    tSirRetStatus nSirStatus;

    nSirStatus = limGetMgmtStaid( pMac, &staid, psessionEntry);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        dot11fLog( pMac, LOG1, FL("Failed to get the STAID in Populat"
                                  "eDot11fTPCReport; limGetMgmtStaid "
                                  "returned status %d."),
                   nSirStatus );
        return eSIR_FAILURE;
    }

    // FramesToDo: This function was "misplaced" in the move to Gen4_TVM...
    // txPower = halGetRateToPwrValue( pMac, staid, pMac->lim.gLimCurrentChannelId, isBeacon );
    txPower = 0;
    pDot11f->tx_power    = ( tANI_U8 )txPower;
    pDot11f->link_margin = 0;
    pDot11f->present     = 1;

    return eSIR_SUCCESS;
} // End PopulateDot11fTPCReport.


void PopulateDot11fTSInfo(tSirMacTSInfo   *pInfo,
                          tDot11fFfTSInfo *pDot11f)
{
    pDot11f->traffic_type   = pInfo->traffic.trafficType;
    pDot11f->tsid           = pInfo->traffic.tsid;
    pDot11f->direction      = pInfo->traffic.direction;
    pDot11f->access_policy  = pInfo->traffic.accessPolicy;
    pDot11f->aggregation    = pInfo->traffic.aggregation;
    pDot11f->psb            = pInfo->traffic.psb;
    pDot11f->user_priority  = pInfo->traffic.userPrio;
    pDot11f->tsinfo_ack_pol = pInfo->traffic.ackPolicy;
    pDot11f->schedule       = pInfo->schedule.schedule;
} // End PopulatedDot11fTSInfo.

void PopulateDot11fWMM(tpAniSirGlobal      pMac,
                       tDot11fIEWMMInfoAp  *pInfo,
                       tDot11fIEWMMParams *pParams,
                       tDot11fIEWMMCaps   *pCaps,
                       tpPESession        psessionEntry)
{
    if ( psessionEntry->limWmeEnabled )
    {
        if ( eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole )
        {
            //if ( ! sirIsPropCapabilityEnabled( pMac, SIR_MAC_PROP_CAPABILITY_WME ) )
            {
                PopulateDot11fWMMInfoAp( pMac, pInfo, psessionEntry );
            }
        }
        else
        {
            {
                PopulateDot11fWMMParams( pMac, pParams, psessionEntry);
            }

           if ( psessionEntry->limWsmEnabled )
            {
                PopulateDot11fWMMCaps( pCaps );
            }
        }
    }
} // End PopulateDot11fWMM.

void PopulateDot11fWMMCaps(tDot11fIEWMMCaps *pCaps)
{
    pCaps->version       = SIR_MAC_OUI_VERSION_1;
    pCaps->qack          = 0;
    pCaps->queue_request = 1;
    pCaps->txop_request  = 0;
    pCaps->more_ack      = 0;
    pCaps->present       = 1;
} // End PopulateDot11fWmmCaps.

#ifdef FEATURE_WLAN_ESE
void PopulateDot11fReAssocTspec(tpAniSirGlobal pMac, tDot11fReAssocRequest *pReassoc, tpPESession psessionEntry)
{
    tANI_U8 numTspecs = 0, idx;
    tTspecInfo *pTspec = NULL;

    numTspecs = psessionEntry->pLimReAssocReq->eseTspecInfo.numTspecs;
    pTspec = &psessionEntry->pLimReAssocReq->eseTspecInfo.tspec[0];
    pReassoc->num_WMMTSPEC = numTspecs;
    if (numTspecs) {
        for (idx=0; idx<numTspecs; idx++) {
            PopulateDot11fWMMTSPEC(&pTspec->tspec, &pReassoc->WMMTSPEC[idx]);
            pTspec->tspec.mediumTime = 0;
            pTspec++;
        }
    }
}
#endif 

void PopulateDot11fWMMInfoAp(tpAniSirGlobal pMac, tDot11fIEWMMInfoAp *pInfo, 
                             tpPESession psessionEntry)
{
    pInfo->version = SIR_MAC_OUI_VERSION_1;

    /* WMM Specification 3.1.3, 3.2.3
     * An IBSS staion shall always use its default WMM parameters.
     */
    if ( eLIM_STA_IN_IBSS_ROLE == psessionEntry->limSystemRole )
    {
        pInfo->param_set_count = 0;
        pInfo->uapsd = 0;
    }
    else
    {
        pInfo->param_set_count = ( 0xf & psessionEntry->gLimEdcaParamSetCount );
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE ){
            pInfo->uapsd = ( 0x1 & psessionEntry->apUapsdEnable );
        }
        else
            pInfo->uapsd = ( 0x1 & pMac->lim.gUapsdEnable );
    }
    pInfo->present = 1;
}

void PopulateDot11fWMMInfoStation(tpAniSirGlobal pMac, tDot11fIEWMMInfoStation *pInfo)
{
    tANI_U32  val = 0;

    limLog(pMac, LOG1, FL("populate WMM IE in Setup Request Frame"));
    pInfo->version = SIR_MAC_OUI_VERSION_1;
    pInfo->acvo_uapsd = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcBitmask);
    pInfo->acvi_uapsd = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcBitmask);
    pInfo->acbk_uapsd = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcBitmask);
    pInfo->acbe_uapsd = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcBitmask);

    if(wlan_cfgGetInt(pMac, WNI_CFG_MAX_SP_LENGTH, &val) != eSIR_SUCCESS) 
        PELOGE(limLog(pMac, LOGE, FL("could not retrieve Max SP Length "));)
    pInfo->max_sp_length = (tANI_U8)val;
    pInfo->present = 1;
}

void PopulateDot11fWMMParams(tpAniSirGlobal      pMac,
                             tDot11fIEWMMParams *pParams,
                             tpPESession        psessionEntry)
{
    pParams->version = SIR_MAC_OUI_VERSION_1;

    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
       pParams->qosInfo =
           (psessionEntry->apUapsdEnable << 7) | ((tANI_U8)(0x0f & psessionEntry->gLimEdcaParamSetCount));
    else 
       pParams->qosInfo =
           (pMac->lim.gUapsdEnable << 7) | ((tANI_U8)(0x0f & psessionEntry->gLimEdcaParamSetCount));

    // Fill each EDCA parameter set in order: be, bk, vi, vo
    pParams->acbe_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[0].aci.aifsn) );
    pParams->acbe_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[0].aci.acm );
    pParams->acbe_aci       = ( 0x3 & SIR_MAC_EDCAACI_BESTEFFORT );
    pParams->acbe_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[0].cw.min );
    pParams->acbe_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[0].cw.max );
    pParams->acbe_txoplimit = psessionEntry->gLimEdcaParamsBC[0].txoplimit;

    pParams->acbk_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[1].aci.aifsn) );
    pParams->acbk_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[1].aci.acm );
    pParams->acbk_aci       = ( 0x3 & SIR_MAC_EDCAACI_BACKGROUND );
    pParams->acbk_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[1].cw.min );
    pParams->acbk_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[1].cw.max );
    pParams->acbk_txoplimit = psessionEntry->gLimEdcaParamsBC[1].txoplimit;

    if(psessionEntry->limSystemRole == eLIM_AP_ROLE )
        pParams->acvi_aifsn     = ( 0xf & psessionEntry->gLimEdcaParamsBC[2].aci.aifsn );
    else
        pParams->acvi_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[2].aci.aifsn) );



    pParams->acvi_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[2].aci.acm );
    pParams->acvi_aci       = ( 0x3 & SIR_MAC_EDCAACI_VIDEO );
    pParams->acvi_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[2].cw.min );
    pParams->acvi_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[2].cw.max );
    pParams->acvi_txoplimit = psessionEntry->gLimEdcaParamsBC[2].txoplimit;

    if(psessionEntry->limSystemRole == eLIM_AP_ROLE )
        pParams->acvo_aifsn     = ( 0xf & psessionEntry->gLimEdcaParamsBC[3].aci.aifsn );
    else
        pParams->acvo_aifsn     = ( 0xf & SET_AIFSN(psessionEntry->gLimEdcaParamsBC[3].aci.aifsn) );

    pParams->acvo_acm       = ( 0x1 & psessionEntry->gLimEdcaParamsBC[3].aci.acm );
    pParams->acvo_aci       = ( 0x3 & SIR_MAC_EDCAACI_VOICE );
    pParams->acvo_acwmin    = ( 0xf & psessionEntry->gLimEdcaParamsBC[3].cw.min );
    pParams->acvo_acwmax    = ( 0xf & psessionEntry->gLimEdcaParamsBC[3].cw.max );
    pParams->acvo_txoplimit = psessionEntry->gLimEdcaParamsBC[3].txoplimit;

    pParams->present = 1;

} // End PopulateDot11fWMMParams.

void PopulateDot11fWMMSchedule(tSirMacScheduleIE    *pSchedule,
                               tDot11fIEWMMSchedule *pDot11f)
{
    pDot11f->version            = 1;
    pDot11f->aggregation        = pSchedule->info.aggregation;
    pDot11f->tsid               = pSchedule->info.tsid;
    pDot11f->direction          = pSchedule->info.direction;
    pDot11f->reserved           = pSchedule->info.rsvd;
    pDot11f->service_start_time = pSchedule->svcStartTime;
    pDot11f->service_interval   = pSchedule->svcInterval;
    pDot11f->max_service_dur    = pSchedule->maxSvcDuration;
    pDot11f->spec_interval      = pSchedule->specInterval;

    pDot11f->present = 1;
} // End PopulateDot11fWMMSchedule.

tSirRetStatus
PopulateDot11fWPA(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIEWPA   *pDot11f)
{
    tANI_U32        status;
    int idx;

    if ( pRsnIe->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_WPA ) ) )
        {
        status = dot11fUnpackIeWPA( pMac,
                                    pRsnIe->rsnIEdata + idx + 2 + 4,  // EID, length, OUI
                                    pRsnIe->rsnIEdata[ idx + 1 ] - 4, // OUI
                                    pDot11f );
        if ( DOT11F_FAILED( status ) )
        {
            dot11fLog( pMac, LOGE, FL("Parse failure in PopulateDot11fWP"
                                   "A (0x%08x)."),
                    status );
            return eSIR_FAILURE;
        }
    }
    }

    return eSIR_SUCCESS;
} // End PopulateDot11fWPA.



tSirRetStatus PopulateDot11fWPAOpaque( tpAniSirGlobal      pMac,
                                       tpSirRSNie          pRsnIe,
                                       tDot11fIEWPAOpaque *pDot11f )
{
    int idx;

    if ( pRsnIe->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, pRsnIe, DOT11F_EID_WPA ) ) )
        {
        pDot11f->present  = 1;
        pDot11f->num_data = pRsnIe->rsnIEdata[ idx + 1 ] - 4;
        vos_mem_copy(  pDot11f->data,
                       pRsnIe->rsnIEdata + idx + 2 + 4,    // EID, len, OUI
                       pRsnIe->rsnIEdata[ idx + 1 ] - 4 ); // OUI
    }
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fWPAOpaque.

////////////////////////////////////////////////////////////////////////

tSirRetStatus
sirGetCfgPropCaps(tpAniSirGlobal pMac, tANI_U16 *caps)
{
#if 0
    tANI_U32 val;

    *caps = 0;
    if (wlan_cfgGetInt(pMac, WNI_CFG_PROPRIETARY_ANI_FEATURES_ENABLED, &val)
        != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not retrieve PropFeature enabled flag"));
        return eSIR_FAILURE;
    }
    if (wlan_cfgGetInt(pMac, WNI_CFG_PROP_CAPABILITY, &val) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not retrieve PROP_CAPABLITY flag"));
        return eSIR_FAILURE;
    }

    *caps = (tANI_U16) val;
#endif
    return eSIR_SUCCESS;
}

tSirRetStatus
sirConvertProbeReqFrame2Struct(tpAniSirGlobal  pMac,
                               tANI_U8             *pFrame,
                               tANI_U32             nFrame,
                               tpSirProbeReq   pProbeReq)
{
    tANI_U32 status;
    tDot11fProbeRequest pr;

    // Ok, zero-init our [out] parameter,
    vos_mem_set( (tANI_U8*)pProbeReq, sizeof(tSirProbeReq), 0);

    // delegate to the framesc-generated code,
    status = dot11fUnpackProbeRequest(pMac, pFrame, nFrame, &pr);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse a Probe Request (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking a Probe Request (0x%08x, %d bytes)"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fProbeRequestto' a 'tSirProbeReq'...
    if ( ! pr.SSID.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE SSID not present!"));)
    }
    else
    {
        pProbeReq->ssidPresent = 1;
        ConvertSSID( pMac, &pProbeReq->ssId, &pr.SSID );
    }

    if ( ! pr.SuppRates.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
        return eSIR_FAILURE;
    }
    else
    {
        pProbeReq->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pProbeReq->supportedRates, &pr.SuppRates );
    }

    if ( pr.ExtSuppRates.present )
    {
        pProbeReq->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pProbeReq->extendedRates, &pr.ExtSuppRates );
    }

    if ( pr.HTCaps.present )
    {
        vos_mem_copy( &pProbeReq->HTCaps, &pr.HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( pr.WscProbeReq.present )
    {
        pProbeReq->wscIePresent = 1;
        vos_mem_copy(&pProbeReq->probeReqWscIeInfo, &pr.WscProbeReq, sizeof(tDot11fIEWscProbeReq));
    }
#ifdef WLAN_FEATURE_11AC
    if ( pr.VHTCaps.present )
    {
        vos_mem_copy( &pProbeReq->VHTCaps, &pr.VHTCaps, sizeof( tDot11fIEVHTCaps ) );
    }
#endif


    if ( pr.P2PProbeReq.present )
    {
        pProbeReq->p2pIePresent = 1;
    }

    return eSIR_SUCCESS;

} // End sirConvertProbeReqFrame2Struct.

/* function ValidateAndRectifyIEs checks for the malformed frame.
 * The frame would contain fixed IEs of 12 bytes follwed by Variable IEs
 * (Tagged elements).
 * Every Tagged IE has tag number, tag length and data. Tag length indicates
 * the size of data in bytes.
 * This function checks for size of Frame recived with the sum of all IEs.
 * And also rectifies missing optional fields in IE.
 *
 * NOTE : Presently this function rectifies RSN capability in RSN IE, can
 * extended to rectify other optional fields in other IEs.
 *
 */
tSirRetStatus ValidateAndRectifyIEs(tpAniSirGlobal pMac,
                                    tANI_U8 *pMgmtFrame,
                                    tANI_U32 nFrameBytes,
                                    tANI_U32 *nMissingRsnBytes)
{
    tANI_U32 length = SIZE_OF_FIXED_PARAM;
    tANI_U8 *refFrame;

    // Frame contains atleast one IE
    if (nFrameBytes > (SIZE_OF_FIXED_PARAM + 2))
    {
        while (length < nFrameBytes)
        {
            /*refFrame points to next IE */
            refFrame = pMgmtFrame + length;
            length += (tANI_U32)(SIZE_OF_TAG_PARAM_NUM + SIZE_OF_TAG_PARAM_LEN
                                 + (*(refFrame + SIZE_OF_TAG_PARAM_NUM)));
        }
        if (length != nFrameBytes)
        {
            /* Workaround : Some APs may not include RSN Capability but
             * the length of which is included in RSN IE length.
             * this may cause in updating RSN Capability with junk value.
             * To avoid this, add RSN Capability value with default value.
             * Going further we can have such workaround for other IEs
             */
            if ((*refFrame == RSNIEID) &&
                (length == (nFrameBytes + RSNIE_CAPABILITY_LEN)))
            {
                //Assume RSN Capability as 00
                vos_mem_set( ( tANI_U8* ) (pMgmtFrame + (nFrameBytes)),
                             RSNIE_CAPABILITY_LEN, DEFAULT_RSNIE_CAP_VAL );
                *nMissingRsnBytes = RSNIE_CAPABILITY_LEN;
                limLog(pMac, LOG1,
                       FL("Added RSN Capability to the RSNIE as 0x00 0x00"));

                return eHAL_STATUS_SUCCESS;
            }
            return eSIR_FAILURE;
        }
    }
    return eHAL_STATUS_SUCCESS;
}

tSirRetStatus sirConvertProbeFrame2Struct(tpAniSirGlobal       pMac,
                                          tANI_U8             *pFrame,
                                          tANI_U32             nFrame,
                                          tpSirProbeRespBeacon pProbeResp)
{
    tANI_U32             status;
    tDot11fProbeResponse *pr;

    // Ok, zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pProbeResp, sizeof(tSirProbeRespBeacon), 0 );

    pr = vos_mem_vmalloc(sizeof(tDot11fProbeResponse));
    if ( NULL == pr )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (!HAL_STATUS_SUCCESS(status))
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }

    vos_mem_set( ( tANI_U8* )pr, sizeof(tDot11fProbeResponse), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackProbeResponse( pMac, pFrame, nFrame, pr );
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse a Probe Response (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        vos_mem_vfree(pr);
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking a Probe Response (0x%08x, %d bytes)"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fProbeResponse' to a 'tSirProbeRespBeacon'...

    // Timestamp
    vos_mem_copy( ( tANI_U8* )pProbeResp->timeStamp, ( tANI_U8* )&pr->TimeStamp,
                  sizeof(tSirMacTimeStamp) );

    // Beacon Interval
    pProbeResp->beaconInterval = pr->BeaconInterval.interval;

    // Capabilities
    pProbeResp->capabilityInfo.ess            = pr->Capabilities.ess;
    pProbeResp->capabilityInfo.ibss           = pr->Capabilities.ibss;
    pProbeResp->capabilityInfo.cfPollable     = pr->Capabilities.cfPollable;
    pProbeResp->capabilityInfo.cfPollReq      = pr->Capabilities.cfPollReq;
    pProbeResp->capabilityInfo.privacy        = pr->Capabilities.privacy;
    pProbeResp->capabilityInfo.shortPreamble  = pr->Capabilities.shortPreamble;
    pProbeResp->capabilityInfo.pbcc           = pr->Capabilities.pbcc;
    pProbeResp->capabilityInfo.channelAgility = pr->Capabilities.channelAgility;
    pProbeResp->capabilityInfo.spectrumMgt    = pr->Capabilities.spectrumMgt;
    pProbeResp->capabilityInfo.qos            = pr->Capabilities.qos;
    pProbeResp->capabilityInfo.shortSlotTime  = pr->Capabilities.shortSlotTime;
    pProbeResp->capabilityInfo.apsd           = pr->Capabilities.apsd;
    pProbeResp->capabilityInfo.rrm            = pr->Capabilities.rrm;
    pProbeResp->capabilityInfo.dsssOfdm       = pr->Capabilities.dsssOfdm;
    pProbeResp->capabilityInfo.delayedBA       = pr->Capabilities.delayedBA;
    pProbeResp->capabilityInfo.immediateBA    = pr->Capabilities.immediateBA;

    if ( ! pr->SSID.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE SSID not present!"));)
    }
    else
    {
        pProbeResp->ssidPresent = 1;
        ConvertSSID( pMac, &pProbeResp->ssId, &pr->SSID );
    }

    if ( ! pr->SuppRates.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
    }
    else
    {
        pProbeResp->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pProbeResp->supportedRates, &pr->SuppRates );
    }

    if ( pr->ExtSuppRates.present )
    {
        pProbeResp->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pProbeResp->extendedRates, &pr->ExtSuppRates );
    }


    if ( pr->CFParams.present )
    {
        pProbeResp->cfPresent = 1;
        ConvertCFParams( pMac, &pProbeResp->cfParamSet, &pr->CFParams );
    }

    if ( pr->Country.present )
    {
        pProbeResp->countryInfoPresent = 1;
        ConvertCountry( pMac, &pProbeResp->countryInfoParam, &pr->Country );
    }

    if ( pr->EDCAParamSet.present )
    {
        pProbeResp->edcaPresent = 1;
        ConvertEDCAParam( pMac, &pProbeResp->edcaParams, &pr->EDCAParamSet );
    }

    if ( pr->ChanSwitchAnn.present )
    {
        pProbeResp->channelSwitchPresent = 1;
        vos_mem_copy( &pProbeResp->channelSwitchIE, &pr->ChanSwitchAnn,
                       sizeof(tDot11fIEExtChanSwitchAnn) );
    }

       if ( pr->ExtChanSwitchAnn.present )
    {
        pProbeResp->extChannelSwitchPresent = 1;
        vos_mem_copy ( &pProbeResp->extChannelSwitchIE, &pr->ExtChanSwitchAnn,
                       sizeof(tDot11fIEExtChanSwitchAnn) );
    }

    if( pr->TPCReport.present)
    {
        pProbeResp->tpcReportPresent = 1;
        vos_mem_copy( &pProbeResp->tpcReport, &pr->TPCReport, sizeof(tDot11fIETPCReport));
    }

    if( pr->PowerConstraints.present)
    {
        pProbeResp->powerConstraintPresent = 1;
        vos_mem_copy( &pProbeResp->localPowerConstraint, &pr->PowerConstraints,
                      sizeof(tDot11fIEPowerConstraints));
    }

    if ( pr->Quiet.present )
    {
        pProbeResp->quietIEPresent = 1;
        vos_mem_copy( &pProbeResp->quietIE, &pr->Quiet, sizeof(tDot11fIEQuiet) );
    }

    if ( pr->HTCaps.present )
    {
        vos_mem_copy( &pProbeResp->HTCaps, &pr->HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( pr->HTInfo.present )
    {
        vos_mem_copy( &pProbeResp->HTInfo, &pr->HTInfo, sizeof( tDot11fIEHTInfo ) );
    }

    if ( pr->DSParams.present )
    {
        pProbeResp->dsParamsPresent = 1;
        pProbeResp->channelNumber = pr->DSParams.curr_channel;
    }
    else if(pr->HTInfo.present)
    {
        pProbeResp->channelNumber = pr->HTInfo.primaryChannel;
    }

    if ( pr->RSNOpaque.present )
    {
        pProbeResp->rsnPresent = 1;
        ConvertRSNOpaque( pMac, &pProbeResp->rsn, &pr->RSNOpaque );
    }

    if ( pr->WPA.present )
    {
        pProbeResp->wpaPresent = 1;
        ConvertWPA( pMac, &pProbeResp->wpa, &pr->WPA );
    }

    if ( pr->WMMParams.present )
    {
        pProbeResp->wmeEdcaPresent = 1;
        ConvertWMMParams( pMac, &pProbeResp->edcaParams, &pr->WMMParams );
        PELOG1(limLog(pMac, LOG1, FL("WMM Parameter present in Probe Response Frame!"));
                                __printWMMParams(pMac, &pr->WMMParams);)
    }

    if ( pr->WMMInfoAp.present )
    {
        pProbeResp->wmeInfoPresent = 1;
        PELOG1(limLog(pMac, LOG1, FL("WMM Information Element present in Probe Response Frame!"));)
    }

    if ( pr->WMMCaps.present )
    {
        pProbeResp->wsmCapablePresent = 1;
    }


    if ( pr->ERPInfo.present )
    {
        pProbeResp->erpPresent = 1;
        ConvertERPInfo( pMac, &pProbeResp->erpIEInfo, &pr->ERPInfo );
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pr->MobilityDomain.present)
    {
        // MobilityDomain
        pProbeResp->mdiePresent = 1;
        vos_mem_copy( (tANI_U8 *)&(pProbeResp->mdie[0]), (tANI_U8 *)&(pr->MobilityDomain.MDID),
                       sizeof(tANI_U16) );
        pProbeResp->mdie[2] = ((pr->MobilityDomain.overDSCap << 0) | (pr->MobilityDomain.resourceReqCap << 1));
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        limLog(pMac, LOG2, FL("mdie=%02x%02x%02x"), (unsigned int)pProbeResp->mdie[0],
               (unsigned int)pProbeResp->mdie[1], (unsigned int)pProbeResp->mdie[2]);
#endif
    }
#endif

#if defined FEATURE_WLAN_ESE
    if (pr->QBSSLoad.present)
    {
        vos_mem_copy(&pProbeResp->QBSSLoad, &pr->QBSSLoad, sizeof(tDot11fIEQBSSLoad));
    }
#endif
    if (pr->P2PProbeRes.present)
    {
       vos_mem_copy( &pProbeResp->P2PProbeRes, &pr->P2PProbeRes,
                                                sizeof(tDot11fIEP2PProbeRes) );
    }
#ifdef WLAN_FEATURE_11AC
    if ( pr->VHTCaps.present )
    {
       vos_mem_copy( &pProbeResp->VHTCaps, &pr->VHTCaps, sizeof( tDot11fIEVHTCaps ) );
    }
    if ( pr->VHTOperation.present )
    {
        vos_mem_copy( &pProbeResp->VHTOperation, &pr->VHTOperation, sizeof( tDot11fIEVHTOperation) );
    }
    if ( pr->VHTExtBssLoad.present )
    {
        vos_mem_copy( &pProbeResp->VHTExtBssLoad, &pr->VHTExtBssLoad, sizeof( tDot11fIEVHTExtBssLoad) );
    }
#endif

    vos_mem_vfree(pr);
    return eSIR_SUCCESS;

} // End sirConvertProbeFrame2Struct.

tSirRetStatus
sirConvertAssocReqFrame2Struct(tpAniSirGlobal pMac,
                               tANI_U8            *pFrame,
                               tANI_U32            nFrame,
                               tpSirAssocReq  pAssocReq)
{
    tDot11fAssocRequest *ar;
    tANI_U32                 status;

    ar = vos_mem_malloc(sizeof(tDot11fAssocRequest));
    if ( NULL == ar )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (!HAL_STATUS_SUCCESS(status))
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }
        // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAssocReq, sizeof(tSirAssocReq), 0 );
    vos_mem_set( ( tANI_U8* )ar, sizeof( tDot11fAssocRequest ), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackAssocRequest( pMac, pFrame, nFrame, ar );
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Association Request (0x%08x, %d bytes):"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        vos_mem_free(ar);
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking an Assoication Request (0x%08x, %d bytes):"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fAssocRequest' to a 'tSirAssocReq'...

    // make sure this is seen as an assoc request
    pAssocReq->reassocRequest = 0;

    // Capabilities
    pAssocReq->capabilityInfo.ess            = ar->Capabilities.ess;
    pAssocReq->capabilityInfo.ibss           = ar->Capabilities.ibss;
    pAssocReq->capabilityInfo.cfPollable     = ar->Capabilities.cfPollable;
    pAssocReq->capabilityInfo.cfPollReq      = ar->Capabilities.cfPollReq;
    pAssocReq->capabilityInfo.privacy        = ar->Capabilities.privacy;
    pAssocReq->capabilityInfo.shortPreamble  = ar->Capabilities.shortPreamble;
    pAssocReq->capabilityInfo.pbcc           = ar->Capabilities.pbcc;
    pAssocReq->capabilityInfo.channelAgility = ar->Capabilities.channelAgility;
    pAssocReq->capabilityInfo.spectrumMgt    = ar->Capabilities.spectrumMgt;
    pAssocReq->capabilityInfo.qos            = ar->Capabilities.qos;
    pAssocReq->capabilityInfo.shortSlotTime  = ar->Capabilities.shortSlotTime;
    pAssocReq->capabilityInfo.apsd           = ar->Capabilities.apsd;
    pAssocReq->capabilityInfo.rrm            = ar->Capabilities.rrm;
    pAssocReq->capabilityInfo.dsssOfdm       = ar->Capabilities.dsssOfdm;
    pAssocReq->capabilityInfo.delayedBA       = ar->Capabilities.delayedBA;
    pAssocReq->capabilityInfo.immediateBA    = ar->Capabilities.immediateBA;

    // Listen Interval
    pAssocReq->listenInterval = ar->ListenInterval.interval;

    // SSID
    if ( ar->SSID.present )
    {
        pAssocReq->ssidPresent = 1;
        ConvertSSID( pMac, &pAssocReq->ssId, &ar->SSID );
    }

    // Supported Rates
    if ( ar->SuppRates.present )
    {
        pAssocReq->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pAssocReq->supportedRates, &ar->SuppRates );
    }

    // Extended Supported Rates
    if ( ar->ExtSuppRates.present )
    {
        pAssocReq->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pAssocReq->extendedRates, &ar->ExtSuppRates );
    }

    // QOS Capabilities:
    if ( ar->QOSCapsStation.present )
    {
        pAssocReq->qosCapabilityPresent = 1;
        ConvertQOSCapsStation( pMac, &pAssocReq->qosCapability, &ar->QOSCapsStation );
    }

    // WPA
    if ( ar->WPAOpaque.present )
    {
        pAssocReq->wpaPresent = 1;
        ConvertWPAOpaque( pMac, &pAssocReq->wpa, &ar->WPAOpaque );
    }

    // RSN
    if ( ar->RSNOpaque.present )
    {
        pAssocReq->rsnPresent = 1;
        ConvertRSNOpaque( pMac, &pAssocReq->rsn, &ar->RSNOpaque );
    }

    // WSC IE
    if (ar->WscIEOpaque.present) 
    {
        pAssocReq->addIEPresent = 1;
        ConvertWscOpaque(pMac, &pAssocReq->addIE, &ar->WscIEOpaque);
    }
    

    if(ar->P2PIEOpaque.present)
    {
        pAssocReq->addIEPresent = 1;
        ConvertP2POpaque( pMac, &pAssocReq->addIE, &ar->P2PIEOpaque);
    }
#ifdef WLAN_FEATURE_WFD
    if(ar->WFDIEOpaque.present)
    {
        pAssocReq->addIEPresent = 1;
        ConvertWFDOpaque( pMac, &pAssocReq->addIE, &ar->WFDIEOpaque);
    }
#endif

    // Power Capabilities
    if ( ar->PowerCaps.present )
    {
        pAssocReq->powerCapabilityPresent     = 1;
        ConvertPowerCaps( pMac, &pAssocReq->powerCapability, &ar->PowerCaps );
    }

    // Supported Channels
    if ( ar->SuppChannels.present )
    {
        pAssocReq->supportedChannelsPresent = 1;
        ConvertSuppChannels( pMac, &pAssocReq->supportedChannels, &ar->SuppChannels );
    }

    if ( ar->HTCaps.present )
    {
        vos_mem_copy( &pAssocReq->HTCaps, &ar->HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( ar->WMMInfoStation.present )
    {
        pAssocReq->wmeInfoPresent = 1;
        vos_mem_copy( &pAssocReq->WMMInfoStation, &ar->WMMInfoStation,
                      sizeof( tDot11fIEWMMInfoStation ) );

    }


    if ( ar->WMMCaps.present ) pAssocReq->wsmCapablePresent = 1;

    if ( ! pAssocReq->ssidPresent )
    {
        PELOG2(limLog(pMac, LOG2, FL("Received Assoc without SSID IE."));)
        vos_mem_free(ar);
        return eSIR_FAILURE;
    }

    if ( !pAssocReq->suppRatesPresent && !pAssocReq->extendedRatesPresent )
    {
        PELOG2(limLog(pMac, LOG2, FL("Received Assoc without supp rate IE."));)
        vos_mem_free(ar);
        return eSIR_FAILURE;
    }

#ifdef WLAN_FEATURE_11AC
    if ( ar->VHTCaps.present )
    {
        vos_mem_copy( &pAssocReq->VHTCaps, &ar->VHTCaps, sizeof( tDot11fIEVHTCaps ) );
        limLog( pMac, LOGW, FL("Received Assoc Req with VHT Cap"));
        limLogVHTCap( pMac, &pAssocReq->VHTCaps);
    }
    if ( ar->OperatingMode.present )
    {
        vos_mem_copy( &pAssocReq->operMode, &ar->OperatingMode, sizeof (tDot11fIEOperatingMode));
        limLog( pMac, LOGW, FL("Received Assoc Req with Operating Mode IE"));
        limLogOperatingMode( pMac, &pAssocReq->operMode);
    }
#endif
    vos_mem_free(ar);
    return eSIR_SUCCESS;

} // End sirConvertAssocReqFrame2Struct.

tSirRetStatus
sirConvertAssocRespFrame2Struct(tpAniSirGlobal pMac,
                                tANI_U8            *pFrame,
                                tANI_U32            nFrame,
                                tpSirAssocRsp  pAssocRsp)
{
    static tDot11fAssocResponse ar;
    tANI_U32                  status;
    tANI_U8  cnt =0;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAssocRsp, sizeof(tSirAssocRsp), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackAssocResponse( pMac, pFrame, nFrame, &ar);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Association Response (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking an Association Response (0x%08x, %d bytes)"),
                   status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fAssocResponse' a 'tSirAssocRsp'...

    // Capabilities
    pAssocRsp->capabilityInfo.ess            = ar.Capabilities.ess;
    pAssocRsp->capabilityInfo.ibss           = ar.Capabilities.ibss;
    pAssocRsp->capabilityInfo.cfPollable     = ar.Capabilities.cfPollable;
    pAssocRsp->capabilityInfo.cfPollReq      = ar.Capabilities.cfPollReq;
    pAssocRsp->capabilityInfo.privacy        = ar.Capabilities.privacy;
    pAssocRsp->capabilityInfo.shortPreamble  = ar.Capabilities.shortPreamble;
    pAssocRsp->capabilityInfo.pbcc           = ar.Capabilities.pbcc;
    pAssocRsp->capabilityInfo.channelAgility = ar.Capabilities.channelAgility;
    pAssocRsp->capabilityInfo.spectrumMgt    = ar.Capabilities.spectrumMgt;
    pAssocRsp->capabilityInfo.qos            = ar.Capabilities.qos;
    pAssocRsp->capabilityInfo.shortSlotTime  = ar.Capabilities.shortSlotTime;
    pAssocRsp->capabilityInfo.apsd           = ar.Capabilities.apsd;
    pAssocRsp->capabilityInfo.rrm            = ar.Capabilities.rrm;
    pAssocRsp->capabilityInfo.dsssOfdm       = ar.Capabilities.dsssOfdm;
    pAssocRsp->capabilityInfo.delayedBA       = ar.Capabilities.delayedBA;
    pAssocRsp->capabilityInfo.immediateBA    = ar.Capabilities.immediateBA;

    pAssocRsp->statusCode = ar.Status.status;
    pAssocRsp->aid        = ar.AID.associd;

    if ( ! ar.SuppRates.present )
    {
        pAssocRsp->suppRatesPresent = 0;
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
    }
    else
    {
        pAssocRsp->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pAssocRsp->supportedRates, &ar.SuppRates );
    }

    if ( ar.ExtSuppRates.present )
    {
        pAssocRsp->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pAssocRsp->extendedRates, &ar.ExtSuppRates );
    }

    if ( ar.EDCAParamSet.present )
    {
        pAssocRsp->edcaPresent = 1;
        ConvertEDCAParam( pMac, &pAssocRsp->edca, &ar.EDCAParamSet );
    }
    if (ar.ExtCap.present)
    {
        vos_mem_copy(&pAssocRsp->ExtCap, &ar.ExtCap, sizeof(tDot11fIEExtCap));
        limLog(pMac, LOG1,
               FL("ExtCap is present, TDLSChanSwitProhibited: %d"),
               ar.ExtCap.TDLSChanSwitProhibited);
    }
    if ( ar.WMMParams.present )
    {
        pAssocRsp->wmeEdcaPresent = 1;
        ConvertWMMParams( pMac, &pAssocRsp->edca, &ar.WMMParams);
        limLog(pMac, LOG1, FL("WMM Parameter Element present in Association Response Frame!"));
        __printWMMParams(pMac, &ar.WMMParams);
    }

    if ( ar.HTCaps.present )
    {
        limLog(pMac, LOG1, FL("Received Assoc Response with HT Cap"));
        vos_mem_copy( &pAssocRsp->HTCaps, &ar.HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( ar.HTInfo.present )
    {   limLog(pMac, LOG1, FL("Received Assoc Response with HT Info"));
        vos_mem_copy( &pAssocRsp->HTInfo, &ar.HTInfo, sizeof( tDot11fIEHTInfo ) );
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (ar.MobilityDomain.present)
    {
        // MobilityDomain
        pAssocRsp->mdiePresent = 1;
        vos_mem_copy( (tANI_U8 *)&(pAssocRsp->mdie[0]), (tANI_U8 *)&(ar.MobilityDomain.MDID),
                       sizeof(tANI_U16) );
        pAssocRsp->mdie[2] = ((ar.MobilityDomain.overDSCap << 0) | (ar.MobilityDomain.resourceReqCap << 1));
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        limLog(pMac, LOG1, FL("new mdie=%02x%02x%02x"), (unsigned int)pAssocRsp->mdie[0],
               (unsigned int)pAssocRsp->mdie[1], (unsigned int)pAssocRsp->mdie[2]);
#endif
    }

    if ( ar.FTInfo.present )
    {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        limLog(pMac, LOG1, FL("FT Info present %d %d %d"), ar.FTInfo.R0KH_ID.num_PMK_R0_ID,
                ar.FTInfo.R0KH_ID.present,
                ar.FTInfo.R1KH_ID.present);
#endif
        pAssocRsp->ftinfoPresent = 1;
        vos_mem_copy( &pAssocRsp->FTInfo, &ar.FTInfo, sizeof(tDot11fIEFTInfo) );
    }
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (ar.num_RICDataDesc) {
        for (cnt=0; cnt < ar.num_RICDataDesc; cnt++) {
            if (ar.RICDataDesc[cnt].present) {
                vos_mem_copy( &pAssocRsp->RICData[cnt], &ar.RICDataDesc[cnt],
                              sizeof(tDot11fIERICDataDesc));
            }
        }
        pAssocRsp->num_RICData = ar.num_RICDataDesc;
        pAssocRsp->ricPresent = TRUE;
    }
#endif    

#ifdef FEATURE_WLAN_ESE
    if (ar.num_WMMTSPEC) {
        pAssocRsp->num_tspecs = ar.num_WMMTSPEC;
        for (cnt=0; cnt < ar.num_WMMTSPEC; cnt++) {
            vos_mem_copy( &pAssocRsp->TSPECInfo[cnt], &ar.WMMTSPEC[cnt],
                          (sizeof(tDot11fIEWMMTSPEC)*ar.num_WMMTSPEC));
        }
        pAssocRsp->tspecPresent = TRUE;
    }
   
    if(ar.ESETrafStrmMet.present)
    {
        pAssocRsp->tsmPresent = 1;
        vos_mem_copy(&pAssocRsp->tsmIE.tsid,
                &ar.ESETrafStrmMet.tsid,sizeof(tSirMacESETSMIE));
    }    
#endif

#ifdef WLAN_FEATURE_11AC
    if ( ar.VHTCaps.present )
    {
        vos_mem_copy( &pAssocRsp->VHTCaps, &ar.VHTCaps, sizeof( tDot11fIEVHTCaps ) );
        limLog( pMac, LOG1, FL("Received Assoc Response with VHT Cap"));
        limLogVHTCap(pMac, &pAssocRsp->VHTCaps);
    }
    if ( ar.VHTOperation.present )
    {
        vos_mem_copy( &pAssocRsp->VHTOperation, &ar.VHTOperation, sizeof( tDot11fIEVHTOperation) );
        limLog( pMac, LOG1, FL("Received Assoc Response with VHT Operation"));
        limLogVHTOperation(pMac, &pAssocRsp->VHTOperation);
    }
#endif
    if(ar.OBSSScanParameters.present)
    {
       vos_mem_copy( &pAssocRsp->OBSSScanParameters, &ar.OBSSScanParameters,
                      sizeof( tDot11fIEOBSSScanParameters));
    }
    if ( ar.QosMapSet.present )
    {
        pAssocRsp->QosMapSet.present = 1;
        ConvertQosMapsetFrame( pMac, &pAssocRsp->QosMapSet, &ar.QosMapSet);
        limLog( pMac, LOG1, FL("Received Assoc Response with Qos Map Set"));
        limLogQosMapSet(pMac, &pAssocRsp->QosMapSet);
    }
    return eSIR_SUCCESS;
} // End sirConvertAssocRespFrame2Struct.

tSirRetStatus
sirConvertReassocReqFrame2Struct(tpAniSirGlobal pMac,
                                 tANI_U8            *pFrame,
                                 tANI_U32            nFrame,
                                 tpSirAssocReq  pAssocReq)
{
    static tDot11fReAssocRequest ar;
    tANI_U32                   status;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAssocReq, sizeof(tSirAssocReq), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackReAssocRequest( pMac, pFrame, nFrame, &ar );
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse a Re-association Request (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking a Re-association Request (0x%08x, %d bytes)"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fReAssocRequest' to a 'tSirAssocReq'...

    // make sure this is seen as a re-assoc request
    pAssocReq->reassocRequest = 1;

    // Capabilities
    pAssocReq->capabilityInfo.ess            = ar.Capabilities.ess;
    pAssocReq->capabilityInfo.ibss           = ar.Capabilities.ibss;
    pAssocReq->capabilityInfo.cfPollable     = ar.Capabilities.cfPollable;
    pAssocReq->capabilityInfo.cfPollReq      = ar.Capabilities.cfPollReq;
    pAssocReq->capabilityInfo.privacy        = ar.Capabilities.privacy;
    pAssocReq->capabilityInfo.shortPreamble  = ar.Capabilities.shortPreamble;
    pAssocReq->capabilityInfo.pbcc           = ar.Capabilities.pbcc;
    pAssocReq->capabilityInfo.channelAgility = ar.Capabilities.channelAgility;
    pAssocReq->capabilityInfo.spectrumMgt    = ar.Capabilities.spectrumMgt;
    pAssocReq->capabilityInfo.qos            = ar.Capabilities.qos;
    pAssocReq->capabilityInfo.shortSlotTime  = ar.Capabilities.shortSlotTime;
    pAssocReq->capabilityInfo.apsd           = ar.Capabilities.apsd;
    pAssocReq->capabilityInfo.rrm            = ar.Capabilities.rrm;
    pAssocReq->capabilityInfo.dsssOfdm      = ar.Capabilities.dsssOfdm;
    pAssocReq->capabilityInfo.delayedBA       = ar.Capabilities.delayedBA;
    pAssocReq->capabilityInfo.immediateBA    = ar.Capabilities.immediateBA;

    // Listen Interval
    pAssocReq->listenInterval = ar.ListenInterval.interval;

    // SSID
    if ( ar.SSID.present )
    {
        pAssocReq->ssidPresent = 1;
        ConvertSSID( pMac, &pAssocReq->ssId, &ar.SSID );
    }

    // Supported Rates
    if ( ar.SuppRates.present )
    {
        pAssocReq->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pAssocReq->supportedRates, &ar.SuppRates );
    }

    // Extended Supported Rates
    if ( ar.ExtSuppRates.present )
    {
        pAssocReq->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pAssocReq->extendedRates,
                             &ar.ExtSuppRates );
    }

    // QOS Capabilities:
    if ( ar.QOSCapsStation.present )
    {
        pAssocReq->qosCapabilityPresent = 1;
        ConvertQOSCapsStation( pMac, &pAssocReq->qosCapability, &ar.QOSCapsStation );
    }

    // WPA
    if ( ar.WPAOpaque.present )
    {
        pAssocReq->wpaPresent = 1;
        ConvertWPAOpaque( pMac, &pAssocReq->wpa, &ar.WPAOpaque );
    }

    // RSN
    if ( ar.RSNOpaque.present )
    {
        pAssocReq->rsnPresent = 1;
        ConvertRSNOpaque( pMac, &pAssocReq->rsn, &ar.RSNOpaque );
    }


    // Power Capabilities
    if ( ar.PowerCaps.present )
    {
        pAssocReq->powerCapabilityPresent     = 1;
        ConvertPowerCaps( pMac, &pAssocReq->powerCapability, &ar.PowerCaps );
    }

    // Supported Channels
    if ( ar.SuppChannels.present )
    {
        pAssocReq->supportedChannelsPresent = 1;
        ConvertSuppChannels( pMac, &pAssocReq->supportedChannels, &ar.SuppChannels );
    }

    if ( ar.HTCaps.present )
    {
        vos_mem_copy( &pAssocReq->HTCaps, &ar.HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( ar.WMMInfoStation.present )
    {
        pAssocReq->wmeInfoPresent = 1;
        vos_mem_copy( &pAssocReq->WMMInfoStation, &ar.WMMInfoStation,
                      sizeof( tDot11fIEWMMInfoStation ) );

    }

    if ( ar.WMMCaps.present ) pAssocReq->wsmCapablePresent = 1;

    if ( ! pAssocReq->ssidPresent )
    {
        PELOG2(limLog(pMac, LOG2, FL("Received Assoc without SSID IE."));)
        return eSIR_FAILURE;
    }

    if ( ! pAssocReq->suppRatesPresent && ! pAssocReq->extendedRatesPresent )
    {
        PELOG2(limLog(pMac, LOG2, FL("Received Assoc without supp rate IE."));)
        return eSIR_FAILURE;
    }

    // Why no call to 'updateAssocReqFromPropCapability' here, like
    // there is in 'sirConvertAssocReqFrame2Struct'?

    // WSC IE
    if (ar.WscIEOpaque.present) 
    {
        pAssocReq->addIEPresent = 1;
        ConvertWscOpaque(pMac, &pAssocReq->addIE, &ar.WscIEOpaque);
    }
    
    if(ar.P2PIEOpaque.present)
    {
        pAssocReq->addIEPresent = 1;
        ConvertP2POpaque( pMac, &pAssocReq->addIE, &ar.P2PIEOpaque);
    }

#ifdef WLAN_FEATURE_WFD
    if(ar.WFDIEOpaque.present)
    {
        pAssocReq->addIEPresent = 1;
        ConvertWFDOpaque( pMac, &pAssocReq->addIE, &ar.WFDIEOpaque);
    }
#endif

#ifdef WLAN_FEATURE_11AC
    if ( ar.VHTCaps.present )
    {
        vos_mem_copy( &pAssocReq->VHTCaps, &ar.VHTCaps, sizeof( tDot11fIEVHTCaps ) );
    }
    if ( ar.OperatingMode.present )
    {
        vos_mem_copy( &pAssocReq->operMode, &ar.OperatingMode, sizeof( tDot11fIEOperatingMode  ) );
        limLog( pMac, LOGW, FL("Received Assoc Req with Operating Mode IE"));
        limLogOperatingMode( pMac, &pAssocReq->operMode);
    }
#endif
    return eSIR_SUCCESS;

} // End sirConvertReassocReqFrame2Struct.


#if defined(FEATURE_WLAN_ESE_UPLOAD)
tSirRetStatus
sirFillBeaconMandatoryIEforEseBcnReport(tpAniSirGlobal   pMac,
                                        tANI_U8         *pPayload,
                                        const tANI_U32   nPayload,
                                        tANI_U8        **outIeBuf,
                                        tANI_U32        *pOutIeLen)
{
    tDot11fBeaconIEs            *pBies = NULL;
    tANI_U32                    status = eHAL_STATUS_SUCCESS;
    tSirRetStatus               retStatus = eSIR_SUCCESS;
    tSirEseBcnReportMandatoryIe eseBcnReportMandatoryIe;

    /* To store how many bytes are required to be allocated
           for Bcn report mandatory Ies */
    tANI_U16 numBytes = 0, freeBytes = 0;
    tANI_U8  *pos = NULL;

    // Zero-init our [out] parameter,
    vos_mem_set( (tANI_U8*)&eseBcnReportMandatoryIe, sizeof(eseBcnReportMandatoryIe), 0 );
    pBies = vos_mem_malloc(sizeof(tDot11fBeaconIEs));
    if ( NULL == pBies )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (!HAL_STATUS_SUCCESS(status))
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }
    // delegate to the framesc-generated code,
    status = dot11fUnpackBeaconIEs( pMac, pPayload, nPayload, pBies );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse Beacon IEs (0x%08x, %d bytes)"),
                  status, nPayload);
        vos_mem_free(pBies);
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking Beacon IEs (0x%08x, %d bytes)"),
                 status, nPayload );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pPayload, nPayload);)
    }

    // & "transliterate" from a 'tDot11fBeaconIEs' to a 'eseBcnReportMandatoryIe'...
    if ( !pBies->SSID.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE SSID not present!"));)
    }
    else
    {
        eseBcnReportMandatoryIe.ssidPresent = 1;
        ConvertSSID( pMac, &eseBcnReportMandatoryIe.ssId, &pBies->SSID );
        /* 1 for EID, 1 for length and length bytes */
        numBytes += 1 + 1 + eseBcnReportMandatoryIe.ssId.length;
    }

    if ( !pBies->SuppRates.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
    }
    else
    {
        eseBcnReportMandatoryIe.suppRatesPresent = 1;
        ConvertSuppRates( pMac, &eseBcnReportMandatoryIe.supportedRates, &pBies->SuppRates );
        numBytes += 1 + 1 + eseBcnReportMandatoryIe.supportedRates.numRates;
    }

    if ( pBies->FHParamSet.present)
    {
        eseBcnReportMandatoryIe.fhParamPresent = 1;
        ConvertFHParams( pMac, &eseBcnReportMandatoryIe.fhParamSet, &pBies->FHParamSet );
        numBytes += 1 + 1 + SIR_MAC_FH_PARAM_SET_EID_MAX;
    }

    if ( pBies->DSParams.present )
    {
        eseBcnReportMandatoryIe.dsParamsPresent = 1;
        eseBcnReportMandatoryIe.dsParamSet.channelNumber = pBies->DSParams.curr_channel;
        numBytes += 1 + 1 + SIR_MAC_DS_PARAM_SET_EID_MAX;
    }

    if ( pBies->CFParams.present )
    {
        eseBcnReportMandatoryIe.cfPresent = 1;
        ConvertCFParams( pMac, &eseBcnReportMandatoryIe.cfParamSet, &pBies->CFParams );
        numBytes += 1 + 1 + SIR_MAC_CF_PARAM_SET_EID_MAX;
    }

    if ( pBies->IBSSParams.present )
    {
        eseBcnReportMandatoryIe.ibssParamPresent = 1;
        eseBcnReportMandatoryIe.ibssParamSet.atim = pBies->IBSSParams.atim;
        numBytes += 1 + 1 + SIR_MAC_IBSS_PARAM_SET_EID_MAX;
    }

    if ( pBies->TIM.present )
    {
        eseBcnReportMandatoryIe.timPresent = 1;
        eseBcnReportMandatoryIe.tim.dtimCount     = pBies->TIM.dtim_count;
        eseBcnReportMandatoryIe.tim.dtimPeriod    = pBies->TIM.dtim_period;
        eseBcnReportMandatoryIe.tim.bitmapControl = pBies->TIM.bmpctl;
        /* As per the ESE spec, May truncate and report first 4 octets only */
        numBytes += 1 + 1 + SIR_MAC_TIM_EID_MIN;
    }

    if ( pBies->RRMEnabledCap.present )
    {
        eseBcnReportMandatoryIe.rrmPresent = 1;
        vos_mem_copy( &eseBcnReportMandatoryIe.rmEnabledCapabilities, &pBies->RRMEnabledCap, sizeof( tDot11fIERRMEnabledCap ) );
        numBytes += 1 + 1 + SIR_MAC_RM_ENABLED_CAPABILITY_EID_MAX;
    }

    *outIeBuf = vos_mem_malloc(numBytes);
    if (NULL == *outIeBuf)
    {
        limLog(pMac, LOGP, FL("Memory Allocation failure"));
        vos_mem_free(pBies);
        return eSIR_FAILURE;
    }
    pos = *outIeBuf;
    *pOutIeLen = numBytes;
    freeBytes = numBytes;

    /* Start filling the output Ie with Mandatory IE information */
    /* Fill SSID IE */
    if (eseBcnReportMandatoryIe.ssidPresent)
    {
       if (freeBytes < (1 + 1 + eseBcnReportMandatoryIe.ssId.length))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy SSID"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_SSID_EID;
       pos++;
       *pos = eseBcnReportMandatoryIe.ssId.length;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)eseBcnReportMandatoryIe.ssId.ssId,
                    eseBcnReportMandatoryIe.ssId.length);
       pos += eseBcnReportMandatoryIe.ssId.length;
       freeBytes -= (1 + 1 + eseBcnReportMandatoryIe.ssId.length);
    }

    /* Fill Supported Rates IE */
    if (eseBcnReportMandatoryIe.suppRatesPresent)
    {
       if (freeBytes < (1 + 1 + eseBcnReportMandatoryIe.supportedRates.numRates))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy Rates IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_RATESET_EID;
       pos++;
       *pos = eseBcnReportMandatoryIe.supportedRates.numRates;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)eseBcnReportMandatoryIe.supportedRates.rate,
                    eseBcnReportMandatoryIe.supportedRates.numRates);
       pos += eseBcnReportMandatoryIe.supportedRates.numRates;
       freeBytes -= (1 + 1 + eseBcnReportMandatoryIe.supportedRates.numRates);
    }

    /* Fill FH Parameter set IE */
    if (eseBcnReportMandatoryIe.fhParamPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_FH_PARAM_SET_EID_MAX))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy FHIE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_FH_PARAM_SET_EID;
       pos++;
       *pos = SIR_MAC_FH_PARAM_SET_EID_MAX;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)&eseBcnReportMandatoryIe.fhParamSet,
                    SIR_MAC_FH_PARAM_SET_EID_MAX);
       pos += SIR_MAC_FH_PARAM_SET_EID_MAX;
       freeBytes -= (1 + 1 + SIR_MAC_FH_PARAM_SET_EID_MAX);
    }

    /* Fill DS Parameter set IE */
    if (eseBcnReportMandatoryIe.dsParamsPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_DS_PARAM_SET_EID_MAX))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy DS IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_DS_PARAM_SET_EID;
       pos++;
       *pos = SIR_MAC_DS_PARAM_SET_EID_MAX;
       pos++;
       *pos = eseBcnReportMandatoryIe.dsParamSet.channelNumber;
       pos += SIR_MAC_DS_PARAM_SET_EID_MAX;
       freeBytes -= (1 + 1 + SIR_MAC_DS_PARAM_SET_EID_MAX);
    }

    /* Fill CF Parameter set */
    if (eseBcnReportMandatoryIe.cfPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_CF_PARAM_SET_EID_MAX))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy CF IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_CF_PARAM_SET_EID;
       pos++;
       *pos = SIR_MAC_CF_PARAM_SET_EID_MAX;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)&eseBcnReportMandatoryIe.cfParamSet,
                    SIR_MAC_CF_PARAM_SET_EID_MAX);
       pos += SIR_MAC_CF_PARAM_SET_EID_MAX;
       freeBytes -= (1 + 1 + SIR_MAC_CF_PARAM_SET_EID_MAX);
    }

    /* Fill IBSS Parameter set IE */
    if (eseBcnReportMandatoryIe.ibssParamPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_IBSS_PARAM_SET_EID_MAX))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy IBSS IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_IBSS_PARAM_SET_EID;
       pos++;
       *pos = SIR_MAC_IBSS_PARAM_SET_EID_MAX;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)&eseBcnReportMandatoryIe.ibssParamSet.atim,
                    SIR_MAC_IBSS_PARAM_SET_EID_MAX);
       pos += SIR_MAC_IBSS_PARAM_SET_EID_MAX;
       freeBytes -= (1 + 1 + SIR_MAC_IBSS_PARAM_SET_EID_MAX);
    }

    /* Fill TIM IE */
    if (eseBcnReportMandatoryIe.timPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_TIM_EID_MIN))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy TIM IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_TIM_EID;
       pos++;
       *pos = SIR_MAC_TIM_EID_MIN;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)&eseBcnReportMandatoryIe.tim,
                    SIR_MAC_TIM_EID_MIN);
       pos += SIR_MAC_TIM_EID_MIN;
       freeBytes -= (1 + 1 + SIR_MAC_TIM_EID_MIN);
    }

    /* Fill RM Capability IE */
    if (eseBcnReportMandatoryIe.rrmPresent)
    {
       if (freeBytes < (1 + 1 + SIR_MAC_RM_ENABLED_CAPABILITY_EID_MAX))
       {
           limLog(pMac, LOGP, FL("Insufficient memory to copy RRM IE"));
           retStatus = eSIR_FAILURE;
           goto err_bcnrep;
       }
       *pos = SIR_MAC_RM_ENABLED_CAPABILITY_EID;
       pos++;
       *pos = SIR_MAC_RM_ENABLED_CAPABILITY_EID_MAX;
       pos++;
       vos_mem_copy(pos, (tANI_U8*)&eseBcnReportMandatoryIe.rmEnabledCapabilities,
                    SIR_MAC_RM_ENABLED_CAPABILITY_EID_MAX);
       freeBytes -= (1 + 1 + SIR_MAC_RM_ENABLED_CAPABILITY_EID_MAX);
    }

    if (freeBytes != 0)
    {
       limLog(pMac, LOGP, FL("Mismatch in allocation and copying of IE in Bcn Rep"));
       retStatus = eSIR_FAILURE;
    }

err_bcnrep:
    /* The message counter would not be incremented in case of
     * returning failure and hence next time, this function gets
     * called, it would be using the same msg ctr for a different
     * BSS.So, it is good to clear the memory allocated for a BSS
     * that is returning failure.On success, the caller would take
     * care of freeing up the memory*/
    if (retStatus == eSIR_FAILURE)
    {
       vos_mem_free(*outIeBuf);
       *outIeBuf = NULL;
    }
    vos_mem_free(pBies);
    return retStatus;
}

#endif /* FEATURE_WLAN_ESE_UPLOAD */

tSirRetStatus
sirParseBeaconIE(tpAniSirGlobal        pMac,
                 tpSirProbeRespBeacon  pBeaconStruct,
                 tANI_U8                   *pPayload,
                 tANI_U32                   nPayload)
{
    tDot11fBeaconIEs *pBies;
    tANI_U32              status;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pBeaconStruct, sizeof(tSirProbeRespBeacon), 0 );

    pBies = vos_mem_malloc(sizeof(tDot11fBeaconIEs));
    if ( NULL == pBies )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (!HAL_STATUS_SUCCESS(status))
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }
    // delegate to the framesc-generated code,
    status = dot11fUnpackBeaconIEs( pMac, pPayload, nPayload, pBies );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse Beacon IEs (0x%08x, %d bytes)"),
                  status, nPayload);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pPayload, nPayload);)
        vos_mem_free(pBies);
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking Beacon IEs (0x%08x, %d bytes)"),
                 status, nPayload );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pPayload, nPayload);)
    }

    // & "transliterate" from a 'tDot11fBeaconIEs' to a 'tSirProbeRespBeacon'...
    if ( ! pBies->SSID.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE SSID not present!"));)
    }
    else
    {
        pBeaconStruct->ssidPresent = 1;
        ConvertSSID( pMac, &pBeaconStruct->ssId, &pBies->SSID );
    }

    if ( ! pBies->SuppRates.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
    }
    else
    {
        pBeaconStruct->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pBeaconStruct->supportedRates, &pBies->SuppRates );
    }

    if ( pBies->ExtSuppRates.present )
    {
        pBeaconStruct->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pBeaconStruct->extendedRates, &pBies->ExtSuppRates );
    }

    if ( pBies->CFParams.present )
    {
        pBeaconStruct->cfPresent = 1;
        ConvertCFParams( pMac, &pBeaconStruct->cfParamSet, &pBies->CFParams );
    }

    if ( pBies->TIM.present )
    {
        pBeaconStruct->timPresent = 1;
        ConvertTIM( pMac, &pBeaconStruct->tim, &pBies->TIM );
    }

    if ( pBies->Country.present )
    {
        pBeaconStruct->countryInfoPresent = 1;
        ConvertCountry( pMac, &pBeaconStruct->countryInfoParam, &pBies->Country );
    }

    // 11h IEs
    if(pBies->TPCReport.present)
    {
        pBeaconStruct->tpcReportPresent = 1;
        vos_mem_copy( &pBeaconStruct->tpcReport,
                      &pBies->TPCReport,
                      sizeof( tDot11fIETPCReport));
    }

    if(pBies->PowerConstraints.present)
    {
        pBeaconStruct->powerConstraintPresent = 1;
        vos_mem_copy( &pBeaconStruct->localPowerConstraint,
                      &pBies->PowerConstraints,
                      sizeof(tDot11fIEPowerConstraints));
    }
#ifdef FEATURE_WLAN_ESE
    if(pBies->ESETxmitPower.present)
    {
        pBeaconStruct->eseTxPwr.present = 1;
        pBeaconStruct->eseTxPwr.power_limit = pBies->ESETxmitPower.power_limit;
    }
    if (pBies->QBSSLoad.present)
    {
        vos_mem_copy( &pBeaconStruct->QBSSLoad, &pBies->QBSSLoad, sizeof(tDot11fIEQBSSLoad));
    }
#endif

    if ( pBies->EDCAParamSet.present )
    {
        pBeaconStruct->edcaPresent = 1;
        ConvertEDCAParam( pMac, &pBeaconStruct->edcaParams, &pBies->EDCAParamSet );
    }

    // QOS Capabilities:
    if ( pBies->QOSCapsAp.present )
    {
        pBeaconStruct->qosCapabilityPresent = 1;
        ConvertQOSCaps( pMac, &pBeaconStruct->qosCapability, &pBies->QOSCapsAp );
    }



    if ( pBies->ChanSwitchAnn.present )
    {
        pBeaconStruct->channelSwitchPresent = 1;
        vos_mem_copy( &pBeaconStruct->channelSwitchIE, &pBies->ChanSwitchAnn,
                      sizeof(tDot11fIEChanSwitchAnn));
    }

    if ( pBies->ExtChanSwitchAnn.present)
    {
        pBeaconStruct->extChannelSwitchPresent= 1;
        vos_mem_copy( &pBeaconStruct->extChannelSwitchIE, &pBies->ExtChanSwitchAnn,
                      sizeof(tDot11fIEExtChanSwitchAnn));
    }

    if ( pBies->Quiet.present )
    {
        pBeaconStruct->quietIEPresent = 1;
        vos_mem_copy( &pBeaconStruct->quietIE, &pBies->Quiet, sizeof(tDot11fIEQuiet) );
    }

    if ( pBies->HTCaps.present )
    {
        vos_mem_copy( &pBeaconStruct->HTCaps, &pBies->HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( pBies->HTInfo.present )
    {
        vos_mem_copy( &pBeaconStruct->HTInfo, &pBies->HTInfo, sizeof( tDot11fIEHTInfo ) );
    }

    if ( pBies->DSParams.present )
    {
        pBeaconStruct->dsParamsPresent = 1;
        pBeaconStruct->channelNumber = pBies->DSParams.curr_channel;
    }
    else if(pBies->HTInfo.present)
    {
        pBeaconStruct->channelNumber = pBies->HTInfo.primaryChannel;
    }
    
    if ( pBies->RSN.present )
    {
        pBeaconStruct->rsnPresent = 1;
        ConvertRSN( pMac, &pBeaconStruct->rsn, &pBies->RSN );
    }

    if ( pBies->WPA.present )
    {
        pBeaconStruct->wpaPresent = 1;
        ConvertWPA( pMac, &pBeaconStruct->wpa, &pBies->WPA );
    }

    if ( pBies->WMMParams.present )
    {
        pBeaconStruct->wmeEdcaPresent = 1;
        ConvertWMMParams( pMac, &pBeaconStruct->edcaParams, &pBies->WMMParams );
    }

    if ( pBies->WMMInfoAp.present )
    {
        pBeaconStruct->wmeInfoPresent = 1;
    }

    if ( pBies->WMMCaps.present )
    {
        pBeaconStruct->wsmCapablePresent = 1;
    }


    if ( pBies->ERPInfo.present )
    {
        pBeaconStruct->erpPresent = 1;
        ConvertERPInfo( pMac, &pBeaconStruct->erpIEInfo, &pBies->ERPInfo );
    }

#ifdef WLAN_FEATURE_11AC
    if ( pBies->VHTCaps.present )
    {
         pBeaconStruct->VHTCaps.present = 1;
         vos_mem_copy( &pBeaconStruct->VHTCaps, &pBies->VHTCaps, sizeof( tDot11fIEVHTCaps ) );
    }
    if ( pBies->VHTOperation.present )
    {
         pBeaconStruct->VHTOperation.present = 1;
         vos_mem_copy( &pBeaconStruct->VHTOperation, &pBies->VHTOperation,
                       sizeof( tDot11fIEVHTOperation) );
    }
    if ( pBies->VHTExtBssLoad.present )
    {
         pBeaconStruct->VHTExtBssLoad.present = 1;
         vos_mem_copy( &pBeaconStruct->VHTExtBssLoad, &pBies->VHTExtBssLoad,
                       sizeof( tDot11fIEVHTExtBssLoad) );
    }
    if( pBies->OperatingMode.present)
    {
        pBeaconStruct->OperatingMode.present = 1;
        vos_mem_copy( &pBeaconStruct->OperatingMode, &pBies->OperatingMode,
                      sizeof( tDot11fIEOperatingMode) );
    }

#endif
    if (pBies->ExtCap.present )
    {
        pBeaconStruct->ExtCap.present = 1;
        vos_mem_copy( &pBeaconStruct->ExtCap, &pBies->ExtCap,
                        sizeof(tDot11fIEExtCap));
    }
    vos_mem_free(pBies);


    return eSIR_SUCCESS;

} // End sirParseBeaconIE.

tSirRetStatus
sirConvertBeaconFrame2Struct(tpAniSirGlobal       pMac,
                             tANI_U8             *pFrame,
                             tpSirProbeRespBeacon pBeaconStruct)
{
    tDot11fBeacon  *pBeacon;
    tANI_U32        status, nPayload;
    tANI_U8        *pPayload;
    tpSirMacMgmtHdr pHdr;
    tANI_U8         mappedRXCh;
    tANI_U8         rfBand;

    pPayload = WDA_GET_RX_MPDU_DATA( pFrame );
    nPayload = WDA_GET_RX_PAYLOAD_LEN( pFrame );
    pHdr     = WDA_GET_RX_MAC_HEADER( pFrame );
    mappedRXCh = WDA_GET_RX_CH( pFrame );
    rfBand = WDA_GET_RX_RFBAND( pFrame );
    
    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pBeaconStruct, sizeof(tSirProbeRespBeacon), 0 );

    pBeacon = vos_mem_vmalloc(sizeof(tDot11fBeacon));
    if ( NULL == pBeacon )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (!HAL_STATUS_SUCCESS(status))
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }

    vos_mem_set( ( tANI_U8* )pBeacon, sizeof(tDot11fBeacon), 0 );

    // get the MAC address out of the BD,
    vos_mem_copy( pBeaconStruct->bssid, pHdr->sa, 6 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackBeacon( pMac, pPayload, nPayload, pBeacon );
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse Beacon IEs (0x%08x, %d bytes)"),
                  status, nPayload);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pPayload, nPayload);)
        vos_mem_vfree(pBeacon);
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking Beacon IEs (0x%08x, %d bytes)"),
                 status, nPayload );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pPayload, nPayload);)
    }

    // & "transliterate" from a 'tDot11fBeacon' to a 'tSirProbeRespBeacon'...
    // Timestamp
    vos_mem_copy( ( tANI_U8* )pBeaconStruct->timeStamp, ( tANI_U8* )&pBeacon->TimeStamp,
                   sizeof(tSirMacTimeStamp) );

    // Beacon Interval
    pBeaconStruct->beaconInterval = pBeacon->BeaconInterval.interval;

    // Capabilities
    pBeaconStruct->capabilityInfo.ess            = pBeacon->Capabilities.ess;
    pBeaconStruct->capabilityInfo.ibss           = pBeacon->Capabilities.ibss;
    pBeaconStruct->capabilityInfo.cfPollable     = pBeacon->Capabilities.cfPollable;
    pBeaconStruct->capabilityInfo.cfPollReq      = pBeacon->Capabilities.cfPollReq;
    pBeaconStruct->capabilityInfo.privacy        = pBeacon->Capabilities.privacy;
    pBeaconStruct->capabilityInfo.shortPreamble  = pBeacon->Capabilities.shortPreamble;
    pBeaconStruct->capabilityInfo.pbcc           = pBeacon->Capabilities.pbcc;
    pBeaconStruct->capabilityInfo.channelAgility = pBeacon->Capabilities.channelAgility;
    pBeaconStruct->capabilityInfo.spectrumMgt    = pBeacon->Capabilities.spectrumMgt;
    pBeaconStruct->capabilityInfo.qos            = pBeacon->Capabilities.qos;
    pBeaconStruct->capabilityInfo.shortSlotTime  = pBeacon->Capabilities.shortSlotTime;
    pBeaconStruct->capabilityInfo.apsd           = pBeacon->Capabilities.apsd;
    pBeaconStruct->capabilityInfo.rrm            = pBeacon->Capabilities.rrm;
    pBeaconStruct->capabilityInfo.dsssOfdm      = pBeacon->Capabilities.dsssOfdm;
    pBeaconStruct->capabilityInfo.delayedBA     = pBeacon->Capabilities.delayedBA;
    pBeaconStruct->capabilityInfo.immediateBA    = pBeacon->Capabilities.immediateBA;
 
    if ( ! pBeacon->SSID.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE SSID not present!"));)
    }
    else
    {
        pBeaconStruct->ssidPresent = 1;
        ConvertSSID( pMac, &pBeaconStruct->ssId, &pBeacon->SSID );
    }

    if ( ! pBeacon->SuppRates.present )
    {
        PELOGW(limLog(pMac, LOGW, FL("Mandatory IE Supported Rates not present!"));)
    }
    else
    {
        pBeaconStruct->suppRatesPresent = 1;
        ConvertSuppRates( pMac, &pBeaconStruct->supportedRates, &pBeacon->SuppRates );
    }

    if ( pBeacon->ExtSuppRates.present )
    {
        pBeaconStruct->extendedRatesPresent = 1;
        ConvertExtSuppRates( pMac, &pBeaconStruct->extendedRates, &pBeacon->ExtSuppRates );
    }


    if ( pBeacon->CFParams.present )
    {
        pBeaconStruct->cfPresent = 1;
        ConvertCFParams( pMac, &pBeaconStruct->cfParamSet, &pBeacon->CFParams );
    }

    if ( pBeacon->TIM.present )
    {
        pBeaconStruct->timPresent = 1;
        ConvertTIM( pMac, &pBeaconStruct->tim, &pBeacon->TIM );
    }

    if ( pBeacon->Country.present )
    {
        pBeaconStruct->countryInfoPresent = 1;
        ConvertCountry( pMac, &pBeaconStruct->countryInfoParam, &pBeacon->Country );
    }

    // QOS Capabilities:
    if ( pBeacon->QOSCapsAp.present )
    {
        pBeaconStruct->qosCapabilityPresent = 1;
        ConvertQOSCaps( pMac, &pBeaconStruct->qosCapability, &pBeacon->QOSCapsAp );
    }

    if ( pBeacon->EDCAParamSet.present )
    {
        pBeaconStruct->edcaPresent = 1;
        ConvertEDCAParam( pMac, &pBeaconStruct->edcaParams, &pBeacon->EDCAParamSet );
    }

    if ( pBeacon->ChanSwitchAnn.present )
    {
        pBeaconStruct->channelSwitchPresent = 1;
        vos_mem_copy( &pBeaconStruct->channelSwitchIE, &pBeacon->ChanSwitchAnn,
                                                       sizeof(tDot11fIEChanSwitchAnn) );
    }

    if ( pBeacon->ExtChanSwitchAnn.present )
    {
        pBeaconStruct->extChannelSwitchPresent = 1;
        vos_mem_copy( &pBeaconStruct->extChannelSwitchIE, &pBeacon->ExtChanSwitchAnn,
                                                       sizeof(tDot11fIEExtChanSwitchAnn) );
    }

    if( pBeacon->TPCReport.present)
    {
        pBeaconStruct->tpcReportPresent = 1;
        vos_mem_copy( &pBeaconStruct->tpcReport, &pBeacon->TPCReport,
                                                     sizeof(tDot11fIETPCReport));
    }

    if( pBeacon->PowerConstraints.present)
    {
        pBeaconStruct->powerConstraintPresent = 1;
        vos_mem_copy( &pBeaconStruct->localPowerConstraint, &pBeacon->PowerConstraints,
                                                               sizeof(tDot11fIEPowerConstraints));
    }

    if ( pBeacon->Quiet.present )
    {
        pBeaconStruct->quietIEPresent = 1;
        vos_mem_copy( &pBeaconStruct->quietIE, &pBeacon->Quiet, sizeof(tDot11fIEQuiet));
    }

    if ( pBeacon->HTCaps.present )
    {
        vos_mem_copy( &pBeaconStruct->HTCaps, &pBeacon->HTCaps, sizeof( tDot11fIEHTCaps ) );
    }

    if ( pBeacon->HTInfo.present )
    {
        vos_mem_copy( &pBeaconStruct->HTInfo, &pBeacon->HTInfo, sizeof( tDot11fIEHTInfo) );

    }

    if ( pBeacon->DSParams.present )
    {
        pBeaconStruct->dsParamsPresent = 1;
        pBeaconStruct->channelNumber = pBeacon->DSParams.curr_channel;
    }
    else if(pBeacon->HTInfo.present)
    {
        pBeaconStruct->channelNumber = pBeacon->HTInfo.primaryChannel;
    }
    else
    {
       if ((!rfBand) || IS_5G_BAND(rfBand))
           pBeaconStruct->channelNumber = limUnmapChannel(mappedRXCh);
       else if (IS_24G_BAND(rfBand))
           pBeaconStruct->channelNumber = mappedRXCh;
       else
       {
           /*Only 0, 1, 2 are expected values for RF band from FW
            * if FW fixes are not present then rf band value will
            * be 0, else either 1 or 2 are expected from FW, 3 is
            * not expected from FW */
           PELOGE(limLog(pMac, LOGE,
           FL("Channel info is not present in Beacon and"
                   " mapping is not done correctly"));)
           pBeaconStruct->channelNumber = mappedRXCh;
       }
    }

    if ( pBeacon->RSN.present )
    {
        pBeaconStruct->rsnPresent = 1;
        ConvertRSN( pMac, &pBeaconStruct->rsn, &pBeacon->RSN );
    }

    if ( pBeacon->WPA.present )
    {
        pBeaconStruct->wpaPresent = 1;
        ConvertWPA( pMac, &pBeaconStruct->wpa, &pBeacon->WPA );
    }

    if ( pBeacon->WMMParams.present )
    {
        pBeaconStruct->wmeEdcaPresent = 1;
        ConvertWMMParams( pMac, &pBeaconStruct->edcaParams, &pBeacon->WMMParams );
        PELOG1(limLog(pMac, LOG1, FL("WMM Parameter present in Beacon Frame!"));
        __printWMMParams(pMac, &pBeacon->WMMParams); )
    }

    if ( pBeacon->WMMInfoAp.present )
    {
        pBeaconStruct->wmeInfoPresent = 1;
        PELOG1(limLog(pMac, LOG1, FL("WMM Info present in Beacon Frame!"));)
    }

    if ( pBeacon->WMMCaps.present )
    {
        pBeaconStruct->wsmCapablePresent = 1;
    }

    if ( pBeacon->ERPInfo.present )
    {
        pBeaconStruct->erpPresent = 1;
        ConvertERPInfo( pMac, &pBeaconStruct->erpIEInfo, &pBeacon->ERPInfo );
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pBeacon->MobilityDomain.present)
    {
        // MobilityDomain
        pBeaconStruct->mdiePresent = 1;
        vos_mem_copy( (tANI_U8 *)&(pBeaconStruct->mdie[0]),
                      (tANI_U8 *)&(pBeacon->MobilityDomain.MDID), sizeof(tANI_U16) );
        pBeaconStruct->mdie[2] = ((pBeacon->MobilityDomain.overDSCap << 0) | (pBeacon->MobilityDomain.resourceReqCap << 1));

    }
#endif

#ifdef FEATURE_WLAN_ESE
    if (pBeacon->ESETxmitPower.present)
    {
        //ESE Tx Power
        pBeaconStruct->eseTxPwr.present = 1;
        vos_mem_copy(&pBeaconStruct->eseTxPwr,
                                   &pBeacon->ESETxmitPower,
                                   sizeof(tDot11fIEESETxmitPower));
    }
#endif

#ifdef WLAN_FEATURE_11AC
    if ( pBeacon->VHTCaps.present )
    {
        vos_mem_copy( &pBeaconStruct->VHTCaps, &pBeacon->VHTCaps, sizeof( tDot11fIEVHTCaps ) );
    }
    if ( pBeacon->VHTOperation.present )
    {
        vos_mem_copy( &pBeaconStruct->VHTOperation, &pBeacon->VHTOperation,
                      sizeof( tDot11fIEVHTOperation) );
    }
    if ( pBeacon->VHTExtBssLoad.present )
    {
        vos_mem_copy( &pBeaconStruct->VHTExtBssLoad, &pBeacon->VHTExtBssLoad,
                      sizeof( tDot11fIEVHTExtBssLoad) );
    }
    if(pBeacon->OperatingMode.present)
    {
        vos_mem_copy( &pBeaconStruct->OperatingMode, &pBeacon->OperatingMode,
                      sizeof( tDot11fIEOperatingMode) );
    }
    if(pBeacon->WiderBWChanSwitchAnn.present)
    {
        pBeaconStruct->WiderBWChanSwitchAnnPresent = 1;
        vos_mem_copy( &pBeaconStruct->WiderBWChanSwitchAnn, &pBeacon->WiderBWChanSwitchAnn,
                      sizeof( tDot11fIEWiderBWChanSwitchAnn));
    }      
#endif
    if(pBeacon->OBSSScanParameters.present)
    {
       vos_mem_copy( &pBeaconStruct->OBSSScanParameters,
                     &pBeacon->OBSSScanParameters,
                     sizeof( tDot11fIEOBSSScanParameters));
    }

    vos_mem_vfree(pBeacon);
    return eSIR_SUCCESS;

} // End sirConvertBeaconFrame2Struct.

tSirRetStatus
sirConvertAuthFrame2Struct(tpAniSirGlobal        pMac,
                           tANI_U8                   *pFrame,
                           tANI_U32                   nFrame,
                           tpSirMacAuthFrameBody pAuth)
{
    static tDot11fAuthentication auth;
    tANI_U32                   status;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAuth, sizeof(tSirMacAuthFrameBody), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackAuthentication( pMac, pFrame, nFrame, &auth );
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Authentication frame (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      limLog( pMac, LOGW, FL("There were warnings while unpacking an Authentication frame (0x%08x, %d bytes)"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fAuthentication' to a 'tSirMacAuthFrameBody'...
    pAuth->authAlgoNumber           = auth.AuthAlgo.algo;
    pAuth->authTransactionSeqNumber = auth.AuthSeqNo.no;
    pAuth->authStatusCode           = auth.Status.status;

    if ( auth.ChallengeText.present )
    {
        pAuth->type   = SIR_MAC_CHALLENGE_TEXT_EID;
        pAuth->length = auth.ChallengeText.num_text;
        vos_mem_copy( pAuth->challengeText, auth.ChallengeText.text, auth.ChallengeText.num_text );
    }

    return eSIR_SUCCESS;

} // End sirConvertAuthFrame2Struct.

tSirRetStatus
sirConvertAddtsReq2Struct(tpAniSirGlobal    pMac,
                          tANI_U8               *pFrame,
                          tANI_U32               nFrame,
                          tSirAddtsReqInfo *pAddTs)
{
    tDot11fAddTSRequest    addts = {{0}};
    tDot11fWMMAddTSRequest wmmaddts = {{0}};
    tANI_U8                     j;
    tANI_U16                    i;
    tANI_U32                    status;

    if ( SIR_MAC_QOS_ADD_TS_REQ != *( pFrame + 1 ) )
    {
        limLog( pMac, LOGE, FL("sirConvertAddtsReq2Struct invoked "
                                  "with an Action of %d; this is not "
                                  "supported & is probably an error."),
                   *( pFrame + 1 ) );
        return eSIR_FAILURE;
    }

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAddTs, sizeof(tSirAddtsReqInfo), 0 );

    // delegate to the framesc-generated code,
    switch ( *pFrame )
    {
    case SIR_MAC_ACTION_QOS_MGMT:
        status = dot11fUnpackAddTSRequest( pMac, pFrame, nFrame, &addts );
        break;
    case SIR_MAC_ACTION_WME:
        status = dot11fUnpackWMMAddTSRequest( pMac, pFrame, nFrame, &wmmaddts );
        break;
    default:
        limLog( pMac, LOGE, FL("sirConvertAddtsReq2Struct invoked "
                                  "with a Category of %d; this is not"
                                  " supported & is probably an error."),
                   *pFrame );
        return eSIR_FAILURE;
    }

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Add TS Request f"
                                 "rame (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpackin"
                                  "g an Add TS Request frame (0x%08x,"
                                  "%d bytes)"),
                   status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fAddTSRequest' or a
    // 'tDot11WMMAddTSRequest' to a 'tSirMacAddtsReqInfo'...
    if ( SIR_MAC_ACTION_QOS_MGMT == *pFrame )
    {
        pAddTs->dialogToken = addts.DialogToken.token;

        if ( addts.TSPEC.present )
        {
            ConvertTSPEC( pMac, &pAddTs->tspec, &addts.TSPEC );
        }
        else
        {
            limLog( pMac, LOGE, FL("Mandatory TSPEC element missing in Add TS Request.") );
            return eSIR_FAILURE;
        }

        if ( addts.num_TCLAS )
        {
            pAddTs->numTclas = (tANI_U8)addts.num_TCLAS;

            for ( i = 0U; i < addts.num_TCLAS; ++i )
            {
                if ( eSIR_SUCCESS != ConvertTCLAS( pMac, &( pAddTs->tclasInfo[i] ), &( addts.TCLAS[i] ) ) )
                {
                    limLog( pMac, LOGE, FL("Failed to convert a TCLAS IE.") );
                    return eSIR_FAILURE;
                }
            }
        }

        if ( addts.TCLASSPROC.present )
        {
            pAddTs->tclasProcPresent = 1;
            pAddTs->tclasProc = addts.TCLASSPROC.processing;
        }

        if ( addts.WMMTSPEC.present )
        {
            pAddTs->wsmTspecPresent = 1;
            ConvertWMMTSPEC( pMac, &pAddTs->tspec, &addts.WMMTSPEC );
        }

        if ( addts.num_WMMTCLAS )
        {
            j = (tANI_U8)(pAddTs->numTclas + addts.num_WMMTCLAS);
            if ( SIR_MAC_TCLASIE_MAXNUM > j ) j = SIR_MAC_TCLASIE_MAXNUM;

            for ( i = pAddTs->numTclas; i < j; ++i )
            {
                if ( eSIR_SUCCESS != ConvertWMMTCLAS( pMac, &( pAddTs->tclasInfo[i] ), &( addts.WMMTCLAS[i] ) ) )
                {
                    limLog( pMac, LOGE, FL("Failed to convert a TCLAS IE.") );
                    return eSIR_FAILURE;
                }
            }
        }

        if ( addts.WMMTCLASPROC.present )
        {
            pAddTs->tclasProcPresent = 1;
            pAddTs->tclasProc = addts.WMMTCLASPROC.processing;
        }

        if ( 1 < pAddTs->numTclas && ( ! pAddTs->tclasProcPresent ) )
        {
            limLog( pMac, LOGE, FL("%d TCLAS IE but not TCLASPROC IE."),
                       pAddTs->numTclas );
            return eSIR_FAILURE;
        }
    }
    else
    {
        pAddTs->dialogToken = wmmaddts.DialogToken.token;

        if ( wmmaddts.WMMTSPEC.present )
        {
            pAddTs->wmeTspecPresent = 1;
            ConvertWMMTSPEC( pMac, &pAddTs->tspec, &wmmaddts.WMMTSPEC );
        }
        else
        {
            limLog( pMac, LOGE, FL("Mandatory WME TSPEC element missing!") );
            return eSIR_FAILURE;
        }
    }

    return eSIR_SUCCESS;

} // End sirConvertAddtsReq2Struct.

tSirRetStatus
sirConvertAddtsRsp2Struct(tpAniSirGlobal    pMac,
                          tANI_U8               *pFrame,
                          tANI_U32               nFrame,
                          tSirAddtsRspInfo *pAddTs)
{
    tDot11fAddTSResponse    addts = {{0}};
    tDot11fWMMAddTSResponse wmmaddts = {{0}};
    tANI_U8                      j;
    tANI_U16                     i;
    tANI_U32                     status;

    if ( SIR_MAC_QOS_ADD_TS_RSP != *( pFrame + 1 ) )
    {
        limLog( pMac, LOGE, FL("sirConvertAddtsRsp2Struct invoked "
                                  "with an Action of %d; this is not "
                                  "supported & is probably an error."),
                   *( pFrame + 1 ) );
        return eSIR_FAILURE;
    }

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pAddTs, sizeof(tSirAddtsRspInfo), 0 );
    vos_mem_set( ( tANI_U8* )&addts, sizeof(tDot11fAddTSResponse), 0 );
    vos_mem_set( ( tANI_U8* )&wmmaddts, sizeof(tDot11fWMMAddTSResponse), 0 );


    // delegate to the framesc-generated code,
    switch ( *pFrame )
    {
    case SIR_MAC_ACTION_QOS_MGMT:
        status = dot11fUnpackAddTSResponse( pMac, pFrame, nFrame, &addts );
        break;
    case SIR_MAC_ACTION_WME:
        status = dot11fUnpackWMMAddTSResponse( pMac, pFrame, nFrame, &wmmaddts );
        break;
    default:
        limLog( pMac, LOGE, FL("sirConvertAddtsRsp2Struct invoked "
                                  "with a Category of %d; this is not"
                                  " supported & is probably an error."),
                   *pFrame );
        return eSIR_FAILURE;
    }

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Add TS Response f"
                                 "rame (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpackin"
                                  "g an Add TS Response frame (0x%08x,"
                                  "%d bytes)"),
                   status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fAddTSResponse' or a
    // 'tDot11WMMAddTSResponse' to a 'tSirMacAddtsRspInfo'...
    if ( SIR_MAC_ACTION_QOS_MGMT == *pFrame )
    {
        pAddTs->dialogToken = addts.DialogToken.token;
        pAddTs->status      = ( tSirMacStatusCodes )addts.Status.status;

        if ( addts.TSDelay.present )
        {
            ConvertTSDelay( pMac, &pAddTs->delay, &addts.TSDelay );
        }

        // TS Delay is present iff status indicates its presence
        if ( eSIR_MAC_TS_NOT_CREATED_STATUS == pAddTs->status && ! addts.TSDelay.present )
        {
            limLog( pMac, LOGW, FL("Missing TSDelay IE.") );
        }

        if ( addts.TSPEC.present )
        {
            ConvertTSPEC( pMac, &pAddTs->tspec, &addts.TSPEC );
        }
        else
        {
            limLog( pMac, LOGE, FL("Mandatory TSPEC element missing in Add TS Response.") );
            return eSIR_FAILURE;
        }

        if ( addts.num_TCLAS )
        {
            pAddTs->numTclas = (tANI_U8)addts.num_TCLAS;

            for ( i = 0U; i < addts.num_TCLAS; ++i )
            {
                if ( eSIR_SUCCESS != ConvertTCLAS( pMac, &( pAddTs->tclasInfo[i] ), &( addts.TCLAS[i] ) ) )
                {
                    limLog( pMac, LOGE, FL("Failed to convert a TCLAS IE.") );
                    return eSIR_FAILURE;
                }
            }
        }

        if ( addts.TCLASSPROC.present )
        {
            pAddTs->tclasProcPresent = 1;
            pAddTs->tclasProc = addts.TCLASSPROC.processing;
        }
#ifdef FEATURE_WLAN_ESE
        if(addts.ESETrafStrmMet.present)
        {
            pAddTs->tsmPresent = 1;
            vos_mem_copy(&pAddTs->tsmIE.tsid,
                      &addts.ESETrafStrmMet.tsid,sizeof(tSirMacESETSMIE));
        }
#endif
        if ( addts.Schedule.present )
        {
            pAddTs->schedulePresent = 1;
            ConvertSchedule( pMac, &pAddTs->schedule, &addts.Schedule );
        }

        if ( addts.WMMSchedule.present )
        {
            pAddTs->schedulePresent = 1;
            ConvertWMMSchedule( pMac, &pAddTs->schedule, &addts.WMMSchedule );
        }

        if ( addts.WMMTSPEC.present )
        {
            pAddTs->wsmTspecPresent = 1;
            ConvertWMMTSPEC( pMac, &pAddTs->tspec, &addts.WMMTSPEC );
        }

        if ( addts.num_WMMTCLAS )
        {
            j = (tANI_U8)(pAddTs->numTclas + addts.num_WMMTCLAS);
            if ( SIR_MAC_TCLASIE_MAXNUM > j ) j = SIR_MAC_TCLASIE_MAXNUM;

            for ( i = pAddTs->numTclas; i < j; ++i )
            {
                if ( eSIR_SUCCESS != ConvertWMMTCLAS( pMac, &( pAddTs->tclasInfo[i] ), &( addts.WMMTCLAS[i] ) ) )
                {
                    limLog( pMac, LOGE, FL("Failed to convert a TCLAS IE.") );
                    return eSIR_FAILURE;
                }
            }
        }

        if ( addts.WMMTCLASPROC.present )
        {
            pAddTs->tclasProcPresent = 1;
            pAddTs->tclasProc = addts.WMMTCLASPROC.processing;
        }

        if ( 1 < pAddTs->numTclas && ( ! pAddTs->tclasProcPresent ) )
        {
            limLog( pMac, LOGE, FL("%d TCLAS IE but not TCLASPROC IE."),
                       pAddTs->numTclas );
            return eSIR_FAILURE;
        }
    }
    else
    {
        pAddTs->dialogToken = wmmaddts.DialogToken.token;
        pAddTs->status      = ( tSirMacStatusCodes )wmmaddts.StatusCode.statusCode;

        if ( wmmaddts.WMMTSPEC.present )
        {
            pAddTs->wmeTspecPresent = 1;
            ConvertWMMTSPEC( pMac, &pAddTs->tspec, &wmmaddts.WMMTSPEC );
        }
        else
        {
            limLog( pMac, LOGE, FL("Mandatory WME TSPEC element missing!") );
            return eSIR_FAILURE;
        }

#ifdef FEATURE_WLAN_ESE
        if(wmmaddts.ESETrafStrmMet.present)
        {
            pAddTs->tsmPresent = 1;
            vos_mem_copy(&pAddTs->tsmIE.tsid,
                         &wmmaddts.ESETrafStrmMet.tsid,sizeof(tSirMacESETSMIE));
        }
#endif

    }

    return eSIR_SUCCESS;

} // End sirConvertAddtsRsp2Struct.

tSirRetStatus
sirConvertDeltsReq2Struct(tpAniSirGlobal    pMac,
                          tANI_U8               *pFrame,
                          tANI_U32               nFrame,
                          tSirDeltsReqInfo *pDelTs)
{
    tDot11fDelTS    delts = {{0}};
    tDot11fWMMDelTS wmmdelts = {{0}};
    tANI_U32             status;

    if ( SIR_MAC_QOS_DEL_TS_REQ != *( pFrame + 1 ) )
    {
        limLog( pMac, LOGE, FL("sirConvertDeltsRsp2Struct invoked "
                                  "with an Action of %d; this is not "
                                  "supported & is probably an error."),
                   *( pFrame + 1 ) );
        return eSIR_FAILURE;
    }

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pDelTs, sizeof(tSirDeltsReqInfo), 0 );

    // delegate to the framesc-generated code,
    switch ( *pFrame )
    {
    case SIR_MAC_ACTION_QOS_MGMT:
        status = dot11fUnpackDelTS( pMac, pFrame, nFrame, &delts );
        break;
    case SIR_MAC_ACTION_WME:
        status = dot11fUnpackWMMDelTS( pMac, pFrame, nFrame, &wmmdelts );
        break;
    default:
        limLog( pMac, LOGE, FL("sirConvertDeltsRsp2Struct invoked "
                                  "with a Category of %d; this is not"
                                  " supported & is probably an error."),
                   *pFrame );
        return eSIR_FAILURE;
    }

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an Del TS Request f"
                                 "rame (0x%08x, %d bytes)"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        dot11fLog( pMac, LOGW, FL("There were warnings while unpackin"
                                  "g an Del TS Request frame (0x%08x,"
                                  "%d bytes):"),
                   status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fDelTSResponse' or a
    // 'tDot11WMMDelTSResponse' to a 'tSirMacDeltsReqInfo'...
    if ( SIR_MAC_ACTION_QOS_MGMT == *pFrame )
    {
        pDelTs->tsinfo.traffic.trafficType  = (tANI_U16)delts.TSInfo.traffic_type;
        pDelTs->tsinfo.traffic.tsid         = (tANI_U16)delts.TSInfo.tsid;
        pDelTs->tsinfo.traffic.direction    = (tANI_U16)delts.TSInfo.direction;
        pDelTs->tsinfo.traffic.accessPolicy = (tANI_U16)delts.TSInfo.access_policy;
        pDelTs->tsinfo.traffic.aggregation  = (tANI_U16)delts.TSInfo.aggregation;
        pDelTs->tsinfo.traffic.psb          = (tANI_U16)delts.TSInfo.psb;
        pDelTs->tsinfo.traffic.userPrio     = (tANI_U16)delts.TSInfo.user_priority;
        pDelTs->tsinfo.traffic.ackPolicy    = (tANI_U16)delts.TSInfo.tsinfo_ack_pol;

        pDelTs->tsinfo.schedule.schedule    = (tANI_U8)delts.TSInfo.schedule;
    }
    else
    {
        if ( wmmdelts.WMMTSPEC.present )
        {
            pDelTs->wmeTspecPresent = 1;
            ConvertWMMTSPEC( pMac, &pDelTs->tspec, &wmmdelts.WMMTSPEC );
        }
        else
        {
            dot11fLog( pMac, LOGE, FL("Mandatory WME TSPEC element missing!") );
            return eSIR_FAILURE;
        }
    }

    return eSIR_SUCCESS;

} // End sirConvertDeltsReq2Struct.

tSirRetStatus
sirConvertQosMapConfigureFrame2Struct(tpAniSirGlobal    pMac,
                          tANI_U8               *pFrame,
                          tANI_U32               nFrame,
                          tSirQosMapSet      *pQosMapSet)
{
    tDot11fQosMapConfigure mapConfigure;
    tANI_U32 status;
    status = dot11fUnpackQosMapConfigure(pMac, pFrame, nFrame, &mapConfigure);
    if ( DOT11F_FAILED( status ) )
    {
        dot11fLog(pMac, LOGE, FL("Failed to parse Qos Map Configure frame (0x%08x, %d bytes):"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      dot11fLog( pMac, LOGW, FL("There were warnings while unpacking Qos Map Configure frame (0x%08x, %d bytes):"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }
    pQosMapSet->present = mapConfigure.QosMapSet.present;
    ConvertQosMapsetFrame(pMac->hHdd, pQosMapSet, &mapConfigure.QosMapSet);
    limLogQosMapSet(pMac, pQosMapSet);
    return eSIR_SUCCESS;
}

#ifdef ANI_SUPPORT_11H
tSirRetStatus
sirConvertTpcReqFrame2Struct(tpAniSirGlobal            pMac,
                             tANI_U8                       *pFrame,
                             tpSirMacTpcReqActionFrame pTpcReqFrame,
                             tANI_U32                       nFrame)
{
    tDot11fTPCRequest     req;
    tANI_U32                   status;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pTpcReqFrame, sizeof(tSirMacTpcReqActionFrame), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackTPCRequest( pMac, pFrame, nFrame, &req );
    if ( DOT11F_FAILED( status ) )
    {
        dot11fLog(pMac, LOGE, FL("Failed to parse a TPC Request frame (0x%08x, %d bytes):"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      dot11fLog( pMac, LOGW, FL("There were warnings while unpacking a TPC Request frame (0x%08x, %d bytes):"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fTPCRequest' to a
    // 'tSirMacTpcReqActionFrame'...
    pTpcReqFrame->actionHeader.category    = req.Category.category;
    pTpcReqFrame->actionHeader.actionID    = req.Action.action;
    pTpcReqFrame->actionHeader.dialogToken = req.DialogToken.token;
    if ( req.TPCRequest.present )
    {
        pTpcReqFrame->type   = DOT11F_EID_TPCREQUEST;
        pTpcReqFrame->length = 0;
    }
    else
    {
        dot11fLog( pMac, LOGW, FL("!!!Rcv TPC Req of inalid type!") );
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

} // End sirConvertTpcReqFrame2Struct.


tSirRetStatus
sirConvertMeasReqFrame2Struct(tpAniSirGlobal             pMac,
                              tANI_U8                        *pFrame,
                              tpSirMacMeasReqActionFrame pMeasReqFrame,
                              tANI_U32                        nFrame)
{
    tDot11fMeasurementRequest mr;
    tANI_U32                       status;

    // Zero-init our [out] parameter,
    vos_mem_set( ( tANI_U8* )pMeasReqFrame, sizeof(tpSirMacMeasReqActionFrame), 0 );

    // delegate to the framesc-generated code,
    status = dot11fUnpackMeasurementRequest( pMac, pFrame, nFrame, &mr );
    if ( DOT11F_FAILED( status ) )
    {
        dot11fLog(pMac, LOGE, FL("Failed to parse a Measurement Request frame (0x%08x, %d bytes):"),
                  status, nFrame);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
      dot11fLog( pMac, LOGW, FL("There were warnings while unpacking a Measurement Request frame (0x%08x, %d bytes)"),
                 status, nFrame );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }

    // & "transliterate" from a 'tDot11fMeasurementRequest' to a
    // 'tpSirMacMeasReqActionFrame'...
    pMeasReqFrame->actionHeader.category    = mr.Category.category;
    pMeasReqFrame->actionHeader.actionID    = mr.Action.action;
    pMeasReqFrame->actionHeader.dialogToken = mr.DialogToken.token;

    if ( 0 == mr.num_MeasurementRequest )
    {
        dot11fLog( pMac, LOGE, FL("Missing mandatory IE in Measurement Request Frame.") );
        return eSIR_FAILURE;
    }
    else if ( 1 < mr.num_MeasurementRequest )
    {
        limLog( pMac, LOGW, FL("Warning: dropping extra Measurement Request IEs!") );
    }

    pMeasReqFrame->measReqIE.type        = DOT11F_EID_MEASUREMENTREQUEST;
    pMeasReqFrame->measReqIE.length      = DOT11F_IE_MEASUREMENTREQUEST_MIN_LEN;
    pMeasReqFrame->measReqIE.measToken   = mr.MeasurementRequest[0].measurement_token;
    pMeasReqFrame->measReqIE.measReqMode = ( mr.MeasurementRequest[0].reserved << 3 ) |
                                           ( mr.MeasurementRequest[0].enable   << 2 ) |
                                           ( mr.MeasurementRequest[0].request  << 1 ) |
                                           ( mr.MeasurementRequest[0].report /*<< 0*/ );
    pMeasReqFrame->measReqIE.measType    = mr.MeasurementRequest[0].measurement_type;

    pMeasReqFrame->measReqIE.measReqField.channelNumber = mr.MeasurementRequest[0].channel_no;

    vos_mem_copy(  pMeasReqFrame->measReqIE.measReqField.measStartTime,
                   mr.MeasurementRequest[0].meas_start_time, 8 );

    pMeasReqFrame->measReqIE.measReqField.measDuration = mr.MeasurementRequest[0].meas_duration;

    return eSIR_SUCCESS;

} // End sirConvertMeasReqFrame2Struct.
#endif


void
PopulateDot11fTSPEC(tSirMacTspecIE  *pOld,
                    tDot11fIETSPEC  *pDot11f)
{
    pDot11f->traffic_type         = pOld->tsinfo.traffic.trafficType;
    pDot11f->tsid                 = pOld->tsinfo.traffic.tsid;
    pDot11f->direction            = pOld->tsinfo.traffic.direction;
    pDot11f->access_policy        = pOld->tsinfo.traffic.accessPolicy;
    pDot11f->aggregation          = pOld->tsinfo.traffic.aggregation;
    pDot11f->psb                  = pOld->tsinfo.traffic.psb;
    pDot11f->user_priority        = pOld->tsinfo.traffic.userPrio;
    pDot11f->tsinfo_ack_pol       = pOld->tsinfo.traffic.ackPolicy;
    pDot11f->schedule             = pOld->tsinfo.schedule.schedule;
    /* As defined in IEEE 802.11-2007, section 7.3.2.30
     * Nominal MSDU size: Bit[0:14]=Size, Bit[15]=Fixed
     */
    pDot11f->size                 = ( pOld->nomMsduSz & 0x7fff );
    pDot11f->fixed                = ( pOld->nomMsduSz & 0x8000 ) ? 1 : 0;
    pDot11f->max_msdu_size        = pOld->maxMsduSz;
    pDot11f->min_service_int      = pOld->minSvcInterval;
    pDot11f->max_service_int      = pOld->maxSvcInterval;
    pDot11f->inactivity_int       = pOld->inactInterval;
    pDot11f->suspension_int       = pOld->suspendInterval;
    pDot11f->service_start_time   = pOld->svcStartTime;
    pDot11f->min_data_rate        = pOld->minDataRate;
    pDot11f->mean_data_rate       = pOld->meanDataRate;
    pDot11f->peak_data_rate       = pOld->peakDataRate;
    pDot11f->burst_size           = pOld->maxBurstSz;
    pDot11f->delay_bound          = pOld->delayBound;
    pDot11f->min_phy_rate         = pOld->minPhyRate;
    pDot11f->surplus_bw_allowance = pOld->surplusBw;
    pDot11f->medium_time          = pOld->mediumTime;

    pDot11f->present = 1;

} // End PopulateDot11fTSPEC.

void
PopulateDot11fWMMTSPEC(tSirMacTspecIE     *pOld,
                       tDot11fIEWMMTSPEC  *pDot11f)
{
    pDot11f->traffic_type         = pOld->tsinfo.traffic.trafficType;
    pDot11f->tsid                 = pOld->tsinfo.traffic.tsid;
    pDot11f->direction            = pOld->tsinfo.traffic.direction;
    pDot11f->access_policy        = pOld->tsinfo.traffic.accessPolicy;
    pDot11f->aggregation          = pOld->tsinfo.traffic.aggregation;
    pDot11f->psb                  = pOld->tsinfo.traffic.psb;
    pDot11f->user_priority        = pOld->tsinfo.traffic.userPrio;
    pDot11f->tsinfo_ack_pol       = pOld->tsinfo.traffic.ackPolicy;
    pDot11f->burst_size_defn      = pOld->tsinfo.traffic.burstSizeDefn;
    /* As defined in IEEE 802.11-2007, section 7.3.2.30
     * Nominal MSDU size: Bit[0:14]=Size, Bit[15]=Fixed
     */
    pDot11f->size                 = ( pOld->nomMsduSz & 0x7fff );
    pDot11f->fixed                = ( pOld->nomMsduSz & 0x8000 ) ? 1 : 0;
    pDot11f->max_msdu_size        = pOld->maxMsduSz;
    pDot11f->min_service_int      = pOld->minSvcInterval;
    pDot11f->max_service_int      = pOld->maxSvcInterval;
    pDot11f->inactivity_int       = pOld->inactInterval;
    pDot11f->suspension_int       = pOld->suspendInterval;
    pDot11f->service_start_time   = pOld->svcStartTime;
    pDot11f->min_data_rate        = pOld->minDataRate;
    pDot11f->mean_data_rate       = pOld->meanDataRate;
    pDot11f->peak_data_rate       = pOld->peakDataRate;
    pDot11f->burst_size           = pOld->maxBurstSz;
    pDot11f->delay_bound          = pOld->delayBound;
    pDot11f->min_phy_rate         = pOld->minPhyRate;
    pDot11f->surplus_bw_allowance = pOld->surplusBw;
    pDot11f->medium_time          = pOld->mediumTime;

    pDot11f->version = 1;
    pDot11f->present = 1;

} // End PopulateDot11fWMMTSPEC.

#if defined(FEATURE_WLAN_ESE)

// Fill the ESE version currently supported
void PopulateDot11fESEVersion(tDot11fIEESEVersion *pESEVersion)
{
    pESEVersion->present = 1;
    pESEVersion->version = ESE_VERSION_SUPPORTED;
}

// Fill the ESE ie for the station.
// The State is Normal (1)
// The MBSSID for station is set to 0.
void PopulateDot11fESERadMgmtCap(tDot11fIEESERadMgmtCap *pESERadMgmtCap)
{
    pESERadMgmtCap->present = 1;
    pESERadMgmtCap->mgmt_state = RM_STATE_NORMAL;
    pESERadMgmtCap->mbssid_mask = 0;
    pESERadMgmtCap->reserved = 0;
}

tSirRetStatus PopulateDot11fESECckmOpaque( tpAniSirGlobal pMac,
                                           tpSirCCKMie    pCCKMie,
                                           tDot11fIEESECckmOpaque *pDot11f )
{
    int idx;

    if ( pCCKMie->length )
    {
        if( 0 <= ( idx = FindIELocation( pMac, (tpSirRSNie)pCCKMie, DOT11F_EID_ESECCKMOPAQUE ) ) )
        {
            pDot11f->present  = 1;
            pDot11f->num_data = pCCKMie->cckmIEdata[ idx + 1 ] - 4; // Dont include OUI
            vos_mem_copy(pDot11f->data,
                           pCCKMie->cckmIEdata + idx + 2 + 4,    // EID, len, OUI
                           pCCKMie->cckmIEdata[ idx + 1 ] - 4 ); // Skip OUI
        }
    }

    return eSIR_SUCCESS;

} // End PopulateDot11fESECckmOpaque.

void PopulateDot11TSRSIE(tpAniSirGlobal  pMac,
                               tSirMacESETSRSIE     *pOld,
                               tDot11fIEESETrafStrmRateSet  *pDot11f,
                               tANI_U8 rate_length)
{
    pDot11f->tsid = pOld->tsid;
    vos_mem_copy(pDot11f->tsrates, pOld->rates,rate_length);
    pDot11f->num_tsrates = rate_length;
    pDot11f->present = 1;
}
#endif


tSirRetStatus
PopulateDot11fTCLAS(tpAniSirGlobal  pMac,
                    tSirTclasInfo  *pOld,
                    tDot11fIETCLAS *pDot11f)
{
    pDot11f->user_priority   = pOld->tclas.userPrio;
    pDot11f->classifier_type = pOld->tclas.classifierType;
    pDot11f->classifier_mask = pOld->tclas.classifierMask;

    switch ( pDot11f->classifier_type )
    {
    case SIR_MAC_TCLASTYPE_ETHERNET:
        vos_mem_copy( ( tANI_U8* )&pDot11f->info.EthParams.source,
                      ( tANI_U8* )&pOld->tclasParams.eth.srcAddr, 6 );
        vos_mem_copy( ( tANI_U8* )&pDot11f->info.EthParams.dest,
                      ( tANI_U8* )&pOld->tclasParams.eth.dstAddr, 6 );
        pDot11f->info.EthParams.type = pOld->tclasParams.eth.type;
        break;
    case SIR_MAC_TCLASTYPE_TCPUDPIP:
        pDot11f->info.IpParams.version = pOld->version;
        if ( SIR_MAC_TCLAS_IPV4 == pDot11f->info.IpParams.version )
        {
            vos_mem_copy( pDot11f->info.IpParams.params.
                          IpV4Params.source,
                          pOld->tclasParams.ipv4.srcIpAddr, 4 );
            vos_mem_copy( pDot11f->info.IpParams.params.
                          IpV4Params.dest,
                          pOld->tclasParams.ipv4.dstIpAddr, 4 );
            pDot11f->info.IpParams.params.IpV4Params.src_port  =
              pOld->tclasParams.ipv4.srcPort;
            pDot11f->info.IpParams.params.IpV4Params.dest_port =
              pOld->tclasParams.ipv4.dstPort;
            pDot11f->info.IpParams.params.IpV4Params.DSCP      =
              pOld->tclasParams.ipv4.dscp;
            pDot11f->info.IpParams.params.IpV4Params.proto     =
              pOld->tclasParams.ipv4.protocol;
            pDot11f->info.IpParams.params.IpV4Params.reserved  =
              pOld->tclasParams.ipv4.rsvd;
        }
        else
        {
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.source,
                           ( tANI_U8* )pOld->tclasParams.ipv6.srcIpAddr, 16 );
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.dest,
                           ( tANI_U8* )pOld->tclasParams.ipv6.dstIpAddr, 16 );
            pDot11f->info.IpParams.params.IpV6Params.src_port  =
              pOld->tclasParams.ipv6.srcPort;
            pDot11f->info.IpParams.params.IpV6Params.dest_port =
              pOld->tclasParams.ipv6.dstPort;
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.flow_label,
                           ( tANI_U8* )pOld->tclasParams.ipv6.flowLabel, 3 );
        }
        break;
    case SIR_MAC_TCLASTYPE_8021DQ:
        pDot11f->info.Params8021dq.tag_type = pOld->tclasParams.t8021dq.tag;
        break;
    default:
        limLog( pMac, LOGE, FL("Bad TCLAS type %d in PopulateDot11fTCLAS."),
                pDot11f->classifier_type );
        return eSIR_FAILURE;
    }

    pDot11f->present = 1;

    return eSIR_SUCCESS;

} // End PopulateDot11fTCLAS.

tSirRetStatus
PopulateDot11fWMMTCLAS(tpAniSirGlobal     pMac,
                       tSirTclasInfo     *pOld,
                       tDot11fIEWMMTCLAS *pDot11f)
{
    pDot11f->version         = 1;
    pDot11f->user_priority   = pOld->tclas.userPrio;
    pDot11f->classifier_type = pOld->tclas.classifierType;
    pDot11f->classifier_mask = pOld->tclas.classifierMask;

    switch ( pDot11f->classifier_type )
    {
    case SIR_MAC_TCLASTYPE_ETHERNET:
        vos_mem_copy(  ( tANI_U8* )&pDot11f->info.EthParams.source,
                       ( tANI_U8* )&pOld->tclasParams.eth.srcAddr, 6 );
        vos_mem_copy(  ( tANI_U8* )&pDot11f->info.EthParams.dest,
                       ( tANI_U8* )&pOld->tclasParams.eth.dstAddr, 6 );
        pDot11f->info.EthParams.type = pOld->tclasParams.eth.type;
        break;
    case SIR_MAC_TCLASTYPE_TCPUDPIP:
        pDot11f->info.IpParams.version = pOld->version;
        if ( SIR_MAC_TCLAS_IPV4 == pDot11f->info.IpParams.version )
        {
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV4Params.source,
                           ( tANI_U8* )pOld->tclasParams.ipv4.srcIpAddr, 4 );
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV4Params.dest,
                           ( tANI_U8* )pOld->tclasParams.ipv4.dstIpAddr, 4 );
            pDot11f->info.IpParams.params.IpV4Params.src_port  =
              pOld->tclasParams.ipv4.srcPort;
            pDot11f->info.IpParams.params.IpV4Params.dest_port =
              pOld->tclasParams.ipv4.dstPort;
            pDot11f->info.IpParams.params.IpV4Params.DSCP      =
              pOld->tclasParams.ipv4.dscp;
            pDot11f->info.IpParams.params.IpV4Params.proto     =
              pOld->tclasParams.ipv4.protocol;
            pDot11f->info.IpParams.params.IpV4Params.reserved  =
              pOld->tclasParams.ipv4.rsvd;
        }
        else
        {
            vos_mem_copy( ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.source,
                           ( tANI_U8* )pOld->tclasParams.ipv6.srcIpAddr, 16 );
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.dest,
                           ( tANI_U8* )pOld->tclasParams.ipv6.dstIpAddr, 16 );
            pDot11f->info.IpParams.params.IpV6Params.src_port  =
              pOld->tclasParams.ipv6.srcPort;
            pDot11f->info.IpParams.params.IpV6Params.dest_port =
              pOld->tclasParams.ipv6.dstPort;
            vos_mem_copy(  ( tANI_U8* )&pDot11f->info.IpParams.params.
                           IpV6Params.flow_label,
                           ( tANI_U8* )pOld->tclasParams.ipv6.flowLabel, 3 );
        }
        break;
    case SIR_MAC_TCLASTYPE_8021DQ:
        pDot11f->info.Params8021dq.tag_type = pOld->tclasParams.t8021dq.tag;
        break;
    default:
        limLog( pMac, LOGE, FL("Bad TCLAS type %d in PopulateDot11fTCLAS."),
                pDot11f->classifier_type );
        return eSIR_FAILURE;
    }

    pDot11f->present = 1;

    return eSIR_SUCCESS;

} // End PopulateDot11fWMMTCLAS.


tSirRetStatus PopulateDot11fWsc(tpAniSirGlobal pMac,
                                tDot11fIEWscBeacon *pDot11f)
{

    tANI_U32 wpsState;

    pDot11f->Version.present = 1;
    pDot11f->Version.major = 0x01;
    pDot11f->Version.minor = 0x00;

    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_STATE, &wpsState) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_STATE );

    pDot11f->WPSState.present = 1;
    pDot11f->WPSState.state = (tANI_U8) wpsState;

    pDot11f->APSetupLocked.present = 0;

    pDot11f->SelectedRegistrar.present = 0;

    pDot11f->DevicePasswordID.present = 0;

    pDot11f->SelectedRegistrarConfigMethods.present = 0;

    pDot11f->UUID_E.present = 0;

    pDot11f->RFBands.present = 0;

    pDot11f->present = 1;
    return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11fWscRegistrarInfo(tpAniSirGlobal pMac,
                                             tDot11fIEWscBeacon *pDot11f)
{
    const struct sLimWscIeInfo *const pWscIeInfo = &(pMac->lim.wscIeInfo);
    tANI_U32 devicepasswdId;


    pDot11f->APSetupLocked.present = 1;
    pDot11f->APSetupLocked.fLocked = pWscIeInfo->apSetupLocked;

    pDot11f->SelectedRegistrar.present = 1;
    pDot11f->SelectedRegistrar.selected = pWscIeInfo->selectedRegistrar;

    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_DEVICE_PASSWORD_ID, &devicepasswdId) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_DEVICE_PASSWORD_ID );

    pDot11f->DevicePasswordID.present = 1;
    pDot11f->DevicePasswordID.id = (tANI_U16) devicepasswdId;

    pDot11f->SelectedRegistrarConfigMethods.present = 1;
    pDot11f->SelectedRegistrarConfigMethods.methods = pWscIeInfo->selectedRegistrarConfigMethods;

    // UUID_E and RF Bands are applicable only for dual band AP

    return eSIR_SUCCESS;
}

tSirRetStatus DePopulateDot11fWscRegistrarInfo(tpAniSirGlobal pMac,
                                               tDot11fIEWscBeacon *pDot11f)
{
    pDot11f->APSetupLocked.present = 0;
    pDot11f->SelectedRegistrar.present = 0;
    pDot11f->DevicePasswordID.present = 0;
    pDot11f->SelectedRegistrarConfigMethods.present = 0;

    return eSIR_SUCCESS;
}
tSirRetStatus PopulateDot11fProbeResWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscProbeRes *pDot11f, tpPESession psessionEntry)
{
 
   tSirWPSProbeRspIE *pSirWPSProbeRspIE;

   pSirWPSProbeRspIE = &psessionEntry->APWPSIEs.SirWPSProbeRspIE;
   
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_VER_PRESENT)
    {
        pDot11f->present = 1;
        pDot11f->Version.present = 1;
        pDot11f->Version.major = (tANI_U8) ((pSirWPSProbeRspIE->Version & 0xF0)>>4);
        pDot11f->Version.minor = (tANI_U8) (pSirWPSProbeRspIE->Version & 0x0F);
    }
    else
    {
        pDot11f->present = 0;
        pDot11f->Version.present = 0;
    }

    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_STATE_PRESENT)
    {
        
        pDot11f->WPSState.present = 1;
        pDot11f->WPSState.state = (tANI_U8)pSirWPSProbeRspIE->wpsState;
    }
    else
        pDot11f->WPSState.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_APSETUPLOCK_PRESENT)
    {
        pDot11f->APSetupLocked.present = 1;
        pDot11f->APSetupLocked.fLocked = pSirWPSProbeRspIE->APSetupLocked;
    }
    else
        pDot11f->APSetupLocked.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT)
    {
        pDot11f->SelectedRegistrar.present = 1;
        pDot11f->SelectedRegistrar.selected = pSirWPSProbeRspIE->SelectedRegistra;
    }
    else
         pDot11f->SelectedRegistrar.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_DEVICEPASSWORDID_PRESENT)
    {
        pDot11f->DevicePasswordID.present = 1;
        pDot11f->DevicePasswordID.id = pSirWPSProbeRspIE->DevicePasswordID;
    }
    else
        pDot11f->DevicePasswordID.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT)
    {
        pDot11f->SelectedRegistrarConfigMethods.present = 1;
        pDot11f->SelectedRegistrarConfigMethods.methods = pSirWPSProbeRspIE->SelectedRegistraCfgMethod;
    }
    else
        pDot11f->SelectedRegistrarConfigMethods.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_RESPONSETYPE_PRESENT)
    {
        pDot11f->ResponseType.present = 1;
        pDot11f->ResponseType.resType = pSirWPSProbeRspIE->ResponseType;
    }
    else
        pDot11f->ResponseType.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_UUIDE_PRESENT)
    {
        pDot11f->UUID_E.present = 1;
        vos_mem_copy(pDot11f->UUID_E.uuid, pSirWPSProbeRspIE->UUID_E, WNI_CFG_WPS_UUID_LEN);
    }
    else
        pDot11f->UUID_E.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_MANUFACTURE_PRESENT)
    {
        pDot11f->Manufacturer.present = 1;
        pDot11f->Manufacturer.num_name = pSirWPSProbeRspIE->Manufacture.num_name;
        vos_mem_copy(pDot11f->Manufacturer.name, pSirWPSProbeRspIE->Manufacture.name,
                     pSirWPSProbeRspIE->Manufacture.num_name);
    } 
    else
        pDot11f->Manufacturer.present = 0;
        
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_MODELNUMBER_PRESENT)
    {
        pDot11f->ModelName.present = 1;
        pDot11f->ModelName.num_text = pSirWPSProbeRspIE->ModelName.num_text;
        vos_mem_copy(pDot11f->ModelName.text, pSirWPSProbeRspIE->ModelName.text,
                     pDot11f->ModelName.num_text);
    }
    else
      pDot11f->ModelName.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_MODELNUMBER_PRESENT)
    {
        pDot11f->ModelNumber.present = 1;
        pDot11f->ModelNumber.num_text = pSirWPSProbeRspIE->ModelNumber.num_text;
        vos_mem_copy(pDot11f->ModelNumber.text, pSirWPSProbeRspIE->ModelNumber.text,
                     pDot11f->ModelNumber.num_text);
    }
    else
        pDot11f->ModelNumber.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_SERIALNUMBER_PRESENT)
    {
        pDot11f->SerialNumber.present = 1;
        pDot11f->SerialNumber.num_text = pSirWPSProbeRspIE->SerialNumber.num_text;
        vos_mem_copy(pDot11f->SerialNumber.text, pSirWPSProbeRspIE->SerialNumber.text,
                     pDot11f->SerialNumber.num_text);
    }
    else
        pDot11f->SerialNumber.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT)
    {
        pDot11f->PrimaryDeviceType.present = 1;
        vos_mem_copy(pDot11f->PrimaryDeviceType.oui, pSirWPSProbeRspIE->PrimaryDeviceOUI,
                     sizeof(pSirWPSProbeRspIE->PrimaryDeviceOUI));
        pDot11f->PrimaryDeviceType.primary_category = (tANI_U16)pSirWPSProbeRspIE->PrimaryDeviceCategory;
        pDot11f->PrimaryDeviceType.sub_category = (tANI_U16)pSirWPSProbeRspIE->DeviceSubCategory;
    }
    else
        pDot11f->PrimaryDeviceType.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_DEVICENAME_PRESENT)
    {
        pDot11f->DeviceName.present = 1;
        pDot11f->DeviceName.num_text = pSirWPSProbeRspIE->DeviceName.num_text;
        vos_mem_copy(pDot11f->DeviceName.text, pSirWPSProbeRspIE->DeviceName.text,
                     pDot11f->DeviceName.num_text);
    }
    else
        pDot11f->DeviceName.present = 0;
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_CONFIGMETHODS_PRESENT)
    {
        pDot11f->ConfigMethods.present = 1;
        pDot11f->ConfigMethods.methods = pSirWPSProbeRspIE->ConfigMethod;
    }
    else
        pDot11f->ConfigMethods.present = 0;
    
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_RF_BANDS_PRESENT)
    {
        pDot11f->RFBands.present = 1;
        pDot11f->RFBands.bands = pSirWPSProbeRspIE->RFBand;
    }
    else
       pDot11f->RFBands.present = 0;      
       
    return eSIR_SUCCESS;
}
tSirRetStatus PopulateDot11fAssocResWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscAssocRes *pDot11f, tpPESession psessionEntry)
{
   tSirWPSProbeRspIE *pSirWPSProbeRspIE;

   pSirWPSProbeRspIE = &psessionEntry->APWPSIEs.SirWPSProbeRspIE;
   
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_VER_PRESENT)
    {
        pDot11f->present = 1;
        pDot11f->Version.present = 1;
        pDot11f->Version.major = (tANI_U8) ((pSirWPSProbeRspIE->Version & 0xF0)>>4);
        pDot11f->Version.minor = (tANI_U8) (pSirWPSProbeRspIE->Version & 0x0F);
    }
    else
    {
        pDot11f->present = 0;
        pDot11f->Version.present = 0;
    }
    
    if(pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_RESPONSETYPE_PRESENT)
    {
        pDot11f->ResponseType.present = 1;
        pDot11f->ResponseType.resType = pSirWPSProbeRspIE->ResponseType;
    }
    else
        pDot11f->ResponseType.present = 0;    

    return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11fBeaconWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscBeacon *pDot11f, tpPESession psessionEntry)
{
 
   tSirWPSBeaconIE *pSirWPSBeaconIE;

   pSirWPSBeaconIE = &psessionEntry->APWPSIEs.SirWPSBeaconIE;
   
    
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_PROBRSP_VER_PRESENT)
    {
        pDot11f->present = 1;
        pDot11f->Version.present = 1;
        pDot11f->Version.major = (tANI_U8) ((pSirWPSBeaconIE->Version & 0xF0)>>4);
        pDot11f->Version.minor = (tANI_U8) (pSirWPSBeaconIE->Version & 0x0F);
    }
    else
    {
        pDot11f->present = 0;
        pDot11f->Version.present = 0;
    }
    
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_STATE_PRESENT)
    {
        
        pDot11f->WPSState.present = 1;
        pDot11f->WPSState.state = (tANI_U8)pSirWPSBeaconIE->wpsState;
    }
    else
        pDot11f->WPSState.present = 0;
        
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_APSETUPLOCK_PRESENT)
    {
        pDot11f->APSetupLocked.present = 1;
        pDot11f->APSetupLocked.fLocked = pSirWPSBeaconIE->APSetupLocked;
    }
    else
        pDot11f->APSetupLocked.present = 0;
        
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_SELECTEDREGISTRA_PRESENT)
    {
        pDot11f->SelectedRegistrar.present = 1;
        pDot11f->SelectedRegistrar.selected = pSirWPSBeaconIE->SelectedRegistra;
    }
    else
         pDot11f->SelectedRegistrar.present = 0;
    
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_DEVICEPASSWORDID_PRESENT)
    {
        pDot11f->DevicePasswordID.present = 1;
        pDot11f->DevicePasswordID.id = pSirWPSBeaconIE->DevicePasswordID;
    }
    else
        pDot11f->DevicePasswordID.present = 0;
        
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT)
    {
        pDot11f->SelectedRegistrarConfigMethods.present = 1;
        pDot11f->SelectedRegistrarConfigMethods.methods = pSirWPSBeaconIE->SelectedRegistraCfgMethod;
    }
    else
        pDot11f->SelectedRegistrarConfigMethods.present = 0;
        
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_UUIDE_PRESENT)
    {
        pDot11f->UUID_E.present = 1;
        vos_mem_copy(pDot11f->UUID_E.uuid, pSirWPSBeaconIE->UUID_E, WNI_CFG_WPS_UUID_LEN);
    }
    else
        pDot11f->UUID_E.present = 0;
    
       
    if(pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_RF_BANDS_PRESENT)
    {
        pDot11f->RFBands.present = 1;
        pDot11f->RFBands.bands = pSirWPSBeaconIE->RFBand;
    }
    else
       pDot11f->RFBands.present = 0;      
       
    return eSIR_SUCCESS;
}
tSirRetStatus PopulateDot11fWscInProbeRes(tpAniSirGlobal pMac,
                                          tDot11fIEWscProbeRes *pDot11f)
{
    tANI_U32 cfgMethods;
    tANI_U32 cfgStrLen;
    tANI_U32 val;
    tANI_U32 wpsVersion, wpsState;


    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_VERSION, &wpsVersion) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_VERSION );

    pDot11f->Version.present = 1;
    pDot11f->Version.major = (tANI_U8) ((wpsVersion & 0xF0)>>4);
    pDot11f->Version.minor = (tANI_U8) (wpsVersion & 0x0F);

    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_STATE, &wpsState) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_STATE );

    pDot11f->WPSState.present = 1;
    pDot11f->WPSState.state = (tANI_U8) wpsState;

    pDot11f->APSetupLocked.present = 0;

    pDot11f->SelectedRegistrar.present = 0;

    pDot11f->DevicePasswordID.present = 0;

    pDot11f->SelectedRegistrarConfigMethods.present = 0;

    pDot11f->ResponseType.present = 1;
    if ((pMac->lim.wscIeInfo.reqType == REQ_TYPE_REGISTRAR) ||
        (pMac->lim.wscIeInfo.reqType == REQ_TYPE_WLAN_MANAGER_REGISTRAR)){
        pDot11f->ResponseType.resType = RESP_TYPE_ENROLLEE_OPEN_8021X;
    }
    else{
         pDot11f->ResponseType.resType = RESP_TYPE_AP;
    }

    /* UUID is a 16 byte long binary. Still use wlan_cfgGetStr to get it. */
    pDot11f->UUID_E.present = 1;
    cfgStrLen = WNI_CFG_WPS_UUID_LEN;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_WPS_UUID,
                  pDot11f->UUID_E.uuid,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        *(pDot11f->UUID_E.uuid) = '\0';
    }

    pDot11f->Manufacturer.present = 1;
    cfgStrLen = WNI_CFG_MANUFACTURER_NAME_LEN - 1;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_MANUFACTURER_NAME,
                  pDot11f->Manufacturer.name,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        pDot11f->Manufacturer.num_name = 0;
        *(pDot11f->Manufacturer.name) = '\0';
    }
    else
    {
        pDot11f->Manufacturer.num_name = (tANI_U8) (cfgStrLen & 0x000000FF);
        pDot11f->Manufacturer.name[cfgStrLen - 1] = '\0';
    }

    pDot11f->ModelName.present = 1;
    cfgStrLen = WNI_CFG_MODEL_NAME_LEN - 1;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_MODEL_NAME,
                  pDot11f->ModelName.text,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        pDot11f->ModelName.num_text = 0;
        *(pDot11f->ModelName.text) = '\0';
    }
    else
    {
        pDot11f->ModelName.num_text = (tANI_U8) (cfgStrLen & 0x000000FF);
        pDot11f->ModelName.text[cfgStrLen - 1] = '\0';
    }

    pDot11f->ModelNumber.present = 1;
    cfgStrLen = WNI_CFG_MODEL_NUMBER_LEN - 1;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_MODEL_NUMBER,
                  pDot11f->ModelNumber.text,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        pDot11f->ModelNumber.num_text = 0;
        *(pDot11f->ModelNumber.text) = '\0';
    }
    else
    {
        pDot11f->ModelNumber.num_text = (tANI_U8) (cfgStrLen & 0x000000FF);
        pDot11f->ModelNumber.text[cfgStrLen - 1] = '\0';
    }

    pDot11f->SerialNumber.present = 1;
    cfgStrLen = WNI_CFG_MANUFACTURER_PRODUCT_VERSION_LEN - 1;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_MANUFACTURER_PRODUCT_VERSION,
                  pDot11f->SerialNumber.text,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        pDot11f->SerialNumber.num_text = 0;
        *(pDot11f->SerialNumber.text) = '\0';
    }
    else
    {
        pDot11f->SerialNumber.num_text = (tANI_U8) (cfgStrLen & 0x000000FF);
        pDot11f->SerialNumber.text[cfgStrLen - 1] = '\0';
    }

    pDot11f->PrimaryDeviceType.present = 1;

    if (wlan_cfgGetInt(pMac, WNI_CFG_WPS_PRIMARY_DEVICE_CATEGORY, &val) != eSIR_SUCCESS)
    {
       limLog(pMac, LOGP, FL("cfg get prim device category failed"));
    }
    else
       pDot11f->PrimaryDeviceType.primary_category = (tANI_U16) val;

    if (wlan_cfgGetInt(pMac, WNI_CFG_WPS_PIMARY_DEVICE_OUI, &val) != eSIR_SUCCESS)
    {
       limLog(pMac, LOGP, FL("cfg get prim device OUI failed"));
    }
    else
    {
       *(pDot11f->PrimaryDeviceType.oui) = (tANI_U8)((val >> 24)& 0xff);
       *(pDot11f->PrimaryDeviceType.oui+1) = (tANI_U8)((val >> 16)& 0xff);
       *(pDot11f->PrimaryDeviceType.oui+2) = (tANI_U8)((val >> 8)& 0xff);
       *(pDot11f->PrimaryDeviceType.oui+3) = (tANI_U8)((val & 0xff));
    }

    if (wlan_cfgGetInt(pMac, WNI_CFG_WPS_DEVICE_SUB_CATEGORY, &val) != eSIR_SUCCESS)
    {
       limLog(pMac, LOGP, FL("cfg get prim device sub category failed"));
    }
    else
       pDot11f->PrimaryDeviceType.sub_category = (tANI_U16) val;

    pDot11f->DeviceName.present = 1;
    cfgStrLen = WNI_CFG_MANUFACTURER_PRODUCT_NAME_LEN - 1;
    if (wlan_cfgGetStr(pMac,
                  WNI_CFG_MANUFACTURER_PRODUCT_NAME,
                  pDot11f->DeviceName.text,
                  &cfgStrLen) != eSIR_SUCCESS)
    {
        pDot11f->DeviceName.num_text = 0;
        *(pDot11f->DeviceName.text) = '\0';
    }
    else
    {
        pDot11f->DeviceName.num_text = (tANI_U8) (cfgStrLen & 0x000000FF);
        pDot11f->DeviceName.text[cfgStrLen - 1] = '\0';
    }

    if (wlan_cfgGetInt(pMac,
                  WNI_CFG_WPS_CFG_METHOD,
                  &cfgMethods) != eSIR_SUCCESS)
    {
        pDot11f->ConfigMethods.present = 0;
        pDot11f->ConfigMethods.methods = 0;
    }
    else
    {
        pDot11f->ConfigMethods.present = 1;
        pDot11f->ConfigMethods.methods = (tANI_U16) (cfgMethods & 0x0000FFFF);
    }

    pDot11f->RFBands.present = 0;

    pDot11f->present = 1;
    return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11fWscRegistrarInfoInProbeRes(tpAniSirGlobal pMac,
                                                       tDot11fIEWscProbeRes *pDot11f)
{
    const struct sLimWscIeInfo *const pWscIeInfo = &(pMac->lim.wscIeInfo);
    tANI_U32 devicepasswdId;

    pDot11f->APSetupLocked.present = 1;
    pDot11f->APSetupLocked.fLocked = pWscIeInfo->apSetupLocked;

    pDot11f->SelectedRegistrar.present = 1;
    pDot11f->SelectedRegistrar.selected = pWscIeInfo->selectedRegistrar;

    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_DEVICE_PASSWORD_ID, &devicepasswdId) != eSIR_SUCCESS)
       limLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_DEVICE_PASSWORD_ID );

    pDot11f->DevicePasswordID.present = 1;
    pDot11f->DevicePasswordID.id = (tANI_U16) devicepasswdId;

    pDot11f->SelectedRegistrarConfigMethods.present = 1;
    pDot11f->SelectedRegistrarConfigMethods.methods = pWscIeInfo->selectedRegistrarConfigMethods;

    // UUID_E and RF Bands are applicable only for dual band AP

    return eSIR_SUCCESS;
}

tSirRetStatus DePopulateDot11fWscRegistrarInfoInProbeRes(tpAniSirGlobal pMac,
                                                         tDot11fIEWscProbeRes *pDot11f)
{
    pDot11f->APSetupLocked.present = 0;
    pDot11f->SelectedRegistrar.present = 0;
    pDot11f->DevicePasswordID.present = 0;
    pDot11f->SelectedRegistrarConfigMethods.present = 0;

    return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11fAssocResWscIE(tpAniSirGlobal pMac, 
                                          tDot11fIEWscAssocRes *pDot11f, 
                                          tpSirAssocReq pRcvdAssocReq)
{
    tDot11fIEWscAssocReq parsedWscAssocReq = { 0, };
    tANI_U8         *wscIe;
    

    wscIe = limGetWscIEPtr(pMac, pRcvdAssocReq->addIE.addIEdata, pRcvdAssocReq->addIE.length);
    if(wscIe != NULL)
    {
        // retreive WSC IE from given AssocReq 
        dot11fUnpackIeWscAssocReq( pMac,
                                    wscIe + 2 + 4,  // EID, length, OUI
                                    wscIe[ 1 ] - 4, // length without OUI
                                    &parsedWscAssocReq );
        pDot11f->present = 1;
        // version has to be 0x10
        pDot11f->Version.present = 1;
        pDot11f->Version.major = 0x1;
        pDot11f->Version.minor = 0x0;

        pDot11f->ResponseType.present = 1;
        
        if ((parsedWscAssocReq.RequestType.reqType == REQ_TYPE_REGISTRAR) ||
            (parsedWscAssocReq.RequestType.reqType == REQ_TYPE_WLAN_MANAGER_REGISTRAR))
        {
            pDot11f->ResponseType.resType = RESP_TYPE_ENROLLEE_OPEN_8021X;
        }
        else
        {
             pDot11f->ResponseType.resType = RESP_TYPE_AP;
        }
        // Version infomration should be taken from our capability as well as peers
        // TODO: currently it takes from peers only
        if(parsedWscAssocReq.VendorExtension.present &&
           parsedWscAssocReq.VendorExtension.Version2.present)
        {   
            pDot11f->VendorExtension.present = 1;
            pDot11f->VendorExtension.vendorId[0] = 0x00;
            pDot11f->VendorExtension.vendorId[1] = 0x37;
            pDot11f->VendorExtension.vendorId[2] = 0x2A;
            pDot11f->VendorExtension.Version2.present = 1;
            pDot11f->VendorExtension.Version2.major = parsedWscAssocReq.VendorExtension.Version2.major;
            pDot11f->VendorExtension.Version2.minor = parsedWscAssocReq.VendorExtension.Version2.minor;
        }
    }
    return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11AssocResP2PIE(tpAniSirGlobal pMac, 
                                       tDot11fIEP2PAssocRes *pDot11f, 
                                       tpSirAssocReq pRcvdAssocReq)
{
    tANI_U8         *p2pIe;

    p2pIe = limGetP2pIEPtr(pMac, pRcvdAssocReq->addIE.addIEdata, pRcvdAssocReq->addIE.length);
    if(p2pIe != NULL)
    {
        pDot11f->present = 1;
        pDot11f->P2PStatus.present = 1;
        pDot11f->P2PStatus.status = eSIR_SUCCESS;
        pDot11f->ExtendedListenTiming.present = 0;
    }
    return eSIR_SUCCESS;
}

#if defined WLAN_FEATURE_VOWIFI

tSirRetStatus PopulateDot11fWFATPC( tpAniSirGlobal        pMac,
                                    tDot11fIEWFATPC *pDot11f, tANI_U8 txPower, tANI_U8 linkMargin )
{
     pDot11f->txPower = txPower;
     pDot11f->linkMargin = linkMargin;
     pDot11f->present = 1;

     return eSIR_SUCCESS;
}

tSirRetStatus PopulateDot11fBeaconReport( tpAniSirGlobal pMac, tDot11fIEMeasurementReport *pDot11f, tSirMacBeaconReport *pBeaconReport )
{

     pDot11f->report.Beacon.regClass = pBeaconReport->regClass;
     pDot11f->report.Beacon.channel = pBeaconReport->channel;
     vos_mem_copy( pDot11f->report.Beacon.meas_start_time, pBeaconReport->measStartTime,
                   sizeof(pDot11f->report.Beacon.meas_start_time) );
     pDot11f->report.Beacon.meas_duration = pBeaconReport->measDuration;
     pDot11f->report.Beacon.condensed_PHY = pBeaconReport->phyType;
     pDot11f->report.Beacon.reported_frame_type = !pBeaconReport->bcnProbeRsp;
     pDot11f->report.Beacon.RCPI = pBeaconReport->rcpi;
     pDot11f->report.Beacon.RSNI = pBeaconReport->rsni;
     vos_mem_copy( pDot11f->report.Beacon.BSSID, pBeaconReport->bssid, sizeof(tSirMacAddr));
     pDot11f->report.Beacon.antenna_id = pBeaconReport->antennaId;
     pDot11f->report.Beacon.parent_TSF = pBeaconReport->parentTSF;

     if( pBeaconReport->numIes )
     {
          pDot11f->report.Beacon.BeaconReportFrmBody.present = 1;
          vos_mem_copy( pDot11f->report.Beacon.BeaconReportFrmBody.reportedFields,
                        pBeaconReport->Ies, pBeaconReport->numIes );
          pDot11f->report.Beacon.BeaconReportFrmBody.num_reportedFields = pBeaconReport->numIes;
     }

     return eSIR_SUCCESS;

}

tSirRetStatus PopulateDot11fRRMIe( tpAniSirGlobal pMac, tDot11fIERRMEnabledCap *pDot11f, tpPESession    psessionEntry )
{  
   tpRRMCaps pRrmCaps;

   pRrmCaps = rrmGetCapabilities( pMac, psessionEntry );

   pDot11f->LinkMeasurement         = pRrmCaps->LinkMeasurement ;
   pDot11f->NeighborRpt             = pRrmCaps->NeighborRpt ;
   pDot11f->parallel                = pRrmCaps->parallel ;
   pDot11f->repeated                = pRrmCaps->repeated ;
   pDot11f->BeaconPassive           = pRrmCaps->BeaconPassive ;
   pDot11f->BeaconActive            = pRrmCaps->BeaconActive ;
   pDot11f->BeaconTable             = pRrmCaps->BeaconTable ;
   pDot11f->BeaconRepCond           = pRrmCaps->BeaconRepCond ;
   pDot11f->FrameMeasurement        = pRrmCaps->FrameMeasurement ;
   pDot11f->ChannelLoad             = pRrmCaps->ChannelLoad ;
   pDot11f->NoiseHistogram          = pRrmCaps->NoiseHistogram ;
   pDot11f->statistics              = pRrmCaps->statistics ;
   pDot11f->LCIMeasurement          = pRrmCaps->LCIMeasurement ;
   pDot11f->LCIAzimuth              = pRrmCaps->LCIAzimuth ;
   pDot11f->TCMCapability           = pRrmCaps->TCMCapability ;
   pDot11f->triggeredTCM            = pRrmCaps->triggeredTCM ;
   pDot11f->APChanReport            = pRrmCaps->APChanReport ;
   pDot11f->RRMMIBEnabled           = pRrmCaps->RRMMIBEnabled ;
   pDot11f->operatingChanMax        = pRrmCaps->operatingChanMax ;
   pDot11f->nonOperatinChanMax      = pRrmCaps->nonOperatingChanMax ;
   pDot11f->MeasurementPilot        = pRrmCaps->MeasurementPilot ;
   pDot11f->MeasurementPilotEnabled = pRrmCaps->MeasurementPilotEnabled ;
   pDot11f->NeighborTSFOffset       = pRrmCaps->NeighborTSFOffset ;
   pDot11f->RCPIMeasurement         = pRrmCaps->RCPIMeasurement ;
   pDot11f->RSNIMeasurement         = pRrmCaps->RSNIMeasurement ;
   pDot11f->BssAvgAccessDelay       = pRrmCaps->BssAvgAccessDelay ;
   pDot11f->BSSAvailAdmission       = pRrmCaps->BSSAvailAdmission ;
   pDot11f->AntennaInformation      = pRrmCaps->AntennaInformation ;

   pDot11f->present = 1;
   return eSIR_SUCCESS;
}
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
void PopulateMDIE( tpAniSirGlobal        pMac,
                   tDot11fIEMobilityDomain *pDot11f, tANI_U8 mdie[SIR_MDIE_SIZE] )
{
   pDot11f->present = 1;
   pDot11f->MDID = (tANI_U16)((mdie[1] << 8) | (mdie[0]));

   // Plugfest fix
   pDot11f->overDSCap =   (mdie[2] & 0x01);
   pDot11f->resourceReqCap = ((mdie[2] >> 1) & 0x01);
   
}

void PopulateFTInfo( tpAniSirGlobal      pMac,
                     tDot11fIEFTInfo     *pDot11f )
{
   pDot11f->present = 1;
   pDot11f->IECount = 0; //TODO: put valid data during reassoc.
   //All other info is zero.

}
#endif

void PopulateDot11fAssocRspRates ( tpAniSirGlobal pMac, tDot11fIESuppRates *pSupp, 
      tDot11fIEExtSuppRates *pExt, tANI_U16 *_11bRates, tANI_U16 *_11aRates )
{
  tANI_U8 num_supp = 0, num_ext = 0;
  tANI_U8 i,j;

  for( i = 0 ; (i < SIR_NUM_11B_RATES && _11bRates[i]) ; i++, num_supp++ )
  {
      pSupp->rates[num_supp] = (tANI_U8)_11bRates[i];
  }  
  for( j = 0 ; (j < SIR_NUM_11A_RATES && _11aRates[j]) ; j++ )
  {
     if( num_supp < 8 )
         pSupp->rates[num_supp++] = (tANI_U8)_11aRates[j];
     else
         pExt->rates[num_ext++] =  (tANI_U8)_11aRates[j]; 
  }  

  if( num_supp )
  {
      pSupp->num_rates = num_supp;
      pSupp->present = 1;
  }
  if( num_ext )
  {
     pExt->num_rates = num_ext;
     pExt->present = 1;
  }
}

void PopulateDot11fTimeoutInterval( tpAniSirGlobal pMac,
                                    tDot11fIETimeoutInterval *pDot11f,
                                    tANI_U8 type, tANI_U32 value )
{
   pDot11f->present = 1;
   pDot11f->timeoutType = type;
   pDot11f->timeoutValue = value;
}
// parserApi.c ends here.
