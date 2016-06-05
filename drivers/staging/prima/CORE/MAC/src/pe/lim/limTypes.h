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
 * This file limTypes.h contains the definitions used by all
 * all LIM modules.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_TYPES_H
#define __LIM_TYPES_H

#include "wniApi.h"
#include "sirApi.h"
#include "sirCommon.h"
#include "sirMacProtDef.h"
#include "utilsApi.h"

#include "wlan_qct_wdi_ds.h"

#include "limApi.h"
#include "limDebug.h"
#include "limTrace.h"
#include "limSendSmeRspMessages.h"
#include "sysGlobal.h"
#include "dphGlobal.h"
#include "parserApi.h"

#define LINK_TEST_DEFER 1

#define TRACE_EVENT_CNF_TIMER_DEACT        0x6600
#define TRACE_EVENT_CNF_TIMER_ACT          0x6601
#define TRACE_EVENT_AUTH_RSP_TIMER_DEACT   0x6602
#define TRACE_EVENT_AUTH_RSP_TIMER_ACT     0x6603

// MLM message types
#define LIM_MLM_MSG_START           1000
#define LIM_MLM_SCAN_REQ            LIM_MLM_MSG_START
#define LIM_MLM_SCAN_CNF            (LIM_MLM_MSG_START + 1)
#define LIM_MLM_START_REQ           (LIM_MLM_MSG_START + 2)
#define LIM_MLM_START_CNF           (LIM_MLM_MSG_START + 3)
#define LIM_MLM_JOIN_REQ            (LIM_MLM_MSG_START + 4)
#define LIM_MLM_JOIN_CNF            (LIM_MLM_MSG_START + 5)
#define LIM_MLM_AUTH_REQ            (LIM_MLM_MSG_START + 6)
#define LIM_MLM_AUTH_CNF            (LIM_MLM_MSG_START + 7)
#define LIM_MLM_AUTH_IND            (LIM_MLM_MSG_START + 8)
#define LIM_MLM_ASSOC_REQ           (LIM_MLM_MSG_START + 9)
#define LIM_MLM_ASSOC_CNF           (LIM_MLM_MSG_START + 10)
#define LIM_MLM_ASSOC_IND           (LIM_MLM_MSG_START + 11)
#define LIM_MLM_DISASSOC_REQ        (LIM_MLM_MSG_START + 12)
#define LIM_MLM_DISASSOC_CNF        (LIM_MLM_MSG_START + 13)
#define LIM_MLM_DISASSOC_IND        (LIM_MLM_MSG_START + 14)
#define LIM_MLM_REASSOC_REQ         (LIM_MLM_MSG_START + 15)
#define LIM_MLM_REASSOC_CNF         (LIM_MLM_MSG_START + 16)
#define LIM_MLM_REASSOC_IND         (LIM_MLM_MSG_START + 17)
#define LIM_MLM_DEAUTH_REQ          (LIM_MLM_MSG_START + 18)
#define LIM_MLM_DEAUTH_CNF          (LIM_MLM_MSG_START + 19)
#define LIM_MLM_DEAUTH_IND          (LIM_MLM_MSG_START + 20)
#define LIM_MLM_TSPEC_REQ           (LIM_MLM_MSG_START + 21)
#define LIM_MLM_TSPEC_CNF           (LIM_MLM_MSG_START + 22)
#define LIM_MLM_TSPEC_IND           (LIM_MLM_MSG_START + 23)
#define LIM_MLM_SETKEYS_REQ         (LIM_MLM_MSG_START + 24)
#define LIM_MLM_SETKEYS_CNF         (LIM_MLM_MSG_START + 25)
#define LIM_MLM_LINK_TEST_STOP_REQ  (LIM_MLM_MSG_START + 30)
#define LIM_MLM_PURGE_STA_IND       (LIM_MLM_MSG_START + 31)
#define LIM_MLM_ADDBA_REQ           (LIM_MLM_MSG_START + 32)
#define LIM_MLM_ADDBA_CNF           (LIM_MLM_MSG_START + 33)
#define LIM_MLM_ADDBA_IND           (LIM_MLM_MSG_START + 34)
#define LIM_MLM_ADDBA_RSP           (LIM_MLM_MSG_START + 35)
#define LIM_MLM_DELBA_REQ           (LIM_MLM_MSG_START + 36)
#define LIM_MLM_DELBA_CNF           (LIM_MLM_MSG_START + 37)
#define LIM_MLM_DELBA_IND           (LIM_MLM_MSG_START + 38)
#define LIM_MLM_REMOVEKEY_REQ  (LIM_MLM_MSG_START + 39)
#define LIM_MLM_REMOVEKEY_CNF  (LIM_MLM_MSG_START + 40)

#ifdef FEATURE_OEM_DATA_SUPPORT
#define LIM_MLM_OEM_DATA_REQ            (LIM_MLM_MSG_START + 41)
#define LIM_MLM_OEM_DATA_CNF            (LIM_MLM_MSG_START + 42)
#endif

#define LIM_HASH_ADD            0
#define LIM_HASH_UPDATE         1

#define LIM_WEP_IN_FC           1
#define LIM_NO_WEP_IN_FC        0

#define LIM_DECRYPT_ICV_FAIL    1

/// Definitions to distinquish between Association/Reassociaton
#define LIM_ASSOC    0
#define LIM_REASSOC  1

/// Minimum Memory blocks require for different scenario
#define LIM_MIN_MEM_ASSOC       4

