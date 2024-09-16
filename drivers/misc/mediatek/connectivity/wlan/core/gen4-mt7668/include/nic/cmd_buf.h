/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id:
*/

/*! \file   "cmd_buf.h"
 *   \brief  In this file we define the structure for Command Packet.
 *
 *		In this file we define the structure for Command Packet and the control unit
 *   of MGMT Memory Pool.
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

typedef VOID(*PFN_HIF_TX_CMD_DONE_CB) (IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

struct _CMD_INFO_T {
	QUE_ENTRY_T rQueEntry;

	COMMAND_TYPE eCmdType;

	UINT_16 u2InfoBufLen;	/* This is actual CMD buffer length */
	PUINT_8 pucInfoBuffer;	/* May pointer to structure in prAdapter */
	P_MSDU_INFO_T prMsduInfo;	/* only valid when it's a security/MGMT frame */
	P_NATIVE_PACKET prPacket;	/* only valid when it's a security frame */

	PFN_CMD_DONE_HANDLER pfCmdDoneHandler;
	PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler;
	PFN_HIF_TX_CMD_DONE_CB pfHifTxCmdDoneCb;

	BOOLEAN fgIsOid;	/* Used to check if we need indicate */

	UINT_8 ucCID;
	BOOLEAN fgSetQuery;
	BOOLEAN fgNeedResp;
	UINT_8 ucCmdSeqNum;
	UINT_32 u4SetInfoLen;	/* Indicate how many byte we read for Set OID */

	/* information indicating by OID/ioctl */
	PVOID pvInformationBuffer;
	UINT_32 u4InformationBufferLength;

	/* private data */
	UINT_32 u4PrivateData;

	/* TXD/TXP pointer/len for hif tx copy */
	UINT_32 u4TxdLen;
	UINT_32 u4TxpLen;
	PUINT_8 pucTxd;
	PUINT_8 pucTxp;
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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID cmdBufInitialize(IN P_ADAPTER_T prAdapter);

P_CMD_INFO_T cmdBufAllocateCmdInfo(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length);

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

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _CMD_BUF_H */
