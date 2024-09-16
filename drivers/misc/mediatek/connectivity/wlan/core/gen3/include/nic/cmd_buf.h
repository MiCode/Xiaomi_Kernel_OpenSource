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
** Id:
*/

/*
 * ! \file   "cmd_buf.h"
 *  \brief  In this file we define the structure for Command Packet.
 *
 *    In this file we define the structure for Command Packet and the control unit
 *    of MGMT Memory Pool.
 */

#ifndef _CMD_BUF_H
#define _CMD_BUF_H

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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _COMMAND_TYPE {
	COMMAND_TYPE_GENERAL_IOCTL,
	COMMAND_TYPE_NETWORK_IOCTL,
	COMMAND_TYPE_SECURITY_FRAME,
	COMMAND_TYPE_MANAGEMENT_FRAME,
	COMMAND_TYPE_NUM
} COMMAND_TYPE, *P_COMMAND_TYPE;

typedef VOID(*PFN_CMD_DONE_HANDLER) (IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

typedef VOID(*PFN_CMD_TIMEOUT_HANDLER) (IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

struct _CMD_INFO_T {
	QUE_ENTRY_T rQueEntry;

	COMMAND_TYPE eCmdType;

	UINT_16 u2InfoBufLen;	/* This is actual CMD buffer length */
	PUINT_8 pucInfoBuffer;	/* May pointer to structure in prAdapter */
	P_MSDU_INFO_T prMsduInfo;	/* only valid when it's a security/MGMT frame */
	P_NATIVE_PACKET prPacket;	/* only valid when it's a security frame */

	UINT_8 ucBssIndex;
	UINT_8 ucStaRecIndex;	/* only valid when it's a security frame */

	PFN_CMD_DONE_HANDLER pfCmdDoneHandler;
	PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler;

	BOOLEAN fgIsOid;	/* Used to check if we need indicate */

	UINT_8 ucCID;
	BOOLEAN fgSetQuery;
	BOOLEAN fgNeedResp;
	BOOLEAN fgDriverDomainMCR;	/* Access Driver Domain MCR, for CMD_ID_ACCESS_REG only */
	UINT_8 ucCmdSeqNum;
	UINT_32 u4SetInfoLen;	/* Indicate how many byte we read for Set OID */

	/* information indicating by OID/ioctl */
	PVOID pvInformationBuffer;
	UINT_32 u4InformationBufferLength;

	/* private data */
	UINT_32 u4PrivateData;
};

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
#if CFG_DBG_MGT_BUF
#define cmdBufAllocateCmdInfo(_prAdapter, u4Length) \
	cmdBufAllocateCmdInfoX(_prAdapter, u4Length, __FILE__ ":" STRLINE(__LINE__))
#endif
/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID cmdBufInitialize(IN P_ADAPTER_T prAdapter);

#if CFG_DBG_MGT_BUF
P_CMD_INFO_T cmdBufAllocateCmdInfoX(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length, PUINT_8 fileAndLine);
#else
P_CMD_INFO_T cmdBufAllocateCmdInfo(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length);
#endif

VOID cmdBufFreeCmdInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

/*----------------------------------------------------------------------------*/
/* Routines for CMDs                                                          */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanSendSetQueryCmd(IN P_ADAPTER_T prAdapter,
		    UINT_8 ucCID,
		    BOOLEAN fgSetQuery,
		    BOOLEAN fgNeedResp,
		    BOOLEAN fgIsOid,
		    PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		    PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		    UINT_32 u4SetQueryInfoLen,
		    PUINT_8 pucInfoBuffer, OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen);

#if CFG_SUPPORT_TX_BF
WLAN_STATUS
wlanSendSetQueryExtCmd(IN P_ADAPTER_T prAdapter,
		       UINT_8 ucCID,
		       UINT_8 ucExtCID,
		       BOOLEAN fgSetQuery,
		       BOOLEAN fgNeedResp,
		       BOOLEAN fgIsOid,
		       PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		       PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		       UINT_32 u4SetQueryInfoLen,
		       PUINT_8 pucInfoBuffer, OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen);
#endif

VOID cmdBufDumpCmdQueue(P_QUE_T prQueue, CHAR *queName);
#if (CFG_SUPPORT_TRACE_TC4 == 1)
VOID wlanDebugTC4Init(VOID);
VOID wlanDebugTC4Uninit(VOID);
VOID wlanTraceReleaseTcRes(P_ADAPTER_T prAdapter, UINT_16 u2TxRlsCnt, UINT_16 u2Available);
VOID wlanTraceTxCmd(P_CMD_INFO_T prCmd);
VOID wlanDumpTcResAndTxedCmd(PUINT_8 pucBuf, UINT_32 maxLen);
#endif

#if CFG_SUPPORT_MGMT_FRAME_DEBUG
VOID wlanMgmtFrameDebugInit(VOID);
VOID wlanMgmtFrameDebugUnInit(VOID);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _CMD_BUF_H */