/// Verifies whether given mac addr matches the CURRENT Bssid
#define IS_CURRENT_BSSID(pMac, addr,psessionEntry)  (vos_mem_compare( addr, \
                                                                      psessionEntry->bssId, \
                                                                      sizeof(psessionEntry->bssId)))
/// Verifies whether given addr matches the REASSOC Bssid
#define IS_REASSOC_BSSID(pMac, addr,psessionEntry)  (vos_mem_compare( addr, \
                                                                      psessionEntry->limReAssocbssId, \
                                                                      sizeof(psessionEntry->limReAssocbssId)))

#define REQ_TYPE_REGISTRAR                   (0x2)
#define REQ_TYPE_WLAN_MANAGER_REGISTRAR      (0x3)

#define RESP_TYPE_REGISTRAR                  (0x2)
#define RESP_TYPE_ENROLLEE_INFO_ONLY         (0x0)
#define RESP_TYPE_ENROLLEE_OPEN_8021X        (0x1)
#define RESP_TYPE_AP                         (0x3)
#define LIM_TX_FRAMES_THRESHOLD_ON_CHIP       300


// enums used by LIM are as follows

enum eLimDisassocTrigger
{
    eLIM_HOST_DISASSOC,
    eLIM_PEER_ENTITY_DISASSOC,
    eLIM_LINK_MONITORING_DISASSOC,
    eLIM_PROMISCUOUS_MODE_DISASSOC,
    eLIM_HOST_DEAUTH,
    eLIM_PEER_ENTITY_DEAUTH,
    eLIM_LINK_MONITORING_DEAUTH,
    eLIM_JOIN_FAILURE,
    eLIM_REASSOC_REJECT
};

/* Reason code to determine the channel change context while sending 
 * WDA_CHNL_SWITCH_REQ message to HAL
 */
enum eChannelChangeReasonCodes
{
    LIM_SWITCH_CHANNEL_REASSOC,
    LIM_SWITCH_CHANNEL_JOIN,
    LIM_SWITCH_CHANNEL_OPERATION, // Generic change channel
    LIM_SWITCH_CHANNEL_CSA,
};

typedef struct sLimAuthRspTimeout
{
    tSirMacAddr    peerMacAddr;
} tLimAuthRspTimeout;

typedef struct sLimMlmStartReq
{
    tSirMacSSid           ssId;
    tSirBssType           bssType;
    tSirMacAddr           bssId;
    tSirMacBeaconInterval beaconPeriod;
    tANI_U8               dtimPeriod;
    tSirMacCfParamSet     cfParamSet;
    tSirMacChanNum        channelNumber;
    ePhyChanBondState     cbMode;
    tANI_U16              atimWindow;
    tSirMacRateSet        rateSet;
    tANI_U8               sessionId; //Added For BT-AMP Support   

    // Parameters reqd for new HAL (message) interface
    tSirNwType            nwType;
    tANI_U8               htCapable;
    tSirMacHTOperatingMode     htOperMode;
    tANI_U8                    dualCTSProtection;
    tANI_U8                    txChannelWidthSet;
    tANI_U8              ssidHidden;
    tANI_U8              wps_state;
    tANI_U8              obssProtEnabled;
} tLimMlmStartReq, *tpLimMlmStartReq;

typedef struct sLimMlmStartCnf
{
    tSirResultCodes resultCode;
    tANI_U8         sessionId;
} tLimMlmStartCnf, *tpLimMlmStartCnf;

typedef struct sLimMlmScanCnf
{
    tSirResultCodes         resultCode;
    tANI_U16                scanResultLength;
    tSirBssDescription      bssDescription[1];
    tANI_U8                 sessionId;
} tLimMlmScanCnf, *tpLimMlmScanCnf;

typedef struct sLimScanResult
{
    tANI_U16                numBssDescriptions;
    tSirBssDescription bssDescription[1];
} tLimScanResult;

typedef struct sLimMlmJoinCnf
{
    tSirResultCodes resultCode;
    tANI_U16 protStatusCode;
    tANI_U8  sessionId;
} tLimMlmJoinCnf, *tpLimMlmJoinCnf;

typedef struct sLimMlmAssocReq
{
    tSirMacAddr           peerMacAddr;
    tANI_U32                   assocFailureTimeout;
    tANI_U16                   capabilityInfo;
    tSirMacListenInterval listenInterval;
    tANI_U8             sessionId;
} tLimMlmAssocReq, *tpLimMlmAssocReq;

typedef struct sLimMlmAssocCnf
{
    tSirResultCodes resultCode; //Internal status code.
    tANI_U16 protStatusCode; //Protocol Status code.
    tANI_U8  sessionId;
} tLimMlmAssocCnf, *tpLimMlmAssocCnf;

typedef struct sLimMlmAssocInd
{
    tSirMacAddr          peerMacAddr;
    tANI_U16                  aid;
    tAniAuthType         authType;
    tAniSSID             ssId;
    tSirRSNie            rsnIE;
    tSirAddie            addIE; // additional IE received from the peer, which possibly includes WSC IE and/or P2P IE.
    tSirMacCapabilityInfo capabilityInfo;
    tAniBool                spectrumMgtIndicator;
    tSirMacPowerCapInfo     powerCap;
    tSirSupChnl             supportedChannels;
    tANI_U8                 sessionId;


    tAniBool               WmmStaInfoPresent;
#ifdef WLAN_FEATURE_AP_HT40_24G
    tAniBool               HT40MHzIntoPresent;
#endif

    // Required for indicating the frames to upper layer
    tANI_U32             beaconLength;
    tANI_U8*             beaconPtr;
    tANI_U32             assocReqLength;
    tANI_U8*             assocReqPtr;    
} tLimMlmAssocInd, *tpLimMlmAssocInd;

