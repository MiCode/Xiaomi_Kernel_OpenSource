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

#ifndef WLAN_QCT_WLANBAP_INTERNAL_H
#define WLAN_QCT_WLANBAP_INTERNAL_H

/*===========================================================================

               W L A N   B T - A M P  P A L   L A Y E R 
                       I N T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the internal API exposed by the wlan BT-AMP PAL layer 
  module.
  
      
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /home/labuser/ampBlueZ_2/CORE/BAP/src/bapInternal.h,v 1.3 2010/07/12 20:40:18 labuser Exp labuser $ $DateTime: $ $Author: labuser $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
09/15/08    jez     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_api.h" 
#include "vos_packet.h" 

// Pick up the CSR API definitions
#include "csrApi.h"

/* BT-AMP PAL API structure types  - FramesC generated */ 
#include "btampHCI.h" 
#include "bapApi.h" 

// Pick up the BTAMP FSM definitions
#include "fsmDefs.h"
//#include "btampFsm.h"
#include "btampFsm_ext.h"
#include "bapRsn8021xFsm.h"
#include "bapRsnErrors.h"

#include "csrApi.h"
#include "sirApi.h"
#include "wniApi.h"
#include "palApi.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 

/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
// Temporary so that I can compile
//#define VOS_MODULE_ID_BAP 9
// Temporary 
//#define BAP_DEBUG

// Used to enable or disable security on the BT-AMP link 
#define WLANBAP_SECURITY_ENABLED_STATE VOS_TRUE

// How do I get BAP context from voss context? 
#define VOS_GET_BAP_CB(ctx) vos_get_context( VOS_MODULE_ID_BAP, ctx) 
// How do I get halHandle from voss context? 
#define VOS_GET_HAL_CB(ctx) vos_get_context( VOS_MODULE_ID_SME, ctx) 

// Default timeout values (in BR/EDR baseband slots)
// Physical Link Connection Accept Timer interval (0x1FA0 * 0.625 = 5.06 sec)
/* chose to double it to 3FFF as we saw conn timeout in lab*/
//#define WLANBAP_CONNECTION_ACCEPT_TIMEOUT  0xFFFF
// Set default to 0x1F40.  Which is ~5 secs.
#define WLANBAP_CONNECTION_ACCEPT_TIMEOUT  0x1F40

/* Link Supervision Timer interval (0x7D00 * 0.625 = 20 sec) */
#ifdef FEATURE_WLAN_BTAMP_UT
#define WLANBAP_LINK_SUPERVISION_TIMEOUT   0x7D00
#else
#define WLANBAP_LINK_SUPERVISION_TIMEOUT   0x3E80  // 10 seconds
#endif
#define WLANBAP_LINK_SUPERVISION_RETRIES   2

/* Logical Link Accept Timer interval (0x1FA0 * 0.625 = 5.06 sec)*/
#define WLANBAP_LOGICAL_LINK_ACCEPT_TIMEOUT 0x1F40

/* BR/EDR baseband 1 slot time period */
#define WLANBAP_BREDR_BASEBAND_SLOT_TIME  1 // 0.625

/* Maximum allowed range for connection accept timeout interval */
#define WLANBAP_CON_ACCEPT_TIMEOUT_MAX_RANGE  0xB540

/* Minimum allowed range for connection accept timeout interval */
#define WLANBAP_CON_ACCEPT_TIMEOUT_MIN_RANGE   0x01

/* Best Effort Flush timer interval*/
#define WLANBAP_BE_FLUSH_TIMEOUT 10

/* Length of the LLC header*/
#define WLANBAP_LLC_HEADER_LEN   8 

/*Size of the protocol type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_PROTO_TYPE_SIZE            2

/*Size of the OUI type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_OUI_SIZE                   3

/*Offset of the OUI field inside the LLC/SNAP header*/
#define WLANBAP_LLC_OUI_OFFSET                 3

/*Offset of the protocol type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_PROTO_TYPE_OFFSET  (WLANBAP_LLC_OUI_OFFSET +  WLANBAP_LLC_OUI_SIZE)

#define WLANBAP_MAX_NUM_TRIPLETS               5

#define WLANBAP_MAX_SIZE_TRIPLETS              3
/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 *  Type Declarations - For internal BAP context information
 * -------------------------------------------------------------------------*/
