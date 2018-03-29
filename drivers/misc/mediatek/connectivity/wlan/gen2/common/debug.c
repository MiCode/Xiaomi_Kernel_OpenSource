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

#include "precomp.h"
#include "gl_kal.h"

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
	UINT_8	ucTc4RelCnt;
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

typedef struct _COMMAND_ENTRY {
	UINT_64 u8TxTime;
	UINT_64 u8ReadFwTime;
	UINT_32 u4ReadFwValue;
	UINT_32 u4RelCID;
	UINT_16 u2Counter;
	struct COMMAND rCmd;
} COMMAND_ENTRY, *P_COMMAND_ENTRY;

typedef struct _PKT_INFO_ENTRY {
	UINT_64 u8Timestamp;
	UINT_8 status;
	UINT_16 u2EtherType;
	UINT_8 ucIpProto;
	UINT_16 u2IpId;
	UINT_16 u2ArpOpCode;

} PKT_INFO_ENTRY, *P_PKT_INFO_ENTRY;

typedef struct _PKT_TRACE_RECORD {
	P_PKT_INFO_ENTRY pTxPkt;
	P_PKT_INFO_ENTRY pRxPkt;
	UINT_32 u4TxIndex;
	UINT_32 u4RxIndex;
} PKT_TRACE_RECORD, *P_PKT_TRACE_RECORD;


#define PKT_INFO_BUF_MAX_NUM 50
#define PKT_INFO_MSG_LENGTH 200
#define PKT_INFO_MSG_GROUP_RANGE 3
#define TC_RELEASE_TRACE_BUF_MAX_NUM 100
#define TXED_CMD_TRACE_BUF_MAX_NUM 100
#define TXED_COMMAND_BUF_MAX_NUM 10

static P_TC_RES_RELEASE_ENTRY gprTcReleaseTraceBuffer;
static P_CMD_TRACE_ENTRY gprCmdTraceEntry;
static P_COMMAND_ENTRY gprCommandEntry;
static PKT_TRACE_RECORD grPktRec;


VOID wlanPktDebugTraceInfoARP(UINT_8 status, UINT_8 eventType, UINT_16 u2ArpOpCode)
{
	if (eventType == PKT_TX)
		status = 0xFF;

	wlanPktDebugTraceInfo(status, eventType, ETH_P_ARP, 0, 0, u2ArpOpCode);

}
VOID wlanPktDebugTraceInfoIP(UINT_8 status, UINT_8 eventType, UINT_8 ucIpProto, UINT_16 u2IpId)
{
	if (eventType == PKT_TX)
		status = 0xFF;

	wlanPktDebugTraceInfo(status, eventType, ETH_P_IP, ucIpProto, u2IpId, 0);

}

VOID wlanPktDebugTraceInfo(UINT_8 status, UINT_8 eventType
	, UINT_16 u2EtherType, UINT_8 ucIpProto, UINT_16 u2IpId, UINT_16 u2ArpOpCode)
{

	P_PKT_INFO_ENTRY prPkt = NULL;
	UINT_32 index;

	DBGLOG(TX, LOUD, "PKT id = 0x%02x, status =%d, Proto = %d, type =%d\n"
		, u2IpId, status, ucIpProto, eventType);
	do {

		if (grPktRec.pTxPkt == NULL || grPktRec.pRxPkt == NULL) {
			DBGLOG(TX, ERROR, "pTxPkt is null point !");
			break;
		}

		/* debug for Package info begin */
		if (eventType == PKT_TX) {
			prPkt = &grPktRec.pTxPkt[grPktRec.u4TxIndex];
			grPktRec.u4TxIndex++;
			if (grPktRec.u4TxIndex == PKT_INFO_BUF_MAX_NUM)
				grPktRec.u4TxIndex = 0;
		} else if (eventType == PKT_RX) {
			prPkt = &grPktRec.pRxPkt[grPktRec.u4RxIndex];
			grPktRec.u4RxIndex++;
			if (grPktRec.u4RxIndex == PKT_INFO_BUF_MAX_NUM)
				grPktRec.u4RxIndex = 0;
		}

		if (prPkt) {
			prPkt->u8Timestamp = sched_clock();
			prPkt->status = status;
			prPkt->u2EtherType = u2EtherType;
			prPkt->ucIpProto = ucIpProto;
			prPkt->u2IpId = u2IpId;
			prPkt->u2ArpOpCode = u2ArpOpCode;
		}


		/* Update tx status */
		if (eventType == PKT_TX_DONE) {
			/* Support Ethernet type = IP*/
			if (u2EtherType == ETH_P_IP) {
				for (index = 0; index < PKT_INFO_BUF_MAX_NUM; index++) {
					if (grPktRec.pTxPkt[index].u2IpId == u2IpId) {
						grPktRec.pTxPkt[index].status = status;
						DBGLOG(TX, LOUD, "PKT_TX_DONE match\n");
						break;
					}
				}
			}
		}
	} while (FALSE);
}

