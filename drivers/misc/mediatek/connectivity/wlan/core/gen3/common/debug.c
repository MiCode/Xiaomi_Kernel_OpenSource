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

#include "precomp.h"

#if (CFG_SUPPORT_DEBUG_STATISTICS == 1)
struct _PKT_STATUS_ENTRY {
	UINT_8 u1Type;
	UINT_16 u2IpId;
	UINT_8 status;
};

struct _PKT_STATUS_RECORD {
	struct _PKT_STATUS_ENTRY *pTxPkt;
	struct _PKT_STATUS_ENTRY *pRxPkt;
	UINT_32 u4TxIndex;
	UINT_32 u4RxIndex;
};

#define PKT_STATUS_BUF_MAX_NUM 450
#define PKT_STATUS_MSG_GROUP_RANGE 80
#define PKT_STATUS_MSG_LENGTH 900

static PKT_STATUS_RECORD grPktStaRec;
#endif

#if (CFG_SUPPORT_TRACE_TC4 == 1)
struct COMMAND {
	UINT_8 ucCID;
	BOOLEAN fgSetQuery;
	BOOLEAN fgNeedResp;
	UINT_8 ucCmdSeqNum;
};

struct SECURITY_FRAME {
	UINT_16 u2EthType;
	UINT_16 u2Reserved;
};

struct MGMT_FRAME {
	UINT_16 u2FrameCtl;
	UINT_16 u2DurationID;
};

typedef struct _TC_RES_RELEASE_ENTRY {
	UINT_64 u8RelaseTime;
	UINT_32 u4RelCID;
	UINT_16	u2Tc4RelCnt;
	UINT_16	u2AvailableTc4;
} TC_RES_RELEASE_ENTRY, *P_TC_RES_RELEASE_ENTRY;

typedef struct _CMD_TRACE_ENTRY {
	UINT_64 u8TxTime;
	COMMAND_TYPE eCmdType;
	union {
		struct COMMAND rCmd;
		struct SECURITY_FRAME rSecFrame;
		struct MGMT_FRAME rMgmtFrame;
	} u;
} CMD_TRACE_ENTRY, *P_CMD_TRACE_ENTRY;

#define TC_RELEASE_TRACE_BUF_MAX_NUM 100
#define TXED_CMD_TRACE_BUF_MAX_NUM 100

static P_TC_RES_RELEASE_ENTRY gprTcReleaseTraceBuffer;
static P_CMD_TRACE_ENTRY gprCmdTraceEntry;

VOID wlanDebugTC4Init(VOID)
{
	/* debug for command/tc4 resource begin */
	gprTcReleaseTraceBuffer =
		kalMemAlloc(TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprTcReleaseTraceBuffer, TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY));
	gprCmdTraceEntry = kalMemAlloc(TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprCmdTraceEntry, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY));
	/* debug for command/tc4 resource end */
}

VOID wlanDebugTC4Uninit(VOID)
{
	/* debug for command/tc4 resource begin */
	kalMemFree(gprTcReleaseTraceBuffer, PHY_MEM_TYPE,
			TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY));
	kalMemFree(gprCmdTraceEntry, PHY_MEM_TYPE, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY));
	/* debug for command/tc4 resource end */
}