typedef struct sBtampHCI_Buffer_Size {
//    v_U8_t       present;
    /* D9r14 says Max80211PALPDUSize 1492 */
    v_U16_t      HC_ACL_Data_Packet_Length;  
    v_U8_t       HC_SCO_Packet_Length;
    v_U16_t      HC_Total_Num_ACL_Packets;
    v_U16_t      HC_Total_Num_SCO_Packets;
} tBtampHCI_Buffer_Size;

typedef struct sBtampHCI_Data_Block_Size {
//    v_U8_t       present;
    v_U8_t       status;
    /* D9r14 says Max80211PALPDUSize 1492 */
    v_U16_t      HC_Max_ACL_Data_Packet_Length;  
    v_U16_t      HC_Data_Block_Length;
    v_U16_t      HC_Total_Num_Data_Blocks;
} tBtampHCI_Data_Block_Size;

typedef struct sBtampHCI_Version_Info {
//    v_U8_t       present;
    v_U8_t       HC_HCI_Version;
    v_U16_t      HC_HCI_Revision;  
    v_U8_t       HC_PAL_Version;  /* for 802.11 AMP: 0x01 */
    v_U16_t      HC_PAL_Sub_Version; /* for 802.11 AMP: Vendor specific */ 
    v_U16_t      HC_Manufac_Name; /* See BT assigned numbers */  
} tBtampHCI_Version_Info;

typedef struct sBtampHCI_Supported_Cmds {
//    v_U8_t       present;
    v_U8_t       HC_Support_Cmds[64]; /* a bitmask of cmds */
} tBtampHCI_Supported_Cmds;

typedef struct sBtampHCI_AMP_Info {
//    v_U8_t       present;
    v_U8_t       HC_AMP_Status;
    v_U32_t      HC_Total_BW; /* combined uplink and downlink */
    v_U32_t      HC_Max_Guaranteed_BW; /* upper bound */
    v_U32_t      HC_Min_Latency; /* AMP HCI latency + DIFS + CWMin */
    v_U32_t      HC_Max_PDU_Size; /* Equal to Max80211PALPDUSize */
    v_U8_t       HC_Controller_Type; /* 0x01 for 802.11 BT-AMP PAL  */
    v_U16_t      HC_PAL_Capabilities;  /* Bit 0: 0 = No Guarantee; 1 = Guarantee */
    v_U16_t      HC_AMP_Assoc_Length;  /* Length of AMP Assoc Info */
                                       /* Equal to Max80211AMPASSOCLen (672) */
    v_U16_t      HC_Max_Flush_Timeout;  /* Maximum time Tx attempted. 0 is inf retry */
    v_U16_t      HC_BE_Flush_Timeout;  /* Maximum time BE Tx attempted. 0 is inf retry */
} tBtampHCI_AMP_Info;

typedef struct sBtampHCI_AMP_Assoc {
//    v_U8_t       present;
    v_U8_t       HC_cnct_country[3];   /* Connected channel */
    v_U8_t       HC_cnct_num_triplets;
    v_U8_t       HC_cnct_triplets[WLANBAP_MAX_NUM_TRIPLETS][WLANBAP_MAX_SIZE_TRIPLETS];
    v_U8_t       HC_mac_addr[6];
    v_U32_t      HC_pal_capabilities;
    v_U8_t       HC_pref_country[3];   /* Preferred channels */
    v_U8_t       HC_pref_num_triplets;
    v_U8_t       HC_pref_triplets[WLANBAP_MAX_NUM_TRIPLETS][WLANBAP_MAX_SIZE_TRIPLETS];
    v_U8_t       HC_pal_version;
    v_U16_t      HC_pal_CompanyID;
    v_U16_t      HC_pal_subversion;
}  tBtampHCI_AMP_Assoc, *tpBtampHCI_AMP_Assoc ;

typedef struct sBtampTLVHCI_Location_Data_Info {
    v_U8_t       loc_domain_aware;
    v_U8_t       loc_domain[3];
    v_U8_t       loc_options;
} tBtampTLVHCI_Location_Data_Info;

