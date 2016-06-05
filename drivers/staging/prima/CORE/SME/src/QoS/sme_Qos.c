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

/**=========================================================================
  
  \file  sme_Qos.c
  
  \brief implementation for SME QoS APIs
  
  
  ========================================================================*/
/* $Header$ */
/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/


#include "aniGlobal.h"

#include "smeInside.h"
#include "vos_diag_core_event.h"
#include "vos_diag_core_log.h"

#ifdef WLAN_FEATURE_VOWIFI_11R
#include "smsDebug.h"
#include "utilsParser.h"
#endif
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include <csrEse.h>
#endif

#include "vos_utils.h"

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
/* TODO : 6Mbps as Cisco APs seem to like only this value; analysis req.   */
#define SME_QOS_MIN_PHY_RATE         0x5B8D80    
#define SME_QOS_SURPLUS_BW_ALLOWANCE  0x2000     /* Ratio of 1.0           */
/*---------------------------------------------------------------------------
  Max values to bound tspec params against and avoid rollover
---------------------------------------------------------------------------*/
#define SME_QOS_32BIT_MAX  0xFFFFFFFF
#define SME_QOS_16BIT_MAX  0xFFFF
#define SME_QOS_16BIT_MSB  0x8000
/*---------------------------------------------------------------------------
  Adds y to x, but saturates at 32-bit max to avoid rollover
---------------------------------------------------------------------------*/
#define SME_QOS_BOUNDED_U32_ADD_Y_TO_X( _x, _y )                            \
  do                                                                        \
  {                                                                         \
    (_x) = ( (SME_QOS_32BIT_MAX-(_x))<(_y) ) ?                              \
    (SME_QOS_32BIT_MAX) : (_x)+(_y);                                        \
  } while(0)
/*---------------------------------------------------------------------------
  As per WMM spec there could be max 2 TSPEC running on the same AC with 
  different direction. We will refer each TSPEC with an index
---------------------------------------------------------------------------*/
#define SME_QOS_TSPEC_INDEX_0            0
#define SME_QOS_TSPEC_INDEX_1            1
#define SME_QOS_TSPEC_INDEX_MAX          2
#define SME_QOS_TSPEC_MASK_BIT_1_SET     1
#define SME_QOS_TSPEC_MASK_BIT_2_SET     2
#define SME_QOS_TSPEC_MASK_BIT_1_2_SET   3
#define SME_QOS_TSPEC_MASK_CLEAR         0

//which key to search on, in the flowlist (1 = flowID, 2 = AC, 4 = reason)
#define SME_QOS_SEARCH_KEY_INDEX_1       1
#define SME_QOS_SEARCH_KEY_INDEX_2       2
#define SME_QOS_SEARCH_KEY_INDEX_3       4
#define SME_QOS_SEARCH_KEY_INDEX_4       8  // ac + direction
#define SME_QOS_SEARCH_KEY_INDEX_5       0x10  // ac + tspec_mask
//special value for searching any Session Id
#define SME_QOS_SEARCH_SESSION_ID_ANY    CSR_ROAM_SESSION_MAX
#define SME_QOS_ACCESS_POLICY_EDCA       1
#define SME_QOS_MAX_TID                  255
#define SME_QOS_TSPEC_IE_LENGTH          61
#define SME_QOS_TSPEC_IE_TYPE            2
#define SME_QOS_MIN_FLOW_ID              1
#define SME_QOS_MAX_FLOW_ID              0xFFFFFFFE
#define SME_QOS_INVALID_FLOW_ID          0xFFFFFFFF
// per the WMM Specification v1.2 Section 2.2.10
// The Dialog Token field shall be set [...] to a non-zero value
#define SME_QOS_MIN_DIALOG_TOKEN         1
#define SME_QOS_MAX_DIALOG_TOKEN         0xFF
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
   Enumeration of the various states in the QoS state m/c
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_CLOSED = 0,
   SME_QOS_INIT,
   SME_QOS_LINK_UP,
   SME_QOS_REQUESTED,
   SME_QOS_QOS_ON,
   SME_QOS_HANDOFF,
   
}sme_QosStates;
/*---------------------------------------------------------------------------
   Enumeration of the various Release QoS trigger
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_RELEASE_DEFAULT = 0,
   SME_QOS_RELEASE_BY_AP,
}sme_QosRelTriggers;
/*---------------------------------------------------------------------------
   Enumeration of the various QoS cmds 
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_SETUP_REQ = 0,
   SME_QOS_RELEASE_REQ,
   SME_QOS_MODIFY_REQ,
   SME_QOS_RESEND_REQ,
   SME_QOS_CMD_MAX
}sme_QosCmdType;
/*---------------------------------------------------------------------------
   Enumeration of the various QoS reason codes to be used in the Flow list 
---------------------------------------------------------------------------*/
typedef enum
{
   SME_QOS_REASON_SETUP = 0,
   SME_QOS_REASON_RELEASE,
   SME_QOS_REASON_MODIFY,
   SME_QOS_REASON_MODIFY_PENDING,
   SME_QOS_REASON_REQ_SUCCESS,
   SME_QOS_REASON_MAX
}sme_QosReasonType;

/*---------------------------------------------------------------------------
  Table to map user priority passed in as an argument to appropriate Access 
  Category as specified in 802.11e/WMM
---------------------------------------------------------------------------*/
sme_QosEdcaAcType sme_QosUPtoACMap[SME_QOS_WMM_UP_MAX] =
{
   SME_QOS_EDCA_AC_BE, /* User Priority 0 */
   SME_QOS_EDCA_AC_BK, /* User Priority 1 */
   SME_QOS_EDCA_AC_BK, /* User Priority 2 */
   SME_QOS_EDCA_AC_BE, /* User Priority 3 */
   SME_QOS_EDCA_AC_VI, /* User Priority 4 */
   SME_QOS_EDCA_AC_VI, /* User Priority 5 */
   SME_QOS_EDCA_AC_VO, /* User Priority 6 */
   SME_QOS_EDCA_AC_VO  /* User Priority 7 */
};

/*---------------------------------------------------------------------------
  Table to map access category (AC) to appropriate user priority as specified
  in 802.11e/WMM
  Note: there is a quantization loss here because 4 ACs are mapped to 8 UPs
  Mapping is done for consistency
---------------------------------------------------------------------------*/
sme_QosWmmUpType sme_QosACtoUPMap[SME_QOS_EDCA_AC_MAX] = 
{
   SME_QOS_WMM_UP_BE,   /* AC BE */
   SME_QOS_WMM_UP_BK,   /* AC BK */
   SME_QOS_WMM_UP_VI,   /* AC VI */
   SME_QOS_WMM_UP_VO    /* AC VO */
};
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's FLOW Link List structure. This list can hold information per 
  flow/request, like TSPEC params requested, which AC it is running on 
---------------------------------------------------------------------------*/
typedef struct sme_QosFlowInfoEntry_s
{
    tListElem             link;  /* list links */
    v_U8_t                sessionId;
    v_U8_t                tspec_mask;
    sme_QosReasonType     reason;
    v_U32_t               QosFlowID;
    sme_QosEdcaAcType     ac_type;
    sme_QosWmmTspecInfo   QoSInfo;
    void                * HDDcontext;
    sme_QosCallback       QoSCallback;
    v_BOOL_t              hoRenewal;//set to TRUE while re-negotiating flows after
                                     //handoff, will set to FALSE once done with
                                     //the process. Helps SME to decide if at all 
                                     //to notify HDD/LIS for flow renewal after HO
} sme_QosFlowInfoEntry;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's setup request cmd related information structure. 
---------------------------------------------------------------------------*/
typedef struct sme_QosSetupCmdInfo_s
{
    v_U32_t               QosFlowID;
    sme_QosWmmTspecInfo   QoSInfo;
    void                 *HDDcontext;
    sme_QosCallback       QoSCallback;
    sme_QosWmmUpType      UPType;
    v_BOOL_t              hoRenewal;//set to TRUE while re-negotiating flows after
                                     //handoff, will set to FALSE once done with
                                     //the process. Helps SME to decide if at all 
                                     //to notify HDD/LIS for flow renewal after HO
} sme_QosSetupCmdInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's modify cmd related information structure. 
---------------------------------------------------------------------------*/
typedef struct sme_QosModifyCmdInfo_s
{
    v_U32_t               QosFlowID;
    sme_QosEdcaAcType     ac;
    sme_QosWmmTspecInfo   QoSInfo;
} sme_QosModifyCmdInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's resend cmd related information structure. 
---------------------------------------------------------------------------*/
typedef struct sme_QosResendCmdInfo_s
{
    v_U8_t                tspecMask;
    sme_QosEdcaAcType     ac;
    sme_QosWmmTspecInfo   QoSInfo;
} sme_QosResendCmdInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's release cmd related information structure. 
---------------------------------------------------------------------------*/
typedef struct sme_QosReleaseCmdInfo_s
{
    v_U32_t               QosFlowID;
} sme_QosReleaseCmdInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's buffered cmd related information structure. 
---------------------------------------------------------------------------*/
typedef struct sme_QosCmdInfo_s
{
    sme_QosCmdType        command;
    tpAniSirGlobal        pMac;
    v_U8_t                sessionId;
    union
    {
       sme_QosSetupCmdInfo    setupCmdInfo;
       sme_QosModifyCmdInfo   modifyCmdInfo;
       sme_QosResendCmdInfo   resendCmdInfo;
       sme_QosReleaseCmdInfo  releaseCmdInfo;
    }u;
} sme_QosCmdInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's buffered cmd List structure. This list can hold information 
  related to any pending cmd from HDD
---------------------------------------------------------------------------*/
typedef struct sme_QosCmdInfoEntry_s
{
    tListElem             link;  /* list links */
    sme_QosCmdInfo        cmdInfo;
} sme_QosCmdInfoEntry;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's Per AC information structure. This can hold information on
  how many flows running on the AC, the current, previous states the AC is in 
---------------------------------------------------------------------------*/
typedef struct sme_QosACInfo_s
{
   v_U8_t                 num_flows[SME_QOS_TSPEC_INDEX_MAX];
   sme_QosStates          curr_state;
   sme_QosStates          prev_state;
   sme_QosWmmTspecInfo    curr_QoSInfo[SME_QOS_TSPEC_INDEX_MAX];
   sme_QosWmmTspecInfo    requested_QoSInfo[SME_QOS_TSPEC_INDEX_MAX];
   v_BOOL_t               reassoc_pending;//reassoc requested for APSD
   //As per WMM spec there could be max 2 TSPEC running on the same AC with 
   //different direction. We will refer each TSPEC with an index
   v_U8_t                 tspec_mask_status; //status showing if both the indices are in use
   v_U8_t                 tspec_pending;//tspec negotiation going on for which index
   v_BOOL_t               hoRenewal;//set to TRUE while re-negotiating flows after
                                    //handoff, will set to FALSE once done with
                                    //the process. Helps SME to decide if at all 
                                    //to notify HDD/LIS for flow renewal after HO
#ifdef WLAN_FEATURE_VOWIFI_11R
   v_U8_t                 ricIdentifier[SME_QOS_TSPEC_INDEX_MAX];
   /* stores the ADD TS response for each AC. The ADD TS response is formed by
   parsing the RIC received in the the reassoc response */
   tSirAddtsRsp           addTsRsp[SME_QOS_TSPEC_INDEX_MAX];
#endif
   sme_QosRelTriggers     relTrig;

} sme_QosACInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's Per session information structure. This can hold information
  on the state of the session
---------------------------------------------------------------------------*/
typedef struct sme_QosSessionInfo_s
{
   // what is this entry's session id
   v_U8_t                 sessionId;
   // is the session currently active
   v_BOOL_t               sessionActive;
   // All AC info for this session
   sme_QosACInfo          ac_info[SME_QOS_EDCA_AC_MAX];
   // Bitmask of the ACs with APSD on 
   // Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored
   v_U8_t                 apsdMask;
   // association information for this session
   sme_QosAssocInfo       assocInfo;
   // ID assigned to our reassoc request
   v_U32_t                roamID;
   // maintaining a powersave status in QoS module, to be fed back to PMC at 
   // times through the sme_QosPmcCheckRoutine
   v_BOOL_t               readyForPowerSave;
   // are we in the process of handing off to a different AP
   v_BOOL_t               handoffRequested;
   // following reassoc or AddTS has UAPSD already been requested from PMC
   v_BOOL_t               uapsdAlreadyRequested;
   // commands that are being buffered for this session
   tDblLinkList           bufferedCommandList;

#ifdef WLAN_FEATURE_VOWIFI_11R
   v_BOOL_t               ftHandoffInProgress;
#endif

} sme_QosSessionInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  Search key union. We can use the flowID, ac type, or reason to find an entry 
  in the flow list
---------------------------------------------------------------------------*/
typedef union sme_QosSearchKey_s
{
   v_U32_t               QosFlowID;
   sme_QosEdcaAcType     ac_type;
   sme_QosReasonType     reason;
}sme_QosSearchKey;
/*---------------------------------------------------------------------------
DESCRIPTION
  We can either use the flowID or the ac type to find an entry in the flow list.
  The index is a bitmap telling us which key to use. Starting from LSB,
  bit 0 - Flow ID
  bit 1 - AC type
---------------------------------------------------------------------------*/
typedef struct sme_QosSearchInfo_s
{
   v_U8_t           sessionId;
   v_U8_t           index;
   sme_QosSearchKey key;
   sme_QosWmmDirType   direction;
   v_U8_t              tspec_mask;
}sme_QosSearchInfo;
/*---------------------------------------------------------------------------
DESCRIPTION
  SME QoS module's internal control block.
---------------------------------------------------------------------------*/
struct sme_QosCb_s
{
   //global Mac pointer
   tpAniSirGlobal   pMac;
   //All Session Info
   sme_QosSessionInfo     sessionInfo[CSR_ROAM_SESSION_MAX];
   //All FLOW info
   tDblLinkList           flow_list;
   //default TSPEC params
   sme_QosWmmTspecInfo    def_QoSInfo[SME_QOS_EDCA_AC_MAX];
   //counter for assigning Flow IDs
   v_U32_t                nextFlowId;
   //counter for assigning Dialog Tokens
   v_U8_t                nextDialogToken;
}sme_QosCb;
typedef eHalStatus (*sme_QosProcessSearchEntry)(tpAniSirGlobal pMac, tListElem *pEntry);
/*-------------------------------------------------------------------------- 
                         Internal function declarations
  ------------------------------------------------------------------------*/
sme_QosStatusType sme_QosInternalSetupReq(tpAniSirGlobal pMac, 
                                          v_U8_t sessionId,
                                          sme_QosWmmTspecInfo * pQoSInfo,
                                          sme_QosCallback QoSCallback, 
                                          void * HDDcontext,
                                          sme_QosWmmUpType UPType, 
                                          v_U32_t QosFlowID,
                                          v_BOOL_t buffered_cmd,
                                          v_BOOL_t hoRenewal);
sme_QosStatusType sme_QosInternalModifyReq(tpAniSirGlobal pMac, 
                                           sme_QosWmmTspecInfo * pQoSInfo,
                                           v_U32_t QosFlowID,
                                           v_BOOL_t buffered_cmd);
sme_QosStatusType sme_QosInternalReleaseReq(tpAniSirGlobal pMac, 
                                            v_U32_t QosFlowID,
                                            v_BOOL_t buffered_cmd);
sme_QosStatusType sme_QosSetup(tpAniSirGlobal pMac,
                               v_U8_t sessionId,
                               sme_QosWmmTspecInfo *pTspec_Info, 
                               sme_QosEdcaAcType ac);
eHalStatus sme_QosAddTsReq(tpAniSirGlobal pMac,
                           v_U8_t sessionId,
                           sme_QosWmmTspecInfo * pTspec_Info,
                           sme_QosEdcaAcType ac);
eHalStatus sme_QosDelTsReq(tpAniSirGlobal pMac,
                           v_U8_t sessionId,
                           sme_QosEdcaAcType ac,
                           v_U8_t tspec_mask);
eHalStatus sme_QosProcessAddTsRsp(tpAniSirGlobal pMac, void *pMsgBuf);
eHalStatus sme_QosProcessDelTsInd(tpAniSirGlobal pMac, void *pMsgBuf);
eHalStatus sme_QosProcessDelTsRsp(tpAniSirGlobal pMac, void *pMsgBuf);
eHalStatus sme_QosProcessAssocCompleteEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessReassocReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessReassocSuccessEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessReassocFailureEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessDisconnectEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessJoinReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessHandoffAssocReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessHandoffSuccessEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessHandoffFailureEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
#ifdef WLAN_FEATURE_VOWIFI_11R
eHalStatus sme_QosProcessPreauthSuccessInd(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessSetKeySuccessInd(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info);
eHalStatus sme_QosProcessAggrQosRsp(tpAniSirGlobal pMac, void *pMsgBuf);
eHalStatus sme_QosFTAggrQosReq( tpAniSirGlobal pMac, v_U8_t sessionId );
#endif
eHalStatus sme_QosProcessAddTsSuccessRsp(tpAniSirGlobal pMac, 
                                         v_U8_t sessionId,
                                         tSirAddtsRspInfo * pRsp);
eHalStatus sme_QosProcessAddTsFailureRsp(tpAniSirGlobal pMac, 
                                         v_U8_t sessionId,
                                         tSirAddtsRspInfo * pRsp);
eHalStatus sme_QosAggregateParams(
   sme_QosWmmTspecInfo * pInput_Tspec_Info,
   sme_QosWmmTspecInfo * pCurrent_Tspec_Info,
   sme_QosWmmTspecInfo * pUpdated_Tspec_Info);
static eHalStatus sme_QosUpdateParams(v_U8_t sessionId,
                                      sme_QosEdcaAcType ac,
                                      v_U8_t tspec_mask, 
                                      sme_QosWmmTspecInfo * pTspec_Info);
sme_QosWmmUpType sme_QosAcToUp(sme_QosEdcaAcType ac);
sme_QosEdcaAcType sme_QosUpToAc(sme_QosWmmUpType up);
v_BOOL_t sme_QosIsACM(tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc, 
                      sme_QosEdcaAcType ac, tDot11fBeaconIEs *pIes);
tListElem *sme_QosFindInFlowList(sme_QosSearchInfo search_key);
eHalStatus sme_QosFindAllInFlowList(tpAniSirGlobal pMac,
                                    sme_QosSearchInfo search_key, 
                                    sme_QosProcessSearchEntry fnp);
static void sme_QosStateTransition(v_U8_t sessionId,
                                   sme_QosEdcaAcType ac,
                                   sme_QosStates new_state);
eHalStatus sme_QosBufferCmd(sme_QosCmdInfo *pcmd, v_BOOL_t insert_head);
static eHalStatus sme_QosProcessBufferedCmd(v_U8_t sessionId);
eHalStatus sme_QosSaveAssocInfo(sme_QosSessionInfo *pSession, sme_QosAssocInfo *pAssoc_info);
eHalStatus sme_QosSetupFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosModificationNotifyFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosModifyFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosDelTsIndFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosReassocSuccessEvFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosAddTsFailureFnp(tpAniSirGlobal pMac, tListElem *pEntry);
eHalStatus sme_QosAddTsSuccessFnp(tpAniSirGlobal pMac, tListElem *pEntry);
static v_BOOL_t sme_QosIsRspPending(v_U8_t sessionId, sme_QosEdcaAcType ac);
static v_BOOL_t sme_QosIsUapsdActive(void);
void sme_QosPmcFullPowerCallback(void *callbackContext, eHalStatus status);
void sme_QosPmcStartUapsdCallback(void *callbackContext, eHalStatus status);
v_BOOL_t sme_QosPmcCheckRoutine(void *callbackContext);
void sme_QosPmcDeviceStateUpdateInd(void *callbackContext, tPmcState pmcState);
eHalStatus sme_QosProcessOutOfUapsdMode(tpAniSirGlobal pMac);
eHalStatus sme_QosProcessIntoUapsdMode(tpAniSirGlobal pMac);
static eHalStatus sme_QosBufferExistingFlows(tpAniSirGlobal pMac,
                                             v_U8_t sessionId);
static eHalStatus sme_QosDeleteExistingFlows(tpAniSirGlobal pMac,
                                             v_U8_t sessionId);
static void sme_QosCleanupCtrlBlkForHandoff(tpAniSirGlobal pMac,
                                            v_U8_t sessionId);
static eHalStatus sme_QosDeleteBufferedRequests(tpAniSirGlobal pMac,
                                                v_U8_t sessionId);
v_BOOL_t sme_QosValidateRequestedParams(tpAniSirGlobal pMac,
    sme_QosWmmTspecInfo * pQoSInfo,
    v_U8_t sessionId);

extern eHalStatus sme_AcquireGlobalLock( tSmeStruct *psSme);
extern eHalStatus sme_ReleaseGlobalLock( tSmeStruct *psSme);
static eHalStatus qosIssueCommand( tpAniSirGlobal pMac, v_U8_t sessionId,
                                   eSmeCommandType cmdType, sme_QosWmmTspecInfo * pQoSInfo,
                                   sme_QosEdcaAcType ac, v_U8_t tspec_mask );
/*
    sme_QosReRequestAddTS to re-send AddTS for the combined QoS request
*/
static sme_QosStatusType sme_QosReRequestAddTS(tpAniSirGlobal pMac,
                                               v_U8_t sessionId,
                                               sme_QosWmmTspecInfo * pQoSInfo,
                                               sme_QosEdcaAcType ac,
                                               v_U8_t tspecMask);
static void sme_QosInitACs(tpAniSirGlobal pMac, v_U8_t sessionId);
static eHalStatus sme_QosRequestReassoc(tpAniSirGlobal pMac, tANI_U8 sessionId,
                                        tCsrRoamModifyProfileFields *pModFields,
                                        v_BOOL_t fForce );
static v_U32_t sme_QosAssignFlowId(void);
static v_U8_t sme_QosAssignDialogToken(void);
static eHalStatus sme_QosUpdateTspecMask(v_U8_t sessionId,
                                      sme_QosSearchInfo search_key,
                                      v_U8_t new_tspec_mask);
/*-------------------------------------------------------------------------- 
                         External APIs definitions
  ------------------------------------------------------------------------*/
/* --------------------------------------------------------------------------
    \brief sme_QosOpen() - This function must be called before any API call to 
    SME QoS module.
    \param pMac - Pointer to the global MAC parameter structure.
    
    \return eHalStatus     
----------------------------------------------------------------------------*/
eHalStatus sme_QosOpen(tpAniSirGlobal pMac)
{
   sme_QosSessionInfo *pSession;
   v_U8_t sessionId;
   eHalStatus status;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: initializing SME-QoS module",
             __func__, __LINE__);
   //init the control block
   //(note that this will make all sessions invalid)
   vos_mem_zero(&sme_QosCb, sizeof(sme_QosCb));
   sme_QosCb.pMac = pMac;
   sme_QosCb.nextFlowId = SME_QOS_MIN_FLOW_ID;
   sme_QosCb.nextDialogToken = SME_QOS_MIN_DIALOG_TOKEN;
   //init flow list
   status = csrLLOpen(pMac->hHdd, &sme_QosCb.flow_list);
   if (!HAL_STATUS_SUCCESS(status))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: %d: cannot initialize Flow List",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   
   for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; ++sessionId)
   {
      pSession = &sme_QosCb.sessionInfo[sessionId];
      pSession->sessionId = sessionId;
      // initialize the session's per-AC information
      sme_QosInitACs(pMac, sessionId);
      // initialize the session's buffered command list
      status = csrLLOpen(pMac->hHdd, &pSession->bufferedCommandList);
      if (!HAL_STATUS_SUCCESS(status))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                   "%s: %d: cannot initialize cmd list for session %d",
                   __func__, __LINE__,
                   sessionId);
         return eHAL_STATUS_FAILURE;
      }
      pSession->readyForPowerSave = VOS_TRUE;
   }
   //the routine registered here gets called by PMC whenever the device is about 
   //to enter one of the power save modes. PMC runs a poll with all the 
   //registered modules if device can enter powersave mode or remain full power
   if(!HAL_STATUS_SUCCESS(
      pmcRegisterPowerSaveCheck(pMac, sme_QosPmcCheckRoutine, pMac)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: %d: cannot register with pmcRegisterPowerSaveCheck()",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   //the routine registered here gets called by PMC whenever there is a device 
   // state change. PMC might go to full power because of many reasons and this 
   // is the way for PMC to inform all the other registered modules so that 
   // everyone is in sync.
   if(!HAL_STATUS_SUCCESS(
      pmcRegisterDeviceStateUpdateInd(pMac, sme_QosPmcDeviceStateUpdateInd, pMac)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: %d: cannot register with pmcRegisterDeviceStateUpdateInd()",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: done initializing SME-QoS module",
             __func__, __LINE__);
   return eHAL_STATUS_SUCCESS;
}
/* --------------------------------------------------------------------------
    \brief sme_QosClose() - To close down SME QoS module. There should not be 
    any API call into this module after calling this function until another
    call of sme_QosOpen.
    \param pMac - Pointer to the global MAC parameter structure.
    
    \return eHalStatus     
----------------------------------------------------------------------------*/
eHalStatus sme_QosClose(tpAniSirGlobal pMac)
{
   sme_QosSessionInfo *pSession;
   sme_QosEdcaAcType ac;
   v_U8_t sessionId;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: closing down SME-QoS",
             __func__, __LINE__);
   // deregister with PMC
   if(!HAL_STATUS_SUCCESS(
      pmcDeregisterDeviceStateUpdateInd(pMac, sme_QosPmcDeviceStateUpdateInd)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: %d: cannot deregister with pmcDeregisterDeviceStateUpdateInd()",
                __func__, __LINE__);
   }
   if(!HAL_STATUS_SUCCESS(
      pmcDeregisterPowerSaveCheck(pMac, sme_QosPmcCheckRoutine)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: %d: cannot deregister with pmcDeregisterPowerSaveCheck()",
                __func__, __LINE__);
   }
   //cleanup control block
   //close the flow list
   csrLLClose(&sme_QosCb.flow_list);
   // shut down all of the sessions
   for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; ++sessionId)
   {
      pSession = &sme_QosCb.sessionInfo[sessionId];
      if (pSession == NULL)
            continue;

       sme_QosInitACs(pMac, sessionId);
       // this session doesn't require UAPSD
       pSession->apsdMask = 0;

       pSession->uapsdAlreadyRequested = VOS_FALSE;
       pSession->handoffRequested = VOS_FALSE;
       pSession->readyForPowerSave = VOS_TRUE;
       pSession->roamID = 0;
       //need to clean up buffered req
       sme_QosDeleteBufferedRequests(pMac, sessionId);
       //need to clean up flows
       sme_QosDeleteExistingFlows(pMac, sessionId);

       // Clean up the assoc info if already allocated
       if (pSession->assocInfo.pBssDesc) {
          vos_mem_free(pSession->assocInfo.pBssDesc);
          pSession->assocInfo.pBssDesc = NULL;
       }

      // close the session's buffered command list
      csrLLClose(&pSession->bufferedCommandList);
      for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
      {
         sme_QosStateTransition(sessionId, ac, SME_QOS_CLOSED);
      }
      pSession->sessionActive = VOS_FALSE;
      pSession->readyForPowerSave = VOS_TRUE;
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: closed down QoS",
             __func__, __LINE__);
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosSetupReq() - The SME QoS API exposed to HDD to request for QoS 
  on a particular AC. This function should be called after a link has been 
  established, i.e. STA is associated with an AP etc. If the request involves 
  admission control on the requested AC, HDD needs to provide the necessary 
  Traffic Specification (TSPEC) parameters otherwise SME is going to use the
  default params.
  
  \param hHal - The handle returned by macOpen.
  \param sessionId - sessionId returned by sme_OpenSession.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QoSCallback - The callback which is registered per flow while 
                       requesting for QoS. Used for any notification for the 
                       flow (i.e. setup success/failure/release) which needs to 
                       be sent to HDD
  \param HDDcontext - A cookie passed by HDD to be used by SME during any QoS 
                      notification (through the callabck) to HDD 
  \param UPType - Useful only if HDD or any other upper layer module (BAP etc.)
                  looking for implicit QoS setup, in that 
                  case, the pQoSInfo will be NULL & SME will know about the AC
                  (from the UP provided in this param) QoS is requested on
  \param pQosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow is 
                     successful
                  
  \return eHAL_STATUS_SUCCESS - Setup is successful.
  
          Other status means Setup request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosSetupReq(tHalHandle hHal, tANI_U32 sessionId,
                                  sme_QosWmmTspecInfo * pQoSInfo,
                                  sme_QosCallback QoSCallback,
                                  void * HDDcontext,
                                  sme_QosWmmUpType UPType, v_U32_t * pQosFlowID)
{
   sme_QosSessionInfo *pSession;
   eHalStatus lock_status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   sme_QosStatusType status;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Setup requested by client on session %d",
             __func__, __LINE__,
             sessionId);
   lock_status = sme_AcquireGlobalLock( &pMac->sme );
   if ( !HAL_STATUS_SUCCESS( lock_status ) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Unable to obtain lock",
                __func__, __LINE__);
      return SME_QOS_STATUS_SETUP_FAILURE_RSP;
   }
   //Make sure the session is valid
   if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Supplied Session ID %d is invalid",
                __func__, __LINE__,
                sessionId);
      status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   }
   else
   {
      //Make sure the session is active
      pSession = &sme_QosCb.sessionInfo[sessionId];
      if (!pSession->sessionActive)
      { 
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: Supplied Session ID %d is inactive",
                   __func__, __LINE__,
                   sessionId);
         status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
      }
      else
      {
         //Assign a Flow ID
         *pQosFlowID = sme_QosAssignFlowId();
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: QoS request on session %d assigned Flow ID %d",
                   __func__, __LINE__,
                   sessionId, *pQosFlowID);
         //Call the internal function for QoS setup,
         // adding a layer of abstraction
         status = sme_QosInternalSetupReq(pMac, (v_U8_t)sessionId, pQoSInfo,
                                          QoSCallback, HDDcontext, UPType,
                                          *pQosFlowID, VOS_FALSE, VOS_FALSE);
      }
   }
   sme_ReleaseGlobalLock( &pMac->sme );
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS setup return status on session %d is %d",
             __func__, __LINE__,
             sessionId, status);
   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosModifyReq() - The SME QoS API exposed to HDD to request for 
  modification of certain QoS params on a flow running on a particular AC. 
  This function should be called after a link has been established, i.e. STA is 
  associated with an AP etc. & a QoS setup has been succesful for that flow. 
  If the request involves admission control on the requested AC, HDD needs to 
  provide the necessary Traffic Specification (TSPEC) parameters & SME might
  start the renegotiation process through ADDTS.
  
  \param hHal - The handle returned by macOpen.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow has 
                     been successful already
                  
  \return SME_QOS_STATUS_SETUP_SUCCESS_RSP - Modification is successful.
  
          Other status means request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosModifyReq(tHalHandle hHal, 
                                   sme_QosWmmTspecInfo * pQoSInfo,
                                   v_U32_t QosFlowID)
{
   eHalStatus lock_status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   sme_QosStatusType status;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Modify requested by client for Flow %d",
             __func__, __LINE__,
             QosFlowID);
   lock_status = sme_AcquireGlobalLock( &pMac->sme );
   if ( !HAL_STATUS_SUCCESS( lock_status ) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Unable to obtain lock",
                __func__, __LINE__);
      return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
   }
   //Call the internal function for QoS modify, adding a layer of abstraction
   status = sme_QosInternalModifyReq(pMac, pQoSInfo, QosFlowID, VOS_FALSE);
   sme_ReleaseGlobalLock( &pMac->sme );
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Modify return status on Flow %d is %d",
             __func__, __LINE__,
             QosFlowID, status);
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosReleaseReq() - The SME QoS API exposed to HDD to request for 
  releasing a QoS flow running on a particular AC. This function should be 
  called only if a QoS is set up with a valid FlowID. HDD sould invoke this 
  API only if an explicit request for QoS release has come from Application 
  
  \param hHal - The handle returned by macOpen.
  \param QosFlowID - Identification per flow running on each AC generated by SME
                     It is only meaningful if the QoS setup for the flow is 
                     successful
  
  \return eHAL_STATUS_SUCCESS - Release is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosReleaseReq(tHalHandle hHal, v_U32_t QosFlowID)
{
   eHalStatus lock_status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   sme_QosStatusType status;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Release requested by client for Flow %d",
             __func__, __LINE__,
             QosFlowID);
   lock_status = sme_AcquireGlobalLock( &pMac->sme );
   if ( !HAL_STATUS_SUCCESS( lock_status ) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Unable to obtain lock",
                __func__, __LINE__);
      return SME_QOS_STATUS_RELEASE_FAILURE_RSP;
   }
   //Call the internal function for QoS release, adding a layer of abstraction
   status = sme_QosInternalReleaseReq(pMac, QosFlowID, VOS_FALSE);
   sme_ReleaseGlobalLock( &pMac->sme );
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Release return status on Flow %d is %d",
             __func__, __LINE__,
             QosFlowID, status);
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosSetParams() - This function is used by HDD to provide the 
   default TSPEC params to SME.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info per AC as defined above, provided by HDD
  
  \return eHAL_STATUS_SUCCESS - Setparam is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosSetParams(tpAniSirGlobal pMac, sme_QosWmmTspecInfo * pQoSInfo)
{
   sme_QosEdcaAcType ac;
   // find the AC
   ac = sme_QosUpToAc(pQoSInfo->ts_info.up);
   if(SME_QOS_EDCA_AC_MAX == ac)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Invalid AC %d (via UP %d)",
                __func__, __LINE__,
                ac, pQoSInfo->ts_info.up );
      return eHAL_STATUS_FAILURE;
   }
   //copy over the default params for this AC
   sme_QosCb.def_QoSInfo[ac] = *pQoSInfo;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS default params set for AC %d (via UP %d)",
             __func__, __LINE__,
             ac, pQoSInfo->ts_info.up );
   return eHAL_STATUS_SUCCESS;
}

void qosReleaseCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
   vos_mem_zero( &pCommand->u.qosCmd, sizeof( tGenericQosCmd ) );
   smeReleaseCommand( pMac, pCommand );
}

