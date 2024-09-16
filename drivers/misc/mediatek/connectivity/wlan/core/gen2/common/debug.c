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
	UINT_32 u4RelCID;
	union {
		struct COMMAND rCmd;
		struct SECURITY_FRAME rSecFrame;
		struct MGMT_FRAME rMgmtFrame;
	} u;
} CMD_TRACE_ENTRY, *P_CMD_TRACE_ENTRY;

#define READ_CPUPCR_MAX_NUM 3
typedef struct _COMMAND_ENTRY {
	UINT_64 u8TxTime;
	UINT_64 u8ReadFwTime;
	UINT_32 u4ReadFwValue;
	UINT_32 arCpupcrValue[READ_CPUPCR_MAX_NUM];
	UINT_32 u4RelCID;
	UINT_16 u2Counter;
	struct COMMAND rCmd;
} COMMAND_ENTRY, *P_COMMAND_ENTRY;

struct COMMAND_DEBUG_INFO {
	UINT_8 ucCID;
	UINT_8 ucCmdSeqNum;
	UINT_32 u4InqueTime;
	UINT_32 u4SendToFwTime;
	UINT_32 u4FwResponseTime;
};
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

typedef struct _SCAN_HIF_DESC_RECORD {
	P_HIF_TX_DESC_T pTxDescScanWriteBefore;
	P_HIF_TX_DESC_T pTxDescScanWriteDone;
	UINT_64 u8ScanWriteBeforeTime;
	UINT_64 u8ScanWriteDoneTime;
	UINT_32 aucFreeBufCntScanWriteBefore;
	UINT_32 aucFreeBufCntScanWriteDone;
} SCAN_HIF_DESC_RECORD, *P_SCAN_HIF_DESC_RECORD;

typedef struct _FWDL_DEBUG_T {
	UINT_32	u4TxStartTime;
	UINT_32	u4TxDoneTime;
	UINT_32	u4RxStartTime;
	UINT_32	u4RxDoneTime;
	UINT_32	u4Section;
	UINT_32	u4DownloadSize;
	UINT_32	u4ResponseTime;
} FWDL_DEBUG_T, *P_FWDL_DEBUG_T;

typedef struct _BSS_TRACE_RECORD {
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 ucRCPI;
} BSS_TRACE_RECORD, *P_BSS_TRACE_RECORD;

typedef struct _SCAN_TARGET_BSS_LIST {
	P_BSS_TRACE_RECORD prBssTraceRecord;
	UINT_32 u4BSSIDCount;
} SCAN_TARGET_BSS_LIST, *P_SCAN_TARGET_BSS_LIST;

#define PKT_INFO_BUF_MAX_NUM 50
#define PKT_INFO_MSG_LENGTH 200
#define PKT_INFO_MSG_GROUP_RANGE 3

typedef struct _PKT_STATUS_ENTRY {
	UINT_8 u1Type;
	UINT_16 u2IpId;
	UINT_8 status;
	UINT_64 u4pktXmitTime; /* unit: naro seconds */
	UINT_64 u4pktToHifTime; /* unit: naro seconds */
	UINT_32 u4ProcessTimeDiff; /*ms*/
} PKT_STATUS_ENTRY, *P_PKT_STATUS_ENTRY;

typedef struct _PKT_STATUS_RECORD {
	P_PKT_STATUS_ENTRY pTxPkt;
	P_PKT_STATUS_ENTRY pRxPkt;
	UINT_32 u4TxIndex;
	UINT_32 u4RxIndex;
} PKT_STATUS_RECORD, *P_PKT_STATUS_RECORD;

#define TC_RELEASE_TRACE_BUF_MAX_NUM 100
#define TXED_CMD_TRACE_BUF_MAX_NUM 100
#define TXED_COMMAND_BUF_MAX_NUM 10
#define MAX_FW_IMAGE_PACKET_COUNT	500
#define SCAN_TARGET_BSS_MAX_NUM 20
#define SCAN_MSG_MAX_LEN 256

#define PKT_STATUS_BUF_MAX_NUM 450
#define PKT_STATUS_MSG_GROUP_RANGE 80
#define PKT_STATUS_MSG_LENGTH 900

#define CMD_BUF_MSG_LENGTH 1024

#if CFG_SUPPORT_EMI_DEBUG
#define WLAN_EMI_DEBUG_BUF_SIZE 512
#define WLAN_EMI_DEBUG_LINE_SIZE 256
#endif

static P_TC_RES_RELEASE_ENTRY gprTcReleaseTraceBuffer;
static P_CMD_TRACE_ENTRY gprCmdTraceEntry;
static P_COMMAND_ENTRY gprCommandEntry;
static struct COMMAND_DEBUG_INFO *gprCommandDebugInfo;


static PKT_TRACE_RECORD grPktRec;
static PKT_STATUS_RECORD grPktStaRec;
static SCAN_HIF_DESC_RECORD grScanHifDescRecord;
P_FWDL_DEBUG_T gprFWDLDebug;



UINT_32 u4FWDL_packet_count;
static SCAN_TARGET_BSS_LIST grScanTargetBssList;
static UINT_16 gau2PktSeq[PKT_STATUS_BUF_MAX_NUM];
UINT_32 u4PktSeqCount;



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
						DBGLOG(TX, INFO, "%s\n", pucMsg);
					else if (i == 1)
						DBGLOG(RX, INFO, "%s\n", pucMsg);

					offsetMsg = 0;
					kalMemSet(pucMsg, '\0', PKT_INFO_MSG_LENGTH);
				}
			}
		}

	} while (FALSE);

}
VOID wlanPktStausDebugUpdateProcessTime(UINT_32 u4DbgTxPktStatusIndex)
{
	P_PKT_STATUS_ENTRY prPktInfo;

	if (u4DbgTxPktStatusIndex == 0xffff || u4DbgTxPktStatusIndex >= PKT_STATUS_BUF_MAX_NUM) {
		DBGLOG(TX, WARN, "Can't support the index %d!\n", u4DbgTxPktStatusIndex);
		return;
	}

	prPktInfo = &grPktStaRec.pTxPkt[u4DbgTxPktStatusIndex];
	if (prPktInfo != NULL) {

		prPktInfo->u4pktToHifTime = sched_clock();
		if (prPktInfo->u4pktToHifTime > prPktInfo->u4pktXmitTime)
			prPktInfo->u4ProcessTimeDiff = (UINT_32)(prPktInfo->u4pktToHifTime - prPktInfo->u4pktXmitTime);
		else
			prPktInfo->u4ProcessTimeDiff = 0;

		/* transfer the time's uint from 'ns' to 'ms'*/
		prPktInfo->u4ProcessTimeDiff /= 1000000;
	}

}


VOID wlanPktStatusDebugTraceInfoSeq(P_ADAPTER_T prAdapter, UINT_16 u2NoSeq)
{
	if (u4PktSeqCount >= PKT_STATUS_BUF_MAX_NUM)
		u4PktSeqCount = 0;
	gau2PktSeq[u4PktSeqCount] = u2NoSeq;
	u4PktSeqCount++;
}