/*----------------------------------------------------------------------------
 *  Type Declarations - For BAP logical link context information
 * -------------------------------------------------------------------------*/
typedef struct sBtampLogLinkCtx {
    v_U8_t       present;  /* In use? */

    v_U8_t       log_link_index;  /* small integer (<16) value assigned by us */
    v_U16_t      log_link_handle;  /* 8 bits of phy_link_handle and our index */

    /* The flow spec (From section 5.6 of Generic AMP spec)  */
    tBtampTLVFlow_Spec btampFlowSpec;   

    /* The Access category  */
    WLANTL_ACEnumType btampAC;   

    /* The TID  */
    v_U8_t    ucTID;

    /* UP of the packet being sent */
    v_U8_t    ucUP;

    /*Number of packets completed since the last time num pkt complete event 
      was issued*/
    v_U32_t   uTxPktCompleted;    

}  tBtampLogLinkCtx, *tpBtampLogLinkCtx ;

/*----------------------------------------------------------------------------
 *  Type Declarations - QOS related
 * -------------------------------------------------------------------------*/
/* BT-AMP QOS config */
typedef struct sBtampQosCfg {
    v_U8_t                    bWmmIsEnabled;
} tBtampQosCfg;

/*----------------------------------------------------------------------------
 *  Opaque BAP context Type Declaration
 * -------------------------------------------------------------------------*/
// We were only using this syntax, when this was truly opaque. 
// (I.E., it was defined in a different file.)
//typedef struct sBtampContext tBtampContext, *ptBtampContext;


// Validity check the logical link value 
#define BTAMP_VALID_LOG_LINK(a) ( a > 0 && a < WLANBAP_MAX_LOG_LINKS ? 1 : 0)  

/* Instance data definition of state machine */
// Moved here from the BTAMP FSM definitions in btampFsm.h
typedef struct{
  BTAMPFSM_ENTRY_FLAG_T disconnectedEntry;
  BTAMPFSM_STATEVAR_T stateVar;
  BTAMPFSM_INST_ID_T inst_id;
} BTAMPFSM_INSTANCEDATA_T;

/* BT-AMP device role */
typedef enum{
    BT_RESPONDER,
    BT_INITIATOR
} tWLAN_BAPRole;

/* BT-AMP device role */
typedef enum{
    WLAN_BAPLogLinkClosed,
    WLAN_BAPLogLinkOpen,
    WLAN_BAPLogLinkInProgress,
} tWLAN_BAPLogLinkState;

typedef struct{
    v_U8_t       phyLinkHandle;
    v_U8_t       txFlowSpec[18];
    v_U8_t       rxFlowSpec[18];
} tBtampLogLinkReqInfo;

/*----------------------------------------------------------------------------
 *  BAP context Data Type Declaration
 * -------------------------------------------------------------------------*/