/*--------------------------------------------------------------------------
  \brief sme_QosMsgProcessor() - sme_ProcessMsg() calls this function for the 
  messages that are handled by SME QoS module.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param msg_type - the type of msg passed by PE as defined in wniApi.h
  \param pMsgBuf - a pointer to a buffer that maps to various structures base 
                   on the message type.
                   The beginning of the buffer can always map to tSirSmeRsp.
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosMsgProcessor( tpAniSirGlobal pMac,  v_U16_t msg_type, 
                                void *pMsgBuf)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tListElem *pEntry;
   tSmeCmd *pCommand;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: msg = %d for QoS",
             __func__, __LINE__, msg_type);
   //switch on the msg type & make the state transition accordingly
   switch(msg_type)
   {
      case eWNI_SME_ADDTS_RSP:
         pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
         if( pEntry )
         {
             pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
             if( eSmeCommandAddTs == pCommand->command )
             {
                status = sme_QosProcessAddTsRsp(pMac, pMsgBuf);
                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                {
                   qosReleaseCommand( pMac, pCommand );
                }
                smeProcessPendingQueue( pMac );
             }
         }
         break;
      case eWNI_SME_DELTS_RSP:
         pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
         if( pEntry )
         {
             pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
             if( eSmeCommandDelTs == pCommand->command )
             {
                status = sme_QosProcessDelTsRsp(pMac, pMsgBuf);
                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                {
                   qosReleaseCommand( pMac, pCommand );
                }
                smeProcessPendingQueue( pMac );
             }
         }
         break;
      case eWNI_SME_DELTS_IND:
         status = sme_QosProcessDelTsInd(pMac, pMsgBuf);
         break;
#ifdef WLAN_FEATURE_VOWIFI_11R
      case eWNI_SME_FT_AGGR_QOS_RSP:
         status = sme_QosProcessAggrQosRsp(pMac, pMsgBuf);
         break;
#endif

      default:
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: unknown msg type = %d",
                   __func__, __LINE__, msg_type);
         break;
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosValidateParams() - The SME QoS API exposed to CSR to validate AP
  capabilities regarding QoS support & any other QoS parameter validation.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pBssDesc - Pointer to the BSS Descriptor information passed down by 
                    CSR to PE while issuing the Join request
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosValidateParams(tpAniSirGlobal pMac, 
                                 tSirBssDescription *pBssDesc)
{
   tDot11fBeaconIEs *pIes = NULL;
   eHalStatus status = eHAL_STATUS_FAILURE;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: validation for QAP & APSD",
             __func__, __LINE__);
   do
   {
      if(!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIes)))
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: csrGetParsedBssDescriptionIEs() failed",
                   __func__, __LINE__);
         break;
      }
      //check if the AP is QAP & it supports APSD
      if( !CSR_IS_QOS_BSS(pIes) )
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: AP doesn't support QoS",
                   __func__, __LINE__);
         
         break;
      }
      if(!(pIes->WMMParams.qosInfo & SME_QOS_AP_SUPPORTS_APSD) &&
         !(pIes->WMMInfoAp.uapsd))
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: AP doesn't support APSD",
                   __func__, __LINE__);
         break;
      }
      status = eHAL_STATUS_SUCCESS;
   }while(0);
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: validated with status = %d",
             __func__, __LINE__, status);
   if(pIes)
   {
      vos_mem_free(pIes);
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosCsrEventInd() - The QoS sub-module in SME expects notifications 
  from CSR when certain events occur as mentioned in sme_QosCsrEventIndType.
  \param pMac - Pointer to the global MAC parameter structure.
  \param ind - The event occurred of type sme_QosCsrEventIndType.
  \param pEvent_info - Information related to the event
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosCsrEventInd(tpAniSirGlobal pMac,
                              v_U8_t sessionId,
                              sme_QosCsrEventIndType ind, 
                              void *pEvent_info)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: On Session %d Event %d received from CSR",
             __func__, __LINE__,
             sessionId, ind );
   switch(ind)
   {
      case SME_QOS_CSR_ASSOC_COMPLETE:
         //expecting assoc info in pEvent_info
         status = sme_QosProcessAssocCompleteEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_REASSOC_REQ:
         //nothing expected in pEvent_info
         status = sme_QosProcessReassocReqEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_REASSOC_COMPLETE:
         //expecting assoc info in pEvent_info
         status = sme_QosProcessReassocSuccessEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_REASSOC_FAILURE:
         //nothing expected in pEvent_info
         status = sme_QosProcessReassocFailureEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_DISCONNECT_REQ:
      case SME_QOS_CSR_DISCONNECT_IND:
         //nothing expected in pEvent_info
         status = sme_QosProcessDisconnectEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_JOIN_REQ:
         //nothing expected in pEvent_info
         status = sme_QosProcessJoinReqEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_HANDOFF_ASSOC_REQ:
         //nothing expected in pEvent_info
         status = sme_QosProcessHandoffAssocReqEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_HANDOFF_COMPLETE:
         //nothing expected in pEvent_info
         status = sme_QosProcessHandoffSuccessEv(pMac, sessionId, pEvent_info);
         break;
      case SME_QOS_CSR_HANDOFF_FAILURE:
         //nothing expected in pEvent_info
         status = sme_QosProcessHandoffFailureEv(pMac, sessionId, pEvent_info);
         break;
#ifdef WLAN_FEATURE_VOWIFI_11R
      case SME_QOS_CSR_PREAUTH_SUCCESS_IND:
         status = sme_QosProcessPreauthSuccessInd(pMac, sessionId, pEvent_info);
         break;
#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
      case SME_QOS_CSR_SET_KEY_SUCCESS_IND:
         status = sme_QosProcessSetKeySuccessInd(pMac, sessionId, pEvent_info);
         break;
#endif
#endif
      default:
         //Err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On Session %d Unknown Event %d received from CSR",
                   __func__, __LINE__,
                   sessionId, ind );
         break;
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: On Session %d processed Event %d with status %d",
             __func__, __LINE__,
             sessionId, ind, status );
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosGetACMMask() - The QoS sub-module API to find out on which ACs
  AP mandates Admission Control (ACM = 1)
  (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored)
  \param pMac - Pointer to the global MAC parameter structure.
  \param pSirBssDesc - The event occurred of type sme_QosCsrEventIndType.

  \return a bit mask indicating for which ACs AP has ACM set to 1
  
  \sa
  
  --------------------------------------------------------------------------*/
