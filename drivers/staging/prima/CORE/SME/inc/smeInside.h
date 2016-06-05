/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#if !defined( __SMEINSIDE_H )
#define __SMEINSIDE_H


/**=========================================================================
  
  \file  smeInside.h
  
  \brief prototype for SME structures and APIs used insside SME
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_status.h"
#include "vos_lock.h"
#include "vos_trace.h"
#include "vos_memory.h"
#include "vos_types.h"
#include "sirApi.h"
#include "csrInternal.h"
#include "sme_QosApi.h"
#include "smeQosInternal.h"


#ifdef FEATURE_OEM_DATA_SUPPORT
#include "oemDataInternal.h"
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "sme_RrmApi.h"
#endif


/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

#define SME_TOTAL_COMMAND  30
#define SME_START_CHAN_STEP 4

typedef struct sGenericPmcCmd
{
    tANI_U32 size;  //sizeof the data in the union, if any
    tRequestFullPowerReason fullPowerReason;
    tANI_BOOLEAN fReleaseWhenDone; //if TRUE, the command shall not put back to the queue, free te memory instead.
    union
    {
        tExitBmpsInfo exitBmpsInfo;
        tSirSmeWowlEnterParams enterWowlInfo;
    }u;
} tGenericPmcCmd;


typedef struct sGenericQosCmd
{
    sme_QosWmmTspecInfo tspecInfo;
    sme_QosEdcaAcType ac;
    v_U8_t tspec_mask;
} tGenericQosCmd;

typedef struct sRemainChlCmd
{
    tANI_U8 chn;
    tANI_U8 phyMode;
    tANI_U32 duration;
    tANI_U8 isP2PProbeReqAllowed;
    void* callback;
    void* callbackCtx;
}tRemainChlCmd;

typedef struct sNoACmd
{
    tP2pPsConfig NoA;
} tNoACmd;
#ifdef FEATURE_WLAN_TDLS
typedef struct TdlsSendMgmtInfo
{
  tSirMacAddr peerMac;
  tANI_U8 frameType;
  tANI_U8 dialog;
  tANI_U16 statusCode;
  tANI_U8 responder;
  tANI_U32 peerCapability;
  tANI_U8 *buf;
  tANI_U8 len;
} tTdlsSendMgmtCmdInfo;

typedef struct TdlsLinkEstablishInfo
{
  tSirMacAddr peerMac;
  tANI_U8 uapsdQueues;
  tANI_U8 maxSp;
  tANI_U8 isBufSta;
  tANI_U8 isOffChannelSupported;
  tANI_U8 isResponder;
  tANI_U8 supportedChannelsLen;
  tANI_U8 supportedChannels[SIR_MAC_MAX_SUPP_CHANNELS];
  tANI_U8 supportedOperClassesLen;
  tANI_U8 supportedOperClasses[SIR_MAC_MAX_SUPP_OPER_CLASSES];
} tTdlsLinkEstablishCmdInfo;

typedef struct TdlsAddStaInfo
{
  eTdlsAddOper tdlsAddOper;
  tSirMacAddr peerMac;
  tANI_U16  capability;
  tANI_U8   extnCapability[SIR_MAC_MAX_EXTN_CAP];
  tANI_U8   supportedRatesLen;
  tANI_U8   supportedRates[SIR_MAC_MAX_SUPP_RATES];
  tANI_U8    htcap_present;
  tSirHTCap  HTCap;
  tANI_U8    vhtcap_present;
  tSirVHTCap VHTCap;
  tANI_U8   uapsdQueues;
  tANI_U8   maxSp;
} tTdlsAddStaCmdInfo;

typedef struct TdlsDelStaInfo
{
  tSirMacAddr peerMac;
} tTdlsDelStaCmdInfo;

// tdlsoffchan
typedef struct TdlsChanSwitchInfo
{
  tSirMacAddr peerMac;
  tANI_U8 tdlsOffCh;
  tANI_U8 tdlsOffChBwOffset;
  tANI_U8 tdlsSwMode;
} tTdlsChanSwitchCmdInfo;

/*
 * TDLS cmd info, CMD from SME to PE.
 */
typedef struct s_tdls_cmd
{
  tANI_U32 size;
  union
  {
    tTdlsLinkEstablishCmdInfo tdlsLinkEstablishCmdInfo;
    tTdlsSendMgmtCmdInfo tdlsSendMgmtCmdInfo;
    tTdlsAddStaCmdInfo   tdlsAddStaCmdInfo;
    tTdlsDelStaCmdInfo   tdlsDelStaCmdInfo;
    tTdlsChanSwitchCmdInfo tdlsChanSwitchCmdInfo; //tdlsoffchan
  }u;
} tTdlsCmd;
#endif  /* FEATURE_WLAN_TDLS */