typedef struct sLimMlmReassocReq
{
    tSirMacAddr           peerMacAddr;
    tANI_U32                   reassocFailureTimeout;
    tANI_U16                   capabilityInfo;
    tSirMacListenInterval listenInterval;
    tANI_U8                sessionId; 
} tLimMlmReassocReq, *tpLimMlmReassocReq;

typedef struct sLimMlmReassocCnf
{
    tSirResultCodes resultCode;
    tANI_U16 protStatusCode; //Protocol Status code.
    tANI_U8  sessionId;
} tLimMlmReassocCnf, *tpLimMlmReassocCnf;

typedef struct sLimMlmReassocInd
{
    tSirMacAddr          peerMacAddr;
    tSirMacAddr          currentApAddr;
    tANI_U16             aid;
    tAniAuthType         authType;
    tAniSSID             ssId;
    tSirRSNie            rsnIE;
    tSirAddie            addIE; // additional IE received from the peer, which can be WSC IE and/or P2P IE.
    tSirMacCapabilityInfo capabilityInfo;
    tAniBool                spectrumMgtIndicator;
    tSirMacPowerCapInfo     powerCap;
    tSirSupChnl             supportedChannels;

    tAniBool               WmmStaInfoPresent;
#ifdef WLAN_FEATURE_AP_HT40_24G
    tAniBool               HT40MHzIntoPresent;
#endif

    // Required for indicating the frames to upper layer
    tANI_U32             beaconLength;
    tANI_U8*             beaconPtr;
    tANI_U32             assocReqLength;
    tANI_U8*             assocReqPtr;    
} tLimMlmReassocInd, *tpLimMlmReassocInd;

typedef struct sLimMlmAuthCnf
{
    tSirMacAddr     peerMacAddr;
    tAniAuthType    authType;
    tSirResultCodes resultCode;
    tANI_U16        protStatusCode;
    tANI_U8         sessionId;
} tLimMlmAuthCnf, *tpLimMlmAuthCnf;

typedef struct sLimMlmAuthInd
{
    tSirMacAddr    peerMacAddr;
    tAniAuthType   authType;
    tANI_U8        sessionId;
} tLimMlmAuthInd, *tpLimMlmAuthInd;

typedef struct sLimMlmDeauthReq
{
    tSirMacAddr peerMacAddr;
    tANI_U16         reasonCode;
    tANI_U16         deauthTrigger;
    tANI_U16         aid;
    tANI_U8         sessionId; //Added for BT-AMP SUPPORT
    
} tLimMlmDeauthReq, *tpLimMlmDeauthReq;

typedef struct sLimMlmDeauthCnf
{
    tSirMacAddr     peerMacAddr;
    tSirResultCodes resultCode;
    tANI_U16        deauthTrigger;
    tANI_U16        aid;
    tANI_U8         sessionId;
} tLimMlmDeauthCnf, *tpLimMLmDeauthCnf;

typedef struct sLimMlmDeauthInd
{
    tSirMacAddr peerMacAddr;
    tANI_U16         reasonCode;
    tANI_U16         deauthTrigger;
    tANI_U16         aid;
} tLimMlmDeauthInd, *tpLimMlmDeauthInd;

typedef struct sLimMlmDisassocReq
{
    tSirMacAddr peerMacAddr;
    tANI_U16         reasonCode;
    tANI_U16         disassocTrigger;
    tANI_U16         aid;
    tANI_U8         sessionId;
} tLimMlmDisassocReq, *tpLimMlmDisassocReq;

typedef struct sLimMlmDisassocCnf
{
    tSirMacAddr     peerMacAddr;
    tSirResultCodes resultCode;
    tANI_U16             disassocTrigger;
    tANI_U16             aid;
    tANI_U8         sessionId;
} tLimMlmDisassocCnf, *tpLimMlmDisassocCnf;

typedef struct sLimMlmDisassocInd
{
    tSirMacAddr     peerMacAddr;
    tANI_U16        reasonCode;
    tANI_U16        disassocTrigger;
    tANI_U16        aid;
    tANI_U8         sessionId;
} tLimMlmDisassocInd, *tpLimMlmDisassocInd;

typedef struct sLimMlmPurgeStaReq
{
    tSirMacAddr     peerMacAddr;
    tANI_U16        aid;
    tANI_U8         sessionId;//Added For BT-AMP Support
} tLimMlmPurgeStaReq, *tpLimMlmPurgeStaReq;

typedef struct sLimMlmPurgeStaInd
{
    tSirMacAddr     peerMacAddr;
    tANI_U16        reasonCode;
    tANI_U16        purgeTrigger;
    tANI_U16        aid;
    tANI_U8         sessionId;
} tLimMlmPurgeStaInd, *tpLimMlmPurgeStaInd;

typedef struct sLimMlmSetKeysReq
{
    tSirMacAddr     peerMacAddr;
    tANI_U8         sessionId;      //Added For BT-AMP Support
    tANI_U16        aid;
    tAniEdType      edType;    // Encryption/Decryption type
    tANI_U8         numKeys;
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
} tLimMlmSetKeysReq, *tpLimMlmSetKeysReq;

typedef struct sLimMlmSetKeysCnf
{
    tSirMacAddr     peerMacAddr;
    tANI_U16        resultCode;
    tANI_U16        aid;
    tANI_U8         sessionId;
} tLimMlmSetKeysCnf, *tpLimMlmSetKeysCnf;

