/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
 * aniGlobal.h: MAC Modules Adapter Definitions.
 * Author:      V. K. Kandarpa
 * Date:    10/25/2002
 *
 * History:-
 * Date: 04/08/2008     Modified by: Santosh Mandiganal         
 * Modification Information: Added the logDump.h header file and defined the 
 *                        dumpTablecurrentId, dumpTableEntry.
 * --------------------------------------------------------------------------
 *
 */

#ifndef _ANIGLOBAL_H
#define _ANIGLOBAL_H

// Take care to avoid redefinition of this type, if it is
// already defined in "halWmmApi.h"
#if !defined(_HALMAC_WMM_API_H)
typedef struct sAniSirGlobal *tpAniSirGlobal;
#endif

#include "halTypes.h"
#include "sirCommon.h"
#include "aniSystemDefs.h"
#include "sysDef.h"
#include "dphGlobal.h"
#include "limGlobal.h"
#include "pmmGlobal.h"
#include "schGlobal.h"
#include "sysGlobal.h"
#include "cfgGlobal.h"
#include "utilsGlobal.h"
#include "sirApi.h"


#include "wlan_qct_hal.h"

#include "pmc.h"

#include "csrApi.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "sme_FTApi.h"
#endif
#include "csrSupport.h"
#include "smeInternal.h"
#include "ccmApi.h"
#include "btcApi.h"
#include "csrInternal.h"

#ifdef FEATURE_OEM_DATA_SUPPORT
#include "oemDataInternal.h" 
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "smeRrmInternal.h"
#include "rrmGlobal.h"
#endif
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#include "eseGlobal.h"
#endif
#include "p2p_Api.h"

#if defined WLAN_FEATURE_VOWIFI_11R
#include <limFTDefs.h>
#endif


#ifdef ANI_DVT_DEBUG
#include "dvtModule.h"
#endif

// New HAL API interface defs.
#include "logDump.h"

//Check if this definition can actually move here from halInternal.h even for Volans. In that case
//this featurization can be removed.
#define PMAC_STRUCT( _hHal )  (  (tpAniSirGlobal)_hHal )

#define ANI_DRIVER_TYPE(pMac)     (((tpAniSirGlobal)(pMac))->gDriverType)
// -------------------------------------------------------------------
// Bss Qos Caps bit map definition
#define LIM_BSS_CAPS_OFFSET_HCF 0
#define LIM_BSS_CAPS_OFFSET_WME 1
#define LIM_BSS_CAPS_OFFSET_WSM 2

#define LIM_BSS_CAPS_HCF (1 << LIM_BSS_CAPS_OFFSET_HCF)
#define LIM_BSS_CAPS_WME (1 << LIM_BSS_CAPS_OFFSET_WME)
#define LIM_BSS_CAPS_WSM (1 << LIM_BSS_CAPS_OFFSET_WSM)

// cap should be one of HCF/WME/WSM
#define LIM_BSS_CAPS_GET(cap, val) (((val) & (LIM_BSS_CAPS_ ## cap)) >> LIM_BSS_CAPS_OFFSET_ ## cap)
#define LIM_BSS_CAPS_SET(cap, val) ((val) |= (LIM_BSS_CAPS_ ## cap ))
#define LIM_BSS_CAPS_CLR(cap, val) ((val) &= (~ (LIM_BSS_CAPS_ ## cap)))

// 40 beacons per heart beat interval is the default + 1 to count the rest
#define MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL 41

/* max number of legacy bssid we can store during scan on one channel */
#define MAX_NUM_LEGACY_BSSID_PER_CHANNEL    10

#define P2P_WILDCARD_SSID "DIRECT-" //TODO Put it in proper place;
#define P2P_WILDCARD_SSID_LEN 7

#ifdef WLAN_FEATURE_CONCURRENT_P2P
#define MAX_NO_OF_P2P_SESSIONS  5
#endif //WLAN_FEATURE_CONCURRENT_P2P

#define SPACE_ASCII_VALUE  32

#ifdef FEATURE_WLAN_BATCH_SCAN
#define EQUALS_TO_ASCII_VALUE (61)
#endif

// -------------------------------------------------------------------
// Change channel generic scheme
typedef void (*CHANGE_CHANNEL_CALLBACK)(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data,
    tpPESession psessionEntry);

/// LIM global definitions
typedef struct sAniSirLimIbss
{
    void *pHdr;
    void *pBeacon;
} tAniSirLimIbss;

typedef struct sDialogueToken
{
    //bytes 0-3
    tANI_U16 assocId;
    tANI_U8 token;
    tANI_U8 rsvd1;
    //Bytes 4-7
    tANI_U16 tid;
    tANI_U8 rsvd2[2];

    struct sDialogueToken* next;
}tDialogueToken, *tpDialogueToken;

