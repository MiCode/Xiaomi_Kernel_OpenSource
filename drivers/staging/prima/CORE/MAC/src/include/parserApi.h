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
 * This file parserApi.h contains the definitions used
 * for parsing received 802.11 frames
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __PARSE_H__
#define __PARSE_H__

#include <stdarg.h>
#include "sirMacPropExts.h"
#include "dot11f.h"
#include "limSession.h"

#define COUNTRY_STRING_LENGTH    (  3 )
#define COUNTRY_INFO_MAX_CHANNEL ( 84 )
#define MAX_SIZE_OF_TRIPLETS_IN_COUNTRY_IE (COUNTRY_STRING_LENGTH * COUNTRY_INFO_MAX_CHANNEL)
#define HIGHEST_24GHZ_CHANNEL_NUM  ( 14 )

#define IS_24G_CH(__chNum) ((__chNum > 0) && (__chNum < 14))
#define IS_5G_CH(__chNum) ((__chNum >= 36) && (__chNum <= 165))

#define SIZE_OF_FIXED_PARAM 12
#define SIZE_OF_TAG_PARAM_NUM 1
#define SIZE_OF_TAG_PARAM_LEN 1
#define RSNIEID 0x30
#define RSNIE_CAPABILITY_LEN 2
#define DEFAULT_RSNIE_CAP_VAL 0x00

typedef struct sSirCountryInformation
{
    tANI_U8 countryString[COUNTRY_STRING_LENGTH];
    tANI_U8 numIntervals; //number of channel intervals
    struct channelPowerLim
    {
        tANI_U8 channelNumber;
        tANI_U8 numChannel;
        tANI_U8 maxTransmitPower;
    } channelTransmitPower[COUNTRY_INFO_MAX_CHANNEL];
} tSirCountryInformation,*tpSirCountryInformation;


/// Structure common to Beaons & Probe Responses
typedef struct sSirProbeRespBeacon
{
    tSirMacTimeStamp          timeStamp;
    tANI_U16                  beaconInterval;
    tSirMacCapabilityInfo     capabilityInfo;

    tSirMacSSid               ssId;
    tSirMacRateSet            supportedRates;
    tSirMacRateSet            extendedRates;
    tSirMacChanNum            channelNumber;
    tSirMacCfParamSet         cfParamSet;
    tSirMacTim                tim;
    tSirMacEdcaParamSetIE     edcaParams;
    tSirMacQosCapabilityIE    qosCapability;

    tSirCountryInformation    countryInfoParam;
    tSirMacWpaInfo            wpa;
    tSirMacRsnInfo            rsn;

    tSirMacErpInfo            erpIEInfo;

    tSirPropIEStruct          propIEinfo;
    tDot11fIEPowerConstraints localPowerConstraint;
    tDot11fIETPCReport        tpcReport;
    tDot11fIEChanSwitchAnn    channelSwitchIE;
    tDot11fIEExtChanSwitchAnn extChannelSwitchIE;
    tSirMacAddr               bssid;
    tDot11fIEQuiet            quietIE;
    tDot11fIEHTCaps           HTCaps;
    tDot11fIEHTInfo           HTInfo;
    tDot11fIEP2PProbeRes      P2PProbeRes;
#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8                   mdie[SIR_MDIE_SIZE];
#endif
#ifdef FEATURE_WLAN_ESE
    tDot11fIEESETxmitPower    eseTxPwr;
    tDot11fIEQBSSLoad         QBSSLoad;
#endif
    tANI_U8                   ssidPresent;
    tANI_U8                   suppRatesPresent;
    tANI_U8                   extendedRatesPresent;
    tANI_U8                   cfPresent;
    tANI_U8                   dsParamsPresent;
    tANI_U8                   timPresent;

    tANI_U8                   edcaPresent;
    tANI_U8                   qosCapabilityPresent;
    tANI_U8                   wmeEdcaPresent;
    tANI_U8                   wmeInfoPresent;
    tANI_U8                   wsmCapablePresent;

    tANI_U8                   countryInfoPresent;
    tANI_U8                   wpaPresent;
    tANI_U8                   rsnPresent;
    tANI_U8                   erpPresent;
    tANI_U8                   channelSwitchPresent;
    tANI_U8                   extChannelSwitchPresent;
    tANI_U8                   quietIEPresent;
    tANI_U8                   tpcReportPresent;
    tANI_U8                   powerConstraintPresent;

#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8                   mdiePresent;
#endif

#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps          VHTCaps;
    tDot11fIEVHTOperation     VHTOperation;
    tDot11fIEVHTExtBssLoad    VHTExtBssLoad;
    tDot11fIEExtCap           ExtCap;
    tDot11fIEOperatingMode    OperatingMode;
    tANI_U8                   WiderBWChanSwitchAnnPresent;
    tDot11fIEWiderBWChanSwitchAnn WiderBWChanSwitchAnn;
#endif
    tDot11fIEOBSSScanParameters OBSSScanParameters;
} tSirProbeRespBeacon, *tpSirProbeRespBeacon;