typedef struct sLimMlmRemoveKeyReq
{
    tSirMacAddr     peerMacAddr;
    tANI_U8         sessionId; //Added FOr BT-AMP Support
    tAniEdType      edType;    // Encryption/Decryption type
    tANI_U8         wepType; //STATIC / DYNAMIC specifier
    tANI_U8         keyId; //Key Id To be removed.
    tANI_BOOLEAN    unicast;
} tLimMlmRemoveKeyReq, *tpLimMlmRemoveKeyReq;

typedef struct sLimMlmRemoveKeyCnf
{
    tSirMacAddr     peerMacAddr;
    tANI_U16        resultCode;
    tANI_U8         sessionId;
} tLimMlmRemoveKeyCnf, *tpLimMlmRemoveKeyCnf;


typedef struct sLimMlmResetReq
{
    tSirMacAddr macAddr;
    tANI_U8        performCleanup;
    tANI_U8       sessionId;
} tLimMlmResetReq, *tpLimMlmResetReq;

typedef struct sLimMlmResetCnf
{
    tSirMacAddr macAddr;
    tSirResultCodes resultCode;
    tANI_U8         sessionId;
} tLimMlmResetCnf, *tpLimMlmResetCnf;


typedef struct sLimMlmLinkTestStopReq
{
    tSirMacAddr    peerMacAddr;
    tANI_U8       sessionId;
} tLimMlmLinkTestStopReq, *tpLimMlmLinkTestStopReq;


//
// Block ACK related MLME data structures
//

typedef struct sLimMlmAddBAReq
{

  // ADDBA recipient
  tSirMacAddr peerMacAddr;

  // ADDBA Action Frame dialog token
  tANI_U8 baDialogToken;

  // ADDBA requested for TID
  tANI_U8 baTID;

  // BA policy
  // 0 - Delayed BA (Not supported)
  // 1 - Immediate BA
  tANI_U8 baPolicy;

  // BA buffer size - (0..127) max size MSDU's
  tANI_U16 baBufferSize;

  // BA timeout in TU's
  // 0 means no timeout will occur
  tANI_U16 baTimeout;

  // ADDBA failure timeout in TU's
  // Greater than or equal to 1
  tANI_U16 addBAFailureTimeout;

  // BA Starting Sequence Number
  tANI_U16 baSSN;

  tANI_U8       sessionId;

} tLimMlmAddBAReq, *tpLimMlmAddBAReq;

typedef struct sLimMlmAddBACnf
{

  // ADDBA recipient
  tSirMacAddr peerMacAddr;

  // ADDBA Action Frame dialog token
  tANI_U8 baDialogToken;

  // ADDBA requested for TID
  tANI_U8 baTID;

  // BA status code
  tSirMacStatusCodes addBAResultCode;

  // BA policy
  // 0 - Delayed BA (Not supported)
  // 1 - Immediate BA
  tANI_U8 baPolicy;

  // BA buffer size - (0..127) max size MSDU's
  tANI_U16 baBufferSize;

  // BA timeout in TU's
  // 0 means no timeout will occur
  tANI_U16 baTimeout;

  // ADDBA direction
  // 1 - Originator
  // 0 - Recipient
  tANI_U8 baDirection;
  tANI_U8 sessionId;


} tLimMlmAddBACnf, *tpLimMlmAddBACnf;

typedef struct sLimMlmAddBAInd
{

  // ADDBA recipient
  tSirMacAddr peerMacAddr;

  // ADDBA Action Frame dialog token
  tANI_U8 baDialogToken;

  // ADDBA requested for TID
  tANI_U8 baTID;

  // BA policy
  // 0 - Delayed BA (Not supported)
  // 1 - Immediate BA
  tANI_U8 baPolicy;

  // BA buffer size - (0..127) max size MSDU's
  tANI_U16 baBufferSize;

  // BA timeout in TU's
  // 0 means no timeout will occur
  tANI_U16 baTimeout;

} tLimMlmAddBAInd, *tpLimMlmAddBAInd;

typedef struct sLimMlmAddBARsp
{

  // ADDBA recipient
  tSirMacAddr peerMacAddr;

  // ADDBA Action Frame dialog token
  tANI_U8 baDialogToken;

  // ADDBA requested for TID
  tANI_U8 baTID;

  // BA status code
  tSirMacStatusCodes addBAResultCode;

  // BA policy
  // 0 - Delayed BA (Not supported)
  // 1 - Immediate BA
  tANI_U8 baPolicy;
  
  // BA buffer size - (0..127) max size MSDU's
  tANI_U16 baBufferSize;

  // BA timeout in TU's
  // 0 means no timeout will occur
  tANI_U16 baTimeout;

  //reserved for alignment
  tANI_U8 rsvd[2];

  /* PE session id*/  
  tANI_U8    sessionId;

 } tLimMlmAddBARsp, *tpLimMlmAddBARsp;

//
// NOTE - Overloading DELBA IND and DELBA CNF
// to use the same data structure as DELBA REQ
// as the parameters do not vary too much.
//
typedef struct sLimMlmDelBAReq
{

  // ADDBA recipient
  tSirMacAddr peerMacAddr;

  // DELBA direction
  // 1 - Originator
  // 0 - Recipient
  tANI_U8 baDirection;

  // DELBA requested for TID
  tANI_U8 baTID;

  // DELBA reason code
  tSirMacReasonCodes delBAReasonCode;

  tANI_U8       sessionId;

} tLimMlmDelBAReq, *tpLimMlmDelBAReq, tLimMlmDelBAInd, *tpLimMlmDelBAInd, tLimMlmDelBACnf, *tpLimMlmDelBACnf;