v_U8_t sme_QosGetACMMask(tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes)
{
   sme_QosEdcaAcType ac;
   v_U8_t acm_mask = 0;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked",
             __func__, __LINE__);
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
   {
      if(sme_QosIsACM(pMac, pSirBssDesc, ac, pIes))
      {
         acm_mask = acm_mask | (1 << (SME_QOS_EDCA_AC_VO - ac));
      }
      
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: mask is %d",
             __func__, __LINE__, acm_mask);
   return acm_mask;
}
/*-------------------------------------------------------------------------- 
                         Internal function definitions
  ------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------
  \brief sme_QosInternalSetupReq() - The SME QoS internal setup request handling
  function.
  If the request involves admission control on the requested AC, HDD needs to 
  provide the necessary Traffic Specification (TSPEC) parameters otherwise SME 
  is going to use the default params.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QoSCallback - The callback which is registered per flow while 
                       requesting for QoS. Used for any notification for the 
                       flow (i.e. setup success/failure/release) which needs to 
                       be sent to HDD
  \param HDDcontext - A cookie passed by HDD to be used by SME during any QoS 
                      notification (through the callabck) to HDD 
  \param UPType - Useful only if HDD or any other upper layer module (BAP etc.)
                  looking for implicit QoS setup, in that 
                  case, the pQoSInfo will be NULL & SME will know about the AC
                  (from the UP provided in this param) QoS is requested on
  \param QosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow is 
                     successful
  \param buffered_cmd - tells us if the cmd was a buffered one or fresh from 
                        client
                  
  \return eHAL_STATUS_SUCCESS - Setup is successful.
  
          Other status means Setup request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosInternalSetupReq(tpAniSirGlobal pMac, 
                                          v_U8_t sessionId,
                                          sme_QosWmmTspecInfo * pQoSInfo,
                                          sme_QosCallback QoSCallback, 
                                          void * HDDcontext,
                                          sme_QosWmmUpType UPType, 
                                          v_U32_t QosFlowID,
                                          v_BOOL_t buffered_cmd,
                                          v_BOOL_t hoRenewal)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac;
   sme_QosWmmTspecInfo Tspec_Info;
   sme_QosStates new_state = SME_QOS_CLOSED;
   sme_QosFlowInfoEntry *pentry = NULL;
   sme_QosCmdInfo  cmd;
   sme_QosStatusType status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   v_U8_t tmask = 0;
   v_U8_t new_tmask = 0;
   sme_QosSearchInfo search_key;
   v_BOOL_t bufferCommand;
   eHalStatus hstatus;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for flow %d",
             __func__, __LINE__,
             sessionId, QosFlowID);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   // if caller sent an empty TSPEC, fill up with the default one
   if(!pQoSInfo)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
                "%s: %d: caller sent an empty QoS param list, using defaults",
                __func__, __LINE__);
      // find the AC with UPType passed in
      ac = sme_QosUpToAc(UPType);
      if(SME_QOS_EDCA_AC_MAX == ac)
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: invalid AC %d from UP %d",
                   __func__, __LINE__,
                   ac, UPType);
         
         return SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP;
      }
      Tspec_Info = sme_QosCb.def_QoSInfo[ac];
   }
   else
   {
      // find the AC
      ac = sme_QosUpToAc(pQoSInfo->ts_info.up);
      if(SME_QOS_EDCA_AC_MAX == ac)
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: invalid AC %d from UP %d",
                   __func__, __LINE__,
                   ac, pQoSInfo->ts_info.up);
         
         return SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP;
      }
      //validate QoS params
      if(!sme_QosValidateRequestedParams(pMac, pQoSInfo, sessionId))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: invalid params",
                   __func__, __LINE__);
         return SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP;
      }
      Tspec_Info = *pQoSInfo;
   }
   pACInfo = &pSession->ac_info[ac];
   // need to vote off powersave for the duration of this request
   pSession->readyForPowerSave = VOS_FALSE;
   // assume we won't have to (re)buffer the command
   bufferCommand = VOS_FALSE;
   //check to consider the following flowing scenario
   //Addts request is pending on one AC, while APSD requested on another which 
   //needs a reassoc. Will buffer a request if Addts is pending on any AC, 
   //which will safegaurd the above scenario, & also won't confuse PE with back 
   //to back Addts or Addts followed by Reassoc
   if(sme_QosIsRspPending(sessionId, ac))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: buffering the setup request for flow %d in state %d "
                "since another request is pending",
                __func__, __LINE__, 
                QosFlowID, pACInfo->curr_state );
      bufferCommand = VOS_TRUE;
   }
   else
   {
      // make sure we are in full power so that we can issue
      // an AddTS or reassoc if necessary
      hstatus = pmcRequestFullPower(pMac, sme_QosPmcFullPowerCallback,
                                    pSession, eSME_REASON_OTHER);
      if( eHAL_STATUS_PMC_PENDING == hstatus )
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                   "%s: %d: buffering the setup request for flow %d in state %d, "
                   "waiting for full power",
                   __func__, __LINE__, 
                   QosFlowID, pACInfo->curr_state );
         bufferCommand = VOS_TRUE;
      }
   }
   if (bufferCommand)
   {
      // we need to buffer the command
      cmd.command = SME_QOS_SETUP_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.setupCmdInfo.HDDcontext = HDDcontext;
      cmd.u.setupCmdInfo.QoSInfo = Tspec_Info;
      cmd.u.setupCmdInfo.QoSCallback = QoSCallback;
      cmd.u.setupCmdInfo.UPType = UPType;
      cmd.u.setupCmdInfo.hoRenewal = hoRenewal;
      cmd.u.setupCmdInfo.QosFlowID = QosFlowID;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the setup request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_SETUP_FAILURE_RSP;
      }
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Buffered setup request for flow = %d",
                __func__, __LINE__,
                QosFlowID);
      return SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
   }

   //get into the state m/c to see if the request can be granted
   switch(pACInfo->curr_state)
   {
   case SME_QOS_LINK_UP:
      //call the internal qos setup logic to decide on if the
      // request is NOP, or need reassoc for APSD and/or need to send out ADDTS
      status = sme_QosSetup(pMac, sessionId, &Tspec_Info, ac);
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: On session %d with AC %d in state SME_QOS_LINK_UP "
                "sme_QosSetup returned with status %d",
                __func__, __LINE__,
                sessionId, ac, status);
      if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP != status)
      {
         // we aren't waiting for a response from the AP
         // so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
      }
      if((SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status)||
         (SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
         (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status))
      {
         // we received an expected "good" status
         //create an entry in the flow list
         pentry = vos_mem_malloc(sizeof(*pentry));
         if (!pentry)
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: couldn't allocate memory for the new "
                      "entry in the Flow List",
                      __func__, __LINE__);
            return SME_QOS_STATUS_SETUP_FAILURE_RSP;
         }
         pentry->ac_type = ac;
         pentry->HDDcontext = HDDcontext;
         pentry->QoSCallback = QoSCallback;
         pentry->hoRenewal = hoRenewal;
         pentry->QosFlowID = QosFlowID;
         pentry->sessionId = sessionId;
         // since we are in state SME_QOS_LINK_UP this must be the
         // first TSPEC on this AC, so use index 0 (mask bit 1)
         pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0] = Tspec_Info;
         if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status)
         {
            if(pACInfo->tspec_mask_status &&
               !pACInfo->reassoc_pending)
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: On session %d with AC %d in state "
                            "SME_QOS_LINK_UP tspec_mask_status is %d "
                         "but should not be set yet",
                         __func__, __LINE__,
                         sessionId, ac, pACInfo->tspec_mask_status);
               //ASSERT
               VOS_ASSERT(0);
               vos_mem_free(pentry);
               return SME_QOS_STATUS_SETUP_FAILURE_RSP;
            }
            pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_BIT_1_SET;
            if(!pACInfo->reassoc_pending)
            {
               // we didn't request for reassoc, it must be a tspec negotiation
               pACInfo->tspec_pending = 1;
            }
             
            pentry->reason = SME_QOS_REASON_SETUP;
            new_state = SME_QOS_REQUESTED;
         }
         else
         {
            // SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP or
            // SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY
            pentry->reason = SME_QOS_REASON_REQ_SUCCESS;
            new_state = SME_QOS_QOS_ON;
            pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_BIT_1_SET;
            pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0] = Tspec_Info;
            if(buffered_cmd && !pentry->hoRenewal)
            {
               QoSCallback(pMac, HDDcontext, 
                           &pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0],
                           status,
                           pentry->QosFlowID);
            }
            pentry->hoRenewal = VOS_FALSE;
         }
         pACInfo->num_flows[SME_QOS_TSPEC_INDEX_0]++;

         //indicate on which index the flow entry belongs to & add it to the 
         //Flow List at the end
         pentry->tspec_mask = pACInfo->tspec_mask_status;
         pentry->QoSInfo = Tspec_Info;
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Creating entry on session %d at %p with flowID %d",
                   __func__, __LINE__,
                   sessionId, pentry, QosFlowID);
         csrLLInsertTail(&sme_QosCb.flow_list, &pentry->link, VOS_TRUE);
      }
      else
      {
         // unexpected status returned by sme_QosSetup()
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On session %d unexpected status %d "
                   "returned by sme_QosSetup",
                   __func__, __LINE__,
                   sessionId, status);
         new_state = pACInfo->curr_state;
         if(buffered_cmd && hoRenewal)
         {
            QoSCallback(pMac, HDDcontext, 
                        &pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0],
                        SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
                        QosFlowID);
         }
      }
      break;
   case SME_QOS_HANDOFF:
   case SME_QOS_REQUESTED:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: Buffering setup request for flow %d in state = %d",
                __func__, __LINE__,
                QosFlowID, pACInfo->curr_state );
      //buffer cmd
      cmd.command = SME_QOS_SETUP_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.setupCmdInfo.HDDcontext = HDDcontext;
      cmd.u.setupCmdInfo.QoSInfo = Tspec_Info;
      cmd.u.setupCmdInfo.QoSCallback = QoSCallback;
      cmd.u.setupCmdInfo.UPType = UPType;
      cmd.u.setupCmdInfo.hoRenewal = hoRenewal;
      cmd.u.setupCmdInfo.QosFlowID = QosFlowID;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On session %d couldn't buffer the setup "
                   "request for flow %d in state = %d",
                   __func__, __LINE__,
                   sessionId, QosFlowID, pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_SETUP_FAILURE_RSP;
      }
      status = SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
      new_state = pACInfo->curr_state;
      break;
   case SME_QOS_QOS_ON:
      
      //check if multiple flows running on the ac
      if((pACInfo->num_flows[SME_QOS_TSPEC_INDEX_0] > 0)||
         (pACInfo->num_flows[SME_QOS_TSPEC_INDEX_1] > 0))
      {
         //do we need to care about the case where APSD needed on ACM = 0 below?
         if(CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(pMac) ||
            sme_QosIsACM(pMac, pSession->assocInfo.pBssDesc, ac, NULL))
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                      "%s: %d: tspec_mask_status = %d for AC = %d",
                      __func__, __LINE__,
                      pACInfo->tspec_mask_status, ac);
            if(!pACInfo->tspec_mask_status)
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: tspec_mask_status can't be 0 for ac = %d in "
                         "state = %d",
                         __func__, __LINE__,
                         ac, pACInfo->curr_state);
               //ASSERT
               VOS_ASSERT(0);
               // unable to service the request
               // nothing is pending so vote powersave back on
               pSession->readyForPowerSave = VOS_TRUE;
               return status;
            }
            /* Flow aggregation */
            if(((pACInfo->tspec_mask_status > 0) &&
                (pACInfo->tspec_mask_status <= SME_QOS_TSPEC_INDEX_MAX)))
            {
              /* Either of upstream, downstream or bidirectional flows are present */
              /* If either of new stream or current stream is for bidirecional, aggregate 
               * the new stream with the current streams present and send out aggregated Tspec.*/
              if((Tspec_Info.ts_info.direction == SME_QOS_WMM_TS_DIR_BOTH) ||
                 (pACInfo->curr_QoSInfo[pACInfo->tspec_mask_status - 1].
                      ts_info.direction == SME_QOS_WMM_TS_DIR_BOTH))
              {
                // Aggregate the new stream with the current stream(s).
                tmask = pACInfo->tspec_mask_status;
              }
              /* None of new stream or current (aggregated) streams are for bidirectional.
               * Check if the new stream direction matches the current stream direction. */
              else if(pACInfo->curr_QoSInfo[pACInfo->tspec_mask_status - 1].
                  ts_info.direction == Tspec_Info.ts_info.direction)
              {
                // Aggregate the new stream with the current stream(s).
                tmask = pACInfo->tspec_mask_status;
              }
              /* New stream is in different direction. */
              else
              {
                // No Aggregation. Mark the 2nd tpsec index also as active.
                tmask = SME_QOS_TSPEC_MASK_CLEAR;
                new_tmask = SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~pACInfo->tspec_mask_status;
                pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_BIT_1_2_SET;
              }
            }
            else if(SME_QOS_TSPEC_MASK_BIT_1_2_SET == pACInfo->tspec_mask_status)
            {
              /* Both uplink and downlink streams are present. */
              /* If new stream is bidirectional, aggregate new stream with all existing
               * upstreams and downstreams. Send out new aggregated tpsec. */
              if(Tspec_Info.ts_info.direction == SME_QOS_WMM_TS_DIR_BOTH)
              {
                // Only one tspec index (0) will be in use after this aggregation.
                tmask = SME_QOS_TSPEC_MASK_BIT_1_2_SET;
                pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_BIT_1_SET;
              }
              /* New stream is also uni-directional
               * Find out the tsepc index with which it needs to be aggregated */
              else if(pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0].ts_info.direction != 
                   Tspec_Info.ts_info.direction)
              {
                // Aggregate with 2nd tspec index
                tmask = SME_QOS_TSPEC_MASK_BIT_2_SET;
              }
              else
              {
                // Aggregate with 1st tspec index
                tmask = SME_QOS_TSPEC_MASK_BIT_1_SET;
              }
            }
            else
            {
              VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
                "%s: %d: wrong tmask = %d", __func__, __LINE__,
                pACInfo->tspec_mask_status );
            }
         }
         else
         {
            //ACM = 0
            // We won't be sending a TSPEC to the AP but we still need
            // to aggregate to calculate trigger frame parameters
            tmask = SME_QOS_TSPEC_MASK_BIT_1_SET;
         }
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
                   "%s: %d: tmask = %d, new_tmask = %d in state = %d",
                   __func__, __LINE__,
                   tmask, new_tmask, pACInfo->curr_state );
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
                   "%s: %d: tspec_mask_status = %d for AC = %d",
                   __func__, __LINE__,
                   pACInfo->tspec_mask_status, ac);
         if(tmask)
         {
            // create the aggregate TSPEC
            if(tmask != SME_QOS_TSPEC_MASK_BIT_1_2_SET)
            {
              hstatus = sme_QosAggregateParams(&Tspec_Info, 
                                               &pACInfo->curr_QoSInfo[tmask - 1],
                                               &pACInfo->requested_QoSInfo[tmask - 1]);
            }
            else
            {
              /* Aggregate the new bidirectional stream with the existing upstreams and 
               * downstreams in tspec indices 0 and 1. */
              tmask = SME_QOS_TSPEC_MASK_BIT_1_SET;

              if((hstatus = sme_QosAggregateParams(&Tspec_Info, 
                                                   &pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0],
                                                   &pACInfo->requested_QoSInfo[tmask - 1]))
                          == eHAL_STATUS_SUCCESS)
              {
                hstatus = sme_QosAggregateParams(&pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_1], 
                                                 &pACInfo->requested_QoSInfo[tmask - 1],
                                                 NULL);
              }
            }

            if(!HAL_STATUS_SUCCESS(hstatus))
            {
               //err msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: failed to aggregate params",
                         __func__, __LINE__);
               // unable to service the request
               // nothing is pending so vote powersave back on
               pSession->readyForPowerSave = VOS_TRUE;
               return SME_QOS_STATUS_SETUP_FAILURE_RSP;
            }
         }
         else
         {
            if (!(new_tmask > 0 && new_tmask <= SME_QOS_TSPEC_INDEX_MAX))
            {
                 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: %d: ArrayIndexOutOfBoundsException",
                         __func__, __LINE__);

                 return SME_QOS_STATUS_SETUP_FAILURE_RSP;
            }
            tmask = new_tmask;
            pACInfo->requested_QoSInfo[tmask-1] = Tspec_Info;
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: no flows running for ac = %d while in state = %d",
                   __func__, __LINE__,
                   ac, pACInfo->curr_state );
         //ASSERT
         VOS_ASSERT(0);
         // unable to service the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return status;
      }
      //although aggregating, make sure to request on the correct UP,TID,PSB
      //and direction
      pACInfo->requested_QoSInfo[tmask - 1].ts_info.up = Tspec_Info.ts_info.up;
      pACInfo->requested_QoSInfo[tmask - 1].ts_info.tid =
                                            Tspec_Info.ts_info.tid;
      pACInfo->requested_QoSInfo[tmask - 1].ts_info.direction =
                                            Tspec_Info.ts_info.direction;
      pACInfo->requested_QoSInfo[tmask - 1].ts_info.psb =
                                            Tspec_Info.ts_info.psb;
      status = sme_QosSetup(pMac, sessionId,
                            &pACInfo->requested_QoSInfo[tmask - 1], ac);
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: On session %d with AC %d in state SME_QOS_QOS_ON "
                "sme_QosSetup returned with status %d",
                __func__, __LINE__,
                sessionId, ac, status);
      if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP != status)
      {
         // we aren't waiting for a response from the AP
         // so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
      }
      if((SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status)||
         (SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
         (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status))
      {
         // we received an expected "good" status
         //create an entry in the flow list
         pentry = (sme_QosFlowInfoEntry *) vos_mem_malloc(sizeof(*pentry));
         if (!pentry)
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: couldn't allocate memory for the new "
                      "entry in the Flow List",
                      __func__, __LINE__);
            return SME_QOS_STATUS_SETUP_FAILURE_RSP;
         }
         pentry->ac_type = ac;
         pentry->HDDcontext = HDDcontext;
         pentry->QoSCallback = QoSCallback;
         pentry->hoRenewal = hoRenewal;
         pentry->QosFlowID = QosFlowID;
         pentry->sessionId = sessionId;
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Creating flow %d",
                   __func__, __LINE__,
                   QosFlowID);
         if((SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status)||
            (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status))
         {
            new_state = pACInfo->curr_state;
            pentry->reason = SME_QOS_REASON_REQ_SUCCESS;
            pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0] = 
               pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0];
            if(buffered_cmd && !pentry->hoRenewal)
            {
               QoSCallback(pMac, HDDcontext, 
                           &pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0],
                           status,
                           pentry->QosFlowID);
            }
            if(SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status)
            {
               // if we are not in handoff, then notify all flows on
               // this AC that the aggregate TSPEC may have changed
               if(!pentry->hoRenewal)
               {
                  vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
                  search_key.key.ac_type = ac;
                  search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
                  search_key.sessionId = sessionId;
                  hstatus = sme_QosFindAllInFlowList(pMac, search_key,
                                                     sme_QosSetupFnp);
                  if(!HAL_STATUS_SUCCESS(hstatus))
                  {
                     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                               "%s: %d: couldn't notify other "
                               "entries on this AC =%d",
                               __func__, __LINE__, ac);
                  }
               }
            }
            pentry->hoRenewal = VOS_FALSE;
         }
         else
         {
            // SME_QOS_STATUS_SETUP_REQ_PENDING_RSP
            new_state = SME_QOS_REQUESTED;
            pentry->reason = SME_QOS_REASON_SETUP;
            //Need this info when addts comes back from PE to know on
            //which index of the AC the request was from
            pACInfo->tspec_pending = tmask;
         }
         if(tmask)
            pACInfo->num_flows[tmask - 1]++;
         else
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: ArrayIndexOutOfBoundsException",
                       __func__, __LINE__);
         //indicate on which index the flow entry belongs to & add it to the 
         //Flow List at the end
         pentry->tspec_mask = tmask;
         pentry->QoSInfo = Tspec_Info;
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: On session %d creating entry at %p with flowID %d",
                   __func__, __LINE__,
                   sessionId, pentry, QosFlowID);
         csrLLInsertTail(&sme_QosCb.flow_list, &pentry->link, VOS_TRUE);
      }
      else
      {
         // unexpected status returned by sme_QosSetup()
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On session %d unexpected status %d "
                   "returned by sme_QosSetup",
                   __func__, __LINE__,
                   sessionId, status);
         new_state = pACInfo->curr_state;
      }
      break;
   case SME_QOS_CLOSED:
   case SME_QOS_INIT:
   default:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: setup requested in unexpected state = %d",
                __func__, __LINE__,
                pACInfo->curr_state);
      // unable to service the request
      // nothing is pending so vote powersave back on
      pSession->readyForPowerSave = VOS_TRUE;
      VOS_ASSERT(0);
      new_state = pACInfo->curr_state;
   }
   /* if current state is same as previous no need for transistion,
      if we are doing reassoc & we are already in handoff state, no need to move
      to requested state. But make sure to set the previous state as requested
      state
   */
   if((new_state != pACInfo->curr_state)&&
      (!(pACInfo->reassoc_pending && 
         (SME_QOS_HANDOFF == pACInfo->curr_state))))
   {
      sme_QosStateTransition(sessionId, ac, new_state);
   }
   
   if(pACInfo->reassoc_pending && 
      (SME_QOS_HANDOFF == pACInfo->curr_state))
   {
      pACInfo->prev_state = SME_QOS_REQUESTED;
   }
   if((SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
      (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status)) 
   {
      (void)sme_QosProcessBufferedCmd(sessionId);
   }
   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosInternalModifyReq() - The SME QoS internal function to request 
  for modification of certain QoS params on a flow running on a particular AC. 
  If the request involves admission control on the requested AC, HDD needs to 
  provide the necessary Traffic Specification (TSPEC) parameters & SME might
  start the renegotiation process through ADDTS.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info as defined above, provided by HDD
  \param QosFlowID - Identification per flow running on each AC generated by 
                      SME. 
                     It is only meaningful if the QoS setup for the flow has 
                     been successful already
                  
  \return SME_QOS_STATUS_SETUP_SUCCESS_RSP - Modification is successful.
  
          Other status means request failed     
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosInternalModifyReq(tpAniSirGlobal pMac, 
                                           sme_QosWmmTspecInfo * pQoSInfo,
                                           v_U32_t QosFlowID,
                                           v_BOOL_t buffered_cmd)
{
   tListElem *pEntry= NULL;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *pNewEntry= NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosEdcaAcType ac;
   sme_QosStates new_state = SME_QOS_CLOSED;
   sme_QosStatusType status = SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
   sme_QosWmmTspecInfo Aggr_Tspec_Info;
   sme_QosSearchInfo search_key;
   sme_QosCmdInfo  cmd;
   v_U8_t sessionId;
   v_BOOL_t bufferCommand;
   eHalStatus hstatus;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked for flow %d",
             __func__, __LINE__,
             QosFlowID);

   vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
   //set the key type & the key to be searched in the Flow List
   search_key.key.QosFlowID = QosFlowID;
   search_key.index = SME_QOS_SEARCH_KEY_INDEX_1;
   search_key.sessionId = SME_QOS_SEARCH_SESSION_ID_ANY;
   //go through the link list to find out the details on the flow
   pEntry = sme_QosFindInFlowList(search_key);
   if(!pEntry)
   {
      //Err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: no match found for flowID = %d",
                __func__, __LINE__,
                QosFlowID);
      return SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP;
   }
   // find the AC
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;

   sessionId = flow_info->sessionId;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];

   //validate QoS params
   if(!sme_QosValidateRequestedParams(pMac, pQoSInfo, sessionId))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: invalid params",
                __func__, __LINE__);
      return SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP;
   }
   // For modify, make sure that direction, TID and UP are not being altered
   if((pQoSInfo->ts_info.direction != flow_info->QoSInfo.ts_info.direction) ||
      (pQoSInfo->ts_info.up != flow_info->QoSInfo.ts_info.up) ||
      (pQoSInfo->ts_info.tid != flow_info->QoSInfo.ts_info.tid))
   {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
               "%s: %d: Modification of direction/tid/up is not allowed",
               __func__, __LINE__);

     return SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP;
   }

   //should not be same as previous ioctl parameters
   if ((pQoSInfo->nominal_msdu_size == flow_info->QoSInfo.nominal_msdu_size) &&
       (pQoSInfo->maximum_msdu_size == flow_info->QoSInfo.maximum_msdu_size) &&
       (pQoSInfo->min_data_rate == flow_info->QoSInfo.min_data_rate) &&
       (pQoSInfo->mean_data_rate == flow_info->QoSInfo.mean_data_rate) &&
       (pQoSInfo->peak_data_rate == flow_info->QoSInfo.peak_data_rate) &&
       (pQoSInfo->min_service_interval ==
                  flow_info->QoSInfo.min_service_interval) &&
       (pQoSInfo->max_service_interval ==
                  flow_info->QoSInfo.max_service_interval) &&
       (pQoSInfo->inactivity_interval ==
                  flow_info->QoSInfo.inactivity_interval) &&
       (pQoSInfo->suspension_interval ==
                  flow_info->QoSInfo.suspension_interval) &&
       (pQoSInfo->surplus_bw_allowance ==
                  flow_info->QoSInfo.surplus_bw_allowance))
   {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               "%s: %d: the addts parameters are same as last request,"
               "dropping the current request",
               __func__, __LINE__);

     return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
   }

   // need to vote off powersave for the duration of this request
   pSession->readyForPowerSave = VOS_FALSE;
   // assume we won't have to (re)buffer the command
   bufferCommand = VOS_FALSE;
   //check to consider the following flowing scenario
   //Addts request is pending on one AC, while APSD requested on another which 
   //needs a reassoc. Will buffer a request if Addts is pending on any AC, 
   //which will safegaurd the above scenario, & also won't confuse PE with back 
   //to back Addts or Addts followed by Reassoc
   if(sme_QosIsRspPending(sessionId, ac))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: buffering the modify request for flow %d in state %d "
                "since another request is pending",
                __func__, __LINE__, 
                QosFlowID, pACInfo->curr_state );
      bufferCommand = VOS_TRUE;
   }
   else
   {
      // make sure we are in full power so that we can issue
      // an AddTS or reassoc if necessary
      hstatus = pmcRequestFullPower(pMac, sme_QosPmcFullPowerCallback,
                                    pSession, eSME_REASON_OTHER);
      if( eHAL_STATUS_PMC_PENDING == hstatus )
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                   "%s: %d: buffering the modify request for flow %d in state %d, "
                   "waiting for full power",
                   __func__, __LINE__, 
                   QosFlowID, pACInfo->curr_state );
         bufferCommand = VOS_TRUE;
      }
   }
   if (bufferCommand)
   {
      // we need to buffer the command
      cmd.command = SME_QOS_MODIFY_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.modifyCmdInfo.QosFlowID = QosFlowID;
      cmd.u.modifyCmdInfo.QoSInfo = *pQoSInfo;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the modify request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      }
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Buffered modify request for flow = %d",
                __func__, __LINE__,
                QosFlowID);
      return SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
   }
   //get into the stat m/c to see if the request can be granted
   switch(pACInfo->curr_state)
   {
   case SME_QOS_QOS_ON:
      //save the new params adding a new (duplicate) entry in the Flow List
      //Once we have decided on OTA exchange needed or not we can delete the
      //original one from the List
      pNewEntry = (sme_QosFlowInfoEntry *) vos_mem_malloc(sizeof(*pNewEntry));
      if (!pNewEntry)
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "%s: %d: couldn't allocate memory for the new "
                   "entry in the Flow List",
                   __func__, __LINE__);
         // unable to service the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      }
      pNewEntry->ac_type = ac;
      pNewEntry->sessionId = sessionId;
      pNewEntry->HDDcontext = flow_info->HDDcontext;
      pNewEntry->QoSCallback = flow_info->QoSCallback;
      pNewEntry->QosFlowID = flow_info->QosFlowID;
      pNewEntry->reason = SME_QOS_REASON_MODIFY_PENDING;
      //since it is a modify request, use the same index on which the flow
      //entry originally was running & add it to the Flow List at the end
      pNewEntry->tspec_mask = flow_info->tspec_mask;
      pNewEntry->QoSInfo = *pQoSInfo;
      //update the entry from Flow List which needed to be modified
      flow_info->reason = SME_QOS_REASON_MODIFY;
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: On session %d creating modified "
                "entry at %p with flowID %d",
                __func__, __LINE__,
                sessionId, pNewEntry, pNewEntry->QosFlowID);
      //add the new entry under construction to the Flow List
      csrLLInsertTail(&sme_QosCb.flow_list, &pNewEntry->link, VOS_TRUE);
      //update TSPEC with the new param set
      hstatus = sme_QosUpdateParams(sessionId,
                                    ac, pNewEntry->tspec_mask, 
                                    &Aggr_Tspec_Info);
      if(HAL_STATUS_SUCCESS(hstatus))
      {
         pACInfo->requested_QoSInfo[pNewEntry->tspec_mask -1] = Aggr_Tspec_Info;
         //if ACM, send out a new ADDTS
         status = sme_QosSetup(pMac, sessionId,
                               &pACInfo->requested_QoSInfo[pNewEntry->tspec_mask -1],
                               ac);
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: On session %d with AC %d in state SME_QOS_QOS_ON "
                   "sme_QosSetup returned with status %d",
                   __func__, __LINE__,
                   sessionId, ac, status);
         if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP != status)
         {
            // we aren't waiting for a response from the AP
            // so vote powersave back on
            pSession->readyForPowerSave = VOS_TRUE;
         }
         if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status) 
         {
            new_state = SME_QOS_REQUESTED;
            status = SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
            pACInfo->tspec_pending = pNewEntry->tspec_mask;
         }
         else if((SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
                 (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status))
         {
            new_state = SME_QOS_QOS_ON;

            vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
            //delete the original entry in FLOW list which got modified
            search_key.key.ac_type = ac;
            search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
            search_key.sessionId = sessionId;
            hstatus = sme_QosFindAllInFlowList(pMac, search_key,
                                               sme_QosModifyFnp);
            if(!HAL_STATUS_SUCCESS(hstatus))
            {
               status = SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
            }
            if(SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP != status)
            {
               pACInfo->curr_QoSInfo[pNewEntry->tspec_mask -1] = 
                  pACInfo->requested_QoSInfo[pNewEntry->tspec_mask -1];
               if(SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status)
               {
                  status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY;
                  vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
                  search_key.key.ac_type = ac;
                  search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
                  search_key.sessionId = sessionId;
                  hstatus = sme_QosFindAllInFlowList(pMac, search_key, 
                                                     sme_QosModificationNotifyFnp);
                  if(!HAL_STATUS_SUCCESS(hstatus))
                  {
                     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                               "%s: %d: couldn't notify other "
                               "entries on this AC =%d",
                               __func__, __LINE__, ac);
                  }
               }
               else if(SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status)
               {
                  status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP;
               }
            }
            if(buffered_cmd)
            {
               flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                      &pACInfo->curr_QoSInfo[pNewEntry->tspec_mask -1],
                                      status,
                                      flow_info->QosFlowID);
            }
            
         }
         else
         {
            // unexpected status returned by sme_QosSetup()
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d unexpected status %d "
                      "returned by sme_QosSetup",
                      __func__, __LINE__,
                      sessionId, status);
            new_state = SME_QOS_QOS_ON;
         }
      }
      else
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: sme_QosUpdateParams() failed",
                   __func__, __LINE__);
         // unable to service the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         new_state = SME_QOS_LINK_UP;
      }
      /* if we are doing reassoc & we are already in handoff state, no need
         to move to requested state. But make sure to set the previous state
         as requested state
      */
      if(!(pACInfo->reassoc_pending && 
           (SME_QOS_HANDOFF == pACInfo->curr_state)))
      {
         sme_QosStateTransition(sessionId, ac, new_state);
      }
      else
      {
         pACInfo->prev_state = SME_QOS_REQUESTED;
      }
      break;
   case SME_QOS_HANDOFF:
   case SME_QOS_REQUESTED:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: Buffering modify request for flow %d in state = %d",
                __func__, __LINE__,
                QosFlowID, pACInfo->curr_state );
      //buffer cmd
      cmd.command = SME_QOS_MODIFY_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.modifyCmdInfo.QosFlowID = QosFlowID;
      cmd.u.modifyCmdInfo.QoSInfo = *pQoSInfo;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the modify request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      }
      status = SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
      break;
   case SME_QOS_CLOSED:
   case SME_QOS_INIT:
   case SME_QOS_LINK_UP:
   default:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: modify requested in unexpected state = %d",
                __func__, __LINE__,
                pACInfo->curr_state);
      // unable to service the request
      // nothing is pending so vote powersave back on
      pSession->readyForPowerSave = VOS_TRUE;
      break;
   }
   if((SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
      (SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY == status)) 
   {
      (void)sme_QosProcessBufferedCmd(sessionId);
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosInternalReleaseReq() - The SME QoS internal function to request 
  for releasing a QoS flow running on a particular AC. 
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param QosFlowID - Identification per flow running on each AC generated by SME 
                     It is only meaningful if the QoS setup for the flow is 
                     successful
  
  \return eHAL_STATUS_SUCCESS - Release is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosInternalReleaseReq(tpAniSirGlobal pMac, 
                                            v_U32_t QosFlowID,
                                            v_BOOL_t buffered_cmd)
{
   tListElem *pEntry= NULL;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosFlowInfoEntry *pDeletedFlow = NULL;
   sme_QosEdcaAcType ac;
   sme_QosStates new_state = SME_QOS_CLOSED;
   sme_QosStatusType status = SME_QOS_STATUS_RELEASE_FAILURE_RSP;
   sme_QosWmmTspecInfo Aggr_Tspec_Info;
   sme_QosSearchInfo search_key;
   sme_QosCmdInfo  cmd;
   tCsrRoamModifyProfileFields modifyProfileFields;
   v_BOOL_t  deltsIssued = VOS_FALSE;
   v_U8_t sessionId;
   v_BOOL_t bufferCommand;
   eHalStatus hstatus;
   v_BOOL_t biDirectionalFlowsPresent = VOS_FALSE;
   v_BOOL_t uplinkFlowsPresent = VOS_FALSE;
   v_BOOL_t downlinkFlowsPresent = VOS_FALSE;
   tListElem *pResult= NULL;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked for flow %d",
             __func__, __LINE__,
             QosFlowID);

   vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
   //set the key type & the key to be searched in the Flow List
   search_key.key.QosFlowID = QosFlowID;
   search_key.index = SME_QOS_SEARCH_KEY_INDEX_1;
   search_key.sessionId = SME_QOS_SEARCH_SESSION_ID_ANY;
   //go through the link list to find out the details on the flow
   pEntry = sme_QosFindInFlowList(search_key);
   
   if(!pEntry)
   {
      //Err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: no match found for flowID = %d",
                __func__, __LINE__,
                QosFlowID);
      return SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP;
   }
   // find the AC
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   sessionId = flow_info->sessionId;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   // need to vote off powersave for the duration of this request
   pSession->readyForPowerSave = VOS_FALSE;
   // assume we won't have to (re)buffer the command
   bufferCommand = VOS_FALSE;
   //check to consider the following flowing scenario
   //Addts request is pending on one AC, while APSD requested on another which 
   //needs a reassoc. Will buffer a request if Addts is pending on any AC, 
   //which will safegaurd the above scenario, & also won't confuse PE with back 
   //to back Addts or Addts followed by Reassoc
   if(sme_QosIsRspPending(sessionId, ac))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: buffering the release request for flow %d in state %d "
                "since another request is pending",
                __func__, __LINE__, 
                QosFlowID, pACInfo->curr_state );
      bufferCommand = VOS_TRUE;
   }
   else
   {
      // make sure we are in full power so that we can issue
      // a DelTS or reassoc if necessary
      hstatus = pmcRequestFullPower(pMac, sme_QosPmcFullPowerCallback,
                                    pSession, eSME_REASON_OTHER);
      if( eHAL_STATUS_PMC_PENDING == hstatus )
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                   "%s: %d: buffering the release request for flow %d in state %d, "
                   "waiting for full power",
                   __func__, __LINE__, 
                   QosFlowID, pACInfo->curr_state );
         bufferCommand = VOS_TRUE;
      }
   }
   if (bufferCommand)
   {
      // we need to buffer the command
      cmd.command = SME_QOS_RELEASE_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.releaseCmdInfo.QosFlowID = QosFlowID;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the release request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_RELEASE_FAILURE_RSP;
      }
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Buffered release request for flow = %d",
                __func__, __LINE__,
                QosFlowID);
      return SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP;
   }
   //get into the stat m/c to see if the request can be granted
   switch(pACInfo->curr_state)
   {
   case SME_QOS_QOS_ON:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
                "%s: %d: tspec_mask_status = %d for AC = %d with "
                "entry tspec_mask = %d",
                __func__, __LINE__, 
                pACInfo->tspec_mask_status, ac, flow_info->tspec_mask);

      //check if multiple flows running on the ac
      if(pACInfo->num_flows[flow_info->tspec_mask - 1] > 1)
      {
         //don't want to include the flow in the new TSPEC on which release 
         //is requested
         flow_info->reason = SME_QOS_REASON_RELEASE;

         /* Check if the flow being released is for bi-diretional.
          * Following flows may present in the system. 
          * a) bi-directional flows
          * b) uplink flows
          * c) downlink flows. 
          * If the flow being released is for bidirectional, splitting of existing 
          * streams into two tspec indices is required in case ff (b), (c) are present 
          * and not (a).
          * In case if split occurs, all upstreams are aggregated into tspec index 0, 
          * downstreams are aggregaed into tspec index 1 and two tspec requests for 
          * (aggregated) upstream(s) followed by (aggregated) downstream(s) is sent
          * to AP. */
         if(flow_info->QoSInfo.ts_info.direction == SME_QOS_WMM_TS_DIR_BOTH)
         {
           vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
           //set the key type & the key to be searched in the Flow List
           search_key.key.ac_type = ac;
           search_key.index = SME_QOS_SEARCH_KEY_INDEX_4;
           search_key.sessionId = sessionId;
           search_key.direction = SME_QOS_WMM_TS_DIR_BOTH;
           pResult = sme_QosFindInFlowList(search_key);
           if(pResult)
             biDirectionalFlowsPresent = VOS_TRUE;

           if(!biDirectionalFlowsPresent)
           {
             // The only existing bidirectional flow is being released

             // Check if uplink flows exist
             search_key.direction = SME_QOS_WMM_TS_DIR_UPLINK;
             pResult = sme_QosFindInFlowList(search_key);
             if(pResult)
               uplinkFlowsPresent = VOS_TRUE;

             // Check if downlink flows exist
             search_key.direction = SME_QOS_WMM_TS_DIR_DOWNLINK;
             pResult = sme_QosFindInFlowList(search_key);
             if(pResult)
               downlinkFlowsPresent = VOS_TRUE;

             if(uplinkFlowsPresent && downlinkFlowsPresent)
             {
               // Need to split the uni-directional flows into SME_QOS_TSPEC_INDEX_0 and SME_QOS_TSPEC_INDEX_1

               vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
               // Mark all downstream flows as using tspec index 1
               search_key.key.ac_type = ac;
               search_key.index = SME_QOS_SEARCH_KEY_INDEX_4;
               search_key.sessionId = sessionId;
               search_key.direction = SME_QOS_WMM_TS_DIR_DOWNLINK;
               sme_QosUpdateTspecMask(sessionId, search_key, SME_QOS_TSPEC_MASK_BIT_2_SET);

               // Aggregate all downstream flows
               hstatus = sme_QosUpdateParams(sessionId,
                                             ac, SME_QOS_TSPEC_MASK_BIT_2_SET,
                                             &Aggr_Tspec_Info);

               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: %d: On session %d buffering the AddTS request "
                            "for AC %d in state %d as Addts is pending "
                         "on other Tspec index of this AC",
                         __func__, __LINE__,
                         sessionId, ac, pACInfo->curr_state);

               // Buffer the (aggregated) tspec request for downstream flows.
               // Please note that the (aggregated) tspec for upstream flows is sent 
               // out by the susequent logic.
               cmd.command = SME_QOS_RESEND_REQ;
               cmd.pMac = pMac;
               cmd.sessionId = sessionId;
               cmd.u.resendCmdInfo.ac = ac;
               cmd.u.resendCmdInfo.tspecMask = SME_QOS_TSPEC_MASK_BIT_2_SET;
               cmd.u.resendCmdInfo.QoSInfo = Aggr_Tspec_Info;
               pACInfo->requested_QoSInfo[SME_QOS_TSPEC_MASK_BIT_2_SET - 1] = Aggr_Tspec_Info;
               if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_FALSE)))
               {
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                            "%s: %d: On session %d unable to buffer the AddTS "
                            "request for AC %d TSPEC %d in state %d",
                            __func__, __LINE__,
                            sessionId, ac, SME_QOS_TSPEC_MASK_BIT_2_SET, pACInfo->curr_state);

                  // unable to buffer the request
                  // nothing is pending so vote powersave back on
                  pSession->readyForPowerSave = VOS_TRUE;

                  return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
               }
               pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_BIT_1_2_SET;

             }
           }
         }

         /* In case of splitting of existing streams,
          * tspec_mask will be pointing to tspec index 0 and 
          * aggregated tspec for upstream(s) is sent out here. */
         hstatus = sme_QosUpdateParams(sessionId,
                                       ac, flow_info->tspec_mask,
                                       &Aggr_Tspec_Info);
         if(HAL_STATUS_SUCCESS(hstatus))
         {
            pACInfo->requested_QoSInfo[flow_info->tspec_mask - 1] = Aggr_Tspec_Info;
            //if ACM, send out a new ADDTS
            status = sme_QosSetup(pMac, sessionId,
                                  &pACInfo->requested_QoSInfo[flow_info->tspec_mask - 1], ac);
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: On session %d with AC %d in state SME_QOS_QOS_ON "
                      "sme_QosSetup returned with status %d",
                      __func__, __LINE__,
                      sessionId, ac, status);
            if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP != status)
            {
               // we aren't waiting for a response from the AP
               // so vote powersave back on
               pSession->readyForPowerSave = VOS_TRUE;
            }
            if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status) 
            {
               new_state = SME_QOS_REQUESTED;
               status = SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP;
               pACInfo->tspec_pending = flow_info->tspec_mask;
            }
            else if((SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
                    (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status))
            {
               new_state = SME_QOS_QOS_ON;
               pACInfo->num_flows[flow_info->tspec_mask - 1]--;
               pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1] =
                  pACInfo->requested_QoSInfo[flow_info->tspec_mask - 1];
               //delete the entry from Flow List
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: Deleting entry at %p with flowID %d",
                         __func__, __LINE__,
                         flow_info, QosFlowID);
               csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
               pDeletedFlow = flow_info;
               if(SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status)
               {
                  vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
                  search_key.key.ac_type = ac;
                  search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
                  search_key.sessionId = sessionId;
                  hstatus = sme_QosFindAllInFlowList(pMac, search_key, 
                                                     sme_QosSetupFnp);
                  if(!HAL_STATUS_SUCCESS(hstatus))
                  {
                     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                               "%s: %d: couldn't notify other "
                               "entries on this AC =%d",
                               __func__, __LINE__, ac);
                  }
               }
               status = SME_QOS_STATUS_RELEASE_SUCCESS_RSP;
               if(buffered_cmd)
               {
                  flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                         &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                                         status,
                                         flow_info->QosFlowID);
               }
            }
            else
            {
               // unexpected status returned by sme_QosSetup()
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: On session %d unexpected status %d "
                         "returned by sme_QosSetup",
                         __func__, __LINE__,
                         sessionId, status);
               new_state = SME_QOS_LINK_UP;
               pACInfo->num_flows[flow_info->tspec_mask - 1]--;
               pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1] =
                  pACInfo->requested_QoSInfo[flow_info->tspec_mask - 1];
               //delete the entry from Flow List
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: On session %d deleting entry at "
                         "%p with flowID %d",
                         __func__, __LINE__,
                         sessionId, flow_info, QosFlowID);
               csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
               pDeletedFlow = flow_info;
               if(buffered_cmd)
               {
                  flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                         &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                                         status,
                                         flow_info->QosFlowID);
               }
            }
         }
         else
         {
            //err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosUpdateParams() failed",
                      __func__, __LINE__);
            // unable to service the request
            // nothing is pending so vote powersave back on
            pSession->readyForPowerSave = VOS_TRUE;
            new_state = SME_QOS_LINK_UP;
            if(buffered_cmd)
            {
               flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                      &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                                      status,
                                      flow_info->QosFlowID);
            }
         }
      }
      else
      {
         // this is the only flow aggregated in this TSPEC
         status = SME_QOS_STATUS_RELEASE_SUCCESS_RSP;
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
         if (ac == SME_QOS_EDCA_AC_VO)
         {
            // Indicate to neighbor roam logic of the new required VO
            // ac bandwidth requirement.
            csrNeighborRoamIndicateVoiceBW( pMac, pACInfo->curr_QoSInfo[0].peak_data_rate, FALSE );
         }
#endif
         //check if delts needs to be sent
         if(CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(pMac) ||
            sme_QosIsACM(pMac, pSession->assocInfo.pBssDesc, ac, NULL))
         {
            //check if other TSPEC for this AC is also in use
            if(SME_QOS_TSPEC_MASK_BIT_1_2_SET != pACInfo->tspec_mask_status)
            {
               // this is the only TSPEC active on this AC
               // so indicate that we no longer require APSD
               pSession->apsdMask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
               //Also update modifyProfileFields.uapsd_mask in CSR for consistency
               csrGetModifyProfileFields(pMac, flow_info->sessionId, &modifyProfileFields);
               modifyProfileFields.uapsd_mask = pSession->apsdMask; 
               csrSetModifyProfileFields(pMac, flow_info->sessionId, &modifyProfileFields);
               if(!pSession->apsdMask)
               {
                  // this session no longer needs UAPSD
                  // do any sessions still require UAPSD?
                  if (!sme_QosIsUapsdActive())
                  {
                     // No sessions require UAPSD so turn it off
                     // (really don't care when PMC stops it)
                     (void)pmcStopUapsd(pMac);
                  }
               }
            }
            if (SME_QOS_RELEASE_DEFAULT == pACInfo->relTrig)
            {
               //send delts
               hstatus = qosIssueCommand(pMac, sessionId, eSmeCommandDelTs,
                                         NULL, ac, flow_info->tspec_mask);
               if(!HAL_STATUS_SUCCESS(hstatus))
               {
                  //err msg
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                            "%s: %d: sme_QosDelTsReq() failed",
                            __func__, __LINE__);
                  status = SME_QOS_STATUS_RELEASE_FAILURE_RSP;
                  // we won't be waiting for a response from the AP
                  // so vote powersave back on
                  pSession->readyForPowerSave = VOS_TRUE;
               }
               else
               {
                  pACInfo->tspec_mask_status &= SME_QOS_TSPEC_MASK_BIT_1_2_SET &
                                                (~flow_info->tspec_mask);
                  deltsIssued = VOS_TRUE;
               }
            }
            else
            {
               pSession->readyForPowerSave = VOS_TRUE;
               pACInfo->tspec_mask_status &= SME_QOS_TSPEC_MASK_BIT_1_2_SET &
                                              (~flow_info->tspec_mask);
               deltsIssued = VOS_TRUE;
            }
         }
         else if(pSession->apsdMask & (1 << (SME_QOS_EDCA_AC_VO - ac)))
         {
            //reassoc logic
            csrGetModifyProfileFields(pMac, sessionId, &modifyProfileFields);
            modifyProfileFields.uapsd_mask |= pSession->apsdMask;
            modifyProfileFields.uapsd_mask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
            pSession->apsdMask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
            if(!pSession->apsdMask)
            {
               // this session no longer needs UAPSD
               // do any sessions still require UAPSD?
               if (!sme_QosIsUapsdActive())
               {
                  // No sessions require UAPSD so turn it off
                  // (really don't care when PMC stops it)
                  (void)pmcStopUapsd(pMac);
               }
            }
            hstatus = sme_QosRequestReassoc(pMac, sessionId,
                                            &modifyProfileFields, VOS_FALSE);
            if(!HAL_STATUS_SUCCESS(hstatus))
            {
               //err msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: Reassoc failed",
                         __func__, __LINE__);
               status = SME_QOS_STATUS_RELEASE_FAILURE_RSP;
               // we won't be waiting for a response from the AP
               // so vote powersave back on
               pSession->readyForPowerSave = VOS_TRUE;
            }
            else
            {
               pACInfo->reassoc_pending = VOS_FALSE;//no need to wait
               pACInfo->prev_state = SME_QOS_LINK_UP;
               pACInfo->tspec_pending = 0;
            }
         }
         else
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: nothing to do for AC = %d",
                      __func__, __LINE__, ac);
            // we won't be waiting for a response from the AP
            // so vote powersave back on
            pSession->readyForPowerSave = VOS_TRUE;
         }

         if (SME_QOS_RELEASE_BY_AP == pACInfo->relTrig)
         {
            flow_info->QoSCallback(pMac, flow_info->HDDcontext,
                          &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                          SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
                          flow_info->QosFlowID);

            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                      "%s: %d: Deleting entry at %p with flowID %d",
                      __func__, __LINE__,
                      flow_info, flow_info->QosFlowID);
         }
         else if(buffered_cmd)
         {
            flow_info->QoSCallback(pMac, flow_info->HDDcontext,
                                   NULL,
                                   status,
                                   flow_info->QosFlowID);
         }

         if(SME_QOS_STATUS_RELEASE_FAILURE_RSP == status)
         {
            break;
         }

         if(((SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~flow_info->tspec_mask) > 0) &&
            ((SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~flow_info->tspec_mask) <= 
                SME_QOS_TSPEC_INDEX_MAX))
         {
            if(pACInfo->num_flows[(SME_QOS_TSPEC_MASK_BIT_1_2_SET & 
                                    ~flow_info->tspec_mask) - 1] > 0)
            {
               new_state = SME_QOS_QOS_ON;
            }
            else
            {
               new_state = SME_QOS_LINK_UP;
            }         
         }
         else
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: Exceeded the array bounds of pACInfo->num_flows",
                      __func__, __LINE__);
            VOS_ASSERT (0);
            return SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP;
         }

         if(VOS_FALSE == deltsIssued)
         {
            vos_mem_zero(&pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1], 
                      sizeof(sme_QosWmmTspecInfo));
         }
         vos_mem_zero(&pACInfo->requested_QoSInfo[flow_info->tspec_mask - 1], 
                      sizeof(sme_QosWmmTspecInfo));
         pACInfo->num_flows[flow_info->tspec_mask - 1]--;
         //delete the entry from Flow List
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: On session %d deleting entry at %p with flowID %d",
                   __func__, __LINE__,
                   sessionId, flow_info, QosFlowID);
         csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
         pDeletedFlow = flow_info;
         pACInfo->relTrig = SME_QOS_RELEASE_DEFAULT;
      }
      /* if we are doing reassoc & we are already in handoff state, no need
         to move to requested state. But make sure to set the previous state
         as requested state
      */
      if(SME_QOS_HANDOFF != pACInfo->curr_state)
      {
         sme_QosStateTransition(sessionId, ac, new_state);
      }
      if(pACInfo->reassoc_pending)
      {
         pACInfo->prev_state = SME_QOS_REQUESTED;
      }
      break;
   case SME_QOS_HANDOFF:
   case SME_QOS_REQUESTED:
      //buffer cmd
      cmd.command = SME_QOS_RELEASE_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.releaseCmdInfo.QosFlowID = QosFlowID;
      hstatus = sme_QosBufferCmd(&cmd, buffered_cmd);
      if(!HAL_STATUS_SUCCESS(hstatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the release request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to service the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_RELEASE_FAILURE_RSP;
      }
      status = SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP;
      break;
   case SME_QOS_CLOSED:
   case SME_QOS_INIT:
   case SME_QOS_LINK_UP:
   default:
      //print error msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: release request in unexpected state = %d",
                __func__, __LINE__,
                pACInfo->curr_state );
      //ASSERT
      VOS_ASSERT(0);
      // unable to service the request
      // nothing is pending so vote powersave back on
      pSession->readyForPowerSave = VOS_TRUE;
      break;
   }
   // if we deleted a flow, reclaim the memory
   if (pDeletedFlow)
   {
      vos_mem_free(pDeletedFlow);
   }
   if((SME_QOS_STATUS_RELEASE_SUCCESS_RSP == status)) 
   {
      (void)sme_QosProcessBufferedCmd(sessionId);
   }
   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosSetup() - The internal qos setup function which has the 
  intelligence if the request is NOP, or for APSD and/or need to send out ADDTS.
  It also does the sanity check for QAP, AP supports APSD etc.
  \param pMac - Pointer to the global MAC parameter structure.   
  \param sessionId - Session upon which setup is being performed
  \param pTspec_Info - Pointer to sme_QosWmmTspecInfo which contains the WMM 
                       TSPEC related info as defined above
  \param ac - Enumeration of the various EDCA Access Categories.
  
  \return SME_QOS_STATUS_SETUP_SUCCESS_RSP if the setup is successful
  The logic used in the code might be confusing. Trying to cover all the cases 
  here.
  AP supports  App wants   ACM = 1  Already set APSD   Result
  |    0     |    0     |     0   |       0          |  NO ACM NO APSD
  |    0     |    0     |     0   |       1          |  NO ACM NO APSD/INVALID
  |    0     |    0     |     1   |       0          |  ADDTS
  |    0     |    0     |     1   |       1          |  ADDTS
  |    0     |    1     |     0   |       0          |  FAILURE
  |    0     |    1     |     0   |       1          |  INVALID
  |    0     |    1     |     1   |       0          |  ADDTS
  |    0     |    1     |     1   |       1          |  ADDTS
  |    1     |    0     |     0   |       0          |  NO ACM NO APSD
  |    1     |    0     |     0   |       1          |  NO ACM NO APSD
  |    1     |    0     |     1   |       0          |  ADDTS
  |    1     |    0     |     1   |       1          |  ADDTS
  |    1     |    1     |     0   |       0          |  REASSOC
  |    1     |    1     |     0   |       1          |  NOP: APSD SET ALREADY
  |    1     |    1     |     1   |       0          |  ADDTS
  |    1     |    1     |     1   |       1          |  ADDTS
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosStatusType sme_QosSetup(tpAniSirGlobal pMac,
                               v_U8_t sessionId,
                               sme_QosWmmTspecInfo *pTspec_Info, 
                               sme_QosEdcaAcType ac)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosStatusType status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   tDot11fBeaconIEs *pIes = NULL;
   tCsrRoamModifyProfileFields modifyProfileFields;
   eHalStatus hstatus;
   if( !CSR_IS_SESSION_VALID( pMac, sessionId ) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Session Id %d is invalid",
                __func__, __LINE__,
                sessionId);
      return status;
   }
   pSession = &sme_QosCb.sessionInfo[sessionId];
   if( !pSession->sessionActive )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Session %d is inactive",
                __func__, __LINE__,
                sessionId);
      return status;
   }
   if(!pSession->assocInfo.pBssDesc)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Session %d has an Invalid BSS Descriptor",
                __func__, __LINE__,
                sessionId);
      return status;
   }
   hstatus = csrGetParsedBssDescriptionIEs(pMac,
                                           pSession->assocInfo.pBssDesc,
                                           &pIes);
   if(!HAL_STATUS_SUCCESS(hstatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: On session %d unable to parse BSS IEs",
                __func__, __LINE__,
                sessionId);
      return status;
   }

   /* success so pIes was allocated */

   if( !CSR_IS_QOS_BSS(pIes) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d AP doesn't support QoS",
                __func__, __LINE__,
                sessionId);
      vos_mem_free(pIes);
      //notify HDD through the synchronous status msg
      return SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP;
   }

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
             "%s: %d: UAPSD/PSB set %d: ", __func__, __LINE__,
             pTspec_Info->ts_info.psb);

   pACInfo = &pSession->ac_info[ac];
   do
   {
      // is ACM enabled for this AC?
      if(CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(pMac) ||
         sme_QosIsACM(pMac, pSession->assocInfo.pBssDesc, ac, NULL))
      {
         // ACM is enabled for this AC so we must send an AddTS
         if(pTspec_Info->ts_info.psb && 
            (!pMac->pmc.uapsdEnabled ))
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: Request is looking for APSD but PMC doesn't "
                      "have support for APSD",
                      __func__, __LINE__);
            break;
         }

         if (pTspec_Info->ts_info.psb &&
             !(pIes->WMMParams.qosInfo & SME_QOS_AP_SUPPORTS_APSD) &&
             !(pIes->WMMInfoAp.uapsd))
         {
            // application is looking for APSD but AP doesn't support it
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: On session %d AP doesn't support APSD",
                      __func__, __LINE__,
                      sessionId);
            break;
         }

         if(SME_QOS_MAX_TID == pTspec_Info->ts_info.tid)
         {
            //App didn't set TID, generate one
            pTspec_Info->ts_info.tid =
               (v_U8_t)(SME_QOS_WMM_UP_NC - pTspec_Info->ts_info.up);
         }
         //addts logic
         hstatus = qosIssueCommand(pMac, sessionId, eSmeCommandAddTs,
                                   pTspec_Info, ac, 0);
         if(!HAL_STATUS_SUCCESS(hstatus))
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosAddTsReq() failed",
                      __func__, __LINE__);
            break;
         }
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: %d: On session %d AddTS on AC %d is pending",
                   __func__, __LINE__,
                   sessionId, ac);
         status = SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
         break;
      }
      // ACM is not enabled for this AC
      // Is the application looking for APSD?
      if(0 == pTspec_Info->ts_info.psb)
      {
         //no, we don't need APSD
         //but check the case, if the setup is called as a result of a release 
         // or modify which boils down to the fact that APSD was set on this AC
         // but no longer needed - so we need a reassoc for the above case to 
         // let the AP know
         if(pSession->apsdMask & (1 << (SME_QOS_EDCA_AC_VO - ac)))
         {
            // APSD was formerly enabled on this AC but is no longer required
            // so we must reassociate
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: On session %d reassoc needed "
                      "to disable APSD on AC %d",
                      __func__, __LINE__,
                      sessionId, ac);
            csrGetModifyProfileFields(pMac, sessionId, &modifyProfileFields);
            modifyProfileFields.uapsd_mask |= pSession->apsdMask;
            modifyProfileFields.uapsd_mask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
            hstatus = sme_QosRequestReassoc(pMac, sessionId,
                                            &modifyProfileFields, VOS_FALSE);
            if(!HAL_STATUS_SUCCESS(hstatus))
            {
               //err msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: Unable to request reassociation",
                         __func__, __LINE__);
               break;
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                         "%s: %d: On session %d reassociation to enable "
                         "APSD on AC %d is pending",
                         __func__, __LINE__,
                         sessionId, ac);
               status = SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
               pACInfo->reassoc_pending = VOS_TRUE;
            }
         }
         else
         {
            // we don't need APSD on this AC
            // and we don't currently have APSD on this AC
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: Request is not looking for APSD & Admission "
                      "Control isn't mandatory for the AC",
                      __func__, __LINE__);
            //return success right away
            status = SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP;
         }
         break;
      }
      else if(!(pIes->WMMParams.qosInfo & SME_QOS_AP_SUPPORTS_APSD) &&
              !(pIes->WMMInfoAp.uapsd))
      {
         // application is looking for APSD but AP doesn't support it
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On session %d AP doesn't support APSD",
                   __func__, __LINE__,
                   sessionId);
         break;
      }
      else if(pSession->apsdMask & (1 << (SME_QOS_EDCA_AC_VO - ac)))
      {
         // application is looking for APSD
         // and it is already enabled on this AC
         status = SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY;
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Request is looking for APSD and it is already "
                   "set for the AC",
                   __func__, __LINE__);
         break;
      }
      else
      {
         // application is looking for APSD
         // but it is not enabled on this AC
         // so we need to reassociate
         if(pMac->pmc.uapsdEnabled)
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: On session %d reassoc needed "
                      "to enable APSD on AC %d",
                      __func__, __LINE__,
                      sessionId, ac);
            //reassoc logic
            // update the UAPSD mask to include the new 
            // AC on which APSD is requested
            csrGetModifyProfileFields(pMac, sessionId, &modifyProfileFields);
            modifyProfileFields.uapsd_mask |= pSession->apsdMask;
            modifyProfileFields.uapsd_mask |= 1 << (SME_QOS_EDCA_AC_VO - ac);
            hstatus = sme_QosRequestReassoc(pMac, sessionId,
                                            &modifyProfileFields, VOS_FALSE);
            if(!HAL_STATUS_SUCCESS(hstatus))
            {
               //err msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: Unable to request reassociation",
                         __func__, __LINE__);
               break;
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                         "%s: %d: On session %d reassociation to enable "
                         "APSD on AC %d is pending",
                         __func__, __LINE__,
                         sessionId, ac);
               status = SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
               pACInfo->reassoc_pending = VOS_TRUE;
            }
         }
         else
         {
            //err msg: no support for APSD from PMC
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: no support for APSD or BMPS from PMC",
                      __func__, __LINE__);
         }
      }
   }while(0);

   vos_mem_free(pIes);
   return status;
}

#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
/* This is a dummy function now. But the purpose of me adding this was to 
 * delay the TSPEC processing till SET_KEY completes. This function can be 
 * used to do any SME_QOS processing after the SET_KEY. As of now, it is 
 * not required as we are ok with tspec getting programmed before set_key 
 * as the roam timings are measured without tspec in reassoc!
 */
eHalStatus sme_QosProcessSetKeySuccessInd(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
            "########### Set Key Complete #############");
    (void)sme_QosProcessBufferedCmd(sessionId);
    return eHAL_STATUS_SUCCESS;
}
#endif

#ifdef FEATURE_WLAN_ESE
/*--------------------------------------------------------------------------
  \brief sme_QosESESaveTspecResponse() - This function saves the TSPEC
         parameters that came along in the TSPEC IE in the reassoc response
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param sessionId - SME session ID 
  \param pTspec - Pointer to the TSPEC IE from the reassoc rsp
  \param ac - Access Category for which this TSPEC rsp is received
  \param tspecIndex - flow/direction
  
  \return eHAL_STATUS_SUCCESS - Release is successful.
  --------------------------------------------------------------------------*/
