/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef WLAN_QCT_WLANTL_H
#define WLAN_QCT_WLANTL_H

/*===========================================================================

               W L A N   T R A N S P O R T   L A Y E R
                       E X T E R N A L  A P I


DESCRIPTION
  This file contains the external API exposed by the wlan transport layer
  module.
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
01/08/10    lti     Added TL Data Caching 
10/15/09    rnair   Modifying STADescType struct
10/06/09    rnair   Adding support for WAPI 
09/22/09    lti     Add deregistration API for management client
02/02/09    sch     Add Handoff support
12/09/08    lti     Fixes for AMSS compilation 
09/05/08    lti     Fixes after QOS unit testing 
08/06/08    lti     Added QOS support 
05/01/08    lti     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_api.h" 
#include "vos_packet.h" 
#include "sirApi.h"
#include "csrApi.h"
#include "sapApi.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 
/*Offset of the OUI field inside the LLC/SNAP header*/
#define WLANTL_LLC_OUI_OFFSET                 3

/*Size of the OUI type field inside the LLC/SNAP header*/
#define WLANTL_LLC_OUI_SIZE                   3

/*Offset of the LLC/SNAP header*/
#define WLANTL_LLC_SNAP_OFFSET                0

/*Size of the LLC/SNAP header*/
#define WLANTL_LLC_SNAP_SIZE                   8

/*============================================================================
 *     GENERIC STRUCTURES - not belonging to TL 
 *     TO BE MOVED TO A GLOBAL HEADER
 ============================================================================*/
/*Maximum number of ACs */
#define WLANTL_MAX_AC                         4

/* Maximum number of station supported by TL, including BC. */
#define WLAN_MAX_STA_COUNT  (HAL_NUM_STA)
#define WLAN_NON32_STA_COUNT   14
/* The symbolic station ID return to HDD to specify the packet is bc/mc */
#define WLAN_RX_BCMC_STA_ID (WLAN_MAX_STA_COUNT + 1)

/* The symbolic station ID return to HDD to specify the packet is to soft-AP itself */
#define WLAN_RX_SAP_SELF_STA_ID (WLAN_MAX_STA_COUNT + 2)

/* Used by HDS systme. This station ID is used by TL to tell upper layer that
   this packet is for WDS and not for a loopback for an associated station. */
#define WLANTL_RX_WDS_STAID WLAN_MAX_STA_COUNT

/* Station ID used for BC traffic. This value will be used when upper layer registers
   the broadcast client or allocate station strcuture to keep per-station info.*/
//#define WLANTL_BC_STA_ID  0x00


#define WLANTL_MAX_TID                        15
/* Default RSSI average Alpha */
#define WLANTL_HO_DEFAULT_ALPHA               5
#define WLANTL_HO_TDLS_ALPHA                  7

// Choose the largest possible value that can be accomodates in 8 bit signed
// variable.
#define SNR_HACK_BMPS                         (127)
/*--------------------------------------------------------------------------
  Access category enum used by TL
  - order must be kept as these values are used to setup the AC mask
 --------------------------------------------------------------------------*/
typedef enum
{
  WLANTL_AC_BK = 0,
  WLANTL_AC_BE = 1,
  WLANTL_AC_VI = 2,
  WLANTL_AC_VO = 3
}WLANTL_ACEnumType; 

/*---------------------------------------------------------------------------
  STA Type
---------------------------------------------------------------------------*/
typedef enum
{
  /* Indicates a link to an AP*/
  WLAN_STA_INFRA  = 0,  

  /* AD-hoc link*/
  WLAN_STA_IBSS,   

  /* BT-AMP link*/
  WLAN_STA_BT_AMP,

  /* SoftAP station */
  WLAN_STA_SOFTAP,

#ifdef FEATURE_WLAN_TDLS
  /* TDLS direct link */
  WLAN_STA_TDLS,    /* 4 */
#endif


  /* Invalid link*/
  WLAN_STA_MAX

}WLAN_STAType;

/*---------------------------------------------------------------------------
  BAP Management frame type
---------------------------------------------------------------------------*/
typedef enum
{
    /* BT-AMP packet of type data */
    WLANTL_BT_AMP_TYPE_DATA = 0x0001,

    /* BT-AMP packet of type activity report */
    WLANTL_BT_AMP_TYPE_AR = 0x0002,

    /* BT-AMP packet of type security frame */
    WLANTL_BT_AMP_TYPE_SEC = 0x0003,

    /* BT-AMP packet of type Link Supervision request frame */
    WLANTL_BT_AMP_TYPE_LS_REQ = 0x0004,

    /* BT-AMP packet of type Link Supervision reply frame */
    WLANTL_BT_AMP_TYPE_LS_REP = 0x0005,

    /* Invalid Frame */
    WLANTL_BAP_INVALID_FRAME

} WLANTL_BAPFrameEnumType;

/* Type used to specify LWM threshold unit */
typedef enum  {
    WLAN_LWM_THRESHOLD_BYTE = 0,

    WLAN_LWM_THRESHOLD_PACKET
} WLAN_LWM_Threshold_Type;

/*---------------------------------------------------------------------------
  TL States
---------------------------------------------------------------------------*/
typedef enum
{
  /* Transition in this state made upon creation*/
  WLANTL_STA_INIT = 0,

  /* Transition happens after Assoc success if second level authentication
     is needed*/
  WLANTL_STA_CONNECTED,

  /* Transition happens when second level auth is successful and keys are
     properly installed */
  WLANTL_STA_AUTHENTICATED,

  /* Transition happens when connectivity is lost*/
  WLANTL_STA_DISCONNECTED,

  WLANTL_STA_MAX_STATE
}WLANTL_STAStateType;


/*---------------------------------------------------------------------------
  STA Descriptor Type
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA unique identifier, originating from HAL*/
  v_U8_t         ucSTAId; 

  /*STA MAC Address*/
  v_MACADDR_t    vSTAMACAddress; 

  /*BSSID for IBSS*/
  v_MACADDR_t    vBSSIDforIBSS; 

  /*Self MAC Address*/
  v_MACADDR_t    vSelfMACAddress;

  /*Type of the STA*/
  WLAN_STAType   wSTAType; 

  /*flag for setting the state of the QOS for the link*/
  v_U8_t         ucQosEnabled;

  /*enable FT in TL */
  v_U8_t         ucSwFrameTXXlation; 
  v_U8_t         ucSwFrameRXXlation;

  /*Flag for signaling TL if LLC header needs to be added for outgoing 
    packets*/
  v_U8_t         ucAddRmvLLC;

 /*Flag for signaling if the privacy bit needs to be set*/
  v_U8_t         ucProtectedFrame;

 /*DPU Signature used for unicast data - used for data caching*/
  v_U8_t              ucUcastSig;
 /*Flag to indicate if STA is a WAPI STA*/
  v_U8_t         ucIsWapiSta;

#ifdef FEATURE_WLAN_CCX
 /*Flag to indicate if STA is a CCX STA*/
  v_U8_t         ucIsCcxSta;
