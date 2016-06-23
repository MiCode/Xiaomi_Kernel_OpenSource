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

/******************************************************************************
*
* Name:  p2p_Api.h
*
* Description: P2P FSM defines.
*
*
******************************************************************************/

#ifndef __P2P_API_H__
#define __P2P_API_H__

#include "vos_types.h"
#include "halTypes.h"
#include "palTimer.h"
#include "vos_lock.h"

typedef struct sP2pPsConfig{
  tANI_U8   opp_ps;
  tANI_U32  ctWindow;
  tANI_U8   count; 
  tANI_U32  duration;
  tANI_U32  interval;
  tANI_U32  single_noa_duration;
  tANI_U8   psSelection;
  tANI_U8   sessionid;   
}tP2pPsConfig,*tpP2pPsConfig;

typedef eHalStatus (*remainOnChanCallback)( tHalHandle, void* context, 
                                            eHalStatus status );

typedef struct sRemainOnChn{
    tANI_U8 chn;
    tANI_U32 duration;
    remainOnChanCallback callback;
  void *pCBContext;
}tRemainOnChn, tpRemainOnChn;

#define SIZE_OF_NOA_DESCRIPTOR 13
#define MAX_NOA_PERIOD_IN_MICROSECS 3000000

#define P2P_CLEAR_POWERSAVE 0
#define P2P_OPPORTUNISTIC_PS 1
#define P2P_PERIODIC_NOA 2
#define P2P_SINGLE_NOA 4

#ifdef WLAN_FEATURE_P2P_INTERNAL

#define MAX_SOCIAL_CHANNELS 3
#define P2P_OPERATING_CHANNEL 6
#define P2P_MAX_GROUP_LIMIT 5
#define P2P_MAC_ADDRESS_LEN 6
#define MAX_LISTEN_SEARCH_CYCLE 3
#define P2P_LISTEN_TIMEOUT_AUTO    500 //0.5 sec
#define P2P_LISTEN_TIMEOUT_HIGH    200 //0.4 sec
#define P2P_LISTEN_TIMEOUT         1000  //1 sec
#define P2P_REMAIN_ON_CHAN_TIMEOUT 300
#define P2P_REMAIN_ON_CHAN_TIMEOUT_HIGH 1000
#define P2P_REMAIN_ON_CHAN_TIMEOUT_LOW 100
#define ACTION_FRAME_RETRY_TIMEOUT 50
#define P2P_COUNTRY_CODE_LEN 3

/* Wi-Fi Direct Device Discovery Type */
typedef enum ep2pDiscoverType {
   /** Driver must perform device discovery only using the scan phase*/
   WFD_DISCOVER_TYPE_SCAN_ONLY = 1,
   /** Driver must perform device discovery only using the find phase*/
   WFD_DISCOVER_TYPE_FIND_ONLY = 2,
   /** Driver can use either use scan phase or find phase to discovery 
   P2P devices. In our case Driver uses scan phase */
   WFD_DISCOVER_TYPE_AUTO = 3,
   /*Scan only social channel*/
   WFD_DISCOVER_SCAN_ONLY_SOCIAL_CHN,
   /** If it is set, driver must perform a complete discovery, 
   If it is false, it can do partial discovery.*/
   WFD_DISCOVER_TYPE_FORCED = 0x80000000
} ep2pDiscoverType, *ePp2pDiscoverType;

//bit mask for what to discover
#define QCWLAN_P2P_DISCOVER_DEVICE     0x1
#define QCWLAN_P2P_DISCOVER_GO         0x2
#define QCWLAN_P2P_DISCOVER_ANY        0x8

#define P2P_DISCOVER_SCAN_ONLY(t) ( (WFD_DISCOVER_TYPE_SCAN_ONLY == (t)) \
                                    || (WFD_DISCOVER_SCAN_ONLY_SOCIAL_CHN == (t)) )

/* Scan Type */
typedef enum ep2pScanType {
   P2P_SCAN_TYPE_ACTIVE = 1,  /**  device should perform active scans for the scan phase of device discovery */
   P2P_SCAN_TYPE_PASSIVE = 2, /** device should perform passive scanning for the scan phase of device discovery  */
   P2P_SCAN_TYPE_AUTO = 3     /** The selection of the scan type is upto the driver */
} ep2pScanType, * ePp2pScanType;

