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
 * This file limUtils.h contains the utility definitions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_UTILS_H
#define __LIM_UTILS_H

#include "sirApi.h"
#include "sirDebug.h"
#include "cfgApi.h"

#include "limTypes.h"
#include "limScanResultUtils.h"
#include "limTimerUtils.h"
#include "limTrace.h"
typedef enum
{
    ONE_BYTE   = 1,
    TWO_BYTE   = 2
} eSizeOfLenField;

#define LIM_STA_ID_MASK                        0x00FF
#define LIM_AID_MASK                              0xC000
#define LIM_SPECTRUM_MANAGEMENT_BIT_MASK          0x0100
#define LIM_RRM_BIT_MASK                          0x1000
#define LIM_SHORT_PREAMBLE_BIT_MASK               0x0020
#define LIM_IMMEDIATE_BLOCK_ACK_MASK              0x8000
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
#define LIM_MAX_REASSOC_RETRY_LIMIT            2
#endif

// classifier ID is coded as 0-3: tsid, 4-5:direction
#define LIM_MAKE_CLSID(tsid, dir) (((tsid) & 0x0F) | (((dir) & 0x03) << 4))

#define LIM_SET_STA_BA_STATE(pSta, tid, newVal) \
{\
    pSta->baState = ((pSta->baState | (0x3 << tid*2)) & ((newVal << tid*2) | ~(0x3 << tid*2)));\
}

#define LIM_GET_STA_BA_STATE(pSta, tid, pCurVal)\
{\
    *pCurVal = (tLimBAState)(((pSta->baState >> tid*2) & 0x3));\
}

typedef struct sAddBaInfo
{
    tANI_U16 fBaEnable : 1;
    tANI_U16 startingSeqNum: 12;
    tANI_U16 reserved : 3;
}tAddBaInfo, *tpAddBaInfo;

typedef struct sAddBaCandidate
{
    tSirMacAddr staAddr;
    tAddBaInfo baInfo[STACFG_MAX_TC];
}tAddBaCandidate, *tpAddBaCandidate;

#ifdef WLAN_FEATURE_11W
typedef union uPmfSaQueryTimerId
{
    struct
    {
        tANI_U8 sessionId;
        tANI_U16 peerIdx;
    } fields;
    tANI_U32 value;
} tPmfSaQueryTimerId, *tpPmfSaQueryTimerId;
#endif

typedef enum offset {
    BW20,
    BW40PLUS,
    BW40MINUS,
    BWALL
} offset_t;

typedef struct op_class_map {
    tANI_U8 op_class;
    tANI_U8 ch_spacing;
    offset_t    offset;
    tANI_U8 channels[15];
}op_class_map_t;
// LIM utility functions
void limGetBssidFromPkt(tpAniSirGlobal, tANI_U8 *, tANI_U8 *, tANI_U32 *);
char * limMlmStateStr(tLimMlmStates state);
char * limSmeStateStr(tLimSmeStates state);
char * limMsgStr(tANI_U32 msgType);
char * limResultCodeStr(tSirResultCodes resultCode);
char* limDot11ModeStr(tpAniSirGlobal pMac, tANI_U8 dot11Mode);
char* limStaOpRateModeStr(tStaRateMode opRateMode);
void limPrintMlmState(tpAniSirGlobal pMac, tANI_U16 logLevel, tLimMlmStates state);
void limPrintSmeState(tpAniSirGlobal pMac, tANI_U16 logLevel, tLimSmeStates state);
void limPrintMsgName(tpAniSirGlobal pMac, tANI_U16 logLevel, tANI_U32 msgType);
void limPrintMsgInfo(tpAniSirGlobal pMac, tANI_U16 logLevel, tSirMsgQ *msg);
char* limBssTypeStr(tSirBssType bssType);

#if defined FEATURE_WLAN_ESE || defined WLAN_FEATURE_VOWIFI
extern tSirRetStatus limSendSetMaxTxPowerReq ( tpAniSirGlobal pMac, 
                                  tPowerdBm txPower, 
                                  tpPESession pSessionEntry );
extern tANI_U8 limGetMaxTxPower(tPowerdBm regMax, tPowerdBm apTxPower, tANI_U8 iniTxPower);
#endif

tANI_U32            limPostMsgApiNoWait(tpAniSirGlobal, tSirMsgQ *);
tANI_U8           limIsAddrBC(tSirMacAddr);
tANI_U8           limIsGroupAddr(tSirMacAddr);