#endif

  /*DPU Signature used for broadcast data - used for data caching*/
  v_U8_t              ucBcastSig;

  /*Initial state at which the STA should be brought up to*/
  WLANTL_STAStateType ucInitState;
 /* 1 means replay check is needed for the station,
    0 means replay check is not needed for the station*/ 
  v_BOOL_t      ucIsReplayCheckValid; 
}WLAN_STADescType;

/*---------------------------------------------------------------------------
  TL Configuration
---------------------------------------------------------------------------*/      
typedef struct
{
  /*AC weight for WFQ*/
  v_U8_t   ucAcWeights[WLANTL_MAX_AC]; 

  /*Delayed trigger frame timmer: - used by TL to send trigger frames less 
    often when it has established that the App is suspended*/
  v_U32_t  uDelayedTriggerFrmInt;  

  /* Min Threshold for Processing Frames in TL */
  v_U8_t   uMinFramesProcThres;

  /* Re-order Aging Time */
  v_U16_t  ucReorderAgingTime[WLANTL_MAX_AC];
}WLANTL_ConfigInfoType;

/*---------------------------------------------------------------------------
  TSPEC Direction Enum Type
---------------------------------------------------------------------------*/
typedef enum
{
  /* uplink */
  WLANTL_TX_DIR = 0,

  /* downlink */
  WLANTL_RX_DIR = 1,

  /*bidirectional*/
  WLANTL_BI_DIR = 2,
}WLANTL_TSDirType;

/*============================================================================
 *     GENERIC STRUCTURES - END
 ============================================================================*/



/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  TL Error Type 
---------------------------------------------------------------------------*/      
typedef enum
{
  /* Generic error */
  WLANTL_ERROR = 0,      

  /* No rx callback registered for data path */
  WLANTL_NO_RX_DATA_CB,  

  /* No rx callback registered for management path*/
  WLANTL_NO_RX_MGMT_CB,  

  /* Generic memory error*/
  WLANTL_MEM_ERROR,      

  /* Bus error notified by BAL */
  WLANTL_BUS_ERROR       

}WLANTL_ErrorType;

/*---------------------------------------------------------------------------
  STA priority type
---------------------------------------------------------------------------*/      
typedef enum 
{
  /* STA gets to tx every second round*/
  WLANTL_STA_PRI_VERY_LOW  = -2, 

  /* STA gets to tx every other round*/
  WLANTL_STA_PRI_LOW       = -1, 

  /* STA gets to tx each time */
  WLANTL_STA_PRI_NORMAL    =  0, 

  /* STA gets to tx twice each time*/ 
  WLANTL_STA_PRI_HIGH      =  1, 

  /* STA gets to tx three times each time*/
  WLANTL_STA_PRI_VERY_HIGH =  2  

}WLANTL_STAPriorityType;

/*---------------------------------------------------------------------------
  Meta information requested from HDD by TL 
---------------------------------------------------------------------------*/      
typedef struct
{
  /* TID of the packet being sent */
  v_U8_t    ucTID;

  /* UP of the packet being sent */
  v_U8_t    ucUP;

  /* notifying TL if this is an EAPOL frame or not */
  v_U8_t    ucIsEapol;
#ifdef FEATURE_WLAN_WAPI
  /* notifying TL if this is a WAI frame or not */
  v_U8_t    ucIsWai;
#endif
  /* frame is 802.11 and it does not need translation */
  v_U8_t    ucDisableFrmXtl;

  /* frame is broadcast */
  v_U8_t    ucBcast;

  /* frame is multicast */
  v_U8_t    ucMcast;

  /* frame type */
  v_U8_t    ucType;

  /* timestamp */
  v_U16_t   usTimeStamp;

  /* STA has more packets to send */
  v_BOOL_t  bMorePackets;
  /* notifying TL if this is an ARP frame or not */
  v_U8_t    ucIsArp;
}WLANTL_MetaInfoType;

/*---------------------------------------------------------------------------
  Meta information provided by TL to HDD on rx path  
---------------------------------------------------------------------------*/      
typedef struct
{
  /* UP of the packet being sent */
  v_U8_t    ucUP;
  /* Address 3 Index of the received packet */
  v_U16_t   ucDesSTAId;
 /*Rssi based on the received packet */
  v_S7_t    rssiAvg;
 #ifdef FEATURE_WLAN_TDLS
 /* Packet received on direct link/AP link */
  v_U8_t    isStaTdls;
 #endif
}WLANTL_RxMetaInfoType;


/*---------------------------------------------------------------------------
  Handoff support and statistics defines and enum types
---------------------------------------------------------------------------*/
/* Threshold crossed event type definitions */
#define WLANTL_HO_THRESHOLD_NA    0x00
#define WLANTL_HO_THRESHOLD_DOWN  0x01
#define WLANTL_HO_THRESHOLD_UP    0x02
#define WLANTL_HO_THRESHOLD_CROSS 0x04

/* Realtime traffic status */
typedef enum
{
   WLANTL_HO_RT_TRAFFIC_STATUS_OFF,
   WLANTL_HO_RT_TRAFFIC_STATUS_ON
} WLANTL_HO_RT_TRAFFIC_STATUS_TYPE;

/* Non-Realtime traffic status */
typedef enum
{
   WLANTL_HO_NRT_TRAFFIC_STATUS_OFF,
   WLANTL_HO_NRT_TRAFFIC_STATUS_ON
} WLANTL_HO_NRT_TRAFFIC_STATUS_TYPE;

/* Statistics type TL supported */
typedef enum
{
   WLANTL_STATIC_TX_UC_FCNT,
   WLANTL_STATIC_TX_MC_FCNT,
   WLANTL_STATIC_TX_BC_FCNT,
   WLANTL_STATIC_TX_UC_BCNT,
   WLANTL_STATIC_TX_MC_BCNT,
   WLANTL_STATIC_TX_BC_BCNT,
   WLANTL_STATIC_RX_UC_FCNT,
   WLANTL_STATIC_RX_MC_FCNT,
   WLANTL_STATIC_RX_BC_FCNT,
   WLANTL_STATIC_RX_UC_BCNT,
   WLANTL_STATIC_RX_MC_BCNT,
   WLANTL_STATIC_RX_BC_BCNT,
   WLANTL_STATIC_RX_BCNT,
   WLANTL_STATIC_RX_BCNT_CRC_OK,
   WLANTL_STATIC_RX_RATE
} WLANTL_TRANSFER_STATIC_TYPE;

/*---------------------------------------------------------------------------
  Handoff support and statistics structures
---------------------------------------------------------------------------*/
typedef struct
{
   WLANTL_HO_RT_TRAFFIC_STATUS_TYPE   rtTrafficStatus;
   WLANTL_HO_NRT_TRAFFIC_STATUS_TYPE  nrtTrafficStatus;
} WLANTL_HO_TRAFFIC_STATUS_TYPE;

typedef tSap_SoftapStats WLANTL_TRANSFER_STA_TYPE;

/* Under here not public items, just use for internal */
/* 3 SME 1 HDD */
#define WLANTL_MAX_AVAIL_THRESHOLD   5
#define WLANTL_HS_NUM_CLIENT         2
#define WLANTL_SINGLE_CLNT_THRESHOLD 4