#undef BTAMP_MULTIPLE_PHY_LINKS
typedef struct sBtampContext {
#ifndef BTAMP_MULTIPLE_PHY_LINKS

    // Include the enclosing VOSS context here
    v_PVOID_t                 pvosGCtx; 

    //  include the phy link state machine structure here
    tWLAN_BAPbapPhysLinkMachine   bapPhysLinkMachine;

    // BAP device role
    tWLAN_BAPRole             BAPDeviceRole;
    // Include the SME(CSR) sessionId here
    v_U8_t                    sessionId;

    // Actual storage for AP and self (STA) SSID 
    //tSirMacSSid               SSID[2];
    tCsrSSIDInfo              SSIDList[2];
    // Actual storage for AP bssid 
    tCsrBssid                 bssid;
    // Include the SME(CSR) context here
    tCsrRoamProfile           csrRoamProfile; 
    tANI_U32                  csrRoamId;

    // QOS config
    tBtampQosCfg              bapQosCfg;

    /*Flag for signaling if security is enabled*/
    v_U8_t                    ucSecEnabled;

    // associated boolean flag
    v_U8_t                    mAssociated;
    // associated status 
    v_U8_t                    mAssociatedStatus;
    tCsrBssid                 assocBssid;  
    tBssSystemRole            systemRole; 

    // own SSID  
    v_U8_t                    ownSsid[32];
    v_U32_t                   ownSsidLen;

    // incoming Assoc SSID  
    v_U8_t                    assocSsid[32];
    v_U32_t                   assocSsidLen;

    // gNeedPhysLinkCompEvent
    v_U8_t                    gNeedPhysLinkCompEvent;
    // gPhysLinkStatus 
    v_U8_t                    gPhysLinkStatus;
    // gDiscRequested
    v_U8_t                    gDiscRequested;
    // gDiscReason 
    v_U8_t                    gDiscReason;

    // Include the BSL per-application context here
    v_PVOID_t                 pAppHdl;  // Per-app BSL context
    // Include the BSL per-association contexts here.
    // (Right now, there is only one)
    v_PVOID_t                 pHddHdl;
    /* 8 bits of phy_link_handle identifies this association */
    v_U8_t                    phy_link_handle;  
    // Short Range Mode setting for this physical link
    v_U8_t                    phy_link_srm;

    // Include the key material for this physical link
    v_U8_t                    key_type;
    v_U8_t                    key_length;
    v_U8_t                    key_material[32];
    
    /* Physical link quality status
       After the physical link is up, SME indicates the link quality through
       callback. This value is returned to upper layer on request.
       */
    v_U8_t                    link_quality;
    
    /* Connection Accept timer*/
    vos_timer_t               bapConnectionAcceptTimer; 
    /* Link Supervision timer*/
    vos_timer_t               bapLinkSupervisionTimer; 
    /* Logical Link Accept timer*/
    vos_timer_t               bapLogicalLinkAcceptTimer; 
    /* Best Effort Flush timer*/
    vos_timer_t               bapBEFlushTimer; 

    /* TX Packet Monitoring timer*/
    vos_timer_t               bapTxPktMonitorTimer; 

    /* Connection Accept Timer interval (in BR/EDR baseband slots)
     * Interval length = N * 0.625 msec (1 BR/EDR baseband slot)
     */
    v_U16_t                   bapConnectionAcceptTimerInterval; 

    /* Link Supervision Timer interval (in BR/EDR baseband slots) */
    v_U16_t                   bapLinkSupervisionTimerInterval; 

    /* Logical Link Accept Timer interval (in BR/EDR baseband slots) */
    v_U16_t                   bapLogicalLinkAcceptTimerInterval; 

    /* Best Effort Flush timer interval*/
    v_U32_t                   bapBEFlushTimerInterval; 

    // Include the current channel here
    v_U32_t                   channel;

    // Include the associations STA Id
    v_U8_t                    ucSTAId;

    // Include the associations MAC addresses
    v_U8_t                    self_mac_addr[6];
    v_U8_t                    peer_mac_addr[6];

    // The array of logical links
    /* the last small integer (<16) value assigned by us */
    v_U8_t                    current_log_link_index; /* assigned mod 16 */  
    v_U8_t                    total_log_link_index; /* should never be >16 */  
    /* The actual array */
    tBtampLogLinkCtx          btampLogLinkCtx[WLANBAP_MAX_LOG_LINKS];  
    
    // Include the HDD BAP Shim Layer callbacks for Fetch, TxComp, and RxPkt
    WLANBAP_STAFetchPktCBType pfnBtampFetchPktCB;
    WLANBAP_STARxCBType       pfnBtamp_STARxCB;
    WLANBAP_TxCompCBType      pfnBtampTxCompCB;

    /* Implements the callback for ALL asynchronous events. */ 
    tpWLAN_BAPEventCB         pBapHCIEventCB;

    // Save Page2 of the event mask.  
    v_U8_t                    event_mask_page_2[8];

    // Include the Local Assoc structure.
    // (This gets filled during initialization. It is used, for example, to 
    // obtain the local MAC address for forming the 802.3 frame.) 
    // <<Why don't I just pull out the individ fields I need?  Like MAC addr.>>
    tBtampHCI_AMP_Assoc       btamp_AMP_Assoc;

    // Remote AMP Assoc
    tBtampHCI_AMP_Assoc       btamp_Remote_AMP_Assoc;

    tBtampTLVHCI_Location_Data_Info  btamp_Location_Data_Info;

    union
    {
        tAuthRsnFsm authFsm;
        tSuppRsnFsm suppFsm;
    }uFsm;
    //LinkSupervision packet
    tANI_BOOLEAN lsReqPktPending;
    tANI_BOOLEAN dataPktPending;
    tANI_U8 retries;
    vos_pkt_t *pPacket;
    vos_pkt_t *lsReqPacket;
    vos_pkt_t *lsRepPacket;
    v_U16_t    lsPktln;
    v_U16_t    lsPending;
    WLANTL_MetaInfoType  metaInfo;   
    tANI_BOOLEAN isBapSessionOpen;

    tWLAN_BAPLogLinkState  btamp_logical_link_state;

    tBtampLogLinkReqInfo   btamp_logical_link_req_info;

    tANI_BOOLEAN           btamp_async_logical_link_create;

    tANI_BOOLEAN           btamp_logical_link_cancel_pending;

    tANI_BOOLEAN           btamp_session_on;

#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

    // Include the enclosing VOSS context here
    v_PVOID_t                 pvosGCtx; 

    //    include the state machine structure here

    // Include the BSL per-application context here
    v_PVOID_t                 pAppHdl;  // Per-app BSL context
    // Include the BSL per-association contexts here.
    // (Right now, there is only one)
    v_PVOID_t                 pHddHdl;
    /* 8 bits of phy_link_handle identifies this association */
    v_U8_t                    phy_link_handle;  
    // Short Range Mode setting for this physical link
    v_U8_t                    phy_link_srm;

    // Include the associations STA Id
    v_U8_t                    ucSTAId;

    // Include the associations MAC addresses
    v_U8_t                    self_mac_addr[6];
    v_U8_t                    peer_mac_addr[6];

    // The array of logical links
    /* the last small integer (<16) value assigned by us */
    v_U8_t                    current_log_link_index; /* assigned mod 16 */  
    v_U8_t                    total_log_link_index; /* should never be >16 */  
    /* The actual array */
    tBtampLogLinkCtx          btampLogLinkCtx[WLANBAP_MAX_LOG_LINKS];  
    
    // Include the HDD BAP Shim Layer callbacks for Fetch, TxComp, and RxPkt
    WLANBAP_STAFetchPktCBType pfnBtampFetchPktCB;
    WLANBAP_STARxCBType       pfnBtamp_STARxCB;
    WLANBAP_TxCompCBType      pfnBtampTxCompCB;

    /* Implements the callback for ALL asynchronous events. */ 
    tpWLAN_BAPEventCB         pBapHCIEventCB;

    // Include the Local Assoc structure.
    // (This gets filled during initialization. It is used, for example, to 
    // obtain the local MAC address for forming the 802.3 frame.) 
    // <<Why don't I just pull out the individ fields I need?  Like MAC addr.>>
    tBtampHCI_AMP_Assoc       btamp_AMP_Assoc;
    //LinkSupervision packet 
    tANI_BOOLEAN lsReqPktPending;   
    tANI_U8 retries;
    vos_pkt_t *pPacket;
    vos_pkt_t *lsReqPacket;
    vos_pkt_t *lsRepPacket;
    v_U16_t    lsPktln;
    WLANTL_MetaInfoType*  metaInfo; 
    tANI_BOOLEAN isBapSessionOpen;
    //End of LinkSupervision packet
#endif //BTAMP_MULTIPLE_PHY_LINKS
    WLANBAP_ConfigType   config;
    /*multiple data structures getting accessed/written from both north & south
    bound entities. To avoid multiple access, need a lock*/
    vos_lock_t           bapLock;
    // Either Block mode or Pkt mode
    v_U8_t               ucDataTrafficMode;
}*ptBtampContext; 
//tBtampContext, *ptBtampContext;

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