VOID wlanPktStatusDebugTraceInfoARP(UINT_8 status, UINT_8 eventType, UINT_16 u2ArpOpCode, PUINT_8 pucPkt
	, P_MSDU_INFO_T prMsduInfo)
{
	if (eventType == PKT_TX)
		status = 0xFF;
	wlanPktStatusDebugTraceInfo(status, eventType, ETH_P_ARP, 0, 0, u2ArpOpCode, pucPkt, prMsduInfo);
}

VOID wlanPktStatusDebugTraceInfoIP(UINT_8 status, UINT_8 eventType, UINT_8 ucIpProto, UINT_16 u2IpId, PUINT_8 pucPkt
	, P_MSDU_INFO_T prMsduInfo)
{
	if (eventType == PKT_TX)
		status = 0xFF;
	wlanPktStatusDebugTraceInfo(status, eventType, ETH_P_IP, ucIpProto, u2IpId, 0, pucPkt, prMsduInfo);
}

VOID wlanPktStatusDebugTraceInfo(UINT_8 status, UINT_8 eventType
	, UINT_16 u2EtherType, UINT_8 ucIpProto, UINT_16 u2IpId, UINT_16 u2ArpOpCode
	, PUINT_8 pucPkt, P_MSDU_INFO_T prMsduInfo)
{
	P_PKT_STATUS_ENTRY prPktSta = NULL;
	UINT_32 index;

	DBGLOG(TX, LOUD, "PKT id = 0x%02x, status =%d, Proto = %d, type =%d\n"
		, u2IpId, status, ucIpProto, eventType);
	do {
		if (grPktStaRec.pTxPkt == NULL) {
			DBGLOG(TX, ERROR, "pTxStaPkt is null point !");
			break;
		}

		/* debug for Package info begin */
		if (eventType == PKT_TX)
			prPktSta = &grPktStaRec.pTxPkt[grPktStaRec.u4TxIndex];
		else if (eventType == PKT_RX)
			prPktSta = &grPktStaRec.pRxPkt[grPktStaRec.u4RxIndex];


		if (prPktSta) {
			prPktSta->u1Type = kalGetPktEtherType(pucPkt);
			prPktSta->status = status;
			prPktSta->u2IpId = u2IpId;
			if (eventType == PKT_TX) {
				prMsduInfo->u4DbgTxPktStatusIndex = grPktStaRec.u4TxIndex;
				prPktSta->u4pktXmitTime = GLUE_GET_PKT_XTIME(prMsduInfo->prPacket);

			}
		}

		/* Update tx status */
		if (eventType == PKT_TX_DONE) {
			/* Support Ethernet type = IP*/
			if (u2EtherType == ETH_P_IP) {
				for (index = 0; index < PKT_STATUS_BUF_MAX_NUM; index++) {
					if (grPktStaRec.pTxPkt[index].u2IpId == u2IpId) {
						grPktStaRec.pTxPkt[index].status = status;
						DBGLOG(TX, TRACE, "Status: PKT_TX_DONE match\n");
						break;
					}
				}
			}
		}

		/*update the index of record */
		if (eventType == PKT_TX) {
			grPktStaRec.u4TxIndex++;
			if (grPktStaRec.u4TxIndex == PKT_STATUS_BUF_MAX_NUM) {
				DBGLOG(TX, INFO, "grPktStaRec.u4TxIndex reset");
				grPktStaRec.u4TxIndex = 0;
			}
		} else if (eventType == PKT_RX) {
			grPktStaRec.u4RxIndex++;
			if (grPktStaRec.u4RxIndex == PKT_STATUS_BUF_MAX_NUM) {
				DBGLOG(TX, INFO, "grPktStaRec.u4RxIndex reset");
				grPktStaRec.u4RxIndex = 0;
			}
		}

	} while (FALSE);
}