// probe Request structure
typedef struct sSirProbeReq
{
    tSirMacSSid               ssId;
    tSirMacRateSet            supportedRates;
    tSirMacRateSet            extendedRates;
    tDot11fIEWscProbeReq      probeReqWscIeInfo;
    tDot11fIEHTCaps           HTCaps;
    tANI_U8                   ssidPresent;
    tANI_U8                   suppRatesPresent;
    tANI_U8                   extendedRatesPresent;
    tANI_U8                   wscIePresent;
    tANI_U8                   p2pIePresent;
#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps          VHTCaps;
#endif


} tSirProbeReq, *tpSirProbeReq;

/// Association Request structure (one day to be replaced by
/// tDot11fAssocRequest)
typedef struct sSirAssocReq
{

    tSirMacCapabilityInfo     capabilityInfo;
    tANI_U16                  listenInterval;
    tSirMacAddr               currentApAddr; /* only in reassoc frames */
    tSirMacSSid               ssId;
    tSirMacRateSet            supportedRates;
    tSirMacRateSet            extendedRates;

    tSirAddtsReqInfo          addtsReq;
    tSirMacQosCapabilityStaIE qosCapability;

    tSirMacWpaInfo            wpa;
    tSirMacRsnInfo            rsn;
    tSirAddie                 addIE;
    
    tSirPropIEStruct          propIEinfo;
    tSirMacPowerCapabilityIE  powerCapability;
    tSirMacSupportedChannelIE supportedChannels;
    tDot11fIEHTCaps   HTCaps;
    tDot11fIEWMMInfoStation   WMMInfoStation;
    /// This is set if the frame is a reassoc request:
    tANI_U8                   reassocRequest;
    tANI_U8                   ssidPresent;
    tANI_U8                   suppRatesPresent;
    tANI_U8                   extendedRatesPresent;

    tANI_U8                   wmeInfoPresent;
    tANI_U8                   qosCapabilityPresent;
    tANI_U8                   addtsPresent;
    tANI_U8                   wsmCapablePresent;

    tANI_U8                   wpaPresent;
    tANI_U8                   rsnPresent;
    tANI_U8                   addIEPresent;

    tANI_U8                   powerCapabilityPresent;
    tANI_U8                   supportedChannelsPresent;
    // keeing copy of assoction request received, this is 
    // required for indicating the frame to upper layers
    tANI_U32                  assocReqFrameLength;
    tANI_U8*                  assocReqFrame;
#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps          VHTCaps;
    tDot11fIEOperatingMode    operMode;
#endif
} tSirAssocReq, *tpSirAssocReq;


/// Association Response structure (one day to be replaced by
/// tDot11fAssocRequest)
typedef struct sSirAssocRsp
{

    tSirMacCapabilityInfo     capabilityInfo;
    tANI_U16                  aid;
    tANI_U16                  statusCode;
    tSirMacRateSet            supportedRates;
    tSirMacRateSet            extendedRates;
    tSirPropIEStruct          propIEinfo;
    tSirMacEdcaParamSetIE     edca;
    tSirAddtsRspInfo          addtsRsp;
    tDot11fIEHTCaps   HTCaps;
    tDot11fIEHTInfo           HTInfo;
#if defined WLAN_FEATURE_VOWIFI_11R
    tDot11fIEFTInfo           FTInfo;
    tANI_U8                   mdie[SIR_MDIE_SIZE];
    tANI_U8                   num_RICData; 
    tDot11fIERICDataDesc      RICData[2];
#endif

#ifdef FEATURE_WLAN_ESE
    tANI_U8                   num_tspecs;
    tDot11fIEWMMTSPEC         TSPECInfo[SIR_ESE_MAX_TSPEC_IES];
    tSirMacESETSMIE           tsmIE;
#endif

    tANI_U8                   suppRatesPresent;
    tANI_U8                   extendedRatesPresent;

    tANI_U8                   edcaPresent;
    tANI_U8                   wmeEdcaPresent;
    tANI_U8                   addtsPresent;
    tANI_U8                   wsmCapablePresent;
#if defined WLAN_FEATURE_VOWIFI_11R
    tANI_U8                   ftinfoPresent;
    tANI_U8                   mdiePresent;
    tANI_U8                   ricPresent;
#endif
#ifdef FEATURE_WLAN_ESE
    tANI_U8                   tspecPresent;
    tANI_U8                   tsmPresent;
#endif    
#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps          VHTCaps;
    tDot11fIEVHTOperation     VHTOperation;
#endif
    tDot11fIEExtCap           ExtCap;
    tDot11fIEOBSSScanParameters OBSSScanParameters;
    tSirQosMapSet QosMapSet;
} tSirAssocRsp, *tpSirAssocRsp;

