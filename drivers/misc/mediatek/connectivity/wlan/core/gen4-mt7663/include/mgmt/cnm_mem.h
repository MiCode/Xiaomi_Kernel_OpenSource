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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3
 *     /include/mgmt/cnm_mem.h#1
 */

/*! \file   "cnm_mem.h"
 *    \brief  In this file we define the structure of the control unit of
 *    packet buffer and MGT/MSG Memory Buffer.
 */


#ifndef _CNM_MEM_H
#define _CNM_MEM_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "qosmap.h"
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#ifndef POWER_OF_2
#define POWER_OF_2(n)				BIT(n)
#endif

/* Size of a basic management buffer block in power of 2 */
/* 7 to the power of 2 = 128 */
#define MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2	7

/* 5 to the power of 2 = 32 */
#define MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2	5

/* Size of a basic management buffer block */
#define MGT_BUF_BLOCK_SIZE	POWER_OF_2(MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2)
#define MSG_BUF_BLOCK_SIZE	POWER_OF_2(MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2)

/* Total size of (n) basic management buffer blocks */
#define MGT_BUF_BLOCKS_SIZE(n) \
	((uint32_t)(n) << MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2)
#define MSG_BUF_BLOCKS_SIZE(n) \
	((uint32_t)(n) << MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2)

/* Number of management buffer block */
#define MAX_NUM_OF_BUF_BLOCKS			32	/* Range: 1~32 */

/* Size of overall management frame buffer */
#define MGT_BUFFER_SIZE		(MAX_NUM_OF_BUF_BLOCKS * MGT_BUF_BLOCK_SIZE)
#define MSG_BUFFER_SIZE		(MAX_NUM_OF_BUF_BLOCKS * MSG_BUF_BLOCK_SIZE)

/* STA_REC related definitions */
#define STA_REC_INDEX_BMCAST		0xFF
#define STA_REC_INDEX_NOT_FOUND		0xFE

/* Number of SW queues in each STA_REC: AC0~AC4 */
#define STA_WAIT_QUEUE_NUM		5

/* Number of SC caches in each STA_REC: AC0~AC4 */
#define SC_CACHE_INDEX_NUM		5

/* P2P related definitions */
#if CFG_ENABLE_WIFI_DIRECT
/* Moved from p2p_fsm.h */
#define WPS_ATTRI_MAX_LEN_DEVICE_NAME		32	/* 0x1011 */

/* NOTE(Kevin): Shall <= 16 */
#define P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT	8
#endif

/* Define the argument of cnmStaFreeAllStaByNetwork when all station records
 * will be free. No one will be free
 */
#define STA_REC_EXCLUDE_NONE		CFG_STA_REC_NUM

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Use 32 bits to represent buffur bitmap in BUF_INFO, i.e., the variable
 * rFreeBlocksBitmap in BUF_INFO structure.
 */
#if (MAX_NUM_OF_BUF_BLOCKS != 32)
#error > #define MAX_NUM_OF_MGT_BUF_BLOCKS should be 32 !
#endif /* MAX_NUM_OF_MGT_BUF_BLOCKS */

/* Control variable of TX management memory pool */
struct BUF_INFO {
	uint8_t *pucBuf;

#if CFG_DBG_MGT_BUF
	uint32_t u4AllocCount;
	uint32_t u4FreeCount;
	uint32_t u4AllocNullCount;
#endif	/* CFG_DBG_MGT_BUF */

	uint32_t rFreeBlocksBitmap;
	uint8_t aucAllocatedBlockNum[MAX_NUM_OF_BUF_BLOCKS];
};

/* Wi-Fi divides RAM into three types
 * MSG:     Mailbox message (Small size)
 * BUF:     HW DMA buffers (HIF/MAC)
 */
enum ENUM_RAM_TYPE {
	RAM_TYPE_MSG = 0,
	RAM_TYPE_BUF
};

enum ENUM_BUFFER_SOURCE {
	BUFFER_SOURCE_HIF_TX0 = 0,
	BUFFER_SOURCE_HIF_TX1,
	BUFFER_SOURCE_MAC_RX,
	BUFFER_SOURCE_MNG,
	BUFFER_SOURCE_BCN,
	BUFFER_SOURCE_NUM
};

enum ENUM_SEC_STATE {
	SEC_STATE_INIT,
	SEC_STATE_INITIATOR_PORT_BLOCKED,
	SEC_STATE_RESPONDER_PORT_BLOCKED,
	SEC_STATE_CHECK_OK,
	SEC_STATE_SEND_EAPOL,
	SEC_STATE_SEND_DEAUTH,
	SEC_STATE_COUNTERMEASURE,
	SEC_STATE_NUM
};

struct TSPEC_ENTRY {
	uint8_t ucStatus;
	uint8_t ucToken;	/* Dialog Token in ADDTS_REQ or ADDTS_RSP */
	uint16_t u2MediumTime;
	uint32_t u4TsInfo;
	/* PARAM_QOS_TS_INFO rParamTsInfo; */
	/* Add other retained QoS parameters below */
};

