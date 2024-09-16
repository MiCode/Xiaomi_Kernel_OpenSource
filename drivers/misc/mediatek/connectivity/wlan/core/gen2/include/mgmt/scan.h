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

#ifndef _SCAN_H
#define _SCAN_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "gl_vendor.h"

/* TDLS test purpose */
extern BOOLEAN flgTdlsTestExtCapElm;
extern UINT8 aucTdlsTestExtCapElm[];

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/*! Maximum buffer size of SCAN list */
#define SCN_MAX_BUFFER_SIZE                 (CFG_MAX_NUM_BSS_LIST * ALIGN_4(sizeof(BSS_DESC_T)))
#define SCN_ROAM_MAX_BUFFER_SIZE	(CFG_MAX_NUM_ROAM_BSS_LIST * ALIGN_4(sizeof(ROAM_BSS_DESC_T)))

#define SCN_RM_POLICY_EXCLUDE_CONNECTED     BIT(0)	/* Remove SCAN result except the connected one. */
#define SCN_RM_POLICY_TIMEOUT               BIT(1)	/* Remove the timeout one */
#define SCN_RM_POLICY_OLDEST_HIDDEN         BIT(2)	/* Remove the oldest one with hidden ssid */
#define SCN_RM_POLICY_SMART_WEAKEST         BIT(3)	/* If there are more than half BSS which has the
							 * same ssid as connection setting, remove the
							 * weakest one from them
							 * Else remove the weakest one.
							 */
#define SCN_RM_POLICY_ENTIRE                BIT(4)	/* Remove entire SCAN result */

#define SCN_BSS_DESC_SAME_SSID_THRESHOLD    20	/* This is used by POLICY SMART WEAKEST,
						 * If exceed this value, remove weakest BSS_DESC_T
						 * with same SSID first in large network.
						 */

/* the scan time in WFD mode + 2.4G/5G is about 9s so we need to enlarge the value */
#define SCN_BSS_DESC_REMOVE_TIMEOUT_SEC     15	/* Second. */
					      /* This is used by POLICY TIMEOUT,
					       * If exceed this value, remove timeout BSS_DESC_T.
					       */

#define SCN_PROBE_DELAY_MSEC                0

#define SCN_ADHOC_BSS_DESC_TIMEOUT_SEC      15	/* Second. */

#define SCN_NLO_NETWORK_CHANNEL_NUM         (4)

#define REMOVE_TIMEOUT_TWO_DAY			(60*60*24*2)

/*----------------------------------------------------------------------------*/
/* MSG_SCN_SCAN_REQ                                                           */
/*----------------------------------------------------------------------------*/
#define SCAN_REQ_SSID_WILDCARD              BIT(0)
#define SCAN_REQ_SSID_P2P_WILDCARD          BIT(1)
#define SCAN_REQ_SSID_SPECIFIED             BIT(2) /* two probe req will be sent, wildcard and specified */
#define SCAN_REQ_SSID_SPECIFIED_ONLY        BIT(3) /* only a specified ssid probe request will be sent */
/*----------------------------------------------------------------------------*/
/* Support Multiple SSID SCAN                                                 */
/*----------------------------------------------------------------------------*/
#define SCN_SSID_MAX_NUM                    CFG_SCAN_SSID_MAX_NUM
#define SCN_SSID_MATCH_MAX_NUM              CFG_SCAN_SSID_MATCH_MAX_NUM

#define SWC_NUM_BSSID_THRESHOLD_DEFAULT 8
#define SWC_RSSI_WINDSIZE_DEFAULT 8
#define LOST_AP_WINDOW 16
#define MAX_CHANNEL_NUM_PER_BUCKETS 8

#define SCN_BSS_JOIN_FAIL_THRESOLD				4
#define SCN_BSS_JOIN_FAIL_CNT_RESET_SEC				15
#define SCN_BSS_JOIN_FAIL_RESET_STEP				2

#if CFG_SUPPORT_BATCH_SCAN
/*----------------------------------------------------------------------------*/
/* SCAN_BATCH_REQ                                                             */
/*----------------------------------------------------------------------------*/
#define SCAN_BATCH_REQ_START                BIT(0)
#define SCAN_BATCH_REQ_STOP                 BIT(1)
#define SCAN_BATCH_REQ_RESULT               BIT(2)
#endif