eHalStatus sme_QosESESaveTspecResponse(tpAniSirGlobal pMac, v_U8_t sessionId, tDot11fIEWMMTSPEC *pTspec, v_U8_t ac, v_U8_t tspecIndex)
{
    tpSirAddtsRsp pAddtsRsp = &sme_QosCb.sessionInfo[sessionId].ac_info[ac].addTsRsp[tspecIndex];

    ac = sme_QosUPtoACMap[pTspec->user_priority];

    vos_mem_zero(pAddtsRsp, sizeof(tSirAddtsRsp));

    pAddtsRsp->messageType = eWNI_SME_ADDTS_RSP;
    pAddtsRsp->length = sizeof(tSirAddtsRsp);
    pAddtsRsp->rc = eSIR_SUCCESS;
    pAddtsRsp->sessionId = sessionId;
    pAddtsRsp->rsp.dialogToken = 0;
    pAddtsRsp->rsp.status = eSIR_SUCCESS;
    pAddtsRsp->rsp.wmeTspecPresent = pTspec->present;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
            "%s: Copy Tspec to local data structure ac=%d, tspecIdx=%d", 
            __func__, ac, tspecIndex);

    if (pAddtsRsp->rsp.wmeTspecPresent)
    {
        //Copy TSPEC params received in assoc response to addts response
        ConvertWMMTSPEC(pMac, &pAddtsRsp->rsp.tspec, pTspec);
    }

    return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_QosESEProcessReassocTspecRsp() - This function processes the
         WMM TSPEC IE in the reassoc response. Reassoc triggered as part of 
         ESE roaming to another ESE capable AP. If the TSPEC was added before
         reassoc, as part of Call Admission Control, the reasso req from the
         STA would carry the TSPEC parameters which were already negotiated
         with the older AP.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param sessionId - SME session ID 
  \param pEven_info - Pointer to the smeJoinRsp structure
  
  \return eHAL_STATUS_SUCCESS - Release is successful.
  --------------------------------------------------------------------------*/
eHalStatus sme_QosESEProcessReassocTspecRsp(tpAniSirGlobal pMac, v_U8_t sessionId, void* pEvent_info)
{
    sme_QosSessionInfo *pSession;
    sme_QosACInfo *pACInfo;
    tDot11fIEWMMTSPEC *pTspecIE = NULL;
    tCsrRoamSession *pCsrSession = CSR_GET_SESSION( pMac, sessionId );
    tCsrRoamConnectedInfo *pCsrConnectedInfo = &pCsrSession->connectedInfo;
    eHalStatus status = eHAL_STATUS_FAILURE;
    v_U8_t ac, numTspec, cnt;
    v_U8_t tspec_flow_index, tspec_mask_status;
    v_U32_t tspecIeLen;

    pSession = &sme_QosCb.sessionInfo[sessionId];

    // Get the TSPEC IEs which came along with the reassoc response 
    // from the pbFrames pointer
    pTspecIE = (tDot11fIEWMMTSPEC *)(pCsrConnectedInfo->pbFrames + pCsrConnectedInfo->nBeaconLength +
        pCsrConnectedInfo->nAssocReqLength + pCsrConnectedInfo->nAssocRspLength + pCsrConnectedInfo->nRICRspLength);

    // Get the number of tspecs Ies in the frame, the min length
    // should be atleast equal to the one TSPEC IE 
    tspecIeLen = pCsrConnectedInfo->nTspecIeLength;
    if (tspecIeLen < sizeof(tDot11fIEWMMTSPEC)) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                FL("ESE Tspec IE len %d less than min %d"),
                tspecIeLen, sizeof(tDot11fIEWMMTSPEC));
        return eHAL_STATUS_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
             "TspecLen = %d, pbFrames = %p, pTspecIE = %p",
             tspecIeLen, pCsrConnectedInfo->pbFrames, pTspecIE);

    numTspec = (tspecIeLen)/sizeof(tDot11fIEWMMTSPEC);
    for(cnt=0; cnt<numTspec; cnt++) {
        ac = sme_QosUpToAc(pTspecIE->user_priority);
        pACInfo = &pSession->ac_info[ac];
        tspec_mask_status = pACInfo->tspec_mask_status;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
                FL("UP=%d, ac=%d, tspec_mask_status=%x"),
                pTspecIE->user_priority, ac,  tspec_mask_status );

            for (tspec_flow_index = 0; tspec_flow_index < SME_QOS_TSPEC_INDEX_MAX; tspec_flow_index++) {
                if (tspec_mask_status & (1 << tspec_flow_index)) {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
                FL("Found Tspec entry flow = %d AC = %d"),tspec_flow_index, ac);
                    sme_QosESESaveTspecResponse(pMac, sessionId, pTspecIE, ac, tspec_flow_index);
                } else {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
                FL("Not found Tspec entry flow = %d AC = %d"),tspec_flow_index, ac);
                }
            }
        // Increment the pointer to point it to the next TSPEC IE
        pTspecIE++;
    }

    /* Send the Aggregated QoS request to HAL */
    status = sme_QosFTAggrQosReq(pMac,sessionId);

    return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosCopyTspecInfo() - This function copies the existing TSPEC 
         parameters from the source structure to the destination structure.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param pTspec_Info - source structure
  \param pTspec - destination structure
  
  \return void 
  --------------------------------------------------------------------------*/
static void sme_QosCopyTspecInfo(tpAniSirGlobal pMac, sme_QosWmmTspecInfo *pTspec_Info, tSirMacTspecIE* pTspec)
{
    /* As per WMM_AC_testplan_v0.39 Minimum Service Interval, Maximum Service
     * Interval, Service Start Time, Suspension Interval and Delay Bound are
     * all intended for HCCA operation and therefore must be set to zero*/
    pTspec->delayBound        = pTspec_Info->delay_bound;
    pTspec->inactInterval     = pTspec_Info->inactivity_interval;
    pTspec->length            = SME_QOS_TSPEC_IE_LENGTH;
    pTspec->maxBurstSz        = pTspec_Info->max_burst_size;
    pTspec->maxMsduSz         = pTspec_Info->maximum_msdu_size;
    pTspec->maxSvcInterval    = pTspec_Info->max_service_interval;
    pTspec->meanDataRate      = pTspec_Info->mean_data_rate;
    pTspec->mediumTime        = pTspec_Info->medium_time;
    pTspec->minDataRate       = pTspec_Info->min_data_rate;
    pTspec->minPhyRate        = pTspec_Info->min_phy_rate;
    pTspec->minSvcInterval    = pTspec_Info->min_service_interval;
    pTspec->nomMsduSz         = pTspec_Info->nominal_msdu_size;
    pTspec->peakDataRate      = pTspec_Info->peak_data_rate;
    pTspec->surplusBw         = pTspec_Info->surplus_bw_allowance;
    pTspec->suspendInterval   = pTspec_Info->suspension_interval;
    pTspec->svcStartTime      = pTspec_Info->svc_start_time;
    pTspec->tsinfo.traffic.direction = pTspec_Info->ts_info.direction;

    //Make sure UAPSD is allowed. BTC may want to disable UAPSD while keep QoS setup
    if (pTspec_Info->ts_info.psb && btcIsReadyForUapsd(pMac)) {
        pTspec->tsinfo.traffic.psb = pTspec_Info->ts_info.psb;
    } else {
        pTspec->tsinfo.traffic.psb = 0;
        pTspec_Info->ts_info.psb = 0;
    }
    pTspec->tsinfo.traffic.tsid           = pTspec_Info->ts_info.tid;
    pTspec->tsinfo.traffic.userPrio       = pTspec_Info->ts_info.up;
    pTspec->tsinfo.traffic.accessPolicy   = SME_QOS_ACCESS_POLICY_EDCA;
    pTspec->tsinfo.traffic.burstSizeDefn  = pTspec_Info->ts_info.burst_size_defn;
    pTspec->tsinfo.traffic.ackPolicy      = pTspec_Info->ts_info.ack_policy;
    pTspec->type                          = SME_QOS_TSPEC_IE_TYPE;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: %d: up = %d, tid = %d",
            __func__, __LINE__,
            pTspec_Info->ts_info.up,
            pTspec_Info->ts_info.tid);
}

/*--------------------------------------------------------------------------
  \brief sme_QosEseRetrieveTspecInfo() - This function is called by CSR
         when try to create reassoc request message to PE - csrSendSmeReassocReqMsg
         This functions get the existing tspec parameters to be included
         in the reassoc request.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param sessionId - SME session ID 
  \param pTspecInfo - Pointer to the structure to carry back the TSPEC parameters
  
  \return v_U8_t - number of existing negotiated TSPECs
  --------------------------------------------------------------------------*/
v_U8_t sme_QosESERetrieveTspecInfo(tpAniSirGlobal pMac, v_U8_t sessionId, tTspecInfo *pTspecInfo)
{
    sme_QosSessionInfo *pSession;
    sme_QosACInfo *pACInfo;
    v_U8_t tspec_mask_status = 0;
    v_U8_t tspec_pending_status = 0;
    v_U8_t ac, numTspecs = 0;
    tTspecInfo *pDstTspec = pTspecInfo;

    //TODO: Check if TSPEC has already been established, if not return

    pSession = &sme_QosCb.sessionInfo[sessionId];    

    for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) {
        volatile v_U8_t tspec_index = 0;

        pACInfo = &pSession->ac_info[ac];
        tspec_pending_status = pACInfo->tspec_pending;
        tspec_mask_status = pACInfo->tspec_mask_status;

        do {
            if (tspec_mask_status & SME_QOS_TSPEC_MASK_BIT_1_SET) {
                /* If a tspec status is pending, take requested_QoSInfo for RIC request, else use curr_QoSInfo
                   for the RIC request */
                if (tspec_pending_status & SME_QOS_TSPEC_MASK_BIT_1_SET) {
                    sme_QosCopyTspecInfo(pMac, &pACInfo->requested_QoSInfo[tspec_index], &pDstTspec->tspec);
                } else {
                    sme_QosCopyTspecInfo(pMac, &pACInfo->curr_QoSInfo[tspec_index], &pDstTspec->tspec);
                }
                pDstTspec->valid = TRUE;
                numTspecs++;
                pDstTspec++;
            }
            tspec_mask_status >>= 1;
            tspec_pending_status >>= 1;
            tspec_index++;
        } while (tspec_mask_status);
    }

    return numTspecs;
}

#endif

#ifdef WLAN_FEATURE_VOWIFI_11R

eHalStatus sme_QosCreateTspecRICIE(tpAniSirGlobal pMac, sme_QosWmmTspecInfo *pTspec_Info,
                                                       v_U8_t *pRICBuffer, v_U32_t *pRICLength, v_U8_t *pRICIdentifier)
{
    tDot11fIERICDataDesc    ricIE;
    tANI_U32                nStatus;

    VOS_ASSERT(NULL != pRICBuffer);
    VOS_ASSERT(NULL != pRICLength);
    VOS_ASSERT(NULL != pRICIdentifier);

    if (pRICBuffer == NULL || pRICIdentifier == NULL || pRICLength == NULL)
        return eHAL_STATUS_FAILURE;

    vos_mem_zero(&ricIE, sizeof(tDot11fIERICDataDesc));

    ricIE.present = 1;
    ricIE.RICData.present = 1;
    ricIE.RICData.resourceDescCount = 1;
    ricIE.RICData.statusCode = 0;
    ricIE.RICData.Identifier = sme_QosAssignDialogToken();
#ifndef USE_80211_WMMTSPEC_FOR_RIC
    ricIE.TSPEC.present = 1;
    ricIE.TSPEC.delay_bound = pTspec_Info->delay_bound;
    ricIE.TSPEC.inactivity_int = pTspec_Info->inactivity_interval;
    ricIE.TSPEC.burst_size = pTspec_Info->max_burst_size;
    ricIE.TSPEC.max_msdu_size = pTspec_Info->maximum_msdu_size;
    ricIE.TSPEC.max_service_int = pTspec_Info->max_service_interval;
    ricIE.TSPEC.mean_data_rate = pTspec_Info->mean_data_rate;
    ricIE.TSPEC.medium_time = 0;
    ricIE.TSPEC.min_data_rate = pTspec_Info->min_data_rate;
    ricIE.TSPEC.min_phy_rate = pTspec_Info->min_phy_rate;
    ricIE.TSPEC.min_service_int = pTspec_Info->min_service_interval;
    ricIE.TSPEC.size = pTspec_Info->nominal_msdu_size;
    ricIE.TSPEC.peak_data_rate = pTspec_Info->peak_data_rate;
    ricIE.TSPEC.surplus_bw_allowance = pTspec_Info->surplus_bw_allowance;
    ricIE.TSPEC.suspension_int = pTspec_Info->suspension_interval;
    ricIE.TSPEC.service_start_time = pTspec_Info->svc_start_time;
    ricIE.TSPEC.direction = pTspec_Info->ts_info.direction;
    //Make sure UAPSD is allowed. BTC may want to disable UAPSD while keep QoS setup
    if( pTspec_Info->ts_info.psb && btcIsReadyForUapsd(pMac) )
    {
       ricIE.TSPEC.psb = pTspec_Info->ts_info.psb;
    }
    else
    {
       ricIE.TSPEC.psb = 0;
    }
    ricIE.TSPEC.tsid = pTspec_Info->ts_info.tid;
    ricIE.TSPEC.user_priority = pTspec_Info->ts_info.up;
    ricIE.TSPEC.access_policy = SME_QOS_ACCESS_POLICY_EDCA;

    *pRICIdentifier = ricIE.RICData.Identifier;
    
    nStatus = dot11fPackIeRICDataDesc(pMac, &ricIE, pRICBuffer, sizeof(ricIE), pRICLength);
    if (DOT11F_FAILED(nStatus))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                FL("Packing of RIC Data of length %d failed with status %d"), 
                                        *pRICLength, nStatus);
    }
#else // WMM TSPEC
    /*As per WMM_AC_testplan_v0.39 Minimum Service Interval, Maximum Service 
      Interval, Service Start Time, Suspension Interval and Delay Bound are 
      all intended for HCCA operation and therefore must be set to zero*/
    ricIE.WMMTSPEC.present = 1;
    ricIE.WMMTSPEC.version = 1;
    ricIE.WMMTSPEC.delay_bound = pTspec_Info->delay_bound;
    ricIE.WMMTSPEC.inactivity_int = pTspec_Info->inactivity_interval;
    ricIE.WMMTSPEC.burst_size = pTspec_Info->max_burst_size;
    ricIE.WMMTSPEC.max_msdu_size = pTspec_Info->maximum_msdu_size;
    ricIE.WMMTSPEC.max_service_int = pTspec_Info->max_service_interval;
    ricIE.WMMTSPEC.mean_data_rate = pTspec_Info->mean_data_rate;
    ricIE.WMMTSPEC.medium_time = 0;
    ricIE.WMMTSPEC.min_data_rate = pTspec_Info->min_data_rate;
    ricIE.WMMTSPEC.min_phy_rate = pTspec_Info->min_phy_rate;
    ricIE.WMMTSPEC.min_service_int = pTspec_Info->min_service_interval;
    ricIE.WMMTSPEC.size = pTspec_Info->nominal_msdu_size;
    ricIE.WMMTSPEC.peak_data_rate = pTspec_Info->peak_data_rate;
    ricIE.WMMTSPEC.surplus_bw_allowance = pTspec_Info->surplus_bw_allowance;
    ricIE.WMMTSPEC.suspension_int = pTspec_Info->suspension_interval;
    ricIE.WMMTSPEC.service_start_time = pTspec_Info->svc_start_time;
    ricIE.WMMTSPEC.direction = pTspec_Info->ts_info.direction;
    //Make sure UAPSD is allowed. BTC may want to disable UAPSD while keep QoS setup
    if( pTspec_Info->ts_info.psb && btcIsReadyForUapsd(pMac) )
    {
       ricIE.WMMTSPEC.psb = pTspec_Info->ts_info.psb;
    }
    else
    {
       ricIE.WMMTSPEC.psb = 0;
    }
    ricIE.WMMTSPEC.tsid = pTspec_Info->ts_info.tid;
    ricIE.WMMTSPEC.user_priority = pTspec_Info->ts_info.up;
    ricIE.WMMTSPEC.access_policy = SME_QOS_ACCESS_POLICY_EDCA;

    
    nStatus = dot11fPackIeRICDataDesc(pMac, &ricIE, pRICBuffer, sizeof(ricIE), pRICLength);
    if (DOT11F_FAILED(nStatus))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                FL("Packing of RIC Data of length %d failed with status %d"), 
                                        *pRICLength, nStatus);
    }
#endif /* 80211_TSPEC */
    *pRICIdentifier = ricIE.RICData.Identifier;
    return nStatus;
}

eHalStatus sme_QosProcessFTReassocReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
    sme_QosSessionInfo *pSession;
    sme_QosACInfo *pACInfo;
    v_U8_t ac, qos_requested = FALSE;
    v_U8_t tspec_flow_index;
    sme_QosFlowInfoEntry *flow_info = NULL;
    tListElem *pEntry= NULL;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            FL("Invoked on session %d"), sessionId);

    pSession = &sme_QosCb.sessionInfo[sessionId];

    for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
    {
        pACInfo = &pSession->ac_info[ac];
        qos_requested = FALSE;

        for (tspec_flow_index = 0; tspec_flow_index < SME_QOS_TSPEC_INDEX_MAX; tspec_flow_index++)
        {
            /* Only in the below case, copy the AC's curr QoS Info to requested QoS info */
            if ((pACInfo->ricIdentifier[tspec_flow_index] && !pACInfo->tspec_pending) ||
                    (pACInfo->tspec_mask_status & (1<<tspec_flow_index)))
            {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                        FL("Copying the currentQos to requestedQos for AC=%d, flow=%d"),
                        ac, tspec_flow_index );

                pACInfo->requested_QoSInfo[tspec_flow_index] = pACInfo->curr_QoSInfo[tspec_flow_index];
                vos_mem_zero(&pACInfo->curr_QoSInfo[tspec_flow_index], sizeof(sme_QosWmmTspecInfo));
                qos_requested = TRUE;
            }
        }

        // Only if the tspec is required, transition the state to 
        // SME_QOS_REQUESTED for this AC
        if (qos_requested) 
        {
            switch(pACInfo->curr_state)
            {
                case SME_QOS_HANDOFF:
                    sme_QosStateTransition(sessionId, ac, SME_QOS_REQUESTED);
                    break;
                default:
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                            FL("FT Reassoc req event in unexpected state %d"), pACInfo->curr_state);
                    VOS_ASSERT(0);
            }
        }

    }

    /* At this point of time, we are disconnected from the old AP, so it is safe
     *             to reset all these session variables */
    pSession->apsdMask = 0;
    pSession->uapsdAlreadyRequested = 0;
    pSession->readyForPowerSave = 0;

    /* Now change reason and HO renewal of all the flow in this session only */
    pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
    if(!pEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
                "%s: %d: Flow List empty, nothing to update",
                __func__, __LINE__);
        return eHAL_STATUS_FAILURE;
    }

    do
    {
        flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
        if(sessionId == flow_info->sessionId)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                    "%s: %d: Changing FlowID %d reason to SETUP and HO renewal to FALSE",
                    __func__, __LINE__,
                    flow_info->QosFlowID);
            flow_info->reason = SME_QOS_REASON_SETUP;
            flow_info->hoRenewal = eANI_BOOLEAN_TRUE;
        }
        pEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
    } while( pEntry );

    return eHAL_STATUS_SUCCESS;
}


eHalStatus sme_QosFTAggrQosReq( tpAniSirGlobal pMac, v_U8_t sessionId )
{
    tSirAggrQosReq *pMsg = NULL;
    sme_QosSessionInfo *pSession;
    eHalStatus status = eHAL_STATUS_FAILURE;
    int i, j = 0;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: %d: invoked on session %d", __func__, __LINE__,
            sessionId);

    pSession = &sme_QosCb.sessionInfo[sessionId];

    pMsg = (tSirAggrQosReq *)vos_mem_malloc(sizeof(tSirAggrQosReq));

    if (!pMsg)
    {
        //err msg
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: couldn't allocate memory for the msg buffer",
                __func__, __LINE__);

        return eHAL_STATUS_FAILURE;
      }

    vos_mem_zero(pMsg, sizeof(tSirAggrQosReq));

    pMsg->messageType = pal_cpu_to_be16((v_U16_t)eWNI_SME_FT_AGGR_QOS_REQ);
    pMsg->length = sizeof(tSirAggrQosReq);
    pMsg->sessionId = sessionId;
    pMsg->timeout = 0;
    pMsg->rspReqd = VOS_TRUE;
    vos_mem_copy( &pMsg->bssId[ 0 ],
            &pSession->assocInfo.pBssDesc->bssId[ 0 ],
            sizeof(tCsrBssid) );

    for( i = 0; i < SME_QOS_EDCA_AC_MAX; i++ )
    {
        for( j = 0; j < SME_QOS_TSPEC_INDEX_MAX; j++ )
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                    FL("ac=%d, tspec_mask_staus=%x, tspec_index=%d"),
                    i, pSession->ac_info[i].tspec_mask_status, j);
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                    FL("direction = %d"), pSession->ac_info[i].addTsRsp[j].rsp.tspec.tsinfo.traffic.direction);
            // Check if any flow is active on this AC
            if ((pSession->ac_info[i].tspec_mask_status) & (1 << j))
            {
                tANI_U8 direction = pSession->ac_info[i].addTsRsp[j].rsp.tspec.tsinfo.traffic.direction;
                if ((direction == SME_QOS_WMM_TS_DIR_UPLINK) ||
                        (direction == SME_QOS_WMM_TS_DIR_BOTH))
                {
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
                            FL("Found tspec entry AC=%d, flow=%d, direction = %d"), i, j, direction);
                    pMsg->aggrInfo.aggrAddTsInfo[i].dialogToken =
                        sme_QosAssignDialogToken();
                    pMsg->aggrInfo.aggrAddTsInfo[i].lleTspecPresent =
                        pSession->ac_info[i].addTsRsp[j].rsp.lleTspecPresent;
                    pMsg->aggrInfo.aggrAddTsInfo[i].numTclas =
                        pSession->ac_info[i].addTsRsp[j].rsp.numTclas;
                    vos_mem_copy( pMsg->aggrInfo.aggrAddTsInfo[i].tclasInfo,
                            pSession->ac_info[i].addTsRsp[j].rsp.tclasInfo,
                            SIR_MAC_TCLASIE_MAXNUM );
                    pMsg->aggrInfo.aggrAddTsInfo[i].tclasProc =
                        pSession->ac_info[i].addTsRsp[j].rsp.tclasProc;
                    pMsg->aggrInfo.aggrAddTsInfo[i].tclasProcPresent =
                        pSession->ac_info[i].addTsRsp[j].rsp.tclasProcPresent;
                    pMsg->aggrInfo.aggrAddTsInfo[i].tspec =
                        pSession->ac_info[i].addTsRsp[j].rsp.tspec;
                    pMsg->aggrInfo.aggrAddTsInfo[i].wmeTspecPresent =
                        pSession->ac_info[i].addTsRsp[j].rsp.wmeTspecPresent;
                    pMsg->aggrInfo.aggrAddTsInfo[i].wsmTspecPresent =
                        pSession->ac_info[i].addTsRsp[j].rsp.wsmTspecPresent;
                    pMsg->aggrInfo.tspecIdx |= ( 1 << i );

                    // Mark the index for this AC as pending for response, which would be 
                    // used to validate the AddTS response from HAL->PE->SME
                    pSession->ac_info[i].tspec_pending = (1<<j);
                }
            }
        }
    }

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
            "Sending aggregated message to HAL 0x%x", pMsg->aggrInfo.tspecIdx);

    if(HAL_STATUS_SUCCESS(palSendMBMessage(pMac->hHdd, pMsg)))
    {
        status = eHAL_STATUS_SUCCESS;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: %d: sent down a AGGR QoS req to PE",
                __func__, __LINE__);
    }

    return status;
}

eHalStatus sme_QosProcessFTRICResponse(tpAniSirGlobal pMac, v_U8_t sessionId, tDot11fIERICDataDesc *pRicDataDesc, v_U8_t ac, v_U8_t tspecIndex)
{
    tANI_U8        i = 0;
    tpSirAddtsRsp   pAddtsRsp
        = &sme_QosCb.sessionInfo[sessionId].ac_info[ac].addTsRsp[tspecIndex];

    vos_mem_zero(pAddtsRsp, sizeof(tSirAddtsRsp));

    pAddtsRsp->messageType = eWNI_SME_ADDTS_RSP;
    pAddtsRsp->length = sizeof(tSirAddtsRsp);
    pAddtsRsp->rc = pRicDataDesc->RICData.statusCode;
    pAddtsRsp->sessionId = sessionId;
    pAddtsRsp->rsp.dialogToken = pRicDataDesc->RICData.Identifier;
    pAddtsRsp->rsp.status = pRicDataDesc->RICData.statusCode;
    pAddtsRsp->rsp.wmeTspecPresent = pRicDataDesc->TSPEC.present;
    if (pAddtsRsp->rsp.wmeTspecPresent)
    {
        //Copy TSPEC params received in RIC response to addts response
        ConvertTSPEC(pMac, &pAddtsRsp->rsp.tspec, &pRicDataDesc->TSPEC);
    }

    pAddtsRsp->rsp.numTclas = pRicDataDesc->num_TCLAS;
    if (pAddtsRsp->rsp.numTclas)
    {
        for (i = 0; i < pAddtsRsp->rsp.numTclas; i++)
        {
            //Copy TCLAS info per index to the addts response
            ConvertTCLAS(pMac, &pAddtsRsp->rsp.tclasInfo[i], &pRicDataDesc->TCLAS[i]);
        }
    }

    pAddtsRsp->rsp.tclasProcPresent = pRicDataDesc->TCLASSPROC.present;
    if (pAddtsRsp->rsp.tclasProcPresent)
        pAddtsRsp->rsp.tclasProc = pRicDataDesc->TCLASSPROC.processing;


    pAddtsRsp->rsp.schedulePresent = pRicDataDesc->Schedule.present;
    if (pAddtsRsp->rsp.schedulePresent)
   {
        //Copy Schedule IE params to addts response
        ConvertSchedule(pMac, &pAddtsRsp->rsp.schedule, &pRicDataDesc->Schedule);
    }

    //Need to check the below portion is a part of WMM TSPEC
    //Process Delay element
    if (pRicDataDesc->TSDelay.present)
        ConvertTSDelay(pMac, &pAddtsRsp->rsp.delay, &pRicDataDesc->TSDelay);

    //Need to call for WMMTSPEC
    if (pRicDataDesc->WMMTSPEC.present)
    {
        ConvertWMMTSPEC(pMac, &pAddtsRsp->rsp.tspec, &pRicDataDesc->WMMTSPEC);
    }
    //return sme_QosProcessAddTsRsp(pMac, &addtsRsp);
    return eHAL_STATUS_SUCCESS;
}
eHalStatus sme_QosProcessAggrQosRsp(tpAniSirGlobal pMac, void *pMsgBuf)
{
    tpSirAggrQosRsp pAggrRsp = (tpSirAggrQosRsp)pMsgBuf;
    tSirAddtsRsp   addtsRsp;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    int i, j = 0;
    tANI_U8 sessionId = pAggrRsp->sessionId;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
            FL("Received AGGR_QOS resp from LIM"));

    /* Copy over the updated response information for TSPEC of all the ACs */
    for( i = 0; i < SIR_QOS_NUM_AC_MAX; i++ )
    {
        tANI_U8 tspec_mask_status = sme_QosCb.sessionInfo[sessionId].ac_info[i].tspec_mask_status;
        for( j = 0; j < SME_QOS_TSPEC_INDEX_MAX; j++ ) 
        {
            tANI_U8 direction = sme_QosCb.sessionInfo[sessionId].ac_info[i].
                addTsRsp[j].rsp.tspec.tsinfo.traffic.direction;

            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                    FL("Addts rsp from LIM AC=%d, flow=%d dir=%d, tspecIdx=%x"),
                    i, j, direction, pAggrRsp->aggrInfo.tspecIdx);
            // Check if the direction is Uplink or bi-directional
            if( ((1<<i) & pAggrRsp->aggrInfo.tspecIdx) &&
                    ((tspec_mask_status) & (1<<j)) &&
                    ((direction == SME_QOS_WMM_TS_DIR_UPLINK) ||
                     (direction == SME_QOS_WMM_TS_DIR_BOTH)))
            {
                addtsRsp = sme_QosCb.sessionInfo[sessionId].ac_info[i].addTsRsp[j];
                addtsRsp.rc = pAggrRsp->aggrInfo.aggrRsp[i].status;
                addtsRsp.rsp.status = pAggrRsp->aggrInfo.aggrRsp[i].status;
                addtsRsp.rsp.tspec = pAggrRsp->aggrInfo.aggrRsp[i].tspec;

                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                        FL("Processing Addts rsp from LIM AC=%d, flow=%d"), i, j);
                /* post ADD TS response for each */
                if (sme_QosProcessAddTsRsp(pMac, &addtsRsp) != eHAL_STATUS_SUCCESS)
                {
                    status = eHAL_STATUS_FAILURE;
                }
            }
        }
    }
   return status;
}


eHalStatus sme_QosProcessFTReassocRspEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
    sme_QosSessionInfo *pSession;
    sme_QosACInfo *pACInfo;
    v_U8_t ac;
    v_U8_t tspec_flow_index;
    tDot11fIERICDataDesc *pRicDataDesc = NULL;
    eHalStatus            status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pCsrSession = CSR_GET_SESSION( pMac, sessionId );
    tCsrRoamConnectedInfo *pCsrConnectedInfo = NULL;
    tANI_U32    ricRspLen;

    if(NULL == pCsrSession)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                FL("The Session pointer is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    pCsrConnectedInfo = &pCsrSession->connectedInfo;

    ricRspLen = pCsrConnectedInfo->nRICRspLength;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: %d: invoked on session %d",
            __func__, __LINE__,
            sessionId);

    pSession = &sme_QosCb.sessionInfo[sessionId];

    pRicDataDesc = (tDot11fIERICDataDesc *)((pCsrConnectedInfo->pbFrames) +
        (pCsrConnectedInfo->nBeaconLength + pCsrConnectedInfo->nAssocReqLength +
        pCsrConnectedInfo->nAssocRspLength));

    for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
    {
        pACInfo = &pSession->ac_info[ac];

        for (tspec_flow_index = 0; tspec_flow_index < SME_QOS_TSPEC_INDEX_MAX; tspec_flow_index++)
        {
            /* Only in the below case, copy the AC's curr QoS Info to requested QoS info */
            if (pACInfo->ricIdentifier[tspec_flow_index])
            {

                if (!ricRspLen)
                {
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                            FL("RIC Response not received for AC %d on TSPEC Index %d, RIC Req Identifier = %d"),
                            ac, tspec_flow_index, pACInfo->ricIdentifier[tspec_flow_index]);
                    VOS_ASSERT(0);
                }
                else
                {
                    /* Now we got response for this identifier. Process it. */
                    if (pRicDataDesc->present)
                    {
                        if (pRicDataDesc->RICData.present)
                        {
                            if (pRicDataDesc->RICData.Identifier != pACInfo->ricIdentifier[tspec_flow_index])
                            {
                                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                                        FL("RIC response order not same as request sent. Request ID = %d, Response ID = %d"),
                                        pACInfo->ricIdentifier[tspec_flow_index], pRicDataDesc->RICData.Identifier);
                                VOS_ASSERT(0);
                            }
                            else
                            {
                                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                                        FL("Processing RIC Response for AC %d, TSPEC Flow index %d with RIC ID %d "),
                                        ac, tspec_flow_index, pRicDataDesc->RICData.Identifier);
                                status = sme_QosProcessFTRICResponse(pMac, sessionId, pRicDataDesc, ac, tspec_flow_index);
                                if (eHAL_STATUS_SUCCESS != status)
                                {
                                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                                            FL("Failed with status %d for AC %d in TSPEC Flow index = %d"),
                                            status, ac, tspec_flow_index);
                                }
                            }
                            pRicDataDesc++;
                            ricRspLen -= sizeof(tDot11fIERICDataDesc);
                        }
                    }
                }
            }

        }
    }

    if (ricRspLen)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                FL("RIC Response still follows despite traversing through all ACs. Remaining len = %d"), ricRspLen);
        VOS_ASSERT(0);
    }

    /* Send the Aggregated QoS request to HAL */
    status = sme_QosFTAggrQosReq(pMac,sessionId);

    return status;
}

#endif /* WLAN_FEATURE_VOWIFI_11R */