/*----------------------------------------------------------------------------
 *   TL callback types
 *--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Type of the tx complete callback registered with TL. 
    
    TL will call this to notify the client when a transmission for a 
    packet  has ended. 

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to 
                    TL/HAL/PE/BAP/HDD control block can be extracted from 
                    its context 
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted 
    wTxSTAtus:      status of the transmission 

  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_TxCompCBType)( v_PVOID_t      pvosGCtx,
                                           vos_pkt_t*     pFrameDataBuff,
                                           VOS_STATUS     wTxSTAtus );


/*----------------------------------------------------------------------------
    INTERACTION WITH HDD
 ---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------

  DESCRIPTION   
    Type of the fetch packet callback registered with TL. 
    
    It is called by the TL when the scheduling algorithms allows for 
    transmission of another packet to the module. 
    It will be called in the context of the BAL fetch transmit packet 
    function, initiated by the bus lower layer. 


  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle 
                    to TL's or HDD's control block can be extracted 
                    from its context 

    IN/OUT
    pucSTAId:       the Id of the station for which TL is requesting a 
                    packet, in case HDD does not maintain per station 
                    queues it can give the next packet in its queue 
                    and put in the right value for the 
    pucAC:          access category requested by TL, if HDD does not have 
                    packets on this AC it can choose to service another AC 
                    queue in the order of priority

    OUT
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted 
    tlMetaInfo:    meta info related to the data frame


  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_STAFetchPktCBType)( 
                                            v_PVOID_t             pvosGCtx,
                                            v_U8_t*               pucSTAId,
                                            WLANTL_ACEnumType     ucAC,
                                            vos_pkt_t**           vosDataBuff,
                                            WLANTL_MetaInfoType*  tlMetaInfo);

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Type of the receive callback registered with TL. 
    
    TL will call this to notify the client when a packet was received 
    for a registered STA.

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to 
                    TL's or HDD's control block can be extracted from 
                    its context 
    vosDataBuff:   pointer to the VOSS data buffer that was received
                    (it may be a linked list) 
    ucSTAId:        station id
    pRxMetaInfo:   meta info for the received packet(s) 
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_STARxCBType)( v_PVOID_t              pvosGCtx,
                                          vos_pkt_t*             vosDataBuff,
                                          v_U8_t                 ucSTAId,
                                          WLANTL_RxMetaInfoType* pRxMetaInfo);


/*----------------------------------------------------------------------------
    INTERACTION WITH BAP
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Type of the receive callback registered with TL for BAP. 
    
    The registered reception callback is being triggered by TL whenever a 
    frame was received and it was filtered as a non-data BT AMP packet. 

  PARAMETERS 

    IN
    pvosGCtx:      pointer to the global vos context; a handle to TL's 
                   or SME's control block can be extracted from its context 
    vosDataBuff:   pointer to the vOSS buffer containing the received packet; 
                   no chaining will be done on this path
    frameType:     type of the frame to be indicated to BAP.
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_BAPRxCBType)( v_PVOID_t               pvosGCtx,
                                          vos_pkt_t*              vosDataBuff,
                                          WLANTL_BAPFrameEnumType frameType);

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Callback registered with TL for BAP, this is required inorder for
    TL to inform BAP, that the flush operation requested has been completed. 
    
    The registered reception callback is being triggered by TL whenever a 
    frame SIR_TL_HAL_FLUSH_AC_RSP is received by TL from HAL.

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    vosDataBuff:   pointer to the vOSS buffer containing the received packet; 
                    no chaining will be done on this path 
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_FlushOpCompCBType)( v_PVOID_t     pvosGCtx,
                                                v_U8_t        ucStaId,
                                                v_U8_t        ucTID, 
                                                v_U8_t        status);
/*----------------------------------------------------------------------------
    INTERACTION WITH PE
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Type of the receive callback registered with TL for PE. 
    
    Upon receipt of a management frame TL will call the registered receive 
    callback and forward this frame to the interested module, in our case PE. 

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    vosFrmBuf:     pointer to a vOSS buffer containing the management frame 
                    received
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_MgmtFrmRxCBType)( v_PVOID_t  pvosGCtx, 
                                              v_PVOID_t  vosBuff);


/*----------------------------------------------------------------------------
    INTERACTION WITH HAL
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  DESCRIPTION 
    Type of the fetch packet callback registered with TL. 
    
    HAL calls this API when it wishes to suspend transmission for a 
    particular STA.
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier of the station for which the request is made; 
                    a value of 0 assumes suspend on all active station
    pfnSuspendTxCB: pointer to the suspend result notification in case the 
                    call is asynchronous
   
  RETURN VALUE
    The result code associated with performing the operation  
  
----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANTL_SuspendCBType)( v_PVOID_t      pvosGCtx,
                                            v_U8_t*        ucSTAId,
                                            VOS_STATUS     vosStatus);


/*==========================================================================

  DESCRIPTION 
    Traffic status changed callback function
    Should be registered to let client know that traffic status is changed
    REF WLANTL_RegGetTrafficStatus

  PARAMETERS 
    pAdapter       Global handle pointer
    trafficStatus  RT and NRT current traffic status
    pUserCtxt      pre registered client context
   
  RETURN VALUE
    VOS_STATUS

  SIDE EFFECTS 
    NONE
 
============================================================================*/
/* IF traffic status is changed, send notification to SME */
typedef VOS_STATUS (*WLANTL_TrafficStatusChangedCBType)
(
   v_PVOID_t                     pAdapter,
   WLANTL_HO_TRAFFIC_STATUS_TYPE trafficStatus,
   v_PVOID_t                     pUserCtxt
);

/*==========================================================================

  DESCRIPTION 
    RSSI threshold crossed notification callback function
    REF WLANTL_RegRSSIIndicationCB

  PARAMETERS 
    pAdapter          Global handle pointer
    rssiNotification  Notification event type
    pUserCtxt         pre registered client context

  RETURN VALUE

  SIDE EFFECTS 
  
============================================================================*/
/* If RSSI realm is changed, send notification to Clients, SME, HDD */
typedef VOS_STATUS (*WLANTL_RSSICrossThresholdCBType)
(
   v_PVOID_t                       pAdapter,
   v_U8_t                          rssiNotification,
   v_PVOID_t                       pUserCtxt,
   v_S7_t                          avgRssi
);

typedef struct
{
    // Common for all types are requests
    v_U16_t                         msgType;    // message type is same as the request type
    v_U16_t                         msgLen;  // length of the entire request
    v_U8_t                          sessionId; //sme Session Id
    v_U8_t                          rssiNotification;    
    v_U8_t                          avgRssi;
    v_PVOID_t                       tlCallback;
    v_PVOID_t                       pAdapter;
    v_PVOID_t                       pUserCtxt;
} WLANTL_TlIndicationReq;

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_Open

  DESCRIPTION 
    Called by HDD at driver initialization. TL will initialize all its 
    internal resources and will wait for the call to start to register 
    with the other modules. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    pTLConfig:      TL Configuration 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANTL_Open
