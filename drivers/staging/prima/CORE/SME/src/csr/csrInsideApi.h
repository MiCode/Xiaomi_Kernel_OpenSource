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

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file csrInsideApi.h
  
    Define interface only used by CSR.
   ========================================================================== */
#ifndef CSR_INSIDE_API_H__
#define CSR_INSIDE_API_H__


#include <linux/version.h>
#include "csrSupport.h"
#include "smeInside.h"
#include "vos_nvitem.h"

#define CSR_PASSIVE_MAX_CHANNEL_TIME   110
#define CSR_PASSIVE_MIN_CHANNEL_TIME   60

#define CSR_ACTIVE_MAX_CHANNEL_TIME    40
#define CSR_ACTIVE_MIN_CHANNEL_TIME    20

#define CSR_ACTIVE_MAX_CHANNEL_TIME_BTC    120
#define CSR_ACTIVE_MIN_CHANNEL_TIME_BTC    60

#ifdef WLAN_AP_STA_CONCURRENCY
#define CSR_PASSIVE_MAX_CHANNEL_TIME_CONC   110
#define CSR_PASSIVE_MIN_CHANNEL_TIME_CONC   60 

#define CSR_ACTIVE_MAX_CHANNEL_TIME_CONC    27
#define CSR_ACTIVE_MIN_CHANNEL_TIME_CONC    20

#define CSR_REST_TIME_CONC                  100

#define CSR_NUM_STA_CHAN_COMBINED_CONC      3
#define CSR_NUM_P2P_CHAN_COMBINED_CONC      1
#endif

#define CSR_MAX_NUM_SUPPORTED_CHANNELS 55

#define CSR_MAX_2_4_GHZ_SUPPORTED_CHANNELS 14

#define CSR_MAX_BSS_SUPPORT            250
#define SYSTEM_TIME_MSEC_TO_USEC      1000

//This number minus 1 means the number of times a channel is scanned before a BSS is remvoed from
//cache scan result
#define CSR_AGING_COUNT     3   
//The following defines are used by palTimer
//This is used for palTimer when request to imps fails
#define CSR_IDLE_SCAN_WAIT_TIME     (1 * PAL_TIMER_TO_SEC_UNIT)     //1 second 
//This is used for palTimer when imps ps is disabled
//This number shall not be smaller than 5-6 seconds in general because a full scan may take 3-4 seconds
#define CSR_IDLE_SCAN_NO_PS_INTERVAL     (10 * PAL_TIMER_TO_SEC_UNIT)     //10 second 
#define CSR_IDLE_SCAN_NO_PS_INTERVAL_MIN (5 * PAL_TIMER_TO_SEC_UNIT)
#define CSR_SCAN_GET_RESULT_INTERVAL    (5 * PAL_TIMER_TO_SEC_UNIT)     //5 seconds
#define CSR_MIC_ERROR_TIMEOUT  (60 * PAL_TIMER_TO_SEC_UNIT)     //60 seconds
#define CSR_TKIP_COUNTER_MEASURE_TIMEOUT  (60 * PAL_TIMER_TO_SEC_UNIT)     //60 seconds
#define CSR_SCAN_RESULT_AGING_INTERVAL    (5 * PAL_TIMER_TO_SEC_UNIT)     //5 seconds
#define CSR_SCAN_RESULT_CFG_AGING_INTERVAL    (PAL_TIMER_TO_SEC_UNIT)     // 1  second
//the following defines are NOT used by palTimer
#define CSR_SCAN_AGING_TIME_NOT_CONNECT_NO_PS 50     //50 seconds
#define CSR_SCAN_AGING_TIME_NOT_CONNECT_W_PS 300     //300 seconds
#define CSR_SCAN_AGING_TIME_CONNECT_NO_PS 150        //150 seconds
#define CSR_SCAN_AGING_TIME_CONNECT_W_PS 600         //600 seconds
#define CSR_JOIN_FAILURE_TIMEOUT_DEFAULT ( 3000 )
#define CSR_JOIN_FAILURE_TIMEOUT_MIN   (1000)  //minimal value
//These are going against the signed RSSI (tANI_S8) so it is between -+127
#define CSR_BEST_RSSI_VALUE         (-30)   //RSSI >= this is in CAT4
#define CSR_DEFAULT_RSSI_DB_GAP     30 //every 30 dbm for one category
#define CSR_BSS_CAP_VALUE_NONE  0    //not much value
#define CSR_BSS_CAP_VALUE_HT    2    
#define CSR_BSS_CAP_VALUE_WMM   1
#define CSR_BSS_CAP_VALUE_UAPSD 1
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
#define CSR_BSS_CAP_VALUE_5GHZ  1
#endif
#define CSR_DEFAULT_ROAMING_TIME 10   //10 seconds
#define CSR_ROAM_MIN(X, Y)  ((X) < (Y) ? (X) : (Y))
#define CSR_ROAM_MAX(X, Y)  ((X) > (Y) ? (X) : (Y))

#ifdef FEATURE_WLAN_BTAMP_UT_RF
#define CSR_JOIN_MAX_RETRY_COUNT             10
#define CSR_JOIN_RETRY_TIMEOUT_PERIOD        ( 1 *  PAL_TIMER_TO_SEC_UNIT )  // 1 second
#endif

typedef enum 
{
    eCsrNextScanNothing,
    eCsrNextLostLinkScan1Success,
    eCsrNextLostLinkScan1Failed,
    eCsrNextLostLinkScan2Success,
    eCsrNextLostLinkScan2Failed,
    eCsrNextLostLinkScan3Success,
    eCsrNexteScanForSsidSuccess,
    eCsrNextLostLinkScan3Failed,
    eCsrNext11dScan1Failure,
    eCsrNext11dScan1Success,
    eCsrNext11dScan2Failure, 
    eCsrNext11dScan2Success,
    eCsrNext11dScanComplete,
    eCsrNexteScanForSsidFailure,
    eCsrNextIdleScanComplete,
    eCsrNextCapChangeScanComplete,

}eCsrScanCompleteNextCommand;

typedef enum  
{
    eCsrJoinSuccess, 
    eCsrJoinFailure,
    eCsrReassocSuccess,
    eCsrReassocFailure, 
    eCsrNothingToJoin, 
    eCsrStartBssSuccess,
    eCsrStartBssFailure,
    eCsrSilentlyStopRoaming,
    eCsrSilentlyStopRoamingSaveState,
    eCsrJoinWdsFailure,
    eCsrJoinFailureDueToConcurrency,
    
}eCsrRoamCompleteResult;

typedef struct tagScanReqParam
{
    tANI_U8 bReturnAfter1stMatch;
    tANI_U8 fUniqueResult;
    tANI_U8 freshScan;
    tANI_U8 hiddenSsid;
    tANI_U8 reserved;
}tScanReqParam;

