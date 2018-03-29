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
	UINT_16	ucTc4RelCnt;
	UINT_8	ucAvailableTc4;
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

VOID wlanTraceReleaseTcRes(P_ADAPTER_T prAdapter, PUINT_16 aucTxRlsCnt, UINT_8 ucAvaliable)
{
	static UINT_16 u2CurEntry;
	P_TC_RES_RELEASE_ENTRY prCurBuf = &gprTcReleaseTraceBuffer[u2CurEntry];
	/* Here we should wait FW to find right way to trace release CID */
	/* HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurBuf->u4RelCID); */
	prCurBuf->u8RelaseTime = sched_clock();
	prCurBuf->ucTc4RelCnt = aucTxRlsCnt[TC4_INDEX];
	prCurBuf->ucAvailableTc4 = ucAvaliable;
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
				i*2, prTcRel[i*2].u8RelaseTime, prTcRel[i*2].ucTc4RelCnt, prTcRel[i*2].ucAvailableTc4,
				prTcRel[i*2].u4RelCID,
				i*2+1, prTcRel[i*2+1].u8RelaseTime, prTcRel[i*2+1].ucTc4RelCnt,
				prTcRel[i*2+1].ucAvailableTc4, prTcRel[i*2+1].u4RelCID);
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
				i*4, prTcRel[i*4].u8RelaseTime, prTcRel[i*4].ucTc4RelCnt,
				prTcRel[i*4].ucAvailableTc4, prTcRel[i*4].u4RelCID,
				i*4+1, prTcRel[i*4+1].u8RelaseTime, prTcRel[i*4+1].ucTc4RelCnt,
				prTcRel[i*4+1].ucAvailableTc4, prTcRel[i*4+1].u4RelCID);
			LOG_FUNC(
				" %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x\n",
				i*4+2, prTcRel[i*4+2].u8RelaseTime, prTcRel[i*4+2].ucTc4RelCnt,
				prTcRel[i*4+2].ucAvailableTc4, prTcRel[i*4+2].u4RelCID,
				i*4+3, prTcRel[i*4+3].u8RelaseTime, prTcRel[i*4+3].ucTc4RelCnt,
				prTcRel[i*4+3].ucAvailableTc4, prTcRel[i*4+3].u4RelCID);
		}
	}
}
#endif

VOID wlanPrintFwLog(PUINT_8 pucLogContent, UINT_16 u2MsgSize, UINT_8 ucMsgType)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#define OLD_LOG_FUNC LOG_FUNC
#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME "wlan_gen3_fw"
#define LOG_FUNC pr_debug

	if (u2MsgSize > DEBUG_MSG_SIZE_MAX - 1) {
		DBGLOG(SW4, TRACE, "Firmware Log Size(%d) is too large, type %d\n", u2MsgSize, ucMsgType);
		return;
	}
	switch (ucMsgType) {
	case DEBUG_MSG_TYPE_ASCII:
		pucLogContent[u2MsgSize] = '\0';
		DBGLOG(SW4, TRACE, "%s\n", pucLogContent);
		break;
	case DEBUG_MSG_TYPE_MEM8:
		DBGLOG_MEM8(SW4, TRACE, pucLogContent, u2MsgSize);
		break;
	default:
		DBGLOG_MEM32(SW4, TRACE, (PUINT_32)pucLogContent, u2MsgSize);
		break;
	}

#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
#define LOG_FUNC OLD_LOG_FUNC
#undef OLD_KBUILD_MODNAME
#undef OLD_LOG_FUNC
}