VOID wlanTraceTxCmd(P_CMD_INFO_T prCmd)
{
	static UINT_16 u2CurEntry;
	P_CMD_TRACE_ENTRY prCurCmd = &gprCmdTraceEntry[u2CurEntry];

	prCurCmd->u8TxTime = sched_clock();
	prCurCmd->eCmdType = prCmd->eCmdType;
	if (prCmd->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
		P_WLAN_MAC_MGMT_HEADER_T prMgmt = (P_WLAN_MAC_MGMT_HEADER_T)prCmd->prMsduInfo->prPacket;

		prCurCmd->u.rMgmtFrame.u2FrameCtl = prMgmt->u2FrameCtrl;
		prCurCmd->u.rMgmtFrame.u2DurationID = prMgmt->u2Duration;
	} else if (prCmd->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
		PUINT_8 pucPkt = (PUINT_8)((struct sk_buff *)prCmd->prPacket)->data;

		prCurCmd->u.rSecFrame.u2EthType =
				(pucPkt[ETH_TYPE_LEN_OFFSET] << 8) | (pucPkt[ETH_TYPE_LEN_OFFSET + 1]);
	} else {
		prCurCmd->u.rCmd.ucCID = prCmd->ucCID;
		prCurCmd->u.rCmd.ucCmdSeqNum = prCmd->ucCmdSeqNum;
		prCurCmd->u.rCmd.fgNeedResp = prCmd->fgNeedResp;
		prCurCmd->u.rCmd.fgSetQuery = prCmd->fgSetQuery;
	}
	u2CurEntry++;
	if (u2CurEntry == TC_RELEASE_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}

VOID wlanTraceReleaseTcRes(P_ADAPTER_T prAdapter, UINT_16 u2TxRlsCnt, UINT_16 u2Available)
{
	static UINT_16 u2CurEntry;
	P_TC_RES_RELEASE_ENTRY prCurBuf = &gprTcReleaseTraceBuffer[u2CurEntry];
	/* Here we should wait FW to find right way to trace release CID */
	/* HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurBuf->u4RelCID); */
	prCurBuf->u8RelaseTime = sched_clock();
	prCurBuf->u2Tc4RelCnt = u2TxRlsCnt;
	prCurBuf->u2AvailableTc4 = u2Available;
	u2CurEntry++;
	if (u2CurEntry == TXED_CMD_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}

VOID wlanDumpTcResAndTxedCmd(PUINT_8 pucBuf, UINT_32 maxLen)
{
	UINT_16 i = 0;
	P_CMD_TRACE_ENTRY prCmd = gprCmdTraceEntry;
	P_TC_RES_RELEASE_ENTRY prTcRel = gprTcReleaseTraceBuffer;

	if (pucBuf) {
		int bufLen = 0;

		for (; i < TXED_CMD_TRACE_BUF_MAX_NUM/2; i++) {
			bufLen = snprintf(pucBuf, maxLen,
				"%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x\n",
				i*2, prCmd[i*2].u8TxTime, prCmd[i*2].eCmdType, *(PUINT_32)(&prCmd[i*2].u.rCmd.ucCID),
				i*2+1, prCmd[i*2+1].u8TxTime, prCmd[i*2+1].eCmdType,
				*(PUINT_32)(&prCmd[i*2+1].u.rCmd.ucCID));
			if (bufLen <= 0)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
		for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM/2; i++) {
			bufLen = snprintf(pucBuf, maxLen,
				"%d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d CID %08x\n",
				i*2, prTcRel[i*2].u8RelaseTime, prTcRel[i*2].u2Tc4RelCnt, prTcRel[i*2].u2AvailableTc4,
				prTcRel[i*2].u4RelCID,
				i*2+1, prTcRel[i*2+1].u8RelaseTime, prTcRel[i*2+1].u2Tc4RelCnt,
				prTcRel[i*2+1].u2AvailableTc4, prTcRel[i*2+1].u4RelCID);
			if (bufLen <= 0)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
	} else {
		for (; i < TXED_CMD_TRACE_BUF_MAX_NUM/4; i++) {
			LOG_FUNC("%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x; ",
				i*4, prCmd[i*4].u8TxTime, prCmd[i*4].eCmdType,
				*(PUINT_32)(&prCmd[i*4].u.rCmd.ucCID),
				i*4+1, prCmd[i*4+1].u8TxTime, prCmd[i*4+1].eCmdType,
				*(PUINT_32)(&prCmd[i*4+1].u.rCmd.ucCID));
			LOG_FUNC("%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x\n",
				i*4+2, prCmd[i*4+2].u8TxTime, prCmd[i*4+2].eCmdType,
				*(PUINT_32)(&prCmd[i*4+2].u.rCmd.ucCID),
				i*4+3, prCmd[i*4+3].u8TxTime, prCmd[i*4+3].eCmdType,
				*(PUINT_32)(&prCmd[i*4+3].u.rCmd.ucCID));
		}
		for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM/4; i++) {
			LOG_FUNC(
				"%d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x;",
				i*4, prTcRel[i*4].u8RelaseTime, prTcRel[i*4].u2Tc4RelCnt,
				prTcRel[i*4].u2AvailableTc4, prTcRel[i*4].u4RelCID,
				i*4+1, prTcRel[i*4+1].u8RelaseTime, prTcRel[i*4+1].u2Tc4RelCnt,
				prTcRel[i*4+1].u2AvailableTc4, prTcRel[i*4+1].u4RelCID);
			LOG_FUNC(
				" %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x\n",
				i*4+2, prTcRel[i*4+2].u8RelaseTime, prTcRel[i*4+2].u2Tc4RelCnt,
				prTcRel[i*4+2].u2AvailableTc4, prTcRel[i*4+2].u4RelCID,
				i*4+3, prTcRel[i*4+3].u8RelaseTime, prTcRel[i*4+3].u2Tc4RelCnt,
				prTcRel[i*4+3].u2AvailableTc4, prTcRel[i*4+3].u4RelCID);
		}
	}
}
#endif

#if (CFG_SUPPORT_DEBUG_STATISTICS == 1)
VOID wlanPktStatusDebugTraceInfoARP(UINT_8 status, UINT_8 eventType, UINT_16 u2ArpOpCode, PUINT_8 pucPkt)
{
	if (eventType == PKT_TX)
		status = 0xFF;
	wlanPktStatusDebugTraceInfo(status, eventType, ETH_P_ARP, 0, 0, u2ArpOpCode, pucPkt);
}

VOID wlanPktStatusDebugTraceInfoIP(UINT_8 status, UINT_8 eventType, UINT_8 ucIpProto, UINT_16 u2IpId, PUINT_8 pucPkt)
{
	if (eventType == PKT_TX)
		status = 0xFF;
	wlanPktStatusDebugTraceInfo(status, eventType, ETH_P_IP, ucIpProto, u2IpId, 0, pucPkt);
}

VOID wlanPktStatusDebugTraceInfo(UINT_8 status, UINT_8 eventType
	, UINT_16 u2EtherType, UINT_8 ucIpProto, UINT_16 u2IpId, UINT_16 u2ArpOpCode, PUINT_8 pucPkt)
{
	struct _PKT_STATUS_ENTRY *prPktSta = NULL;
	UINT_32 index;

	DBGLOG(TX, LOUD, "PKT id = 0x%02x, status =%d, Proto = %d, type =%d\n"
		, u2IpId, status, ucIpProto, eventType);
	do {
		if (grPktStaRec.pTxPkt == NULL) {
			DBGLOG(TX, ERROR, "pTxStaPkt is null point !");
			break;
		}

		/* debug for Package info begin */
		if (eventType == PKT_TX) {
			prPktSta = &grPktStaRec.pTxPkt[grPktStaRec.u4TxIndex];
			grPktStaRec.u4TxIndex++;
			if (grPktStaRec.u4TxIndex == PKT_STATUS_BUF_MAX_NUM) {
				DBGLOG(TX, INFO, "grPktStaRec.u4TxIndex reset");
				grPktStaRec.u4TxIndex = 0;
			}
		} else if (eventType == PKT_RX) {
			prPktSta = &grPktStaRec.pRxPkt[grPktStaRec.u4RxIndex];
			grPktStaRec.u4RxIndex++;
			if (grPktStaRec.u4RxIndex == PKT_STATUS_BUF_MAX_NUM) {
				DBGLOG(TX, INFO, "grPktStaRec.u4RxIndex reset");
				grPktStaRec.u4RxIndex = 0;
			}
		}

		if (prPktSta) {
			prPktSta->u1Type = kalGetPktEtherType(pucPkt);
			prPktSta->status = status;
			prPktSta->u2IpId = u2IpId;
		}

		/* Update tx status */
		if (eventType == PKT_TX_DONE) {
			/* Support Ethernet type = IP*/
			if (u2EtherType == ETH_P_IP) {
				for (index = 0; index < PKT_STATUS_BUF_MAX_NUM; index++) {
					if (grPktStaRec.pTxPkt[index].u2IpId == u2IpId) {
						grPktStaRec.pTxPkt[index].status = status;
						DBGLOG(TX, INFO, "Status: PKT_TX_DONE match\n");
						break;
					}
				}
			}
		}
	} while (FALSE);
}

VOID wlanPktStatusDebugDumpInfo(P_ADAPTER_T prAdapter)
{
	UINT_32 i;
	UINT_32 index;
	UINT_32 offsetMsg;
	struct _PKT_STATUS_ENTRY *prPktInfo;
	UINT_8 pucMsg[PKT_STATUS_MSG_LENGTH];
	UINT_32 u4PktCnt;

	do {

		if (grPktStaRec.pTxPkt == NULL || grPktStaRec.pRxPkt == NULL)
			break;

		if (grPktStaRec.u4TxIndex == 0 && grPktStaRec.u4RxIndex == 0)
			break;

		DBGLOG(TX, INFO, "Pkt dump: TxCnt %d, RxCnt %d\n", grPktStaRec.u4TxIndex, grPktStaRec.u4RxIndex);
		offsetMsg = 0;
		/* start dump pkt info of tx/rx by decrease timestap */
		for (i = 0 ; i < 2 ; i++) {
			if (i == 0)
				u4PktCnt = grPktStaRec.u4TxIndex;
			else
				u4PktCnt = grPktStaRec.u4RxIndex;

			for (index = 0; index < u4PktCnt; index++) {
				if (i == 0)
					prPktInfo = &grPktStaRec.pTxPkt[index];
				else
					prPktInfo = &grPktStaRec.pRxPkt[index];
				/*ucIpProto = 0x01 ICMP */
				/*ucIpProto = 0x11 UPD */
				/*ucIpProto = 0x06 TCP */
				offsetMsg += kalSnprintf(pucMsg + offsetMsg
				, PKT_STATUS_MSG_LENGTH
				, "%d,%02x,%x"
				, prPktInfo->u1Type
				, prPktInfo->u2IpId
				, prPktInfo->status);

				if (((index + 1) % PKT_STATUS_MSG_GROUP_RANGE == 0) || (index == (u4PktCnt - 1))) {
					if (i == 0)
						DBGLOG(TX, INFO, "%s\n", pucMsg);
					else if (i == 1)
						DBGLOG(RX, INFO, "%s\n", pucMsg);

					offsetMsg = 0;
					kalMemSet(pucMsg, '\0', PKT_STATUS_MSG_LENGTH);
				}
			}
		}
	} while (FALSE);
	u4PktCnt = grPktStaRec.u4TxIndex = 0;
	u4PktCnt = grPktStaRec.u4RxIndex = 0;
}
#endif

VOID wlanDebugTC4AndPktInit(VOID)
{
#if (CFG_SUPPORT_TRACE_TC4 == 1)
	wlanDebugTC4Init();
#endif
#if (CFG_SUPPORT_DEBUG_STATISTICS == 1)
	/* debug for package info begin */
	grPktStaRec.pTxPkt = kalMemAlloc(PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktStaRec.pTxPkt, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4TxIndex = 0;
	grPktStaRec.pRxPkt = kalMemAlloc(PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktStaRec.pRxPkt, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4RxIndex = 0;
	/* debug for package info end */
#endif
}

VOID wlanDebugTC4AndPktUninit(VOID)
{
#if (CFG_SUPPORT_TRACE_TC4 == 1)
	wlanDebugTC4Uninit();
#endif
#if (CFG_SUPPORT_DEBUG_STATISTICS == 1)

	/* debug for package status info begin */
	kalMemFree(grPktStaRec.pTxPkt, VIR_MEM_TYPE, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4TxIndex = 0;
	kalMemFree(grPktStaRec.pRxPkt, VIR_MEM_TYPE, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4RxIndex = 0;
	/* debug for package status info end */
#endif
}

static UINT_32 gu4LogLevel[ENUM_WIFI_LOG_MODULE_NUM];

VOID wlanDbgLogLevelInit(VOID)
{
	UINT_32 u4LogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
#if DBG
	/* Adjust log level to extreme in DBG mode */
	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, ENUM_WIFI_LOG_LEVEL_EXTREME);
	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, ENUM_WIFI_LOG_LEVEL_EXTREME);
#else
	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, ENUM_WIFI_LOG_LEVEL_OFF);
	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, ENUM_WIFI_LOG_LEVEL_OFF);
#endif /* DBG */

	/* Set driver level first, and set fw log level until fw is ready */
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, &u4LogLevel);
	wlanDbgSetLogLevelImpl(NULL,
			       ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			       ENUM_WIFI_LOG_MODULE_DRIVER,
			       u4LogLevel);
}

VOID wlanDbgLogLevelUninit(VOID)
{
	UINT_8 i;
	UINT_32 u4DriverLevel = ENUM_WIFI_LOG_LEVEL_OFF;
	UINT_32 u4FWLevel = ENUM_WIFI_LOG_LEVEL_OFF;

	/* re-enable printk too much detection */
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, &u4DriverLevel);
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, &u4FWLevel);
	if (u4DriverLevel > ENUM_WIFI_LOG_LEVEL_OFF ||
			u4FWLevel > ENUM_WIFI_LOG_LEVEL_OFF) {
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
		set_logtoomuch_enable(1);
#endif
		wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, ENUM_WIFI_LOG_LEVEL_OFF);
		wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, ENUM_WIFI_LOG_LEVEL_OFF);
		for (i = 0; i < DBG_MODULE_NUM; i++)
			aucDebugModule[i] = DBG_LOG_LEVEL_OFF;
	}

}