( 
  v_PVOID_t               pvosGCtx,
  WLANTL_ConfigInfoType*  pTLConfig
);

/*==========================================================================

  FUNCTION    WLANTL_Start

  DESCRIPTION 
    Called by HDD as part of the overall start procedure. TL will use this 
    call to register with BAL as a transport layer entity. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other codes can be returned as a result of a BAL failure; see BAL API 
    for more info
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANTL_Start
( 
  v_PVOID_t  pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANTL_Stop

  DESCRIPTION 
    Called by HDD to stop operation in TL, before close. TL will suspend all 
    frame transfer operation and will wait for the close request to clean up 
    its resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANTL_Stop
( 
  v_PVOID_t  pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANTL_Close

  DESCRIPTION 
    Called by HDD during general driver close procedure. TL will clean up 
    all the internal resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANTL_Close
( 
  v_PVOID_t  pvosGCtx 
);

/*===========================================================================

  FUNCTION    WLANTL_StartForwarding

  DESCRIPTION

    This function is used to ask serialization through TX thread of the
    cached frame forwarding (if statation has been registered in the mean while)
    or flushing (if station has not been registered by the time)

    In case of forwarding, upper layer is only required to call WLANTL_RegisterSTAClient()
    and doesn't need to call this function explicitly. TL will handle this inside
    WLANTL_RegisterSTAClient().

    In case of flushing, upper layer is required to call this function explicitly

  DEPENDENCIES

    TL must have been initialized before this gets called.


  PARAMETERS

   ucSTAId:   station id

  RETURN VALUE

    The result code associated with performing the operation
    Please check return values of vos_tx_mq_serialize.

  SIDE EFFECTS
    If TL was asked to perform WLANTL_CacheSTAFrame() in WLANTL_RxFrames(),
    either WLANTL_RegisterSTAClient() or this function must be called
    within reasonable time. Otherwise, TL will keep cached vos buffer until
    one of this function is called, and may end up with system buffer exhasution.

    It's an upper layer's responsibility to call this function in case of
    flushing

============================================================================*/
VOS_STATUS
WLANTL_StartForwarding
(
  v_U8_t ucSTAId,
  v_U8_t ucUcastSig,
  v_U8_t ucBcastSig
);

/*----------------------------------------------------------------------------
    INTERACTION WITH HDD
 ---------------------------------------------------------------------------*/
/*==========================================================================

  FUNCTION    WLANTL_ConfigureSwFrameTXXlationForAll

  DESCRIPTION
     Function to disable/enable frame translation for all association stations.

  DEPENDENCIES

  PARAMETERS
   IN
    pvosGCtx:           VOS context 
    EnableFrameXlation TRUE means enable SW translation for all stations.
    .

  RETURN VALUE

   void.

============================================================================*/
void
WLANTL_ConfigureSwFrameTXXlationForAll
(
  v_PVOID_t pvosGCtx, 
  v_BOOL_t enableFrameXlation
);

