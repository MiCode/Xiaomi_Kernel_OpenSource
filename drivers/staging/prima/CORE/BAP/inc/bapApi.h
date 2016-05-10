/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef WLAN_QCT_WLANBAP_H
#define WLAN_QCT_WLANBAP_H

/*===========================================================================

               W L A N   B T - A M P  P A L   L A Y E R 
                       E X T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the external API exposed by the wlan BT-AMP PAL layer 
  module.
  
      
  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /cygdrive/d/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_BTAMP_RSN/CORE/BAP/inc/bapApi.h,v 1.21 2009/03/09 08:58:26 jzmuda Exp jzmuda $ $DateTime: $ $Author: jzmuda $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
07/01/08    jez     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_api.h" 
#include "vos_packet.h" 
//I need the TL types and API
#include "wlan_qct_tl.h"

/* BT-AMP PAL API structure types  - FramesC generated */ 
#include "btampHCI.h" 

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 

/*----------------------------------------------------------------------------
 *  HCI Interface supported
 * 
 *   Here we list the HCI Commands and Events which our 802.11 BT-AMP PAL
 *   supports.
 * 
 * -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Supported HCI Commands 
---------------------------------------------------------------------------*/      
#if 0
/** BT v3.0 Link Control commands */
    BTAMP_TLV_HCI_CREATE_PHYSICAL_LINK_CMD,
    BTAMP_TLV_HCI_ACCEPT_PHYSICAL_LINK_CMD,
    BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_CMD,
    BTAMP_TLV_HCI_CREATE_LOGICAL_LINK_CMD,
    BTAMP_TLV_HCI_ACCEPT_LOGICAL_LINK_CMD,
    BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_CMD,
    BTAMP_TLV_HCI_LOGICAL_LINK_CANCEL_CMD,
    BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_CMD,
/*
Host Controller and Baseband Commands
*/
    BTAMP_TLV_HCI_RESET_CMD,
    BTAMP_TLV_HCI_SET_EVENT_MASK_CMD,
    BTAMP_TLV_HCI_FLUSH_CMD,
    BTAMP_TLV_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD,
    BTAMP_TLV_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD,
    BTAMP_TLV_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD,
    BTAMP_TLV_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD,
/* v3.0 Host Controller and Baseband Commands */
    BTAMP_TLV_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD,
    BTAMP_TLV_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD,
    BTAMP_TLV_HCI_SET_EVENT_MASK_PAGE_2_CMD,
    BTAMP_TLV_HCI_READ_LOCATION_DATA_CMD,
    BTAMP_TLV_HCI_WRITE_LOCATION_DATA_CMD,
    BTAMP_TLV_HCI_READ_FLOW_CONTROL_MODE_CMD,
    BTAMP_TLV_HCI_WRITE_FLOW_CONTROL_MODE_CMD,
    BTAMP_TLV_HCI_READ_BEST_EFFORT_FLUSH_TO_CMD,
    BTAMP_TLV_HCI_WRITE_BEST_EFFORT_FLUSH_TO_CMD,
/** opcode definition for this command from AMP HCI CR D9r4 markup */
    BTAMP_TLV_HCI_SET_SHORT_RANGE_MODE_CMD,
/* End of v3.0 Host Controller and Baseband Commands */
/*
Informational Parameters
*/
    BTAMP_TLV_HCI_READ_LOCAL_VERSION_INFORMATION_CMD,
    BTAMP_TLV_HCI_READ_LOCAL_SUPPORTED_COMMANDS_CMD,
    BTAMP_TLV_HCI_READ_BUFFER_SIZE_CMD,
/* v3.0 Informational commands */
    BTAMP_TLV_HCI_READ_DATA_BLOCK_SIZE_CMD,
/*
Status Parameters
*/
    BTAMP_TLV_HCI_READ_FAILED_CONTACT_COUNTER_CMD,
    BTAMP_TLV_HCI_RESET_FAILED_CONTACT_COUNTER_CMD,
    BTAMP_TLV_HCI_READ_LINK_QUALITY_CMD,
    BTAMP_TLV_HCI_READ_RSSI_CMD,
    BTAMP_TLV_HCI_READ_LOCAL_AMP_INFORMATION_CMD,
    BTAMP_TLV_HCI_READ_LOCAL_AMP_ASSOC_CMD,
    BTAMP_TLV_HCI_WRITE_REMOTE_AMP_ASSOC_CMD,
/*
Debug Commands
*/
    BTAMP_TLV_HCI_READ_LOOPBACK_MODE_CMD,
    BTAMP_TLV_HCI_WRITE_LOOPBACK_MODE_CMD,
#endif

/*---------------------------------------------------------------------------
  Supported HCI Events
---------------------------------------------------------------------------*/      
#if 0
/** BT events */
    BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT,
    BTAMP_TLV_HCI_COMMAND_STATUS_EVENT,
    BTAMP_TLV_HCI_HARDWARE_ERROR_EVENT,
    BTAMP_TLV_HCI_FLUSH_OCCURRED_EVENT,
    BTAMP_TLV_HCI_LOOPBACK_COMMAND_EVENT,
    BTAMP_TLV_HCI_DATA_BUFFER_OVERFLOW_EVENT,
    BTAMP_TLV_HCI_QOS_VIOLATION_EVENT,
/** BT v3.0 events */
    BTAMP_TLV_HCI_GENERIC_AMP_LINK_KEY_NOTIFICATION_EVENT,
    BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT ,
    BTAMP_TLV_HCI_CHANNEL_SELECTED_EVENT ,
    BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT ,
    BTAMP_TLV_HCI_PHYSICAL_LINK_LOSS_WARNING_EVENT ,
    BTAMP_TLV_HCI_PHYSICAL_LINK_RECOVERY_EVENT ,
    BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT ,
    BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT ,
    BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_COMPLETE_EVENT ,
    BTAMP_TLV_HCI_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT ,
#endif


/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
  Packet type defines for the AMP to PAL packet encapsulation.
---------------------------------------------------------------------------*/      
#define WLANBAP_HCI_COMMAND_PACKET      0x01     /**< HCI command packet type, characterizing packet types over the
                                              UART and RS232 transports */
#define WLANBAP_HCI_ACL_DATA_PACKET     0x02     /**< HCI ACL data packet type, characterizing packet types over the
                                              UART and RS232 transports */
#define WLANBAP_HCI_SCO_DATA_PACKET     0x03     /**< HCI SCO data packet type, characterizing packet types over the
                                              UART and RS232 transports */