UINT_32 wlanDbgLevelUiSupport(IN P_ADAPTER_T prAdapter, UINT_32 u4Version, UINT_32 ucModule)
{
	UINT_32 u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_DISABLE;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		switch (ucModule) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
#if CFG_SUPPORT_WIFI_FW_LOG_UI
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
#else
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_DISABLE;
#endif
			break;
		}
		break;
	default:
		break;
	}

	return u4Enable;
}

UINT_32 wlanDbgGetLogLevelImpl(IN P_ADAPTER_T prAdapter, UINT_32 u4Version,
		UINT_32 ucModule)
{
	UINT_32 u4Level = ENUM_WIFI_LOG_LEVEL_OFF;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgGetGlobalLogLevel(ucModule, &u4Level);
		break;
	default:
		break;
	}
	return u4Level;
}

VOID wlanDbgSetLogLevelImpl(IN P_ADAPTER_T prAdapter, UINT_32 u4Version,
		UINT_32 u4Module, UINT_32 u4level)
{
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	UINT_32 u4DriverLevel = ENUM_WIFI_LOG_LEVEL_OFF;
	UINT_32 u4FWLevel = ENUM_WIFI_LOG_LEVEL_OFF;
#endif
	if (u4level >= ENUM_WIFI_LOG_LEVEL_NUM)
		return;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgSetGlobalLogLevel(u4Module, u4level);
		switch (u4Module) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
		{
			UINT_8 i;
			UINT_32 u4DriverLogMask;

			if (u4level == ENUM_WIFI_LOG_LEVEL_OFF)
				u4DriverLogMask = DBG_LOG_LEVEL_OFF;
			else if (u4level == ENUM_WIFI_LOG_LEVEL_DEFAULT)
				u4DriverLogMask = DBG_LOG_LEVEL_DEFAULT;
			else
				u4DriverLogMask = DBG_LOG_LEVEL_EXTREME;

			for (i = 0; i < DBG_MODULE_NUM; i++)
				aucDebugModule[i] = u4DriverLogMask & DBG_CLASS_MASK;
		}
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
		{
			struct CMD_EVENT_LOG_LEVEL cmd;

			kalMemZero(&cmd, sizeof(struct CMD_EVENT_LOG_LEVEL));
			cmd.u4Version = u4Version;
			cmd.u4LogLevel = u4level;

			wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_WIFI_LOG_LEVEL,
				   TRUE,
				   FALSE,
				   FALSE,
				   NULL,
				   NULL,
				   sizeof(struct CMD_EVENT_LOG_LEVEL), (PUINT_8)&cmd, NULL, 0);
		}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, &u4DriverLevel);
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, &u4FWLevel);
	if (u4DriverLevel > ENUM_WIFI_LOG_LEVEL_OFF ||
		u4FWLevel > ENUM_WIFI_LOG_LEVEL_OFF) {
		DBGLOG(OID, INFO,
			"Disable printk to much. driver: %d, fw: %d\n",
			u4DriverLevel,
			u4FWLevel);
		set_logtoomuch_enable(0);
	}
