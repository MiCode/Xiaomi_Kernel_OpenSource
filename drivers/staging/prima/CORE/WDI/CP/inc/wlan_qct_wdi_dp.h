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




#ifndef WLAN_QCT_WDI_DP_H
#define WLAN_QCT_WDI_DP_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
              I N T E R N A L     A P I       F O R    T H E
                                 D A T A   P A T H 
                
                   
DESCRIPTION
  This file contains the internal API exposed by the DAL Control Path Core 
  module to be used by the DAL Data Path Core. 
  
      
  Copyright (c) 2010 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
08/19/10    lti     Created module.

===========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_wdi_i.h"
#include "wlan_qct_wdi_bd.h"

/*========================================================================= 
   BD Macro Defines  
=========================================================================*/ 

/*--------------------------------------------------------------------------
  BD Definitions used by the DAL data path 
--------------------------------------------------------------------------*/
#define WDI_TXBD_BDRATE_DEFAULT 0
#define WDI_TXBD_BDRATE_FIRST   1
#define WDI_TXBD_BDRATE_SECOND  2
#define WDI_TXBD_BDRATE_THIRD   3

#define WDI_FRAME_TYPE_MASK     0x30
#define WDI_FRAME_TYPE_OFFSET   0x4
#define WDI_FRAME_SUBTYPE_MASK  0x0F

#define WDI_TXBD_BD_SSN_FILL_HOST         0
#define WDI_TXBD_BD_SSN_FILL_DPU_NON_QOS  1
#define WDI_TXBD_BD_SSN_FILL_DPU_QOS      2

#define WDI_ACKPOLICY_ACK_REQUIRED        0
#define WDI_ACKPOLICY_ACK_NOTREQUIRED     1

#define WDI_BDRATE_BCDATA_FRAME           1
#define WDI_BDRATE_BCMGMT_FRAME           2
#define WDI_BDRATE_CTRL_FRAME             3
    
/* Default values for FillTx BD */
#define WDI_DEFAULT_UNICAST_ENABLED       1
#define WDI_RMF_DISABLED                  0
#define WDI_RMF_ENABLED                   1
#define WDI_NO_ENCRYPTION_DISABLED        0
#define WDI_NO_ENCRYPTION_ENABLED         1
    
#define WDI_RX_BD_ADDR3_SELF_IDX          0

#define WDI_TXCOMP_REQUESTED_MASK           0x1  //bit 0 for TxComp intr requested. 
#define WDI_USE_SELF_STA_REQUESTED_MASK     0x2  //bit 1 for STA overwrite with selfSta Requested.
#define WDI_TX_NO_ENCRYPTION_MASK           0x4  //bit 2. If set, the frame is not to be encrypted
#if defined(LIBRA_WAPI_SUPPORT)
#define WDI_WAPI_STA_MASK            0x8  //bit 3. If set, this frame is for WAPI station
#endif

#define WDI_TRIGGER_ENABLED_AC_MASK         0x10 //bit 4 for data frames belonging to trigger enabled AC
#define WDI_USE_NO_ACK_REQUESTED_MASK       0x20

#define WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40 // Bit 6 will be used to control BD rate for Management frames

#ifdef FEATURE_WLAN_TDLS
#define HAL_TDLS_PEER_STA_MASK              0x80 //bit 7 set for TDLS peer station
#endif

/* Bit 8 is used to route reliable multicast data frames from QID 1.
   This dynamically changes ACK_POLICY = TRUE for multicast frames */
#define WDI_RELIABLE_MCAST_REQUESTED_MASK 0x100

#define WDI_USE_BD_RATE_MASK              0x1000
#define WDI_USE_FW_IN_TX_PATH             0x200 //bit 9 used to route the frames to Work Queue 5

/*Macro for getting the size of the TX BD*/
#define WDI_TX_BD_HEADER_SIZE        sizeof(WDI_TxBdType)

/*Macro for getting the size of the RX BD*/
#define WDI_RX_BD_HEADER_SIZE        sizeof(WDI_RxBdType)

#define WDI_RX_BD_HEADER_OFFSET       0

#define WDI_DPU_FEEDBACK_OFFSET       1

// Frame Type definitions

#define WDI_MAC_MGMT_FRAME    0x0
#define WDI_MAC_CTRL_FRAME    0x1
#define WDI_MAC_DATA_FRAME    0x2