typedef struct tagCsrScanResult
{
    tListElem Link;
    tANI_S32 AgingCount;    //This BSS is removed when it reaches 0 or less
    tANI_U32 preferValue;   //The bigger the number, the better the BSS. This value override capValue
    tANI_U32 capValue;  //The biggger the better. This value is in use only if we have equal preferValue
    //This member must be the last in the structure because the end of tSirBssDescription (inside) is an
    //    array with nonknown size at this time
    
    eCsrEncryptionType ucEncryptionType; //Preferred Encryption type that matched with profile.
    eCsrEncryptionType mcEncryptionType; 
    eCsrAuthType authType; //Preferred auth type that matched with the profile.

    tCsrScanResultInfo Result;
}tCsrScanResult;

typedef struct
{
    tDblLinkList List;
    tListElem *pCurEntry;
}tScanResultList;




#define CSR_IS_ROAM_REASON( pCmd, reason ) ( (reason) == (pCmd)->roamCmd.roamReason )
#define CSR_IS_BETTER_PREFER_VALUE(v1, v2)   ((v1) > (v2))
#define CSR_IS_EQUAL_PREFER_VALUE(v1, v2)   ((v1) == (v2))
#define CSR_IS_BETTER_CAP_VALUE(v1, v2)     ((v1) > (v2))
#define CSR_IS_ENC_TYPE_STATIC( encType ) ( ( eCSR_ENCRYPT_TYPE_NONE == (encType) ) || \
                                            ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == (encType) ) || \
                                            ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == (encType) ) )
#define CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) ( CSR_IS_ROAM_JOINED( pMac, sessionId ) && CSR_IS_ROAM_SUBSTATE_WAITFORKEY( pMac, sessionId ) )
//WIFI has a test case for not using HT rates with TKIP as encryption
//We may need to add WEP but for now, TKIP only.

#define CSR_IS_11n_ALLOWED( encType ) (( eCSR_ENCRYPT_TYPE_TKIP != (encType) ) && \
                                      ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY != (encType) ) && \
                                      ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY != (encType) ) && \
                                      ( eCSR_ENCRYPT_TYPE_WEP40 != (encType) ) && \
                                      ( eCSR_ENCRYPT_TYPE_WEP104 != (encType) ) )

#define CSR_IS_DISCONNECT_COMMAND(pCommand) ( ( eSmeCommandRoam == (pCommand)->command ) &&\
                                              ( ( eCsrForcedDisassoc == (pCommand)->u.roamCmd.roamReason ) ||\
                                                ( eCsrForcedDeauth == (pCommand)->u.roamCmd.roamReason ) ||\
                                                ( eCsrSmeIssuedDisassocForHandoff ==\
                                                                        (pCommand)->u.roamCmd.roamReason ) ||\
                                                ( eCsrForcedDisassocMICFailure ==\
                                                                          (pCommand)->u.roamCmd.roamReason ) ) )