typedef struct sLimTimers
{
    //TIMERS IN LIM ARE NOT SUPPOSED TO BE ZEROED OUT DURING RESET.
    //DURING limInitialize DONOT ZERO THEM OUT.

//STA SPECIFIC TIMERS
    // Periodic background scan timer
    TX_TIMER   gLimBackgroundScanTimer;

    TX_TIMER    gLimPreAuthClnupTimer;
    //TX_TIMER    gLimAuthResponseTimer[HAL_NUM_STA];

    // Association related timers
    TX_TIMER    gLimAssocFailureTimer;
    TX_TIMER    gLimReassocFailureTimer;


    /// Heartbeat timer on STA
    TX_TIMER    gLimHeartBeatTimer;

    /// Wait for Probe after Heartbeat failure timer on STA
    TX_TIMER    gLimProbeAfterHBTimer;


    // Authentication related timers
    TX_TIMER            gLimAuthFailureTimer;

    // Join Failure timeout on STA
    TX_TIMER              gLimJoinFailureTimer;

    // Keepalive timer
    TX_TIMER    gLimKeepaliveTimer;

    // Scan related timers
    TX_TIMER    gLimMinChannelTimer;
    TX_TIMER    gLimMaxChannelTimer;
    TX_TIMER    gLimPeriodicProbeReqTimer;

    // CNF_WAIT timer
    TX_TIMER            *gpLimCnfWaitTimer;

    TX_TIMER       gLimAddtsRspTimer;   // max wait for a response

    // Update OLBC Cache Timer
    TX_TIMER    gLimUpdateOlbcCacheTimer;

    TX_TIMER           gLimChannelSwitchTimer;
    // This TIMER is started on the STA, as indicated by the
    // AP in its Quiet BSS IE, for the specified interval
    TX_TIMER           gLimQuietTimer;
    // This TIMER is started on the AP, prior to the AP going
    // into LEARN mode
    // This TIMER is started on the STA, for the specified
    // quiet duration
    TX_TIMER           gLimQuietBssTimer;

#ifdef WLAN_FEATURE_VOWIFI_11R
    TX_TIMER           gLimFTPreAuthRspTimer;
#endif

#ifdef FEATURE_WLAN_ESE
    TX_TIMER           gLimEseTsmTimer;
#endif
    TX_TIMER           gLimPeriodicJoinProbeReqTimer;
    TX_TIMER           gLimDisassocAckTimer;
    TX_TIMER           gLimDeauthAckTimer;
    TX_TIMER           gLimPeriodicAuthRetryTimer;
    // This timer is started when single shot NOA insert msg is sent to FW for scan in P2P GO mode
    TX_TIMER           gLimP2pSingleShotNoaInsertTimer;
    /* This timer is used to convert active channel to
     * passive channel when there is no beacon
     * for a period of time on a particular DFS channel
     */
    TX_TIMER           gLimActiveToPassiveChannelTimer;
//********************TIMER SECTION ENDS**************************************************
// ALL THE FIELDS BELOW THIS CAN BE ZEROED OUT in limInitialize
//****************************************************************************************

}tLimTimers;

typedef struct {
    void *pMlmDisassocReq;
    void *pMlmDeauthReq;
}tLimDisassocDeauthCnfReq;

