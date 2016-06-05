/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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




#if !defined( __SMEINTERNAL_H )
#define __SMEINTERNAL_H


/**=========================================================================
  
  \file  smeInternal.h
  
  \brief prototype for SME internal structures and APIs used for SME and MAC
  
  
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
#include "csrLinkList.h"
#include "vos_diag_core_event.h"

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

// Mask can be only have one bit set
typedef enum eSmeCommandType 
{
    eSmeNoCommand = 0, 
    eSmeDropCommand,
    //CSR
    eSmeCsrCommandMask = 0x10000,   //this is not a command, it is to identify this is a CSR command
    eSmeCommandScan,
    eSmeCommandRoam, 
    eSmeCommandWmStatusChange, 
    eSmeCommandSetKey,
    eSmeCommandRemoveKey,
    eSmeCommandAddStaSession,
    eSmeCommandDelStaSession,
    eSmeCommandPnoReq,
    eSmeCommandMacSpoofRequest,
    eSmeCommandGetFrameLogRequest,
    eSmeCommandSetMaxTxPower,
#ifdef FEATURE_WLAN_TDLS
    //eSmeTdlsCommandMask = 0x80000,  //To identify TDLS commands <TODO>
    //These can be considered as csr commands. 
    eSmeCommandTdlsSendMgmt, 
    eSmeCommandTdlsAddPeer, 
    eSmeCommandTdlsDelPeer, 
    eSmeCommandTdlsLinkEstablish,
    eSmeCommandTdlsChannelSwitch, // tdlsoffchan
#endif
    eSmeCommandNanReq,
    //PMC
    eSmePmcCommandMask = 0x20000, //To identify PMC commands
    eSmeCommandEnterImps,
    eSmeCommandExitImps,
    eSmeCommandEnterBmps,
    eSmeCommandExitBmps,
    eSmeCommandEnterUapsd,
    eSmeCommandExitUapsd,
    eSmeCommandEnterWowl,
    eSmeCommandExitWowl,
    eSmeCommandEnterStandby,
    //QOS
    eSmeQosCommandMask = 0x40000,  //To identify Qos commands
    eSmeCommandAddTs,
    eSmeCommandDelTs,
#ifdef FEATURE_OEM_DATA_SUPPORT
    eSmeCommandOemDataReq = 0x80000, //To identify the oem data commands
#endif
    eSmeCommandRemainOnChannel,
    eSmeCommandNoAUpdate,
} eSmeCommandType;


typedef enum eSmeState
{
    SME_STATE_STOP,
    SME_STATE_START,
    SME_STATE_READY,
} eSmeState;

#define SME_IS_START(pMac)  (SME_STATE_STOP != (pMac)->sme.state)
#define SME_IS_READY(pMac)  (SME_STATE_READY == (pMac)->sme.state)

/* HDD Callback function */
typedef void(*pEncryptMsgRSPCb)(void *pUserData, void *infoParam);

typedef struct tagSmeEncMsgHddCbkInfo
{
   void *pUserData;
   pEncryptMsgRSPCb pEncMsgCbk;
}tSmeEncMsgHddCbkInfo;

typedef struct tagSmeStruct
{
    eSmeState state;
    vos_lock_t lkSmeGlobalLock;
    tANI_U32 totalSmeCmd;
    void *pSmeCmdBufAddr;
    tDblLinkList smeCmdActiveList;
    tDblLinkList smeCmdPendingList;
    tDblLinkList smeCmdFreeList;   //preallocated roam cmd list
    void (*pTxPerHitCallback) (void *pCallbackContext); /* callback for Tx PER hit to HDD */ 
    void *pTxPerHitCbContext;
    tVOS_CON_MODE currDeviceMode;
#ifdef FEATURE_WLAN_LPHB
    void (*pLphbIndCb) (void *pAdapter, void *indParam);
#endif /* FEATURE_WLAN_LPHB */
    //pending scan command list
    tDblLinkList smeScanCmdPendingList;
    //active scan command list
    tDblLinkList smeScanCmdActiveList;
#ifdef FEATURE_WLAN_CH_AVOID
    void (*pChAvoidNotificationCb) (void *pAdapter, void *indParam);
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
   /* HDD callback to be called after receiving Link Layer Stats Results IND from FW */
   void(*pLinkLayerStatsIndCallback)(void *callbackContext,
                                     int indType, void *pRsp, tANI_U8 *macAddr );
   void *pLinkLayerStatsCallbackContext;
#endif
#ifdef WLAN_FEATURE_EXTSCAN
   void (*pEXTScanIndCb) (void *, const tANI_U16, void *);
   /* Use this request ID while sending Full Scan Results */
   int  extScanStartReqId;
   void *pEXTScanCallbackContext;
#endif /* WLAN_FEATURE_EXTSCAN */
   tSmeEncMsgHddCbkInfo pEncMsgInfoParams;
   void (*pBtCoexTDLSNotification) (void *pAdapter, int);
   void (*nanCallback) (void*, tSirNanEvent*);

} tSmeStruct, *tpSmeStruct;


#endif //#if !defined( __SMEINTERNAL_H )