#define SCAN_NLO_DEFAULT_INTERVAL           30000
/* PNO min period 30s, max period 300s */
#define SCAN_NLO_MIN_INTERVAL               30
#define SCAN_NLO_MAX_INTERVAL               300

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_SCAN_TYPE_T {
	SCAN_TYPE_PASSIVE_SCAN = 0,
	SCAN_TYPE_ACTIVE_SCAN,
	SCAN_TYPE_NUM
} ENUM_SCAN_TYPE_T, *P_ENUM_SCAN_TYPE_T;

typedef enum _ENUM_SCAN_STATE_T {
	SCAN_STATE_IDLE = 0,
	SCAN_STATE_SCANNING,
	SCAN_STATE_NUM
} ENUM_SCAN_STATE_T;

typedef enum _ENUM_SCAN_CHANNEL_T {
	SCAN_CHANNEL_FULL = 0,
	SCAN_CHANNEL_2G4,
	SCAN_CHANNEL_5G,
	SCAN_CHANNEL_P2P_SOCIAL,
	SCAN_CHANNEL_SPECIFIED,
	SCAN_CHANNEL_NUM
} ENUM_SCAN_CHANNEL, *P_ENUM_SCAN_CHANNEL;

typedef struct _MSG_SCN_FSM_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_32 u4Dummy;
} MSG_SCN_FSM_T, *P_MSG_SCN_FSM_T;

typedef enum _ENUM_POSTPONE_SCHED_SCAN_REQUEST_T {
	SCHED_SCAN_POSTPONE_START = 0,
	SCHED_SCAN_POSTPONE_STOP,
	SCHED_SCAN_POSTPONE_NUM
} ENUM_POSTPONE_SCHED_SCAN_REQUEST_T;


typedef enum _ENUM_PSCAN_STATE_T {
	PSCN_IDLE = 0,
	PSCN_RESET,
	PSCN_SCANNING,
	PSCAN_STATE_T_NUM
} ENUM_PSCAN_STATE_T;

/*----------------------------------------------------------------------------*/
/* BSS Descriptors                                                            */
/*----------------------------------------------------------------------------*/
struct _BSS_DESC_T {
	LINK_ENTRY_T rLinkEntry;
	LINK_ENTRY_T rLinkEntryEss;

	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* For IBSS, the SrcAddr is different from BSSID */

	BOOLEAN fgIsConnecting;	/* If we are going to connect to this BSS
				 * (JOIN or ROAMING to another BSS), don't
				 * remove this record from BSS List.
				 */
	BOOLEAN fgIsConnected;	/* If we have connected to this BSS (NORMAL_TR),
				 * don't removed this record from BSS list.
				 */

	BOOLEAN fgIsValidSSID; /* This flag is TRUE if the SSID is not hidden */
	BOOLEAN fgIsHiddenSSID;	/* When this flag is TRUE, means the SSID
				 * of this BSS is not known yet.
				 */
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];

	OS_SYSTIME rUpdateTime;

	ENUM_BSS_TYPE_T eBSSType;

	UINT_16 u2CapInfo;

	UINT_16 u2BeaconInterval;
	UINT_16 u2ATIMWindow;

	UINT_16 u2OperationalRateSet;
	UINT_16 u2BSSBasicRateSet;
	BOOLEAN fgIsUnknownBssBasicRate;

	BOOLEAN fgIsERPPresent;
	BOOLEAN fgIsHTPresent;

	UINT_8 ucPhyTypeSet;	/* Available PHY Type Set of this BSS */

	UINT_8 ucChannelNum;

	ENUM_CHNL_EXT_T eSco;	/*
				 * Record bandwidth for association process
				 * Some AP will send association resp by 40MHz BW
				 */
	ENUM_BAND_T eBand;

	UINT_8 ucDTIMPeriod;

	BOOLEAN fgIsLargerTSF;	/* This BSS's TimeStamp is larger than us(TCL == 1 in RX_STATUS_T) */

	UINT_8 ucRCPI;

	UINT_8 ucWmmFlag;	/* A flag to indicate this BSS's WMM capability */

	/*
	 * \brief The srbiter Search State will matched the scan result,
	 * and saved the selected cipher and akm, and report the score,
	 * for arbiter join state, join module will carry this target BSS
	 * to rsn generate ie function, for gen wpa/rsn ie
	 */
	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;

	UINT_16 u2RsnCap;

	RSN_INFO_T rRSNInfo;
	RSN_INFO_T rWPAInfo;