#if defined(FEATURE_WLAN_ESE_UPLOAD)
// Structure to hold Ese Beacon report mandatory IEs
typedef struct sSirEseBcnReportMandatoryIe
{
    tSirMacSSid           ssId;
    tSirMacRateSet        supportedRates;
    tSirMacFHParamSet     fhParamSet;
    tSirMacDsParamSetIE   dsParamSet;
    tSirMacCfParamSet     cfParamSet;
    tSirMacIBSSParams     ibssParamSet;
    tSirMacTim            tim;
    tSirMacRRMEnabledCap  rmEnabledCapabilities;

    tANI_U8               ssidPresent;
    tANI_U8               suppRatesPresent;
    tANI_U8               fhParamPresent;
    tANI_U8               dsParamsPresent;
    tANI_U8               cfPresent;
    tANI_U8               ibssParamPresent;
    tANI_U8               timPresent;
    tANI_U8               rrmPresent;
} tSirEseBcnReportMandatoryIe, *tpSirEseBcnReportMandatoryIe;
#endif /* FEATURE_WLAN_ESE_UPLOAD */

tANI_U8
sirIsPropCapabilityEnabled(struct sAniSirGlobal *pMac, tANI_U32 bitnum);

tSirRetStatus
sirGetCfgPropCaps(struct sAniSirGlobal *, tANI_U16 *);

void dot11fLog(tpAniSirGlobal pMac, int nSev, const char *lpszFormat, ...);

#define CFG_GET_INT(nStatus, pMac, nItem, cfg )  do {                \
        (nStatus) = wlan_cfgGetInt( (pMac), (nItem), & (cfg) );      \
        if ( eSIR_SUCCESS != (nStatus) )                             \
        {                                                            \
            dot11fLog( (pMac), LOGP, FL("Failed to retrieve "        \
                                        #nItem " from CFG (%d)."), \
                       (nStatus) );                                  \
            return nStatus;                                          \
        }                                                            \
    } while (0)

#define CFG_GET_INT_NO_STATUS(nStatus, pMac, nItem, cfg ) do {       \
        (nStatus) = wlan_cfgGetInt( (pMac), (nItem), & (cfg) );      \
        if ( eSIR_SUCCESS != (nStatus) )                             \
        {                                                            \
            dot11fLog( (pMac), LOGP, FL("Failed to retrieve "        \
                                        #nItem " from CFG (%d)."), \
                       (nStatus) );                                  \
            return;                                                  \
        }                                                            \
    } while (0)

#define CFG_GET_STR(nStatus, pMac, nItem, cfg, nCfg, nMaxCfg) do {      \
        (nCfg) = (nMaxCfg);                                             \
        (nStatus) = wlan_cfgGetStr( (pMac), (nItem), (cfg), & (nCfg) ); \
        if ( eSIR_SUCCESS != (nStatus) )                                \
        {                                                               \
            dot11fLog( (pMac), LOGP, FL("Failed to retrieve "           \
                                        #nItem " from CFG (%d)."),    \
                       (nStatus) );                                     \
            return nStatus;                                             \
        }                                                               \
    } while (0)