// check for type of scan allowed
tANI_U8 limActiveScanAllowed(tpAniSirGlobal, tANI_U8);

// AID pool management functions
void    limInitPeerIdxpool(tpAniSirGlobal,tpPESession);
tANI_U16     limAssignPeerIdx(tpAniSirGlobal,tpPESession);

void limEnableOverlap11gProtection(tpAniSirGlobal pMac, tpUpdateBeaconParams pBeaconParams, tpSirMacMgmtHdr pMh,tpPESession psessionEntry);
void limUpdateOverlapStaParam(tpAniSirGlobal pMac, tSirMacAddr bssId, tpLimProtStaParams pStaParams);
void limUpdateShortPreamble(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
void limUpdateShortSlotTime(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);

/*
 * The below 'product' check tobe removed if 'Association' is
 * allowed in IBSS.
 */
void    limReleasePeerIdx(tpAniSirGlobal, tANI_U16, tpPESession);


void limDecideApProtection(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,  tpUpdateBeaconParams pBeaconParams,tpPESession);
void
limDecideApProtectionOnDelete(tpAniSirGlobal pMac, 
                              tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);

extern tSirRetStatus limEnable11aProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnable11gProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHtProtectionFrom11g(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHT20Protection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession sessionEntry);
extern tSirRetStatus limEnableHTNonGfProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnableHtRifsProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHTLsigTxopProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnableShortPreamble(tpAniSirGlobal pMac, tANI_U8 enable, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
extern tSirRetStatus limEnableHtOBSSProtection (tpAniSirGlobal pMac, tANI_U8 enable,  tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams, tpPESession);
void limDecideStaProtection(tpAniSirGlobal pMac, tpSchBeaconStruct pBeaconStruct, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
void limDecideStaProtectionOnAssoc(tpAniSirGlobal pMac, tpSchBeaconStruct pBeaconStruct, tpPESession psessionEntry);
void limUpdateStaRunTimeHTSwitchChnlParams(tpAniSirGlobal pMac, tDot11fIEHTInfo * pHTInfo, tANI_U8 bssIdx, tpPESession psessionEntry);
// Print MAC address utility function
void    limPrintMacAddr(tpAniSirGlobal, tSirMacAddr, tANI_U8);



// Deferred Message Queue read/write
tANI_U8 limWriteDeferredMsgQ(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirMsgQ* limReadDeferredMsgQ(tpAniSirGlobal pMac);
void limHandleDeferMsgError(tpAniSirGlobal pMac, tpSirMsgQ pLimMsg);

// Deferred Message Queue Reset
void limResetDeferredMsgQ(tpAniSirGlobal pMac);

tSirRetStatus limSysProcessMmhMsgApi(tpAniSirGlobal, tSirMsgQ*, tANI_U8);

void limHandleUpdateOlbcCache(tpAniSirGlobal pMac);

tANI_U8 limIsNullSsid( tSirMacSSid *pSsid );

void limProcessAddtsRspTimeout(tpAniSirGlobal pMac, tANI_U32 param);

// 11h Support
void limStopTxAndSwitchChannel(tpAniSirGlobal pMac, tANI_U8 sessionId);
void limProcessChannelSwitchTimeout(tpAniSirGlobal);
tSirRetStatus limStartChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limUpdateChannelSwitch(tpAniSirGlobal, tpSirProbeRespBeacon, tpPESession psessionEntry);
void limProcessQuietTimeout(tpAniSirGlobal);
void limProcessQuietBssTimeout(tpAniSirGlobal);
void limInitOBSSScanParams(tpAniSirGlobal pMac,
                                   tpPESession psessionEntry);
#if 0
void limProcessWPSOverlapTimeout(tpAniSirGlobal pMac);
#endif

void limStartQuietTimer(tpAniSirGlobal pMac, tANI_U8 sessionId);
void limSwitchPrimaryChannel(tpAniSirGlobal, tANI_U8,tpPESession);
void limSwitchPrimarySecondaryChannel(tpAniSirGlobal, tpPESession, tANI_U8, ePhyChanBondState);
tAniBool limTriggerBackgroundScanDuringQuietBss(tpAniSirGlobal);
void limUpdateStaRunTimeHTSwtichChnlParams(tpAniSirGlobal pMac, tDot11fIEHTInfo *pRcvdHTInfo, tANI_U8 bssIdx);
void limUpdateStaRunTimeHTCapability(tpAniSirGlobal pMac, tDot11fIEHTCaps *pHTCaps);
void limUpdateStaRunTimeHTInfo(struct sAniSirGlobal *pMac, tDot11fIEHTInfo *pRcvdHTInfo, tpPESession psessionEntry);
void limCancelDot11hChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limCancelDot11hQuiet(tpAniSirGlobal pMac, tpPESession psessionEntry);
tAniBool limIsChannelValidForChannelSwitch(tpAniSirGlobal pMac, tANI_U8 channel);
void limFrameTransmissionControl(tpAniSirGlobal pMac, tLimQuietTxMode type, tLimControlTx mode);
tSirRetStatus limRestorePreChannelSwitchState(tpAniSirGlobal pMac, tpPESession psessionEntry);
tSirRetStatus limRestorePreQuietState(tpAniSirGlobal pMac, tpPESession psessionEntry);

void limPrepareFor11hChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limSwitchChannelCback(tpAniSirGlobal pMac, eHalStatus status, 
                           tANI_U32 *data, tpPESession psessionEntry);

static inline tSirRFBand limGetRFBand(tANI_U8 channel)
{
    if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
        (channel <= SIR_11A_CHANNEL_END))
        return SIR_BAND_5_GHZ;

    if ((channel >= SIR_11B_CHANNEL_BEGIN) &&
        (channel <= SIR_11B_CHANNEL_END))
        return SIR_BAND_2_4_GHZ;

    return SIR_BAND_UNKNOWN;
}


static inline tSirRetStatus
limGetMgmtStaid(tpAniSirGlobal pMac, tANI_U16 *staid, tpPESession psessionEntry)
{
    if (psessionEntry->limSystemRole == eLIM_AP_ROLE)
        *staid = 1;
    else if (psessionEntry->limSystemRole == eLIM_STA_ROLE)
        *staid = 0;
    else
        return eSIR_FAILURE;

    return eSIR_SUCCESS;
}

static inline tANI_U8
limIsSystemInSetMimopsState(tpAniSirGlobal pMac)
{
    if (pMac->lim.gLimMlmState == eLIM_MLM_WT_SET_MIMOPS_STATE)
        return true;
    return false;
}
        
static inline tANI_U8
 isEnteringMimoPS(tSirMacHTMIMOPowerSaveState curState, tSirMacHTMIMOPowerSaveState newState)
 {
    if (curState == eSIR_HT_MIMO_PS_NO_LIMIT &&
        (newState == eSIR_HT_MIMO_PS_DYNAMIC ||newState == eSIR_HT_MIMO_PS_STATIC))
        return TRUE;
    return FALSE;
}

/// ANI peer station count management and associated actions
void limUtilCountStaAdd(tpAniSirGlobal pMac, tpDphHashNode pSta, tpPESession psessionEntry);
void limUtilCountStaDel(tpAniSirGlobal pMac, tpDphHashNode pSta, tpPESession psessionEntry);

tANI_U8 limGetHTCapability( tpAniSirGlobal, tANI_U32, tpPESession);
void limTxComplete( tHalHandle hHal, void *pData );

/**********Admit Control***************************************/

//callback function for HAL to issue DelTS request to PE.
//This function will be registered with HAL for callback when TSPEC inactivity timer fires.

void limProcessDelTsInd(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirRetStatus limProcessHalIndMessages(tpAniSirGlobal pMac, tANI_U32 mesgId, void *mesgParam );
tSirRetStatus limValidateDeltsReq(tpAniSirGlobal pMac, tpSirDeltsReq pDeltsReq, tSirMacAddr peerMacAddr,tpPESession psessionEntry);
/**********************************************************/

//callback function registration to HAL for any indication.
void limRegisterHalIndCallBack(tpAniSirGlobal pMac);
void limPktFree (
    tpAniSirGlobal  pMac,
    eFrameType      frmType,
    tANI_U8         *pBD,
    void            *body);



void limGetBDfromRxPacket(tpAniSirGlobal pMac, void *body, tANI_U32 **pBD);

/**
 * \brief Given a base(X) and power(Y), this API will return
 * the result of base raised to power - (X ^ Y)
 *
 * \sa utilsPowerXY
 *
 * \param base Base value
 *
 * \param power Base raised to this Power value
 *
 * \return Result of X^Y
 *
 */
static inline tANI_U32 utilsPowerXY( tANI_U16 base, tANI_U16 power )
{
tANI_U32 result = 1, i;

  for( i = 0; i < power; i++ )
    result *= base;

  return result;
}



tSirRetStatus limPostMlmAddBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pStaDs,
    tANI_U8 tid, tANI_U16 startingSeqNum,tpPESession psessionEntry);
tSirRetStatus limPostMlmAddBARsp( tpAniSirGlobal pMac,
    tSirMacAddr peerMacAddr,
    tSirMacStatusCodes baStatusCode,
    tANI_U8 baDialogToken,
    tANI_U8 baTID,
    tANI_U8 baPolicy,
    tANI_U16 baBufferSize,
    tANI_U16 baTimeout,
    tpPESession psessionEntry);
tSirRetStatus limPostMlmDelBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baDirection,
    tANI_U8 baTID,
    tSirMacReasonCodes baReasonCode ,
    tpPESession psessionEntry);
tSirRetStatus limPostMsgAddBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baDialogToken,
    tANI_U8 baTID,
    tANI_U8 baPolicy,
    tANI_U16 baBufferSize,
    tANI_U16 baTimeout,
    tANI_U16 baSSN,
    tANI_U8 baDirection,
    tpPESession psessionEntry);
tSirRetStatus limPostMsgDelBAInd( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baTID,
    tANI_U8 baDirection,
    tpPESession psessionEntry);

tSirRetStatus limPostSMStateUpdate(tpAniSirGlobal pMac,
    tANI_U16 StaIdx, 
    tSirMacHTMIMOPowerSaveState MIMOPSState);

void limDeleteStaContext(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
void limProcessAddBaInd(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
void limDeleteBASessions(tpAniSirGlobal pMac, tpPESession pSessionEntry,
                         tANI_U32 baDirection, tSirMacReasonCodes baReasonCode);
void limDelPerBssBASessionsBtc(tpAniSirGlobal pMac);
void limDelAllBASessions(tpAniSirGlobal pMac);
void limDeleteDialogueTokenList(tpAniSirGlobal pMac);
tSirRetStatus limSearchAndDeleteDialogueToken(tpAniSirGlobal pMac, tANI_U8 token, tANI_U16 assocId, tANI_U16 tid);
void limRessetScanChannelInfo(tpAniSirGlobal pMac);
void limAddScanChannelInfo(tpAniSirGlobal pMac, tANI_U8 channelId);

tANI_U8 limGetChannelFromBeacon(tpAniSirGlobal pMac, tpSchBeaconStruct pBeacon);
tSirNwType limGetNwType(tpAniSirGlobal pMac, tANI_U8 channelNum, tANI_U32 type, tpSchBeaconStruct pBeacon);
void limSetTspecUapsdMask(tpAniSirGlobal pMac, tSirMacTSInfo *pTsInfo, tANI_U32 action);
void limHandleHeartBeatTimeout(tpAniSirGlobal pMac);
void limHandleHeartBeatTimeoutForSession(tpAniSirGlobal pMac, tpPESession psessionEntry);

//void limProcessBtampAddBssRsp(tpAniSirGlobal pMac,tpSirMsgQ pMsgQ,tpPESession peSession);
void limProcessAddStaRsp(tpAniSirGlobal pMac,tpSirMsgQ pMsgQ);

void limUpdateBeacon(tpAniSirGlobal pMac);

void limProcessBtAmpApMlmAddStaRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ, tpPESession psessionEntry);
void limProcessBtAmpApMlmDelBssRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ,tpPESession psessionEntry);

void limProcessBtAmpApMlmDelStaRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ,tpPESession psessionEntry);
tpPESession  limIsIBSSSessionActive(tpAniSirGlobal pMac);
tpPESession limIsApSessionActive(tpAniSirGlobal pMac);
void limHandleHeartBeatFailureTimeout(tpAniSirGlobal pMac);

void limProcessDelStaSelfRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ);
void limProcessAddStaSelfRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ);
v_U8_t* limGetIEPtr(tpAniSirGlobal pMac, v_U8_t *pIes, int length, v_U8_t eid,eSizeOfLenField size_of_len_field);

tANI_U8 limUnmapChannel(tANI_U8 mapChannel);

#define limGetWscIEPtr(pMac, ie, ie_len) \
    limGetVendorIEOuiPtr(pMac, SIR_MAC_WSC_OUI, SIR_MAC_WSC_OUI_SIZE, ie, ie_len)

#define limGetP2pIEPtr(pMac, ie, ie_len) \
    limGetVendorIEOuiPtr(pMac, SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE, ie, ie_len)

v_U8_t limGetNoaAttrStreamInMultP2pIes(tpAniSirGlobal pMac,v_U8_t* noaStream,v_U8_t noaLen,v_U8_t overFlowLen);
v_U8_t limGetNoaAttrStream(tpAniSirGlobal pMac, v_U8_t*pNoaStream,tpPESession psessionEntry);

v_U8_t limBuildP2pIe(tpAniSirGlobal pMac, tANI_U8 *ie, tANI_U8 *data, tANI_U8 ie_len);
tANI_BOOLEAN limIsNOAInsertReqd(tpAniSirGlobal pMac);
v_U8_t* limGetVendorIEOuiPtr(tpAniSirGlobal pMac, tANI_U8 *oui, tANI_U8 oui_size, tANI_U8 *ie, tANI_U16 ie_len);
tANI_BOOLEAN limIsconnectedOnDFSChannel(tANI_U8 currentChannel);
tANI_U8 limGetCurrentOperatingChannel(tpAniSirGlobal pMac);