// Data frame subtype definitions
#define WDI_MAC_DATA_DATA                 0
#define WDI_MAC_DATA_DATA_ACK             1
#define WDI_MAC_DATA_DATA_POLL            2
#define WDI_MAC_DATA_DATA_ACK_POLL        3
#define WDI_MAC_DATA_NULL                 4
#define WDI_MAC_DATA_NULL_ACK             5
#define WDI_MAC_DATA_NULL_POLL            6
#define WDI_MAC_DATA_NULL_ACK_POLL        7
#define WDI_MAC_DATA_QOS_DATA             8
#define WDI_MAC_DATA_QOS_DATA_ACK         9
#define WDI_MAC_DATA_QOS_DATA_POLL        10
#define WDI_MAC_DATA_QOS_DATA_ACK_POLL    11
#define WDI_MAC_DATA_QOS_NULL             12
#define WDI_MAC_DATA_QOS_NULL_ACK         13
#define WDI_MAC_DATA_QOS_NULL_POLL        14
#define WDI_MAC_DATA_QOS_NULL_ACK_POLL    15

#define WDI_MAC_FRAME_SUBTYPE_START       0
#define WDI_MAC_FRAME_SUBTYPE_END         16

#define WDI_MAC_DATA_QOS_MASK             8
#define WDI_MAC_DATA_NULL_MASK            4
#define WDI_MAC_DATA_POLL_MASK            2
#define WDI_MAC_DATA_ACK_MASK             1

// Management frame subtype definitions

#define WDI_MAC_MGMT_ASSOC_REQ    0x0
#define WDI_MAC_MGMT_ASSOC_RSP    0x1
#define WDI_MAC_MGMT_REASSOC_REQ  0x2
#define WDI_MAC_MGMT_REASSOC_RSP  0x3
#define WDI_MAC_MGMT_PROBE_REQ    0x4
#define WDI_MAC_MGMT_PROBE_RSP    0x5
#define WDI_MAC_MGMT_BEACON       0x8
#define WDI_MAC_MGMT_ATIM         0x9
#define WDI_MAC_MGMT_DISASSOC     0xA
#define WDI_MAC_MGMT_AUTH         0xB
#define WDI_MAC_MGMT_DEAUTH       0xC
#define WDI_MAC_MGMT_ACTION       0xD
#define WDI_MAC_MGMT_RESERVED15   0xF

// Action frame categories

#define WDI_MAC_ACTION_SPECTRUM_MGMT    0
#define WDI_MAC_ACTION_QOS_MGMT         1
#define WDI_MAC_ACTION_DLP              2
#define WDI_MAC_ACTION_BLKACK           3
#define WDI_MAC_ACTION_HT               7
#define WDI_MAC_ACTION_WME              17

// QoS management action codes

#define WDI_MAC_QOS_ADD_TS_REQ      0
#define WDI_MAC_QOS_ADD_TS_RSP      1
#define WDI_MAC_QOS_DEL_TS_REQ      2
#define WDI_MAC_QOS_SCHEDULE        3
// and these are proprietary
#define WDI_MAC_QOS_DEF_BA_REQ      4
#define WDI_MAC_QOS_DEF_BA_RSP      5
#define WDI_MAC_QOS_DEL_BA_REQ      6
#define WDI_MAC_QOS_DEL_BA_RSP      7

#ifdef WLAN_PERF
/* TxBD signature fields
 * serial(8): a monotonically increasing serial # whenever there is a Add/Del STA or Add/Del Key event
 * macHash(16): Hash value of DA
 * tid(4):    TID
 * ucast(1):  Unicast or Broadcast data frame
 */
#define WDI_TXBD_SIG_SERIAL_OFFSET        0   
#define WDI_TXBD_SIG_TID_OFFSET           8
#define WDI_TXBD_SIG_UCAST_DATA_OFFSET    9
#define WDI_TXBD_SIG_MACADDR_HASH_OFFSET  16
#define WDI_TXBD_SIG_MGMT_MAGIC           0xbdbdbdbd

#endif

/*--------------------------------------------------------------------------
   BD header macros - used by the data path to get or set various values
   inside the packet BD 
--------------------------------------------------------------------------*/
#define WDI_RX_BD_GET_MPDU_H_OFFSET( _pvBDHeader )   (((WDI_RxBdType*)_pvBDHeader)->mpduHeaderOffset)

#define WDI_RX_BD_GET_MPDU_D_OFFSET( _pvBDHeader )   (((WDI_RxBdType*)_pvBDHeader)->mpduDataOffset)

#define WDI_RX_BD_GET_MPDU_LEN( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->mpduLength)

