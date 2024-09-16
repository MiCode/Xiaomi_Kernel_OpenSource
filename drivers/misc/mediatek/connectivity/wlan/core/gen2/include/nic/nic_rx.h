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

#ifndef _NIC_RX_H
#define _NIC_RX_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
extern void kalDevLoopbkRxHandle(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MAX_SEQ_NO                  4095
#define MAX_SEQ_NO_COUNT            4096
#define HALF_SEQ_NO_CNOUT           2048

#define HALF_SEQ_NO_COUNT           2048

#define MT6620_FIXED_WIN_SIZE         64
#define CFG_RX_MAX_BA_ENTRY            4
#define CFG_RX_MAX_BA_TID_NUM          8

#if (CFG_SRAM_SIZE_OPTION == 1)
/* set default RXBA size to 16, for DIR-635/DIR-655 IOT issue
 * (ALPS02494017 BA size must be power of 2)
 */
#define IOT_RX_BA_SIZE 16
#elif (CFG_SRAM_SIZE_OPTION == 0)
#define IOT_RX_BA_SIZE 8
#endif

#define RX_STATUS_FLAG_MORE_PACKET    BIT(30)
#define RX_STATUS_CHKSUM_MASK         BITS(0, 10)

#define RX_RFB_LEN_FIELD_LEN        4
#define RX_HEADER_OFFSET            2

#define RX_RETURN_INDICATED_RFB_TIMEOUT_SEC      3

#define RX_FW_FLUSH_PKT_THRESHOLD            20


#if defined(_HIF_SDIO) && defined(WINDOWS_DDK)
/*! On XP, maximum Tx+Rx Statue <= 64-4(HISR)*/
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#else
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_RX_STATISTIC_COUNTER_T {
	RX_MPDU_TOTAL_COUNT = 0,
	RX_SIZE_ERR_DROP_COUNT,

	RX_DATA_INDICATION_COUNT,
	RX_DATA_RETURNED_COUNT,
	RX_DATA_RETAINED_COUNT,

	RX_DROP_TOTAL_COUNT,
	RX_TYPE_ERR_DROP_COUNT,
	RX_CLASS_ERR_DROP_COUNT,
	RX_DST_NULL_DROP_COUNT,

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	RX_CSUM_TCP_FAILED_COUNT,
	RX_CSUM_UDP_FAILED_COUNT,
	RX_CSUM_IP_FAILED_COUNT,
	RX_CSUM_TCP_SUCCESS_COUNT,
	RX_CSUM_UDP_SUCCESS_COUNT,
	RX_CSUM_IP_SUCCESS_COUNT,
	RX_CSUM_UNKNOWN_L4_PKT_COUNT,
	RX_CSUM_UNKNOWN_L3_PKT_COUNT,
	RX_IP_V6_PKT_CCOUNT,
#endif
	RX_STATISTIC_COUNTER_NUM
} ENUM_RX_STATISTIC_COUNTER_T;

typedef enum _ENUM_RX_PKT_DESTINATION_T {
	RX_PKT_DESTINATION_HOST,	/* to OS */
	RX_PKT_DESTINATION_FORWARD,	/* to TX queue for forward, AP mode */
	RX_PKT_DESTINATION_HOST_WITH_FORWARD,	/* to both TX and OS, AP mode broadcast packet */
	RX_PKT_DESTINATION_NULL,	/* packet to be freed */
	RX_PKT_DESTINATION_NUM
} ENUM_RX_PKT_DESTINATION_T;

struct _SW_RFB_T {
	QUE_ENTRY_T rQueEntry;
	PVOID pvPacket;		/*!< ptr to rx Packet Descriptor */
	PUINT_8 pucRecvBuff;	/*!< ptr to receive data buffer */
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_32 u4HifRxHdrFlag;
	PVOID pvHeader;
	UINT_16 u2PacketLen;
	UINT_16 u2HeaderLen;
	UINT_16 u2SSN;
	UINT_8 ucTid;
	UINT_8 ucWlanIdx;
	UINT_8 ucPacketType;
	UINT_8 ucStaRecIdx;

	ENUM_CSUM_RESULT_T aeCSUM[CSUM_TYPE_NUM];
	ENUM_RX_PKT_DESTINATION_T eDst;
	ENUM_TRAFFIC_CLASS_INDEX_T eTC;	/* only valid when eDst == FORWARD */

	UINT_64 rRxTime;
};

/*! RX configuration type structure */
typedef struct _RX_CTRL_T {
	UINT_32 u4RxCachedSize;
	PUINT_8 pucRxCached;
	QUE_T rFreeSwRfbList;
	QUE_T rReceivedRfbList;
	QUE_T rIndicatedRfbList;
	QUE_T rUnInitializedRfbList;

#if CFG_SDIO_RX_AGG
	PUINT_8 pucRxCoalescingBufPtr;
#endif

	PVOID apvIndPacket[CFG_RX_MAX_PKT_NUM];
	PVOID apvRetainedPacket[CFG_RX_MAX_PKT_NUM];

	UINT_8 ucNumIndPacket;
	UINT_8 ucNumRetainedPacket;
	UINT_64 au8Statistics[RX_STATISTIC_COUNTER_NUM];	/*!< RX Counters */

#if CFG_HIF_STATISTICS
	UINT_32 u4TotalRxAccessNum;
	UINT_32 u4TotalRxPacketNum;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	UINT_32 u4QueuedCnt;
	UINT_32 u4DequeuedCnt;
#endif

#if CFG_RX_PKTS_DUMP
	UINT_32 u4RxPktsDumpTypeMask;
#endif

#if CFG_SUPPORT_MULTITHREAD
	QUE_T rRxDataRfbList;
#endif

} RX_CTRL_T, *P_RX_CTRL_T;

typedef struct _RX_MAILBOX_T {
	UINT_32 u4RxMailbox[2];	/* for Device-to-Host Mailbox */
} RX_MAILBOX_T, *P_RX_MAILBOX_T;

typedef WLAN_STATUS(*PROCESS_RX_MGT_FUNCTION) (P_ADAPTER_T, P_SW_RFB_T);

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
#define RX_INC_CNT(prRxCtrl, eCounter)              \
	{((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter]++; }

#define RX_ADD_CNT(prRxCtrl, eCounter, u8Amount)    \
	{((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter] += (UINT_64)u8Amount; }

#define RX_GET_CNT(prRxCtrl, eCounter)              \
	(((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter])

#define RX_RESET_ALL_CNTS(prRxCtrl)                 \
	{kalMemZero(&prRxCtrl->au8Statistics[0], sizeof(prRxCtrl->au8Statistics)); }

#define RX_STATUS_TEST_MORE_FLAG(flag) \
	((BOOLEAN)((flag & RX_STATUS_FLAG_MORE_PACKET) ? TRUE : FALSE))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

VOID nicRxInitialize(IN P_ADAPTER_T prAdapter);

VOID nicRxUninitialize(IN P_ADAPTER_T prAdapter);

VOID nicRxProcessRFBs(IN P_ADAPTER_T prAdapter);

#if !CFG_SDIO_INTR_ENHANCE
VOID nicRxReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

#else
VOID nicRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS
nicRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DataPort, IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb);
#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SDIO_RX_AGG
VOID nicRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS nicRxSetupRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prRfb);

VOID nicRxReturnRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxReturnRFBwithUninit(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb
	, IN BOOLEAN fgIsUninitRfb);

VOID nicProcessRxInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicRxProcessPktWithoutReorder(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxProcessForwardPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxProcessGOBroadcastPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxFillRFB(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

VOID nicRxProcessDataPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

VOID nicRxProcessMgmtPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID nicRxFillChksumStatus(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb, IN UINT_32 u4TcpUdpIpCksStatus);

VOID nicRxUpdateCSUMStatistics(IN P_ADAPTER_T prAdapter, IN const ENUM_CSUM_RESULT_T aeCSUM[]);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

VOID nicRxQueryStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count);

VOID nicRxClearStatistics(IN P_ADAPTER_T prAdapter);

VOID nicRxQueryStatistics(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count);

WLAN_STATUS
nicRxWaitResponse(IN P_ADAPTER_T prAdapter,
		  IN UINT_8 ucPortIdx, OUT PUINT_8 pucRspBuffer, IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length);

VOID nicRxEnablePromiscuousMode(IN P_ADAPTER_T prAdapter);

VOID nicRxDisablePromiscuousMode(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicRxFlush(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicRxProcessActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

#endif /* _NIC_RX_H */