eCsrRoamState csrRoamStateChange( tpAniSirGlobal pMac, eCsrRoamState NewRoamState, tANI_U8 sessionId);
eHalStatus csrScanningStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf );
void csrRoamingStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf );
void csrRoamJoinedStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf );
tANI_BOOLEAN csrScanComplete( tpAniSirGlobal pMac, tSirSmeScanRsp *pScanRsp );
void csrReleaseCommandRoam(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrReleaseCommandScan(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrReleaseCommandWmStatusChange(tpAniSirGlobal pMac, tSmeCmd *pCommand);
//pIes2 can be NULL
tANI_BOOLEAN csrIsDuplicateBssDescription( tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc1, 
                                           tSirBssDescription *pSirBssDesc2, tDot11fBeaconIEs *pIes2, tANI_BOOLEAN fForced );
eHalStatus csrRoamSaveConnectedBssDesc( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDesc );
tANI_BOOLEAN csrIsNetworkTypeEqual( tSirBssDescription *pSirBssDesc1, tSirBssDescription *pSirBssDesc2 );
eHalStatus csrScanSmeScanResponse( tpAniSirGlobal pMac, void *pMsgBuf );
/*
   Prepare a filter base on a profile for parsing the scan results.
   Upon successful return, caller MUST call csrFreeScanFilter on 
   pScanFilter when it is done with the filter.
*/
eHalStatus csrRoamPrepareFilterFromProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile, tCsrScanResultFilter *pScanFilter);
eHalStatus csrRoamCopyProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pDstProfile, tCsrRoamProfile *pSrcProfile);
eHalStatus csrRoamStart(tpAniSirGlobal pMac);
void csrRoamStop(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrRoamStartMICFailureTimer(tpAniSirGlobal pMac);
void csrRoamStopMICFailureTimer(tpAniSirGlobal pMac);
void csrRoamStartTKIPCounterMeasureTimer(tpAniSirGlobal pMac);
void csrRoamStopTKIPCounterMeasureTimer(tpAniSirGlobal pMac);

eHalStatus csrScanOpen(tpAniSirGlobal pMac);
eHalStatus csrScanClose(tpAniSirGlobal pMac);
eHalStatus csrScanRequestLostLink1( tpAniSirGlobal pMac, tANI_U32 sessionId );
eHalStatus csrScanRequestLostLink2( tpAniSirGlobal pMac, tANI_U32 sessionId );
eHalStatus csrScanRequestLostLink3( tpAniSirGlobal pMac, tANI_U32 sessionId );
eHalStatus csrScanHandleFailedLostlink1(tpAniSirGlobal pMac, tANI_U32 sessionId);
eHalStatus csrScanHandleFailedLostlink2(tpAniSirGlobal pMac, tANI_U32 sessionId);
eHalStatus csrScanHandleFailedLostlink3(tpAniSirGlobal pMac, tANI_U32 sessionId);
tCsrScanResult *csrScanAppendBssDescription( tpAniSirGlobal pMac, 
                                             tSirBssDescription *pSirBssDescription,
                                             tDot11fBeaconIEs *pIes, tANI_BOOLEAN fForced);
void csrScanCallCallback(tpAniSirGlobal pMac, tSmeCmd *pCommand, eCsrScanStatus scanStatus);
eHalStatus csrScanCopyRequest(tpAniSirGlobal pMac, tCsrScanRequest *pDstReq, tCsrScanRequest *pSrcReq);
eHalStatus csrScanFreeRequest(tpAniSirGlobal pMac, tCsrScanRequest *pReq);
eHalStatus csrScanCopyResultList(tpAniSirGlobal pMac, tScanResultHandle hIn, tScanResultHandle *phResult);
void csrInitBGScanChannelList(tpAniSirGlobal pMac);
eHalStatus csrScanForSSID(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tANI_U32 roamId, tANI_BOOLEAN notify);
eHalStatus csrScanForCapabilityChange(tpAniSirGlobal pMac, tSirSmeApNewCaps *pNewCaps);
eHalStatus csrScanStartGetResultTimer(tpAniSirGlobal pMac);
eHalStatus csrScanStopGetResultTimer(tpAniSirGlobal pMac);
eHalStatus csrScanStartResultCfgAgingTimer(tpAniSirGlobal pMac);
eHalStatus csrScanStopResultCfgAgingTimer(tpAniSirGlobal pMac);
eHalStatus csrScanBGScanEnable(tpAniSirGlobal pMac);
eHalStatus csrScanStartIdleScanTimer(tpAniSirGlobal pMac, tANI_U32 interval);
eHalStatus csrScanStopIdleScanTimer(tpAniSirGlobal pMac);
eHalStatus csrScanStartIdleScan(tpAniSirGlobal pMac);
//Param: pTimeInterval -- Caller allocated memory in return, if failed, to specify the nxt time interval for 
//idle scan timer interval
//Return: Not success -- meaning it cannot start IMPS, caller needs to start a timer for idle scan
eHalStatus csrScanTriggerIdleScan(tpAniSirGlobal pMac, tANI_U32 *pTimeInterval);
void csrScanCancelIdleScan(tpAniSirGlobal pMac);
void csrScanStopTimers(tpAniSirGlobal pMac);
//This function will remove scan commands that are not related to association or IBSS
tANI_BOOLEAN csrScanRemoveNotRoamingScanCommand(tpAniSirGlobal pMac);
//To remove fresh scan commands from the pending queue
tANI_BOOLEAN csrScanRemoveFreshScanCommand(tpAniSirGlobal pMac, tANI_U8 sessionId);
tSirAbortScanStatus csrScanAbortMacScan(tpAniSirGlobal pMac,
                                        tANI_U8 sessionId,
                                        eCsrAbortReason reason);
void csrRemoveCmdFromPendingList(tpAniSirGlobal pMac, tDblLinkList *pList, 
                                              eSmeCommandType commandType );
void csrRemoveCmdWithSessionIdFromPendingList(tpAniSirGlobal pMac,
                                              tANI_U8 sessionId,
                                              tDblLinkList *pList,
                                              eSmeCommandType commandType);
eHalStatus csrScanAbortMacScanNotForConnect(tpAniSirGlobal pMac,
                                            tANI_U8 sessionId);
eHalStatus csrScanGetScanChannelInfo(tpAniSirGlobal pMac, tANI_U8 sessionId);
eHalStatus csrScanAbortScanForSSID(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrRemoveScanForSSIDFromPendingList(tpAniSirGlobal pMac, tDblLinkList *pList, tANI_U32 sessionId);

//To age out scan results base. tSmeGetScanChnRsp is a pointer returned by LIM that
//has the information regarding scanned channels.
//The logic is that whenever CSR add a BSS to scan result, it set the age count to
//a value. This function deduct the age count if channelId matches the BSS' channelId
//The BSS is remove if the count reaches 0.
eHalStatus csrScanAgeResults(tpAniSirGlobal pMac, tSmeGetScanChnRsp *pScanChnInfo);

eHalStatus csrIbssAgeBss(tpAniSirGlobal pMac);

//If fForce is TRUE we will save the new String that is learn't.
//Typically it will be true in case of Join or user initiated ioctl
tANI_BOOLEAN csrLearnCountryInformation( tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc,
                                         tDot11fBeaconIEs *pIes, tANI_BOOLEAN fForce );
void csrApplyCountryInformation( tpAniSirGlobal pMac, tANI_BOOLEAN fForce );
void csrSetCfgScanControlList( tpAniSirGlobal pMac, tANI_U8 *countryCode, tCsrChannel *pChannelList  );
void csrReinitScanCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrFreeScanResultEntry( tpAniSirGlobal pMac, tCsrScanResult *pResult );

eHalStatus csrRoamCallCallback(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo, 
                               tANI_U32 roamId, eRoamCmdStatus u1, eCsrRoamResult u2);
eHalStatus csrRoamIssueConnect(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                               tScanResultHandle hBSSList, 
                               eCsrRoamReason reason, tANI_U32 roamId, 
                               tANI_BOOLEAN fImediate, tANI_BOOLEAN fClearScan);
eHalStatus csrRoamIssueReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                               tCsrRoamModifyProfileFields *pModProfileFields,
                               eCsrRoamReason reason, tANI_U32 roamId, tANI_BOOLEAN fImediate);
void csrRoamComplete( tpAniSirGlobal pMac, eCsrRoamCompleteResult Result, void *Context );
eHalStatus csrRoamIssueSetContextReq( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrEncryptionType EncryptType, 
                                     tSirBssDescription *pBssDescription,
                                tSirMacAddr *bssId, tANI_BOOLEAN addKey,
                                 tANI_BOOLEAN fUnicast, tAniKeyDirection aniKeyDirection, 
                                 tANI_U8 keyId, tANI_U16 keyLength, 
                                 tANI_U8 *pKey, tANI_U8 paeRole );
eHalStatus csrRoamProcessDisassocDeauth( tpAniSirGlobal pMac, tSmeCmd *pCommand, 
                                         tANI_BOOLEAN fDisassoc, tANI_BOOLEAN fMICFailure );
eHalStatus csrRoamSaveConnectedInfomation(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                                          tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes);
void csrRoamCheckForLinkStatusChange( tpAniSirGlobal pMac, tSirSmeRsp *pSirMsg );
void csrRoamStatsRspProcessor(tpAniSirGlobal pMac, tSirSmeRsp *pSirMsg);
eHalStatus csrRoamIssueStartBss( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamStartBssParams *pParam, 
                                 tCsrRoamProfile *pProfile, tSirBssDescription *pBssDesc, tANI_U32 roamId );
eHalStatus csrRoamIssueStopBss( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamSubState NewSubstate );
tANI_BOOLEAN csrIsSameProfile(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pProfile1, tCsrRoamProfile *pProfile2);
tANI_BOOLEAN csrIsRoamCommandWaiting(tpAniSirGlobal pMac);
tANI_BOOLEAN csrIsRoamCommandWaitingForSession(tpAniSirGlobal pMac, tANI_U32 sessionId);
tANI_BOOLEAN csrIsScanForRoamCommandActive( tpAniSirGlobal pMac );
eRoamCmdStatus csrGetRoamCompleteStatus(tpAniSirGlobal pMac, tANI_U32 sessionId);
//pBand can be NULL if caller doesn't need to get it
//eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, eCsrPhyMode phyModeIn, tANI_U8 operationChn, eCsrBand *pBand );
eHalStatus csrRoamIssueDisassociateCmd( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason );
eHalStatus csrRoamDisconnectInternal(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason);
//pCommand may be NULL
void csrRoamRemoveDuplicateCommand(tpAniSirGlobal pMac, tANI_U32 sessionId, tSmeCmd *pCommand, eCsrRoamReason eRoamReason);
                                 
eHalStatus csrSendJoinReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDescription, 
                              tCsrRoamProfile *pProfile, tDot11fBeaconIEs *pIes, tANI_U16 messageType );
