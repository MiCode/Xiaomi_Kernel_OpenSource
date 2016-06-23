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




#ifndef WLAN_QCT_TLI_H
#define WLAN_QCT_TLI_H

/*===========================================================================

               W L A N   T R A N S P O R T   L A Y E R
                     I N T E R N A L   A P I


DESCRIPTION
  This file contains the internal declarations used within wlan transport
  layer module.

===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
02/19/10    bad     Fixed 802.11 to 802.3 ft issues with WAPI
01/14/10    rnair   Fixed the byte order for the WAI packet type.
01/08/10    lti     Added TL Data Caching
10/09/09    rnair   Add support for WAPI
02/02/09    sch     Add Handoff support
12/09/08    lti     Fixes for AMSS compilation
12/02/08    lti     Fix fo trigger frame generation
10/31/08    lti     Fix fo TL tx suspend
10/01/08    lti     Merged in fixes from reordering
09/05/08    lti     Fixes following QOS unit testing
08/06/08    lti     Added QOS support
07/18/08    lti     Fixes following integration
                    Added frame translation
06/26/08    lti     Fixes following unit testing
05/05/08    lti     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_packet.h"
#include "vos_api.h"
#include "vos_timer.h"
#include "vos_mq.h"
#include "vos_list.h"
#include "wlan_qct_tl.h"
#include "pmcApi.h"
#include "wlan_qct_hal.h"


#define STATIC  static
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*Maximum number of TIDs */
#define WLAN_MAX_TID                          8

/*Offset of the OUI field inside the LLC/SNAP header*/
#define WLANTL_LLC_OUI_OFFSET                 3

/*Size of the OUI type field inside the LLC/SNAP header*/
#define WLANTL_LLC_OUI_SIZE                   3

/*Offset of the protocol type field inside the LLC/SNAP header*/
#define WLANTL_LLC_PROTO_TYPE_OFFSET  (WLANTL_LLC_OUI_OFFSET +  WLANTL_LLC_OUI_SIZE)

/*Size of the protocol type field inside the LLC/SNAP header*/
#define WLANTL_LLC_PROTO_TYPE_SIZE            2

/*802.1x protocol type */
#define WLANTL_LLC_8021X_TYPE            0x888E

/*WAPI protocol type */
#define WLANTL_LLC_WAI_TYPE              0x88b4

#ifdef FEATURE_WLAN_TDLS
#define WLANTL_LLC_TDLS_TYPE             0x890d
#endif

/*Length offset inside the AMSDU sub-frame header*/
#define WLANTL_AMSDU_SUBFRAME_LEN_OFFSET     12

/*802.3 header definitions*/
#define  WLANTL_802_3_HEADER_LEN             14

/* Offset of DA field in a 802.3 header*/
#define  WLANTL_802_3_HEADER_DA_OFFSET        0

/*802.11 header definitions - header len without QOS ctrl field*/
#define  WLANTL_802_11_HEADER_LEN            24

/*802.11 header length + QOS ctrl field*/
#define  WLANTL_MPDU_HEADER_LEN              32

/*802.11 header definitions*/
#define  WLANTL_802_11_MAX_HEADER_LEN        40

/*802.11 header definitions - qos ctrl field len*/
#define  WLANTL_802_11_HEADER_QOS_CTL         2

/*802.11 header definitions - ht ctrl field len*/
#define  WLANTL_802_11_HEADER_HT_CTL          4

/* Offset of Addr1 field in a 802.11 header*/
#define  WLANTL_802_11_HEADER_ADDR1_OFFSET    4

/*802.11 ADDR4 MAC addr field len */
#define  WLANTL_802_11_HEADER_ADDR4_LEN       VOS_MAC_ADDR_SIZE

/* Length of an AMSDU sub-frame */
#define TL_AMSDU_SUBFRM_HEADER_LEN           14

/* Length of the LLC header*/
#define WLANTL_LLC_HEADER_LEN   8

/*As per 802.11 spec */
#define WLANTL_MGMT_FRAME_TYPE       0x00
#define WLANTL_CTRL_FRAME_TYPE       0x10
#define WLANTL_DATA_FRAME_TYPE       0x20

/*Value of the data type field in the 802.11 frame */
#define WLANTL_80211_DATA_TYPE         0x02
#define WLANTL_80211_DATA_QOS_SUBTYPE  0x08
#define WLANTL_80211_NULL_QOS_SUBTYPE  0x0C
#define WLANTL_80211_MGMT_ACTION_SUBTYPE  0x0D
#define WLANTL_80211_MGMT_ACTION_NO_ACK_SUBTYPE  0x0E

/*Defines for internal utility functions */
#define WLANTL_FRAME_TYPE_BCAST 0xff
#define WLANTL_FRAME_TYPE_MCAST 0x01
#define WLANTL_FRAME_TYPE_UCAST 0x00

#define WLANTL_FRAME_TYPESUBTYPE_MASK 0x3F

/*-------------------------------------------------------------------------
  BT-AMP related definition - !!! should probably be moved to BT-AMP header
---------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
  Helper macros
---------------------------------------------------------------------------*/
 /*Checks STA index validity*/
#define WLANTL_STA_ID_INVALID( _staid )( _staid >= WLAN_MAX_STA_COUNT )

/*As per Libra behavior */
#define WLANTL_STA_ID_BCAST     0xFF

/*Checks TID validity*/
#define WLANTL_TID_INVALID( _tid )     ( _tid >= WLAN_MAX_TID )

/*Checks AC validity*/
#define WLANTL_AC_INVALID( _tid )     ( _tid >= WLANTL_MAX_AC )