/** Listen State Discoverability */
typedef enum ep2pListenStateDiscoverability {
   P2P_DEVICE_NOT_DISCOVERABLE, /**  Wi-Fi Direct Device Port must not make itself discoverable */
   P2P_DEVICE_AUTO_AVAILABILITY, /** Wi-Fi Direct Device Port must periodically put itself in the listen state to become discoverable*/
   P2P_DEVICE_HIGH_AVAILABILITY  /** Wi-Fi Direct Device Port must be frequently put itself in the listen state 
                                 to increase the speed and reliability of remote devices discovering it */
} ep2pListenStateDiscoverability, * ePp2pListenStateDiscoverability;

typedef enum ep2pOperatingMode {
   OPERATION_MODE_INVALID,
   OPERATION_MODE_P2P_DEVICE,
   OPERATION_MODE_P2P_GROUP_OWNER,
   OPERATION_MODE_P2P_CLIENT
}ep2pOperatingMode;

typedef struct _tp2pDiscoverDeviceFilter{ 
   tSirMacAddr DeviceID; 
   v_UCHAR_t ucBitmask; 
   tSirMacSSid GroupSSID; 
} tp2pDiscoverDeviceFilter;

typedef struct _tp2pDiscoverRequest {
   ep2pDiscoverType discoverType;
   ep2pScanType scanType;
   tANI_U32 uDiscoverTimeout;
   tANI_U32 uNumDeviceFilters;
   tp2pDiscoverDeviceFilter *pDeviceFilters;
   tANI_BOOLEAN bForceScanLegacyNetworks;
   tANI_U32 uNumOfLegacySSIDs;
   tANI_U8  *pLegacySSIDs;
   tANI_U32 uIELen;
   tANI_U8 *pIEField;
} tP2PDiscoverRequest;

typedef enum _eP2PDiscoverStatus {
   eP2P_DISCOVER_SUCCESS,
   eP2P_DISCOVER_FAILURE,
   eP2P_DISCOVER_ABORT,
   eP2P_DIRECTED_DISCOVER
} eP2PDiscoverStatus;

typedef eHalStatus (*p2pDiscoverCompleteCallback)(tHalHandle hHal, void *pContext, eP2PDiscoverStatus discoverStatus);

typedef struct sP2PGroupId {
    tANI_U8 present;
    tANI_U8 deviceAddress[6];
    tANI_U8 num_ssid;
    tANI_U8 ssid[32];
} tP2PGroupId;

typedef struct sP2PGroupBssid {
    tANI_U8 present;
    tANI_U8 P2PGroupBssid[6];
} tP2PGroupBssid;

typedef struct sP2PChannel {
   tANI_U8 present;
   tANI_U8 countryString[P2P_COUNTRY_CODE_LEN];
   tANI_U8 regulatoryClass;
   tANI_U8 channel;
} tP2P_OperatingChannel, tP2P_ListenChannel;

/** Structure contains parameters required for Wi-Fi Direct Device functionality such as device discovery, Group Owner Negotiation */
typedef enum P2PFrameType {
   eP2P_INVALID_FRM,
   eP2P_PROBE_REQ,
   eP2P_PROBE_RSP,
   eP2P_GONEGO_REQ,
   eP2P_GONEGO_RES,
   eP2P_GONEGO_CNF,
   eP2P_PROVISION_DISCOVERY_REQUEST,
   eP2P_PROVISION_DISCOVERY_RESPONSE,
   eP2P_BEACON,
   eP2P_GROUP_ID,
   eP2P_ASSOC_REQ,
   eP2P_INVITATION_REQ,
   eP2P_INVITATION_RSP,
   eP2P_DEVICE_DISCOVERY_REQ,
   eP2P_DEVICE_DISCOVERY_RSP,
} eP2PFrameType;