#if 1				/* CFG_SUPPORT_WAPI */
	WAPI_INFO_T rIEWAPI;
	BOOLEAN fgIEWAPI;
#endif
	BOOLEAN fgIERSN;
	BOOLEAN fgIEWPA;
	BOOLEAN fgIEOsen;

#if CFG_SUPPORT_DETECT_ATHEROS_AP
	BOOLEAN fgIsAtherosAP;
#endif

	/*! \brief RSN parameters selected for connection */
	/*
	 * ! \brief The Select score for final AP selection,
	 * 0, no sec, 1,2,3 group cipher is WEP, TKIP, CCMP
	 */
	UINT_8 ucEncLevel;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PPresent;
	BOOLEAN fgIsP2PReport;	/* TRUE: report to upper layer */
	P_P2P_DEVICE_DESC_T prP2pDesc;

	UINT_8 aucIntendIfAddr[MAC_ADDR_LEN];	/* For IBSS, the SrcAddr is different from BSSID */
	/* UINT_8 ucDevCapabilityBitmap;  */ /* Device Capability Attribute. (P2P_DEV_CAPABILITY_XXXX) */
	/* UINT_8 ucGroupCapabilityBitmap; */ /* Group Capability Attribute. (P2P_GROUP_CAPABILITY_XXXX) */

	LINK_T rP2pDeviceList;

	/* P_LINK_T prP2pDeviceList; */

	/* For
	 *    1. P2P Capability.
	 *    2. P2P Device ID. ( in aucSrcAddr[] )
	 *    3. NOA   (TODO:)
	 *    4. Extend Listen Timing. (Probe Rsp)  (TODO:)
	 *    5. P2P Device Info. (Probe Rsp)
	 *    6. P2P Group Info. (Probe Rsp)
	 */
#endif

	BOOLEAN fgIsIEOverflow;	/* The received IE length exceed the maximum IE buffer size */
	UINT_16 u2RawLength;	/* The byte count of aucRawBuf[] */
	UINT_16 u2IELength;	/* The byte count of aucIEBuf[] */

	ULARGE_INTEGER u8TimeStamp;	/* Place u8TimeStamp before aucIEBuf[1] to force DW align */
	UINT_8 aucRawBuf[CFG_RAW_BUFFER_SIZE];
	UINT_8 aucIEBuf[CFG_IE_BUFFER_SIZE];
	UINT_8 ucJoinFailureCount;
	OS_SYSTIME rJoinFailTime;
	struct AIS_BLACKLIST_ITEM *prBlack;
	UINT_16 u2StaCnt;
	UINT_16 u2AvaliableAC; /* Available Admission Capacity */
	UINT_8 ucChnlUtilization;
	UINT_8 ucSNR;
	BOOLEAN fgSeenProbeResp;
	BOOLEAN fgExsitBssLoadIE;
	BOOLEAN fgMultiAnttenaAndSTBC;
	BOOLEAN fgDeauthLastTime;
	UINT_32 u4UpdateIdx;
#if CFG_SUPPORT_ROAMING_RETRY
	BOOLEAN fgIsRoamFail;
#endif
	INT_8 cPowerLimit;
#if CFG_SUPPORT_RSN_SCORE
	BOOLEAN fgIsRSNSuitableBss;
#endif
};

struct _ROAM_BSS_DESC_T {
	LINK_ENTRY_T rLinkEntry;
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];
	OS_SYSTIME rUpdateTime;
};

