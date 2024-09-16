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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/cnm_mem.h#1
*/

/*
 * ! \file   "cnm_mem.h"
 *  \brief  In this file we define the structure of the control unit of
 *  packet buffer and MGT/MSG Memory Buffer.
 */

#ifndef _CNM_MEM_H
#define _CNM_MEM_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "qosmap.h"
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#ifndef POWER_OF_2
#define POWER_OF_2(n)                           BIT(n)
#endif

/* Size of a basic management buffer block in power of 2 */
#define MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2        7	/* 7 to the power of 2 = 128 */
#define MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2        5	/* 5 to the power of 2 = 32 */

/* Size of a basic management buffer block */
#define MGT_BUF_BLOCK_SIZE                      POWER_OF_2(MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2)
#define MSG_BUF_BLOCK_SIZE                      POWER_OF_2(MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2)

/* Total size of (n) basic management buffer blocks */
#define MGT_BUF_BLOCKS_SIZE(n)                  ((UINT_32)(n) << MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2)
#define MSG_BUF_BLOCKS_SIZE(n)                  ((UINT_32)(n) << MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2)

/* Number of management buffer block */
#define MAX_NUM_OF_BUF_BLOCKS                   32	/* Range: 1~32 */

/* Size of overall management frame buffer */
#define MGT_BUFFER_SIZE                         (MAX_NUM_OF_BUF_BLOCKS * MGT_BUF_BLOCK_SIZE)
#define MSG_BUFFER_SIZE                         (MAX_NUM_OF_BUF_BLOCKS * MSG_BUF_BLOCK_SIZE)

/* STA_REC related definitions */
#define STA_REC_INDEX_BMCAST        0xFF
#define STA_REC_INDEX_NOT_FOUND     0xFE
#define STA_WAIT_QUEUE_NUM          5	/* Number of SW queues in each STA_REC: AC0~AC4 */
#define SC_CACHE_INDEX_NUM          5	/* Number of SC caches in each STA_REC: AC0~AC4 */

/* P2P related definitions */
#ifdef CFG_ENABLE_WIFI_DIRECT
/* Moved from p2p_fsm.h */
#define WPS_ATTRI_MAX_LEN_DEVICE_NAME               32	/* 0x1011 */
#define P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT        8	/* NOTE(Kevin): Shall <= 16 */
#endif

/* Define the argument of cnmStaFreeAllStaByNetwork when all station records
 * will be free. No one will be free
 */
#define STA_REC_EXCLUDE_NONE        CFG_STA_REC_NUM

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#if ((MAX_NUM_OF_BUF_BLOCKS > 32) || (MAX_NUM_OF_BUF_BLOCKS <= 0))
#error > #define MAX_NUM_OF_MGT_BUF_BLOCKS : Out of boundary !
#elif MAX_NUM_OF_BUF_BLOCKS > 16
typedef UINT_32 BUF_BITMAP;
#elif MAX_NUM_OF_BUF_BLOCKS > 8
typedef UINT_16 BUF_BITMAP;
#else
typedef UINT_8 BUF_BITMAP;
#endif /* MAX_NUM_OF_MGT_BUF_BLOCKS */

/* Control variable of TX management memory pool */
typedef struct _BUF_INFO_T {
	PUINT_8 pucBuf;

#if CFG_DBG_MGT_BUF
	UINT_32 u4AllocCount;
	UINT_32 u4FreeCount;
	UINT_32 u4AllocNullCount;
#endif				/* CFG_DBG_MGT_BUF */

	BUF_BITMAP rFreeBlocksBitmap;
	UINT_8 aucAllocatedBlockNum[MAX_NUM_OF_BUF_BLOCKS];
} BUF_INFO_T, *P_BUF_INFO_T;

/* Wi-Fi divides RAM into three types
 * MSG:     Mailbox message (Small size)
 * BUF:     HW DMA buffers (HIF/MAC)
 */
typedef enum _ENUM_RAM_TYPE_T {
	RAM_TYPE_MSG = 0,
	RAM_TYPE_BUF
} ENUM_RAM_TYPE_T, P_ENUM_RAM_TYPE_T;

typedef enum _ENUM_BUFFER_SOURCE_T {
	BUFFER_SOURCE_HIF_TX0 = 0,
	BUFFER_SOURCE_HIF_TX1,
	BUFFER_SOURCE_MAC_RX,
	BUFFER_SOURCE_MNG,
	BUFFER_SOURCE_BCN,
	BUFFER_SOURCE_NUM
} ENUM_BUFFER_SOURCE_T, *P_ENUM_BUFFER_SOURCE_T;

typedef enum _ENUM_SEC_STATE_T {
	SEC_STATE_INIT,
	SEC_STATE_INITIATOR_PORT_BLOCKED,
	SEC_STATE_RESPONDER_PORT_BLOCKED,
	SEC_STATE_CHECK_OK,
	SEC_STATE_SEND_EAPOL,
	SEC_STATE_SEND_DEAUTH,
	SEC_STATE_COUNTERMEASURE,
	SEC_STATE_NUM
} ENUM_SEC_STATE_T;