VOID wlanPktStatusDebugDumpInfo(P_ADAPTER_T prAdapter)
{
	UINT_32 i;
	UINT_32 index;
	UINT_32 offsetMsg;
	P_PKT_STATUS_ENTRY prPktInfo;
	UINT_8 pucMsg[PKT_STATUS_MSG_LENGTH];
	UINT_32 u4PktCnt;
	UINT_64 u8TxTimeBase; /*ns*/
	UINT_32 u4TxTimeOffset; /*ms*/
	UINT_8 uMsgGroupRange;

	do {

		if (grPktStaRec.pTxPkt == NULL || grPktStaRec.pRxPkt == NULL)
			break;

		if (grPktStaRec.u4TxIndex == 0 && grPktStaRec.u4RxIndex == 0)
			break;

		offsetMsg = 0;
		u4TxTimeOffset = 0;
		u8TxTimeBase = grPktStaRec.pTxPkt[0].u4pktXmitTime;

		DBGLOG(TX, INFO, "Pkt dump: TxCnt %d, RxCnt %d tx_pkt timebase:%lld ns\n"
			, grPktStaRec.u4TxIndex, grPktStaRec.u4RxIndex, u8TxTimeBase);

		/* start dump pkt info of tx/rx by decrease timestap */
		for (i = 0 ; i < 2 ; i++) {
			if (i == 0) {
				u4PktCnt = grPktStaRec.u4TxIndex;
				uMsgGroupRange = PKT_STATUS_MSG_GROUP_RANGE/2;
			} else {
				u4PktCnt = grPktStaRec.u4RxIndex;
				uMsgGroupRange = PKT_STATUS_MSG_GROUP_RANGE;
			}

			for (index = 0; index < u4PktCnt; index++) {
				if (i == 0)
					prPktInfo = &grPktStaRec.pTxPkt[index];
				else
					prPktInfo = &grPktStaRec.pRxPkt[index];
				/*ucIpProto = 0x01 ICMP */
				/*ucIpProto = 0x11 UPD */
				/*ucIpProto = 0x06 TCP */

				if (i == 0) {
					/*tx format*/
					u4TxTimeOffset =  prPktInfo->u4pktXmitTime - u8TxTimeBase;
					/*transfer the time's uint form 'ns' to 'ms'*/
					u4TxTimeOffset /= 1000000;

					DBGLOG(TX, LOUD, "ipid=0x%x,(%lld - %lld = %d ms),offset=%d ms\n"
					, prPktInfo->u2IpId, prPktInfo->u4pktToHifTime
					, prPktInfo->u4pktXmitTime, prPktInfo->u4ProcessTimeDiff
					, u4TxTimeOffset);


					offsetMsg += kalSnprintf(pucMsg + offsetMsg
					, (PKT_STATUS_MSG_LENGTH - offsetMsg)
					, "%d,%02x,%x,%d,%d "
					, prPktInfo->u1Type
					, prPktInfo->u2IpId
					, prPktInfo->status
					, u4TxTimeOffset	/*ms*/
					, prPktInfo->u4ProcessTimeDiff	/*ms*/
					);
				} else {
					/*rx format*/
					offsetMsg += kalSnprintf(pucMsg + offsetMsg
					, (PKT_STATUS_MSG_LENGTH - offsetMsg)
					, "%d,%02x,%x "
					, prPktInfo->u1Type
					, prPktInfo->u2IpId
					, prPktInfo->status);
				}

				if (((index + 1) % uMsgGroupRange == 0) || (index == (u4PktCnt - 1))) {
					if (i == 0)
						DBGLOG(TX, INFO, "%s\n", pucMsg);
					else if (i == 1)
						DBGLOG(RX, INFO, "%s\n", pucMsg);

					offsetMsg = 0;
					kalMemSet(pucMsg, '\0', PKT_STATUS_MSG_LENGTH);
				}
			}
		}

		/*dump rx sequence*/
		kalMemZero(pucMsg, PKT_STATUS_MSG_LENGTH * sizeof(UINT_8));
		offsetMsg = 0;
		offsetMsg += kalSnprintf(pucMsg + offsetMsg, (PKT_STATUS_MSG_LENGTH - offsetMsg)
			, "RX Seq count: %d [", u4PktSeqCount);

		for (index = 0; index < u4PktSeqCount; index++)
			offsetMsg += kalSnprintf(pucMsg + offsetMsg, (PKT_STATUS_MSG_LENGTH - offsetMsg)
			, "%x,", gau2PktSeq[index]);

		DBGLOG(RX, INFO, "%s]\n", pucMsg);

		u4PktSeqCount = 0;
		kalMemZero(gau2PktSeq, sizeof(UINT_16) * PKT_STATUS_BUF_MAX_NUM);
	} while (FALSE);
	u4PktCnt = grPktStaRec.u4TxIndex = 0;
	u4PktCnt = grPktStaRec.u4RxIndex = 0;

}
#if CFG_SUPPORT_EMI_DEBUG
static UINT32 gPrevIdxPagedtrace;
#endif
VOID wlanDebugInit(VOID)
{
	wlanDbgLogLevelInit();
	/* debug for command/tc4 resource begin */
	gprTcReleaseTraceBuffer =
		kalMemAlloc(TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprTcReleaseTraceBuffer, TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(TC_RES_RELEASE_ENTRY));
	gprCmdTraceEntry = kalMemAlloc(TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprCmdTraceEntry, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(CMD_TRACE_ENTRY));

	gprCommandEntry = kalMemAlloc(TXED_COMMAND_BUF_MAX_NUM * sizeof(COMMAND_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprCommandEntry, TXED_COMMAND_BUF_MAX_NUM * sizeof(COMMAND_ENTRY));
#if CFG_SUPPORT_EMI_DEBUG
	gPrevIdxPagedtrace = 0xFFFF;
#endif
	/* debug for command/tc4 resource end */

	/* debug for package info begin */
	grPktRec.pTxPkt = kalMemAlloc(PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktRec.pTxPkt, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4TxIndex = 0;
	grPktRec.pRxPkt = kalMemAlloc(PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktRec.pRxPkt, PKT_INFO_BUF_MAX_NUM * sizeof(PKT_INFO_ENTRY));
	grPktRec.u4RxIndex = 0;
	/* debug for package info end */


	/*debug for scan request tx_description begin*/
	grScanHifDescRecord.pTxDescScanWriteBefore = kalMemAlloc(NIC_TX_BUFF_COUNT_TC4 * sizeof(HIF_TX_DESC_T)
		, VIR_MEM_TYPE);
	grScanHifDescRecord.aucFreeBufCntScanWriteBefore = 0;
	grScanHifDescRecord.pTxDescScanWriteDone = kalMemAlloc(NIC_TX_BUFF_COUNT_TC4 * sizeof(HIF_TX_DESC_T)
	, VIR_MEM_TYPE);
	grScanHifDescRecord.aucFreeBufCntScanWriteDone = 0;
	/*debug for scan request tx_description end*/

	/*debug for scan target bss begin*/
	grScanTargetBssList.prBssTraceRecord = kalMemAlloc(SCAN_TARGET_BSS_MAX_NUM * sizeof(BSS_TRACE_RECORD)
		, VIR_MEM_TYPE);
	kalMemZero(grScanTargetBssList.prBssTraceRecord, SCAN_TARGET_BSS_MAX_NUM * sizeof(BSS_TRACE_RECORD));
	grScanTargetBssList.u4BSSIDCount = 0;
	/*debug for scan target bss end*/

	grPktStaRec.pTxPkt = kalMemAlloc(PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktStaRec.pTxPkt, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4TxIndex = 0;
	grPktStaRec.pRxPkt = kalMemAlloc(PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY), VIR_MEM_TYPE);
	kalMemZero(grPktStaRec.pRxPkt, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4RxIndex = 0;
	/* debug for package info end */

	/*debug for rx sequence tid begin*/
	kalMemZero(gau2PktSeq, PKT_STATUS_BUF_MAX_NUM * sizeof(UINT_16));
	u4PktSeqCount = 0;
	/* debug for rx sequence tid end*/

	/*debug for command record begin*/
	gprCommandDebugInfo = kalMemAlloc(TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(struct COMMAND_DEBUG_INFO)
	, PHY_MEM_TYPE);
	kalMemZero(gprCommandDebugInfo, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(struct COMMAND_DEBUG_INFO));
	/*debug for command record end*/

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


	/*debug for scan request tx_description begin*/
	kalMemFree(grScanHifDescRecord.pTxDescScanWriteBefore
	, VIR_MEM_TYPE, NIC_TX_BUFF_COUNT_TC4 * sizeof(HIF_TX_DESC_T));
	grScanHifDescRecord.aucFreeBufCntScanWriteBefore = 0;
	kalMemFree(grScanHifDescRecord.pTxDescScanWriteDone
	, VIR_MEM_TYPE, NIC_TX_BUFF_COUNT_TC4 * sizeof(HIF_TX_DESC_T));
	grScanHifDescRecord.aucFreeBufCntScanWriteDone = 0;
	/*debug for scan request tx_description end*/

	/*debug for scan target bss begin*/
	kalMemFree(grScanTargetBssList.prBssTraceRecord
	, VIR_MEM_TYPE, SCAN_TARGET_BSS_MAX_NUM * sizeof(BSS_TRACE_RECORD));
	grScanTargetBssList.u4BSSIDCount = 0;
	/*debug for scan target bss end*/

	/* debug for package status info begin */
	kalMemFree(grPktStaRec.pTxPkt, VIR_MEM_TYPE, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4TxIndex = 0;
	kalMemFree(grPktStaRec.pRxPkt, VIR_MEM_TYPE, PKT_STATUS_BUF_MAX_NUM * sizeof(PKT_STATUS_ENTRY));
	grPktStaRec.u4RxIndex = 0;
	/* debug for package status info end */

	/*debug for rx sequence tid begin*/
	u4PktSeqCount = 0;
	/* debug for rx sequence tid end*/

	/*debug for command record begin*/
	kalMemFree(gprCommandDebugInfo, PHY_MEM_TYPE
	, TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(struct COMMAND_DEBUG_INFO));
	/*debug for command record end*/

	wlanDbgLogLevelUninit();
}
VOID wlanDebugScanTargetBSSRecord(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_SCAN_INFO_T prScanInfo;
	P_BSS_TRACE_RECORD prBssTraceRecord;
	UINT_32 i;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);


	if (prBssDesc == NULL) {
		DBGLOG(SCN, LOUD, "Scan Desc bss is null !\n");
		return;
	}
	if (prConnSettings->ucSSIDLen == 0) {
		DBGLOG(SCN, LOUD, "Target BSS length is 0, ignore it!\n");
		return;
	}
	if (prScanInfo->eCurrentState == SCAN_STATE_IDLE) {
		DBGLOG(SCN, LOUD, "Ignore beacon/probeRsp, during SCAN Idle!\n");
		return;
	}


	/*dump beacon and probeRsp by connect setting SSID and ignore null bss*/
	if (EQUAL_SSID(prBssDesc->aucSSID,
		prBssDesc->ucSSIDLen,
		prConnSettings->aucSSID,
		prConnSettings->ucSSIDLen) && (prConnSettings->ucSSIDLen > 0)) {
		/*Insert BssDesc and ignore repeats*/

		if (grScanTargetBssList.u4BSSIDCount > SCAN_TARGET_BSS_MAX_NUM) {
			DBGLOG(SCN, LOUD, "u4BSSIDCount out of bound!\n");
			return;
		}

		for (i = 0 ; i < grScanTargetBssList.u4BSSIDCount ; i++) {
			prBssTraceRecord = &(grScanTargetBssList.prBssTraceRecord[i]);
			if (EQUAL_MAC_ADDR(prBssTraceRecord->aucBSSID, prBssDesc->aucBSSID)) {
				/*if exist ,update it*/
				prBssTraceRecord->ucRCPI = prBssDesc->ucRCPI;
				break;
			}
		}
		if ((i == grScanTargetBssList.u4BSSIDCount) &&
			(grScanTargetBssList.u4BSSIDCount < SCAN_TARGET_BSS_MAX_NUM)) {
			prBssTraceRecord = &(grScanTargetBssList.prBssTraceRecord[i]);
			/*add the new bssDesc recored*/
			COPY_MAC_ADDR(prBssTraceRecord->aucBSSID, prBssDesc->aucBSSID);
			prBssTraceRecord->ucRCPI = prBssDesc->ucRCPI;
			grScanTargetBssList.u4BSSIDCount++;
		}

	}

}

VOID wlanDebugScanTargetBSSDump(P_ADAPTER_T prAdapter)
{

	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_TRACE_RECORD prBssTraceRecord;
	UINT_32 i;
	UINT_8 aucMsg[SCAN_MSG_MAX_LEN];
	UINT_8 offset = 0;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (prConnSettings->ucSSIDLen == 0) {
		DBGLOG(SCN, LOUD, "Target BSS length is 0, ignore it!\n");
		return;
	}

	if (grScanTargetBssList.u4BSSIDCount > SCAN_TARGET_BSS_MAX_NUM) {
		DBGLOG(SCN, WARN, "u4BSSIDCount out of bound :%d\n", grScanTargetBssList.u4BSSIDCount);
		return;

	}

	offset += kalSnprintf(aucMsg + offset, SCAN_MSG_MAX_LEN - offset
		, "[%s: BSSIDNum:%d]:"
		, prConnSettings->aucSSID
		, grScanTargetBssList.u4BSSIDCount);

	for (i = 0 ; i < grScanTargetBssList.u4BSSIDCount ; i++) {
		prBssTraceRecord = &(grScanTargetBssList.prBssTraceRecord[i]);

		DBGLOG(SCN, LOUD, "dump:[%pM],Rssi=%d\n"
			, prBssTraceRecord->aucBSSID, RCPI_TO_dBm(prBssTraceRecord->ucRCPI));

		if (i == (SCAN_TARGET_BSS_MAX_NUM/2) ||
			(i == grScanTargetBssList.u4BSSIDCount-1)) {
			DBGLOG(SCN, INFO, "%s\n", aucMsg);
			offset = 0;
			kalMemZero(aucMsg, sizeof(aucMsg));
		}

		offset += kalSnprintf(aucMsg + offset, SCAN_MSG_MAX_LEN - offset
			, "%pM/%d,"
			, prBssTraceRecord->aucBSSID
			, RCPI_TO_dBm(prBssTraceRecord->ucRCPI));
	}

	grScanTargetBssList.u4BSSIDCount = 0;
	kalMemZero(grScanTargetBssList.prBssTraceRecord, sizeof(BSS_TRACE_RECORD) * SCAN_TARGET_BSS_MAX_NUM);

}

VOID wlanDebugHifDescriptorRecord(P_ADAPTER_T prAdapter, ENUM_AMPDU_TYPE type
	, ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T tcIndex, PUINT_8 pucBuffer)
{
	UINT_32 i;
	UINT_32 u4Offset;
	UINT_32 u4StartAddr;
	P_HIF_TX_DESC_T prTxDesc;
	P_HIF_RX_DESC_T prRxDesc;
	UINT_32 u4TcCount;

	if (pucBuffer == NULL) {
		DBGLOG(TX, ERROR, "wlanDebugHifDescriptorRecord pucBuffer is Null !");
		return;
	}


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
			DBGLOG(TX, ERROR, "Type :%d TC_INDEX :%d don't support !", type, tcIndex);
			return;
		}

		prTxDesc = (P_HIF_TX_DESC_T)pucBuffer;
		for (i = 0; i < u4TcCount ; i++)
			HAL_GET_APMCU_MEM(prAdapter, u4StartAddr, u4Offset, i, (PUINT_8) &prTxDesc[i]
				, sizeof(HIF_TX_DESC_T));


	} else if (type == MTK_AMPDU_RX_DESC) {

		if (tcIndex == DEBUG_TC0_INDEX) {
			u4TcCount = NIC_TX_INIT_BUFF_COUNT_TC0;
			u4StartAddr = AP_MCU_RX_DESC_ADDR;
			u4Offset = AP_MCU_BANK_OFFSET;
		} else {
			DBGLOG(RX, ERROR, "Type :%d TC_INDEX :%d don't support !", type, tcIndex);
			return;
		}

		prRxDesc = (P_HIF_RX_DESC_T)pucBuffer;
		for (i = 0; i < u4TcCount ; i++)
			HAL_GET_APMCU_MEM(prAdapter, u4StartAddr, u4Offset, i, (PUINT_8) &prRxDesc[i]
				, sizeof(HIF_RX_DESC_T));
	}

}

VOID wlanDebugHifDescriptorPrint(P_ADAPTER_T prAdapter, ENUM_AMPDU_TYPE type
	, ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T tcIndex, PUINT_8 pucBuffer)
{
	UINT_32 i;
	UINT_32 u4TcCount;
	P_HIF_TX_DESC_T prTxDesc;
	P_HIF_RX_DESC_T prRxDesc;

	if (pucBuffer == NULL) {
		DBGLOG(TX, ERROR, "wlanDebugHifDescriptorDump pucBuffer is Null !");
		return;
	}

	if (type == MTK_AMPDU_TX_DESC) {
		if (tcIndex == DEBUG_TC0_INDEX)
			u4TcCount = NIC_TX_INIT_BUFF_COUNT_TC0;
		else if (tcIndex == DEBUG_TC4_INDEX)
			u4TcCount = NIC_TX_BUFF_COUNT_TC4;
		else {
			DBGLOG(TX, ERROR, "Type :%d TC_INDEX :%d don't support !", type, tcIndex);
			return;
		}

		prTxDesc = (P_HIF_TX_DESC_T)pucBuffer;
		DBGLOG(TX, INFO, "Start dump Tx_desc from APMCU\n");
		for (i = 0; i < u4TcCount ; i++) {
			DBGLOG(TX, INFO
				, "TC%d[%d]uOwn:%2x,CS:%2x,R1:%2x,ND:0x%08x,SA: 0x%08x,R2:%x\n"
				, tcIndex, i, prTxDesc[i].ucOwn, prTxDesc[i].ucDescChksum
				, prTxDesc[i].u2Rsrv1, prTxDesc[i].u4NextDesc
				, prTxDesc[i].u4BufStartAddr, prTxDesc[i].u4Rsrv2);
		}

	} else if (type == MTK_AMPDU_RX_DESC) {

		if (tcIndex == DEBUG_TC0_INDEX)
			u4TcCount = NIC_TX_INIT_BUFF_COUNT_TC0;
		else {
			DBGLOG(RX, ERROR, "Type :%d TC_INDEX :%d don't support !", type, tcIndex);
			return;
		}

		prRxDesc = (P_HIF_RX_DESC_T)pucBuffer;
		DBGLOG(RX, INFO, "Start dump rx_desc from APMCU\n");
		for (i = 0; i < u4TcCount ; i++) {
			DBGLOG(RX, INFO
				, "RX%d[%d]uOwn:%2x,CS:%2x,TO:%x,CSI:%x,ND:0x%08x,SA:0x%08x,len:%x,R1:%x\n"
				, tcIndex, i, prRxDesc[i].ucOwn, prRxDesc[i].ucDescChksum
				, prRxDesc[i].ucEtherTypeOffset, prRxDesc[i].ucChkSumInfo
				, prRxDesc[i].u4NextDesc, prRxDesc[i].u4BufStartAddr
				, prRxDesc[i].u2RxBufLen, prRxDesc[i].u2Rsrv1);

		}
	}

}

VOID wlanDebugHifDescriptorDump(P_ADAPTER_T prAdapter, ENUM_AMPDU_TYPE type
	, ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T tcIndex)
{
	UINT_32 size = NIC_TX_BUFF_SUM;
	P_HIF_TX_DESC_T prTxDesc;
	P_HIF_RX_DESC_T prRxDesc;


	if (type == MTK_AMPDU_TX_DESC) {

		prTxDesc = (P_HIF_TX_DESC_T) kalMemAlloc(sizeof(HIF_TX_DESC_T) * size, VIR_MEM_TYPE);
		if (prTxDesc == NULL) {
			DBGLOG(TX, WARN, "wlanDebugHifDescriptorDump prTxDesc alloc fail!\n");
			return;
		}
		kalMemZero(prTxDesc, sizeof(HIF_TX_DESC_T) * size);
		wlanDebugHifDescriptorRecord(prAdapter, type, tcIndex, (PUINT_8)prTxDesc);
		wlanDebugHifDescriptorPrint(prAdapter, type, tcIndex, (PUINT_8)prTxDesc);
		kalMemFree(prTxDesc, VIR_MEM_TYPE, sizeof(HIF_TX_DESC_T));

	} else if (type == MTK_AMPDU_RX_DESC) {

		prRxDesc = (P_HIF_RX_DESC_T) kalMemAlloc(sizeof(HIF_RX_DESC_T) * size, VIR_MEM_TYPE);
		if (prRxDesc == NULL) {
			DBGLOG(RX, WARN, "wlanDebugHifDescriptorDump prRxDesc alloc fail!\n");
			return;
		}
		kalMemZero(prRxDesc, sizeof(HIF_RX_DESC_T) * size);
		wlanDebugHifDescriptorRecord(prAdapter, type, tcIndex, (PUINT_8)prRxDesc);
		wlanDebugHifDescriptorPrint(prAdapter, type, tcIndex, (PUINT_8)prRxDesc);
		kalMemFree(prRxDesc, VIR_MEM_TYPE, sizeof(P_HIF_RX_DESC_T));
	}
}
VOID wlanDebugScanRecord(P_ADAPTER_T prAdapter, ENUM_DBG_SCAN_T recordType)
{

	UINT_32 tcIndex = DEBUG_TC4_INDEX;
	UINT_32 type = MTK_AMPDU_TX_DESC;
	P_TX_CTRL_T pTxCtrl = &prAdapter->rTxCtrl;

	if (recordType == DBG_SCAN_WRITE_BEFORE) {
		wlanDebugHifDescriptorRecord(prAdapter, type, tcIndex
		, (PUINT_8)grScanHifDescRecord.pTxDescScanWriteBefore);
		grScanHifDescRecord.aucFreeBufCntScanWriteBefore = pTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX];
		grScanHifDescRecord.u8ScanWriteBeforeTime = sched_clock();
	} else if (recordType == DBG_SCAN_WRITE_DONE) {
		wlanDebugHifDescriptorRecord(prAdapter, type, tcIndex
		, (PUINT_8)grScanHifDescRecord.pTxDescScanWriteDone);
		grScanHifDescRecord.aucFreeBufCntScanWriteDone = pTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX];
		grScanHifDescRecord.u8ScanWriteDoneTime = sched_clock();
	}

}
VOID wlanDebugScanDump(P_ADAPTER_T prAdapter)
{
	UINT_32 tcIndex = DEBUG_TC4_INDEX;
	UINT_32 type = MTK_AMPDU_TX_DESC;

	DBGLOG(TX, INFO, "ScanReq hal write before:Time=%llu ,freeCnt=%d,dump tc4[0]~[3] desc!\n"
		, grScanHifDescRecord.u8ScanWriteBeforeTime
		, grScanHifDescRecord.aucFreeBufCntScanWriteBefore);
	wlanDebugHifDescriptorPrint(prAdapter, type, tcIndex
		, (PUINT_8)grScanHifDescRecord.pTxDescScanWriteBefore);

	DBGLOG(TX, INFO, "ScanReq hal write done:Time=%llu ,freeCnt=%d,dump tc4[0]~[3] desc!\n"
		, grScanHifDescRecord.u8ScanWriteDoneTime
		, grScanHifDescRecord.aucFreeBufCntScanWriteDone);
	wlanDebugHifDescriptorPrint(prAdapter, type, tcIndex
		, (PUINT_8)grScanHifDescRecord.pTxDescScanWriteDone);
}