#endif
}

VOID wlanDbgLevelSync(VOID)
{
	UINT_8 i = 0;
	UINT_32 u4PorcLogLevel = DBG_CLASS_MASK;
	UINT_32 u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;

	/* get the lowest level as module's level */
	for (i = 0; i < DBG_MODULE_NUM; i++)
		u4PorcLogLevel &= aucDebugModule[i];

	if (u4PorcLogLevel & ENUM_WIFI_LOG_LEVEL_EXTREME)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
	else if (u4PorcLogLevel & ENUM_WIFI_LOG_LEVEL_DEFAULT)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
	else
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;

	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, u4DriverLogLevel);
}

BOOLEAN wlanDbgGetGlobalLogLevel(UINT_32 u4Module, UINT_32 *pu4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER && u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	*pu4Level = gu4LogLevel[u4Module];
	return TRUE;
}
BOOLEAN wlanDbgSetGlobalLogLevel(UINT_32 u4Module, UINT_32 u4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER && u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	gu4LogLevel[u4Module] = u4Level;
	return TRUE;
}

#if CFG_SUPPORT_MGMT_FRAME_DEBUG
#define MGMG_FRAME_MAX_LENGTH	(WLAN_MAC_MGMT_HEADER_LEN + CFG_CFG80211_IE_BUF_LEN)
#define MGMG_FRAME_MAX_NUM 6