typedef struct tagSmeCmd
{
    tListElem Link;
    eSmeCommandType command;
    tANI_U32 sessionId;
    union
    {
        tScanCmd scanCmd;
        tRoamCmd roamCmd;
        tWmStatusChangeCmd wmStatusChangeCmd;
        tSetKeyCmd setKeyCmd;
        tRemoveKeyCmd removeKeyCmd;
        tGenericPmcCmd pmcCmd;
        tGenericQosCmd qosCmd;
#ifdef FEATURE_OEM_DATA_SUPPORT
        tOemDataCmd oemDataCmd;
#endif
        tRemainChlCmd remainChlCmd;
        tNoACmd NoACmd;
        tAddStaForSessionCmd addStaSessionCmd;
        tDelStaForSessionCmd delStaSessionCmd;
#ifdef FEATURE_WLAN_TDLS
        tTdlsCmd  tdlsCmd;
#endif
        tSirPNOScanReq pnoInfo;
        tSirSpoofMacAddrReq macAddrSpoofCmd;
        tAniGetFrameLogReq getFramelogCmd;
        struct s_ani_set_tx_max_pwr set_tx_max_pwr;
        tpNanRequest pNanReq;
    }u;
}tSmeCmd;



/*-------------------------------------------------------------------------- 
                         Internal to SME
  ------------------------------------------------------------------------*/

//To get a command buffer
//Return: NULL if there no more command buffer left
tSmeCmd *smeGetCommandBuffer( tpAniSirGlobal pMac );
void smePushCommand( tpAniSirGlobal pMac, tSmeCmd *pCmd, tANI_BOOLEAN fHighPriority );
void smeProcessPendingQueue( tpAniSirGlobal pMac );
void smeReleaseCommand(tpAniSirGlobal pMac, tSmeCmd *pCmd);
void purgeSmeSessionCmdList(tpAniSirGlobal pMac, tANI_U32 sessionId,
        tDblLinkList *pList);
tANI_BOOLEAN smeCommandPending(tpAniSirGlobal pMac);
tANI_BOOLEAN pmcProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
//this function is used to abort a command where the normal processing of the command
//is terminated without going through the normal path. it is here to take care of callbacks for
//the command, if applicable.
void pmcAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping );
tANI_BOOLEAN qosProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrIsValidChannel(tpAniSirGlobal pMac, tANI_U8 chnNum);
tANI_BOOLEAN csrRoamIsValid40MhzChannel(tpAniSirGlobal pMac, tANI_U8 channel);
eHalStatus csrProcessScanCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrRoamProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
void csrRoamProcessWmStatusChangeCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
void csrReinitRoamCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand); 
void csrReinitWmStatusChangeCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrReinitSetKeyCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrReinitRemoveKeyCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand);
eHalStatus csrRoamProcessSetKeyCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrRoamProcessRemoveKeyCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
void csrReleaseCommandSetKey(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrReleaseCommandRemoveKey(tpAniSirGlobal pMac, tSmeCmd *pCommand);
//eHalStatus csrRoamIssueSetKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamSetKey *pSetKey, tANI_U32 roamId );
eHalStatus csrRoamIssueRemoveKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                         tCsrRoamRemoveKey *pRemoveKey, tANI_U32 roamId );
eHalStatus csrIsFullPowerNeeded( tpAniSirGlobal pMac, tSmeCmd *pCommand, tRequestFullPowerReason *pReason,
                                 tANI_BOOLEAN *pfNeedPower);
void csrAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping );

eHalStatus sme_AcquireGlobalLock( tSmeStruct *psSme);
eHalStatus sme_ReleaseGlobalLock( tSmeStruct *psSme);