typedef struct sAniSirLim
{
    //////////////////////////////////////     TIMER RELATED START ///////////////////////////////////////////

    tLimTimers limTimers;
    /// Flag to track if LIM timers are created or not
    tANI_U32   gLimTimersCreated;


    //////////////////////////////////////     TIMER RELATED END ///////////////////////////////////////////

    //////////////////////////////////////     SCAN/LEARN RELATED START ///////////////////////////////////////////
    /**
     * This flag when set, will use scan mode instead of
     * Learn mode on BP/AP. By default this flag is set
     * to true until HIF getting stuck in 0x800 state is
     * debugged.
     */
    tANI_U32     gLimUseScanModeForLearnMode;

    /**
     * This is useful for modules other than LIM
     * to see if system is in scan/learn mode or not
     */
    tANI_U32    gLimSystemInScanLearnMode;

    // Scan related globals on STA
    tANI_U8               gLimReturnAfterFirstMatch;
    tANI_U8               gLim24Band11dScanDone;
    tANI_U8               gLim50Band11dScanDone;
    tANI_U8               gLimReturnUniqueResults;

    // Background Scan related globals on STA
    tANI_U32               gLimNumOfBackgroundScanSuccess;
    tANI_U32               gLimNumOfConsecutiveBkgndScanFailure;
    tANI_U32               gLimNumOfForcedBkgndScan;
    tANI_U8                gLimBackgroundScanDisable;      //based on BG timer
    tANI_U8                gLimForceBackgroundScanDisable; //debug control flag
    tANI_U8                gLimBackgroundScanTerminate;    //controlled by SME
    tANI_U8                gLimReportBackgroundScanResults;//controlled by SME    

    /// Place holder for current channel ID
    /// being scanned
    tANI_U32   gLimCurrentScanChannelId;

    // Hold onto SCAN criteria
    /* The below is used in P2P GO case when we need to defer processing SME Req
     * to LIM and insert NOA first and process SME req once SNOA is started
     */
    tANI_U16 gDeferMsgTypeForNOA;
    tANI_U32 *gpDefdSmeMsgForNOA;

    tLimMlmScanReq *gpLimMlmScanReq;

    /// This indicates total length of 'matched' scan results
    tANI_U16   gLimMlmScanResultLength;

    /// This indicates total length of 'cached' scan results
    tANI_U16   gLimSmeScanResultLength;

    /**
     * Hash table definition for storing SCAN results
     * This is the placed holder for 'cached' scan results
     */
    tLimScanResultNode
           *gLimCachedScanHashTable[LIM_MAX_NUM_OF_SCAN_RESULTS];

    /// This indicates total length of 'matched' scan results
    tANI_U16   gLimMlmLfrScanResultLength;

    /// This indicates total length of 'cached' scan results
    tANI_U16   gLimSmeLfrScanResultLength;

    /**
     * Hash table definition for storing LFR SCAN results
     * This is the placed holder for roaming candidates as forwarded
     * by FW
     */
    tLimScanResultNode
        *gLimCachedLfrScanHashTable[LIM_MAX_NUM_OF_SCAN_RESULTS];

    /// Place holder for current channel ID
    /// being scanned during background scanning
    tANI_U32   gLimBackgroundScanChannelId;
    /// flag to indicate that bacground scan timer has been started
    tANI_U8    gLimBackgroundScanStarted;

    /* Used to store the list of legacy bss sta detected during scan on one channel */
    tANI_U16    gLimRestoreCBNumScanInterval;
    tANI_U16    gLimRestoreCBCount;
    tSirMacAddr gLimLegacyBssidList[MAX_NUM_LEGACY_BSSID_PER_CHANNEL];

    //
    // If this flag is 1,
    //   then, LIM will "try and trigger" a background
    //   scan whenever it receives a Quiet BSS IE
    //
    // If this flag is 0,
    //   then, LIM will simply shut-off Tx/Rx whenever it
    //   receives a Quiet BSS IE.
    //   This is the default behavior when a Quiet BSS IE
    //   is received and 11H is enabled
    //
    tANI_U32 gLimTriggerBackgroundScanDuringQuietBss;


    // This variable store the total duration to do scan
    tANI_U32 gTotalScanDuration;
    tANI_U32 p2pRemOnChanTimeStamp;

    // abort scan is used to abort an on-going scan
    tANI_U8 abortScan;
    tLimScanChnInfo scanChnInfo;

    //////////////////////////////////////     SCAN/LEARN RELATED START ///////////////////////////////////////////
    tSirMacAddr         gSelfMacAddr;   //added for BT-AMP Support 
    tSirMacAddr         spoofMacAddr;   //added for Mac Addr Spoofing support
    tANI_U8             isSpoofingEnabled;

    //////////////////////////////////////////     BSS RELATED END ///////////////////////////////////////////
    // Place holder for StartBssReq message
    // received by SME state machine

    tANI_U8             gLimCurrentBssUapsd;

    /* This is used for testing sta legacy bss detect feature */
    tANI_U8     gLimForceNoPropIE;

    //
    // Store the BSS Index returned by HAL during
    // WDA_ADD_BSS_RSP here.
    //
    // For now:
    // This will be used during WDA_SET_BSSKEY_REQ in
    // order to set the GTK
    // Later:
    // There could be other interfaces needing this info
    //

    //
    // Due to the asynchronous nature of the interface
    // between PE <-> HAL, some transient information
    // like this needs to be cached.
    // This is cached upon receipt of eWNI_SME_SETCONTEXT_REQ.
    // This is released while posting LIM_MLM_SETKEYS_CNF
    //
    void* gpLimMlmSetKeysReq;
    void* gpLimMlmRemoveKeyReq;

    //On STA: staid for self generated by HAL and sent as response to 'ADD STA' msg.
    //On AP:   staid corresponding to BSS generated by HAL and sent as response to 'ADD BSS' msg.
  //  tANI_U16             gLimStaid; // TO SUPPORT BT-AMP

    //////////////////////////////////////////     BSS RELATED END ///////////////////////////////////////////

    //////////////////////////////////////////     IBSS RELATED START ///////////////////////////////////////////
    // This indicates whether we've a partner
    // that is also transmitting Beacon frame
    // in IBSS
    //tANI_U8    gLimIbssActive;  oct1 review

    //This indicates whether this STA coalesced and adapter to peer's capabilities or not.
    tANI_U8    gLimIbssCoalescingHappened;

    /// Definition for storing IBSS peers BSS description
    tLimIbssPeerNode      *gLimIbssPeerList;
    tANI_U32               gLimNumIbssPeers;
    tANI_U32               gLimIbssRetryCnt;

    // ibss info - params for which ibss to join while coalescing
    tAniSirLimIbss      ibssInfo;

    //////////////////////////////////////////     IBSS RELATED END ///////////////////////////////////////////

    //////////////////////////////////////////     STATS/COUNTER RELATED START ///////////////////////////////////////////

    tANI_U16   maxStation;
    tANI_U16   maxBssId;

    tANI_U32    gLimNumBeaconsRcvd;
    tANI_U32    gLimNumBeaconsIgnored;

    tANI_U32    gLimNumDeferredMsgs;

    /// Variable to keep track of number of currently associated STAs
    tANI_U16  gLimNumOfAniSTAs;      // count of ANI peers
    tANI_U16  gLimAssocStaLimit;

    /// This indicates number of RXed Beacons during HB period
   // tANI_U8    gLimRxedBeaconCntDuringHB;

    // Heart-Beat interval value
    tANI_U32   gLimHeartBeatCount;
    tSirMacAddr gLimHeartBeatApMac[2];
    tANI_U8 gLimHeartBeatApMacIndex;

    // Statistics to keep track of no. beacons rcvd in heart beat interval
    tANI_U16            gLimHeartBeatBeaconStats[MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL];

#ifdef WLAN_DEBUG
    // Debug counters
    tANI_U32     numTot, numBbt, numProtErr, numLearn, numLearnIgnore;
    tANI_U32     numSme, numMAC[4][16];

    // Debug counter to track number of Assoc Req frame drops
    // when received in pStaDs->mlmState other than LINK_ESTABLISED
    tANI_U32    gLimNumAssocReqDropInvldState;
    // counters to track rejection of Assoc Req due to Admission Control
    tANI_U32    gLimNumAssocReqDropACRejectTS;
    tANI_U32    gLimNumAssocReqDropACRejectSta;
    // Debug counter to track number of Reassoc Req frame drops
    // when received in pStaDs->mlmState other than LINK_ESTABLISED
    tANI_U32    gLimNumReassocReqDropInvldState;
    // Debug counter to track number of Hash Miss event that
    // will not cause a sending of de-auth/de-associate frame
    tANI_U32    gLimNumHashMissIgnored;

    // Debug counter to track number of Beacon frames
    // received in unexpected state
    tANI_U32    gLimUnexpBcnCnt;

    // Debug counter to track number of Beacon frames
    // received in wt-join-state that do have SSID mismatch
    tANI_U32    gLimBcnSSIDMismatchCnt;

    // Debug counter to track number of Link establishments on STA/BP
    tANI_U32    gLimNumLinkEsts;

    // Debug counter to track number of Rx cleanup
    tANI_U32    gLimNumRxCleanup;

    // Debug counter to track different parse problem
    tANI_U32    gLim11bStaAssocRejectCount;

#endif    

    //Time stamp of the last beacon received from the BSS to which STA is connected.
    tANI_U64 gLastBeaconTimeStamp;
    //RX Beacon count for the current BSS to which STA is connected.
    tANI_U32 gCurrentBssBeaconCnt;
    tANI_U8 gLastBeaconDtimCount;
    tANI_U8 gLastBeaconDtimPeriod;


    //////////////////////////////////////////     STATS/COUNTER RELATED END ///////////////////////////////////////////


    //////////////////////////////////////////     STATES RELATED START ///////////////////////////////////////////
    // Counts Heartbeat failures
    tANI_U8    gLimHBfailureCntInLinkEstState;
    tANI_U8    gLimProbeFailureAfterHBfailedCnt;
    tANI_U8    gLimHBfailureCntInOtherStates;

    /**
     * This variable indicates whether LIM module need to
     * send response to host. Used to identify whether a request
     * is generated internally within LIM module or by host
     */
    tANI_U8             gLimRspReqd;

    /// Previous SME State
    tLimSmeStates       gLimPrevSmeState;

    /// MLM State visible across all Sirius modules
    tLimMlmStates       gLimMlmState;

    /// Previous MLM State
    tLimMlmStates       gLimPrevMlmState;

#ifdef GEN4_SCAN
    // LIM to HAL SCAN Management Message Interface states
    tLimLimHalScanState gLimHalScanState;
//WLAN_SUSPEND_LINK Related
    SUSPEND_RESUME_LINK_CALLBACK gpLimSuspendCallback; 
    tANI_U32 *gpLimSuspendData;
    SUSPEND_RESUME_LINK_CALLBACK gpLimResumeCallback; 
    tANI_U32 *gpLimResumeData;
//end WLAN_SUSPEND_LINK Related
    tANI_U8    fScanDisabled;
    //Can be set to invalid channel. If it is invalid, HAL
    //should move to previous valid channel or stay in the
    //current channel. CB state goes along with channel to resume to
    tANI_U16    gResumeChannel;
    ePhyChanBondState    gResumePhyCbState;
#endif // GEN4_SCAN

    // Change channel generic scheme
    CHANGE_CHANNEL_CALLBACK gpchangeChannelCallback;
    tANI_U32 *gpchangeChannelData;

    /// SME State visible across all Sirius modules
    tLimSmeStates         gLimSmeState;
    /// This indicates whether we're an AP, STA in BSS/IBSS
    tLimSystemRole        gLimSystemRole;

    // Number of STAs that do not support short preamble
    tLimNoShortParams         gLimNoShortParams;

    // Number of STAs that do not support short slot time
    tLimNoShortSlotParams   gLimNoShortSlotParams;


    // OLBC parameters
    tLimProtStaParams  gLimOverlap11gParams;

    tLimProtStaParams  gLimOverlap11aParams;
    tLimProtStaParams gLimOverlapHt20Params;
    tLimProtStaParams gLimOverlapNonGfParams;

    //
    // ---------------- DPH -----------------------
    // these used to live in DPH but are now moved here (where they belong)
    tANI_U32           gLimPhyMode;
    tANI_U32           propRateAdjustPeriod;
    tANI_U32           scanStartTime;    // used to measure scan time

    //tANI_U8            gLimBssid[6];
    tANI_U8            gLimMyMacAddr[6];
    tANI_U8            ackPolicy;

    tANI_U8            gLimQosEnabled:1; //11E
    tANI_U8            gLimWmeEnabled:1; //WME
    tANI_U8            gLimWsmEnabled:1; //WSM
    tANI_U8            gLimHcfEnabled:1;
    tANI_U8            gLim11dEnabled:1;
    tANI_U8            gLimProbeRespDisableFlag:1; // control over probe response
    // ---------------- DPH -----------------------

    //////////////////////////////////////////     STATES RELATED END ///////////////////////////////////////////

    //////////////////////////////////////////     MISC RELATED START ///////////////////////////////////////////

    // WDS info
    tANI_U32            gLimNumWdsInfoInd;
    tANI_U32            gLimNumWdsInfoSet;
    tSirWdsInfo         gLimWdsInfo;

    // Deferred Queue Paramters
    tLimDeferredMsgQParams    gLimDeferredMsgQ;

    // addts request if any - only one can be outstanding at any time
    tSirAddtsReq       gLimAddtsReq;
    tANI_U8            gLimAddtsSent;
    tANI_U8            gLimAddtsRspTimerCount;

    //protection related config cache
    tCfgProtection    cfgProtection;

    tANI_U8 gLimProtectionControl;
    //RF band to determibe 2.4/5 GHZ

    // alternate radio info used by STA
    tSirAlternateRadioInfo  gLimAlternateRadio;

    //This flag will remain to be set except while LIM is waiting for specific response messages
    //from HAL. e.g when LIM issues ADD_STA req it will clear this flag and when it will receive
    //the response the flag will be set.
    tANI_U8   gLimProcessDefdMsgs;

    // UAPSD flag used on AP
    tANI_U8  gUapsdEnable;          

    /* Used on STA, this is a static UAPSD mask setting  
     * derived  from SME_JOIN_REQ and SME_REASSOC_REQ. If a 
     * particular AC bit is set, it means the AC is both  
     * trigger enabled and delivery enabled. 
     */
    tANI_U8  gUapsdPerAcBitmask;   

    /* Used on STA, this is a dynamic UPASD mask setting 
     * derived from AddTS Rsp and DelTS frame. If a 
     * particular AC bit is set, it means AC is trigger
     * enabled. 
     */
    tANI_U8  gUapsdPerAcTriggerEnableMask;  

    /* Used on STA, dynamic UPASD mask setting
     * derived from AddTS Rsp and DelTs frame. If 
     * a particular AC bit is set, it means AC is 
     * delivery enabled. 
     */ 
    tANI_U8  gUapsdPerAcDeliveryEnableMask; 
    
    /* Used on STA for AC downgrade. This is a dynamic mask
     * setting which keep tracks of ACs being admitted. 
     * If bit is set to 0: That partiular AC is not admitted
     * If bit is set to 1: That particular AC is admitted
     */
    tANI_U8  gAcAdmitMask[SIR_MAC_DIRECTION_DIRECT];

    //dialogue token List head/tail for Action frames request sent.
    tpDialogueToken pDialogueTokenHead;
    tpDialogueToken pDialogueTokenTail;

    tLimTspecInfo tspecInfo[LIM_NUM_TSPEC_MAX];

    // admission control policy information
    tLimAdmitPolicyInfo admitPolicyInfo;
    vos_lock_t lkPeGlobalLock;
    tANI_U8 disableLDPCWithTxbfAP;
#ifdef FEATURE_WLAN_TDLS
    tANI_U8 gLimTDLSBufStaEnabled;
    tANI_U8 gLimTDLSUapsdMask;
    tANI_U8 gLimTDLSOffChannelEnabled;
    // TDLS WMM Mode
    tANI_U8 gLimTDLSWmmMode;
#endif



    //////////////////////////////////////////     MISC RELATED END ///////////////////////////////////////////

    //////////////////////////////////////////     ASSOC RELATED START ///////////////////////////////////////////
    // Place holder for JoinReq message
    // received by SME state machine
   // tpSirSmeJoinReq       gpLimJoinReq;

    // Place holder for ReassocReq message
    // received by SME state machine
    //tpSirSmeReassocReq    gpLimReassocReq;  sep23 review

    // Current Authentication type used at STA
    //tAniAuthType        gLimCurrentAuthType;

    // Place holder for current authentication request
    // being handled
    tLimMlmAuthReq     *gpLimMlmAuthReq;

    // Place holder for Join request that we're
    // currently attempting
    //tLimMlmJoinReq       *gpLimMlmJoinReq;

    // Reason code to determine the channel change context while sending 
    // WDA_CHNL_SWITCH_REQ message to HAL       
    tANI_U32 channelChangeReasonCode;
    
    /// MAC level Pre-authentication related globals
    tSirMacChanNum        gLimPreAuthChannelNumber;
    tAniAuthType          gLimPreAuthType;
    tSirMacAddr           gLimPreAuthPeerAddr;
    tANI_U32              gLimNumPreAuthContexts;
    tLimPreAuthTable      gLimPreAuthTimerTable;

    // Placed holder to deauth reason
    tANI_U16 gLimDeauthReasonCode;

    // Place holder for Pre-authentication node list
    struct tLimPreAuthNode *  pLimPreAuthList;

    // Assoc or ReAssoc Response Data/Frame
    void                *gLimAssocResponseData;

    //One cache for each overlap and associated case.
    tCacheParams    protStaOverlapCache[LIM_PROT_STA_OVERLAP_CACHE_SIZE];
    tCacheParams    protStaCache[LIM_PROT_STA_CACHE_SIZE];

    //////////////////////////////////////////     ASSOC RELATED END ///////////////////////////////////////////



    //
    // For DEBUG purposes
    // Primarily for - TITAN BEACON workaround
    // Symptom - TFP/PHY gets stuck
    //
    tANI_U32 gLimScanOverride;
    // Holds the desired tSirScanType, as requested by SME
    tSirScanType gLimScanOverrideSaved;

    //
    // CB State protection, operated upon as follows:
    // 1 - CB is enabled in the hardware ONLY WHEN a Titan
    // STA associates with the AP
    // 0 - CB is enabled/disabled based on the configuration
    // received as per eWNI_SME_START_BSS_REQ
    //
    tANI_U32 gLimCBStateProtection;

    // Count of TITAN STA's currently associated
    tANI_U16 gLimTitanStaCount;

    //
    // For DEBUG purposes
    // Primarily for - TITAN workaround
    // Symptom - Avoid NULL data frames
        // Applies to AP only
    //
    tANI_U32 gLimBlockNonTitanSta;
    /////////////////////////// TITAN related globals       //////////////////////////////////////////


    ////////////////////////////////  HT RELATED           //////////////////////////////////////////
    //
    // The following global LIM variables maintain/manage
    // the runtime configurations related to 802.11n

    // 802.11n Station detected HT capability in Beacon Frame
    tANI_U8 htCapabilityPresentInBeacon;

    // 802.11 HT capability: Enabled or Disabled
    tANI_U8 htCapability;


    tANI_U8 gHTGreenfield;

    tANI_U8 gHTShortGI40Mhz;
    tANI_U8 gHTShortGI20Mhz;

    //Set to 0 for 3839 octets
    //Set to 1 for 7935 octets
    tANI_U8 gHTMaxAmsduLength;


    // DSSS/CCK at 40 MHz: Enabled 1 or Disabled
    tANI_U8 gHTDsssCckRate40MHzSupport;

    // PSMP Support: Enabled 1 or Disabled 0
    tANI_U8 gHTPSMPSupport;

    // L-SIG TXOP Protection used only if peer support available
    tANI_U8 gHTLsigTXOPProtection;

    // MIMO Power Save
    tSirMacHTMIMOPowerSaveState gHTMIMOPSState;

    // Scan In Power Save
    tANI_U8 gScanInPowersave;

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
    tANI_U8 gHTAMpduDensity;

    tANI_BOOLEAN gMaxAmsduSizeEnabled;
    // Maximum Tx/Rx A-MPDU factor
    tANI_U8 gHTMaxRxAMpduFactor;

    //
    // Scheduled PSMP related - Service Interval Granularity
    // 000 - 5 ms
    // 001 - 10 ms
    // 010 - 15 ms
    // 011 - 20 ms
    // 100 - 25 ms
    // 101 - 30 ms
    // 110 - 35 ms
    // 111 - 40 ms
    //
    tANI_U8 gHTServiceIntervalGranularity;

    // Indicates whether an AP wants to associate PSMP enabled Stations
    tANI_U8 gHTControlledAccessOnly;

    // RIFS Mode. Set if no APSD legacy devices associated
    tANI_U8 gHTRifsMode;
   // OBss Mode . set when we have Non HT STA is associated or with in overlap bss
    tANI_U8  gHTObssMode;

    // Identifies the current Operating Mode
    tSirMacHTOperatingMode gHTOperMode;

    // Indicates if PCO is activated in the BSS
    tANI_U8 gHTPCOActive;

    //
    // If PCO is active, indicates which PCO phase to use
    // 0 - switch to 20 MHz phase
    // 1 - switch to 40 MHz phase
    //
    tANI_U8 gHTPCOPhase;

    //
    // Used only in beacons. For PR, this is set to 0
    // 0 - Primary beacon
    // 1 - Secondary beacon
    //
    tANI_U8 gHTSecondaryBeacon;

    //
    // Dual CTS Protection
    // 0 - Use RTS/CTS
    // 1 - Dual CTS Protection is used
    //
    tANI_U8 gHTDualCTSProtection;

    //
    // Identifies a single STBC MCS that shall ne used for
    // STBC control frames and STBC beacons
    //
    tANI_U8 gHTSTBCBasicMCS;

    tANI_U8 gHTNonGFDevicesPresent;

    tANI_U8   gAddBA_Declined;               // Flag to Decline the BAR if the particular bit (0-7) is being set

    ////////////////////////////////  HT RELATED           //////////////////////////////////////////

#ifdef FEATURE_WLAN_TDLS
    tANI_U8 gLimAddStaTdls ;
    tANI_U8 gLimTdlsLinkMode ;
    ////////////////////////////////  TDLS RELATED         //////////////////////////////////////////
#endif

    // wsc info required to form the wsc IE
    tLimWscIeInfo wscIeInfo;
    tpPESession gpSession ;   //Pointer to  session table   
    /*
    * sessionID and transactionID from SME is stored here for those messages, for which
    * there is no session context in PE, e.g. Scan related messages.
    **/
    tANI_U8   gSmeSessionId;
    tANI_U16 gTransactionId;

#ifdef FEATURE_OEM_DATA_SUPPORT
tLimMlmOemDataReq       *gpLimMlmOemDataReq;
tLimMlmOemDataRsp       *gpLimMlmOemDataRsp;
#endif

    tSirRemainOnChnReq  *gpLimRemainOnChanReq; //hold remain on chan request in this buf
    vos_list_t  gLimMgmtFrameRegistratinQueue;
    tANI_U32    mgmtFrameSessionId;
    tSirBackgroundScanMode gLimBackgroundScanMode;

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    tpPESession  pSessionEntry;
    tANI_U8 reAssocRetryAttempt;
#endif
    tLimDisassocDeauthCnfReq limDisassocDeauthCnfReq;
    tANI_U8 deferredMsgCnt;
    tSirDFSChannelList    dfschannelList;
    tANI_U8 deauthMsgCnt;
    tANI_U8 gLimIbssStaLimit;
    tANI_U8 probeCounter;
    tANI_U8 maxProbe;
    tANI_U8 retryPacketCnt;

    // Flag to debug remain on channel
    tANI_BOOLEAN gDebugP2pRemainOnChannel;
    /* Sequence number to keep track of
     * start and end of remain on channel
     * debug marker frame.
     */
    tANI_U32 remOnChnSeqNum;
    tANI_U32 txBdToken;
    tANI_U32 EnableTdls2040BSSCoexIE;
} tAniSirLim, *tpAniSirLim;