#define CFG_GET_STR_NO_STATUS(nStatus, pMac, nItem, cfg, nCfg,          \
                              nMaxCfg) do {                             \
        (nCfg) = (nMaxCfg);                                             \
        (nStatus) = wlan_cfgGetStr( (pMac), (nItem), (cfg), & (nCfg) ); \
        if ( eSIR_SUCCESS != (nStatus) )                                \
        {                                                               \
            dot11fLog( (pMac), LOGP, FL("Failed to retrieve "           \
                                        #nItem " from CFG (%d)."),    \
                       (nStatus) );                                     \
            return;                                                     \
        }                                                               \
    } while (0)

void swapBitField16(tANI_U16 in, tANI_U16 *out);

// Currently implemented as "shims" between callers & the new framesc-
// generated code:

tSirRetStatus
sirConvertProbeReqFrame2Struct(struct sAniSirGlobal *pMac,
                               tANI_U8 *frame,
                               tANI_U32 len,
                               tpSirProbeReq probe);

tSirRetStatus
sirConvertProbeFrame2Struct(struct sAniSirGlobal *pMac, tANI_U8 *frame,
                            tANI_U32 len,
                            tpSirProbeRespBeacon probe);

tSirRetStatus
sirConvertAssocReqFrame2Struct(struct sAniSirGlobal *pMac,
                               tANI_U8 * frame,
                               tANI_U32 len,
                               tpSirAssocReq assoc);

tSirRetStatus
sirConvertAssocRespFrame2Struct(struct sAniSirGlobal *pMac,
                                tANI_U8 * frame,
                                tANI_U32 len,
                                tpSirAssocRsp assoc);

tSirRetStatus
sirConvertReassocReqFrame2Struct(struct sAniSirGlobal *pMac,
                                 tANI_U8 * frame,
                                 tANI_U32 len,
                                 tpSirAssocReq assoc);

tSirRetStatus
sirParseBeaconIE(struct sAniSirGlobal *pMac,
                 tpSirProbeRespBeacon   pBeaconStruct,
                 tANI_U8                    *pPayload,
                 tANI_U32                    payloadLength);

#if defined(FEATURE_WLAN_ESE_UPLOAD)
tSirRetStatus
sirFillBeaconMandatoryIEforEseBcnReport(tpAniSirGlobal    pMac,
                                        tANI_U8          *pPayload,
                                        const tANI_U32    payloadLength,
                                        tANI_U8         **outIeBuf,
                                        tANI_U32         *pOutIeLen);
#endif /* FEATURE_WLAN_ESE_UPLOAD */

tSirRetStatus
sirConvertBeaconFrame2Struct(struct sAniSirGlobal *pMac,
                             tANI_U8 *pBeaconFrame,
                             tpSirProbeRespBeacon pBeaconStruct);

tSirRetStatus
sirConvertAuthFrame2Struct(struct sAniSirGlobal *pMac,
                           tANI_U8 * frame,
                           tANI_U32 len,
                           tpSirMacAuthFrameBody auth);

tSirRetStatus
sirConvertAddtsReq2Struct(struct sAniSirGlobal *pMac,
                          tANI_U8 *frame,
                          tANI_U32 len,
                          tSirAddtsReqInfo *addTs);

tSirRetStatus
sirConvertAddtsRsp2Struct(struct sAniSirGlobal *pMac,
                          tANI_U8 *frame,
                          tANI_U32 len,
                          tSirAddtsRspInfo *addts);

tSirRetStatus
sirConvertDeltsReq2Struct(struct sAniSirGlobal *pMac,
                          tANI_U8 *frame,
                          tANI_U32 len,
                          tSirDeltsReqInfo *delTs);
tSirRetStatus
sirConvertQosMapConfigureFrame2Struct(tpAniSirGlobal    pMac,
                          tANI_U8               *pFrame,
                          tANI_U32               nFrame,
                          tSirQosMapSet      *pQosMapSet);

#ifdef ANI_SUPPORT_11H
tSirRetStatus
sirConvertTpcReqFrame2Struct(struct sAniSirGlobal *, tANI_U8 *,
                             tpSirMacTpcReqActionFrame, tANI_U32);

tSirRetStatus
sirConvertMeasReqFrame2Struct(struct sAniSirGlobal *, tANI_U8 *,
                              tpSirMacMeasReqActionFrame, tANI_U32);
#endif


/**
 * \brief Populated a tDot11fFfCapabilities
 *
 * \sa PopulatedDot11fCapabilities2
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param pDot11f Address of a tDot11fFfCapabilities to be filled in
 *
 *
 * \note If SIR_MAC_PROP_CAPABILITY_11EQOS is enabled, we'll clear the QOS
 * bit in pDot11f
 *
 *
 */

tSirRetStatus
PopulateDot11fCapabilities(tpAniSirGlobal         pMac,
                           tDot11fFfCapabilities *pDot11f,
                           tpPESession            psessionEntry);

/**
 * \brief Populated a tDot11fFfCapabilities
 *
 * \sa PopulatedDot11fCapabilities2
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param pDot11f Address of a tDot11fFfCapabilities to be filled in
 *
 * \param pSta Pointer to a tDphHashNode representing a peer
 *
 *
 * \note If SIR_MAC_PROP_CAPABILITY_11EQOS is enabled on our peer, we'll
 * clear the QOS bit in pDot11f
 *
 *
 */

struct sDphHashNode;

tSirRetStatus
PopulateDot11fCapabilities2(tpAniSirGlobal         pMac,
                            tDot11fFfCapabilities *pDot11f,
                            struct sDphHashNode   *pSta,
                            tpPESession            psessionEntry);

/// Populate a tDot11fIEChanSwitchAnn
void
PopulateDot11fChanSwitchAnn(tpAniSirGlobal          pMac,
                            tDot11fIEChanSwitchAnn *pDot11f,
                            tpPESession psessionEntry);

/// Populate a tDot11fIEChanSwitchAnn
void
PopulateDot11fExtChanSwitchAnn(tpAniSirGlobal          pMac,
                             tDot11fIEExtChanSwitchAnn *pDot11f,
                             tpPESession psessionEntry);

/// Populate a tDot11fIECountry
tSirRetStatus
PopulateDot11fCountry(tpAniSirGlobal    pMac,
                      tDot11fIECountry *pDot11f,  tpPESession psessionEntry);

/// Populated a PopulateDot11fDSParams
tSirRetStatus
PopulateDot11fDSParams(tpAniSirGlobal     pMac,
                       tDot11fIEDSParams *pDot11f, tANI_U8 channel,
                       tpPESession psessionEntry);


/// Populated a tDot11fIEEDCAParamSet
void
PopulateDot11fEDCAParamSet(tpAniSirGlobal         pMac,
                           tDot11fIEEDCAParamSet *pDot11f,
                           tpPESession psessionEntry);

tSirRetStatus
PopulateDot11fERPInfo(tpAniSirGlobal    pMac,
                      tDot11fIEERPInfo *pDot11f, tpPESession psessionEntry);

tSirRetStatus
PopulateDot11fExtSuppRates(tpAniSirGlobal      pMac,
                           tANI_U8  nChannelNum, tDot11fIEExtSuppRates *pDot11f,
                           tpPESession psessionEntry);

#if defined WLAN_FEATURE_VOWIFI
tSirRetStatus 
PopulateDot11fBeaconReport(tpAniSirGlobal       pMac, 
                           tDot11fIEMeasurementReport *pDot11f, 
                           tSirMacBeaconReport *pBeaconReport );
#endif

/**
 * \brief Populate a tDot11fIEExtSuppRates
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param nChannelNum Channel on which the enclosing frame will be going out
 *
 * \param pDot11f Address of a tDot11fIEExtSuppRates struct to be filled in.
 *
 *
 * This method is a NOP if the channel is greater than 14.
 *
 *
 */

tSirRetStatus
PopulateDot11fExtSuppRates1(tpAniSirGlobal         pMac,
                            tANI_U8                     nChannelNum,
                            tDot11fIEExtSuppRates *pDot11f);

tSirRetStatus
PopulateDot11fHCF(tpAniSirGlobal  pMac,
                  tANI_U32        capEnable,
                  tDot11fIEHCF   *pDot11f);

tSirRetStatus
PopulateDot11fHTCaps(tpAniSirGlobal           pMac,
                           tpPESession      psessionEntry,
                           tDot11fIEHTCaps *pDot11f);

tSirRetStatus
PopulateDot11fHTInfo(tpAniSirGlobal   pMac,
                     tDot11fIEHTInfo *pDot11f,
                     tpPESession      psessionEntry);

#ifdef WLAN_FEATURE_AP_HT40_24G
tSirRetStatus
PopulateDot11fOBSSScanParameters(tpAniSirGlobal   pMac,
                     tDot11fIEOBSSScanParameters *pDot11f,
                     tpPESession      psessionEntry);
#endif

void PopulateDot11fIBSSParams(tpAniSirGlobal  pMac,
       tDot11fIEIBSSParams *pDot11f, tpPESession psessionEntry);

#ifdef ANI_SUPPORT_11H
tSirRetStatus
PopulateDot11fMeasurementReport0(tpAniSirGlobal              pMac,
                                 tpSirMacMeasReqActionFrame  pReq,
                                 tDot11fIEMeasurementReport *pDot11f);

/// Populate a tDot11fIEMeasurementReport when the report type is CCA
tSirRetStatus
PopulateDot11fMeasurementReport1(tpAniSirGlobal              pMac,
                                 tpSirMacMeasReqActionFrame  pReq,
                                 tDot11fIEMeasurementReport *pDot11f);

/// Populate a tDot11fIEMeasurementReport when the report type is RPI Hist
tSirRetStatus
PopulateDot11fMeasurementReport2(tpAniSirGlobal              pMac,
                                 tpSirMacMeasReqActionFrame  pReq,
                                 tDot11fIEMeasurementReport *pDot11f);
#endif  //ANI_SUPPORT_11H

/// Populate a tDot11fIEPowerCaps
void
PopulateDot11fPowerCaps(tpAniSirGlobal  pMac,
                        tDot11fIEPowerCaps *pCaps,
                        tANI_U8 nAssocType,tpPESession psessionEntry);

/// Populate a tDot11fIEPowerConstraints
tSirRetStatus
PopulateDot11fPowerConstraints(tpAniSirGlobal             pMac,
                               tDot11fIEPowerConstraints *pDot11f);

tSirRetStatus
PopulateDot11fPropCapability(tpAniSirGlobal           pMac,
                             tANI_U32                      capEnable,
                             tDot11fIEPropCapability *pDot11f);

void
PopulateDot11fPropChannSwitchAnn(tpAniSirGlobal               pMac,
                                 tANI_U32                          capEnable,
                                 tDot11fIEPropChannSwitchAnn *pDot11f);

void
PopulateDot11fPropEDCAParams(tpAniSirGlobal           pMac,
                             tANI_U16                      caps,
                             tDot11fIEPropEDCAParams *pDot11f);

tSirRetStatus
PopulateDot11fPropSuppRates(tpAniSirGlobal          pMac,
                            tANI_U32                     capEnable,
                            tDot11fIEPropSuppRates *pDot11f);

void
PopulateDot11fQOSCapsAp(tpAniSirGlobal      pMac,
                        tDot11fIEQOSCapsAp *pDot11f, tpPESession psessionEntry);

void
PopulateDot11fQOSCapsStation(tpAniSirGlobal           pMac,
                             tDot11fIEQOSCapsStation *pDot11f);

tSirRetStatus 
PopulateDot11fRSN(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIERSN   *pDot11f);

tSirRetStatus
PopulateDot11fRSNOpaque( tpAniSirGlobal      pMac,
                  tpSirRSNie      pRsnIe,
                         tDot11fIERSNOpaque *pDot11f );

#if defined(FEATURE_WLAN_WAPI)

tSirRetStatus
PopulateDot11fWAPI(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIEWAPI   *pDot11f);

tSirRetStatus PopulateDot11fWAPIOpaque( tpAniSirGlobal      pMac,
                                       tpSirRSNie          pRsnIe,
                                       tDot11fIEWAPIOpaque *pDot11f );

#endif //defined(FEATURE_WLAN_WAPI)

/// Populate a tDot11fIESSID given a tSirMacSSid
void
PopulateDot11fSSID(tpAniSirGlobal pMac,
                   tSirMacSSid   *pInternal,
                   tDot11fIESSID *pDot11f);

/// Populate a tDot11fIESSID from CFG
tSirRetStatus
PopulateDot11fSSID2(tpAniSirGlobal pMac,
                    tDot11fIESSID *pDot11f);


/**
 * \brief Populate a tDot11fIESchedule
 *
 * \sa PopulateDot11fWMMSchedule
 *
 *
 * \param pSchedule Address of a tSirMacScheduleIE struct
 *
 * \param pDot11f Address of a tDot11fIESchedule to be filled in
 *
 *
 */

void
PopulateDot11fSchedule(tSirMacScheduleIE *pSchedule,
                       tDot11fIESchedule *pDot11f);

void
PopulateDot11fSuppChannels(tpAniSirGlobal         pMac,
                           tDot11fIESuppChannels *pDot11f,
                           tANI_U8 nAssocType,tpPESession psessionEntry);

/**
 * \brief Populated a tDot11fIESuppRates
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param nChannelNum Channel the enclosing frame will be going out on; see
 * below
 *
 * \param pDot11f Address of a tDot11fIESuppRates struct to be filled in.
 *
 *
 * If nChannelNum is greater than 13, the supported rates will be
 * WNI_CFG_SUPPORTED_RATES_11B.  If it is less than or equal to 13, the
 * supported rates will be WNI_CFG_SUPPORTED_RATES_11A.  If nChannelNum is
 * set to the sentinel value POPULATE_DOT11F_RATES_OPERATIONAL, the struct
 * will be populated with WNI_CFG_OPERATIONAL_RATE_SET.
 *
 *
 */

#define POPULATE_DOT11F_RATES_OPERATIONAL ( 0xff )

tSirRetStatus
PopulateDot11fSuppRates(tpAniSirGlobal      pMac,
                        tANI_U8                  nChannelNum,
                        tDot11fIESuppRates *pDot11f,tpPESession);

tSirRetStatus
PopulateDot11fRatesTdls(tpAniSirGlobal p_mac,
                           tDot11fIESuppRates *p_supp_rates,
                           tDot11fIEExtSuppRates *p_ext_supp_rates);

tSirRetStatus PopulateDot11fTPCReport(tpAniSirGlobal      pMac,
                                      tDot11fIETPCReport *pDot11f,
                                      tpPESession psessionEntry);

/// Populate a tDot11FfTSInfo
void PopulateDot11fTSInfo(tSirMacTSInfo   *pInfo,
                          tDot11fFfTSInfo *pDot11f);


void PopulateDot11fWMM(tpAniSirGlobal      pMac,
                       tDot11fIEWMMInfoAp  *pInfo,
                       tDot11fIEWMMParams *pParams,
                       tDot11fIEWMMCaps   *pCaps,
                       tpPESession        psessionEntry);

void PopulateDot11fWMMCaps(tDot11fIEWMMCaps *pCaps);

#if defined(FEATURE_WLAN_ESE)
// Fill the ESE version IE
void PopulateDot11fESEVersion(tDot11fIEESEVersion *pESEVersion);
// Fill the Radio Management Capability
void PopulateDot11fESERadMgmtCap(tDot11fIEESERadMgmtCap *pESERadMgmtCap);
// Fill the CCKM IE
tSirRetStatus PopulateDot11fESECckmOpaque( tpAniSirGlobal pMac,
                                           tpSirCCKMie    pCCKMie,
                                           tDot11fIEESECckmOpaque *pDot11f );

void PopulateDot11TSRSIE(tpAniSirGlobal  pMac,
                               tSirMacESETSRSIE     *pOld,
                               tDot11fIEESETrafStrmRateSet  *pDot11f,
                               tANI_U8 rate_length);
void PopulateDot11fReAssocTspec(tpAniSirGlobal pMac, tDot11fReAssocRequest *pReassoc, tpPESession psessionEntry);
#endif

void PopulateDot11fWMMInfoAp(tpAniSirGlobal      pMac,
                             tDot11fIEWMMInfoAp *pInfo,
                             tpPESession psessionEntry);

void PopulateDot11fWMMInfoStation(tpAniSirGlobal           pMac,
                                  tDot11fIEWMMInfoStation *pInfo);

void PopulateDot11fWMMParams(tpAniSirGlobal      pMac,
                             tDot11fIEWMMParams *pParams,
                             tpPESession        psessionEntry);

/**
 * \brief Populate a tDot11fIEWMMSchedule
 *
 * \sa PopulatedDot11fSchedule
 *
 *
 * \param pSchedule Address of a tSirMacScheduleIE struct
 *
 * \param pDot11f Address of a tDot11fIEWMMSchedule to be filled in
 *
 *
 */

void
PopulateDot11fWMMSchedule(tSirMacScheduleIE    *pSchedule,
                          tDot11fIEWMMSchedule *pDot11f);

tSirRetStatus
PopulateDot11fWPA(tpAniSirGlobal  pMac,
                  tpSirRSNie      pRsnIe,
                  tDot11fIEWPA   *pDot11f);

tSirRetStatus
PopulateDot11fWPAOpaque( tpAniSirGlobal      pMac,
                         tpSirRSNie          pRsnIe,
                         tDot11fIEWPAOpaque *pDot11f );

void
PopulateDot11fTSPEC(tSirMacTspecIE  *pOld,
                    tDot11fIETSPEC  *pDot11f);

void
PopulateDot11fWMMTSPEC(tSirMacTspecIE     *pOld,
                       tDot11fIEWMMTSPEC  *pDot11f);

tSirRetStatus
PopulateDot11fTCLAS(tpAniSirGlobal  pMac,
                    tSirTclasInfo  *pOld,
                    tDot11fIETCLAS *pDot11f);

tSirRetStatus
PopulateDot11fWMMTCLAS(tpAniSirGlobal     pMac,
                       tSirTclasInfo     *pOld,
                       tDot11fIEWMMTCLAS *pDot11f);


tSirRetStatus PopulateDot11fWsc(tpAniSirGlobal pMac,
                                tDot11fIEWscBeacon *pDot11f);

tSirRetStatus PopulateDot11fWscRegistrarInfo(tpAniSirGlobal pMac,
                                             tDot11fIEWscBeacon *pDot11f);

tSirRetStatus DePopulateDot11fWscRegistrarInfo(tpAniSirGlobal pMac,
                                               tDot11fIEWscBeacon *pDot11f);

tSirRetStatus PopulateDot11fProbeResWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscProbeRes *pDot11f, tpPESession psessionEntry);
tSirRetStatus PopulateDot11fAssocResWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscAssocRes *pDot11f, tpPESession psessionEntry);
tSirRetStatus PopulateDot11fBeaconWPSIEs(tpAniSirGlobal pMac, tDot11fIEWscBeacon *pDot11f, tpPESession psessionEntry);