#define WLANBAP_HCI_EVENT_PACKET        0x04     /**< HCI event packet type, characterizing packet types over the
                                              UART and RS232 transports */
/*---------------------------------------------------------------------------
  HCI Data packet size limitation.
---------------------------------------------------------------------------*/      
#define WLANBAP_MAX_80211_PAL_PDU_SIZE                1492

/*---------------------------------------------------------------------------
  HCI Flow Control Modes.
---------------------------------------------------------------------------*/      
#define WLANBAP_FLOW_CONTROL_MODE_PACKET_BASED        0x00
#define WLANBAP_FLOW_CONTROL_MODE_BLOCK_BASED         0x01

/*---------------------------------------------------------------------------
  BT "assigned numbers"
---------------------------------------------------------------------------*/      
// Qualcomm Company ID
#define WLANBAP_QUALCOMM_COMPANY_ID                     29

// HCI Interface version 
// Parameter Name               Assigned Values 
// HCI_Version                  0  => Bluetooth HCI Specification 1.0B 
//                              1  => Bluetooth HCI Specification 1.1 
//                              2  => Bluetooth HCI Specification 1.2 
//                              3  => Bluetooth HCI Specification 2.0 
//                              4  => Bluetooth HCI Specification 2.1 
//                              5  => Bluetooth HCI Specification 3.0 
#define WLANBAP_HCI_VERSION                              5
#define WLANBAP_HCI_REVISION                             0 
#define WLANBAP_PAL_VERSION                              0x01
#define WLANBAP_PAL_SUBVERSION                           0x00

// AMP device status 
#define WLANBAP_HCI_AMP_STATUS_POWERED_DOWN              0x00
#define WLANBAP_HCI_AMP_STATUS_NOT_SHARED                0x01
#define WLANBAP_HCI_AMP_STATUS_SHARED                    0x02
#define WLANBAP_HCI_AMP_STATUS_RESERVED                  0x03

// ACL Packet types (AMP only uses 0x03) 
#define WLANBAP_HCI_PKT_START_NON_FLUSH                  0x00
#define WLANBAP_HCI_PKT_CONT                             0x01
#define WLANBAP_HCI_PKT_START_FLUSH                      0x02
#define WLANBAP_HCI_PKT_AMP                              0x03

/*---------------------------------------------------------------------------
  BT-AMP PAL supported commands defines 

  The Supported Commands configuration parameter lists which HCI commands the 
local controller supports. It is implied that if a command is listed as 
supported, the feature underlying that command is also supported.
  The Supported Commands is a 64 octet bit field. If a bit is set to 1, then 
this command is supported.

---------------------------------------------------------------------------*/      
//    0     1     2     3     4     5     6     7

#define WLANBAP_PAL_SUPPORTED_HCI_CMDS {  \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02, 0x0c, \
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x88, 0x3c, \
    0x00, 0x00, 0x00, 0x40, 0x00, 0xff, 0xff, 0x07, \
    0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
}

/*---------------------------------------------------------------------------
  BT-AMP PAL "status" and "reason" error code defines 
---------------------------------------------------------------------------*/      
#define WLANBAP_STATUS_SUCCESS                        (0x00) /* Success. Section 3.1.7 */

#define WLANBAP_ERROR_UNKNOWN_HCI_CMND                (0x01)  
#define WLANBAP_ERROR_NO_CNCT                         (0x02)  /* AMP HCI Section 7.1.39 */
#define WLANBAP_ERROR_HARDWARE_FAILURE                (0x03) 
#define WLANBAP_ERROR_PAGE_TIMEOUT                    (0x04)  
/* Section 3.1.10 has this mis-identified as 0x08  */ 
#define WLANBAP_ERROR_AUTHENT_FAILURE                 (0x05)  
#define WLANBAP_ERROR_KEY_MISSING                     (0x06) 
#define WLANBAP_ERROR_MEMORY_FULL                     (0x07) 
#define WLANBAP_ERROR_CNCT_TIMEOUT                    (0x08)  /* Section 3.1.8 */
#define WLANBAP_ERROR_MAX_NUM_CNCTS                   (0x09)  /* Section 3.1.8 */   
#define WLANBAP_ERROR_MAX_NUM_SCO_CNCTS               (0x0a)      
#define WLANBAP_ERROR_MAX_NUM_ACL_CNCTS               (0x0b)      
#define WLANBAP_ERROR_CMND_DISALLOWED                 (0x0c)  /* Section 4.1 */    
#define WLANBAP_ERROR_HOST_REJ_RESOURCES              (0x0d)  /* Section 3.1.7 */ 
#define WLANBAP_ERROR_HOST_REJ_SECURITY               (0x0e)      
#define WLANBAP_ERROR_HOST_REJ_PERSONAL_DEV           (0x0f)      
#define WLANBAP_ERROR_HOST_TIMEOUT                    (0x10)      
#define WLANBAP_ERROR_UNSUPPORT_FEAT_PARAM            (0x11)      
#define WLANBAP_ERROR_INVALID_HCI_CMND_PARAM          (0x12)     
#define WLANBAP_ERROR_TERM_CNCT_USER_ENDED            (0x13)      
#define WLANBAP_ERROR_TERM_CNCT_LOW_RESOURCE          (0x14)      
#define WLANBAP_ERROR_TERM_CNCT_POWER_OFF             (0x15)      
/* Section 3.1.9 has a contradictory semantics of "failed connection" */    
#define WLANBAP_ERROR_TERM_BY_LOCAL_HOST              (0x16) /* Section 3.1.8 */  
#define WLANBAP_ERROR_REPEATED_ATTEMPTS               (0x17)      
#define WLANBAP_ERROR_PAIRING_NOT_ALLOWED             (0x18)      
#define WLANBAP_ERROR_UNKNOWN_LMP_PDU                 (0x19)      
#define WLANBAP_ERROR_UNSUPPORTED_REMOTE_FEAT         (0x1a)      
#define WLANBAP_ERROR_SCO_REJ                         (0x1b)      
#define WLANBAP_ERROR_SCO_INTERVAL_REJ                (0x1c)      
#define WLANBAP_ERROR_SCO_AIR_MODE_REJ                (0x1d)      
#define WLANBAP_ERROR_INVALID_LMP_PARAMETER           (0x1e)      
#define WLANBAP_ERROR_UNSPECIFIED_ERROR               (0x1f)      
#define WLANBAP_ERROR_UNSUPPORTED_LMP_PARAM           (0x20)      
#define WLANBAP_ERROR_ROLE_CHANGE_NOT_ALLOWED         (0x21)      
#define WLANBAP_ERROR_LMP_RESPONSE_TIMEOUT            (0x22)      
#define WLANBAP_ERROR_LMP_ERROR_TRANS_COLLISION       (0x23)      
#define WLANBAP_ERROR_LMP_PDU_NOT_ALLOWED             (0x24)      
#define WLANBAP_ERROR_ENCRYPTION_MODE_NOT_ACCEPTABLE  (0x25)      
#define WLANBAP_ERROR_UNIT_KEY_USED                   (0x26)      
#define WLANBAP_ERROR_QOS_IS_NOT_SUPPORTED            (0x27)      
#define WLANBAP_ERROR_INSTANT_PASSED                  (0x28)      
#define WLANBAP_ERROR_UNIT_KEY_PAIRING_UNSUPPORTED    (0x29)      