/*===========================================================================

  FUNCTION    WLANTL_RegisterSTAClient

  DESCRIPTION 

    This function is used by HDD to register as a client for data services 
    with TL. HDD will call this API for each new station that it adds, 
    thus having the flexibility of registering different callback for each 
    STA it services. 

  DEPENDENCIES 

    TL must have been initialized before this gets called.
     
    Restriction: 
      Main thread will have higher priority that Tx and Rx threads thus 
      guaranteeing that a station will be added before any data can be 
      received for it. (This enables TL to be lock free) 

  PARAMETERS 

   pvosGCtx:        pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   pfnStARx:        function pointer to the receive packet handler from HDD
   pfnSTATxComp:    function pointer to the transmit complete confirmation 
                    handler from HDD 
   pfnSTAFetchPkt:  function pointer to the packet retrieval routine in HDD 
   wSTADescType:    STA Descriptor, contains information related to the 
                    new added STA
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL: Input parameters are invalid 
    VOS_STATUS_E_FAULT: Station ID is outside array boundaries or pointer to 
                        TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: Station was already registered
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_RegisterSTAClient 
( 
  v_PVOID_t                 pvosGCtx,
  WLANTL_STARxCBType        pfnSTARx,  
  WLANTL_TxCompCBType       pfnSTATxComp,  
  WLANTL_STAFetchPktCBType  pfnSTAFetchPkt,
  WLAN_STADescType*         wSTADescType ,
  v_S7_t                    rssi
);

/*===========================================================================

  FUNCTION    WLANTL_ClearSTAClient

  DESCRIPTION 

    HDD will call this API when it no longer needs data services for the 
    particular station. 

  DEPENDENCIES 

    A station must have been registered before the clear registration is 
    called. 

  PARAMETERS 

   pvosGCtx:        pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   ucSTAId:         identifier for the STA to be cleared
   
  RETURN VALUE

    The result code associated with performing the operation  
    
    VOS_STATUS_E_FAULT: Station ID is outside array boundaries or pointer to 
                        TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: Station was not registered 
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_ClearSTAClient 
( 
  v_PVOID_t        pvosGCtx,
  v_U8_t           ucSTAId 
);

/*===========================================================================

  FUNCTION    WLANTL_ChangeSTAState

  DESCRIPTION 

    HDD will make this notification whenever a change occurs in the 
    connectivity state of a particular STA. 

  DEPENDENCIES 

    A station must have been registered before the change state can be
    called.

    RESTRICTION: A station is being notified as authenticated before the 
                 keys are installed in HW. This way if a frame is received 
                 before the keys are installed DPU will drop that frame. 

    Main thread has higher priority that Tx and Rx threads thus guaranteeing 
    the following: 
        - a station will be in assoc state in TL before TL receives any data 
          for it 

  PARAMETERS 

   pvosGCtx:        pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
   ucSTAId:         identifier for the STA that is pending transmission
   tlSTAState:     the new state of the connection to the given station

   
  RETURN VALUE

    The result code associated with performing the operation  
    
    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to 
                         TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: Station was not registered 
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_ChangeSTAState 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  WLANTL_STAStateType   tlSTAState 
);

/*===========================================================================

  FUNCTION    WLANTL_STAPtkInstalled

  DESCRIPTION

    HDD will make this notification whenever PTK is installed for the STA

  DEPENDENCIES

    A station must have been registered before the change state can be
    called.

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   ucSTAId:         identifier for the STA for which Pairwise key is
                    installed

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STAPtkInstalled
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId
);
/*===========================================================================

  FUNCTION    WLANTL_GetSTAState

  DESCRIPTION

    Returns connectivity state of a particular STA.

  DEPENDENCIES

    A station must have been registered before its state can be retrieved.


  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station

    OUT
    ptlSTAState:    the current state of the connection to the given station


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetSTAState
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  WLANTL_STAStateType   *ptlSTAState
);

/*===========================================================================

  FUNCTION    WLANTL_STAPktPending

  DESCRIPTION 

    HDD will call this API when a packet is pending transmission in its 
    queues. 

  DEPENDENCIES 

    A station must have been registered before the packet pending 
    notification can be sent.

    RESTRICTION: TL will not count packets for pending notification. 
                 HDD is expected to send the notification only when 
                 non-empty event gets triggered. Worst case scenario 
                 is that TL might end up making a call when Hdds 
                 queues are actually empty. 

  PARAMETERS 

    pvosGCtx:    pointer to the global vos context; a handle to TL's 
                 control block can be extracted from its context 
    ucSTAId:     identifier for the STA that is pending transmission
    ucAC:        access category of the non-empty queue
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer 
                         to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: Station was not registered 
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_STAPktPending 
( 
  v_PVOID_t            pvosGCtx,
  v_U8_t               ucSTAId,
  WLANTL_ACEnumType    ucAc
);

/*==========================================================================

  FUNCTION    WLANTL_SetSTAPriority

  DESCRIPTION 

    TL exposes this API to allow upper layers a rough control over the 
    priority of transmission for a given station when supporting multiple 
    connections.

  DEPENDENCIES 

    A station must have been registered before the change in priority can be 
    called.

  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier for the STA that has to change priority
   
  RETURN VALUE

    The result code associated with performing the operation  
    
    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer 
                         to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: Station was not registered 
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_SetSTAPriority 
( 
  v_PVOID_t                pvosGCtx,
  v_U8_t                   ucSTAId,
  WLANTL_STAPriorityType   tlSTAPri
);

/*----------------------------------------------------------------------------
    INTERACTION WITH BAP
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_RegisterBAPClient

  DESCRIPTION 
    Called by SME to register itself as client for non-data BT-AMP packets. 

  DEPENDENCIES 
    TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    pfnTlBAPRxFrm:  pointer to the receive processing routine for non-data 
                    BT-AMP packets
    pfnFlushOpCompleteCb: 
                    pointer to the function that will inform BAP that the 
                    flush operation is complete.
   
  RETURN VALUE
  
    The result code associated with performing the operation  
    
    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer 
                         to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: BAL client was already registered
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_RegisterBAPClient 
( 
  v_PVOID_t                   pvosGCtx,
  WLANTL_BAPRxCBType          pfnTlBAPRx,
  WLANTL_FlushOpCompCBType    pfnFlushOpCompleteCb
);


/*==========================================================================

  FUNCTION    WLANTL_TxBAPFrm

  DESCRIPTION 
    BAP calls this when it wants to send a frame to the module

  DEPENDENCIES 
    BAP must be registered with TL before this function can be called. 

    RESTRICTION: BAP CANNOT push any packets to TL until it did not receive 
                 a tx complete from the previous packet, that means BAP
                 sends one packet, wait for tx complete and then 
                 sends another one

                 If BAP sends another packet before TL manages to process the
                 previously sent packet call will end in failure   

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or BAP's control block can be extracted from its context 
    vosDataBuff:   pointer to the vOSS buffer containing the packet to be 
                    transmitted
    pMetaInfo:      meta information about the packet 
    pfnTlBAPTxComp: pointer to a transmit complete routine for notifying 
                    the result of the operation over the bus
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_E_EXISTS: BAL client was not yet registered
    VOS_STATUS_E_BUSY:   The previous BT-AMP packet was not yet transmitted
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other failure messages may be returned from the BD header handling 
    routines, please check apropriate API for more info. 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_TxBAPFrm 
( 
  v_PVOID_t               pvosGCtx,
  vos_pkt_t*              vosDataBuff,  
  WLANTL_MetaInfoType*    pMetaInfo,   
  WLANTL_TxCompCBType     pfnTlBAPTxComp
);


/*----------------------------------------------------------------------------
    INTERACTION WITH SME
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_GetRssi

  DESCRIPTION 
    TL will extract the RSSI information from every data packet from the 
    ongoing traffic and will store it. It will provide the result to SME 
    upon request.

  DEPENDENCIES 

    WARNING: the read and write of this value will not be protected 
             by locks, therefore the information obtained after a read 
             might not always be consistent.  
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    ucSTAId:        station identifier for the requested value

    OUT
    puRssi:         the average value of the RSSI

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer 
                         to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_GetRssi 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_S7_t*               puRssi
);

/*==========================================================================

  FUNCTION    WLANTL_GetSnr

  DESCRIPTION
    TL will extract the SNR information from every data packet from the
    ongoing traffic and will store it. It will provide the result to SME
    upon request.

  DEPENDENCIES

    WARNING: the read and write of this value will not be protected
             by locks, therefore the information obtained after a read
             might not always be consistent.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    ucSTAId:        station identifier for the requested value

    OUT
    puSnr:         the average value of the SNR


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetSnr
(
  tANI_U8           ucSTAId,
  tANI_S8*          pSnr
);

/*==========================================================================

  FUNCTION    WLANTL_GetLinkQuality

  DESCRIPTION 
    TL will extract the LinkQuality information from every data packet from the 
    ongoing traffic and will store it. It will provide the result to SME 
    upon request.

  DEPENDENCIES 

    WARNING: the read and write of this value will not be protected 
             by locks, therefore the information obtained after a read 
             might not always be consistent.  
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    ucSTAId:        station identifier for the requested value

    OUT
    puLinkQuality:         the average value of the LinkQuality

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer 
                         to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_GetLinkQuality 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U32_t*              puLinkQuality
);

/*==========================================================================

  FUNCTION    WLANTL_FlushStaTID

  DESCRIPTION 
    TL provides this API as an interface to SME (BAP) layer. TL inturn posts a 
    message to HAL. This API is called by the SME inorder to perform a flush 
    operation.

  DEPENDENCIES 

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    ucSTAId:        station identifier for the requested value
    ucTid:          Tspec ID for the new BA session

    OUT
    The response for this post is received in the main thread, via a response 
    message from HAL to TL. 
   
  RETURN VALUE
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
============================================================================*/
VOS_STATUS  
WLANTL_FlushStaTID 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U8_t                ucTid
);