typedef struct sLimMgmtFrameRegistration
{
    vos_list_node_t node;     // MUST be first element
    tANI_U16        frameType;
    tANI_U16        matchLen;
    tANI_U16        sessionId;
    tANI_U8         matchData[1];
} tLimMgmtFrameRegistration, *tpLimMgmtFrameRegistration;

#if defined WLAN_FEATURE_VOWIFI
typedef struct sRrmContext
{
  tRrmSMEContext rrmSmeContext;
  tRrmPEContext  rrmPEContext; 
}tRrmContext, *tpRrmContext;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
typedef struct sFTContext
{
  tftSMEContext ftSmeContext;
  tftPEContext  ftPEContext; 
} tftContext, *tpFTContext;
#endif

//Check if this definition can actually move here even for Volans. In that case
//this featurization can be removed.
/** ------------------------------------------------------------------------- * 

    \typedef tDriverType
    
    \brief   Indicate the driver type to the mac, and based on this do
             appropriate initialization.
    
    -------------------------------------------------------------------------- */

typedef enum
{
    eDRIVER_TYPE_PRODUCTION  = 0,
    eDRIVER_TYPE_MFG         = 1,
    eDRIVER_TYPE_DVT         = 2
} tDriverType;

/** ------------------------------------------------------------------------- * 

    \typedef tMacOpenParameters
    
    \brief Parameters needed for Enumeration of all status codes returned by the higher level 
    interface functions.
    
    -------------------------------------------------------------------------- */