#define WLANBAP_ERROR_DIFFERENT_TRANS_COLLISION       (0x2A)      

/* reserved                                           (0x2B) */

#define WLANBAP_ERROR_QOS_UNACCEPTABLE_PARAMETER      (0x2C)      
#define WLANBAP_ERROR_QOS_REJECTED                    (0x2D)      
#define WLANBAP_ERROR_CHANNEL_CLASSIFICATION_NS       (0x2E)      
#define WLANBAP_ERROR_INSUFFICIENT_SECURITY           (0x2F)      
#define WLANBAP_ERROR_PARM_OUT_OF_MANDATORY_RANGE     (0x30)      
                                        
/* reserved                                           (0x31) */

#define WLANBAP_ERROR_ROLE_SWITCH_PENDING             (0x32)      

/* reserved                                           (0x33) */

#define WLANBAP_ERROR_RESERVED_SLOT_VIOLATION         (0x34)      
#define WLANBAP_ERROR_ROLE_SWITCH_FAILED              (0x35)      
#define WLANBAP_ERROR_EIR_TOO_LARGE                   (0x36)      
#define WLANBAP_ERROR_SSP_NOT_SUPPORTED_BY_HOST       (0x37)      
#define WLANBAP_ERROR_HOST_BUSY_PAIRING               (0x38)      
#define WLANBAP_ERROR_NO_SUITABLE_CHANNEL             (0x39)
#define WLANBAP_ERROR_CONTROLLER_BUSY                 (0x3A)      

/*----------------------------------------------------------------------------
 *   Event_Mask_Page_2 defines for events
 * -------------------------------------------------------------------------*/
#define WLANBAP_EVENT_MASK_NONE                    0x0000000000000000 //No events specified (default)
#define WLANBAP_EVENT_MASK_PHY_LINK_COMPLETE_EVENT 0x0000000000000001 //Physical Link Complete Event
#define WLANBAP_EVENT_MASK_CHANNEL_SELECTED_EVENT  0x0000000000000002 //Channel Selected Event
#define WLANBAP_EVENT_MASK_DISC_PHY_LINK_EVENT     0x0000000000000004 //Disconnection Physical Link Event
#define WLANBAP_EVENT_MASK_PHY_LINK_LOSS_EARLY_WARNING_EVENT 0x0000000000000008 //Physical Link Loss Early Warning Event
#define WLANBAP_EVENT_MASK_PHY_LINK_RECOVERY_EVENT 0x0000000000000010 //Physical Link Recovery Event
#define WLANBAP_EVENT_MASK_LOG_LINK_COMPLETE_EVENT 0x0000000000000020 //Logical Link Complete Event
#define WLANBAP_EVENT_MASK_DISC_LOG_LINK_COMPLETE_EVENT 0x0000000000000040 //Disconnection Logical Link Complete Event
#define WLANBAP_EVENT_MASK_FLOW_SPEC_MOD_COMPLETE_EVENT 0x0000000000000080 //Flow Spec Modify Complete Event
#define WLANBAP_EVENT_MASK_NUM_COMPLETED_DATA_BLOCKS_EVENT 0x0000000000000100 //Number of Completed Data Blocks Event
#define WLANBAP_EVENT_MASK_AMP_START_TEST_EVENT    0x0000000000000200 //AMP Start Test Event
#define WLANBAP_EVENT_MASK_AMP_TEST_END_EVENT      0x0000000000000400 //AMP Test End Event
#define WLANBAP_EVENT_MASK_AMP_RCVR_REPORT_EVENT   0x0000000000000800 //AMP Receiver Report Event
#define WLANBAP_EVENT_MASK_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT 0x0000000000001000 //Short Range Mode Change Complete Event
#define WLANBAP_EVENT_MASK_AMP_STATUS_CHANGE_EVENT 0x0000000000002000 //AMP Status Change Event
#define WLANBAP_EVENT_MASK_RESERVED                0xFFFFFFFFFFFFC000 //Reserved for future use

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Opaque BAP handle Type Declaration 
 * -------------------------------------------------------------------------*/
typedef v_PVOID_t tBtampHandle, *ptBtampHandle;

/*----------------------------------------------------------------------------
 *  BAP per-session Context Data Type Declaration
 * -------------------------------------------------------------------------*/
// Move this to bapInternal.h, where it belongs.
// For now, it is just the same thing as the per application context.
//typedef struct sBtampContext tBtampSessCtx;