typedef struct sBtampLsPktData {
    v_U32_t    BufLen;
    v_U8_t     pBuf[1]; // ptr to Data Buffer
}tBtampLsPktData, *ptBtampLsPktData;

typedef struct sBtampLsPkt {
    v_U8_t     SrcMac[6];
    v_U8_t     DstMac[6];
    tBtampLsPktData Data;
} tBtampLsPkt, *ptBtampLsPkt;

/*----------------------------------------------------------------------------
 *  BAP per-session Context Data Type Declaration
 * -------------------------------------------------------------------------*/
// For now, it is just the same thing as the per application context.
typedef struct sBtampContext tBtampSessCtx;

/*----------------------------------------------------------------------------
 *  BAP state machine event definition
 * -------------------------------------------------------------------------*/
/* The event structure */ 
typedef struct sWLAN_BAPEvent {
    v_U32_t   event;  /* State machine input event message */
    v_PVOID_t params;  /* A VOID pointer type for all possible inputs */
    v_U32_t   u1;  /* introduced to handle csrRoamCompleteCallback roamStatus */
    v_U32_t   u2;  /* introduced to handle csrRoamCompleteCallback roamResult */
} tWLAN_BAPEvent, *ptWLAN_BAPEvent; 
 
// Pick up the BTAMP FSM definitions
#include "btampFsm.h"