VOID wlanReadFwStatus(P_ADAPTER_T prAdapter)
{
	static UINT_16 u2CurEntryCmd;
	P_COMMAND_ENTRY prCurCommand = &gprCommandEntry[u2CurEntryCmd];
	UINT_8 i = 0;
	GL_HIF_INFO_T *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	prCurCommand->u8ReadFwTime = sched_clock();
	HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurCommand->u4ReadFwValue);
	for (i = 0; i < READ_CPUPCR_MAX_NUM; i++)
		prCurCommand->arCpupcrValue[i] = MCU_REG_READL(prHifInfo, CONN_MCU_CPUPCR);
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
	/*for every cmd record FW mailbox*/
	HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &prCurCmd->u4RelCID);

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
				"%2d: Time %llu, Type %d, Content %08x, RelCID:%08x; %d: Time %llu, Type %d, Content %08x, RelCID:%08x\n",
				i*2, prCmd[i*2].u8TxTime, prCmd[i*2].eCmdType, *(PUINT_32)(&prCmd[i*2].u.rCmd.ucCID),
				prCmd[i*2].u4RelCID,
				i*2+1, prCmd[i*2+1].u8TxTime, prCmd[i*2+1].eCmdType,
				*(PUINT_32)(&prCmd[i*2+1].u.rCmd.ucCID),
				prCmd[i*2+1].u4RelCID);
			if (bufLen <= 0 || (UINT_32)bufLen >= maxLen)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
		for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM/2; i++) {
			bufLen = snprintf(pucBuf, maxLen,
				"%2d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %2d: Time %llu, Tc4Cnt %d, Free %d CID %08x\n",
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
		DBGLOG(TX, INFO,
			"%2d: Time %llu, Type %d, Content %08x, RelCID:%08x; %2d: Time %llu, Type %d, Content %08x, RelCID:%08x\n",
			i*2, prCmd[i*2].u8TxTime, prCmd[i*2].eCmdType, *(PUINT_32)(&prCmd[i*2].u.rCmd.ucCID),
			prCmd[i*2].u4RelCID,
			i*2+1, prCmd[i*2+1].u8TxTime, prCmd[i*2+1].eCmdType,
			*(PUINT_32)(&prCmd[i*2+1].u.rCmd.ucCID),
			prCmd[i*2+1].u4RelCID);
		DBGLOG(TX, INFO,
			"%2d: Time %llu, Type %d, Content %08x, RelCID:%08x; %2d: Time %llu, Type %d, Content %08x, RelCID:%08x\n",
			i*4+2, prCmd[i*4+2].u8TxTime, prCmd[i*4+2].eCmdType,
			*(PUINT_32)(&prCmd[i*4+2].u.rCmd.ucCID), prCmd[i*4+2].u4RelCID,
			i*4+3, prCmd[i*4+3].u8TxTime, prCmd[i*4+3].eCmdType,
			*(PUINT_32)(&prCmd[i*4+3].u.rCmd.ucCID),
			prCmd[i*4+3].u4RelCID);
	}
	for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM/4; i++) {
		DBGLOG(TX, INFO,
			"%2d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %2d: Time %llu, Tc4Cnt %d, Free %d, CID %08x;",
			i*4, prTcRel[i*4].u8RelaseTime, prTcRel[i*4].ucTc4RelCnt,
			prTcRel[i*4].ucAvailableTc4, prTcRel[i*4].u4RelCID,
			i*4+1, prTcRel[i*4+1].u8RelaseTime, prTcRel[i*4+1].ucTc4RelCnt,
			prTcRel[i*4+1].ucAvailableTc4, prTcRel[i*4+1].u4RelCID);
		DBGLOG(TX, INFO,
			"%2d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %2d: Time %llu, Tc4Cnt %d, Free %d, CID %08x\n",
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
		LOG_FUNC(
		"%d: Time %llu,Content %08x,Count %x,RelCID %08x,FwValue %08x,Time %llu,CPUPCR 0x%08x 0x%08x 0x%08x\n",
			i, prCmd[i].u8TxTime, *(PUINT_32)(&prCmd[i].rCmd.ucCID),
			prCmd[i].u2Counter, prCmd[i].u4RelCID,
			prCmd[i].u4ReadFwValue, prCmd[i].u8ReadFwTime,
			prCmd[i].arCpupcrValue[0], prCmd[i].arCpupcrValue[1], prCmd[i].arCpupcrValue[2]);
	}
}

