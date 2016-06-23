/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 *
 * This file limGlobal.h contains the definitions exported by
 * LIM module.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_GLOBAL_H
#define __LIM_GLOBAL_H

#include "wniApi.h"
#include "sirApi.h"
#include "sirMacProtDef.h"
#include "sirMacPropExts.h"
#include "sirCommon.h"
#include "sirDebug.h"
#include "wniCfgSta.h"
#include "csrApi.h"
#include "sapApi.h"
#include "dot11f.h"

/// Maximum number of scan hash table entries
#define LIM_MAX_NUM_OF_SCAN_RESULTS 256

// Link Test Report Status. This appears in the report frame
#define LINK_TEST_STATUS_SUCCESS                0x1
#define LINK_TEST_STATUS_UNSUPPORTED_RATE       0x2
#define LINK_TEST_STATUS_INVALID_ADDR           0x3

// Amount of time in nanosec to be sleep-waited before
// enabling RHP (1 millisec)
#define LIM_RHP_WORK_AROUND_DURATION 1000000

// Maximum amount of Quiet duration in millisec
#define LIM_MAX_QUIET_DURATION 32

#define LIM_TX_WQ_EMPTY_SLEEP_NS                100000

// Deferred Message Queue Length
#define MAX_DEFERRED_QUEUE_LEN                  20

// Maximum Buffer size
#define LIM_MAX_BUF_SIZE                        8192

// Maximum number of PS - TIM's to be sent with out wakeup from STA
#define LIM_TIM_WAIT_COUNT_FACTOR          5

/** Use this count if (LIM_TIM_WAIT_FACTOR * ListenInterval) is less than LIM_MIN_TIM_WAIT_CNT*/
#define LIM_MIN_TIM_WAIT_COUNT          50  

#define GET_TIM_WAIT_COUNT(LIntrvl)        ((LIntrvl * LIM_TIM_WAIT_COUNT_FACTOR) > LIM_MIN_TIM_WAIT_COUNT ? \
                                                                    (LIntrvl * LIM_TIM_WAIT_COUNT_FACTOR) : LIM_MIN_TIM_WAIT_COUNT)
#define IS_5G_BAND(__rfBand)     ((__rfBand & 0x3) == 0x2)
#define IS_24G_BAND(__rfBand)    ((__rfBand & 0x3) == 0x1)

// enums exported by LIM are as follows

/// System role definition
typedef enum eLimSystemRole
{
    eLIM_UNKNOWN_ROLE,
    eLIM_AP_ROLE,
    eLIM_STA_IN_IBSS_ROLE,
    eLIM_STA_ROLE,
    eLIM_BT_AMP_STA_ROLE,
    eLIM_BT_AMP_AP_ROLE,
    eLIM_P2P_DEVICE_ROLE,
    eLIM_P2P_DEVICE_GO,
    eLIM_P2P_DEVICE_CLIENT
} tLimSystemRole;

/**
 * SME state definition accessible across all Sirius modules.
 * AP only states are LIM_SME_CHANNEL_SCAN_STATE &
 * LIM_SME_NORMAL_CHANNEL_SCAN_STATE.
 * Note that these states may also be present in STA
 * side too when DFS support is present for a STA in IBSS mode.
 */
typedef enum eLimSmeStates
{
    eLIM_SME_OFFLINE_STATE,
    eLIM_SME_IDLE_STATE,
    eLIM_SME_SUSPEND_STATE,
    eLIM_SME_WT_SCAN_STATE,
    eLIM_SME_WT_JOIN_STATE,
    eLIM_SME_WT_AUTH_STATE,
    eLIM_SME_WT_ASSOC_STATE,
    eLIM_SME_WT_REASSOC_STATE,
    eLIM_SME_WT_REASSOC_LINK_FAIL_STATE,
    eLIM_SME_JOIN_FAILURE_STATE,
    eLIM_SME_ASSOCIATED_STATE,
    eLIM_SME_REASSOCIATED_STATE,
    eLIM_SME_LINK_EST_STATE,
    eLIM_SME_LINK_EST_WT_SCAN_STATE,
    eLIM_SME_WT_PRE_AUTH_STATE,
    eLIM_SME_WT_DISASSOC_STATE,
    eLIM_SME_WT_DEAUTH_STATE,
    eLIM_SME_WT_START_BSS_STATE,
    eLIM_SME_WT_STOP_BSS_STATE,
    eLIM_SME_NORMAL_STATE,
    eLIM_SME_CHANNEL_SCAN_STATE,
    eLIM_SME_NORMAL_CHANNEL_SCAN_STATE
} tLimSmeStates;

