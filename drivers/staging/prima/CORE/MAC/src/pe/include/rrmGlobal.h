/*
 * Copyright (c) 2011-2012,2014 The Linux Foundation. All rights reserved.
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



#if !defined( __RRMGLOBAL_H )
#define __RRMGLOBAL_H

/**=========================================================================

  \file  rrmGlobal.h

  \brief Definitions for SME APIs


  ========================================================================*/

typedef enum eRrmRetStatus
{
    eRRM_SUCCESS,
    eRRM_INCAPABLE,
    eRRM_REFUSED,
    eRRM_FAILURE
} tRrmRetStatus;

typedef enum eRrmMsgReqSource
{
    eRRM_MSG_SOURCE_LEGACY_ESE  = 1, /* legacy ese */
    eRRM_MSG_SOURCE_11K         = 2, /* 11k */
    eRRM_MSG_SOURCE_ESE_UPLOAD  = 3  /* ese upload approach */
} tRrmMsgReqSource;

typedef struct sSirChannelInfo
{
   tANI_U8 regulatoryClass;
   tANI_U8 channelNum;
} tSirChannelInfo, * tpSirChannelInfo;

typedef struct sSirBeaconReportReqInd
{
   tANI_U16     messageType; // eWNI_SME_BEACON_REPORT_REQ_IND
   tANI_U16     length;
   tSirMacAddr  bssId;
   tANI_U16     measurementDuration[SIR_ESE_MAX_MEAS_IE_REQS];   //ms
   tANI_U16     randomizationInterval; //ms
   tSirChannelInfo channelInfo;
   tSirMacAddr      macaddrBssid;   //0: wildcard
   tANI_U8      fMeasurementtype[SIR_ESE_MAX_MEAS_IE_REQS];  //0:Passive, 1: Active, 2: table mode
   tAniSSID     ssId;              //May be wilcard.
   tANI_U16      uDialogToken;
   tSirChannelList channelList; //From AP channel report.
   tRrmMsgReqSource msgSource;
} tSirBeaconReportReqInd, * tpSirBeaconReportReqInd;


typedef struct sSirBeaconReportXmitInd
{
   tANI_U16    messageType; // eWNI_SME_BEACON_REPORT_RESP_XMIT_IND
   tANI_U16    length;
   tSirMacAddr bssId;
   tANI_U16     uDialogToken;
   tANI_U8     fMeasureDone;
   tANI_U16    duration;
   tANI_U8     regClass;
   tANI_U8     numBssDesc;
   tpSirBssDescription pBssDescription[SIR_BCN_REPORT_MAX_BSS_DESC];
} tSirBeaconReportXmitInd, * tpSirBeaconReportXmitInd;

typedef struct sSirNeighborReportReqInd
{
   tANI_U16     messageType; // eWNI_SME_NEIGHBOR_REPORT_REQ_IND
   tANI_U16     length;
   tSirMacAddr  bssId;  //For the session.
   tANI_U16     noSSID; //TRUE - dont include SSID in the request.
                        //FALSE  include the SSID. It may be null (wildcard)
   tSirMacSSid  ucSSID;  
} tSirNeighborReportReqInd, * tpSirNeighborReportReqInd;
                                   

typedef struct sSirNeighborBssDescription
{
   tANI_U16        length;
   tSirMacAddr     bssId;
   tANI_U8         regClass;
   tANI_U8         channel;
   tANI_U8         phyType;
   union sSirNeighborBssidInfo {
         struct _rrmInfo {
                tANI_U32      fApPreauthReachable:2;  //see IEEE 802.11k Table 7-43a
                tANI_U32      fSameSecurityMode:1;
                tANI_U32      fSameAuthenticator:1;
                tANI_U32      fCapSpectrumMeasurement:1; //see IEEE 802.11k Table 7-95d
                tANI_U32      fCapQos:1; 
                tANI_U32      fCapApsd:1; 
                tANI_U32      fCapRadioMeasurement:1; 
                tANI_U32      fCapDelayedBlockAck:1; 
                tANI_U32      fCapImmediateBlockAck:1;
                tANI_U32      fMobilityDomain:1;
                tANI_U32      reserved:21; 
         } rrmInfo;
         struct _eseInfo {
                tANI_U32      channelBand:8;
                tANI_U32      minRecvSigPower:8;
                tANI_U32      apTxPower:8;
                tANI_U32      roamHysteresis:8;
                tANI_U32      adaptScanThres:8;

                tANI_U32      transitionTime:8;
                tANI_U32      tsfOffset:16;

                tANI_U32      beaconInterval:16;
                tANI_U32      reserved: 16;
         } eseInfo;
   } bssidInfo;
 
   //Optional sub IEs....ignoring for now.
}tSirNeighborBssDescription, *tpSirNeighborBssDescripton;