#if 0
struct SEC_INFO {

	enum ENUM_SEC_STATE ePreviousState;
	enum ENUM_SEC_STATE eCurrentState;

	u_int8_t fg2nd1xSend;
	u_int8_t fgKeyStored;

	uint8_t aucStoredKey[64];

	u_int8_t fgAllowOnly1x;
};
#endif

#define MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS	3

#define UPDATE_BSS_RSSI_INTERVAL_SEC		3	/* Seconds */

/* Fragment information structure */
struct FRAG_INFO {
	uint16_t u2NextFragSeqCtrl;
	uint8_t *pucNextFragStart;
	struct SW_RFB *pr1stFrag;

	/* The receive time of 1st fragment */
	OS_SYSTIME rReceiveLifetimeLimit;
};

#if CFG_SUPPORT_802_11W
/* AP PMF */
struct AP_PMF_CFG {
	u_int8_t fgMfpc;
	u_int8_t fgMfpr;
	u_int8_t fgSha256;
	u_int8_t fgAPApplyPmfReq;
	u_int8_t fgBipKeyInstalled;
};

struct STA_PMF_CFG {
	u_int8_t fgMfpc;
	u_int8_t fgMfpr;
	u_int8_t fgSha256;
	u_int8_t fgApplyPmf;
	u_int8_t fgBipKeyInstalled;

	/* for certification 4.3.3.1, 4.3.3.2 TX unprotected deauth */
	u_int8_t fgRxDeauthResp;

	/* For PMF SA query TX request retry a timer */
	/* record the start time of 1st SAQ request */
	uint32_t u4SAQueryStart;

	uint32_t u4SAQueryCount;
	uint8_t ucSAQueryTimedOut;	/* retry more than 1000ms */
	struct TIMER rSAQueryTimer;
	uint16_t u2TransactionID;
};
#endif

#if DSCP_SUPPORT
struct _DSCP_EXCEPTION {
	uint8_t dscp;
	uint8_t userPriority;
};

struct _DSCP_RANGE {
	uint8_t lDscp;
	uint8_t hDscp;
};

struct _QOS_MAP_SET {
	struct _DSCP_RANGE dscpRange[8];
	uint8_t dscpExceptionNum;
	struct _DSCP_EXCEPTION dscpException[1];
};
#endif

/* Define STA record structure */
struct STA_RECORD {
	struct LINK_ENTRY rLinkEntry;
	uint8_t ucIndex;	/* Not modify it except initializing */
	uint8_t ucWlanIndex;	/* WLAN table index */

#if 0 /* TODO: Remove this */
	/* The BSS STA Rx WLAN index, IBSS Rx BC WLAN table
	 * index, work at IBSS Open and WEP
	 */
	uint8_t ucBMCWlanIndex;
#endif

	u_int8_t fgIsInUse;	/* Indicate if this entry is in use or not */
	uint8_t aucMacAddr[MAC_ADDR_LEN];	/* MAC address */

	/* SAA/AAA */
	/* Store STATE Value used in SAA/AAA */
	enum ENUM_AA_STATE eAuthAssocState;

	uint8_t ucAuthAssocReqSeqNum;

	/* Indicate the role of this STA in the network (for example, P2P GO) */
	enum ENUM_STA_TYPE eStaType;

	uint8_t ucBssIndex;	/* BSS_INFO_I index */

	uint8_t ucStaState;	/* STATE_1,2,3 */

	/* Available PHY Type Set of this peer (may deduced from
	 * received struct BSS_DESC)
	 */
	uint8_t ucPhyTypeSet;

	/* record from bcn or probe response */
	uint8_t ucVhtCapNumSoundingDimensions;

	/* The match result by AND operation of peer's PhyTypeSet and ours. */
	uint8_t ucDesiredPhyTypeSet;

	/* A flag to indicate a Basic Phy Type which is used to generate some
	 * Phy Attribute IE (e.g. capability, MIB) during association.
	 */
	u_int8_t fgHasBasicPhyType;

	/* The Basic Phy Type chosen among the ucDesiredPhyTypeSet. */
	uint8_t ucNonHTBasicPhyType;

	uint16_t u2HwDefaultFixedRateCode;

	/* For Infra Mode, to store Capability Info. from Association Resp(SAA).
	 * For AP Mode, to store Capability Info. from Association Req(AAA).
	 */
	uint16_t u2CapInfo;

	/* For Infra Mode, to store AID from Association Resp(SAA).
	 * For AP Mode, to store the Assigned AID(AAA).
	 */
	uint16_t u2AssocId;

	uint16_t u2ListenInterval;	/* Listen Interval from STA(AAA) */

	/* Our Current Desired Rate Set after match
	 * with STA's Operational Rate Set
	 */
	uint16_t u2DesiredNonHTRateSet;

	uint16_t u2OperationalRateSet;	/* Operational Rate Set of peer BSS */
	uint16_t u2BSSBasicRateSet;	/* Basic Rate Set of peer BSS */

	/* For IBSS Mode, to indicate that Merge is ongoing */
	u_int8_t fgIsMerging;