tSirRetStatus PopulateDot11fWscInProbeRes(tpAniSirGlobal pMac,
                                          tDot11fIEWscProbeRes *pDot11f);

tSirRetStatus PopulateDot11fWscRegistrarInfoInProbeRes(tpAniSirGlobal pMac,
                                                       tDot11fIEWscProbeRes *pDot11f);

tSirRetStatus DePopulateDot11fWscRegistrarInfoInProbeRes(tpAniSirGlobal pMac,
                                                         tDot11fIEWscProbeRes *pDot11f);


tSirRetStatus PopulateDot11fAssocResWscIE(tpAniSirGlobal pMac, 
                                          tDot11fIEWscAssocRes *pDot11f, 
                                          tpSirAssocReq pRcvdAssocReq);

tSirRetStatus PopulateDot11AssocResP2PIE(tpAniSirGlobal pMac, 
                                       tDot11fIEP2PAssocRes *pDot11f, 
                                       tpSirAssocReq pRcvdAssocReq);

tSirRetStatus PopulateDot11fWscInAssocRes(tpAniSirGlobal pMac,
                                          tDot11fIEWscAssocRes *pDot11f);


#if defined WLAN_FEATURE_VOWIFI
tSirRetStatus PopulateDot11fWFATPC( tpAniSirGlobal        pMac,
                                    tDot11fIEWFATPC *pDot11f, tANI_U8 txPower, tANI_U8 linkMargin );