/*--------------------------------------------------------------------------
  \brief sme_QosAddTsReq() - To send down the ADDTS request with TSPEC params
  to PE 
  
 
  \param pMac - Pointer to the global MAC parameter structure.  
  \param sessionId - Session upon which the TSPEC should be added
  \param pTspec_Info - Pointer to sme_QosWmmTspecInfo which contains the WMM 
                       TSPEC related info as defined above
  \param ac - Enumeration of the various EDCA Access Categories.
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosAddTsReq(tpAniSirGlobal pMac,
                           v_U8_t sessionId,
                           sme_QosWmmTspecInfo * pTspec_Info,
                           sme_QosEdcaAcType ac)
{
   tSirAddtsReq *pMsg = NULL;
   sme_QosSessionInfo *pSession;
   eHalStatus status = eHAL_STATUS_FAILURE;
#ifdef FEATURE_WLAN_ESE
   tCsrRoamSession *pCsrSession = CSR_GET_SESSION( pMac, sessionId );
#endif
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   WLAN_VOS_DIAG_EVENT_DEF(qos, vos_event_wlan_qos_payload_type);
#endif
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for AC %d",
             __func__, __LINE__,
             sessionId, ac);
   if (sessionId >= CSR_ROAM_SESSION_MAX)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: sessionId(%d) is invalid",
                __func__, __LINE__, sessionId);
      return eHAL_STATUS_FAILURE;
   }

   pSession = &sme_QosCb.sessionInfo[sessionId];
   pMsg = (tSirAddtsReq *)vos_mem_malloc(sizeof(tSirAddtsReq));
   if (!pMsg)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: couldn't allocate memory for the msg buffer",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_zero(pMsg, sizeof(tSirAddtsReq));
   pMsg->messageType = pal_cpu_to_be16((v_U16_t)eWNI_SME_ADDTS_REQ);
   pMsg->length = sizeof(tSirAddtsReq);
   pMsg->sessionId = sessionId;
   pMsg->timeout = 0;
   pMsg->rspReqd = VOS_TRUE;
   pMsg->req.dialogToken = sme_QosAssignDialogToken();
   /*As per WMM_AC_testplan_v0.39 Minimum Service Interval, Maximum Service 
     Interval, Service Start Time, Suspension Interval and Delay Bound are 
     all intended for HCCA operation and therefore must be set to zero*/
   pMsg->req.tspec.delayBound = 0;
   pMsg->req.tspec.inactInterval = pTspec_Info->inactivity_interval;
   pMsg->req.tspec.length = SME_QOS_TSPEC_IE_LENGTH;
   pMsg->req.tspec.maxBurstSz = pTspec_Info->max_burst_size;
   pMsg->req.tspec.maxMsduSz = pTspec_Info->maximum_msdu_size;
   pMsg->req.tspec.maxSvcInterval = pTspec_Info->max_service_interval;
   pMsg->req.tspec.meanDataRate = pTspec_Info->mean_data_rate;
   pMsg->req.tspec.mediumTime = pTspec_Info->medium_time;
   pMsg->req.tspec.minDataRate = pTspec_Info->min_data_rate;
   pMsg->req.tspec.minPhyRate = pTspec_Info->min_phy_rate;
   pMsg->req.tspec.minSvcInterval = pTspec_Info->min_service_interval;
   pMsg->req.tspec.nomMsduSz = pTspec_Info->nominal_msdu_size;
   pMsg->req.tspec.peakDataRate = pTspec_Info->peak_data_rate;
   pMsg->req.tspec.surplusBw = pTspec_Info->surplus_bw_allowance;
   pMsg->req.tspec.suspendInterval = pTspec_Info->suspension_interval;
   pMsg->req.tspec.svcStartTime = 0;
   pMsg->req.tspec.tsinfo.traffic.direction = pTspec_Info->ts_info.direction;
   //Make sure UAPSD is allowed. BTC may want to disable UAPSD while keep QoS setup
   if( pTspec_Info->ts_info.psb 
         && btcIsReadyForUapsd(pMac) 
     )
   {
      pMsg->req.tspec.tsinfo.traffic.psb = pTspec_Info->ts_info.psb;
   }
   else
   {
      pMsg->req.tspec.tsinfo.traffic.psb = 0;
      pTspec_Info->ts_info.psb = 0;
   }
   pMsg->req.tspec.tsinfo.traffic.tsid = pTspec_Info->ts_info.tid;
   pMsg->req.tspec.tsinfo.traffic.userPrio = pTspec_Info->ts_info.up;
   pMsg->req.tspec.tsinfo.traffic.accessPolicy = SME_QOS_ACCESS_POLICY_EDCA;
   pMsg->req.tspec.tsinfo.traffic.burstSizeDefn = pTspec_Info->ts_info.burst_size_defn;
   pMsg->req.tspec.tsinfo.traffic.ackPolicy = pTspec_Info->ts_info.ack_policy;
   pMsg->req.tspec.type = SME_QOS_TSPEC_IE_TYPE;
   /*Fill the BSSID pMsg->req.bssId*/
   if (NULL == pSession->assocInfo.pBssDesc)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: BSS descriptor is NULL so we don't send request to PE",
                __func__, __LINE__);
      vos_mem_free(pMsg);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_copy( &pMsg->bssId[ 0 ], 
                 &pSession->assocInfo.pBssDesc->bssId[ 0 ], 
                 sizeof(tCsrBssid) );
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: up = %d, tid = %d",
             __func__, __LINE__, 
             pTspec_Info->ts_info.up,
             pTspec_Info->ts_info.tid);
#ifdef FEATURE_WLAN_ESE
   if(pCsrSession->connectedProfile.isESEAssoc)
   {
      pMsg->req.tsrsIE.tsid = pTspec_Info->ts_info.up;
      pMsg->req.tsrsPresent = 1;
   }
#endif
   if(HAL_STATUS_SUCCESS(palSendMBMessage(pMac->hHdd, pMsg)))
   {
      status = eHAL_STATUS_SUCCESS;
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: sent down a ADDTS req to PE",
                __func__, __LINE__);
      //event: EVENT_WLAN_QOS
#ifdef FEATURE_WLAN_DIAG_SUPPORT          
      qos.eventId = SME_QOS_DIAG_ADDTS_REQ;
      qos.reasonCode = SME_QOS_DIAG_USER_REQUESTED;
      WLAN_VOS_DIAG_EVENT_REPORT(&qos, EVENT_WLAN_QOS);
#endif //FEATURE_WLAN_DIAG_SUPPORT
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosDelTsReq() - To send down the DELTS request with TSPEC params
  to PE 
  
 
  \param pMac - Pointer to the global MAC parameter structure.  
  \param sessionId - Session from which the TSPEC should be deleted
  \param ac - Enumeration of the various EDCA Access Categories.
  \param tspec_mask - on which tspec per AC, the delts is requested
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosDelTsReq(tpAniSirGlobal pMac,
                           v_U8_t sessionId,
                           sme_QosEdcaAcType ac,
                           v_U8_t tspec_mask)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   tSirDeltsReq *pMsg;
   sme_QosWmmTspecInfo *pTspecInfo;
   eHalStatus status = eHAL_STATUS_FAILURE;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   WLAN_VOS_DIAG_EVENT_DEF(qos, vos_event_wlan_qos_payload_type);
#endif
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for AC %d",
             __func__, __LINE__,
             sessionId, ac);
   pMsg = (tSirDeltsReq *)vos_mem_malloc(sizeof(tSirDeltsReq));
   if (!pMsg)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: couldn't allocate memory for the msg buffer",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_zero(pMsg, sizeof(tSirDeltsReq));
   // get pointer to the TSPEC being deleted
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   pTspecInfo = &pACInfo->curr_QoSInfo[tspec_mask - 1];
   pMsg->messageType = pal_cpu_to_be16((v_U16_t)eWNI_SME_DELTS_REQ);
   pMsg->length = sizeof(tSirDeltsReq);
   pMsg->sessionId = sessionId;
   pMsg->rspReqd = VOS_TRUE;
   pMsg->req.tspec.delayBound = pTspecInfo->delay_bound;
   pMsg->req.tspec.inactInterval = pTspecInfo->inactivity_interval;
   pMsg->req.tspec.length = SME_QOS_TSPEC_IE_LENGTH;
   pMsg->req.tspec.maxBurstSz = pTspecInfo->max_burst_size;
   pMsg->req.tspec.maxMsduSz = pTspecInfo->maximum_msdu_size;
   pMsg->req.tspec.maxSvcInterval = pTspecInfo->max_service_interval;
   pMsg->req.tspec.meanDataRate = pTspecInfo->mean_data_rate;
   pMsg->req.tspec.mediumTime = pTspecInfo->medium_time;
   pMsg->req.tspec.minDataRate = pTspecInfo->min_data_rate;
   pMsg->req.tspec.minPhyRate = pTspecInfo->min_phy_rate;
   pMsg->req.tspec.minSvcInterval = pTspecInfo->min_service_interval;
   pMsg->req.tspec.nomMsduSz = pTspecInfo->nominal_msdu_size;
   pMsg->req.tspec.peakDataRate = pTspecInfo->peak_data_rate;
   pMsg->req.tspec.surplusBw = pTspecInfo->surplus_bw_allowance;
   pMsg->req.tspec.suspendInterval = pTspecInfo->suspension_interval;
   pMsg->req.tspec.svcStartTime = pTspecInfo->svc_start_time;
   pMsg->req.tspec.tsinfo.traffic.direction = pTspecInfo->ts_info.direction;
   pMsg->req.tspec.tsinfo.traffic.psb = pTspecInfo->ts_info.psb;
   pMsg->req.tspec.tsinfo.traffic.tsid = pTspecInfo->ts_info.tid;
   pMsg->req.tspec.tsinfo.traffic.userPrio = pTspecInfo->ts_info.up;
   pMsg->req.tspec.tsinfo.traffic.accessPolicy = SME_QOS_ACCESS_POLICY_EDCA;
   pMsg->req.tspec.tsinfo.traffic.burstSizeDefn = pTspecInfo->ts_info.burst_size_defn;
   pMsg->req.tspec.tsinfo.traffic.ackPolicy = pTspecInfo->ts_info.ack_policy;
   pMsg->req.tspec.type = SME_QOS_TSPEC_IE_TYPE;
   /*Fill the BSSID pMsg->req.bssId*/
   if (NULL == pSession->assocInfo.pBssDesc)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: BSS descriptor is NULL so we don't send request to PE",
                __func__, __LINE__);
      vos_mem_free(pMsg);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_copy( &pMsg->bssId[ 0 ], 
                 &pSession->assocInfo.pBssDesc->bssId[ 0 ], 
                 sizeof(tCsrBssid) );

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: up = %d, tid = %d",
             __func__, __LINE__, 
             pTspecInfo->ts_info.up,
             pTspecInfo->ts_info.tid);
   vos_mem_zero(&pACInfo->curr_QoSInfo[tspec_mask - 1], 
                sizeof(sme_QosWmmTspecInfo));
   if(HAL_STATUS_SUCCESS(palSendMBMessage(pMac->hHdd, pMsg)))
   {
      status = eHAL_STATUS_SUCCESS;
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: sme_QosDelTsReq:Test: sent down a DELTS req to PE",
                __func__, __LINE__);
      //event: EVENT_WLAN_QOS
#ifdef FEATURE_WLAN_DIAG_SUPPORT          
      qos.eventId = SME_QOS_DIAG_DELTS;
      qos.reasonCode = SME_QOS_DIAG_USER_REQUESTED;
      WLAN_VOS_DIAG_EVENT_REPORT(&qos, EVENT_WLAN_QOS);
#endif //FEATURE_WLAN_DIAG_SUPPORT
   }

   return status;
}


/*--------------------------------------------------------------------------
  \brief sme_QosProcessAddTsRsp() - Function to process the
  eWNI_SME_ADDTS_RSP came from PE 
  
  \param pMac - Pointer to the global MAC parameter structure.  
  \param pMsgBuf - Pointer to the msg buffer came from PE.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessAddTsRsp(tpAniSirGlobal pMac, void *pMsgBuf)
{
   tpSirAddtsRsp paddts_rsp = (tpSirAddtsRsp)pMsgBuf;
   sme_QosSessionInfo *pSession;
   v_U8_t sessionId = paddts_rsp->sessionId;
   eHalStatus status = eHAL_STATUS_FAILURE;
#ifdef WLAN_FEATURE_VOWIFI_11R
    sme_QosWmmUpType up = (sme_QosWmmUpType)paddts_rsp->rsp.tspec.tsinfo.traffic.userPrio;
    sme_QosACInfo *pACInfo;
    sme_QosEdcaAcType ac;
#endif
#ifdef FEATURE_WLAN_DIAG_SUPPORT
    WLAN_VOS_DIAG_EVENT_DEF(qos, vos_event_wlan_qos_payload_type);
#endif

    pSession = &sme_QosCb.sessionInfo[sessionId];

#ifdef WLAN_FEATURE_VOWIFI_11R
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
            "%s: %d: invoked on session %d for UP %d",
            __func__, __LINE__,
            sessionId, up);

    ac = sme_QosUpToAc(up);
    if(SME_QOS_EDCA_AC_MAX == ac)
    {
        //err msg
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: invalid AC %d from UP %d",
                __func__, __LINE__, ac, up);

        return eHAL_STATUS_FAILURE;
    }
    pACInfo = &pSession->ac_info[ac];   
    if (SME_QOS_HANDOFF == pACInfo->curr_state)
    {
        smsLog(pMac, LOG1, FL("ADDTS Response received for AC %d in HANDOFF State.. Dropping"), ac);
        pSession->readyForPowerSave = VOS_TRUE;
        return eHAL_STATUS_SUCCESS;
    }
#endif

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked on session %d with return code %d",
             __func__, __LINE__,
             sessionId, paddts_rsp->rc);
   // our outstanding request has been serviced
   // we can go into powersave
   pSession->readyForPowerSave = VOS_TRUE;
   if(paddts_rsp->rc)
   {
      //event: EVENT_WLAN_QOS
#ifdef FEATURE_WLAN_DIAG_SUPPORT          
      qos.eventId = SME_QOS_DIAG_ADDTS_RSP;
      qos.reasonCode = SME_QOS_DIAG_ADDTS_REFUSED;
      WLAN_VOS_DIAG_EVENT_REPORT(&qos, EVENT_WLAN_QOS);
#endif //FEATURE_WLAN_DIAG_SUPPORT
      status = sme_QosProcessAddTsFailureRsp(pMac, sessionId, &paddts_rsp->rsp);
   }
   else
   {
      status = sme_QosProcessAddTsSuccessRsp(pMac, sessionId, &paddts_rsp->rsp);
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessDelTsRsp() - Function to process the
  eWNI_SME_DELTS_RSP came from PE 
  
  \param pMac - Pointer to the global MAC parameter structure.  
  \param pMsgBuf - Pointer to the msg buffer came from PE.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessDelTsRsp(tpAniSirGlobal pMac, void *pMsgBuf)
{
   tpSirDeltsRsp pDeltsRsp = (tpSirDeltsRsp)pMsgBuf;
   sme_QosSessionInfo *pSession;
   v_U8_t sessionId = pDeltsRsp->sessionId;
   // msg
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked on session %d with return code %d",
             __func__, __LINE__,
             sessionId, pDeltsRsp->rc);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   // our outstanding request has been serviced
   // we can go into powersave
   pSession->readyForPowerSave = VOS_TRUE;
   (void)sme_QosProcessBufferedCmd(sessionId);
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessDelTsInd() - Function to process the
  eWNI_SME_DELTS_IND came from PE 
  
  Since it's a DELTS indication from AP, will notify all the flows running on 
  this AC about QoS release
  \param pMac - Pointer to the global MAC parameter structure.  
  \param pMsgBuf - Pointer to the msg buffer came from PE.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessDelTsInd(tpAniSirGlobal pMac, void *pMsgBuf)
{
   tpSirDeltsRsp pdeltsind = (tpSirDeltsRsp)pMsgBuf;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   v_U8_t sessionId = pdeltsind->sessionId;
   sme_QosEdcaAcType ac;
   sme_QosSearchInfo search_key;
   sme_QosWmmUpType up = (sme_QosWmmUpType)pdeltsind->rsp.tspec.tsinfo.traffic.userPrio;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   WLAN_VOS_DIAG_EVENT_DEF(qos, vos_event_wlan_qos_payload_type);
#endif
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked on session %d for UP %d",
             __func__, __LINE__,
             sessionId, up);
   ac = sme_QosUpToAc(up);
   if(SME_QOS_EDCA_AC_MAX == ac)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: invalid AC %d from UP %d",
                __func__, __LINE__,
                ac, up);
      return eHAL_STATUS_FAILURE;
   }
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];

   vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
   //set the key type & the key to be searched in the Flow List
   search_key.key.ac_type = ac;
   search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
   search_key.sessionId = sessionId;
   //find all Flows on the perticular AC & delete them, also send HDD indication
   // through the callback it registered per request
   if(!HAL_STATUS_SUCCESS(sme_QosFindAllInFlowList(pMac, search_key, sme_QosDelTsIndFnp)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: no match found for ac = %d",
                __func__, __LINE__, 
                search_key.key.ac_type);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }

//event: EVENT_WLAN_QOS
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   qos.eventId = SME_QOS_DIAG_DELTS;
   qos.reasonCode = SME_QOS_DIAG_DELTS_IND_FROM_AP;
   WLAN_VOS_DIAG_EVENT_REPORT(&qos, EVENT_WLAN_QOS);
#endif //FEATURE_WLAN_DIAG_SUPPORT

   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessAssocCompleteEv() - Function to process the
  SME_QOS_CSR_ASSOC_COMPLETE event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessAssocCompleteEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   eHalStatus status = eHAL_STATUS_FAILURE;
   sme_QosEdcaAcType ac = SME_QOS_EDCA_AC_BE;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   if(((SME_QOS_INIT == pSession->ac_info[SME_QOS_EDCA_AC_BE].curr_state)&&
       (SME_QOS_INIT == pSession->ac_info[SME_QOS_EDCA_AC_BK].curr_state)&&
       (SME_QOS_INIT == pSession->ac_info[SME_QOS_EDCA_AC_VI].curr_state)&&
       (SME_QOS_INIT == pSession->ac_info[SME_QOS_EDCA_AC_VO].curr_state)) ||
       (pSession->handoffRequested))
   {
      //get the association info
      if(!pEvent_info)
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: pEvent_info is NULL",
                   __func__, __LINE__);
         return status;
      }
      if(!((sme_QosAssocInfo *)pEvent_info)->pBssDesc)
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: pBssDesc is NULL",
                   __func__, __LINE__);
         return status;
      }
      if((pSession->assocInfo.pBssDesc) &&
         (csrIsBssidMatch(pMac, (tCsrBssid *)&pSession->assocInfo.pBssDesc->bssId, 
                          (tCsrBssid *) &(((sme_QosAssocInfo *)pEvent_info)->pBssDesc->bssId))))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: assoc with the same BSS, no update needed",
                   __func__, __LINE__);
      }
      else
      {
         status = sme_QosSaveAssocInfo(pSession, pEvent_info);
      }
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: wrong state: BE %d, BK %d, VI %d, VO %d",
                __func__, __LINE__,
                pSession->ac_info[SME_QOS_EDCA_AC_BE].curr_state,
                pSession->ac_info[SME_QOS_EDCA_AC_BK].curr_state,
                pSession->ac_info[SME_QOS_EDCA_AC_VI].curr_state,
                pSession->ac_info[SME_QOS_EDCA_AC_VO].curr_state);
      //ASSERT
      VOS_ASSERT(0);
      return status;
   }
   // the session is active
   pSession->sessionActive = VOS_TRUE;
   if(pSession->handoffRequested)
   {
      pSession->handoffRequested = VOS_FALSE;
      //renew all flows
      (void)sme_QosProcessBufferedCmd(sessionId);
      status = eHAL_STATUS_SUCCESS;
   }
   else
   {
      for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
      {
         pACInfo = &pSession->ac_info[ac];
         switch(pACInfo->curr_state)
         {
            case SME_QOS_INIT:
               sme_QosStateTransition(sessionId, ac, SME_QOS_LINK_UP);
               break;
            case SME_QOS_LINK_UP:
            case SME_QOS_REQUESTED:
            case SME_QOS_QOS_ON:
            case SME_QOS_HANDOFF:
            case SME_QOS_CLOSED:
            default:
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: On session %d AC %d is in wrong state %d",
                         __func__, __LINE__,
                         sessionId, ac, pACInfo->curr_state);
               //ASSERT
               VOS_ASSERT(0);
               break;
         }
      }
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessReassocReqEv() - Function to process the
  SME_QOS_CSR_REASSOC_REQ event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessReassocReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];

#ifdef WLAN_FEATURE_VOWIFI_11R
   if(pSession->ftHandoffInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: %d: no need for state transition, should "
               "already be in handoff state",
               __func__, __LINE__);
       VOS_ASSERT(pSession->ac_info[0].curr_state == SME_QOS_HANDOFF);
       VOS_ASSERT(pSession->ac_info[1].curr_state == SME_QOS_HANDOFF);
       VOS_ASSERT(pSession->ac_info[2].curr_state == SME_QOS_HANDOFF);
       VOS_ASSERT(pSession->ac_info[3].curr_state == SME_QOS_HANDOFF);
       sme_QosProcessFTReassocReqEv(pMac, sessionId, pEvent_info);
       return eHAL_STATUS_SUCCESS;
   }
#endif

   if(pSession->handoffRequested)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: no need for state transition, should "
                "already be in handoff state",
                __func__, __LINE__);
      VOS_ASSERT(pSession->ac_info[0].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[1].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[2].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[3].curr_state == SME_QOS_HANDOFF);

      //buffer the existing flows to be renewed after handoff is done
      sme_QosBufferExistingFlows(pMac, sessionId);
      //clean up the control block partially for handoff
      sme_QosCleanupCtrlBlkForHandoff(pMac, sessionId);
      return eHAL_STATUS_SUCCESS;
   }
//TBH: Assuming both handoff algo & 11r willn't be enabled at the same time
#ifdef WLAN_FEATURE_VOWIFI_11R
   if(pSession->ftHandoffInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: no need for state transition, should "
                "already be in handoff state",
                __func__, __LINE__);
      VOS_ASSERT(pSession->ac_info[0].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[1].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[2].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[3].curr_state == SME_QOS_HANDOFF);

      sme_QosProcessFTReassocReqEv(pMac, sessionId, pEvent_info);
      return eHAL_STATUS_SUCCESS;
   }
#endif

   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_LINK_UP:
         case SME_QOS_REQUESTED:
         case SME_QOS_QOS_ON:
            sme_QosStateTransition(sessionId, ac, SME_QOS_HANDOFF);
            break;
         case SME_QOS_HANDOFF:
            //This is normal because sme_QosRequestReassoc may already change the state
            break;
         case SME_QOS_CLOSED:
         case SME_QOS_INIT:
         default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessReassocSuccessEv() - Function to process the
  SME_QOS_CSR_REASSOC_COMPLETE event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessReassocSuccessEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{

   tCsrRoamSession *pCsrRoamSession = NULL;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac, ac_index;
   sme_QosSearchInfo search_key;
   sme_QosSearchInfo search_key1;
   eHalStatus status = eHAL_STATUS_FAILURE;
   tListElem *pEntry= NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);

   if (CSR_ROAM_SESSION_MAX <= sessionId) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: invoked on session %d",
                __func__, __LINE__,
                sessionId);
       return status;
   }

   pCsrRoamSession = CSR_GET_SESSION( pMac, sessionId );

   pSession = &sme_QosCb.sessionInfo[sessionId];
   // our pending reassociation has completed
   // we can allow powersave
   pSession->readyForPowerSave = VOS_TRUE;
   //get the association info
   if(!pEvent_info)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: pEvent_info is NULL",
                __func__, __LINE__);
      return status;
   }
   if(!((sme_QosAssocInfo *)pEvent_info)->pBssDesc)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: pBssDesc is NULL",
                __func__, __LINE__);
      return status;
   }
   status = sme_QosSaveAssocInfo(pSession, pEvent_info);
   if(status)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: sme_QosSaveAssocInfo() failed",
                __func__, __LINE__);
   }
//TBH: Assuming both handoff algo & 11r willn't be enabled at the same time   
   if(pSession->handoffRequested)
   {
      pSession->handoffRequested = VOS_FALSE;
      //renew all flows
      (void)sme_QosProcessBufferedCmd(sessionId);
      return eHAL_STATUS_SUCCESS;
   }
#ifdef WLAN_FEATURE_VOWIFI_11R
   if (pSession->ftHandoffInProgress)
   {
       if (csrRoamIs11rAssoc(pMac))
       {
           if (pCsrRoamSession && pCsrRoamSession->connectedInfo.nRICRspLength)
           {
               status = sme_QosProcessFTReassocRspEv(pMac, sessionId, pEvent_info);
           }
       }
#ifdef FEATURE_WLAN_ESE
       // If ESE association check for TSPEC IEs in the reassoc rsp frame
       if (csrRoamIsESEAssoc(pMac))
       {
           if (pCsrRoamSession && pCsrRoamSession->connectedInfo.nTspecIeLength)
           {
               status = sme_QosESEProcessReassocTspecRsp(pMac, sessionId, pEvent_info);
           }
       }
#endif
       pSession->ftHandoffInProgress = VOS_FALSE;
       pSession->handoffRequested = VOS_FALSE;
       return status;
   }
#endif

   pSession->sessionActive = VOS_TRUE;
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_HANDOFF:
            // return to our previous state
            sme_QosStateTransition(sessionId, ac, pACInfo->prev_state);
            //for which ac APSD (hence the reassoc) is requested
            if(pACInfo->reassoc_pending)
            {
               //update the apsd mask in CB - make sure to take care of the case
               //where we are resetting the bit in apsd_mask
               if(pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0].ts_info.psb)
               {
                  pSession->apsdMask |= 1 << (SME_QOS_EDCA_AC_VO - ac);
               }
               else
               {
                  pSession->apsdMask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
               }
               pACInfo->reassoc_pending = VOS_FALSE;
               //during setup it gets set as addts & reassoc both gets a pending flag
               //pACInfo->tspec_pending = 0;
               sme_QosStateTransition(sessionId, ac, SME_QOS_QOS_ON);
               // notify HDD with new Service Interval
               pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0] = 
                  pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0];
               vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
               //set the key type & the key to be searched in the Flow List
               search_key.key.ac_type = ac;
               search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
               search_key.sessionId = sessionId;
               //notify PMC that reassoc is done for APSD on certain AC??

               vos_mem_zero(&search_key1, sizeof(sme_QosSearchInfo));
               //set the hoRenewal field in control block if needed
               search_key1.index = SME_QOS_SEARCH_KEY_INDEX_3;
               search_key1.key.reason = SME_QOS_REASON_SETUP;
               search_key1.sessionId = sessionId;
               for(ac_index = SME_QOS_EDCA_AC_BE; ac_index < SME_QOS_EDCA_AC_MAX; ac_index++)
               {
                  pEntry = sme_QosFindInFlowList(search_key1);
                  if(pEntry)
                  {
                     flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
                     if(flow_info->ac_type == ac)
                     {
                        pACInfo->hoRenewal = flow_info->hoRenewal;
                        break;
                     }
                  }
               }
               //notify HDD the success for the requested flow 
               //notify all the other flows running on the AC that QoS got modified
               if(!HAL_STATUS_SUCCESS(sme_QosFindAllInFlowList(pMac, search_key, sme_QosReassocSuccessEvFnp)))
               {
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                            "%s: %d: no match found for ac = %d",
                            __func__, __LINE__,
                            search_key.key.ac_type);
                  //ASSERT
                  VOS_ASSERT(0);
                  return eHAL_STATUS_FAILURE;
               }
               pACInfo->hoRenewal = VOS_FALSE;
               vos_mem_zero(&pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0], 
                            sizeof(sme_QosWmmTspecInfo));
            }
            status = eHAL_STATUS_SUCCESS;
            break;
         case SME_QOS_INIT:
         case SME_QOS_CLOSED:
            //NOP
            status = eHAL_STATUS_SUCCESS;
            break;
         case SME_QOS_LINK_UP:
         case SME_QOS_REQUESTED:
         case SME_QOS_QOS_ON:
         default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   (void)sme_QosProcessBufferedCmd(sessionId);
   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosProcessReassocFailureEv() - Function to process the
  SME_QOS_CSR_REASSOC_FAILURE event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessReassocFailureEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   // our pending reassociation has completed
   // we can allow powersave
   pSession->readyForPowerSave = VOS_TRUE;
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_HANDOFF:
            sme_QosStateTransition(sessionId, ac, SME_QOS_INIT);
            if(pACInfo->reassoc_pending)
            {
               pACInfo->reassoc_pending = VOS_FALSE;
            }
            vos_mem_zero(&pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_1], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_1], 
                         sizeof(sme_QosWmmTspecInfo));
            pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_CLEAR;
            pACInfo->tspec_pending = 0;
            pACInfo->num_flows[SME_QOS_TSPEC_INDEX_0] = 0;
            pACInfo->num_flows[SME_QOS_TSPEC_INDEX_1] = 0;
            break;
         case SME_QOS_INIT:
         case SME_QOS_CLOSED:
            //NOP
            break;
         case SME_QOS_LINK_UP:
         case SME_QOS_REQUESTED:
         case SME_QOS_QOS_ON:
         default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   //need to clean up flows
   sme_QosDeleteExistingFlows(pMac, sessionId);
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessHandoffAssocReqEv() - Function to process the
  SME_QOS_CSR_HANDOFF_ASSOC_REQ event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessHandoffAssocReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   v_U8_t ac;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_LINK_UP:
         case SME_QOS_REQUESTED:
         case SME_QOS_QOS_ON:
            sme_QosStateTransition(sessionId, ac, SME_QOS_HANDOFF);
            break;
         case SME_QOS_HANDOFF:
            //print error msg
#ifdef WLAN_FEATURE_VOWIFI_11R
            if(pSession->ftHandoffInProgress)
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                         "%s: %d: SME_QOS_CSR_HANDOFF_ASSOC_REQ received in "
                         "SME_QOS_HANDOFF state with FT in progress"
                         , __func__, __LINE__); 
               break; 
            }
#endif            

         case SME_QOS_CLOSED:
         case SME_QOS_INIT:
         default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   // If FT handoff is in progress, legacy handoff need not be enabled
   if (!pSession->ftHandoffInProgress) {
       pSession->handoffRequested = VOS_TRUE;
   }
   // this session no longer needs UAPSD
   pSession->apsdMask = 0;
   // do any sessions still require UAPSD?
   if (!sme_QosIsUapsdActive())
   {
      // No sessions require UAPSD so turn it off
      // (really don't care when PMC stops it)
      (void)pmcStopUapsd(pMac);
   }
   pSession->uapsdAlreadyRequested = VOS_FALSE;
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessHandoffSuccessEv() - Function to process the
  SME_QOS_CSR_HANDOFF_COMPLETE event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessHandoffSuccessEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   v_U8_t ac;
   eHalStatus status = eHAL_STATUS_FAILURE;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   //go back to original state before handoff
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_HANDOFF:
            sme_QosStateTransition(sessionId, ac, pACInfo->prev_state);
            //we will retry for the requested flow(s) with the new AP
            if(SME_QOS_REQUESTED == pACInfo->curr_state)
            {
               pACInfo->curr_state = SME_QOS_LINK_UP;
            }
            status = eHAL_STATUS_SUCCESS;
            break;
         // FT logic, has already moved it to QOS_REQUESTED state during the 
         // reassoc request event, which would include the Qos (TSPEC) params
         // in the reassoc req frame
         case SME_QOS_REQUESTED:
            break;
         case SME_QOS_INIT:
         case SME_QOS_CLOSED:
         case SME_QOS_LINK_UP:
         case SME_QOS_QOS_ON:
         default:
#ifdef WLAN_FEATURE_VOWIFI_11R
/* In case of 11r - RIC, we request QoS and Hand-off at the same time hence the
   state may be SME_QOS_REQUESTED */
            if( pSession->ftHandoffInProgress )
               break;
#endif
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessHandoffFailureEv() - Function to process the
  SME_QOS_CSR_HANDOFF_FAILURE event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessHandoffFailureEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   v_U8_t ac;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      switch(pACInfo->curr_state)
      {
         case SME_QOS_HANDOFF:
            sme_QosStateTransition(sessionId, ac, SME_QOS_INIT);
            //need to clean up flows: TODO
            vos_mem_zero(&pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_1], 
                         sizeof(sme_QosWmmTspecInfo));
            vos_mem_zero(&pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_1], 
                         sizeof(sme_QosWmmTspecInfo));
            pACInfo->tspec_mask_status = SME_QOS_TSPEC_MASK_CLEAR;
            pACInfo->tspec_pending = 0;
            pACInfo->num_flows[SME_QOS_TSPEC_INDEX_0] = 0;
            pACInfo->num_flows[SME_QOS_TSPEC_INDEX_1] = 0;
            break;
         case SME_QOS_INIT:
         case SME_QOS_CLOSED:
         case SME_QOS_LINK_UP:
         case SME_QOS_REQUESTED:
         case SME_QOS_QOS_ON:
         default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: On session %d AC %d is in wrong state %d",
                      __func__, __LINE__,
                      sessionId, ac, pACInfo->curr_state);
            //ASSERT
            VOS_ASSERT(0);
            break;
      }
   }
   //no longer in handoff
   pSession->handoffRequested = VOS_FALSE;
   //clean up the assoc info
   if(pSession->assocInfo.pBssDesc)
   {
      vos_mem_free(pSession->assocInfo.pBssDesc);
      pSession->assocInfo.pBssDesc = NULL;
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessDisconnectEv() - Function to process the
  SME_QOS_CSR_DISCONNECT_REQ or  SME_QOS_CSR_DISCONNECT_IND event indication 
  from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessDisconnectEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   if((pSession->handoffRequested)
#ifdef WLAN_FEATURE_VOWIFI_11R
/* In case of 11r - RIC, we request QoS and Hand-off at the same time hence the
   state may be SME_QOS_REQUESTED */
      && !pSession->ftHandoffInProgress
#endif
      )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: no need for state transition, should "
                "already be in handoff state",
                __func__, __LINE__);
      VOS_ASSERT(pSession->ac_info[0].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[1].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[2].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[3].curr_state == SME_QOS_HANDOFF);
      return eHAL_STATUS_SUCCESS;
   }
   sme_QosInitACs(pMac, sessionId);
   // this session doesn't require UAPSD
   pSession->apsdMask = 0;
   // do any sessions still require UAPSD?
   if (!sme_QosIsUapsdActive())
   {
      // No sessions require UAPSD so turn it off
      // (really don't care when PMC stops it)
      (void)pmcStopUapsd(pMac);
   }
   pSession->uapsdAlreadyRequested = VOS_FALSE;
   pSession->handoffRequested = VOS_FALSE;
   pSession->readyForPowerSave = VOS_TRUE;
   pSession->roamID = 0;
   //need to clean up buffered req
   sme_QosDeleteBufferedRequests(pMac, sessionId);
   //need to clean up flows
   sme_QosDeleteExistingFlows(pMac, sessionId);
   //clean up the assoc info
   if(pSession->assocInfo.pBssDesc)
   {
      vos_mem_free(pSession->assocInfo.pBssDesc);
      pSession->assocInfo.pBssDesc = NULL;
   }
   sme_QosCb.sessionInfo[sessionId].sessionActive = VOS_FALSE;
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessJoinReqEv() - Function to process the
  SME_QOS_CSR_JOIN_REQ event indication from CSR
  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessJoinReqEv(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
   sme_QosSessionInfo *pSession;
   sme_QosEdcaAcType ac;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   if(pSession->handoffRequested)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: no need for state transition, should "
                "already be in handoff state",
                __func__, __LINE__);
      VOS_ASSERT(pSession->ac_info[0].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[1].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[2].curr_state == SME_QOS_HANDOFF);
      VOS_ASSERT(pSession->ac_info[3].curr_state == SME_QOS_HANDOFF);
      //buffer the existing flows to be renewed after handoff is done
      sme_QosBufferExistingFlows(pMac, sessionId);
      //clean up the control block partially for handoff
      sme_QosCleanupCtrlBlkForHandoff(pMac, sessionId);
      return eHAL_STATUS_SUCCESS;
   }

   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      sme_QosStateTransition(sessionId, ac, SME_QOS_INIT);
   }
   //clean up the assoc info if already set
   if(pSession->assocInfo.pBssDesc)
   {
      vos_mem_free(pSession->assocInfo.pBssDesc);
      pSession->assocInfo.pBssDesc = NULL;
   }
   return eHAL_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_VOWIFI_11R