	/* For Infra/AP Mode, to diagnose the Connection with this peer
	 * by sending ProbeReq/Null frame
	 */
	u_int8_t fgDiagnoseConnection;

	/*----------------------------------------------------------------------
	 * 802.11n HT capabilities when (prStaRec->ucPhyTypeSet &
	 * PHY_TYPE_BIT_HT) is true. They have the same definition with fields
	 * of information element
	 *----------------------------------------------------------------------
	 */
	uint8_t ucMcsSet;	/* MCS0~7 rate set of peer BSS */
	u_int8_t fgSupMcs32;	/* MCS32 is supported by peer BSS */
	uint8_t aucRxMcsBitmask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	uint16_t u2RxHighestSupportedRate;
	uint32_t u4TxRateInfo;
	uint16_t u2HtCapInfo;	/* HT cap info field by HT cap IE */
	uint8_t ucAmpduParam;	/* Field A-MPDU Parameters in HT cap IE */
	uint16_t u2HtExtendedCap;	/* HT extended cap field by HT cap IE */

	/* TX beamforming cap field by HT cap IE */
	uint32_t u4TxBeamformingCap;

	uint8_t ucAselCap;	/* ASEL cap field by HT cap IE */

#if 1	/* CFG_SUPPORT_802_11AC */
	/*----------------------------------------------------------------------
	 * 802.11ac  VHT capabilities when (prStaRec->ucPhyTypeSet &
	 * PHY_TYPE_BIT_VHT) is true. They have the same definition with fields
	 * of information element
	 *----------------------------------------------------------------------
	 */
	uint32_t u4VhtCapInfo;
	uint16_t u2VhtRxMcsMap;
	uint16_t u2VhtRxHighestSupportedDataRate;
	uint16_t u2VhtTxMcsMap;
	uint16_t u2VhtTxHighestSupportedDataRate;
	uint8_t ucVhtOpMode;
#endif
	/*----------------------------------------------------------------------
	 * 802.11ac  HT operation info when (prStaRec->ucPhyTypeSet &
	 * PHY_TYPE_BIT_HT) is true. They have the same definition with fields
	 * of information element
	 *----------------------------------------------------------------------
	 */
	uint8_t ucHtPeerOpInfo1; /* Backup peer HT OP Info */

	/*----------------------------------------------------------------------
	 * 802.11ac  VHT operation info when (prStaRec->ucPhyTypeSet &
	 * PHY_TYPE_BIT_VHT) is true. They have the same definition with fields
	 * of information element
	 *----------------------------------------------------------------------
	 */
	/* Backup peer VHT Op Info */
	uint8_t ucVhtOpChannelWidth;
	uint8_t ucVhtOpChannelFrequencyS1;
	uint8_t ucVhtOpChannelFrequencyS2;


	uint8_t ucRCPI;		/* RCPI of peer */

	/* Target BSS's DTIM Period, we use this value for
	 * setup Listen Interval
	 * TODO(Kevin): TBD
	 */
	uint8_t ucDTIMPeriod;

	/* For Infra/AP Mode, the Auth Algorithm Num used
	 * in Authentication(SAA/AAA)
	 */
	uint8_t ucAuthAlgNum;
	uint8_t ucAuthTranNum; /* For Infra/AP Mode, the Auth Transaction Number
				  */

	/* For Infra/AP Mode, to indicate ReAssoc Frame was in used(SAA/AAA) */
	u_int8_t fgIsReAssoc;

	/* For Infra Mode, the Retry Count of TX Auth/Assod Frame(SAA) */
	uint8_t ucTxAuthAssocRetryCount;

	/* For Infra Mode, the Retry Limit of TX Auth/Assod Frame(SAA) */
	uint8_t ucTxAuthAssocRetryLimit;

#if CFG_SUPPORT_CFG80211_AUTH
	/* Record what we sent for
	 * retry TX Auth/Assoc without SAA FSM
	 */
	enum ENUM_AA_SENT_T eAuthAssocSent;
#endif

	uint16_t u2StatusCode;	/* Status of Auth/Assoc Req */
	uint16_t u2ReasonCode;	/* Reason that been Deauth/Disassoc */

	/* Point to an allocated buffer for storing Challenge */
	/* Text for Shared Key Authentication */
	struct IE_CHALLENGE_TEXT *prChallengeText;

	/* For Infra Mode, a timer used to send a timeout event
	 * while waiting for TX request done or RX response.
	 */
	struct TIMER rTxReqDoneOrRxRespTimer;

	/* For Infra Mode, a timer used to avoid the Deauth frame
	 * not be sent
	 */
	struct TIMER rDeauthTxDoneTimer;

	/*----------------------------------------------------------------------
	 * Power Management related fields (for STA/ AP/ P2P/ BOW power saving
	 * mode)
	 *----------------------------------------------------------------------
	 */
	/* For Infra Mode, to indicate that outgoing frame need
	 * toggle the Pwr Mgt Bit in its Frame Control Field.
	 */
	u_int8_t fgSetPwrMgtBit;