/*----------------------------------------------------------------------------
    INTERACTION WITH PE
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_RegisterMgmtFrmClient

  DESCRIPTION 
    Called by PE to register as a client for management frames delivery. 

  DEPENDENCIES 
    TL must be initialized before this API can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:           pointer to the global vos context; a handle to 
                        TL's control block can be extracted from its context 
    pfnTlMgmtFrmRx:     pointer to the receive processing routine for 
                        management frames
      
  RETURN VALUE
    The result code associated with performing the operation  
    
    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_E_EXISTS: Mgmt Frame client was already registered
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS  
WLANTL_RegisterMgmtFrmClient
( 
  v_PVOID_t               pvosGCtx,
  WLANTL_MgmtFrmRxCBType  pfnTlMgmtFrmRx
);

/*==========================================================================

  FUNCTION    WLANTL_DeRegisterMgmtFrmClient

  DESCRIPTION
    Called by PE to deregister as a client for management frames delivery.

  DEPENDENCIES
    TL must be initialized before this API can be called.

  PARAMETERS

    IN
    pvosGCtx:           pointer to the global vos context; a handle to
                        TL's control block can be extracted from its context
  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_E_EXISTS: Mgmt Frame client was never registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_DeRegisterMgmtFrmClient
(
  v_PVOID_t               pvosGCtx
);

/*==========================================================================

  FUNCTION    WLANTL_TxMgmtFrm

  DESCRIPTION 
    Called by PE when it want to send out a management frame. 
    HAL will also use this API for the few frames it sends out, they are not 
    management frames howevere it is accepted that an exception will be 
    allowed ONLY for the usage of HAL. 
    Generic data frames SHOULD NOT travel through this function. 

  DEPENDENCIES 
    TL must be initialized before this API can be called. 

    RESTRICTION: If PE sends another packet before TL manages to process the
                 previously sent packet call will end in failure   

                 Frames comming through here must be 802.11 frames, frame 
                 translation in UMA will be automatically disabled. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context;a handle to TL's 
                    control block can be extracted from its context 
    vosFrmBuf:      pointer to a vOSS buffer containing the management  
                    frame to be transmitted
    usFrmLen:       the length of the frame to be transmitted; information 
                    is already included in the vOSS buffer
    wFrmType:       the type of the frame being transmitted
    tid:            tid used to transmit this frame
    pfnCompTxFunc:  function pointer to the transmit complete routine
    pvBDHeader:     pointer to the BD header, if NULL it means it was not 
                    yet constructed and it lies within TL's responsibility  
                    to do so; if not NULL it is expected that it was 
                    already packed inside the vos packet 
    ucAckResponse:  flag notifying it an interrupt is needed for the 
                    acknowledgement received when the frame is sent out 
                    the air and ; the interrupt will be processed by HAL, 
                    only one such frame can be pending in the system at 
                    one time. 

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_E_EXISTS: Mgmt Frame client was not yet registered
    VOS_STATUS_E_BUSY:   The previous Mgmt packet was not yet transmitted
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other failure messages may be returned from the BD header handling 
    routines, please check apropriate API for more info. 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANTL_TxMgmtFrm
( 
  v_PVOID_t            pvosGCtx,  
  vos_pkt_t*           vosFrmBuf,
  v_U16_t              usFrmLen,
  v_U8_t               ucFrmType, 
  v_U8_t               tid,
  WLANTL_TxCompCBType  pfnCompTxFunc,
  v_PVOID_t            voosBDHeader,
  v_U32_t              ucAckResponse
);


/*----------------------------------------------------------------------------
    INTERACTION WITH HAL
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_ResetNotification

  DESCRIPTION 
    HAL notifies TL when the module is being reset.
    Currently not used.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_ResetNotification
( 
  v_PVOID_t   pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANTL_SuspendDataTx

  DESCRIPTION 
    HAL calls this API when it wishes to suspend transmission for a 
    particular STA.
    
  DEPENDENCIES 
    The STA for which the request is made must be first registered with 
    TL by HDD. 

    RESTRICTION:  In case of a suspend, the flag write and read will not be 
                  locked: worst case scenario one more packet can get 
                  through before the flag gets updated (we can make this 
                  write atomic as well to guarantee consistency)

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    pucSTAId:       identifier of the station for which the request is made; 
                    a value of NULL assumes suspend on all active station
    pfnSuspendTxCB: pointer to the suspend result notification in case the 
                    call is asynchronous

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer 
                          to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:  Station was not registered 
    VOS_STATUS_SUCCESS:   Everything is good :)
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_SuspendDataTx
( 
  v_PVOID_t               pvosGCtx,
  v_U8_t*                 ucSTAId,
  WLANTL_SuspendCBType    pfnSuspendTx
);

/*==========================================================================

  FUNCTION    WLANTL_ResumeDataTx

  DESCRIPTION 
    Called by HAL to resume data transmission for a given STA. 

    WARNING: If a station was individually suspended a global resume will 
             not resume that station
             
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    pucSTAId:       identifier of the station which is being resumed; NULL
                    translates into global resume
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer 
                          to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:  Station was not registered 
    VOS_STATUS_SUCCESS:   Everything is good :)
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_ResumeDataTx
( 
  v_PVOID_t      pvosGCtx,
  v_U8_t*        pucSTAId 
);


/*----------------------------------------------------------------------------
    CLIENT INDEPENDENT INTERFACE
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_GetTxPktCount

  DESCRIPTION 
    TL will provide the number of transmitted packets counted per 
    STA per TID. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier of the station 
    ucTid:          identifier of the tspec 

    OUT
    puTxPktCount:   the number of packets tx packet for this STA and TID
   
  RETURN VALUE
    The result code associated with performing the operation  
    
    VOS_STATUS_E_INVAL:   Input parameters are invalid 
    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer 
                          to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:  Station was not registered 
    VOS_STATUS_SUCCESS:   Everything is good :)
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_GetTxPktCount
( 
  v_PVOID_t      pvosGCtx,
  v_U8_t         ucSTAId,
  v_U8_t         ucTid,
  v_U32_t*       puTxPktCount
);

/*==========================================================================

  FUNCTION    WLANTL_GetRxPktCount

  DESCRIPTION 
    TL will provide the number of received packets counted per 
    STA per TID. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier of the station 
    ucTid:          identifier of the tspec 
   
   OUT
    puTxPktCount:   the number of packets rx packet for this STA and TID
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:   Input parameters are invalid 
    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer 
                          to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:  Station was not registered 
    VOS_STATUS_SUCCESS:   Everything is good :)
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_GetRxPktCount
( 
  v_PVOID_t      pvosGCtx,
  v_U8_t         ucSTAId,
  v_U8_t         ucTid,
  v_U32_t*       puRxPktCount
);

/*==========================================================================
    VOSS SCHEDULER INTERACTION
  ==========================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_McProcessMsg

  DESCRIPTION 
    Called by VOSS when a message was serialized for TL through the
    main thread/task. 

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    message:        type and content of the message 
                    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a 
                          page fault  
    VOS_STATUS_SUCCESS:   Everything is good :)
   
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_McProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLANTL_RxProcessMsg

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    rx thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check
  corresponding API for more info.
  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLANTL_McFreeMsg

  DESCRIPTION 
    Called by VOSS to free a given TL message on the Main thread when there 
    are messages pending in the queue when the whole system is been reset. 
    For now, TL does not allocate any body so this function shout translate 
    into a NOOP

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    message:        type and content of the message 
                    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_McFreeMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLANTL_TxProcessMsg

  DESCRIPTION 
    Called by VOSS when a message was serialized for TL through the
    tx thread/task. 

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    message:        type and content of the message 
                    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a 
                          page fault  
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check 
  corresponding API for more info. 
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_TxProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
);

/*==========================================================================
  FUNCTION    WLANTL_McFreeMsg

  DESCRIPTION 
    Called by VOSS to free a given TL message on the Main thread when there 
    are messages pending in the queue when the whole system is been reset. 
    For now, TL does not allocate any body so this function shout translate 
    into a NOOP

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    message:        type and content of the message 
                    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_TxFreeMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
);


/*==========================================================================
  FUNCTION    WLANTL_EnableUAPSDForAC

  DESCRIPTION 
   Called by HDD to enable UAPSD in TL. TL is in charge for sending trigger 
   frames. 

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        station Id 
    ucACId:         AC for which U-APSD is being enabled  
    ucTid           TSpec Id     
    uServiceInt:    service interval used by TL to send trigger frames
    uSuspendInt:    suspend interval used by TL to determine that an 
                    app is idle and should start sending trigg frms less often
    wTSDir:         direction of TSpec 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_EnableUAPSDForAC
(
  v_PVOID_t          pvosGCtx,
  v_U8_t             ucSTAId,
  WLANTL_ACEnumType  ucACId,
  v_U8_t             ucTid,
  v_U8_t             ucUP,
  v_U32_t            uServiceInt,
  v_U32_t            uSuspendInt,
  WLANTL_TSDirType   wTSDir
);


/*==========================================================================
  FUNCTION    WLANTL_DisableUAPSDForAC

  DESCRIPTION 
   Called by HDD to disable UAPSD in TL. TL will stop sending trigger 
   frames. 

  DEPENDENCIES 
    The TL must be initialized before this function can be called. 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        station Id 
    ucACId:         AC for which U-APSD is being enabled       
   
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_DisableUAPSDForAC
(
  v_PVOID_t          pvosGCtx,
  v_U8_t             ucSTAId,
  WLANTL_ACEnumType  ucACId
);

#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
/*==========================================================================
  FUNCTION     WLANTL_RegRSSIIndicationCB

  DESCRIPTION  Registration function to get notification if RSSI cross
               threshold.
               Client should register threshold, direction, and notification
               callback function pointer

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in rssiValue - RSSI threshold value
               in triggerEvent - Cross direction should be notified
                                 UP, DOWN, and CROSS
               in crossCBFunction - Notification CB Function
               in usrCtxt - user context

  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_RegRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID,
   v_PVOID_t                       usrCtxt
);

/*==========================================================================
  FUNCTION     WLANTL_DeregRSSIIndicationCB

  DESCRIPTION  Remove specific threshold from list

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in rssiValue - RSSI threshold value
               in triggerEvent - Cross direction should be notified
                                 UP, DOWN, and CROSS
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_DeregRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_BMPSRSSIRegionChangedNotification
(
   v_PVOID_t             pAdapter,
   tpSirRSSINotification pRSSINotification
);

/*==========================================================================
  FUNCTION     WLANTL_SetAlpha

  DESCRIPTION  ALPLA is weight value to calculate AVG RSSI
               avgRSSI = (ALPHA * historyRSSI) + ((10 - ALPHA) * newRSSI)
               avgRSSI has (ALPHA * 10)% of history RSSI weight and
               (10 - ALPHA)% of newRSSI weight
               This portion is dynamically configurable.
               Default is ?

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in valueAlpah - ALPHA
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_SetAlpha
(
   v_PVOID_t pAdapter,
   v_U8_t    valueAlpha
);

/*==========================================================================
  FUNCTION     WLANTL_RegGetTrafficStatus

  DESCRIPTION  Registration function for traffic status monitoring
               During measure period count data frames.
               If frame count is larger then IDLE threshold set as traffic ON
               or OFF.
               And traffic status is changed send report to client with
               registered callback function

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in idleThreshold - Traffic on or off threshold
               in measurePeriod - Traffic state check period
               in trfficStatusCB - traffic status changed notification
                                   CB function
               in usrCtxt - user context
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_RegGetTrafficStatus
(
   v_PVOID_t                          pAdapter,
   v_U32_t                            idleThreshold,
   v_U32_t                            measurePeriod,
   WLANTL_TrafficStatusChangedCBType  trfficStatusCB,
   v_PVOID_t                          usrCtxt
);
#endif
/*==========================================================================
  FUNCTION      WLANTL_GetStatistics

  DESCRIPTION   Get traffic statistics for identified station 

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                out statBuffer - traffic statistics buffer
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_GetStatistics
(
   v_PVOID_t                  pAdapter,
   WLANTL_TRANSFER_STA_TYPE  *statBuffer,
   v_U8_t                     STAid
);

/*==========================================================================
  FUNCTION      WLANTL_ResetStatistics

  DESCRIPTION   Reset statistics structure for identified station ID
                Reset means set values as 0

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_ResetStatistics
(
   v_PVOID_t                  pAdapter,
   v_U8_t                     STAid
);

/*==========================================================================
  FUNCTION      WLANTL_GetSpecStatistic

  DESCRIPTION   Get specific field within statistics structure for
                identified station ID 

  DEPENDENCIES  NONE

  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                in STAid    - Station ID
                out buffer  - Statistic value
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_GetSpecStatistic
(
   v_PVOID_t                    pAdapter,
   WLANTL_TRANSFER_STATIC_TYPE  statType,
   v_U32_t                     *buffer,
   v_U8_t                       STAid
);

/*==========================================================================
  FUNCTION      WLANTL_ResetSpecStatistic

  DESCRIPTION   Reset specific field within statistics structure for
                identified station ID
                Reset means set as 0

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                in STAid    - Station ID

  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_ResetSpecStatistic
(
   v_PVOID_t                    pAdapter,
   WLANTL_TRANSFER_STATIC_TYPE  statType,
   v_U8_t                       STAid
);
/*===============================================================================
  FUNCTION      WLANTL_IsReplayPacket
   
  DESCRIPTION   This function does replay check for valid stations

  DEPENDENCIES  Validity of replay check must be done before the function 
                is called
                           
  PARAMETERS    currentReplayCounter    current replay counter taken from RX BD 
                previousReplayCounter   previous replay counter taken from TL CB
                                          
  RETRUN        VOS_TRUE    packet is a replay packet
                VOS_FALSE   packet is not a replay packet

  SIDE EFFECTS   none
 ===============================================================================*/