/**
 * MLM state definition.
 * While these states are present on AP too when it is
 * STA mode, per-STA MLM state exclusive to AP is:
 * eLIM_MLM_WT_AUTH_FRAME3.
 */
typedef enum eLimMlmStates
{
    eLIM_MLM_OFFLINE_STATE,
    eLIM_MLM_IDLE_STATE,
    eLIM_MLM_WT_PROBE_RESP_STATE,
    eLIM_MLM_PASSIVE_SCAN_STATE,
    eLIM_MLM_WT_JOIN_BEACON_STATE,
    eLIM_MLM_JOINED_STATE,
    eLIM_MLM_BSS_STARTED_STATE,
    eLIM_MLM_WT_AUTH_FRAME2_STATE,
    eLIM_MLM_WT_AUTH_FRAME3_STATE,
    eLIM_MLM_WT_AUTH_FRAME4_STATE,
    eLIM_MLM_AUTH_RSP_TIMEOUT_STATE,
    eLIM_MLM_AUTHENTICATED_STATE,
    eLIM_MLM_WT_ASSOC_RSP_STATE,
    eLIM_MLM_WT_REASSOC_RSP_STATE,
    eLIM_MLM_ASSOCIATED_STATE,
    eLIM_MLM_REASSOCIATED_STATE,
    eLIM_MLM_LINK_ESTABLISHED_STATE,
    eLIM_MLM_WT_ASSOC_CNF_STATE,
    eLIM_MLM_LEARN_STATE,
    eLIM_MLM_WT_ADD_BSS_RSP_STATE,
    eLIM_MLM_WT_DEL_BSS_RSP_STATE,
    eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE,
    eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE,
    eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE,
    eLIM_MLM_WT_ADD_STA_RSP_STATE,
    eLIM_MLM_WT_DEL_STA_RSP_STATE,
    //MLM goes to this state when LIM initiates DELETE_STA as processing of Assoc req because
    //the entry already exists. LIM comes out of this state when DELETE_STA response from
    //HAL is received. LIM needs to maintain this state so that ADD_STA can be issued while
    //processing DELETE_STA response from HAL.
    eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE,
    eLIM_MLM_WT_SET_BSS_KEY_STATE,
    eLIM_MLM_WT_SET_STA_KEY_STATE,
    eLIM_MLM_WT_SET_STA_BCASTKEY_STATE,    
    eLIM_MLM_WT_ADDBA_RSP_STATE,
    eLIM_MLM_WT_REMOVE_BSS_KEY_STATE,
    eLIM_MLM_WT_REMOVE_STA_KEY_STATE,
    eLIM_MLM_WT_SET_MIMOPS_STATE,
#if defined WLAN_FEATURE_VOWIFI_11R
    eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE,
    eLIM_MLM_WT_FT_REASSOC_RSP_STATE,
#endif
    eLIM_MLM_P2P_LISTEN_STATE,
} tLimMlmStates;

// 11h channel quiet states
/* This enum indicates in which state the device is in
 * when it receives quiet element in beacon or probe-response.
 * The default quiet state of the device is always INIT
 * eLIM_QUIET_BEGIN - When Quiet period is started
 * eLIM_QUIET_CHANGED - When Quiet period is updated
 * eLIM_QUIET_RUNNING - Between two successive Quiet updates
 * eLIM_QUIET_END - When quiet period ends
 */
typedef enum eLimQuietStates
{
    eLIM_QUIET_INIT,
    eLIM_QUIET_BEGIN,
    eLIM_QUIET_CHANGED,
    eLIM_QUIET_RUNNING,
    eLIM_QUIET_END
} tLimQuietStates;