typedef struct _TSPEC_ENTRY_T {
	UINT_8 ucStatus;
	UINT_8 ucToken;		/* Dialog Token in ADDTS_REQ or ADDTS_RSP */
	UINT_16 u2MediumTime;
	UINT_32 u4TsInfo;
	/* PARAM_QOS_TS_INFO rParamTsInfo; */
	/* Add other retained QoS parameters below */
} TSPEC_ENTRY_T, *P_TSPEC_ENTRY_T, TSPEC_TABLE_ENTRY_T, *P_TSPEC_TABLE_ENTRY_T;

#if 0
typedef struct _SEC_INFO_T {

	ENUM_SEC_STATE_T ePreviousState;
	ENUM_SEC_STATE_T eCurrentState;

	BOOL fg2nd1xSend;
	BOOL fgKeyStored;

	UINT_8 aucStoredKey[64];

	BOOL fgAllowOnly1x;
} SEC_INFO_T, *P_SEC_INFO_T;
#endif

#define MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS     3

#define UPDATE_BSS_RSSI_INTERVAL_SEC            3	/* Seconds */

/* Fragment information structure */
typedef struct _FRAG_INFO_T {
	UINT_16 u2NextFragSeqCtrl;
	PUINT_8 pucNextFragStart;
	P_SW_RFB_T pr1stFrag;
	OS_SYSTIME rReceiveLifetimeLimit;	/* The receive time of 1st fragment */
} FRAG_INFO_T, *P_FRAG_INFO_T;

#if CFG_SUPPORT_802_11W
/* AP PMF */
struct AP_PMF_CFG {
	BOOLEAN fgMfpc;
	BOOLEAN fgMfpr;
	BOOLEAN fgSha256;
	BOOLEAN fgAPApplyPmfReq;
	BOOLEAN fgBipKeyInstalled;
};

struct STA_PMF_CFG {
	BOOLEAN fgMfpc;
	BOOLEAN fgMfpr;
	BOOLEAN fgSha256;
	BOOLEAN fgSaeRequireMfp;
	BOOLEAN fgApplyPmf;
	BOOLEAN fgBipKeyInstalled;

	/* for certification 4.3.3.1, 4.3.3.2 TX unprotected deauth */
	BOOLEAN fgRxDeauthResp;

	/* For PMF SA query TX request retry a timer */
	/* record the start time of 1st SAQ request */
	UINT_32 u4SAQueryStart;

	UINT_32 u4SAQueryCount;
	UINT_8 ucSAQueryTimedOut;	/* retry more than 1000ms */
	TIMER_T rSAQueryTimer;
	UINT_16 u2TransactionID;
};
#endif


/* Define STA record structure */
struct _STA_RECORD_T {
	LINK_ENTRY_T rLinkEntry;
	UINT_8 ucIndex;		/* Not modify it except initializing */
	UINT_8 ucWlanIndex;	/* WLAN table index */

	UINT_8 ucBMCWlanIndex;	/* The BSS STA Rx WLAN index,  IBSS Rx BC WLAN table index, work at IBSS Open and WEP */