/*---------------------------------------------------------------------------
  HCI Event union
---------------------------------------------------------------------------*/      
typedef struct sBtampHCI_Event {
    v_U8_t  bapHCIEventCode;  /* The event code.  To dis-ambiguate. */
    union { 
        tBtampTLVHCI_Channel_Selected_Event  btampChannelSelectedEvent;
        tBtampTLVHCI_Command_Complete_Event btampCommandCompleteEvent ;
        tBtampTLVHCI_Command_Status_Event btampCommandStatusEvent ;
        tBtampTLVHCI_Data_Buffer_Overflow_Event btampDataBufferOverflowEvent ;
        tBtampTLVHCI_Disconnect_Logical_Link_Complete_Event btampDisconnectLogicalLinkCompleteEvent ;
        tBtampTLVHCI_Disconnect_Physical_Link_Complete_Event btampDisconnectPhysicalLinkCompleteEvent ;
        /* Flow_Spec_Modify_Complete_Event is generated after the flow spec modify cmd completes */
        tBtampTLVHCI_Flow_Spec_Modify_Complete_Event btampFlowSpecModifyCompleteEvent ;
        /* Asynchronous Flush_Occurred Event CAN ALSO BE generated after the flush cmd completes */
        tBtampTLVHCI_Flush_Occurred_Event btampFlushOccurredEvent ;
        tBtampTLVHCI_Generic_AMP_Link_Key_Notification_Event btampGenericAMPLinkKeyNotificationEvent ;
        tBtampTLVHCI_Hardware_Error_Event btampHardwareErrorEvent ;
        tBtampTLVHCI_Logical_Link_Complete_Event btampLogicalLinkCompleteEvent ;
        tBtampTLVHCI_Loopback_Command_Event btampLoopbackCommandEvent ;
        tBtampTLVHCI_Physical_Link_Complete_Event btampPhysicalLinkCompleteEvent ;
        tBtampTLVHCI_Physical_Link_Loss_Warning_Event btampPhysicalLinkLossWarningEvent ;
        tBtampTLVHCI_Physical_Link_Recovery_Event btampPhysicalLinkRecoveryEvent ;
        tBtampTLVHCI_Qos_Violation_Event btampQosViolationEvent ;
        tBtampTLVHCI_Short_Range_Mode_Change_Complete_Event btampShortRangeModeChangeCompleteEvent ;
        tBtampTLVHCI_Num_Completed_Pkts_Event btampNumOfCompletedPktsEvent;
        tBtampTLVHCI_Num_Completed_Data_Blocks_Event btampNumOfCompletedDataBlocksEvent;
        tBtampTLVHCI_Enhanced_Flush_Complete_Event btampEnhancedFlushCompleteEvent ;
    } u;
} tBtampHCI_Event, *tpBtampHCI_Event;

/* 802.3 header */
typedef struct 
{
 /* Destination address field */
 v_U8_t   vDA[VOS_MAC_ADDR_SIZE];

 /* Source address field */
 v_U8_t   vSA[VOS_MAC_ADDR_SIZE];

 /* Length field */
 v_U16_t  usLenType;  /* Num bytes in info field (i.e., exclude 802.3 hdr) */
                      /* Max length 1500 (0x5dc) (What about 0x5ee? That
                       * includes 802.3 Header and FCS.) */
}WLANBAP_8023HeaderType;


/* 
 * A list of Command Complete event msgs which will be 
 * signalled by the Event Callback 
 */ 
#if 0
/* The tBtampTLVHCI_Command_Complete_Event structure includes each of these*/
/* HCI Reset: status */
/* HCI Flush: status, log_link_handle */

#endif

/* 
 * Command Complete event msgs which will be formed by the caller 
 * Now an invocation of btampPackTlvHCI_Command_Complete_Event()
 * supports generating command complete event messages for all commands...  
 */
/* The tBtampTLVHCI_Command_Complete_Event structure includes each of these*/
#if 0
/* HCI Cancel Logical Link: status, phy_link_handle, tx_flow_spec_id */
/* HCI Set Event Mask: status */
/* HCI Read Connection Accept Timeout: status, connection_accept_timeout */
/* HCI Write Connection Accept Timeout:  status */
/* HCI Read Link Supervision Timeout: status, log_link_handle (8 sig bits only), link_supervision_timeout */
/* HCI Write Link Supervision Timeout: status, log_link_handle (8 bits sig only) */
/* HCI Read Logical Link Accept Timeout: status, logical_link_accept_timeout */
/* HCI Write Logical Link Accept Timeout:  status */
/* HCI Set Event Mask Page 2: status */
/* HCI Read Location Data: status, loc_domain_aware, loc_domain, loc_options */
/* HCI Write Location Data:  status */
/* HCI Read Flow Control Mode: status, flow_control_mode */
/* HCI Write Flow Control Mode:  status */
/* HCI Read Best Effort Flush Timeout: status, (logical_link_handle ? No!), best_effort_flush_timeout */
/* HCI Write Best Effort Flush Timeout:  status */
/* HCI Set Short Range Mode:  status */
/* HCI Read Local Version Info: status, HC_HCI_Version, HC_HCI_Revision, HC_PAL_Version, HC_Manufac_Name, HC_PAL_Sub_Version */
/* HCI Read Local supported commands: status, HC_Support_Cmds */
/* HCI Read Buffer Size:    status, HC_ACL_Data_Packet_Length,  HC_SCO_Packet_Length,  HC_Total_Num_ACL_Packets, HC_Total_Num_SCO_Packets */
/* HCI Read Data Block Size:   status, HC_Max_ACL_Data_Packet_Length,  HC_Data_Block_Length, HC_Total_Num_Data_Blocks */
/* HCI Read Failed Contact Counter: status, log_link_handle, *pFailedContactCounter */
/* HCI Reset Failed Contact Counter: status, log_link_handle */
/* HCI Read Link Quality: status, log_link_handle(?Yes!?), link_quality */
/* HCI Read RSSI: status, phy_link_handle, rssi */
/* HCI Read Local AMP Info: status, HC_AMP_Status, HC_Total_BW, HC_Max_Guaranteed_BW, HC_Min_Latency, HC_Max_PDU_Size, HC_Controller_Type, HC_PAL_Capabilities,  HC_AMP_Assoc_Length,  HC_Max_Flush_Timeout, HC_BE_Flush_Timeout */
/* HCI Read Local AMP Assoc:  status, phy_link_handle, AMP ASSOC remaining length (just actual length, in practice), AMP ASSOC fragment (byte string) */ 
/*  where AMP Assoc consists of:   HC_mac_addr,  pref channel (HC_pref_country, HC_pref_triplets), Cnct channel (HC_cnct_country,  HC_cnct_triplets), HC_pal_capabilities, HC_pal_version */ 
/* HCI Write Remote AMP Assoc:  status, phy_link_handle */
/* HCI Read Loopback Mode: status, loopback_mode */
/* HCI Write Loopback Mode:  status */

#endif

/* BT AMP configuration items */
typedef struct 
{
 /* user preferred channel on which we start the link */
 v_U8_t   ucPreferredChannel;

}WLANBAP_ConfigType;

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
    VOSS interfaces - Device initialization 
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_Open

  DESCRIPTION 
    Called at driver initialization (vos_open). BAP will initialize 
    all its internal resources and will wait for the call to start to 
    register with the other modules. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Open
