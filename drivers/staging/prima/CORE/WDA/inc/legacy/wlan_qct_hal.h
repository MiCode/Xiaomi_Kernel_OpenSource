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




#ifndef WLAN_QCT_HAL_H
#define WLAN_QCT_HAL_H
#include "vos_status.h"
#include "halTypes.h"
#ifndef PALTYPES_H__


/// unsigned 8-bit types
#define tANI_U8        v_U8_t
 
/// unsigned 16-bit types
#define tANI_U16    v_U16_t    

/// unsigned 32-bit types
#define tANI_U32    v_U32_t
 
/// signed 8-bit types
#define    tANI_S8        v_S7_t
 
/// signed 16-bit types
#define tANI_S16    v_S15_t
 
/// signed 32-bit types
#define tANI_S32    v_S31_t

#define eHalStatus    VOS_STATUS

#endif
#define QWLAN_HAL_DXE0_MASTERID  5

typedef struct sHalBdGeneric {
    /* 0x00 */
    // ENDIAN BEGIN
    tANI_U32 dpuRF : 8;
    tANI_U32 dpuSignature:3;     /* Signature on RA's DPU descriptor */
    tANI_U32 staSignature:3;
    tANI_U32 reserved : 14;
    tANI_U32 dpuNE : 1;
    tANI_U32 dpuNC : 1;
    tANI_U32 bdt : 2;                        /* BD type */
    // ENDIAN END

    /* 0x04 */
    // ENDIAN BEGIN
    tANI_U32 reserved1:32;                      
    // ENDIAN END


    /* 0x08 */
    // ENDIAN BEGIN
    tANI_U32 headPduIdx : 16;                /* Head PDU index */
    tANI_U32 tailPduIdx : 16;                /* Tail PDU index */
    // ENDIAN END

    /* 0x0c */
    // ENDIAN BEGIN
    tANI_U32 mpduHeaderLength : 8;           /* MPDU header length */
    tANI_U32 mpduHeaderOffset : 8;           /* MPDU header start offset */
    tANI_U32 mpduDataOffset : 9;             /* MPDU data start offset */
    tANI_U32 pduCount : 7;                   /* PDU count */
    // ENDIAN END

    /* 0x10 */
    // ENDIAN BEGIN
    tANI_U32 mpduLength : 16;                /* MPDU length */
    tANI_U32 reserved3:4;            /* DPU compression feedback */
    tANI_U32 tid : 4;                        /* Traffic identifier, tid */
    tANI_U32 rateIndex : 8;
    // ENDIAN END

    /* 0x14 */
    // ENDIAN BEGIN
    tANI_U32 dpuDescIdx : 8;
    tANI_U32 addr1Index : 8;  //A1 index after RxP binary search
    tANI_U32 addr2Index : 8;  //A2 index after RxP binary search
    tANI_U32 addr3Index : 8;  //A3 index after RxP binary search
    // ENDIAN END
//}__ani_attr_packed __ani_attr_aligned_4 tHalBdGeneric, *tpHalBdGeneric;
} tHalBdGeneric, *tpHalBdGeneric;


/*
 * PDU without BD
 */

typedef struct sHalPdu {
    tANI_U8 payload[124];
    tANI_U32 nextPduIdx;                     /* LSB 16 bits */
//} __ani_attr_packed __ani_attr_aligned_4 tHalPdu, *tpHalPdu;
} tHalPdu, *tpHalPdu;

/* UAPSD parameters passed per AC to HAL from TL */
typedef struct sUapsdInfo {
    tANI_U8  staidx;        // STA index
    tANI_U8  ac;            // Access Category
    tANI_U8  up;            // User Priority
    tANI_U32 srvInterval;   // Service Interval
    tANI_U32 susInterval;   // Suspend Interval
    tANI_U32 delayInterval; // Delay Interval
} tUapsdInfo, tpUapsdInfo;

#define HAL_TXBD_BDRATE_DEFAULT 0
#define HAL_TXBD_BDRATE_FIRST   1
#define HAL_TXBD_BDRATE_SECOND  2
#define HAL_TXBD_BDRATE_THIRD   3

#define HAL_FRAME_TYPE_MASK     0x30
#define HAL_FRAME_TYPE_OFFSET   0x4
#define HAL_FRAME_SUBTYPE_MASK  0x0F