	/* For AP Mode, to indicate the client PS state(PM).
	 * TRUE: In PS Mode; FALSE: In Active Mode.
	 */
	u_int8_t fgIsInPS;

	/* For Infra Mode, to indicate we've sent a PS POLL to AP
	 * and start the PS_POLL Service Period(LP)
	 */
	u_int8_t fgIsInPsPollSP;

	/* For Infra Mode, to indicate we've sent a Trigger Frame
	 * to AP and start the Delivery Service Period(LP)
	 */
	u_int8_t fgIsInTriggerSP;

	uint8_t ucBmpDeliveryAC;	/* 0: AC0, 1: AC1, 2: AC2, 3: AC3 */

	uint8_t ucBmpTriggerAC;		/* 0: AC0, 1: AC1, 2: AC2, 3: AC3 */

	uint8_t ucUapsdSp;		/* Max SP length */

	/*--------------------------------------------------------------------*/

	u_int8_t fgIsRtsEnabled;

	/* (4) System Timestamp of Successful TX and RX */
	uint32_t rUpdateTime;

	/* (4) System Timestamp of latest JOIN process */
	uint32_t rLastJoinTime;

	uint8_t ucJoinFailureCount;	/* Retry Count of JOIN process */

	/* For TXM to defer pkt forwarding to MAC TX DMA */
	struct LINK arStaWaitQueue[STA_WAIT_QUEUE_NUM];

	/* Duplicate removal for HT STA on a
	 * per-TID basis ("+1" is for MMPDU and non-QoS)
	 */
	uint16_t au2CachedSeqCtrl[TID_NUM + 1];

	u_int8_t afgIsIgnoreAmsduDuplicate[TID_NUM + 1];

#if 0
	/* RXM */
	struct RX_BA_ENTRY *aprRxBaTable[TID_NUM];

	/* TXM */
	P_TX_BA_ENTRY_T aprTxBaTable[TID_NUM];
#endif

	struct FRAG_INFO rFragInfo[MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS];

#if 0 /* TODO: Remove this */
	struct SEC_INFO rSecInfo; /* The security state machine */
#endif

#if CFG_SUPPORT_ADHOC
	/* Ad-hoc RSN Rx BC key exist flag, only reserved two
	 * entry for each peer
	 */
	u_int8_t fgAdhocRsnBcKeyExist[2];

	/* Ad-hoc RSN Rx BC wlan index */
	uint8_t ucAdhocRsnBcWlanIndex[2];
#endif

	u_int8_t fgPortBlock;	/* The 802.1x Port Control flag */

	u_int8_t fgTransmitKeyExist;	/* Unicast key exist for this STA */

	u_int8_t fgTxAmpduEn;	/* Enable TX AMPDU for this Peer */
	u_int8_t fgRxAmpduEn;	/* Enable RX AMPDU for this Peer */

	uint8_t *pucAssocReqIe;
	uint16_t u2AssocReqIeLen;

	/* link layer satatistics */
	struct WIFI_WMM_AC_STAT arLinkStatistics[WMM_AC_INDEX_NUM];

	/*----------------------------------------------------------------------
	 * WMM/QoS related fields
	 *----------------------------------------------------------------------
	 */
	/* If the STA is associated as a QSTA or QAP (for TX/RX) */
	u_int8_t fgIsQoS;

	/* If the peer supports WMM, set to TRUE (for association) */
	u_int8_t fgIsWmmSupported;

	/* Set according to the scan result (for association) */
	u_int8_t fgIsUapsdSupported;

	u_int8_t afgAcmRequired[ACI_NUM];

	/*----------------------------------------------------------------------
	 * P2P related fields
	 *----------------------------------------------------------------------
	 */
#if CFG_ENABLE_WIFI_DIRECT
	uint8_t u2DevNameLen;
	uint8_t aucDevName[WPS_ATTRI_MAX_LEN_DEVICE_NAME];

	uint8_t aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */

	uint16_t u2ConfigMethods;

	uint8_t ucDeviceCap;

	uint8_t ucSecondaryDevTypeCount;

	struct DEVICE_TYPE rPrimaryDevTypeBE;

	struct DEVICE_TYPE arSecondaryDevTypeBE[
		P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT];
#endif	/* CFG_SUPPORT_P2P */

	/*----------------------------------------------------------------------
	 * QM related fields
	 *----------------------------------------------------------------------
	 */
	/* Per Sta flow controal. Valid when fgIsInPS is TRUE.
	 * Change it for per Queue flow control
	 */
	uint8_t ucFreeQuota;

#if 0 /* TODO: Remove this */
	/* used in future */
	uint8_t aucFreeQuotaPerQueue[NUM_OF_PER_STA_TX_QUEUES];
#endif
	uint8_t ucFreeQuotaForDelivery;
	uint8_t ucFreeQuotaForNonDelivery;

	/*----------------------------------------------------------------------
	 * TXM related fields
	 *----------------------------------------------------------------------
	 */
	void *aprTxDescTemplate[TX_DESC_TID_NUM];

#if CFG_ENABLE_PKT_LIFETIME_PROFILE && CFG_ENABLE_PER_STA_STATISTICS
	uint32_t u4TotalTxPktsNumber;
	uint32_t u4TotalTxPktsTime;
	uint32_t u4TotalTxPktsHifTxTime;