typedef enum P2PRequest {
   eWFD_DISCOVER_REQUEST,
   eWFD_DEVICE_ID,
   eWFD_DEVICE_CAPABILITY,
   eWFD_GROUP_OWNER_CAPABILITY,
   eWFD_DEVICE_INFO,
   eWFD_SECONDARY_DEVICE_TYPE_LIST,
   eWFD_ADDITIONAL_IE,
   eWFD_GROUP_ID,
   eWFD_SEND_GO_NEGOTIATION_REQUEST,
   eWFD_SEND_GO_NEGOTIATION_RESPONSE,
   eWFD_SEND_GO_NEGOTIATION_CONFIRMATION,
   eWFD_SEND_PROVISION_DISCOVERY_REQUEST,
   eWFD_SEND_PROVISION_DISCOVERY_RESPONSE,
   eWFD_SEND_INVITATION_REQUEST,
   eWFD_SEND_INVITATION_RESPONSE,
   eWFD_OPERATING_CHANNEL,
   eWFD_LISTEN_CHANNEL
} eP2PRequest;

typedef struct _p2p_device_capability_config {
   tANI_BOOLEAN bServiceDiscoveryEnabled;
   tANI_BOOLEAN bClientDiscoverabilityEnabled;
   tANI_BOOLEAN bConcurrentOperationSupported;
   tANI_BOOLEAN bInfrastructureManagementEnabled;
   tANI_BOOLEAN bDeviceLimitReached;
   tANI_BOOLEAN bInvitationProcedureEnabled;
   tANI_U32 WPSVersionsEnabled;
} tp2p_device_capability_config;

typedef struct _p2p_group_owner_capability_config {
   tANI_BOOLEAN bPersistentGroupEnabled;
   tANI_BOOLEAN bIntraBSSDistributionSupported;
   tANI_BOOLEAN bCrossConnectionSupported;
   tANI_BOOLEAN bPersistentReconnectSupported;
   tANI_BOOLEAN bGroupFormationEnabled;
   tANI_U32 uMaximumGroupLimit;
} tp2p_group_owner_capability_config;

typedef struct _tP2P_ProvDiscoveryReq {
   tANI_U8     dialogToken;
   tANI_U8  PeerDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U32 uSendTimeout;
   tANI_U8  GroupCapability;
   tP2PGroupId GroupId;
   tANI_BOOLEAN bUseGroupID;
   tANI_U32 uIELength;
   tANI_U8 *IEdata;
} tP2P_ProvDiscoveryReq;

typedef struct _tP2P_ProvDiscoveryRes {
   tANI_U8  dialogToken;
   tANI_U8  ReceiverDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U32 uSendTimeout;
   tANI_U32 uIELength;
   tANI_U8 *IEdata;
} tP2P_ProvDiscoveryRes;

typedef struct p2p_go_request {
   tANI_U8     dialogToken;
   tANI_U8    peerDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U32 uSendTimeout;
   tANI_U8 GoIntent;
   tANI_U32 GoTimeout;
   tANI_U32 ClientTimeout;
   tANI_U8    IntendedInterfaceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8 GroupCapability;
   tANI_U32 uIELength;
   tANI_U8 *IEdata;
} tP2P_go_request;

typedef struct p2p_go_confirm {
   tANI_U8    peerDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8    dialog_token;
   tANI_U32 uSendTimeout;
   tANI_U8    status;
   tANI_U8 GroupCapability;
   tP2PGroupId GroupId;
   tANI_BOOLEAN bUsedGroupId;
   tANI_U32 uIELength;
   tANI_U8 *IEdata;
} tP2P_go_confirm;

typedef struct p2p_go_response {
   tANI_U8    peerDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8    dialog_token;
   tANI_U32 uSendTimeout;
   tANI_U8    status;
   tANI_U8 GoIntent;
   tANI_U32 GoTimeout;
   tANI_U32 ClientTimeout;
   tANI_U8    IntendedInterfaceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8 GroupCapability;
   tP2PGroupId GroupId;
   tANI_BOOLEAN bUsedGroupId;
   tANI_U32 uIELength;
   tANI_U8 *IEdata;
} tP2P_go_response;