#define HAL_TXBD_BD_SSN_FILL_HOST             0
#define HAL_TXBD_BD_SSN_FILL_DPU_NON_QOS    1
#define HAL_TXBD_BD_SSN_FILL_DPU_QOS        2

#define HAL_ACKPOLICY_ACK_REQUIRED        0
#define HAL_ACKPOLICY_ACK_NOTREQUIRED    1

#define HAL_BDRATE_BCDATA_FRAME            1
#define HAL_BDRATE_BCMGMT_FRAME            2
#define HAL_BDRATE_CTRL_FRAME            3
    
/* Default values for FillTx BD */
#define HAL_DEFAULT_UNICAST_ENABLED     1
#define HAL_RMF_DISABLED                 0
#define HAL_RMF_ENABLED                 1
#define HAL_NO_ENCRYPTION_DISABLED        0
#define HAL_NO_ENCRYPTION_ENABLED         1
    
#define WLANHAL_RX_BD_ADDR3_SELF_IDX      0

// Should not use tHalTxBd nor tHalRxBd. UMAC doesn't know these HAL structure.
#define WLANHAL_TX_BD_HEADER_SIZE 40
#define WLANHAL_RX_BD_HEADER_SIZE 76


#define WLANHAL_RX_BD_HEADER_OFFSET       0

#define WLANHAL_RX_BD_GET_MPDU_H_OFFSET( _pvBDHeader )   (((tpHalRxBd)_pvBDHeader)->mpduHeaderOffset)

#define WLANHAL_RX_BD_GET_MPDU_D_OFFSET( _pvBDHeader )   (((tpHalRxBd)_pvBDHeader)->mpduDataOffset)

#define WLANHAL_RX_BD_GET_MPDU_LEN( _pvBDHeader )        (((tpHalRxBd)_pvBDHeader)->mpduLength)

#define WLANHAL_RX_BD_GET_MPDU_H_LEN( _pvBDHeader )      (((tpHalRxBd)_pvBDHeader)->mpduHeaderLength)

#define WLANHAL_RX_BD_GET_FT( _pvBDHeader )        (((tpHalRxBd)_pvBDHeader)->ft)

#define WLANHAL_RX_BD_GET_LLC( _pvBDHeader )        (((tpHalRxBd)_pvBDHeader)->llc)

#define WLANHAL_RX_BD_GET_TID( _pvBDHeader )        (((tpHalRxBd)_pvBDHeader)->tid)

#define WLANHAL_RX_BD_GET_ASF( _pvBDHeader )        (((tpHalRxBd)_pvBDHeader)->asf)

#define WLANHAL_RX_BD_GET_AEF( _pvBDHeader )           (((tpHalRxBd)_pvBDHeader)->aef)

#define WLANHAL_RX_BD_GET_LSF( _pvBDHeader )           (((tpHalRxBd)_pvBDHeader)->lsf)

#define WLANHAL_RX_BD_GET_ESF( _pvBDHeader )           (((tpHalRxBd)_pvBDHeader)->esf)

#define WLANHAL_RX_BD_GET_STA_ID( _pvBDHeader )     (((tpHalRxBd)_pvBDHeader)->addr2Index)
#define WLANHAL_RX_BD_GET_ADDR3_IDX( _pvBDHeader )     (((tpHalRxBd)_pvBDHeader)->addr3Index)
#define WLANHAL_RX_BD_GET_ADDR1_IDX( _pvBDHeader )     (((tpHalRxBd)_pvBDHeader)->addr1Index)

#define WLANHAL_TX_BD_GET_TID( _pvBDHeader )           (((tpHalTxBd)_pvBDHeader)->tid)
#define WLANHAL_TX_BD_GET_STA_ID( _pvBDHeader )    (((tpHalTxBd)_pvBDHeader)->staIndex)

#define WLANHAL_RX_BD_GET_DPU_SIG( _pvBDHeader )   (((tpHalRxBd)_pvBDHeader)->dpuSignature)