/*--------------------------------------------------------------------------
  \brief sme_QosProcessPreauthSuccessInd() - Function to process the
  SME_QOS_CSR_PREAUTH_SUCCESS_IND event indication from CSR

  \param pEvent_info - Pointer to relevant info from CSR.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessPreauthSuccessInd(tpAniSirGlobal pMac, v_U8_t sessionId, void * pEvent_info)
{
    sme_QosSessionInfo *pSession;
    sme_QosACInfo *pACInfo;
    v_U8_t ac;
    eHalStatus  status = eHAL_STATUS_SUCCESS;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: %d: invoked on session %d",
            __func__, __LINE__,
            sessionId);

    pSession = &sme_QosCb.sessionInfo[sessionId];

    for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
    {
        pACInfo = &pSession->ac_info[ac];

        switch(pACInfo->curr_state)
        {
            case SME_QOS_LINK_UP:
            case SME_QOS_REQUESTED:
            case SME_QOS_QOS_ON:
                sme_QosStateTransition(sessionId, ac, SME_QOS_HANDOFF);
                break;
            case SME_QOS_HANDOFF:
                //print error msg
            case SME_QOS_CLOSED:
            case SME_QOS_INIT:
            default:
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                        "%s: %d: On session %d AC %d is in wrong state %d",
                        __func__, __LINE__,
                        sessionId, ac, pACInfo->curr_state);
                //ASSERT
                VOS_ASSERT(0);
                break;
        }
    }

    pSession->ftHandoffInProgress = VOS_TRUE;

    // Check if its a 11R roaming before preparing the RIC IEs
    if (csrRoamIs11rAssoc(pMac)) 
    {
        v_U16_t ricOffset = 0;
        v_U32_t ricIELength = 0;
        v_U8_t  *ricIE;
        v_U8_t  tspec_mask_status = 0;
        v_U8_t  tspec_pending_status = 0;

        /* Any Block Ack info there, should have been already filled by PE and present in this buffer
           and the ric_ies_length should contain the length of the whole RIC IEs. Filling of TSPEC info
           should start from this length */
        ricIE = pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies;
        ricOffset = pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies_length;

        /* Now we have to process the currentTspeInfo inside this session and create the RIC IEs */
        for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
        {
            volatile v_U8_t   tspec_index = 0;
            ricIELength = 0;
            pACInfo = &pSession->ac_info[ac];
            tspec_pending_status = pACInfo->tspec_pending;
            tspec_mask_status = pACInfo->tspec_mask_status;
            vos_mem_zero(pACInfo->ricIdentifier, SME_QOS_TSPEC_INDEX_MAX);
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                    FL("AC %d ==> TSPEC status = %d, tspec pending = %d"), 
                    ac, tspec_mask_status, tspec_pending_status);

            do
            {
                if (tspec_mask_status & 0x1)
                {
                    /* If a tspec status is pending, take requested_QoSInfo for RIC request, else use curr_QoSInfo
                       for the RIC request */
                    if (tspec_pending_status & 0x1)
                    {
                        status = sme_QosCreateTspecRICIE(pMac, &pACInfo->requested_QoSInfo[tspec_index],
                                ricIE + ricOffset, &ricIELength, &pACInfo->ricIdentifier[tspec_index]);
                    }
                    else
                    {
                        status = sme_QosCreateTspecRICIE(pMac, &pACInfo->curr_QoSInfo[tspec_index],
                                ricIE + ricOffset, &ricIELength, &pACInfo->ricIdentifier[tspec_index]);
                    }
                }
                ricOffset += ricIELength;
                pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies_length += ricIELength;

                tspec_mask_status >>= 1;
                tspec_pending_status >>= 1;
                tspec_index++;
            } while (tspec_mask_status);
        }
    }
    return status;
}

#endif


/*--------------------------------------------------------------------------
  \brief sme_QosProcessAddTsFailureRsp() - Function to process the
  Addts request failure response came from PE 
  
  We will notify HDD only for the requested Flow, other Flows running on the AC 
  stay intact
  
  \param pMac - Pointer to the global MAC parameter structure.  
  \param pRsp - Pointer to the addts response structure came from PE.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessAddTsFailureRsp(tpAniSirGlobal pMac, 
                                         v_U8_t sessionId,
                                         tSirAddtsRspInfo * pRsp)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac;
   sme_QosSearchInfo search_key;
   v_U8_t tspec_pending;
   sme_QosWmmUpType up = (sme_QosWmmUpType)pRsp->tspec.tsinfo.traffic.userPrio;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for UP %d",
             __func__, __LINE__,
             sessionId, up);
   ac = sme_QosUpToAc(up);
   if(SME_QOS_EDCA_AC_MAX == ac)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: invalid AC %d from UP %d",
                __func__, __LINE__, ac, up);
      return eHAL_STATUS_FAILURE;
   }
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   // is there a TSPEC request pending on this AC?
   tspec_pending = pACInfo->tspec_pending;
   if(!tspec_pending)
   {
      //ASSERT
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d an AddTS is not pending on AC %d",
                __func__, __LINE__,
                sessionId, ac);
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }

   vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
   //set the key type & the key to be searched in the Flow List
   search_key.key.ac_type = ac;
   search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
   search_key.sessionId = sessionId;
   if(!HAL_STATUS_SUCCESS(sme_QosFindAllInFlowList(pMac, search_key, sme_QosAddTsFailureFnp)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d no match found for ac = %d",
                __func__, __LINE__,
                sessionId, search_key.key.ac_type);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_zero(&pACInfo->requested_QoSInfo[tspec_pending - 1], 
                sizeof(sme_QosWmmTspecInfo));

   if((!pACInfo->num_flows[0])&&
      (!pACInfo->num_flows[1]))
   {
      pACInfo->tspec_mask_status &= SME_QOS_TSPEC_MASK_BIT_1_2_SET & 
         (~pACInfo->tspec_pending);
      sme_QosStateTransition(sessionId, ac, SME_QOS_LINK_UP);
   }
   else
   {
      sme_QosStateTransition(sessionId, ac, SME_QOS_QOS_ON);
   }
   pACInfo->tspec_pending = 0;

   (void)sme_QosProcessBufferedCmd(sessionId);

   return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_QosUpdateTspecMask() - Utiltity function to update the tspec.
  Typical usage while aggregating unidirectional flows into a bi-directional
  flow on AC which is running multiple flows
  
  \param sessionId - Session upon which the TSPEC is being updated
  \param ac - Enumeration of the various EDCA Access Categories.
  \param old_tspec_mask - on which tspec per AC, the update is requested
  \param new_tspec_mask - tspec to be set for this AC
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosUpdateTspecMask(v_U8_t sessionId,
                                      sme_QosSearchInfo search_key,
                                      v_U8_t new_tspec_mask)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for AC %d TSPEC %d",
             __func__, __LINE__,
             sessionId, search_key.key.ac_type, new_tspec_mask);

   pSession = &sme_QosCb.sessionInfo[sessionId];
   
   if (search_key.key.ac_type < SME_QOS_EDCA_AC_MAX)
   {
   pACInfo = &pSession->ac_info[search_key.key.ac_type];
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Exceeded the array bounds of pSession->ac_info",
                __func__, __LINE__);
      VOS_ASSERT (0);
      return eHAL_STATUS_FAILURE;
   }

   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Flow List empty, nothing to update",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }

   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );

      if(search_key.sessionId == flow_info->sessionId)
      {
         if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_4)
         {
            if((search_key.key.ac_type == flow_info->ac_type) &&
               (search_key.direction == flow_info->QoSInfo.ts_info.direction))
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: Flow %d matches",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
               pACInfo->num_flows[flow_info->tspec_mask - 1]--;
               pACInfo->num_flows[new_tspec_mask - 1]++;
               flow_info->tspec_mask = new_tspec_mask;
            }
         }
         else if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_5)
         {
            if((search_key.key.ac_type == flow_info->ac_type) &&
               (search_key.tspec_mask == flow_info->tspec_mask))
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: Flow %d matches",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
               pACInfo->num_flows[flow_info->tspec_mask - 1]--;
               pACInfo->num_flows[new_tspec_mask - 1]++;
               flow_info->tspec_mask = new_tspec_mask;
            }
         }
      }

      pEntry = pNextEntry;
   }

   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessAddTsSuccessRsp() - Function to process the
  Addts request success response came from PE 
  
  We will notify HDD with addts success for the requested Flow, & for other 
  Flows running on the AC we will send an addts modify status 
  
  
  \param pMac - Pointer to the global MAC parameter structure.  
  \param pRsp - Pointer to the addts response structure came from PE.   
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessAddTsSuccessRsp(tpAniSirGlobal pMac, 
                                         v_U8_t sessionId,
                                         tSirAddtsRspInfo * pRsp)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac, ac_index;
   sme_QosSearchInfo search_key;
   sme_QosSearchInfo search_key1;
   v_U8_t tspec_pending;
   tListElem *pEntry= NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosWmmUpType up = (sme_QosWmmUpType)pRsp->tspec.tsinfo.traffic.userPrio;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   WLAN_VOS_DIAG_EVENT_DEF(qos, vos_event_wlan_qos_payload_type);
   vos_log_qos_tspec_pkt_type *log_ptr = NULL;
#endif //FEATURE_WLAN_DIAG_SUPPORT
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for UP %d",
             __func__, __LINE__,
             sessionId, up);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   ac = sme_QosUpToAc(up);
   if(SME_QOS_EDCA_AC_MAX == ac)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: invalid AC %d from UP %d",
                __func__, __LINE__, ac, up);
      return eHAL_STATUS_FAILURE;
   }
   pACInfo = &pSession->ac_info[ac];
   // is there a TSPEC request pending on this AC?
   tspec_pending = pACInfo->tspec_pending;
   if(!tspec_pending)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d an AddTS is not pending on AC %d",
                __func__, __LINE__,
                sessionId, ac);
      return eHAL_STATUS_FAILURE;
   }
   //App is looking for APSD or the App which was looking for APSD has been 
   //released, so STA re-negotiated with AP
   if(pACInfo->requested_QoSInfo[tspec_pending - 1].ts_info.psb)
   {
      //update the session's apsd mask
      pSession->apsdMask |= 1 << (SME_QOS_EDCA_AC_VO - ac);
   }
   else
   {
      if(((SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~tspec_pending) > 0) &&
         ((SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~tspec_pending) <= 
            SME_QOS_TSPEC_INDEX_MAX))
      {
      if(!pACInfo->requested_QoSInfo
         [(SME_QOS_TSPEC_MASK_BIT_1_2_SET & ~tspec_pending) - 1].ts_info.psb)
      {
         //update the session's apsd mask
         pSession->apsdMask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
      }
   }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Exceeded the array bounds of pACInfo->requested_QosInfo",
                   __func__, __LINE__);
         VOS_ASSERT (0);
         return eHAL_STATUS_FAILURE;
      }
   }

   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.burst_size_defn =
                              pRsp->tspec.tsinfo.traffic.burstSizeDefn;
   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.ack_policy =
                              pRsp->tspec.tsinfo.traffic.ackPolicy;
   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.up =
                              pRsp->tspec.tsinfo.traffic.userPrio;
   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.psb =
                                        pRsp->tspec.tsinfo.traffic.psb;
   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.direction =
                                  pRsp->tspec.tsinfo.traffic.direction;
   pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.tid =
                                       pRsp->tspec.tsinfo.traffic.tsid;
   pACInfo->curr_QoSInfo[tspec_pending - 1].nominal_msdu_size =
                                       pRsp->tspec.nomMsduSz;
   pACInfo->curr_QoSInfo[tspec_pending - 1].maximum_msdu_size =
                                                 pRsp->tspec.maxMsduSz;
   pACInfo->curr_QoSInfo[tspec_pending - 1].min_service_interval =
                                            pRsp->tspec.minSvcInterval;
   pACInfo->curr_QoSInfo[tspec_pending - 1].max_service_interval =
                                            pRsp->tspec.maxSvcInterval;
   pACInfo->curr_QoSInfo[tspec_pending - 1].inactivity_interval =
                                             pRsp->tspec.inactInterval;
   pACInfo->curr_QoSInfo[tspec_pending - 1].suspension_interval =
                                           pRsp->tspec.suspendInterval;
   pACInfo->curr_QoSInfo[tspec_pending - 1].svc_start_time =
                                              pRsp->tspec.svcStartTime;
   pACInfo->curr_QoSInfo[tspec_pending - 1].min_data_rate =
                                              pRsp->tspec.minDataRate;
   pACInfo->curr_QoSInfo[tspec_pending - 1].mean_data_rate =
                                             pRsp->tspec.meanDataRate;
   pACInfo->curr_QoSInfo[tspec_pending - 1].peak_data_rate =
                                             pRsp->tspec.peakDataRate;
   pACInfo->curr_QoSInfo[tspec_pending - 1].max_burst_size =
                                               pRsp->tspec.maxBurstSz;
   pACInfo->curr_QoSInfo[tspec_pending - 1].delay_bound =
                                               pRsp->tspec.delayBound;

   pACInfo->curr_QoSInfo[tspec_pending - 1].min_phy_rate =
                                               pRsp->tspec.minPhyRate;
   pACInfo->curr_QoSInfo[tspec_pending - 1].surplus_bw_allowance =
                                                pRsp->tspec.surplusBw;
   pACInfo->curr_QoSInfo[tspec_pending - 1].medium_time =
                                               pRsp->tspec.mediumTime;
   // Save the expected UAPSD settings by application
   pACInfo->curr_QoSInfo[tspec_pending - 1].expec_psb_byapp =
           pACInfo->requested_QoSInfo[tspec_pending - 1].expec_psb_byapp;

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
             "%s: %d: On session %d AddTspec Medium Time %d",
             __func__, __LINE__,
             sessionId, pRsp->tspec.mediumTime);

   /* Check if the current flow is for bi-directional. If so, update the number of flows
    * to reflect that all flows are aggregated into tspec index 0. */
   if((pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1].ts_info.direction == SME_QOS_WMM_TS_DIR_BOTH) &&
      (pACInfo->num_flows[SME_QOS_TSPEC_INDEX_1] > 0))
   {
     vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
     /* update tspec_mask for all the flows having SME_QOS_TSPEC_MASK_BIT_2_SET to SME_QOS_TSPEC_MASK_BIT_1_SET */
     search_key.key.ac_type = ac;
     search_key.index = SME_QOS_SEARCH_KEY_INDEX_5;
     search_key.sessionId = sessionId;
     search_key.tspec_mask = SME_QOS_TSPEC_MASK_BIT_2_SET;
     sme_QosUpdateTspecMask(sessionId, search_key, SME_QOS_TSPEC_MASK_BIT_1_SET);
   }

   vos_mem_zero(&search_key1, sizeof(sme_QosSearchInfo));
   //set the horenewal field in control block if needed
   search_key1.index = SME_QOS_SEARCH_KEY_INDEX_3;
   search_key1.key.reason = SME_QOS_REASON_SETUP;
   search_key1.sessionId = sessionId;
   for(ac_index = SME_QOS_EDCA_AC_BE; ac_index < SME_QOS_EDCA_AC_MAX; ac_index++)
   {
      pEntry = sme_QosFindInFlowList(search_key1);
      if(pEntry)
      {
         flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
         if(flow_info->ac_type == ac)
         {
            pACInfo->hoRenewal = flow_info->hoRenewal;
            break;
         }
      }
   }
   vos_mem_zero(&search_key, sizeof(sme_QosSearchInfo));
   //set the key type & the key to be searched in the Flow List
   search_key.key.ac_type = ac;
   search_key.index = SME_QOS_SEARCH_KEY_INDEX_2;
   search_key.sessionId = sessionId;
   //notify HDD the success for the requested flow 
   //notify all the other flows running on the AC that QoS got modified
   if(!HAL_STATUS_SUCCESS(sme_QosFindAllInFlowList(pMac, search_key, sme_QosAddTsSuccessFnp)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d no match found for ac %d",
                __func__, __LINE__,
                sessionId, search_key.key.ac_type);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   pACInfo->hoRenewal = VOS_FALSE;
   vos_mem_zero(&pACInfo->requested_QoSInfo[tspec_pending - 1], 
                sizeof(sme_QosWmmTspecInfo));
   //event: EVENT_WLAN_QOS
#ifdef FEATURE_WLAN_DIAG_SUPPORT          
   qos.eventId = SME_QOS_DIAG_ADDTS_RSP;
   qos.reasonCode = SME_QOS_DIAG_ADDTS_ADMISSION_ACCEPTED;
   WLAN_VOS_DIAG_EVENT_REPORT(&qos, EVENT_WLAN_QOS);
   WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_qos_tspec_pkt_type, LOG_WLAN_QOS_TSPEC_C);
   if(log_ptr)
   {
      log_ptr->delay_bound = pACInfo->curr_QoSInfo[tspec_pending - 1].delay_bound;
      log_ptr->inactivity_interval = pACInfo->curr_QoSInfo[tspec_pending - 1].inactivity_interval;
      log_ptr->max_burst_size = pACInfo->curr_QoSInfo[tspec_pending - 1].max_burst_size;
      log_ptr->max_service_interval = pACInfo->curr_QoSInfo[tspec_pending - 1].max_service_interval;
      log_ptr->maximum_msdu_size = pACInfo->curr_QoSInfo[tspec_pending - 1].maximum_msdu_size;
      log_ptr->mean_data_rate = pACInfo->curr_QoSInfo[tspec_pending - 1].mean_data_rate;
      log_ptr->medium_time = pACInfo->curr_QoSInfo[tspec_pending - 1].medium_time;
      log_ptr->min_data_rate = pACInfo->curr_QoSInfo[tspec_pending - 1].min_data_rate;
      log_ptr->min_phy_rate = pACInfo->curr_QoSInfo[tspec_pending - 1].min_phy_rate;
      log_ptr->min_service_interval = pACInfo->curr_QoSInfo[tspec_pending - 1].min_service_interval;
      log_ptr->nominal_msdu_size = pACInfo->curr_QoSInfo[tspec_pending - 1].nominal_msdu_size;
      log_ptr->peak_data_rate = pACInfo->curr_QoSInfo[tspec_pending - 1].peak_data_rate;
      log_ptr->surplus_bw_allowance = pACInfo->curr_QoSInfo[tspec_pending - 1].surplus_bw_allowance;
      log_ptr->suspension_interval = pACInfo->curr_QoSInfo[tspec_pending - 1].surplus_bw_allowance;
      log_ptr->suspension_interval = pACInfo->curr_QoSInfo[tspec_pending - 1].suspension_interval;
      log_ptr->svc_start_time = pACInfo->curr_QoSInfo[tspec_pending - 1].svc_start_time;
      log_ptr->tsinfo[0] = pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.direction << 5 |
         pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.tid << 1;
      log_ptr->tsinfo[1] = pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.up << 11 |
         pACInfo->curr_QoSInfo[tspec_pending - 1].ts_info.psb << 10;
      log_ptr->tsinfo[2] = 0;
   }
   WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
#endif //FEATURE_WLAN_DIAG_SUPPORT
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
   if (ac == SME_QOS_EDCA_AC_VO)
   {
      // Indicate to neighbor roam logic of the new required VO
      // ac bandwidth requirement.
      csrNeighborRoamIndicateVoiceBW( pMac, pACInfo->curr_QoSInfo[tspec_pending - 1].peak_data_rate, TRUE );
   }
#endif
   pACInfo->tspec_pending = 0;

   sme_QosStateTransition(sessionId, ac, SME_QOS_QOS_ON);


   (void)sme_QosProcessBufferedCmd(sessionId);
   return eHAL_STATUS_SUCCESS;
   
}
/*--------------------------------------------------------------------------
  \brief sme_QosAggregateParams() - Utiltity function to increament the TSPEC 
  params per AC. Typical usage while using flow aggregation or deletion of flows
  
  \param pInput_Tspec_Info - Pointer to sme_QosWmmTspecInfo which contains the 
  WMM TSPEC related info with which pCurrent_Tspec_Info will be updated
  \param pCurrent_Tspec_Info - Pointer to sme_QosWmmTspecInfo which contains 
  current the WMM TSPEC related info

  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosAggregateParams(
   sme_QosWmmTspecInfo * pInput_Tspec_Info,
   sme_QosWmmTspecInfo * pCurrent_Tspec_Info,
   sme_QosWmmTspecInfo * pUpdated_Tspec_Info)
{
   sme_QosWmmTspecInfo TspecInfo;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked",
             __func__, __LINE__);
   if(!pInput_Tspec_Info)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: input is NULL, nothing to aggregate",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   if(!pCurrent_Tspec_Info)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Current is NULL, can't aggregate",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_copy(&TspecInfo, pCurrent_Tspec_Info, 
                sizeof(sme_QosWmmTspecInfo));
   TspecInfo.ts_info.psb = pInput_Tspec_Info->ts_info.psb;
   /*-------------------------------------------------------------------------
     APSD preference is only meaningful if service interval was set by app
   -------------------------------------------------------------------------*/
   if(pCurrent_Tspec_Info->min_service_interval &&
      pInput_Tspec_Info->min_service_interval &&
      (pCurrent_Tspec_Info->ts_info.direction !=
      pInput_Tspec_Info->ts_info.direction))
   {
      TspecInfo.min_service_interval = VOS_MIN(
         pCurrent_Tspec_Info->min_service_interval,
         pInput_Tspec_Info->min_service_interval);
   }
   else if(pInput_Tspec_Info->min_service_interval)
   {
      TspecInfo.min_service_interval = pInput_Tspec_Info->min_service_interval;
   }
   if(pCurrent_Tspec_Info->max_service_interval &&
      pInput_Tspec_Info->max_service_interval &&
      (pCurrent_Tspec_Info->ts_info.direction !=
      pInput_Tspec_Info->ts_info.direction))
   {
      TspecInfo.max_service_interval = VOS_MIN(
         pCurrent_Tspec_Info->max_service_interval,
         pInput_Tspec_Info->max_service_interval);
   }
   else
   {
      TspecInfo.max_service_interval = pInput_Tspec_Info->max_service_interval;
   }
   /*-------------------------------------------------------------------------
     If directions don't match, it must necessarily be both uplink and
     downlink
   -------------------------------------------------------------------------*/
   if(pCurrent_Tspec_Info->ts_info.direction != 
      pInput_Tspec_Info->ts_info.direction)
   {
      TspecInfo.ts_info.direction = pInput_Tspec_Info->ts_info.direction;
   }
   /*-------------------------------------------------------------------------
     Max MSDU size : these sizes are `maxed'
   -------------------------------------------------------------------------*/
   TspecInfo.maximum_msdu_size = VOS_MAX(pCurrent_Tspec_Info->maximum_msdu_size,
                                         pInput_Tspec_Info->maximum_msdu_size);

   /*-------------------------------------------------------------------------
     Inactivity interval : these sizes are `maxed'
   -------------------------------------------------------------------------*/
   TspecInfo.inactivity_interval = VOS_MAX(pCurrent_Tspec_Info->inactivity_interval,
                                         pInput_Tspec_Info->inactivity_interval);

   /*-------------------------------------------------------------------------
     Delay bounds: min of all values
     Check on 0: if 0, it means initial value since delay can never be 0!!
   -------------------------------------------------------------------------*/
   if(pCurrent_Tspec_Info->delay_bound)
   {
      TspecInfo.delay_bound = VOS_MIN(pCurrent_Tspec_Info->delay_bound,
                                      pInput_Tspec_Info->delay_bound);
   }
   else
   {
      TspecInfo.delay_bound = pInput_Tspec_Info->delay_bound;
   }
   TspecInfo.max_burst_size = VOS_MAX(pCurrent_Tspec_Info->max_burst_size,
                                      pInput_Tspec_Info->max_burst_size);

   /*-------------------------------------------------------------------------
     Nominal MSDU size also has a fixed bit that needs to be `handled' before
     aggregation
     This can be handled only if previous size is the same as new or both have
     the fixed bit set
     These sizes are not added: but `maxed'
   -------------------------------------------------------------------------*/
   TspecInfo.nominal_msdu_size = VOS_MAX(
      pCurrent_Tspec_Info->nominal_msdu_size & ~SME_QOS_16BIT_MSB,
      pInput_Tspec_Info->nominal_msdu_size & ~SME_QOS_16BIT_MSB);

   if( ((pCurrent_Tspec_Info->nominal_msdu_size == 0) ||
        (pCurrent_Tspec_Info->nominal_msdu_size & SME_QOS_16BIT_MSB)) &&
       ((pInput_Tspec_Info->nominal_msdu_size == 0) ||
        (pInput_Tspec_Info->nominal_msdu_size & SME_QOS_16BIT_MSB)))
   {
     TspecInfo.nominal_msdu_size |= SME_QOS_16BIT_MSB;
   }

   /*-------------------------------------------------------------------------
     Data rates: 
     Add up the rates for aggregation
   -------------------------------------------------------------------------*/
   SME_QOS_BOUNDED_U32_ADD_Y_TO_X( TspecInfo.peak_data_rate,
                                   pInput_Tspec_Info->peak_data_rate );
   SME_QOS_BOUNDED_U32_ADD_Y_TO_X( TspecInfo.min_data_rate,
                                   pInput_Tspec_Info->min_data_rate );
   /* mean data rate = peak data rate: aggregate to be flexible on apps  */
   SME_QOS_BOUNDED_U32_ADD_Y_TO_X( TspecInfo.mean_data_rate,
                                   pInput_Tspec_Info->mean_data_rate );

   /*-------------------------------------------------------------------------
     Suspension interval : this is set to the inactivity interval since per
     spec it is less than or equal to inactivity interval
     This is not provided by app since we currently don't support the HCCA
     mode of operation
     Currently set it to 0 to avoid confusion: Cisco ESE needs ~0; spec
     requires inactivity interval to be > suspension interval: this could
     be tricky!
   -------------------------------------------------------------------------*/
   TspecInfo.suspension_interval = 0;
   /*-------------------------------------------------------------------------
     Remaining parameters do not come from app as they are very WLAN
     air interface specific
     Set meaningful values here
   -------------------------------------------------------------------------*/
   TspecInfo.medium_time = 0;               /* per WMM spec                 */
   TspecInfo.min_phy_rate = SME_QOS_MIN_PHY_RATE;
   TspecInfo.svc_start_time = 0;           /* arbitrary                  */
   TspecInfo.surplus_bw_allowance += pInput_Tspec_Info->surplus_bw_allowance;
   if(TspecInfo.surplus_bw_allowance > SME_QOS_SURPLUS_BW_ALLOWANCE)
   {
      TspecInfo.surplus_bw_allowance = SME_QOS_SURPLUS_BW_ALLOWANCE;
   }
   /* Set ack_policy to block ack even if one stream requests block ack policy */
   if((pInput_Tspec_Info->ts_info.ack_policy == SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK) ||
      (pCurrent_Tspec_Info->ts_info.ack_policy == SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK))
   {
     TspecInfo.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK;
   }

   if(pInput_Tspec_Info->ts_info.burst_size_defn || pCurrent_Tspec_Info->ts_info.burst_size_defn )
   {
     TspecInfo.ts_info.burst_size_defn = 1;
   }
   if(pUpdated_Tspec_Info)
   {
      vos_mem_copy(pUpdated_Tspec_Info, &TspecInfo, 
                   sizeof(sme_QosWmmTspecInfo));
   }
   else
   {
      vos_mem_copy(pCurrent_Tspec_Info, &TspecInfo, 
                   sizeof(sme_QosWmmTspecInfo));
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosUpdateParams() - Utiltity function to update the TSPEC 
  params per AC. Typical usage while deleting flows on AC which is running
  multiple flows
  
  \param sessionId - Session upon which the TSPEC is being updated
  \param ac - Enumeration of the various EDCA Access Categories.
  \param tspec_mask - on which tspec per AC, the update is requested
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosUpdateParams(v_U8_t sessionId,
                                      sme_QosEdcaAcType ac,
                                      v_U8_t tspec_mask, 
                                      sme_QosWmmTspecInfo * pTspec_Info)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosWmmTspecInfo Tspec_Info;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: invoked on session %d for AC %d TSPEC %d",
             __func__, __LINE__,
             sessionId, ac, tspec_mask);
   if(!pTspec_Info)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: output is NULL, can't aggregate",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_zero(&Tspec_Info, sizeof(sme_QosWmmTspecInfo));
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Flow List empty, nothing to update",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   //init the TS info field
   Tspec_Info.ts_info.up  = pACInfo->curr_QoSInfo[tspec_mask - 1].ts_info.up;
   Tspec_Info.ts_info.psb = pACInfo->curr_QoSInfo[tspec_mask - 1].ts_info.psb;
   Tspec_Info.ts_info.tid = pACInfo->curr_QoSInfo[tspec_mask - 1].ts_info.tid;
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      if((sessionId == flow_info->sessionId) &&
         (ac == flow_info->ac_type) &&
         (tspec_mask == flow_info->tspec_mask))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Flow %d matches",
                   __func__, __LINE__,
                   flow_info->QosFlowID);
         
         if((SME_QOS_REASON_RELEASE == flow_info->reason ) ||
            (SME_QOS_REASON_MODIFY == flow_info->reason))
         {
            //msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                      "%s: %d: Skipping Flow %d as it is marked "
                      "for release/modify",
                      __func__, __LINE__,
                      flow_info->QosFlowID);
         }
         else if(!HAL_STATUS_SUCCESS(sme_QosAggregateParams(&flow_info->QoSInfo, 
                                                            &Tspec_Info,
                                                            NULL)))
         {
            //err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosAggregateParams() failed",
                      __func__, __LINE__);
         }
      }
      pEntry = pNextEntry;
   }
   // return the aggregate
   *pTspec_Info = Tspec_Info;
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosAcToUp() - Utiltity function to map an AC to UP
  Note: there is a quantization loss here because 4 ACs are mapped to 8 UPs
  Mapping is done for consistency
  \param ac - Enumeration of the various EDCA Access Categories.
  \return an User Priority
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosWmmUpType sme_QosAcToUp(sme_QosEdcaAcType ac)
{
   sme_QosWmmUpType up = SME_QOS_WMM_UP_MAX;
   if(ac >= 0 && ac < SME_QOS_EDCA_AC_MAX)
   {
      up = sme_QosACtoUPMap[ac];
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
             "%s: %d: ac = %d up = %d returned",
             __func__, __LINE__, ac, up);
   return up;
}
/*--------------------------------------------------------------------------
  \brief sme_QosUpToAc() - Utiltity function to map an UP to AC
  \param up - Enumeration of the various User priorities (UP).
  \return an Access Category
  
  \sa
  
  --------------------------------------------------------------------------*/
sme_QosEdcaAcType sme_QosUpToAc(sme_QosWmmUpType up)
{
   sme_QosEdcaAcType ac = SME_QOS_EDCA_AC_MAX;
   if(up >= 0 && up < SME_QOS_WMM_UP_MAX)
   {
      ac = sme_QosUPtoACMap[up];
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED, 
             "%s: %d: up = %d ac = %d returned",
             __func__, __LINE__, up, ac);
   return ac;
}
/*--------------------------------------------------------------------------
  \brief sme_QosStateTransition() - The state transition function per AC. We
  save the previous state also.
  \param sessionId - Session upon which the state machine is running
  \param ac - Enumeration of the various EDCA Access Categories.
  \param new_state - The state FSM is moving to.
  
  \return None
  
  \sa
  
  --------------------------------------------------------------------------*/
static void sme_QosStateTransition(v_U8_t sessionId,
                                   sme_QosEdcaAcType ac,
                                   sme_QosStates new_state)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   pACInfo->prev_state = pACInfo->curr_state;
   pACInfo->curr_state = new_state;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: On session %d new state=%d, old state=%d, for AC=%d",
             __func__, __LINE__, 
             sessionId, pACInfo->curr_state, pACInfo->prev_state, ac );
}
/*--------------------------------------------------------------------------
  \brief sme_QosFindInFlowList() - Utility function to find an flow entry from
  the flow_list.
  \param search_key -  We can either use the flowID or the ac type to find the 
  entry in the flow list.
  A bitmap in sme_QosSearchInfo tells which key to use. Starting from LSB,
  bit 0 - Flow ID
  bit 1 - AC type
  \return the pointer to the entry in the link list
  
  \sa
  
  --------------------------------------------------------------------------*/