eHalStatus csrSendMBDisassocReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr bssId, tANI_U16 reasonCode );
eHalStatus csrSendMBDeauthReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr bssId, tANI_U16 reasonCode );
eHalStatus csrSendMBDisassocCnfMsg( tpAniSirGlobal pMac, tpSirSmeDisassocInd pDisassocInd );
eHalStatus csrSendMBDeauthCnfMsg( tpAniSirGlobal pMac, tpSirSmeDeauthInd pDeauthInd );
eHalStatus csrSendAssocCnfMsg( tpAniSirGlobal pMac, tpSirSmeAssocInd pAssocInd, eHalStatus status );
eHalStatus csrSendAssocIndToUpperLayerCnfMsg( tpAniSirGlobal pMac, tpSirSmeAssocInd pAssocInd, eHalStatus Halstatus, tANI_U8 sessionId );
eHalStatus csrSendMBStartBssReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamBssType bssType, 
                                    tCsrRoamStartBssParams *pParam, tSirBssDescription *pBssDesc );
eHalStatus csrSendMBStopBssReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId );

tANI_BOOLEAN csrIsMacAddressEqual( tpAniSirGlobal pMac, tCsrBssid *pMacAddr1, tCsrBssid *pMacAddr2 );
//Caller should put the BSS' ssid to fiedl bssSsid when comparing SSID for a BSS.
tANI_BOOLEAN csrIsSsidMatch( tpAniSirGlobal pMac, tANI_U8 *ssid1, tANI_U8 ssid1Len, tANI_U8 *bssSsid, 
                            tANI_U8 bssSsidLen, tANI_BOOLEAN fSsidRequired );
tANI_BOOLEAN csrIsPhyModeMatch( tpAniSirGlobal pMac, tANI_U32 phyMode,
                                    tSirBssDescription *pSirBssDesc, tCsrRoamProfile *pProfile,
                                    eCsrCfgDot11Mode *pReturnCfgDot11Mode,
                                    tDot11fBeaconIEs *pIes);
tANI_BOOLEAN csrRoamIsChannelValid( tpAniSirGlobal pMac, tANI_U8 channel );

//pNumChan is a caller allocated space with the sizeof pChannels
eHalStatus csrGetCfgValidChannels(tpAniSirGlobal pMac, tANI_U8 *pChannels, tANI_U32 *pNumChan);
void csrRoamCcmCfgSetCallback(tHalHandle hHal, tANI_S32 result);
void csrScanCcmCfgSetCallback(tHalHandle hHal, tANI_S32 result);

tPowerdBm csrGetCfgMaxTxPower (tpAniSirGlobal pMac, tANI_U8 channel);

//To free the last roaming profile
void csrFreeRoamProfile(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrFreeConnectBssDesc(tpAniSirGlobal pMac, tANI_U32 sessionId);
eHalStatus csrMoveBssToHeadFromBSSID(tpAniSirGlobal pMac, tCsrBssid *bssid, tScanResultHandle hScanResult);
tANI_BOOLEAN csrCheckPSReady(void *pv);
void csrFullPowerCallback(void *pv, eHalStatus status);
//to free memory allocated inside the profile structure
void csrReleaseProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile);
//To free memory allocated inside scanFilter
void csrFreeScanFilter(tpAniSirGlobal pMac, tCsrScanResultFilter *pScanFilter);
eCsrCfgDot11Mode csrGetCfgDot11ModeFromCsrPhyMode(tCsrRoamProfile *pProfile, eCsrPhyMode phyMode, tANI_BOOLEAN fProprietary);
tANI_U32 csrTranslateToWNICfgDot11Mode(tpAniSirGlobal pMac, eCsrCfgDot11Mode csrDot11Mode);
void csrSaveChannelPowerForBand( tpAniSirGlobal pMac, tANI_BOOLEAN fPopulate5GBand );
void csrApplyChannelPowerCountryInfo( tpAniSirGlobal pMac, tCsrChannel *pChannelList, tANI_U8 *countryCode, tANI_BOOLEAN updateRiva);
void csrUpdateFCCChannelList(tpAniSirGlobal pMac);
void csrApplyPower2Current( tpAniSirGlobal pMac );
void csrAssignRssiForCategory(tpAniSirGlobal pMac, tANI_S8 bestApRssi, tANI_U8 catOffset);
tANI_BOOLEAN csrIsMacAddressZero( tpAniSirGlobal pMac, tCsrBssid *pMacAddr );
tANI_BOOLEAN csrIsMacAddressBroadcast( tpAniSirGlobal pMac, tCsrBssid *pMacAddr );
eHalStatus csrRoamRemoveConnectedBssFromScanCache(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pConnProfile);
eHalStatus csrRoamStartRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamingReason roamingReason);
//return a boolean to indicate whether roaming completed or continue.
tANI_BOOLEAN csrRoamCompleteRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    tANI_BOOLEAN fForce, eCsrRoamResult roamResult);
void csrRoamCompletion(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo, tSmeCmd *pCommand, eCsrRoamResult roamResult, tANI_BOOLEAN fSuccess);
void csrRoamCancelRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrResetCountryInformation( tpAniSirGlobal pMac, tANI_BOOLEAN fForce, tANI_BOOLEAN updateRiva );
void csrResetPMKIDCandidateList( tpAniSirGlobal pMac, tANI_U32 sessionId );
#ifdef FEATURE_WLAN_WAPI
void csrResetBKIDCandidateList( tpAniSirGlobal pMac, tANI_U32 sessionId );
#endif /* FEATURE_WLAN_WAPI */
eHalStatus csrSaveToChannelPower2G_5G( tpAniSirGlobal pMac, tANI_U32 tableSize, tSirMacChanInfo *channelTable );
//Get the list of the base channels to scan for passively 11d info
eHalStatus csrScanGetSupportedChannels( tpAniSirGlobal pMac );
//To check whether a country code matches the one in the IE
//Only check the first two characters, ignoring in/outdoor
//pCountry -- caller allocated buffer contain the country code that is checking against
//the one in pIes. It can be NULL.
//caller must provide pIes, it cannot be NULL
//This function always return TRUE if 11d support is not turned on.
//pIes cannot be NULL
tANI_BOOLEAN csrMatchCountryCode( tpAniSirGlobal pMac, tANI_U8 *pCountry, tDot11fBeaconIEs *pIes );
eHalStatus csrRoamSetKey( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamSetKey *pSetKey, tANI_U32 roamId );
eHalStatus csrRoamOpenSession(tpAniSirGlobal pMac,
                              csrRoamCompleteCallback callback,
                              void *pContext, tANI_U8 *pSelfMacAddr,
                              tANI_U8 *pbSessionId);
