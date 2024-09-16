/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _RRM_H
#define _RRM_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

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
#define RM_REPORT_FRAME_MAX_LENGTH	(1600 - NIC_TX_DESC_AND_PADDING_LENGTH)
/* beacon request mode definition */
#define RM_BCN_REQ_PASSIVE_MODE                     0
#define RM_BCN_REQ_ACTIVE_MODE                      1
#define RM_BCN_REQ_TABLE_MODE                       2

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

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
	struct PARAM_SCAN_REQUEST_ADV rScanRequest;
	uint8_t aucScanIEBuf[MAX_IE_LENGTH];
	u_int8_t fgExist;
};

/* Beacon RM related parameters */
struct BCN_RM_PARAMS {
	enum BCN_RM_STATE eState;
	struct NORMAL_SCAN_PARAMS rNormalScan;

	uint8_t token;
	uint8_t lastIndication;
	u8 ssid[ELEM_MAX_LEN_SSID];
	size_t ssidLen;
	enum BEACON_REPORT_DETAIL reportDetail;
	uint8_t *reportIeIds;
	uint8_t reportIeIdsLen;
	uint8_t *apChannels;
	uint8_t apChannelsLen;
};

struct RM_BEACON_REPORT_PARAMS {
	uint8_t ucChannel;
	uint8_t ucRCPI;
	uint8_t ucRSNI;
	uint8_t ucAntennaID;
	uint8_t ucFrameInfo;
	uint8_t aucBcnFixedField[12];
};

struct RM_MEASURE_REPORT_ENTRY {
	struct LINK_ENTRY rLinkEntry;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint16_t u2MeasReportLen;
	uint8_t *pucMeasReport;
};

struct RADIO_MEASUREMENT_REQ_PARAMS {
	/* Remain Request Elements Length, started at prMeasElem. if it is 0,
	 * means RM is done
	 */
	uint16_t u2RemainReqLen;
	uint16_t u2ReqIeBufLen;
	struct IE_MEASUREMENT_REQ *prCurrMeasElem;
	OS_SYSTIME rStartTime;
	uint16_t u2Repetitions;
	uint8_t *pucReqIeBuf;
	enum RM_REQ_PRIORITY ePriority;
	u_int8_t fgRmIsOngoing;
	u_int8_t fgInitialLoop;
	OS_SYSTIME rScanStartTime;

	struct BCN_RM_PARAMS rBcnRmParam;
};

struct RADIO_MEASUREMENT_REPORT_PARAMS {
	/* the total length of Measurement Report elements */
	uint16_t u2ReportFrameLen;
	uint8_t *pucReportFrameBuff;
	/* Variables to collect report */
	struct LINK rReportLink; /* a link to save received report entry */
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void rrmParamInit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void rrmParamUninit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void rrmProcessNeighborReportResonse(struct ADAPTER *prAdapter,
				     struct WLAN_ACTION_FRAME *prAction,
				     struct SW_RFB *prSwRfb);

void rrmTxNeighborReportRequest(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				struct SUB_ELEMENT_LIST *prSubIEs);

void rrmGenerateRRMEnabledCapIE(IN struct ADAPTER *prAdapter,
				IN struct MSDU_INFO *prMsduInfo);

void rrmProcessRadioMeasurementRequest(struct ADAPTER *prAdapter,
				       struct SW_RFB *prSwRfb);

void rrmProcessLinkMeasurementRequest(struct ADAPTER *prAdapter,
				      struct WLAN_ACTION_FRAME *prAction);

void rrmFillRrmCapa(uint8_t *pucCapa);

void rrmStartNextMeasurement(struct ADAPTER *prAdapter, u_int8_t fgNewStarted,
	uint8_t ucBssIndex);


u_int8_t rrmFillScanMsg(struct ADAPTER *prAdapter,
			struct MSG_SCN_SCAN_REQ_V2 *prMsg);

void rrmDoBeaconMeasurement(struct ADAPTER *prAdapter, unsigned long ulParam);

void rrmTxNeighborReportRequest(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				struct SUB_ELEMENT_LIST *prSubIEs);

void rrmTxRadioMeasurementReport(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void rrmFreeMeasurementResources(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

enum RM_REQ_PRIORITY rrmGetRmRequestPriority(uint8_t *pucDestAddr);

void rrmRunEventProcessNextRm(struct ADAPTER *prAdapter,
			      struct MSG_HDR *prMsgHdr);

void rrmScheduleNextRm(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void rrmUpdateBssTimeTsf(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc);

void rrmCollectBeaconReport(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prBssDesc, IN uint8_t ucBssIndex);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _RRM_H */