typedef struct _SCAN_PARAM_T {	/* Used by SCAN FSM */
	/* Active or Passive */
	ENUM_SCAN_TYPE_T eScanType;

	/* Network Type */
	ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex;

	/* Specified SSID Type */
	UINT_8 ucSSIDType;
	UINT_8 ucSSIDNum;

	/* Length of Specified SSID */
	UINT_8 ucSpecifiedSSIDLen[SCN_SSID_MAX_NUM];

	/* Specified SSID */
	UINT_8 aucSpecifiedSSID[SCN_SSID_MAX_NUM][ELEM_MAX_LEN_SSID];

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgFindSpecificDev;	/* P2P: Discovery Protocol */
	UINT_8 aucDiscoverDevAddr[MAC_ADDR_LEN];
	BOOLEAN fgIsDevType;
	P2P_DEVICE_TYPE_T rDiscoverDevType;

	UINT_16 u2PassiveListenInterval;
	/* TODO: Find Specific Device Type. */
#endif				/* CFG_SUPPORT_P2P */
	UINT_16 u2ChannelDwellTime;
	UINT_16 u2MinChannelDwellTime;
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	BOOLEAN fgIsObssScan;
	BOOLEAN fgIsScanV2;

	/* Run time flags */
	UINT_16 u2ProbeDelayTime;

	/* channel information */
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];

	/* Feedback information */
	UINT_8 ucSeqNum;

	/* Information Element */
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
	UINT_8 aucRandomMac[MAC_ADDR_LEN];
} SCAN_PARAM_T, *P_SCAN_PARAM_T;

typedef struct _NLO_PARAM_T {	/* Used by SCAN FSM */
	SCAN_PARAM_T rScanParam;

	/* NLO */
	BOOLEAN fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;

	/* Match SSID */
	UINT_8 ucMatchSSIDNum;
	UINT_8 ucMatchSSIDLen[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucMatchSSID[SCN_SSID_MATCH_MAX_NUM][ELEM_MAX_LEN_SSID];

#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	/* SSID set*/
	UINT_8 ucSSIDNum;
	UINT_8 ucSSIDLen[CFG_SCAN_HIDDEN_SSID_MAX_NUM];
	UINT_8 aucSSID[CFG_SCAN_HIDDEN_SSID_MAX_NUM][ELEM_MAX_LEN_SSID];
#endif

	UINT_8 aucCipherAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_16 au2AuthAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucChannelHint[SCN_SSID_MATCH_MAX_NUM][SCN_NLO_NETWORK_CHANNEL_NUM];
	P_BSS_DESC_T aprPendingBssDescToInd[SCN_SSID_MATCH_MAX_NUM];
} NLO_PARAM_T, *P_NLO_PARAM_T;

#if 1

typedef struct _GSCN_CHANNEL_INFO_T {
	UINT_8 ucBand;
	UINT_8 ucChannelNumber;	/* Channel Number */
	UINT_8 ucPassive;	/* 0 => active, 1 => passive scan; ignored for DFS */
	UINT_8 aucReserved[1];

	UINT_32 u4DwellTimeMs;	/* dwell time hint */
	/* Add channel class */
} GSCN_CHANNEL_INFO_T, *P_GSCN_CHANNEL_INFO_T;

typedef struct _GSCAN_BUCKET_T {
	UINT_16 u2BucketIndex;	/* bucket index, 0 based */
	UINT_8 ucBucketFreqMultiple;	/*
					 * desired period, in millisecond;
					 * if this is too low, the firmware should choose to generate
					 * results as fast as it can instead of failing the command
					*/
	UINT_8 ucReportFlag;
	UINT_8 ucMaxBucketFreqMultiple; /* max_period / base_period */
	UINT_8 ucStepCount;
	UINT_8 ucNumChannels;
	UINT_8 aucReserved[1];
	WIFI_BAND eBand;	/* when UNSPECIFIED, use channel list */
	GSCN_CHANNEL_INFO_T arChannelList[GSCAN_MAX_CHANNELS];	/* channels to scan; these may include DFS channels */
} GSCAN_BUCKET_T, *P_GSCAN_BUCKET_T;

typedef struct _CMD_GSCN_REQ_T {
	UINT_8 ucFlags;
	UINT_8 ucNumScnToCache;
	UINT_8 aucReserved[2];
	UINT_32 u4BufferThreshold;
	UINT_32 u4BasePeriod;	/* base timer period in ms */
	UINT_32 u4NumBuckets;
	UINT_32 u4MaxApPerScan;	/* number of APs to store in each scan in the */
	/* BSSID/RSSI history buffer (keep the highest RSSI APs) */

	GSCAN_BUCKET_T arBucket[GSCAN_MAX_BUCKETS];
} CMD_GSCN_REQ_T, *P_CMD_GSCN_REQ_T;

#endif

typedef struct _CMD_GSCN_SCN_COFIG_T {
	UINT_8 ucNumApPerScn;		/* GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN */
	UINT_32 u4BufferThreshold;	/* GSCAN_ATTRIBUTE_REPORT_THRESHOLD */
	UINT_32 u4NumScnToCache;	/* GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE */
} CMD_GSCN_SCN_COFIG_T, *P_CMD_GSCN_SCN_COFIG_T;

typedef struct _CMD_GET_GSCAN_RESULT {
	UINT_8 ucVersion;
	UINT_8 aucReserved[2];
	UINT_8 ucFlush;
	UINT_32 u4Num;
} CMD_GET_GSCAN_RESULT_T, *P_CMD_GET_GSCAN_RESULT_T;

typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		/* Start/ Stop */
	UINT_8 ucMScan;		/* an integer number of scans per batch */
	UINT_8 ucBestn;		/* an integer number of the max AP to remember per scan */
	UINT_8 ucRtt;		/*
				 * an integer number of highest-strength AP for which we'd like
				 * approximate distance reported
				 */
	UINT_8 ucChannel;	/* channels */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	UINT_8 aucReserved[3];
	UINT_32 u4Scanfreq;	/* an integer number of seconds between scans */
	CHANNEL_INFO_T arChannelList[32];	/* channels */
} CMD_BATCH_REQ_T, *P_CMD_BATCH_REQ_T;