( 
  v_PVOID_t  pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANBAP_Start

  DESCRIPTION 
    Called as part of the overall start procedure (vos_start). BAP will 
    use this call to register with TL as the BAP entity for 
    BT-AMP RSN frames. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other codes can be returned as a result of a BAL failure;
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Start
( 
  v_PVOID_t  pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANBAP_Stop

  DESCRIPTION 
    Called by vos_stop to stop operation in BAP, before close. BAP will suspend all 
    BT-AMP Protocol Adaption Layer operation and will wait for the close 
    request to clean up its resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Stop
( 
  v_PVOID_t  pvosGCtx 
);

/*==========================================================================

  FUNCTION    WLANBAP_Close

  DESCRIPTION 
    Called by vos_close during general driver close procedure. BAP will clean up 
    all the internal resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Close
( 
  v_PVOID_t  pvosGCtx 
);

/*----------------------------------------------------------------------------
    HDD interfaces - Per instance initialization 
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_GetNewHndl

  DESCRIPTION 
    Called by HDD at driver open (BSL_Open). BAP will initialize 
    allocate a per-instance "file handle" equivalent for this specific
    open call. 
    
    There should only ever be one call to BSL_Open.  Since 
    the open app user is the BT stack.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    hBtampHandle:   Handle to return btampHandle value in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetNewHndl
( 
  ptBtampHandle *hBtampHandle  /* Handle to return btampHandle value in  */ 
);

/*==========================================================================

  FUNCTION    WLANBAP_ReleaseHndl

  DESCRIPTION 
    Called by HDD at driver close (BSL_Close). BAP will reclaim (invalidate) 
    the "file handle" passed into this call.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle:   btampHandle value to invalidate.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to btampHandle is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_ReleaseHndl
( 
  ptBtampHandle btampHandle  /* btamp handle value to release  */ 
);

/*----------------------------------------------------------------------------
    HDD interfaces - Data plane
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
    HDD Data callbacks 
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    (*WLANBAP_STAFetchPktCBType)() 

  DESCRIPTION   
    Type of the fetch packet callback registered with BAP by HDD. 
    
    It is called by the BAP immediately upon the underlying 
    WLANTL_STAFetchPktCBType routine being called.  Which is called by
    TL when the scheduling algorithms allows for transmission of another 
    packet to the module. 
    
    This function is here to "wrap" or abstract WLANTL_STAFetchPktCBType.
    Because the BAP-specific HDD "shim" layer (BSL) doesn't know anything 
    about STAIds, or other parameters required by TL.  


  PARAMETERS 

    IN
    pHddHdl:        The HDD(BSL) specific context for this association.
                    Use the STAId passed to me by TL in WLANTL_STAFetchCBType
                    to retreive this value.

    IN/OUT
    pucAC:          access category requested by TL, if HDD does not have 
                    packets on this AC it can choose to service another AC 
                    queue in the order of priority

    OUT
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted 
    tlMetaInfo:    meta info related to the data frame


  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/

typedef VOS_STATUS (*WLANBAP_STAFetchPktCBType)( 
                                            v_PVOID_t             pHddHdl,
                                            WLANTL_ACEnumType     ucAC,
                                            vos_pkt_t**           vosDataBuff,
                                            WLANTL_MetaInfoType*  tlMetaInfo);

 

/*----------------------------------------------------------------------------

  FUNCTION    (*WLANBAP_STARxCBType)( )

  DESCRIPTION   
    Type of the receive callback registered with BAP by HDD. 
    
    It is called by the BAP immediately upon the underlying 
    WLANTL_STARxCBType routine being called.  Which is called by
    TL to notify when a packet was received for a registered STA.

  PARAMETERS 

    IN
    pHddHdl:        The HDD(BSL) specific context for this association.
                    Use the STAId passed to me by TL in WLANTL_STARxCBType
                    to retrieve this value.

    vosDataBuff:    pointer to the VOSS data buffer that was received
                    (it may be a linked list) 
    pRxMetaInfo:    Rx meta info related to the data frame
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANBAP_STARxCBType)( v_PVOID_t      pHddHdl,
                                          vos_pkt_t*         vosDataBuff,
                                          WLANTL_RxMetaInfoType* pRxMetaInfo);



/*----------------------------------------------------------------------------

  FUNCTION    (*WLANBAP_TxCompCBType)()

  DESCRIPTION   
    Type of the tx complete callback registered with BAP by HDD. 
    
    It is called by the BAP immediately upon the underlying 
    WLANTL_TxCompCBType routine being called.  Which is called by
    TL to notify when a transmission for a packet has ended.

  PARAMETERS 

    IN
    pHddHdl:        The HDD(BSL) specific context for this association.
                    <<How do I retrieve this from pvosGCtx? Which is all 
                    the TL WLANTL_TxCompCBType routine provides me.>>
    vosDataBuff:    pointer to the VOSS data buffer that was transmitted 
    wTxSTAtus:      status of the transmission 

  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
typedef VOS_STATUS (*WLANBAP_TxCompCBType)( v_PVOID_t      pHddHdl,
                                           vos_pkt_t*     vosDataBuff,
                                           VOS_STATUS     wTxSTAtus );

/*----------------------------------------------------------------------------
    HDD Data plane API  
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_RegisterDataPlane

  DESCRIPTION 
    The HDD calls this routine to register the "data plane" routines
    for Tx, Rx, and Tx complete with BT-AMP.  For now, with only one
    physical association supported at a time, this COULD be called 
    by HDD at the same time as WLANBAP_GetNewHndl.  But, in general
    it needs to be called upon each new physical link establishment.
    
    This registration is really two part.  The routines themselves are
    registered here.  But, the mapping between the BSL context and the
    actual physical link takes place during WLANBAP_PhysicalLinkCreate. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_RegisterDataPlane
( 
  ptBtampHandle btampHandle,  /* BTAMP context */ 
  WLANBAP_STAFetchPktCBType pfnBtampFetchPktCB, 
  WLANBAP_STARxCBType pfnBtamp_STARxCB,
  WLANBAP_TxCompCBType pfnBtampTxCompCB,
  // phy_link_handle, of course, doesn't come until much later.  At Physical Link create.
  v_PVOID_t      pHddHdl   /* BSL specific context */
);
//#endif

