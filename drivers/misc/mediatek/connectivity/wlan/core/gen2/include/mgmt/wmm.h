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

#ifndef WMM_HDR_H
#define WMM_HDR_H

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

#define VENDOR_OUI_TYPE_TSRS 8
#define VENDOR_OUI_TYPE_TSM	7
#define VENDOR_OUI_TYPE_EDCALIFE 9
#define WLAN_MAC_ACTION_ADDTS_LEN		(WLAN_MAC_MGMT_HEADER_HTC_LEN + 66)
#define WLAN_MAC_ACTION_DELTS_LEN		(WLAN_MAC_MGMT_HEADER_HTC_LEN + 7)
#define TSPEC_POLICY_CHECK_INTERVAL		2500 /* unit ms */

#define WMM_TS_STATUS_ADMISSION_ACCEPTED 0
#define WMM_TS_STATUS_ADDTS_INVALID_PARAM 1
#define WMM_TS_STATUS_ADDTS_REFUSED 3
#define WMM_TS_STATUS_ASSOC_QOS_FAILED 0xc8
#define WMM_TS_STATUS_POLICY_CONFIG_REFUSED 0xc9
#define WMM_TS_STATUS_ASSOC_INSUFFICIENT_BANDWIDTH 0xca
#define WMM_TS_STATUS_ASSOC_INVALID_PARAM 0xcb

/*
** In WMM, TSs are identified with TIDs values 0 through 7. Any TID may map onto any
** UP and thus onto any AC, however for each AC used between an RA and TA, only the
** following combinations are valid:
** No TS, One uplink TS, one Download link TS, one uplink and one uplink TS, one
** bi-directional TS.
** so maximum 8 TSs are allowed in a RA & TA context.
*/
#define WMM_TSPEC_ID_NUM 8

/*WMM-2.2.11 WMM TSPEC IE*/
#define ELEM_MAX_LEN_WMM_TSPEC 61

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum WMM_ADDTS_STATUS {
	ADDTS_ACCEPTED = 0,
	ADDTS_INVALID_PARAMS = 1,
	ADDTS_REFUSED = 2
};

enum TSPEC_OP_CODE {
	TX_ADDTS_REQ,
	RX_ADDTS_RSP,
	TX_DELTS_REQ,
	RX_DELTS_REQ,
	DISC_DELTS_REQ,
	UPDATE_TS_REQ
};

enum QOS_TS_STATE {
	QOS_TS_INACTIVE,
	QOS_TS_ACTIVE,
	QOS_TS_SETUPING,
	QOS_TS_STATE_MAX
};

enum TSPEC_DIR {
	UPLINK_TS = 0,
	DOWNLINK_TS = 1,
	BI_DIR_TS = 3
};

struct TSPEC_INFO {
	TIMER_T	rAddTsTimer;
	UINT_8 ucToken;
	BOOLEAN fgUapsd;
	ENUM_ACI_T eAC:8;
	enum TSPEC_DIR eDir:8;
	enum QOS_TS_STATE eState:8;
	/* debug information */
	UINT_16 u2MediumTime;
	UINT_32 u4PhyRate;
};

struct TSM_TRIGGER_COND {
	UINT_8 ucCondition;
	UINT_8 ucAvgErrThreshold;
	UINT_8 ucConsecutiveErr;
	UINT_8 ucDelayThreshold;
	UINT_8 ucMeasureCount;
	/* In this time frame, for one condition, only once report is allowed */
	UINT_8 ucTriggerTimeout;
};

struct RM_TSM_REQ {
	UINT_16 u2Duration; /* unit: TUs */
	UINT_8 aucPeerAddr[MAC_ADDR_LEN];
	UINT_8 ucToken;
	UINT_8 ucTID;
	UINT_8 ucACI;
	UINT_8 ucB0Range;  /* 2^(i-1)*ucB0Range =< delay < 2^i * ucB0Range */
	struct TSM_TRIGGER_COND rTriggerCond;
};