	BOOLEAN fgIsInUse;	/* Indicate if this entry is in use or not */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];	/* MAC address */

	/* SAA/AAA */
	ENUM_AA_STATE_T eAuthAssocState;	/* Store STATE Value used in SAA/AAA */
	UINT_8 ucAuthAssocReqSeqNum;

	ENUM_STA_TYPE_T eStaType;	/* Indicate the role of this STA in
					 * the network (for example, P2P GO)
					 */

	UINT_8 ucBssIndex;	/* BSS_INFO_I index */

	UINT_8 ucStaState;	/* STATE_1,2,3 */

	UINT_8 ucPhyTypeSet;	/* Available PHY Type Set of this peer
				 * (may deduced from received BSS_DESC_T)
				 */
	UINT_8 ucDesiredPhyTypeSet;	/* The match result by AND operation of peer's
					 * PhyTypeSet and ours.
					 */
	BOOLEAN fgHasBasicPhyType;	/* A flag to indicate a Basic Phy Type which
					 * is used to generate some Phy Attribute IE
					 * (e.g. capability, MIB) during association.
					 */
	UINT_8 ucNonHTBasicPhyType;	/* The Basic Phy Type chosen among the
					 * ucDesiredPhyTypeSet.
					 */

	UINT_16 u2HwDefaultFixedRateCode;

	UINT_16 u2CapInfo;	/* For Infra Mode, to store Capability Info. from Association Resp(SAA).
				 * For AP Mode, to store Capability Info. from Association Req(AAA).
				 */
	UINT_16 u2AssocId;	/* For Infra Mode, to store AID from Association Resp(SAA).
				 * For AP Mode, to store the Assigned AID(AAA).
				 */

	UINT_16 u2ListenInterval;	/* Listen Interval from STA(AAA) */

	UINT_16 u2DesiredNonHTRateSet;	/* Our Current Desired Rate Set after
					 * match with STA's Operational Rate Set
					 */

	UINT_16 u2OperationalRateSet;	/* Operational Rate Set of peer BSS */
	UINT_16 u2BSSBasicRateSet;	/* Basic Rate Set of peer BSS */

	BOOLEAN fgIsMerging;	/* For IBSS Mode, to indicate that Merge is ongoing */

	BOOLEAN fgDiagnoseConnection;	/*
					 * For Infra/AP Mode, to diagnose the Connection with
					 * this peer by sending ProbeReq/Null frame
					 */

	/*------------------------------------------------------------------------------------------*/
	/* 802.11n HT capabilities when (prStaRec->ucPhyTypeSet & PHY_TYPE_BIT_HT) is true          */
	/* They have the same definition with fields of information element                         */
	/*------------------------------------------------------------------------------------------*/
	UINT_8 ucMcsSet;	/* MCS0~7 rate set of peer BSS */
	BOOLEAN fgSupMcs32;	/* MCS32 is supported by peer BSS */
	UINT_8 aucRxMcsBitmask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	UINT_16 u2RxHighestSupportedRate;
	UINT_32 u4TxRateInfo;
	UINT_16 u2HtCapInfo;	/* HT cap info field by HT cap IE */
	UINT_8 ucAmpduParam;	/* Field A-MPDU Parameters in HT cap IE */
	UINT_16 u2HtExtendedCap;	/* HT extended cap field by HT cap IE */
	UINT_32 u4TxBeamformingCap;	/* TX beamforming cap field by HT cap IE */
	UINT_8 ucAselCap;	/* ASEL cap field by HT cap IE */
#if 1				/* CFG_SUPPORT_802_11AC */
	/*------------------------------------------------------------------------------------------*/
	/* 802.11ac  VHT capabilities when (prStaRec->ucPhyTypeSet & PHY_TYPE_BIT_VHT) is true          */
	/* They have the same definition with fields of information element                         */
	/*------------------------------------------------------------------------------------------*/
	UINT_32 u4VhtCapInfo;
	UINT_16 u2VhtRxMcsMap;
	UINT_16 u2VhtRxHighestSupportedDataRate;
	UINT_16 u2VhtTxMcsMap;
	UINT_16 u2VhtTxHighestSupportedDataRate;
#endif
	UINT_8 ucRCPI;		/* RCPI of peer */

	UINT_8 ucDTIMPeriod;	/* Target BSS's DTIM Period, we use this
				 * value for setup Listen Interval
				 * TODO(Kevin): TBD
				 */
	UINT_8 ucAuthAlgNum;	/* For Infra/AP Mode, the Auth Algorithm Num used in Authentication(SAA/AAA) */
	UINT_8 ucAuthTranNum;   /* For Infra/AP Mode, the Auth Transaction Number */
	BOOLEAN fgIsReAssoc;	/* For Infra/AP Mode, to indicate ReAssoc Frame was in used(SAA/AAA) */

	UINT_8 ucTxAuthAssocRetryCount;	/* For Infra Mode, the Retry Count of TX Auth/Assod Frame(SAA) */
	UINT_8 ucTxAuthAssocRetryLimit;	/* For Infra Mode, the Retry Limit of TX Auth/Assod Frame(SAA) */

	UINT_16 u2StatusCode;	/* Status of Auth/Assoc Req */
	UINT_16 u2ReasonCode;	/* Reason that been Deauth/Disassoc */

	P_IE_CHALLENGE_TEXT_T prChallengeText;	/*
						 * Point to an allocated buffer for storing Challenge
						 * Text for Shared Key Authentication
						 */

	TIMER_T rTxReqDoneOrRxRespTimer;	/*
						 * For Infra Mode, a timer used to send a timeout event
						 * while waiting for TX request done or RX response.
						 */

	/*------------------------------------------------------------------------------------------*/
	/* Power Management related fields  (for STA/ AP/ P2P/ BOW power saving mode)           */
	/*------------------------------------------------------------------------------------------*/
	BOOLEAN fgSetPwrMgtBit;	/*
				 * For Infra Mode, to indicate that outgoing frame need toggle
				 * the Pwr Mgt Bit in its Frame Control Field.
				 */

	BOOLEAN fgIsInPS;	/*
				 * For AP Mode, to indicate the client PS state(PM).
				 * TRUE: In PS Mode; FALSE: In Active Mode.
				 */

	BOOLEAN fgIsInPsPollSP;	/*
				 * For Infra Mode, to indicate we've sent a PS POLL to AP and start
				 * the PS_POLL Service Period(LP)
				 */

	BOOLEAN fgIsInTriggerSP;	/*
					 * For Infra Mode, to indicate we've sent a Trigger Frame to AP and start
					 * the Delivery Service Period(LP)
					 */

	UINT_8 ucBmpDeliveryAC;	/* 0: AC0, 1: AC1, 2: AC2, 3: AC3 */

	UINT_8 ucBmpTriggerAC;	/* 0: AC0, 1: AC1, 2: AC2, 3: AC3 */

	UINT_8 ucUapsdSp;	/* Max SP length */

	/*------------------------------------------------------------------------------------------*/

	BOOLEAN fgIsRtsEnabled;

	OS_SYSTIME rUpdateTime;	/* (4) System Timestamp of Successful TX and RX  */

	OS_SYSTIME rLastJoinTime;	/* (4) System Timestamp of latest JOIN process */

	UINT_8 ucJoinFailureCount;	/* Retry Count of JOIN process */

	LINK_T arStaWaitQueue[STA_WAIT_QUEUE_NUM];	/* For TXM to defer pkt forwarding to MAC TX DMA */

	UINT_16 au2CachedSeqCtrl[TID_NUM + 1];		/*
							 * Duplicate removal for HT STA on a
							 * per-TID basis ("+1" is for MMPDU and non-QoS)
							 */

	BOOLEAN afgIsIgnoreAmsduDuplicate[TID_NUM + 1];