#ifdef WLAN_FEATURE_11AC
tANI_BOOLEAN limCheckVHTOpModeChange( tpAniSirGlobal pMac,
                                      tpPESession psessionEntry, tANI_U8 chanWidth, tANI_U8 staId);
#endif
tANI_BOOLEAN limCheckHTChanBondModeChange(tpAniSirGlobal pMac,
                                                  tpPESession psessionEntry,
                                                  tANI_U8 beaconSecChanWidth,
                                                  tANI_U8 currentSecChanWidth,
                                                  tANI_U8 staId);
#ifdef FEATURE_WLAN_DIAG_SUPPORT

typedef enum
{
    WLAN_PE_DIAG_SCAN_REQ_EVENT = 0,
    WLAN_PE_DIAG_SCAN_ABORT_IND_EVENT,
    WLAN_PE_DIAG_SCAN_RSP_EVENT,
    WLAN_PE_DIAG_JOIN_REQ_EVENT,
    WLAN_PE_DIAG_JOIN_RSP_EVENT,
    WLAN_PE_DIAG_SETCONTEXT_REQ_EVENT,  
    WLAN_PE_DIAG_SETCONTEXT_RSP_EVENT, 
    WLAN_PE_DIAG_REASSOC_REQ_EVENT,
    WLAN_PE_DIAG_REASSOC_RSP_EVENT,
    WLAN_PE_DIAG_AUTH_REQ_EVENT,
    WLAN_PE_DIAG_AUTH_RSP_EVENT,
    WLAN_PE_DIAG_DISASSOC_REQ_EVENT,
    WLAN_PE_DIAG_DISASSOC_RSP_EVENT,
    WLAN_PE_DIAG_DISASSOC_IND_EVENT,
    WLAN_PE_DIAG_DISASSOC_CNF_EVENT,
    WLAN_PE_DIAG_DEAUTH_REQ_EVENT,
    WLAN_PE_DIAG_DEAUTH_RSP_EVENT,
    WLAN_PE_DIAG_DEAUTH_IND_EVENT,
    WLAN_PE_DIAG_START_BSS_REQ_EVENT,
    WLAN_PE_DIAG_START_BSS_RSP_EVENT,
    WLAN_PE_DIAG_AUTH_IND_EVENT,
    WLAN_PE_DIAG_ASSOC_IND_EVENT,
    WLAN_PE_DIAG_ASSOC_CNF_EVENT,
    WLAN_PE_DIAG_REASSOC_IND_EVENT,
    WLAN_PE_DIAG_SWITCH_CHL_REQ_EVENT,
    WLAN_PE_DIAG_SWITCH_CHL_RSP_EVENT,
    WLAN_PE_DIAG_STOP_BSS_REQ_EVENT,
    WLAN_PE_DIAG_STOP_BSS_RSP_EVENT,
    WLAN_PE_DIAG_DEAUTH_CNF_EVENT,
    WLAN_PE_DIAG_ADDTS_REQ_EVENT,
    WLAN_PE_DIAG_ADDTS_RSP_EVENT,
    WLAN_PE_DIAG_DELTS_REQ_EVENT,
    WLAN_PE_DIAG_DELTS_RSP_EVENT,
    WLAN_PE_DIAG_DELTS_IND_EVENT,
    WLAN_PE_DIAG_ENTER_BMPS_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_BMPS_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_IND_EVENT,
    WLAN_PE_DIAG_ENTER_IMPS_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_IMPS_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_IMPS_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_IMPS_RSP_EVENT,
    WLAN_PE_DIAG_ENTER_UAPSD_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_UAPSD_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_UAPSD_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_UAPSD_RSP_EVENT,
    WLAN_PE_DIAG_WOWL_ADD_BCAST_PTRN_EVENT,
    WLAN_PE_DIAG_WOWL_DEL_BCAST_PTRN_EVENT,
    WLAN_PE_DIAG_ENTER_WOWL_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_WOWL_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_WOWL_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_WOWL_RSP_EVENT,
    WLAN_PE_DIAG_HAL_ADDBA_REQ_EVENT,
    WLAN_PE_DIAG_HAL_ADDBA_RSP_EVENT,
    WLAN_PE_DIAG_HAL_DELBA_IND_EVENT,
    WLAN_PE_DIAG_HB_FAILURE_TIMEOUT,
    WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT,
    WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT,
    WLAN_PE_DIAG_PREAUTH_DONE,
    WLAN_PE_DIAG_REASSOCIATING,
    WLAN_PE_DIAG_CONNECTED,
}WLAN_PE_DIAG_EVENT_TYPE;