#define WDI_RX_BD_GET_MPDU_H_LEN( _pvBDHeader )      (((WDI_RxBdType*)_pvBDHeader)->mpduHeaderLength)

#define WDI_RX_BD_GET_FT( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->ft)

#define WDI_RX_BD_GET_DPU_FEEDBACK( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->dpuFeedback)

#define WDI_RX_BD_GET_RX_CHANNEL( _pvBDHeader )         \
        (( (((WDI_RxBdType*)_pvBDHeader)->reserved0) << 4) | (((WDI_RxBdType*)_pvBDHeader)->rxChannel))

#define WDI_FRAME_TYPE_MASK     0x30
#define WDI_FRAME_TYPE_OFFSET   0x4
#define WDI_FRAME_SUBTYPE_MASK  0x0F

#define WDI_RX_BD_GET_SUBTYPE( _pvBDHeader )        ((((WDI_RxBdType*)_pvBDHeader)->frameTypeSubtype) & WDI_FRAME_SUBTYPE_MASK)

#define WDI_RX_BD_GET_TYPE( _pvBDHeader )     (((((WDI_RxBdType*)_pvBDHeader)->frameTypeSubtype) & WDI_FRAME_TYPE_MASK) >> WDI_FRAME_TYPE_OFFSET)

#define WDI_RX_BD_GET_RTSF( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->rtsf)

#define WDI_RX_BD_GET_BSF( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->bsf)

#define WDI_RX_BD_GET_SCAN( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->scanLearn)

#define WDI_RX_BD_GET_DPU_SIG( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->dpuSignature)

#define WDI_RX_BD_GET_NE( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->dpuNE)

#define WDI_RX_BD_GET_LLCR( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->llcr)

#define WDI_RX_BD_GET_TIMESTAMP( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->mclkRxTimestamp)

#define WDI_RX_BD_GET_RXPFLAGS( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->rxpFlags)

#define WDI_RX_BD_GET_RATEINDEX( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->rateIndex)

#define WDI_RX_BD_GET_AMSDU_SIZE( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->totalMsduSize)

#define WDI_RX_BD_GET_LLC( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->llc)

#define WDI_RX_BD_GET_TID( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->tid)

#define WDI_RX_BD_GET_RFBAND( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->rfBand)

#define WDI_RX_BD_GET_ASF( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->asf)

#define WDI_RX_BD_GET_AEF( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->aef)

#define WDI_RX_BD_GET_LSF( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->lsf)

#define WDI_RX_BD_GET_ESF( _pvBDHeader )        (((WDI_RxBdType*)_pvBDHeader)->esf)

#define WDI_RX_BD_GET_STA_ID( _pvBDHeader )     (((WDI_RxBdType*)_pvBDHeader)->addr2Index)
#define WDI_RX_BD_GET_UB( _pvBDHeader )     (((WDI_RxBdType*)_pvBDHeader)->ub)
#define WDI_RX_BD_GET_ADDR3_IDX( _pvBDHeader )  (((WDI_RxBdType*)_pvBDHeader)->addr3Index)
#define WDI_RX_BD_GET_ADDR1_IDX( _pvBDHeader )  (((WDI_RxBdType*)_pvBDHeader)->addr1Index)

#define WDI_TX_BD_GET_TID( _pvBDHeader )   (((WDI_TxBdType*)_pvBDHeader)->tid)
#define WDI_TX_BD_GET_STA_ID( _pvBDHeader ) (((WDI_TxBdType*)_pvBDHeader)->staIndex)

#define WDI_RX_BD_GET_DPU_SIG( _pvBDHeader )     (((WDI_RxBdType*)_pvBDHeader)->dpuSignature)

//flow control related.
#define WDI_RX_FC_BD_GET_STA_TX_DISABLED_BITMAP( _pvBDHeader )     (((WDI_FcRxBdType*)_pvBDHeader)->fcStaTxDisabledBitmap)
#define WDI_RX_FC_BD_GET_FC( _pvBDHeader )     (((WDI_FcRxBdType*)_pvBDHeader)->fc)
#define WDI_RX_FC_BD_GET_STA_VALID_MASK( _pvBDHeader )     (((WDI_FcRxBdType*)_pvBDHeader)->fcSTAValidMask)

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
//LFR scan related
#define WDI_RX_BD_GET_OFFLOADSCANLEARN( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->offloadScanLearn)
#define WDI_RX_BD_GET_ROAMCANDIDATEIND( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->roamCandidateInd)
#endif
#ifdef WLAN_FEATURE_EXTSCAN
#define WDI_RX_BD_GET_EXTSCANFULLSCANRESIND( _pvBDHeader ) (((WDI_RxBdType*)_pvBDHeader)->extscanBuffer)
#endif

