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

#ifndef _CNM_SCAN_H
#define _CNM_SCAN_H

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
#define SCN_CHANNEL_DWELL_TIME_MIN_MSEC         12
#define SCN_CHANNEL_DWELL_TIME_EXT_MSEC         98

#define SCN_TOTAL_PROBEREQ_NUM_FOR_FULL         3
#define SCN_SPECIFIC_PROBEREQ_NUM_FOR_FULL      1

#define SCN_TOTAL_PROBEREQ_NUM_FOR_PARTIAL      2
#define SCN_SPECIFIC_PROBEREQ_NUM_FOR_PARTIAL   1

#define SCN_INTERLACED_CHANNEL_GROUPS_NUM       3	/* Used by partial scan */

#define SCN_PARTIAL_SCAN_NUM                    3

#define SCN_PARTIAL_SCAN_IDLE_MSEC              100

#define	MAXIMUM_OPERATION_CHANNEL_LIST	        46

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* The type of Scan Source */
typedef enum _ENUM_SCN_REQ_SOURCE_T {
	SCN_REQ_SOURCE_HEM = 0,
	SCN_REQ_SOURCE_NET_FSM,
	SCN_REQ_SOURCE_ROAMING,	/* ROAMING Module is independent of AIS FSM */
	SCN_REQ_SOURCE_OBSS,	/* 2.4G OBSS scan */
	SCN_REQ_SOURCE_NUM
} ENUM_SCN_REQ_SOURCE_T, *P_ENUM_SCN_REQ_SOURCE_T;

typedef enum _ENUM_SCAN_PROFILE_T {
	SCAN_PROFILE_FULL = 0,
	SCAN_PROFILE_PARTIAL,
	SCAN_PROFILE_VOIP,
	SCAN_PROFILE_FULL_2G4,
	SCAN_PROFILE_NUM
} ENUM_SCAN_PROFILE_T, *P_ENUM_SCAN_PROFILE_T;

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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#if 0
VOID cnmScanInit(VOID);

VOID cnmScanRunEventScanRequest(IN P_MSG_HDR_T prMsgHdr);

BOOLEAN cnmScanRunEventScanAbort(IN P_MSG_HDR_T prMsgHdr);

VOID cnmScanProfileSelection(VOID);

VOID cnmScanProcessStart(VOID);

VOID cnmScanProcessStop(VOID);

VOID cnmScanRunEventReqAISAbsDone(IN P_MSG_HDR_T prMsgHdr);

VOID cnmScanRunEventCancelAISAbsDone(IN P_MSG_HDR_T prMsgHdr);

VOID cnmScanPartialScanTimeout(UINT_32 u4Param);

VOID cnmScanRunEventScnFsmComplete(IN P_MSG_HDR_T prMsgHdr);
#endif

#endif /* _CNM_SCAN_H */