#if 0
	/* RXM */
	P_RX_BA_ENTRY_T aprRxBaTable[TID_NUM];

	/* TXM */
	P_TX_BA_ENTRY_T aprTxBaTable[TID_NUM];
#endif

	FRAG_INFO_T rFragInfo[MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS];

	/* SEC_INFO_T              rSecInfo; *//* The security state machine */

#if CFG_SUPPORT_ADHOC
	BOOLEAN fgAdhocRsnBcKeyExist[2];	/*
						 * Ad-hoc RSN Rx BC key exist flag,
						 * only reserved two entry for each peer
						 */
	UINT_8 ucAdhocRsnBcWlanIndex[2];	/* Ad-hoc RSN Rx BC wlan index */
#endif

	BOOLEAN fgPortBlock;	/* The 802.1x Port Control flag */

	BOOLEAN fgTransmitKeyExist;	/* Unicast key exist for this STA */

	UINT_8 ucCurrentGtkId;	/* The Current Group key ID */

	BOOLEAN fgTxAmpduEn;	/* Enable TX AMPDU for this Peer */
	BOOLEAN fgRxAmpduEn;	/* Enable RX AMPDU for this Peer */

	PUINT_8 pucAssocReqIe;
	UINT_16 u2AssocReqIeLen;

	WIFI_WMM_AC_STAT_T arLinkStatistics[WMM_AC_INDEX_NUM];	/*link layer satatistics */

	/*------------------------------------------------------------------------------------------*/
	/* WMM/QoS related fields                                                                   */
	/*------------------------------------------------------------------------------------------*/
	BOOLEAN fgIsQoS;	/* If the STA is associated as a QSTA or QAP (for TX/RX) */
	BOOLEAN fgIsWmmSupported;	/* If the peer supports WMM, set to TRUE (for association) */
	BOOLEAN fgIsUapsdSupported;	/* Set according to the scan result (for association) */

	BOOLEAN afgAcmRequired[ACI_NUM];
#if DSCP_SUPPORT
	UINT_8	qosMapSet[64];
#endif
	/*------------------------------------------------------------------------------------------*/
	/* P2P related fields                                                                       */
	/*------------------------------------------------------------------------------------------*/
#if CFG_ENABLE_WIFI_DIRECT
	UINT_8 u2DevNameLen;
	UINT_8 aucDevName[WPS_ATTRI_MAX_LEN_DEVICE_NAME];

	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */

	UINT_16 u2ConfigMethods;

	UINT_8 ucDeviceCap;

	UINT_8 ucSecondaryDevTypeCount;

	DEVICE_TYPE_T rPrimaryDevTypeBE;

	DEVICE_TYPE_T arSecondaryDevTypeBE[P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT];
#endif				/* CFG_SUPPORT_P2P */

	/*------------------------------------------------------------------------------------------*/
	/* QM related fields                                                                       */
	/*------------------------------------------------------------------------------------------*/

	UINT_8 ucFreeQuota;	/*
				 * Per Sta flow controal. Valid when fgIsInPS is TRUE.
				 * Change it for per Queue flow control
				 */
	/* UINT_8                  aucFreeQuotaPerQueue[NUM_OF_PER_STA_TX_QUEUES]; *//* used in future */
	UINT_8 ucFreeQuotaForDelivery;
	UINT_8 ucFreeQuotaForNonDelivery;

	/*------------------------------------------------------------------------------------------*/
	/* TXM related fields                                                                       */
	/*------------------------------------------------------------------------------------------*/
	PVOID aprTxDescTemplate[TX_DESC_TID_NUM];