//Invitation Req parameters
typedef struct p2p_invitation_request {
    tANI_U8 DialogToken;
    tANI_U8 PeerDeviceAddress[P2P_MAC_ADDRESS_LEN];
    tANI_U32 uSendTimeout;
    tANI_U32 GoTimeout;
    tANI_U32 ClientTimeout; 
    tANI_U8 InvitationFlags;
    tP2PGroupBssid GroupBSSID;
    tP2P_OperatingChannel OperatingChannel;
    tP2PGroupId GroupID;
    tANI_U32 uIELength;
    tANI_U8 *IEdata;
} tP2P_invitation_request;


//Invitation Response parameters
typedef struct p2p_invitation_response {
    tANI_U8 ReceiverDeviceAddress[P2P_MAC_ADDRESS_LEN];
    tANI_U8 DialogToken;
    void* RequestContext;
    tANI_U32 uSendTimeout;
    tANI_U8     status;
    tANI_U32 GoTimeout;
    tANI_U32 ClientTimeout; 
    tP2PGroupBssid GroupBSSID;
    tP2P_OperatingChannel OperatingChannel;
    tANI_U32 uIELength;
    tANI_U8 *IEdata;
} tP2P_invitation_response;

typedef enum eOUISubType {
   eOUI_P2P_GONEGO_REQ,
   eOUI_P2P_GONEGO_RES,
   eOUI_P2P_GONEGO_CNF,
   eOUI_P2P_INVITATION_REQ,
   eOUI_P2P_INVITATION_RES,
   eOUI_P2P_DEVICE_DISCOVERABILITY_REQ,
   eOUI_P2P_DEVICE_DISCOVERABILITY_RES,
   eOUI_P2P_PROVISION_DISCOVERY_REQ,
   eOUI_P2P_PROVISION_DISCOVERY_RES,
   eOUI_P2P_INVALID
}eOUISubType;

typedef enum _eP2PPort {
   eP2PPortDevice,
   eP2PPortGroupOwner,
   eP2PPortClient
} eP2PPort;

typedef enum eListenDiscoverableState {
   eStateDisabled,
   eStateEnabled,

}eListenDiscoverableState;

typedef enum P2PRemainOnChnReason
{
   eP2PRemainOnChnReasonUnknown,
   eP2PRemainOnChnReasonDiscovery, //Part of the discovery (search and listen)
   eP2PRemainOnChnReasonSendFrame, //Found peer and before sending request frame
   eP2PRemainOnChnReasonListen,    //In listen-only mode
}eP2PRemainOnChnReason;

typedef struct sGroupFormationReq {
   tCsrBssid deviceAddress;
   tANI_U8 targetListenChannel;
   tANI_U8 persistent_group;
   tANI_U8 group_limit; /* may be internal */
   tANI_U8 GO_config_timeout;
   tANI_U8 CL_config_timeout;
   tANI_U8 GO_intent;
   tANI_U16 devicePasswdId;
   tSirMacAddr groupBssid;
   tSirMacSSid groupSsid;
}tGroupFormationReq;

typedef struct tP2PConfigParam
{
   v_U32_t P2POperatingChannel;
   v_U32_t P2PListenChannel;
   v_U32_t P2PPSSelection;
   v_U32_t P2POpPSCTWindow;
   v_U32_t P2PNoADuration;
   v_U32_t P2PNoACount;
   v_U32_t P2PNoAInterval;
}tP2PConfigParam;

#endif