/*------------ RSSI and SNR Information extraction -------------*/
#define WDI_RX_BD_GET_RSSI0( _pvBDHeader )  \
    (((((WDI_RxBdType*)_pvBDHeader)->phyStats0) >> 24) & 0xff)
#define WDI_RX_BD_GET_RSSI1( _pvBDHeader )  \
    (((((WDI_RxBdType*)_pvBDHeader)->phyStats0) >> 16) & 0xff)
#define WDI_RX_BD_GET_RSSI2( _pvBDHeader )  \
    (((((WDI_RxBdType*)_pvBDHeader)->phyStats0) >> 0) & 0xff)
#define WDI_RX_BD_GET_RSSI3( _pvBDHeader )  \
    ((((WDI_RxBdType*)_pvBDHeader)->phyStats0) & 0xff)

// Get the average of the 4 values.
#define WDI_GET_RSSI_AVERAGE( _pvBDHeader ) \
    (((WDI_RX_BD_GET_RSSI0(_pvBDHeader)) + \
      (WDI_RX_BD_GET_RSSI1(_pvBDHeader)) + \
      (WDI_RX_BD_GET_RSSI2(_pvBDHeader)) + \
      (WDI_RX_BD_GET_RSSI3(_pvBDHeader))) / 4)

// Get the SNR value from PHY Stats
#define WDI_RX_BD_GET_SNR( _pvBDHeader )    \
    (((((WDI_RxBdType*)_pvBDHeader)->phyStats1) >> 24) & 0xff)
/*-----------------------------------------------------------------*/

#define WDI_TX_BD_SET_MPDU_DATA_OFFSET( _bd, _off )      (((WDI_TxBdType*)_bd)->mpduDataOffset = _off)
 
#define WDI_TX_BD_SET_MPDU_HEADER_OFFSET( _bd, _off )    (((WDI_TxBdType*)_bd)->mpduHeaderOffset = _off)

#define WDI_TX_BD_SET_MPDU_HEADER_LEN( _bd, _len )       (((WDI_TxBdType*)_bd)->mpduHeaderLength = _len)

#define WDI_TX_BD_SET_MPDU_LEN( _bd, _len )              (((WDI_TxBdType*)_bd)->mpduLength = _len)

#define WDI_RX_BD_GET_BA_OPCODE(_pvBDHeader)        (((WDI_RxBdType*)_pvBDHeader)->reorderOpcode)

#define WDI_RX_BD_GET_BA_FI(_pvBDHeader)            (((WDI_RxBdType*)_pvBDHeader)->reorderFwdIdx)

#define WDI_RX_BD_GET_BA_SI(_pvBDHeader)            (((WDI_RxBdType*)_pvBDHeader)->reorderSlotIdx)

#define WDI_RX_BD_GET_BA_CSN(_pvBDHeader)           (((WDI_RxBdType*)_pvBDHeader)->currentPktSeqNo)

#define WDI_RX_BD_GET_BA_ESN(_pvBDHeader)           (((WDI_RxBdType*)_pvBDHeader)->expectedPktSeqNo)

#define WDI_RX_BD_GET_RXP_FLAGS(_pvBDHeader)        (((WDI_RxBdType*)_pvBDHeader)->rxpFlags)

#define WDI_RX_BD_GET_PMICMD_20TO23(_pvBDHeader)        (((WDI_RxBdType*)_pvBDHeader)->pmiCmd4to23[4])

#define WDI_RX_BD_GET_PMICMD_24TO25(_pvBDHeader)        (((WDI_RxBdType*)_pvBDHeader)->pmiCmd24to25)

#ifdef WLAN_FEATURE_11W
#define WDI_RX_BD_GET_RMF( _pvBDHeader )         (((WDI_RxBdType*)_pvBDHeader)->rmf)
#endif

#define WDI_RX_BD_ASF_SET               1 /*The value of the field when set and pkt is AMSDU*/

#define WDI_RX_BD_FSF_SET               1

#define WDI_RX_BD_LSF_SET               1

#define WDI_RX_BD_AEF_SET               1

 
#define WDI_RX_BD_LLC_PRESENT           0 /*The value of the field when LLC is present*/