/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/
//  The main per-Physical Link (per WLAN association) context.
//extern tBtampContext btampCtx;
extern ptBtampContext  gpBtampCtx;

//  Include the Local AMP Info structure.
extern tBtampHCI_AMP_Info        btampHCI_AMP_Info;
//  Include the Local Data Block Size info structure.
extern tBtampHCI_Data_Block_Size btampHCI_Data_Block_Size;
//  Include the Local Version info structure.
extern tBtampHCI_Version_Info   btampHCI_Version_Info;
//  Include the Local Supported Cmds info structure.
extern tBtampHCI_Supported_Cmds  btampHCI_Supported_Cmds;


/*----------------------------------------------------------------------------
 *  Function prototypes 
 * -------------------------------------------------------------------------*/

/* I don't think any of this is needed */

/* TL data path callbacks passed into WLANTL_RegisterSTAClient */

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_STAFetchPktCB 

  DESCRIPTION   
    The fetch packet callback registered with TL. 
    
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
VOS_STATUS 
WLANBAP_STAFetchPktCB 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pucSTAId,
  v_U8_t                ucAC,
  vos_pkt_t**           vosDataBuff,
  WLANTL_MetaInfoType*  tlMetaInfo
);

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_STARxCB

  DESCRIPTION   
    The receive callback registered with TL. 
    
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
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS 
WLANBAP_STARxCB
(
  v_PVOID_t              pvosGCtx, 
  vos_pkt_t*             vosDataBuff,
  v_U8_t                 ucSTAId, 
  WLANTL_RxMetaInfoType* pRxMetaInfo
);

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_TxCompCB

  DESCRIPTION   
    The tx complete callback registered with TL. 
    
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
VOS_STATUS 
WLANBAP_TxCompCB
( 
  v_PVOID_t      pvosGCtx,
  vos_pkt_t*     vosDataBuff,
  VOS_STATUS     wTxSTAtus 
);

/* Callbacks Registered with TL by WLANTL_RegisterBAPClient */

/* RSN Callback */

/*----------------------------------------------------------------------------

  DESCRIPTION   
    The receive callback registered with TL for BAP. 
    
    The registered reception callback is being triggered by TL whenever a 
    frame was received and it was filtered as a non-data BT AMP packet. 

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    vosDataBuff:   pointer to the vOSS buffer containing the received packet; 
                    no chaining will be done on this path 
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
WLANTL_BAPRxCBType WLANBAP_TLRsnRxCallback
(
 v_PVOID_t         pvosGCtx,
 vos_pkt_t*        vosDataBuff
); 

/* Flush complete Callback */

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
    ucStaId:        station identifier for the requested value
    ucTid:          identifier of the tspec 
    status:         status of the Flush operation
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_TLFlushCompCallback
( 
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucStaId, 
  v_U8_t        ucTID, 
  v_U8_t        status
);