struct MGMT_FRAME_ENTRY {
	UINT_32 u4Length;
	UINT_8 aucFrameBody[MGMG_FRAME_MAX_LENGTH];
};

struct SAA_MGMT_RECORD {
	struct MGMT_FRAME_ENTRY	*pMgmtFrame;
	UINT_32 u4MgmtIndex;
};

static struct SAA_MGMT_RECORD grSaaMgmtRec;
#endif

#if CFG_SUPPORT_MGMT_FRAME_DEBUG
VOID wlanMgmtFrameDebugInit(VOID)
{
	grSaaMgmtRec.pMgmtFrame = kalMemAlloc(MGMG_FRAME_MAX_NUM * sizeof(struct MGMT_FRAME_ENTRY), VIR_MEM_TYPE);
	if (grSaaMgmtRec.pMgmtFrame)
		kalMemZero(grSaaMgmtRec.pMgmtFrame, MGMG_FRAME_MAX_NUM * sizeof(struct MGMT_FRAME_ENTRY));
	grSaaMgmtRec.u4MgmtIndex = 0;
}

VOID wlanMgmtFrameDebugUnInit(VOID)
{
	if (grSaaMgmtRec.pMgmtFrame)
		kalMemFree(grSaaMgmtRec.pMgmtFrame, VIR_MEM_TYPE, MGMG_FRAME_MAX_NUM * sizeof(struct MGMT_FRAME_ENTRY));
	grSaaMgmtRec.u4MgmtIndex = 0;
}