typedef struct _CMD_SET_PSCAN_PARAM {
	UINT_8 ucVersion;
	CMD_NLO_REQ rCmdNloReq;
	CMD_BATCH_REQ_T rCmdBatchReq;
	CMD_GSCN_REQ_T rCmdGscnReq;
	BOOLEAN fgNLOScnEnable;
	BOOLEAN fgBatchScnEnable;
	BOOLEAN fgGScnEnable;
	UINT_32 u4BasePeriod;	/* GSCAN_ATTRIBUTE_BASE_PERIOD */
} CMD_SET_PSCAN_PARAM, *P_CMD_SET_PSCAN_PARAM;

typedef struct _SCAN_INFO_T {
	ENUM_SCAN_STATE_T eCurrentState;	/* Store the STATE variable of SCAN FSM */

	OS_SYSTIME rLastScanCompletedTime;

	SCAN_PARAM_T rScanParam;
	NLO_PARAM_T rNloParam;

	UINT_32 u4NumOfBssDesc;

	UINT_8 aucScanBuffer[SCN_MAX_BUFFER_SIZE];

	LINK_T rBSSDescList;

	LINK_T rFreeBSSDescList;

	LINK_T rPendingMsgList;

	UINT_8 aucScanRoamBuffer[SCN_ROAM_MAX_BUFFER_SIZE];
	LINK_T rRoamFreeBSSDescList;
	LINK_T rRoamBSSDescList;

	/* Sparse Channel Detection */
	BOOLEAN fgIsSparseChannelValid;
	RF_CHANNEL_INFO_T rSparseChannel;

	/* NLO scanning state tracking */
	BOOLEAN fgNloScanning;
#if CFG_SUPPORT_SCN_PSCN
	BOOLEAN fgPscnOngoing;
	BOOLEAN fgGScnConfigSet;
	BOOLEAN fgGScnParamSet;
	BOOLEAN fgGScnAction;
	P_CMD_SET_PSCAN_PARAM prPscnParam;
	BOOLEAN fgIsPostponeSchedScan;
	ENUM_POSTPONE_SCHED_SCAN_REQUEST_T eCurrendSchedScanReq;
	PARAM_SCHED_SCAN_REQUEST rSchedScanRequest;
	ENUM_PSCAN_STATE_T eCurrentPSCNState;
#endif
#if CFG_SUPPORT_GSCN
	P_PARAM_WIFI_GSCAN_FULL_RESULT prGscnFullResult;
#endif

	UINT_32 u4ScanUpdateIdx;
} SCAN_INFO_T, *P_SCAN_INFO_T;

/* use to save partial scan channel information */
typedef struct _PARTIAL_SCAN_INFO_T {
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
} PARTIAL_SCAN_INFO, *P_PARTIAL_SCAN_INFO;