VOID wlanFWDLDebugInit(VOID)
{
	u4FWDL_packet_count = -1;
	gprFWDLDebug = (P_FWDL_DEBUG_T) kalMemAlloc(sizeof(FWDL_DEBUG_T)*MAX_FW_IMAGE_PACKET_COUNT,
			VIR_MEM_TYPE);

	if (gprFWDLDebug)
		kalMemZero(gprFWDLDebug, sizeof(FWDL_DEBUG_T)*MAX_FW_IMAGE_PACKET_COUNT);
	else
		DBGLOG(INIT, ERROR, "wlanFWDLDebugInit alloc memory error\n");
}

VOID wlanFWDLDebugAddTxStartTime(UINT_32 u4TxStartTime)
{
	if ((gprFWDLDebug != NULL) && (u4FWDL_packet_count < MAX_FW_IMAGE_PACKET_COUNT))
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4TxStartTime = u4TxStartTime;
}

VOID wlanFWDLDebugAddTxDoneTime(UINT_32 u4TxDoneTime)
{
	if ((gprFWDLDebug != NULL) && (u4FWDL_packet_count < MAX_FW_IMAGE_PACKET_COUNT))
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4TxDoneTime = u4TxDoneTime;
}

VOID wlanFWDLDebugAddRxStartTime(UINT_32 u4RxStartTime)
{
	if ((gprFWDLDebug != NULL) && (u4FWDL_packet_count < MAX_FW_IMAGE_PACKET_COUNT))
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4RxStartTime = u4RxStartTime;
}

