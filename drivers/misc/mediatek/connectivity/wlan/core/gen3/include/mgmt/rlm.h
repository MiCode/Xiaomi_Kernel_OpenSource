/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/rlm.h#2
*/

/*
 * ! \file   "rlm.h"
 *  \brief
 */

#ifndef _RLM_H
#define _RLM_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

extern BOOLEAN g_bIcapEnable;
extern BOOLEAN g_bCaptureDone;
extern UINT_16 g_u2DumpIndex;
#if CFG_SUPPORT_QA_TOOL
extern UINT_32 g_au4Offset[2][2];
extern UINT_32 g_au4IQData[20][1024];
extern UINT_32 g_au4I0Data[1][408000];
extern UINT_32 g_au4Q0Data[1][408000];
#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define ELEM_EXT_CAP_DEFAULT_VAL \
	(ELEM_EXT_CAP_20_40_COEXIST_SUPPORT /*| ELEM_EXT_CAP_PSMP_CAP*/)

#if CFG_SUPPORT_RX_STBC
#define FIELD_HT_CAP_INFO_RX_STBC   HT_CAP_INFO_RX_STBC_1_SS
#else
#define FIELD_HT_CAP_INFO_RX_STBC   HT_CAP_INFO_RX_STBC_NO_SUPPORTED
#endif

#if CFG_SUPPORT_RX_SGI
#define FIELD_HT_CAP_INFO_SGI_20M   HT_CAP_INFO_SHORT_GI_20M
#define FIELD_HT_CAP_INFO_SGI_40M   HT_CAP_INFO_SHORT_GI_40M
#else
#define FIELD_HT_CAP_INFO_SGI_20M   0
#define FIELD_HT_CAP_INFO_SGI_40M   0
#endif

#if CFG_SUPPORT_RX_HT_GF
#define FIELD_HT_CAP_INFO_HT_GF     HT_CAP_INFO_HT_GF
#else
#define FIELD_HT_CAP_INFO_HT_GF     0
#endif

#define HT_CAP_INFO_DEFAULT_VAL \
	(HT_CAP_INFO_SUP_CHNL_WIDTH | HT_CAP_INFO_DSSS_CCK_IN_40M)

#define AMPDU_PARAM_DEFAULT_VAL \
	(AMPDU_PARAM_MAX_AMPDU_LEN_64K | AMPDU_PARAM_MSS_NO_RESTRICIT)

#define SUP_MCS_TX_DEFAULT_VAL \
	SUP_MCS_TX_SET_DEFINED	/* TX defined and TX/RX equal (TBD) */

#if CFG_SUPPORT_MFB
#define FIELD_HT_EXT_CAP_MFB    HT_EXT_CAP_MCS_FEEDBACK_BOTH
#else
#define FIELD_HT_EXT_CAP_MFB    HT_EXT_CAP_MCS_FEEDBACK_NO_FB
#endif

#if CFG_SUPPORT_RX_RDG
#define FIELD_HT_EXT_CAP_RDR    HT_EXT_CAP_RD_RESPONDER
#else
#define FIELD_HT_EXT_CAP_RDR    0
#endif

#if CFG_SUPPORT_MFB || CFG_SUPPORT_RX_RDG
#define FIELD_HT_EXT_CAP_HTC    HT_EXT_CAP_HTC_SUPPORT
#else
#define FIELD_HT_EXT_CAP_HTC    0
#endif

#define HT_EXT_CAP_DEFAULT_VAL \
	(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE | \
	 FIELD_HT_EXT_CAP_MFB | FIELD_HT_EXT_CAP_HTC | \
	 FIELD_HT_EXT_CAP_RDR)

#define TX_BEAMFORMING_CAP_DEFAULT_VAL        0

#if CFG_SUPPORT_BFEE
#define TX_BEAMFORMING_CAP_BFEE \
	(TXBF_RX_NDP_CAPABLE | \
	 TXBF_EXPLICIT_COMPRESSED_FEEDBACK_IMMEDIATE_CAPABLE | \
	 TXBF_MINIMAL_GROUPING_1_2_3_CAPABLE | \
	 TXBF_COMPRESSED_TX_ANTENNANUM_4_SUPPORTED | \
	 TXBF_CHANNEL_ESTIMATION_4STS_CAPABILITY)
#else
#define TX_BEAMFORMING_CAP_BFEE        0
#endif

#define ASEL_CAP_DEFAULT_VAL                        0

/* Define bandwidth from user setting */
#define CONFIG_BW_20_40M            0
#define CONFIG_BW_20M               1	/* 20MHz only */

/* Radio Measurement Request Mode definition */
#define RM_REQ_MODE_PARALLEL_BIT                    BIT(0)
#define RM_REQ_MODE_ENABLE_BIT                      BIT(1)
#define RM_REQ_MODE_REQUEST_BIT                     BIT(2)
#define RM_REQ_MODE_REPORT_BIT                      BIT(3)
#define RM_REQ_MODE_DURATION_MANDATORY_BIT          BIT(4)
#define RM_REP_MODE_LATE                            BIT(0)
#define RM_REP_MODE_INCAPABLE                       BIT(1)
#define RM_REP_MODE_REFUSED                         BIT(2)