/* Incoming Mailbox Messages */
typedef struct _MSG_SCN_SCAN_REQ_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDLength;
	UINT_8 aucSSID[PARAM_MAX_LEN_SSID];
	UINT_16 u2ChannelDwellTime;	/* In TU. 1024us. */

	UINT_16 u2TimeoutValue; /* ms unit */ /* MULTI SSID */
	UINT_16 u2MinChannelDwellTime;	/* In TU. 1024us. */
	UINT_8 aucBSSID[MAC_ADDR_LEN];

	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} MSG_SCN_SCAN_REQ, *P_MSG_SCN_SCAN_REQ;

typedef struct _MSG_SCN_SCAN_REQ_V2_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDNum;
	P_PARAM_SSID_T prSsid;
	UINT_16 u2ProbeDelay;
	UINT_16 u2ChannelDwellTime;	/* In TU. 1024us. */
#if CFG_MULTI_SSID_SCAN
	UINT_16 u2TimeoutValue; /* ms unit */
#endif
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
	UINT_8 aucRandomMac[MAC_ADDR_LEN];
} MSG_SCN_SCAN_REQ_V2, *P_MSG_SCN_SCAN_REQ_V2;

typedef struct _MSG_SCN_SCAN_CANCEL_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsChannelExt;
#endif
} MSG_SCN_SCAN_CANCEL, *P_MSG_SCN_SCAN_CANCEL;

/* Outgoing Mailbox Messages */
typedef enum _ENUM_SCAN_STATUS_T {
	SCAN_STATUS_DONE = 0,
	SCAN_STATUS_CANCELLED,
	SCAN_STATUS_FAIL,
	SCAN_STATUS_BUSY,
	SCAN_STATUS_NUM
} ENUM_SCAN_STATUS, *P_ENUM_SCAN_STATUS;

typedef struct _MSG_SCN_SCAN_DONE_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_STATUS eScanStatus;
} MSG_SCN_SCAN_DONE, *P_MSG_SCN_SCAN_DONE;

#if CFG_SUPPORT_AGPS_ASSIST
typedef enum {
	AGPS_PHY_A,
	AGPS_PHY_B,
	AGPS_PHY_G,
} AP_PHY_TYPE;

typedef struct _AGPS_AP_INFO_T {
	UINT_8 aucBSSID[6];
	INT_16 i2ApRssi;	/* -127..128 */
	UINT_16 u2Channel;	/* 0..256 */
	AP_PHY_TYPE ePhyType;
} AGPS_AP_INFO_T, *P_AGPS_AP_INFO_T;

typedef struct _AGPS_AP_LIST_T {
	UINT_8 ucNum;
	AGPS_AP_INFO_T arApInfo[32];
} AGPS_AP_LIST_T, *P_AGPS_AP_LIST_T;
#endif

typedef struct _CMD_SET_PSCAN_ADD_HOTLIST_BSSID {
	UINT_8 aucMacAddr[6];
	UINT_8 ucFlags;
	UINT_8 aucReserved[5];
} CMD_SET_PSCAN_ADD_HOTLIST_BSSID, *P_CMD_SET_PSCAN_ADD_HOTLIST_BSSID;

typedef struct _CMD_SET_PSCAN_ADD_SWC_BSSID {
	INT_32 i4RssiLowThreshold;
	INT_32 i4RssiHighThreshold;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[6];
} CMD_SET_PSCAN_ADD_SWC_BSSID, *P_CMD_SET_PSCAN_ADD_SWC_BSSID;

typedef struct _CMD_SET_PSCAN_MAC_ADDR {
	UINT_8 ucVersion;
	UINT_8 ucFlags;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[8];
} CMD_SET_PSCAN_MAC_ADDR, *P_CMD_SET_PSCAN_MAC_ADDR;

struct RM_BEACON_REPORT_PARAMS {
	UINT_8 ucChannel;
	UINT_8 ucRCPI;
	UINT_8 ucRSNI;
	UINT_8 ucAntennaID;
	UINT_8 ucFrameInfo;
	UINT_8 aucBcnFixedField[12];
};

