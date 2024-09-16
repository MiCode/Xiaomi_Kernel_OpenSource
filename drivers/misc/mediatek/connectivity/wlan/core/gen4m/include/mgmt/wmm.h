/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WMM_HDR_H
#define WMM_HDR_H

#define VENDOR_OUI_TYPE_TSRS 8
#define VENDOR_OUI_TYPE_TSM	7
#define VENDOR_OUI_TYPE_EDCALIFE 9
#define WLAN_MAC_ACTION_ADDTS_LEN (WLAN_MAC_MGMT_HEADER_HTC_LEN + 66)
#define WLAN_MAC_ACTION_DELTS_LEN (WLAN_MAC_MGMT_HEADER_HTC_LEN + 7)
#define TSPEC_POLICY_CHECK_INTERVAL		2500 /* unit ms */

#define WMM_TS_STATUS_ADMISSION_ACCEPTED 0
#define WMM_TS_STATUS_ADDTS_INVALID_PARAM 1
#define WMM_TS_STATUS_ADDTS_REFUSED 3
#define WMM_TS_STATUS_ASSOC_QOS_FAILED 0xc8
#define WMM_TS_STATUS_POLICY_CONFIG_REFUSED 0xc9
#define WMM_TS_STATUS_ASSOC_INSUFFICIENT_BANDWIDTH 0xca
#define WMM_TS_STATUS_ASSOC_INVALID_PARAM 0xcb

#define CFG_SUPPORT_SOFT_ACM 1 /* do Admission Control in driver */
/*
 * In WMM, TSs are identified with TIDs values 0 through 7. Any TID may
 * map onto any UP and thus onto any AC, however for each AC used between
 * an RA and TA, only the following combinations are valid:
 * No TS, One uplink TS, one Download link TS, one uplink and one uplink
 * TS, one bi-directional TS.
 * so maximum 8 TSs are allowed in a RA & TA context.
 */
#define WMM_TSPEC_ID_NUM 8

/*WMM-2.2.11 WMM TSPEC IE*/
#define ELEM_MAX_LEN_WMM_TSPEC 61

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
	struct TIMER	rAddTsTimer;
	uint8_t ucToken;
	u_int8_t fgUapsd;
	enum ENUM_ACI eAC:8;
	enum TSPEC_DIR eDir:8;
	enum QOS_TS_STATE eState:8;
	/* debug information */
	uint16_t u2MediumTime;
	uint32_t u4PhyRate;
	uint8_t ucTid;
};

struct TSM_TRIGGER_COND {
	uint8_t ucCondition;
	uint8_t ucAvgErrThreshold;
	uint8_t ucConsecutiveErr;
	uint8_t ucDelayThreshold;
	uint8_t ucMeasureCount;
	/* In this time frame, for one condition, only once report is allowed */
	uint8_t ucTriggerTimeout;
};

struct RM_TSM_REQ {
	uint16_t u2Duration; /* unit: TUs */
	uint8_t aucPeerAddr[MAC_ADDR_LEN];
	uint8_t ucToken;
	uint8_t ucTID;
	uint8_t ucACI;
	uint8_t ucB0Range;  /* 2^(i-1)*ucB0Range =< delay < 2^i * ucB0Range */
	struct TSM_TRIGGER_COND rTriggerCond;
};

struct ACTIVE_RM_TSM_REQ {
	struct LINK_ENTRY rLinkEntry;
	struct RM_TSM_REQ *prTsmReq;
	struct TIMER rTsmTimer;
	uint8_t ucBssIdx;
};

#if CFG_SUPPORT_SOFT_ACM
struct SOFT_ACM_CTRL {
	uint32_t u4RemainTime;
	uint32_t u4AdmittedTime;
	uint32_t u4IntervalEndSec;
	uint16_t u2DeqNum;
};
#endif