v_BOOL_t WLANTL_IsReplayPacket
(
    v_U64_t    currentReplayCounter,
    v_U64_t    previousReplayCounter
);

/*===============================================================================
  FUNCTION      WLANTL_GetReplayCounterFromRxBD
     
  DESCRIPTION   This function extracts 48-bit replay packet number from RX BD 
 
  DEPENDENCIES  Validity of replay check must be done before the function 
                is called
                          
  PARAMETERS    pucRxHeader pointer to RX BD header
                                       
  RETRUN        v_U64_t    Packet number extarcted from RX BD

  SIDE EFFECTS   none
 ===============================================================================*/
v_U64_t
WLANTL_GetReplayCounterFromRxBD
(
   v_U8_t *pucRxBDHeader
);



/*
 DESCRIPTION 
    TL returns the weight currently maintained in TL.
 IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 

 OUT
    pACWeights:     Caller allocated memory for filling in weights

 RETURN VALUE  VOS_STATUS
*/
VOS_STATUS  
WLANTL_GetACWeights 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pACWeights
);


/*
 DESCRIPTION 
    Change the weight currently maintained by TL.
 IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    pACWeights:     Caller allocated memory contain the weights to use


 RETURN VALUE  VOS_STATUS
*/
VOS_STATUS  
WLANTL_SetACWeights 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pACWeights
);