struct RM_MEASURE_REPORT_ENTRY {
	LINK_ENTRY_T rLinkEntry;
	/* should greater than sizeof(struct RM_BCN_REPORT) +
	** sizeof(IE_MEASUREMENT_REPORT_T) + RM_BCN_REPORT_SUB_ELEM_MAX_LENGTH
	*/
	UINT_8 aucMeasReport[260];
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
/*----------------------------------------------------------------------------*/
/* Routines in scan.c                                                         */
/*----------------------------------------------------------------------------*/
VOID scnInit(IN P_ADAPTER_T prAdapter);

VOID scnUninit(IN P_ADAPTER_T prAdapter);

/* BSS-DESC Search */
P_BSS_DESC_T scanSearchBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

P_BSS_DESC_T
scanSearchBssDescByBssidAndSsid(IN P_ADAPTER_T prAdapter,
				IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

P_BSS_DESC_T scanSearchBssDescByTA(IN P_ADAPTER_T prAdapter, IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchBssDescByTAAndSsid(IN P_ADAPTER_T prAdapter,
			     IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

#if CFG_SUPPORT_HOTSPOT_2_0
P_BSS_DESC_T scanSearchBssDescByBssidAndLatestUpdateTime(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);
#endif

/* BSS-DESC Search - Alternative */
P_BSS_DESC_T
scanSearchExistingBssDesc(IN P_ADAPTER_T prAdapter,
				IN ENUM_BSS_TYPE_T eBSSType, IN UINT_8 aucBSSID[], IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchExistingBssDescWithSsid(IN P_ADAPTER_T prAdapter,
					IN ENUM_BSS_TYPE_T eBSSType,
					IN UINT_8 aucBSSID[],
					IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

/* BSS-DESC Allocation */
P_BSS_DESC_T scanAllocateBssDesc(IN P_ADAPTER_T prAdapter);

/* BSS-DESC Removal */
VOID scanRemoveBssDescsByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemovePolicy);

VOID scanRemoveBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

VOID
scanRemoveBssDescByBandAndNetwork(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BAND_T eBand, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex);

/* BSS-DESC State Change */
VOID scanRemoveConnFlagOfBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

#if 0
/* BSS-DESC Insertion */
P_BSS_DESC_T scanAddToInternalScanResult(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb, IN P_BSS_DESC_T prBssDesc);
#endif

/* BSS-DESC Insertion - ALTERNATIVE */
P_BSS_DESC_T scanAddToBssDesc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS scanProcessBeaconAndProbeResp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb);

VOID
scanBuildProbeReqFrameCommonIEs(IN P_MSDU_INFO_T prMsduInfo,
				IN PUINT_8 pucDesiredSsid, IN UINT_32 u4DesiredSsidLen, IN UINT_16 u2SupportedRateSet);

WLAN_STATUS scanSendProbeReqFrames(IN P_ADAPTER_T prAdapter, IN P_SCAN_PARAM_T prScanParam);

VOID scanUpdateBssDescForSearch(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);

P_BSS_DESC_T scanSearchBssDescByPolicy(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex);

WLAN_STATUS scanAddScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_SW_RFB_T prSwRfb);

BOOLEAN scanCheckBssIsLegal(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);

VOID scanReportBss2Cfg80211(IN P_ADAPTER_T prAdapter, IN ENUM_BSS_TYPE_T eBSSType, IN P_BSS_DESC_T SpecificprBssDesc);

P_ROAM_BSS_DESC_T scanSearchRoamBssDescBySsid(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);
P_ROAM_BSS_DESC_T scanAllocateRoamBssDesc(IN P_ADAPTER_T prAdapter);
VOID scanAddToRoamBssDesc(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);
VOID scanSearchBssDescOfRoamSsid(IN P_ADAPTER_T prAdapter);
VOID scanRemoveRoamBssDescsByTime(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemoveTime);
/*----------------------------------------------------------------------------*/
/* Routines in scan_fsm.c                                                     */
/*----------------------------------------------------------------------------*/
VOID scnFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_SCAN_STATE_T eNextState);

/*----------------------------------------------------------------------------*/
/* Command Routines                                                           */
/*----------------------------------------------------------------------------*/
VOID scnSendScanReqExtCh(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReq(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReqV2ExtCh(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReqV2(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* RX Event Handling                                                          */
/*----------------------------------------------------------------------------*/
VOID scnEventScanDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_SCAN_DONE prScanDone);

VOID scnEventNloDone(IN P_ADAPTER_T	 prAdapter,	IN P_EVENT_NLO_DONE_T prNloDone);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Handling                                                   */
/*----------------------------------------------------------------------------*/
VOID scnFsmMsgStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmMsgAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmHandleScanMsg(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ prScanReqMsg);

VOID scnFsmHandleScanMsgV2(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg);

VOID scnFsmRemovePendingMsg(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Generation                                                 */
/*----------------------------------------------------------------------------*/
VOID
scnFsmGenerateScanDoneMsg(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex, IN ENUM_SCAN_STATUS eScanStatus);

/*----------------------------------------------------------------------------*/
/* Query for sparse channel                                                   */
/*----------------------------------------------------------------------------*/
BOOLEAN scnQuerySparseChannel(IN P_ADAPTER_T prAdapter, P_ENUM_BAND_T prSparseBand, PUINT_8 pucSparseChannel);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmSchedScanRequest(IN P_ADAPTER_T prAdapter);

BOOLEAN scnFsmSchedScanStopRequest(IN P_ADAPTER_T prAdapter);

#if CFG_SUPPORT_SCN_PSCN
BOOLEAN scnFsmPSCNAction(IN P_ADAPTER_T prAdapter, IN ENUM_PSCAN_ACT_T ucPscanAct);

BOOLEAN scnFsmPSCNSetParam(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);

BOOLEAN scnFsmGSCNSetHotlist(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);

BOOLEAN scnFsmPSCNAddSWCBssId(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_ADD_SWC_BSSID prCmdPscnAddSWCBssId);

BOOLEAN scnFsmPSCNSetMacAddr(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_MAC_ADDR prCmdPscnSetMacAddr);

BOOLEAN scnCombineParamsIntoPSCN(IN P_ADAPTER_T prAdapter,
				 IN P_CMD_NLO_REQ prCmdNloReq,
				 IN P_CMD_BATCH_REQ_T prCmdBatchReq,
				 IN P_CMD_GSCN_REQ_T prCmdGscnReq,
				 IN P_CMD_GSCN_SCN_COFIG_T prNewCmdGscnConfig,
				 IN BOOLEAN fgRemoveNLOfromPSCN,
				 IN BOOLEAN fgRemoveBatchSCNfromPSCN, IN BOOLEAN fgRemoveGSCNfromPSCN);

VOID scnPSCNFsm(IN P_ADAPTER_T prAdapter, IN ENUM_PSCAN_STATE_T eNextPSCNState);
#endif
#if CFG_NLO_MSP
VOID scnSetMspParameterIntoPSCN(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);
#endif
#if CFG_SUPPORT_GSCN
BOOLEAN scnSetGSCNParam(IN P_ADAPTER_T prAdapter, IN P_PARAM_WIFI_GSCAN_CMD_PARAMS prCmdGscnParam);

BOOLEAN scnSetGSCNConfig(IN P_ADAPTER_T prAdapter, IN P_CMD_GSCN_SCN_COFIG_T prCmdGscnScnConfig);

BOOLEAN scnFsmGetGSCNResult(IN P_ADAPTER_T prAdapter,
			    IN P_CMD_GET_GSCAN_RESULT_T prGetGscnResultCmd, OUT PUINT_32 pu4SetInfoLen);

BOOLEAN scnFsmGSCNResults(IN P_ADAPTER_T prAdapter, IN P_EVENT_GSCAN_RESULT_T prEventBuffer);
#endif

#if CFG_SUPPORT_AGPS_ASSIST
VOID scanReportScanResultToAgps(P_ADAPTER_T prAdapter);
#endif
P_BSS_DESC_T scanSearchBssDescByScoreForAis(P_ADAPTER_T prAdapter);
VOID scanGetCurrentEssChnlList(P_ADAPTER_T prAdapter);

VOID scanLogEssResult(P_ADAPTER_T prAdapter);
VOID scanInitEssResult(P_ADAPTER_T prAdapter);

#endif /* _SCAN_H */

VOID scanCollectBeaconReport(IN P_ADAPTER_T prAdapter, PUINT_8 pucIEBuf,
			     UINT_16 u2Length, PUINT_8 pucBssid, struct RM_BEACON_REPORT_PARAMS *prRepParams);