typedef struct sSirNeighborReportInd
{
   tANI_U16     messageType; // eWNI_SME_NEIGHBOR_REPORT_IND
   tANI_U16     length;
   tANI_U16     numNeighborReports;
   tSirMacAddr  bssId;  //For the session.
   //tSirResultCodes    statusCode;
   tSirNeighborBssDescription sNeighborBssDescription[1];
} tSirNeighborReportInd, * tpSirNeighborReportInd;

typedef struct sRRMBeaconReportRequestedIes
{
   tANI_U8 num;
   tANI_U8 *pElementIds;
}tRRMBeaconReportRequestedIes, *tpRRMBeaconReportRequestedIes;

//Reporting detail defines.
//Reference - IEEE Std 802.11k-2008 section 7.3.2.21.6 Table 7-29h
#define BEACON_REPORTING_DETAIL_NO_FF_IE 0
#define BEACON_REPORTING_DETAIL_ALL_FF_REQ_IE 1
#define BEACON_REPORTING_DETAIL_ALL_FF_IE 2


typedef struct sRRMReq
{
   tANI_U8 dialog_token; //In action frame;
   tANI_U8 token; //Within individual request;
   tANI_U8 type;
   union {
      struct {
         tANI_U8 reportingDetail;
         tRRMBeaconReportRequestedIes reqIes;
      }Beacon;
   }request;
   tANI_U8 sendEmptyBcnRpt;
}tRRMReq, *tpRRMReq;

typedef struct sRRMCaps
{
    tANI_U8  LinkMeasurement: 1;
    tANI_U8      NeighborRpt: 1;
    tANI_U8         parallel: 1;
    tANI_U8         repeated: 1;
    tANI_U8    BeaconPassive: 1;
    tANI_U8     BeaconActive: 1;
    tANI_U8      BeaconTable: 1;
    tANI_U8    BeaconRepCond: 1;
    tANI_U8 FrameMeasurement: 1;
    tANI_U8      ChannelLoad: 1;
    tANI_U8   NoiseHistogram: 1;
    tANI_U8       statistics: 1;
    tANI_U8   LCIMeasurement: 1;
    tANI_U8       LCIAzimuth: 1;
    tANI_U8    TCMCapability: 1;
    tANI_U8     triggeredTCM: 1;
    tANI_U8     APChanReport: 1;
    tANI_U8    RRMMIBEnabled: 1;
    tANI_U8 MeasurementPilotEnabled: 1;
    tANI_U8 NeighborTSFOffset: 1;
    tANI_U8  RCPIMeasurement: 1;
    tANI_U8  RSNIMeasurement: 1;
    tANI_U8 BssAvgAccessDelay: 1;
    tANI_U8 BSSAvailAdmission: 1;
    tANI_U8 AntennaInformation: 1;

    tANI_U8 operatingChanMax;
    tANI_U8 nonOperatingChanMax;
    tANI_U8 MeasurementPilot;
}tRRMCaps, *tpRRMCaps;

typedef struct sRrmPEContext
{
   tANI_U8  rrmEnable;
   //tChannelList APchannelReport;
   tANI_U32   startTSF[2]; //Used during scan/measurement to store the start TSF. this is not used directly in beacon reports.
                           //This value is stored into bssdescription and beacon report gets it from bss decsription.
   tRRMCaps   rrmEnabledCaps;
   tPowerdBm  txMgmtPower;
   tANI_U8  DialogToken; //Dialog token for the request initiated from station.
   tpRRMReq pCurrentReq;
}tRrmPEContext, *tpRrmPEContext;

// 2008 11k spec reference: 18.4.8.5 RCPI Measurement
#define RCPI_LOW_RSSI_VALUE   (-110)
#define RCPI_MAX_VALUE        (220)
#define CALCULATE_RCPI(rssi)  (((rssi) + 110) * 2)


#endif //#if defined __RRMGLOBAL_H
