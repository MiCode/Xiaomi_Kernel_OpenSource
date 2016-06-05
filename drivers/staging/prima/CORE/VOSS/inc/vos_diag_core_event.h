/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_DIAG_CORE_EVENT_H )
#define __VOS_DIAG_CORE_EVENT_H

/**=========================================================================
  
  \file  vos_event.h
  
  \brief virtual Operating System Services (vOSS) DIAG Events
               
   Definitions for vOSS Events
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_types.h"
#include "vos_pack_align.h"
#include "i_vos_diag_core_event.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define WAKE_LOCK_NAME_LEN 80


/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_SECURITY
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t eventId;
   v_U8_t authMode;
   v_U8_t encryptionModeUnicast;
   v_U8_t encryptionModeMulticast;
   v_U8_t pmkIDMatch;
   v_U8_t bssid[6];
   v_U8_t keyId;
   v_U8_t status;
} vos_event_wlan_security_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_STATUS
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t eventId;
   v_U8_t ssid[6];
   v_U8_t bssType;
   v_U8_t rssi;
   v_U8_t channel;
   v_U8_t qosCapability;
   v_U8_t authType;
   v_U8_t encryptionType;
   v_U8_t reason;
   v_U8_t reasonDisconnect;
} vos_event_wlan_status_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_HANDOFF
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t eventId;
   v_U8_t currentApBssid[6];
   v_U8_t currentApRssi;
   v_U8_t candidateApBssid[6];
   v_U8_t candidateApRssi;
} vos_event_wlan_handoff_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_VCC
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t eventId;
   v_U8_t rssi;
   v_U8_t txPer;
   v_U8_t rxPer;
   int    linkQuality;
} vos_event_wlan_vcc_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_QOS
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t eventId;
   v_U8_t reasonCode;
} vos_event_wlan_qos_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_PE
  ------------------------------------------------------------------------*/
typedef struct
{
   char       bssid[6];
   v_U16_t    event_type;
   v_U16_t    sme_state;
   v_U16_t    mlm_state;
   v_U16_t    status;
   v_U16_t    reason_code;   
} vos_event_wlan_pe_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_ADD_BLOCK_ACK_SUCCESS
  ------------------------------------------------------------------------*/
typedef struct
{
  char        ucBaPeerMac[6];
  v_U8_t      ucBaTid;
  v_U8_t      ucBaBufferSize;
  v_U16_t     usBaSSN;
  v_U8_t      fInitiator; 
} vos_event_wlan_add_block_ack_success_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_ADD_BLOCK_ACK_FAILED
  ------------------------------------------------------------------------*/
typedef struct
{
  char        ucBaPeerMac[6]; 
  v_U8_t      ucBaTid;
  v_U8_t      ucReasonCode;
  v_U8_t      fInitiator; 
} vos_event_wlan_add_block_ack_failed_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_DELETE_BLOCK_ACK_SUCCESS
  ------------------------------------------------------------------------*/
typedef struct
{
  char        ucBaPeerMac[6]; 
  v_U8_t      ucBaTid;
  v_U8_t      ucDeleteReasonCode; 
} vos_event_wlan_add_block_ack_deleted_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_DELETE_BLOCK_ACK_FAILED
  ------------------------------------------------------------------------*/
typedef struct
{
  char        ucBaPeerMac[6]; 
  v_U8_t      ucBaTid;
  v_U8_t      ucDeleteReasonCode; 
  v_U8_t      ucFailReasonCode;
} vos_event_wlan_add_block_ack_delete_failed_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_BSS_PROTECTION
  ------------------------------------------------------------------------*/
typedef struct
{
  v_U8_t      event_type;
  v_U8_t      prot_type; 
} vos_event_wlan_bss_prot_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_BRINGUP_STATUS
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U16_t  wlanStatus;
   char     driverVersion[10];
} vos_event_wlan_bringup_status_payload_type;

VOS_PACK_START

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_POWERSAVE_GENERIC
  ------------------------------------------------------------------------*/