/* Radio Measurement Report Frame Max Length */
#define RM_REPORT_FRAME_MAX_LENGTH                  1600
#define RM_BCN_REPORT_SUB_ELEM_MAX_LENGTH           224
/* beacon request mode definition */
#define RM_BCN_REQ_PASSIVE_MODE                     0
#define RM_BCN_REQ_ACTIVE_MODE                      1
#define RM_BCN_REQ_TABLE_MODE                       2

#define RLM_INVALID_POWER_LIMIT                     -127 /* dbm */

#define RLM_MAX_TX_PWR		20	/* dbm */
#define RLM_MIN_TX_PWR		8	/* dbm */

#if CFG_SUPPORT_802_11AC
#if CFG_SUPPORT_BFEE
#define FIELD_VHT_CAP_INFO_BF \
	(VHT_CAP_INFO_SU_BEAMFORMEE_CAPABLE | \
	     VHT_CAP_INFO_COMPRESSED_STEERING_NUMBER_OF_BEAMFORMER_ANTENNAS_4_SUPPOERTED)
#else
#define FIELD_VHT_CAP_INFO_BF     0
#endif

#define VHT_CAP_INFO_DEFAULT_VAL \
	(VHT_CAP_INFO_MAX_MPDU_LEN_3K | \
	 (AMPDU_PARAM_MAX_AMPDU_LEN_1024K << VHT_CAP_INFO_MAX_AMPDU_LENGTH_OFFSET))

#define VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE			0
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
struct SUB_ELEMENT_LIST {
	struct SUB_ELEMENT_LIST *prNext;
	struct SUB_ELEMENT_T rSubIE;
};

enum BCN_RM_STATE {
	RM_NO_REQUEST,
	RM_ON_GOING,
	RM_WAITING, /*waiting normal scan done */
};

enum RM_REQ_PRIORITY {
	RM_PRI_BROADCAST,
	RM_PRI_MULTICAST,
	RM_PRI_UNICAST
};

struct NORMAL_SCAN_PARAMS {
	PARAM_SSID_T arSSID[SCN_SSID_MAX_NUM];
	UINT_8 aucScanIEBuf[MAX_IE_LENGTH];
	UINT_32 u4IELen;
	UINT_8 ucSsidNum;
	BOOLEAN fgExist;
	BOOLEAN fgFull2Partial;
	struct _PARAM_SCAN_RANDOM_MAC_ADDR_T rScanRandMacAddr;
};

/* Beacon RM related parameters */
struct BCN_RM_PARAMS {
	BOOLEAN fgExistBcnReq;
	enum BCN_RM_STATE eState;
	struct NORMAL_SCAN_PARAMS rNormalScan;
};

struct RADIO_MEASUREMENT_REQ_PARAMS {
	/* Remain Request Elements Length, started at prMeasElem. if it is 0, means RM is done */
	UINT_16 u2RemainReqLen;
	UINT_16 u2ReqIeBufLen;
	P_IE_MEASUREMENT_REQ_T prCurrMeasElem;
	OS_SYSTIME rStartTime;
	UINT_16 u2Repetitions;
	PUINT_8 pucReqIeBuf;
	enum RM_REQ_PRIORITY ePriority;
	BOOLEAN fgRmIsOngoing;
	BOOLEAN fgInitialLoop;

	struct BCN_RM_PARAMS rBcnRmParam;
};

struct RADIO_MEASUREMENT_REPORT_PARAMS {
	UINT_16 u2ReportFrameLen; /* the total length of Measurement Report elements */
	PUINT_8 pucReportFrameBuff;
	/* Variables to collect report */
	LINK_T rReportLink; /* a link to save received report entry */
	LINK_T rFreeReportLink;
};

#if CFG_SUPPORT_DFS
struct CHANNEL_SWITCH_ANNOUNCE_PARAMS {
	BOOLEAN fgReadyToSwitch;
	BOOLEAN fgHasSCOIE;
	BOOLEAN fgHasWideBandIE;
	UINT_8 ucNewChannel;
	ENUM_CHNL_EXT_T eNewSCO;
	UINT_8 ucNewVhtBw;
	UINT_8 ucNewVhtS1;
	UINT_8 ucNewVhtS2;
};
#endif

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define RM_EXIST_REPORT(_prRmReportParam) \
	(((struct RADIO_MEASUREMENT_REPORT_PARAMS *)_prRmReportParam)->u2ReportFrameLen == \
	OFFSET_OF(ACTION_RM_REPORT_FRAME, aucInfoElem))

/* It is used for RLM module to judge if specific network is valid
 * Note: Ad-hoc mode of AIS is not included now. (TBD)
 */