VOID wlanMgmtFrameDebugReset(VOID)
{
	UINT_8 uIndex = 0;
	struct MGMT_FRAME_ENTRY *prMgmtFrame = NULL;

	if (!grSaaMgmtRec.pMgmtFrame) {
		DBGLOG(SAA, WARN, "grSaaMgmtRec.pMgmtFrame is NULL!");
		return;
	}

	for (uIndex = 0; uIndex < MGMG_FRAME_MAX_NUM; uIndex++) {
		prMgmtFrame = &grSaaMgmtRec.pMgmtFrame[uIndex];
		kalMemZero(prMgmtFrame->aucFrameBody, MGMG_FRAME_MAX_LENGTH);
		prMgmtFrame->u4Length = 0;
	}
	grSaaMgmtRec.u4MgmtIndex = 0;
}

VOID wlanMgmtFrameDebugAdd(IN PUINT_8 pucStartAddr, IN UINT_32 u4Length)
{
	struct MGMT_FRAME_ENTRY *prMgmtFrame = NULL;

	if (!grSaaMgmtRec.pMgmtFrame) {
		DBGLOG(SAA, WARN, "grSaaMgmtRec.pMgmtFrame is NULL!");
		return;
	}

	if (grSaaMgmtRec.u4MgmtIndex == MGMG_FRAME_MAX_NUM) {
		DBGLOG(SAA, INFO, "Management frame buffer full, dump old records.");
		wlanMgmtFrameDebugDump();
	}

	if (grSaaMgmtRec.u4MgmtIndex < MGMG_FRAME_MAX_NUM) {
		prMgmtFrame = &grSaaMgmtRec.pMgmtFrame[grSaaMgmtRec.u4MgmtIndex];
		if (u4Length < MGMG_FRAME_MAX_LENGTH) {
			kalMemCopy(prMgmtFrame->aucFrameBody, pucStartAddr, u4Length);
			prMgmtFrame->u4Length = u4Length;
		} else {
			kalMemCopy(prMgmtFrame->aucFrameBody, pucStartAddr, MGMG_FRAME_MAX_LENGTH);
			prMgmtFrame->u4Length = MGMG_FRAME_MAX_LENGTH;
		}
		grSaaMgmtRec.u4MgmtIndex++;
	}
}