struct WMM_INFO {
	/* A TS is identified uniquely by its TID value within the context of
	 ** the RA and TA
	 ** the index is TID for this array
	 */
	struct TSPEC_INFO arTsInfo[WMM_TSPEC_ID_NUM];
	struct LINK rActiveTsmReq;
	OS_SYSTIME rTriggeredTsmRptTime;
	struct TIMER rTsmTimer;
#if CFG_SUPPORT_SOFT_ACM
	struct TIMER rAcmDeqTimer;
	struct SOFT_ACM_CTRL arAcmCtrl[ACI_NUM]; /* 0 ~ 3, BE, BK, VI, VO */
#endif
};

struct WMM_ADDTS_RSP_STEP_PARAM {
	uint8_t	ucDlgToken;
	uint8_t	ucStatusCode;
	uint8_t  ucApsd;
	enum TSPEC_DIR eDir:8;
	uint16_t u2EdcaLifeTime;
	uint16_t u2MediumTime;
	uint32_t u4PhyRate;
};

struct MSG_TS_OPERATE {
	struct MSG_HDR rMsgHdr;
	enum TSPEC_OP_CODE eOpCode;
	uint8_t ucTid;
	struct PARAM_QOS_TSPEC rTspecParam;
	uint8_t ucBssIdx;
};

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

extern uint8_t const aucUp2ACIMap[8];
void wmmFillTsinfo(struct PARAM_QOS_TSINFO *prTsInfo, uint8_t *pucTsInfo);
void wmmSetupTspecTimeOut(struct ADAPTER *prAdapter, unsigned long ulParam);
void wmmStartTsmMeasurement(struct ADAPTER *prAdapter, unsigned long ulParam,
	uint8_t ucBssIndex);
void wmmRunEventTSOperate(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr);
u_int8_t wmmParseQosAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb);
u_int8_t wmmParseTspecIE(struct ADAPTER *prAdapter, uint8_t *pucIE,
			 struct PARAM_QOS_TSPEC *prTspec);
void wmmTspecSteps(struct ADAPTER *prAdapter, uint8_t ucTid,
	enum TSPEC_OP_CODE eOpCode, void *prStepParams, uint8_t ucBssIndex);
uint8_t wmmHasActiveTspec(struct WMM_INFO *prWmmInfo);
void wmmNotifyDisconnected(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
void wmmComposeTsmRpt(struct ADAPTER *prAdapter, struct CMD_INFO *prCmdInfo,
		      uint8_t *pucEventBuf);
void wmmInit(IN struct ADAPTER *prAdapter);
void wmmUnInit(IN struct ADAPTER *prAdapter);
u_int8_t wmmTsmIsOngoing(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
void wmmRemoveAllTsmMeasurement(struct ADAPTER *prAdapter,
	u_int8_t fgOnlyTriggered, uint8_t ucBssIndex);
uint8_t wmmCalculateUapsdSetting(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
uint32_t wmmDumpActiveTspecs(struct ADAPTER *prAdapter, uint8_t *pucBuffer,
	uint16_t u2BufferLenu, uint8_t ucBssIndex);
#if CFG_SUPPORT_SOFT_ACM
u_int8_t wmmAcmCanDequeue(struct ADAPTER *prAdapter, uint8_t ucAc,
	uint32_t u4PktTxTime, uint8_t ucBssIndex);
void wmmAcmTxStatistic(struct ADAPTER *prAdapter, uint8_t ucAc,
		       uint32_t u4Remain, uint16_t u2DeqNum);

uint32_t wmmCalculatePktUsedTime(struct BSS_INFO *prBssInfo,
				 struct STA_RECORD *prStaRec,
				 uint16_t u2PktLen);
void wmmPrepareFastUsedTimeCal(struct ADAPTER *prAdapter);
uint32_t wmmFastCalPktUsedTime(struct WMM_INFO *prWmmInfo, uint8_t ucAc,
			       uint16_t u2PktLen);
#endif /* CFG_SUPPORT_SOFT_ACM */
#endif
