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

#ifndef _WLAN_BOW_H
#define _WLAN_BOW_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "nic/bow.h"
#include "nic/cmd_buf.h"

#if CFG_ENABLE_BT_OVER_WIFI

#if CFG_BOW_TEST
extern UINT_32 g_arBowRevPalPacketTime[32];
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define BOWCMD_STATUS_SUCCESS       0
#define BOWCMD_STATUS_FAILURE       1
#define BOWCMD_STATUS_UNACCEPTED    2
#define BOWCMD_STATUS_INVALID       3
#define BOWCMD_STATUS_TIMEOUT       4

#define BOW_WILDCARD_SSID               "AMP"
#define BOW_WILDCARD_SSID_LEN       3
#define BOW_SSID_LEN                            21

 /* 0: query, 1: setup, 2: destroy */
#define BOW_QUERY_CMD                   0
#define BOW_SETUP_CMD                   1
#define BOW_DESTROY_CMD               2

#define BOW_INITIATOR                   0
#define BOW_RESPONDER                  1

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

typedef struct _BOW_TABLE_T {
	UINT_8 ucAcquireID;
	BOOLEAN fgIsValid;
	ENUM_BOW_DEVICE_STATE eState;
	UINT_8 aucPeerAddress[6];
	/* UINT_8                      ucRole; */
	/* UINT_8                      ucChannelNum; */
	UINT_16 u2Reserved;
} BOW_TABLE_T, *P_BOW_TABLE_T;

typedef WLAN_STATUS(*PFN_BOW_CMD_HANDLE) (P_ADAPTER_T, P_AMPC_COMMAND);

typedef struct _BOW_CMD_T {
	UINT_8 uCmdID;
	PFN_BOW_CMD_HANDLE pfCmdHandle;
} BOW_CMD_T, *P_BOW_CMD_T;

typedef struct _BOW_EVENT_ACTIVITY_REPORT_T {
	UINT_8 ucReason;
	UINT_8 aucReserved;
	UINT_8 aucPeerAddress[6];
} BOW_EVENT_ACTIVITY_REPORT_T, *P_BOW_EVENT_ACTIVITY_REPORT_T;

/*
 *  ucReason:	0: success
 *      1: general failure
 *      2: too much time (> 2/3 second totally) requested for scheduling.
 *      Others: reserved.
 */

typedef struct _BOW_EVENT_SYNC_TSF_T {
	UINT_64 u4TsfTime;
	UINT_32 u4TsfSysTime;
	UINT_32 u4ScoTime;
	UINT_32 u4ScoSysTime;
} BOW_EVENT_SYNC_TSF_T, *P_BOW_EVENT_SYNC_TSF_T;

typedef struct _BOW_ACTIVITY_REPORT_BODY_T {
	UINT_32 u4StartTime;
	UINT_32 u4Duration;
	UINT_32 u4Periodicity;
} BOW_ACTIVITY_REPORT_BODY_T, *P_BOW_ACTIVITY_REPORT_BODY_T;

typedef struct _BOW_ACTIVITY_REPORT_T {
	UINT_8 aucPeerAddress[6];
	UINT_8 ucScheduleKnown;
	UINT_8 ucNumReports;
	BOW_ACTIVITY_REPORT_BODY_T arBowActivityReportBody[MAX_ACTIVITY_REPORT];
} BOW_ACTIVITY_REPORT_T, *P_BOW_ACTIVITY_REPORT_T;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*--------------------------------------------------------------*/
/* Firmware Command Packer                                      */
/*--------------------------------------------------------------*/
WLAN_STATUS
wlanoidSendSetQueryBowCmd(IN P_ADAPTER_T prAdapter,
			  UINT_8 ucCID,
			  BOOLEAN fgSetQuery,
			  BOOLEAN fgNeedResp,
			  PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			  PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			  UINT_32 u4SetQueryInfoLen, PUINT_8 pucInfoBuffer, IN UINT_8 ucSeqNumber);

/*--------------------------------------------------------------*/
/* Command Dispatcher                                           */
/*--------------------------------------------------------------*/
WLAN_STATUS wlanbowHandleCommand(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

/*--------------------------------------------------------------*/
/* Routines to handle command                                   */
/*--------------------------------------------------------------*/
WLAN_STATUS bowCmdGetMacStatus(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdSetupConnection(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdDestroyConnection(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdSetPTK(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdReadRSSI(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdReadLinkQuality(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdShortRangeMode(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

WLAN_STATUS bowCmdGetChannelList(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd);

VOID wlanbowCmdEventSetStatus(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd, IN UINT_8 ucEventBuf);

/*--------------------------------------------------------------*/
/* Callbacks for event indication                               */
/*--------------------------------------------------------------*/
VOID wlanbowCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdEventLinkConnected(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdEventLinkDisconnected(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdEventSetSetupConnection(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdEventReadLinkQuality(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdEventReadRssi(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanbowCmdTimeoutHandler(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

VOID bowStopping(IN P_ADAPTER_T prAdapter);

VOID bowStarting(IN P_ADAPTER_T prAdapter);

VOID bowAssignSsid(IN PUINT_8 pucSsid, IN PUINT_8 pucSsidLen);

BOOLEAN bowValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags);

VOID bowSendBeacon(IN P_ADAPTER_T prAdapter, ULONG ulParam);

VOID bowResponderScan(IN P_ADAPTER_T prAdapter);

VOID bowResponderScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID bowResponderCancelScan(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsChannelExtention);

VOID bowResponderJoin(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);

VOID bowFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
bowIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				ENUM_PARAM_MEDIA_STATE_T eConnectionState, BOOLEAN fgDelayIndication);

VOID bowRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

WLAN_STATUS bowRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

WLAN_STATUS bowRunEventRxDeAuth(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

VOID bowDisconnectLink(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

BOOLEAN bowValidateAssocReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_16 pu2StatusCode);

BOOLEAN
bowValidateAuth(IN P_ADAPTER_T prAdapter,
		IN P_SW_RFB_T prSwRfb, IN PP_STA_RECORD_T pprStaRec, OUT PUINT_16 pu2StatusCode);

VOID bowRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID bowRequestCh(IN P_ADAPTER_T prAdapter);

VOID bowReleaseCh(IN P_ADAPTER_T prAdapter);

VOID bowChGrantedTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParam);

BOOLEAN bowNotifyAllLinkDisconnected(IN P_ADAPTER_T prAdapter);

BOOLEAN bowCheckBowTableIfVaild(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6]);

BOOLEAN bowGetBowTableContent(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBowTableIdx, OUT P_BOW_TABLE_T prBowTable);

BOOLEAN
bowGetBowTableEntryByPeerAddress(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6], OUT PUINT_8 pucBowTableIdx);

BOOLEAN bowGetBowTableFreeEntry(IN P_ADAPTER_T prAdapter, OUT PUINT_8 pucBowTableIdx);

ENUM_BOW_DEVICE_STATE bowGetBowTableState(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6]);

BOOLEAN bowSetBowTableState(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6], IN ENUM_BOW_DEVICE_STATE eState);

BOOLEAN bowSetBowTableContent(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBowTableIdx, IN P_BOW_TABLE_T prBowTable);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif
#endif /* _WLAN_BOW_H */