// 11h channel switch states
/* This enum indicates in which state the channel-swith
 * is presently operating.
 * eLIM_11H_CHANSW_INIT - Default state
 * eLIM_11H_CHANSW_RUNNING - When channel switch is running
 * eLIM_11H_CHANSW_END - After channel switch is complete
 */
typedef enum eLimDot11hChanSwStates
{
    eLIM_11H_CHANSW_INIT,
    eLIM_11H_CHANSW_RUNNING,
    eLIM_11H_CHANSW_END
} tLimDot11hChanSwStates;

#ifdef GEN4_SCAN

//WLAN_SUSPEND_LINK Related
typedef void (*SUSPEND_RESUME_LINK_CALLBACK)(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data);

// LIM to HAL SCAN Management Message Interface states
typedef enum eLimHalScanState
{
  eLIM_HAL_IDLE_SCAN_STATE,
  eLIM_HAL_INIT_SCAN_WAIT_STATE,
  eLIM_HAL_START_SCAN_WAIT_STATE,
  eLIM_HAL_END_SCAN_WAIT_STATE,
  eLIM_HAL_FINISH_SCAN_WAIT_STATE,
  eLIM_HAL_INIT_LEARN_WAIT_STATE,
  eLIM_HAL_START_LEARN_WAIT_STATE,
  eLIM_HAL_END_LEARN_WAIT_STATE,
  eLIM_HAL_FINISH_LEARN_WAIT_STATE,
  eLIM_HAL_SCANNING_STATE,
//WLAN_SUSPEND_LINK Related
  eLIM_HAL_SUSPEND_LINK_WAIT_STATE,
  eLIM_HAL_SUSPEND_LINK_STATE,
  eLIM_HAL_RESUME_LINK_WAIT_STATE,
//end WLAN_SUSPEND_LINK Related
} tLimLimHalScanState;
#endif // GEN4_SCAN

// LIM states related to A-MPDU/BA
// This is used for maintaining the state between PE and HAL only.
typedef enum eLimBAState
{
  eLIM_BA_STATE_IDLE, // we are not waiting for anything from HAL.
  eLIM_BA_STATE_WT_ADD_RSP, //We are waiting for Add rsponse from HAL.
  eLIM_BA_STATE_WT_DEL_RSP //  We are waiting for Del response from HAL.
} tLimBAState;





// MLM Req/Cnf structure definitions
typedef struct sLimMlmAuthReq
{
    tSirMacAddr    peerMacAddr;
    tAniAuthType   authType;
    tANI_U32       authFailureTimeout;
    tANI_U8        sessionId; 
} tLimMlmAuthReq, *tpLimMlmAuthReq;

typedef struct sLimMlmJoinReq
{
    tANI_U32               joinFailureTimeout;
    tSirMacRateSet         operationalRateSet;
    tANI_U8                 sessionId;
    tSirBssDescription     bssDescription;
} tLimMlmJoinReq, *tpLimMlmJoinReq;

typedef struct sLimMlmScanReq
{
    tSirBssType        bssType;
    tSirMacAddr        bssId;
    tSirMacSSid        ssId[SIR_SCAN_MAX_NUM_SSID];
    tSirScanType       scanType;
    tANI_U32           minChannelTime;
    tANI_U32           maxChannelTime;
    tANI_U32           minChannelTimeBtc;
    tANI_U32           maxChannelTimeBtc;
    tSirBackgroundScanMode  backgroundScanMode;
    tANI_U32 dot11mode;
    /* Number of SSIDs to scan(send Probe request) */
    tANI_U8            numSsid;

    tANI_BOOLEAN   p2pSearch;
    tANI_U16           uIEFieldLen;
    tANI_U16           uIEFieldOffset;

    //channelList MUST be the last field of this structure
    tSirChannelList    channelList;
    /*-----------------------------
      tLimMlmScanReq....
      -----------------------------
      uIEFiledLen 
      -----------------------------
      uIEFiledOffset               ----+
      -----------------------------    |
      channelList.numChannels          |
      -----------------------------    |
      ... variable size up to          |
      channelNumber[numChannels-1]     |
      This can be zero, if             |
      numChannel is zero.              |
      ----------------------------- <--+
      ... variable size uIEFiled 
      up to uIEFieldLen (can be 0)
      -----------------------------*/
} tLimMlmScanReq, *tpLimMlmScanReq;