// Function templates

tANI_BOOLEAN limProcessSmeReqMessages(tpAniSirGlobal, tpSirMsgQ);
void limProcessMlmReqMessages(tpAniSirGlobal, tpSirMsgQ);
void limProcessMlmRspMessages(tpAniSirGlobal, tANI_U32, tANI_U32 *);
void limProcessLmmMessages(tpAniSirGlobal, tANI_U32, tANI_U32 *);
void limProcessSmeDelBssRsp( tpAniSirGlobal , tANI_U32,tpPESession);

void limGetRandomBssid(tpAniSirGlobal pMac ,tANI_U8 *data);

// Function to handle HT and HT IE CFG parameter intializations
void handleHTCapabilityandHTInfo(struct sAniSirGlobal *pMac, tpPESession psessionEntry);

// Function to handle CFG parameter updates
void limHandleCFGparamUpdate(tpAniSirGlobal, tANI_U32);

// Function to apply CFG parameters before join/reassoc/start BSS
void limApplyConfiguration(tpAniSirGlobal,tpPESession);

void limSetCfgProtection(tpAniSirGlobal pMac, tpPESession pesessionEntry);


// Function to Initialize MLM state machine on STA
void limInitMlm(tpAniSirGlobal);

// Function to cleanup MLM state machine
void limCleanupMlm(tpAniSirGlobal);

// Function to cleanup LMM state machine
void limCleanupLmm(tpAniSirGlobal);

// Management frame handling functions
void limProcessBeaconFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
void limProcessBeaconFrameNoSession(tpAniSirGlobal, tANI_U8 *);
void limProcessProbeReqFrame(tpAniSirGlobal, tANI_U8 *, tpPESession);
void limProcessProbeRspFrame(tpAniSirGlobal, tANI_U8 *, tpPESession);
void limProcessProbeRspFrameNoSession(tpAniSirGlobal, tANI_U8 *);
void limProcessProbeReqFrame_multiple_BSS(tpAniSirGlobal, tANI_U8 *,tpPESession);


// Process Auth frame when we have a session in progress.
void limProcessAuthFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
#ifdef WLAN_FEATURE_VOWIFI_11R
tSirRetStatus limProcessAuthFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *, void *body);
#endif

void limProcessAssocReqFrame(tpAniSirGlobal, tANI_U8 *, tANI_U8, tpPESession);
void limSendMlmAssocInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry);


void limProcessAssocRspFrame(tpAniSirGlobal, tANI_U8 *, tANI_U8,tpPESession);
void limProcessDisassocFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
void limProcessDeauthFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
void limProcessActionFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
void limProcessActionFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *pRxMetaInfo);


tSirRetStatus limPopulateMacHeader(tpAniSirGlobal, tANI_U8*, tANI_U8, tANI_U8, tSirMacAddr,tSirMacAddr);
tSirRetStatus limSendProbeReqMgmtFrame(tpAniSirGlobal, tSirMacSSid *, tSirMacAddr, tANI_U8, tSirMacAddr, tANI_U32, tANI_U32, tANI_U8 *); 
void limSendProbeRspMgmtFrame(tpAniSirGlobal, tSirMacAddr, tpAniSSID, short, tANI_U8, tpPESession, tANI_U8);
void limSendAuthMgmtFrame(tpAniSirGlobal, tSirMacAuthFrameBody *, tSirMacAddr,
                                             tANI_U8, tpPESession , tAniBool);
void limSendAssocReqMgmtFrame(tpAniSirGlobal, tLimMlmAssocReq *,tpPESession);
void limSendReassocReqMgmtFrame(tpAniSirGlobal, tLimMlmReassocReq *,tpPESession);
#ifdef WLAN_FEATURE_VOWIFI_11R
void limSendReassocReqWithFTIEsMgmtFrame(tpAniSirGlobal     pMac,
                           tLimMlmReassocReq *pMlmReassocReq,tpPESession psessionEntry);
#endif
void limSendDeltsReqActionFrame(tpAniSirGlobal pMac, tSirMacAddr  peer,
                           tANI_U8  wmmTspecPresent, tSirMacTSInfo  *pTsinfo,
                           tSirMacTspecIE  *pTspecIe, tpPESession psessionEntry);
void limSendAddtsReqActionFrame(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,
                          tSirAddtsReqInfo *addts,tpPESession);
void limSendAddtsRspActionFrame(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,
                           tANI_U16 statusCode, tSirAddtsReqInfo *addts, tSirMacScheduleIE *pSchedule,tpPESession);

void limSendAssocRspMgmtFrame(tpAniSirGlobal, tANI_U16, tANI_U16, tSirMacAddr, tANI_U8, tpDphHashNode pSta,tpPESession);

void limSendNullDataFrame(tpAniSirGlobal, tpDphHashNode);
void limSendDisassocMgmtFrame(tpAniSirGlobal, tANI_U16, tSirMacAddr, tpPESession, tANI_BOOLEAN waitForAck);
void limSendDeauthMgmtFrame(tpAniSirGlobal, tANI_U16, tSirMacAddr, tpPESession, tANI_BOOLEAN waitForAck);
void limSendSmeDisassocDeauthNtf( tpAniSirGlobal pMac,
                                eHalStatus status, tANI_U32 *pCtx );