#if CFG_ENABLE_PKT_LIFETIME_PROFILE && CFG_ENABLE_PER_STA_STATISTICS
	UINT_32 u4TotalTxPktsNumber;
	UINT_32 u4TotalTxPktsTime;
	UINT_32 u4TotalTxPktsHifTxTime;

	UINT_32 u4TotalRxPktsNumber;
	UINT_32 u4MaxTxPktsTime;
	UINT_32 u4MaxTxPktsHifTime;

	UINT_32 u4ThresholdCounter;
	UINT_32 u4EnqueueCounter;
	UINT_32 u4DeqeueuCounter;
#endif

#if 1
	/*------------------------------------------------------------------------------------------*/
	/* To be removed, this is to make que_mgt compilation success only                          */
	/*------------------------------------------------------------------------------------------*/
	/* When this STA_REC is in use, set to TRUE. */
	BOOLEAN fgIsValid;

	/* TX key is ready */
	BOOLEAN fgIsTxKeyReady;

	/* When the STA is connected or TX key is ready */
	BOOLEAN fgIsTxAllowed;

	/* Per-STA Queues: [0] AC0, [1] AC1, [2] AC2, [3] AC3, [4] MGMT, [5] non-QoS */
	QUE_T arTxQueue[NUM_OF_PER_STA_TX_QUEUES];

	/* Reorder Parameter reference table */
	P_RX_BA_ENTRY_T aprRxReorderParamRefTbl[CFG_RX_MAX_BA_TID_NUM];
#endif

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
	TIMINGMSMT_PARAM_T rWNMTimingMsmt;
#endif
	UINT_8 ucTrafficDataType;	/* 0: auto 1: data 2: video 3: voice */
	UINT_8 ucTxGfMode;	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	UINT_8 ucTxSgiMode;	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	UINT_8 ucTxStbcMode;	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	UINT_32 u4FixedPhyRate;	/* */
	UINT_16 u2MaxLinkSpeed;	/* unit is 0.5 Mbps */
	UINT_16 u2MinLinkSpeed;
	UINT_32 u4Flags;	/* reserved for MTK Synergies */

#if CFG_SUPPORT_TDLS
	BOOLEAN fgTdlsIsProhibited;	/* TRUE: AP prohibits TDLS links */
	BOOLEAN fgTdlsIsChSwProhibited;	/* TRUE: AP prohibits TDLS chan switch */

	BOOLEAN flgTdlsIsInitiator;	/* TRUE: the peer is the initiator */
	IE_HT_CAP_T rTdlsHtCap;	/* temp to queue HT capability element */
	PARAM_KEY_T rTdlsKeyTemp;	/* temp to queue the key information */
	UINT_8 ucTdlsIndex;
#endif				/* CFG_SUPPORT_TDLS */
	UINT_16 u2MaxIdlePeriod;
	UINT_8 ucIdleOption;
	BOOLEAN fgSupportBTM; /* flag to indicate Capbility for Bss Transition Management */
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
	UINT_32 u4RxVector0;
	UINT_32 u4RxVector1;
	UINT_32 u4RxVector2;
	UINT_32 u4RxVector3;
	UINT_32 u4RxVector4;
#endif
#if CFG_SUPPORT_802_11W
	/* AP PMF */
	struct STA_PMF_CFG rPmfCfg;
#endif
};