typedef struct tLimScanResultNode tLimScanResultNode;
struct tLimScanResultNode
{
    tLimScanResultNode *next;
    tSirBssDescription bssDescription;
};

#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_REQ_SIZE 
#define OEM_DATA_REQ_SIZE 134
#endif
#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

// OEM Data related structure definitions
typedef struct sLimMlmOemDataReq
{
    tSirMacAddr           selfMacAddr;
    tANI_U8               oemDataReq[OEM_DATA_REQ_SIZE];
} tLimMlmOemDataReq, *tpLimMlmOemDataReq;

typedef struct sLimMlmOemDataRsp
{
   tANI_U8                oemDataRsp[OEM_DATA_RSP_SIZE];
} tLimMlmOemDataRsp, *tpLimMlmOemDataRsp;
#endif

// Pre-authentication structure definition
typedef struct tLimPreAuthNode
{
    struct tLimPreAuthNode     *next;
    tSirMacAddr         peerMacAddr;
    tAniAuthType        authType;
    tLimMlmStates       mlmState;
    tANI_U8             authNodeIdx;
    tANI_U8             challengeText[SIR_MAC_AUTH_CHALLENGE_LENGTH];
    tANI_U8             fTimerStarted:1;
    tANI_U8             fSeen:1;
    tANI_U8             fFree:1;
    tANI_U8             rsvd:5;
    TX_TIMER            timer;
}tLimPreAuthNode, *tpLimPreAuthNode;

// Pre-authentication table definition
typedef struct tLimPreAuthTable
{
    tANI_U32        numEntry;
    tpLimPreAuthNode pTable;
}tLimPreAuthTable, *tpLimPreAuthTable;

/// Per STA context structure definition
typedef struct sLimMlmStaContext
{
    tLimMlmStates           mlmState;
    tAniAuthType            authType;
    tANI_U16                listenInterval;
    tSirMacCapabilityInfo   capabilityInfo;
    tSirMacPropRateSet      propRateSet;
    tSirMacReasonCodes      disassocReason;
    tANI_U16                cleanupTrigger;

    tSirResultCodes resultCode;
    tANI_U16 protStatusCode;
    
    tANI_U8                 subType:1; // Indicates ASSOC (0) or REASSOC (1)
    tANI_U8                 updateContext:1;
    tANI_U8                 schClean:1;
    // 802.11n HT Capability in Station: Enabled 1 or DIsabled 0
    tANI_U8                 htCapability:1;
#ifdef WLAN_FEATURE_11AC
    tANI_U8                 vhtCapability:1;
#endif
} tLimMlmStaContext, *tpLimMlmStaContext;

// Structure definition to hold deferred messages queue parameters
typedef struct sLimDeferredMsgQParams
{
    tSirMsgQ    deferredQueue[MAX_DEFERRED_QUEUE_LEN];
    tANI_U16         size;
    tANI_U16         read;
    tANI_U16         write;
} tLimDeferredMsgQParams, *tpLimDeferredMsgQParams;

typedef struct sLimTraceQ
{
    tANI_U32                type;
    tLimSmeStates      smeState;
    tLimMlmStates      mlmState;
    tANI_U32                value;
    tANI_U32                value2;
} tLimTraceQ;

typedef struct sLimTraceParams
{
    tLimTraceQ    traceQueue[1024];
    tANI_U16           write;
    tANI_U16           enabled;
} tLimTraceParams;