VOID wlanPktDebugDumpInfo(P_ADAPTER_T prAdapter)
{

	UINT_32 i;
	UINT_32 index;
	UINT_32 offsetMsg;
	UINT_32 pktIndex;
	P_PKT_INFO_ENTRY prPktInfo;
	UINT_8 pucMsg[PKT_INFO_MSG_LENGTH];

	do {

		if (grPktRec.pTxPkt == NULL || grPktRec.pRxPkt == NULL)
			break;

		if (grPktRec.u4TxIndex == 0 && grPktRec.u4RxIndex == 0)
			break;

		offsetMsg = 0;
		/* start dump pkt info of tx/rx by decrease timestap */

		for (i = 0 ; i < 2 ; i++) {
			for (index = 0; index < PKT_INFO_BUF_MAX_NUM ; index++) {
				if (i == 0) {
					/* TX */
					pktIndex = (PKT_INFO_BUF_MAX_NUM + (grPktRec.u4TxIndex - 1) - index)
						% PKT_INFO_BUF_MAX_NUM;
					prPktInfo = &grPktRec.pTxPkt[pktIndex];
				} else if (i == 1) {
					/* RX */
					pktIndex = (PKT_INFO_BUF_MAX_NUM + (grPktRec.u4RxIndex - 1) - index)
						% PKT_INFO_BUF_MAX_NUM;
					prPktInfo = &grPktRec.pRxPkt[pktIndex];
				}
				/*ucIpProto = 0x01 ICMP */
				/*ucIpProto = 0x11 UPD */
				/*ucIpProto = 0x06 TCP */
				offsetMsg += kalSnprintf(pucMsg + offsetMsg
				, PKT_INFO_MSG_LENGTH - offsetMsg
				, "(%2d)t=%llu s=%d e=0x%02x,p=0x%2x id=0x%4x,op=%d  "
				, index, prPktInfo->u8Timestamp
				, prPktInfo->status
				, prPktInfo->u2EtherType
				, prPktInfo->ucIpProto
				, prPktInfo->u2IpId
				, prPktInfo->u2ArpOpCode);

				if ((index == PKT_INFO_BUF_MAX_NUM - 1) ||
					(index % PKT_INFO_MSG_GROUP_RANGE == (PKT_INFO_MSG_GROUP_RANGE - 1))) {
					if (i == 0)
						DBGLOG(TX, INFO, "%s\n" , pucMsg);
					else if (i == 1)
						DBGLOG(RX, INFO, "%s\n" , pucMsg);

					offsetMsg = 0;
					kalMemSet(pucMsg, '\0', PKT_INFO_MSG_LENGTH);
				}
			}
		}

	} while (FALSE);

}
VOID wlanDebugInit(VOID)
{
	/* debug for command/tc4 resource begin */
	gprTcReleaseTraceBuffer =
		kalMemAlloc(TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprTcReleaseTraceBuffer, TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY));
	gprCmdTraceEntry = kalMemAlloc(TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprCmdTraceEntry, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY));

	gprCommandEntry = kalMemAlloc(TXED_COMMAND_BUF_MAX_NUM * sizeof(COMMAND_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprCommandEntry, TXED_COMMAND_BUF_MAX_NUM * sizeof(COMMAND_ENTRY));
	/* debug for command/tc4 resource end */

	/* debug for package info begin */
	grPktRec.pTxPkt = kalMemAlloc(PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktRec.pTxPkt, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4TxIndex = 0;
	grPktRec.pRxPkt = kalMemAlloc(PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktRec.pRxPkt, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4RxIndex = 0;

	/* debug for package info end */

}

VOID wlanDebugUninit(VOID)
{
	/* debug for command/tc4 resource begin */
	kalMemFree(gprTcReleaseTraceBuffer, PHY_MEM_TYPE,
			TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY));
	kalMemFree(gprCmdTraceEntry, PHY_MEM_TYPE, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY));
	kalMemFree(gprCommandEntry, PHY_MEM_TYPE, TXED_COMMAND_BUF_MAX_NUM * sizeof(COMMAND_ENTRY));
	/* debug for command/tc4 resource end */

	/* debug for package info begin */
	kalMemFree(grPktRec.pTxPkt, VIR_MEM_TYPE, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4TxIndex = 0;
	kalMemFree(grPktRec.pRxPkt, VIR_MEM_TYPE, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4RxIndex = 0;
	/* debug for package info end */
}

VOID wlanDebugHifDescriptorDump(P_ADAPTER_T prAdapter , ENUM_AMPDU_TYPE type
	, ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T tcIndex)
{
	/* debug for tx/rx description begin */
	UINT_32 i;
	UINT_32 u4TcCount;
	UINT_32 u4Offset;
	UINT_32 u4StartAddr;
	P_HIF_TX_DESC_T prTxDesc;
	P_HIF_RX_DESC_T prRxDesc;

	do {
		if (type == MTK_AMPDU_TX_DESC) {

			if (tcIndex == DEBUG_TC0_INDEX) {
				u4TcCount = NIC_TX_INIT_BUFF_COUNT_TC0;
				u4StartAddr = AP_MCU_TX_DESC_ADDR;
				u4Offset = AP_MCU_BANK_OFFSET;
			} else if (tcIndex == DEBUG_TC4_INDEX) {
				u4TcCount = NIC_TX_BUFF_COUNT_TC4;
				u4StartAddr = AP_MCU_TC_INDEX_4_ADDR;
				u4Offset = AP_MCU_TC_INDEX_4_OFFSET;
			} else {
				DBGLOG(TX, ERROR, "Type :%d TC_INDEX :%d don't support !", type , tcIndex);
				break;
			}

			prTxDesc = (P_HIF_TX_DESC_T) kalMemAlloc(sizeof(HIF_TX_DESC_T), VIR_MEM_TYPE);

			for (i = 0; i < u4TcCount ; i++) {
				HAL_GET_APMCU_MEM(prAdapter, u4StartAddr, u4Offset, i, (PUINT_8)prTxDesc
					, sizeof(HIF_TX_DESC_T));
				DBGLOG(TX, INFO
					, "TC%d[%d]uOwn:%2x,CS:%2x,R1:%2x,ND:0x%08x,SA: 0x%08x,R2:%x\n"
					, tcIndex, i, prTxDesc->ucOwn, prTxDesc->ucDescChksum
					, prTxDesc->u2Rsrv1, prTxDesc->u4NextDesc
					, prTxDesc->u4BufStartAddr, prTxDesc->u4Rsrv2);
			}
			kalMemFree(prTxDesc, VIR_MEM_TYPE, sizeof(HIF_TX_DESC_T));

		} else if (type == MTK_AMPDU_RX_DESC) {

			if (tcIndex == DEBUG_TC0_INDEX) {
				u4TcCount = NIC_TX_INIT_BUFF_COUNT_TC0;
				u4StartAddr = AP_MCU_RX_DESC_ADDR;
				u4Offset = AP_MCU_BANK_OFFSET;
			} else {
				DBGLOG(RX, ERROR, "Type :%d TC_INDEX :%d don't support !", type, tcIndex);
				break;
			}

			prRxDesc = (P_HIF_RX_DESC_T) kalMemAlloc(sizeof(HIF_RX_DESC_T), VIR_MEM_TYPE);
			DBGLOG(RX, INFO, "Start dump rx_desc from APMCU\n");
			for (i = 0; i < NIC_TX_INIT_BUFF_COUNT_TC0 ; i++) {
				HAL_GET_APMCU_MEM(prAdapter, u4StartAddr, u4Offset, i, (PUINT_8)prRxDesc
					, sizeof(HIF_RX_DESC_T));
				DBGLOG(RX, INFO
					, "RX%d[%d]uOwn:%2x,CS:%2x,TO:%x,CSI:%x,ND:0x%08x,SA:0x%08x,len:%x,R1:%x\n"
					, tcIndex, i, prRxDesc->ucOwn , prRxDesc->ucDescChksum
					, prRxDesc->ucEtherTypeOffset, prRxDesc->ucChkSumInfo
					, prRxDesc->u4NextDesc, prRxDesc->u4BufStartAddr
					, prRxDesc->u2RxBufLen, prRxDesc->u2Rsrv1);
			}
			kalMemFree(prRxDesc, VIR_MEM_TYPE, sizeof(P_HIF_RX_DESC_T));
		}
	} while (FALSE);
	/* debug for tx/rx description end */

}


VOID wlanReadFwStatus(P_ADAPTER_T prAdapter)
{
	static UINT_16 u2CurEntryCmd;
	P_COMMAND_ENTRY prCurCommand = &gprCommandEntry[u2CurEntryCmd];

	prCurCommand->u8ReadFwTime = sched_clock();
	HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurCommand->u4ReadFwValue);
	u2CurEntryCmd++;
	if (u2CurEntryCmd == TXED_COMMAND_BUF_MAX_NUM)
		u2CurEntryCmd = 0;
}

VOID wlanTraceTxCmd(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmd)
{
	static UINT_16 u2CurEntry;
	static UINT_16 u2CurEntryCmd;
	P_CMD_TRACE_ENTRY prCurCmd = &gprCmdTraceEntry[u2CurEntry];
	P_COMMAND_ENTRY prCurCommand = &gprCommandEntry[u2CurEntryCmd];

	prCurCmd->u8TxTime = sched_clock();
	prCurCommand->u8TxTime = prCurCmd->u8TxTime;
	prCurCmd->eCmdType = prCmd->eCmdType;
	if (prCmd->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
		P_WLAN_MAC_MGMT_HEADER_T prMgmt = (P_WLAN_MAC_MGMT_HEADER_T)((P_MSDU_INFO_T)prCmd->prPacket)->prPacket;

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

		prCurCommand->rCmd.ucCID = prCmd->ucCID;
		prCurCommand->rCmd.ucCmdSeqNum = prCmd->ucCmdSeqNum;
		prCurCommand->rCmd.fgNeedResp = prCmd->fgNeedResp;
		prCurCommand->rCmd.fgSetQuery = prCmd->fgSetQuery;

		prCurCommand->u2Counter = u2CurEntryCmd;
		u2CurEntryCmd++;
		if (u2CurEntryCmd == TXED_COMMAND_BUF_MAX_NUM)
			u2CurEntryCmd = 0;
		HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurCommand->u4RelCID);
	}
	u2CurEntry++;
	if (u2CurEntry == TC_RELEASE_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}

VOID wlanTraceReleaseTcRes(P_ADAPTER_T prAdapter, PUINT_8 aucTxRlsCnt, UINT_8 ucAvailable)
{
	static UINT_16 u2CurEntry;
	P_TC_RES_RELEASE_ENTRY prCurBuf = &gprTcReleaseTraceBuffer[u2CurEntry];
	HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurBuf->u4RelCID);
	prCurBuf->u8RelaseTime = sched_clock();
	prCurBuf->ucTc4RelCnt = aucTxRlsCnt[TC4_INDEX];
	prCurBuf->ucAvailableTc4 = ucAvailable;
	u2CurEntry++;
	if (u2CurEntry == TXED_CMD_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}
VOID wlanDumpTxReleaseCount(P_ADAPTER_T prAdapter)
{
	UINT_32 au4WTSR[2];

	HAL_READ_TX_RELEASED_COUNT(prAdapter, au4WTSR);
	DBGLOG(TX, INFO, "WTSR[1]=%d, WTSR[0]=%d\n", au4WTSR[1], au4WTSR[0]);
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
			if (bufLen <= 0 || (UINT_32)bufLen >= maxLen)
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
			if (bufLen <= 0 || (UINT_32)bufLen >= maxLen)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
		return;
	}
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
VOID wlanDumpCommandFwStatus(VOID)
{
	UINT_16 i = 0;
	P_COMMAND_ENTRY prCmd = gprCommandEntry;

	LOG_FUNC("Start\n");
	for (; i < TXED_COMMAND_BUF_MAX_NUM; i++) {
		LOG_FUNC("%d: Time %llu, Content %08x, Count %x, RelCID %08x, ReadFwValue %08x, ReadFwTime %llu\n",
			i, prCmd[i].u8TxTime, *(PUINT_32)(&prCmd[i].rCmd.ucCID),
			prCmd[i].u2Counter, prCmd[i].u4RelCID,
			prCmd[i].u4ReadFwValue, prCmd[i].u8ReadFwTime);
	}
}