typedef struct sMacOpenParameters
{
    tANI_U16 maxStation;
    tANI_U16 maxBssId;
    tANI_U32 frameTransRequired;
    tDriverType  driverType;
} tMacOpenParameters;

typedef struct sHalMacStartParameters
{
    // parametes for the Firmware
    //tHalFirmwareParameters FW;    
    tDriverType  driverType;

} tHalMacStartParameters;

typedef enum
{
    LIM_AUTH_ACK_NOT_RCD,
    LIM_AUTH_ACK_RCD_SUCCESS,
    LIM_AUTH_ACK_RCD_FAILURE,
} tAuthAckStatus;

// -------------------------------------------------------------------
/// MAC Sirius parameter structure
typedef struct sAniSirGlobal

{
    tDriverType  gDriverType;

    // we should be able to save this hddHandle in here and deprecate
    // the pAdapter.  For now, compiles are a problem because there
    // are dependencides on the header files that are not handling the
    // compiler very gracefully.
//    tHddHandle   hHdd;       // Handle to the HDD.
    //void        *hHdd;
    void        *pAdapter;   // deprecate this pAdapter pointer eventually...
                             // all interfaces to the HDD should pass hHdd, which
                             // is stored in this struct above.....
    tSirMbMsg*   pResetMsg;
    tAniSirCfg   cfg;
    tAniSirLim   lim;
    tAniSirPmm   pmm;
    tAniSirSch   sch;
    tAniSirSys   sys;
    tAniSirUtils utils;
    // PAL/HDD handle
    tHddHandle hHdd;

#ifdef ANI_DVT_DEBUG
    tAniSirDvt   dvt;
#endif

    tSmeStruct sme;
    tCsrScanStruct scan;
    tCsrRoamStruct roam;

#ifdef FEATURE_OEM_DATA_SUPPORT
    tOemDataStruct oemData;
#endif
    tPmcInfo     pmc;
    tSmeBtcInfo  btc;

    tCcm ccm;

#if defined WLAN_FEATURE_VOWIFI
    tRrmContext rrm;
#endif
#ifdef WLAN_FEATURE_CONCURRENT_P2P
    tp2pContext p2pContext[MAX_NO_OF_P2P_SESSIONS];
#else
    tp2pContext p2pContext;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
    tftContext   ft;
#endif

    tANI_U32     gCurrentLogSize;
    tANI_U32     menuCurrent;
    /* logDump specific */
    tANI_U32 dumpTablecurrentId;
    /* Instead of static allocation I will dyanamically allocate memory for dumpTableEntry
        Thinking of using linkedlist  */ 
    tDumpModuleEntry *dumpTableEntry[MAX_DUMP_TABLE_ENTRY];
#ifdef FEATURE_WLAN_TDLS
    v_BOOL_t isTdlsPowerSaveProhibited;
#endif
    tANI_U8 fScanOffload;
    tANI_U8 isCoalesingInIBSSAllowed;
    tANI_U32 fEnableDebugLog;
    tANI_U32 fDeferIMPSTime;
    tANI_BOOLEAN deferImps;

#ifdef WLAN_FEATURE_11AC
    /* Alow Mu BFormee session only if MU BF session doesnt exist.
     */
    v_BOOL_t isMuBfsessionexist;
#endif

    v_BOOL_t isCoexScoIndSet;
    v_U8_t miracast_mode;
    v_U8_t fBtcEnableIndTimerVal;
    v_U8_t roamDelayStatsEnabled;
    tANI_BOOLEAN miracastVendorConfig;
    v_BOOL_t fActiveScanOnDFSChannels;
    tAuthAckStatus  authAckStatus;
    sir_mgmt_frame_ind_callback mgmt_frame_ind_cb;
} tAniSirGlobal;

#ifdef FEATURE_WLAN_TDLS

#define RFC1042_HDR_LENGTH      (6)
#define GET_BE16(x)             ((tANI_U16) (((x)[0] << 8) | (x)[1]))
#define ETH_TYPE_89_0d          (0x890d)
#define ETH_TYPE_LEN            (2)
#define PAYLOAD_TYPE_TDLS_SIZE  (1)
#define PAYLOAD_TYPE_TDLS       (2)

#endif

#endif /* _ANIGLOBAL_H */