typedef struct sCfgProtection
{
    tANI_U32 overlapFromlla:1;
    tANI_U32 overlapFromllb:1;
    tANI_U32 overlapFromllg:1;
    tANI_U32 overlapHt20:1;
    tANI_U32 overlapNonGf:1;
    tANI_U32 overlapLsigTxop:1;
    tANI_U32 overlapRifs:1;
    tANI_U32 overlapOBSS:1; /* added for obss */
    tANI_U32 fromlla:1;
    tANI_U32 fromllb:1;
    tANI_U32 fromllg:1;
    tANI_U32 ht20:1;
    tANI_U32 nonGf:1;
    tANI_U32 lsigTxop:1;
    tANI_U32 rifs:1;
    tANI_U32 obss:1; /* added for Obss */
}tCfgProtection, *tpCfgProtection;

typedef enum eLimProtStaCacheType
{
    eLIM_PROT_STA_CACHE_TYPE_INVALID,
    eLIM_PROT_STA_CACHE_TYPE_llB,
    eLIM_PROT_STA_CACHE_TYPE_llG,  
    eLIM_PROT_STA_CACHE_TYPE_HT20
}tLimProtStaCacheType;

typedef struct sCacheParams
{
    tANI_U8        active;
    tSirMacAddr   addr;    
    tLimProtStaCacheType protStaCacheType;
    
} tCacheParams, *tpCacheParams;

#define LIM_PROT_STA_OVERLAP_CACHE_SIZE    HAL_NUM_ASSOC_STA
#define LIM_PROT_STA_CACHE_SIZE            HAL_NUM_ASSOC_STA

typedef struct sLimProtStaParams
{
    tANI_U8               numSta;
    tANI_U8               protectionEnabled;
} tLimProtStaParams, *tpLimProtStaParams;


typedef struct sLimNoShortParams
{
    tANI_U8           numNonShortPreambleSta;
    tCacheParams      staNoShortCache[LIM_PROT_STA_CACHE_SIZE];
} tLimNoShortParams, *tpLimNoShortParams;

typedef struct sLimNoShortSlotParams
{
    tANI_U8           numNonShortSlotSta;
    tCacheParams      staNoShortSlotCache[LIM_PROT_STA_CACHE_SIZE];
} tLimNoShortSlotParams, *tpLimNoShortSlotParams;


typedef struct tLimIbssPeerNode tLimIbssPeerNode;
struct tLimIbssPeerNode
{
    tLimIbssPeerNode         *next;
    tSirMacAddr              peerMacAddr;
    tANI_U8                       aniIndicator:1;
    tANI_U8                       extendedRatesPresent:1;
    tANI_U8                       edcaPresent:1;
    tANI_U8                       wmeEdcaPresent:1;
    tANI_U8                       wmeInfoPresent:1;
    tANI_U8                       htCapable:1;
    tANI_U8                       vhtCapable:1;
    tANI_U8                       rsvd:1;
    tANI_U8                       htSecondaryChannelOffset;
    tSirMacCapabilityInfo    capabilityInfo;
    tSirMacRateSet           supportedRates;
    tSirMacRateSet           extendedRates;
    tANI_U8                   supportedMCSSet[SIZE_OF_SUPPORTED_MCS_SET];
    tSirMacEdcaParamSetIE    edcaParams;
    tANI_U16 propCapability;
    tANI_U8  erpIePresent;

    //HT Capabilities of IBSS Peer
    tANI_U8 htGreenfield;
    tANI_U8 htShortGI40Mhz;
    tANI_U8 htShortGI20Mhz;

    // DSSS/CCK at 40 MHz: Enabled 1 or Disabled
    tANI_U8 htDsssCckRate40MHzSupport;

    // MIMO Power Save
    tSirMacHTMIMOPowerSaveState htMIMOPSState;

    //
    // A-MPDU Density
    // 000 - No restriction
    // 001 - 1/8 usec
    // 010 - 1/4 usec
    // 011 - 1/2 usec
    // 100 - 1 usec
    // 101 - 2 usec
    // 110 - 4 usec
    // 111 - 8 usec
    //
    tANI_U8 htAMpduDensity;

    // Maximum Rx A-MPDU factor
    tANI_U8 htMaxRxAMpduFactor;

    //Set to 0 for 3839 octets
    //Set to 1 for 7935 octets
    tANI_U8 htMaxAmsduLength;