typedef struct sp2pContext
{
   v_CONTEXT_t vosContext;
   tHalHandle hHal;
   tANI_U8 sessionId; //Session id corresponding to P2P. On windows it is same as HDD sessionid not sme sessionid.
   tANI_U8 SMEsessionId;
   tANI_U8 probeReqForwarding;
   tANI_U8 *probeRspIe;
   tANI_U32 probeRspIeLength;
#ifdef WLAN_FEATURE_P2P_INTERNAL
   tANI_U8 numClients;
   tANI_U32 maxGroupLimit;
   ep2pOperatingMode operatingmode;
   tANI_U8 state;
   tANI_U8 socialChannel[MAX_SOCIAL_CHANNELS];
   tANI_U8 currentSearchIndex;
   tANI_U8 listenIndex;
   tANI_U8 dialogToken;
   tANI_U8 receivedDialogToken;
   eOUISubType actionFrameOUI;
   eP2PFrameType actionFrameType;
   tANI_BOOLEAN actionFrameTimeout;
   tANI_U8 *pSentActionFrame;
   tANI_U32 ActionFrameLen;
   tANI_U32 ActionFrameSendTimeout;
   eListenDiscoverableState listenDiscoverableState;
   vos_timer_t listenTimerHandler;
   vos_timer_t WPSRegistrarCheckTimerHandler;
   tANI_U32 WPSRegistrarSet;
   tANI_U8 bWaitForWPSReady;
   tANI_U8 bInGroupFormation;
   vos_timer_t discoverTimer;
   vos_timer_t retryActionFrameTimer;
   vos_timer_t actionFrameTimer;
   tPalTimerHandle nextActionFrameTimer;
   tANI_U8 peerMacAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8 selfMacAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8 ReceiverDeviceAddress[P2P_MAC_ADDRESS_LEN];
   tANI_U8 listen_search_cycle;
   ep2pDiscoverType discoverType;
   ep2pScanType scanType;
   tANI_U32 uDiscoverTimeout;
   tp2pDiscoverDeviceFilter *directedDiscoveryFilter;
   tANI_U32 uNumDeviceFilters;
   //Number of deviceFilter directedDiscoveryFilter holds
   tANI_U32 uNumDeviceFilterAllocated; 
   tGroupFormationReq formationReq;
   tANI_U8 GroupFormationPending;
   tANI_BOOLEAN PeerFound;
   tANI_BOOLEAN directedDiscovery;
   tANI_U32 listenDuration;
   tANI_U32 expire_time;
   p2pDiscoverCompleteCallback p2pDiscoverCBFunc;
   void *pContext;
   tANI_BOOLEAN bForceScanLegacyNetworks;
   tANI_U8 *DiscoverReqIeField;
   tANI_U32 DiscoverReqIeLength;
   tANI_U8 *GoNegoReqIeField;
   tANI_U32 GoNegoReqIeLength;
   tANI_U8 *GoNegoResIeField;
   tANI_U32 GoNegoResIeLength;
   tANI_U8 *GoNegoCnfIeField;
   tANI_U32 GoNegoCnfIeLength;
   tANI_U8 *ProvDiscReqIeField;
   tANI_U32 ProvDiscReqIeLength;
   tANI_U8 *ProvDiscResIeField;
   tANI_U32 ProvDiscResIeLength;
   tANI_U8 *InvitationReqIeField;
   tANI_U32 InvitationReqIeLength;
   tANI_U8 *InvitationResIeField;
   tANI_U32 InvitationResIeLength;
   tANI_U32 DiscoverableCfg;
   vos_spin_lock_t lState;
   tANI_U8 *pNextActionFrm;
   tANI_U32 nNextFrmLen;
   tANI_U32 nNextFrameTimeOut;
   eP2PFrameType NextActionFrameType;
   tANI_U8 ssid[32];
   v_U32_t P2PListenChannel;
   v_U32_t P2POperatingChannel;
   tP2pPsConfig pNoA;
   tANI_U8 OriginalGroupCapability;
#endif
} tp2pContext, *tPp2pContext;

eHalStatus sme_RemainOnChannel( tHalHandle hHal, tANI_U8 sessionId,
                                tANI_U8 channel, tANI_U32 duration,
                                remainOnChanCallback callback,
                                void *pContext,
                                tANI_U8 isP2PProbeReqAllowed);
eHalStatus sme_ReportProbeReq( tHalHandle hHal, tANI_U8 flag );
eHalStatus sme_updateP2pIe( tHalHandle hHal, void *p2pIe, 
                            tANI_U32 p2pIeLength );
eHalStatus sme_sendAction( tHalHandle hHal, tANI_U8 sessionId,
                           const tANI_U8 *pBuf, tANI_U32 len,
                           tANI_U16 wait, tANI_BOOLEAN noack);
