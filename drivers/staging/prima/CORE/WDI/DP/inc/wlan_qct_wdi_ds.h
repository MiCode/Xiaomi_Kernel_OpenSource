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

#if !defined( __WLAN_QCT_WTI_DS_H )
#define __WLAN_QCT_WTI_DS_H

/**=========================================================================
 *     
 *       \file  wlan_qct_wdi_ds.h
 *          
 *       \brief define Dataservice API 
 *                               
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *  This file contains the external API exposed by the 
 *   wlan device abstarction layer module.
 *
 */


#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_pal_packet.h"
#include "wlan_qct_wdi.h"
   

typedef struct 
{
   wpt_uint32 txFlags;
   wpt_uint8 ac;
   wpt_uint8 isEapol:1; //0 - not eapol 1 - eapol
   wpt_uint8 isWai:1;   //WAPI 0 - not WAI 1 WAI 
   wpt_uint8 fdisableFrmXlt:1;   //0 - Let ADU do FT. 1 - bypass ADU FT
   wpt_uint8 qosEnabled:1; //0 - non-Qos 1 - Qos
   wpt_uint8 fenableWDS:1; //0 - not WDS 1 WDS
   wpt_uint8 reserved1:3;
   wpt_uint8 typeSubtype;
   wpt_uint8 fUP;
   wpt_uint8 fSTAMACAddress[6];
   wpt_uint8 addr2MACAddress[6];
   wpt_uint8 frmType;
   wpt_uint8 fStaType;
   wpt_uint8 fProtMgmtFrame;
   wpt_uint16 fPktlen;
   wpt_status txCompleteStatus;
   wpt_uint8  staIdx;
   wpt_uint32  txBdToken;
} WDI_DS_TxMetaInfoType;


typedef enum
{
  WDI_DS_OPCODE_INVALID         = 0,
  WDI_DS_OPCODE_QCUR_FWDBUF     = 1,
  WDI_DS_OPCODE_FWDBUF_FWDCUR   = 2,
  WDI_DS_OPCODE_QCUR            = 3,
  WDI_DS_OPCODE_FWDBUF_QUEUECUR = 4,
  WDI_DS_OPCODE_FWDBUF_DROPCUR  = 5,
  WDI_DS_OPCODE_FWDALL_DROPCUR  = 6,
  WDI_DS_OPCODE_FWDALL_QCUR     = 7,
  WDI_DS_OPCODE_TEARDOWN        = 8,
  WDI_DS_OPCODE_DROPCUR         = 9,
  WDI_DS_OPCODE_MAX
}WDI_DS_BAOpCodeEnumType;

#define WDI_DS_LOG_PKT_TYPE_LEN 4
typedef enum
{
  WDI_DS_PACKET_LOG = 1<<0,

  // Insert new values before this

  // If the value of WDI_DS_MAX LOG is increased please
  // make sure to change the data type of
  // WDI_DS_RxMetaInfoType.loggingData from wpt_uint8
  // to accommodate more values
  WDI_DS_MAX_LOG    = 1<<31
}WDI_DS_LoggingDataEnumType;