#define WLANHAL_FC_RX_BD_REPORT_CONTENT_SIZE        (2 * HAL_NUM_STA * sizeof(tANI_U8))   // size of fcSTATxQLen[HAL_NUM_STA]+fcSTACurTxRate[HAL_NUM_STA]
#define WLANHAL_FC_TX_BD_HEADER_SIZE                sizeof(tHalFcTxBd)
#define WLANHAL_RX_BD_GET_FC( _pvBDHeader )                      (((tpHalFcRxBd)_pvBDHeader)->fc)
#define WLANHAL_RX_BD_GET_RX_TIME_STAMP( _pvBDHeader )           (((tpHalFcRxBd)_pvBDHeader)->mclkRxTimestamp)
#define WLANHAL_RX_BD_GET_STA_VALID_MASK( _pvBDHeader )          (((tpHalFcRxBd)_pvBDHeader)->fcSTAValidMask)
#define WLANHAL_RX_BD_GET_STA_PS_STATE( _pvBDHeader )            (((tpHalFcRxBd)_pvBDHeader)->fcSTAPwrSaveStateMask)
#define WLANHAL_RX_BD_GET_STA_TH_IND( _pvBDHeader )              (((tpHalFcRxBd)_pvBDHeader)->fcSTAThreshIndMask)
#define WLANHAL_RX_BD_GET_STA_TXQ_STATUS( _pvBDHeader )          (((tpHalFcRxBd)_pvBDHeader)->fcSTATxQStatus)
#define WLANHAL_RX_BD_GET_STA_TXQ_LEN( _pvBDHeader, staIdx )     (((tpHalFcRxBd)_pvBDHeader)->fcSTATxQLen[staIdx])
#define WLANHAL_RX_BD_GET_STA_CUR_TX_RATE( _pvBDHeader, staIdx ) (((tpHalFcRxBd)_pvBDHeader)->fcSTACurTxRate[staIdx])

#define WLANHAL_TX_BD_GET_RMF(_pvBDHeader)          (((tpHalRxBd)_pvBDHeader)->rmf)

#define WLANHAL_TX_BD_GET_UB(_pvBDHeader)           (((tpHalRxBd)_pvBDHeader)->ub)

#define WLANHAL_RX_BD_GET_RMF(_pvBDHeader)          (((tpHalRxBd)_pvBDHeader)->rmf)

#define WLANHAL_RX_BD_GET_UB(_pvBDHeader)           (((tpHalRxBd)_pvBDHeader)->ub)

#define WLANHAL_RX_BD_GET_RATEINDEX(_pvBDHeader)    (((tpHalRxBd)_pvBDHeader)->rateIndex)

#define WLANHAL_RX_BD_GET_TIMESTAMP(_pvBDHeader)    (((tpHalRxBd)_pvBDHeader)->mclkRxTimestamp)

#define tHalFcRxBd       halFcRxBd_type             
#define tpHalFcRxBd      phalFcRxBd_type
#define tHalFcTxBd       halFcTxBd_type
#define tpHalFcTxBd      pHalFcTxBd_type              
#define tHalFcTxParams   tFcTxParams_type
#define tHalFcRxParams   tFcRxParams_type               
#define tpHalFcTxParams  pFcTxParams_type               
#define tpHalFcRxParams  pFcRxParams_type             

/*------------ RSSI and SNR Information extraction -------------*/
#define WLANHAL_RX_BD_GET_RSSI0( _pvBDHeader )  \
    (((((tpHalRxBd)_pvBDHeader)->phyStats0) >> 24) & 0xff)
#define WLANHAL_RX_BD_GET_RSSI1( _pvBDHeader )  \
    (((((tpHalRxBd)_pvBDHeader)->phyStats0) >> 16) & 0xff)
#define WLANHAL_RX_BD_GET_RSSI2( _pvBDHeader )  \
    (((((tpHalRxBd)_pvBDHeader)->phyStats0) >> 0) & 0xff)
#define WLANHAL_RX_BD_GET_RSSI3( _pvBDHeader )  \
    ((((tpHalRxBd)_pvBDHeader)->phyStats0) & 0xff)

// Get the average of the 4 values.
#define WLANHAL_GET_RSSI_AVERAGE( _pvBDHeader ) \
    (((WLANHAL_RX_BD_GET_RSSI0(_pvBDHeader)) + \
    (WLANHAL_RX_BD_GET_RSSI1(_pvBDHeader)) + \
    (WLANHAL_RX_BD_GET_RSSI2(_pvBDHeader)) + \
    (WLANHAL_RX_BD_GET_RSSI3(_pvBDHeader))) / 4)

// Get the SNR value from PHY Stats
#define WLANHAL_RX_BD_GET_SNR( _pvBDHeader )    \
    (((((tpHalRxBd)_pvBDHeader)->phyStats1) >> 24) & 0xff)