#define WDI_RX_BD_FT_DONE                1 /* The value of the field when frame xtl was done*/

/*========================================================================= 
   API Definition  
=========================================================================*/ 


/**
 @brief WDI_RxBD_GetFrameTypeSubType - Called by the data path 
        to retrieve the type/subtype of the received frame.
  
 @param       pvBDHeader:    Void pointer to the RxBD buffer.
    usFrmCtrl:     the frame ctrl of the 802.11 header 
  
 @return   A byte which contains both type and subtype info. LSB four bytes 
 (b0 to b3)is subtype and b5-b6 is type info. 
*/

wpt_uint8 
WDI_RxBD_GetFrameTypeSubType
(
  void*       _pvBDHeader, 
  wpt_uint16  usFrmCtrl
);


/**
 @brief WDI_FillTxBd - Called by the data path to fill the TX BD 
  
 @param       pWDICtx:       Context to the WDI
     ucTypeSubtype: of the frame
     pDestMacAddr:  destination MAC address
     pTid:          pointer to the TID (in/out value in case DAL Ctrl causes
                    a TID downgrade - not done for now)
     ucDisableFrmXtl: set to 1 if this frame is not to be translated by HW
     pTxBd:          pointer to the TX BD
     ucTxFlag:       can have appropriate bit setting as required
     ucProtMgmtFrame: for management frames, whether the frame is
                      protected (protect bit is set in FC)
     uTimestamp:     pkt timestamp
  
  
 @return success or not
*/

WDI_Status
WDI_FillTxBd
(
    WDI_ControlBlockType*  pWDICtx, 
    wpt_uint8              ucTypeSubtype, 
    void*                  pDestMacAddr,
    void*                  pAddr2,
    wpt_uint8*             pTid, 
    wpt_uint8              ucDisableFrmXtl, 
    void*                  pTxBd, 
    wpt_uint32             ucTxFlag,
    wpt_uint8              ucProtMgmtFrame,
    wpt_uint32             uTimeStamp,
    wpt_uint8              isEapol,
    wpt_uint8*             staIndex
);

/**
 @brief WDI_SwapRxBd swaps the RX BD.

  
 @param pBd - pointer to the BD (in/out)
  
 @return None
*/
void 
WDI_SwapRxBd
(
  wpt_uint8 *pBd
);

/**
 @brief WDI_SwapTxBd - Swaps the TX BD
  
 @param  pBd - pointer to the BD (in/out)
  
 @return   none
*/
void 
WDI_SwapTxBd
(
  wpt_uint8 *pBd
);

/**
 @brief WDI_RxAmsduBdFix - fix for HW issue for AMSDU 

  
 @param   pWDICtx:       Context to the WDI
          pBDHeader - pointer to the BD header
  
 @return None
*/
void 
WDI_RxAmsduBdFix
(
  WDI_ControlBlockType*  pWDICtx, 
  void*                  pBDHeader
);

#ifdef WLAN_PERF
/**
 @brief WDI_TxBdFastFwd - evaluates if a frame can be fast 
        forwarded 
  
 @param   pWDICtx: Context to the WDI 
          pDestMac: Destination MAC
          ucTid: packet TID pBDHeader
          ucUnicastDst: is packet unicast
          pTxBd:       pointer to the BD header
          usMpduLength: len 
  
 @return 1 - if the frame can be fast fwd-ed ; 0 if not 
*/
wpt_uint32 
WDI_TxBdFastFwd
(
  WDI_ControlBlockType*  pWDICtx,  
  wpt_uint8*             pDestMac, 
  wpt_uint8              ucTid, 
  wpt_uint8              ucUnicastDst,  
  void*                  pTxBd, 
  wpt_uint16             usMpduLength);
#endif /*WLAN_PERF*/

/**
 @brief WDI_DP_UtilsInit - Intializes the parameters required to 
        interact with the data path
  
 @param       pWDICtx:    pointer to the main WDI Ctrl Block
  
 @return   success always
*/
WDI_Status 
WDI_DP_UtilsInit
(
  WDI_ControlBlockType*  pWDICtx
);

/**
 @brief WDI_DP_UtilsExit - Clears the parameters required to
        interact with the data path
  
 @param       pWDICtx:    pointer to the main WDI Ctrl Block
  
 @return   success always
*/
WDI_Status
WDI_DP_UtilsExit
( 
    WDI_ControlBlockType*  pWDICtx
);

#endif /*WLAN_QCT_WDI_DP_H*/

