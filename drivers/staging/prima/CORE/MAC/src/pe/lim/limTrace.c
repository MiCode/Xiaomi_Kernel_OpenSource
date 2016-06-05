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

/*

  \file  limTrace.c

  \brief implementation for trace related APIs

  \author Sunit Bhatia


  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

#include "aniGlobal.h" //for tpAniSirGlobal

#include "limTrace.h"
#include "limTimerUtils.h"
#include "vos_trace.h"


#ifdef LIM_TRACE_RECORD
tANI_U32 gMgmtFrameStats[14];

#define LIM_TRACE_MAX_SUBTYPES 14


static tANI_U8* __limTraceGetTimerString( tANI_U16 timerId )
{
    switch( timerId )
    {
        CASE_RETURN_STRING(eLIM_MIN_CHANNEL_TIMER);
        CASE_RETURN_STRING(eLIM_MAX_CHANNEL_TIMER);
        CASE_RETURN_STRING(eLIM_JOIN_FAIL_TIMER);
        CASE_RETURN_STRING(eLIM_AUTH_FAIL_TIMER);
        CASE_RETURN_STRING(eLIM_AUTH_RESP_TIMER);
        CASE_RETURN_STRING(eLIM_ASSOC_FAIL_TIMER);
        CASE_RETURN_STRING(eLIM_REASSOC_FAIL_TIMER);
        CASE_RETURN_STRING(eLIM_PRE_AUTH_CLEANUP_TIMER);
        CASE_RETURN_STRING(eLIM_HEART_BEAT_TIMER);
        CASE_RETURN_STRING(eLIM_BACKGROUND_SCAN_TIMER);
        CASE_RETURN_STRING(eLIM_KEEPALIVE_TIMER);
        CASE_RETURN_STRING(eLIM_CNF_WAIT_TIMER);
        CASE_RETURN_STRING(eLIM_AUTH_RSP_TIMER);
        CASE_RETURN_STRING(eLIM_UPDATE_OLBC_CACHE_TIMER);
        CASE_RETURN_STRING(eLIM_PROBE_AFTER_HB_TIMER);
        CASE_RETURN_STRING(eLIM_ADDTS_RSP_TIMER);
        CASE_RETURN_STRING(eLIM_CHANNEL_SWITCH_TIMER);
        CASE_RETURN_STRING(eLIM_LEARN_DURATION_TIMER);
        CASE_RETURN_STRING(eLIM_QUIET_TIMER);
        CASE_RETURN_STRING(eLIM_QUIET_BSS_TIMER);
        CASE_RETURN_STRING(eLIM_WPS_OVERLAP_TIMER);
#ifdef WLAN_FEATURE_VOWIFI_11R
        CASE_RETURN_STRING(eLIM_FT_PREAUTH_RSP_TIMER);
#endif
        CASE_RETURN_STRING(eLIM_PERIODIC_PROBE_REQ_TIMER);
#ifdef FEATURE_WLAN_ESE
        CASE_RETURN_STRING(eLIM_TSM_TIMER);
#endif
        CASE_RETURN_STRING(eLIM_DISASSOC_ACK_TIMER);
        CASE_RETURN_STRING(eLIM_DEAUTH_ACK_TIMER);
        CASE_RETURN_STRING(eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER);
        CASE_RETURN_STRING(eLIM_INSERT_SINGLESHOT_NOA_TIMER);
        CASE_RETURN_STRING(eLIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE);
        CASE_RETURN_STRING(eLIM_AUTH_RETRY_TIMER);
        default:
            return( "UNKNOWN" );
            break;
    }
}


static tANI_U8* __limTraceGetMgmtDropReasonString( tANI_U16 dropReason )
{

    switch( dropReason )
    {
        CASE_RETURN_STRING(eMGMT_DROP_INFRA_BCN_IN_IBSS);
        CASE_RETURN_STRING(eMGMT_DROP_INVALID_SIZE);
        CASE_RETURN_STRING(eMGMT_DROP_NON_SCAN_MODE_FRAME);
        CASE_RETURN_STRING(eMGMT_DROP_NOT_LAST_IBSS_BCN);
        CASE_RETURN_STRING(eMGMT_DROP_NO_DROP);
        CASE_RETURN_STRING(eMGMT_DROP_SCAN_MODE_FRAME);

        default:
            return( "UNKNOWN" );
            break;
    }
}



void limTraceInit(tpAniSirGlobal pMac)
{
    vosTraceRegister(VOS_MODULE_ID_PE, (tpvosTraceCb)&limTraceDump);
}




void limTraceDump(tpAniSirGlobal pMac, tpvosTraceRecord pRecord, tANI_U16 recIndex)
{

    static char *frameSubtypeStr[LIM_TRACE_MAX_SUBTYPES] =
    {
        "Association request",
        "Association response",
        "Reassociation request",
        "Reassociation response",
        "Probe request",
        "Probe response",
        NULL,
        NULL,
        "Beacon",
        "ATIM",
        "Disassocation",
        "Authentication",
        "Deauthentication",
        "Action"
    };


    switch (pRecord->code) {
        case TRACE_CODE_MLM_STATE:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s  %-30s(0x%x)",
               recIndex, pRecord->time, pRecord->session,
               "MLM State:",
               limTraceGetMlmStateString((tANI_U16)pRecord->data),
               pRecord->data);
            break;
        case TRACE_CODE_SME_STATE:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
               recIndex, pRecord->time, pRecord->session,
               "SME State:",
               limTraceGetSmeStateString((tANI_U16)pRecord->data),
               pRecord->data);
            break;
        case TRACE_CODE_TX_MGMT:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
               recIndex, pRecord->time, pRecord->session,
               "TX Mgmt:", frameSubtypeStr[pRecord->data], pRecord->data);
            break;

        case TRACE_CODE_RX_MGMT:
            if (LIM_TRACE_MAX_SUBTYPES <= LIM_TRACE_GET_SUBTYPE(pRecord->data))
            {
                limLog(pMac, LOG1, "Wrong Subtype - %d",
                    LIM_TRACE_GET_SUBTYPE(pRecord->data));
            }
            else
            {
                limLog(pMac,
                    LOG1, "%04d %012u S%d %-14s %-30s(%d) SN: %d ",
                    recIndex, pRecord->time, pRecord->session,
                    "RX Mgmt:",
                    frameSubtypeStr[LIM_TRACE_GET_SUBTYPE(pRecord->data)],
                    LIM_TRACE_GET_SUBTYPE(pRecord->data),
                    LIM_TRACE_GET_SSN(pRecord->data));
            }
            break;
        case TRACE_CODE_RX_MGMT_DROP:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(%d)",
                   recIndex, pRecord->time, pRecord->session,
                   "Drop RX Mgmt:",
                   __limTraceGetMgmtDropReasonString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;


        case TRACE_CODE_RX_MGMT_TSF:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s0x%x(%d)",
                   recIndex, pRecord->time, pRecord->session,
                   "RX Mgmt TSF:", " ", pRecord->data, pRecord->data);
            break;

        case TRACE_CODE_TX_COMPLETE:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s  %d",
                   recIndex, pRecord->time, pRecord->session,
                   "TX Complete", pRecord->data);
            break;

        case TRACE_CODE_TX_SME_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   "TX SME Msg:",
                   macTraceGetSmeMsgString((tANI_U16)pRecord->data),
                   pRecord->data );
            break;
        case TRACE_CODE_RX_SME_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   LIM_TRACE_GET_DEFRD_OR_DROPPED(pRecord->data)
                   ? "Def/Drp LIM Msg:": "RX Sme Msg:",
                   macTraceGetSmeMsgString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;

        case TRACE_CODE_TX_WDA_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   "TX WDA Msg:",
                   macTraceGetWdaMsgString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;

        case TRACE_CODE_RX_WDA_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   LIM_TRACE_GET_DEFRD_OR_DROPPED(pRecord->data)
                   ? "Def/Drp LIM Msg:": "RX WDA Msg:",
                   macTraceGetWdaMsgString((tANI_U16)pRecord->data),
                   pRecord->data );
            break;

        case TRACE_CODE_TX_LIM_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   "TX LIM Msg:",
                   macTraceGetLimMsgString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;
        case TRACE_CODE_RX_LIM_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   LIM_TRACE_GET_DEFRD_OR_DROPPED(pRecord->data)
                   ? "Def/Drp LIM Msg:": "RX LIM Msg",
                   macTraceGetLimMsgString((tANI_U16)pRecord->data),
                   pRecord->data );
            break;
        case TRACE_CODE_TX_CFG_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   "TX CFG Msg:",
                   macTraceGetCfgMsgString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;
        case TRACE_CODE_RX_CFG_MSG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   LIM_TRACE_GET_DEFRD_OR_DROPPED(pRecord->data)
                   ? "Def/Drp LIM Msg:": "RX CFG Msg:",
                   macTraceGetCfgMsgString
                   ((tANI_U16)MAC_TRACE_GET_MSG_ID(pRecord->data)),
                   pRecord->data);
            break;

        case TRACE_CODE_TIMER_ACTIVATE:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                   recIndex, pRecord->time, pRecord->session,
                   "Timer Actvtd",
                   __limTraceGetTimerString((tANI_U16)pRecord->data),
                   pRecord->data);
            break;
        case TRACE_CODE_TIMER_DEACTIVATE:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                  recIndex, pRecord->time, pRecord->session,
                  "Timer DeActvtd",
                  __limTraceGetTimerString((tANI_U16)pRecord->data),
                  pRecord->data);
            break;

        case TRACE_CODE_INFO_LOG:
            limLog(pMac, LOG1, "%04d %012u S%d %-14s %-30s(0x%x)",
                  recIndex, pRecord->time, pRecord->session,
                  "INFORMATION_LOG",
                  macTraceGetInfoLogString((tANI_U16)pRecord->data),
                  pRecord->data);
            break;
        default :
            limLog(pMac, LOG1, "%04d %012u S%d %-14s(%d) (0x%x)",
                  recIndex, pRecord->time, pRecord->session,
                  "Unknown Code", pRecord->code, pRecord->data);
            break;
    }
}


void macTraceMsgTx(tpAniSirGlobal pMac, tANI_U8 session, tANI_U32 data)
{

    tANI_U16 msgId = (tANI_U16)MAC_TRACE_GET_MSG_ID(data);
    tANI_U8 moduleId = (tANI_U8)MAC_TRACE_GET_MODULE_ID(data);

    switch(moduleId)
    {
        case SIR_LIM_MODULE_ID:
            if(msgId >= SIR_LIM_ITC_MSG_TYPES_BEGIN)
                macTrace(pMac, TRACE_CODE_TX_LIM_MSG, session, data);
            else
                macTrace(pMac, TRACE_CODE_TX_SME_MSG, session, data);
            break;
        case SIR_WDA_MODULE_ID:
            macTrace(pMac, TRACE_CODE_TX_WDA_MSG, session, data);
            break;
        case SIR_CFG_MODULE_ID:
            macTrace(pMac, TRACE_CODE_TX_CFG_MSG, session, data);
            break;
        default:
            macTrace(pMac, moduleId, session, data);
            break;
    }
}


void macTraceMsgTxNew(tpAniSirGlobal pMac, tANI_U8 module, tANI_U8 session, tANI_U32 data)
{
    tANI_U16 msgId = (tANI_U16)MAC_TRACE_GET_MSG_ID(data);
    tANI_U8 moduleId = (tANI_U8)MAC_TRACE_GET_MODULE_ID(data);

    switch(moduleId)
    {
        case SIR_LIM_MODULE_ID:
            if(msgId >= SIR_LIM_ITC_MSG_TYPES_BEGIN)
                macTraceNew(pMac, module, TRACE_CODE_TX_LIM_MSG, session, data);
            else
                macTraceNew(pMac, module, TRACE_CODE_TX_SME_MSG, session, data);
            break;
        case SIR_WDA_MODULE_ID:
            macTraceNew(pMac, module, TRACE_CODE_TX_WDA_MSG, session, data);
            break;
        case SIR_CFG_MODULE_ID:
            macTraceNew(pMac, module, TRACE_CODE_TX_CFG_MSG, session, data);
            break;
        default:
            macTrace(pMac, moduleId, session, data);
            break;
        }
}

/*
* bit31: Rx message defferred or not
* bit 0-15: message ID:
*/
void macTraceMsgRx(tpAniSirGlobal pMac, tANI_U8 session, tANI_U32 data)
{
    tANI_U16 msgId = (tANI_U16)MAC_TRACE_GET_MSG_ID(data);
    tANI_U8 moduleId = (tANI_U8)MAC_TRACE_GET_MODULE_ID(data);


    switch(moduleId)
    {
        case SIR_LIM_MODULE_ID:
            if(msgId >= SIR_LIM_ITC_MSG_TYPES_BEGIN)
                macTrace(pMac, TRACE_CODE_RX_LIM_MSG, session, data);
            else
                macTrace(pMac, TRACE_CODE_RX_SME_MSG, session, data);
            break;
        case SIR_WDA_MODULE_ID:
            macTrace(pMac, TRACE_CODE_RX_WDA_MSG, session, data);
            break;
        case SIR_CFG_MODULE_ID:
            macTrace(pMac, TRACE_CODE_RX_CFG_MSG, session, data);
            break;
        default:
            macTrace(pMac, moduleId, session, data);
            break;
        }
}