/*==========================================================================
  FUNCTION      WLANTL_GetSoftAPStatistics

  DESCRIPTION   Collect the cumulative statistics for all Softap stations

  DEPENDENCIES  NONE
    
  PARAMETERS    in pvosGCtx  - Pointer to the global vos context
                   bReset    - If set TL statistics will be cleared after reading
                out statsSum - pointer to collected statistics

  RETURN VALUE  VOS_STATUS_SUCCESS : if the Statistics are successfully extracted

  SIDE EFFECTS  NONE

============================================================================*/
VOS_STATUS WLANTL_GetSoftAPStatistics(v_PVOID_t pAdapter, WLANTL_TRANSFER_STA_TYPE *statsSum, v_BOOL_t bReset);

#ifdef __cplusplus
 }
#endif 

/*===========================================================================

  FUNCTION    WLANTL_EnableCaching

  DESCRIPTION

    This function is used to enable caching only when assoc/reassoc req is send.
    that is cache packets only for such STA ID.


  DEPENDENCIES

    TL must have been initialized before this gets called.


  PARAMETERS

   staId:   station id.

  RETURN VALUE

   none

============================================================================*/
void WLANTL_EnableCaching(v_U8_t staId);

 /*===========================================================================

  FUNCTION    WLANTL_AssocFailed

  DESCRIPTION

    This function is used by PE to notify TL that cache needs to flushed
    when association is not successfully completed 

    Internally, TL post a message to TX_Thread to serialize the request to 
    keep lock-free mechanism.

   
  DEPENDENCIES

    TL must have been initialized before this gets called.

   
  PARAMETERS

   ucSTAId:   station id 

  RETURN VALUE

   none
   
  SIDE EFFECTS
   There may be race condition that PE call this API and send another association
   request immediately with same staId before TX_thread can process the message.

   To avoid this, we might need PE to wait for TX_thread process the message,
   but this is not currently implemented. 
   
============================================================================*/
void WLANTL_AssocFailed(v_U8_t staId);


/*===============================================================================
  FUNCTION      WLANTL_PostResNeeded
     
  DESCRIPTION   This function posts message to TL to reserve BD/PDU memory
 
  DEPENDENCIES  None
                          
  PARAMETERS    pvosGCtx
                                       
  RETURN        None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_PostResNeeded(v_PVOID_t pvosGCtx);

/*===========================================================================

  FUNCTION    WLANTL_Finish_ULA

  DESCRIPTION
     This function is used by HDD to notify TL to finish Upper layer authentication
     incase the last EAPOL packet is pending in the TL queue. 
     To avoid the race condition between sme set key and the last EAPOL packet 
     the HDD module calls this function just before calling the sme_RoamSetKey.
   
  DEPENDENCIES

    TL must have been initialized before this gets called.

   
  PARAMETERS

   callbackRoutine:   HDD Callback function.
   callbackContext : HDD userdata context.

  RETURN VALUE

   VOS_STATUS_SUCCESS/VOS_STATUS_FAILURE
   
  SIDE EFFECTS
   
============================================================================*/

VOS_STATUS WLANTL_Finish_ULA( void (*callbackRoutine) (void *callbackContext),
                              void *callbackContext);

/*===============================================================================
  FUNCTION       WLANTL_UpdateRssiBmps

  DESCRIPTION    This function updates the TL's RSSI (in BMPS mode)

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    rssi             RSSI (BMPS mode)     RSSI in BMPS mode

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_UpdateRssiBmps(v_PVOID_t pvosGCtx, v_U8_t staId, v_S7_t rssi);

/*===============================================================================
  FUNCTION       WLANTL_UpdateSnrBmps

  DESCRIPTION    This function updates the TL's SNR (in BMPS mode)

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    snr             SNR (BMPS mode)     SNR in BMPS mode

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_UpdateSnrBmps(v_PVOID_t pvosGCtx, v_U8_t staId, v_S7_t snr);

/*==========================================================================
  FUNCTION   WLANTL_SetTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to indicate that WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_SetTxXmitPending
(
  v_PVOID_t       pvosGCtx
);

/*==========================================================================
  FUNCTION   WLANTL_IsTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to know whether WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    0:   No WDA_DS_TX_START_XMIT msg pending 
    1:   Msg WDA_DS_TX_START_XMIT already pending in TL msg queue 

  SIDE EFFECTS

============================================================================*/

v_BOOL_t
WLANTL_IsTxXmitPending
(
  v_PVOID_t       pvosGCtx
);

/*==========================================================================
  FUNCTION   WLANTL_ClearTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to indicate that no WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_ClearTxXmitPending
(
  v_PVOID_t       pvosGCtx
);

/*==========================================================================
  FUNCTION   WLANTL_UpdateSTABssIdforIBSS

  DESCRIPTION
    HDD will call this API to update the BSSID for this Station.

  DEPENDENCIES
    The HDD Should registered the staID with TL before calling this function.

  PARAMETERS

    IN
    pvosGCtx:    Pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context
    IN
    ucSTAId       The Station ID for Bssid to be updated
    IN
    pBssid          BSSID to be updated

  RETURN VALUE
      The result code associated with performing the operation

      VOS_STATUS_E_INVAL:  Input parameters are invalid
      VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                           TL cb is NULL ; access would cause a page fault
      VOS_STATUS_E_EXISTS: Station was not registered
      VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS
============================================================================*/

VOS_STATUS
WLANTL_UpdateSTABssIdforIBSS
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U8_t               *pBssid
);



/*===============================================================================
  FUNCTION       WLANTL_UpdateLinkCapacity

  DESCRIPTION    This function updates the STA's Link Capacity in TL

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    linkCapacity     linkCapacity         Link Capacity

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void
WLANTL_UpdateLinkCapacity
(
  v_PVOID_t pvosGCtx,
  v_U8_t staId,
  v_U32_t linkCapacity);

/*===========================================================================

  FUNCTION    WLANTL_GetSTALinkCapacity

  DESCRIPTION

    Returns Link Capacity of a particular STA.

  DEPENDENCIES

    A station must have been registered before its state can be retrieved.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station

    OUT
    plinkCapacity:  the current link capacity the connection to
                    the given station


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_GetSTALinkCapacity
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U32_t               *plinkCapacity
);

/*===========================================================================
  FUNCTION   WLANTL_TxThreadDebugHandler

  DESCRIPTION
    Printing TL Snapshot dump, processed under TxThread context, currently
    information regarding the global TlCb struture. Dumps information related
    to per active STA connection currently in use by TL.

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    IN
    pvosGCtx:    Pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE   None

  SIDE EFFECTS
============================================================================*/

v_VOID_t
WLANTL_TxThreadDebugHandler
(
  v_PVOID_t       *pvosGCtx
);

/*==========================================================================
  FUNCTION   WLANTL_TLDebugMessage

  DESCRIPTION
    Post a TL Snapshot request, posts message in TxThread.

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    IN
    displaySnapshot Boolean showing whether to dump the snapshot or not.

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_TLDebugMessage
(
  v_BOOL_t displaySnapshot
);

#endif /* #ifndef WLAN_QCT_WLANTL_H */