#if 0
/* use nic_tx.h instead */
/* MSDU_INFO and SW_RFB structure */
typedef struct _MSDU_INFO_T {

	/* 4 ----------------MSDU_INFO and SW_RFB Common Fields------------------ */

	LINK_ENTRY_T rLinkEntry;
	PUINT_8 pucBuffer;	/* Pointer to the associated buffer */

	UINT_8 ucBufferSource;	/* HIF TX0, HIF TX1, MAC RX, or MNG Pool */
	UINT_8 ucNetworkTypeIndex;	/* Network type index that this TX packet is assocaited with */
	UINT_8 ucTC;		/* 0 to 5 (used by HIF TX to increment the corresponding TC counter) */
	UINT_8 ucTID;		/* Traffic Identification */

	BOOLEAN fgIs802_11Frame;	/* Set to TRUE for 802.11 frame */
	UINT_8 ucMacHeaderLength;
	UINT_16 u2PayloadLength;
	PUINT_8 pucMacHeader;	/* 802.11 header  */
	PUINT_8 pucPayload;	/* 802.11 payload */

	OS_SYSTIME rArrivalTime;	/* System Timestamp (4) */
	P_STA_RECORD_T prStaRec;

#if CFG_PROFILE_BUFFER_TRACING
	ENUM_BUFFER_ACTIVITY_TYPE_T eActivity[2];
	UINT_32 rActivityTime[2];
#endif
#if DBG && CFG_BUFFER_FREE_CHK
	BOOLEAN fgBufferInSource;
#endif

	UINT_8 ucControlFlag;	/* For specify some Control Flags, e.g. Basic Rate */

	/* 4 -----------------------Non-Common ------------------------- */
	/* TODO: move flags to ucControlFlag */

	BOOLEAN fgIs1xFrame;	/* Set to TRUE for 802.1x frame */

	/* TXM: For TX Done handling, callback function & parameter (5) */
	BOOLEAN fgIsTxFailed;	/* Set to TRUE if transmission failure */

	PFN_TX_DONE_HANDLER pfTxDoneHandler;

	UINT_64 u8TimeStamp;	/* record the TX timestamp */

	/* TXM: For PS forwarding control (per-STA flow control) */
	UINT_8 ucPsForwardingType;	/* Delivery-enabled, non-delivery-enabled, non-PS */
	UINT_8 ucPsSessionID;	/* The Power Save session id for PS forwarding control */

	/* TXM: For MAC TX DMA operations */
	UINT_8 ucMacTxQueIdx;	/*  MAC TX queue: AC0-AC6, BCM, or BCN */
	BOOLEAN fgNoAck;	/* Set to true if Ack is not required for this packet */
	BOOLEAN fgBIP;		/* Set to true if BIP is used for this packet */
	UINT_8 ucFragTotalCount;
	UINT_8 ucFragFinishedCount;
	UINT_16 u2FragThreshold;	/* Fragmentation threshold without WLAN Header & FCS */
	BOOLEAN fgFixedRate;	/* If a fixed rate is used, set to TRUE. */
	UINT_8 ucFixedRateCode;	/* The rate code copied to MAC TX Desc */
	UINT_8 ucFixedRateRetryLimit;	/* The retry limit when a fixed rate is used */
	BOOLEAN fgIsBmcQueueEnd;	/* Set to true if this packet is the end of BMC */

	/* TXM: For flushing ACL frames */
	UINT_16 u2PalLLH;	/* 802.11 PAL LLH */
	/* UINT_16     u2LLH; */
	UINT_16 u2ACLSeq;	/* u2LLH+u2ACLSeq for AM HCI flush ACL frame */

	/* TXM for retransmitting a flushed packet */
	BOOLEAN fgIsSnAssigned;
	UINT_16 u2SequenceNumber;	/* To remember the Sequence Control field of this MPDU */

} MSDU_INFO_T, *P_MSDU_INFO_T;
#endif

#if 0
/* nic_rx.h */
typedef struct _SW_RFB_T {

	/* 4 ----------------MSDU_INFO and SW_RFB Common Fields------------------ */

	LINK_ENTRY_T rLinkEntry;
	PUINT_8 pucBuffer;	/* Pointer to the associated buffer */

	UINT_8 ucBufferSource;	/* HIF TX0, HIF TX1, MAC RX, or MNG Pool */
	UINT_8 ucNetworkTypeIndex;	/* Network type index that this TX packet is assocaited with */
	UINT_8 ucTC;		/* 0 to 5 (used by HIF TX to increment the corresponding TC counter) */
	UINT_8 ucTID;		/* Traffic Identification */

	BOOLEAN fgIs802_11Frame;	/* Set to TRUE for 802.11 frame */
	UINT_8 ucMacHeaderLength;
	UINT_16 u2PayloadLength;
	PUINT_8 pucMacHeader;	/* 802.11 header  */
	PUINT_8 pucPayload;	/* 802.11 payload */

	OS_SYSTIME rArrivalTime;	/* System Timestamp (4) */
	P_STA_RECORD_T prStaRec;

#if CFG_PROFILE_BUFFER_TRACING
	ENUM_BUFFER_ACTIVITY_TYPE_T eActivity[2];
	UINT_32 rActivityTime[2];
#endif
#if DBG && CFG_BUFFER_FREE_CHK
	BOOLEAN fgBufferInSource;
#endif

	UINT_8 ucControlFlag;	/* For specify some Control Flags, e.g. Basic Rate */

	/* 4 -----------------------Non-Common ------------------------- */

	/* For composing the HIF RX Header (TODO: move flags to ucControlFlag) */
	PUINT_8 pucHifRxPacket;	/* Pointer to the Response packet to HIF RX0 or RX1 */
	UINT_16 u2HifRxPacketLength;
	UINT_8 ucHeaderOffset;
	UINT_8 ucHifRxPortIndex;

	UINT_16 u2SequenceControl;
	BOOLEAN fgIsA4Frame;	/* (For MAC RX packet parsing) set to TRUE if 4 addresses are present */
	BOOLEAN fgIsBAR;
	BOOLEAN fgIsQoSData;
	BOOLEAN fgIsAmsduSubframe;	/* Set to TRUE for A-MSDU Subframe */

	/* For HIF RX DMA Desc */
	BOOLEAN fgTUChecksumCheckRequired;
	BOOLEAN fgIPChecksumCheckRequired;
	UINT_8 ucEtherTypeOffset;

} SW_RFB_T, *P_SW_RFB_T;
#endif