/* ---------------------------------------------------------------------------
    \fn sme_SetCfgScanControlList
    \brief  API to set Scan Control List
    \param  hHal - The handle returned by macOpen.
    \param  countryCode -  Pointer to the countryCode
    \param  pChannelList -  Pointer to the valid channel list
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetCfgScanControlList(tHalHandle hHal, tANI_U8 *countryCode,
                                                    tCsrChannel *pChannelList);

#ifdef FEATURE_OEM_DATA_SUPPORT
eHalStatus oemData_ProcessOemDataReqCommand(tpAniSirGlobal pMac, tSmeCmd *pCommand);
#endif

eHalStatus csrProcessAddStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrProcessAddStaSessionRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg);
eHalStatus csrProcessDelStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrProcessMacAddrSpoofCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
eHalStatus csrProcessDelStaSessionRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg);
eHalStatus csrProcessGetFrameLogCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );

#ifdef WLAN_NS_OFFLOAD
/* ---------------------------------------------------------------------------
    \fn pmcSetNSOffload
    \brief  Set the host offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest - Pointer to the offload request.
    \param  sessionId .  Session index of the session
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcSetNSOffload (tHalHandle hHal, tpSirHostOffloadReq pRequest, tANI_U8 sessionId);
#endif //WLAN_NS_OFFLOAD

#ifdef FEATURE_WLAN_SCAN_PNO
eHalStatus pmcSetPreferredNetworkList(tHalHandle hHal, tpSirPNOScanReq pRequest, tANI_U8 sessionId, preferredNetworkFoundIndCallback callbackRoutine,  void *callbackContext);
eHalStatus pmcUpdateScanParams(tHalHandle hHal, tCsrConfig *pRequest, tCsrChannel *pChannelList, tANI_U8 b11dResolved);
eHalStatus pmcSetRssiFilter(tHalHandle hHal,   v_U8_t        rssiThreshold);
#endif // FEATURE_WLAN_SCAN_PNO
eHalStatus pmcSetPowerParams(tHalHandle hHal,   tSirSetPowerParamsReq*  pwParams, tANI_BOOLEAN forced);

tANI_BOOLEAN csrRoamGetConcurrencyConnectStatusForBmps(tpAniSirGlobal pMac);
#ifdef FEATURE_WLAN_TDLS
eHalStatus csrTdlsSendMgmtReq(tHalHandle hHal, tANI_U8 sessionId, tCsrTdlsSendMgmt *tdlsSendMgmt);
VOS_STATUS csrTdlsSendLinkEstablishParams(tHalHandle hHal,
                                          tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                          const tSirMacAddr peerMac,
#else
                                          tSirMacAddr peerMac,
#endif
                                          tCsrTdlsLinkEstablishParams *tdlsLinkEstablishParams);
eHalStatus csrTdlsAddPeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const tSirMacAddr peerMac
#else
                             tSirMacAddr peerMac
#endif
                             );
eHalStatus csrTdlsChangePeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                const tSirMacAddr peerMac,
#else
                                tSirMacAddr peerMac,
#endif
                                tCsrStaParams *pstaParams);
eHalStatus csrTdlsDelPeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                            const tSirMacAddr peerMac
#else
                             tSirMacAddr peerMac
#endif
                             );
eHalStatus csrTdlsProcessCmd(tpAniSirGlobal pMac,tSmeCmd *pCommand );
eHalStatus csrTdlsProcessLinkEstablish( tpAniSirGlobal pMac, tSmeCmd *cmd );
eHalStatus csrTdlsProcessChanSwitchReq(tpAniSirGlobal pMac, tSmeCmd *cmd ); //tdlsoffchan
eHalStatus tdlsMsgProcessor(tpAniSirGlobal pMac,v_U16_t msg_type, void *pMsgBuf);
VOS_STATUS csrTdlsSendChanSwitchReq(tHalHandle hHal,
                                    tANI_U8 sessionId,
                                    tSirMacAddr peerMac,
                                    tANI_S32 tdlsOffCh,
                                    tANI_S32 tdlsOffChBwOffset,
                                    tANI_U8 tdlsSwMode);
#endif /* FEATURE_WLAN_TDLS */

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
eHalStatus csrFlushCfgBgScanRoamChannelList(tpAniSirGlobal pMac);
eHalStatus csrCreateBgScanRoamChannelList(tpAniSirGlobal pMac,
                                            const tANI_U8 *pChannelList,
                                            const tANI_U8 numChannels);
eHalStatus csrUpdateBgScanConfigIniChannelList(tpAniSirGlobal pMac, eCsrBand eBand);
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
eHalStatus csrCreateRoamScanChannelList(tpAniSirGlobal pMac,
                                                tANI_U8 *pChannelList,
                                                tANI_U8 numChannels,
                                                const eCsrBand eBand);
#endif
void activeListCmdTimeoutHandle(void *userData);

void csrGetStaticUapsdMask(tpAniSirGlobal pMac, tANI_U8 *staticUapsdMask);

#endif //#if !defined( __SMEINSIDE_H )