VOID wlanFWDLDebugAddRxDoneTime(UINT_32 u4RxDoneTime)
{
	if ((gprFWDLDebug != NULL) && (u4FWDL_packet_count < MAX_FW_IMAGE_PACKET_COUNT))
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4RxDoneTime = u4RxDoneTime;
}

VOID wlanFWDLDebugStartSectionPacketInfo(UINT_32 u4Section, UINT_32 u4DownloadSize,
	UINT_32 u4ResponseTime)
{
	u4FWDL_packet_count++;
	if ((gprFWDLDebug != NULL) && (u4FWDL_packet_count < MAX_FW_IMAGE_PACKET_COUNT)) {
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4Section = u4Section;
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4DownloadSize = u4DownloadSize;
		(*(gprFWDLDebug+u4FWDL_packet_count)).u4ResponseTime = u4ResponseTime;
	}
}

VOID wlanFWDLDebugDumpInfo(VOID)
{
	UINT_32 i;

	for (i = 0; i <= u4FWDL_packet_count; i++) {
		/* Tx:[TxStartTime][TxDoneTime]
		*	Pkt:[DL Pkt Section][DL Pkt Size][DL Pkt Resp Time]
		*/
		DBGLOG(INIT, TRACE, "wlanFWDLDumpLog > Tx:[%u][%u] Rx:[%u][%u] Pkt:[%d][%d][%u]\n"
		, (*(gprFWDLDebug+i)).u4TxStartTime, (*(gprFWDLDebug+i)).u4TxDoneTime
		, (*(gprFWDLDebug+i)).u4RxStartTime, (*(gprFWDLDebug+i)).u4RxDoneTime
		, (*(gprFWDLDebug+i)).u4Section, (*(gprFWDLDebug+i)).u4DownloadSize
		, (*(gprFWDLDebug+i)).u4ResponseTime);
	}
}