/*-----------------------------------------------------------------*/
#define WLANHAL_RX_BD_GET_DPU_SIG( _pvBDHeader )   (((tpHalRxBd)_pvBDHeader)->dpuSignature)


#define WLANHAL_TX_BD_SET_MPDU_DATA_OFFSET( _bd, _off )        (((tpHalTxBd)_bd)->mpduDataOffset = _off)
 
#define WLANHAL_TX_BD_SET_MPDU_HEADER_OFFSET( _bd, _off )    (((tpHalTxBd)_bd)->mpduHeaderOffset = _off)

#define WLANHAL_TX_BD_SET_MPDU_HEADER_LEN( _bd, _len )        (((tpHalTxBd)_bd)->mpduHeaderLength = _len)

#define WLANHAL_TX_BD_SET_MPDU_LEN( _bd, _len )                (((tpHalTxBd)_bd)->mpduLength = _len)

#define WLANHAL_RX_BD_GET_BA_OPCODE(_pvBDHeader)        (((tpHalRxBd)_pvBDHeader)->reorderOpcode)

#define WLANHAL_RX_BD_GET_BA_FI(_pvBDHeader)            (((tpHalRxBd)_pvBDHeader)->reorderFwdIdx)

#define WLANHAL_RX_BD_GET_BA_SI(_pvBDHeader)            (((tpHalRxBd)_pvBDHeader)->reorderSlotIdx)

#define WLANHAL_RX_BD_GET_BA_CSN(_pvBDHeader)           (((tpHalRxBd)_pvBDHeader)->currentPktSeqNo)

#define WLANHAL_RX_BD_GET_BA_ESN(_pvBDHeader)           (((tpHalRxBd)_pvBDHeader)->expectedPktSeqNo)

#define WLANHAL_RX_BD_GET_RXP_FLAGS(_pvBDHeader)            (((tpHalRxBd)_pvBDHeader)->rxpFlags)

#define WLANHAL_RX_BD_GET_TYPE_SUBTYPE(_pvBDHeader)            (((tpHalRxBd)_pvBDHeader)->frameTypeSubtype)
#define WLANHAL_RX_BD_SET_TYPE_SUBTYPE( _bd, _typeSubtype )        (((tpHalRxBd)_bd)->frameTypeSubtype = _typeSubtype)


#define WLANHAL_RX_BD_ASF_SET                1 /*The value of the field when set and pkt is AMSDU*/

#define WLANHAL_RX_BD_FSF_SET               1

#define WLANHAL_RX_BD_LSF_SET               1

#define WLANHAL_RX_BD_AEF_SET               1

 
#define WLANHAL_RX_BD_LLC_PRESENT            0 /*The value of the field when LLC is present*/

#define WLANHAL_RX_BD_FT_DONE                  1 /* The value of the field when frame xtl was done*/

/*DPU_FEEDBACK_WPI_UNPROTECTED macro defined in volansdefs.h which is not available
  for UMAC in prima so declared it here */
#define DPU_FEEDBACK_WPI_UNPROTECTED 0x20   
#define WLANHAL_RX_IS_UNPROTECTED_WPI_FRAME(_pvBDHeader)  \
        (DPU_FEEDBACK_WPI_UNPROTECTED == ((WDI_DS_RxMetaInfoType *)_pvBDHeader)->dpuFeedback)

/*==========================================================================

  FUNCTION    WLANHAL_RxBD_GetFrameTypeSubType

  DESCRIPTION 
    Called by TL to retrieve the type/subtype of the received frame.

  DEPENDENCIES 
    TL should pass a valid RxBD buffer pointer.
    
  PARAMETERS 

    IN
    pvBDHeader:    Void pointer to the RxBD buffer.
    usFrmCtrl:the frame ctrl of the 802.11 header 
   
  RETURN VALUE
    A byte which contains both type and subtype info. LSB four bytes (b0 to b3)
    is subtype and b5-b6 is type info. 

  SIDE EFFECTS 
  
============================================================================*/

tANI_U8 WLANHAL_RxBD_GetFrameTypeSubType(v_PVOID_t _pvBDHeader, tANI_U16 usFrmCtrl);