//fSync: TRUE means cleanupneeds to handle synchronously.
eHalStatus csrRoamCloseSession( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                tANI_BOOLEAN fSync, tANI_U8 bPurgeList,
                                csrRoamSessionCloseCallback callback,
                                void *pContext );
void csrPurgeSmeCmdList(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrCleanupSession(tpAniSirGlobal pMac, tANI_U32 sessionId);
eHalStatus csrRoamGetSessionIdFromBSSID( tpAniSirGlobal pMac, tCsrBssid *bssid, tANI_U32 *pSessionId );
eCsrCfgDot11Mode csrFindBestPhyMode( tpAniSirGlobal pMac, tANI_U32 phyMode );

/* ---------------------------------------------------------------------------
    \fn csrScanEnable
    \brief Enable the scanning feature of CSR. It must be called before any scan request can be performed.
    \param tHalHandle - HAL context handle
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanEnable(tpAniSirGlobal);

/* ---------------------------------------------------------------------------
    \fn csrScanDisable
    \brief Disableing the scanning feature of CSR. After this function return success, no scan is performed until 
a successfull to csrScanEnable
    \param tHalHandle - HAL context handle
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanDisable(tpAniSirGlobal);
/* ---------------------------------------------------------------------------
    \fn csrScanRequest
    \brief Request a 11d or full scan.
    \param pScanRequestID - pointer to an object to get back the request ID
    \param callback - a callback function that scan calls upon finish, will not be called if csrScanRequest returns error
    \param pContext - a pointer passed in for the callback
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanRequest(tpAniSirGlobal, tANI_U16, tCsrScanRequest *,
                   tANI_U32 *pScanRequestID, csrScanCompleteCallback callback,
                   void *pContext);

/* ---------------------------------------------------------------------------
    \fn csrScanAbort
    \brief If a scan request is abort, the scan complete callback will be called first before csrScanAbort returns.
    \param pScanRequestID - The request ID returned from csrScanRequest
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanAbort(tpAniSirGlobal, tANI_U32 scanRequestID);

eHalStatus csrScanSetBGScanparams(tpAniSirGlobal, tCsrBGScanRequest *);
eHalStatus csrScanBGScanAbort(tpAniSirGlobal);

/* ---------------------------------------------------------------------------
    \fn csrScanGetResult
    \brief Return scan results.
    \param pFilter - If pFilter is NULL, all cached results are returned
    \param phResult - an object for the result.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanGetResult(tpAniSirGlobal, tCsrScanResultFilter *pFilter, tScanResultHandle *phResult);

#ifdef FEATURE_WLAN_LFR
/* ---------------------------------------------------------------------------
    \fn csrAddChannelToOccupiedChannelList
    \brief Add channel no given by fast reassoc cmd into occ chn list
    \param channel - channel no passed by fast reassoc cmd
    \return void
  -------------------------------------------------------------------------------*/
void csrAddChannelToOccupiedChannelList(tpAniSirGlobal pMac, tANI_U8 channel);
#endif
/* ---------------------------------------------------------------------------
    \fn csrScanFlushResult
    \brief Clear scan results.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanFlushResult(tpAniSirGlobal);
/* ---------------------------------------------------------------------------
 *  \fn csrScanFilterResults
 *  \brief Filter scan results based on valid channel list.
 *  \return eHalStatus
 *-------------------------------------------------------------------------------
 */
eHalStatus csrScanFilterResults(tpAniSirGlobal pMac);

/* ---------------------------------------------------------------------------
 *  \fn csrScanFilterDFSResults
 *  \brief Filter BSSIDs on DFS channels from the scan results.
 *  \return eHalStatus
 *-------------------------------------------------------------------------------
 */
eHalStatus csrScanFilterDFSResults(tpAniSirGlobal pMac);

eHalStatus csrScanFlushSelectiveResult(tpAniSirGlobal, v_BOOL_t flushP2P);

eHalStatus csrScanFlushSelectiveResultForBand(tpAniSirGlobal, v_BOOL_t flushP2P, tSirRFBand band);

/* ---------------------------------------------------------------------------
    \fn csrScanBGScanGetParam
    \brief Returns the current background scan settings.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanBGScanGetParam(tpAniSirGlobal, tCsrBGScanRequest *);

/* ---------------------------------------------------------------------------
    \fn csrScanResultGetFirst
    \brief Returns the first element of scan result.
    \param hScanResult - returned from csrScanGetResult
    \return tCsrScanResultInfo * - NULL if no result     
  -------------------------------------------------------------------------------*/
tCsrScanResultInfo *csrScanResultGetFirst(tpAniSirGlobal, tScanResultHandle hScanResult);
/* ---------------------------------------------------------------------------
    \fn csrScanResultGetNext
    \brief Returns the next element of scan result. It can be called without calling csrScanResultGetFirst first
    \param hScanResult - returned from csrScanGetResult
    \return Null if no result or reach the end     
  -------------------------------------------------------------------------------*/
tCsrScanResultInfo *csrScanResultGetNext(tpAniSirGlobal, tScanResultHandle hScanResult);

/* ---------------------------------------------------------------------------
    \fn csrGetCountryCode
    \brief this function is to get the country code current being used
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return, this has the country code
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrGetCountryCode(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U8 *pbLen);

/* ---------------------------------------------------------------------------
    \fn csrSetCountryCode
    \brief this function is to set the country code so channel/power setting matches the countrycode and
    the domain it belongs to.
    \param pCountry - Caller allocated buffer with at least 3 bytes specifying the country code
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether 
    a restart is needed to apply the change
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrSetCountryCode(tpAniSirGlobal pMac, tANI_U8 *pCountry, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------
    \fn csrResetCountryCodeInformation
    \brief this function is to reset the country code current being used back to EEPROM default
    this includes channel list and power setting.
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether 
    a restart is needed to apply the change
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrResetCountryCodeInformation(tpAniSirGlobal pMac, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------
    \fn csrGetSupportedCountryCode
    \brief this function is to get a list of the country code current being supported
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return, 
    this has the country code list. 3 bytes for each country code. This may be NULL if
    caller wants to know the needed bytes.
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrGetSupportedCountryCode(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U32 *pbLen);

/* ---------------------------------------------------------------------------
    \fn csrSetRegulatoryDomain
    \brief this function is to set the current regulatory domain.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    CSR.
    \param domainId - indicate the domain (defined in the driver) needs to set to.  
    See eRegDomainId for definition
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether 
    a restart is needed to apply the change
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrSetRegulatoryDomain(tpAniSirGlobal pMac, v_REGDOMAIN_t domainId, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------
    \fn csrGetCurrentRegulatoryDomain
    \brief this function is to get the current regulatory domain.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    CSR.
    \return eRegDomainId     
  -------------------------------------------------------------------------------*/
v_REGDOMAIN_t csrGetCurrentRegulatoryDomain(tpAniSirGlobal pMac);

/* ---------------------------------------------------------------------------
    \fn csrGetRegulatoryDomainForCountry
    \brief this function is to get the regulatory domain for a country.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    CSR.
    \param pCountry - Caller allocated buffer with at least 3 bytes specifying the country code
    \param pDomainId - Caller allocated buffer to get the return domain ID upon success return. Can be NULL.
    \param source - the source of country information.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrGetRegulatoryDomainForCountry(tpAniSirGlobal pMac,
                                            tANI_U8 *pCountry,
                                            v_REGDOMAIN_t *pDomainId,
                                            v_CountryInfoSource_t source);


tANI_BOOLEAN csrSave11dCountryString( tpAniSirGlobal pMac, tANI_U8 *pCountryCode, tANI_BOOLEAN fForce );

//some support functions
tANI_BOOLEAN csrIs11dSupported(tpAniSirGlobal pMac);
tANI_BOOLEAN csrIs11hSupported(tpAniSirGlobal pMac);
tANI_BOOLEAN csrIs11eSupported(tpAniSirGlobal pMac);
tANI_BOOLEAN csrIsWmmSupported(tpAniSirGlobal pMac);
tANI_BOOLEAN csrIsMCCSupported(tpAniSirGlobal pMac);

//Upper layer to get the list of the base channels to scan for passively 11d info from csr
eHalStatus csrScanGetBaseChannels( tpAniSirGlobal pMac, tCsrChannelInfo * pChannelInfo );
//Return SUCCESS is the command is queued, failed
eHalStatus csrQueueSmeCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fHighPriority );
tSmeCmd *csrGetCommandBuffer( tpAniSirGlobal pMac );
void csrReleaseCommand(tpAniSirGlobal pMac, tSmeCmd *pCommand);
#ifdef FEATURE_WLAN_WAPI
tANI_BOOLEAN csrIsProfileWapi( tCsrRoamProfile *pProfile );
#endif /* FEATURE_WLAN_WAPI */

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR

//Security
#define WLAN_SECURITY_EVENT_SET_PTK_REQ     1
#define WLAN_SECURITY_EVENT_SET_PTK_RSP     2
#define WLAN_SECURITY_EVENT_SET_GTK_REQ     3
#define WLAN_SECURITY_EVENT_SET_GTK_RSP     4
#define WLAN_SECURITY_EVENT_REMOVE_KEY_REQ  5
#define WLAN_SECURITY_EVENT_REMOVE_KEY_RSP  6
#define WLAN_SECURITY_EVENT_PMKID_CANDIDATE_FOUND  7
#define WLAN_SECURITY_EVENT_PMKID_UPDATE    8
#define WLAN_SECURITY_EVENT_MIC_ERROR       9   

#define AUTH_OPEN       0
#define AUTH_SHARED     1
#define AUTH_WPA_EAP    2
#define AUTH_WPA_PSK    3
#define AUTH_WPA2_EAP   4
#define AUTH_WPA2_PSK   5
#ifdef FEATURE_WLAN_WAPI
#define AUTH_WAPI_CERT  6
#define AUTH_WAPI_PSK   7
#endif /* FEATURE_WLAN_WAPI */

#define ENC_MODE_OPEN   0
#define ENC_MODE_WEP40  1
#define ENC_MODE_WEP104 2
#define ENC_MODE_TKIP   3
#define ENC_MODE_AES    4
#ifdef FEATURE_WLAN_WAPI
#define ENC_MODE_SMS4   5 //WAPI
#endif /* FEATURE_WLAN_WAPI */

#define NO_MATCH    0
#define MATCH       1

#define WLAN_SECURITY_STATUS_SUCCESS        0
#define WLAN_SECURITY_STATUS_FAILURE        1

//Scan
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_REQ     1
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_RSP     2
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_REQ    3
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_RSP    4
#define WLAN_SCAN_EVENT_HO_SCAN_REQ         5
#define WLAN_SCAN_EVENT_HO_SCAN_RSP         6

#define WLAN_SCAN_STATUS_SUCCESS        0
#define WLAN_SCAN_STATUS_FAILURE        1
#define WLAN_SCAN_STATUS_ABORT          2

//Ibss
#define WLAN_IBSS_EVENT_START_IBSS_REQ      0
#define WLAN_IBSS_EVENT_START_IBSS_RSP      1
#define WLAN_IBSS_EVENT_JOIN_IBSS_REQ       2
#define WLAN_IBSS_EVENT_JOIN_IBSS_RSP       3
#define WLAN_IBSS_EVENT_COALESCING          4
#define WLAN_IBSS_EVENT_PEER_JOIN           5
#define WLAN_IBSS_EVENT_PEER_LEAVE          6
#define WLAN_IBSS_EVENT_STOP_REQ            7
#define WLAN_IBSS_EVENT_STOP_RSP            8

#define AUTO_PICK       0
#define SPECIFIED       1

#define WLAN_IBSS_STATUS_SUCCESS        0
#define WLAN_IBSS_STATUS_FAILURE        1

//11d
#define WLAN_80211D_EVENT_COUNTRY_SET   0
#define WLAN_80211D_EVENT_RESET         1

#define WLAN_80211D_DISABLED         0
#define WLAN_80211D_SUPPORT_MULTI_DOMAIN     1
#define WLAN_80211D_NOT_SUPPORT_MULTI_DOMAIN     2

int diagAuthTypeFromCSRType(eCsrAuthType authType);
int diagEncTypeFromCSRType(eCsrEncryptionType encType);
#endif //#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/* ---------------------------------------------------------------------------
    \fn csrScanResultPurge
    \brief remove all items(tCsrScanResult) in the list and free memory for each item
    \param hScanResult - returned from csrScanGetResult. hScanResult is considered gone by 
    calling this function and even before this function reutrns.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrScanResultPurge(tpAniSirGlobal pMac, tScanResultHandle hScanResult);


///////////////////////////////////////////Common Scan ends

/* ---------------------------------------------------------------------------
    \fn csrRoamConnect
    \brief To inititiate an association
    \param pProfile - can be NULL to join to any open ones
    \param hBssListIn - a list of BSS descriptor to roam to. It is returned from csrScanGetResult
    \param pRoamId - to get back the request ID
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamConnect(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                          tScanResultHandle hBssListIn, tANI_U32 *pRoamId);

/* ---------------------------------------------------------------------------
    \fn csrRoamReassoc
    \brief To inititiate a re-association
    \param pProfile - can be NULL to join the currently connected AP. In that 
    case modProfileFields should carry the modified field(s) which could trigger
    reassoc  
    \param modProfileFields - fields which are part of tCsrRoamConnectedProfile 
    that might need modification dynamically once STA is up & running and this 
    could trigger a reassoc
    \param pRoamId - to get back the request ID
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                          tCsrRoamModifyProfileFields modProfileFields,
                          tANI_U32 *pRoamId);


/* ---------------------------------------------------------------------------
    \fn csrRoamReconnect
    \brief To disconnect and reconnect with the same profile
    \return eHalStatus. It returns fail if currently not connected     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamReconnect(tpAniSirGlobal pMac, tANI_U32 sessionId);

/* ---------------------------------------------------------------------------
    \fn csrRoamSetPMKIDCache
    \brief return the PMKID candidate list
    \param pPMKIDCache - caller allocated buffer point to an array of tPmkidCacheInfo
    \param numItems - a variable that has the number of tPmkidCacheInfo allocated
    when retruning, this is either the number needed or number of items put into pPMKIDCache
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough and pNumItems
    has the number of tPmkidCacheInfo.
    \Note: pNumItems is a number of tPmkidCacheInfo, not sizeof(tPmkidCacheInfo) * something
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamSetPMKIDCache( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                 tPmkidCacheInfo *pPMKIDCache,
                                 tANI_U32 numItems,
                                 tANI_BOOLEAN update_entire_cache );

/* ---------------------------------------------------------------------------
    \fn csrRoamGetWpaRsnReqIE
    \brief return the WPA or RSN IE CSR passes to PE to JOIN request or START_BSS request
    \param pLen - caller allocated memory that has the length of pBuf as input. Upon returned, *pLen has the 
    needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamGetWpaRsnReqIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetWpaRsnRspIE
    \brief return the WPA or RSN IE from the beacon or probe rsp if connected
    \param pLen - caller allocated memory that has the length of pBuf as input. Upon returned, *pLen has the 
    needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamGetWpaRsnRspIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf);


/* ---------------------------------------------------------------------------
    \fn csrRoamGetNumPMKIDCache
    \brief return number of PMKID cache entries
    \return tANI_U32 - the number of PMKID cache entries
  -------------------------------------------------------------------------------*/
tANI_U32 csrRoamGetNumPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetPMKIDCache
    \brief return PMKID cache from CSR
    \param pNum - caller allocated memory that has the space of the number of pBuf tPmkidCacheInfo as input. Upon returned, *pNum has the 
    needed or actually number in tPmkidCacheInfo.
    \param pPmkidCache - Caller allocated memory that contains PMKID cache, if any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamGetPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId, 
                                tANI_U32 *pNum, tPmkidCacheInfo *pPmkidCache);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetConnectProfile
    \brief To return the current connect profile. Caller must call csrRoamFreeConnectProfile
           after it is done and before reuse for another csrRoamGetConnectProfile call.
    \param pProfile - pointer to a caller allocated structure tCsrRoamConnectedProfile
    \return eHalStatus. Failure if not connected     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamGetConnectProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, 
                                    tCsrRoamConnectedProfile *pProfile);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetConnectState
    \brief To return the current connect state of Roaming
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamGetConnectState(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrConnectState *pState);