tSirRetStatus PopulateDot11fRRMIe( tpAniSirGlobal pMac, 
                                   tDot11fIERRMEnabledCap *pDot11f, 
                                   tpPESession    psessionEntry );
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
void PopulateMDIE( tpAniSirGlobal        pMac,
                   tDot11fIEMobilityDomain *pDot11f, tANI_U8 mdie[] );
void PopulateFTInfo( tpAniSirGlobal      pMac,
                     tDot11fIEFTInfo     *pDot11f );
#endif

void PopulateDot11fAssocRspRates ( tpAniSirGlobal pMac, tDot11fIESuppRates *pSupp, 
      tDot11fIEExtSuppRates *pExt, tANI_U16 *_11bRates, tANI_U16 *_11aRates );

int FindIELocation( tpAniSirGlobal pMac,
                           tpSirRSNie pRsnIe,
                           tANI_U8 EID);
#endif

#ifdef WLAN_FEATURE_11AC
tSirRetStatus
PopulateDot11fVHTCaps(tpAniSirGlobal  pMac, tDot11fIEVHTCaps *pDot11f,
          tANI_U8 nChannelNum, tAniBool isProbeRspAssocRspBeacon);

tSirRetStatus
PopulateDot11fVHTOperation(tpAniSirGlobal  pMac,
                     tDot11fIEVHTOperation  *pDot11f, tANI_U8 nChannelNum);

tSirRetStatus
PopulateDot11fVHTExtBssLoad(tpAniSirGlobal  pMac, tDot11fIEVHTExtBssLoad   *pDot11f);

tSirRetStatus
PopulateDot11fExtCap(tpAniSirGlobal pMac, tDot11fIEExtCap * pDot11f,
                            tPESession *sessionEntry);

tSirRetStatus
PopulateDot11fOperatingMode(tpAniSirGlobal pMac, tDot11fIEOperatingMode *pDot11f, tpPESession psessionEntry );

void
PopulateDot11fWiderBWChanSwitchAnn(tpAniSirGlobal pMac,
                                   tDot11fIEWiderBWChanSwitchAnn *pDot11f,
                                   tpPESession psessionEntry);
#endif

void PopulateDot11fTimeoutInterval( tpAniSirGlobal pMac,
                                    tDot11fIETimeoutInterval *pDot11f,
                                    tANI_U8 type, tANI_U32 value );

tSirRetStatus ValidateAndRectifyIEs(tpAniSirGlobal pMac,
                                    tANI_U8 *pMgmtFrame,
                                    tANI_U32 nFrameBytes,
                                    tANI_U32 *nMissingRsnBytes);