	uint32_t u4TotalRxPktsNumber;
	uint32_t u4MaxTxPktsTime;
	uint32_t u4MaxTxPktsHifTime;

	uint32_t u4ThresholdCounter;
	uint32_t u4EnqueueCounter;
	uint32_t u4DeqeueuCounter;
#endif

#if 1
	/*----------------------------------------------------------------------
	 * To be removed, this is to make que_mgt compilation success only
	 *----------------------------------------------------------------------
	 */
	/* When this STA_REC is in use, set to TRUE. */
	u_int8_t fgIsValid;

	/* TX key is ready */
	u_int8_t fgIsTxKeyReady;

	/* When the STA is connected or TX key is ready */
	u_int8_t fgIsTxAllowed;

	/* Per-STA Queues: [0] AC0, [1] AC1, [2] AC2, [3] AC3 */
	struct QUE arTxQueue[NUM_OF_PER_STA_TX_QUEUES];

	/* Per-STA Pending Queues: [0] AC0, [1] AC1, [2] AC2, [3] AC3 */
	/* This queue is for Tx packet in protected BSS before key is set */
	struct QUE arPendingTxQueue[NUM_OF_PER_STA_TX_QUEUES];

	/* Tx packet target queue pointer. Select between arTxQueue &
	 * arPendingTxQueue
	 */
	struct QUE *aprTargetQueue[NUM_OF_PER_STA_TX_QUEUES];

	/* Reorder Parameter reference table */
	struct RX_BA_ENTRY *aprRxReorderParamRefTbl[CFG_RX_MAX_BA_TID_NUM];
#endif

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
	struct TIMINGMSMT_PARAM rWNMTimingMsmt;
#endif
	uint8_t ucTrafficDataType;	/* 0: auto 1: data 2: video 3: voice */

	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	uint8_t ucTxGfMode;

	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	uint8_t ucTxSgiMode;

	/* 0: auto 1:Force enable 2: Force disable 3: enable by peer */
	uint8_t ucTxStbcMode;

	uint32_t u4FixedPhyRate;
	uint16_t u2MaxLinkSpeed;	/* unit is 0.5 Mbps */
	uint16_t u2MinLinkSpeed;
	uint32_t u4Flags;	/* reserved for MTK Synergies */

#if CFG_SUPPORT_TDLS
	u_int8_t fgTdlsIsProhibited;	/* TRUE: AP prohibits TDLS links */

	/* TRUE: AP prohibits TDLS chan switch */
	u_int8_t fgTdlsIsChSwProhibited;

	u_int8_t flgTdlsIsInitiator;	/* TRUE: the peer is the initiator */

	/* temp to queue HT capability element */
	struct IE_HT_CAP rTdlsHtCap;

	struct PARAM_KEY rTdlsKeyTemp;	/* temp to queue the key information */
	uint8_t ucTdlsIndex;
#endif	/* CFG_SUPPORT_TDLS */
#if CFG_SUPPORT_TX_BF
	struct TXBF_PFMU_STA_INFO rTxBfPfmuStaInfo;
#endif
#if CFG_SUPPORT_MSP
	uint32_t u4RxVector0;
	uint32_t u4RxVector1;
	uint32_t u4RxVector2;
	uint32_t u4RxVector3;
	uint32_t u4RxVector4;
#endif
	uint8_t ucSmDialogToken;	/* Spectrum Mngt Dialog Token */
	uint8_t ucSmMsmtRequestMode;	/* Measurement Request Mode */
	uint8_t ucSmMsmtToken;		/* Measurement Request Token */

	/* Element for AMSDU */
	uint8_t ucAmsduEnBitmap;	/* Tid bit mask of AMSDU enable */
	uint8_t ucMaxMpduCount;
	uint32_t u4MaxMpduLen;
	uint32_t u4MinMpduLen;
#if CFG_SUPPORT_802_11W
	/* AP PMF */
	struct STA_PMF_CFG rPmfCfg;
#endif
#if DSCP_SUPPORT
	struct _QOS_MAP_SET *qosMapSet;
#endif
	u_int8_t fgSupportBTM; /* Indicates whether to support BTM */
	uint8_t eapol_re_enqueue_cnt;

#if CFG_SUPPORT_GET_MCS_INFO
	uint32_t au4RxV0[MCS_INFO_SAMPLE_CNT];
	uint32_t au4RxV1[MCS_INFO_SAMPLE_CNT];
#endif

};

#if 0
/* use nic_tx.h instead */
/* MSDU_INFO and SW_RFB structure */
struct MSDU_INFO {

	/* 4 ----------------MSDU_INFO and SW_RFB Common Fields-------------- */

	struct LINK_ENTRY rLinkEntry;
	uint8_t *pucBuffer;	/* Pointer to the associated buffer */

	uint8_t ucBufferSource;	/* HIF TX0, HIF TX1, MAC RX, or MNG Pool */