typedef enum _ENUM_STA_REC_CMD_ACTION_T {
	STA_REC_CMD_ACTION_STA = 0,
	STA_REC_CMD_ACTION_BSS = 1,
	STA_REC_CMD_ACTION_BSS_EXCLUDE_STA = 2
} ENUM_STA_REC_CMD_ACTION_T, *P_ENUM_STA_REC_CMD_ACTION_T;

#if CFG_SUPPORT_TDLS

/* TDLS FSM */
typedef struct _CMD_PEER_ADD_T {

	UINT_8 aucPeerMac[6];
	ENUM_STA_TYPE_T eStaType;
} CMD_PEER_ADD_T;

typedef struct _CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T {
	UINT_8 arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	UINT_16 u2RxHighest;
	UINT_8 ucTxParams;
	UINT_8 Reserved[3];
} CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T;

typedef struct _CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T {
	UINT_8 arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
} CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T;

typedef struct _CMD_PEER_UPDATE_HT_CAP_T {
	UINT_16 u2CapInfo;
	UINT_8 ucAmpduParamsInfo;

	/* 16 bytes MCS information */
	CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T rMCS;

	UINT_16 u2ExtHtCapInfo;
	UINT_32 u4TxBfCapInfo;
	UINT_8 ucAntennaSelInfo;
} CMD_PEER_UPDATE_HT_CAP_T;

typedef struct _CMD_PEER_UPDATE_VHT_CAP_T {
	UINT_16 u2CapInfo;
	/* 16 bytes MCS information */
	CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T rVMCS;

} CMD_PEER_UPDATE_VHT_CAP_T;

typedef struct _CMD_PEER_UPDATE_T {

	UINT_8 aucPeerMac[6];

#define CMD_PEER_UPDATE_SUP_CHAN_MAX			50
	UINT_8 aucSupChan[CMD_PEER_UPDATE_SUP_CHAN_MAX];

	UINT_16 u2StatusCode;

#define CMD_PEER_UPDATE_SUP_RATE_MAX			50
	UINT_8 aucSupRate[CMD_PEER_UPDATE_SUP_RATE_MAX];
	UINT_16 u2SupRateLen;

	UINT_8 UapsdBitmap;
	UINT_8 UapsdMaxSp;	/* MAX_SP */

	UINT_16 u2Capability;
#define CMD_PEER_UPDATE_EXT_CAP_MAXLEN			5
	UINT_8 aucExtCap[CMD_PEER_UPDATE_EXT_CAP_MAXLEN];
	UINT_16 u2ExtCapLen;

	CMD_PEER_UPDATE_HT_CAP_T rHtCap;
	CMD_PEER_UPDATE_VHT_CAP_T rVHtCap;

	BOOLEAN fgIsSupHt;
	ENUM_STA_TYPE_T eStaType;

	/* TODO */
	/* So far, TDLS only a few of the parameters, the rest will be added in the future requiements */
	/* kernel 3.10 station paramenters */
	/*
	 * struct station_parameters {
	 * const u8 *supported_rates;
	 * struct net_device *vlan;
	 * u32 sta_flags_mask, sta_flags_set;
	 * u32 sta_modify_mask;
	 * int listen_interval;
	 * u16 aid;
	 * u8 supported_rates_len;
	 * u8 plink_action;
	 * u8 plink_state;
	 * const struct ieee80211_ht_cap *ht_capa;
	 * const struct ieee80211_vht_cap *vht_capa;
	 * u8 uapsd_queues;
	 * u8 max_sp;
	 * enum nl80211_mesh_power_mode local_pm;
	 * u16 capability;
	 * const u8 *ext_capab;
	 * u8 ext_capab_len;
	 * const u8 *supported_channels;
	 * u8 supported_channels_len;
	 * const u8 *supported_oper_classes;
	 * u8 supported_oper_classes_len;
	 * };
	 */

} CMD_PEER_UPDATE_T;

#endif

#if CFG_DBG_MGT_BUF
struct MEM_TRACK {
	LINK_ENTRY_T rLinkEntry;
	UINT_16 u2CmdIdAndWhere;
	PUINT_8 pucFileAndLine;
};
#endif
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
#define cnmMgtPktAlloc(_prAdapter, _u4Length) \
	cnmPktAllocWrapper((_prAdapter), (_u4Length), (PUINT_8)__func__)

#define cnmMgtPktFree(_prAdapter, _prMsduInfo) \
	cnmPktFreeWrapper((_prAdapter), (_prMsduInfo), (PUINT_8)__func__)

#define cnmMemAlloc(_prAdapter, eRameType, u4Length) \
	cnmMemAllocX(_prAdapter, eRameType, u4Length, __FILE__ ":" STRLINE(__LINE__))