eHalStatus sme_CancelRemainOnChannel( tHalHandle hHal, tANI_U8 sessionId );
eHalStatus sme_p2pOpen( tHalHandle hHal );
eHalStatus p2pStop( tHalHandle hHal );
eHalStatus sme_p2pClose( tHalHandle hHal );
eHalStatus sme_p2pSetPs( tHalHandle hHal, tP2pPsConfig * data );
#ifdef WLAN_FEATURE_P2P_INTERNAL
eHalStatus p2pRemainOnChannel( tHalHandle hHal, tANI_U8 sessionId,
                               tANI_U8 channel, tANI_U32 duration,
                               remainOnChanCallback callback, void *pContext,
                               tANI_U8 isP2PProbeReqAllowed,
                               eP2PRemainOnChnReason reason);
#else
eHalStatus p2pRemainOnChannel( tHalHandle hHal, tANI_U8 sessionId,
                               tANI_U8 channel, tANI_U32 duration,
                               remainOnChanCallback callback,
                               void *pContext,
                               tANI_U8 isP2PProbeReqAllowed);
#endif
eHalStatus p2pSendAction( tHalHandle hHal, tANI_U8 sessionId,
                          const tANI_U8 *pBuf, tANI_U32 len,
                           tANI_U16 wait, tANI_BOOLEAN noack);
eHalStatus p2pCancelRemainOnChannel( tHalHandle hHal, tANI_U8 sessionId );
eHalStatus p2pSetPs( tHalHandle hHal, tP2pPsConfig *pNoA );
tSirRFBand GetRFBand(tANI_U8 channel);
#ifdef WLAN_FEATURE_P2P_INTERNAL
eHalStatus p2pRemainOnChannelCallback(tHalHandle halHandle, void *pContext, eHalStatus scan_status);
eHalStatus P2P_DiscoverRequest(tHalHandle hHal, tANI_U8 SessionID, tP2PDiscoverRequest *pDiscoverRequest, 
                               p2pDiscoverCompleteCallback callback, void *pContext);
tANI_U8 p2pGetDialogToken(tHalHandle hHal, tANI_U8 SessionID, eP2PFrameType actionFrameType);
eHalStatus P2P_ListenStateDiscoverable(tHalHandle hHal, tANI_U8 sessionId, ep2pListenStateDiscoverability listenState);
eHalStatus p2pCreateSendActionFrame(tHalHandle hHal, tANI_U8 SessionID, 
      void *p2pactionframe, eP2PFrameType actionFrameType, tANI_U32 timeout);
eHalStatus p2pScanRequest(tp2pContext *p2pContext, p2pDiscoverCompleteCallback callback, void *pContext);
void p2pActionFrameTimerHandler(void *pContext);
void p2pListenDiscoverTimerHandler(void *pContext);
void p2pDiscoverTimerHandler(void *pContext);
void p2pRetryActionFrameTimerHandler(void *pContext);
eHalStatus p2pGrpFormationRemainOnChanRspCallback(tHalHandle halHandle, void *pContext, tANI_U32 scanId, eCsrScanStatus scan_status);
eHalStatus p2pChangeDefaultConfigParam(tHalHandle hHal, tP2PConfigParam *pParam);
eHalStatus p2pGetConfigParam(tHalHandle hHal, tP2PConfigParam *pParam);
eHalStatus p2pPS(tHalHandle hHal, tANI_U8 sessionId);
eHalStatus p2pCloseSession(tHalHandle hHal, tANI_U8 SessionID);
eHalStatus p2pSetSessionId(tHalHandle hHal, tANI_U8 SessionID, tANI_U8 SmeSessionId);
tANI_BOOLEAN p2pIsOperatingChannEqualListenChann(tHalHandle hHal, tANI_U8 SessionID);
eHalStatus p2pGetListenChannel(tHalHandle hHal, tANI_U8 SessionID, tANI_U8 *channel);
eHalStatus p2pSetListenChannel(tHalHandle hHal, tANI_U8 SessionID, tANI_U8 channel);
eHalStatus p2pStopDiscovery(tHalHandle hHal, tANI_U8 SessionID);
tANI_U8 getP2PSessionIdFromSMESessionId(tHalHandle hHal, tANI_U8 SessionID);
void p2pCallDiscoverCallback(tp2pContext *p2pContext, eP2PDiscoverStatus statusCode);
eHalStatus p2pGetResultFilter(tp2pContext *pP2pContext,
                              tCsrScanResultFilter *pFilter);
#endif//INTERNAL
#endif //__P2P_API_H__