	/* Network type index that this TX packet is assocaited with */
	uint8_t ucNetworkTypeIndex;

	/* 0 to 5 (used by HIF TX to increment the corresponding TC counter) */
	uint8_t ucTC;

	uint8_t ucTID;		/* Traffic Identification */

	u_int8_t fgIs802_11Frame;	/* Set to TRUE for 802.11 frame */
	uint8_t ucMacHeaderLength;
	uint16_t u2PayloadLength;
	uint8_t *pucMacHeader;	/* 802.11 header  */
	uint8_t *pucPayload;	/* 802.11 payload */

	OS_SYSTIME rArrivalTime;	/* System Timestamp (4) */
	struct STA_RECORD *prStaRec;

#if CFG_PROFILE_BUFFER_TRACING
	ENUM_BUFFER_ACTIVITY_TYPE_T eActivity[2];
	uint32_t rActivityTime[2];
#endif
#if DBG && CFG_BUFFER_FREE_CHK
	u_int8_t fgBufferInSource;
#endif

	/* For specify some Control Flags, e.g. Basic Rate */
	uint8_t ucControlFlag;

	/* 4 -----------------------Non-Common ------------------------- */
	/* TODO: move flags to ucControlFlag */

	u_int8_t fgIs1xFrame;	/* Set to TRUE for 802.1x frame */

	/* TXM: For TX Done handling, callback function & parameter (5) */
	u_int8_t fgIsTxFailed;	/* Set to TRUE if transmission failure */

	PFN_TX_DONE_HANDLER pfTxDoneHandler;

	uint64_t u8TimeStamp;	/* record the TX timestamp */

	/* TXM: For PS forwarding control (per-STA flow control) */
	/* Delivery-enabled, non-delivery-enabled, non-PS */
	uint8_t ucPsForwardingType;

	/* The Power Save session id for PS forwarding control */
	uint8_t ucPsSessionID;

	/* TXM: For MAC TX DMA operations */
	uint8_t ucMacTxQueIdx;	/*  MAC TX queue: AC0-AC6, BCM, or BCN */

	/* Set to true if Ack is not required for this packet */
	u_int8_t fgNoAck;

	u_int8_t fgBIP;		/* Set to true if BIP is used for this packet */
	uint8_t ucFragTotalCount;
	uint8_t ucFragFinishedCount;

	/* Fragmentation threshold without WLAN Header & FCS */
	uint16_t u2FragThreshold;

	u_int8_t fgFixedRate;	/* If a fixed rate is used, set to TRUE. */

	/* The rate code copied to MAC TX Desc */
	uint8_t ucFixedRateCode;

	/* The retry limit when a fixed rate is used */
	uint8_t ucFixedRateRetryLimit;

	/* Set to true if this packet is the end of BMC */
	u_int8_t fgIsBmcQueueEnd;

	/* TXM: For flushing ACL frames */
	uint16_t u2PalLLH;	/* 802.11 PAL LLH */
	/* UINT_16     u2LLH; */
	uint16_t u2ACLSeq;	/* u2LLH+u2ACLSeq for AM HCI flush ACL frame */

	/* TXM for retransmitting a flushed packet */
	u_int8_t fgIsSnAssigned;

	/* To remember the Sequence Control field of this MPDU */
	uint16_t u2SequenceNumber;
};
#endif

#if 0
/* nic_rx.h */
struct SW_RFB {

	/* 4 ----------------MSDU_INFO and SW_RFB Common Fields-------------- */

	struct LINK_ENTRY rLinkEntry;
	uint8_t *pucBuffer;	/* Pointer to the associated buffer */

	uint8_t ucBufferSource;	/* HIF TX0, HIF TX1, MAC RX, or MNG Pool */

	/* Network type index that this TX packet is assocaited with */
	uint8_t ucNetworkTypeIndex;

	/* 0 to 5 (used by HIF TX to increment the corresponding TC counter) */
	uint8_t ucTC;

	uint8_t ucTID;		/* Traffic Identification */

	u_int8_t fgIs802_11Frame;	/* Set to TRUE for 802.11 frame */
	uint8_t ucMacHeaderLength;
	uint16_t u2PayloadLength;
	uint8_t *pucMacHeader;	/* 802.11 header  */
	uint8_t *pucPayload;	/* 802.11 payload */

	OS_SYSTIME rArrivalTime;	/* System Timestamp (4) */
	struct STA_RECORD *prStaRec;

#if CFG_PROFILE_BUFFER_TRACING
	ENUM_BUFFER_ACTIVITY_TYPE_T eActivity[2];
	uint32_t rActivityTime[2];
#endif
#if DBG && CFG_BUFFER_FREE_CHK
	u_int8_t fgBufferInSource;
#endif

	/* For specify some Control Flags, e.g. Basic Rate */
	uint8_t ucControlFlag;

	/* 4 -----------------------Non-Common ------------------------- */

	/* For composing the HIF RX Header (TODO: move flags to
	 * ucControlFlag)
	 */
	/* Pointer to the Response packet to * HIF RX0 or RX1 */
	uint8_t *pucHifRxPacket;

