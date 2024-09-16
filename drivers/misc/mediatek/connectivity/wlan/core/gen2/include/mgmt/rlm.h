/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
	(HT_CAP_INFO_SUP_CHNL_WIDTH | FIELD_HT_CAP_INFO_HT_GF | \
	 FIELD_HT_CAP_INFO_SGI_20M | FIELD_HT_CAP_INFO_SGI_40M | \
	 FIELD_HT_CAP_INFO_RX_STBC | HT_CAP_INFO_DSSS_CCK_IN_40M)

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

#define TX_BEAMFORMING_CAP_DEFAULT_VAL              0
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
	UINT_8 aucRandomMac[MAC_ADDR_LEN];
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

typedef enum _ENUM_NET_ACTIVE_SRC_T {
	NET_ACTIVE_SRC_NONE = 0,
	NET_ACTIVE_SRC_CONNECT = 1,
	NET_ACTIVE_SRC_SCAN = 2,
	NET_ACTIVE_SRC_SCHED_SCAN = 4,
	NET_ACTIVE_SRC_NUM
} ENUM_NET_ACTIVE_SRC_T, *P_ENUM_NET_ACTIVE_SRC_T;
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
	  RLM_NET_IS_BOW(_prBssInfo)) \
	)

#define RLM_NET_IS_11N(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11N)
#define RLM_NET_IS_11GN(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11GN)

/* This macro is used to sweep all 3 networks */
#define RLM_NET_FOR_EACH(_ucNetIdx) \
	for ((_ucNetIdx) = 0; \
		(_ucNetIdx) < NETWORK_TYPE_INDEX_NUM; \
		(_ucNetIdx)++)

/* This macro is used to sweep all networks excluding BOW */
#if CFG_ENABLE_BT_OVER_WIFI
    /* Note: value of enum NETWORK_TYPE_BOW_INDEX is validated in
     *       rlmStuctureCheck().
     */
#define RLM_NET_FOR_EACH_NO_BOW(_ucNetIdx) \
	for ((_ucNetIdx) = 0; \
		(_ucNetIdx) < NETWORK_TYPE_BOW_INDEX; \
		(_ucNetIdx)++)

#define RLM_NET_IS_BOW(_prBssInfo) \
	((_prBssInfo)->ucNetTypeIndex == NETWORK_TYPE_BOW_INDEX)

#else
#define RLM_NET_FOR_EACH_NO_BOW(_ucNetIdx)  RLM_NET_FOR_EACH(_ucNetIdx)
#define RLM_NET_IS_BOW(_prBssInfo)          (FALSE)

#endif /* end of CFG_ENABLE_BT_OVER_WIFI */

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
VOID rlmGenerateMTKOuiIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);

VOID rlmProcessBcn(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmProcessAssocRsp(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmFillSyncCmdParam(P_CMD_SET_BSS_RLM_PARAM_T prCmdBody, P_BSS_INFO_T prBssInfo);

VOID rlmSyncOperationParams(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

VOID rlmBssInitForAPandIbss(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

VOID rlmProcessAssocReq(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

VOID rlmBssAborted(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

UINT32
rlmFillHtCapIEByParams(BOOLEAN fg40mAllowed,
		       BOOLEAN fgShortGIDisabled,
		       UINT_8 u8SupportRxSgi20,
		       UINT_8 u8SupportRxSgi40,
		       UINT_8 u8SupportRxGf, UINT_8 u8SupportRxSTBC, ENUM_OP_MODE_T eCurrentOPMode, UINT_8 *pOutBuf);

UINT32 rlmFillHtOpIeBody(P_BSS_INFO_T prBssInfo, UINT_8 *pFme);

#if CFG_SUPPORT_DFS		/* Add by Enlai */
VOID rlmProcessSpecMgtAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

VOID rlmProcessChannelSwitchIE(P_ADAPTER_T prAdapter, P_IE_CHANNEL_SWITCH_T prChannelSwitchIE);
#endif

VOID
rlmTxRateEnhanceConfig(
	P_ADAPTER_T	prAdapter
	);

VOID
rlmCmd(
	P_GLUE_INFO_T	prGlueInfo,
	UINT_8		*prInBuf,
	UINT_32	u4InBufLen
	);
VOID rlmProcessNeighborReportResonse(
	P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction, UINT_16 u2PacketLen);
VOID rlmTxNeighborReportRequest(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec,
		struct SUB_ELEMENT_LIST *prSubIEs);

VOID rlmGernerateRRMEnabledCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID rlmGerneratePowerCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID rlmProcessRadioMeasurementRequest(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

VOID rlmProcessLinkMeasurementRequest(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction);

VOID rlmProcessNeighborReportResonse(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction, UINT_16 u2PacketLen);

VOID rlmFillRrmCapa(PUINT_8 pucCapa);

VOID rlmSetMaxTxPwrLimit(IN P_ADAPTER_T prAdapter, INT_8 cLimit, UINT_8 ucEnable);

VOID rlmStartNextMeasurement(P_ADAPTER_T prAdapter, BOOLEAN fgNewStarted);

BOOLEAN rlmBcnRmRunning(P_ADAPTER_T prAdapter);

BOOLEAN rlmFillScanMsg(P_ADAPTER_T prAdapter, P_MSG_SCN_SCAN_REQ prMsg);

VOID rlmDoBeaconMeasurement(P_ADAPTER_T prAdapter, ULONG ulParam);

VOID rlmTxNeighborReportRequest(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, struct SUB_ELEMENT_LIST *prSubIEs);

VOID rlmTxRadioMeasurementReport(P_ADAPTER_T prAdapter);

VOID rlmCancelRadioMeasurement(P_ADAPTER_T prAdapter);

enum RM_REQ_PRIORITY rlmGetRmRequestPriority(PUINT_8 pucDestAddr);

VOID rlmRunEventProcessNextRm(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr);

VOID rlmScheduleNextRm(P_ADAPTER_T prAdapter);
#if CFG_SUPPORT_P2P_ECSA
void rlmGenActionCSHdr(u8 *buf,
			u8 *da, u8 *sa, u8 *bssid,
			u8 category, u8 action);

void rlmGenActionCSA(u8 *buf,
			u8 mode,
			u8 channel,
			u8 count,
			u8 sco);

void rlmGenActionECSA(u8 *buf,
			u8 mode,
			u8 channel,
			u8 count,
			u8 op_class);
VOID rlmGenerateCSAIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);
VOID rlmGenerateECSAIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo);
VOID  rlmFreqToChannelExt(unsigned int freq,
			int sec_channel,
			u8 *op_class, u8 *channel);
#endif

#if CFG_SUPPORT_RLM_ACT_NETWORK
VOID rlmActivateNetwork(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx,
			ENUM_NET_ACTIVE_SRC_T eNetActiveSrcIdx);

VOID rlmDeactivateNetwork(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx,
			ENUM_NET_ACTIVE_SRC_T eNetActiveSrcIdx);
#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#ifndef _lint
static inline VOID rlmDataTypeCheck(VOID)
{
#if CFG_ENABLE_BT_OVER_WIFI
	DATA_STRUCT_INSPECTING_ASSERT(NETWORK_TYPE_AIS_INDEX < NETWORK_TYPE_BOW_INDEX);

#if CFG_ENABLE_WIFI_DIRECT
	DATA_STRUCT_INSPECTING_ASSERT(NETWORK_TYPE_P2P_INDEX < NETWORK_TYPE_BOW_INDEX);
#endif
#endif

}
#endif /* _lint */

#endif /* _RLM_H */