VOID wlanMgmtFrameDebugDump(VOID)
{
	UINT_8 uIndex = 0;
	struct MGMT_FRAME_ENTRY *prMgmtFrame = NULL;

	if (!grSaaMgmtRec.pMgmtFrame) {
		DBGLOG(SAA, WARN, "grSaaMgmtRec.pMgmtFrame is NULL!");
		return;
	}

	if (grSaaMgmtRec.u4MgmtIndex > MGMG_FRAME_MAX_NUM) {
		DBGLOG(SAA, WARN, "grSaaMgmtRec.u4MgmtIndex(%d) exceeds maximum value!",
						grSaaMgmtRec.u4MgmtIndex);
		grSaaMgmtRec.u4MgmtIndex = MGMG_FRAME_MAX_NUM;
	}

	for (uIndex = 0; uIndex < grSaaMgmtRec.u4MgmtIndex; uIndex++) {
		prMgmtFrame = &grSaaMgmtRec.pMgmtFrame[uIndex];
		dumpMemory8(prMgmtFrame->aucFrameBody, prMgmtFrame->u4Length);
		kalMemZero(prMgmtFrame->aucFrameBody, MGMG_FRAME_MAX_LENGTH);
		prMgmtFrame->u4Length = 0;
	}
	grSaaMgmtRec.u4MgmtIndex = 0;
}
#endif

VOID wlanPrintFwLog(PUINT_8 pucLogContent, UINT_16 u2MsgSize, UINT_8 ucMsgType)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#define OLD_LOG_FUNC LOG_FUNC
#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME "wlan_gen3_fw"
#define LOG_FUNC pr_info

	if (u2MsgSize > DEBUG_MSG_SIZE_MAX - 1) {
		LOG_FUNC("Firmware Log Size(%d) is too large, type %d\n", u2MsgSize, ucMsgType);
		return;
	}
	switch (ucMsgType) {
	case DEBUG_MSG_TYPE_ASCII:
		pucLogContent[u2MsgSize] = '\0';
		LOG_FUNC("%s\n", pucLogContent);
		break;
	case DEBUG_MSG_TYPE_MEM8:
		DBGLOG_MEM8(RX, INFO, pucLogContent, u2MsgSize);
		break;
	default:
		DBGLOG_MEM32(RX, INFO, (PUINT_32)pucLogContent, u2MsgSize);
		break;
	}

#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
#define LOG_FUNC OLD_LOG_FUNC
#undef OLD_KBUILD_MODNAME
#undef OLD_LOG_FUNC
}

/* Begin: Functions used to breakdown packet jitter, for test case VoE 5.7 */
static VOID wlanSetBE32(UINT_32 u4Val, PUINT_8 pucBuf)
{
	PUINT_8 littleEn = (PUINT_8)&u4Val;

	pucBuf[0] = littleEn[3];
	pucBuf[1] = littleEn[2];
	pucBuf[2] = littleEn[1];
	pucBuf[3] = littleEn[0];
}

VOID wlanFillTimestamp(P_ADAPTER_T prAdapter, PVOID pvPacket, UINT_8 ucPhase)
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;
	PUINT_8 pucEth = NULL;
	UINT_32 u4Length = 0;
	PUINT_8 pucUdp = NULL;
	struct timeval tval;

	if (!prAdapter || !prAdapter->rDebugInfo.fgVoE5_7Test || !skb)
		return;
	pucEth = skb->data;
	u4Length = skb->len;
	if (u4Length < 200 ||
		((pucEth[ETH_TYPE_LEN_OFFSET] << 8) | (pucEth[ETH_TYPE_LEN_OFFSET + 1])) != ETH_P_IPV4)
		return;
	if (pucEth[ETH_HLEN+9] != IP_PRO_UDP)
		return;
	pucUdp = &pucEth[ETH_HLEN+28];
	if (kalStrnCmp(pucUdp, "1345678", 7))
		return;
	do_gettimeofday(&tval);
	switch (ucPhase) {
	case PHASE_XMIT_RCV: /* xmit */
		pucUdp += 20;
		break;
	case PHASE_ENQ_QM: /* enq */
		pucUdp += 28;
		break;
	case PHASE_HIF_TX: /* tx */
		pucUdp += 36;
		break;
	}
	wlanSetBE32(tval.tv_sec, pucUdp);
	wlanSetBE32(tval.tv_usec, pucUdp+4);
}
/* End: Functions used to breakdown packet jitter, for test case VoE 5.7 */