	uint16_t u2HifRxPacketLength;
	uint8_t ucHeaderOffset;
	uint8_t ucHifRxPortIndex;

	uint16_t u2SequenceControl;

	/* (For MAC RX packet parsing) set to TRUE if 4 addresses are present */
	u_int8_t fgIsA4Frame;

	u_int8_t fgIsBAR;
	u_int8_t fgIsQoSData;
	u_int8_t fgIsAmsduSubframe;	/* Set to TRUE for A-MSDU Subframe */

	/* For HIF RX DMA Desc */
	u_int8_t fgTUChecksumCheckRequired;
	u_int8_t fgIPChecksumCheckRequired;
	uint8_t ucEtherTypeOffset;

};
#endif

enum ENUM_STA_REC_CMD_ACTION {
	STA_REC_CMD_ACTION_STA = 0,
	STA_REC_CMD_ACTION_BSS = 1,
	STA_REC_CMD_ACTION_BSS_EXCLUDE_STA = 2
};

#if CFG_SUPPORT_TDLS

/* TDLS FSM */
struct CMD_PEER_ADD {

	uint8_t aucPeerMac[6];
	enum ENUM_STA_TYPE eStaType;
};

struct CMD_PEER_UPDATE_HT_CAP_MCS_INFO {
	uint8_t arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	uint16_t u2RxHighest;
	uint8_t ucTxParams;
	uint8_t Reserved[3];
};

struct CMD_PEER_UPDATE_VHT_CAP_MCS_INFO {
	uint8_t arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
};

struct CMD_PEER_UPDATE_HT_CAP {
	uint16_t u2CapInfo;
	uint8_t ucAmpduParamsInfo;

	/* 16 bytes MCS information */
	struct CMD_PEER_UPDATE_HT_CAP_MCS_INFO rMCS;

	uint16_t u2ExtHtCapInfo;
	uint32_t u4TxBfCapInfo;
	uint8_t ucAntennaSelInfo;
};

struct CMD_PEER_UPDATE_VHT_CAP {
	uint16_t u2CapInfo;
	/* 16 bytes MCS information */
	struct CMD_PEER_UPDATE_VHT_CAP_MCS_INFO rVMCS;

};

struct CMD_PEER_UPDATE {

	uint8_t aucPeerMac[6];

#define CMD_PEER_UPDATE_SUP_CHAN_MAX			50
	uint8_t aucSupChan[CMD_PEER_UPDATE_SUP_CHAN_MAX];

	uint16_t u2StatusCode;

#define CMD_PEER_UPDATE_SUP_RATE_MAX			50
	uint8_t aucSupRate[CMD_PEER_UPDATE_SUP_RATE_MAX];
	uint16_t u2SupRateLen;

	uint8_t UapsdBitmap;
	uint8_t UapsdMaxSp;	/* MAX_SP */

	uint16_t u2Capability;
#define CMD_PEER_UPDATE_EXT_CAP_MAXLEN			5
	uint8_t aucExtCap[CMD_PEER_UPDATE_EXT_CAP_MAXLEN];
	uint16_t u2ExtCapLen;

	struct CMD_PEER_UPDATE_HT_CAP rHtCap;
	struct CMD_PEER_UPDATE_VHT_CAP rVHtCap;

	u_int8_t fgIsSupHt;
	enum ENUM_STA_TYPE eStaType;

	/* TODO */
	/* So far, TDLS only a few of the parameters, the rest will be added
	 * in the future requiements
	 */
	/* kernel 3.10 station paramenters */
#if 0
	struct station_parameters {
	   const u8 *supported_rates;
	   struct net_device *vlan;
	   u32 sta_flags_mask, sta_flags_set;
	   u32 sta_modify_mask;
	   int listen_interval;
	   u16 aid;
	   u8 supported_rates_len;
	   u8 plink_action;
	   u8 plink_state;
	   const struct ieee80211_ht_cap *ht_capa;
	   const struct ieee80211_vht_cap *vht_capa;
	   u8 uapsd_queues;
	   u8 max_sp;
	   enum nl80211_mesh_power_mode local_pm;
	   u16 capability;
	   const u8 *ext_capab;
	   u8 ext_capab_len;
	   const u8 *supported_channels;
	   u8 supported_channels_len;
	   const u8 *supported_oper_classes;
	   u8 supported_oper_classes_len;
	   };
#endif

};

#endif
/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

#if CFG_DBG_MGT_BUF
#define cnmMgtPktAlloc(_prAdapter, _u4Length) \
	cnmPktAllocWrapper((_prAdapter), (_u4Length), (uint8_t *)__func__)

#define cnmMgtPktFree(_prAdapter, _prMsduInfo) \
	cnmPktFreeWrapper((_prAdapter), (_prMsduInfo), (uint8_t *)__func__)
#else
#define cnmMgtPktAlloc cnmPktAlloc
#define cnmMgtPktFree cnmPktFree
#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

struct MSDU_INFO *cnmPktAllocWrapper(IN struct ADAPTER *prAdapter,
	IN uint32_t u4Length, IN uint8_t *pucStr);