tListElem *sme_QosFindInFlowList(sme_QosSearchInfo search_key)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Flow List empty, can't search",
                __func__, __LINE__);
      return NULL;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      if((search_key.sessionId == flow_info->sessionId) ||
         (search_key.sessionId == SME_QOS_SEARCH_SESSION_ID_ANY))
      {
         if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_1)
         {
            if(search_key.key.QosFlowID == flow_info->QosFlowID)
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on flowID, ending search",
                         __func__, __LINE__);
               break;
            }
         }
         else if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_2)
         {
            if(search_key.key.ac_type == flow_info->ac_type)
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on ac, ending search",
                         __func__, __LINE__);
               break;
            }
         }
         else if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_3)
         {
            if(search_key.key.reason == flow_info->reason)
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on reason, ending search",
                         __func__, __LINE__);
               break;
            }
         }
         else if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_4)
         {
            if((search_key.key.ac_type == flow_info->ac_type) && 
               (search_key.direction == flow_info->QoSInfo.ts_info.direction))
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on reason, ending search",
                         __func__, __LINE__);

               break;
            }
         }
      }
      pEntry = pNextEntry;
   }
   return pEntry;
}
/*--------------------------------------------------------------------------
  \brief sme_QosFindAllInFlowList() - Utility function to find an flow entry 
  from the flow_list & act on it.
  \param search_key -  We can either use the flowID or the ac type to find the 
  entry in the flow list.
  A bitmap in sme_QosSearchInfo tells which key to use. Starting from LSB,
  bit 0 - Flow ID
  bit 1 - AC type
  \param fnp - function pointer specifying the action type for the entry found
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosFindAllInFlowList(tpAniSirGlobal pMac,
                                    sme_QosSearchInfo search_key, 
                                    sme_QosProcessSearchEntry fnp)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosSessionInfo *pSession;
   sme_QosFlowInfoEntry *flow_info = NULL;
   eHalStatus status = eHAL_STATUS_FAILURE;
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Flow List empty, can't search",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
      if((search_key.sessionId == flow_info->sessionId) ||
         (search_key.sessionId == SME_QOS_SEARCH_SESSION_ID_ANY))
      {
         if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_1)
         {
            if(search_key.key.QosFlowID == flow_info->QosFlowID)
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on flowID, ending search",
                         __func__, __LINE__);
               status = fnp(pMac, pEntry);
               if(eHAL_STATUS_FAILURE == status)
               {
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                            "%s: %d: Failed to process entry",
                            __func__, __LINE__);
                  break;
               }
            }
         }
         else if(search_key.index & SME_QOS_SEARCH_KEY_INDEX_2)
         {
            if(search_key.key.ac_type == flow_info->ac_type)
            {
               //msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: match found on ac, ending search",
                         __func__, __LINE__);
               flow_info->hoRenewal = pSession->ac_info[flow_info->ac_type].hoRenewal;
               status = fnp(pMac, pEntry);
               if(eHAL_STATUS_FAILURE == status)
               {
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                            "%s: %d: Failed to process entry",
                            __func__, __LINE__);
                  break;
               }
            }
         }
      }
      pEntry = pNextEntry;
   }
   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_QosIsACM() - Utility function to check if a particular AC
  mandates Admission Control.
  \param ac - Enumeration of the various EDCA Access Categories.
  
  \return VOS_TRUE if the AC mandates Admission Control
  
  \sa
  
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosIsACM(tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc, 
                      sme_QosEdcaAcType ac, tDot11fBeaconIEs *pIes)
{
   v_BOOL_t ret_val = VOS_FALSE;
   tDot11fBeaconIEs *pIesLocal;
   if(!pSirBssDesc)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: pSirBssDesc is NULL",
                __func__, __LINE__);
      return VOS_FALSE;
   }

   if (NULL != pIes)
   {
      /* IEs were provided so use them locally */
      pIesLocal = pIes;
   }
   else
   {
      /* IEs were not provided so parse them ourselves */
      if (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal)))
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: csrGetParsedBssDescriptionIEs() failed",
                   __func__, __LINE__);
         return VOS_FALSE;
      }

      /* if success then pIesLocal was allocated */
   }

   if(CSR_IS_QOS_BSS(pIesLocal))
   {
       switch(ac)
       {
          case SME_QOS_EDCA_AC_BE:
             if(pIesLocal->WMMParams.acbe_acm) ret_val = VOS_TRUE;
             break;
          case SME_QOS_EDCA_AC_BK:
             if(pIesLocal->WMMParams.acbk_acm) ret_val = VOS_TRUE;
             break;
          case SME_QOS_EDCA_AC_VI:
             if(pIesLocal->WMMParams.acvi_acm) ret_val = VOS_TRUE;
             break;
          case SME_QOS_EDCA_AC_VO:
             if(pIesLocal->WMMParams.acvo_acm) ret_val = VOS_TRUE;
             break;
          default:
             VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                       "%s: %d: unknown AC = %d",
                       __func__, __LINE__, ac);
             //Assert
             VOS_ASSERT(0);
             break;
       }
   }//IS_QOS_BSS
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: ACM = %d for AC = %d",
             __func__, __LINE__, ret_val, ac );
   if (NULL == pIes)
   {
      /* IEs were allocated locally so free them */
      vos_mem_free(pIesLocal);
   }
   return ret_val;
}
/*--------------------------------------------------------------------------
  \brief sme_QosBufferExistingFlows() - Utility function to buffer the existing
  flows in flow_list, so that we can renew them after handoff is done.

  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosBufferExistingFlows(tpAniSirGlobal pMac,
                                             v_U8_t sessionId)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosSessionInfo *pSession;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosCmdInfo  cmd;
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: %d: Flow List empty, nothing to buffer",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      if (flow_info->sessionId == sessionId)
      {
         if((SME_QOS_REASON_REQ_SUCCESS == flow_info->reason )||
            (SME_QOS_REASON_SETUP == flow_info->reason ))
         {
            cmd.command = SME_QOS_SETUP_REQ;
            cmd.pMac = pMac;
            cmd.sessionId = sessionId;
            cmd.u.setupCmdInfo.HDDcontext = flow_info->HDDcontext;
            cmd.u.setupCmdInfo.QoSInfo = flow_info->QoSInfo;
            cmd.u.setupCmdInfo.QoSCallback = flow_info->QoSCallback;
            cmd.u.setupCmdInfo.UPType = SME_QOS_WMM_UP_MAX;//shouldn't be needed
            cmd.u.setupCmdInfo.QosFlowID = flow_info->QosFlowID;
            if(SME_QOS_REASON_SETUP == flow_info->reason )
            {
               cmd.u.setupCmdInfo.hoRenewal = VOS_FALSE;
            }
            else
            {
               cmd.u.setupCmdInfo.hoRenewal = VOS_TRUE;//TODO: might need this for modify
            }
            if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_TRUE)))
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: couldn't buffer the setup request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: buffered a setup request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
         }
         else if(SME_QOS_REASON_RELEASE == flow_info->reason ) 
         {
            cmd.command = SME_QOS_RELEASE_REQ;
            cmd.pMac = pMac;
            cmd.sessionId = sessionId;
            cmd.u.releaseCmdInfo.QosFlowID = flow_info->QosFlowID;
            if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_TRUE)))
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: couldn't buffer the release request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: buffered a release request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
         }
         else if(SME_QOS_REASON_MODIFY_PENDING == flow_info->reason)
         {
            cmd.command = SME_QOS_MODIFY_REQ;
            cmd.pMac = pMac;
            cmd.sessionId = sessionId;
            cmd.u.modifyCmdInfo.QosFlowID = flow_info->QosFlowID;
            cmd.u.modifyCmdInfo.QoSInfo = flow_info->QoSInfo;
            if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_TRUE)))
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                         "%s: %d: couldn't buffer the modify request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                         "%s: %d: buffered a modify request for "
                         "flow %d in handoff state",
                         __func__, __LINE__,
                         flow_info->QosFlowID);
            }
         }
         //delete the entry from Flow List
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "%s: %d: Deleting original entry at %p with flowID %d",
                   __func__, __LINE__,
                   flow_info, flow_info->QosFlowID);
         csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
         vos_mem_free(flow_info);
      }
      pEntry = pNextEntry;
   }
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pSession->uapsdAlreadyRequested = VOS_FALSE;
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosDeleteExistingFlows() - Utility function to Delete the existing
  flows in flow_list, if we lost connectivity.

  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosDeleteExistingFlows(tpAniSirGlobal pMac,
                                             v_U8_t sessionId)
{
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_TRUE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
                "%s: %d: Flow List empty, nothing to delete",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_TRUE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      if (flow_info->sessionId == sessionId)
      {
         if((SME_QOS_REASON_REQ_SUCCESS == flow_info->reason )||
            (SME_QOS_REASON_SETUP == flow_info->reason )||
            (SME_QOS_REASON_RELEASE == flow_info->reason )||
            (SME_QOS_REASON_MODIFY == flow_info->reason ))
         {
            flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                   NULL,
                                   SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
                                   flow_info->QosFlowID);
         }
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: %d: Deleting entry at %p with flowID %d",
                   __func__, __LINE__,
                   flow_info, flow_info->QosFlowID);
         //delete the entry from Flow List
         csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
         vos_mem_free(flow_info);
      }
      pEntry = pNextEntry;
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosBufferCmd() - Utility function to buffer a request (setup/modify/
  release) from client while processing another one on the same AC.
  \param pcmd - a pointer to the cmd structure to be saved inside the buffered
                cmd link list
                
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosBufferCmd(sme_QosCmdInfo *pcmd, v_BOOL_t insert_head)
{
   sme_QosSessionInfo *pSession;
   sme_QosCmdInfoEntry * pentry = NULL;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked",
             __func__, __LINE__);
   pentry = (sme_QosCmdInfoEntry *) vos_mem_malloc(sizeof(sme_QosCmdInfoEntry));
   if (!pentry)
   {
      //err msg
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Memory allocation failure",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   // copy the entire CmdInfo
   pentry->cmdInfo = *pcmd;
 
   pSession = &sme_QosCb.sessionInfo[pcmd->sessionId];
   if(insert_head) 
   {
      csrLLInsertHead(&pSession->bufferedCommandList, &pentry->link, VOS_TRUE);
   }
   else
   {
      csrLLInsertTail(&pSession->bufferedCommandList, &pentry->link, VOS_TRUE);
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessBufferedCmd() - Utility function to process a buffered 
  request (setup/modify/release) initially came from the client.

  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosProcessBufferedCmd(v_U8_t sessionId)
{
   sme_QosSessionInfo *pSession;
   sme_QosCmdInfoEntry *pcmd = NULL;
   tListElem *pEntry= NULL;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked on session %d",
             __func__, __LINE__,
             sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   if(!csrLLIsListEmpty( &pSession->bufferedCommandList, VOS_FALSE ))
   {
      pEntry = csrLLRemoveHead( &pSession->bufferedCommandList, VOS_TRUE );
      if(!pEntry)
      {
         //Err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: no more buffered commands on session %d",
                   __func__, __LINE__,
                   sessionId);
         pSession->readyForPowerSave = VOS_TRUE;
         return eHAL_STATUS_FAILURE;
      }
      pcmd = GET_BASE_ADDR( pEntry, sme_QosCmdInfoEntry, link );
      switch(pcmd->cmdInfo.command)
      {
      case SME_QOS_SETUP_REQ:
         hdd_status = sme_QosInternalSetupReq(pcmd->cmdInfo.pMac, 
                                              pcmd->cmdInfo.sessionId,
                                              &pcmd->cmdInfo.u.setupCmdInfo.QoSInfo,
                                              pcmd->cmdInfo.u.setupCmdInfo.QoSCallback, 
                                              pcmd->cmdInfo.u.setupCmdInfo.HDDcontext, 
                                              pcmd->cmdInfo.u.setupCmdInfo.UPType, 
                                              pcmd->cmdInfo.u.setupCmdInfo.QosFlowID, 
                                              VOS_TRUE,
                                              pcmd->cmdInfo.u.setupCmdInfo.hoRenewal);
         if(SME_QOS_STATUS_SETUP_FAILURE_RSP == hdd_status)
         {
            //Err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosInternalSetupReq failed on session %d",
                      __func__, __LINE__,
                      sessionId);
            halStatus = eHAL_STATUS_FAILURE;
         }
         break;
      case SME_QOS_RELEASE_REQ:
         hdd_status = sme_QosInternalReleaseReq(pcmd->cmdInfo.pMac, 
                                                pcmd->cmdInfo.u.releaseCmdInfo.QosFlowID,
                                                VOS_TRUE);
         if(SME_QOS_STATUS_RELEASE_FAILURE_RSP == hdd_status)
         {
            //Err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosInternalReleaseReq failed on session %d",
                      __func__, __LINE__,
                      sessionId);
            halStatus = eHAL_STATUS_FAILURE;
         }
         break;
      case SME_QOS_MODIFY_REQ:
         hdd_status = sme_QosInternalModifyReq(pcmd->cmdInfo.pMac, 
                                               &pcmd->cmdInfo.u.modifyCmdInfo.QoSInfo,
                                               pcmd->cmdInfo.u.modifyCmdInfo.QosFlowID,
                                               VOS_TRUE);
         if(SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP == hdd_status)
         {
            //Err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosInternalModifyReq failed on session %d",
                      __func__, __LINE__,
                      sessionId);
            halStatus = eHAL_STATUS_FAILURE;
         }
         break;
      case SME_QOS_RESEND_REQ:
         hdd_status = sme_QosReRequestAddTS(pcmd->cmdInfo.pMac, 
                                            pcmd->cmdInfo.sessionId,
                                            &pcmd->cmdInfo.u.resendCmdInfo.QoSInfo,
                                            pcmd->cmdInfo.u.resendCmdInfo.ac,
                                            pcmd->cmdInfo.u.resendCmdInfo.tspecMask);
         if(SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP == hdd_status)
         {
            //Err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: sme_QosReRequestAddTS failed on session %d",
                      __func__, __LINE__,
                      sessionId);
            halStatus = eHAL_STATUS_FAILURE;
         }
         break;
      default:
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: On session %d unknown cmd = %d",
                   __func__, __LINE__,
                   sessionId, pcmd->cmdInfo.command);
         //ASSERT
         VOS_ASSERT(0);
         break;
      }
      // buffered command has been processed, reclaim the memory
      vos_mem_free(pcmd);
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: cmd buffer empty",
                __func__, __LINE__);
      pSession->readyForPowerSave = VOS_TRUE;
   }
   return halStatus;
}
/*--------------------------------------------------------------------------
  \brief sme_QosDeleteBufferedRequests() - Utility function to Delete the buffered
  requests in the buffered_cmd_list, if we lost connectivity.

  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_QosDeleteBufferedRequests(tpAniSirGlobal pMac,
                                                v_U8_t sessionId)
{
   sme_QosSessionInfo *pSession;
   sme_QosCmdInfoEntry *pcmd = NULL;
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: Invoked on session %d",
             __func__, __LINE__, sessionId);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pEntry = csrLLPeekHead( &pSession->bufferedCommandList, VOS_TRUE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN, 
                "%s: %d: Buffered List empty, nothing to delete on session %d",
                __func__, __LINE__,
                sessionId);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &pSession->bufferedCommandList, pEntry, VOS_TRUE );
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                "%s: %d: deleting entry from buffered List",
                __func__, __LINE__);
      //delete the entry from Flow List
      csrLLRemoveEntry(&pSession->bufferedCommandList, pEntry, VOS_TRUE );
      // reclaim the memory
      pcmd = GET_BASE_ADDR( pEntry, sme_QosCmdInfoEntry, link );
      vos_mem_free(pcmd);
      pEntry = pNextEntry;
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosSaveAssocInfo() - Utility function to save the assoc info in the
  CB like BSS descritor of the AP, the profile that HDD sent down with the 
  connect request, while CSR notifies for assoc/reassoc success.
  \param pAssoc_info - pointer to the assoc structure to store the BSS descritor 
                       of the AP, the profile that HDD sent down with the 
                       connect request
                       
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosSaveAssocInfo(sme_QosSessionInfo *pSession, sme_QosAssocInfo *pAssoc_info)
{
   tSirBssDescription    *pBssDesc = NULL;
   v_U32_t                bssLen = 0;
   if(NULL == pAssoc_info)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: pAssoc_info is NULL",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   //clean up the assoc info if already set
   if(pSession->assocInfo.pBssDesc)
   {
      vos_mem_free(pSession->assocInfo.pBssDesc);
      pSession->assocInfo.pBssDesc = NULL;
   }
   bssLen = pAssoc_info->pBssDesc->length + 
      sizeof(pAssoc_info->pBssDesc->length);
   //save the bss Descriptor
   pBssDesc = (tSirBssDescription *)vos_mem_malloc(bssLen);
   if (!pBssDesc)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: couldn't allocate memory for the bss Descriptor",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   vos_mem_copy(pBssDesc, pAssoc_info->pBssDesc, bssLen);
   pSession->assocInfo.pBssDesc = pBssDesc;
   //save the apsd info from assoc
   if(pAssoc_info->pProfile)
   {
       pSession->apsdMask |= pAssoc_info->pProfile->uapsd_mask;
   }
   // [TODO] Do we need to update the global APSD bitmap?
   return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_QosSetupFnp() - Utility function (pointer) to notify other entries 
  in FLOW list on the same AC that qos params got modified
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosSetupFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_MODIFIED_IND;
   sme_QosEdcaAcType ac;
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   if(SME_QOS_REASON_REQ_SUCCESS == flow_info->reason)
   {
      //notify HDD, only the other Flows running on the AC 
      flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                             &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                             hdd_status,
                             flow_info->QosFlowID);
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Entry with flowID = %d getting notified",
                __func__, __LINE__,
                flow_info->QosFlowID);
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosModificationNotifyFnp() - Utility function (pointer) to notify 
  other entries in FLOW list on the same AC that qos params got modified
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosModificationNotifyFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_MODIFIED_IND;
   sme_QosEdcaAcType ac;
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   if(SME_QOS_REASON_REQ_SUCCESS == flow_info->reason)
   {
      //notify HDD, only the other Flows running on the AC 
      flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                             &pACInfo->curr_QoSInfo[flow_info->tspec_mask - 1],
                             hdd_status,
                             flow_info->QosFlowID);
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Entry with flowID = %d getting notified",
                __func__, __LINE__,
                flow_info->QosFlowID);
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosModifyFnp() - Utility function (pointer) to delete the origianl 
  entry in FLOW list & add the modified one
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosModifyFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosFlowInfoEntry *flow_info = NULL;
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   switch(flow_info->reason)
   {
   case SME_QOS_REASON_MODIFY_PENDING:
      //set the proper reason code for the new (with modified params) entry
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
      break;
   case SME_QOS_REASON_MODIFY:
      //delete the original entry from Flow List
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Deleting original entry at %p with flowID %d",
                __func__, __LINE__,
                flow_info, flow_info->QosFlowID);
      csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
      // reclaim the memory
      vos_mem_free(flow_info);
      break;
   default:
      break;
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosDelTsIndFnp() - Utility function (pointer) to find all Flows on 
  the perticular AC & delete them, also send HDD indication through the callback 
  it registered per request
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosDelTsIndFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   sme_QosEdcaAcType ac;
   eHalStatus lock_status = eHAL_STATUS_FAILURE;
   sme_QosStatusType status;

   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   //delete the entry from Flow List
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   pACInfo->relTrig = SME_QOS_RELEASE_BY_AP;

   lock_status = sme_AcquireGlobalLock( &pMac->sme );
   if ( !HAL_STATUS_SUCCESS( lock_status ) )
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: Unable to obtain lock",
                __func__, __LINE__);
      return SME_QOS_STATUS_RELEASE_FAILURE_RSP;
   }
   //Call the internal function for QoS release, adding a layer of abstraction
   status = sme_QosInternalReleaseReq(pMac, flow_info->QosFlowID, VOS_FALSE);
   sme_ReleaseGlobalLock( &pMac->sme );
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
             "%s: %d: QoS Release return status on Flow %d is %d",
             __func__, __LINE__,
             flow_info->QosFlowID, status);

   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosReassocSuccessEvFnp() - Utility function (pointer) to notify HDD 
  the success for the requested flow & notify all the other flows running on the 
  same AC that QoS params got modified
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosReassocSuccessEvFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   v_BOOL_t delete_entry = VOS_FALSE;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   sme_QosEdcaAcType ac;
   eHalStatus pmc_status = eHAL_STATUS_FAILURE;
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR(pEntry, sme_QosFlowInfoEntry, link);
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   switch(flow_info->reason)
   {
   case SME_QOS_REASON_SETUP:
      hdd_status = SME_QOS_STATUS_SETUP_SUCCESS_IND;
      delete_entry = VOS_FALSE;
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
      //check for the case where we had to do reassoc to reset the apsd bit
      //for the ac - release or modify scenario
      if(pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0].ts_info.psb)
      {
         // notify PMC as App is looking for APSD. If we already requested
         // then we don't need to do anything.
         if(!pSession->uapsdAlreadyRequested)
         {
            // this is the first flow to detect we need PMC in UAPSD mode
   
            pmc_status = pmcStartUapsd(pMac,
                                       sme_QosPmcStartUapsdCallback,
                                       pSession);
            // if PMC doesn't return success right away means it is yet to put
            // the module in BMPS state & later to UAPSD state
         
            if(eHAL_STATUS_FAILURE == pmc_status)
            {
               hdd_status = SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_SET_FAILED;
               //we need to always notify this case
               flow_info->hoRenewal = VOS_FALSE;
            }
            else if(eHAL_STATUS_PMC_PENDING == pmc_status)
            {
               // let other flows know PMC has been notified
               pSession->uapsdAlreadyRequested = VOS_TRUE;
            }
            // for any other pmc status we declare success
         }
      }
      break;
   case SME_QOS_REASON_RELEASE:
      pACInfo->num_flows[SME_QOS_TSPEC_INDEX_0]--;
      // fall through
   case SME_QOS_REASON_MODIFY:
      delete_entry = VOS_TRUE;
      break;
   case SME_QOS_REASON_MODIFY_PENDING:
      hdd_status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND;
      delete_entry = VOS_FALSE;
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
      if(pACInfo->requested_QoSInfo[SME_QOS_TSPEC_INDEX_0].ts_info.psb)
      {
   
         if(!pSession->uapsdAlreadyRequested)
         {
            // this is the first flow to detect we need PMC in UAPSD mode
            pmc_status = pmcStartUapsd(pMac,
                                       sme_QosPmcStartUapsdCallback,
                                       pSession);
         
            // if PMC doesn't return success right away means it is yet to put
            // the module in BMPS state & later to UAPSD state
            if(eHAL_STATUS_FAILURE == pmc_status)
            {
               hdd_status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_SET_FAILED;
               // we need to always notify this case
               flow_info->hoRenewal = VOS_FALSE;
            }
            else if(eHAL_STATUS_PMC_PENDING == pmc_status)
            {
               pSession->uapsdAlreadyRequested = VOS_TRUE;
            }
            // for any other pmc status we declare success
         }
      }
      break;
   case SME_QOS_REASON_REQ_SUCCESS:
      hdd_status = SME_QOS_STATUS_SETUP_MODIFIED_IND;
      // fall through
   default:
      delete_entry = VOS_FALSE;
      break;
   }
   if(!delete_entry)
   {
      if(!flow_info->hoRenewal)
      {
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pACInfo->curr_QoSInfo[SME_QOS_TSPEC_INDEX_0],
                                hdd_status,
                                flow_info->QosFlowID);
      }
      else
      {
         flow_info->hoRenewal = VOS_FALSE;
      }
   }
   else
   {
      //delete the entry from Flow List
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Deleting entry at %p with flowID %d",
                __func__, __LINE__,
                flow_info, flow_info->QosFlowID);
      csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
      // reclaim the memory
      vos_mem_free(flow_info);
   }
   
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosAddTsFailureFnp() - Utility function (pointer), 
  if the Addts request was for for an flow setup request, delete the entry from 
  Flow list & notify HDD 
  if the Addts request was for downgrading of QoS params because of an flow 
  release requested on the AC, delete the entry from Flow list & notify HDD 
  if the Addts request was for change of QoS params because of an flow 
  modification requested on the AC, delete the new entry from Flow list & notify 
  HDD 

  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosAddTsFailureFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   v_BOOL_t inform_hdd = VOS_FALSE;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   sme_QosEdcaAcType ac;
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   switch(flow_info->reason)
   {
   case SME_QOS_REASON_SETUP:
      hdd_status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
      pACInfo->num_flows[pACInfo->tspec_pending - 1]--;
      inform_hdd = VOS_TRUE;
      break;
   case SME_QOS_REASON_RELEASE:
      hdd_status = SME_QOS_STATUS_RELEASE_FAILURE_RSP;
      pACInfo->num_flows[pACInfo->tspec_pending - 1]--;
      inform_hdd = VOS_TRUE;
      break;
   case SME_QOS_REASON_MODIFY_PENDING:
      hdd_status = SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      inform_hdd = VOS_TRUE;
      break;
   case SME_QOS_REASON_MODIFY:
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
   case SME_QOS_REASON_REQ_SUCCESS:
      if(flow_info->hoRenewal == VOS_TRUE)
      {
        // This case will occur when re-requesting AddTs during BT Coex
        hdd_status = SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  "%s: %d: ac:%d num_flows:%d",__func__, __LINE__,
                  ac,pACInfo->num_flows[pACInfo->tspec_pending - 1]);
        pACInfo->num_flows[pACInfo->tspec_pending - 1]--;
        inform_hdd = VOS_TRUE;
        break;
      }
   default:
      inform_hdd = VOS_FALSE;
      break;
   }
   if(inform_hdd)
   {
      //notify HDD, only the requested Flow, other Flows running on the AC stay 
      // intact
      if(!flow_info->hoRenewal)
      {
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1],
                                hdd_status,
                                flow_info->QosFlowID);
      }
      else
      {
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1],
                                SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
                                flow_info->QosFlowID);
      }
      //delete the entry from Flow List
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Deleting entry at %p with flowID %d",
                __func__, __LINE__,
                flow_info, flow_info->QosFlowID);
      csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
      // reclaim the memory
      vos_mem_free(flow_info);
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosAddTsSuccessFnp() - Utility function (pointer), 
  if the Addts request was for for an flow setup request, notify HDD for success
  for the flow & notify all the other flows running on the same AC that QoS 
  params got modified
  if the Addts request was for downgrading of QoS params because of an flow 
  release requested on the AC, delete the entry from Flow list & notify HDD 
  if the Addts request was for change of QoS params because of an flow 
  modification requested on the AC, delete the old entry from Flow list & notify 
  HDD for success for the flow & notify all the other flows running on the same 
  AC that QoS params got modified
  \param pMac - Pointer to the global MAC parameter structure.
  \param pEntry - Pointer to an entry in the flow_list(i.e. tListElem structure)
  
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosAddTsSuccessFnp(tpAniSirGlobal pMac, tListElem *pEntry)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosFlowInfoEntry *flow_info = NULL;
   v_BOOL_t inform_hdd = VOS_FALSE;
   v_BOOL_t delete_entry = VOS_FALSE;
   sme_QosStatusType hdd_status = SME_QOS_STATUS_SETUP_FAILURE_RSP;
   sme_QosEdcaAcType ac;
   eHalStatus pmc_status = eHAL_STATUS_FAILURE;
   tCsrRoamModifyProfileFields modifyProfileFields;

   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Entry is NULL",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
      return eHAL_STATUS_FAILURE;
   }
   flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
   ac = flow_info->ac_type;
   pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
   pACInfo = &pSession->ac_info[ac];
   if(flow_info->tspec_mask != pACInfo->tspec_pending)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: No need to notify the HDD, the ADDTS "
                "success is not for index = %d of the AC = %d",
                __func__, __LINE__,
                flow_info->tspec_mask, ac);
      return eHAL_STATUS_SUCCESS;
   }
   switch(flow_info->reason)
   {
   case SME_QOS_REASON_SETUP:
      hdd_status = SME_QOS_STATUS_SETUP_SUCCESS_IND;
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
      delete_entry = VOS_FALSE;
      inform_hdd = VOS_TRUE;
      // check if App is looking for APSD
      if(pACInfo->requested_QoSInfo[pACInfo->tspec_pending - 1].ts_info.psb)
      {
         // notify PMC as App is looking for APSD. If we already requested
         // then we don't need to do anything
         if(!pSession->uapsdAlreadyRequested)
         {
            // this is the first flow to detect we need PMC in UAPSD mode
            pmc_status = pmcStartUapsd(pMac,
                                       sme_QosPmcStartUapsdCallback,
                                       pSession);
            // if PMC doesn't return success right away means it is yet to put
            // the module in BMPS state & later to UAPSD state
            if(eHAL_STATUS_FAILURE == pmc_status)
            {
               hdd_status = SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_SET_FAILED;
               // we need to always notify this case
               flow_info->hoRenewal = VOS_FALSE;
            }
            else if(eHAL_STATUS_PMC_PENDING == pmc_status)
            {
               // let other flows know PMC has been notified
               pSession->uapsdAlreadyRequested = VOS_TRUE;
            }
            // for any other pmc status we declare success
         }
      }
      break;
   case SME_QOS_REASON_RELEASE:
      pACInfo->num_flows[pACInfo->tspec_pending - 1]--;
      hdd_status = SME_QOS_STATUS_RELEASE_SUCCESS_RSP;
      inform_hdd = VOS_TRUE;
      delete_entry = VOS_TRUE;
      break;
   case SME_QOS_REASON_MODIFY:
      delete_entry = VOS_TRUE;
      inform_hdd = VOS_FALSE;
      break;
   case SME_QOS_REASON_MODIFY_PENDING:
      hdd_status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND;
      delete_entry = VOS_FALSE;
      flow_info->reason = SME_QOS_REASON_REQ_SUCCESS;
      inform_hdd = VOS_TRUE;
      //notify PMC if App is looking for APSD
      if(pACInfo->requested_QoSInfo[pACInfo->tspec_pending - 1].ts_info.psb)
      {
         // notify PMC as App is looking for APSD. If we already requested
         // then we don't need to do anything.
         if(!pSession->uapsdAlreadyRequested)
         {
            // this is the first flow to detect we need PMC in UAPSD mode
            pmc_status = pmcStartUapsd(pMac,
                                       sme_QosPmcStartUapsdCallback,
                                       pSession);
            // if PMC doesn't return success right away means it is yet to put
            // the module in BMPS state & later to UAPSD state
            if(eHAL_STATUS_FAILURE == pmc_status)
            {
               hdd_status = SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_SET_FAILED;
               // we need to always notify this case
               flow_info->hoRenewal = VOS_FALSE;
            }
            else if(eHAL_STATUS_PMC_PENDING == pmc_status)
            {
               // let other flows know PMC has been notified
               pSession->uapsdAlreadyRequested = VOS_TRUE;
            }
            // for any other pmc status we declare success
         }
      }
      else
      {
        if((pACInfo->num_flows[flow_info->tspec_mask - 1] == 1) && 
           (SME_QOS_TSPEC_MASK_BIT_1_2_SET != pACInfo->tspec_mask_status))
        {
          // this is the only TSPEC active on this AC
          // so indicate that we no longer require APSD
          pSession->apsdMask &= ~(1 << (SME_QOS_EDCA_AC_VO - ac));
          //Also update modifyProfileFields.uapsd_mask in CSR for consistency
          csrGetModifyProfileFields(pMac, flow_info->sessionId, &modifyProfileFields);
          modifyProfileFields.uapsd_mask = pSession->apsdMask; 
          csrSetModifyProfileFields(pMac, flow_info->sessionId, &modifyProfileFields);
          if(!pSession->apsdMask)
          {
             // this session no longer needs UAPSD
             // do any sessions still require UAPSD?
             if (!sme_QosIsUapsdActive())
             {
                // No sessions require UAPSD so turn it off
                // (really don't care when PMC stops it)
                (void)pmcStopUapsd(pMac);
             }
          }
        }
      }
      break;
   case SME_QOS_REASON_REQ_SUCCESS:
      hdd_status = SME_QOS_STATUS_SETUP_MODIFIED_IND;
      inform_hdd = VOS_TRUE;
   default:
      delete_entry = VOS_FALSE;
      break;
   }
   if(inform_hdd)
   {
      if(!flow_info->hoRenewal)
      {
      
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1],
                                hdd_status,
                                flow_info->QosFlowID);
      }
      else
      {
         //For downgrading purpose Hdd set WmmTspecValid to false during roaming
         //So need to set that flag we need to call the hdd in successful case.
         if(hdd_status == SME_QOS_STATUS_SETUP_SUCCESS_IND)
         {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       "%s calling hdd_wmm_smecallback during  roaming for ac = %d", __func__, ac);
             flow_info->QoSCallback(pMac, flow_info->HDDcontext,
                                    &pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1],
                                    hdd_status,
                                    flow_info->QosFlowID
                                    );
         }
         flow_info->hoRenewal = VOS_FALSE;
      }
   }

   if (pMac->roam.configParam.roamDelayStatsEnabled)
   {
       if (pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1].ts_info.up ==  SME_QOS_WMM_UP_VO ||
           pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1].ts_info.up ==  SME_QOS_WMM_UP_NC)
       {
           vos_record_roam_event(e_SME_VO_ADDTS_RSP, NULL, 0);
       }

       if (pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1].ts_info.up ==  SME_QOS_WMM_UP_VI ||
           pACInfo->curr_QoSInfo[pACInfo->tspec_pending - 1].ts_info.up ==  SME_QOS_WMM_UP_CL)
       {
           vos_record_roam_event(e_SME_VI_ADDTS_RSP, NULL, 0);
       }
   }

   if(delete_entry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Deleting entry at %p with flowID %d",
                __func__, __LINE__,
                flow_info, flow_info->QosFlowID);
      //delete the entry from Flow List
      csrLLRemoveEntry(&sme_QosCb.flow_list, pEntry, VOS_TRUE );
      // reclaim the memory
      vos_mem_free(flow_info);
   }

   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosIsRspPending() - Utility function to check if we are waiting 
  for an AddTS or reassoc response on some AC other than the given AC
  
  \param sessionId - Session we are interted in
  \param ac - Enumeration of the various EDCA Access Categories.
  
  \return boolean
  TRUE - Response is pending on an AC
  
  \sa
  
  --------------------------------------------------------------------------*/
static v_BOOL_t sme_QosIsRspPending(v_U8_t sessionId, sme_QosEdcaAcType ac)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType acIndex;
   v_BOOL_t status = VOS_FALSE;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   for(acIndex = SME_QOS_EDCA_AC_BE; acIndex < SME_QOS_EDCA_AC_MAX; acIndex++) 
   {
      if(acIndex == ac)
      {
         continue;
      }
      pACInfo = &pSession->ac_info[acIndex];
      if((pACInfo->tspec_pending) || (pACInfo->reassoc_pending))
      {
         status = VOS_TRUE;
         break;
      }
   }
   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_QosUpdateHandOff() - Function which can be called to update
   Hand-off state of SME QoS Session
  \param sessionId - session id
  \param updateHandOff - value True/False to update the handoff flag

  \sa

-------------------------------------------------------------------------*/
void sme_QosUpdateHandOff(v_U8_t sessionId,
                          v_BOOL_t updateHandOff)
{
   sme_QosSessionInfo *pSession;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
             "%s: %d: handoffRequested %d updateHandOff %d",
             __func__, __LINE__,pSession->handoffRequested,
             updateHandOff);

   pSession->handoffRequested = updateHandOff;

}

/*--------------------------------------------------------------------------
  \brief sme_QosIsUapsdActive() - Function which can be called to determine
  if any sessions require PMC to be in U-APSD mode.
  \return boolean
  
  Returns true if at least one session required PMC to be in U-APSD mode
  Returns false if no sessions require PMC to be in U-APSD mode
  
  \sa
  
  --------------------------------------------------------------------------*/
static v_BOOL_t sme_QosIsUapsdActive(void)
{
   sme_QosSessionInfo *pSession;
   v_U8_t sessionId;
   for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; ++sessionId)
   {
      pSession = &sme_QosCb.sessionInfo[sessionId];
      if ((pSession->sessionActive) && (pSession->apsdMask))
      {
         return VOS_TRUE;
      }
   }
   // no active sessions have U-APSD active
   return VOS_FALSE;
}
/*--------------------------------------------------------------------------
  \brief sme_QosPmcFullPowerCallback() - Callback function registered with PMC 
  to notify SME-QoS when it puts the chip into full power
  
  \param callbackContext - The context passed to PMC during pmcRequestFullPower
  call.
  \param status - eHalStatus returned by PMC.
  
  \return None
  
  \sa
  
  --------------------------------------------------------------------------*/
void sme_QosPmcFullPowerCallback(void *callbackContext, eHalStatus status)
{
   sme_QosSessionInfo *pSession = callbackContext;
   if(HAL_STATUS_SUCCESS(status))
   {
      (void)sme_QosProcessBufferedCmd(pSession->sessionId);
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: PMC failed to put the chip in Full power",
                __func__, __LINE__);
      //ASSERT
      VOS_ASSERT(0);
   }
}