/*Determines the addr field offset based on the frame xtl bit*/
#define WLANTL_MAC_ADDR_ALIGN( _dxtl )                                    \
      ( ( 0 == _dxtl ) ?                              \
        WLANTL_802_3_HEADER_DA_OFFSET: WLANTL_802_11_HEADER_ADDR1_OFFSET )

/*Determines the header len based on the disable xtl field*/
#define WLANTL_MAC_HEADER_LEN( _dxtl)                                     \
      ( ( 0 == _dxtl )?                               \
         WLANTL_802_3_HEADER_LEN:WLANTL_802_11_HEADER_LEN )

/*Determines the necesary length of the BD header - in case
  UMA translation is enabled enough room needs to be left in front of the
  packet for the 802.11 header to be inserted*/
#define WLANTL_BD_HEADER_LEN( _dxtl )                                    \
      ( ( 0 == _dxtl )?                               \
         (WLANHAL_TX_BD_HEADER_SIZE+WLANTL_802_11_MAX_HEADER_LEN): WLANHAL_TX_BD_HEADER_SIZE )


#define WLAN_TL_CEIL( _a, _b)  (( 0 != (_a)%(_b))? (_a)/(_b) + 1: (_a)/(_b))

/*get TL control block from vos global context */
#define VOS_GET_TL_CB(_pvosGCtx) \
  ((WLANTL_CbType*)vos_get_context( VOS_MODULE_ID_TL, _pvosGCtx))

/* Check whether Rx frame is LS or EAPOL packet (other than data) */
#define WLANTL_BAP_IS_NON_DATA_PKT_TYPE(usType) \
  ((WLANTL_BT_AMP_TYPE_AR == usType) || (WLANTL_BT_AMP_TYPE_SEC == usType) || \
   (WLANTL_BT_AMP_TYPE_LS_REQ == usType) || (WLANTL_BT_AMP_TYPE_LS_REP == usType))

/*get RSSI0 from a RX BD*/
/* 7 bits in phystats represent -100dBm to +27dBm */
#define WLAN_TL_RSSI_CORRECTION 100
#define WLANTL_GETRSSI0(pBD)    (WDA_GETRSSI0(pBD) - WLAN_TL_RSSI_CORRECTION)

/*get RSSI1 from a RX BD*/
#define WLANTL_GETRSSI1(pBD)    (WDA_GETRSSI1(pBD) - WLAN_TL_RSSI_CORRECTION)

#define WLANTL_GETSNR(pBD)      WDA_GET_RX_SNR(pBD)

/* Check whether Rx frame is LS or EAPOL packet (other than data) */
#define WLANTL_BAP_IS_NON_DATA_PKT_TYPE(usType) \
  ((WLANTL_BT_AMP_TYPE_AR == usType) || (WLANTL_BT_AMP_TYPE_SEC == usType) || \
   (WLANTL_BT_AMP_TYPE_LS_REQ == usType) || (WLANTL_BT_AMP_TYPE_LS_REP == usType))

#define WLANTL_CACHE_TRACE_WATERMARK 100
/*---------------------------------------------------------------------------
  TL signals for TX thread
---------------------------------------------------------------------------*/
typedef enum
{
  /*Suspend signal - following serialization of a HAL suspend request*/
  WLANTL_TX_SIG_SUSPEND = 0,

  /*Res need signal - triggered when all pending TxComp have been received
   and TL is low on resources*/
  WLANTL_TX_RES_NEEDED  = 1,

  /* Forwarding RX cached frames. This is not used anymore as it is
     replaced by WLANTL_RX_FWD_CACHED in RX thread*/
  WLANTL_TX_FWD_CACHED  = 2,

  /* Serialized STAID AC Indication */
  WLANTL_TX_STAID_AC_IND = 3,

  /* Serialzie TX transmit request */
  WLANTL_TX_START_XMIT = 4,

  /* Serialzie Finish UL Authentication request */
  WLANTL_FINISH_ULA   = 5,

  /* Serialized Snapshot request indication */
  WLANTL_TX_SNAPSHOT = 6,

  /* Detected a fatal error issue SSR */
  WLANTL_TX_FATAL_ERROR = 7,

  WLANTL_TX_FW_DEBUG = 8,

  WLANTL_TX_MAX
}WLANTL_TxSignalsType;


/*---------------------------------------------------------------------------
  TL signals for RX thread
---------------------------------------------------------------------------*/
typedef enum
{

  /* Forwarding RX cached frames */
  WLANTL_RX_FWD_CACHED  = 0,

}WLANTL_RxSignalsType;

/*---------------------------------------------------------------------------
  STA Event type
---------------------------------------------------------------------------*/
typedef enum
{
  /* Transmit frame event */
  WLANTL_TX_EVENT = 0,

  /* Receive frame event */
  WLANTL_RX_EVENT = 1,

  WLANTL_MAX_EVENT
}WLANTL_STAEventType;

/*---------------------------------------------------------------------------

  DESCRIPTION
    State machine used by transport layer for receiving or transmitting
    packets.

  PARAMETERS

   IN
   pAdapter:        pointer to the global adapter context; a handle to TL's
                    control block can be extracted from its context
   ucSTAId:         identifier of the station being processed
   vosDataBuff:    pointer to the tx/rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

---------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_STAFuncType)( v_PVOID_t     pAdapter,
                                          v_U8_t        ucSTAId,
                                          vos_pkt_t**   pvosDataBuff,
                                          v_BOOL_t      bForwardIAPPwithLLC);

/*---------------------------------------------------------------------------
  STA FSM Entry type
---------------------------------------------------------------------------*/
typedef struct
{
  WLANTL_STAFuncType  pfnSTATbl[WLANTL_MAX_EVENT];
} WLANTL_STAFsmEntryType;