/*===========================================================================

  FUNCTION    WLANBAP_XlateTxDataPkt

  DESCRIPTION 

    HDD will call this API when it has a HCI Data Packet and it wants 
    to translate it into a 802.3 LLC frame - ready to send using TL.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    phy_link_handle: Used by BAP to indentify the WLAN assoc. (StaId) 

    pucAC:       Pointer to return the access category 
    vosDataBuff: The data buffer containing the BT-AMP packet to be 
                 translated to an 802.3 LLC frame
    tlMetaInfo:  return meta info gleaned from the outgoing frame, here.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_XlateTxDataPkt
( 
  ptBtampHandle     btampHandle,  /* Used by BAP to identify the actual session
                                    and therefore addresses */ 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  WLANTL_ACEnumType           *pucAC,        /* Return the AC here */
  WLANTL_MetaInfoType  *tlMetaInfo, /* Return the MetaInfo here. An assist to WLANBAP_STAFetchPktCBType */
  vos_pkt_t        *vosDataBuff
);

/*===========================================================================

  FUNCTION    WLANBAP_XlateRxDataPkt

  DESCRIPTION 

    HDD will call this API when it has received a 802.3 (TL/UMA has 
    Xlated from 802.11) frame from TL and it wants to form a 
    BT HCI Data Packet - ready to signal up to the BT stack application.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    pucAC:       Pointer to return the access category 
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
WLANBAP_XlateRxDataPkt
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  WLANTL_ACEnumType           *pucAC,        /* Return the AC here. I don't think this is needed */
  vos_pkt_t        *vosDataBuff
);

/*===========================================================================

  FUNCTION    WLANBAP_STAPktPending

  DESCRIPTION 

    HDD will call this API when a packet is pending transmission in its 
    queues. HDD uses this instead of WLANTL_STAPktPending because he is
    not aware of the mapping from session to STA ID.

  DEPENDENCIES 

    HDD must have called WLANBAP_GetNewHndl before calling this API.

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
                 BSL can obtain this from the physical handle value in the
                 downgoing HCI Data Packet. He, after all, was there
                 when the PhysicalLink was created. He knew the btampHandle 
                 value returned by WLANBAP_GetNewHndl. He knows as well, his
                 own pHddHdl (see next).
    phy_link_handle: Used by BAP to indentify the WLAN assoc. (StaId)
    ucAc:        The access category for the pending frame
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_STAPktPending 
( 
  ptBtampHandle  btampHandle,  /* Used by BAP to identify the app context and VOSS ctx (!?) */ 
  v_U8_t         phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  WLANTL_ACEnumType ucAc   /* This is the first instance of a TL type in bapApi.h */
);

/*----------------------------------------------------------------------------
 *   BT-AMP PAL HCI Event callback types
 *--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    (*tpWLAN_BAPEventCB)() 

  DESCRIPTION 
    Implements the callback for ALL asynchronous events. 
    Including Events resulting from:
     * HCI Create Physical Link, 
     * Disconnect Physical Link, 
     * Create Logical Link,
     * Flow Spec Modify, 
     * HCI Reset,
     * HCI Flush,...

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pHddHdl:     The HDD(BSL) specific context for this association.
                 BSL gets this from the downgoing packets Physical handle
                 value. 
    pBapHCIEvent:  pointer to the union of "HCI Event" structures.  Contains all info 
                   needed for HCI event.
    assoc_specific_event:  flag indicates assoc-specific (1) or global (0) event
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
typedef VOS_STATUS (*tpWLAN_BAPEventCB)
(
  v_PVOID_t      pHddHdl,   /* this could refer to either the BSL per 
                               association context which got passed in during 
                               register data plane OR the BSL per application 
                               context passed in during register BAP callbacks 
                               based on setting of the Boolean flag below */ 
                            /* It's like each of us is using the other */
                            /* guys reference when invoking him. */
  tpBtampHCI_Event pBapHCIEvent, /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
  v_BOOL_t assoc_specific_event /* Flag to indicate global or assoc-specific event */
);   


/*----------------------------------------------------------------------------
    HCI Event Callback Registration routine
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPRegisterBAPCallbacks() 

  DESCRIPTION 
    Register the BAP "Event" callbacks.
    Return the per instance handle.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    pBapHCIEventCB:  pointer to the Event callback
    pAppHdl:  The context passed in by caller. (I.E., BSL app specific context.)

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEventCB is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPRegisterBAPCallbacks 
( 
  ptBtampHandle           btampHandle, /* BSL uses my handle to talk to me */
                            /* Returned from WLANBAP_GetNewHndl() */
                            /* It's like each of us is using the other */
                            /* guys reference when invoking him. */
  tpWLAN_BAPEventCB       pBapHCIEventCB, /*Implements the callback for ALL asynchronous events. */ 
  v_PVOID_t               pAppHdl  // Per-app BSL context
);



/*----------------------------------------------------------------------------
    Host Controller Interface Procedural API
 ---------------------------------------------------------------------------*/

/** BT v3.0 Link Control commands */