UINT_32 wlanFWDLDebugGetPktCnt(VOID)
{
	return (u4FWDL_packet_count + 1);
}

VOID wlanFWDLDebugUninit(VOID)
{
	kalMemFree(gprFWDLDebug, VIR_MEM_TYPE, sizeof(FWDL_DEBUG_T)*MAX_FW_IMAGE_PACKET_COUNT);
	gprFWDLDebug = NULL;
	u4FWDL_packet_count = -1;
}
VOID wlanDumpMcuChipId(P_ADAPTER_T prAdapter)
{
	GL_HIF_INFO_T *pHifInfo;

	ASSERT(prAdapter);

	pHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	DBGLOG(INIT, INFO, "Offset:0x%x, Value:0x%08x ", CONN_MCU_CHIPID, MCU_REG_READL(pHifInfo, CONN_MCU_CHIPID));
}
#if CFG_SUPPORT_EMI_DEBUG
void wlanDumpFwInforPrintBuff(PUINT8 pBuffer, UINT32 u4Len)
{
	UINT32 i = 0;
	UINT32 idx = 0;
	UINT8 aucOutput[WLAN_EMI_DEBUG_LINE_SIZE] = {0};

	DBGLOG(RX, TRACE, "%s Start Print %d B!\n", __func__, u4Len);
#if 0
	dumpMemory8(pBuffer, u4Len);
#endif
	for (i = 0; i < u4Len; i++) {
		aucOutput[idx] = pBuffer[i];
		if (pBuffer[i] == '\n' || i == u4Len - 1) {
			aucOutput[idx + 1] = '\0';
			DBGLOG(RX, INFO, "%s", aucOutput);
			idx = 0;
		} else
			idx++;

		if (idx == WLAN_EMI_DEBUG_LINE_SIZE - 1) {
			pBuffer[idx] = '\0';
			aucOutput[idx] = '\0';
			DBGLOG(RX, INFO, "%s", aucOutput);
			idx = 0;
		}
	}
}