/*----------------------------------------------------------------------------
 *  CSR Roam (Connection Status) callback 
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_RoamCallback()

  DESCRIPTION 
    Callback for Roam (connection status) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
      pContext:  is the pContext passed in with the roam request
      pParam: is a pointer to a tCsrRoamInfo, see definition of eRoamCmdStatus and
      eRoamCmdResult: for detail valid members. It may be NULL
      roamId: is to identify the callback related roam request. 0 means unsolicited
      roamStatus: is a flag indicating the status of the callback
      roamResult: is the result
   
  RETURN VALUE
    The result code associated with performing the operation  

    eHAL_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
eHalStatus
WLANBAP_RoamCallback
(
  void *pContext, 
  tCsrRoamInfo *pCsrRoamInfo,
  tANI_U32 roamId, 
  eRoamCmdStatus roamStatus, 
  eCsrRoamResult roamResult
);

/*----------------------------------------------------------------------------
 *  Utility Function prototypes 
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_CleanCB

  DESCRIPTION 
    Clear out all fields in the BAP context.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampCtx:  pointer to the BAP control block
    freeFlag:   flag indicating whether to free any allocations.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CleanCB
( 
  ptBtampContext  pBtampCtx,
  v_U32_t freeFlag // 0 /*do not empty*/);
);

/*==========================================================================

  FUNCTION    WLANBAP_GetCtxFromStaId

  DESCRIPTION 
    Called inside the BT-AMP PAL (BAP) layer whenever we need either the
    BSL context or the BTAMP context from the StaId.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    ucSTAId:   The StaId (used by TL, PE, and HAL) 
   
    OUT
    hBtampHandle: Handle (pointer to a pointer) to return the 
                  btampHandle value in.
    hHddHdl:      Handle to return the BSL context pointer in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetCtxFromStaId
( 
  v_U8_t         ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
  ptBtampHandle *hBtampHandle,  /* Handle to return per app btampHandle value in  */ 
  ptBtampContext *hBtampContext, /* Handle to return per assoc btampContext value in  */ 
  v_PVOID_t     *hHddHdl /* Handle to return BSL context in */
);

/*==========================================================================

  FUNCTION    WLANBAP_GetStaIdFromLinkCtx

  DESCRIPTION 
    Called inside the BT-AMP PAL (BAP) layer whenever we need the
    StaId (or hHddHdl) from the BTAMP context and phy_link_handle.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    hBtampHandle: Handle (pointer to a pointer) to return the 
                  btampHandle value in.
    phy_link_handle: physical link handle value. Unique per assoc. 
    
    OUT
    pucSTAId:   The StaId (used by TL, PE, and HAL) 
    hHddHdl:   Handle to return the BSL context pointer in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetStaIdFromLinkCtx
( 
  ptBtampHandle  btampHandle,  /* btampHandle value in  */ 
  v_U8_t         phy_link_handle,  /* phy_link_handle value in */
  v_U8_t        *pucSTAId,  /* The StaId (used by TL, PE, and HAL) */
  v_PVOID_t     *hHddHdl /* Handle to return BSL context */
);

/*==========================================================================

  FUNCTION    WLANBAP_CreateNewPhyLinkCtx

  DESCRIPTION 
    Called in order to create (or update) a BAP Physical Link "context"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle:     BAP app context handle
    phy_link_handle: phy_link_handle from the Command 
    pHddHdl:         BSL passes in its specific context
    
    OUT
    hBtampContext:  Handle (pointer to a pointer) to return the 
                    per "Phy Link" ptBtampContext value in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CreateNewPhyLinkCtx
( 
  ptBtampHandle  btampHandle,
  v_U8_t         phy_link_handle, /*  I get phy_link_handle from the Command */
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
  ptBtampContext *hBtampContext, /* Handle to return per assoc btampContext value in  */ 
  tWLAN_BAPRole  BAPDeviceRole  /* Needed to determine which MAC address to use for self MAC  */
);

/*==========================================================================

  FUNCTION    WLANBAP_UpdatePhyLinkCtxStaId

  DESCRIPTION 
    Called to update the STAId value associated with Physical Link "context"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampContext:   ptBtampContext to update.
    ucSTAId:         The StaId (used by TL, PE, and HAL) 
    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_UpdatePhyLinkCtxStaId
( 
  ptBtampContext pBtampContext, /* btampContext value in  */ 
  v_U8_t         ucSTAId
);