/* ---------------------------------------------------------------------------
    \fn csrRoamFreeConnectProfile
    \brief To free and reinitialize the profile return previous by csrRoamGetConnectProfile.
    \param pProfile - pointer to a caller allocated structure tCsrRoamConnectedProfile
    \return eHalStatus.      
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamFreeConnectProfile(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pProfile);

/* ---------------------------------------------------------------------------
    \fn csrInitChannelList
    \brief HDD calls this function to set the WNI_CFG_VALID_CHANNEL_LIST base on the band/mode settings.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    CSR.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrInitChannelList( tHalHandle hHal );

/* ---------------------------------------------------------------------------
    \fn csrChangeConfigParams
    \brief The CSR API exposed for HDD to provide config params to CSR during 
    SMEs stop -> start sequence.
    If HDD changed the domain that will cause a reset. This function will 
    provide the new set of 11d information for the new domain. Currrently this
    API provides info regarding 11d only at reset but we can extend this for
    other params (PMC, QoS) which needs to be initialized again at reset.
    \param 
    hHal - Handle to the HAL. The HAL handle is returned by the HAL after it is 
           opened (by calling halOpen).
    pUpdateConfigParam - a pointer to a structure (tCsrUpdateConfigParam) that 
                currently provides 11d related information like Country code, 
                Regulatory domain, valid channel list, Tx power per channel, a 
                list with active/passive scan allowed per valid channel. 

    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus csrChangeConfigParams(tpAniSirGlobal pMac, 
                                 tCsrUpdateConfigParam *pUpdateConfigParam);

/* ---------------------------------------------------------------------------
    \fn csrRoamConnectToLastProfile
    \brief To disconnect and reconnect with the same profile
    \return eHalStatus. It returns fail if currently connected     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamConnectToLastProfile(tpAniSirGlobal pMac, tANI_U32 sessionId);

/* ---------------------------------------------------------------------------
    \fn csrRoamDisconnect
    \brief To disconnect from a network
    \param reason -- To indicate the reason for disconnecting. Currently, only eCSR_DISCONNECT_REASON_MIC_ERROR is meanful.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrRoamDisconnect(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason);

/* ---------------------------------------------------------------------------
    \fn csr_abortConnection
    \brief To disconnect from a connecting network
    \retutn void.
----------------------------------------------------------------------------*/