/*--------------------------------------------------------------------------
  \brief sme_QosPmcStartUAPSDCallback() - Callback function registered with PMC 
  to notify SME-QoS when it puts the chip into UAPSD mode
  
  \param callbackContext - The context passed to PMC during pmcStartUapsd call.
  \param status - eHalStatus returned by PMC.
  
  \return None
  
  \sa
  
  --------------------------------------------------------------------------*/
void sme_QosPmcStartUapsdCallback(void *callbackContext, eHalStatus status)
{
   sme_QosSessionInfo *pSession = callbackContext;
   // NOTE WELL
   //
   // In the orignal QoS design the TL module was responsible for
   // the generation of trigger frames.  When that design was in
   // use, we had to queue up any flows which were waiting for PMC
   // since we didn't want to notify HDD until PMC had changed to
   // UAPSD state.  Otherwise HDD would provide TL with the trigger
   // frame parameters, and TL would start trigger frame generation
   // before PMC was ready.  The flows were queued in various places
   // throughout this module, and they were dequeued here following
   // a successful transition to the UAPSD state by PMC.
   //
   // In the current QoS design the Firmware is responsible for the
   // generation of trigger frames, but the parameters are still
   // provided by TL via HDD.  The Firmware will be notified of the
   // change to UAPSD state directly by PMC, at which time it will be
   // responsible for the generation of trigger frames. Therefore
   // where we used to queue up flows waiting for PMC to transition
   // to the UAPSD state, we now always transition directly to the
   // "success" state so that HDD will immediately provide the trigger
   // frame parameters to TL, who will in turn plumb them down to the
   // Firmware.  That way the Firmware will have the trigger frame
   // parameters when it needs them
   // just note that there is no longer an outstanding request
   pSession->uapsdAlreadyRequested = VOS_FALSE;
}
/*--------------------------------------------------------------------------
  \brief sme_QosPmcCheckRoutine() - Function registered with PMC to check with 
  SME-QoS whenever the device is about to enter one of the power 
  save modes. PMC runs a poll with all the registered modules if device can 
  enter powersave mode or remain in full power  
  
  \param callbackContext - The context passed to PMC during registration through
  pmcRegisterPowerSaveCheck.
  \return boolean
  
  SME-QOS returns PMC true or false respectively if it wants to vote for 
  entering power save or not
  
  \sa
  
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosPmcCheckRoutine(void *callbackContext)
{
   sme_QosSessionInfo *pSession;
   v_U8_t sessionId;
   for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; ++sessionId)
   {
      pSession = &sme_QosCb.sessionInfo[sessionId];
      if ((pSession->sessionActive) &&
          (!pSession->readyForPowerSave))
      {
         return VOS_FALSE;
      }
   }
   // all active sessions have voted for powersave
   return VOS_TRUE;
}
/*--------------------------------------------------------------------------
  \brief sme_QosPmcDeviceStateUpdateInd() - Callback function registered with 
  PMC to notify SME-QoS when it changes the power state
  
  \param callbackContext - The context passed to PMC during registration 
  through pmcRegisterDeviceStateUpdateInd.
  \param pmcState - Current power state that PMC moved into.
  
  \return None
  
  \sa
  
  --------------------------------------------------------------------------*/
void sme_QosPmcDeviceStateUpdateInd(void *callbackContext, tPmcState pmcState)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( callbackContext );
   //check all the entries in Flow list for non-zero service interval, which will
   //tell us if we need to notify HDD when PMC is out of UAPSD mode or going 
   // back to UAPSD mode
   switch(pmcState)
   {
   case FULL_POWER:
      status = sme_QosProcessOutOfUapsdMode(pMac);
      break;
   case UAPSD:
      status = sme_QosProcessIntoUapsdMode(pMac);
      break;
   default:
      status = eHAL_STATUS_SUCCESS;
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                "%s: %d: nothing to process in PMC state %s (%d)",
                __func__, __LINE__,
                sme_PmcStatetoString(pmcState), pmcState);
   }
   if(!HAL_STATUS_SUCCESS(status))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: ignoring Device(PMC) state change to %s (%d)",
                __func__, __LINE__,
                sme_PmcStatetoString(pmcState), pmcState);
   }

}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessOutOfUapsdMode() - Function to notify HDD when PMC 
  notifies SME-QoS that it moved out of UAPSD mode to FULL power
  
  \param pMac - Pointer to the global MAC parameter structure.
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessOutOfUapsdMode(tpAniSirGlobal pMac)
{
   sme_QosSessionInfo *pSession;
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;
   
   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: %d: Flow List empty, can't search",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
      //only notify the flows which already successfully setup UAPSD
      if((flow_info->QoSInfo.max_service_interval ||
          flow_info->QoSInfo.min_service_interval) &&
         (SME_QOS_REASON_REQ_SUCCESS == flow_info->reason))
      {
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pSession->ac_info[flow_info->ac_type].curr_QoSInfo[flow_info->tspec_mask - 1],
                                SME_QOS_STATUS_OUT_OF_APSD_POWER_MODE_IND,
                                flow_info->QosFlowID);
      }
      pEntry = pNextEntry;
   }
   return eHAL_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------
  \brief sme_QosProcessIntoUapsdMode() - Function to notify HDD when PMC 
  notifies SME-QoS that it is moving into UAPSD mode 
  
  \param pMac - Pointer to the global MAC parameter structure.
  \return eHalStatus
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_QosProcessIntoUapsdMode(tpAniSirGlobal pMac)
{
   sme_QosSessionInfo *pSession;
   tListElem *pEntry= NULL, *pNextEntry = NULL;
   sme_QosFlowInfoEntry *flow_info = NULL;

   pEntry = csrLLPeekHead( &sme_QosCb.flow_list, VOS_FALSE );
   if(!pEntry)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Flow List empty, can't search",
                __func__, __LINE__);
      return eHAL_STATUS_FAILURE;
   }
   while( pEntry )
   {
      pNextEntry = csrLLNext( &sme_QosCb.flow_list, pEntry, VOS_FALSE );
      flow_info = GET_BASE_ADDR( pEntry, sme_QosFlowInfoEntry, link );
      pSession = &sme_QosCb.sessionInfo[flow_info->sessionId];
      //only notify the flows which already successfully setup UAPSD
      if( (flow_info->QoSInfo.ts_info.psb) &&
         (SME_QOS_REASON_REQ_SUCCESS == flow_info->reason) )
      {
         flow_info->QoSCallback(pMac, flow_info->HDDcontext, 
                                &pSession->ac_info[flow_info->ac_type].curr_QoSInfo[flow_info->tspec_mask - 1],
                                SME_QOS_STATUS_INTO_APSD_POWER_MODE_IND,
                                flow_info->QosFlowID);
      }
      pEntry = pNextEntry;
   }
   return eHAL_STATUS_SUCCESS;
}

void sme_QosCleanupCtrlBlkForHandoff(tpAniSirGlobal pMac, v_U8_t sessionId)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosEdcaAcType ac;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      pACInfo = &pSession->ac_info[ac];
      vos_mem_zero(pACInfo->curr_QoSInfo, 
                   sizeof(sme_QosWmmTspecInfo) * SME_QOS_TSPEC_INDEX_MAX);
      vos_mem_zero(pACInfo->requested_QoSInfo, 
                   sizeof(sme_QosWmmTspecInfo) * SME_QOS_TSPEC_INDEX_MAX);
      pACInfo->num_flows[0] = 0;
      pACInfo->num_flows[1] = 0;
      pACInfo->reassoc_pending = VOS_FALSE;
      pACInfo->tspec_mask_status = 0;
      pACInfo->tspec_pending = VOS_FALSE;
      pACInfo->hoRenewal = VOS_FALSE;
      pACInfo->prev_state = SME_QOS_LINK_UP;
   }
}

/*--------------------------------------------------------------------------
  \brief sme_QosIsTSInfoAckPolicyValid() - The SME QoS API exposed to HDD to 
  check if TS info ack policy field can be set to "HT-immediate block acknowledgement" 
  
  \param pMac - The handle returned by macOpen.
  \param pQoSInfo - Pointer to sme_QosWmmTspecInfo which contains the WMM TSPEC
                    related info, provided by HDD
  \param sessionId - sessionId returned by sme_OpenSession.
  
  \return VOS_TRUE - Current Association is HT association and so TS info ack policy
                     can be set to "HT-immediate block acknowledgement"
  
  \sa
  
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosIsTSInfoAckPolicyValid(tpAniSirGlobal pMac,
    sme_QosWmmTspecInfo * pQoSInfo,
    v_U8_t sessionId)
{
  tDot11fBeaconIEs *pIes = NULL;
  sme_QosSessionInfo *pSession;
  eHalStatus hstatus;
  if( !CSR_IS_SESSION_VALID( pMac, sessionId ) )
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
               "%s: %d: Session Id %d is invalid",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  pSession = &sme_QosCb.sessionInfo[sessionId];

  if( !pSession->sessionActive )
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
               "%s: %d: Session %d is inactive",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  if(!pSession->assocInfo.pBssDesc)
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
               "%s: %d: Session %d has an Invalid BSS Descriptor",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  hstatus = csrGetParsedBssDescriptionIEs(pMac,
                                          pSession->assocInfo.pBssDesc,
                                          &pIes);
  if(!HAL_STATUS_SUCCESS(hstatus))
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               "%s: %d: On session %d unable to parse BSS IEs",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  /* success means pIes was allocated */

  if(!pIes->HTCaps.present &&
     pQoSInfo->ts_info.ack_policy == SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK)
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: On session %d HT Caps aren't present but application set ack policy to HT ",
                __func__, __LINE__,
                sessionId);
      
      vos_mem_free(pIes);
      return VOS_FALSE;
  }

  vos_mem_free(pIes);
  return VOS_TRUE;
}

/*--------------------------------------------------------------------------
  \brief sme_QosTspecActive() - The SME QoS API exposed to HDD to
  check no of active Tspecs

  \param pMac - The handle returned by macOpen.
  \param ac - Determines type of Access Category
  \param sessionId - sessionId returned by sme_OpenSession.

  \return VOS_TRUE -When there is no error with pSession

  \sa
  --------------------------------------------------------------------------*/
v_BOOL_t sme_QosTspecActive(tpAniSirGlobal pMac,
    WLANTL_ACEnumType ac, v_U8_t sessionId, v_U8_t *pActiveTspec)
{
  sme_QosSessionInfo *pSession = NULL;
  sme_QosACInfo *pACInfo = NULL;

  if( !CSR_IS_SESSION_VALID( pMac, sessionId ) )
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               "%s: %d: Session Id %d is invalid",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  pSession = &sme_QosCb.sessionInfo[sessionId];

  if (NULL == pSession)
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
         "%s: %d pSession not found sessionId:%d",__func__,__LINE__,sessionId);
      return VOS_FALSE;
  }

  if( !pSession->sessionActive )
  {
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               "%s: %d: Session %d is inactive",
               __func__, __LINE__,
               sessionId);
     return VOS_FALSE;
  }

  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: %d: Session %d is active", __func__, __LINE__, sessionId);

  pACInfo = &pSession->ac_info[ac];

  // Does this AC have QoS active?
  if( SME_QOS_QOS_ON == pACInfo->curr_state )
  {
     // Yes, QoS is active on this AC
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: %d: On session %d AC %d has QoS active",
               __func__, __LINE__, sessionId, ac);

     // Are any TSPECs active?
     if( pACInfo->tspec_mask_status )
     {
         // Yes, at least 1 TSPEC is active.  Are they both active?
         if( SME_QOS_TSPEC_MASK_BIT_1_2_SET == pACInfo->tspec_mask_status )
         {
             //both TSPECS are active
             *pActiveTspec = 2;
         }
         else
         {
             // only one TSPEC is active
             *pActiveTspec = 1;
         }
     }
     else
     {
        *pActiveTspec = 0;
     }
  }
  else
  {
    // Hardcoding value to INVALID_TSPEC (invalid non-zero in this context,
    // valid values are 0,1,2) to indicate the caller not to update UAPSD
    // parameters as QOS is not active

     *pActiveTspec = INVALID_TSPEC;
     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: %d: On session %d AC %d has no QoS active",
               __func__, __LINE__, sessionId, ac);
  }

  return VOS_TRUE;
}

v_BOOL_t sme_QosValidateRequestedParams(tpAniSirGlobal pMac,
    sme_QosWmmTspecInfo * pQoSInfo,
    v_U8_t sessionId)
{
   v_BOOL_t rc = VOS_FALSE;

   do
   {
      if(SME_QOS_WMM_TS_DIR_RESV == pQoSInfo->ts_info.direction) break;
      if(!sme_QosIsTSInfoAckPolicyValid(pMac, pQoSInfo, sessionId)) break;

      rc = VOS_TRUE;
   }while(0);
   return rc;
}

static eHalStatus qosIssueCommand( tpAniSirGlobal pMac, v_U8_t sessionId,
                                   eSmeCommandType cmdType, sme_QosWmmTspecInfo * pQoSInfo,
                                   sme_QosEdcaAcType ac, v_U8_t tspec_mask )
{
    eHalStatus status = eHAL_STATUS_RESOURCES;
    tSmeCmd *pCommand = NULL;
    do
    {
        pCommand = smeGetCommandBuffer( pMac );
        if ( !pCommand )
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: %d: fail to get command buffer for command %d",
                         __func__, __LINE__, cmdType);
            break;
        }
        pCommand->command = cmdType;
        pCommand->sessionId = sessionId;
        switch ( cmdType )
        {
        case eSmeCommandAddTs:
            if( pQoSInfo )
            {
                status = eHAL_STATUS_SUCCESS;
                pCommand->u.qosCmd.tspecInfo = *pQoSInfo;
                pCommand->u.qosCmd.ac = ac;
            }
            else
            {
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: %d: NULL pointer passed",
                         __func__, __LINE__);
               status = eHAL_STATUS_INVALID_PARAMETER;
            }
            break;
        case eSmeCommandDelTs:
            status = eHAL_STATUS_SUCCESS;
            pCommand->u.qosCmd.ac = ac;
            pCommand->u.qosCmd.tspec_mask = tspec_mask;
            break;
        default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: invalid command type %d",
                      __func__, __LINE__, cmdType );
            status = eHAL_STATUS_INVALID_PARAMETER;
            break;
        }
    } while( 0 );
    if( HAL_STATUS_SUCCESS( status ) && pCommand )
    {
        smePushCommand( pMac, pCommand, eANI_BOOLEAN_FALSE );
    }
    else if( pCommand )
    {
        qosReleaseCommand( pMac, pCommand );
    }
    return( status );
}
tANI_BOOLEAN qosProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fRemoveCmd = eANI_BOOLEAN_TRUE;
    do
    {
        switch ( pCommand->command )
        {
        case eSmeCommandAddTs:
            status = sme_QosAddTsReq( pMac, (v_U8_t)pCommand->sessionId, &pCommand->u.qosCmd.tspecInfo, pCommand->u.qosCmd.ac);
            if( HAL_STATUS_SUCCESS( status ) )
            {
                fRemoveCmd = eANI_BOOLEAN_FALSE;
                status = SME_QOS_STATUS_SETUP_REQ_PENDING_RSP;
            }
            break;
        case eSmeCommandDelTs:
            status = sme_QosDelTsReq( pMac, (v_U8_t)pCommand->sessionId, pCommand->u.qosCmd.ac, pCommand->u.qosCmd.tspec_mask );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                fRemoveCmd = eANI_BOOLEAN_FALSE;
            }
            break;
        default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                      "%s: %d: invalid command type %d",
                      __func__, __LINE__, pCommand->command );
            break;
        }//switch
    } while(0);
    return( fRemoveCmd );
}

/*
  sme_QosTriggerUapsdChange
  Invoked by BTC when UAPSD bypass is enabled or disabled
  We, in turn, must disable or enable UAPSD on all flows as appropriate
  That may require us to re-add TSPECs or to reassociate
*/
sme_QosStatusType sme_QosTriggerUapsdChange( tpAniSirGlobal pMac )
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   v_U8_t ac, tspec1 = 0, tspec2 = 0; 
   v_U8_t uapsd_mask;
   tDot11fBeaconIEs *pIesLocal;
   v_U8_t acm_mask;
   v_BOOL_t fIsUapsdNeeded;
   v_U8_t sessionId;
   v_BOOL_t addtsWhenACMNotSet = CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(pMac);
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: Invoked",
             __func__, __LINE__);
   for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; ++sessionId)
   {
      pSession = &sme_QosCb.sessionInfo[sessionId];
      if( !pSession->sessionActive )
      {
         continue;
      }
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: %d: Session %d is active",
                __func__, __LINE__,
                sessionId);
      if( HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSession->assocInfo.pBssDesc, &pIesLocal)) )
      {
         // get the ACM mask
         acm_mask = sme_QosGetACMMask(pMac, pSession->assocInfo.pBssDesc, pIesLocal);
         vos_mem_free(pIesLocal);
         // get the uapsd mask for this session
         uapsd_mask = pSession->apsdMask;
         // unmask the bits with ACM on to avoid reassoc on them 
         uapsd_mask &= ~acm_mask;
         // iterate through the ACs to determine if we need to re-add any TSPECs
         for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
         {
            pACInfo = &pSession->ac_info[ac];
            // Does this AC have QoS active?
            if( SME_QOS_QOS_ON == pACInfo->curr_state )
            {
               // Yes, QoS is active on this AC
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                         "%s: %d: On session %d AC %d has QoS active",
                         __func__, __LINE__,
                         sessionId, ac);
               // Does this AC require ACM?
               if(( acm_mask & (1 << (SME_QOS_EDCA_AC_VO - ac)) ) || addtsWhenACMNotSet )
               {
                  // Yes, so we need to re-add any TSPECS
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                            "%s: %d: On session %d AC %d has ACM enabled",
                            __func__, __LINE__,
                            sessionId, ac);
                  // Are any TSPECs active?
                  if( pACInfo->tspec_mask_status )
                  {
                     // Yes, at least 1 TSPEC is active.  Are they both active?
                     if( SME_QOS_TSPEC_MASK_BIT_1_2_SET == pACInfo->tspec_mask_status )
                     {
                        //both TSPECS are active
                        tspec1 = SME_QOS_TSPEC_MASK_BIT_1_SET;
                        tspec2 = SME_QOS_TSPEC_MASK_BIT_2_SET;
                     }
                     else
                     {
                        // only one TSPEC is active, get its mask
                        tspec1 = SME_QOS_TSPEC_MASK_BIT_1_2_SET & pACInfo->tspec_mask_status;
                     }
                     // Does TSPEC 1 really require UAPSD?
                     fIsUapsdNeeded = (v_BOOL_t)(pACInfo->curr_QoSInfo[tspec1 - 1].ts_info.psb);
                     //double check whether we need to do anything
                     if( fIsUapsdNeeded )
                     {
                        pACInfo->requested_QoSInfo[tspec1 - 1] = 
                           pACInfo->curr_QoSInfo[tspec1 - 1];
                        sme_QosReRequestAddTS( pMac, sessionId,
                                               &pACInfo->requested_QoSInfo[tspec1 - 1],
                                               ac,
                                               tspec1 );
                     }
                     // Is TSPEC 2 active?
                     if( tspec2 )
                     {
                        // Does TSPEC 2 really require UAPSD?
                        fIsUapsdNeeded = (v_BOOL_t)(pACInfo->curr_QoSInfo[tspec2 - 1].ts_info.psb);
                        if( fIsUapsdNeeded )
                        {
                           //No need to inform HDD
                           //pACInfo->hoRenewal = VOS_TRUE;
                           pACInfo->requested_QoSInfo[tspec2 - 1] = 
                              pACInfo->curr_QoSInfo[tspec2 - 1];
                           sme_QosReRequestAddTS( pMac, sessionId,
                                                  &pACInfo->requested_QoSInfo[tspec2 - 1],
                                                  ac,
                                                  tspec2);
                        }
                     }
                  }
                  else
                  {
                     // QoS is set, ACM is on, but no TSPECs -- inconsistent state
                     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                               "%s: %d: On session %d AC %d has QoS enabled and ACM is set, but no TSPEC",
                               __func__, __LINE__,
                               sessionId, ac);
                     VOS_ASSERT(0);
                  }
               }
               else
               {
                  //Since ACM bit is not set, there should be only one QoS information for both directions.
                  fIsUapsdNeeded = (v_BOOL_t)(pACInfo->curr_QoSInfo[0].ts_info.psb);
                  if(fIsUapsdNeeded)
                  {
                     // we need UAPSD on this AC (and we may not currently have it)
                     uapsd_mask |= 1 << (SME_QOS_EDCA_AC_VO - ac);
                     VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                               "%s: %d: On session %d AC %d has ACM disabled, uapsd mask now 0x%X",
                               __func__, __LINE__,
                               sessionId, ac, uapsd_mask);
                  }
               }
            }
         }
         // do we need to reassociate?
         if(uapsd_mask)
         {
            tCsrRoamModifyProfileFields modifyProfileFields;
            //we need to do a reassoc on these AC 
            csrGetModifyProfileFields(pMac, sessionId, &modifyProfileFields);
            if( btcIsReadyForUapsd(pMac) )
            {
               modifyProfileFields.uapsd_mask = uapsd_mask;
            }
            else
            {  
               modifyProfileFields.uapsd_mask = 0;
            }
            //Do we need to inform HDD?
            if(!HAL_STATUS_SUCCESS(sme_QosRequestReassoc(pMac, sessionId, &modifyProfileFields, VOS_TRUE)))
            {
               //err msg
               VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: %d: On Session %d Reassoc failed",
                         __func__, __LINE__,
                         sessionId);
            }
         }
      }
      else
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "%s: %d: On Session %d failed to parse IEs",
                   __func__, __LINE__,
                   sessionId);
      }
   }
   // return status is ignored by BTC
   return SME_QOS_STATUS_SETUP_SUCCESS_IND;
}

/*
  sme_QoSUpdateUapsdBTEvent
  Invoked by BTC when there is a need to disable/enable UAPSD
  The driver in turn must come out of UAPSD and re-negotiate Tspec
  changed UAPSD settings on all active Tspecs.
*/
void sme_QoSUpdateUapsdBTEvent(tpAniSirGlobal pMac)
{
    sme_QosSessionInfo *pSession = NULL;
    sme_QosACInfo *pACInfo = NULL;
    v_U8_t ac, tspec1 = 0, tspec2 = 0;
    tDot11fBeaconIEs *pIesLocal;
    v_U8_t acm_mask;
    v_S7_t sessionId;
    v_BOOL_t addtsWhenACMNotSet = CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(pMac);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: Invoked", __func__, __LINE__);

    if (csrIsConcurrentSessionRunning(pMac)) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: %d Concurrent Sessions running, do nothing",
                  __func__,__LINE__);
        return;
    }

    sessionId = csrGetInfraSessionId(pMac);
    if (-1 == sessionId) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: %d Invalid sessionId",__func__,__LINE__);
        return;
    }

    pSession = &sme_QosCb.sessionInfo[sessionId];
    if (NULL == pSession) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: %d pSession not found sessionId:%d",__func__,__LINE__,sessionId);
        return;
    }

    if( !pSession->sessionActive )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: %d Session %d not active",__func__,__LINE__,sessionId);
        return;
    }

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
              "%s: %d: Session %d is active", __func__, __LINE__, sessionId);

    if( HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSession->assocInfo.pBssDesc, &pIesLocal)) )
    {
        // get the ACM mask
        acm_mask = sme_QosGetACMMask(pMac, pSession->assocInfo.pBssDesc, pIesLocal);
        vos_mem_free(pIesLocal);

        // iterate through the ACs to determine if we need to re-add any TSPECs
        for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++)
        {
            pACInfo = &pSession->ac_info[ac];

            // Does this AC have QoS active?
            if( SME_QOS_QOS_ON == pACInfo->curr_state )
            {
                // Yes, QoS is active on this AC
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                          "%s: %d: On session %d AC %d has QoS active",
                          __func__, __LINE__, sessionId, ac);
                // Does this AC require ACM?
                if(( acm_mask & (1 << (SME_QOS_EDCA_AC_VO - ac)) ) || addtsWhenACMNotSet )
                {
                    // Yes, so we need to re-add any TSPECS
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                              "%s: %d: On session %d AC %d has ACM enabled",
                               __func__, __LINE__, sessionId, ac);
                    // Are any TSPECs active?
                    if( pACInfo->tspec_mask_status )
                    {
                        // Yes, at least 1 TSPEC is active.  Are they both active?
                        if( SME_QOS_TSPEC_MASK_BIT_1_2_SET == pACInfo->tspec_mask_status )
                        {
                            //both TSPECS are active
                            tspec1 = SME_QOS_TSPEC_MASK_BIT_1_SET;
                            tspec2 = SME_QOS_TSPEC_MASK_BIT_2_SET;
                        }
                        else
                        {
                            // only one TSPEC is active, get its mask
                            tspec1 = SME_QOS_TSPEC_MASK_BIT_1_2_SET & pACInfo->tspec_mask_status;
                        }

                        if (pACInfo->curr_QoSInfo[tspec1 - 1].expec_psb_byapp )
                        {
                            pACInfo->requested_QoSInfo[tspec1 - 1] =
                            pACInfo->curr_QoSInfo[tspec1 - 1];

                            pACInfo->requested_QoSInfo[tspec1 - 1].ts_info.psb = 1;

                            if(!btcIsReadyForUapsd(pMac))
                                pACInfo->requested_QoSInfo[tspec1 - 1].ts_info.psb = 0;

                            sme_QosReRequestAddTS (pMac, sessionId,
                                                   &pACInfo->requested_QoSInfo[tspec1 - 1],
                                                   ac,
                                                   tspec1);
                        }

                        if (tspec2)
                        {
                            if (pACInfo->curr_QoSInfo[tspec2 - 1].expec_psb_byapp)
                            {
                                pACInfo->requested_QoSInfo[tspec2 - 1] =
                                pACInfo->curr_QoSInfo[tspec2 - 1];

                                pACInfo->requested_QoSInfo[tspec2 - 1].ts_info.psb = 1;

                                if(!btcIsReadyForUapsd(pMac))
                                    pACInfo->requested_QoSInfo[tspec2 - 1].ts_info.psb = 0;

                                sme_QosReRequestAddTS (pMac, sessionId,
                                                       &pACInfo->requested_QoSInfo[tspec2 - 1],
                                                       ac,
                                                       tspec2);
                            }
                        }
                    }
                    else
                    {
                        // QoS is ON, ACM is on, but no TSPECs -- Inconsistent state
                        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                                  "%s: %d: On session %d AC %d has QoS enabled and ACM is set, but no TSPEC",
                                  __func__, __LINE__,
                                  sessionId, ac);
                    }
                }
            }
        }
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: %d: On Session %d failed to parse IEs",
                  __func__, __LINE__,
                  sessionId);
    }
}

/*
    sme_QosReRequestAddTS to re-send AddTS for the combined QoS request
*/
static sme_QosStatusType sme_QosReRequestAddTS(tpAniSirGlobal pMac,
                                               v_U8_t sessionId,
                                               sme_QosWmmTspecInfo * pQoSInfo,
                                               sme_QosEdcaAcType ac,
                                               v_U8_t tspecMask)
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   sme_QosStatusType status = SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
   sme_QosCmdInfo  cmd;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: Invoked on session %d for AC %d TSPEC %d",
             __func__, __LINE__,
             sessionId, ac, tspecMask);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   pACInfo = &pSession->ac_info[ac];
   // need to vote off powersave for the duration of this request
   pSession->readyForPowerSave = VOS_FALSE;
   //call PMC's request for power function
   // AND
   //another check is added considering the flowing scenario
   //Addts reqest is pending on one AC, while APSD requested on another which 
   //needs a reassoc. Will buffer a request if Addts is pending on any AC, 
   //which will safegaurd the above scenario, & also won't confuse PE with back 
   //to back Addts or Addts followed by Reassoc
   if(sme_QosIsRspPending(sessionId, ac) || 
      ( eHAL_STATUS_PMC_PENDING == pmcRequestFullPower(pMac, sme_QosPmcFullPowerCallback, pSession, eSME_REASON_OTHER)))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: %d: On session %d buffering the AddTS request "
                   "for AC %d in state %d as Addts is pending "
                "on other AC or waiting for full power",
                __func__, __LINE__,
                sessionId, ac, pACInfo->curr_state);
      //buffer cmd
      cmd.command = SME_QOS_RESEND_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.resendCmdInfo.ac = ac;
      cmd.u.resendCmdInfo.tspecMask = tspecMask;
      cmd.u.resendCmdInfo.QoSInfo = *pQoSInfo;
      if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_FALSE)))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "%s: %d: On session %d unable to buffer the AddTS "
                   "request for AC %d TSPEC %d in state %d",
                   __func__, __LINE__,
                   sessionId, ac, tspecMask, pACInfo->curr_state);
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      }
      return SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
   }
   //get into the stat m/c to see if the request can be granted
   switch(pACInfo->curr_state)
   {
   case SME_QOS_QOS_ON:
      {
         //if ACM, send out a new ADDTS
         pACInfo->hoRenewal = VOS_TRUE;
         status = sme_QosSetup(pMac, sessionId, pQoSInfo, ac);
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: %d: sme_QosSetup returned in SME_QOS_QOS_ON state on "
                   "AC %d with status =%d",
                   __func__, __LINE__,
                   ac, status);
         if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP != status)
         {
            // we aren't waiting for a response from the AP
            // so vote powersave back on
            pSession->readyForPowerSave = VOS_TRUE;
         }
         if(SME_QOS_STATUS_SETUP_REQ_PENDING_RSP == status) 
         {
            status = SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
            pACInfo->tspec_pending = tspecMask;
         }
         else if((SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
                 (SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY == status) ||
                 (SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING == status))
         {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: UAPSD is setup already status = %d "
                      "returned by sme_QosSetup",
                      __func__, __LINE__,
                      status);  
         }
         else
         {
            //err msg
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                      "%s: %d: unexpected status = %d returned by sme_QosSetup",
                      __func__, __LINE__,
                      status);
         }
      }
      break;
   case SME_QOS_HANDOFF:
   case SME_QOS_REQUESTED:
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: Re-Add request in state = %d  buffer the request",
                __func__, __LINE__,
                pACInfo->curr_state);
      cmd.command = SME_QOS_RESEND_REQ;
      cmd.pMac = pMac;
      cmd.sessionId = sessionId;
      cmd.u.resendCmdInfo.ac = ac;
      cmd.u.resendCmdInfo.tspecMask = tspecMask;
      cmd.u.resendCmdInfo.QoSInfo = *pQoSInfo;
      if(!HAL_STATUS_SUCCESS(sme_QosBufferCmd(&cmd, VOS_FALSE)))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                   "%s: %d: couldn't buffer the readd request in state = %d",
                   __func__, __LINE__,
                   pACInfo->curr_state );
         // unable to buffer the request
         // nothing is pending so vote powersave back on
         pSession->readyForPowerSave = VOS_TRUE;
         return SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP;
      }
      status = SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP;
      break;
   case SME_QOS_CLOSED:
   case SME_QOS_INIT:
   case SME_QOS_LINK_UP:
   default:
      //print error msg, 
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                "%s: %d: ReAdd request in unexpected state = %d",
                __func__, __LINE__,
                pACInfo->curr_state );
      // unable to service the request
      // nothing is pending so vote powersave back on
      pSession->readyForPowerSave = VOS_TRUE;
      // ASSERT?
      break;
   }
   if((SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP == status) ||
      (SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY == status)) 
   {
      (void)sme_QosProcessBufferedCmd(sessionId);
   }
   return (status);
}

static void sme_QosInitACs(tpAniSirGlobal pMac, v_U8_t sessionId)
{
   sme_QosSessionInfo *pSession;
   sme_QosEdcaAcType ac;
   pSession = &sme_QosCb.sessionInfo[sessionId];
   for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
   {
      vos_mem_zero(&pSession->ac_info[ac], sizeof(sme_QosACInfo));
      sme_QosStateTransition(sessionId, ac, SME_QOS_INIT);
   }
}
static eHalStatus sme_QosRequestReassoc(tpAniSirGlobal pMac, tANI_U8 sessionId,
                                        tCsrRoamModifyProfileFields *pModFields,
                                        v_BOOL_t fForce )
{
   sme_QosSessionInfo *pSession;
   sme_QosACInfo *pACInfo;
   eHalStatus status;
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: %d: Invoked on session %d with UAPSD mask 0x%X",
             __func__, __LINE__,
             sessionId, pModFields->uapsd_mask);
   pSession = &sme_QosCb.sessionInfo[sessionId];
   status = csrReassoc(pMac, sessionId, pModFields, &pSession->roamID, fForce);
   if(HAL_STATUS_SUCCESS(status))
   {
      //Update the state to Handoff so subsequent requests are queued until
      // this one is finished
      sme_QosEdcaAcType ac;
      for(ac = SME_QOS_EDCA_AC_BE; ac < SME_QOS_EDCA_AC_MAX; ac++) 
      {
         pACInfo = &pSession->ac_info[ac];
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                   "%s: %d: AC[%d] is in state [%d]",
                   __func__, __LINE__,
                   ac, pACInfo->curr_state );
         // If it is already in HANDOFF state, don't do anything since we
         // MUST preserve the previous state and sme_QosStateTransition
         // will change the previous state
         if(SME_QOS_HANDOFF != pACInfo->curr_state)
         {
            sme_QosStateTransition(sessionId, ac, SME_QOS_HANDOFF);
         }
      }
   }
   return status;
}
static v_U32_t sme_QosAssignFlowId(void)
{
   v_U32_t flowId;
   flowId = sme_QosCb.nextFlowId;
   if (SME_QOS_MAX_FLOW_ID == flowId)
   {
      // The Flow ID wrapped.  This is obviously not a real life scenario
      // but handle it to keep the software test folks happy
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
                "%s: %d: Software Test made the flow counter wrap, "
                "QoS may no longer be functional",
                __func__, __LINE__);
      sme_QosCb.nextFlowId = SME_QOS_MIN_FLOW_ID;
   }
   else
   {
      sme_QosCb.nextFlowId++;
   }
   return flowId;
}

static v_U8_t sme_QosAssignDialogToken(void)
{
   v_U8_t token;
   token = sme_QosCb.nextDialogToken;
   if (SME_QOS_MAX_DIALOG_TOKEN == token)
   {
      // wrap is ok
      sme_QosCb.nextDialogToken = SME_QOS_MIN_DIALOG_TOKEN;
   }
   else
   {
      sme_QosCb.nextDialogToken++;
   }
   return token;
}
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
