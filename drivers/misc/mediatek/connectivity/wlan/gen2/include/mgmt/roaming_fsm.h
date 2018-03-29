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

#ifndef _ROAMING_FSM_H
#define _ROAMING_FSM_H

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
/* Roaming Discovery interval, SCAN result need to be updated */
#define ROAMING_DISCOVERY_TIMEOUT_SEC               5	/* Seconds. */

/* #define ROAMING_NO_SWING_RCPI_STEP                  5 //rcpi */
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_ROAMING_FAIL_REASON_T {
	ROAMING_FAIL_REASON_CONNLIMIT = 0,
	ROAMING_FAIL_REASON_NOCANDIDATE,
	ROAMING_FAIL_REASON_NUM
} ENUM_ROAMING_FAIL_REASON_T;

/* events of roaming between driver and firmware */
typedef enum _ENUM_ROAMING_EVENT_T {
	ROAMING_EVENT_START = 0,
	ROAMING_EVENT_DISCOVERY,
	ROAMING_EVENT_ROAM,
	ROAMING_EVENT_FAIL,
	ROAMING_EVENT_ABORT,
	ROAMING_EVENT_NUM
} ENUM_ROAMING_EVENT_T;

#define ROAMING_EVENT_REASON_TX_ERR	BIT(0)
#define ROAMING_EVENT_REASON_RCPI	BIT(1)

typedef struct _ROAMING_PARAM_T {
	UINT_16 u2Event;
	UINT_16 u2Data;
	UINT_16 u2Reason;
} ROAMING_PARAM_T, *P_ROAMING_PARAM_T;

 /**/ typedef enum _ENUM_ROAMING_STATE_T {
	ROAMING_STATE_IDLE = 0,
	ROAMING_STATE_DECISION,
	ROAMING_STATE_DISCOVERY,
	ROAMING_STATE_ROAM,
	ROAMING_STATE_NUM
} ENUM_ROAMING_STATE_T;

typedef struct _ROAMING_INFO_T {
	BOOLEAN fgIsEnableRoaming;

	ENUM_ROAMING_STATE_T eCurrentState;

	OS_SYSTIME rRoamingDiscoveryUpdateTime;

#define ROAMING_ENTRY_TIMEOUT_SKIP_COUNT_MAX		2
	UINT_32 RoamingEntryTimeoutSkipCount;

	UINT_32 DrvRoamingAllow;
} ROAMING_INFO_T, *P_ROAMING_INFO_T;

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

#if CFG_SUPPORT_ROAMING
#define IS_ROAMING_ACTIVE(prAdapter) \
	(prAdapter->rWifiVar.rRoamingInfo.eCurrentState == ROAMING_STATE_ROAM)
#else
#define IS_ROAMING_ACTIVE(prAdapter) FALSE
#endif /* CFG_SUPPORT_ROAMING */

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID roamingFsmInit(IN P_ADAPTER_T prAdapter);

VOID roamingFsmUninit(IN P_ADAPTER_T prAdapter);

VOID roamingFsmSendCmd(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam);

VOID roamingFsmScanResultsUpdate(IN P_ADAPTER_T prAdapter);

VOID roamingFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_ROAMING_STATE_T eNextState);

VOID roamingFsmRunEventStart(IN P_ADAPTER_T prAdapter);

VOID roamingFsmRunEventDiscovery(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam);

VOID roamingFsmRunEventRoam(IN P_ADAPTER_T prAdapter);

VOID roamingFsmRunEventFail(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Reason);

VOID roamingFsmRunEventAbort(IN P_ADAPTER_T prAdapter);

WLAN_STATUS roamingFsmProcessEvent(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _ROAMING_FSM_H */