    //
    // Recommended Tx Width Set
    // 0 - use 20 MHz channel (control channel)
    // 1 - use 40 Mhz channel
    //
    tANI_U8 htSupportedChannelWidthSet;

    tANI_U8 beaconHBCount;
    tANI_U8 heartbeatFailure;

    tANI_U8 *beacon; //Hold beacon to be sent to HDD/CSR
    tANI_U16 beaconLen;

#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps VHTCaps;
    tANI_U8 vhtSupportedChannelWidthSet;
    tANI_U8 vhtBeamFormerCapable;
#endif
};

// Enums used for channel switching.
typedef enum eLimChannelSwitchState
{
    eLIM_CHANNEL_SWITCH_IDLE,
    eLIM_CHANNEL_SWITCH_PRIMARY_ONLY,
    eLIM_CHANNEL_SWITCH_SECONDARY_ONLY,
    eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY
} tLimChannelSwitchState;


// Channel Switch Info
typedef struct sLimChannelSwitchInfo
{
    tLimChannelSwitchState   state;
    tANI_U8                  primaryChannel;
    ePhyChanBondState        secondarySubBand;
    tANI_U32                 switchCount;
    tANI_U32                 switchTimeoutValue;
    tANI_U8                  switchMode;
} tLimChannelSwitchInfo, *tpLimChannelSwitchInfo;

#ifdef WLAN_FEATURE_11AC
typedef struct sLimOperatingModeInfo
{
    tANI_U8        present;
    tANI_U8        chanWidth: 2;
    tANI_U8         reserved: 2;
    tANI_U8            rxNSS: 3;
    tANI_U8        rxNSSType: 1;
}tLimOperatingModeInfo, *tpLimOperatingModeInfo;

typedef struct sLimWiderBWChannelSwitch
{
    tANI_U8      newChanWidth;
    tANI_U8      newCenterChanFreq0;
    tANI_U8      newCenterChanFreq1;
}tLimWiderBWChannelSwitchInfo, *tpLimWiderBWChannelSwitchInfo;
#endif
// Enums used when stopping the Tx.
typedef enum eLimQuietTxMode
{
    eLIM_TX_ALL = 0,       /* Stops/resumes the transmission of all stations, Uses the global flag. */
    eLIM_TX_STA,           /* Stops/resumes the transmission of specific stations identified by staId. */
    eLIM_TX_BSS,           /* Stops/resumes the transmission of all the packets in BSS */
    eLIM_TX_BSS_BUT_BEACON /* Stops/resumes the transmission of all packets except beacons in BSS
                                                 * This is used when radar is detected in the current operating channel.
                                                 * Beacon has to be sent to notify the stations associated about the
                                                 * scheduled channel switch */
} tLimQuietTxMode;

typedef enum eLimControlTx
{
    eLIM_RESUME_TX = 0,
    eLIM_STOP_TX
} tLimControlTx;


// --------------------------------------------------------------------

typedef __ani_attr_pre_packed struct sLimTspecInfo
{
    tANI_U8          inuse;       // 0==free, else used
    tANI_U8          idx;         // index in list
    tANI_U8          staAddr[6];
    tANI_U16         assocId;
    tSirMacTspecIE   tspec;
    tANI_U8          numTclas; // number of Tclas elements
    tSirTclasInfo    tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
    tANI_U8          tclasProc;
    tANI_U8          tclasProcPresent:1; //tclassProc is valid only if this is set to 1.
} __ani_attr_packed tLimTspecInfo, *tpLimTspecInfo;

typedef struct sLimAdmitPolicyInfo
{
    tANI_U8          type;      // admit control policy type
    tANI_U8          bw_factor; // oversubscription factor : 0 means nothing is allowed
                              // valid only when 'type' is set BW_FACTOR
} tLimAdmitPolicyInfo, *tpLimAdmitPolicyInfo;


typedef enum eLimWscEnrollState
{
    eLIM_WSC_ENROLL_NOOP,
    eLIM_WSC_ENROLL_BEGIN,
    eLIM_WSC_ENROLL_IN_PROGRESS,
    eLIM_WSC_ENROLL_END
    
} tLimWscEnrollState;