typedef struct 
{
   wpt_uint8 staId;
   wpt_uint8 addr3Idx;
   wpt_uint8 rxChannel;
   wpt_uint8 type:2;
   wpt_uint8 subtype:4;
   wpt_uint8 rfBand:2;

   wpt_uint16 rtsf:1;  //For beacon only. 1 ~V Riva TSF is bigger(later) than the one received
   wpt_uint16 bsf:1;   //1 Riva sends the last beacon, 0 not.
   wpt_uint16 unknownUcastPkt:1;   //1 ~V unicast frame received with unknown A2
   wpt_uint16 scan:1;   //1 frame received in scan state. 0 not.
   wpt_uint16 dpuSig:3;   //DPU signature
   wpt_uint16 ft:1;   //0~Wframe translation is not done. 1~Wdone
   wpt_uint16 ne:1;   //1 ~V frame is not encrypted OTA. This is for WAPI~Rs WAI packet.
   wpt_uint16 llcr:1;   // Has the LLC been stripped by H/W
   wpt_uint16 bcast:1;   //0 ~V unicast frame 1 ~V broadcast/multicast frame
   wpt_uint16 tid:4;
   wpt_uint16 reserved1:1;
   wpt_uint8 dpuFeedback;
   wpt_int8 snr;

   wpt_uint32 currentPktSeqNo:12;  /*current sequence number */
   wpt_uint32 ampdu_reorderOpcode:4;
   wpt_uint32 ampdu_reorderSlotIdx:6;
   wpt_uint32 ampdu_reorderFwdIdx:6;
   wpt_uint32 reserved3:4;

   wpt_uint16 amsdu_size;
   wpt_uint32 amsdu_asf:1;
   wpt_uint32 amsdu_esf:1;
   wpt_uint32 amsdu_lsf:1;
   wpt_uint32 amsdu_aef:1;
   wpt_uint32 reserved2:4;

   wpt_uint8 *mpduHeaderPtr;
   wpt_uint8 *mpduDataPtr;
   wpt_uint32  mpduLength;
   wpt_uint32  mpduHeaderLength;

   wpt_uint32  rateIndex;
   wpt_uint32  rxpFlags;
   wpt_uint32  mclkRxTimestamp;

   //Flow control frames
   wpt_uint8  fc;
   wpt_uint32 fcSTATxQStatus:16;
   wpt_uint32 fcSTAThreshIndMask:16;
   wpt_uint32 fcSTAPwrSaveStateMask:16;
   wpt_uint32 fcSTAValidMask:16;

   wpt_uint16 fcStaTxDisabledBitmap;
   wpt_uint8 fcSTATxQLen[12]; // one byte per STA. 
   wpt_uint8 fcSTACurTxRate[12]; // current Tx rate for each sta.

   wpt_uint64 replayCount;

   wpt_uint32 rssi0;
   wpt_uint32 rssi1;

#ifdef WLAN_FEATURE_11W
   wpt_uint32 rmf:1;
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   wpt_uint32 offloadScanLearn;
   wpt_uint32 roamCandidateInd;
#endif
#ifdef WLAN_FEATURE_EXTSCAN
   wpt_uint32 extscanBuffer;
#endif
   wpt_uint32 loggingData;
} WDI_DS_RxMetaInfoType;

typedef struct sPktMetaInfo
{
   union
   {
      WDI_DS_TxMetaInfoType txMetaInfo;
      WDI_DS_RxMetaInfoType rxMetaInfo;
   } u;
} WDI_DS_MetaInfoType;

typedef struct
{
   wpt_boolean active;
   wpt_uint64 logBuffAddress[MAX_NUM_OF_BUFFER];
   wpt_uint32 logBuffLength[MAX_NUM_OF_BUFFER];
   /* Log type i.e. Mgmt frame = 0, QXDM = 1, FW Mem dump = 2 */
   wpt_uint8   logType;
   /* Indicate if Last segment of log is received*/
   wpt_boolean done;
} WDI_DS_LoggingSessionType;

WPT_STATIC WPT_INLINE WDI_DS_RxMetaInfoType* WDI_DS_ExtractRxMetaData (wpt_packet *pFrame)
{
  WDI_DS_RxMetaInfoType * pRxMetadata =
    &((WDI_DS_MetaInfoType *)WPAL_PACKET_GET_METAINFO_POINTER(pFrame))->u.rxMetaInfo;
  return pRxMetadata;
}


WPT_STATIC WPT_INLINE WDI_DS_TxMetaInfoType* WDI_DS_ExtractTxMetaData (wpt_packet *pFrame)
{
        WDI_DS_TxMetaInfoType * pTxMetadata =
                &((WDI_DS_MetaInfoType *)WPAL_PACKET_GET_METAINFO_POINTER(pFrame))->u.txMetaInfo;
        return pTxMetadata;
}