void cnmPktFreeWrapper(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint8_t *pucStr);

struct MSDU_INFO *cnmPktAlloc(IN struct ADAPTER *prAdapter,
	IN uint32_t u4Length);

void cnmPktFree(IN struct ADAPTER *prAdapter, IN struct MSDU_INFO *prMsduInfo);

void cnmMemInit(IN struct ADAPTER *prAdapter);

void *cnmMemAlloc(IN struct ADAPTER *prAdapter, IN enum ENUM_RAM_TYPE eRamType,
	IN uint32_t u4Length);

void cnmMemFree(IN struct ADAPTER *prAdapter, IN void *pvMemory);

void cnmStaRecInit(IN struct ADAPTER *prAdapter);

struct STA_RECORD *
cnmStaRecAlloc(IN struct ADAPTER *prAdapter, IN enum ENUM_STA_TYPE eStaType,
	IN uint8_t ucBssIndex, IN uint8_t *pucMacAddr);

void cnmStaRecFree(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec);

void cnmStaFreeAllStaByNetwork(struct ADAPTER *prAdapter, uint8_t ucBssIndex,
	uint8_t ucStaRecIndexExcluded);

struct STA_RECORD *cnmGetStaRecByIndex(IN struct ADAPTER *prAdapter,
	IN uint8_t ucIndex);

struct STA_RECORD *cnmGetStaRecByAddress(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint8_t aucPeerMACAddress[]);

void cnmStaRecChangeState(IN struct ADAPTER *prAdapter,
	IN OUT struct STA_RECORD *prStaRec, IN uint8_t ucNewState);

struct STA_RECORD *cnmGetAnyStaRecByAddress(struct ADAPTER *prAdapter,
					    uint8_t *pucPeerMacAddr);

void cnmDumpStaRec(IN struct ADAPTER *prAdapter, IN uint8_t ucStaRecIdx);

uint32_t cnmDumpMemoryStatus(IN struct ADAPTER *prAdapter, IN uint8_t *pucBuf,
	IN uint32_t u4Max);

#if CFG_SUPPORT_TDLS
uint32_t			/* TDLS_STATUS */
cnmPeerAdd(struct ADAPTER *prAdapter, void *pvSetBuffer,
	uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen);

uint32_t			/* TDLS_STATUS */
cnmPeerUpdate(struct ADAPTER *prAdapter, void *pvSetBuffer,
	uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen);

struct STA_RECORD *cnmGetTdlsPeerByAddress(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint8_t aucPeerMACAddress[]);
#endif

void cnmStaSendUpdateCmd(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec,
	 struct TXBF_PFMU_STA_INFO *prTxBfPfmuStaInfo, u_int8_t fgNeedResp);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#ifndef _lint
/* Kevin: we don't have to call following function to inspect the data
 * structure. It will check automatically while at compile time.
 * We'll need this for porting driver to different RTOS.
 */
static __KAL_INLINE__ void cnmMemDataTypeCheck(void)
{
#if 0
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, rLinkEntry)
			== 0);

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, rLinkEntry)
			== OFFSET_OF(struct SW_RFB, rLinkEntry));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, pucBuffer)
			== OFFSET_OF(struct SW_RFB, pucBuffer));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucBufferSource)
			== OFFSET_OF(struct SW_RFB, ucBufferSource));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, pucMacHeader)
			== OFFSET_OF(struct SW_RFB, pucMacHeader));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucMacHeaderLength)
			== OFFSET_OF(struct SW_RFB, ucMacHeaderLength));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, pucPayload)
			== OFFSET_OF(struct SW_RFB, pucPayload));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, u2PayloadLength)
			 == OFFSET_OF(struct SW_RFB, u2PayloadLength));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, prStaRec)
			== OFFSET_OF(struct SW_RFB, prStaRec));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucNetworkTypeIndex)
			== OFFSET_OF(struct SW_RFB, ucNetworkTypeIndex));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucTID)
			== OFFSET_OF(struct SW_RFB, ucTID));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, fgIs802_11Frame)
			== OFFSET_OF(struct SW_RFB, fgIs802_11Frame));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucControlFlag)
			== OFFSET_OF(struct SW_RFB, ucControlFlag));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, rArrivalTime)
			== OFFSET_OF(struct SW_RFB, rArrivalTime));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, ucTC)
			== OFFSET_OF(struct SW_RFB, ucTC));

#if CFG_PROFILE_BUFFER_TRACING
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, eActivity[0])
			== OFFSET_OF(struct SW_RFB, eActivity[0]));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, rActivityTime[0])
			== OFFSET_OF(struct SW_RFB, rActivityTime[0]));
#endif

#if DBG && CFG_BUFFER_FREE_CHK
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSDU_INFO, fgBufferInSource)
			== OFFSET_OF(struct SW_RFB, fgBufferInSource));
#endif

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct STA_RECORD, rLinkEntry)
			== 0);

	return;
#endif
}
#endif /* _lint */

#endif /* _CNM_MEM_H */