#define HAL_TXCOMP_REQUESTED_MASK           0x1  //bit 0 for TxComp intr requested. 
#define HAL_USE_SELF_STA_REQUESTED_MASK     0x2  //bit 1 for STA overwrite with selfSta Requested.
#define HAL_TX_NO_ENCRYPTION_MASK           0x4  //bit 2. If set, the frame is not to be encrypted
#if defined(LIBRA_WAPI_SUPPORT)
#define HAL_WAPI_STA_MASK            0x8  //bit 3. If set, this frame is for WAPI station
#endif

#define HAL_TRIGGER_ENABLED_AC_MASK         0x10 //bit 4 for data frames belonging to trigger enabled AC
#define HAL_USE_NO_ACK_REQUESTED_MASK       0x20

#define HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40 // Bit 6 will be used to control BD rate for Management frames
#define HAL_USE_PEER_STA_REQUESTED_MASK   0x80 //bit 7 will be used to control frames for p2p interface

#ifdef FEATURE_WLAN_TDLS
#define HAL_TDLS_PEER_STA_MASK              0x80 //bit 7 set for TDLS peer station 
#endif

#define HAL_RELIABLE_MCAST_REQUESTED_MASK   0x100

#define HAL_USE_BD_RATE_1_MASK              0x1000 // bit 12 for BD RATE 1
#define HAL_USE_BD_RATE_2_MASK              0x2000 // bit 13 for BD RATE 1
#define HAL_USE_BD_RATE_3_MASK              0x4000 // bit 14 for BD RATE 1
#define HAL_USE_FW_IN_TX_PATH               0x200 //bit 9 to send via WQ5
/*==========================================================================

  FUNCTION    WLANHAL_FillTxBd

  DESCRIPTION 
    Called by PE to register as a client for management frames delivery. 

  DEPENDENCIES 
    TL must be initialized before this API can be called. 
    
  PARAMETERS 

    IN
    pAdapter:       pointer to the global adapter context;a handle to TL's 
                    control block can be extracted from its context 
    vosFrmBuf:     pointer to a vOSS buffer containing the management  
                    frame to be transmitted
    usFrmLen:       the length of the frame to be transmitted; information 
                    is already included in the vOSS buffer
    wFrmType:       the type of the frame being transmitted
    tid:            tid used to transmit this frame
    pfnCompTxFunc:  function pointer to the transmit complete routine
    voosBDHeader:   pointer to the BD header
    txFlag:  can have appropriate bit setting as required
    
                #define HAL_TXCOMP_REQUESTED_MASK           0x1  //bit 0 for TxComp intr requested. 
                #define HAL_USE_SELF_STA_REQUESTED_MASK    0x2  //bit 1 for STA overwrite with selfSta Requested.
                #define HAL_TX_NO_ENCRYPTION_MASK           0x4  //bit 2. If set, the frame is not to be encrypted
#if defined(FEATURE_WLAN_WAPI)
                #define HAL_WAPI_STA_MASK            0x8  //bit 3. If set, this frame is for WAPI station
#endif
                
    uTimestamp:     pkt timestamp

   
  RETURN VALUE
    The result code associated with performing the operation  

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANHAL_FillTxBd(void *pAdapter, tANI_U8 typeSubtype, void *pDestMacAddr, void *pAddr2,
        tANI_U8* ptid, tANI_U8 disableFrmXtl, void *pTxBd, tANI_U32 txFlag, tANI_U32 timeStamp);

VOS_STATUS WLANHAL_FillFcTxBd(void *pVosGCtx, void *pFcParams, void *pFcTxBd);
/** To swap the report part of FC RxBD */
void WLANHAL_SwapFcRxBd(tANI_U8 *pBd);

/* To swap the data */
void WLANHAL_Swap32Bytes(tANI_U8* pData, tANI_U32 size);

/** To swap the RxBD */
void WLANHAL_SwapRxBd(tANI_U8 *pBd);
void WLANHAL_RxAmsduBdFix(void *pVosGCtx,v_PVOID_t _pvBDHeader);

#ifdef WLAN_PERF
tANI_U32 WLANHAL_TxBdFastFwd(void *pAdapter, tANI_U8 *pDestMac, tANI_U8 tid, tANI_U8 unicastDst,  void *pTxBd, tANI_U16);
#endif