void limContinueChannelScan(tpAniSirGlobal);
tSirResultCodes limMlmAddBss(tpAniSirGlobal, tLimMlmStartReq *,tpPESession psessionEntry);

tSirRetStatus limSendChannelSwitchMgmtFrame(tpAniSirGlobal, tSirMacAddr, tANI_U8, tANI_U8, tANI_U8, tpPESession);

#ifdef WLAN_FEATURE_11AC
tSirRetStatus limSendVHTOpmodeNotificationFrame(tpAniSirGlobal pMac,tSirMacAddr peer,tANI_U8 nMode, tpPESession  psessionEntry );
tSirRetStatus limSendVHTChannelSwitchMgmtFrame(tpAniSirGlobal pMac,tSirMacAddr peer,tANI_U8 nChanWidth, tANI_U8 nNewChannel, tANI_U8 ncbMode, tpPESession  psessionEntry );
#endif

#if defined WLAN_FEATURE_VOWIFI
tSirRetStatus limSendNeighborReportRequestFrame(tpAniSirGlobal, tpSirMacNeighborReportReq, tSirMacAddr, tpPESession);
tSirRetStatus limSendLinkReportActionFrame(tpAniSirGlobal, tpSirMacLinkReport, tSirMacAddr, tpPESession ); 
tSirRetStatus limSendRadioMeasureReportActionFrame(tpAniSirGlobal, tANI_U8, tANI_U8, tpSirMacRadioMeasureReport, tSirMacAddr, tpPESession);
#endif

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
void limProcessIappFrame(tpAniSirGlobal, tANI_U8 *,tpPESession);
#endif

#ifdef FEATURE_WLAN_TDLS
void limInitTdlsData(tpAniSirGlobal, tpPESession);
tSirRetStatus limProcessSmeTdlsMgmtSendReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf);
tSirRetStatus limProcessSmeTdlsAddStaReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf);
tSirRetStatus limProcesSmeTdlsLinkEstablishReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf);
tSirRetStatus limProcessSmeTdlsDelStaReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf);
void limSendSmeTDLSDeleteAllPeerInd(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limSendSmeMgmtTXCompletion(tpAniSirGlobal pMac,
                                tANI_U32 smeSessionId,
                                tANI_U32 txCompleteStatus);
tSirRetStatus limDeleteTDLSPeers(tpAniSirGlobal pMac, tpPESession psessionEntry);
eHalStatus limProcessTdlsAddStaRsp(tpAniSirGlobal pMac, void *msg, tpPESession);
tSirRetStatus limSendTdlsTeardownFrame(tpAniSirGlobal pMac,
           tSirMacAddr peerMac, tANI_U16 reason, tANI_U8 responder, tpPESession psessionEntry,
           tANI_U8 *addIe, tANI_U16 addIeLen);
// tdlsoffchan
tSirRetStatus limProcesSmeTdlsChanSwitchReq(tpAniSirGlobal pMac,
                                            tANI_U32 *pMsgBuf);
#endif

// Algorithms & Link Monitoring related functions
tSirBackgroundScanMode limSelectsBackgroundScanMode(tpAniSirGlobal);
void limTriggerBackgroundScan(tpAniSirGlobal);
void limAbortBackgroundScan(tpAniSirGlobal);

/// Function that handles heartbeat failure
void limHandleHeartBeatFailure(tpAniSirGlobal,tpPESession);

/// Function that triggers link tear down with AP upon HB failure
void limTearDownLinkWithAp(tpAniSirGlobal,tANI_U8, tSirMacReasonCodes);


/// Function that sends keep alive message to peer(s)
void limSendKeepAliveToPeer(tpAniSirGlobal);

/// Function that processes Max retries interrupt from TFP
void limHandleMaxRetriesInterrupt(tANI_U32);

/// Function that processes messages deferred during Learn mode
void limProcessDeferredMessageQueue(tpAniSirGlobal);

/// Function that defers the messages received
tANI_U32 limDeferMsg(tpAniSirGlobal, tSirMsgQ *);

/// Function that sets system into scan mode
void limSetScanMode(tpAniSirGlobal pMac);

/// Function that Switches the Channel and sets the CB Mode 
void limSetChannel(tpAniSirGlobal pMac, tANI_U8 channel, tANI_U8 secChannelOffset, tPowerdBm maxTxPower, tANI_U8 peSessionId);

/// Function that completes channel scan
void limCompleteMlmScan(tpAniSirGlobal, tSirResultCodes);

#ifdef FEATURE_OEM_DATA_SUPPORT
/// Funtion that sets system into meas mode for oem data req
void limSetOemDataReqMode(tpAniSirGlobal pMac, eHalStatus status, tANI_U32* data);
#endif

#ifdef ANI_SUPPORT_11H
/// Function that sends Measurement Report action frame
tSirRetStatus limSendMeasReportFrame(tpAniSirGlobal, tpSirMacMeasReqActionFrame, tSirMacAddr);

/// Function that sends TPC Report action frame
tSirRetStatus limSendTpcReportFrame(tpAniSirGlobal, tpSirMacTpcReqActionFrame, tSirMacAddr);
#endif

/// Function that sends TPC Request action frame
void limSendTpcRequestFrame(tpAniSirGlobal, tSirMacAddr);

// Function(s) to handle responses received from HAL
void limProcessMlmAddBssRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ );
void limProcessMlmAddStaRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQt,tpPESession psessionEntry);
void limProcessMlmDelStaRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ );
void limProcessMlmDelBssRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ,tpPESession);
void limProcessStaMlmAddStaRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ ,tpPESession psessionEntry);
void limProcessStaMlmDelStaRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ,tpPESession psessionEntry);
void limProcessStaMlmDelBssRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ ,tpPESession psessionEntry);
void limProcessMlmSetStaKeyRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ );
void limProcessMlmSetBssKeyRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ );



#ifdef GEN4_SCAN
// Function to process WDA_INIT_SCAN_RSP message
void limProcessInitScanRsp(tpAniSirGlobal,  void * );

// Function to process WDA_START_SCAN_RSP message
void limProcessStartScanRsp(tpAniSirGlobal,  void * );

// Function to process WDA_END_SCAN_RSP message
void limProcessEndScanRsp(tpAniSirGlobal, void * );

// Function to process WDA_FINISH_SCAN_RSP message
void limProcessFinishScanRsp(tpAniSirGlobal,  void * );

// Function to process WDA_SWITCH_CHANNEL_RSP message
void limProcessSwitchChannelRsp(tpAniSirGlobal pMac,  void * );
  
void limSendHalInitScanReq( tpAniSirGlobal, tLimLimHalScanState, tSirLinkTrafficCheck);
void limSendHalStartScanReq( tpAniSirGlobal, tANI_U8, tLimLimHalScanState);
void limSendHalEndScanReq( tpAniSirGlobal, tANI_U8, tLimLimHalScanState);
void limSendHalFinishScanReq( tpAniSirGlobal, tLimLimHalScanState);

void limContinuePostChannelScan(tpAniSirGlobal pMac);
void limCovertChannelScanType(tpAniSirGlobal pMac,tANI_U8 channelNum, tANI_BOOLEAN passiveToActive);
void limSetDFSChannelList(tpAniSirGlobal pMac,tANI_U8 channelNum, tSirDFSChannelList *dfsChannelList);
void limContinueChannelLearn( tpAniSirGlobal );
//WLAN_SUSPEND_LINK Related
tANI_U8 limIsLinkSuspended(tpAniSirGlobal pMac);
void limSuspendLink(tpAniSirGlobal, tSirLinkTrafficCheck, SUSPEND_RESUME_LINK_CALLBACK, tANI_U32*);
void limResumeLink(tpAniSirGlobal, SUSPEND_RESUME_LINK_CALLBACK, tANI_U32*);
//end WLAN_SUSPEND_LINK Related
#endif // GEN4_SCAN

tSirRetStatus limSendAddBAReq( tpAniSirGlobal pMac,
    tpLimMlmAddBAReq pMlmAddBAReq,tpPESession);

tSirRetStatus limSendAddBARsp( tpAniSirGlobal pMac,
    tpLimMlmAddBARsp pMlmAddBARsp,tpPESession);

tSirRetStatus limSendDelBAInd( tpAniSirGlobal pMac,
    tpLimMlmDelBAReq pMlmDelBAReq ,tpPESession psessionEntry);
#if 0
tSirRetStatus limSendSMPowerStateFrame( tpAniSirGlobal pMac, 
      tSirMacAddr peer, tSirMacHTMIMOPowerSaveState State );
#endif

void limProcessMlmHalAddBARsp( tpAniSirGlobal pMac,
    tpSirMsgQ limMsgQ );

void limProcessMlmHalBADeleteInd( tpAniSirGlobal pMac,
    tpSirMsgQ limMsgQ );

void limProcessMlmRemoveKeyRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ );

void limProcessLearnIntervalTimeout(tpAniSirGlobal pMac);

#ifdef WLAN_FEATURE_11W
//11w send SA query request action frame
tSirRetStatus limSendSaQueryRequestFrame( tpAniSirGlobal pMac, tANI_U8 *transId,
                                          tSirMacAddr peer, tpPESession psessionEntry );
//11w SA query request action frame handler
tSirRetStatus limSendSaQueryResponseFrame( tpAniSirGlobal pMac, 
                   tANI_U8 *transId, tSirMacAddr peer,tpPESession psessionEntry);
#endif
// Inline functions

/**
 * limPostSmeMessage()
 *
 *FUNCTION:
 * This function is called by limProcessMlmMessages(). In this
 * function MLM sub-module invokes MLM ind/cnf primitives.
 *
 *LOGIC:
 * Initially MLM makes an SME function call to invoke MLM ind/cnf
 * primitive. In future this can be enhanced to 'post' messages to SME.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac      Pointer to Global MAC structure
 * @param msgType   Indicates the MLM primitive message type
 * @param *pMsgBuf  A pointer to the MLM message buffer
 *
 * @return None
 */
static inline void
limPostSmeMessage(tpAniSirGlobal pMac, tANI_U32 msgType, tANI_U32 *pMsgBuf)
{
     tSirMsgQ msg;
    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL"));
           return;
    }
      
    msg.type = (tANI_U16)msgType;
    msg.bodyptr = pMsgBuf;
    msg.bodyval = 0;
    if (msgType > eWNI_SME_MSG_TYPES_BEGIN)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TX_SME_MSG, NO_SESSION, msg.type));
        limProcessSmeReqMessages(pMac, &msg);
    }
    else
        limProcessMlmRspMessages(pMac, msgType, pMsgBuf);
} /*** end limPostSmeMessage() ***/