/*----------------------------------------------------------------------------
    Each of the next eight command result in asynchronous events (e.g.,  
    HCI_PHYSICAL_LINK_COMPLETE_EVENT, HCI_LOGICAL_LINK_COMPLETE_EVENT, etc...)
    These are signalled thru the event callback. (I.E., (*tpWLAN_BAPEventCB).)
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkCreate()

  DESCRIPTION 
    Implements the actual HCI Create Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
                 WLANBAP_GetNewHndl has to be called before every call to 
                 WLAN_BAPPhysicalLinkCreate. Since the context is per 
                 physical link.
    pBapHCIPhysLinkCreate:  pointer to the "HCI Create Physical Link" Structure.
    pHddHdl:  The context passed in by the caller. (e.g., BSL specific context)

    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkCreate is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkCreate 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Create_Physical_Link_Cmd   *pBapHCIPhysLinkCreate,
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
                            /* And I get phy_link_handle from the Command */
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkAccept()

  DESCRIPTION 
    Implements the actual HCI Accept Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIPhysLinkAccept:  pointer to the "HCI Accept Physical Link" Structure.
    pHddHdl:  The context passed in by the caller. (e.g., BSL specific context)
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkAccept is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkAccept 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Accept_Physical_Link_Cmd   *pBapHCIPhysLinkAccept,
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
                            /* And I get phy_link_handle from the Command */
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkDisconnect()

  DESCRIPTION 
    Implements the actual HCI Disconnect Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIPhysLinkDisconnect:  pointer to the "HCI Disconnect Physical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkDisconnect is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkDisconnect 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Disconnect_Physical_Link_Cmd   *pBapHCIPhysLinkDisconnect,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkCreate()

  DESCRIPTION 
    Implements the actual HCI Create Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkCreate:  pointer to the "HCI Create Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkCreate is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkCreate
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Create_Logical_Link_Cmd   *pBapHCILogLinkCreate,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkAccept()

  DESCRIPTION 
    Implements the actual HCI Accept Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkAccept:  pointer to the "HCI Accept Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkAccept is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkAccept
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Accept_Logical_Link_Cmd   *pBapHCILogLinkAccept,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkDisconnect()

  DESCRIPTION 
    Implements the actual HCI Disconnect Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkDisconnect:  pointer to the "HCI Disconnect Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkDisconnect is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkDisconnect
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Disconnect_Logical_Link_Cmd   *pBapHCILogLinkDisconnect,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkCancel()

  DESCRIPTION 
    Implements the actual HCI Cancel Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkCancel:  pointer to the "HCI Cancel Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
                (BTW, the required "HCI Logical Link Complete Event" 
                will be generated by the BAP state machine and sent up 
                via the (*tpWLAN_BAPEventCB).)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkCancel is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkCancel
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Logical_Link_Cancel_Cmd   *pBapHCILogLinkCancel,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPFlowSpecModify()

  DESCRIPTION 
    Implements the actual HCI Modify Logical Link command
    Produces an asynchronous flow spec modify complete event. Through the 
    event callback.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlowSpecModify:  pointer to the "HCI Flow Spec Modify" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlowSpecModify is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPFlowSpecModify
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Flow_Spec_Modify_Cmd   *pBapHCIFlowSpecModify,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/* Host Controller and Baseband Commands */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReset()

  DESCRIPTION 
    Implements the actual HCI Reset command.
    Produces an asynchronous command complete event. Through the 
    command complete callback.  (I.E., (*tpWLAN_BAPEventCB).)

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReset
( 
  ptBtampHandle btampHandle
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetEventMask()

  DESCRIPTION 
    Implements the actual HCI Set Event Mask command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCISetEventMask:  pointer to the "HCI Set Event Mask" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCISetEventMask is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetEventMask
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Event_Mask_Cmd   *pBapHCISetEventMask,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPFlush()

  DESCRIPTION 
    Implements the actual HCI Flush command
    Produces an asynchronous command complete event. Through the 
    event callback. And an asynchronous Flush occurred event. Also through the 
    event callback.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlush:  pointer to the "HCI Flush" Structure.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlush is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPFlush
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Flush_Cmd     *pBapHCIFlush
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_EnhancedBAPFlush()

  DESCRIPTION 
    Implements the actual HCI Enhanced Flush command
    Produces an asynchronous command complete event. Through the command status 
    event callback. And an asynchronous Enhanced Flush Complete event. 

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlush:  pointer to the "HCI Enhanced Flush" Structure.
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlush is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_EnhancedBAPFlush
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Enhanced_Flush_Cmd     *pBapHCIFlush,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/

);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadConnectionAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Connection Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadConnectionAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadConnectionAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete */
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteConnectionAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Connection Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteConnectionAcceptTimeout:  pointer to the "HCI Connection Accept Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteConnectionAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteConnectionAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Connection_Accept_Timeout_Cmd   *pBapHCIWriteConnectionAcceptTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLinkSupervisionTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Link Supervision Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLinkSupervisionTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLinkSupervisionTimeout
( 
  ptBtampHandle btampHandle,
  /* Only 8 bits (phy_link_handle) of this log_link_handle are valid. */
  tBtampTLVHCI_Read_Link_Supervision_Timeout_Cmd *pBapHCIReadLinkSupervisionTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLinkSupervisionTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Link Supervision Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLinkSupervisionTimeout:  pointer to the "HCI Link Supervision Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLinkSupervisionTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLinkSupervisionTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Link_Supervision_Timeout_Cmd   *pBapHCIWriteLinkSupervisionTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/* v3.0 Host Controller and Baseband Commands */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLogicalLinkAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Logical Link Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLogicalLinkAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLogicalLinkAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLogicalLinkAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Logical Link Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLogicalLinkAcceptTimeout:  pointer to the "HCI Logical Link Accept Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLogicalLinkAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLogicalLinkAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Logical_Link_Accept_Timeout_Cmd   *pBapHCIWriteLogicalLinkAcceptTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetEventMaskPage2()

  DESCRIPTION 
    Implements the actual HCI Set Event Mask Page 2 command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCISetEventMaskPage2:  pointer to the "HCI Set Event Mask Page 2" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCISetEventMaskPage2 is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetEventMaskPage2
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Event_Mask_Page_2_Cmd   *pBapHCISetEventMaskPage2,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocationData()

  DESCRIPTION 
    Implements the actual HCI Read Location Data command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocationData is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocationData
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLocationData()

  DESCRIPTION 
    Implements the actual HCI Write Location Data command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLocationData:  pointer to the "HCI Write Location Data" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLocationData is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLocationData
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Location_Data_Cmd   *pBapHCIWriteLocationData,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadFlowControlMode()

  DESCRIPTION 
    Implements the actual HCI Read Flow Control Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadFlowControlMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadFlowControlMode
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteFlowControlMode()

  DESCRIPTION 
    Implements the actual HCI Write Flow Control Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteFlowControlMode:  pointer to the "HCI Write Flow Control Mode" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteFlowControlMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteFlowControlMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Flow_Control_Mode_Cmd   *pBapHCIWriteFlowControlMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadBestEffortFlushTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Best Effort Flush Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadBEFlushTO is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadBestEffortFlushTimeout
( 
  ptBtampHandle btampHandle,
  /* The log_link_hanlde identifies which logical link's BE TO*/
  tBtampTLVHCI_Read_Best_Effort_Flush_Timeout_Cmd   *pBapHCIReadBEFlushTO,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteBestEffortFlushTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Best Effort Flush TO command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteBEFlushTO:  pointer to the "HCI Write BE Flush TO" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteBEFlushTO is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteBestEffortFlushTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Best_Effort_Flush_Timeout_Cmd   *pBapHCIWriteBEFlushTO,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetShortRangeMode()

  DESCRIPTION 
    Implements the actual HCI Set Short Range Mode command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIShortRangeMode:  pointer to the "HCI Set Short Range Mode" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIShortRangeMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetShortRangeMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Short_Range_Mode_Cmd   *pBapHCIShortRangeMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPVendorSpecificCmd0()

  DESCRIPTION
    Implements the actual HCI Vendor Specific Command 0 (OGF 0x3f, OCF 0x0000).
    There is no need for a callback because when this call returns the action has
    been completed.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.

    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event.
                (The caller of this routine is responsible for sending
                the Command Complete event up the HCI interface.)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL
    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS

----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPVendorSpecificCmd0
(
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPVendorSpecificCmd1()

  DESCRIPTION
    Implements the actual HCI Vendor Specific Command 1 (OGF 0x3f, OCF 0x0001).
    There is no need for a callback because when this call returns the action has
    been completed.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.

    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event.
                (The caller of this routine is responsible for sending
                the Command Complete event up the HCI interface.)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL
    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS

----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPVendorSpecificCmd1
(
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/* End of v3.0 Host Controller and Baseband Commands */


/* Informational Parameters */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalVersionInfo()

  DESCRIPTION 
    Implements the actual HCI Read Local Version Info command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    // There are really no input parameters in this command.  
    // Just the command opcode itself is sufficient.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalVersionInfo is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalVersionInfo
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalSupportedCmds()

  DESCRIPTION 
    Implements the actual HCI Read Local Supported Commands.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    // There are really no input parameters in this command.  
    // Just the command opcode itself is sufficient.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalSupportedCmds is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalSupportedCmds
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadBufferSize()

  DESCRIPTION 
    Implements the actual HCI Read Buffer Size command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadBufferSize is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadBufferSize
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadDataBlockSize()

  DESCRIPTION 
    Implements the actual HCI Read Data Block Size command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadDataBlockSize is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadDataBlockSize
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*
Status Parameters
*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadFailedContactCounter()

  DESCRIPTION 
    Implements the actual HCI Read Failed Contact Counter command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadFailedContactCounter:  pointer to the "HCI Read Failed Contact Counter" structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadFailedContactCounter or
                         pFailedContactCounter is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadFailedContactCounter
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Failed_Contact_Counter_Cmd  *pBapHCIReadFailedContactCounter,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPResetFailedContactCounter()

  DESCRIPTION 
    Implements the actual HCI Reset Failed Contact Counter command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIResetFailedContactCounter:  pointer to the "HCI Reset Failed Contact Counter" structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIResetFailedContactCounter is NULL
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPResetFailedContactCounter
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Reset_Failed_Contact_Counter_Cmd *pBapHCIResetFailedContactCounter,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLinkQuality()

  DESCRIPTION 
    Implements the actual HCI Read Link Quality command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLinkQuality:  pointer to the "HCI Read Link Quality" structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLinkQuality or 
                         pBapHCILinkQuality is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLinkQuality
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Link_Quality_Cmd *pBapHCIReadLinkQuality,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadRSSI()

  DESCRIPTION 
    Implements the actual HCI Read RSSI command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadRSSI:  pointer to the "HCI Read RSSI" structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadRSSI or
                         pBapHCIRSSI is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadRSSI
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_RSSI_Cmd *pBapHCIReadRSSI,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalAMPInfo()

  DESCRIPTION 
    Implements the actual HCI Read Local AMP Information command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLocalAMPInfo:  pointer to the "HCI Read Local AMP Info" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalAMPInfo or 
                         pBapHCILocalAMPInfo is NULL
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalAMPInfo
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Local_AMP_Information_Cmd *pBapHCIReadLocalAMPInfo,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalAMPAssoc()

  DESCRIPTION 
    Implements the actual HCI Read Local AMP Assoc command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLocalAMPAssoc:  pointer to the "HCI Read Local AMP Assoc" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalAMPAssoc 
                        (or pBapHCILocalAMPAssoc) is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalAMPAssoc
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Local_AMP_Assoc_Cmd   *pBapHCIReadLocalAMPAssoc,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteRemoteAMPAssoc()

  DESCRIPTION 
    Implements the actual HCI Write Remote AMP Assoc command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteRemoteAMPAssoc:  pointer to the "HCI Write Remote AMP Assoc" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteRemoteAMPAssoc is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteRemoteAMPAssoc
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Remote_AMP_ASSOC_Cmd   *pBapHCIWriteRemoteAMPAssoc,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*
Debug Commands
*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLoopbackMode()

  DESCRIPTION 
    Implements the actual HCI Read Loopback Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLoopbackMode:  pointer to the "HCI Read Loopback Mode".
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLoopbackMode or 
                         pBapHCILoopbackMode is NULL.
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLoopbackMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Loopback_Mode_Cmd  *pBapHCIReadLoopbackMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLoopbackMode()

  DESCRIPTION 
    Implements the actual HCI Write Loopback Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLoopbackMode:  pointer to the "HCI Write Loopback Mode" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLoopbackMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLoopbackMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Loopback_Mode_Cmd   *pBapHCIWriteLoopbackMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
);


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetConfig()

  DESCRIPTION 
     The function updates some configuration for BAP module in SME during SMEs
     close -> open sequence.
   
     BAP applies the new configuration at the next transaction.


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadRSSI:  pointer to the "HCI Read RSSI" structure.
   
    IN
    pConfig: a pointer to a caller allocated object of typedef struct WLANBAP_ConfigType.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pConfig or btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetConfig
( 
  ptBtampHandle btampHandle,
  WLANBAP_ConfigType *pConfig
);

/*===========================================================================

  FUNCTION    WLANBAP_GetAcFromTxDataPkt

  DESCRIPTION 

    HDD will call this API when it has a HCI Data Packet (SKB) and it wants 
    to find AC type of the data frame from the HCI header on the data pkt
    - to be send using TL.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
 
    pHciData: Pointer to the HCI data frame
 
    pucAC:       Pointer to return the access category 
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_GetAcFromTxDataPkt
( 
  ptBtampHandle     btampHandle,  /* Used by BAP to identify the actual session
                                    and therefore addresses */
  void              *pHciData,     /* Pointer to the HCI data frame */
  WLANTL_ACEnumType *pucAC        /* Return the AC here */
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPGetMask()

  DESCRIPTION 
     The function gets the updated event mask from BAP core.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    
   
    IN
    pEvent_mask_page_2: a pointer to a caller allocated object of 8 bytes.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pEvent_mask_page_2 or btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPGetMask( ptBtampHandle btampHandle, 
                 v_U8_t       *pEvent_mask_page_2);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPDisconnect()

  DESCRIPTION 
     The function to request to BAP core to disconnect currecnt AMP connection.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPDisconnect
( 
  ptBtampHandle btampHandle
);

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSessionOn()

  DESCRIPTION 
     The function to check from BAP core if AMP connection is up right now.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_TRUE:  AMP connection is on 
    VOS_FALSE: AMP connection is not on
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
v_BOOL_t WLAN_BAPSessionOn
( 
  ptBtampHandle btampHandle
);

#ifdef __cplusplus
 }
#endif 


#endif /* #ifndef WLAN_QCT_WLANBAP_H */