typedef VOS_PACK_PRE struct
{
   v_U8_t   event_subtype;
   v_U32_t  imps_period;
   v_U8_t   full_power_request_reason;
   v_U8_t   pmc_current_state;
   v_U8_t   enable_disable_powersave_mode;
   v_U8_t   winmob_d_power_state;
   v_U8_t   dtim_period;
   v_U16_t  final_listen_intv;
   v_U16_t  bmps_auto_timer_duration;
   v_U16_t  bmps_period;
} VOS_PACK_POST vos_event_wlan_powersave_payload_type;

VOS_PACK_END

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_POWERSAVE_WOW
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t   event_subtype;
   v_U8_t   wow_type;
   v_U8_t   wow_magic_pattern[6];
   v_U8_t   wow_del_ptrn_id;
   v_U8_t   wow_wakeup_cause;
   v_U8_t   wow_wakeup_cause_pbm_ptrn_id;
} vos_event_wlan_powersave_wow_payload_type;

/*------------------------------------------------------------------------- 
  Event ID: EVENT_WLAN_BTC
  ------------------------------------------------------------------------*/
typedef struct
{
   v_U8_t  eventId;
   v_U8_t  btAddr[6];
   v_U16_t connHandle;
   v_U8_t  connStatus;
   v_U8_t  linkType;
   v_U8_t  scoInterval;
   v_U8_t  scoWindow;
   v_U8_t  retransWindow;
   v_U8_t  mode;
} vos_event_wlan_btc_type;

/*-------------------------------------------------------------------------
  Event ID: EVENT_WLAN_EAPOL
  ------------------------------------------------------------------------*/
struct vos_event_wlan_eapol
{
       uint8_t   event_sub_type;
       uint8_t   eapol_packet_type;
       uint16_t  eapol_key_info;
       uint16_t  eapol_rate;
       uint8_t   dest_addr[6];
       uint8_t   src_addr[6];
};
/*-------------------------------------------------------------------------
  Event ID: EVENT_WLAN_WAKE_LOCK
  ------------------------------------------------------------------------*/
/*
 * struct vos_event_wlan_wake_lock - Structure holding the wakelock information
 * @status: Whether the wakelock is taken/released
 * @reason: Reason for taking this wakelock
 * @timeout: Timeout value in case of timed wakelocks
 * @name_len: Length of the name of the wakelock that will follow
 * @name: Name of the wakelock
 *
 * This structure will hold the wakelock informations
 */
struct vos_event_wlan_wake_lock
{
       uint32_t status;
       uint32_t reason;
       uint32_t timeout;
       uint32_t name_len;
       char     name[WAKE_LOCK_NAME_LEN];
};




/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
enum wifi_connectivity_events {
       WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED,
       WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED,
};

/*
 * enum wake_lock_reason - Reason for taking wakelock
* @WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT: Driver initialization
 * @WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT: Driver re-initialization
 * @WIFI_POWER_EVENT_WAKELOCK_SCAN: Scan request/response handling
 * @WIFI_POWER_EVENT_WAKELOCK_RESUME_WLAN: Driver resume
 * @WIFI_POWER_EVENT_WAKELOCK_ROC: Remain on channel request/response handling
 * @WIFI_POWER_EVENT_WAKELOCK_HOLD_RX: Wakelocks taken for receive
 * @WIFI_POWER_EVENT_WAKELOCK_SAP: SoftAP related wakelocks
 * This enum has the reason codes why the wakelocks were taken/released
 */
enum wake_lock_reason {
       WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT,
       WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT,
       WIFI_POWER_EVENT_WAKELOCK_SCAN,
       WIFI_POWER_EVENT_WAKELOCK_RESUME_WLAN,
       WIFI_POWER_EVENT_WAKELOCK_ROC,
       WIFI_POWER_EVENT_WAKELOCK_HOLD_RX,
       WIFI_POWER_EVENT_WAKELOCK_SAP,
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __VOS_DIAG_CORE_EVENT_H