#define RLM_NET_PARAM_VALID(_prBssInfo) \
	(IS_BSS_ACTIVE(_prBssInfo) && \
	 ((_prBssInfo)->eConnectionState == PARAM_MEDIA_STATE_CONNECTED || \
	  (_prBssInfo)->eCurrentOPMode == OP_MODE_ACCESS_POINT || \
	  (_prBssInfo)->eCurrentOPMode == OP_MODE_IBSS || \
	  IS_BSS_BOW(_prBssInfo)) \
	)

#define RLM_NET_IS_11N(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11N)
#define RLM_NET_IS_11GN(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11GN)
#define RLM_NET_IS_11AC(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)

/* The bandwidth modes are not used anymore. They represent if AP
 * can use 20/40 bandwidth, not all modes. (20110411)
 */
#define RLM_AP_IS_BW_40_ALLOWED(_prAdapter, _prBssInfo) \
	(((_prBssInfo)->eBand == BAND_2G4 && \
	(_prAdapter)->rWifiVar.rConnSettings.uc2G4BandwidthMode \
	== CONFIG_BW_20_40M) || \
	((_prBssInfo)->eBand == BAND_5G && \
	(_prAdapter)->rWifiVar.rConnSettings.uc5GBandwidthMode \
	== CONFIG_BW_20_40M))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID rlmFsmEventInit(P_ADAPTER_T prAdapter);

VOID rlmFsmEventUninit(P_ADAPTER_T prAdapter);

VOID rlmReqGenerateHtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmReqGenerateExtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateHtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateExtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateHtOpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateErpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateBssMaxIdlePeriodIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmGenerateMTKOuiIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

BOOLEAN rlmParseCheckMTKOuiIE(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN PUINT_32 pu4Cap);

VOID rlmProcessBcn(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmProcessAssocRsp(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmFillSyncCmdParam(P_CMD_SET_BSS_RLM_PARAM_T prCmdBody, P_BSS_INFO_T prBssInfo);

VOID rlmSyncOperationParams(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

VOID rlmBssInitForAPandIbss(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

VOID rlmProcessAssocReq(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmBssAborted(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

#if CFG_SUPPORT_TDLS
UINT_32
rlmFillHtCapIEByParams(BOOLEAN fg40mAllowed,
		       BOOLEAN fgShortGIDisabled,
		       UINT_8 u8SupportRxSgi20,
		       UINT_8 u8SupportRxSgi40, UINT_8 u8SupportRxGf, ENUM_OP_MODE_T eCurrentOPMode, UINT_8 *pOutBuf);

UINT_32 rlmFillHtCapIEByAdapter(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, UINT_8 *pOutBuf);

UINT_32 rlmFillVhtCapIEByAdapter(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, UINT_8 *pOutBuf);

#endif

#if CFG_SUPPORT_802_11AC
VOID rlmReqGenerateVhtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateVhtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmRspGenerateVhtOpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmFillVhtOpIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo);
#endif

#if CFG_SUPPORT_DFS
VOID rlmProcessSpecMgtAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

VOID
rlmSendOpModeNotificationFrame(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, UINT_8 ucChannelWidth, UINT_8 ucNss);

#endif
VOID rlmProcessNeighborReportResonse(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction, UINT_16 u2PacketLen);
VOID rlmTxNeighborReportRequest(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, struct SUB_ELEMENT_LIST *prSubIEs);

VOID rlmGenerateRRMEnabledCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID rlmGeneratePowerCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID rlmProcessRadioMeasurementRequest(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

VOID rlmProcessLinkMeasurementRequest(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction);

VOID rlmProcessNeighborReportResonse(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction, UINT_16 u2PacketLen);

VOID rlmFillRrmCapa(PUINT_8 pucCapa);

VOID rlmSetMaxTxPwrLimit(IN P_ADAPTER_T prAdapter, INT_8 cLimit, UINT_8 ucEnable);

VOID rlmStartNextMeasurement(P_ADAPTER_T prAdapter, BOOLEAN fgNewStarted);

BOOLEAN rlmBcnRmRunning(P_ADAPTER_T prAdapter);

BOOLEAN rlmFillScanMsg(P_ADAPTER_T prAdapter, struct _MSG_SCN_SCAN_REQ_V3_T *prMsg);

VOID rlmDoBeaconMeasurement(P_ADAPTER_T prAdapter, ULONG ulParam);

VOID rlmTxNeighborReportRequest(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, struct SUB_ELEMENT_LIST *prSubIEs);

VOID rlmTxRadioMeasurementReport(P_ADAPTER_T prAdapter);

VOID rlmCancelRadioMeasurement(P_ADAPTER_T prAdapter);

enum RM_REQ_PRIORITY rlmGetRmRequestPriority(PUINT_8 pucDestAddr);

VOID rlmRunEventProcessNextRm(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr);

VOID rlmScheduleNextRm(P_ADAPTER_T prAdapter);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#ifndef _lint
static __KAL_INLINE__ VOID rlmDataTypeCheck(VOID)
{
}
#endif /* _lint */

#endif /* _RLM_H */