/**
 * limPostMlmMessage()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages(). In this
 * function SME invokes MLME primitives.
 *
 *PARAMS:
 *
 *LOGIC:
 * Initially SME makes an MLM function call to invoke MLM primitive.
 * In future this can be enhanced to 'post' messages to MLM.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac      Pointer to Global MAC structure
 * @param msgType   Indicates the MLM primitive message type
 * @param *pMsgBuf  A pointer to the MLM message buffer
 *
 * @return None
 */
static inline void
limPostMlmMessage(tpAniSirGlobal pMac, tANI_U32 msgType, tANI_U32 *pMsgBuf)
{

    tSirMsgQ msg;
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL"));
           return;
    }
    msg.type = (tANI_U16) msgType;
    msg.bodyptr = pMsgBuf;
    msg.bodyval = 0;
    MTRACE(macTraceMsgRx(pMac, NO_SESSION, msg.type));
    limProcessMlmReqMessages(pMac, &msg);
} /*** end limPostMlmMessage() ***/



/**
 * limGetCurrentScanChannel()
 *
 *FUNCTION:
 * This function is called in various places to get current channel
 * number being scanned.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return Channel number
 */
static inline tANI_U8
limGetCurrentScanChannel(tpAniSirGlobal pMac)
{
    tANI_U8 *pChanNum = pMac->lim.gpLimMlmScanReq->channelList.channelNumber;

    return (*(pChanNum + pMac->lim.gLimCurrentScanChannelId));
} /*** end limGetCurrentScanChannel() ***/



/**
 * limGetIElenFromBssDescription()
 *
 *FUNCTION:
 * This function is called in various places to get IE length
 * from tSirBssDescription structure
 * number being scanned.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param     pBssDescr
 * @return    Total IE length
 */

static inline tANI_U16
limGetIElenFromBssDescription(tpSirBssDescription pBssDescr)
{
    if (!pBssDescr)
        return 0;

    return ((tANI_U16) (pBssDescr->length + sizeof(tANI_U16) +
                   sizeof(tANI_U32) - sizeof(tSirBssDescription)));
} /*** end limGetIElenFromBssDescription() ***/

/**
 * limSendBeaconInd()
 *
 *FUNCTION:
 * This function is called  to send the beacon indication
 * number being scanned.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
*/

void 
limSendBeaconInd(tpAniSirGlobal pMac, tpPESession psessionEntry);


void limGetWPSPBCSessions(tpAniSirGlobal pMac, tANI_U8 *addr, tANI_U8 *uuid_e, eWPSPBCOverlap *overlap, tpPESession psessionEntry);
void limWPSPBCTimeout(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limWPSPBCClose(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limRemovePBCSessions(tpAniSirGlobal pMac, tSirMacAddr pRemoveMac,tpPESession psessionEntry);


tSirRetStatus
limIsSmeGetWPSPBCSessionsReqValid(tpAniSirGlobal pMac, tSirSmeGetWPSPBCSessionsReq *pGetWPSPBCSessionsReq, tANI_U8 *pBuf);

#define LIM_WPS_OVERLAP_TIMER_MS                 10000

void
limSuspendLink(tpAniSirGlobal pMac, tSirLinkTrafficCheck trafficCheck,  SUSPEND_RESUME_LINK_CALLBACK callback, tANI_U32 *data);
void
limResumeLink(tpAniSirGlobal pMac, SUSPEND_RESUME_LINK_CALLBACK callback, tANI_U32 *data);

void
limChangeChannelWithCallback(tpAniSirGlobal pMac, tANI_U8 newChannel, 
   CHANGE_CHANNEL_CALLBACK callback, tANI_U32 *cbdata, tpPESession psessionEntry);

void limSendSmeMgmtFrameInd(
                    tpAniSirGlobal pMac, tANI_U16 sessionId,
                    tANI_U8 *pRxPacketInfo,
                    tpPESession psessionEntry, tANI_S8 rxRssi);
void limProcessRemainOnChnTimeout(tpAniSirGlobal pMac);
void limProcessInsertSingleShotNOATimeout(tpAniSirGlobal pMac);
void limConvertActiveChannelToPassiveChannel(tpAniSirGlobal pMac);
void limSendP2PActionFrame(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
void limAbortRemainOnChan(tpAniSirGlobal pMac);
tSirRetStatus __limProcessSmeNoAUpdate(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf);
void limProcessRegdDefdSmeReqAfterNOAStart(tpAniSirGlobal pMac);
void limProcessDisassocAckTimeout(tpAniSirGlobal pMac);
void limProcessDeauthAckTimeout(tpAniSirGlobal pMac);
eHalStatus limSendDisassocCnf(tpAniSirGlobal pMac);
eHalStatus limSendDeauthCnf(tpAniSirGlobal pMac);

#ifdef WLAN_FEATURE_VOWIFI_11R
typedef struct sSetLinkCbackParams
{
    void * cbackDataPtr;
} tSetLinkCbackParams;
#endif

void limProcessRxScanEvent(tpAniSirGlobal mac, void *buf);

int limProcessRemainOnChnlReq(tpAniSirGlobal pMac, tANI_U32 *pMsg);
void limRemainOnChnRsp(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data);
void limProcessMlmSpoofMacAddrRsp(tpAniSirGlobal pMac, tSirRetStatus rspStatus);
tSirRetStatus limProcessSmeSetTdls2040BSSCoexReq(tpAniSirGlobal pMac,
                                                 tANI_U32 *pMsgBuf);
tSirRetStatus limProcessSmeDelAllTdlsPeers(tpAniSirGlobal pMac,
                                           tANI_U32 *pMsgBuf);
#endif /* __LIM_TYPES_H */