void csr_abortConnection(tpAniSirGlobal pMac, tANI_U32 sessionId);

/* ---------------------------------------------------------------------------
    \fn csrScanGetPMKIDCandidateList
    \brief return the PMKID candidate list
    \param pPmkidList - caller allocated buffer point to an array of tPmkidCandidateInfo
    \param pNumItems - pointer to a variable that has the number of tPmkidCandidateInfo allocated
    when retruning, this is either the number needed or number of items put into pPmkidList
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough and pNumItems
    has the number of tPmkidCandidateInfo.
    \Note: pNumItems is a number of tPmkidCandidateInfo, not sizeof(tPmkidCandidateInfo) * something
  -------------------------------------------------------------------------------*/
eHalStatus csrScanGetPMKIDCandidateList(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                        tPmkidCandidateInfo *pPmkidList, tANI_U32 *pNumItems );

//This function is used to stop a BSS. It is similar of csrRoamIssueDisconnect but this function
//doesn't have any logic other than blindly trying to stop BSS
eHalStatus csrRoamIssueStopBssCmd( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_BOOLEAN fHighPriority );

void csrCallRoamingCompletionCallback(tpAniSirGlobal pMac, tCsrRoamSession *pSession, 
                                      tCsrRoamInfo *pRoamInfo, tANI_U32 roamId, eCsrRoamResult roamResult);