#define IS_FROM_BUF(_prAdapter, pucInfoBuffer) \
	(((PUINT_8)(pucInfoBuffer) >= (PUINT_8)_prAdapter->rMgtBufInfo.pucBuf) && \
	((PUINT_8)(pucInfoBuffer) < (PUINT_8)_prAdapter->rMgtBufInfo.pucBuf + MGT_BUFFER_SIZE))
#else
#define cnmMgtPktAlloc cnmPktAlloc
#define cnmMgtPktFree cnmPktFree
#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

P_MSDU_INFO_T cnmPktAllocWrapper(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length, IN PUINT_8 pucStr);

VOID cnmPktFreeWrapper(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN PUINT_8 pucStr);

P_MSDU_INFO_T cnmPktAlloc(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Length);

VOID cnmPktFree(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID cnmMemInit(IN P_ADAPTER_T prAdapter);

#if CFG_DBG_MGT_BUF
PVOID cnmMemAllocX(IN P_ADAPTER_T prAdapter, IN ENUM_RAM_TYPE_T eRamType, IN UINT_32 u4Length,
		   PUINT_8 fileAndLine);
#else
PVOID cnmMemAlloc(IN P_ADAPTER_T prAdapter, IN ENUM_RAM_TYPE_T eRamType, IN UINT_32 u4Length);
#endif

VOID cnmMemFree(IN P_ADAPTER_T prAdapter, IN PVOID pvMemory);

VOID cnmStaRecInit(IN P_ADAPTER_T prAdapter);

P_STA_RECORD_T
cnmStaRecAlloc(IN P_ADAPTER_T prAdapter, IN ENUM_STA_TYPE_T eStaType, IN UINT_8 ucBssIndex, IN PUINT_8 pucMacAddr);

VOID cnmStaRecFree(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID cnmStaFreeAllStaByNetwork(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 ucStaRecIndexExcluded);

P_STA_RECORD_T cnmGetStaRecByIndex(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex);

P_STA_RECORD_T cnmGetStaRecByAddress(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 aucPeerMACAddress[]);

VOID cnmStaRecChangeState(IN P_ADAPTER_T prAdapter, IN OUT P_STA_RECORD_T prStaRec, IN UINT_8 ucNewState);

VOID cnmDumpStaRec(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIdx);

VOID cnmDumpMemoryStatus(IN P_ADAPTER_T prAdapter);

#if CFG_SUPPORT_TDLS
WLAN_STATUS			/* TDLS_STATUS */
cnmPeerAdd(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen);

WLAN_STATUS			/* TDLS_STATUS */
cnmPeerUpdate(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen);

P_STA_RECORD_T cnmGetTdlsPeerByAddress(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 aucPeerMACAddress[]);
#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#ifndef _lint
/* Kevin: we don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 * We'll need this for porting driver to different RTOS.
 */
static __KAL_INLINE__ VOID cnmMemDataTypeCheck(VOID)
{
#if 0
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, rLinkEntry) == 0);

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, rLinkEntry) == OFFSET_OF(SW_RFB_T, rLinkEntry));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, pucBuffer) == OFFSET_OF(SW_RFB_T, pucBuffer));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucBufferSource) == OFFSET_OF(SW_RFB_T, ucBufferSource));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, pucMacHeader) == OFFSET_OF(SW_RFB_T, pucMacHeader));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucMacHeaderLength) ==
				     OFFSET_OF(SW_RFB_T, ucMacHeaderLength));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, pucPayload) == OFFSET_OF(SW_RFB_T, pucPayload));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, u2PayloadLength) == OFFSET_OF(SW_RFB_T, u2PayloadLength));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, prStaRec) == OFFSET_OF(SW_RFB_T, prStaRec));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucNetworkTypeIndex) ==
				     OFFSET_OF(SW_RFB_T, ucNetworkTypeIndex));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucTID) == OFFSET_OF(SW_RFB_T, ucTID));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, fgIs802_11Frame) == OFFSET_OF(SW_RFB_T, fgIs802_11Frame));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucControlFlag) == OFFSET_OF(SW_RFB_T, ucControlFlag));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, rArrivalTime) == OFFSET_OF(SW_RFB_T, rArrivalTime));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, ucTC) == OFFSET_OF(SW_RFB_T, ucTC));

#if CFG_PROFILE_BUFFER_TRACING
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, eActivity[0]) == OFFSET_OF(SW_RFB_T, eActivity[0]));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, rActivityTime[0]) == OFFSET_OF(SW_RFB_T,
		rActivityTime[0]));
#endif

#if DBG && CFG_BUFFER_FREE_CHK
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(MSDU_INFO_T, fgBufferInSource) == OFFSET_OF(SW_RFB_T,
		fgBufferInSource));
#endif

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(STA_RECORD_T, rLinkEntry) == 0);

	return;
#endif
}
#endif /* _lint */

#endif /* _CNM_MEM_H */