void limDiagEventReport(tpAniSirGlobal pMac, tANI_U16 eventType, tpPESession pSessionEntry, tANI_U16 status, tANI_U16 reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

void peSetResumeChannel(tpAniSirGlobal pMac, tANI_U16 channel, ePhyChanBondState cbState);
/*--------------------------------------------------------------------------
  
  \brief peGetResumeChannel() - Returns the  channel number for scanning, from a valid session.

  This function returns the channel to resume to during link resume. channel id of 0 means HAL will
  resume to previous channel before link suspend
    
  \param pMac                   - pointer to global adapter context
  \return                            - channel to scan from valid session else zero.
  
  \sa
  
  --------------------------------------------------------------------------*/
void peGetResumeChannel(tpAniSirGlobal pMac, tANI_U8* resumeChannel, ePhyChanBondState* resumePhyCbState);

#ifdef FEATURE_WLAN_TDLS_INTERNAL
tANI_U8 limTdlsFindLinkPeer(tpAniSirGlobal pMac, tSirMacAddr peerMac, tLimTdlsLinkSetupPeer  **setupPeer);
void limTdlsDelLinkPeer(tpAniSirGlobal pMac, tSirMacAddr peerMac);
void limStartTdlsTimer(tpAniSirGlobal pMac, tANI_U8 sessionId, TX_TIMER *timer, tANI_U32 timerId, 
                                      tANI_U16 timerType, tANI_U32 timerMsg);
#endif
void limGetShortSlotFromPhyMode(tpAniSirGlobal pMac, tpPESession psessionEntry, tANI_U32 phyMode,
                                tANI_U8 *pShortSlotEnable);

void limCleanUpDisassocDeauthReq(tpAniSirGlobal pMac, tANI_U8 *staMac, tANI_BOOLEAN cleanRxPath);

tANI_BOOLEAN limCheckDisassocDeauthAckPending(tpAniSirGlobal pMac, tANI_U8 *staMac);


void limUtilsframeshtons(tpAniSirGlobal  pCtx,
                            tANI_U8  *pOut,
                            tANI_U16  pIn,
                            tANI_U8  fMsb);

void limUtilsframeshtonl(tpAniSirGlobal  pCtx,
                            tANI_U8  *pOut,
                            tANI_U32  pIn,
                            tANI_U8  fMsb);

void limUpdateOBSSScanParams(tpPESession psessionEntry ,
             tDot11fIEOBSSScanParameters *pOBSSScanParameters);

#ifdef WLAN_FEATURE_11W
void limPmfSaQueryTimerHandler(void *pMacGlobal, tANI_U32 param);

void limSetProtectedBit(tpAniSirGlobal  pMac,
                           tpPESession     psessionEntry,
                           tSirMacAddr     peer,
                           tpSirMacMgmtHdr pMacHdr);
#endif
void limInitOperatingClasses(tHalHandle hHal);
tANI_U8 limGetOPClassFromChannel(tANI_U8 *country,
                                 tANI_U8 channel,
                                 tANI_U8 offset);
#endif /* __LIM_UTILS_H */