/*
* bit31: Rx message defferred or not
* bit 0-15: message ID:
*/
void macTraceMsgRxNew(tpAniSirGlobal pMac, tANI_U8 module, tANI_U8 session, tANI_U32 data)
{
    tANI_U16 msgId = (tANI_U16)MAC_TRACE_GET_MSG_ID(data);
    tANI_U8 moduleId = (tANI_U8)MAC_TRACE_GET_MODULE_ID(data);


    switch(moduleId)
    {
        case SIR_LIM_MODULE_ID:
            if(msgId >= SIR_LIM_ITC_MSG_TYPES_BEGIN)
                macTraceNew(pMac, module, TRACE_CODE_RX_LIM_MSG, session, data);
            else
                macTraceNew(pMac, module, TRACE_CODE_RX_SME_MSG, session, data);
            break;
        case SIR_WDA_MODULE_ID:
            macTraceNew(pMac, module, TRACE_CODE_RX_WDA_MSG, session, data);
            break;
        case SIR_CFG_MODULE_ID:
            macTraceNew(pMac, module, TRACE_CODE_RX_CFG_MSG, session, data);
            break;
        default:
            macTrace(pMac, moduleId, session, data);
            break;
        }
}



tANI_U8* limTraceGetMlmStateString( tANI_U32 mlmState )
{
    switch( mlmState )
    {
        CASE_RETURN_STRING( eLIM_MLM_OFFLINE_STATE);
        CASE_RETURN_STRING( eLIM_MLM_IDLE_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_PROBE_RESP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_PASSIVE_SCAN_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_JOIN_BEACON_STATE);
        CASE_RETURN_STRING( eLIM_MLM_JOINED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_BSS_STARTED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_AUTH_FRAME2_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_AUTH_FRAME3_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_AUTH_FRAME4_STATE);
        CASE_RETURN_STRING( eLIM_MLM_AUTH_RSP_TIMEOUT_STATE);
        CASE_RETURN_STRING( eLIM_MLM_AUTHENTICATED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ASSOC_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_REASSOC_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_ASSOCIATED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_REASSOCIATED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_LINK_ESTABLISHED_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ASSOC_CNF_STATE);
        CASE_RETURN_STRING( eLIM_MLM_LEARN_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADD_BSS_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_DEL_BSS_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADD_STA_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_DEL_STA_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_SET_BSS_KEY_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_SET_STA_KEY_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_SET_STA_BCASTKEY_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_ADDBA_RSP_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_REMOVE_BSS_KEY_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_REMOVE_STA_KEY_STATE);
        CASE_RETURN_STRING( eLIM_MLM_WT_SET_MIMOPS_STATE);
#if defined WLAN_FEATURE_VOWIFI_11R
        CASE_RETURN_STRING(eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE);
        CASE_RETURN_STRING(eLIM_MLM_WT_FT_REASSOC_RSP_STATE);
#endif
        CASE_RETURN_STRING(eLIM_MLM_P2P_LISTEN_STATE);
        default:
            return( "UNKNOWN" );
            break;
    }
}