VOID wlanReadFwInfoFromEmi(IN PUINT32 pAddr)
{
	UINT32 offset = 0;
	UINT32 u4Buflen = 0;
	UINT32 cur_idx_pagedtrace;
	PUINT8 pEmiBuf;
	UINT32 i = 0;

	offset = 0;
	u4Buflen = 0;

	DBGLOG(RX, TRACE, "%s Start !\n", __func__);

	if (gPrevIdxPagedtrace == 0xFFFF) {
		/* FW will provide start index for the first time. */
		gPrevIdxPagedtrace = *pAddr;
		DBGLOG(RX, WARN, "Invalid PreIdx, reset PreIdx to %d\n", gPrevIdxPagedtrace);
		return;
	}

	pEmiBuf = kalMemAlloc(WLAN_EMI_DEBUG_BUF_SIZE, VIR_MEM_TYPE);
	if (pEmiBuf == NULL) {
		DBGLOG(RX, WARN, "Buffer allocate fail !\n");
		return;
	}

	kalMemSet(pEmiBuf, 0, WLAN_EMI_DEBUG_BUF_SIZE);
	cur_idx_pagedtrace = *pAddr;

	DBGLOG(RX, TRACE, ">>Addr:0x%p CurIdx:%d,PreIdx:%d!\n", pAddr, cur_idx_pagedtrace, gPrevIdxPagedtrace);

	if (cur_idx_pagedtrace > gPrevIdxPagedtrace) {

		u4Buflen = cur_idx_pagedtrace - gPrevIdxPagedtrace;
		if (u4Buflen > WLAN_EMI_DEBUG_BUF_SIZE) {
			for (offset = gPrevIdxPagedtrace ; offset < cur_idx_pagedtrace
				; offset += WLAN_EMI_DEBUG_BUF_SIZE) {
				kalGetFwInfoFormEmi(1, offset, (PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				i++;
			}
			u4Buflen = cur_idx_pagedtrace - (i-1)*WLAN_EMI_DEBUG_BUF_SIZE - gPrevIdxPagedtrace;
			kalGetFwInfoFormEmi(1, (i-1)*WLAN_EMI_DEBUG_BUF_SIZE, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		} else {
			kalGetFwInfoFormEmi(1, gPrevIdxPagedtrace, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		}
		gPrevIdxPagedtrace = cur_idx_pagedtrace;
	} else if (cur_idx_pagedtrace < gPrevIdxPagedtrace) {
		if (gPrevIdxPagedtrace >= 0x8000) {
			DBGLOG(RX, WARN, "++ prev_idx_pagedtrace invalid ...+\n");
			gPrevIdxPagedtrace = 0x8000 - 1;
			kalMemFree(pEmiBuf, VIR_MEM_TYPE, WLAN_EMI_DEBUG_BUF_SIZE);
			return;
		}

		i = 0;
		u4Buflen = 0x8000 - gPrevIdxPagedtrace - 1;
		DBGLOG(RX, WARN, "-- CONNSYS paged trace ascii output (cont...) --\n");
		if (u4Buflen > WLAN_EMI_DEBUG_BUF_SIZE) {
			for (offset = gPrevIdxPagedtrace ; offset < 0x8000
				; offset += WLAN_EMI_DEBUG_BUF_SIZE) {
				kalGetFwInfoFormEmi(1, offset, (PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				i++;
			}
			u4Buflen = 0x8000 - (i-1)*WLAN_EMI_DEBUG_BUF_SIZE - gPrevIdxPagedtrace-1;
			kalGetFwInfoFormEmi(1, (i-1)*WLAN_EMI_DEBUG_BUF_SIZE, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		} else {
			kalGetFwInfoFormEmi(1, gPrevIdxPagedtrace, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		}

		i = 0;
		u4Buflen = cur_idx_pagedtrace;
		DBGLOG(RX, WARN, " -- CONNSYS paged trace ascii output (end) --\n");
		if (u4Buflen > WLAN_EMI_DEBUG_BUF_SIZE) {
			for (offset = 0x0 ; offset < u4Buflen ; offset += WLAN_EMI_DEBUG_BUF_SIZE) {
				kalGetFwInfoFormEmi(1, offset, (PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, WLAN_EMI_DEBUG_BUF_SIZE);
				i++;
			}
			u4Buflen = cur_idx_pagedtrace - (i-1)*WLAN_EMI_DEBUG_BUF_SIZE;
			kalGetFwInfoFormEmi(1, (i-1)*WLAN_EMI_DEBUG_BUF_SIZE, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		} else {
			kalGetFwInfoFormEmi(1, 0x0, (PUINT_8)pEmiBuf, u4Buflen);
			wlanDumpFwInforPrintBuff((PUINT_8)pEmiBuf, u4Buflen);
		}



		gPrevIdxPagedtrace = cur_idx_pagedtrace;
	}

	kalMemFree(pEmiBuf, VIR_MEM_TYPE, WLAN_EMI_DEBUG_BUF_SIZE);
}
#endif
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
		((pucEth[ETH_TYPE_LEN_OFFSET] << 8) | (pucEth[ETH_TYPE_LEN_OFFSET + 1])) != ETH_P_IP)
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
VOID wlanDebugCommandRecodTime(P_CMD_INFO_T prCmdInfo)
{
	UINT_32 u4CurCmdIndex = 0;
	struct COMMAND_DEBUG_INFO *pCommandDebugInfo = NULL;

	if (prCmdInfo == NULL) {
		DBGLOG(RX, WARN, "prCmdInfo is NULL!\n");
		return;
	}
	/*Getting a index by using  (seq mod size)*/
	u4CurCmdIndex = prCmdInfo->ucCmdSeqNum % TXED_CMD_TRACE_BUF_MAX_NUM;
	pCommandDebugInfo = &(gprCommandDebugInfo[u4CurCmdIndex]);

	pCommandDebugInfo->ucCID = prCmdInfo->ucCID;
	pCommandDebugInfo->ucCmdSeqNum = prCmdInfo->ucCmdSeqNum;
	pCommandDebugInfo->u4InqueTime = prCmdInfo->u4InqueTime;
	pCommandDebugInfo->u4SendToFwTime = prCmdInfo->u4SendToFwTime;
	pCommandDebugInfo->u4FwResponseTime = prCmdInfo->u4FwResponseTime;

	DBGLOG(RX, LOUD, "(%d),CID:0x%02x,SEQ:%d,INQ[%u],SEND[%u],RSP[%u],now[%u]\n"
	, u4CurCmdIndex
	, pCommandDebugInfo->ucCID
	, pCommandDebugInfo->ucCmdSeqNum
	, pCommandDebugInfo->u4InqueTime
	, pCommandDebugInfo->u4SendToFwTime
	, pCommandDebugInfo->u4FwResponseTime, kalGetTimeTick());

}
VOID wlanDebugCommandRecodDump(VOID)
{
	UINT_16 i = 0;
	UINT_8 pucMsg[CMD_BUF_MSG_LENGTH];
	UINT_32 offsetMsg = 0;

	DBGLOG(RX, INFO, "now getTimeTick:%u\n", kalGetTimeTick());
	kalMemZero(pucMsg, sizeof(UINT_8)*CMD_BUF_MSG_LENGTH);

	for (i = 0; i < TXED_CMD_TRACE_BUF_MAX_NUM; i++) {
		offsetMsg += kalSnprintf(pucMsg + offsetMsg, CMD_BUF_MSG_LENGTH - offsetMsg
		, "SEQ:%d,CID:0x%02x,,INQ:%u,SEND:%u,RSP:%u |"
		, gprCommandDebugInfo[i].ucCmdSeqNum
		, gprCommandDebugInfo[i].ucCID
		, gprCommandDebugInfo[i].u4InqueTime
		, gprCommandDebugInfo[i].u4SendToFwTime
		, gprCommandDebugInfo[i].u4FwResponseTime);

		if (i%5 == 0 && i > 0) {
			DBGLOG(RX, INFO, "%s\n", pucMsg);
			kalMemZero(pucMsg, sizeof(UINT_8)*CMD_BUF_MSG_LENGTH);
			offsetMsg = 0;
		}
	}
	DBGLOG(RX, INFO, "%s\n", pucMsg);
	kalMemZero(gprCommandDebugInfo, sizeof(struct COMMAND_DEBUG_INFO)*TXED_CMD_TRACE_BUF_MAX_NUM);
}

VOID wlanDbgLogLevelInit(VOID)
{
#if DBG
	/* Adjust log level to extreme in DBG mode */
	u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
	u4FwLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
#else
	u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
	u4FwLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
#endif /* DBG */

	/* Set driver level first, and set fw log level until fw is ready */
	wlanDbgSetLogLevelImpl(NULL, ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			ENUM_WIFI_LOG_MODULE_DRIVER, u4DriverLogLevel);
}

VOID wlanDbgLogLevelUninit(VOID)
{
	UINT_8 i;

	if (u4DriverLogLevel > ENUM_WIFI_LOG_LEVEL_OFF ||
			u4FwLogLevel > ENUM_WIFI_LOG_LEVEL_OFF) {
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
		u4FwLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
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
		switch (ucModule) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
			u4Level = u4DriverLogLevel;
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
			u4Level = u4FwLogLevel;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return u4Level;
}

VOID wlanDbgSetLogLevelImpl(IN P_ADAPTER_T prAdapter, UINT_32 u4Version,
		UINT_32 u4Module, UINT_32 u4level)
{
	do {
		switch (u4Version) {
		case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
			switch (u4Module) {
			case ENUM_WIFI_LOG_MODULE_DRIVER:
			{
				UINT_8 i;
				UINT_32 u4DriverLogMask;

				if (u4level >= ENUM_WIFI_LOG_LEVEL_NUM)
					return;

				u4DriverLogLevel = u4level;

				if (u4DriverLogLevel == ENUM_WIFI_LOG_LEVEL_OFF)
					u4DriverLogMask = DBG_LOG_LEVEL_OFF;
				else if (u4DriverLogLevel == ENUM_WIFI_LOG_LEVEL_DEFAULT)
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

				if (u4level >= ENUM_WIFI_LOG_LEVEL_NUM)
					return;

				u4FwLogLevel = u4level;

				kalMemZero(&cmd, sizeof(struct CMD_EVENT_LOG_LEVEL));
				cmd.u4Version = u4Version;
				cmd.u4LogLevel = u4FwLogLevel;

				wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_WIFI_LOG_LEVEL,
					   TRUE,
					   FALSE,
					   TRUE,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
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
	} while (FALSE);
#if KERNEL_VERSION(4, 14, 0) >= CFG80211_VERSION_CODE
	if (u4DriverLogLevel > ENUM_WIFI_LOG_LEVEL_OFF ||
		u4FwLogLevel > ENUM_WIFI_LOG_LEVEL_OFF) {
		DBGLOG(OID, INFO,
			"Disable printk to much. driver: %d, fw: %d\n",
			u4DriverLogLevel,
			u4FwLogLevel);
		set_logtoomuch_enable(0);
	}
#endif
}

VOID wlanDbgLevelSync(VOID)
{
	UINT_8 i = 0;
	UINT_32 u4Level = ENUM_WIFI_LOG_LEVEL_EXTREME;

	do {
		/* get the lowest level as module's level */
		for (i = 0; i < DBG_MODULE_NUM; i++)
			u4Level &= aucDebugModule[i];

		if (u4Level & ENUM_WIFI_LOG_LEVEL_EXTREME)
			u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
		else if (u4Level & ENUM_WIFI_LOG_LEVEL_DEFAULT)
			u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
		else
			u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;

		DBGLOG(OID, INFO,
			"Driver log level after sync from proc: %d\n",
			u4DriverLogLevel);
	} while (FALSE);
}