struct ACTIVE_RM_TSM_REQ {
	LINK_ENTRY_T rLinkEntry;
	struct RM_TSM_REQ *prTsmReq;
	TIMER_T rTsmTimer;
};

struct WMM_INFO {
	/* A TS is identified uniquely by its TID value within the context of the RA and TA
	** the index is TID for this array
	*/
	struct TSPEC_INFO arTsInfo[WMM_TSPEC_ID_NUM];
	LINK_T rActiveTsmReq;
	OS_SYSTIME rTriggeredTsmRptTime;
	TIMER_T rTsmTimer;
};

struct WMM_ADDTS_RSP_STEP_PARAM {
	UINT_8	ucDlgToken;
	UINT_8	ucStatusCode;
	UINT_8  ucApsd;
	enum TSPEC_DIR eDir:8;
	UINT_16 u2EdcaLifeTime;
	UINT_16 u2MediumTime;
	UINT_32 u4PhyRate;
};

struct MSG_TS_OPERATE {
	MSG_HDR_T rMsgHdr;
	enum TSPEC_OP_CODE eOpCode;
	UINT_8 ucTid;
	PARAM_QOS_TSPEC rTspecParam;
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

extern UINT_8 const aucUp2ACIMap[8];

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define WMM_TSINFO_TRAFFIC_TYPE(tsinfo) (tsinfo & BIT(0))
#define WMM_TSINFO_TSID(tsinfo) ((tsinfo & BITS(1, 4)) >> 1)
#define WMM_TSINFO_DIR(tsinfo) ((tsinfo & BITS(5, 6)) >> 5)
#define WMM_TSINFO_AC(tsinfo) ((tsinfo & BITS(7, 8)) >> 7)
#define WMM_TSINFO_PSB(tsinfo) ((tsinfo & BIT(10)) >> 10)
#define WMM_TSINFO_UP(tsinfo) ((tsinfo & BITS(11, 13)) >> 11)

#define TSM_TRIGGER_CONDITION_ALL BITS(0, 2)
#define TSM_TRIGGER_AVG			  BIT(0)
#define TSM_TRIGGER_CONSECUTIVE	  BIT(1)
#define TSM_TIRGGER_DELAY			  BIT(2)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

VOID wmmFillTsinfo(P_PARAM_QOS_TSINFO prTsInfo, P_UINT_8 pucTsInfo);
VOID wmmSetupTspecTimeOut(P_ADAPTER_T prAdapter, ULONG ulParam);
void wmmStartTsmMeasurement(P_ADAPTER_T prAdapter, ULONG ulParam);
VOID wmmRunEventTSOperate(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr);
BOOLEAN wmmParseQosAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);
BOOLEAN
wmmParseTspecIE(P_ADAPTER_T prAdapter, PUINT_8 pucIE, P_PARAM_QOS_TSPEC  prTspec);
VOID
wmmTspecSteps(P_ADAPTER_T prAdapter, UINT_8 ucTid,
					   enum TSPEC_OP_CODE eOpCode, VOID *prStepParams);
UINT_8 wmmHasActiveTspec(struct WMM_INFO *prWmmInfo);
VOID wmmNotifyDisconnected(P_ADAPTER_T prAdapter);
void wmmComposeTsmRpt(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmdInfo, PUINT_8 pucEventBuf);
VOID wmmInit(IN P_ADAPTER_T prAdapter);
VOID wmmUnInit(IN P_ADAPTER_T prAdapter);
BOOLEAN wmmTsmIsOngoing(P_ADAPTER_T prAdapter);
VOID wmmNotifyDisconnected(P_ADAPTER_T prAdapter);
VOID wmmRemoveAllTsmMeasurement(P_ADAPTER_T prAdapter, BOOLEAN fgOnlyTriggered);
UINT_8 wmmCalculateUapsdSetting(P_ADAPTER_T prAdapter);
UINT_32 wmmDumpActiveTspecs(P_ADAPTER_T prAdapter, PUINT_8 pucBuffer, UINT_16 u2BufferLen);
#endif