tANI_U8* limTraceGetSmeStateString( tANI_U32 smeState )
{
    switch( smeState )
    {

    CASE_RETURN_STRING(eLIM_SME_OFFLINE_STATE);
    CASE_RETURN_STRING(eLIM_SME_IDLE_STATE);
    CASE_RETURN_STRING(eLIM_SME_SUSPEND_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_SCAN_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_JOIN_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_AUTH_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_ASSOC_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_REASSOC_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_REASSOC_LINK_FAIL_STATE);
    CASE_RETURN_STRING(eLIM_SME_JOIN_FAILURE_STATE);
    CASE_RETURN_STRING(eLIM_SME_ASSOCIATED_STATE);
    CASE_RETURN_STRING(eLIM_SME_REASSOCIATED_STATE);
    CASE_RETURN_STRING(eLIM_SME_LINK_EST_STATE);
    CASE_RETURN_STRING(eLIM_SME_LINK_EST_WT_SCAN_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_PRE_AUTH_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_DISASSOC_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_DEAUTH_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_START_BSS_STATE);
    CASE_RETURN_STRING(eLIM_SME_WT_STOP_BSS_STATE);
    CASE_RETURN_STRING(eLIM_SME_NORMAL_STATE);
    CASE_RETURN_STRING(eLIM_SME_CHANNEL_SCAN_STATE);
    CASE_RETURN_STRING(eLIM_SME_NORMAL_CHANNEL_SCAN_STATE);
    default:
        return( "UNKNOWN" );
        break;
    }
}






#endif