/*==========================================================================

  FUNCTION    WLANBAP_CreateNewLogLinkCtx

  DESCRIPTION 
    Called in order to allocate a BAP Logical Link "context" and "index"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampContext:  Pointer to the ptBtampContext value in.
    phy_link_handle: phy_link_handle involved 
    
    OUT
    pLog_link_handle: return the log_link_handle here 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CreateNewLogLinkCtx
( 
  ptBtampContext pBtampContext, /* pointer to the per assoc btampContext value */ 
  v_U8_t         phy_link_handle, /*  I get phy_link_handle from the Command */
  v_U8_t         tx_flow_spec[18],
  v_U8_t         rx_flow_spec[18],
  v_U16_t         *pLog_link_handle /*  Return the logical link index here */
);

 /*==========================================================================

  FUNCTION    WLANBAP_pmcFullPwrReqCB

  DESCRIPTION 
    Callback provide to PMC in the pmcRequestFullPower API. 
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    callbackContext:  The user passed in a context to identify 
    status:           The halStatus 
    
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
void 
WLANBAP_pmcFullPwrReqCB
( 
  void *callbackContext, 
  eHalStatus status
);
  
/*===========================================================================

  FUNCTION    WLANBAP_RxProcLsPkt

  DESCRIPTION 

    This API will be called when Link Supervision frames are received at BAP

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    pucAC:       Pointer to return the access category 
    RxProtoType: Protocol type of Received Packet
    vosDataBuff: The data buffer containing the 802.3 frame to be 
                 translated to BT HCI Data Packet
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_RxProcLsPkt
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  v_U16_t            RxProtoType,     /* Protocol Type from the frame received */
  vos_pkt_t         *vosRxLsBuff
);


/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_TxLinkSupervisionReq()

  DESCRIPTION 
    Implements the LinkSupervision Tx Request procedure.This will be called by APIs that want
    to transmit LinkSupervision Packets  
    Calls PktPending CB to indicate a packet is pending for transmission
    

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    phy_link_handle: Used by BAP to indentify the WLAN assoc. (StaId)
    vosDataBuff:The actual packet being sent in Tx request
    protoType : specifies if it is a LS REQ or LS REP packet 
    
     RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  Failure of Transmit procedure 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS
WLANBAP_TxLinkSupervision
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  vos_pkt_t         *vosDataBuff,
  v_U16_t           protoType
);

/*==========================================================================

  FUNCTION    WLANBAP_ReadMacConfig

  DESCRIPTION 
    This function sets the MAC config (Address and SSID to BT-AMP context
        
  DEPENDENCIES 
    
  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
void
WLANBAP_ReadMacConfig
( 
  ptBtampContext  pBtampCtx 
);

/*==========================================================================

  FUNCTION    WLANBAP_NeedBTCoexPriority

  DESCRIPTION 
    This function will cause a message to be sent to BTC firmware
    if a change in priority has occurred.  (From AMP's point-of-view.)
        
  DEPENDENCIES 
    
  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to HAL's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
void
WLANBAP_NeedBTCoexPriority
( 
  ptBtampContext  pBtampCtx, 
  v_U32_t         needCoexPriority
);


/*==========================================================================

  FUNCTION    WLANBAP_RxCallback

  DESCRIPTION 
    This function is called by TL call this function for all frames except for Data frames
        
  DEPENDENCIES 
    
  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context
    pPacket         Vos packet
    frameType       Frame type
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANBAP_RxCallback
(
    v_PVOID_t               pvosGCtx, 
    vos_pkt_t              *pPacket,
    WLANTL_BAPFrameEnumType frameType
);


/*===========================================================================

  FUNCTION    WLANBAP_InitLinkSupervision

  DESCRIPTION 

    This API will be called when Link Supervision module is to be initialized when connected at BAP

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
#define TX_LS_DATALEN   32

VOS_STATUS
WLANBAP_InitLinkSupervision
( 
  ptBtampHandle     btampHandle
);


/*===========================================================================

  FUNCTION    WLANBAP_DeInitLinkSupervision

  DESCRIPTION 

    This API will be called when Link Supervision module is to be stopped after disconnected at BAP 

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_DeInitLinkSupervision
( 
  ptBtampHandle     btampHandle 
);

void WLAN_BAPEstablishLogicalLink(ptBtampContext btampContext);

 #ifdef __cplusplus
 }


#endif 


#endif /* #ifndef WLAN_QCT_WLANBAP_INTERNAL_H */