/* Receive in connected state - only EAPOL or WAI*/
VOS_STATUS WLANTL_STARxConn( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* Transmit in connected state - only EAPOL or WAI*/
VOS_STATUS WLANTL_STATxConn( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* Receive in authenticated state - all data allowed*/
VOS_STATUS WLANTL_STARxAuth( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* Transmit in authenticated state - all data allowed*/
VOS_STATUS WLANTL_STATxAuth( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* Receive in disconnected state - no data allowed*/
VOS_STATUS WLANTL_STARxDisc( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* Transmit in disconnected state - no data allowed*/
VOS_STATUS WLANTL_STATxDisc( v_PVOID_t     pAdapter,
                             v_U8_t        ucSTAId,
                             vos_pkt_t**   pvosDataBuff,
                             v_BOOL_t      bForwardIAPPwithLLC);

/* TL State Machine */
STATIC const WLANTL_STAFsmEntryType tlSTAFsm[WLANTL_STA_MAX_STATE] =
{
  /* WLANTL_STA_INIT */
  { {
    NULL,      /* WLANTL_TX_EVENT - no packets should get transmitted*/
    NULL,      /* WLANTL_RX_EVENT - no packets should be received - drop*/
  } },

  /* WLANTL_STA_CONNECTED */
  { {
    WLANTL_STATxConn,      /* WLANTL_TX_EVENT - only EAPoL or WAI frames are allowed*/
    WLANTL_STARxConn,      /* WLANTL_RX_EVENT - only EAPoL or WAI frames can be rx*/
  } },

  /* WLANTL_STA_AUTHENTICATED */
  { {
    WLANTL_STATxAuth,      /* WLANTL_TX_EVENT - all data frames allowed*/
    WLANTL_STARxAuth,      /* WLANTL_RX_EVENT - all data frames can be rx */
  } },

  /* WLANTL_STA_DISCONNECTED */
  { {
    WLANTL_STATxDisc,      /* WLANTL_TX_EVENT - do nothing */
    WLANTL_STARxDisc,      /* WLANTL_RX_EVENT - frames will still be fwd-ed*/
  } }
};

/*---------------------------------------------------------------------------
  Reordering information
---------------------------------------------------------------------------*/

#define WLANTL_MAX_WINSIZE      64
#define WLANTL_MAX_BA_SESSION   40

typedef struct
{
   v_BOOL_t     isAvailable;
   v_U64_t      ullReplayCounter[WLANTL_MAX_WINSIZE];
   v_PVOID_t    arrayBuffer[WLANTL_MAX_WINSIZE];
} WLANTL_REORDER_BUFFER_T;


/* To handle Frame Q aging, timer is needed
 * After timer expired, Qed frames have to be routed to upper layer
 * WLANTL_TIMER_EXPIER_UDATA_T is user data type for timer callback
 */
typedef struct
{
   /* Global contect, HAL, HDD need this */
   v_PVOID_t          pAdapter;

   /* TL context handle */
   v_PVOID_t          pTLHandle;

   /* Current STAID, to know STA context */
   v_U32_t            STAID;

   v_U8_t             TID;
} WLANTL_TIMER_EXPIER_UDATA_T;

typedef struct
{
  /*specifies if re-order session exists*/
  v_U8_t             ucExists;

  /* Current Index */
  v_U32_t             ucCIndex;

  /* Count of the total packets in list*/
  v_U16_t            usCount;

  /* vos ttimer to handle Qed frames aging */
  vos_timer_t        agingTimer;

  /* Q windoe size */
  v_U32_t            winSize;

  /* Available RX frame buffer size */
  v_U32_t            bufferSize;

  /* Start Sequence number */
  v_U32_t            SSN;

  /* BA session ID, generate by HAL */
  v_U32_t            sessionID;

  v_U32_t            currentESN;

  v_U32_t            pendingFramesCount;

  vos_lock_t         reorderLock;

  /* Aging timer callback user data */
  WLANTL_TIMER_EXPIER_UDATA_T timerUdata;

  WLANTL_REORDER_BUFFER_T     *reorderBuffer;

  v_U16_t            LastSN;
}WLANTL_BAReorderType;


/*---------------------------------------------------------------------------
  UAPSD information
---------------------------------------------------------------------------*/
typedef struct
{
  /* flag set when a UAPSD session with triggers generated in fw is being set*/
  v_U8_t              ucSet;
}WLANTL_UAPSDInfoType;

/*---------------------------------------------------------------------------
  per-STA cache info
---------------------------------------------------------------------------*/
typedef struct
{
  v_U16_t               cacheSize;
  v_TIME_t              cacheInitTime;
  v_TIME_t              cacheDoneTime;
  v_TIME_t              cacheClearTime;
}WLANTL_CacheInfoType;


/*---------------------------------------------------------------------------
  STA Client type
---------------------------------------------------------------------------*/
typedef struct
{
  /* Flag that keeps track of registration; only one STA with unique
     ID allowed */
  v_U8_t                        ucExists;

  /* Function pointer to the receive packet handler from HDD */
  WLANTL_STARxCBType            pfnSTARx;

  /* Function pointer to the transmit complete confirmation handler
    from HDD */
  WLANTL_TxCompCBType           pfnSTATxComp;

  /* Function pointer to the packet retrieval routine in HDD */
  WLANTL_STAFetchPktCBType      pfnSTAFetchPkt;

  /* Reordering information for the STA */
  WLANTL_BAReorderType          atlBAReorderInfo[WLAN_MAX_TID];

  /* STA Descriptor, contains information related to the new added STA */
  WLAN_STADescType              wSTADesc;

  /* Current connectivity state of the STA */
  WLANTL_STAStateType           tlState;

  /* Station priority */
  WLANTL_STAPriorityType        tlPri;

  /* Value of the averaged RSSI for this station */
  v_S7_t                        rssiAvg;

  /* Value of the averaged RSSI for this station in BMPS */
  v_S7_t                        rssiAvgBmps;

  /* Value of the Alpha to calculate RSSI average */
  v_S7_t                        rssiAlpha;

  /* Value of the averaged RSSI for this station */
  v_U32_t                       uLinkQualityAvg;

  /* Sum of SNR for snrIdx number of consecutive frames */
  v_U32_t                       snrSum;

  /* Number of consecutive frames over which snrSum is calculated */
  v_S7_t                        snrIdx;

  /* Average SNR of previous 20 frames */
  v_S7_t                        prevSnrAvg;

  /* Average SNR returned by fw */
  v_S7_t                        snrAvgBmps;

  /* Tx packet count per station per TID */
  v_U32_t                       auTxCount[WLAN_MAX_TID];

  /* Rx packet count per station per TID */
  v_U32_t                       auRxCount[WLAN_MAX_TID];

  /* Suspend flag */
  v_U8_t                        ucTxSuspended;

  /* Pointer to the AMSDU chain maintained by the AMSDU de-aggregation
     completion sub-module */
  vos_pkt_t*                    vosAMSDUChainRoot;

  /* Pointer to the root of the chain */
  vos_pkt_t*                    vosAMSDUChain;

  /* Used for saving/restoring frame header for 802.3/11 AMSDU sub-frames */
  v_U8_t                        aucMPDUHeader[WLANTL_MPDU_HEADER_LEN];

  /* length of the header */
  v_U8_t                        ucMPDUHeaderLen;

  /* Enabled ACs currently serviced by TL (automatic setup in TL)*/
  v_U8_t                        aucACMask[WLANTL_MAX_AC];

  /* Current AC to be retrieved */
  WLANTL_ACEnumType             ucCurrentAC;

  /*Packet pending flag - set if tx is pending for the station*/
  v_U8_t                        ucPktPending;

  /*EAPOL Packet pending flag - set if EAPOL packet is pending for the station*/
  v_U8_t                        ucEapolPktPending;

  /*used on tx packet to signal when there is no more data to tx for the
   moment=> packets can be passed to BAL */
  v_U8_t                    ucNoMoreData;

  /* Last serviced AC to be retrieved */
  WLANTL_ACEnumType             ucServicedAC;

   /* Current weight for the AC */
  v_U8_t                        ucCurrentWeight;

  /* Info used for UAPSD trigger frame generation  */
  WLANTL_UAPSDInfoType          wUAPSDInfo[WLANTL_MAX_AC];

  /* flag to signal if a trigger frames is pending */
  v_U8_t                        ucPendingTrigFrm;

  WLANTL_TRANSFER_STA_TYPE      trafficStatistics;

  /*Members needed for packet caching in TL*/
  /*Begining of the cached packets chain*/
  vos_pkt_t*                 vosBegCachedFrame;

  /*Begining of the cached packets chain*/
  vos_pkt_t*                 vosEndCachedFrame;

  WLANTL_CacheInfoType       tlCacheInfo;
  /* LWM related fields */

  v_BOOL_t  enableCaching;

  //current station is slow. LWM mode is enabled.
  v_BOOL_t ucLwmModeEnabled;
  //LWM events is reported when LWM mode is on. Able to send more traffic
  //under the constraints of uBuffThresholdMax
  v_BOOL_t ucLwmEventReported;

  //v_U8_t uLwmEventReported;

  /* Flow control fields */
  //memory used in previous round
  v_U8_t bmuMemConsumed;

  //the number packets injected in this round
  v_U32_t uIngress_length;

  //number of packets allowed in current round beforing receiving new FW memory updates
  v_U32_t uBuffThresholdMax;


  // v_U32_t uEgress_length;

  // v_U32_t uIngress_length;

  // v_U32_t uBuffThresholdMax;

  // v_U32_t uBuffThresholdUsed;

  /* Value used to set LWM in FW. Initialized to 1/3* WLAN_STA_BMU_THRESHOLD_MAX
     In the future, it can be dynamically adjusted if we find the reason to implement
     such algorithm. */
  v_U32_t uLwmThreshold;

  //tx disable forced by Riva software
  v_U16_t fcStaTxDisabled;

  /** HDD buffer status for packet scheduling. Once HDD
   *  stores a new packet in a previously empty queue, it
   *  will call TL interface to set the fields. The fields
   *  will be cleaned by TL when TL can't fetch more packets
   *  from the queue. */
  // the fields are ucPktPending and ucACMask;

  /* Queue to keep unicast station management frame */
  vos_list_t pStaManageQ;

 /* 1 means replay check is needed for the station,
  * 0 means replay check is not needed for the station*/
  v_BOOL_t      ucIsReplayCheckValid;

 /* It contains 48-bit replay counter per TID*/
  v_U64_t       ullReplayCounter[WLANTL_MAX_TID];

 /* It contains no of replay packets found per STA.
    It is for debugging purpose only.*/
  v_U32_t       ulTotalReplayPacketsDetected;

 /* Set when pairwise key is installed, if ptkInstalled is
    1 then we have to encrypt the data irrespective of TL
    state (CONNECTED/AUTHENTICATED) */
  v_U8_t ptkInstalled;

  v_U32_t       linkCapacity;

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

  /* Value of the averaged Data RSSI for this station */
  v_S7_t                        rssiDataAvg;

  /* Value of the averaged Data RSSI for this station in BMPS */
  v_S7_t                        rssiDataAvgBmps;

  /* Value of the Alpha to calculate Data RSSI average */
  v_S7_t                        rssiDataAlpha;

  WLANTL_InterfaceStatsType         interfaceStats;
#endif
}WLANTL_STAClientType;

/*---------------------------------------------------------------------------
  BAP Client type
---------------------------------------------------------------------------*/
typedef struct
{
  /* flag that keeps track of registration; only one non-data BT-AMP client
     allowed */
  v_U8_t                    ucExists;

  /* pointer to the receive processing routine for non-data BT-AMP frames */
  WLANTL_BAPRxCBType        pfnTlBAPRx;

  /* pointer to the flush call back complete function */
  WLANTL_FlushOpCompCBType    pfnFlushOpCompleteCb;

  /* pointer to the non-data BT-AMP frame pending transmission */
  vos_pkt_t*                vosPendingDataBuff;

  /* BAP station ID */
  v_U8_t                    ucBAPSTAId;
}WLANTL_BAPClientType;


/*---------------------------------------------------------------------------
  Management Frame Client type
---------------------------------------------------------------------------*/
typedef struct
{
  /* flag that keeps track of registration; only one management frame
     client allowed */
  v_U8_t                       ucExists;

  /* pointer to the receive processing routine for management frames */
  WLANTL_MgmtFrmRxCBType       pfnTlMgmtFrmRx;

  /* pointer to the management frame pending transmission */
  vos_pkt_t*                   vosPendingDataBuff;
}WLANTL_MgmtFrmClientType;

typedef struct
{
   WLANTL_TrafficStatusChangedCBType  trafficCB;
   WLANTL_HO_TRAFFIC_STATUS_TYPE      trafficStatus;
   v_U32_t                            idleThreshold;
   v_U32_t                            measurePeriod;
   v_U32_t                            rtRXFrameCount;
   v_U32_t                            rtTXFrameCount;
   v_U32_t                            nrtRXFrameCount;
   v_U32_t                            nrtTXFrameCount;
   vos_timer_t                        trafficTimer;
   v_PVOID_t                          usrCtxt;
} WLANTL_HO_TRAFFIC_STATUS_HANDLE_TYPE;

typedef struct
{
   v_S7_t                          rssiValue;
   v_U8_t                          triggerEvent[WLANTL_HS_NUM_CLIENT];
   v_PVOID_t                       usrCtxt[WLANTL_HS_NUM_CLIENT];
   v_U8_t                          whoIsClient[WLANTL_HS_NUM_CLIENT];
   WLANTL_RSSICrossThresholdCBType crossCBFunction[WLANTL_HS_NUM_CLIENT];
   v_U8_t                          numClient;
} WLANTL_HO_RSSI_INDICATION_TYPE;

typedef struct
{
   v_U8_t                             numThreshold;
   v_U8_t                             regionNumber;
   v_S7_t                             historyRSSI;
   v_U8_t                             alpha;
   v_U32_t                            sampleTime;
   v_U32_t                            fwNotification;
} WLANTL_CURRENT_HO_STATE_TYPE;

typedef struct
{
   WLANTL_HO_RSSI_INDICATION_TYPE       registeredInd[WLANTL_MAX_AVAIL_THRESHOLD];
   WLANTL_CURRENT_HO_STATE_TYPE         currentHOState;
   WLANTL_HO_TRAFFIC_STATUS_HANDLE_TYPE currentTraffic;
   v_PVOID_t                            macCtxt;
   vos_lock_t                           hosLock;
} WLANTL_HO_SUPPORT_TYPE;

/*---------------------------------------------------------------------------
  TL control block type
---------------------------------------------------------------------------*/
typedef struct
{
  /* TL configuration information */
  WLANTL_ConfigInfoType     tlConfigInfo;

  /* list of the active stations */
  WLANTL_STAClientType*      atlSTAClients[WLAN_MAX_STA_COUNT];


  /* information on the management frame client */
  WLANTL_MgmtFrmClientType  tlMgmtFrmClient;

  /* information on the BT AMP client */
  WLANTL_BAPClientType      tlBAPClient;

  /* number of packets sent to BAL waiting for tx complete confirmation */
  v_U16_t                   usPendingTxCompleteCount;

  /* global suspend flag */
  v_U8_t                    ucTxSuspended;

  /* resource flag */
  v_U32_t                   uResCount;

  /* dummy vos buffer - used for chains */
  vos_pkt_t*                vosDummyBuf;

  /* temporary buffer for storing the packet that no longer fits */
  vos_pkt_t*                vosTempBuf;

  /* The value of the station id and AC for the cached buffer */
  v_U8_t                    ucCachedSTAId;
  v_U8_t                    ucCachedAC;

  /* Last registered STA - until multiple sta support is added this will
     be used always for tx */
  v_U8_t                    ucRegisteredStaId;

  /*Current TL STA used for TX*/
  v_U8_t                    ucCurrentSTA;

  WLANTL_REORDER_BUFFER_T   *reorderBufferPool; /* Allocate memory for [WLANTL_MAX_BA_SESSION] sessions */

  WLANTL_HO_SUPPORT_TYPE    hoSupport;

  v_BOOL_t                  bUrgent;


  /* resource flag */
  v_U32_t bd_pduResCount;

  /* time interval to evaluate LWM mode*/
  //vos_timer_t tThresholdSamplingTimer;

#if 0
  //information fields for flow control
  tFcTxParams_type tlFCInfo;
#endif

  vos_pkt_t*                vosTxFCBuf;

  /* LWM mode is enabled or not for each station. Bit-wise operation.32 station maximum. */
  //  v_U32_t uStaLwmMask;

  /* LWM event is reported by FW. */
  //  v_U32_t uStaLwmEventReported;

  /** Multiple Station Scheduling and TL queue management.
      4 HDD BC/MC data packet queue status is specified as Station 0's status. Weights used
      in WFQ algorith are initialized in WLANTL_OPEN and contained in tlConfigInfo field.
      Each station has fields of ucPktPending and AC mask to tell whether a AC has traffic
      or not.

      Stations are served in a round-robin fashion from highest priority to lowest priority.
      The number of round-robin times of each prioirty equals to the WFQ weights and differetiates
      the traffic of different prioirty. As such, stations can not provide low priority packets if
      high priority packets are all served.
      */

  /* Currently served station id. Reuse ucCurrentSTA field. */
  //v_U8_t uCurStaId;

  /* Current served station ID in round-robin method to traverse all stations.*/
  WLANTL_ACEnumType uCurServedAC;

  /* How many weights have not been served in current AC. */
  v_U8_t ucCurLeftWeight;

  /* BC/MC management queue. Current implementation uses queue size 1. Will check whether
    size N is supported. */
  vos_list_t pMCBCManageQ;

  v_U32_t sendFCFrame;

  v_U8_t done_once;
  v_U8_t uFramesProcThres;
#ifdef FEATURE_WLAN_TDLS
  /*number of total TDLS peers registered to TL
    Incremented at WLANTL_RegisterSTAClient(staType == WLAN_STA_TDLS)
    Decremented at WLANTL_ClearSTAClient(staType == WLAN_STA_TDLS) */
  v_U8_t        ucTdlsPeerCount;
#endif
  /*whether we are in BMPS/UAPSD/WOWL mode, since the latter 2 need to be BMPS first*/
  v_BOOL_t                  isBMPS;
  /* Whether WDA_DS_TX_START_XMIT msg is pending or not */
  v_BOOL_t   isTxTranmitMsgPending;
}WLANTL_CbType;

/*==========================================================================

  FUNCTION    WLANTL_GetFrames

  DESCRIPTION

    BAL calls this function at the request of the lower bus interface.
    When this request is being received TL will retrieve packets from HDD
    in accordance with the priority rules and the count supplied by BAL.

  DEPENDENCIES

    HDD must have registered with TL at least one STA before this function
    can be called.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    or BAL's control block can be extracted from its context
    uSize:          maximum size accepted by the lower layer
    uFlowMask       TX flow control mask. Each bit is defined as
                    WDA_TXFlowEnumType

    OUT
    vosDataBuff:   it will contain a pointer to the first buffer supplied
                    by TL, if there is more than one packet supplied, TL
                    will chain them through vOSS buffers

  RETURN VALUE

    The result code associated with performing the operation

    1 or more: number of required resources if there are still frames to fetch
               For Volans, it's BD/PDU numbers. For Prima, it's free DXE descriptors.
    0 : error or HDD queues are drained

  SIDE EFFECTS

============================================================================*/
v_U32_t
WLANTL_GetFrames
(
  v_PVOID_t       pAdapter,
  vos_pkt_t     **ppFrameDataBuff,
  v_U32_t         uSize,
  v_U8_t          uFlowMask,
  v_BOOL_t*       pbUrgent
);

/*==========================================================================

  FUNCTION    WLANTL_TxComp

  DESCRIPTION
    It is being called by BAL upon asynchronous notification of the packet
    or packets  being sent over the bus.

  DEPENDENCIES
    Tx complete cannot be called without a previous transmit.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    or BAL's control block can be extracted from its context
    vosDataBuff:   it will contain a pointer to the first buffer for which
                    the BAL report is being made, if there is more then one
                    packet they will be chained using vOSS buffers.
    wTxSTAtus:      the status of the transmitted packet, see above chapter
                    on HDD interaction for a list of possible values

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxComp
(
  v_PVOID_t      pAdapter,
  vos_pkt_t     *pFrameDataBuff,
  VOS_STATUS     wTxStatus
);

/*==========================================================================

  FUNCTION    WLANTL_RxFrames

  DESCRIPTION
    Callback registered by TL and called by BAL when a packet is received
    over the bus. Upon the call of this function TL will make the necessary
    decision with regards to the forwarding or queuing of this packet and
    the layer it needs to be delivered to.

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    If the frame carried is a data frame then the station for which it is
    destined to must have been previously registered with TL.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    or BAL's control block can be extracted from its context

    vosDataBuff:   it will contain a pointer to the first buffer received,
                    if there is more then one packet they will be chained
                    using vOSS buffers.

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxFrames
(
  v_PVOID_t      pAdapter,
  vos_pkt_t     *pFrameDataBuff
);

/*==========================================================================

  FUNCTION    WLANTL_RxCachedFrames

  DESCRIPTION
    Utility function used by TL to forward the cached frames to a particular
    station;

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    If the frame carried is a data frame then the station for which it is
    destined to must have been previously registered with TL.

  PARAMETERS

    IN
    pTLCb:   pointer to TL handle

    ucSTAId:    station for which we need to forward the packets

    vosDataBuff:   it will contain a pointer to the first cached buffer
                   received, if there is more then one packet they will be
                   chained using vOSS buffers.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxCachedFrames
(
  WLANTL_CbType*  pTLCb,
  v_U8_t          ucSTAId,
  vos_pkt_t*      vosDataBuff
);

/*==========================================================================
  FUNCTION    WLANTL_ResourceCB

  DESCRIPTION
    Called by the TL when it has packets available for transmission.

  DEPENDENCIES
    The TL must be registered with BAL before this function can be called.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    or BAL's control block can be extracted from its context
    uCount:         avail resource count obtained from hw

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ResourceCB
(
  v_PVOID_t       pAdapter,
  v_U32_t         uCount
);


/*==========================================================================
  FUNCTION    WLANTL_ProcessMainMessage

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    main thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ProcessMainMessage
(
  v_PVOID_t        pAdapter,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLANTL_ProcessTxMessage

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    tx thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE

    The result code associated with performing the operation
    VOS_STATUS_SUCCESS:  Everything is good :)


  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ProcessTxMessage
(
  v_PVOID_t        pAdapter,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLAN_TLGetNextTxIds

  DESCRIPTION
    Gets the next station and next AC in the list

  DEPENDENCIES

  PARAMETERS

   OUT
   pucSTAId:    STAtion ID

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLAN_TLGetNextTxIds
(
  v_PVOID_t    pAdapter,
  v_U8_t*      pucSTAId
);

/*==========================================================================

  FUNCTION    WLANTL_CleanCb

  DESCRIPTION
    Cleans TL control block

  DEPENDENCIES

  PARAMETERS

    IN
    pTLCb:       pointer to TL's control block
    ucEmpty:     set if TL has to clean up the queues and release pedning pkts

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_CleanCB
(
  WLANTL_CbType*  pTLCb,
  v_U8_t          ucEmpty
);

/*==========================================================================

  FUNCTION    WLANTL_CleanSTA

  DESCRIPTION
    Cleans a station control block.

  DEPENDENCIES

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to TL's
                    control block can be extracted from its context
    ucEmpty:        if set the queues and pending pkts will be emptyed

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_CleanSTA
(
  WLANTL_STAClientType*  ptlSTAClient,
  v_U8_t                 ucEmpty
);

/*==========================================================================
  FUNCTION    WLANTL_GetTxResourcesCB

  DESCRIPTION
    Processing function for Resource needed signal. A request will be issued
    to BAL to get mor tx resources.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check
  corresponding API for more info.
  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetTxResourcesCB
(
  v_PVOID_t        pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANTL_PrepareBDHeader

  DESCRIPTION
    Callback function for serializing Suspend signal through Tx thread

  DEPENDENCIES
    Just notify HAL that suspend in TL is complete.

  PARAMETERS

   IN
   pAdapter:       pointer to the global adapter context; a handle to TL's
                   control block can be extracted from its context
   pUserData:      user data sent with the callback

  RETURN VALUE
    The result code associated with performing the operation

  SIDE EFFECTS

============================================================================*/
void
WLANTL_PrepareBDHeader
(
  vos_pkt_t*      vosDataBuff,
  v_PVOID_t*      ppvBDHeader,
  v_MACADDR_t*    pvDestMacAdddr,
  v_U8_t          ucDisableFrmXtl,
  VOS_STATUS*     pvosSTAtus,
  v_U16_t*        usPktLen,
  v_U8_t          ucQosEnabled,
  v_U8_t          ucWDSEnabled,
  v_U8_t          extraHeadSpace
);

/*==========================================================================
  FUNCTION    WLANTL_Translate8023To80211Header

  DESCRIPTION
    Inline function for translating and 802.3 header into an 802.11 header.

  DEPENDENCIES


  PARAMETERS

   IN
    pTLCb:            TL control block

    *pucStaId         Station ID. In case of TDLS, this return the actual
                      station index used to transmit.

   IN/OUT
    vosDataBuff:      vos data buffer, will contain the new header on output

   OUT
    pvosStatus:       status of the operation

  RETURN VALUE
    No return.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Translate8023To80211Header
(
  vos_pkt_t*      vosDataBuff,
  VOS_STATUS*     pvosStatus,
  WLANTL_CbType*  pTLCb,
  v_U8_t          *pucStaId,
  WLANTL_MetaInfoType* pTlMetaInfo,
  v_U8_t          *ucWDSEnabled,
  v_U8_t          *extraHeadSpace
);
/*==========================================================================
  FUNCTION    WLANTL_Translate80211To8023Header

  DESCRIPTION
    Inline function for translating and 802.11 header into an 802.3 header.

  DEPENDENCIES


  PARAMETERS

   IN
    pTLCb:            TL control block
    ucStaId:          station ID
    ucHeaderLen:      Length of the header from BD
    ucActualHLen:     Length of header including padding or any other trailers

   IN/OUT
    vosDataBuff:      vos data buffer, will contain the new header on output

   OUT
    pvosStatus:       status of the operation

  RETURN VALUE
    Status of the operation

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Translate80211To8023Header
(
  vos_pkt_t*      vosDataBuff,
  VOS_STATUS*     pvosStatus,
  v_U16_t         usActualHLen,
  v_U8_t          ucHeaderLen,
  WLANTL_CbType*  pTLCb,
  v_U8_t          ucSTAId,
  v_BOOL_t	  bForwardIAPPwithLLC
);

/*==========================================================================
  FUNCTION    WLANTL_FindFrameTypeBcMcUc

  DESCRIPTION
    Utility function to find whether received frame is broadcast, multicast
    or unicast.

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pTLCb:          pointer to the TL's control block
   ucSTAId:        identifier of the station being processed
   vosDataBuff:    pointer to the vos buffer

   IN/OUT
    pucBcMcUc:       pointer to buffer, will contain frame type on return

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_BADMSG:  failed to extract info from data buffer
    VOS_STATUS_SUCCESS:   success

  SIDE EFFECTS
    None.
============================================================================*/
VOS_STATUS
WLANTL_FindFrameTypeBcMcUc
(
  WLANTL_CbType *pTLCb,
  v_U8_t        ucSTAId,
  vos_pkt_t     *vosDataBuff,
  v_U8_t        *pucBcMcUc
);

/*==========================================================================

  FUNCTION    WLANTL_MgmtFrmRxDefaultCb

  DESCRIPTION
    Default Mgmt Frm rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for Mgmt Frm.

  DEPENDENCIES

  PARAMETERS

    Not used.

  RETURN VALUE
   Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_MgmtFrmRxDefaultCb
(
  v_PVOID_t  pAdapter,
  v_PVOID_t  vosBuff
);

/*==========================================================================

  FUNCTION    WLANTL_STARxDefaultCb

  DESCRIPTION
    Default BAP rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for BAP.

  DEPENDENCIES

  PARAMETERS

    Not used.

  RETURN VALUE
   Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_BAPRxDefaultCb
(
  v_PVOID_t    pAdapter,
  vos_pkt_t*   vosDataBuff,
  WLANTL_BAPFrameEnumType frameType
);

/*==========================================================================

  FUNCTION    WLANTL_STARxDefaultCb

  DESCRIPTION
    Default STA rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for station.
    (Mem corruption most likely, it should never happen)

  DEPENDENCIES

  PARAMETERS

    Not used.

  RETURN VALUE
   Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_STARxDefaultCb
(
  v_PVOID_t               pAdapter,
  vos_pkt_t*              vosDataBuff,
  v_U8_t                  ucSTAId,
  WLANTL_RxMetaInfoType*  pRxMetaInfo
);

/*==========================================================================

  FUNCTION    WLANTL_STAFetchPktDefaultCb

  DESCRIPTION
    Default fetch callback: asserts all the time. If this function gets
    called  it means there is no registered fetch cb pointer for station.
    (Mem corruption most likely, it should never happen)

  DEPENDENCIES

  PARAMETERS

    Not used.

  RETURN VALUE
   Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_STAFetchPktDefaultCb
(
  v_PVOID_t              pAdapter,
  v_U8_t*                pucSTAId,
  WLANTL_ACEnumType      ucAC,
  vos_pkt_t**            vosDataBuff,
  WLANTL_MetaInfoType*   tlMetaInfo
);

/*==========================================================================

  FUNCTION    WLANTL_TxCompDefaultCb

  DESCRIPTION
    Default tx complete handler. It will release the completed pkt to
    prevent memory leaks.

  PARAMETERS

    IN
    pAdapter:       pointer to the global adapter context; a handle to
                    TL/HAL/PE/BAP/HDD control block can be extracted from
                    its context
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted
    wTxSTAtus:      status of the transmission


  RETURN VALUE
    The result code associated with performing the operation; please
    check vos_pkt_return_pkt for possible error codes.

============================================================================*/
VOS_STATUS
WLANTL_TxCompDefaultCb
(
 v_PVOID_t      pAdapter,
 vos_pkt_t*     vosDataBuff,
 VOS_STATUS     wTxSTAtus
);

/*==========================================================================

  FUNCTION    WLANTL_PackUpTriggerFrame

  DESCRIPTION
    Packs up a trigger frame and places it in TL's cache for tx and notifies
    BAL

  DEPENDENCIES

  PARAMETERS

  IN
    pTLCb:         pointer to the TL control block
    pfnSTATxComp:  Tx Complete Cb to be used when frame is received
    ucSTAId:       station id
    ucAC:          access category

  RETURN VALUE
    None

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_PackUpTriggerFrame
(
  WLANTL_CbType*            pTLCb,
  WLANTL_TxCompCBType       pfnSTATxComp,
  v_U8_t                    ucSTAId,
  WLANTL_ACEnumType         ucAC
);

/*==========================================================================

  FUNCTION    WLANTL_TxCompTriggFrameSI

  DESCRIPTION
    Tx complete handler for the service interval trigger frame.
    It will restart the SI timer.

  PARAMETERS

   IN
    pvosGCtx:       pointer to the global vos context; a handle to
                    TL/HAL/PE/BAP/HDD control block can be extracted from
                    its context
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted
    wTxSTAtus:      status of the transmission


  RETURN VALUE
    The result code associated with performing the operation

 ============================================================================*/
VOS_STATUS
WLANTL_TxCompTriggFrameSI
(
  v_PVOID_t      pvosGCtx,
  vos_pkt_t*     vosDataBuff,
  VOS_STATUS     wTxSTAtus
);

/*==========================================================================

  FUNCTION    WLANTL_TxCompTriggFrameSI

  DESCRIPTION
    Tx complete handler for the service interval trigger frame.
    It will restart the SI timer.

  PARAMETERS

   IN
    pvosGCtx:       pointer to the global vos context; a handle to
                    TL/HAL/PE/BAP/HDD control block can be extracted from
                    its context
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted
    wTxSTAtus:      status of the transmission


  RETURN VALUE
    The result code associated with performing the operation

============================================================================*/
VOS_STATUS
WLANTL_TxCompTriggFrameDI
(
 v_PVOID_t      pvosGCtx,
 vos_pkt_t*     vosDataBuff,
 VOS_STATUS     wTxSTAtus
);

/*==========================================================================

   FUNCTION

   DESCRIPTION   Read RSSI value out of a RX BD

   PARAMETERS: Caller must validate all parameters

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_ReadRSSI
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid
);

/*==========================================================================

   FUNCTION

   DESCRIPTION   Read SNR value out of a RX BD

   PARAMETERS: Caller must validate all parameters

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_ReadSNR
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid
);


void WLANTL_PowerStateChangedCB
(
   v_PVOID_t pAdapter,
   tPmcState newState
);

/*==========================================================================
  FUNCTION   WLANTL_FwdPktToHDD

  DESCRIPTION
    Determine the Destation Station ID and route the Frame to Upper Layer

  DEPENDENCIES

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_FwdPktToHDD
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t*     pvosDataBuff,
  v_U8_t          ucSTAId
);

#endif /* #ifndef WLAN_QCT_TLI_H */