/* ---------------------------------------------------------------------------
    \fn csrRoamIssueDisassociateStaCmd
    \brief csr function that HDD calls to disassociate a associated station
    \param sessionId    - session Id for Soft AP
    \param pPeerMacAddr - MAC of associated station to delete
    \param reason - reason code, be one of the tSirMacReasonCodes
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueDisassociateStaCmd( tpAniSirGlobal pMac, 
                                           tANI_U32 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                           const tANI_U8 *pPeerMacAddr,
#else
                                           tANI_U8 *pPeerMacAddr,
#endif
                                           tANI_U32 reason);

/* ---------------------------------------------------------------------------
    \fn csrRoamIssueDeauthSta
    \brief csr function that HDD calls to delete a associated station
    \param sessionId    - session Id for Soft AP
    \param pDelStaParams- Pointer to parameters of the station to deauthenticate
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueDeauthStaCmd( tpAniSirGlobal pMac, 
                                     tANI_U32 sessionId,
                                     struct tagCsrDelStaParams *pDelStaParams);

/* ---------------------------------------------------------------------------
    \fn csrRoamIssueTkipCounterMeasures
    \brief csr function that HDD calls to start and stop tkip countermeasures
    \param sessionId - session Id for Soft AP
    \param bEnable   - Flag to start/stop countermeasures
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueTkipCounterMeasures( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_BOOLEAN bEnable);

eHalStatus csrSendMBTkipCounterMeasuresReqMsg( tpAniSirGlobal pMac, tANI_U32 sessinId, tANI_BOOLEAN bEnable, tSirMacAddr bssId );

/* ---------------------------------------------------------------------------
    \fn csrRoamGetAssociatedStas
    \brief csr function that HDD calls to get list of associated stations based on module ID
    \param sessionId - session Id for Soft AP
    \param modId - module ID - PE/HAL/TL
    \param pUsrContext - Opaque HDD context
    \param pfnSapEventCallback - Sap event callback in HDD
    \param pAssocStasBuf - Caller allocated memory to be filled with associatd stations info
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamGetAssociatedStas( tpAniSirGlobal pMac, tANI_U32 sessionId, VOS_MODULE_ID modId,
                                     void *pUsrContext, void *pfnSapEventCallback, tANI_U8 *pAssocStasBuf );

eHalStatus csrSendMBGetAssociatedStasReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, VOS_MODULE_ID modId,  tSirMacAddr bssId,
                                             void *pUsrContext, void *pfnSapEventCallback, tANI_U8 *pAssocStasBuf );

/* ---------------------------------------------------------------------------
    \fn csrRoamGetWpsSessionOverlap
    \brief csr function that HDD calls to get WPS PBC session overlap information
    \param sessionId - session Id for Soft AP
    \param pUsrContext - Opaque HDD context
    \param pfnSapEventCallback - Sap event callback in HDD
    \param pRemoveMac - pointer to MAC address of session to be removed
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamGetWpsSessionOverlap( tpAniSirGlobal pMac, tANI_U32 sessionId,
                             void *pUsrContext, void *pfnSapEventCallback,v_MACADDR_t pRemoveMac );
                                        
eHalStatus csrSendMBGetWPSPBCSessions( tpAniSirGlobal pMac, tANI_U32 sessionId,
                            tSirMacAddr bssId, void *pUsrContext, void *pfnSapEventCallback,v_MACADDR_t pRemoveMac);

/* ---------------------------------------------------------------------------
    \fn csrSendChngMCCBeaconInterval
    \brief csr function that HDD calls to send Update beacon interval
    \param sessionId - session Id for Soft AP
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus
csrSendChngMCCBeaconInterval(tpAniSirGlobal pMac, tANI_U32 sessionId);

#ifdef FEATURE_WLAN_BTAMP_UT_RF
eHalStatus csrRoamStartJoinRetryTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval);
eHalStatus csrRoamStopJoinRetryTimer(tpAniSirGlobal pMac, tANI_U32 sessionId);
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
/* ---------------------------------------------------------------------------
    \fn csrRoamFTPreAuthRspProcessor
    \brief csr function that handles pre auth response from LIM 
  ---------------------------------------------------------------------------*/
void csrRoamFTPreAuthRspProcessor( tHalHandle hHal, tpSirFTPreAuthRsp pFTPreAuthRsp );
#endif

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
void csrEseSendAdjacentApRepMsg(tpAniSirGlobal pMac, tCsrRoamSession *pSession);
#endif

#if defined(FEATURE_WLAN_ESE)
void UpdateCCKMTSF(tANI_U32 *timeStamp0, tANI_U32 *timeStamp1, tANI_U32 *incr);
#endif

eHalStatus csrGetDefaultCountryCodeFrmNv(tpAniSirGlobal pMac, tANI_U8 *pCountry);
eHalStatus csrGetCurrentCountryCode(tpAniSirGlobal pMac, tANI_U8 *pCountry);


eHalStatus csrRoamEnqueuePreauth(tpAniSirGlobal pMac, tANI_U32 sessionId, tpSirBssDescription pBssDescription,
                                eCsrRoamReason reason, tANI_BOOLEAN fImmediate);
eHalStatus csrRoamDequeuePreauth(tpAniSirGlobal pMac);
#ifdef FEATURE_WLAN_LFR
void csrInitOccupiedChannelsList(tpAniSirGlobal pMac);
tANI_BOOLEAN csrNeighborRoamIsNewConnectedProfile(tpAniSirGlobal pMac);
tANI_BOOLEAN csrNeighborRoamConnectedProfileMatch(tpAniSirGlobal pMac, tCsrScanResult *pResult,
                                                  tDot11fBeaconIEs *pIes);
#endif
eHalStatus csrSetTxPower(tpAniSirGlobal pMac, v_U8_t sessionId, v_U8_t mW);
eHalStatus csrHT40StopOBSSScan(tpAniSirGlobal pMac, v_U8_t sessionId);

eHalStatus csrScanCreateEntryInScanCache(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                         tCsrBssid bssid, tANI_U8 channel);

eHalStatus csrUpdateChannelList(tpAniSirGlobal pMac);
eHalStatus csrRoamDelPMKIDfromCache( tpAniSirGlobal pMac, tANI_U32 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                     const tANI_U8 *pBSSId,
#else
                                     tANI_U8 *pBSSId,
#endif
                                     tANI_BOOLEAN flush_cache );
tANI_BOOLEAN csrElectedCountryInfo(tpAniSirGlobal pMac);
void csrAddVoteForCountryInfo(tpAniSirGlobal pMac, tANI_U8 *pCountryCode);
void csrClearVotesForCountryInfo(tpAniSirGlobal pMac);
#ifdef WLAN_FEATURE_AP_HT40_24G
eHalStatus csrSetHT2040Mode(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U8 cbMode);
#endif
#endif