typedef void (*WDI_DS_TxCompleteCallback)(void *pContext, wpt_packet *pFrame);
typedef void (*WDI_DS_RxPacketCallback) (void *pContext, wpt_packet *pFrame);
typedef void (*WDI_DS_TxFlowControlCallback)(void *pContext, wpt_uint8 ac_mask);
typedef void (*WDI_DS_RxLogCallback)(void);



/* DAL registration function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along 
 *  with the callback.
 *  pfnTxCompleteCallback:Callback function that is to be invoked to return 
 *  packets which have been transmitted.
 *  pfnRxPacketCallback:Callback function that is to be invoked to deliver 
 *  packets which have been received
 *  pfnTxFlowControlCallback:Callback function that is to be invoked to 
 *  indicate/clear congestion. 
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
WDI_Status WDI_DS_Register( void *pContext, 
  WDI_DS_TxCompleteCallback pfnTxCompleteCallback,
  WDI_DS_RxPacketCallback pfnRxPacketCallback, 
  WDI_DS_TxFlowControlCallback pfnTxFlowControlCallback,
  WDI_DS_RxLogCallback pfnRxLogCallback,
  void *pCallbackContext);



/* DAL Transmit function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  pFrame:Refernce to PAL frame.
 *  more: Does the invokee have more than one packet pending?
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxPacket(void *pContext,
  wpt_packet *pFrame,
  wpt_boolean more);
  
  
/* DAL Transmit Complete function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  ucTxResReq:TX resource number required by TL
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxComplete(void *pContext, wpt_uint32 ucTxResReq);

/* DAL Suspend Transmit function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxSuspend(void *pContext);


/* DAL Resume Transmit function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxResume(void *pContext);

/* DAL Get Reserved resource by STA 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  wdiResPool: MemPool, MGMT ot DATA
 *  staId: STA ID
 * Return Value: Number of reserved resouce count
 *
 */
wpt_uint32 WDI_DS_GetReservedResCountPerSTA(void *pContext,
                                                        WDI_ResPoolType wdiResPool,
                                                        wpt_uint8 staId);

/* DAL ADD STA into memPool
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  staId: STA ID
 * Return Value: SUCCESS or FAIL
 *
 */
WDI_Status WDI_DS_AddSTAMemPool(void *pContext, wpt_uint8 staIndex);

/* DAL Remove STA from memPool
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  staId: STA ID
 * Return Value: SUCCESS or FAIL
 *
 */
WDI_Status WDI_DS_DelSTAMemPool(void *pContext, wpt_uint8 staIndex);

/* DAL Set STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_SetStaIdxPerBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 staIdx);

/* DAL Get STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_GetStaIdxFromBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 *staIdx);

/* DAL Clear STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_ClearStaIdxPerBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 staIdx);

/* @brief: WDI_DS_GetTrafficStats
 * This function should be invoked to fetch the current stats
  * Parameters:
 *  pStats:Pointer to the collected stats
 *  len: length of buffer pointed to by pStats
 *  Return Status: None
 */
void WDI_DS_GetTrafficStats(WDI_TrafficStatsType** pStats, wpt_uint32 *len);

/* @brief: WDI_DS_DeactivateTrafficStats
 * This function should be invoked to deactivate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_DeactivateTrafficStats(void);

/* @brief: WDI_DS_ActivateTrafficStats
 * This function should be invoked to activate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_ActivateTrafficStats(void);

/* @brief: WDI_DS_ClearTrafficStats
 * This function should be invoked to clear all past stats
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_ClearTrafficStats(void);

void *WDI_DS_GetLoggingMbPhyAddr(void *pContext);
void *WDI_DS_GetLoggingMbAddr(void *pContext);
void *WDI_DS_GetLoggingSession(void *pContext);
#endif