VOS_STATUS WLANHAL_EnableUapsdAcParams(void* pVosGCtx, tANI_U8 staIdx, tUapsdInfo *pUapsdInfo);
VOS_STATUS WLANHAL_DisableUapsdAcParams(void* pVosGCtx, tANI_U8 staIdx, tANI_U8 ac);

VOS_STATUS WLANHAL_EnableIdleBdPduInterrupt(void* pVosGCtx, tANI_U8 idleBdPduThreshold);

#ifdef FEATURE_ON_CHIP_REORDERING
tANI_U8 WLANHAL_IsOnChipReorderingEnabledForTID(void* pVosGCtx, tANI_U8 staIdx, tANI_U8 tid);
#endif

#ifdef WLAN_SOFTAP_VSTA_FEATURE
v_BOOL_t WLANHAL_IsHwFrameTxTranslationCapable(v_PVOID_t pVosGCtx, tANI_U8 staIdx);
#endif

#define tHalRxBd    halRxBd_type
#define tpHalRxBd    phalRxBd_type

#define tHalTxBd    halTxBd_type
#define tpHalTxBd    pHalTxBd_type

#ifdef BA_PARAM_STRUCTURE
#else
#define BA_PARAM_STRUCTURE
//
// HAL --> TL
// Messages indicating the setup and/or teardown of
// A-MPDU/BA sessions with a given peer HT MAC entity
//

//
// A data structure identifying all of the variables
// in a typical A-MPDU/BA setup
//
typedef struct sBAParams
{

  // A unique BA Session ID that has been assigned by HAL
  // for the curent BA Session
  tANI_U16 baSessionID;

  // TID for which the BA session has been setup
  tANI_U8 baTID;

  // BA Buffer Size allocated for the current BA session   //Should be deleted. needs TL change. use winSize instead
  tANI_U8 baBufferSize;

  tANI_U16 SSN;
  tANI_U8 winSize;
  tANI_U8 STAID;

} tBAParams, *tpBAParams;

//
// TL -> HAL
// tSirMsgQ.type = SIR_HAL_HDD_ADDBA_RSP
//
typedef struct sAddBARsp
{
  // Message Type
  tANI_U16 mesgType;

  // Message Length
  tANI_U16 mesgLen;

  //BA session ID
  tANI_U16 baSessionID;

  tANI_U16 replyWinSize;
}tAddBARsp, *tpAddBARsp;

//
// HAL -> TL
// tSirMsgQ.type = SIR_HAL_ADDBA_IND
// tSirMsgQ.reserved = 0
// tSirMsgQ.body = "allocated" instance of tpAddBAInd
//
typedef struct sAddBAInd
{

  // Message Type
  tANI_U16 mesgType;

  // Message Length
  tANI_U16 mesgLen;

  tBAParams baSession;

} tAddBAInd, *tpAddBAInd;

//
// HAL -> TL
// tSirMsgQ.type = SIR_HAL_DELBA_IND
// tSirMsgQ.reserved = 0
// tSirMsgQ.body = "allocated" instance of tpDelBAInd
//
// TL -> HAL
// tSirMsgQ.type = SIR_HAL_BA_FAIL_IND
// tSirMsgQ.reserved = 0
// tSirMsgQ.body = "allocated" instance of tpDelBAInd
//
typedef struct sDelBAInd
{
  tANI_U8 staIdx;

  tANI_U8 baTID;
  // Message Type
  tANI_U16 mesgType;

  // Message Length
  tANI_U16 mesgLen;

} tDelBAInd, *tpDelBAInd;
#endif

/*===============================================
 *
 *  TL <-> HAL structures 
 *
 *===============================================
 */
//
// TL -> HAL 
// tSirMsgQ.type = SIR_HAL_TL_FLUSH_AC_REQ
//
typedef struct sFlushACReq
{
    // Message Type
    tANI_U16 mesgType;

    // Message Length
    tANI_U16 mesgLen;

    // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed 
    tANI_U8   ucTid;

} tFlushACReq, *tpFlushACReq;

//
//
// HAL -> TL 
// tSirMsgQ.type = SIR_HAL_TL_FLUSH_AC_RSP
//
typedef struct sFlushACRsp
{
    // Message Type
    tANI_U16 mesgType;

    // Message Length
    tANI_U16 mesgLen;

    // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed 
    tANI_U8   ucTid;

    // status of the Flush operation 
    tANI_U8 status;
} tFlushACRsp, *tpFlushACRsp;

#endif