#define WSC_PASSWD_ID_PUSH_BUTTON         (0x0004)

typedef struct sLimWscIeInfo
{
    tANI_BOOLEAN       apSetupLocked;
    tANI_BOOLEAN       selectedRegistrar;
    tANI_U16           selectedRegistrarConfigMethods;
    tLimWscEnrollState wscEnrollmentState;
    tLimWscEnrollState probeRespWscEnrollmentState;
    tANI_U8            reqType;
    tANI_U8            respType;
} tLimWscIeInfo, *tpLimWscIeInfo;

// maximum number of tspec's supported
#define LIM_NUM_TSPEC_MAX      15


//structure to hold all 11h specific data
typedef struct sLimSpecMgmtInfo
{
    tLimQuietStates    quietState;
    tANI_U32           quietCount;
    tANI_U32           quietDuration;    /* This is in units of system TICKS */
    tANI_U32           quietDuration_TU; /* This is in units of TU, for over the air transmission */
    tANI_U32           quietTimeoutValue; /* After this timeout, actual quiet starts */
    tANI_BOOLEAN       fQuietEnabled;    /* Used on AP, if quiet is enabled during learning */

    tLimDot11hChanSwStates dot11hChanSwState;
        
    tANI_BOOLEAN       fRadarDetCurOperChan; /* Radar detected in cur oper chan on AP */
    tANI_BOOLEAN       fRadarIntrConfigured; /* Whether radar interrupt has been configured */
}tLimSpecMgmtInfo, *tpLimSpecMgmtInfo;

#ifdef FEATURE_WLAN_TDLS_INTERNAL
typedef struct sLimDisResultList
{
    struct sLimDisResultList *next ;
    tSirTdlsPeerInfo tdlsDisPeerInfo ;
}tLimDisResultList ;
#endif

#ifdef FEATURE_WLAN_TDLS
/*
 * Peer info needed for TDLS setup..
 */
typedef struct tLimTDLSPeerSta
{
    struct tLimTDLSPeerSta   *next;
    tANI_U8                  dialog ;
    tSirMacAddr              peerMac;
    tSirMacCapabilityInfo    capabilityInfo;
    tSirMacRateSet           supportedRates;
    tSirMacRateSet           extendedRates;
    tSirMacQosCapabilityStaIE qosCaps;
    tSirMacEdcaParamSetIE    edcaParams;
    tANI_U8                  mcsSet[SIZE_OF_SUPPORTED_MCS_SET];    
    tANI_U8                  tdls_bIsResponder ;
    /* HT Capabilties */
    tDot11fIEHTCaps tdlsPeerHTCaps ;
    tDot11fIEExtCap tdlsPeerExtCaps;
    tANI_U8 tdls_flags ;
    tANI_U8 tdls_link_state ;
    tANI_U8 tdls_prev_link_state ;
    tANI_U8 tdls_sessionId;
    tANI_U8 ExtRatesPresent ;
    TX_TIMER gLimTdlsLinkSetupRspTimeoutTimer ;
    TX_TIMER gLimTdlsLinkSetupCnfTimeoutTimer ;
}tLimTdlsLinkSetupPeer, *tpLimTdlsLinkSetupPeer ;

typedef struct tLimTdlsLinkSetupInfo
{
    tLimTdlsLinkSetupPeer *tdlsLinkSetupList ;
    tANI_U8 num_tdls_peers ;
    tANI_U8 tdls_flags ;
    tANI_U8 tdls_state ;
    tANI_U8 tdls_prev_state ; 
}tLimTdlsLinkSetupInfo, *tpLimTdlsLinkSetupInfo ;

typedef enum tdlsLinkMode
{
    TDLS_LINK_MODE_BG,
    TDLS_LINK_MODE_N,
    TDLS_LINK_MODE_AC,
    TDLS_LINK_MODE_NONE
} eLimTdlsLinkMode ;
#endif  /* FEATURE_WLAN_TDLS */

#endif
